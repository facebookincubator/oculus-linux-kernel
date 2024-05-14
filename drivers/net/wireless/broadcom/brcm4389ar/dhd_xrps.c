// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * @file dhd_xrps.c
 * @brief Implementation of driver interfaces for XRPS
 *
 *****************************************************************************/

#ifdef __linux__
#include <linux/errno.h>
#elif __ZEPHYR__
#include <errno.h>
#endif

// XRPS module must be independent of DHD.
#ifdef CONFIG_XRPS_DHD_HOOKS

#include <typedefs.h>
#include <osl.h>
#include <dhd.h>
#include <dhd_dbg.h>
#include "dhd_bus.h"
#include <dhd_flowring.h>
#include "xrps.h"

#ifdef __linux__
#include <dhd_linux_priv.h>
#include <wl_android.h>
#elif __ZEPHYR__
#include "wl_api.h"
#endif

extern dhd_pub_t *g_dhd_pub;

static bool dhd_flowring_has_work_to_do(int flowid);
static uint16_t get_num_queued(int flowid);
static void dhd_xrps_unpause_queue(int flowid);
static int xrapi_send_eot(void);
static bool is_link_up(void);

struct xrps_drv_intf xrps_drv_intf = {
	.flowring_has_work_to_do = dhd_flowring_has_work_to_do,
	.get_num_queued = get_num_queued,
	.unpause_queue = dhd_xrps_unpause_queue,
	.send_eot = xrapi_send_eot,
	.is_link_up = is_link_up,
};

static uint16_t get_num_queued(int flowid)
{
	flow_ring_node_t *flow_ring_node;
	flow_queue_t *queue;

	if (g_dhd_pub->flow_ring_table == NULL)
		return 0;

	flow_ring_node = DHD_FLOW_RING(g_dhd_pub, flowid);
	if (flow_ring_node == NULL)
		return 0;

	queue = &flow_ring_node->queue;

	return queue->len;
}

static bool dhd_flowring_has_work_to_do(int flowid)
{
#ifdef __linux__
	static char *SAP_DNGL_NAME = "wl0.2";
	int ifindex;
#endif
	if (get_num_queued(flowid) == 0)
		return FALSE;

#ifdef __linux__
	ifindex = dhd_ifname2idx(g_dhd_pub->info, SAP_DNGL_NAME);
	if (dhd_flowid_find_by_ifidx(g_dhd_pub, ifindex, flowid) != BCME_OK)
		return FALSE;

#endif // __linux__

	return TRUE;
}

static void dhd_xrps_unpause_queue(int flowid)
{
	int ret;

	// There will usually be work to do, but there may not if executed in the non-tx path,
	// in which case, flow rings may be invalid.
	if (!dhd_flowring_has_work_to_do(flowid))
		return;

	ret = dhd_bus_schedule_queue(g_dhd_pub->bus, flowid, FALSE, 0, NULL);
	if (ret != BCME_OK) {
		// May occur if schedule_queue invoked from higher priority thread
		DHD_ERROR(("XRPS FATAL ERR %d\n", ret));
	}
}

int dhd_xrps_init(struct xrps_intf *xrps_intf, struct xrps_drv_intf **drv_intf)
{
	DHD_ERROR(("%s\n", __func__));
	g_dhd_pub->xrps_intf = NULL;
	if (!drv_intf || !xrps_intf)
		return -EINVAL;
	/* assuming that dhd_pub_t type pointer is available from a global variable */
	g_dhd_pub->xrps_intf = xrps_intf;
	*drv_intf = &xrps_drv_intf;
	return 0;
}

static int xrapi_send_eot(void)
{
	static char *SAP_DNGL_NAME = "wl0.2";
	int ifindex = dhd_ifname2idx(g_dhd_pub->info, SAP_DNGL_NAME);
	struct net_device *dev = dhd_idx2net(g_dhd_pub, ifindex);

	return wl_send_txdone_indication(dev);
}

static bool is_link_up(void)
{
	static char *SAP_DNGL_NAME = "wl0.2";
	int ifindex = dhd_ifname2idx(g_dhd_pub->info, SAP_DNGL_NAME);
	dhd_if_t const *ifp = dhd_get_ifp(g_dhd_pub, ifindex);

	return (list_empty(&ifp->sta_list) == 0);
}

#else

int dhd_xrps_init(void *xrps_intf, void **drv_intf)
{
	return -EINVAL;
}

#endif /* CONFIG_XRPS_DHD_HOOKS */
