// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __CAM_RPMSG_H__
#define __CAM_RPMSG_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/bitfield.h>
#include <linux/rpmsg.h>
#include <linux/dma-buf.h>
#include <linux/sched/task.h>

#include "cam_req_mgr.h"

#define CAM_ADD_REG_VAL_PAIR(buf_array, index, offset, val)     \
        do {                                                    \
		CAM_DBG(CAM_ISP, "idx %d offset %03x val %08x", \
				(index), (offset), (val));      \
                buf_array[(index)++] = (offset);                \
                buf_array[(index)++] = (val);                   \
        } while (0)

typedef int (*cam_rpmsg_recv_cb)(struct rpmsg_device *rpdev, void *data,
	int len, void *priv, unsigned int src);

#define CAM_RPMSG_HANDLE_SLAVE          0
#define CAM_RPMSG_HANDLE_JPEG           1
#define CAM_RPMSG_HANDLE_MAX            2

#define CAM_SLAVE_HW_CSID               0
#define CAM_SLAVE_HW_VFE                1

#define JPEG_DSP2CPU_RESERVED           8

/*
 * struct cam_slave_pkt_hdr - describes header for camera slave
 * version           : 7 bits
 * direction         : 1 bits
 * num_packet        : 8 bit
 * packet_sz         : 16 bits
 */
struct cam_slave_pkt_hdr {
	uint32_t raw;
};

#define CAM_RPMSG_V1                             1

#define CAM_CDM_TYPE_REG_CONT                    3
#define CAM_CDM_TYPE_REG_RAND                    4
#define CAM_CDM_TYPE_SW_DMI32                    76
#define CAM_CDM_MASK_TYPE                        GENMASK(31, 24)
#define CAM_CDM_MASK_SIZE                        GENMASK(15, 0)
#define CAM_CDM_MASK_REG_CONT_OFFSET             GENMASK(23, 0)
#define CAM_CDM_MASK_REG_RAND_OFFSET             GENMASK(23, 0)
#define CAM_CDM_MASK_SW_DMI32_SEL                GENMASK(31, 24)
#define CAM_CDM_MASK_SW_DMI32_OFFSET             GENMASK(23, 0)
#define CAM_CDM_GET_TYPE(ptr) \
	FIELD_GET(CAM_CDM_MASK_TYPE, *(ptr))
#define CAM_CDM_GET_SIZE(ptr) \
	FIELD_GET(CAM_CDM_MASK_SIZE, *(ptr))
#define CAM_CDM_GET_REG_CONT_OFFSET(ptr) \
	FIELD_GET(CAM_CDM_MASK_REG_CONT_OFFSET, *(ptr))
#define CAM_CDM_GET_REG_RAND_OFFSET(ptr) \
	FIELD_GET(CAM_CDM_MASK_REG_RAND_OFFSET, *(ptr))
#define CAM_CDM_GET_SW_DMI32_SEL(ptr) \
	FIELD_GET(CAM_CDM_MASK_SW_DMI32_SEL, *(ptr))
#define CAM_CDM_GET_SW_DMI32_OFFSET(ptr) \
	FIELD_GET(CAM_CDM_MASK_SW_DMI32_OFFSET, *(ptr))
#define CAM_CDM_SET_TYPE(ptr, val) \
	(*(ptr) |= FIELD_PREP(CAM_CDM_MASK_TYPE, (val)))
#define CAM_CDM_SET_SIZE(ptr, val) \
	(*(ptr) |= FIELD_PREP(CAM_CDM_MASK_SIZE, (val)))
#define CAM_CDM_SET_REG_CONT_OFFSET(ptr, val) \
	(*(ptr) |= FIELD_PREP(CAM_CDM_MASK_REG_CONT_OFFSET, (val)))
#define CAM_CDM_SET_REG_RAND_OFFSET(ptr, val) \
	(*(ptr) |= FIELD_PREP(CAM_CDM_MASK_REG_RAND_OFFSET, (val)))

