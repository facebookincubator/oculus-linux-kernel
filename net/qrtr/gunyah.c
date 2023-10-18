// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020-2021, The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/io.h>
#include <linux/sizes.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_dbl.h>
#include <soc/qcom/secure_buffer.h>
#include "qrtr.h"

#define GUNYAH_MAGIC_KEY	0x24495043 /* "$IPC" */
#define FIFO_SIZE	0x4000
#define FIFO_FULL_RESERVE 8
#define FIFO_0_START	0x1000
#define FIFO_1_START	(FIFO_0_START + FIFO_SIZE)
#define GUNYAH_MAGIC_IDX	0x0
#define TAIL_0_IDX	0x1
#define HEAD_0_IDX	0x2
#define TAIL_1_IDX	0x3
#define HEAD_1_IDX	0x4
#define NOTIFY_0_IDX	0x5
#define NOTIFY_1_IDX	0x6
#define QRTR_DBL_MASK	0x1

#define MAX_PKT_SZ	SZ_64K

struct gunyah_ring {
	void *buf;
	size_t len;
	u32 offset;
};

struct gunyah_pipe {
	__le32 *tail;
	__le32 *head;
	__le32 *read_notify;

	void *fifo;
	size_t length;
};

/**
 * qrtr_gunyah_dev - qrtr gunyah transport structure
 * @ep: qrtr endpoint specific info.
 * @dev: device from platform_device.
 * @pkt: buf for reading from fifo.
 * @res: resource of reserved mem region
 * @memparcel: memparcel handle returned from sharing mem
 * @base: Base of the shared fifo.
 * @size: fifo size.
 * @master: primary vm indicator.
 * @peer_name: name of vm peer.
 * @rm_nb: notifier block for vm status from rm
 * @label: label for gunyah resources
 * @tx_dbl: doorbell for tx notifications.
 * @rx_dbl: doorbell for rx notifications.
 * @tx_pipe: TX gunyah specific info.
 * @rx_pipe: RX gunyah specific info.
 */
struct qrtr_gunyah_dev {
	struct qrtr_endpoint ep;
	struct device *dev;
	struct gunyah_ring ring;

	struct resource res;
	u32 memparcel;
	void *base;
	size_t size;
	bool master;
	u32 peer_name;
	struct notifier_block rm_nb;

	u32 label;
	void *tx_dbl;
	void *rx_dbl;
	struct work_struct work;

	struct gunyah_pipe tx_pipe;
	struct gunyah_pipe rx_pipe;
	wait_queue_head_t tx_avail_notify;
};

static void qrtr_gunyah_read(struct qrtr_gunyah_dev *qdev);
static void qrtr_gunyah_fifo_init(struct qrtr_gunyah_dev *qdev);

static void qrtr_gunyah_kick(struct qrtr_gunyah_dev *qdev)
{
	gh_dbl_flags_t dbl_mask = QRTR_DBL_MASK;
	int ret;

	ret = gh_dbl_send(qdev->tx_dbl, &dbl_mask, GH_DBL_NONBLOCK);
	if (ret) {
		dev_err(qdev->dev, "failed to raise doorbell %d\n", ret);
		if (!qdev->master)
			schedule_work(&qdev->work);
	}
}

static void qrtr_gunyah_retry_work(struct work_struct *work)
{
	struct qrtr_gunyah_dev *qdev = container_of(work, struct qrtr_gunyah_dev,
						   work);
	gh_dbl_flags_t dbl_mask = QRTR_DBL_MASK;

	gh_dbl_send(qdev->tx_dbl, &dbl_mask, 0);
}

static void qrtr_gunyah_cb(int irq, void *data)
{
	qrtr_gunyah_read((struct qrtr_gunyah_dev *)data);
}

static size_t gunyah_rx_avail(struct gunyah_pipe *pipe)
{
	size_t len;
	u32 head;
	u32 tail;

	head = le32_to_cpu(*pipe->head);
	tail = le32_to_cpu(*pipe->tail);

	if (head < tail)
		len = pipe->length - tail + head;
	else
		len = head - tail;

	if (WARN_ON_ONCE(len > pipe->length))
		len = 0;

	return len;
}

