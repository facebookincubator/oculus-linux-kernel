// SPDX-License-Identifier: GPL+
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/cypd.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/usb/pd.h>
#include <linux/usb/pd_vdo.h>

#include "cypd.h"

#define PD_MSG_HDR_IS_EXTENDED(hdr)	((hdr) & PD_HEADER_EXT_HDR)

/* Timeouts */
#define SENDER_RESPONSE_TIME	26

#define SVDM_HDR(svid, ver, obj, cmd_type, cmd) \
	(((svid) << 16) | (1 << 15) | ((ver) << 13) \
	| ((obj) << 8) | ((cmd_type) << 6) | (cmd))
#define IS_DATA(m, t) ((m) && !PD_MSG_HDR_IS_EXTENDED((m)->hdr) && \
		pd_header_cnt((m)->hdr) && \
		(pd_header_type((m)->hdr) == (t)))
#define IS_CTRL(m, t) ((m) && !pd_header_cnt((m)->hdr) && \
		(pd_header_type((m)->hdr) == (t)))
#define IS_EXT(m, t) ((m) && PD_MSG_HDR_IS_EXTENDED((m)->hdr) && \
		(pd_header_type((m)->hdr) == (t)))

struct vdm_tx {
	u32			data[PD_MAX_PAYLOAD];
	int			size;
};

struct rx_msg {
	u16			hdr;
	u16			data_len;	/* size of payload in bytes */
	struct list_head	entry;
	u8			payload[];
};

enum cypd_state {
	PE_UNKNOWN,
	PE_SNK_READY,
	PE_DISCONNECT,
	PE_MAX_STATES,
};

static const char * const cypd_state_strings[] = {
	"UNKNOWN",
	"SNK_Ready",
	"PD_Disconnect",
};

enum vdm_state {
	VDM_NONE,
	DISCOVERED_ID,
	DISCOVERED_SVIDS,
	DISCOVERED_MODES,
	MODE_ENTERED,
	MODE_EXITED,
};

struct cypd {
	struct device			dev;
	struct workqueue_struct	*wq;
	struct work_struct	sm_work;
	struct work_struct	psy_changed_work;
	bool			sm_queued;
	struct hrtimer			timer;
	enum cypd_state		current_state;
	bool					pd_connected;
	struct power_supply		*cypd_psy;
	struct notifier_block	psy_nb;
	enum typec_data_role		current_dr;
	enum typec_role		current_pr;
	int					spec_rev;
	u8			tx_msgid[SOPII_MSG + 1];
	u8			rx_msgid[SOPII_MSG + 1];
	struct list_head	rx_q;
	spinlock_t		rx_lock;

	/* VDM related fields start here */
	enum vdm_state		vdm_state;
	u16			*discovered_svids;
	int			num_svids;
	u16 pid;
	struct vdm_tx		*vdm_tx;
	struct vdm_tx		*vdm_tx_retry;
	struct mutex		svid_handler_lock;
	struct list_head	svid_handlers;
	ktime_t			svdm_start_time;
	bool			vdm_in_suspend;
};

struct vdm_work {
	struct cypd *pd;
	struct work_struct work;
};

struct cypd_state_handler {
	void (*enter_state)(struct cypd *pd);
	void (*handle_state)(struct cypd *pd, struct rx_msg *msg);
};

static const struct cypd_state_handler state_handlers[];
static void handle_vdm_rx(struct cypd *pd, struct rx_msg *rx_msg);
static void handle_vdm_tx(struct cypd *pd, enum pd_sop_type sop_type);
static void cypd_set_state(struct cypd *pd, enum cypd_state next_state);
static int pd_send_msg(struct cypd *pd, u8 msg_type, const u32 *data,
		size_t num_data, enum pd_sop_type sop);
static const char *msg_to_string(u8 id, bool is_data, bool is_ext);
static void cypd_startup_common(struct cypd *pd);
static int queue_vdm_open_work(struct cypd *pd);
static int queue_vdm_close_work(struct cypd *pd);

static void kick_sm(struct cypd *pd, int ms)
{
	pm_stay_awake(&pd->dev);
	pd->sm_queued = true;

	if (ms) {
		dev_dbg(&pd->dev, "delay %d ms", ms);
		hrtimer_start(&pd->timer, ms_to_ktime(ms), HRTIMER_MODE_REL);
	} else {
		queue_work(pd->wq, &pd->sm_work);
	}
}

/* Enters new state and executes actions on entry */
static void cypd_set_state(struct cypd *pd, enum cypd_state next_state)
{
	dev_dbg(&pd->dev, "%s -> %s\n",
			cypd_state_strings[pd->current_state],
			cypd_state_strings[next_state]);

	pd->current_state = next_state;

	if (pd->current_state < PE_MAX_STATES &&
		state_handlers[pd->current_state].enter_state)
		state_handlers[pd->current_state].enter_state(pd);
	else
		dev_dbg(&pd->dev, "No action for state %s\n",
				cypd_state_strings[pd->current_state]);
}