#define CAM_CDM_GET_PAYLOAD_SIZE(type, num_entries)                                 \
	({                                                                          \
		int cdm_payload_size = 0;                                           \
                                                                                    \
		switch(type) {                                                      \
			case CAM_CDM_TYPE_REG_CONT:                                 \
				cdm_payload_size = (num_entries + 2) * 4;           \
			break;                                                      \
			case CAM_CDM_TYPE_REG_RAND:                                 \
				cdm_payload_size = (num_entries * 2 + 1) * 4;       \
			break;                                                      \
			case CAM_CDM_TYPE_SW_DMI32:                                 \
				cdm_payload_size = ((num_entries + 1) / 4 + 3) * 4; \
			break;                                                      \
			default:                                                    \
				CAM_INFO(CAM_RPMSG, "Invalid CDM Type %d", type);   \
				cdm_payload_size = INT_MAX;                         \
		}                                                                   \
		CAM_DBG(CAM_RPMSG, "cdm type %d, size %d", type,                    \
				cdm_payload_size);                                  \
		cdm_payload_size;                                                   \
	})

#define CAM_RPMSG_SLAVE_HDR_MASK_VERSION         GENMASK(6, 0)
#define CAM_RPMSG_SLAVE_HDR_MASK_DIRECTION       BIT(7)
#define CAM_RPMSG_SLAVE_HDR_MASK_NUM_PACKET      GENMASK(15, 8)
#define CAM_RPMSG_SLAVE_HDR_MASK_PACKET_SZ       GENMASK(31, 16)

#define CAM_RPMSG_SLAVE_GET_HDR_VERSION(hdr) \
	FIELD_GET(CAM_RPMSG_SLAVE_HDR_MASK_VERSION, (hdr)->raw)
#define CAM_RPMSG_SLAVE_GET_HDR_DIRECTION(hdr) \
	FIELD_GET(CAM_RPMSG_SLAVE_HDR_MASK_DIRECTION, (hdr)->raw)
#define CAM_RPMSG_SLAVE_GET_HDR_NUM_PACKET(hdr) \
	FIELD_GET(CAM_RPMSG_SLAVE_HDR_MASK_NUM_PACKET, (hdr)->raw)
#define CAM_RPMSG_SLAVE_GET_HDR_PACKET_SZ(hdr) \
	FIELD_GET(CAM_RPMSG_SLAVE_HDR_MASK_PACKET_SZ, (hdr)->raw)

#define CAM_RPMSG_SLAVE_SET_HDR_VERSION(hdr, val) \
	((hdr)->raw |= FIELD_PREP(CAM_RPMSG_SLAVE_HDR_MASK_VERSION, (val)))
#define CAM_RPMSG_SLAVE_SET_HDR_DIRECTION(hdr, val) \
	((hdr)->raw |= FIELD_PREP(CAM_RPMSG_SLAVE_HDR_MASK_DIRECTION, (val)))
#define CAM_RPMSG_SLAVE_SET_HDR_NUM_PACKET(hdr, val) \
	((hdr)->raw |= FIELD_PREP(CAM_RPMSG_SLAVE_HDR_MASK_NUM_PACKET, (val)))
#define CAM_RPMSG_SLAVE_SET_HDR_PACKET_SZ(hdr, val) \
	do {                                                                          \
		(hdr)->raw &= ~CAM_RPMSG_SLAVE_HDR_MASK_PACKET_SZ;                    \
		((hdr)->raw |= FIELD_PREP(CAM_RPMSG_SLAVE_HDR_MASK_PACKET_SZ, (val)));\
	} while(0)

#define CAM_RPMSG_DIR_MASTER_TO_SLAVE                        0x0
#define CAM_RPMSG_DIR_SLAVE_TO_MASTER                        0x1

