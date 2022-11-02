// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2013-2019, The Linux Foundation. All rights reserved. */

#include <linux/netdevice.h>
#include "rmnet_config.h"
#include "rmnet_map.h"
#include "rmnet_private.h"
#include "rmnet_vnd.h"

#define RMNET_DL_IND_HDR_SIZE (sizeof(struct rmnet_map_dl_ind_hdr) + \
			       sizeof(struct rmnet_map_header) + \
			       sizeof(struct rmnet_map_control_command_header))

#define RMNET_MAP_CMD_SIZE (sizeof(struct rmnet_map_header) + \
			    sizeof(struct rmnet_map_control_command_header))

#define RMNET_DL_IND_TRL_SIZE (sizeof(struct rmnet_map_dl_ind_trl) + \
			       sizeof(struct rmnet_map_header) + \
			       sizeof(struct rmnet_map_control_command_header))

static u8 rmnet_map_do_flow_control(struct sk_buff *skb,
				    struct rmnet_port *port,
				    int enable)
{
	struct rmnet_map_header *qmap;
	struct rmnet_map_control_command *cmd;
	struct rmnet_endpoint *ep;
	struct net_device *vnd;
	u16 ip_family;
	u16 fc_seq;
	u32 qos_id;
	u8 mux_id;
	int r;

	qmap = (struct rmnet_map_header *)rmnet_map_data_ptr(skb);
	mux_id = qmap->mux_id;
	cmd = rmnet_map_get_cmd_start(skb);

	if (mux_id >= RMNET_MAX_LOGICAL_EP) {
		kfree_skb(skb);
		return RX_HANDLER_CONSUMED;
	}

	ep = rmnet_get_endpoint(port, mux_id);
	if (!ep) {
		kfree_skb(skb);
		return RX_HANDLER_CONSUMED;
	}

	vnd = ep->egress_dev;

	ip_family = cmd->flow_control.ip_family;
	fc_seq = ntohs(cmd->flow_control.flow_control_seq_num);
	qos_id = ntohl(cmd->flow_control.qos_id);

	/* Ignore the ip family and pass the sequence number for both v4 and v6
	 * sequence. User space does not support creating dedicated flows for
	 * the 2 protocols
	 */
	r = rmnet_vnd_do_flow_control(vnd, enable);
	if (r) {
		kfree_skb(skb);
		return RMNET_MAP_COMMAND_UNSUPPORTED;
	} else {
		return RMNET_MAP_COMMAND_ACK;
	}
}

static void rmnet_map_send_ack(struct sk_buff *skb,
			       unsigned char type,
			       struct rmnet_port *port)
{
	struct rmnet_map_control_command *cmd;
	struct net_device *dev = skb->dev;

	if (port->data_format & RMNET_FLAGS_INGRESS_MAP_CKSUMV4)
		pskb_trim(skb,
			  skb->len - sizeof(struct rmnet_map_dl_csum_trailer));

	skb->protocol = htons(ETH_P_MAP);

	cmd = rmnet_map_get_cmd_start(skb);
	cmd->cmd_type = type & 0x03;

	netif_tx_lock(dev);
	dev->netdev_ops->ndo_start_xmit(skb, dev);
	netif_tx_unlock(dev);
}

void
rmnet_map_dl_hdr_notify_v2(struct rmnet_port *port,
			   struct rmnet_map_dl_ind_hdr *dlhdr,
			   struct rmnet_map_control_command_header *qcmd)
{
	struct rmnet_map_dl_ind *tmp;

	port->dl_marker_flush = 0;

	list_for_each_entry(tmp, &port->dl_list, list)
		tmp->dl_hdr_handler_v2(dlhdr, qcmd);
}

void
rmnet_map_dl_hdr_notify(struct rmnet_port *port,
			struct rmnet_map_dl_ind_hdr *dlhdr)
{
	struct rmnet_map_dl_ind *tmp;

	port->dl_marker_flush = 0;

	list_for_each_entry(tmp, &port->dl_list, list)
		tmp->dl_hdr_handler(dlhdr);
}

void
rmnet_map_dl_trl_notify_v2(struct rmnet_port *port,
			   struct rmnet_map_dl_ind_trl *dltrl,
			   struct rmnet_map_control_command_header *qcmd)
{
	struct rmnet_map_dl_ind *tmp;
	struct napi_struct *napi;

	list_for_each_entry(tmp, &port->dl_list, list)
		tmp->dl_trl_handler_v2(dltrl, qcmd);