static void handle_state_unknown(struct cypd *pd, struct rx_msg *rx_msg)
{
	dev_dbg(&pd->dev, "Unexpected state\n");
}

static void enter_state_snk_ready(struct cypd *pd)
{
	pd->current_dr = TYPEC_DEVICE;
	pd->current_pr = TYPEC_SINK;

	cypd_startup_common(pd);

	if (pd->vdm_tx)
		kick_sm(pd, 0);
	else if (pd->vdm_state == VDM_NONE)
		cypd_send_svdm(pd, USB_SID_PD,
				CMD_DISCOVER_IDENT,
				CMDT_INIT, 0, NULL, 0);
}

static bool handle_ctrl_snk_ready(struct cypd *pd, struct rx_msg *rx_msg)
{
	dev_dbg(&pd->dev, "Ctrl messages are not handled\n");
	return true;
}

static bool handle_ext_snk_ready(struct cypd *pd, struct rx_msg *rx_msg)
{
	dev_dbg(&pd->dev, "Ext messages are not handled\n");
	return true;
}

static bool handle_data_snk_ready(struct cypd *pd, struct rx_msg *rx_msg)
{
	u8 msg_type = pd_header_type(rx_msg->hdr);
	u8 num_objs = pd_header_cnt(rx_msg->hdr);

	switch (msg_type) {
	case PD_DATA_VENDOR_DEF:
		handle_vdm_rx(pd, rx_msg);
		break;
	default:
		dev_dbg(&pd->dev, "Unexpected message type %s received\n",
				msg_to_string(msg_type, num_objs,
					PD_MSG_HDR_IS_EXTENDED(rx_msg->hdr)));
		return false;
	}

	return true;
}

static void handle_snk_ready_tx(struct cypd *pd, struct rx_msg *rx_msg)
{
	handle_vdm_tx(pd, SOP_MSG);
}

static void handle_state_snk_ready(struct cypd *pd, struct rx_msg *rx_msg)
{
	int ret;

	if (rx_msg && !pd_header_cnt(rx_msg->hdr) &&
		handle_ctrl_snk_ready(pd, rx_msg)) {
		return;
	} else if (rx_msg && !PD_MSG_HDR_IS_EXTENDED(rx_msg->hdr) &&
		handle_data_snk_ready(pd, rx_msg)) {
		return;
	} else if (rx_msg && PD_MSG_HDR_IS_EXTENDED(rx_msg->hdr) &&
		handle_ext_snk_ready(pd, rx_msg)) {
		/* continue to handle tx */
	} else if (rx_msg && !IS_CTRL(rx_msg, PD_CTRL_NOT_SUPP)) {
		dev_dbg(&pd->dev, "Unsupported message\n");
		ret = pd_send_msg(pd, pd->spec_rev == PD_REV30 ?
				PD_CTRL_NOT_SUPP : PD_CTRL_REJECT,
				NULL, 0, SOP_MSG);
		if (ret)
			/* TODO : Handle return failure */
			dev_err(&pd->dev, "pd_send_msg failed\n");
		return;
	}

	handle_snk_ready_tx(pd, rx_msg);
}

static void rx_msg_cleanup(struct cypd *pd)
{
	struct rx_msg *msg, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&pd->rx_lock, flags);
	list_for_each_entry_safe(msg, tmp, &pd->rx_q, entry) {
		list_del(&msg->entry);
		kfree(msg);
	}
	spin_unlock_irqrestore(&pd->rx_lock, flags);
}

static void reset_vdm_state(struct cypd *pd)
{
	struct cypd_svid_handler *handler;

	/* in_interrupt() == true when handling VDM RX during suspend */
	if (!in_interrupt())
		mutex_lock(&pd->svid_handler_lock);

	list_for_each_entry(handler, &pd->svid_handlers, entry) {
		if (handler->discovered) {
			dev_dbg(&pd->dev, "Notify SVID: 0x%04x disconnect\n",
					handler->svid);
			handler->disconnect(handler);
			handler->discovered = false;
		}
	}

	if (!in_interrupt())
		mutex_unlock(&pd->svid_handler_lock);

	pd->num_svids = 0;
	kfree(pd->discovered_svids);
	pd->discovered_svids = NULL;
	pd->pid = 0;
	pd->vdm_state = VDM_NONE;
	kfree(pd->vdm_tx_retry);
	pd->vdm_tx_retry = NULL;
	kfree(pd->vdm_tx);
	pd->vdm_tx = NULL;
}

static void enter_state_disconnect(struct cypd *pd)
{
	dev_info(&pd->dev, "PD disconnect\n");

	queue_vdm_close_work(pd);
	rx_msg_cleanup(pd);
	reset_vdm_state(pd);

	pd->current_state = PE_UNKNOWN;

	/*
	 * first Rx ID should be 0; set this to a sentinel of -1 so that in
	 * phy_msg_received() we can check if we had seen it before.
	 */
	memset(pd->rx_msgid, -1, sizeof(pd->rx_msgid));
	memset(pd->tx_msgid, 0, sizeof(pd->tx_msgid));
}

