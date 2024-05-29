// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020-2021 The Linux Foundation. All rights reserved. */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/nospec.h>
#include <linux/kthread.h>
#include <linux/neuron.h>
#include <asm-generic/barrier.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_dbl.h>
#include <soc/qcom/secure_buffer.h>
#include "ch_mq_shmem_common.h"

#define CHANNEL_VERSION NEURON_SHMEM_CHANNEL_V1
#define CH_DBL_MASK 0x1

/* Write data to the ring buffer */
static inline void ring_write_msg(struct neuron_msg_queue *msgq,
				  struct buffer_list src, size_t n)
{
	off_t offset = msgq->offset;
	struct neuron_msg_hdr *hdr;
	void *dest;

	hdr = (struct neuron_msg_hdr *)(msgq->ring_buffer_p + offset);

	/* Size index is from 0. */
	hdr->size = n - 1;

	offset = round_up(offset + PACKET_HEADER_SIZE,
			  msgq->message_alignment);
	if (offset >= msgq->ring_buffer_len)
		offset -= msgq->ring_buffer_len;

	dest = msgq->ring_buffer_p + offset;

	if (offset + n <= msgq->ring_buffer_len) {
		skb_copy_bits(src.head, src.offset, dest, n);
	} else {
		size_t n_1 = msgq->ring_buffer_len - offset;
		size_t n_2 = n - n_1;

		skb_copy_bits(src.head, src.offset, dest, n_1);
		skb_copy_bits(src.head, src.offset + n_1, msgq->ring_buffer_p,
			      n_2);
	}

	offset = round_up(offset + n, msgq->message_alignment);
	if (offset >= msgq->ring_buffer_len)
		offset -= msgq->ring_buffer_len;

	msgq->offset = offset;
}

static inline int channel_gh_kick(struct neuron_mq_data_priv *priv)
{
	gh_dbl_flags_t dbl_mask = CH_DBL_MASK;
	int ret;

	ret = gh_dbl_send(priv->tx_dbl, &dbl_mask, 0);
	if (ret)
		pr_err("failed to raise virq to the sender %d\n", ret);

	return ret;
}

static int channel_gh_sendv(struct neuron_channel *channel_dev,
			    struct buffer_list buf)
{
	size_t space, space_needed, len;
	off_t tail, head;
	int prev_empty = 0;

	struct neuron_mq_data_priv *priv = dev_get_drvdata(&channel_dev->dev);
	struct neuron_msg_queue *msgq = &priv->msgq;

	/* read shared variable from memory */
	if (unlikely(!smp_load_acquire(&priv->synced)))
		return -EAGAIN;

	len = buf.size;

	/* Nothing to send */
	if (len == 0) {
		dev_err(&channel_dev->dev, "len is 0\n");
		return -EINVAL;
	}

	if (len > channel_dev->max_size) {
		dev_err(&channel_dev->dev,
			"buffer is oversized - len is %zd and %lu is supported\n",
			len, channel_dev->max_size);
		return -EMSGSIZE;
	}

	space_needed =
		round_up(PACKET_HEADER_SIZE, msgq->message_alignment) +
		round_up(len, msgq->message_alignment);
	WRITE_ONCE(*msgq->space_for_next_p, space_needed);

	/* Order the space_for_next write before the tail read */
	mb();

	/* Get the tail from the shared memory */
	tail = smp_load_acquire(msgq->tailp);
	head = msgq->offset;

	/* The receiver rebooting has been detected. */
	if (unlikely(tail >= msgq->ring_buffer_len)) {
		dev_warn(&channel_dev->dev,
			 "The receiver rebooted. Start to sync again!\n");
		return -ECONNRESET;
	}

	if (head < tail)
		space = tail - head - 1;
	else
		space = msgq->ring_buffer_len + tail - head - 1;

	if (space < space_needed)
		return -EAGAIN;

	prev_empty = msgq->offset == head ? 1 : 0;

	ring_write_msg(msgq, buf, len);

	/* Publish to the shared header */
	smp_store_release(msgq->headp, msgq->offset);

	if (prev_empty)
		channel_gh_kick(priv);

	return 0;
}

static int channel_gh_send(struct neuron_channel *channel_dev,
			   struct sk_buff *skb)
{
	struct buffer_list buf = {
		.head = skb,
		.offset = 0,
		.size = skb->len
	};

