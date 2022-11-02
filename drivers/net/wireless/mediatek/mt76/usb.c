/*
 * Copyright (C) 2018 Lorenzo Bianconi <lorenzo.bianconi83@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "mt76.h"
#include "usb_trace.h"
#include "dma.h"

#define MT_VEND_REQ_MAX_RETRY	10
#define MT_VEND_REQ_TOUT_MS	300

/* should be called with usb_ctrl_mtx locked */
static int __mt76u_vendor_request(struct mt76_dev *dev, u8 req,
				  u8 req_type, u16 val, u16 offset,
				  void *buf, size_t len)
{
	struct usb_interface *intf = to_usb_interface(dev->dev);
	struct usb_device *udev = interface_to_usbdev(intf);
	unsigned int pipe;
	int i, ret;

	pipe = (req_type & USB_DIR_IN) ? usb_rcvctrlpipe(udev, 0)
				       : usb_sndctrlpipe(udev, 0);
	for (i = 0; i < MT_VEND_REQ_MAX_RETRY; i++) {
		if (test_bit(MT76_REMOVED, &dev->state))
			return -EIO;

		ret = usb_control_msg(udev, pipe, req, req_type, val,
				      offset, buf, len, MT_VEND_REQ_TOUT_MS);
		if (ret == -ENODEV)
			set_bit(MT76_REMOVED, &dev->state);
		if (ret >= 0 || ret == -ENODEV)
			return ret;
		usleep_range(5000, 10000);
	}

	dev_err(dev->dev, "vendor request req:%02x off:%04x failed:%d\n",
		req, offset, ret);
	return ret;
}

int mt76u_vendor_request(struct mt76_dev *dev, u8 req,
			 u8 req_type, u16 val, u16 offset,
			 void *buf, size_t len)
{
	int ret;

	mutex_lock(&dev->usb.usb_ctrl_mtx);
	ret = __mt76u_vendor_request(dev, req, req_type,
				     val, offset, buf, len);
	trace_usb_reg_wr(dev, offset, val);
	mutex_unlock(&dev->usb.usb_ctrl_mtx);

	return ret;
}
EXPORT_SYMBOL_GPL(mt76u_vendor_request);

/* should be called with usb_ctrl_mtx locked */
static u32 __mt76u_rr(struct mt76_dev *dev, u32 addr)
{
	struct mt76_usb *usb = &dev->usb;
	u32 data = ~0;
	u16 offset;
	int ret;
	u8 req;

	switch (addr & MT_VEND_TYPE_MASK) {
	case MT_VEND_TYPE_EEPROM:
		req = MT_VEND_READ_EEPROM;
		break;
	case MT_VEND_TYPE_CFG:
		req = MT_VEND_READ_CFG;
		break;
	default:
		req = MT_VEND_MULTI_READ;
		break;
	}
	offset = addr & ~MT_VEND_TYPE_MASK;

	ret = __mt76u_vendor_request(dev, req,
				     USB_DIR_IN | USB_TYPE_VENDOR,
				     0, offset, usb->data, sizeof(__le32));
	if (ret == sizeof(__le32))
		data = get_unaligned_le32(usb->data);
	trace_usb_reg_rr(dev, addr, data);

	return data;
}

u32 mt76u_rr(struct mt76_dev *dev, u32 addr)
{
	u32 ret;

	mutex_lock(&dev->usb.usb_ctrl_mtx);
	ret = __mt76u_rr(dev, addr);
	mutex_unlock(&dev->usb.usb_ctrl_mtx);

	return ret;
}

/* should be called with usb_ctrl_mtx locked */
static void __mt76u_wr(struct mt76_dev *dev, u32 addr, u32 val)
{
	struct mt76_usb *usb = &dev->usb;
	u16 offset;
	u8 req;

	switch (addr & MT_VEND_TYPE_MASK) {
	case MT_VEND_TYPE_CFG:
		req = MT_VEND_WRITE_CFG;
		break;
	default:
		req = MT_VEND_MULTI_WRITE;
		break;
	}
	offset = addr & ~MT_VEND_TYPE_MASK;

	put_unaligned_le32(val, usb->data);
	__mt76u_vendor_request(dev, req,
			       USB_DIR_OUT | USB_TYPE_VENDOR, 0,
			       offset, usb->data, sizeof(__le32));
	trace_usb_reg_wr(dev, addr, val);
}

