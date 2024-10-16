/*
 * Copyright (c) 2014-2017 The Linux Foundation. All rights reserved.
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
 * DOC: i_qdf_net_types
 * This file provides OS dependent net types API's.
 */

#ifndef _I_QDF_NET_TYPES_H
#define _I_QDF_NET_TYPES_H

#include <qdf_types.h>          /* uint8_t, etc. */
#include <asm/checksum.h>
#include <net/ip6_checksum.h>
#include <net/tcp.h>

typedef struct in6_addr __in6_addr_t;
typedef __wsum __wsum_t;

static inline  int32_t __qdf_csum_ipv6(const struct in6_addr *saddr,
				       const struct in6_addr *daddr,
				       __u32 len, unsigned short proto,
				       __wsum sum)
{
	return csum_ipv6_magic((struct in6_addr *)saddr,
			       (struct in6_addr *)daddr, len, proto, sum);
}

static inline char *__qdf_netdev_get_devname(qdf_netdev_t dev)
{
	return dev->name;
}

static inline
__sum16 __qdf_csum_tcpudp_magic(uint32_t ip_saddr, uint32_t ip_daddr,
				uint16_t adj_ip_len, uint8_t ip_proto,
				uint32_t sum)
{
	return csum_tcpudp_magic(ip_saddr, ip_daddr,
				 adj_ip_len, ip_proto, sum);
}

static inline
uint16_t __qdf_ip_fast_csum(void *ip_hdr, uint8_t ip_hl)
{
	return ip_fast_csum((struct iphdr *)ip_hdr, ip_hl);
}

#define __QDF_TCPHDR_FIN TCPHDR_FIN
#define __QDF_TCPHDR_SYN TCPHDR_SYN
#define __QDF_TCPHDR_RST TCPHDR_RST
#define __QDF_TCPHDR_PSH TCPHDR_PSH
#define __QDF_TCPHDR_ACK TCPHDR_ACK
#define __QDF_TCPHDR_URG TCPHDR_URG
#define __QDF_TCPHDR_ECE TCPHDR_ECE
#define __QDF_TCPHDR_CWR TCPHDR_CWR
#endif /* _I_QDF_NET_TYPES_H */