#define CAM_RPMSG_SLAVE_PACKET_BASE_SYSTEM                   0x0
#define CAM_RPMSG_SLAVE_PACKET_BASE_ISP                      0x20
#define CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR                   0x40
#define CAM_RPMSG_SLAVE_PACKET_BASE_PHY                      0x50
#define CAM_RPMSG_SLAVE_PACKET_BASE_DEBUG                    0x70

#define CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_UNUSED     \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SYSTEM + 0x0)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_PING       \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SYSTEM + 0x1)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_SYNC       \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SYSTEM + 0x2)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_MAX        \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SYSTEM + 0x19)

#define CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_UNUSED        \
	(CAM_RPMSG_SLAVE_PACKET_BASE_ISP + 0x0)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_ACQUIRE       \
	(CAM_RPMSG_SLAVE_PACKET_BASE_ISP + 0x1)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_RELEASE       \
	(CAM_RPMSG_SLAVE_PACKET_BASE_ISP + 0x2)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_INIT_CONFIG   \
	(CAM_RPMSG_SLAVE_PACKET_BASE_ISP + 0x3)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_START_DEV     \
	(CAM_RPMSG_SLAVE_PACKET_BASE_ISP + 0x4)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_STOP_DEV      \
	(CAM_RPMSG_SLAVE_PACKET_BASE_ISP + 0x5)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_ERROR         \
	(CAM_RPMSG_SLAVE_PACKET_BASE_ISP + 0x6)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_MAX           \
	(CAM_RPMSG_SLAVE_PACKET_BASE_ISP + 0x19)


#define CAM_RPMSG_SLAVE_PACKET_TYPE_SENSOR_UNUSED     \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0)
#define HCM_PKT_OPCODE_SENSOR_PROBE                   \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0x2)
#define HCM_PKT_OPCODE_SENSOR_PROBE_RESPONSE          \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0x3)
#define HCM_PKT_OPCODE_SENSOR_SYS_CMD_FLUSH           \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0x5)
#define HCM_PKT_OPCODE_SENSOR_ACQUIRE                 \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0x6)
#define HCM_PKT_OPCODE_SENSOR_RELEASE                 \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0x7)
#define HCM_PKT_OPCODE_SENSOR_INIT                    \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0x8)
#define HCM_PKT_OPCODE_SENSOR_CONFIG                  \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0x9)
#define HCM_PKT_OPCODE_SENSOR_START_DEV               \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0xA)
#define HCM_PKT_OPCODE_SENSOR_STOP_DEV                \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0xB)
#define HCM_PKT_OPCODE_SENSOR_ERROR                   \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0xC)
#define HCM_PKT_OPCODE_SENSOR_STREAMING_CONFIG        \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0xD)
#define HCM_PKT_OPCODE_SENSOR_EXPOSURE_CONFIG         \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0xE)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_SENSOR_MAX        \
	(CAM_RPMSG_SLAVE_PACKET_BASE_SENSOR + 0xF)

#define HCM_PKT_OPCODE_PHY_ACQUIRE                    \
	(CAM_RPMSG_SLAVE_PACKET_BASE_PHY + 0)
#define HCM_PKT_OPCODE_PHY_RELEASE                    \
	(CAM_RPMSG_SLAVE_PACKET_BASE_PHY + 0x1)
#define HCM_PKT_OPCODE_PHY_INIT_CONFIG                \
	(CAM_RPMSG_SLAVE_PACKET_BASE_PHY + 0x2)
#define HCM_PKT_OPCODE_PHY_START_DEV                  \
	(CAM_RPMSG_SLAVE_PACKET_BASE_PHY + 0x3)
#define HCM_PKT_OPCODE_PHY_STOP_DEV                   \
	(CAM_RPMSG_SLAVE_PACKET_BASE_PHY + 0x4)
#define HCM_PKT_OPCODE_PHY_ERROR                      \
	(CAM_RPMSG_SLAVE_PACKET_BASE_PHY + 0x5)
