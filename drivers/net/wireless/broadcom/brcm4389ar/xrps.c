// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * @file xrps.c
 * @brief Implementation of XRPS interfaces.
 *
 *****************************************************************************/

#ifdef __linux__
#include <linux/errno.h>
#include <linux/string.h>
#endif

#include "xrps.h"
#ifndef __linux__
#include <stdbool.h>
// Redefinition in include/linux/types.h
#include <stdint.h>
#endif
#include "xrps_osl.h"

#ifdef CONFIG_XRPS_PROFILING
#include "xrps_profiling.h"
#endif /* CONFIG_XRPS_PROFILING */

#ifdef CONFIG_XRPS

XRPS_LOG_DECLARE();

static bool xrps_handle_flowring(int flowid);
static void xrps_txcmplt_cb(bool status, bool active_tx);
static void xrps_eot_handler(void *pktbuf);
static void xrps_rx_cb(void);
#ifdef CONFIG_XRPS_PROFILING
static void xrps_put_profiling_event(enum profile_event_type type);
#endif /* CONFIG_XRPS_PROFILING */
static void reset_flowrings(void);

static struct xrps xrps = {
	.xrps_intf = {
		.handle_flowring = xrps_handle_flowring,
		.txcmplt_cb = xrps_txcmplt_cb,
		.rx_eot_cb = xrps_eot_handler,
		.rx_cb = xrps_rx_cb,
#ifdef CONFIG_XRPS_PROFILING
		.put_profiling_event = xrps_put_profiling_event,
#endif /* CONFIG_XRPS_PROFILING */
	},
};

#ifdef CONFIG_XRPS_PROFILING
static void xrps_init_profiling_helper(void)
{
	xrps_init_profiling(xrps.osl_intf, &xrps.stats.profiling_events);
}

static void xrps_put_profiling_event(enum profile_event_type type)
{
	put_profiling_event(&xrps.stats.profiling_events, type);
}
#endif /* CONFIG_XRPS_PROFILING */

static bool is_flowid_stored(int flowid)
{
	int i;

	for (i = 0; i < xrps.flowids.next_flowid_idx; ++i) {
		if (xrps.flowids.flowid[i] == flowid)
			return true;
	}
	return false;
}

static bool store_flowid(int flowid)
{
	XRPS_LOG_DBG("XRAPI: store flowid %d", flowid);
	if (xrps.flowids.next_flowid_idx >= MAX_TX_FLOW_RINGS) {
		XRPS_LOG_WRN("XRAPI: reached max flowid");
		return false;
	}

	xrps.flowids.flowid[xrps.flowids.next_flowid_idx] = flowid;
	++xrps.flowids.next_flowid_idx;
	return true;
}

static void reset_flowrings(void)
{
	xrps.flowids.next_flowid_idx = 0;
}

// This can be called from xrps or dhd contexts.
static bool xrps_handle_flowring(int flowid)
{
	xrps_osl_spinlock_flag_t flag;
	bool handled = false;

	if (!xrps.drv_intf->flowring_has_work_to_do(flowid))
		return false;

	flag = xrps.osl_intf->spin_lock(&xrps.lock);

	if (xrps.queue_pause)
		handled = (is_flowid_stored(flowid) || store_flowid(flowid));

	xrps.osl_intf->spin_unlock(&xrps.lock, flag);

	return handled;
}

static void xrps_txcmplt_cb(bool status, bool active_tx)
{
	xrps_osl_spinlock_flag_t flag;
	if (xrps.mode == XRPS_MODE_DISABLED)
		return;

	XRPS_LOG_DBG("%s", __func__);

	flag = xrps.osl_intf->spin_lock(&xrps.stats.lock);
	xrps.stats.data_txcmplt++;
	xrps.osl_intf->spin_unlock(&xrps.stats.lock, flag);
}

static void unpause_queue_with_flush(void)
{
	xrps_set_queue_pause(0);
}

static bool is_tx_pending(void)
{
	return (xrps.flowids.next_flowid_idx != 0);
}