static const struct cypd_state_handler state_handlers[] = {
	[PE_UNKNOWN] = {NULL, handle_state_unknown},
	[PE_SNK_READY] = {enter_state_snk_ready, handle_state_snk_ready},
	[PE_DISCONNECT] = {enter_state_disconnect, NULL},
};

static struct cypd_svid_handler *find_svid_handler(struct cypd *pd,
		u16 svid, u16 pid)
{
	struct cypd_svid_handler *handler;

	/* in_interrupt() == true when handling VDM RX during suspend */
	if (!in_interrupt())
		mutex_lock(&pd->svid_handler_lock);

	list_for_each_entry(handler, &pd->svid_handlers, entry) {
		if (svid == handler->svid)
			/* Treat a registered PID of 0 as a wildcard */
			if (!handler->pid || pid == handler->pid) {
				if (!in_interrupt())
					mutex_unlock(&pd->svid_handler_lock);
				return handler;
			}
	}

	if (!in_interrupt())
		mutex_unlock(&pd->svid_handler_lock);

	return NULL;
}

static void handle_vdm_resp_ack(struct cypd *pd, u32 *vdos, u8 num_vdos,
	u16 vdm_hdr)
{
	int i;
	u16 svid, *psvid;
	u8 cmd = PD_VDO_CMD(vdm_hdr);
	struct cypd_svid_handler *handler;

	switch (cmd) {
	case CMD_DISCOVER_IDENT:
		kfree(pd->vdm_tx_retry);
		pd->vdm_tx_retry = NULL;

		if (!num_vdos) {
			dev_dbg(&pd->dev, "Discarding Discover ID response with no VDOs\n");
			break;
		}

		if (PD_VDO_OPOS(vdm_hdr) != 0) {
			dev_dbg(&pd->dev, "Discarding Discover ID response with incorrect object position:%d\n",
					PD_VDO_OPOS(vdm_hdr));
			break;
		}

		if (num_vdos >= VDO_INDEX_PRODUCT)
			pd->pid = PD_PRODUCT_PID(vdos[VDO_INDEX_PRODUCT - 1]);

		pd->vdm_state = DISCOVERED_ID;
		cypd_send_svdm(pd, USB_SID_PD,
				CMD_DISCOVER_SVID,
				CMDT_INIT, 0, NULL, 0);
		break;

	case CMD_DISCOVER_SVID:
		pd->vdm_state = DISCOVERED_SVIDS;

		kfree(pd->vdm_tx_retry);
		pd->vdm_tx_retry = NULL;

		if (!pd->discovered_svids) {
			pd->num_svids = 2 * num_vdos;
			pd->discovered_svids = kcalloc(pd->num_svids,
							sizeof(u16),
							GFP_KERNEL);
			if (!pd->discovered_svids)
				break;

			psvid = pd->discovered_svids;
		} else { /* handle > 12 SVIDs */
			void *ptr;
			size_t oldsize = pd->num_svids * sizeof(u16);
			size_t newsize = oldsize +
					(2 * num_vdos * sizeof(u16));

			ptr = krealloc(pd->discovered_svids, newsize,
					GFP_KERNEL);
			if (!ptr)
				break;

			pd->discovered_svids = ptr;
			psvid = pd->discovered_svids + pd->num_svids;
			memset(psvid, 0, (2 * num_vdos));
			pd->num_svids += 2 * num_vdos;
		}

		/* convert 32-bit VDOs to list of 16-bit SVIDs */
		for (i = 0; i < num_vdos * 2; i++) {
			/*
			 * Within each 32-bit VDO,
			 *    SVID[i]: upper 16-bits
			 *    SVID[i+1]: lower 16-bits
			 * where i is even.
			 */
			if (!(i & 1))
				svid = vdos[i >> 1] >> 16;
			else
				svid = vdos[i >> 1] & 0xFFFF;

			/*
			 * There are some devices that incorrectly
			 * swap the order of SVIDs within a VDO. So in
			 * case of an odd-number of SVIDs it could end
			 * up with SVID[i] as 0 while SVID[i+1] is
			 * non-zero. Just skip over the zero ones.
			 */
			if (svid) {
				dev_dbg(&pd->dev, "Discovered SVID: 0x%04x\n",
						svid);
				*psvid++ = svid;
			}
		}

		/* if more than 12 SVIDs, resend the request */
		if (num_vdos == 6 && vdos[5] != 0) {
			cypd_send_svdm(pd, USB_SID_PD,
					CMD_DISCOVER_SVID,
					CMDT_INIT, 0,
					NULL, 0);
			break;
		}

		/* now that all SVIDs are discovered, notify handlers */
		for (i = 0; i < pd->num_svids; i++) {
			svid = pd->discovered_svids[i];
			if (svid) {
				handler = find_svid_handler(pd, svid, pd->pid);
				if (handler) {
					dev_dbg(&pd->dev,
							"Notify SVID/PID: 0x%04x/0x%04x connect\n",
							handler->svid, pd->pid);
					handler->connect(handler);
					handler->discovered = true;
				}
			}
		}
		break;

	default:
		dev_dbg(&pd->dev, "unhandled ACK for command:0x%x\n",
				cmd);
		break;
	}

}

