// SPDX-License-Identifier: GPL-2.0
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/inet.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <net/sock.h>
#include <asm/unaligned.h>
#include <linux/etherdevice.h>
#include "type_header.h"

DECLARE_HASHTABLE(session_tab, SESSION_HASH_BITS);

static u32 session_hash_idx(const unsigned char *addr)
{
	u64 value = get_unaligned((u64 *)addr);
#ifdef __BIG_ENDIAN
	value >>= 16;
#else
	value <<= 16;
#endif
	return hash_64(value, SESSION_HASH_BITS);
}

struct session_entry *session_hash_lookup(const unsigned char *addr)
{
	struct session_entry *entry;
	u32 idx = session_hash_idx(addr);
	struct hlist_head *h = &session_tab[idx];

	hlist_for_each_entry_rcu(entry, h, hlist) {
		if (ether_addr_equal_64bits(entry->addr, addr))
			return entry;
	}
	return NULL;
}

struct session_entry *session_hash_add(const unsigned char *addr, MSG_BUF_t *msg)
{
	struct session_entry *entry;
	struct hlist_head *h;

	entry = session_hash_lookup(addr);
	if (entry)
		return NULL;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return NULL;

	ether_addr_copy(entry->addr, addr);
	entry->msg = msg;
	h = &session_tab[session_hash_idx(addr)];
	hlist_add_head_rcu(&entry->hlist, h);

	return entry;
}

bool session_hash_del(struct session_entry *entry)
{
	if (entry) {
		hlist_del_rcu(&entry->hlist);
		netinfo_pr_info("msg %p\n", entry->msg);
		put_msg(entry->msg);
		kfree(entry);
		return true;
	}
	return false;
}

void session_flush_sources(void)
{
	struct session_entry *entry;
	struct hlist_node *next;
	int i = 0;

	hash_for_each_safe(session_tab, i, next,
				entry, hlist)
		session_hash_del(entry);
}

void session_table_init(void)
{
	hash_init(session_tab);
}

void session_table_deinit(void)
{
	session_flush_sources();
}
