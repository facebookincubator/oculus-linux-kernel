/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: qdf_net_if
 * QCA driver framework (QDF) network interface management APIs
 */

#if !defined(__I_QDF_NET_IF_H)
#define __I_QDF_NET_IF_H

/* Include Files */
#include <qdf_types.h>
#include <qdf_util.h>
#include <linux/netdevice.h>

struct qdf_net_if;

/**
 * __qdf_net_if_create_dummy_if() - create dummy interface
 * @nif: interface handle
 *
 * This function will create a dummy network interface
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static inline QDF_STATUS
__qdf_net_if_create_dummy_if(struct qdf_net_if *nif)
{
	int ret;

	ret = init_dummy_netdev((struct net_device *)nif);

	return qdf_status_from_os_return(ret);
}

/**
 * __qdf_net_if_get_dev_by_name() - Find a network device by its name
 * @nif_name: network device name
 *
 * This function retrieves the network device by its name
 *
 * Return: qdf network device
 */
static inline struct qdf_net_if *
__qdf_net_if_get_dev_by_name(char *nif_name)
{
	if (!nif_name)
		return NULL;

	return ((struct qdf_net_if *)dev_get_by_name(&init_net, nif_name));
}

/**
 * __qdf_net_if_release_dev() - Release reference to network device
 * @nif: network device
 *
 * This function releases reference to the network device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static inline QDF_STATUS
__qdf_net_if_release_dev(struct qdf_net_if  *nif)
{
	if (!nif)
		return QDF_STATUS_E_INVAL;

	dev_put((struct net_device *)nif);

	return QDF_STATUS_SUCCESS;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0))
/**
 * __qdf_net_update_net_device_dev_addr() - update net_device dev_addr
 * @ndev: net_device
 * @src_addr: source mac address
 * @len: length
 *
 * kernel version 5.17 onwards made net_device->dev_addr as const unsigned char*
 * so to update dev_addr, this function calls kernel api dev_addr_mod.
 *
 * Return: void
 */
static inline void
__qdf_net_update_net_device_dev_addr(struct net_device *ndev,
				     const void *src_addr,
				     size_t len)
{
	dev_addr_mod(ndev, 0, src_addr, len);
}
#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)) */
/**
 * __qdf_net_update_net_device_dev_addr() - update net_device dev_addr
 * @ndev: net_device
 * @src_addr: source mac address
 * @len: length
 *
 * This function updates dev_addr in net_device using mem copy.
 *
 * Return: void
 */
static inline void
__qdf_net_update_net_device_dev_addr(struct net_device *ndev,
				     const void *src_addr,
				     size_t len)
{
	memcpy(ndev->dev_addr, src_addr, len);
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)) */

/**
 * __qdf_napi_enable() - Enable the napi schedule
 * @napi: NAPI context
 *
 * This function resume NAPI from being scheduled on this context
 *
 * Return: NONE
 */
static inline void
__qdf_napi_enable(struct napi_struct *napi)
{
	napi_enable(napi);
}

/**
 * __qdf_napi_disable() - Disable the napi schedule
 * @napi: NAPI context
 *
 * This function suspends NAPI from being scheduled on this context
 *
 * Return: NONE
 */
static inline void
__qdf_napi_disable(struct napi_struct *napi)
{
	napi_disable(napi);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0))
/**
 * __qdf_netif_napi_add - initialize a NAPI context
 * @netdev:  network device
 * @napi: NAPI context
 * @poll: polling function
 * @weight: default weight
 *
 * Upstream commit b48b89f9c189 ("net: drop the weight argument from
 * netif_napi_add") was introduced in Linux 6.1.  As described by the
 * subject, this removes the weight argument from netif_napi_add().
 *
 * This was preceded by commit 58caed3dacb4 ("netdev: reshuffle
 * netif_napi_add() APIs to allow dropping weight") in Linux 5.19
 * which added new APIs to call when a non-default weight wishes to be
 * sent.
 *
 * Return: NONE
 */
static inline void
__qdf_netif_napi_add(struct net_device *netdev, struct napi_struct *napi,
		     int (*poll)(struct napi_struct *, int), int weight)
{
	netif_napi_add_weight(netdev, napi, poll, weight);
}
#else
static inline void
__qdf_netif_napi_add(struct net_device *netdev, struct napi_struct *napi,
		     int (*poll)(struct napi_struct *, int), int weight)
{
	netif_napi_add(netdev, napi, poll, weight);
}
#endif

/**
 * __qdf_netif_napi_del: remove a NAPI context
 * @napi: NAPI context
 *
 * Return: NONE
 */
static inline void
__qdf_netif_napi_del(struct napi_struct *napi)
{
	netif_napi_del(napi);
}

#endif /*__I_QDF_NET_IF_H */