static void handle_vdm_rx(struct cypd *pd, struct rx_msg *rx_msg)
{
	u32 vdm_hdr =
	rx_msg->data_len >= sizeof(u32) ? ((u32 *)rx_msg->payload)[0] : 0;

	u32 *vdos = (u32 *)&rx_msg->payload[sizeof(u32)];
	u16 svid = PD_VDO_VID(vdm_hdr);

	u8 num_vdos = pd_header_cnt(rx_msg->hdr) - 1;
	u8 cmd = PD_VDO_CMD(vdm_hdr);
	u8 cmd_type = PD_VDO_CMDT(vdm_hdr);
	struct cypd_svid_handler *handler;
	ktime_t recvd_time = ktime_get();

	dev_dbg(&pd->dev,
			"VDM rx: svid:%04x cmd:%x cmd_type:%x vdm_hdr:%x\n",
			svid, cmd, cmd_type, vdm_hdr);

	/* if it's a supported SVID, pass the message to the handler */
	handler = find_svid_handler(pd, svid, pd->pid);

	/* Unstructured VDM */
	if (!PD_VDO_SVDM(vdm_hdr)) {
		if (handler && handler->vdm_received)
			handler->vdm_received(handler, vdm_hdr, vdos, num_vdos);
		/* TODO: Handle pd->spec_rev == PD_REV30 */
		return;
	}

	if (handler && handler->svdm_received) {
		handler->svdm_received(handler, cmd, cmd_type, vdos, num_vdos);

		/* TODO: handle any previously queued TX */

		return;
	}

	/* Standard Discovery or unhandled messages go here */
	switch (cmd_type) {
	case CMDT_INIT:
		if (cmd != CMD_ATTENTION) {
			/* TODO: Handle pd->spec_rev == PD_REV30 */
			cypd_send_svdm(pd, svid, cmd,
					CMDT_RSP_NAK,
					PD_VDO_OPOS(vdm_hdr),
					NULL, 0);
		}
		break;

	case CMDT_RSP_ACK:
		if (svid != USB_SID_PD) {
			dev_err(&pd->dev, "unhandled ACK for SVID:0x%x\n",
					svid);
			break;
		}

		if (ktime_ms_delta(recvd_time, pd->svdm_start_time) >
				SENDER_RESPONSE_TIME) {
			dev_warn(&pd->dev, "SVDM response time %lld exceeded threshold %d\n",
				ktime_ms_delta(recvd_time, pd->svdm_start_time),
				SENDER_RESPONSE_TIME);
		}

		handle_vdm_resp_ack(pd, vdos, num_vdos, vdm_hdr);
		break;

	case CMDT_RSP_NAK:
		dev_info(&pd->dev, "VDM NAK received for SVID:0x%04x command:0x%x\n",
				svid, cmd);
		break;

	case CMDT_RSP_BUSY:
		switch (cmd) {
		case CMD_DISCOVER_IDENT:
		case CMD_DISCOVER_SVID:
			if (!pd->vdm_tx_retry) {
				dev_err(&pd->dev, "Discover command %d VDM was unexpectedly freed\n",
						cmd);
				break;
			}

			/* wait tVDMBusy, then retry */
			pd->vdm_tx = pd->vdm_tx_retry;
			pd->vdm_tx_retry = NULL;
			/* TODO: Handle VDM busy */
			break;
		default:
			break;
		}
		break;
	}
}

static const char * const cypd_control_msg_strings[] = {
	"", "GoodCRC", "GotoMin", "Accept", "Reject", "Ping", "PS_RDY",
	"Get_Source_Cap", "Get_Sink_Cap", "DR_Swap", "PR_Swap", "VCONN_Swap",
	"Wait", "Soft_Reset", "", "", "Not_Supported",
	"Get_Source_Cap_Extended", "Get_Status", "FR_Swap", "Get_PPS_Status",
	"Get_Country_Codes",
};

static const char * const cypd_data_msg_strings[] = {
	"", "Source_Capabilities", "Request", "BIST", "Sink_Capabilities",
	"Battery_Status", "Alert", "Get_Country_Info", "", "", "", "", "", "",
	"", "Vendor_Defined",
};

static const char * const cypd_ext_msg_strings[] = {
	"", "Source_Capabilities_Extended", "Status", "Get_Battery_Cap",
	"Get_Battery_Status", "Get_Manufacturer_Info", "Manufacturer_Info",
	"Security_Request", "Security_Response", "Firmware_Update_Request",
	"Firmware_Update_Response", "PPS_Status", "Country_Info",
	"Country_Codes",
};

static const char *msg_to_string(u8 id, bool is_data, bool is_ext)
{
	if (is_ext) {
		if (id < ARRAY_SIZE(cypd_ext_msg_strings))
			return cypd_ext_msg_strings[id];
	} else if (is_data) {
		if (id < ARRAY_SIZE(cypd_data_msg_strings))
			return cypd_data_msg_strings[id];
	} else if (id < ARRAY_SIZE(cypd_control_msg_strings)) {
		return cypd_control_msg_strings[id];
	}

	return "Invalid";
}