	return channel_gh_sendv(channel_dev, buf);
}

static void channel_gh_cb(int irq, void *data)
{
	struct neuron_mq_data_priv *priv = data;

	wake_up(&priv->wait_q);
	neuron_channel_wakeup(priv->dev);
}

static int read_config(struct neuron_mq_data_priv *priv)
{
	int ret;
	u32 offset;
	struct neuron_shmem_channel_header *hdr = priv->base;
	struct neuron_msg_queue *msgq = &priv->msgq;
	struct neuron_channel *channel = priv->dev;

	/* Clear local offset. */
	msgq->offset = 0;
	/* Get message_alignment value. */
	msgq->message_alignment = hdr->message_alignment;
	/* Get ring_buffer_len value. */
	msgq->ring_buffer_len = hdr->ring_buffer_len;
	if (msgq->ring_buffer_len > resource_size(&priv->buffer))
		return -ECONNRESET;
	/* Get tail_offset, making sure the value is valid. */
	offset = hdr->tail_offset;
	if (offset > resource_size(&priv->buffer))
		return -ECONNRESET;
	offset = array_index_nospec(offset, resource_size(&priv->buffer));
	msgq->tailp = (u32 *)((u8 *)hdr + offset);

	msgq->space_for_next_p = &hdr->space_for_next;
	/* Get ring_buffer_offset, making sure the value is valid*/
	offset = hdr->ring_buffer_offset;
	if (offset > resource_size(&priv->buffer))
		return -ECONNRESET;
	offset = array_index_nospec(offset, resource_size(&priv->buffer));
	msgq->ring_buffer_p = (void *)round_up((uintptr_t)hdr + offset,
					       msgq->message_alignment);

	/* send channel max message size is:
	 * 1. From device tree definition if it is not bigger than the receiver
	 *    max message size. Or
	 * 2. The receiver max message size if not defined.
	 */
	if (channel->max_size && channel->max_size > hdr->max_msg_size) {
		dev_err(&channel->dev,
			"send max message size is bigger than receiver max message size!\n");
		return -EINVAL;
	} else if (channel->max_size == 0) {
		channel->max_size = hdr->max_msg_size;
	}

	ret = channel_set_limits(channel, msgq);
	if (ret < 0)
		return ret;

	/* write shared variable to memory */
	smp_store_release(&priv->synced, 1);

	return 0;
}

static void msgq_init(struct neuron_mq_data_priv *priv)
{
	struct neuron_shmem_channel_header *hdr = priv->base;
	struct neuron_msg_queue *msgq = &priv->msgq;

	msgq->headp = &hdr->head;
	msgq->space_for_next_p = &hdr->space_for_next;
	msgq->offset = (u32)-1;

	/* Set it to -1 as UNINITIALIZED */
	smp_store_release(msgq->headp, (u32)-1);
}

/* Thread to sync with the receiver. Note: this thread might never finishes if
 * it fails to sync with the peer.
 */
static int channel_sync_thread(void *data)
{
	struct neuron_mq_data_priv *priv = (struct neuron_mq_data_priv *)data;
	struct neuron_shmem_channel_header *hdr;

	msgq_init(priv);
	hdr = (struct neuron_shmem_channel_header *)priv->base;
	hdr->version = CHANNEL_VERSION;
	hdr->tail_offset = -1;

	/* Make sure the memory writing is in order. */
	mb();
	hdr->head = -1;
	channel_gh_kick(priv);

	/* Waiting for tail_offset being updated by the send. */
	wait_event_killable(priv->wait_q, kthread_should_stop() ||
			    /* read shared variable from memory */
			    smp_load_acquire(&hdr->tail_offset) != -1);

	if (kthread_should_stop())
		return 0;

	/* If the version doesn't match (it could also be case the peer is of
	 * version without version field), quit the sync.
	 */
	if (smp_load_acquire(&hdr->version) != CHANNEL_VERSION) {
		pr_err("Mismatched channel version: Me: %u, Peer: %u\n",
		       CHANNEL_VERSION, hdr->version);
		return -EPROTO;
	}
	/* Clear head offset to 0 */
	smp_store_release(&hdr->head, 0);

	if (read_config(priv)) {
		pr_err("config failed\n");
		return -EINVAL;
	}

	channel_gh_kick(priv);

	neuron_channel_wakeup(priv->dev);

	return 0;
}