	if (port->dl_marker_flush) {
		napi = get_current_napi_context();
		napi_gro_flush(napi, false);
	}

	port->dl_marker_flush = -1;
}

void
rmnet_map_dl_trl_notify(struct rmnet_port *port,
			struct rmnet_map_dl_ind_trl *dltrl)
{
	struct rmnet_map_dl_ind *tmp;
	struct napi_struct *napi;

	list_for_each_entry(tmp, &port->dl_list, list)
		tmp->dl_trl_handler(dltrl);

	if (port->dl_marker_flush) {
		napi = get_current_napi_context();
		napi_gro_flush(napi, false);
	}

	port->dl_marker_flush = -1;
}

static void rmnet_map_process_flow_start(struct sk_buff *skb,
					 struct rmnet_port *port,
					 bool rmnet_perf)
{
	struct rmnet_map_dl_ind_hdr *dlhdr;
	struct rmnet_map_control_command_header *qcmd;
	u32 data_format;
	bool is_dl_mark_v2;

	if (skb->len < RMNET_DL_IND_HDR_SIZE)
		return;

	data_format = port->data_format;
	is_dl_mark_v2 = data_format & RMNET_INGRESS_FORMAT_DL_MARKER_V2;
	if (is_dl_mark_v2) {
		pskb_pull(skb, sizeof(struct rmnet_map_header));
		qcmd = (struct rmnet_map_control_command_header *)
			rmnet_map_data_ptr(skb);
		port->stats.dl_hdr_last_ep_id = qcmd->source_id;
		port->stats.dl_hdr_last_qmap_vers = qcmd->reserved;
		port->stats.dl_hdr_last_trans_id = qcmd->transaction_id;
		pskb_pull(skb, sizeof(struct rmnet_map_control_command_header));
	} else {
		pskb_pull(skb, RMNET_MAP_CMD_SIZE);
	}

	dlhdr = (struct rmnet_map_dl_ind_hdr *)rmnet_map_data_ptr(skb);

	port->stats.dl_hdr_last_seq = dlhdr->le.seq;
	port->stats.dl_hdr_last_bytes = dlhdr->le.bytes;
	port->stats.dl_hdr_last_pkts = dlhdr->le.pkts;
	port->stats.dl_hdr_last_flows = dlhdr->le.flows;
	port->stats.dl_hdr_total_bytes += port->stats.dl_hdr_last_bytes;
	port->stats.dl_hdr_total_pkts += port->stats.dl_hdr_last_pkts;
	port->stats.dl_hdr_count++;

	if (is_dl_mark_v2)
		rmnet_map_dl_hdr_notify_v2(port, dlhdr, qcmd);
	else
		rmnet_map_dl_hdr_notify(port, dlhdr);

	if (rmnet_perf) {
		unsigned int pull_size;

		pull_size = sizeof(struct rmnet_map_dl_ind_hdr);
		if (data_format & RMNET_FLAGS_INGRESS_MAP_CKSUMV4)
			pull_size += sizeof(struct rmnet_map_dl_csum_trailer);
		pskb_pull(skb, pull_size);
	}
}

static void rmnet_map_process_flow_end(struct sk_buff *skb,
				       struct rmnet_port *port,
				       bool rmnet_perf)
{
	struct rmnet_map_dl_ind_trl *dltrl;
	struct rmnet_map_control_command_header *qcmd;
	u32 data_format;
	bool is_dl_mark_v2;

	if (skb->len < RMNET_DL_IND_TRL_SIZE)
		return;

	data_format = port->data_format;
	is_dl_mark_v2 = data_format & RMNET_INGRESS_FORMAT_DL_MARKER_V2;
	if (is_dl_mark_v2) {
		pskb_pull(skb, sizeof(struct rmnet_map_header));
		qcmd = (struct rmnet_map_control_command_header *)
			rmnet_map_data_ptr(skb);
		pskb_pull(skb, sizeof(struct rmnet_map_control_command_header));
	} else {
		pskb_pull(skb, RMNET_MAP_CMD_SIZE);
	}

	dltrl = (struct rmnet_map_dl_ind_trl *)rmnet_map_data_ptr(skb);

	port->stats.dl_trl_last_seq = dltrl->seq_le;
	port->stats.dl_trl_count++;

	if (is_dl_mark_v2)
		rmnet_map_dl_trl_notify_v2(port, dltrl, qcmd);
	else
		rmnet_map_dl_trl_notify(port, dltrl);

	if (rmnet_perf) {
		unsigned int pull_size;

		pull_size = sizeof(struct rmnet_map_dl_ind_trl);
		if (data_format & RMNET_FLAGS_INGRESS_MAP_CKSUMV4)
			pull_size += sizeof(struct rmnet_map_dl_csum_trailer);
		pskb_pull(skb, pull_size);
	}
}