static void gunyah_rx_peak(struct gunyah_pipe *pipe, void *data,
			   unsigned int offset, size_t count)
{
	size_t len;
	u32 tail;

	tail = le32_to_cpu(*pipe->tail);
	tail += offset;
	if (tail >= pipe->length)
		tail -= pipe->length;

	if (WARN_ON_ONCE(tail > pipe->length))
		return;

	len = min_t(size_t, count, pipe->length - tail);
	if (len)
		memcpy_fromio(data, pipe->fifo + tail, len);

	if (len != count)
		memcpy_fromio(data + len, pipe->fifo, (count - len));
}

static void gunyah_rx_advance(struct gunyah_pipe *pipe, size_t count)
{
	u32 tail;

	tail = le32_to_cpu(*pipe->tail);

	tail += count;
	if (tail >= pipe->length)
		tail %= pipe->length;

	*pipe->tail = cpu_to_le32(tail);
}

static size_t gunyah_tx_avail(struct gunyah_pipe *pipe)
{
	u32 avail;
	u32 head;
	u32 tail;

	head = le32_to_cpu(*pipe->head);
	tail = le32_to_cpu(*pipe->tail);

	if (tail <= head)
		avail = pipe->length - head + tail;
	else
		avail = tail - head;

	if (avail < FIFO_FULL_RESERVE)
		avail = 0;
	else
		avail -= FIFO_FULL_RESERVE;

	if (WARN_ON_ONCE(avail > pipe->length))
		avail = 0;

	return avail;
}

static void gunyah_tx_write(struct gunyah_pipe *pipe, const void *data,
			    size_t count)
{
	size_t len;
	u32 head;

	head = le32_to_cpu(*pipe->head);
	if (WARN_ON_ONCE(head > pipe->length))
		return;

	len = min_t(size_t, count, pipe->length - head);
	if (len)
		memcpy_toio(pipe->fifo + head, data, len);

	if (len != count)
		memcpy_toio(pipe->fifo, data + len, count - len);

	head += count;
	if (head >= pipe->length)
		head -= pipe->length;

	/* Ensure ordering of fifo and head update */
	smp_wmb();

	*pipe->head = cpu_to_le32(head);
}

static void gunyah_set_tx_notify(struct qrtr_gunyah_dev *qdev)
{
	*qdev->tx_pipe.read_notify = cpu_to_le32(1);
}

static void gunyah_clr_tx_notify(struct qrtr_gunyah_dev *qdev)
{
	*qdev->tx_pipe.read_notify = 0;
}

static bool gunyah_get_read_notify(struct qrtr_gunyah_dev *qdev)
{
	return le32_to_cpu(*qdev->rx_pipe.read_notify);
}

static void gunyah_wait_for_tx_avail(struct qrtr_gunyah_dev *qdev)
{
	gunyah_set_tx_notify(qdev);
	wait_event_timeout(qdev->tx_avail_notify,
			   gunyah_tx_avail(&qdev->tx_pipe), 10 * HZ);
}

/* from qrtr to gunyah */
static int qrtr_gunyah_send(struct qrtr_endpoint *ep, struct sk_buff *skb)
{
	struct qrtr_gunyah_dev *qdev;
	size_t tx_avail;
	int chunk_size;
	int left_size;
	int offset;

	int rc;

	qdev = container_of(ep, struct qrtr_gunyah_dev, ep);

	rc = skb_linearize(skb);
	if (rc) {
		kfree_skb(skb);
		return rc;
	}

	left_size = skb->len;
	offset = 0;
	while (left_size > 0) {
		tx_avail = gunyah_tx_avail(&qdev->tx_pipe);
		if (!tx_avail) {
			gunyah_wait_for_tx_avail(qdev);
			continue;
		}
		if (tx_avail < left_size)
			chunk_size = tx_avail;
		else
			chunk_size = left_size;

		gunyah_tx_write(&qdev->tx_pipe, skb->data + offset, chunk_size);
		offset += chunk_size;
		left_size -= chunk_size;

		qrtr_gunyah_kick(qdev);
	}
	gunyah_clr_tx_notify(qdev);
	kfree_skb(skb);

	return 0;
}