#define CAM_RPMSG_SLAVE_PACKET_TYPE_PHY_MAX           \
	(CAM_RPMSG_SLAVE_PACKET_BASE_PHY + 0xF)

#define CAM_RPMSG_SLAVE_PACKET_TYPE_DEBUG_LOG     \
	(CAM_RPMSG_SLAVE_PACKET_BASE_DEBUG + 0)

#define PACKET_VERSION_1                   1

#define ERR_TYPE_SLAVE_FATAL               1
#define ERR_TYPE_SLAVE_RECOVERY            2

/*
 * slave payload - describes payload for camera slave
 * type     : 8 bits
 * size     : 16 bits      Does not include payload size
 * reserved : 8 bits
 */
struct cam_rpmsg_slave_payload_desc {
	uint32_t raw;
};

#define CAM_RPMSG_SLAVE_PAYLOAD_MASK_TYPE GENMASK(7, 0)
#define CAM_RPMSG_SLAVE_PAYLOAD_MASK_SIZE GENMASK(23, 8)
#define CAM_RPMSG_SLAVE_PAYLOAD_MASK_RES  GENMASK(31, 24)

#define CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(payload) \
	FIELD_GET(CAM_RPMSG_SLAVE_PAYLOAD_MASK_TYPE, (payload)->raw)
#define CAM_RPMSG_SLAVE_GET_PAYLOAD_SIZE(payload) \
	FIELD_GET(CAM_RPMSG_SLAVE_PAYLOAD_MASK_SIZE, (payload)->raw)
#define CAM_RPMSG_SLAVE_GET_PAYLOAD_RES(payload) \
		FIELD_GET(CAM_RPMSG_SLAVE_PAYLOAD_MASK_RES, (payload)->raw)

#define CAM_RPMSG_SLAVE_SET_PAYLOAD_TYPE(payload, val) \
	((payload)->raw |= FIELD_PREP(CAM_RPMSG_SLAVE_PAYLOAD_MASK_TYPE, (val)))
#define CAM_RPMSG_SLAVE_SET_PAYLOAD_SIZE(payload, val) \
	do {                                                                              \
		(payload)->raw &= ~CAM_RPMSG_SLAVE_PAYLOAD_MASK_SIZE;                     \
		((payload)->raw |= FIELD_PREP(CAM_RPMSG_SLAVE_PAYLOAD_MASK_SIZE, (val))); \
	} while(0)

#define CAM_RPMSG_SLAVE_CLIENT_SYSTEM    0
#define CAM_RPMSG_SLAVE_CLIENT_ISP       1
#define CAM_RPMSG_SLAVE_CLIENT_SENSOR    2
#define CAM_RPMSG_SLAVE_CLIENT_PHY       3
#define CAM_RPMSG_SLAVE_CLIENT_MAX       4

#define CAM_RPMSG_TRACE_BEGIN_TX    "Tx Begin"
#define CAM_RPMSG_TRACE_END_TX      "Tx End"
#define CAM_RPMSG_TRACE_RX          "Rx"
#define CAM_RPMSG_TRACE_TX          "Tx"

enum cam_jpeg_dsp_status {
	CAM_JPEG_DSP_POWEROFF,
	CAM_JPEG_DSP_POWERON,
};

enum cam_jpeg_dsp_error_type {
	CAM_JPEG_DSP_INVALID  = 0,
	CAM_JPEG_DSP_PF_ERROR  = 1,
	CAM_JPEG_DSP_PC_ERROR  = 2,
	CAM_JPEG_DSP_MAX_ERROR = 3,
};

