// SPDX-License-Identifier: GPL-2.0
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_bridge.h>
#include <linux/skbuff.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <net/ip.h>
#include <linux/inet.h>
#include <linux/printk.h>
#include "type_header.h"
#include "stats_header.h"

DECLARE_COMPLETION(msg_arrive);

static MSG_BUF_t msg_array[MSG_ARRAY_SIZE];
// The list will be processed soon
LIST_HEAD(msg_queue_list);
spinlock_t msg_queue_lock;
// The free list as memory pool
LIST_HEAD(msg_free_list);
spinlock_t msg_free_lock;

static MSG_BUF_t *get_msg(void)
{
	MSG_BUF_t *tmp = NULL;

	spin_lock(&msg_free_lock);
	tmp = list_first_entry_or_null(&msg_free_list, MSG_BUF_t, list);
	if (tmp) {
		list_del(&tmp->list);
	}
	spin_unlock(&msg_free_lock);
	netinfo_pr_debug("msg %p\n", tmp);
	return tmp;
}

void put_msg(MSG_BUF_t *msg)
{
	if (msg != NULL) {
		if (delayed_work_pending(&msg->latency_work)) {
			netinfo_pr_info("cancel_delayed_work\n");
			cancel_delayed_work_sync(&msg->latency_work);
		}
		memset(msg, 0, sizeof(MSG_BUF_t));
		spin_lock(&msg_free_lock);
		list_add_tail(&msg->list, &msg_free_list);
		spin_unlock(&msg_free_lock);
		netinfo_pr_debug("type %d dev %p\n", msg->type, msg);
	}
}

static MSG_BUF_t *fetch_msg(void)
{
	MSG_BUF_t *tmp = NULL;

	spin_lock_irq(&msg_queue_lock);
	tmp = list_first_entry_or_null(&msg_queue_list, MSG_BUF_t, list);
	if (tmp) {
		list_del(&tmp->list);
	}
	spin_unlock_irq(&msg_queue_lock);
	return tmp;
}

static void init_msg_freelist(void)
{
	MSG_BUF_t *tmp = NULL;
	u32 i;

	for (i = 0; i < MSG_ARRAY_SIZE; i++) {
		tmp = &msg_array[i];
		memset(tmp, 0, sizeof(MSG_BUF_t));
		spin_lock(&msg_free_lock);
		list_add_tail(&tmp->list, &msg_free_list);
		spin_unlock(&msg_free_lock);
	}
}

static void deinit_msg_freelist(void)
{
	spin_lock(&msg_free_lock);
	INIT_LIST_HEAD(&msg_free_list);
	spin_unlock(&msg_free_lock);
}

static void process_msg_wakeup(MSG_BUF_t *msg)
{
	spin_lock(&msg_queue_lock);
	list_add_tail(&msg->list, &msg_queue_list);
	spin_unlock(&msg_queue_lock);
	complete_all(&msg_arrive);
}

int send_session_update_msg(const u8 *mac, ADDR_INFO_t addr,
			    struct net_device *dev)
{
	MSG_BUF_t *msg = get_msg();

	if (msg == NULL) {
		netinfo_pr_info("msg alloc failed");
		return 0;
	}
	msg->type = MSG_SESSION_ADD;
	memcpy(msg->src_macaddr, mac, ETH_ALEN);
	memcpy(&msg->addr, &addr, sizeof(ADDR_INFO_t));
	netinfo_pr_info("msg %p", msg);
	msg->dev = dev;
	process_msg_wakeup(msg);

	return 1;
}

void send_session_delete_msg(const u8 *mac)
{
// disable for SN user build
#if defined(CONFIG_DEBUG_INFO) || !defined(CONFIG_META_HAMMERHEAD)
	MSG_BUF_t *msg = get_msg();

	if (msg == NULL) {
		netinfo_pr_info("msg alloc failed");
		return;
	}
	netinfo_pr_debug("msg %p", msg);
	msg->type = MSG_SESSION_DEL;
	memcpy(msg->src_macaddr, mac, ETH_ALEN);
	process_msg_wakeup(msg);
#endif
}
EXPORT_SYMBOL(send_session_delete_msg);

