/*
 * Copyright (c) 2012, 2014-2016, 2018 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef __HTC_H__
#define __HTC_H__

#ifndef ATH_TARGET
#include "athstartpack.h"
#endif
#ifdef ATHR_WIN_NWF
#pragma warning( disable:4214 ) //bit field types other than int
#endif
#undef MS
#define MS(_v, _f) (((_v) & _f##_MASK) >> _f##_LSB)
#undef SM
#define SM(_v, _f) (((_v) << _f##_LSB) & _f##_MASK)
#undef WO
#define WO(_f)      ((_f##_OFFSET) >> 2)

#undef GET_FIELD
#define GET_FIELD(_addr, _f) MS(*((A_UINT32 *)(_addr) + WO(_f)), _f)
#undef SET_FIELD
#define SET_FIELD(_addr, _f, _val)  \
    (*((A_UINT32 *)(_addr) + WO(_f)) = \
     (*((A_UINT32 *)(_addr) + WO(_f)) & ~_f##_MASK) | SM(_val, _f))

#define HTC_GET_FIELD(_msg_buf, _msg_type, _f) \
    GET_FIELD(_msg_buf, _msg_type ## _ ## _f)

#define HTC_SET_FIELD(_msg_buf, _msg_type, _f, _val) \
    SET_FIELD(_msg_buf, _msg_type ## _ ## _f, _val)

#define HTC_WRITE32(_addr, _val) \
    (*(A_UINT32 *)(_addr) = (_val))

#ifndef A_OFFSETOF
#define A_OFFSETOF(type,field) (unsigned long)(&(((type *)NULL)->field))
#endif

#define ASSEMBLE_UNALIGNED_UINT16(p,highbyte,lowbyte) \
        (((A_UINT16)(((A_UINT8 *)(p))[(highbyte)])) << 8 | (A_UINT16)(((A_UINT8 *)(p))[(lowbyte)]))

/****** DANGER DANGER ***************
 *
 *   The frame header length and message formats defined herein were
 *   selected to accommodate optimal alignment for target processing.  This reduces code
 *   size and improves performance.
 *
 *   Any changes to the header length may alter the alignment and cause exceptions
 *   on the target. When adding to the message structures insure that fields are
 *   properly aligned.
 *
 */

/* HTC frame header */
typedef PREPACK struct _HTC_FRAME_HDR{
        /* do not remove or re-arrange these fields, these are minimally required
         * to take advantage of 4-byte lookaheads in some hardware implementations */
    A_UINT32   EndpointID : 8,
               Flags : 8,
               PayloadLen : 16; /* length of data (including trailer) that follows the header */

    /***** end of 4-byte lookahead ****/

    A_UINT32   ControlBytes0 : 8, /* used for CRC check if CRC_CHECK flag set */
               ControlBytes1 : 8, /* used for seq check if SEQ_CHECK flag set */
               reserved : 16; /* used by bundle processing in SDIO systems */

    /* message payload starts after the header */

} POSTPACK HTC_FRAME_HDR;

#define HTC_FRAME_HDR_ENDPOINTID_LSB                0
#define HTC_FRAME_HDR_ENDPOINTID_MASK               0x000000ff
#define HTC_FRAME_HDR_ENDPOINTID_OFFSET             0x00000000
#define HTC_FRAME_HDR_FLAGS_LSB                     8
#define HTC_FRAME_HDR_FLAGS_MASK                    0x0000ff00
#define HTC_FRAME_HDR_FLAGS_OFFSET                  0x00000000
#define HTC_FRAME_HDR_PAYLOADLEN_LSB                16
#define HTC_FRAME_HDR_PAYLOADLEN_MASK               0xffff0000
#define HTC_FRAME_HDR_PAYLOADLEN_OFFSET             0x00000000
#define HTC_FRAME_HDR_CONTROLBYTES0_LSB             0
#define HTC_FRAME_HDR_CONTROLBYTES0_MASK            0x000000ff
#define HTC_FRAME_HDR_CONTROLBYTES0_OFFSET          0x00000004
#define HTC_FRAME_HDR_CONTROLBYTES1_LSB             8
#define HTC_FRAME_HDR_CONTROLBYTES1_MASK            0x0000ff00
#define HTC_FRAME_HDR_CONTROLBYTES1_OFFSET          0x00000004
#define HTC_FRAME_HDR_RESERVED_LSB                  16
#define HTC_FRAME_HDR_RESERVED_MASK                 0xffff0000
#define HTC_FRAME_HDR_RESERVED_OFFSET               0x00000004

