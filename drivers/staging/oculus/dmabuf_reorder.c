// SPDX-License-Identifier: GPL-2.0

/*
 * DMA-buf region reorder module
 *
 * Creates a new dma-buf wrapping an existing buffer with a reordered
 * scatterlist.  Accepts an array of (offset, length) regions describing
 * which byte ranges of the source to map and in what order.
 * All offsets and lengths must be page-aligned.
 *
 * At map_dma_buf time the source buffer's physical pages are read from the
 * qcom_sg backing table, copied into a new sg_table according to the region
 * list, and DMA-mapped into the consumer's IOMMU in that requested order.
 *
 * The mem_buf_dma_buf_ops wrapper provides its own vmperm object
 * (allocated as locally-owned) so that downstream consumers (e.g. Venus
 * video encoder) recognise the wrapper as locally-owned / non-secure.
 * The source buffer must itself be exclusively locally-owned at ioctl
 * time; this is enforced by the mem_buf_dma_buf_exclusive_owner() check.
 * Source and wrapper vmperm objects are pinned for each active DMA mapping,
 * and CPU sync targets the wrapper's mapped sg_tables directly.
 */

#include <linux/scatterlist.h>
#include <linux/sort.h>
#include <linux/dmabuf_reorder.h>

/* ---- scatterlist helpers (work on physical pages, pre-DMA-map) ---- */

static int count_sg(struct sg_table *sgt, size_t start, size_t len)
{
	struct scatterlist *sg;
	size_t pos = 0;
	int i, n = 0;

	for_each_sg(sgt->sgl, sg, sgt->orig_nents, i) {
		size_t sg_end = pos + sg->length;

		if (sg_end > start && pos < start + len)
			n++;
		pos = sg_end;
		if (pos >= start + len)
			break;
	}
	return n;
}

static void copy_sg(struct scatterlist **cur, struct scatterlist **last,
		    struct sg_table *src_sgt, size_t start, size_t len)
{
	struct scatterlist *sg;
	size_t pos = 0;
	int i;

	for_each_sg(src_sgt->sgl, sg, src_sgt->orig_nents, i) {
		size_t sg_len = sg->length;
		size_t sg_end = pos + sg_len;

		if (sg_end > start && pos < start + len) {
			size_t skip = (pos < start) ? (start - pos) : 0;
			size_t trim = (sg_end > start + len) ?
				      (sg_end - start - len) : 0;
			size_t use = sg_len - skip - trim;
			unsigned long pg_off = sg->offset + skip;

			sg_set_page(*cur,
				    nth_page(sg_page(sg),
					     pg_off >> PAGE_SHIFT),
				    use, offset_in_page(pg_off));
			*last = *cur;
			*cur = sg_next(*cur);
		}
		pos += sg_len;
		if (pos >= start + len)
			break;
	}
}

/* ---- overlap detection ------------------------------------------- */

struct region_span {
	u64 start;
	u64 end;
};

static int cmp_span(const void *a, const void *b)
{
	const struct region_span *sa = a;
	const struct region_span *sb = b;

	if (sa->start < sb->start)
		return -1;
	if (sa->start > sb->start)
		return 1;
	return 0;
}

static bool regions_overlap(const struct dmabuf_reorder_region *regions,
			    u32 n)
{
	struct region_span spans[DMABUF_REORDER_MAX_REGIONS];
	u32 i;

	for (i = 0; i < n; i++) {
		spans[i].start = regions[i].offset;
		spans[i].end = regions[i].offset + regions[i].length;
	}

	sort(spans, n, sizeof(spans[0]), cmp_span, NULL);

	for (i = 1; i < n; i++) {
		if (spans[i].start < spans[i - 1].end)
			return true;
	}
	return false;
}

#if IS_ENABLED(CONFIG_QCOM_MEM_BUF)

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/mem-buf.h>
#include <linux/mem-buf-exporter.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "../../../drivers/dma-buf/heaps/qcom_sg_ops.h"

/* ---- data structures --------------------------------------------- */

struct reorder_buffer {
	struct dma_buf *src;
	struct mem_buf_vmperm *vmperm;
	struct sg_table dummy_sgt;
	struct mutex map_lock;		/* serializes mapped_list updates and CPU sync walks */
	struct list_head mapped_list;	/* active reorder_attach objects */
	u32 num_regions;
	size_t total_size;
	struct dmabuf_reorder_region regions[DMABUF_REORDER_MAX_REGIONS];
};