int send_session_info_msg(const u8 *mac, void *info)
{
	MSG_BUF_t *msg = get_msg();

	if (msg == NULL) {
		return 0;
	}
	msg->type = MSG_SESSION_STATS;
	memcpy(msg->src_macaddr, mac, ETH_ALEN);
	memcpy(&msg->link_info, info, sizeof(LINK_INFO_t));
	process_msg_wakeup(msg);

	return 1;
}

int send_session_send_garp_msg(ADDR_INFO_t addr, struct net_device *dev)
{
	MSG_BUF_t *msg = get_msg();

	if (msg == NULL) {
		netinfo_pr_info("msg alloc failed");
		return 0;
	}
	msg->type = MSG_SESSION_SEND_GARP;
	memcpy(&msg->addr, &addr, sizeof(ADDR_INFO_t));
	netinfo_pr_info("msg %p", msg);
	msg->dev = dev;
	process_msg_wakeup(msg);

	return 1;
}

int send_session_cancel_rx_handler_msg(struct net_device *dev)
{
	MSG_BUF_t *msg = get_msg();

	if (msg == NULL) {
		netinfo_pr_info("msg alloc failed");
		return 0;
	}
	if (msg == NULL) {
		netinfo_pr_info("msg alloc failed");
		return 0;
	}
	msg->type = MSG_SESSION_CANCEL_RX_HANDLER;
	msg->dev = dev;
	netinfo_pr_info("msg %p", msg);
	process_msg_wakeup(msg);

	return 1;
}

int send_session_latency_msg(const u8 *mac, unsigned long ts)
{
	MSG_BUF_t *msg = get_msg();

	if (msg == NULL) {
		return 0;
	}
	msg->type = MSG_SESSION_LATENCY;
	memcpy(msg->src_macaddr, mac, ETH_ALEN);
	msg->last_rcv_ts = ts;
	spin_lock(&msg_queue_lock);
	list_add_tail(&msg->list, &msg_queue_list);
	spin_unlock(&msg_queue_lock);
	complete_all(&msg_arrive);

	return 1;
}

static void latency_worker(struct work_struct *work)
{
	struct session_entry *entry = NULL;
	MSG_BUF_t *msg = container_of(work, MSG_BUF_t, latency_work.work);

	if (msg != NULL) {
		long diff = time_diff(jiffies, msg->last_rcv_ts);
		entry = session_hash_lookup(msg->src_macaddr);
		if (entry == NULL) {
			netinfo_pr_info("no valid entry for ping, stop it\n");
			return;
		}
		if (diff < SESSION_MAX_TIMEOUT) {
			send_icmp_skb(msg);
			msg->last_snd_ts = jiffies;
			schedule_delayed_work(&msg->latency_work, 10 * HZ);
		} else {
			netinfo_pr_info(
				"timeout %lx %lx and delete the session of " PRINT_MAC_ADDR_FMT
				"\n",
				jiffies, msg->last_rcv_ts,
				PRINT_MAC_ADDR_REF(msg->src_macaddr));
			send_session_delete_msg(msg->src_macaddr);
		}
	}
}