void mt76u_wr(struct mt76_dev *dev, u32 addr, u32 val)
{
	mutex_lock(&dev->usb.usb_ctrl_mtx);
	__mt76u_wr(dev, addr, val);
	mutex_unlock(&dev->usb.usb_ctrl_mtx);
}

static u32 mt76u_rmw(struct mt76_dev *dev, u32 addr,
		     u32 mask, u32 val)
{
	mutex_lock(&dev->usb.usb_ctrl_mtx);
	val |= __mt76u_rr(dev, addr) & ~mask;
	__mt76u_wr(dev, addr, val);
	mutex_unlock(&dev->usb.usb_ctrl_mtx);

	return val;
}

static void mt76u_copy(struct mt76_dev *dev, u32 offset,
		       const void *data, int len)
{
	struct mt76_usb *usb = &dev->usb;
	const u32 *val = data;
	int i, ret;

	mutex_lock(&usb->usb_ctrl_mtx);
	for (i = 0; i < (len / 4); i++) {
		put_unaligned_le32(val[i], usb->data);
		ret = __mt76u_vendor_request(dev, MT_VEND_MULTI_WRITE,
					     USB_DIR_OUT | USB_TYPE_VENDOR,
					     0, offset + i * 4, usb->data,
					     sizeof(__le32));
		if (ret < 0)
			break;
	}
	mutex_unlock(&usb->usb_ctrl_mtx);
}

void mt76u_single_wr(struct mt76_dev *dev, const u8 req,
		     const u16 offset, const u32 val)
{
	mutex_lock(&dev->usb.usb_ctrl_mtx);
	__mt76u_vendor_request(dev, req,
			       USB_DIR_OUT | USB_TYPE_VENDOR,
			       val & 0xffff, offset, NULL, 0);
	__mt76u_vendor_request(dev, req,
			       USB_DIR_OUT | USB_TYPE_VENDOR,
			       val >> 16, offset + 2, NULL, 0);
	mutex_unlock(&dev->usb.usb_ctrl_mtx);
}
EXPORT_SYMBOL_GPL(mt76u_single_wr);

static int
mt76u_set_endpoints(struct usb_interface *intf,
		    struct mt76_usb *usb)
{
	struct usb_host_interface *intf_desc = intf->cur_altsetting;
	struct usb_endpoint_descriptor *ep_desc;
	int i, in_ep = 0, out_ep = 0;

	for (i = 0; i < intf_desc->desc.bNumEndpoints; i++) {
		ep_desc = &intf_desc->endpoint[i].desc;

		if (usb_endpoint_is_bulk_in(ep_desc) &&
		    in_ep < __MT_EP_IN_MAX) {
			usb->in_ep[in_ep] = usb_endpoint_num(ep_desc);
			usb->in_max_packet = usb_endpoint_maxp(ep_desc);
			in_ep++;
		} else if (usb_endpoint_is_bulk_out(ep_desc) &&
			   out_ep < __MT_EP_OUT_MAX) {
			usb->out_ep[out_ep] = usb_endpoint_num(ep_desc);
			usb->out_max_packet = usb_endpoint_maxp(ep_desc);
			out_ep++;
		}
	}

	if (in_ep != __MT_EP_IN_MAX || out_ep != __MT_EP_OUT_MAX)
		return -EINVAL;
	return 0;
}