static void qrtr_gunyah_read_new(struct qrtr_gunyah_dev *qdev)
{
	struct gunyah_ring *ring = &qdev->ring;
	size_t rx_avail;
	size_t pkt_len;
	u32 hdr[8];
	int rc;
	size_t hdr_len = sizeof(hdr);

	gunyah_rx_peak(&qdev->rx_pipe, &hdr, 0, hdr_len);
	pkt_len = qrtr_peek_pkt_size((void *)&hdr);
	if ((int)pkt_len < 0 || pkt_len > MAX_PKT_SZ) {
		dev_err(qdev->dev, "invalid pkt_len %zu\n", pkt_len);
		return;
	}

	rx_avail = gunyah_rx_avail(&qdev->rx_pipe);
	if (rx_avail > pkt_len)
		rx_avail = pkt_len;

	gunyah_rx_peak(&qdev->rx_pipe, ring->buf, 0, rx_avail);
	gunyah_rx_advance(&qdev->rx_pipe, rx_avail);

	if (rx_avail == pkt_len) {
		rc = qrtr_endpoint_post(&qdev->ep, ring->buf, pkt_len);
		if (rc == -EINVAL)
			dev_err(qdev->dev, "invalid ipcrouter packet\n");
	} else {
		ring->len = pkt_len;
		ring->offset = rx_avail;
	}
}

static void qrtr_gunyah_read_frag(struct qrtr_gunyah_dev *qdev)
{
	struct gunyah_ring *ring = &qdev->ring;
	size_t rx_avail;
	int rc;

	rx_avail = gunyah_rx_avail(&qdev->rx_pipe);
	if (rx_avail + ring->offset > ring->len)
		rx_avail = ring->len - ring->offset;

	gunyah_rx_peak(&qdev->rx_pipe, ring->buf + ring->offset, 0, rx_avail);
	gunyah_rx_advance(&qdev->rx_pipe, rx_avail);

	if (rx_avail + ring->offset == ring->len) {
		rc = qrtr_endpoint_post(&qdev->ep, ring->buf, ring->len);
		if (rc == -EINVAL)
			dev_err(qdev->dev, "invalid ipcrouter packet\n");
		ring->offset = 0;
		ring->len = 0;
	} else {
		ring->offset += rx_avail;
	}
}

static void qrtr_gunyah_read(struct qrtr_gunyah_dev *qdev)
{
	wake_up_all(&qdev->tx_avail_notify);

	while (gunyah_rx_avail(&qdev->rx_pipe)) {
		if (qdev->ring.offset)
			qrtr_gunyah_read_frag(qdev);
		else
			qrtr_gunyah_read_new(qdev);

		if (gunyah_get_read_notify(qdev))
			qrtr_gunyah_kick(qdev);
	}
}