/**
 * @brief Unpause queue, pause queue, send EOT.
 *
 * @param[in] eot_workq If true, submit EOT work to workqueue. This is needed because ioctl must not be done in sysworkq context.
 */
static void unpause_pause_and_send_eot(bool eot_workq)
{
	bool tx_pending = is_tx_pending();

	if (tx_pending) {
		unpause_queue_with_flush();
		xrps_set_queue_pause(1);
	}

	if (eot_workq) {
#ifdef CONFIG_XRPS_PROFILING
		xrps_put_profiling_event(EOT_SUBMIT);
#endif /* CONFIG_XRPS_PROFILING */

		xrps.osl_intf->submit_eot_work();
	} else {
		xrps_send_eot();
	}
}

static void xrps_rx_cb(void)
{
	if ((xrps.mode == XRPS_MODE_SLAVE) && xrps.first_rx_in_interval) {
		// No rx yet
		XRPS_LOG_DBG("%s", __func__);
#ifdef CONFIG_XRPS_PROFILING
		xrps_put_profiling_event(FIRST_RX_CB);
#endif /* CONFIG_XRPS_PROFILING */

		xrps.first_rx_in_interval = false;
		unpause_pause_and_send_eot(true);
	}
}

static void xrps_eot_handler(void *pktbuf)
{
	xrps_osl_spinlock_flag_t flag;
	XRPS_LOG_DBG("%s", __func__);

	if (xrps.mode == XRPS_MODE_SLAVE) {
#ifdef CONFIG_XRPS_PROFILING
		xrps_put_profiling_event(EOT_RX);
#endif /* CONFIG_XRPS_PROFILING */

		if (!xrps.first_rx_in_interval) {
			// Any RX after this point is considered RX of new interval.
			xrps.first_rx_in_interval = true;
		} else {
			// No data RX for this interval. Still need to do tx.
			unpause_pause_and_send_eot(true);
		}
	}

	flag = xrps.osl_intf->spin_lock(&xrps.stats.lock);
	xrps.stats.eot_rx++;
	xrps.osl_intf->spin_unlock(&xrps.stats.lock, flag);
}

static void unpause_all_queues(void)
{
	xrps_osl_spinlock_flag_t flag;
	uint64_t unhold_start = 0;
	uint64_t total_num_queued = 0;
	uint64_t latency = 0;
	int i = 0;

	if (!is_tx_pending())
		return;

#ifdef CONFIG_XRPS_PROFILING
	xrps_put_profiling_event(UNHOLD_START);
#endif /* CONFIG_XRPS_PROFILING */

	unhold_start = xrps.osl_intf->get_ktime();
	for (i = 0; i < xrps.flowids.next_flowid_idx; ++i) {
		int flowid = xrps.flowids.flowid[i];

		total_num_queued += xrps.drv_intf->get_num_queued(flowid);

		XRPS_LOG_DBG("xrps ringing doorbell %d", flowid);
		xrps.drv_intf->unpause_queue(flowid);
	}
	reset_flowrings();

#ifdef CONFIG_XRPS_PROFILING
	xrps_put_profiling_event(UNHOLD_END);
#endif /* CONFIG_XRPS_PROFILING */

	latency = xrps.osl_intf->ktime_to_us(xrps.osl_intf->get_ktime() -
					     unhold_start);

	flag = xrps.osl_intf->spin_lock(&xrps.stats.lock);
	xrps.stats.max_ring_latency_us =
		MAX(latency, xrps.stats.max_ring_latency_us);
	xrps.stats.avg_ring_latency_us =
		((latency + ((xrps.stats.unpause_queue - 1) *
			     xrps.stats.avg_ring_latency_us)) /
		 xrps.stats.unpause_queue);
	xrps.stats.max_num_queued =
		MAX(total_num_queued, xrps.stats.max_num_queued);
	xrps.stats.avg_num_queued =
		((total_num_queued + ((xrps.stats.unpause_queue - 1) *
				      xrps.stats.avg_num_queued)) /
		 xrps.stats.unpause_queue);
	xrps.osl_intf->spin_unlock(&xrps.stats.lock, flag);
}