static int
mt76u_fill_rx_sg(struct mt76_dev *dev, struct mt76u_buf *buf,
		 int nsgs, int len, int sglen)
{
	struct urb *urb = buf->urb;
	int i;

	for (i = 0; i < nsgs; i++) {
		struct page *page;
		void *data;
		int offset;

		data = netdev_alloc_frag(len);
		if (!data)
			break;

		page = virt_to_head_page(data);
		offset = data - page_address(page);
		sg_set_page(&urb->sg[i], page, sglen, offset);
	}

	if (i < nsgs) {
		int j;

		for (j = nsgs; j < urb->num_sgs; j++)
			skb_free_frag(sg_virt(&urb->sg[j]));
		urb->num_sgs = i;
	}

	urb->num_sgs = max_t(int, i, urb->num_sgs);
	buf->len = urb->num_sgs * sglen,
	sg_init_marker(urb->sg, urb->num_sgs);

	return i ? : -ENOMEM;
}

int mt76u_buf_alloc(struct mt76_dev *dev, struct mt76u_buf *buf,
		    int nsgs, int len, int sglen, gfp_t gfp)
{
	buf->urb = usb_alloc_urb(0, gfp);
	if (!buf->urb)
		return -ENOMEM;

	buf->urb->sg = devm_kcalloc(dev->dev, nsgs, sizeof(*buf->urb->sg),
				    gfp);
	if (!buf->urb->sg)
		return -ENOMEM;

	sg_init_table(buf->urb->sg, nsgs);
	buf->dev = dev;

	return mt76u_fill_rx_sg(dev, buf, nsgs, len, sglen);
}
EXPORT_SYMBOL_GPL(mt76u_buf_alloc);

void mt76u_buf_free(struct mt76u_buf *buf)
{
	struct urb *urb = buf->urb;
	int i;

	for (i = 0; i < urb->num_sgs; i++)
		skb_free_frag(sg_virt(&urb->sg[i]));
	usb_free_urb(buf->urb);
}
EXPORT_SYMBOL_GPL(mt76u_buf_free);

int mt76u_submit_buf(struct mt76_dev *dev, int dir, int index,
		     struct mt76u_buf *buf, gfp_t gfp,
		     usb_complete_t complete_fn, void *context)
{
	struct usb_interface *intf = to_usb_interface(dev->dev);
	struct usb_device *udev = interface_to_usbdev(intf);
	unsigned int pipe;

	if (dir == USB_DIR_IN)
		pipe = usb_rcvbulkpipe(udev, dev->usb.in_ep[index]);
	else
		pipe = usb_sndbulkpipe(udev, dev->usb.out_ep[index]);

	usb_fill_bulk_urb(buf->urb, udev, pipe, NULL, buf->len,
			  complete_fn, context);

	return usb_submit_urb(buf->urb, gfp);
}
EXPORT_SYMBOL_GPL(mt76u_submit_buf);

static inline struct mt76u_buf
*mt76u_get_next_rx_entry(struct mt76_queue *q)
{
	struct mt76u_buf *buf = NULL;
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);
	if (q->queued > 0) {
		buf = &q->entry[q->head].ubuf;
		q->head = (q->head + 1) % q->ndesc;
		q->queued--;
	}
	spin_unlock_irqrestore(&q->lock, flags);

	return buf;
}

static int mt76u_get_rx_entry_len(u8 *data, u32 data_len)
{
	u16 dma_len, min_len;

	dma_len = get_unaligned_le16(data);
	min_len = MT_DMA_HDR_LEN + MT_RX_RXWI_LEN +
		  MT_FCE_INFO_LEN;

	if (data_len < min_len || WARN_ON(!dma_len) ||
	    WARN_ON(dma_len + MT_DMA_HDR_LEN > data_len) ||
	    WARN_ON(dma_len & 0x3))
		return -EINVAL;
	return dma_len;
}