struct reorder_attach {
	struct sg_table sgt;
	struct mem_buf_vmperm *src_vmperm;
	struct device *dev;
	struct list_head node;
	bool can_cmo;
};

/* ---- dma_buf_ops ------------------------------------------------- */

/*
 * dmabuf_reorder is intentionally scoped to qcom_sg mem_buf-backed dma-bufs.
 * dma-buf does not expose a generic raw backing sg_table API; qcom_sg keeps
 * the physical sg_table in dmabuf->priv.
 */
static bool reorder_src_is_qcom_sg(struct dma_buf *dmabuf)
{
	return dmabuf->ops == &qcom_sg_buf_ops.dma_ops;
}

static struct sg_table *reorder_map(struct dma_buf_attachment *attach,
				    enum dma_data_direction dir)
{
	struct reorder_buffer *rb = attach->dmabuf->priv;
	struct qcom_sg_buffer *src_buf;
	struct reorder_attach *ra;
	struct sg_table *src;
	unsigned int total = 0;
	int ret;
	u32 r;

	if (attach->priv)
		return ERR_PTR(-EBUSY);

	if (!reorder_src_is_qcom_sg(rb->src))
		return ERR_PTR(-EOPNOTSUPP);

	if (!mem_buf_dma_buf_exclusive_owner(rb->src))
		return ERR_PTR(-EPERM);

	src_buf = rb->src->priv;

	ra = kzalloc(sizeof(*ra), GFP_KERNEL);
	if (!ra)
		return ERR_PTR(-ENOMEM);

	if (!src_buf || !src_buf->sg_table.sgl || !src_buf->vmperm) {
		ret = -EINVAL;
		goto err_free;
	}

	src = &src_buf->sg_table;
	ra->src_vmperm = src_buf->vmperm;
	ra->dev = attach->dev;

	/* Keep source VM permissions stable for the lifetime of this mapping. */
	mem_buf_vmperm_pin(ra->src_vmperm);

	ra->can_cmo = !src_buf->uncached && mem_buf_vmperm_can_cmo(src_buf->vmperm);

	/*
	 * qcom_sg treats qcom_sg_buffer->sg_table as stable for the dma-buf
	 * lifetime; qcom_sg_attach() duplicates the same table locklessly.
	 */
	for (r = 0; r < rb->num_regions; r++) {
		unsigned int n = (unsigned int)count_sg(src,
				rb->regions[r].offset, rb->regions[r].length);

		if (check_add_overflow(total, n, &total)) {
			ret = -EOVERFLOW;
			goto err_unpin_src;
		}
	}
	if (!total) {
		ret = -EINVAL;
		goto err_unpin_src;
	}

	ret = sg_alloc_table(&ra->sgt, total, GFP_KERNEL);
	if (ret)
		goto err_unpin_src;

	{
		struct scatterlist *cur = ra->sgt.sgl;
		struct scatterlist *last = ra->sgt.sgl;

		for (r = 0; r < rb->num_regions; r++)
			copy_sg(&cur, &last, src,
				rb->regions[r].offset, rb->regions[r].length);
		sg_mark_end(last);
	}

	mem_buf_vmperm_pin(rb->vmperm);

	ra->sgt.nents = dma_map_sg_attrs(attach->dev, ra->sgt.sgl,
					  total, dir,
					  attach->dma_map_attrs);
	if (!ra->sgt.nents) {
		ret = -ENOMEM;
		goto err_unpin_wrapper;
	}

	mutex_lock(&rb->map_lock);
	if (attach->priv) {
		/* Lost the race: another concurrent map_dma_buf installed first. */
		mutex_unlock(&rb->map_lock);
		ret = -EBUSY;
		goto err_unmap_sg;
	}
	attach->priv = ra;
	list_add(&ra->node, &rb->mapped_list);
	mutex_unlock(&rb->map_lock);
	return &ra->sgt;

err_unmap_sg:
	dma_unmap_sg_attrs(attach->dev, ra->sgt.sgl, ra->sgt.orig_nents,
			   dir, attach->dma_map_attrs);
err_unpin_wrapper:
	mem_buf_vmperm_unpin(rb->vmperm);
	sg_free_table(&ra->sgt);
err_unpin_src:
	mem_buf_vmperm_unpin(ra->src_vmperm);
err_free:
	kfree(ra);
	return ERR_PTR(ret);
}

static void reorder_unmap(struct dma_buf_attachment *attach,
			  struct sg_table *sgt, enum dma_data_direction dir)
{
	struct reorder_buffer *rb = attach->dmabuf->priv;
	struct reorder_attach *ra;