static void phy_msg_received(struct cypd *pd, enum pd_sop_type sop,
		u8 *buf, size_t len)
{
	struct rx_msg *rx_msg;
	unsigned long flags;
	u16 header;
	u8 msg_type, num_objs;

	if (sop == SOPII_MSG) {
		dev_err(&pd->dev, "only SOP/SOP' supported\n");
		return;
	}

	if (len < 2) {
		dev_err(&pd->dev, "invalid message received, len=%zd\n", len);
		return;
	}

	header = *((u16 *)buf);
	buf += sizeof(u16);
	len -= sizeof(u16);

	if (len % 4 != 0) {
		dev_err(&pd->dev, "len=%zd not multiple of 4\n", len);
		return;
	}

	/* if MSGID already seen, discard */
	if (pd_header_msgid(header) == pd->rx_msgid[sop]) {
		dev_dbg(&pd->dev, "MessageID already seen, discarding\n");
		return;
	}

	pd->rx_msgid[sop] = pd_header_msgid(header);

	/* check header's count field to see if it matches len */
	if (pd_header_cnt(header) != (len / 4)) {
		dev_err(&pd->dev, "header count (%d) mismatch, len=%zd\n",
				pd_header_cnt(header), len);
		return;
	}

	msg_type = pd_header_type(header);
	num_objs = pd_header_cnt(header);
	dev_dbg(&pd->dev, "%s type(%d) num_objs(%d)\n",
			msg_to_string(msg_type, num_objs,
				PD_MSG_HDR_IS_EXTENDED(header)),
			msg_type, num_objs);

	if (msg_type != PD_DATA_VENDOR_DEF)
		return;

	rx_msg = kzalloc(sizeof(*rx_msg) + len, GFP_ATOMIC);
	if (!rx_msg)
		return;

	rx_msg->hdr = header;
	rx_msg->data_len = len;
	memcpy(rx_msg->payload, buf, len);

	if (pd->vdm_in_suspend) {
		dev_dbg(&pd->dev, "Skip wq and handle VDM directly\n");
		handle_vdm_rx(pd, rx_msg);
		kfree(rx_msg);
	}

	spin_lock_irqsave(&pd->rx_lock, flags);
	list_add_tail(&rx_msg->entry, &pd->rx_q);
	spin_unlock_irqrestore(&pd->rx_lock, flags);

	if (!work_busy(&pd->sm_work))
		kick_sm(pd, 0);
	else
		dev_dbg(&pd->dev, "cypd_sm already running\n");
}

static int pd_send_msg(struct cypd *pd, u8 msg_type, const u32 *data,
		size_t num_data, enum pd_sop_type sop)
{
	int ret;
	u16 hdr;

	if (sop == SOP_MSG)
		hdr = PD_HEADER(msg_type, pd->current_pr, pd->current_dr,
				pd->spec_rev, pd->tx_msgid[sop], num_data, 0);
	else
		/* sending SOP'/SOP'' to a cable, PR/DR fields should be 0 */
		hdr = PD_HEADER(msg_type, 0, 0, pd->spec_rev, pd->tx_msgid[sop],
				num_data, 0);

	ret = cypd_phy_write(hdr, (u8 *)data, num_data * sizeof(u32), sop);
	if (ret) {
		if (pd->pd_connected)
			dev_err(&pd->dev, "Error sending VDM %d\n",	ret);
		return ret;
	}

	pd->tx_msgid[sop] = (pd->tx_msgid[sop] + 1) & PD_HEADER_ID_MASK;
	return 0;
}

static void handle_vdm_tx(struct cypd *pd, enum pd_sop_type sop_type)
{
	u32 vdm_hdr;
	int ret;

	mutex_lock(&pd->svid_handler_lock);
	if (!pd->vdm_tx) {
		mutex_unlock(&pd->svid_handler_lock);
		return;
	}

	/* only send one VDM at a time */
	vdm_hdr = pd->vdm_tx->data[0];

	ret = pd_send_msg(pd, PD_DATA_VENDOR_DEF, pd->vdm_tx->data,
			pd->vdm_tx->size, sop_type);
	if (ret) {
		dev_err(&pd->dev, "Error (%d) sending VDM command %d\n",
				ret, PD_VDO_CMD(pd->vdm_tx->data[0]));

		mutex_unlock(&pd->svid_handler_lock);

		/* TODO: consider retrying send */

		return;
	}

	/* start tVDMSenderResponse timer */
	if (PD_VDO_SVDM(vdm_hdr) &&
		PD_VDO_CMDT(vdm_hdr) == CMDT_INIT) {
		pd->svdm_start_time = ktime_get();
	}

	/*
	 * special case: keep initiated Discover ID/SVIDs
	 * around in case we need to re-try when receiving BUSY
	 */
	if (PD_VDO_SVDM(vdm_hdr) &&
		PD_VDO_CMDT(vdm_hdr) == CMDT_INIT &&
		PD_VDO_CMD(vdm_hdr) <= CMD_DISCOVER_SVID) {
		if (pd->vdm_tx_retry) {
			dev_dbg(&pd->dev, "Previous Discover VDM command %d not ACKed/NAKed\n",
				PD_VDO_CMD(
					pd->vdm_tx_retry->data[0]));
			kfree(pd->vdm_tx_retry);
		}
		pd->vdm_tx_retry = pd->vdm_tx;
	} else {
		kfree(pd->vdm_tx);
	}

	pd->vdm_tx = NULL;
	mutex_unlock(&pd->svid_handler_lock);
}

