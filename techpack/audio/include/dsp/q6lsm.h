/*
 * Copyright (c) 2013-2017, 2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __Q6LSM_H__
#define __Q6LSM_H__

#include <linux/list.h>
#include <linux/msm_ion.h>
#include <dsp/apr_audio-v2.h>
#include <sound/lsm_params.h>
#include <ipc/apr.h>

#define MAX_NUM_CONFIDENCE 20

#define ADM_LSM_PORT_ID 0xADCB

#define LSM_MAX_NUM_CHANNELS 8

typedef void (*lsm_app_cb)(uint32_t opcode, uint32_t token,
		       uint32_t *payload, uint16_t client_size, void *priv);

struct lsm_sound_model {
	dma_addr_t      phys;
	void		*data;
	size_t		size; /* size of buffer */
	uint32_t	actual_size; /* actual number of bytes read by DSP */
	struct ion_handle *handle;
	struct ion_client *client;
	uint32_t	mem_map_handle;
};

struct snd_lsm_event_status_v2 {
	uint16_t status;
	uint16_t payload_size;
	uint8_t  confidence_value[0];
};

struct lsm_lab_buffer {
	dma_addr_t phys;
	void *data;
	size_t size;
	struct ion_handle *handle;
	struct ion_client *client;
	uint32_t mem_map_handle;
};

struct lsm_hw_params {
	u16 sample_rate;
	u16 sample_size;
	u32 buf_sz;
	u32 period_count;
	u16 num_chs;
};

struct lsm_client {
	int		session;
	lsm_app_cb	cb;
	atomic_t	cmd_state;
	void		*priv;
	struct apr_svc  *apr;
	struct apr_svc  *mmap_apr;
	struct mutex    cmd_lock;
	struct lsm_sound_model sound_model;
	wait_queue_head_t cmd_wait;
	uint32_t	cmd_err_code;
	uint16_t	mode;
	uint16_t	connect_to_port;
	uint8_t		num_confidence_levels;
	uint8_t		*confidence_levels;
	bool		opened;
	bool		started;
	dma_addr_t	lsm_cal_phy_addr;
	uint32_t	lsm_cal_size;
	uint32_t	app_id;
	bool		lab_enable;
	bool		lab_started;
	struct lsm_lab_buffer *lab_buffer;
	struct lsm_hw_params hw_params;
	bool		use_topology;
	int		session_state;
	bool		poll_enable;
	int		perf_mode;
	uint32_t	event_mode;
};

struct lsm_stream_cmd_open_tx {
	struct apr_hdr  hdr;
	uint16_t	app_id;
	uint16_t	reserved;
	uint32_t	sampling_rate;
} __packed;

struct lsm_stream_cmd_open_tx_v2 {
	struct apr_hdr hdr;
	uint32_t	topology_id;
} __packed;

struct lsm_custom_topologies {
	struct apr_hdr hdr;
	uint32_t data_payload_addr_lsw;
	uint32_t data_payload_addr_msw;
	uint32_t mem_map_handle;
	uint32_t buffer_size;
} __packed;

struct lsm_param_size_reserved {
	uint16_t param_size;
	uint16_t reserved;
} __packed;

union lsm_param_size {
	uint32_t param_size;
	struct lsm_param_size_reserved sr;
} __packed;

struct lsm_param_payload_common {
	uint32_t	module_id;
	uint32_t	param_id;
	union lsm_param_size p_size;
} __packed;

struct lsm_param_op_mode {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	uint16_t	mode;
	uint16_t	reserved;
} __packed;

struct lsm_param_connect_to_port {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	/* AFE port id that receives voice wake up data */
	uint16_t	port_id;
	uint16_t	reserved;
} __packed;

struct lsm_param_poll_enable {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	/* indicates to voice wakeup that HW MAD/SW polling is enabled or not */
	uint32_t	polling_enable;
} __packed;

struct lsm_param_fwk_mode_cfg {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	uint32_t	mode;
} __packed;

struct lsm_param_media_fmt {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	uint32_t	sample_rate;
	uint16_t	num_channels;
	uint16_t	bit_width;
	uint8_t		channel_mapping[LSM_MAX_NUM_CHANNELS];
} __packed;

/*
 * This param cannot be sent in this format.
 * The actual number of confidence level values
 * need to appended to this param payload.
 */
struct lsm_param_min_confidence_levels {
	struct lsm_param_payload_common common;
	uint8_t		num_confidence_levels;
} __packed;

struct lsm_set_params_hdr {
	uint32_t	data_payload_size;
	uint32_t	data_payload_addr_lsw;
	uint32_t	data_payload_addr_msw;
	uint32_t	mem_map_handle;
} __packed;

struct lsm_cmd_set_params {
	struct apr_hdr  msg_hdr;
	struct lsm_set_params_hdr param_hdr;
} __packed;

struct lsm_cmd_set_params_conf {
	struct apr_hdr  msg_hdr;
	struct lsm_set_params_hdr params_hdr;
	struct lsm_param_min_confidence_levels	conf_payload;
} __packed;