/* frame header flags */

    /* send direction */
#define HTC_FLAGS_NEED_CREDIT_UPDATE (1 << 0)
#define HTC_FLAGS_SEND_BUNDLE        (1 << 1) /* start or part of bundle */
#define HTC_FLAGS_SEQ_CHECK          (1 << 2) /* seq check on rx side */
#define HTC_FLAGS_CRC_CHECK          (1 << 3) /* CRC check on rx side */
/* HTC_FLAGS_PADDING_CHECK
 * Set by the sender to inform the receiver that the HTC packet begins
 * with continuation (block) alignment padding from the prior HTC packet.
 */
#define HTC_FLAGS_PADDING_CHECK      (1 << 4)
    /* receive direction */
#define HTC_FLAGS_RECV_1MORE_BLOCK   (1 << 0) /* bit 0 bundle trailer present */
#define HTC_FLAGS_RECV_TRAILER       (1 << 1) /* bit 1 trailer data present */
#define HTC_FLAGS_RECV_BUNDLE_CNT_MASK   (0xFC)    /* bits 7..2  */
#define HTC_FLAGS_RECV_BUNDLE_CNT_SHIFT  2
/*
 * To be compatible with an older definition of a smaller (4-bit)
 * bundle count field, the bundle count is stored in a segmented
 * format - the 4 LSbs of the bundle count value are stored in bits 5:2
 * of the BUNDLE_CNT field, which is bits 7:4 of the HTC_FLAGS word;
 * the next 2 bits of the bundle count value are stored in bits 1:0 of
 * the BUNDLE_CNT field, which is bits 3:2 of the HTC_FLAGS word.
 */
#define HTC_FLAGS_RECV_BUNDLE_CNT_SET(x)  \
  ((((x) << 2) | ((x) >> 4)) << HTC_FLAGS_RECV_BUNDLE_CNT_SHIFT)
#define HTC_FLAGS_RECV_BUNDLE_CNT_GET(x)  \
  ((((x) & HTC_FLAGS_RECV_BUNDLE_CNT_MASK) >> (HTC_FLAGS_RECV_BUNDLE_CNT_SHIFT + 2)) | \
  ((((x) >> HTC_FLAGS_RECV_BUNDLE_CNT_SHIFT) & 0x3) << 4))

#define HTC_HDR_LENGTH  (sizeof(HTC_FRAME_HDR))
#define HTC_HDR_ALIGNMENT_PADDING           \
    (((sizeof(HTC_FRAME_HDR) + 3) & (~0x3)) - sizeof(HTC_FRAME_HDR))
#define HTC_MAX_TRAILER_LENGTH   255
#define HTC_MAX_PAYLOAD_LENGTH   (4096 - sizeof(HTC_FRAME_HDR))

/* HTC control message IDs */

#define HTC_MSG_READY_ID                    1
#define HTC_MSG_CONNECT_SERVICE_ID          2
#define HTC_MSG_CONNECT_SERVICE_RESPONSE_ID 3
#define HTC_MSG_SETUP_COMPLETE_ID           4
#define HTC_MSG_SETUP_COMPLETE_EX_ID        5
#define HTC_MSG_SEND_SUSPEND_COMPLETE       6
#define HTC_MSG_NACK_SUSPEND                7
#define HTC_MSG_WAKEUP_FROM_SUSPEND_ID      8

#define HTC_MAX_CONTROL_MESSAGE_LENGTH  256

/* base message ID header */
typedef PREPACK struct {
    A_UINT32 MessageID : 16,
             reserved : 16;
} POSTPACK HTC_UNKNOWN_MSG;

#define HTC_UNKNOWN_MSG_MESSAGEID_LSB                 0
#define HTC_UNKNOWN_MSG_MESSAGEID_MASK                0x0000ffff
#define HTC_UNKNOWN_MSG_MESSAGEID_OFFSET              0x00000000

/* HTC ready message
 * direction : target-to-host  */
