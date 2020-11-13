/*
 * Copyright (C) 2014 Sergey Senozhatsky.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/cpu.h>

#include "zcomp.h"

static const char * const backends[] = {
#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
	/* Only Zstd is available when CONFIG_ZRAM_ZSTD_ADVANCED is enabled */
	"zstd",
#else
	"lzo",
#if IS_ENABLED(CONFIG_CRYPTO_LZ4)
	"lz4",
#endif
#if IS_ENABLED(CONFIG_CRYPTO_LZ4HC)
	"lz4hc",
#endif
#if IS_ENABLED(CONFIG_CRYPTO_842)
	"842",
#endif
#if IS_ENABLED(CONFIG_CRYPTO_ZSTD)
	"zstd",
#endif
#endif
	NULL
};

static void zcomp_strm_free(struct zcomp_strm *zstrm)
{
#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
	if (!IS_ERR_OR_NULL(zstrm->cctx_wksp)) {
		kvfree(zstrm->cctx_wksp);
		zstrm->cctx_wksp = NULL;
		zstrm->cctx = NULL;
	}

	if (!IS_ERR_OR_NULL(zstrm->dctx_wksp)) {
		kvfree(zstrm->dctx_wksp);
		zstrm->dctx_wksp = NULL;
		zstrm->dctx = NULL;
	}
#else
	if (!IS_ERR_OR_NULL(zstrm->tfm))
		crypto_free_comp(zstrm->tfm);
#endif
	free_pages((unsigned long)zstrm->buffer, 1);
	kfree(zstrm);
}

#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
static int zcomp_alloc_zstd_contexts(struct zcomp_strm *zstrm)
{
	size_t cctx_wksp_size, dctx_wksp_size;

	zstrm->params = ZSTD_getParams(CONFIG_ZRAM_ZSTD_COMPRESSION_LEVEL, PAGE_SIZE, 0);
	if (zstrm->params.cParams.windowLog > ilog2(PAGE_SIZE))
		zstrm->params.cParams.windowLog = ilog2(PAGE_SIZE);

	cctx_wksp_size = ZSTD_CCtxWorkspaceBound(zstrm->params.cParams);
	dctx_wksp_size = ZSTD_DCtxWorkspaceBound();

	zstrm->cctx_wksp = kvmalloc(cctx_wksp_size, GFP_KERNEL | __GFP_ZERO);
	zstrm->dctx_wksp = kvmalloc(dctx_wksp_size, GFP_KERNEL | __GFP_ZERO);
	if (IS_ERR_OR_NULL(zstrm->cctx_wksp) || IS_ERR_OR_NULL(zstrm->dctx_wksp))
		return -ENOMEM;

	zstrm->cctx = ZSTD_initCCtx(zstrm->cctx_wksp, cctx_wksp_size);
	zstrm->dctx = ZSTD_initDCtx(zstrm->dctx_wksp, dctx_wksp_size);
	if (!zstrm->cctx || !zstrm->dctx)
		return -EINVAL;

	return 0;
}
#endif

/*
 * allocate new zcomp_strm structure with ->tfm initialized by
 * backend, return NULL on error
 */
static struct zcomp_strm *zcomp_strm_alloc(struct zcomp *comp)
{
	struct zcomp_strm *zstrm = kmalloc(sizeof(*zstrm), GFP_KERNEL);
	if (!zstrm)
		return NULL;

#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
	if (zcomp_alloc_zstd_contexts(zstrm))
		goto error;
#else
	zstrm->tfm = crypto_alloc_comp(comp->name, 0, 0);
	if (IS_ERR_OR_NULL(zstrm->tfm))
		goto error;
#endif

	/*
	 * allocate 2 pages. 1 for compressed data, plus 1 extra for the
	 * case when compressed size is larger than the original one
	 */
	zstrm->buffer = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
	if (!zstrm->buffer)
		goto error;

	return zstrm;

error:
	zcomp_strm_free(zstrm);
	return NULL;
}

bool zcomp_available_algorithm(const char *comp)
{
	int i;

	i = __sysfs_match_string(backends, -1, comp);
	if (i >= 0)
		return true;

#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
	return false;
#else
	/*
	 * Crypto does not ignore a trailing new line symbol,
	 * so make sure you don't supply a string containing
	 * one.
	 * This also means that we permit zcomp initialisation
	 * with any compressing algorithm known to crypto api.
	 */
	return crypto_has_comp(comp, 0, 0) == 1;
#endif
}

/* show available compressors */
ssize_t zcomp_available_show(const char *comp, char *buf)
{
	bool known_algorithm = false;
	ssize_t sz = 0;
	int i = 0;

	for (; backends[i]; i++) {
		if (!strcmp(comp, backends[i])) {
			known_algorithm = true;
			sz += scnprintf(buf + sz, PAGE_SIZE - sz - 2,
					"[%s] ", backends[i]);
		} else {
			sz += scnprintf(buf + sz, PAGE_SIZE - sz - 2,
					"%s ", backends[i]);
		}
	}

#if !IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
	/*
	 * Out-of-tree module known to crypto api or a missing
	 * entry in `backends'.
	 */
	if (!known_algorithm && crypto_has_comp(comp, 0, 0) == 1)
		sz += scnprintf(buf + sz, PAGE_SIZE - sz - 2,
				"[%s] ", comp);
#endif

	sz += scnprintf(buf + sz, PAGE_SIZE - sz, "\n");
	return sz;
}