enum cam_jpeg_dsp_cmd {
	CAM_CPU2DSP_SUSPEND = 1,
	CAM_CPU2DSP_RESUME = 2,
	CAM_CPU2DSP_SHUTDOWN = 3,
	CAM_CPU2DSP_REGISTER_BUFFER = 4,
	CAM_CPU2DSP_DEREGISTER_BUFFER = 5,
	CAM_CPU2DSP_INIT = 6,
	CAM_CPU2DSP_SET_DEBUG_LEVEL = 7,
	CAM_CPU2DSP_NOTIFY_ERROR = 8,
	CAM_CPU2DSP_MAX_CMD = 9,
	CAM_DSP2CPU_POWERON = 11,
	CAM_DSP2CPU_POWEROFF = 12,
	CAM_DSP2CPU_START = 13,
	CAM_DSP2CPU_DETELE_SESSION = 14,
	CAM_DSP2CPU_POWER_REQUEST = 15,
	CAM_DSP2CPU_POWER_CANCEL = 16,
	CAM_DSP2CPU_REGISTER_BUFFER = 17,
	CAM_DSP2CPU_DEREGISTER_BUFFER = 18,
	CAM_DSP2CPU_MEM_ALLOC = 19,
	CAM_DSP2CPU_MEM_FREE = 20,
	CAM_DSP2CPU_LOG = 21,
	CAM_JPEG_DSP_MAX_CMD = 22,
};

/** struct cam_jpeg_mem_remote - buffer information for cdsp
 *
 * @type        : Type of buffer
 * @size        : Type of buffer
 * @fd          : File descriptor of buffer
 * @iova        : IOVA address of buffer
 * @flags       : Flags of buffer
 * @buf_handle  : Buffer handle of buffer
 * @ipa_addr    : DSP accesible virtual address of buffer
 */
struct cam_jpeg_mem_remote {
    uint32_t    type;
    uint32_t    size;
    uint32_t    fd;
    uint32_t    iova;
    uint32_t    flags;
    uint32_t    buf_handle;
    uint64_t    ipa_addr;
} ;

/** struct cam_jpeg_power_req_qdi - Power request structure
 *
 * @clk        : Requested clock for jpeg
 * @bw_ddr     : Bandwidth required
 * @op_bw_DDR  : Operational bandwidth required
 */
struct cam_jpeg_power_req_qdi {
    uint32_t clk;
    uint32_t bw_DDR;
    uint32_t op_bw_DDR;
} ;

/** struct cam_jpeg_dsp2cpu_cmd_msg - DSP to CPU structure
 *
 * @type        : Type of command
 * @ver         : Version of command
 * @len         : Length of command
 * @session_type: Type of session
 * @kernel_mask : Mask of kernel
 * @is_secure   : Is secure session
 * @pid         : Process id of DSP
 * @buf_info    : Buffer information
 * @power_req   : information of requested power
 */
struct cam_jpeg_dsp2cpu_cmd_msg {
    uint32_t type;
    uint32_t ver;
    uint32_t len;
    uint32_t session_type;
    uint32_t kernel_mask;
    uint32_t is_secure;
    uint32_t dsp_access_mask;
    int32_t  pid;
    struct cam_jpeg_mem_remote buf_info;
    struct cam_jpeg_power_req_qdi power_req;

    uint32_t data[JPEG_DSP2CPU_RESERVED];
};

union error_data {
	uint32_t far;
};

/** struct cam_jpeg_dsp_error_info - CPU to DSP error info
 *
 * @core_id    : Jpeg core on which error is seen
 * @error_type : Type of the error
 * @far        : Page fault address, valid only in case of pf
 */
struct cam_jpeg_dsp_error_info {
	uint32_t   core_id;
	uint32_t   error_type;
	union error_data data;
};

/** struct cam_jpegd_cmd_msg - CPU to DSP command structure
 *
 * @cmd_msg_type        : Type of command
 * @ret_val             : Return value of command
 * @iova                : IOVA address of buffer
 * @buf_index           : Buffer index
 * @buf_size            : Size of buffer
 * @fd                  : FD of buffer
 * @buf_offset          : Offset of buffer
 * @jpeg_dsp_debug_level: Debug level of dsp
 * @buf_info            : Information of buffer
 * @error_info          : Error Info
 */