typedef PREPACK struct {
    A_UINT32  MessageID : 16, /* ID */
              CreditCount: 16;  /* number of credits the target can offer */
    A_UINT32  CreditSize : 16,   /* size of each credit */
              MaxEndpoints : 8, /* maximum number of endpoints the target has resources for */
              _Pad1 : 8;
} POSTPACK HTC_READY_MSG;

#define HTC_READY_MSG_MESSAGEID_LSB                   0
#define HTC_READY_MSG_MESSAGEID_MASK                  0x0000ffff
#define HTC_READY_MSG_MESSAGEID_OFFSET                0x00000000
#define HTC_READY_MSG_CREDITCOUNT_LSB                 16
#define HTC_READY_MSG_CREDITCOUNT_MASK                0xffff0000
#define HTC_READY_MSG_CREDITCOUNT_OFFSET              0x00000000
#define HTC_READY_MSG_CREDITSIZE_LSB                  0
#define HTC_READY_MSG_CREDITSIZE_MASK                 0x0000ffff
#define HTC_READY_MSG_CREDITSIZE_OFFSET               0x00000004
#define HTC_READY_MSG_MAXENDPOINTS_LSB                16
#define HTC_READY_MSG_MAXENDPOINTS_MASK               0x00ff0000
#define HTC_READY_MSG_MAXENDPOINTS_OFFSET             0x00000004

    /* extended HTC ready message */
typedef PREPACK struct {
    HTC_READY_MSG   Version2_0_Info;   /* legacy version 2.0 information at the front... */
    /* extended information */
    A_UINT32        HTCVersion : 8,
                    MaxMsgsPerHTCBundle : 8, /* used in SDIO systems */
                    AltDataCreditSize : 12,  /* used in HL (SDIO/USB) systems */
                    reserved : 4;
} POSTPACK HTC_READY_EX_MSG;

#define HTC_READY_EX_MSG_HTCVERSION_LSB               0
#define HTC_READY_EX_MSG_HTCVERSION_MASK              0x000000ff
#define HTC_READY_EX_MSG_HTCVERSION_OFFSET            sizeof(HTC_READY_MSG)
#define HTC_READY_EX_MSG_MAXMSGSPERHTCBUNDLE_LSB      8
#define HTC_READY_EX_MSG_MAXMSGSPERHTCBUNDLE_MASK     0x0000ff00
#define HTC_READY_EX_MSG_MAXMSGSPERHTCBUNDLE_OFFSET   sizeof(HTC_READY_MSG)

#define HTC_VERSION_2P0  0x00
#define HTC_VERSION_2P1  0x01  /* HTC 2.1 */

#define HTC_SERVICE_META_DATA_MAX_LENGTH 128

/* connect service
 * direction : host-to-target */
typedef PREPACK struct {
    A_UINT32  MessageID : 16,
              ServiceID : 16;           /* service ID of the service to connect to */
    A_UINT32  ConnectionFlags : 16,     /* connection flags */

#define HTC_CONNECT_FLAGS_REDUCE_CREDIT_DRIBBLE (1 << 2)  /* reduce credit dribbling when
                                                             the host needs credits */
#define HTC_CONNECT_FLAGS_THRESHOLD_LEVEL_MASK             (0x3)
#define HTC_CONNECT_FLAGS_THRESHOLD_LEVEL_ONE_FOURTH        0x0
#define HTC_CONNECT_FLAGS_THRESHOLD_LEVEL_ONE_HALF          0x1
#define HTC_CONNECT_FLAGS_THRESHOLD_LEVEL_THREE_FOURTHS     0x2
#define HTC_CONNECT_FLAGS_THRESHOLD_LEVEL_UNITY             0x3
    /* disable credit flow control on a specific service */
#define HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL          (1 << 3)
    /* enable htc schedule on a specific service */
#define HTC_CONNECT_FLAGS_ENABLE_HTC_SCHEDULE          (1 << 4)

              ServiceMetaLength : 8,   /* length of meta data that follows */
              LookAheadV2 : 1, /* 1 if host supports HTC_LOOKAHEAD_REPORT_V2 */
              _Pad1 : 7;

    /* service-specific meta data starts after the header */

} POSTPACK HTC_CONNECT_SERVICE_MSG;

