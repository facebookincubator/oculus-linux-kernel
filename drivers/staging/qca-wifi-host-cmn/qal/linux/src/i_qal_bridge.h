/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: i_qal_bridge
 * QCA abstraction layer (QAL) bridge APIs
 */

#if !defined(__I_QAL_BRIDGE_H)
#define __I_QAL_BRIDGE_H

/* Include Files */
#include <linux/if_bridge.h>
#include "qdf_types.h"

typedef struct notifier_block *__qal_notify_blk_t;
typedef struct net_device *__qal_netdev_t;
typedef struct net_bridge_fdb_entry *__qal_br_fdb_entry_t;

static inline QDF_STATUS
__qal_bridge_fdb_register_notify(__qal_notify_blk_t nb)
{
	br_fdb_register_notify(nb);
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
__qal_bridge_fdb_unregister_notify(__qal_notify_blk_t nb)
{
	br_fdb_unregister_notify(nb);
	return QDF_STATUS_SUCCESS;
}

static inline __qal_br_fdb_entry_t
__qal_bridge_fdb_has_entry(__qal_netdev_t dev, const char *addr, uint16_t vid)
{
	return br_fdb_has_entry(dev, addr, vid);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 24)
static inline QDF_STATUS
__qal_bridge_fdb_delete_by_netdev(__qal_netdev_t dev,
				  const unsigned char *addr, uint16_t vid)
{
	/* Use 5.4-specific API */
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
__qal_bridge_fdb_delete_by_netdev(__qal_netdev_t dev,
				  const unsigned char *addr, uint16_t vid)
{
	int ret;

	ret = br_fdb_delete_by_netdev(dev, addr, vid);

	return qdf_status_from_os_return(ret);
}
#endif

static inline QDF_STATUS
__qal_bridge_fdb_update_register_notify(__qal_notify_blk_t nb)
{
	br_fdb_update_register_notify(nb);
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
__qal_bridge_fdb_update_unregister_notify(__qal_notify_blk_t nb)
{
	br_fdb_update_unregister_notify(nb);
	return QDF_STATUS_SUCCESS;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
static inline QDF_STATUS
__qal_bridge_fdb_add_or_refresh_by_netdev(__qal_netdev_t dev,
					  const unsigned char *addr,
					  uint16_t vid,
					  uint16_t state)
{
	int ret;

	ret = br_fdb_add_or_refresh_by_netdev(dev, addr, vid, state);
	return qdf_status_from_os_return(ret);
}
#else
static inline QDF_STATUS
__qal_bridge_fdb_add_or_refresh_by_netdev(__qal_netdev_t dev,
					  const unsigned char *addr,
					  uint16_t vid,
					  uint16_t state)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif /* __I_QAL_BRIDGE_H */
