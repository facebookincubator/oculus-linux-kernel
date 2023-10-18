// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/mhi.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/of.h>
#include <net/sock.h>

#include "qrtr.h"

struct qrtr_mhi_dev {
	struct qrtr_endpoint ep;
	struct mhi_device *mhi_dev;
	struct device *dev;
	struct completion prepared;
};

/* From MHI to QRTR */
static void qcom_mhi_qrtr_dl_callback(struct mhi_device *mhi_dev,
				      struct mhi_result *mhi_res)
{
	struct qrtr_mhi_dev *qdev = dev_get_drvdata(&mhi_dev->dev);
	int rc;

	if (!qdev || mhi_res->transaction_status)
		return;

	rc = qrtr_endpoint_post(&qdev->ep, mhi_res->buf_addr,
				mhi_res->bytes_xferd);
	if (rc == -EINVAL)
		dev_err(qdev->dev, "invalid ipcrouter packet\n");
}

/* From QRTR to MHI */
static void qcom_mhi_qrtr_ul_callback(struct mhi_device *mhi_dev,
				      struct mhi_result *mhi_res)
{
	struct sk_buff *skb = mhi_res->buf_addr;

	if (skb->sk)
		sock_put(skb->sk);
	consume_skb(skb);
}

/* Send data over MHI */
static int __qcom_mhi_qrtr_send(struct qrtr_endpoint *ep, struct sk_buff *skb)
{
	struct qrtr_mhi_dev *qdev = container_of(ep, struct qrtr_mhi_dev, ep);
	int rc;

	if (skb->sk)
		sock_hold(skb->sk);

	rc = wait_for_completion_interruptible(&qdev->prepared);
	if (rc)
		goto free_skb;

	rc = skb_linearize(skb);
	if (rc)
		goto free_skb;

	rc = mhi_queue_skb(qdev->mhi_dev, DMA_TO_DEVICE, skb, skb->len,
			   MHI_EOT);
	if (rc && rc != -EAGAIN)
		goto free_skb;

	return rc;

free_skb:
	if (skb->sk)
		sock_put(skb->sk);
	kfree_skb(skb);

	return rc;
}

static int qcom_mhi_qrtr_send(struct qrtr_endpoint *ep, struct sk_buff *skb)
{
	int rc;

	do {
		rc = __qcom_mhi_qrtr_send(ep, skb);
		usleep_range(1000, 2000);
	} while (rc == -EAGAIN);

	return rc;
}

static void qrtr_mhi_of_parse(struct mhi_device *mhi_dev,
			      u32 *net_id, bool *rt)
{
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	struct device_node *np = NULL;
	struct pci_dev *pci_device;
	u32 dev_id, nid;
	int rc;

	*net_id = QRTR_EP_NET_ID_AUTO;
	*rt = false;

	np = of_find_compatible_node(np, NULL, "qcom,qrtr-mhi");
	if (!np)
		return;

	rc = of_property_read_u32(np, "qcom,dev-id", &dev_id);
	if (!rc) {
		pci_device = to_pci_dev(mhi_cntrl->cntrl_dev);
		if (pci_device->device == dev_id) {
			rc = of_property_read_u32(np, "qcom,net-id", &nid);
			if (!rc)
				*net_id = nid;
			*rt = of_property_read_bool(np, "qcom,low-latency");
		}
	}
	of_node_put(np);
}

static int qcom_mhi_qrtr_probe(struct mhi_device *mhi_dev,
			       const struct mhi_device_id *id)
{
	struct qrtr_mhi_dev *qdev;
	u32 net_id;
	bool rt;
	int rc;

	qdev = devm_kzalloc(&mhi_dev->dev, sizeof(*qdev), GFP_KERNEL);
	if (!qdev)
		return -ENOMEM;

	qdev->mhi_dev = mhi_dev;
	qdev->dev = &mhi_dev->dev;
	qdev->ep.xmit = qcom_mhi_qrtr_send;
	init_completion(&qdev->prepared);

	dev_set_drvdata(&mhi_dev->dev, qdev);

	qrtr_mhi_of_parse(mhi_dev, &net_id, &rt);

	rc = qrtr_endpoint_register(&qdev->ep, net_id, rt, NULL);
	if (rc)
		return rc;

	/* start channels */
	rc = mhi_prepare_for_transfer(mhi_dev);
	if (rc) {
		qrtr_endpoint_unregister(&qdev->ep);
		dev_set_drvdata(&mhi_dev->dev, NULL);
		return rc;
	}

	complete_all(&qdev->prepared);
	dev_dbg(qdev->dev, "Qualcomm MHI QRTR driver probed\n");

	return 0;
}

static void qcom_mhi_qrtr_remove(struct mhi_device *mhi_dev)
{
	struct qrtr_mhi_dev *qdev = dev_get_drvdata(&mhi_dev->dev);

	qrtr_endpoint_unregister(&qdev->ep);
	mhi_unprepare_from_transfer(mhi_dev);
	dev_set_drvdata(&mhi_dev->dev, NULL);
}

static const struct mhi_device_id qcom_mhi_qrtr_id_table[] = {
	{ .chan = "IPCR" },
	{}
};
MODULE_DEVICE_TABLE(mhi, qcom_mhi_qrtr_id_table);

static struct mhi_driver qcom_mhi_qrtr_driver = {
	.probe = qcom_mhi_qrtr_probe,
	.remove = qcom_mhi_qrtr_remove,
	.dl_xfer_cb = qcom_mhi_qrtr_dl_callback,
	.ul_xfer_cb = qcom_mhi_qrtr_ul_callback,
	.id_table = qcom_mhi_qrtr_id_table,
	.driver = {
		.name = "qcom_mhi_qrtr",
	},
};

module_mhi_driver(qcom_mhi_qrtr_driver);

MODULE_AUTHOR("Chris Lew <clew@codeaurora.org>");
MODULE_AUTHOR("Manivannan Sadhasivam <manivannan.sadhasivam@linaro.org>");
MODULE_DESCRIPTION("Qualcomm IPC-Router MHI interface driver");
MODULE_LICENSE("GPL v2");