int cypd_send_vdm(struct cypd *pd, u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	struct vdm_tx *vdm_tx;

	if (pd->vdm_tx) {
		dev_warn(&pd->dev, "Discarding previously queued VDM tx (SVID:0x%04x)\n",
				PD_VDO_VID(pd->vdm_tx->data[0]));

		kfree(pd->vdm_tx);
		pd->vdm_tx = NULL;
	}

	vdm_tx = kzalloc(sizeof(*vdm_tx), GFP_ATOMIC);
	if (!vdm_tx)
		return -ENOMEM;

	vdm_tx->data[0] = vdm_hdr;
	if (vdos && num_vdos)
		memcpy(&vdm_tx->data[1], vdos, num_vdos * sizeof(u32));
	vdm_tx->size = num_vdos + 1; /* include the header */

	/* VDM will get sent in PE_SRC/SNK_READY state handling */
	pd->vdm_tx = vdm_tx;

	/* slight delay before queuing to prioritize handling of incoming VDM */
	kick_sm(pd, 2);

	return 0;
}
EXPORT_SYMBOL(cypd_send_vdm);

int cypd_send_svdm(struct cypd *pd, u16 svid, u8 cmd,
		int cmd_type, int obj_pos,
		const u32 *vdos, int num_vdos)
{
	u32 svdm_hdr = SVDM_HDR(svid, pd->spec_rev == PD_REV30 ? 1 : 0,
			obj_pos, cmd_type, cmd);

	dev_dbg(&pd->dev, "VDM tx: svid:%04x ver:%d obj_pos:%d cmd:%x cmd_type:%x svdm_hdr:%x\n",
			svid, pd->spec_rev == PD_REV30 ? 1 : 0, obj_pos,
			cmd, cmd_type, svdm_hdr);

	return cypd_send_vdm(pd, svdm_hdr, vdos, num_vdos);
}
EXPORT_SYMBOL(cypd_send_svdm);