static int channel_gh_share_mem(struct neuron_mq_data_priv *priv,
				gh_vmid_t self, gh_vmid_t peer)
{
	u32 src_vmlist[1] = {self};
	int dst_vmlist[2] = {self, peer};
	int dst_perms[2] = {PERM_READ | PERM_WRITE, PERM_READ | PERM_WRITE};
	struct gh_acl_desc *acl;
	struct gh_sgl_desc *sgl;
	int ret;

	ret = hyp_assign_phys(priv->buffer.start, resource_size(&priv->buffer),
			      src_vmlist, 1,
			      dst_vmlist, dst_perms, 2);
	if (ret) {
		pr_err("hyp_assign_phys failed addr=%x size=%u err=%d\n",
		       priv->buffer.start, resource_size(&priv->buffer), ret);
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
	sgl->sgl_entries[0].ipa_base = priv->buffer.start;
	sgl->sgl_entries[0].size = resource_size(&priv->buffer);
	ret = gh_rm_mem_qcom_lookup_sgl(GH_RM_MEM_TYPE_NORMAL,
					priv->gunyah_label,
					acl, sgl, NULL,
					&priv->shm_memparcel);
	kfree(acl);
	kfree(sgl);

	return ret;
}

static int channel_gh_rm_cb(struct notifier_block *nb, unsigned long cmd,
			    void *data)
{
	struct gh_rm_notif_vm_status_payload *vm_status_payload = data;
	struct neuron_mq_data_priv *priv;
	gh_vmid_t peer_vmid;
	gh_vmid_t self_vmid;

	priv = container_of(nb, struct neuron_mq_data_priv, rm_nb);
	if (cmd != GH_RM_NOTIF_VM_STATUS)
		return NOTIFY_DONE;

	if (vm_status_payload->vm_status != GH_RM_VM_STATUS_READY)
		return NOTIFY_DONE;
	if (gh_rm_get_vmid(priv->peer_name, &peer_vmid))
		return NOTIFY_DONE;
	if (gh_rm_get_vmid(GH_PRIMARY_VM, &self_vmid))
		return NOTIFY_DONE;
	if (peer_vmid != vm_status_payload->vmid)
		return NOTIFY_DONE;

	if (channel_gh_share_mem(priv, self_vmid, peer_vmid))
		pr_err("%s: failed to share memory\n", __func__);

	return NOTIFY_DONE;
}

static struct device_node *
channel_gh_svm_of_parse(struct neuron_mq_data_priv *priv, struct device *dev)
{
	const char *compat = "qcom,neuron-channel-gunyah-shmem-gen";
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
		if (label == priv->gunyah_label)
			break;

		of_node_put(np);
	}
	if (!np)
		return NULL;

	shm_np = of_parse_phandle(np, "memory-region", 0);
	if (!shm_np)
		dev_err(dev, "cant parse svm shared mem node!\n");

	of_node_put(np);
	return shm_np;
}

static int channel_gh_map_memory(struct neuron_mq_data_priv *priv,
				 struct device *dev)
{
	struct device_node *np;
	resource_size_t size;
	struct resource *r;
	int ret;

	np = of_parse_phandle(dev->of_node, "shared-buffer", 0);
	if (!np) {
		np = channel_gh_svm_of_parse(priv, dev);
		if (!np) {
			dev_err(dev, "cant parse shared mem node!\n");
			return -EINVAL;
		}
	}

	ret = of_address_to_resource(np, 0, &priv->buffer);
	of_node_put(np);
	if (ret) {
		dev_err(dev, "of_address_to_resource failed!\n");
		return -EINVAL;
	}
	size = resource_size(&priv->buffer);

	/* buffer parameters checking */
	if (!priv->buffer.start || size % PAGE_SIZE) {
		dev_err(dev, "invalid memory region: start:%llx, size:%llx\n",
			priv->buffer.start, size);
		return -EINVAL;
	}

	r = devm_request_mem_region(dev, priv->buffer.start, size,
				    dev_name(dev));
	if (!r) {
		dev_err(dev, "request memory region failed!\n");
		return -ENXIO;
	}
	priv->base = ioremap_cache(priv->buffer.start, size);
	if (!priv->base) {
		dev_err(dev, "ioremap failed!\n");
		return -ENXIO;
	}