static void process_msg(MSG_BUF_t *msg)
{
	struct session_entry *entry = NULL;

	if (msg == NULL) {
		pr_err_ratelimited("msg is null\n");
		return;
	}

	switch (msg->type) {
	case MSG_SESSION_ADD:
		if (session_hash_add(msg->src_macaddr, msg)) {
			netinfo_pr_info(
				"Create session from src mac " PRINT_MAC_ADDR_FMT
				" msg %p\n",
				PRINT_MAC_ADDR_REF(msg->src_macaddr), msg);
			msg->last_rcv_ts = jiffies;
			INIT_DELAYED_WORK(&msg->latency_work, latency_worker);
			schedule_delayed_work(&msg->latency_work, HZ);
		} else {
			netinfo_pr_debug("create session failed\n");
			put_msg(msg);
		}
		break;
	case MSG_SESSION_DEL:
		entry = session_hash_lookup(msg->src_macaddr);
		if (entry != NULL) {
			session_hash_del(entry);
			netinfo_pr_info(
				"DISCONNECTED: Deleted from src mac " PRINT_MAC_ADDR_FMT
				"\n",
				PRINT_MAC_ADDR_REF(msg->src_macaddr));
		} else {
			netinfo_pr_info(
				"DISCONNECTED: Not find session src mac " PRINT_MAC_ADDR_FMT
				"\n",
				PRINT_MAC_ADDR_REF(msg->src_macaddr));
			put_msg(msg);
		}
		break;
	case MSG_SESSION_LATENCY:
		entry = session_hash_lookup(msg->src_macaddr);
		if (entry != NULL) {
			MSG_BUF_t *tmp = entry->msg;
			if (tmp) {
				entry->last_rtt = time_diff(msg->last_rcv_ts,
							    tmp->last_snd_ts);
				entry->total_rcv += 1;
				entry->total_rtt += entry->last_rtt;
				tmp->last_rcv_ts = msg->last_rcv_ts;
			}
			//since HZ is 250, we need to multiply by 4 to get ms
			netinfo_pr_info(
				"latency from src mac " PRINT_MAC_ADDR_FMT
				" last_rtt %ld avg %ld msg %p\n",
				PRINT_MAC_ADDR_REF(msg->src_macaddr),
				entry->last_rtt << 2,
				(entry->total_rtt / entry->total_rcv) << 2,
				tmp);
		} else {
			netinfo_pr_info(
				"No session found mac " PRINT_MAC_ADDR_FMT
				" for MSG_SESSION_LATENCY\n",
				PRINT_MAC_ADDR_REF(msg->src_macaddr));
		}
		put_msg(msg);
		break;
	case MSG_SESSION_SEND_GARP:
		send_arp_reply(msg->dev, msg->addr.dst_macaddr,
			       msg->addr.dst_ip);
		put_msg(msg);
		break;
	case MSG_SESSION_CANCEL_RX_HANDLER:
		rtnl_lock();
		unregister_pretcp_rx_handler(msg->dev);
		rtnl_unlock();
		put_msg(msg);
		netinfo_pr_info("MSG_SESSION_CANCEL_RX_HANDLER Done\n");
		break;
	case MSG_SESSION_STATS:
	default:
		netinfo_pr_info("Unknown msg type %x\n", msg->type);
		put_msg(msg);
		break;
	}
}

static int msgq_receive_task(void *data)
{
	MSG_BUF_t *tmp = NULL;

	// Loop to receive packets
	while (!kthread_should_stop()) {
		reinit_completion(&msg_arrive);
		wait_for_completion_interruptible_timeout(&msg_arrive, 1000);
		do {
			tmp = fetch_msg();
			if (tmp != NULL) {
				process_msg(tmp);
			}
		} while (tmp != NULL);
	}
	return 0;
}

// Define the sysfs attribute
static struct kobject *netinfo_kobj = NULL, *stats_kobj = NULL;
static atomic_t netinfo_feature_flags, netinfo_feature_dbg_flags;

int netinfo_tcp_reset_feature_enabled(void)
{
	return NETINFO_IS_FEATURE_ENABLED(netinfo_feature_flags,
					  NETINFO_TCPRST_FEATURE);
}

int netinfo_garp_feature_enabled(void)
{
	return NETINFO_IS_FEATURE_ENABLED(netinfo_feature_flags,
					  NETINFO_GARP_FEATURE);
}

int netinfo_garp_debug_feature_enabled(void)
{
	return NETINFO_IS_FEATURE_ENABLED(netinfo_feature_dbg_flags,
					  NETINFO_GARP_FEATURE);
}

void netinfo_garp_debug_feature_disable(void)
{
	NETINFO_CLEAR_FEATURE(netinfo_feature_dbg_flags, NETINFO_GARP_FEATURE);
}

int netinfo_bypass_dhcp_feature_enabled(void)
{
	return NETINFO_IS_FEATURE_ENABLED(netinfo_feature_flags,
					  NETINFO_BYPASS_DHCP_FEATURE);
}

void netinfo_disable_all_feature(void)
{
	atomic_set(&netinfo_feature_flags, 0);
	atomic_set(&netinfo_feature_dbg_flags, 0);
}