static int
mt76u_process_rx_entry(struct mt76_dev *dev, struct urb *urb)
{
	struct mt76_queue *q = &dev->q_rx[MT_RXQ_MAIN];
	u8 *data = sg_virt(&urb->sg[0]);
	int data_len, len, nsgs = 1;
	struct sk_buff *skb;

	if (!test_bit(MT76_STATE_INITIALIZED, &dev->state))
		return 0;

	len = mt76u_get_rx_entry_len(data, urb->actual_length);
	if (len < 0)
		return 0;

	skb = build_skb(data, q->buf_size);
	if (!skb)
		return 0;

	data_len = min_t(int, len, urb->sg[0].length - MT_DMA_HDR_LEN);
	skb_reserve(skb, MT_DMA_HDR_LEN);
	if (skb->tail + data_len > skb->end) {
		dev_kfree_skb(skb);
		return 1;
	}

	__skb_put(skb, data_len);
	len -= data_len;

	while (len > 0) {
		data_len = min_t(int, len, urb->sg[nsgs].length);
		skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
				sg_page(&urb->sg[nsgs]),
				urb->sg[nsgs].offset,
				data_len, q->buf_size);
		len -= data_len;
		nsgs++;
	}
	dev->drv->rx_skb(dev, MT_RXQ_MAIN, skb);

	return nsgs;
}

static void mt76u_complete_rx(struct urb *urb)
{
	struct mt76_dev *dev = urb->context;
	struct mt76_queue *q = &dev->q_rx[MT_RXQ_MAIN];
	unsigned long flags;

	switch (urb->status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
	case -ENOENT:
		return;
	default:
		dev_err(dev->dev, "rx urb failed: %d\n", urb->status);
		/* fall through */
	case 0:
		break;
	}

	spin_lock_irqsave(&q->lock, flags);
	if (WARN_ONCE(q->entry[q->tail].ubuf.urb != urb, "rx urb mismatch"))
		goto out;

	q->tail = (q->tail + 1) % q->ndesc;
	q->queued++;
	tasklet_schedule(&dev->usb.rx_tasklet);
out:
	spin_unlock_irqrestore(&q->lock, flags);
}

static void mt76u_rx_tasklet(unsigned long data)
{
	struct mt76_dev *dev = (struct mt76_dev *)data;
	struct mt76_queue *q = &dev->q_rx[MT_RXQ_MAIN];
	int err, nsgs, buf_len = q->buf_size;
	struct mt76u_buf *buf;

	rcu_read_lock();

	while (true) {
		buf = mt76u_get_next_rx_entry(q);
		if (!buf)
			break;

		nsgs = mt76u_process_rx_entry(dev, buf->urb);
		if (nsgs > 0) {
			err = mt76u_fill_rx_sg(dev, buf, nsgs,
					       buf_len,
					       SKB_WITH_OVERHEAD(buf_len));
			if (err < 0)
				break;
		}
		mt76u_submit_buf(dev, USB_DIR_IN, MT_EP_IN_PKT_RX,
				 buf, GFP_ATOMIC,
				 mt76u_complete_rx, dev);
	}
	mt76_rx_poll_complete(dev, MT_RXQ_MAIN, NULL);

	rcu_read_unlock();
}

int mt76u_submit_rx_buffers(struct mt76_dev *dev)
{
	struct mt76_queue *q = &dev->q_rx[MT_RXQ_MAIN];
	unsigned long flags;
	int i, err = 0;

	spin_lock_irqsave(&q->lock, flags);
	for (i = 0; i < q->ndesc; i++) {
		err = mt76u_submit_buf(dev, USB_DIR_IN, MT_EP_IN_PKT_RX,
				       &q->entry[i].ubuf, GFP_ATOMIC,
				       mt76u_complete_rx, dev);
		if (err < 0)
			break;
	}
	q->head = q->tail = 0;
	q->queued = 0;
	spin_unlock_irqrestore(&q->lock, flags);

	return err;
}
EXPORT_SYMBOL_GPL(mt76u_submit_rx_buffers);