struct cam_jpeg_cmd_msg {
	uint32_t cmd_msg_type;
	int32_t  ret_val;
	uint32_t iova;
	uint32_t buf_index;
	uint32_t buf_size;
	uint32_t fd;
	uint32_t buf_offset;
	uint32_t fd_size;
	uint32_t jpeg_dsp_debug_level;
	struct   cam_jpeg_mem_remote buf_info;
	struct   cam_jpeg_dsp_error_info error_info;
};


/** struct cam_rpmsg_slave_cbs - slave client callback
 *
 * @registered : Flag to indicate if callbacks are registered
 * @cookie     : Cookie to be passed back in callbacks
 * @recv       : Function pointer to receive callback
 */
struct cam_rpmsg_slave_cbs {
	int registered;
	void *cookie;
	int (*recv)(void *cookie, void *data, int len);
};

/** struct cam_rpmsg_slave_pvt - slave channel private data
 *
 * @tx_dump : bool to enable/disable hexdump of slave tx packets
 * @rx_dump : bool to enable/disable hexdump of slave rx packets
 * @dentry  : directory entry of debugfs entries
 * @cbs     : slave client callback data
 */
struct cam_rpmsg_slave_pvt {
	bool tx_dump;
	bool rx_dump;
	struct dentry *dentry;
	struct cam_rpmsg_slave_cbs cbs[CAM_RPMSG_SLAVE_CLIENT_MAX];
};


/** struct cam_rpmsg_jpeg_error_data - jpeg error data
 *
 * @complete: signals completetion of error handling
 */
struct cam_rpmsg_jpeg_error_data {
	struct completion complete;
};

/** struct cam_rpmsg_jpeg_pvt - jpeg channel private data
 *
 * @status:          DSP state (POWERON/POWEROFF)
 * @jpeg_work_queue: workqeue for nsp jpeg
 * @jpeg_iommu_hdl:  jpeg Iommu handle
 * @jpeg_task:       jpeg tasklets
 * @dmabuf_f_op:     dma buffer info
 * @error_data:      error related info
 * @jpeg_mutex:      Jpeg mutex
 */
struct cam_rpmsg_jpeg_pvt {
	enum cam_jpeg_dsp_status           status;
	struct workqueue_struct           *jpeg_work_queue;
	uint32_t                           jpeg_iommu_hdl;
	struct task_struct                *jpeg_task;
	const struct file_operations      *dmabuf_f_op;
	struct cam_rpmsg_jpeg_error_data   error_data;
	struct mutex                       jpeg_mutex;
	int32_t                            pid;
};

/** struct cam_rpmsg_instance_data - rpmsg per channel data
 *
 * @sp_lock              : spin_lock variable
 * @recv_cb              : channel receive callback
 * @pvt                  : channel private data
 * @rpdev                : rpmsg device pointer for channel
 * @status_change_notify : notification chain
 */
struct cam_rpmsg_instance_data {
	spinlock_t           sp_lock;
	struct mutex         rpmsg_mutex;
	cam_rpmsg_recv_cb    recv_cb;
	void *pvt;
	struct rpmsg_device *rpdev;
	struct blocking_notifier_head status_change_notify;
	bool state;
};

/* struct cam_rpmsg_isp_acq_payload
 *
 * @hdr        : packet header
 * @phdr       : payload header
 * @version    : packet version
 * @sensor_id  : sensor id
 */
struct cam_rpmsg_isp_acq_payload {
	struct cam_slave_pkt_hdr hdr;
	struct cam_rpmsg_slave_payload_desc phdr;
	uint32_t version;
	uint32_t sensor_id;
};

/* struct cam_rpmsg_isp_rel_payload
 *
 * @hdr        : packet header
 * @phdr       : payload header
 * @version    : packet version
 * @sensor_id  : sensor id
 */