	mutex_lock(&rb->map_lock);
	ra = attach->priv;
	attach->priv = NULL;
	if (ra)
		list_del(&ra->node);
	mutex_unlock(&rb->map_lock);

	if (!ra)
		return;

	dma_unmap_sg_attrs(attach->dev, ra->sgt.sgl, ra->sgt.orig_nents,
			   dir, attach->dma_map_attrs);
	mem_buf_vmperm_unpin(rb->vmperm);
	sg_free_table(&ra->sgt);
	mem_buf_vmperm_unpin(ra->src_vmperm);
	kfree(ra);
}

static void reorder_release(struct dma_buf *dmabuf)
{
	struct reorder_buffer *rb = dmabuf->priv;

	mutex_destroy(&rb->map_lock);
#ifdef MEM_BUF_WRAPPER_FLAG_ZOMBIE
	mem_buf_vmperm_free(rb->vmperm);
#else
	mem_buf_vmperm_release(rb->vmperm);
#endif
	dma_buf_put(rb->src);
	kfree(rb);
}

static int reorder_begin_cpu_access(struct dma_buf *dmabuf,
				    enum dma_data_direction dir)
{
	struct reorder_buffer *rb = dmabuf->priv;
	struct reorder_attach *ra;

	mutex_lock(&rb->map_lock);
	list_for_each_entry(ra, &rb->mapped_list, node) {
		if (ra->can_cmo)
			dma_sync_sgtable_for_cpu(ra->dev, &ra->sgt, dir);
	}
	mutex_unlock(&rb->map_lock);
	return 0;
}

static int reorder_end_cpu_access(struct dma_buf *dmabuf,
				  enum dma_data_direction dir)
{
	struct reorder_buffer *rb = dmabuf->priv;
	struct reorder_attach *ra;

	mutex_lock(&rb->map_lock);
	list_for_each_entry(ra, &rb->mapped_list, node) {
		if (ra->can_cmo)
			dma_sync_sgtable_for_device(ra->dev, &ra->sgt, dir);
	}
	mutex_unlock(&rb->map_lock);
	return 0;
}

/* ---- remaining ops callbacks ------------------------------------- */

static struct mem_buf_vmperm *reorder_lookup(struct dma_buf *dmabuf)
{
	struct reorder_buffer *rb = dmabuf->priv;

	return rb->vmperm;
}

static int reorder_attach_cb(struct dma_buf *dmabuf,
			     struct dma_buf_attachment *attachment)
{
	return 0;
}

/*
 * .uncached was removed from struct mem_buf_dma_buf_ops on niobe.
 * MEM_BUF_WRAPPER_FLAG_ZOMBIE is defined only in the niobe header.
 */
#ifndef MEM_BUF_WRAPPER_FLAG_ZOMBIE
static bool reorder_uncached(struct dma_buf *dmabuf)
{
	return false;
}
#endif

/*
 * Not const: mem_buf_dma_buf_export() writes dma_ops.attach once
 * (sets it to mem_buf_dma_buf_attach for CFI compatibility).
 *
 * Intentionally omitted ops:
 *   .mmap — This wrapper is DMA-map only.  The reordered scatterlist
 *           exists solely in the IOMMU mapping created by map_dma_buf;
 *           there is no corresponding CPU virtual mapping.  mmap() on
 *           the wrapper fd will return -EINVAL.  If CPU access to the
 *           underlying pages is needed, mmap the *source* dma-buf fd
 *           (which retains the original, un-reordered layout).
 *   .vmap — Same rationale as mmap.
 */
static struct mem_buf_dma_buf_ops reorder_mem_buf_ops = {
	.lookup = reorder_lookup,
	.attach = reorder_attach_cb,
#ifndef MEM_BUF_WRAPPER_FLAG_ZOMBIE
	.uncached = reorder_uncached,
#endif
	.dma_ops = {
		.map_dma_buf = reorder_map,
		.unmap_dma_buf = reorder_unmap,
		.release = reorder_release,
		.begin_cpu_access = reorder_begin_cpu_access,
		.end_cpu_access = reorder_end_cpu_access,
	},
};

/* ---- ioctl ------------------------------------------------------- */