int xrps_get_queue_pause(void)
{
	return xrps.queue_pause;
}

void xrps_set_queue_pause(int en)
{
	xrps_osl_spinlock_flag_t flag;
	XRPS_LOG_DBG("%s %d", __func__, en);

	if (en == xrps.queue_pause) {
		XRPS_LOG_WRN("redundant queue_pause");
		return;
	}

	if (en) {
		flag = xrps.osl_intf->spin_lock(&xrps.stats.lock);
		++xrps.stats.pause_queue;
		xrps.osl_intf->spin_unlock(&xrps.stats.lock, flag);

		if (is_tx_pending()) {
			// This is OK if the flush timer/work is still pending.
			XRPS_LOG_WRN(
				"queue paused with tx pending %d %d",
				xrps.flowids.next_flowid_idx,
				xrps.flowids.flowid[xrps.flowids.next_flowid_idx -
						    1]);
		}

		flag = xrps.osl_intf->spin_lock(&xrps.lock);
		xrps.queue_pause = 1;
		xrps.osl_intf->spin_unlock(&xrps.lock, flag);

	} else {
		flag = xrps.osl_intf->spin_lock(&xrps.stats.lock);
		++xrps.stats.unpause_queue;
		xrps.osl_intf->spin_unlock(&xrps.stats.lock, flag);

		flag = xrps.osl_intf->spin_lock(&xrps.lock);
		xrps.queue_pause = 0;
		xrps.osl_intf->spin_unlock(&xrps.lock, flag);

		// Grabbing lock in here results in deadlock due to dhd invocation of handle_flowring.
		unpause_all_queues();
	}
}

int xrps_send_eot(void)
{
	int ret = 0;
	xrps_osl_spinlock_flag_t flag;
#ifdef CONFIG_XRPS_PROFILING
	xrps_put_profiling_event(EOT_TX);
#endif /* CONFIG_XRPS_PROFILING */

	ret = xrps.drv_intf->send_eot();
	if (ret) {
		XRPS_LOG_ERR("send_eot failed %d", ret);
		flag = xrps.osl_intf->spin_lock(&xrps.stats.lock);
		++xrps.stats.send_eot_fail;
		xrps.osl_intf->spin_unlock(&xrps.stats.lock, flag);
		return ret;
	}
	XRPS_LOG_DBG("%s", __func__);
	flag = xrps.osl_intf->spin_lock(&xrps.stats.lock);
	xrps.stats.eot_tx++;
	xrps.osl_intf->spin_unlock(&xrps.stats.lock, flag);

#ifdef CONFIG_XRPS_PROFILING
	dump_profiling_events(&xrps.stats.profiling_events);
#endif // CONFIG_XRPS_PROFILING

	return 0;
}

void xrps_pause(bool check_mode)
{
	if (check_mode && (xrps_get_mode() != XRPS_MODE_MASTER))
		return;

	xrps.osl_intf->stop_sysint_timer();
	xrps_set_queue_pause(0);
#ifdef CONFIG_XRPS_PROFILING
	dump_profiling_events(&xrps.stats.profiling_events);
#endif /* CONFIG_XRPS_PROFILING */
}

void xrps_resume(bool check_mode)
{
	if (!xrps.drv_intf->is_link_up() || (check_mode && (xrps_get_mode() != XRPS_MODE_MASTER)))
		return;

	xrps_set_queue_pause(1);
	xrps.osl_intf->start_sysint_timer(xrps_get_sys_interval_us());
}

int xrps_get_mode(void)
{
	return (int)xrps.mode;
}