	if (of_property_read_bool(dev->of_node, "qcom,primary")) {
		memset(priv->base, 0,
		       sizeof(struct neuron_shmem_channel_header));

		ret = of_property_read_u32(dev->of_node, "peer-name",
					   &priv->peer_name);
		if (ret)
			priv->peer_name = GH_SELF_VM;

		priv->rm_nb.notifier_call = channel_gh_rm_cb;
		priv->rm_nb.priority = INT_MAX;
		gh_rm_register_notifier(&priv->rm_nb);
	}

	return 0;
}

static int channel_gh_probe(struct neuron_channel *cdev)
{
	struct device_node *node = cdev->dev.of_node;
	struct device *dev = &cdev->dev;
	struct neuron_mq_data_priv *priv;
	enum gh_dbl_label dbl_label;
	int ret;

	if (!node)
		return -ENODEV;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->dev = cdev;
	init_waitqueue_head(&priv->wait_q);

	ret = of_property_read_u32(node, "gunyah-label", &priv->gunyah_label);
	if (ret) {
		dev_err(dev, "failed to read label info %d\n", ret);
		return ret;
	}

	ret = channel_gh_map_memory(priv, dev);
	if (ret)
		return ret;

	/* Get outgoing gunyah doorbell information */
	dbl_label = priv->gunyah_label;
	priv->tx_dbl = gh_dbl_tx_register(dbl_label);
	if (IS_ERR_OR_NULL(priv->tx_dbl)) {
		ret = PTR_ERR(priv->tx_dbl);
		dev_err(dev, "failed to get gunyah tx dbl %d\n", ret);
		goto fail_tx_dbl;
	}

	priv->rx_dbl = gh_dbl_rx_register(dbl_label, channel_gh_cb, priv);
	if (IS_ERR_OR_NULL(priv->rx_dbl)) {
		ret = PTR_ERR(priv->rx_dbl);
		dev_err(dev, "failed to get gunyah rx dbl %d\n", ret);
		goto fail_rx_dbl;
	}
	/* Start the thread for syncing with the receiver. */
	priv->sync_thread = kthread_run(channel_sync_thread, priv,
					"send_sync_thread");

	dev_set_drvdata(dev, priv);
	return 0;

fail_rx_dbl:
	gh_dbl_tx_unregister(priv->tx_dbl);
fail_tx_dbl:
	iounmap(priv->base);
	return ret;
}

static void channel_gh_remove(struct neuron_channel *cdev)
{
	struct neuron_mq_data_priv *priv = dev_get_drvdata(&cdev->dev);

	/* Stop it anyway. */
	kthread_stop(priv->sync_thread);
	iounmap(priv->base);
	devm_release_mem_region(&cdev->dev, priv->buffer.start,
				resource_size(&priv->buffer));

	gh_dbl_tx_unregister(priv->tx_dbl);
	gh_dbl_rx_unregister(priv->rx_dbl);
}

static const struct of_device_id channel_gh_match[] = {
	{ .compatible = "qcom,neuron-channel-gunyah-shmem" },
	{},
};
MODULE_DEVICE_TABLE(of, channel_gh_match);

static struct neuron_channel_driver channel_gh_send_driver = {
	.driver = {
		.name = "ch_gunyah_send",
		.of_match_table = channel_gh_match,
	},
	.type = NEURON_CHANNEL_MESSAGE_QUEUE,
	.direction = NEURON_CHANNEL_SEND,
	.send_msgv = channel_gh_sendv,
	.send_msg = channel_gh_send,
	.probe  = channel_gh_probe,
	.remove = channel_gh_remove,
};

static int __init channel_gh_init(void)
{
	int ret;

	ret = neuron_register_channel_driver(&channel_gh_send_driver);
	if (ret < 0) {
		pr_err("Failed to register driver: %d\n", ret);
		return ret;
	}

	return 0;
}

static void __exit channel_gh_exit(void)
{
	neuron_unregister_channel_driver(&channel_gh_send_driver);
}
module_init(channel_gh_init);
module_exit(channel_gh_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Neuron channel gunyah shared memory send driver");
