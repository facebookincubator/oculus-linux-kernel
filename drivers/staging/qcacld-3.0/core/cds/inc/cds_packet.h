/*
 * Copyright (c) 2014-2016, 2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#if !defined(__CDS_PKT_H)
#define __CDS_PKT_H

/**
 * DOC: cds_packet.h
 *      Connectivity driver services (CDS) network Packet APIs
 *      Network Protocol packet/buffer support interfaces
 */

#include <qdf_types.h>
#include <qdf_status.h>

/*--------------------------------------------------------------------------
   Preprocessor definitions and constants
   ------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
   Type declarations
   ------------------------------------------------------------------------*/
struct cds_pkt_t;
typedef struct cds_pkt_t cds_pkt_t;

#include "qdf_nbuf.h"

/**
 * cds_pkt_return_packet() - Free the cds Packet
 * @packet: cds Packet
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_pkt_return_packet(cds_pkt_t *packet);

/**
 * cds_pkt_get_packet_length() - Get packet length for a cds Packet
 * @pPacket: the cds Packet to get the packet length from
 * @pPacketSize: location to return the total size of the data
 *               contained in the cds Packet.
 *
 * Return: QDF_STATUS_SUCCESS if the length was returned, otherwise an
 *         appropriate QDF_STATUS_E_* status code.
 */
QDF_STATUS cds_pkt_get_packet_length(cds_pkt_t *pPacket,
				     uint16_t *pPacketSize);

/*
 * TODO: Remove later
 * All the below definitions are not
 * required for Host Driver 2.0
 * once corresponding references are removed
 * from HDD and other layers
 * below code will be removed
 */

/**
 * cds_packet_alloc() - Allocate a network buffer for TX
 * @size: size of the packet
 * @data: packet payload
 * @ppPacket: pointer to return allocated packet
 *
 * Allocates a packet of the indicated @size, populates it with the
 * @data payload, and returns the pointer via @ppPacket. Caller is
 * responsible for calling cds_packet_free() after the packet has been
 * sent to reclaim the packet.
 *
 * Return: QDF_STATUS_SUCCESS if a packet is allocated, otherwise a
 * appropriate QDF_STATUS_E_* status code.
 */
#ifdef MEMORY_DEBUG
#define cds_packet_alloc(size, data, ppPacket)	\
	cds_packet_alloc_debug(size, data, ppPacket, __func__, __LINE__)

QDF_STATUS cds_packet_alloc_debug(uint16_t size, void **data, void **ppPacket,
				  const char *func_name, uint32_t line_num);
#else
QDF_STATUS cds_packet_alloc(uint16_t size, void **data, void **ppPacket);
#endif

/**
 * cds_packet_free() - Free input network buffer
 * @pPacket: network buffer
 */
void cds_packet_free(void *pPacket);

#endif /* !defined( __CDS_PKT_H ) */