struct lsm_cmd_set_params_opmode {
	struct apr_hdr  msg_hdr;
	struct lsm_set_params_hdr params_hdr;
	struct lsm_param_op_mode op_mode;
} __packed;

struct lsm_cmd_set_connectport {
	struct apr_hdr msg_hdr;
	struct lsm_set_params_hdr params_hdr;
	struct lsm_param_connect_to_port connect_to_port;
} __packed;

struct lsm_cmd_poll_enable {
	struct apr_hdr  msg_hdr;
	struct lsm_set_params_hdr params_hdr;
	struct lsm_param_poll_enable poll_enable;
} __packed;

struct lsm_param_epd_thres {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	uint32_t	epd_begin;
	uint32_t	epd_end;
} __packed;

struct lsm_cmd_set_epd_threshold {
	struct apr_hdr msg_hdr;
	struct lsm_set_params_hdr param_hdr;
	struct lsm_param_epd_thres epd_thres;
} __packed;

struct lsm_param_gain {
	struct lsm_param_payload_common common;
	uint32_t	minor_version;
	uint16_t	gain;
	uint16_t	reserved;
} __packed;

struct lsm_cmd_set_gain {
	struct apr_hdr msg_hdr;
	struct lsm_set_params_hdr param_hdr;
	struct lsm_param_gain lsm_gain;
} __packed;

struct lsm_cmd_reg_snd_model {
	struct apr_hdr	hdr;
	uint32_t	model_size;
	uint32_t	model_addr_lsw;
	uint32_t	model_addr_msw;
	uint32_t	mem_map_handle;
} __packed;

struct lsm_lab_enable {
	struct lsm_param_payload_common common;
	uint16_t enable;
	uint16_t reserved;
} __packed;

struct lsm_params_lab_enable {
	struct apr_hdr msg_hdr;
	struct lsm_set_params_hdr params_hdr;
	struct lsm_lab_enable lab_enable;
} __packed;

struct lsm_lab_config {
	struct lsm_param_payload_common common;
	uint32_t minor_version;
	uint32_t wake_up_latency_ms;
} __packed;


struct lsm_params_lab_config {
	struct apr_hdr  msg_hdr;
	struct lsm_set_params_hdr params_hdr;
	struct lsm_lab_config lab_config;
} __packed;

struct lsm_cmd_read {
	struct apr_hdr hdr;
	uint32_t buf_addr_lsw;
	uint32_t buf_addr_msw;
	uint32_t mem_map_handle;
	uint32_t buf_size;
} __packed;

struct lsm_cmd_read_done {
	struct apr_hdr hdr;
	uint32_t status;
	uint32_t buf_addr_lsw;
	uint32_t buf_addr_msw;
	uint32_t mem_map_handle;
	uint32_t total_size;
	uint32_t offset;
	uint32_t timestamp_lsw;
	uint32_t timestamp_msw;
	uint32_t flags;
} __packed;

struct lsm_cmd_set_fwk_mode_cfg {
	struct apr_hdr  msg_hdr;
	struct lsm_set_params_hdr params_hdr;
	struct lsm_param_fwk_mode_cfg fwk_mode_cfg;
} __packed;

struct lsm_cmd_set_media_fmt {
	struct apr_hdr  msg_hdr;
	struct lsm_set_params_hdr params_hdr;
	struct lsm_param_media_fmt media_fmt;
} __packed;


struct lsm_client *q6lsm_client_alloc(lsm_app_cb cb, void *priv);
void q6lsm_client_free(struct lsm_client *client);
int q6lsm_open(struct lsm_client *client, uint16_t app_id);
int q6lsm_start(struct lsm_client *client, bool wait);
int q6lsm_stop(struct lsm_client *client, bool wait);
int q6lsm_snd_model_buf_alloc(struct lsm_client *client, size_t len,
			      bool allocate_module_data);
int q6lsm_snd_model_buf_free(struct lsm_client *client);
int q6lsm_close(struct lsm_client *client);
int q6lsm_register_sound_model(struct lsm_client *client,
			       enum lsm_detection_mode mode,
			       bool detectfailure);
int q6lsm_set_data(struct lsm_client *client,
		   enum lsm_detection_mode mode,
		   bool detectfailure);
int q6lsm_deregister_sound_model(struct lsm_client *client);
void set_lsm_port(int lsm_port);
int get_lsm_port(void);
int q6lsm_lab_control(struct lsm_client *client, u32 enable);
int q6lsm_stop_lab(struct lsm_client *client);
int q6lsm_read(struct lsm_client *client, struct lsm_cmd_read *read);
int q6lsm_lab_buffer_alloc(struct lsm_client *client, bool alloc);
int q6lsm_set_one_param(struct lsm_client *client,
			struct lsm_params_info *p_info, void *data,
			uint32_t param_type);
void q6lsm_sm_set_param_data(struct lsm_client *client,
		struct lsm_params_info *p_info,
		size_t *offset);
int q6lsm_set_port_connected(struct lsm_client *client);
int q6lsm_set_fwk_mode_cfg(struct lsm_client *client, uint32_t event_mode);
int q6lsm_set_media_fmt_params(struct lsm_client *client);
#endif /* __Q6LSM_H__ */