static int mt76u_alloc_rx(struct mt76_dev *dev)
{
	struct mt76_queue *q = &dev->q_rx[MT_RXQ_MAIN];
	int i, err, nsgs;

	spin_lock_init(&q->lock);
	q->entry = devm_kcalloc(dev->dev,
				MT_NUM_RX_ENTRIES, sizeof(*q->entry),
				GFP_KERNEL);
	if (!q->entry)
		return -ENOMEM;

	if (mt76u_check_sg(dev)) {
		q->buf_size = MT_RX_BUF_SIZE;
		nsgs = MT_SG_MAX_SIZE;
	} else {
		q->buf_size = PAGE_SIZE;
		nsgs = 1;
	}

	for (i = 0; i < MT_NUM_RX_ENTRIES; i++) {
		err = mt76u_buf_alloc(dev, &q->entry[i].ubuf,
				      nsgs, q->buf_size,
				      SKB_WITH_OVERHEAD(q->buf_size),
				      GFP_KERNEL);
		if (err < 0)
			return err;
	}
	q->ndesc = MT_NUM_RX_ENTRIES;

	return mt76u_submit_rx_buffers(dev);
}

static void mt76u_free_rx(struct mt76_dev *dev)
{
	struct mt76_queue *q = &dev->q_rx[MT_RXQ_MAIN];
	int i;

	for (i = 0; i < q->ndesc; i++)
		mt76u_buf_free(&q->entry[i].ubuf);
}

static void mt76u_stop_rx(struct mt76_dev *dev)
{
	struct mt76_queue *q = &dev->q_rx[MT_RXQ_MAIN];
	int i;

	for (i = 0; i < q->ndesc; i++)
		usb_kill_urb(q->entry[i].ubuf.urb);
}