#define HTC_CONNECT_SERVICE_MSG_MESSAGEID_LSB             0
#define HTC_CONNECT_SERVICE_MSG_MESSAGEID_MASK            0x0000ffff
#define HTC_CONNECT_SERVICE_MSG_MESSAGEID_OFFSET          0x00000000
#define HTC_CONNECT_SERVICE_MSG_SERVICE_ID_LSB            16
#define HTC_CONNECT_SERVICE_MSG_SERVICE_ID_MASK           0xffff0000
#define HTC_CONNECT_SERVICE_MSG_SERVICE_ID_OFFSET         0x00000000
#define HTC_CONNECT_SERVICE_MSG_CONNECTIONFLAGS_LSB       0
#define HTC_CONNECT_SERVICE_MSG_CONNECTIONFLAGS_MASK      0x0000ffff
#define HTC_CONNECT_SERVICE_MSG_CONNECTIONFLAGS_OFFSET    0x00000004
#define HTC_CONNECT_SERVICE_MSG_SERVICEMETALENGTH_LSB     16
#define HTC_CONNECT_SERVICE_MSG_SERVICEMETALENGTH_MASK    0x00ff0000
#define HTC_CONNECT_SERVICE_MSG_SERVICEMETALENGTH_OFFSET  0x00000004

#define HTC_SET_RECV_ALLOC_SHIFT    8
#define HTC_SET_RECV_ALLOC_MASK     0xFF00
#define HTC_CONNECT_FLAGS_SET_RECV_ALLOCATION(value) (((A_UINT8)value) << HTC_SET_RECV_ALLOC_SHIFT)
#define HTC_CONNECT_FLAGS_GET_RECV_ALLOCATION(value) (A_UINT8)(((value) & HTC_SET_RECV_ALLOC_MASK) >> HTC_SET_RECV_ALLOC_SHIFT)

/* connect response
 * direction : target-to-host */
typedef PREPACK struct {
    A_UINT32  MessageID : 16,
              ServiceID : 16;       /* service ID that the connection request was made */
    A_UINT32  Status : 8,           /* service connection status */
              EndpointID : 8,       /* assigned endpoint ID */
              MaxMsgSize : 16;      /* maximum expected message size on this endpoint */
    A_UINT32  ServiceMetaLength : 8,    /* length of meta data that follows */
              LookAheadV2 : 1,/* 1 if target supports HTC_LOOKAHEAD_REPORT_V2 */
              _Pad1 : 7,
              reserved : 16;

    /* service-specific meta data starts after the header */

} POSTPACK HTC_CONNECT_SERVICE_RESPONSE_MSG;

#define HTC_CONNECT_SERVICE_RESPONSE_MSG_MESSAGEID_LSB            0
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_MESSAGEID_MASK           0x0000ffff
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_MESSAGEID_OFFSET         0x00000000
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_SERVICEID_LSB            16
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_SERVICEID_MASK           0xffff0000
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_SERVICEID_OFFSET         0x00000000
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_STATUS_LSB               0
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_STATUS_MASK              0x000000ff
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_STATUS_OFFSET            0x00000004
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_ENDPOINTID_LSB           8
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_ENDPOINTID_MASK          0x0000ff00
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_ENDPOINTID_OFFSET        0x00000004
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_MAXMSGSIZE_LSB           16
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_MAXMSGSIZE_MASK          0xffff0000
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_MAXMSGSIZE_OFFSET        0x00000004
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_SERVICEMETALENGTH_LSB    0
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_SERVICEMETALENGTH_MASK   0x000000ff
#define HTC_CONNECT_SERVICE_RESPONSE_MSG_SERVICEMETALENGTH_OFFSET 0x00000008

typedef PREPACK struct {
    A_UINT32  MessageID : 16,
              reserved : 16;
    /* currently, no other fields */
} POSTPACK HTC_SETUP_COMPLETE_MSG;

#define HTC_SETUP_COMPLETE_MSG_MESSAGEID_LSB          0
#define HTC_SETUP_COMPLETE_MSG_MESSAGEID_MASK         0x0000ffff
#define HTC_SETUP_COMPLETE_MSG_MESSAGEID_OFFSET       0x00000000

    /* extended setup completion message */