static int qrtr_gunyah_share_mem(struct qrtr_gunyah_dev *qdev, gh_vmid_t self,
				 gh_vmid_t peer)
{
	u32 src_vmlist[1] = {self};
	int src_perms[2] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int dst_vmlist[2] = {self, peer};
	int dst_perms[2] = {PERM_READ | PERM_WRITE, PERM_READ | PERM_WRITE};
	struct gh_acl_desc *acl;
	struct gh_sgl_desc *sgl;
	int ret;

	ret = hyp_assign_phys(qdev->res.start, resource_size(&qdev->res),
			      src_vmlist, 1,
			      dst_vmlist, dst_perms, 2);
	if (ret) {
		pr_err("%s: hyp_assign_phys failed addr=%x size=%u err=%d\n",
		       __func__, qdev->res.start, qdev->size, ret);
		return ret;
	}

	acl = kzalloc(offsetof(struct gh_acl_desc, acl_entries[2]), GFP_KERNEL);
	if (!acl)
		return -ENOMEM;
	sgl = kzalloc(offsetof(struct gh_sgl_desc, sgl_entries[1]), GFP_KERNEL);
	if (!sgl) {
		kfree(acl);
		return -ENOMEM;
	}
	acl->n_acl_entries = 2;
	acl->acl_entries[0].vmid = (u16)self;
	acl->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;
	acl->acl_entries[1].vmid = (u16)peer;
	acl->acl_entries[1].perms = GH_RM_ACL_R | GH_RM_ACL_W;

	sgl->n_sgl_entries = 1;
	sgl->sgl_entries[0].ipa_base = qdev->res.start;
	sgl->sgl_entries[0].size = resource_size(&qdev->res);

	ret = gh_rm_mem_share(GH_RM_MEM_TYPE_NORMAL, 0, qdev->label,
			      acl, sgl, NULL, &qdev->memparcel);
	if (ret) {
		pr_err("%s: gh_rm_mem_share failed addr=%x size=%u err=%d\n",
		       __func__, qdev->res.start, qdev->size, ret);
		/* Attempt to give resource back to HLOS */
		hyp_assign_phys(qdev->res.start, resource_size(&qdev->res),
				dst_vmlist, 2,
				src_vmlist, src_perms, 1);
	}

	kfree(acl);
	kfree(sgl);

	return ret;
}

static void qrtr_gunyah_unshare_mem(struct qrtr_gunyah_dev *qdev,
				    gh_vmid_t self, gh_vmid_t peer)
{
	int dst_perms[2] = {PERM_READ | PERM_WRITE | PERM_EXEC};
	int src_vmlist[2] = {self, peer};
	u32 dst_vmlist[1] = {self};
	int ret;

	ret = gh_rm_mem_reclaim(qdev->memparcel, 0);
	if (ret)
		pr_err("%s: Gunyah reclaim failed\n", __func__);

	hyp_assign_phys(qdev->res.start, resource_size(&qdev->res),
			src_vmlist, 2, dst_vmlist, dst_perms, 1);
}

static int qrtr_gunyah_rm_cb(struct notifier_block *nb, unsigned long cmd,
			     void *data)
{
	struct gh_rm_notif_vm_status_payload *vm_status_payload;
	struct qrtr_gunyah_dev *qdev;
	gh_vmid_t peer_vmid;
	gh_vmid_t self_vmid;

	qdev = container_of(nb, struct qrtr_gunyah_dev, rm_nb);

	if (cmd != GH_RM_NOTIF_VM_STATUS)
		return NOTIFY_DONE;

	vm_status_payload = data;
	if (vm_status_payload->vm_status != GH_RM_VM_STATUS_READY &&
	    vm_status_payload->vm_status != GH_RM_VM_STATUS_RESET)
		return NOTIFY_DONE;
	if (gh_rm_get_vmid(qdev->peer_name, &peer_vmid))
		return NOTIFY_DONE;
	if (gh_rm_get_vmid(GH_PRIMARY_VM, &self_vmid))
		return NOTIFY_DONE;
	if (peer_vmid != vm_status_payload->vmid)
		return NOTIFY_DONE;

	if (vm_status_payload->vm_status == GH_RM_VM_STATUS_READY) {
		qrtr_gunyah_fifo_init(qdev);
		if (qrtr_endpoint_register(&qdev->ep, QRTR_EP_NET_ID_AUTO,
					   false, NULL)) {
			pr_err("%s: endpoint register failed\n", __func__);
			return NOTIFY_DONE;
		}
		if (qrtr_gunyah_share_mem(qdev, self_vmid, peer_vmid)) {
			pr_err("%s: failed to share memory\n", __func__);
			return NOTIFY_DONE;
		}
	}
	if (vm_status_payload->vm_status == GH_RM_VM_STATUS_RESET) {
		qrtr_endpoint_unregister(&qdev->ep);
		qrtr_gunyah_unshare_mem(qdev, self_vmid, peer_vmid);
	}

	return NOTIFY_DONE;
}

/**
 * qrtr_gunyah_fifo_init() - init gunyah xprt configs
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called to initialize the gunyah XPRT pointer with
 * the gunyah XPRT configurations either from device tree or static arrays.
 */