struct cam_rpmsg_isp_rel_payload {
	struct cam_slave_pkt_hdr hdr;
	struct cam_rpmsg_slave_payload_desc phdr;
	uint32_t version;
	uint32_t sensor_id;
};

/* struct cam_rpmsg_isp_start_payload
 *
 * @hdr        : packet header
 * @phdr       : payload header
 * @version    : packet version
 * @sensor_id  : sensor id
 */
struct cam_rpmsg_isp_start_payload {
	struct cam_slave_pkt_hdr hdr;
	struct cam_rpmsg_slave_payload_desc phdr;
	uint32_t version;
	uint32_t sensor_id;
};

/* struct cam_rpmsg_isp_stop_payload
 *
 * @hdr        : packet header
 * @phdr       : payload header
 * @version    : packet version
 * @sensor_id  : sensor id
 */
struct cam_rpmsg_isp_stop_payload {
	struct cam_slave_pkt_hdr hdr;
	struct cam_rpmsg_slave_payload_desc phdr;
	uint32_t version;
	uint32_t sensor_id;
};

/* struct cam_rpmsg_system_ping_payload
 *
 * @hdr        : packet header
 * @phdr       : payload header
 */
struct cam_rpmsg_system_ping_payload {
	struct cam_slave_pkt_hdr hdr;
	struct cam_rpmsg_slave_payload_desc phdr;
};
/* struct cam_rpmsg_system_sync_payload
 *
 * @hdr        : packet header
 * @phdr       : payload header
 * @num_cams   : Number of camera to sync
 * @camera_id  : array of camera_ids of length num_cams
 */
struct cam_rpmsg_system_sync_payload {
	struct cam_slave_pkt_hdr hdr;
	struct cam_rpmsg_slave_payload_desc phdr;
	uint32_t num_cams;
	uint32_t camera_id[1];
};

/* struct cam_rpmsg_isp_err_payload
 *
 * @phdr       : payload header
 * @version    : packet version
 * @sensor_id  : sensor id
 */
struct cam_rpmsg_isp_err_payload {
	struct cam_slave_pkt_hdr hdr;
	struct cam_rpmsg_slave_payload_desc phdr;
	uint32_t version;
	uint32_t err_type;
	uint32_t sensor_id;
	uint32_t size;
	uint8_t  errDesc[1];
};

struct cam_rpmsg_isp_init_pipeline_cfg {
	uint32_t size;
	uint32_t sensor_mode;
	struct cam_rpmsg_vcdt {
		uint32_t vc;
		uint32_t dt;
	}vcdt[4];
	uint32_t num_ports;
	struct cam_rpmsg_out_port {
		uint32_t type;
		uint32_t width;
		uint32_t height;
		uint32_t format;
	}out_port[1];
};

struct cam_rpmsg_isp_init_cfg_payload {
	struct cam_slave_pkt_hdr hdr;
	struct cam_rpmsg_slave_payload_desc phdr;
	uint16_t major_ver;
	uint16_t minor_ver;
	uint32_t sensor_id;
	uint32_t num_sensor_mode;
	struct cam_rpmsg_isp_init_pipeline_cfg cfg[1];
};

struct cam_rpmsg_isp_init_hw_cfg {
	uint32_t hw_id;
	uint32_t size;
};

#define SLAVE_PKT_HDR_SIZE sizeof(struct cam_slave_pkt_hdr)
#define SLAVE_PKT_PLD_SIZE sizeof(struct cam_rpmsg_slave_payload_desc)

/**
 * @brief     : send isp acquire packet
 * @sensor_id : sensor id
 *
 * @return zero on success or error code
 */
int cam_rpmsg_isp_send_acq(uint32_t sensor_id);

/**
 * @brief     : return device name string based
 *              on handle
 * @val       : device handle for rpdev
 *
 * @return device name
 */
const char *cam_rpmsg_dev_hdl_to_string(unsigned int val);