struct zcomp_strm *zcomp_stream_get(struct zcomp *comp)
{
	return *get_cpu_ptr(comp->stream);
}

void zcomp_stream_put(struct zcomp *comp)
{
	put_cpu_ptr(comp->stream);
}

int zcomp_compress(struct zcomp_strm *zstrm,
		const void *src, unsigned int *dst_len)
{
#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
	size_t out_len;
#endif

	/*
	 * Our dst memory (zstrm->buffer) is always `2 * PAGE_SIZE' sized
	 * because sometimes we can endup having a bigger compressed data
	 * due to various reasons: for example compression algorithms tend
	 * to add some padding to the compressed buffer. Speaking of padding,
	 * comp algorithm `842' pads the compressed length to multiple of 8
	 * and returns -ENOSP when the dst memory is not big enough, which
	 * is not something that ZRAM wants to see. We can handle the
	 * `compressed_size > PAGE_SIZE' case easily in ZRAM, but when we
	 * receive -ERRNO from the compressing backend we can't help it
	 * anymore. To make `842' happy we need to tell the exact size of
	 * the dst buffer, zram_drv will take care of the fact that
	 * compressed buffer is too big.
	 */
	*dst_len = PAGE_SIZE * 2;

#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
	out_len = ZSTD_compressCCtx(zstrm->cctx, zstrm->buffer, *dst_len, src,
			PAGE_SIZE, zstrm->params);
	if (ZSTD_isError(out_len))
		return -EINVAL;

	*dst_len = out_len;
	return 0;
#else
	return crypto_comp_compress(zstrm->tfm,
			src, PAGE_SIZE,
			zstrm->buffer, dst_len);
#endif
}

int zcomp_decompress(struct zcomp_strm *zstrm,
		const void *src, unsigned int src_len, void *dst)
{
	unsigned int dst_len = PAGE_SIZE;

#if IS_ENABLED(CONFIG_ZRAM_ZSTD_ADVANCED)
	size_t out_len;

	out_len = ZSTD_decompressDCtx(zstrm->dctx, dst, dst_len, src, src_len);
	return ZSTD_isError(out_len) ? -EINVAL : 0;
#else
	return crypto_comp_decompress(zstrm->tfm,
			src, src_len,
			dst, &dst_len);
#endif
}

int zcomp_cpu_up_prepare(unsigned int cpu, struct hlist_node *node)
{
	struct zcomp *comp = hlist_entry(node, struct zcomp, node);
	struct zcomp_strm *zstrm;

	if (WARN_ON(*per_cpu_ptr(comp->stream, cpu)))
		return 0;

	zstrm = zcomp_strm_alloc(comp);
	if (IS_ERR_OR_NULL(zstrm)) {
		pr_err("Can't allocate a compression stream\n");
		return -ENOMEM;
	}
	*per_cpu_ptr(comp->stream, cpu) = zstrm;
	return 0;
}

int zcomp_cpu_dead(unsigned int cpu, struct hlist_node *node)
{
	struct zcomp *comp = hlist_entry(node, struct zcomp, node);
	struct zcomp_strm *zstrm;

	zstrm = *per_cpu_ptr(comp->stream, cpu);
	if (!IS_ERR_OR_NULL(zstrm))
		zcomp_strm_free(zstrm);
	*per_cpu_ptr(comp->stream, cpu) = NULL;
	return 0;
}

static int zcomp_init(struct zcomp *comp)
{
	int ret;

	comp->stream = alloc_percpu(struct zcomp_strm *);
	if (!comp->stream)
		return -ENOMEM;

	ret = cpuhp_state_add_instance(CPUHP_ZCOMP_PREPARE, &comp->node);
	if (ret < 0)
		goto cleanup;
	return 0;

cleanup:
	free_percpu(comp->stream);
	return ret;
}

void zcomp_destroy(struct zcomp *comp)
{
	cpuhp_state_remove_instance(CPUHP_ZCOMP_PREPARE, &comp->node);
	free_percpu(comp->stream);
	kfree(comp);
}

/*
 * search available compressors for requested algorithm.
 * allocate new zcomp and initialize it. return compressing
 * backend pointer or ERR_PTR if things went bad. ERR_PTR(-EINVAL)
 * if requested algorithm is not supported, ERR_PTR(-ENOMEM) in
 * case of allocation error, or any other error potentially
 * returned by zcomp_init().
 */
struct zcomp *zcomp_create(const char *compress)
{
	struct zcomp *comp;
	int error;

	if (!zcomp_available_algorithm(compress))
		return ERR_PTR(-EINVAL);

	comp = kzalloc(sizeof(struct zcomp), GFP_KERNEL);
	if (!comp)
		return ERR_PTR(-ENOMEM);

	comp->name = compress;
	error = zcomp_init(comp);
	if (error) {
		kfree(comp);
		return ERR_PTR(error);
	}
	return comp;
}