static void qrtr_gunyah_fifo_init(struct qrtr_gunyah_dev *qdev)
{
	__le32 *descs;

	if (qdev->master)
		memset(qdev->base, 0, sizeof(*descs) * 10);

	descs = qdev->base;
	descs[GUNYAH_MAGIC_IDX] = GUNYAH_MAGIC_KEY;

	if (qdev->master) {
		qdev->tx_pipe.tail = &descs[TAIL_0_IDX];
		qdev->tx_pipe.head = &descs[HEAD_0_IDX];
		qdev->tx_pipe.fifo = qdev->base + FIFO_0_START;
		qdev->tx_pipe.length = FIFO_SIZE;
		qdev->tx_pipe.read_notify = &descs[NOTIFY_0_IDX];

		qdev->rx_pipe.tail = &descs[TAIL_1_IDX];
		qdev->rx_pipe.head = &descs[HEAD_1_IDX];
		qdev->rx_pipe.fifo = qdev->base + FIFO_1_START;
		qdev->rx_pipe.length = FIFO_SIZE;
		qdev->rx_pipe.read_notify = &descs[NOTIFY_1_IDX];
	} else {
		qdev->tx_pipe.tail = &descs[TAIL_1_IDX];
		qdev->tx_pipe.head = &descs[HEAD_1_IDX];
		qdev->tx_pipe.fifo = qdev->base + FIFO_1_START;
		qdev->tx_pipe.length = FIFO_SIZE;
		qdev->tx_pipe.read_notify = &descs[NOTIFY_1_IDX];

		qdev->rx_pipe.tail = &descs[TAIL_0_IDX];
		qdev->rx_pipe.head = &descs[HEAD_0_IDX];
		qdev->rx_pipe.fifo = qdev->base + FIFO_0_START;
		qdev->rx_pipe.length = FIFO_SIZE;
		qdev->rx_pipe.read_notify = &descs[NOTIFY_0_IDX];
	}

	/* Reset respective index */
	*qdev->tx_pipe.head = 0;
	*qdev->tx_pipe.read_notify = 0;
	*qdev->rx_pipe.tail = 0;
}

static struct device_node *qrtr_gunyah_svm_of_parse(struct qrtr_gunyah_dev *qdev)
{
	const char *compat = "qcom,qrtr-gunyah-gen";
	struct device_node *np = NULL;
	struct device_node *shm_np;
	u32 label;
	int ret;

	while ((np = of_find_compatible_node(np, NULL, compat))) {
		ret = of_property_read_u32(np, "qcom,label", &label);
		if (ret) {
			of_node_put(np);
			continue;
		}
		if (label == qdev->label)
			break;

		of_node_put(np);
	}
	if (!np)
		return NULL;

	shm_np = of_parse_phandle(np, "memory-region", 0);
	if (!shm_np)
		dev_err(qdev->dev, "cant parse svm shared mem node!\n");

	of_node_put(np);
	return shm_np;
}

static int qrtr_gunyah_map_memory(struct qrtr_gunyah_dev *qdev)
{
	struct device *dev = qdev->dev;
	struct device_node *np;
	resource_size_t size;
	int ret;

	np = of_parse_phandle(dev->of_node, "shared-buffer", 0);
	if (!np) {
		np = qrtr_gunyah_svm_of_parse(qdev);
		if (!np) {
			dev_err(dev, "cant parse shared mem node!\n");
			return -EINVAL;
		}
	}

	ret = of_address_to_resource(np, 0, &qdev->res);
	of_node_put(np);
	if (ret) {
		dev_err(dev, "of_address_to_resource failed!\n");
		return -EINVAL;
	}
	size = resource_size(&qdev->res);

	qdev->base = devm_ioremap_resource(dev, &qdev->res);
	if (IS_ERR(qdev->base)) {
		dev_err(dev, "ioremap failed!\n");
		return PTR_ERR(qdev->base);
	}
	qdev->size = size;

	return 0;
}