typedef PREPACK struct {
    A_UINT32  MessageID : 16,
              reserved : 16;
    A_UINT32  SetupFlags : 32;
    A_UINT32  MaxMsgsPerBundledRecv : 8,
              Rsvd0 : 8,
              Rsvd1 : 8,
              Rsvd2 : 8;
} POSTPACK HTC_SETUP_COMPLETE_EX_MSG;

#define HTC_SETUP_COMPLETE_EX_MSG_MESSAGEID_LSB               0
#define HTC_SETUP_COMPLETE_EX_MSG_MESSAGEID_MASK              0x0000ffff
#define HTC_SETUP_COMPLETE_EX_MSG_MESSAGEID_OFFSET            0x00000000
#define HTC_SETUP_COMPLETE_EX_MSG_SETUPFLAGS_LSB              0
#define HTC_SETUP_COMPLETE_EX_MSG_SETUPFLAGS_MASK             0xffffffff
#define HTC_SETUP_COMPLETE_EX_MSG_SETUPFLAGS_OFFSET           0x00000004
#define HTC_SETUP_COMPLETE_EX_MSG_MAXMSGSPERBUNDLEDRECV_LSB       0
#define HTC_SETUP_COMPLETE_EX_MSG_MAXMSGSPERBUNDLEDRECV_MASK      0x000000ff
#define HTC_SETUP_COMPLETE_EX_MSG_MAXMSGSPERBUNDLEDRECV_OFFSET    0x00000008
#define HTC_SETUP_COMPLETE_EX_MSG_RSVD0_LSB                   8
#define HTC_SETUP_COMPLETE_EX_MSG_RSVD0_MASK                  0x0000ff00
#define HTC_SETUP_COMPLETE_EX_MSG_RSVD0_OFFSET                0x00000008
#define HTC_SETUP_COMPLETE_EX_MSG_RSVD1_LSB                   16
#define HTC_SETUP_COMPLETE_EX_MSG_RSVD1_MASK                  0x00ff0000
#define HTC_SETUP_COMPLETE_EX_MSG_RSVD1_OFFSET                0x00000008
#define HTC_SETUP_COMPLETE_EX_MSG_RSVD2_LSB                   24
#define HTC_SETUP_COMPLETE_EX_MSG_RSVD2_MASK                  0xff000000
#define HTC_SETUP_COMPLETE_EX_MSG_RSVD2_OFFSET                0x00000008

#define HTC_SETUP_COMPLETE_FLAGS_ENABLE_BUNDLE_RECV     (1 << 0)    /* enable recv bundling from target */
#define HTC_SETUP_COMPLETE_FLAGS_DISABLE_TX_CREDIT_FLOW (1 << 1)    /* disable credit based flow control,
                                                                       only supported on some interconnects */

/* connect response status codes */
#define HTC_SERVICE_SUCCESS      0  /* success */
#define HTC_SERVICE_NOT_FOUND    1  /* service could not be found */
#define HTC_SERVICE_FAILED       2  /* specific service failed the connect */
#define HTC_SERVICE_NO_RESOURCES 3  /* no resources (i.e. no more endpoints) */
#define HTC_SERVICE_NO_MORE_EP   4  /* specific service is not allowing any more
                                       endpoints */

/* report record IDs */

#define HTC_RECORD_NULL             0
#define HTC_RECORD_CREDITS          1
#define HTC_RECORD_LOOKAHEAD        2
#define HTC_RECORD_LOOKAHEAD_BUNDLE 3

typedef PREPACK struct {
    A_UINT32 RecordID : 8, /* Record ID */
             Length : 8,   /* Length of record */
             reserved : 16;
} POSTPACK HTC_RECORD_HDR;

#define HTC_RECORD_HDR_RECORDID_LSB           0
#define HTC_RECORD_HDR_RECORDID_MASK          0x000000ff
#define HTC_RECORD_HDR_RECORDID_OFFSET        0x00000000
#define HTC_RECORD_HDR_LENGTH_LSB             8
#define HTC_RECORD_HDR_LENGTH_MASK            0x0000ff00
#define HTC_RECORD_HDR_LENGTH_OFFSET          0x00000000

typedef PREPACK struct {
    A_UINT32 EndpointID : 8,     /* Endpoint that owns these credits */
             Credits : 8,        /* credits to report since last report */
             reserved : 16;
} POSTPACK HTC_CREDIT_REPORT;

