/*
 * Copyright (c) 2014-2020 The Linux Foundation. All rights reserved.
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

#if !defined(__QDF_NET_IF_H)
#define __QDF_NET_IF_H

/* Include Files */
#include <qdf_types.h>
#include <i_qdf_net_if.h>

struct qdf_net_if;

#ifdef ENHANCED_OS_ABSTRACTION
/**
 * qdf_net_if_create_dummy_if() - create dummy interface
 * @nif: interface handle
 *
 * This function will create a dummy network interface
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qdf_net_if_create_dummy_if(struct qdf_net_if *nif);

/**
 * qdf_net_if_get_dev_by_name() - Find a network device by its name
 * @nif_name: network device name
 *
 * This function retrieves the network device by its name
 *
 * Return: qdf network device
 */
struct qdf_net_if *
qdf_net_if_get_dev_by_name(char *nif_name);

/**
 * qdf_net_if_release_dev() - Release reference to network device
 * @nif: network device
 *
 * This function releases reference to the network device
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS
qdf_net_if_release_dev(struct qdf_net_if *nif);

/**
 * qdf_napi_enable() - Enable the napi schedule
 * @napi: NAPI context
 *
 * This function resume NAPI from being scheduled on this context
 *
 * Return: NONE
 */
void qdf_napi_enable(struct napi_struct *napi);

/**
 * qdf_napi_disable() - Disable the napi schedule
 * @napi: NAPI context
 *
 * This function suspends NAPI from being scheduled on this context
 *
 * Return: NONE
 */
void qdf_napi_disable(struct napi_struct *napi);

/**
 * qdf_netif_napi_add - initialize a NAPI context
 * @netdev:  network device
 * @napi: NAPI context
 * @poll: polling function
 * @weight: default weight
 *
 * Return: NONE
 */
void qdf_netif_napi_add(struct net_device *netdev, struct napi_struct *napi,
			int (*poll)(struct napi_struct *, int), int weight);

/**
 *  qdf_netif_napi_del - remove a NAPI context
 *  @napi: NAPI context
 *
 *  Return: NONE
 */
void qdf_netif_napi_del(struct napi_struct *napi);

/**
 * qdf_net_update_net_device_dev_addr() - update net_device dev_addr
 * @ndev: net_device
 * @src_addr: source mac address
 * @len: length
 *
 * This function updates dev_addr in net_device
 *
 * Return: void
 */
void
qdf_net_update_net_device_dev_addr(struct net_device *ndev,
				   const void *src_addr,
				   size_t len);
#else /* ENHANCED_OS_ABSTRACTION */
static inline QDF_STATUS
qdf_net_if_create_dummy_if(struct qdf_net_if *nif)
{
	return __qdf_net_if_create_dummy_if(nif);
}

static inline struct qdf_net_if *
qdf_net_if_get_dev_by_name(char *nif_name)
{
	return __qdf_net_if_get_dev_by_name(nif_name);
}

static inline QDF_STATUS
qdf_net_if_release_dev(struct qdf_net_if *nif)
{
	return __qdf_net_if_release_dev(nif);
}

/**
 * qdf_net_update_net_device_dev_addr() - update net_device dev_addr
 * @ndev: net_device
 * @src_addr: source mac address
 * @len: length
 *
 * This function updates dev_addr in net_device
 *
 * Return: void
 */
static inline void
qdf_net_update_net_device_dev_addr(struct net_device *ndev,
				   const void *src_addr,
				   size_t len)
{
	__qdf_net_update_net_device_dev_addr(ndev, src_addr, len);
}

static inline void
qdf_napi_enable(struct napi_struct *napi)
{
	__qdf_napi_enable(napi);
}

static inline void
qdf_napi_disable(struct napi_struct *napi)
{
	__qdf_napi_disable(napi);
}

static inline void
qdf_netif_napi_add(struct net_device *netdev, struct napi_struct *napi,
		   int (*poll)(struct napi_struct *, int), int weight)
{
	__qdf_netif_napi_add(netdev, napi, poll, weight);
}

static inline void
qdf_netif_napi_del(struct napi_struct *napi)
{
	__qdf_netif_napi_del(napi);
}
#endif /* ENHANCED_OS_ABSTRACTION */

/**
 * qdf_net_if_get_devname() - Retrieve netdevice name
 * @nif: Abstraction of netdevice
 *
 * Return: netdevice name
 */
char *qdf_net_if_get_devname(struct qdf_net_if *nif);
#endif /* __QDF_NET_IF_H */