static ssize_t netinfo_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buf)
{
	int val = sprintf(buf, "Feature set %x dbg %x\n",
			  atomic_read(&netinfo_feature_flags),
			  atomic_read(&netinfo_feature_dbg_flags));
	return val;
}

// sysfs write function: feature_bit <enable/disable> <debug enable/disable>
static ssize_t netinfo_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	u32 value1 = 0, value2 = 0, value3 = 0;
	int ret = 0;

	ret = sscanf(buf, "%d %d %d", &value1, &value2, &value3);
	if (ret != 3) {
		netinfo_pr_info(
			"Invalid input format. Expected two integers with space in between: <feature number> <enable(1) / disable(0)>.\n");
		return -EINVAL;
	}
	netinfo_pr_info("Received sysfs command: %d - %d - %d\n", value1,
			value2, value3);

	if (value1 >= 0 && value1 <= NETINFO_MAX_FEATURE) {
		if (value2) {
			NETINFO_SET_FEATURE(netinfo_feature_flags, value1);
		} else {
			NETINFO_CLEAR_FEATURE(netinfo_feature_flags, value1);
		}
		if (value3) {
			NETINFO_SET_FEATURE(netinfo_feature_dbg_flags, value1);
		} else {
			NETINFO_CLEAR_FEATURE(netinfo_feature_dbg_flags,
					      value1);
		}
		netinfo_pr_info("feature flag %x dbg %x\n",
				atomic_read(&netinfo_feature_flags),
				atomic_read(&netinfo_feature_dbg_flags));
	}
	return count;
}

static void unregister_driver(void)
{
	unregister_dev_queue_xmit_kp();
	if (stats_kobj)
		kobject_put(stats_kobj);
	if (netinfo_kobj)
		kobject_put(netinfo_kobj);

	unregister_carrier_notifier();
// disable for SN user build
#if defined(CONFIG_DEBUG_INFO) || !defined(CONFIG_META_HAMMERHEAD)
	unregister_nf_callback();
#endif
	session_table_deinit();
	deinit_msg_freelist();
}

static struct kobj_attribute netinfo_attr =
	__ATTR(netinfo, 0660, netinfo_show, netinfo_store);
static struct kobj_attribute dhcp_attr =
	__ATTR(arp_dhcp, 0660, dhcp_counters_show, dhcp_counters_store);
static struct task_struct *ni_kthread;
static int __init netinfo_module_init(void)
{
	init_msg_freelist();
	session_table_init();
// disable for SN user build
#if defined(CONFIG_DEBUG_INFO) || !defined(CONFIG_META_HAMMERHEAD)
	register_nf_callback();
#endif
	register_carrier_notifier();
	netinfo_kobj = kobject_create_and_add("netinfo", kernel_kobj);
	if (!netinfo_kobj)
		goto unregister_drv;
	atomic_set(&netinfo_feature_flags, 0);
	atomic_set(&netinfo_feature_dbg_flags, 0);
	sysfs_create_file(netinfo_kobj, &netinfo_attr.attr);
	sysfs_chmod_file(netinfo_kobj, &netinfo_attr.attr, 0666);

	stats_kobj = kobject_create_and_add("stats", netinfo_kobj);
	if (!stats_kobj)
		goto unregister_drv;
	sysfs_create_file(stats_kobj, &dhcp_attr.attr);
	sysfs_chmod_file(stats_kobj, &dhcp_attr.attr, 0666);

	register_dev_queue_xmit_kp();
	disable_dev_queue_xmit_kp();

	ni_kthread = kthread_create(msgq_receive_task, NULL, "NetInfo KThread");
	if (ni_kthread) {
		init_completion(&msg_arrive);
		wake_up_process(ni_kthread);
		netinfo_pr_info("init is done\n");
	} else {
		netinfo_pr_err("Cannot create kthread\n");
		goto unregister_drv;
	}

	return 0;

unregister_drv:
	unregister_driver();
	return -1;
}

static void __exit netinfo_exit(void)
{
	kthread_stop(ni_kthread);
	unregister_driver();
}

module_init(netinfo_module_init);
module_exit(netinfo_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(
	"Wireless network link information collect and latency measure");