int cypd_register_svid(struct cypd *pd, struct cypd_svid_handler *hdlr)
{
	if (find_svid_handler(pd, hdlr->svid, hdlr->pid)) {
		dev_err(&pd->dev, "SVID/PID 0x%04x/0x%04x already registered\n",
				hdlr->svid, hdlr->pid);
		return -EINVAL;
	}

	/* require connect/disconnect callbacks be implemented */
	if (!hdlr->connect || !hdlr->disconnect) {
		dev_err(&pd->dev,
				"SVID/PID 0x%04x/0x%04x connect/disconnect must be non-NULL\n",
				hdlr->svid, hdlr->pid);
		return -EINVAL;
	}

	dev_dbg(&pd->dev, "registered handler(%pK) for SVID/PID 0x%04x/0x%04x\n",
							hdlr, hdlr->svid, hdlr->pid);
	mutex_lock(&pd->svid_handler_lock);
	list_add_tail(&hdlr->entry, &pd->svid_handlers);
	mutex_unlock(&pd->svid_handler_lock);

	/* already connected with this SVID discovered? */
	if (pd->vdm_state >= DISCOVERED_SVIDS) {
		int i;

		for (i = 0; i < pd->num_svids; i++) {
			if (pd->discovered_svids[i] == hdlr->svid) {
				dev_dbg(&pd->dev, "Notify SVID: 0x%04x disconnect\n",
						hdlr->svid);
				hdlr->connect(hdlr);
				hdlr->discovered = true;
				break;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(cypd_register_svid);

void cypd_unregister_svid(struct cypd *pd, struct cypd_svid_handler *hdlr)
{

	dev_dbg(&pd->dev, "unregistered handler(%pK) for SVID 0x%04x\n",
							hdlr, hdlr->svid);
	mutex_lock(&pd->svid_handler_lock);
	list_del_init(&hdlr->entry);
	mutex_unlock(&pd->svid_handler_lock);
}
EXPORT_SYMBOL(cypd_unregister_svid);

/* Handles current state and determines transitions */
static void cypd_sm(struct work_struct *w)
{
	struct cypd *pd = container_of(w, struct cypd, sm_work);
	int ret;
	struct rx_msg *rx_msg = NULL;
	unsigned long flags;

	dev_dbg(&pd->dev, "handle state %s\n",
			cypd_state_strings[pd->current_state]);

	hrtimer_cancel(&pd->timer);
	pd->sm_queued = false;

	spin_lock_irqsave(&pd->rx_lock, flags);
	if (!list_empty(&pd->rx_q)) {
		rx_msg = list_first_entry(&pd->rx_q, struct rx_msg, entry);
		list_del(&rx_msg->entry);
	}
	spin_unlock_irqrestore(&pd->rx_lock, flags);

	if (pd->current_state < PE_MAX_STATES &&
			state_handlers[pd->current_state].handle_state)
		state_handlers[pd->current_state].handle_state(pd, rx_msg);
	else
		dev_err(&pd->dev, "Unhandled state %s\n",
				cypd_state_strings[pd->current_state]);

	kfree(rx_msg);

	spin_lock_irqsave(&pd->rx_lock, flags);
	ret = list_empty(&pd->rx_q);
	spin_unlock_irqrestore(&pd->rx_lock, flags);

	/* requeue if there are any new/pending RX messages */
	if (!ret && !pd->sm_queued)
		kick_sm(pd, 0);

	if (!pd->sm_queued)
		pm_relax(&pd->dev);
}

static void cypd_vdm_open_work(struct work_struct *w)
{
	struct vdm_work *work = container_of(w, struct vdm_work, work);
	struct cypd *pd = work->pd;
	struct cypd_phy_params phy_params = {
		.msg_rx_cb		= phy_msg_received,
	};
	int ret;

	ret = cypd_phy_open(&phy_params);
	if (ret && ret != -EBUSY)
		dev_err(&pd->dev, "error opening PD PHY %d\n", ret);

	kfree(work);
}

static void cypd_vdm_close_work(struct work_struct *w)
{
	struct vdm_work *work = container_of(w, struct vdm_work, work);

	cypd_phy_close();
	kfree(work);
}

static int queue_vdm_open_work(struct cypd *pd)
{
	struct vdm_work *vdm_work;

	vdm_work = kzalloc(sizeof(struct vdm_work), GFP_ATOMIC);
	if (!vdm_work)
		return -ENOMEM;

	vdm_work->pd = pd;
	INIT_WORK(&vdm_work->work, cypd_vdm_open_work);
	queue_work(pd->wq, &vdm_work->work);

	return 0;
}

static int queue_vdm_close_work(struct cypd *pd)
{
	struct vdm_work *vdm_work;

	vdm_work = kzalloc(sizeof(struct vdm_work), GFP_ATOMIC);
	if (!vdm_work)
		return -ENOMEM;

	vdm_work->pd = pd;
	INIT_WORK(&vdm_work->work, cypd_vdm_close_work);
	queue_work(pd->wq, &vdm_work->work);

	return 0;
}

static void cypd_pe_psy_changed_work(struct work_struct *work)
{
	int ret;
	union power_supply_propval val;
	struct cypd *pd = container_of(work, struct cypd, psy_changed_work);

	ret = power_supply_get_property(pd->cypd_psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (ret < 0) {
		dev_err(&pd->dev, "Unable to read cypd STATUS: %d\n", ret);
		return;
	}

	pd->pd_connected = (val.intval == POWER_SUPPLY_STATUS_CHARGING);

	if (pd->pd_connected)
		cypd_set_state(pd, PE_SNK_READY);
	else
		cypd_set_state(pd, PE_DISCONNECT);

	if (!work_busy(&pd->sm_work))
		kick_sm(pd, 0);
	else
		dev_dbg(&pd->dev, "cypd_sm already running\n");
}

static int psy_changed(struct notifier_block *nb, unsigned long evt, void *ptr)
{
	struct cypd *pd = container_of(nb, struct cypd, psy_nb);

	if (ptr != pd->cypd_psy || evt != PSY_EVENT_PROP_CHANGED)
		return 0;

	dev_dbg(&pd->dev, "Received psy changed\n");

	schedule_work(&pd->psy_changed_work);

	return NOTIFY_OK;
}

static enum hrtimer_restart pd_timeout(struct hrtimer *timer)
{
	struct cypd *pd = container_of(timer, struct cypd, timer);

	dev_dbg(&pd->dev, "PD timed out\n");
	queue_work(pd->wq, &pd->sm_work);
	return HRTIMER_NORESTART;
}

static int cypd_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct cypd *pd = dev_get_drvdata(dev);

	dev_dbg(&pd->dev, "Received uevent\n");
	return 0;
}

static struct class cypd_class = {
	.name = "cypd_policy_engine",
	.owner = THIS_MODULE,
	.dev_uevent = cypd_uevent,
};

static int match_cypd_device(struct device *dev, const void *data)
{
	return dev->parent == data;
}

static void devm_cypd_put(struct device *dev, void *res)
{
	struct cypd **ppd = res;

	put_device(&(*ppd)->dev);
}

struct cypd *devm_cypd_get_by_phandle(struct device *dev, const char *phandle)
{
	struct cypd **ptr, *pd = NULL;
	struct device_node *pd_np;
	struct i2c_client *i2c_dev;
	struct device *pd_dev;

	if (!cypd_class.p)
		return ERR_PTR(-EPROBE_DEFER);

	if (!dev->of_node)
		return ERR_PTR(-EINVAL);

	pd_np = of_parse_phandle(dev->of_node, phandle, 0);
	if (!pd_np)
		return ERR_PTR(-ENXIO);

	i2c_dev = of_find_i2c_device_by_node(pd_np);
	if (!i2c_dev)
		return ERR_PTR(-EPROBE_DEFER);

	pd_dev = class_find_device(&cypd_class, NULL, &i2c_dev->dev,
			match_cypd_device);
	if (!pd_dev) {
		put_device(&i2c_dev->dev);
		/* device was found but maybe hadn't probed yet, so defer */
		return ERR_PTR(-EPROBE_DEFER);
	}

	ptr = devres_alloc(devm_cypd_put, sizeof(*ptr), GFP_KERNEL);
	if (!ptr) {
		put_device(pd_dev);
		put_device(&i2c_dev->dev);
		return ERR_PTR(-ENOMEM);
	}

	pd = dev_get_drvdata(pd_dev);
	if (!pd)
		return ERR_PTR(-EPROBE_DEFER);

	*ptr = pd;
	devres_add(dev, ptr);

	return pd;
}
EXPORT_SYMBOL(devm_cypd_get_by_phandle);

static void cypd_startup_common(struct cypd *pd)
{
	/* force spec rev to be 3.0 */
	pd->spec_rev = PD_REV30;

	/* Close first, in case it was already open */
	queue_vdm_close_work(pd);
	queue_vdm_open_work(pd);
}

static int num_pd_instances;

/**
 * cypd_create - Create a new instance of PD policy engine
 * @parent - parent device to associate with
 *
 * This creates a new cypd device which manages the state of a
 * Cypress PD-capable sink port. The parent device that is passed in should be
 * associated with the physical device.
 *
 * Return: struct cypd pointer, or an ERR_PTR value
 */
struct cypd *cypd_create(struct device *parent)
{
	int ret;
	struct cypd *pd;

	if (!cypd_class.p)
		return ERR_PTR(-EPROBE_DEFER);

	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);

	device_initialize(&pd->dev);
	pd->dev.class = &cypd_class;
	pd->dev.parent = parent;
	dev_set_drvdata(&pd->dev, pd);

	ret = dev_set_name(&pd->dev, "cypd%d", num_pd_instances++);
	if (ret)
		goto free_pd;

	ret = device_init_wakeup(&pd->dev, true);
	if (ret)
		goto free_pd;

	ret = device_add(&pd->dev);
	if (ret)
		goto free_pd;

	pd->wq = alloc_ordered_workqueue("cypd_wq", WQ_FREEZABLE | WQ_HIGHPRI);
	if (!pd->wq) {
		ret = -ENOMEM;
		goto del_pd;
	}

	INIT_WORK(&pd->sm_work, cypd_sm);
	hrtimer_init(&pd->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pd->timer.function = pd_timeout;
	mutex_init(&pd->svid_handler_lock);
	spin_lock_init(&pd->rx_lock);
	INIT_LIST_HEAD(&pd->rx_q);
	INIT_LIST_HEAD(&pd->svid_handlers);

	pd->cypd_psy = power_supply_get_by_name("cypd3177");
	if (!pd->cypd_psy) {
		dev_dbg(&pd->dev, "Could not get cypd power_supply, deferring probe\n");
		ret = -EPROBE_DEFER;
		goto destroy_wq;
	}

	pd->psy_nb.notifier_call = psy_changed;
	ret = power_supply_reg_notifier(&pd->psy_nb);
	if (ret)
		goto del_inst;
	INIT_WORK(&pd->psy_changed_work, cypd_pe_psy_changed_work);

	/* force read initial power_supply values */
	psy_changed(&pd->psy_nb, PSY_EVENT_PROP_CHANGED, pd->cypd_psy);

	dev_dbg(&pd->dev, "cypd policy engine created successfully\n");
	return pd;

del_inst:
destroy_wq:
	destroy_workqueue(pd->wq);
del_pd:
	device_del(&pd->dev);
free_pd:
	num_pd_instances--;
	put_device(&pd->dev);
	kfree(pd);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(cypd_create);

/**
 * cypd_destroy - Removes and frees a cypd instance
 * @pd: the instance to destroy
 */
void cypd_destroy(struct cypd *pd)
{
	if (!pd)
		return;

	power_supply_unreg_notifier(&pd->psy_nb);
	power_supply_put(pd->cypd_psy);
	device_unregister(&pd->dev);
	kfree(pd);
}
EXPORT_SYMBOL(cypd_destroy);

static int __init cypd_init(void)
{
	return class_register(&cypd_class);
}
module_init(cypd_init);

static void __exit cypd_exit(void)
{
	class_unregister(&cypd_class);
}
module_exit(cypd_exit);

MODULE_DESCRIPTION("Cypress Power Delivery Policy Engine");
MODULE_LICENSE("GPL v2");