int mt76u_skb_dma_info(struct sk_buff *skb, int port, u32 flags)
{
	struct sk_buff *iter, *last = skb;
	u32 info, pad;

	/* Buffer layout:
	 *	|   4B   | xfer len |      pad       |  4B  |
	 *	| TXINFO | pkt/cmd  | zero pad to 4B | zero |
	 *
	 * length field of TXINFO should be set to 'xfer len'.
	 */
	info = FIELD_PREP(MT_TXD_INFO_LEN, round_up(skb->len, 4)) |
	       FIELD_PREP(MT_TXD_INFO_DPORT, port) | flags;
	put_unaligned_le32(info, skb_push(skb, sizeof(info)));

	pad = round_up(skb->len, 4) + 4 - skb->len;
	skb_walk_frags(skb, iter) {
		last = iter;
		if (!iter->next) {
			skb->data_len += pad;
			skb->len += pad;
			break;
		}
	}

	if (unlikely(pad)) {
		if (__skb_pad(last, pad, true))
			return -ENOMEM;
		__skb_put(last, pad);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mt76u_skb_dma_info);

static void mt76u_tx_tasklet(unsigned long data)
{
	struct mt76_dev *dev = (struct mt76_dev *)data;
	struct mt76u_buf *buf;
	struct mt76_queue *q;
	bool wake;
	int i;

	for (i = 0; i < IEEE80211_NUM_ACS; i++) {
		q = &dev->q_tx[i];

		spin_lock_bh(&q->lock);
		while (true) {
			buf = &q->entry[q->head].ubuf;
			if (!buf->done || !q->queued)
				break;

			dev->drv->tx_complete_skb(dev, q,
						  &q->entry[q->head],
						  false);

			if (q->entry[q->head].schedule) {
				q->entry[q->head].schedule = false;
				q->swq_queued--;
			}

			q->head = (q->head + 1) % q->ndesc;
			q->queued--;
		}
		mt76_txq_schedule(dev, q);
		wake = i < IEEE80211_NUM_ACS && q->queued < q->ndesc - 8;
		if (!q->queued)
			wake_up(&dev->tx_wait);

		spin_unlock_bh(&q->lock);

		if (!test_and_set_bit(MT76_READING_STATS, &dev->state))
			ieee80211_queue_delayed_work(dev->hw,
						     &dev->usb.stat_work,
						     msecs_to_jiffies(10));

		if (wake)
			ieee80211_wake_queue(dev->hw, i);
	}
}

static void mt76u_tx_status_data(struct work_struct *work)
{
	struct mt76_usb *usb;
	struct mt76_dev *dev;
	u8 update = 1;
	u16 count = 0;

	usb = container_of(work, struct mt76_usb, stat_work.work);
	dev = container_of(usb, struct mt76_dev, usb);

	while (true) {
		if (test_bit(MT76_REMOVED, &dev->state))
			break;

		if (!dev->drv->tx_status_data(dev, &update))
			break;
		count++;
	}

	if (count && test_bit(MT76_STATE_RUNNING, &dev->state))
		ieee80211_queue_delayed_work(dev->hw, &usb->stat_work,
					     msecs_to_jiffies(10));
	else
		clear_bit(MT76_READING_STATS, &dev->state);
}

static void mt76u_complete_tx(struct urb *urb)
{
	struct mt76u_buf *buf = urb->context;
	struct mt76_dev *dev = buf->dev;

	if (mt76u_urb_error(urb))
		dev_err(dev->dev, "tx urb failed: %d\n", urb->status);
	buf->done = true;

	tasklet_schedule(&dev->usb.tx_tasklet);
}

static int
mt76u_tx_build_sg(struct sk_buff *skb, struct urb *urb)
{
	int nsgs = 1 + skb_shinfo(skb)->nr_frags;
	struct sk_buff *iter;

	skb_walk_frags(skb, iter)
		nsgs += 1 + skb_shinfo(iter)->nr_frags;

	memset(urb->sg, 0, sizeof(*urb->sg) * MT_SG_MAX_SIZE);

	nsgs = min_t(int, MT_SG_MAX_SIZE, nsgs);
	sg_init_marker(urb->sg, nsgs);
	urb->num_sgs = nsgs;

	return skb_to_sgvec_nomark(skb, urb->sg, 0, skb->len);
}

static int
mt76u_tx_queue_skb(struct mt76_dev *dev, struct mt76_queue *q,
		   struct sk_buff *skb, struct mt76_wcid *wcid,
		   struct ieee80211_sta *sta)
{
	struct usb_interface *intf = to_usb_interface(dev->dev);
	struct usb_device *udev = interface_to_usbdev(intf);
	u8 ep = q2ep(q->hw_idx);
	struct mt76u_buf *buf;
	u16 idx = q->tail;
	unsigned int pipe;
	int err;

	if (q->queued == q->ndesc)
		return -ENOSPC;

	err = dev->drv->tx_prepare_skb(dev, NULL, skb, q, wcid, sta, NULL);
	if (err < 0)
		return err;

	buf = &q->entry[idx].ubuf;
	buf->done = false;

	err = mt76u_tx_build_sg(skb, buf->urb);
	if (err < 0)
		return err;

	pipe = usb_sndbulkpipe(udev, dev->usb.out_ep[ep]);
	usb_fill_bulk_urb(buf->urb, udev, pipe, NULL, skb->len,
			  mt76u_complete_tx, buf);

	q->tail = (q->tail + 1) % q->ndesc;
	q->entry[idx].skb = skb;
	q->queued++;

	return idx;
}

static void mt76u_tx_kick(struct mt76_dev *dev, struct mt76_queue *q)
{
	struct mt76u_buf *buf;
	int err;

	while (q->first != q->tail) {
		buf = &q->entry[q->first].ubuf;
		err = usb_submit_urb(buf->urb, GFP_ATOMIC);
		if (err < 0) {
			if (err == -ENODEV)
				set_bit(MT76_REMOVED, &dev->state);
			else
				dev_err(dev->dev, "tx urb submit failed:%d\n",
					err);
			break;
		}
		q->first = (q->first + 1) % q->ndesc;
	}
}

static int mt76u_alloc_tx(struct mt76_dev *dev)
{
	struct mt76u_buf *buf;
	struct mt76_queue *q;
	size_t size;
	int i, j;

	size = MT_SG_MAX_SIZE * sizeof(struct scatterlist);
	for (i = 0; i < IEEE80211_NUM_ACS; i++) {
		q = &dev->q_tx[i];
		spin_lock_init(&q->lock);
		INIT_LIST_HEAD(&q->swq);
		q->hw_idx = q2hwq(i);

		q->entry = devm_kcalloc(dev->dev,
					MT_NUM_TX_ENTRIES, sizeof(*q->entry),
					GFP_KERNEL);
		if (!q->entry)
			return -ENOMEM;

		q->ndesc = MT_NUM_TX_ENTRIES;
		for (j = 0; j < q->ndesc; j++) {
			buf = &q->entry[j].ubuf;
			buf->dev = dev;

			buf->urb = usb_alloc_urb(0, GFP_KERNEL);
			if (!buf->urb)
				return -ENOMEM;

			buf->urb->sg = devm_kzalloc(dev->dev, size, GFP_KERNEL);
			if (!buf->urb->sg)
				return -ENOMEM;
		}
	}
	return 0;
}

static void mt76u_free_tx(struct mt76_dev *dev)
{
	struct mt76_queue *q;
	int i, j;

	for (i = 0; i < IEEE80211_NUM_ACS; i++) {
		q = &dev->q_tx[i];
		for (j = 0; j < q->ndesc; j++)
			usb_free_urb(q->entry[j].ubuf.urb);
	}
}

static void mt76u_stop_tx(struct mt76_dev *dev)
{
	struct mt76_queue *q;
	int i, j;

	for (i = 0; i < IEEE80211_NUM_ACS; i++) {
		q = &dev->q_tx[i];
		for (j = 0; j < q->ndesc; j++)
			usb_kill_urb(q->entry[j].ubuf.urb);
	}
}

void mt76u_stop_queues(struct mt76_dev *dev)
{
	tasklet_disable(&dev->usb.rx_tasklet);
	tasklet_disable(&dev->usb.tx_tasklet);

	mt76u_stop_rx(dev);
	mt76u_stop_tx(dev);
}
EXPORT_SYMBOL_GPL(mt76u_stop_queues);

void mt76u_stop_stat_wk(struct mt76_dev *dev)
{
	cancel_delayed_work_sync(&dev->usb.stat_work);
	clear_bit(MT76_READING_STATS, &dev->state);
}
EXPORT_SYMBOL_GPL(mt76u_stop_stat_wk);

void mt76u_queues_deinit(struct mt76_dev *dev)
{
	mt76u_stop_queues(dev);

	mt76u_free_rx(dev);
	mt76u_free_tx(dev);
}
EXPORT_SYMBOL_GPL(mt76u_queues_deinit);

int mt76u_alloc_queues(struct mt76_dev *dev)
{
	int err;

	err = mt76u_alloc_rx(dev);
	if (err < 0)
		return err;

	return mt76u_alloc_tx(dev);
}
EXPORT_SYMBOL_GPL(mt76u_alloc_queues);

static const struct mt76_queue_ops usb_queue_ops = {
	.tx_queue_skb = mt76u_tx_queue_skb,
	.kick = mt76u_tx_kick,
};

int mt76u_init(struct mt76_dev *dev,
	       struct usb_interface *intf)
{
	static const struct mt76_bus_ops mt76u_ops = {
		.rr = mt76u_rr,
		.wr = mt76u_wr,
		.rmw = mt76u_rmw,
		.copy = mt76u_copy,
	};
	struct mt76_usb *usb = &dev->usb;

	tasklet_init(&usb->rx_tasklet, mt76u_rx_tasklet, (unsigned long)dev);
	tasklet_init(&usb->tx_tasklet, mt76u_tx_tasklet, (unsigned long)dev);
	INIT_DELAYED_WORK(&usb->stat_work, mt76u_tx_status_data);
	skb_queue_head_init(&dev->rx_skb[MT_RXQ_MAIN]);

	init_completion(&usb->mcu.cmpl);
	mutex_init(&usb->mcu.mutex);

	mutex_init(&usb->usb_ctrl_mtx);
	dev->bus = &mt76u_ops;
	dev->queue_ops = &usb_queue_ops;

	return mt76u_set_endpoints(intf, usb);
}
EXPORT_SYMBOL_GPL(mt76u_init);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi83@gmail.com>");
MODULE_LICENSE("Dual BSD/GPL");
