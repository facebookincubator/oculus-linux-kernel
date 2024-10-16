/*
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
 * DOC: qdf_net_stats_public API
 * This file defines the net dev stats abstraction.
 */

#if !defined(__QDF_NET_STATS_H)
#define __QDF_NET_STATS_H

#include <qdf_types.h>
#include <i_qdf_net_stats.h>
#include <qdf_net_types.h>

/**
 * qdf_net_stats_add_rx_pkts() - Add RX pkts in n/w stats
 * @stats: Network stats instance
 * @value: Value to be added
 *
 * Return: None.
 */
static inline
void qdf_net_stats_add_rx_pkts(qdf_net_dev_stats *stats, uint32_t value)
{
	__qdf_net_stats_add_rx_pkts(stats, value);
}

/**
 * qdf_net_stats_get_rx_pkts() - Get RX pkts in net stats
 * @stats: Network stats instance
 *
 * Return: Rx packets received on N/W interface
 */
static inline
unsigned long qdf_net_stats_get_rx_pkts(qdf_net_dev_stats *stats)
{
	return __qdf_net_stats_get_rx_pkts(stats);
}

/**
 * qdf_net_stats_add_rx_bytes() - Add RX bytes in n/w stats
 * @stats: Network stats instance
 * @value: Value to be added
 *
 * Return: None.
 */
static inline
void qdf_net_stats_add_rx_bytes(qdf_net_dev_stats *stats, uint32_t value)
{
	__qdf_net_stats_add_rx_bytes(stats, value);
}

/**
 * qdf_net_stats_get_rx_bytes() - Get RX bytes in net stats
 * @stats: Network stats instance
 *
 * Return: Rx bytes received on N/W interface
 */
static inline
unsigned long qdf_net_stats_get_rx_bytes(qdf_net_dev_stats *stats)
{
	return __qdf_net_stats_get_rx_bytes(stats);
}

/**
 * qdf_net_stats_inc_rx_errors() - inc RX errors n/w stats
 * @stats: Network stats instance
 *
 * Return: None.
 */
static inline
void qdf_net_stats_inc_rx_errors(qdf_net_dev_stats *stats)
{
	__qdf_net_stats_inc_rx_errors(stats);
}

/**
 * qdf_net_stats_get_rx_errors() - Get RX errors in net stats
 * @stats: Network stats instance
 *
 * Return: Rx packet errors on N/W interface
 */
static inline
unsigned long qdf_net_stats_get_rx_errors(qdf_net_dev_stats *stats)
{
	return __qdf_net_stats_get_rx_errors(stats);
}

/**
 * qdf_net_stats_inc_rx_dropped() - inc RX dropped n/w stats
 * @stats: Network stats instance
 *
 * Return: None.
 */
static inline
void qdf_net_stats_inc_rx_dropped(qdf_net_dev_stats *stats)
{
	__qdf_net_stats_inc_rx_dropped(stats);
}

/**
 * qdf_net_stats_get_rx_dropped() - Get RX dropped in net stats
 * @stats: Network stats instance
 *
 * Return: Rx packet dropped on N/W interface
 */
static inline
unsigned long qdf_net_stats_get_rx_dropped(qdf_net_dev_stats *stats)
{
	return __qdf_net_stats_get_rx_dropped(stats);
}

/**
 * qdf_net_stats_add_tx_pkts() - Add Tx packets in n/w stats
 * @stats: Network stats instance
 * @value: Value to be added
 *
 * Return: None.
 */
static inline
void qdf_net_stats_add_tx_pkts(qdf_net_dev_stats *stats, uint32_t value)
{
	__qdf_net_stats_add_tx_pkts(stats, value);
}

/**
 * qdf_net_stats_get_tx_pkts() - Get Tx packets in net stats
 * @stats: Network stats instance
 *
 * Return: Tx packets transmitted on N/W interface
 */
static inline
unsigned long qdf_net_stats_get_tx_pkts(qdf_net_dev_stats *stats)
{
	return __qdf_net_stats_get_tx_pkts(stats);
}

/**
 * qdf_net_stats_add_tx_bytes() - Add Tx bytes in n/w stats
 * @stats: Network stats instance
 * @value: Value to be added
 *
 * Return: None.
 */
static inline
void qdf_net_stats_add_tx_bytes(qdf_net_dev_stats *stats, uint32_t value)
{
	__qdf_net_stats_add_tx_bytes(stats, value);
}

/**
 * qdf_net_stats_get_tx_bytes() - Get Tx bytes in net stats
 * @stats: Network stats instance
 *
 * Return: Tx bytes transmitted on N/W interface
 */
static inline
unsigned long qdf_net_stats_get_tx_bytes(qdf_net_dev_stats *stats)
{
	return __qdf_net_stats_get_tx_bytes(stats);
}

/**
 * qdf_net_stats_inc_tx_errors() - inc Tx errors n/w stats
 * @stats: Network stats instance
 *
 * Return: None.
 */
static inline
void qdf_net_stats_inc_tx_errors(qdf_net_dev_stats *stats)
{
	__qdf_net_stats_inc_tx_errors(stats);
}

/**
 * qdf_net_stats_get_tx_errors() - Get Tx errors in net stats
 * @stats: Network stats instance
 *
 * Return: Tx errors on N/W interface
 */
static inline
unsigned long qdf_net_stats_get_tx_errors(qdf_net_dev_stats *stats)
{
	return __qdf_net_stats_get_tx_errors(stats);
}

/**
 * qdf_net_stats_inc_tx_dropped() - inc Tx dropped n/w stats
 * @stats: Network stats instance
 *
 * Return: None.
 */
static inline
void qdf_net_stats_inc_tx_dropped(qdf_net_dev_stats *stats)
{
	__qdf_net_stats_inc_tx_dropped(stats);
}

/**
 * qdf_net_stats_get_tx_dropped() - Get Tx dropped in net stats
 * @stats: Network stats instance
 *
 * Return: Tx dropped on N/W interface
 */
static inline
unsigned long qdf_net_stats_get_tx_dropped(qdf_net_dev_stats *stats)
{
	return __qdf_net_stats_get_tx_dropped(stats);
}
#endif /*__QDF_NET_STATS_H*/