/**
 * qrtr_gunyah_probe() - Probe a gunyah xprt
 *
 * @pdev: Platform device corresponding to gunyah xprt.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to a gunyah transport.
 */
static int qrtr_gunyah_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct qrtr_gunyah_dev *qdev;
	enum gh_dbl_label dbl_label;
	int ret;

	qdev = devm_kzalloc(&pdev->dev, sizeof(*qdev), GFP_KERNEL);
	if (!qdev)
		return -ENOMEM;
	qdev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, qdev);

	qdev->ring.buf = devm_kzalloc(&pdev->dev, MAX_PKT_SZ, GFP_KERNEL);
	if (!qdev->ring.buf)
		return -ENOMEM;

	ret = of_property_read_u32(node, "gunyah-label", &qdev->label);
	if (ret) {
		dev_err(qdev->dev, "failed to read label info %d\n", ret);
		return ret;
	}
	qdev->master = of_property_read_bool(node, "qcom,master");

	ret = qrtr_gunyah_map_memory(qdev);
	if (ret)
		return ret;

	if (!qdev->master)
		qrtr_gunyah_fifo_init(qdev);
	init_waitqueue_head(&qdev->tx_avail_notify);

	if (qdev->master) {
		ret = of_property_read_u32(node, "peer-name", &qdev->peer_name);
		if (ret)
			qdev->peer_name = GH_SELF_VM;

		qdev->rm_nb.notifier_call = qrtr_gunyah_rm_cb;
		qdev->rm_nb.priority = INT_MAX;
		gh_rm_register_notifier(&qdev->rm_nb);
	}

	dbl_label = qdev->label;
	qdev->tx_dbl = gh_dbl_tx_register(dbl_label);
	if (IS_ERR_OR_NULL(qdev->tx_dbl)) {
		ret = PTR_ERR(qdev->tx_dbl);
		dev_err(qdev->dev, "failed to get gunyah tx dbl %d\n", ret);
		return ret;
	}
	INIT_WORK(&qdev->work, qrtr_gunyah_retry_work);

	qdev->ep.xmit = qrtr_gunyah_send;
	if (!qdev->master) {
		ret = qrtr_endpoint_register(&qdev->ep, QRTR_EP_NET_ID_AUTO,
					     false, NULL);
		if (ret)
			goto register_fail;
	}

	qdev->rx_dbl = gh_dbl_rx_register(dbl_label, qrtr_gunyah_cb, qdev);
	if (IS_ERR_OR_NULL(qdev->rx_dbl)) {
		ret = PTR_ERR(qdev->rx_dbl);
		dev_err(qdev->dev, "failed to get gunyah rx dbl %d\n", ret);
		goto fail_rx_dbl;
	}

	if (!qdev->master && gunyah_rx_avail(&qdev->rx_pipe))
		qrtr_gunyah_read(qdev);

	return 0;

fail_rx_dbl:
	qrtr_endpoint_unregister(&qdev->ep);
register_fail:
	cancel_work_sync(&qdev->work);
	gh_dbl_tx_unregister(qdev->tx_dbl);

	return ret;
}

static int qrtr_gunyah_remove(struct platform_device *pdev)
{
	struct qrtr_gunyah_dev *qdev = dev_get_drvdata(&pdev->dev);

	cancel_work_sync(&qdev->work);
	gh_dbl_tx_unregister(qdev->tx_dbl);
	gh_dbl_rx_unregister(qdev->rx_dbl);

	return 0;
}

static const struct of_device_id qrtr_gunyah_match_table[] = {
	{ .compatible = "qcom,qrtr-gunyah" },
	{}
};

static struct platform_driver qrtr_gunyah_driver = {
	.driver = {
		.name = "qcom_gunyah_qrtr",
		.of_match_table = qrtr_gunyah_match_table,
	 },
	.probe = qrtr_gunyah_probe,
	.remove = qrtr_gunyah_remove,
};
module_platform_driver(qrtr_gunyah_driver);

MODULE_DESCRIPTION("QTI IPC-Router Gunyah interface driver");
MODULE_LICENSE("GPL v2");