/* Process MAP command frame and send N/ACK message as appropriate. Message cmd
 * name is decoded here and appropriate handler is called.
 */
void rmnet_map_command(struct sk_buff *skb, struct rmnet_port *port)
{
	struct rmnet_map_control_command *cmd;
	unsigned char command_name;
	unsigned char rc = 0;

	cmd = rmnet_map_get_cmd_start(skb);
	command_name = cmd->command_name;

	switch (command_name) {
	case RMNET_MAP_COMMAND_FLOW_ENABLE:
		rc = rmnet_map_do_flow_control(skb, port, 1);
		break;

	case RMNET_MAP_COMMAND_FLOW_DISABLE:
		rc = rmnet_map_do_flow_control(skb, port, 0);
		break;

	default:
		rc = RMNET_MAP_COMMAND_UNSUPPORTED;
		kfree_skb(skb);
		break;
	}
	if (rc == RMNET_MAP_COMMAND_ACK)
		rmnet_map_send_ack(skb, rc, port);
}

int rmnet_map_flow_command(struct sk_buff *skb, struct rmnet_port *port,
			   bool rmnet_perf)
{
	struct rmnet_map_control_command *cmd;
	unsigned char command_name;

	cmd = rmnet_map_get_cmd_start(skb);
	command_name = cmd->command_name;

	switch (command_name) {
	case RMNET_MAP_COMMAND_FLOW_START:
		rmnet_map_process_flow_start(skb, port, rmnet_perf);
		break;

	case RMNET_MAP_COMMAND_FLOW_END:
		rmnet_map_process_flow_end(skb, port, rmnet_perf);
		break;

	default:
		return 1;
	}

	/* rmnet_perf module will handle the consuming */
	if (!rmnet_perf)
		consume_skb(skb);

	return 0;
}
EXPORT_SYMBOL(rmnet_map_flow_command);

void rmnet_map_cmd_exit(struct rmnet_port *port)
{
	struct rmnet_map_dl_ind *tmp, *idx;

	list_for_each_entry_safe(tmp, idx, &port->dl_list, list)
		list_del_rcu(&tmp->list);
}

void rmnet_map_cmd_init(struct rmnet_port *port)
{
	INIT_LIST_HEAD(&port->dl_list);

	port->dl_marker_flush = -1;
}

int rmnet_map_dl_ind_register(struct rmnet_port *port,
			      struct rmnet_map_dl_ind *dl_ind)
{
	struct rmnet_map_dl_ind *dl_ind_iterator;
	bool empty_ind_list = true;

	if (!port || !dl_ind || !dl_ind->dl_hdr_handler ||
	    !dl_ind->dl_trl_handler)
		return -EINVAL;

	list_for_each_entry_rcu(dl_ind_iterator, &port->dl_list, list) {
		empty_ind_list = false;
		if (dl_ind_iterator->priority < dl_ind->priority) {
			if (dl_ind_iterator->list.next) {
				if (dl_ind->priority
				    < list_entry_rcu(dl_ind_iterator->list.next,
				    typeof(*dl_ind_iterator), list)->priority) {
					list_add_rcu(&dl_ind->list,
						     &dl_ind_iterator->list);
					break;
				}
			} else {
				list_add_rcu(&dl_ind->list,
					     &dl_ind_iterator->list);
				break;
			}
		} else {
			list_add_tail_rcu(&dl_ind->list,
					  &dl_ind_iterator->list);
			break;
		}
	}

	if (empty_ind_list)
		list_add_rcu(&dl_ind->list, &port->dl_list);

	return 0;
}
EXPORT_SYMBOL(rmnet_map_dl_ind_register);

int rmnet_map_dl_ind_deregister(struct rmnet_port *port,
				struct rmnet_map_dl_ind *dl_ind)
{
	struct rmnet_map_dl_ind *tmp;

	if (!port || !dl_ind)
		return -EINVAL;

	list_for_each_entry(tmp, &port->dl_list, list) {
		if (tmp == dl_ind) {
			list_del_rcu(&dl_ind->list);
			goto done;
		}
	}

done:
	return 0;
}
EXPORT_SYMBOL(rmnet_map_dl_ind_deregister);