/**
 * @brief     : return helios packet type string
 * @val       : packet type
 *
 * @return packet-type string
 */
const char *cam_rpmsg_slave_pl_type_to_string(unsigned int val);
/**
 * @brief     : send isp release packet
 * @sensor_id : sensor id
 *
 * @return zero on success or error code
 */
int cam_rpmsg_isp_send_rel(uint32_t sensor_id);

/**
 * @brief     : send isp start packet
 * @sensor_id : sensor id
 *
 * @return zero on success or error code
 */
int cam_rpmsg_isp_send_start(uint32_t sensor_id);

/**
 * @brief     : send isp stop packet
 * @sensor_id : sensor id
 *
 * @return zero on success or error code
 */
int cam_rpmsg_isp_send_stop(uint32_t sensor_id);

/**
 * @prief : Request handle for JPEG/SLAVE rpmsg
 * @name  : name of device, can be either "helios" or "jpeg"
 *
 * @return handle on success, otherwise error codes
 */
unsigned int cam_rpmsg_get_handle(char *name);

/**
 * @brief  : Returns true if channel is connected, false otherwise
 * @handle : handle for channel
 *
 * @return 0 and 1 as channel state, otherwise error codes
 */
int cam_rpmsg_is_channel_connected(unsigned int handle);

/**
 * @brief  : Send data to rpmsg channel
 * @handle : channel handle
 * @data   : pointer to data
 * @len    : length of data
 *
 * @return zero on success, otherwise error codes
 */
int cam_rpmsg_send(unsigned int handle, void *data, int len);

/**
 * @brief     : Subscribe callback for slave data receive
 * @module_id : onw of CAM_RPMSG_SLAVE_CLIENT_SYSTEM/ISP/SENSOR
 * @cbs       : structure for callback function and private data
 *
 * @return zero on success, otherwise error codes
 */
int cam_rpmsg_subscribe_slave_callback(unsigned int module_id,
	struct cam_rpmsg_slave_cbs cbs);

/**
 * @brief     : Unsubscribes callback for slave data receive
 * @module_id : onw of CAM_RPMSG_SLAVE_CLIENT_SYSTEM/ISP/SENSOR
 *
 * @return zero on success, otherwise error codes
 */
int cam_rpmsg_unsubscribe_slave_callback(int module_id);

/**
 * @brief  : start listening for status change event, channel UP/DOWN
 * @handle : Handle for channel
 * @nb     : notification block
 *
 * @return zero on success, otherwise error codes
 */
int cam_rpmsg_register_status_change_event(unsigned int handle,
	struct notifier_block *nb);

/**
 * @brief  : stop listening for status change event, channel UP/DOWN
 * @handle : Handle for channel
 * @nb     : notification block
 *
 * @return zero on success, otherwise error codes
 */
int cam_rpmsg_unregister_status_change_event(unsigned int handle,
	struct notifier_block *nb);

/**
 * @brief  : update receive callback for a channel
 * @handle : Handle for channel
 * @cb     : receive callback function
 *
 * @return zero on success, otherwise error codes
 */
int cam_rpmsg_set_recv_cb(unsigned int handle, cam_rpmsg_recv_cb cb);

int cam_rpmsg_system_send_sync(struct cam_req_mgr_sync_mode_v2 *sync_info);

/**
 * @brief : API to register rpmsg to platform framework.
 * @return zero on success, or ERR_PTR() on error.
 */
int cam_rpmsg_init(void);

/**
 * @brief : API to remove rpmsg from platform framework.
 */
void cam_rpmsg_exit(void);

/**
 * @brief      : API to send error info to cdsp.
 * @error_type : Type of the error.
 * @core_id    : core on which error is seen.
 * @far        : FAR address valid in case of page fault.
 *
 * @return zero on success, or ERR_PTR() on error.
 */
int cam_rpmsg_send_cpu2dsp_error(int error_type, int core_id, void *data);

#endif /* __CAM_RPMSG_H__ */