#define HTC_CREDIT_REPORT_ENDPOINTID_LSB      0
#define HTC_CREDIT_REPORT_ENDPOINTID_MASK     0x000000ff
#define HTC_CREDIT_REPORT_ENDPOINTID_OFFSET   0x00000000
#define HTC_CREDIT_REPORT_CREDITS_LSB         8
#define HTC_CREDIT_REPORT_CREDITS_MASK        0x0000ff00
#define HTC_CREDIT_REPORT_CREDITS_OFFSET      0x00000000

typedef PREPACK struct {
    A_UINT32 PreValid : 8,      /* pre valid guard */
             reserved0 : 24;
    A_UINT32 LookAhead0 : 8,    /* 4 byte lookahead */
             LookAhead1 : 8,
             LookAhead2 : 8,
             LookAhead3 : 8;
    A_UINT32 PostValid : 8,     /* post valid guard */
             reserved1 : 24;

   /* NOTE: the LookAhead array is guarded by a PreValid and Post Valid guard bytes.
    * The PreValid bytes must equal the inverse of the PostValid byte */

} POSTPACK HTC_LOOKAHEAD_REPORT;

/*
 * If the host sets the HTC_CONNECT_SERVICE_MSG.LookAheadV2 flag and the
 * target sets the HTC_CONNECT_SERVICE_RESPONSE_MSG.LookAheadV2 flag,
 * HTC_LOOKAHEAD_REPORT_V2 is used; otherwise HTC_LOOKAHEAD_REPORT is used.
 */
typedef PREPACK struct {
    A_UINT32 PreValid : 8,   /* pre valid guard */
        reserved0 : 24;
    A_UINT32 LookAhead0 : 8,  /* 8 byte lookahead */
        LookAhead1 : 8,
        LookAhead2 : 8,
        LookAhead3 : 8;
    A_UINT32 LookAhead4 : 8,  /* 8 byte lookahead */
        LookAhead5 : 8,
        LookAhead6 : 8,
        LookAhead7 : 8;
    A_UINT32 PostValid : 8,   /* post valid guard */
        reserved1 : 24;
    /* NOTE: the LookAhead array is guarded by PreValid and Post Valid
     * guard bytes.
     * The PreValid byte must equal the inverse of the PostValid byte.
     */
} POSTPACK HTC_LOOKAHEAD_REPORT_V2;

#define HTC_LOOKAHEAD_REPORT_PREVALID_LSB         0
#define HTC_LOOKAHEAD_REPORT_PREVALID_MASK        0x000000ff
#define HTC_LOOKAHEAD_REPORT_PREVALID_OFFSET      0x00000000
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD0_LSB       0
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD0_MASK      0x000000ff
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD0_OFFSET    0x00000004
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD1_LSB       8
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD1_MASK      0x0000ff00
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD1_OFFSET    0x00000004
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD2_LSB       16
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD2_MASK      0x00ff0000
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD2_OFFSET    0x00000004
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD3_LSB       24
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD3_MASK      0xff000000
#define HTC_LOOKAHEAD_REPORT_LOOKAHEAD3_OFFSET    0x00000004
#define HTC_LOOKAHEAD_REPORT_POSTVALID_LSB        0
#define HTC_LOOKAHEAD_REPORT_POSTVALID_MASK       0x000000ff
#define HTC_LOOKAHEAD_REPORT_POSTVALID_OFFSET     0x00000008

typedef PREPACK struct {
    A_UINT32 LookAhead0 : 8,    /* 4 byte lookahead */
             LookAhead1 : 8,
             LookAhead2 : 8,
             LookAhead3 : 8;
} POSTPACK HTC_BUNDLED_LOOKAHEAD_REPORT;

#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD0_LSB           0
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD0_MASK          0x000000ff
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD0_OFFSET        0x00000000
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD1_LSB           8
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD1_MASK          0x0000ff00
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD1_OFFSET        0x00000000
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD2_LSB           16
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD2_MASK          0x00ff0000
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD2_OFFSET        0x00000000
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD3_LSB           24
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD3_MASK          0xff000000
#define HTC_BUNDLED_LOOKAHEAD_REPORT_LOOKAHEAD3_OFFSET        0x00000000

#ifndef ATH_TARGET
#include "athendpack.h"
#endif


#endif /* __HTC_H__ */

