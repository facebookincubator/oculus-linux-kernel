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
 * DOC: qdf_net_dev_stats
 * QCA driver framework (QDF) network interface stats management APIs
 */

#if !defined(__I_QDF_NET_STATS_H)
#define __I_QDF_NET_STATS_H

/* Include Files */
#include <qdf_types.h>
#include <qdf_util.h>
#include <linux/netdevice.h>

/**
 * __qdf_net_stats_add_rx_pkts() - Add RX pkts in n/w stats
 * @stats: Network stats instance
 * @value: Value to be added
 *
 * Return: None.
 */
static inline
void __qdf_net_stats_add_rx_pkts(struct net_device_stats *stats, uint32_t value)
{
	stats->rx_packets += value;
}

/**
 * __qdf_net_stats_get_rx_pkts() - Get RX pkts in net stats
 * @stats: Network stats instance
 *
 * Return: Rx packets received on N/W interface
 */
static inline
unsigned long __qdf_net_stats_get_rx_pkts(struct net_device_stats *stats)
{
	return stats->rx_packets;
}

/**
 * __qdf_net_stats_add_rx_bytes() - Add RX bytes in n/w stats
 * @stats: Network stats instance
 * @value: Value to be added
 *
 * Return: None.
 */
static inline
void __qdf_net_stats_add_rx_bytes(struct net_device_stats *stats,
				  uint32_t value)
{
	stats->rx_bytes += value;
}

/**
 * __qdf_net_stats_get_rx_bytes() - Get RX bytes in net stats
 * @stats: Network stats instance
 *
 * Return: Rx bytes received on N/W interface
 */
static inline
unsigned long __qdf_net_stats_get_rx_bytes(struct net_device_stats *stats)
{
	return stats->rx_bytes;
}

/**
 * __qdf_net_stats_inc_rx_errors() - inc RX errors n/w stats
 * @stats: Network stats instance
 *
 * Return: None.
 */
static inline
void __qdf_net_stats_inc_rx_errors(struct net_device_stats *stats)
{
	stats->rx_errors++;
}

/**
 * __qdf_net_stats_get_rx_errors() - Get RX errors in net stats
 * @stats: Network stats instance
 *
 * Return: Rx packet errors on N/W interface
 */
static inline
unsigned long __qdf_net_stats_get_rx_errors(struct net_device_stats *stats)
{
	return stats->rx_errors;
}

/**
 * __qdf_net_stats_inc_rx_dropped() - inc RX dropped n/w stats
 * @stats: Network stats instance
 *
 * Return: None.
 */
static inline
void __qdf_net_stats_inc_rx_dropped(struct net_device_stats *stats)
{
	stats->rx_dropped++;
}

/**
 * __qdf_net_stats_get_rx_dropped() - Get RX dropped in net stats
 * @stats: Network stats instance
 *
 * Return: Rx packet dropped on N/W interface
 */
static inline
unsigned long __qdf_net_stats_get_rx_dropped(struct net_device_stats *stats)
{
	return stats->rx_dropped;
}

/**
 * __qdf_net_stats_add_tx_pkts() - Add Tx packets n/w stats
 * @stats: Network stats instance
 * @value: Value to be added
 *
 * Return: None.
 */
static inline
void __qdf_net_stats_add_tx_pkts(struct net_device_stats *stats, uint32_t value)
{
	stats->tx_packets += value;
}

/**
 * __qdf_net_stats_get_tx_pkts() - Get Tx packets in net stats
 * @stats: Network stats instance
 *
 * Return: Tx packets transmitted on N/W interface
 */
static inline
unsigned long __qdf_net_stats_get_tx_pkts(struct net_device_stats *stats)
{
	return stats->tx_packets;
}

/**
 * __qdf_net_stats_add_tx_bytes() - Add Tx bytes n/w stats
 * @stats: Network stats instance
 * @value: Value to be added
 *
 * Return: None.
 */
static inline
void __qdf_net_stats_add_tx_bytes(struct net_device_stats *stats,
				  uint32_t value)
{
	stats->tx_bytes += value;
}

/**
 * __qdf_net_stats_get_tx_bytes() - Get Tx bytes in net stats
 * @stats: Network stats instance
 *
 * Return: Tx bytes transmitted on N/W interface
 */
static inline
unsigned long __qdf_net_stats_get_tx_bytes(struct net_device_stats *stats)
{
	return stats->tx_bytes;
}

/**
 * __qdf_net_stats_inc_tx_errors() - inc Tx errors n/w stats
 * @stats: Network stats instance
 *
 * Return: None.
 */
static inline
void __qdf_net_stats_inc_tx_errors(struct net_device_stats *stats)
{
	stats->tx_errors++;
}

/**
 * __qdf_net_stats_get_tx_errors() - Get Tx errors in net stats
 * @stats: Network stats instance
 *
 * Return: Tx errors on N/W interface
 */
static inline
unsigned long __qdf_net_stats_get_tx_errors(struct net_device_stats *stats)
{
	return stats->tx_errors;
}

/**
 * __qdf_net_stats_inc_tx_dropped() - inc Tx dropped n/w stats
 * @stats: Network stats instance
 *
 * Return: None.
 */
static inline
void __qdf_net_stats_inc_tx_dropped(struct net_device_stats *stats)
{
	stats->tx_dropped++;
}

/**
 * __qdf_net_stats_get_tx_dropped() - Get Tx dropped in net stats
 * @stats: Network stats instance
 *
 * Return: Tx dropped on N/W interface
 */
static inline
unsigned long __qdf_net_stats_get_tx_dropped(struct net_device_stats *stats)
{
	return stats->tx_dropped;
}
#endif /*__I_QDF_NET_STATS_H*/