int xrps_set_mode(enum xrps_mode mode)
{
	xrps_osl_spinlock_flag_t flag;

	if (mode < 0 || mode >= XRPS_MODE_COUNT)
		return -EINVAL;

	flag = xrps.osl_intf->spin_lock(&xrps.lock);
	xrps.mode = mode;
	xrps.osl_intf->spin_unlock(&xrps.lock, flag);

	xrps.mode = mode;
	switch (mode) {
	case XRPS_MODE_DISABLED:
		xrps_pause(false);
		break;
	case XRPS_MODE_MASTER:
		if (!xrps_is_init())
			return -ENODEV;
		xrps_resume(false);
		break;
	case XRPS_MODE_SLAVE:
		if (!xrps_is_init())
			return -ENODEV;
		xrps.osl_intf->stop_sysint_timer();
		xrps.first_rx_in_interval = true;
		xrps_set_queue_pause(1);
		break;
	default:
		break;
	};

	return 0;
}

int xrps_get_sys_interval_us(void)
{
	return xrps.sys_interval;
}

int xrps_set_sys_interval_us(uint32_t us)
{
	if ((us / 1000U) < XRPS_MIN_SYS_INT_MS ||
	    (us / 1000U) > XRPS_MAX_SYS_INT_MS)
		return -EINVAL;
	xrps.sys_interval = us;
	return 0;
}

void xrps_get_stats(struct xrps_stats *dest)
{
	xrps_osl_spinlock_flag_t flag =
		xrps.osl_intf->spin_lock(&xrps.stats.lock);
	memcpy(dest, &xrps.stats, sizeof(xrps.stats));
	xrps.osl_intf->spin_unlock(&xrps.stats.lock, flag);
}

void xrps_clear_stats(void)
{
	xrps_osl_spinlock_flag_t flag =
		xrps.osl_intf->spin_lock(&xrps.stats.lock);
	memset(&xrps.stats, 0, sizeof(xrps.stats));
	xrps.osl_intf->spin_unlock(&xrps.stats.lock, flag);
#ifdef CONFIG_XRPS_PROFILING
	xrps_init_profiling_helper();
#endif /* CONFIG_XRPS_PROFILING */
}

void xrps_sysint_handler(void)
{
	xrps_osl_spinlock_flag_t flag;
	XRPS_LOG_DBG("%s", __func__);
	flag = xrps.osl_intf->spin_lock(&xrps.stats.lock);
	xrps.stats.sys_ints++;
	xrps.osl_intf->spin_unlock(&xrps.stats.lock, flag);
	unpause_pause_and_send_eot(false);
}

bool xrps_is_init(void)
{
	return xrps.is_init;
}

int xrps_init(void)
{
	int ret;

	XRPS_LOG_INF("%s", __func__);

	xrps.is_init = false;

	ret = xrps_init_osl_intf(&xrps.osl_intf);
	if (ret) {
		XRPS_LOG_ERR("xrps_init_osl_intf failed, err=%d", ret);
		return ret;
	}

	ret = xrps.osl_intf->init_osl(&xrps);
	if (ret) {
		XRPS_LOG_ERR("init_osl failed, err=%d", ret);
		return ret;
	}

	xrps.osl_intf->spin_lock_init(&xrps.lock);
	xrps.osl_intf->spin_lock_init(&xrps.stats.lock);
	xrps.sys_interval = 100000;
	reset_flowrings();
	xrps.first_rx_in_interval = true;
#ifdef CONFIG_XRPS_PROFILING
	xrps_init_profiling_helper();
#endif /* CONFIG_XRPS_PROFILING */
	ret = dhd_xrps_init(&xrps.xrps_intf, &xrps.drv_intf);
	if (ret) {
		XRPS_LOG_ERR("dhd_xrps_init failed, err=%d", ret);
		xrps.osl_intf->cleanup_osl(&xrps);
		return ret;
	}

	xrps.is_init = true;
	return 0;
}

void xrps_cleanup(void)
{
	if (!xrps_is_init())
		return;

	XRPS_LOG_INF("%s", __func__);
	xrps_set_queue_pause(0);
#ifdef CONFIG_XRPS_PROFILING
	dump_profiling_events(&xrps.stats.profiling_events);
#endif // CONFIG_XRPS_PROFILING
	xrps.osl_intf->cleanup_osl(&xrps);
	dhd_xrps_init(NULL, NULL);
	xrps.is_init = false;
}

#endif /* CONFIG_XRPS */