static long dmabuf_reorder_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	struct dmabuf_reorder_request req;
	struct reorder_buffer *rb;
	struct dma_buf *src;
	DEFINE_DMA_BUF_EXPORT_INFO(exp);
	struct dma_buf *buf;
	size_t total_len = 0;
	int fd, ret;
	u32 r;

	if (cmd != DMABUF_REORDER)
		return -ENOTTY;
	if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
		return -EFAULT;
	if (req.num_regions < 1 || req.num_regions > DMABUF_REORDER_MAX_REGIONS)
		return -EINVAL;
	if (req.pad != 0)
		return -EINVAL;

	src = dma_buf_get(req.src_fd);
	if (IS_ERR(src))
		return PTR_ERR(src);

	if (!mem_buf_dma_buf_exclusive_owner(src)) {
		dma_buf_put(src);
		return -EPERM;
	}
	if (!reorder_src_is_qcom_sg(src)) {
		dma_buf_put(src);
		return -EOPNOTSUPP;
	}

	rb = kzalloc(sizeof(*rb), GFP_KERNEL);
	if (!rb) {
		dma_buf_put(src);
		return -ENOMEM;
	}
	mutex_init(&rb->map_lock);
	INIT_LIST_HEAD(&rb->mapped_list);

	if (copy_from_user(rb->regions, u64_to_user_ptr(req.regions_ptr),
			   req.num_regions * sizeof(struct dmabuf_reorder_region))) {
		ret = -EFAULT;
		goto err_rb;
	}

	for (r = 0; r < req.num_regions; r++) {
		u64 off = rb->regions[r].offset;
		u64 len = rb->regions[r].length;

		if (!len || (off & (PAGE_SIZE - 1)) ||
		    (len & (PAGE_SIZE - 1))) {
			ret = -EINVAL;
			goto err_rb;
		}
		if (off > src->size || len > src->size - off) {
			ret = -EINVAL;
			goto err_rb;
		}
		if (check_add_overflow(total_len, (size_t)len, &total_len)) {
			ret = -EOVERFLOW;
			goto err_rb;
		}
	}

	if (regions_overlap(rb->regions, req.num_regions)) {
		ret = -EINVAL;
		goto err_rb;
	}

	rb->src = src;
	rb->num_regions = req.num_regions;
	rb->total_size = total_len;

#ifdef MEM_BUF_WRAPPER_FLAG_ZOMBIE
	rb->vmperm = mem_buf_vmperm_alloc(&rb->dummy_sgt, NULL, NULL);
#else
	rb->vmperm = mem_buf_vmperm_alloc(&rb->dummy_sgt);
#endif
	if (IS_ERR(rb->vmperm)) {
		ret = PTR_ERR(rb->vmperm);
		goto err_rb;
	}

	exp.size = total_len;
	exp.flags = src->file->f_flags & O_ACCMODE;
	exp.resv = src->resv;
	exp.priv = rb;

	buf = mem_buf_dma_buf_export(&exp, &reorder_mem_buf_ops);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto err_vmperm;
	}

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		dma_buf_put(buf);
		return fd;
	}

	req.out_fd = fd;
	if (copy_to_user((void __user *)arg, &req, sizeof(req))) {
		put_unused_fd(fd);
		dma_buf_put(buf);
		return -EFAULT;
	}

	fd_install(fd, buf->file);
	return 0;

err_vmperm:
#ifdef MEM_BUF_WRAPPER_FLAG_ZOMBIE
	mem_buf_vmperm_free(rb->vmperm);
#else
	mem_buf_vmperm_release(rb->vmperm);
#endif
err_rb:
	mutex_destroy(&rb->map_lock);
	kfree(rb);
	dma_buf_put(src);
	return ret;
}

static const struct file_operations dmabuf_reorder_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = dmabuf_reorder_ioctl,
	.compat_ioctl = dmabuf_reorder_ioctl,
};

static struct miscdevice dmabuf_reorder_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dmabuf_reorder",
	.fops = &dmabuf_reorder_fops,
};

/* Module boilerplate suppressed when included directly by dmabuf_reorder_test.c. */
#ifndef DMABUF_REORDER_KUNIT_INCLUDE
static int __init dmabuf_reorder_init(void)
{
	return misc_register(&dmabuf_reorder_dev);
}

static void __exit dmabuf_reorder_exit(void)
{
	misc_deregister(&dmabuf_reorder_dev);
}

module_init(dmabuf_reorder_init);
module_exit(dmabuf_reorder_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DMA-buf scatterlist region reorder");
MODULE_AUTHOR("Anthony Bajoua <anthonybajoua@meta.com>");
MODULE_IMPORT_NS(DMA_BUF);
#endif

#endif /* IS_ENABLED(CONFIG_QCOM_MEM_BUF) */
