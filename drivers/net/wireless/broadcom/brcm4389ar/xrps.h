/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 * @file xrps.h
 * @brief XR Power Save (XRPS) interfaces, type definitions, constant declarations
 *
 * @details XRPS saves power by transmitting data in batches. There are two
 * modes of operation: master and slave. In master mode, transmission is
 * dictated by a system interval. In slave mode, transmission is allowed after
 * first data reception from master. After burst transmission is finished, an
 * end-of-transmission (EOT) packet is sent.
 *
 *****************************************************************************/

#ifndef XRPS_H
#define XRPS_H

#ifdef CONFIG_XRPS

#ifndef __linux__
#include <stdbool.h>
// Redefinition in linux/types.h
#include <stdint.h>
#endif
#include "xrps_osl.h"
#ifdef CONFIG_XRPS_PROFILING
#include "xrps_profiling.h"
#endif /* CONFIG_XRPS_PROFILING */

#define XRPS_MIN_SYS_INT_MS 15
#define XRPS_MAX_SYS_INT_MS 10000

#define MAX_TX_FLOW_RINGS 40

enum xrps_mode {
	XRPS_MODE_DISABLED,
	XRPS_MODE_MASTER,
	XRPS_MODE_SLAVE,
	XRPS_MODE_COUNT,
};

// XRPS interfaces
struct xrps_intf {
	/**
	 * @brief Handle the flow ring.
	 *
	 * The flow ring is handled if it will be transmitted at a later time.
	 *
	 * @param[in] flowid The ID of the flow ring
	 *
	 * @return True if flow ring has been handled. Else false.
	 */
	bool (*handle_flowring)(int flowid);

	/**
	 * @brief Called from TX completion path when tx status is received.
	 *
	 * @param[in] status TX complete status
	 * @param[in] active_tx True if there is outstanding data for TX.
	 */
	void (*txcmplt_cb)(bool status, bool active_tx);

	/**
	 * @brief Called from RX path when EoT is received from peer.
	 *
	 * @param[in] pktbuf A pointer to the packet buffer
	 */
	void (*rx_eot_cb)(void *pktbuf);

	/**
	 * @brief RX callback
	 */
	void (*rx_cb)(void);

#ifdef CONFIG_XRPS_PROFILING
	/**
	 * @brief Put a profiling event
	 *
	 * @param[in] type The profiling event type
	 */
	void (*put_profiling_event)(enum profile_event_type type);
#endif /* CONFIG_XRPS_PROFILING */
};

// Driver interfaces for XRPS
struct xrps_drv_intf {
	/**
	 * @brief Return true if the flow ring has work to do.
	 *
	 * @param[in] flowid The ID of the flow ring
	 */
	bool (*flowring_has_work_to_do)(int flowid);

	/**
	 * @brief Get the number of queued elements in the flow ring.
	 *
	 * @param[in] flowid The ID of the flow ring
	 */
	uint16_t (*get_num_queued)(int flowid);

	/**
	 * @brief Unhold the flow ring.
	 *
	 * The contents in the flow ring queue will be moved to the flow ring.
	 *
	 * @param[in] flowid The ID of the flow ring
	 */
	void (*unpause_queue)(int flowid);

	/**
	 * @brief Send EOT packet.
	 *
	 * @return The return value from sending EOT packet.
	 */
	int (*send_eot)(void);
};

// XRPS statistics
struct xrps_stats {
	xrps_osl_spinlock_t lock;
	uint64_t eot_tx;
	uint64_t eot_rx;
	uint64_t data_txcmplt;
	uint64_t sys_ints;
	uint64_t max_ring_latency_us;
	uint64_t avg_ring_latency_us;
	uint64_t send_eot_fail;
	uint64_t max_num_queued; // Max number of packets queued before queue unpaused.
	uint64_t avg_num_queued; // Avg number of packets queued before queue unpaused.
	uint64_t unpause_queue;
	uint64_t pause_queue;
#ifdef CONFIG_XRPS_PROFILING
	struct profiling_event_array profiling_events;
#endif /* CONFIG_XRPS_PROFILING */
};

// Data structure to hold flowid's
struct flowids {
	int flowid[MAX_TX_FLOW_RINGS];
	int next_flowid_idx; // Track next position in flowid.
};

// XRPS state
struct xrps {
	bool is_init;
	struct xrps_intf xrps_intf;
	struct xrps_drv_intf *drv_intf;
	struct xrps_osl_intf *osl_intf;
	xrps_osl_spinlock_t lock;
	enum xrps_mode mode;
	uint8_t queue_pause;
	uint32_t sys_interval;
	struct flowids flowids;
	struct xrps_stats stats;
	/**
	 * For slave only. Flag that indicates first RX (data or EOT) for an interval.
	 * An interval is completed by EOT received.
	 */
	bool first_rx_in_interval;
};

/**
 * @brief Return true iff XRPS is initialized.
 */
bool xrps_is_init(void);

/**
 * @brief Initialize XRPS.
 *
 * @return Error code, else 0 on success.
 */
int xrps_init(void);

/**
 * @brief Clean up XRPS.
 */
void xrps_cleanup(void);

/**
 * @brief Get XRPS system interval in us.
 */
int xrps_get_sys_interval_us(void);

/**
 * @brief Set XRPS system interval in us.
 *
 * @param[in] us The new system interval
 *
 * @return Error code, else 0 on success.
 */
int xrps_set_sys_interval_us(uint32_t us);

/**
 * @brief Get XRPS mode.
 */
int xrps_get_mode(void);

/**
 * @brief Set XRPS mode.
 *
 * @param[in] mode The new mode
 *
 * @return Error code, else 0 on success.
 */
int xrps_set_mode(enum xrps_mode mode);

/**
 * @brief Get queue pause.
 */
int xrps_get_queue_pause(void);

/**
 * @brief Set queue pause.
 *
 * @param[in] en The new setting
 */
void xrps_set_queue_pause(int en);

/**
 * @brief Get XRPS stats.
 *
 * @param[in] dest The statistics will be copied into this buffer
 */
void xrps_get_stats(struct xrps_stats *dest);

/**
 * @brief Clear XRPS stats.
 */
void xrps_clear_stats(void);

/**
 * @brief Send EOT packet.
 *
 * @note FW sends EoT in sequence; no need to wait for txcomplete.
 *       EOT is not blocked by queue pause.
 *
 * @return The return value from sending EOT packet.
 */
int xrps_send_eot(void);

/**
 * @brief Sysint handler
 */
void xrps_sysint_handler(void);

/**
 * @brief Initialize driver interface for XRPS.
 *
 * This is an advance declaration to be defined by the driver.
 *
 * @param[in] xrps_intf Pointer to XRPS interface
 * @param[in] drv_intf Pointer to driver interface
 *
 * @return Error code, else 0 on success.
 */
int dhd_xrps_init(struct xrps_intf *xrps_intf, struct xrps_drv_intf **drv_intf);

#endif /* CONFIG_XRPS */

#endif
