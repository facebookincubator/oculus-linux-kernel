// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#include "cam_sensor_core.h"
#include "cam_sensor_util.h"
#include "cam_soc_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"

#define MIN_PD 1
#define DEFAULT_PD 2
#define MAX_PD 3
#define WITH_NO_CRM_MASK  0x1


static int cam_sensor_update_req_mgr(
	struct cam_sensor_ctrl_t *s_ctrl,
	struct cam_packet *csl_packet)
{
	int rc = 0;
	struct cam_req_mgr_add_request add_req;

	/* Don't call add request if its no crm case*/
	if (s_ctrl->bridge_intf.enable_crm != 1)
		return rc;

	memset(&add_req, 0, sizeof(add_req));
	add_req.link_hdl = s_ctrl->bridge_intf.link_hdl;
	add_req.req_id = csl_packet->header.request_id;
	CAM_DBG(CAM_SENSOR, " Rxed Req Id: %llu",
		csl_packet->header.request_id);
	add_req.dev_hdl = s_ctrl->bridge_intf.device_hdl;
	if (s_ctrl->bridge_intf.crm_cb &&
		s_ctrl->bridge_intf.crm_cb->add_req) {
		rc = s_ctrl->bridge_intf.crm_cb->add_req(&add_req);
		if (rc) {
			CAM_ERR(CAM_SENSOR,
				"Adding request: %llu failed with request manager rc: %d",
				csl_packet->header.request_id, rc);
			return rc;
		}
	}

	CAM_DBG(CAM_SENSOR, "Successfully add req: %llu to req mgr",
			add_req.req_id);
	return rc;
}

static void cam_sensor_release_stream_rsc(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	struct i2c_settings_array *i2c_set = NULL;
	int rc;

	i2c_set = &(s_ctrl->i2c_data.streamoff_settings);
	if (i2c_set->is_settings_valid == 1) {
		i2c_set->is_settings_valid = -1;
		rc = delete_request(i2c_set);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed while deleting Streamoff settings");
	}

	i2c_set = &(s_ctrl->i2c_data.streamon_settings);
	if (i2c_set->is_settings_valid == 1) {
		i2c_set->is_settings_valid = -1;
		rc = delete_request(i2c_set);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed while deleting Streamon settings");
	}
}

static void cam_sensor_release_per_frame_resource(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	struct i2c_settings_array *i2c_set = NULL;
	int i, rc;

	if (s_ctrl->i2c_data.per_frame != NULL) {
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(s_ctrl->i2c_data.per_frame[i]);
			if (i2c_set->is_settings_valid == 1) {
				i2c_set->is_settings_valid = -1;
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"delete request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
	}

	if (s_ctrl->i2c_data.frame_skip != NULL) {
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(s_ctrl->i2c_data.frame_skip[i]);
			if (i2c_set->is_settings_valid == 1) {
				i2c_set->is_settings_valid = -1;
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"delete request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
	}
}


static int32_t cam_sensor_handle_config_no_io(struct cam_sensor_ctrl_t *s_ctrl,
				void *arg)
{
	struct cam_packet *csl_packet = NULL;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_config_dev_cmd config;
	uintptr_t generic_ptr;
	size_t len_of_buff = 0;
	size_t remain_len = 0;
	int rc = 0;
	ioctl_ctrl = (struct cam_control *)arg;

	/* If no crm usecase no need to validate the packet */
	if (!s_ctrl->bridge_intf.enable_crm)
			return rc;

	if (ioctl_ctrl->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_SENSOR, "Invalid Handle Type");
		return -EINVAL;
	}

	if (copy_from_user(&config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(config)))
		return -EFAULT;

	rc = cam_mem_get_cpu_buf(
		config.packet_handle,
		&generic_ptr,
		&len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed in getting the packet: %d", rc);
		return rc;
	}

	remain_len = len_of_buff;
	if ((sizeof(struct cam_packet) > len_of_buff) ||
		((size_t)config.offset >= len_of_buff -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_SENSOR,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), len_of_buff);
		rc = -EINVAL;
		goto end;
	}

	remain_len -= (size_t)config.offset;
	csl_packet = (struct cam_packet *)(generic_ptr +
		(uint32_t)config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_SENSOR, "Invalid packet params");
		rc = -EINVAL;
		goto end;
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG &&
		csl_packet->header.request_id <= s_ctrl->last_flush_req
		&& s_ctrl->last_flush_req != 0) {
		CAM_ERR(CAM_SENSOR,
			"reject request %lld, last request to flush %u",
			csl_packet->header.request_id, s_ctrl->last_flush_req);
		rc = -EINVAL;
		goto end;
	}

	if (csl_packet->header.request_id > s_ctrl->last_flush_req)
		s_ctrl->last_flush_req = 0;

	/*
	 * Since this is a hw_no_io_ops sensor
	 * No need to parse the i2c packet we just need to
	 * add the packet to crm
	 */
	rc = cam_sensor_update_req_mgr(s_ctrl, csl_packet);

end:
	return rc;
}

static int32_t cam_sensor_i2c_pkt_parse(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int32_t rc = 0;
	uintptr_t generic_ptr;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_packet *csl_packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct cam_buf_io_cfg *io_cfg = NULL;
	struct i2c_settings_array *i2c_reg_settings = NULL;
	size_t len_of_buff = 0;
	size_t remain_len = 0;
	uint32_t *offset = NULL;
	struct cam_config_dev_cmd config;
	struct i2c_data_settings *i2c_data = NULL;

	ioctl_ctrl = (struct cam_control *)arg;

	if (ioctl_ctrl->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_SENSOR, "Invalid Handle Type");
		return -EINVAL;
	}

	if (copy_from_user(&config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(config)))
		return -EFAULT;

	rc = cam_mem_get_cpu_buf(
		config.packet_handle,
		&generic_ptr,
		&len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed in getting the packet: %d", rc);
		return rc;
	}

	remain_len = len_of_buff;
	if ((sizeof(struct cam_packet) > len_of_buff) ||
		((size_t)config.offset >= len_of_buff -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_SENSOR,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), len_of_buff);
		rc = -EINVAL;
		goto end;
	}

	remain_len -= (size_t)config.offset;
	csl_packet = (struct cam_packet *)(generic_ptr +
		(uint32_t)config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_SENSOR, "Invalid packet params");
		rc = -EINVAL;
		goto end;
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG &&
		csl_packet->header.request_id <= s_ctrl->last_flush_req
		&& s_ctrl->last_flush_req != 0) {
		CAM_ERR(CAM_SENSOR,
			"reject request %lld, last request to flush %u",
			csl_packet->header.request_id, s_ctrl->last_flush_req);
		rc = -EINVAL;
		goto end;
	}

	if (csl_packet->header.request_id > s_ctrl->last_flush_req)
		s_ctrl->last_flush_req = 0;

	i2c_data = &(s_ctrl->i2c_data);
	CAM_DBG(CAM_SENSOR, "Header OpCode: %d", csl_packet->header.op_code);
	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG: {
		i2c_reg_settings = &i2c_data->init_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG: {
		i2c_reg_settings = &i2c_data->config_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
		if (s_ctrl->streamon_count > 0)
			goto end;

		s_ctrl->streamon_count = s_ctrl->streamon_count + 1;
		i2c_reg_settings = &i2c_data->streamon_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
		if (s_ctrl->streamoff_count > 0)
			goto end;

		s_ctrl->streamoff_count = s_ctrl->streamoff_count + 1;
		i2c_reg_settings = &i2c_data->streamoff_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_READ: {
		i2c_reg_settings = &(i2c_data->read_settings);
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;

		CAM_DBG(CAM_SENSOR, "number of IO configs: %d:",
			csl_packet->num_io_configs);
		if (csl_packet->num_io_configs == 0) {
			CAM_ERR(CAM_SENSOR, "No I/O configs to process");
			goto end;
		}

		io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
			&csl_packet->payload +
			csl_packet->io_configs_offset);

		if (io_cfg == NULL) {
			CAM_ERR(CAM_SENSOR, "I/O config is invalid(NULL)");
			goto end;
		}
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed Update packets without linking");
			goto end;
		}

		i2c_reg_settings =
			&i2c_data->per_frame[csl_packet->header.request_id %
				MAX_PER_FRAME_ARRAY];
		CAM_DBG(CAM_SENSOR, "Received Packet: %lld req: %lld",
			csl_packet->header.request_id % MAX_PER_FRAME_ARRAY,
			csl_packet->header.request_id);
		if (i2c_reg_settings->is_settings_valid == 1) {
			CAM_ERR(CAM_SENSOR,
				"Already some pkt in offset req : %lld",
				csl_packet->header.request_id);
			/*
			 * Update req mgr even in case of failure.
			 * This will help not to wait indefinitely
			 * and freeze. If this log is triggered then
			 * fix it.
			 */
			rc = cam_sensor_update_req_mgr(s_ctrl, csl_packet);
			if (rc)
				CAM_ERR(CAM_SENSOR,
					"Failed in adding request to req_mgr");
			goto end;
		}
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed Update packets without linking");
			goto end;
		}

		i2c_reg_settings =
			&i2c_data->frame_skip[csl_packet->header.request_id %
				MAX_PER_FRAME_ARRAY];
		CAM_DBG(CAM_SENSOR, "Received not ready packet: %lld req: %lld",
			csl_packet->header.request_id % MAX_PER_FRAME_ARRAY,
			csl_packet->header.request_id);
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_NOP: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed NOP packets without linking");
			goto end;
		}

		i2c_reg_settings =
			&i2c_data->per_frame[csl_packet->header.request_id %
				MAX_PER_FRAME_ARRAY];
		i2c_reg_settings->request_id = csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;

		rc = cam_sensor_update_req_mgr(s_ctrl, csl_packet);
		if (rc)
			CAM_ERR(CAM_SENSOR,
				"Failed in adding request to req_mgr");
		goto end;
	}
	default:
		CAM_ERR(CAM_SENSOR, "Invalid Packet Header opcode: %d",
			csl_packet->header.op_code & 0xFFFFFF);
		rc = -EINVAL;
		goto end;
	}

	offset = (uint32_t *)&csl_packet->payload;
	offset += csl_packet->cmd_buf_offset / 4;
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);

	rc = cam_sensor_i2c_command_parser(&s_ctrl->io_master_info,
			i2c_reg_settings, cmd_desc, 1, io_cfg);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Fail parsing I2C Pkt: %d", rc);
		goto end;
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE) {
		i2c_reg_settings->request_id =
			csl_packet->header.request_id;
		rc = cam_sensor_update_req_mgr(s_ctrl, csl_packet);
		if (rc) {
			CAM_ERR(CAM_SENSOR,
				"Failed in adding request to req_mgr");
			goto end;
		}
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE) {
		i2c_reg_settings->request_id =
			csl_packet->header.request_id;
	}

end:
	cam_mem_put_cpu_buf(config.packet_handle);
	return rc;
}

static int32_t cam_sensor_i2c_modes_util(
	struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list)
{
	int32_t rc = 0;
	uint32_t i, size;

	if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_RANDOM) {
		rc = camera_io_dev_write(io_master_info,
			&(i2c_list->i2c_settings));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to random write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_SEQ) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			CAM_SENSOR_I2C_WRITE_SEQ);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to seq write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_BURST) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			CAM_SENSOR_I2C_WRITE_BURST);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to burst write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_POLL) {
		size = i2c_list->i2c_settings.size;
		for (i = 0; i < size; i++) {
			rc = camera_io_dev_poll(
			io_master_info,
			i2c_list->i2c_settings.reg_setting[i].reg_addr,
			i2c_list->i2c_settings.reg_setting[i].reg_data,
			i2c_list->i2c_settings.reg_setting[i].data_mask,
			i2c_list->i2c_settings.addr_type,
			i2c_list->i2c_settings.data_type,
			i2c_list->i2c_settings.reg_setting[i].delay);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"i2c poll apply setting Fail: %d", rc);
				return rc;
			}
		}
	}

	return rc;
}

int32_t cam_sensor_update_i2c_info(struct cam_cmd_i2c_info *i2c_info,
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_cci_client   *cci_client = NULL;

	if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
		cci_client = s_ctrl->io_master_info.cci_client;
		if (!cci_client) {
			CAM_ERR(CAM_SENSOR, "failed: cci_client %pK",
				cci_client);
			return -EINVAL;
		}
		cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
		cci_client->sid = i2c_info->slave_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->i2c_freq_mode = i2c_info->i2c_freq_mode;
		CAM_DBG(CAM_SENSOR, " Master: %d sid: %d freq_mode: %d",
			cci_client->cci_i2c_master, i2c_info->slave_addr,
			i2c_info->i2c_freq_mode);
	}

	s_ctrl->sensordata->slave_info.sensor_slave_addr =
		i2c_info->slave_addr;
	return rc;
}

int32_t cam_sensor_update_slave_info(void *probe_info,
	uint32_t cmd, struct cam_sensor_ctrl_t *s_ctrl, uint8_t probe_ver)
{
	int32_t rc = 0;
	struct cam_cmd_probe *sensor_probe_info;
	struct cam_cmd_probe_v2 *sensor_probe_info_v2;

	memset(s_ctrl->sensor_name, 0, CAM_SENSOR_NAME_MAX_SIZE);

	if (probe_ver == CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE) {
		sensor_probe_info = (struct cam_cmd_probe *)probe_info;
		s_ctrl->sensordata->slave_info.sensor_id_reg_addr =
			sensor_probe_info->reg_addr;
		s_ctrl->sensordata->slave_info.sensor_id =
			sensor_probe_info->expected_data;
		s_ctrl->sensordata->slave_info.sensor_id_mask =
			sensor_probe_info->data_mask;
		s_ctrl->pipeline_delay =
			sensor_probe_info->reserved;

		s_ctrl->sensor_probe_addr_type = sensor_probe_info->addr_type;
		s_ctrl->sensor_probe_data_type = sensor_probe_info->data_type;
	} else if (probe_ver == CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE_V2) {
		sensor_probe_info_v2 = (struct cam_cmd_probe_v2 *)probe_info;
		s_ctrl->sensordata->slave_info.sensor_id_reg_addr =
			sensor_probe_info_v2->reg_addr;
		s_ctrl->sensordata->slave_info.sensor_id =
			sensor_probe_info_v2->expected_data;
		s_ctrl->sensordata->slave_info.sensor_id_mask =
			sensor_probe_info_v2->data_mask;
		s_ctrl->pipeline_delay =
			sensor_probe_info_v2->pipeline_delay;

		s_ctrl->sensor_probe_addr_type =
			sensor_probe_info_v2->addr_type;
		s_ctrl->sensor_probe_data_type =
			sensor_probe_info_v2->data_type;

		memcpy(s_ctrl->sensor_name, sensor_probe_info_v2->sensor_name,
			CAM_SENSOR_NAME_MAX_SIZE-1);
	}

	CAM_DBG(CAM_SENSOR,
		"%s Sensor Addr: 0x%x sensor_id: 0x%x sensor_mask: 0x%x sensor_pipeline_delay:0x%x",
		s_ctrl->sensor_name,
		s_ctrl->sensordata->slave_info.sensor_id_reg_addr,
		s_ctrl->sensordata->slave_info.sensor_id,
		s_ctrl->sensordata->slave_info.sensor_id_mask,
		s_ctrl->pipeline_delay);
	return rc;
}

int32_t cam_handle_cmd_buffers_for_probe(void *cmd_buf,
	struct cam_sensor_ctrl_t *s_ctrl,
	int32_t cmd_buf_num, uint32_t cmd,
	uint32_t cmd_buf_length, size_t remain_len,
	uint32_t probe_ver)
{
	int32_t rc = 0;
	size_t required_size = 0;

	switch (cmd_buf_num) {
	case 0: {
		struct cam_cmd_i2c_info *i2c_info = NULL;
		void *probe_info;

		if (probe_ver == CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE)
			required_size = sizeof(struct cam_cmd_i2c_info) +
				sizeof(struct cam_cmd_probe);
		else if(probe_ver == CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE_V2)
			required_size = sizeof(struct cam_cmd_i2c_info) +
				sizeof(struct cam_cmd_probe_v2);

		if (remain_len < required_size) {
			CAM_ERR(CAM_SENSOR,
				"not enough buffer for cam_cmd_i2c_info");
			return -EINVAL;
		}
		i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
		rc = cam_sensor_update_i2c_info(i2c_info, s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed in Updating the i2c Info");
			return rc;
		}
		probe_info = cmd_buf + sizeof(struct cam_cmd_i2c_info);
		rc = cam_sensor_update_slave_info(probe_info, cmd, s_ctrl,
							probe_ver);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Updating the slave Info");
			return rc;
		}
		cmd_buf = probe_info;
	}
		break;
	case 1: {
		rc = cam_sensor_update_power_settings(cmd_buf,
			cmd_buf_length, &s_ctrl->sensordata->power_info,
			remain_len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed in updating power settings");
			return rc;
		}
	}
		break;
	default:
		CAM_ERR(CAM_SENSOR, "Invalid command buffer");
		break;
	}
	return rc;
}

int32_t cam_handle_mem_ptr(uint64_t handle, uint32_t cmd,
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0, i;
	uint32_t *cmd_buf;
	void *ptr;
	size_t len;
	struct cam_packet *pkt = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uintptr_t cmd_buf1 = 0;
	uintptr_t packet = 0;
	size_t    remain_len = 0;
	uint32_t probe_ver = 0;

	rc = cam_mem_get_cpu_buf(handle,
		&packet, &len);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed to get the command Buffer");
		return -EINVAL;
	}

	pkt = (struct cam_packet *)packet;
	if (pkt == NULL) {
		CAM_ERR(CAM_SENSOR, "packet pos is invalid");
		rc = -EINVAL;
		goto end;
	}

	if ((len < sizeof(struct cam_packet)) ||
		(pkt->cmd_buf_offset >= (len - sizeof(struct cam_packet)))) {
		CAM_ERR(CAM_SENSOR, "Not enough buf provided");
		rc = -EINVAL;
		goto end;
	}

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&pkt->payload + pkt->cmd_buf_offset/4);
	if (cmd_desc == NULL) {
		CAM_ERR(CAM_SENSOR, "command descriptor pos is invalid");
		rc = -EINVAL;
		goto end;
	}
	if (pkt->num_cmd_buf != 2) {
		CAM_ERR(CAM_SENSOR, "Expected More Command Buffers : %d",
			 pkt->num_cmd_buf);
		rc = -EINVAL;
		goto end;
	}

	probe_ver = pkt->header.op_code & 0xFFFFFF;
	CAM_DBG(CAM_SENSOR, "Received Header opcode: %u", probe_ver);

	for (i = 0; i < pkt->num_cmd_buf; i++) {
		if (!(cmd_desc[i].length))
			continue;
		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			&cmd_buf1, &len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to parse the command Buffer Header");
			goto end;
		}
		if (cmd_desc[i].offset >= len) {
			CAM_ERR(CAM_SENSOR,
				"offset past length of buffer");
			rc = -EINVAL;
			goto end;
		}
		remain_len = len - cmd_desc[i].offset;
		if (cmd_desc[i].length > remain_len) {
			CAM_ERR(CAM_SENSOR,
				"Not enough buffer provided for cmd");
			rc = -EINVAL;
			goto end;
		}
		cmd_buf = (uint32_t *)cmd_buf1;
		cmd_buf += cmd_desc[i].offset/4;
		ptr = (void *) cmd_buf;

		rc = cam_handle_cmd_buffers_for_probe(ptr, s_ctrl,
			i, cmd, cmd_desc[i].length, remain_len, probe_ver);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to parse the command Buffer Header");
			goto end;
		}
		cam_mem_put_cpu_buf(cmd_desc[i].mem_handle);
	}

end:
	cam_mem_put_cpu_buf(handle);
	return rc;
}

void cam_sensor_query_cap(struct cam_sensor_ctrl_t *s_ctrl,
	struct  cam_sensor_query_cap *query_cap)
{
	query_cap->pos_roll = s_ctrl->sensordata->pos_roll;
	query_cap->pos_pitch = s_ctrl->sensordata->pos_pitch;
	query_cap->pos_yaw = s_ctrl->sensordata->pos_yaw;
	query_cap->secure_camera = 0;
	query_cap->actuator_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_ACTUATOR];
	query_cap->csiphy_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY];
	query_cap->eeprom_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_EEPROM];
	query_cap->flash_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_LED_FLASH];
	query_cap->ois_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_OIS];
	query_cap->slot_info =
		s_ctrl->soc_info.index;
}

void cam_sensor_query_cap_v2(struct cam_sensor_ctrl_t *s_ctrl,
	struct  cam_sensor_query_cap_v2 *query_cap)
{
	query_cap->pos_roll = s_ctrl->sensordata->pos_roll;
	query_cap->pos_pitch = s_ctrl->sensordata->pos_pitch;
	query_cap->pos_yaw = s_ctrl->sensordata->pos_yaw;
	query_cap->secure_camera = 0;
	query_cap->reserved = 0;
	query_cap->actuator_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_ACTUATOR];
	query_cap->csiphy_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY];
	query_cap->eeprom_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_EEPROM];
	query_cap->flash_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_LED_FLASH];
	query_cap->ois_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_OIS];
	query_cap->slot_info =
		s_ctrl->soc_info.index;
	query_cap->queue_depth = MAX_PER_FRAME_ARRAY;
}

static uint16_t cam_sensor_id_by_mask(struct cam_sensor_ctrl_t *s_ctrl,
	uint32_t chipid)
{
	uint16_t sensor_id = (uint16_t)(chipid & 0xFFFF);
	int16_t sensor_id_mask = s_ctrl->sensordata->slave_info.sensor_id_mask;

	if (!sensor_id_mask)
		sensor_id_mask = ~sensor_id_mask;

	sensor_id &= sensor_id_mask;
	sensor_id_mask &= -sensor_id_mask;
	sensor_id_mask -= 1;
	while (sensor_id_mask) {
		sensor_id_mask >>= 1;
		sensor_id >>= 1;
	}
	return sensor_id;
}

void cam_sensor_shutdown(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;
	int rc = 0;

	if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) &&
		(s_ctrl->is_probe_succeed == 0))
		return;

	cam_sensor_release_stream_rsc(s_ctrl);
	cam_sensor_release_per_frame_resource(s_ctrl);

	if (s_ctrl->sensor_state != CAM_SENSOR_INIT)
		cam_sensor_power_down(s_ctrl);

	if (s_ctrl->bridge_intf.device_hdl != -1) {
		rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"dhdl already destroyed: rc = %d", rc);
	}

	s_ctrl->bridge_intf.device_hdl = -1;
	s_ctrl->bridge_intf.link_hdl = -1;
	s_ctrl->bridge_intf.session_hdl = -1;
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_setting_size = 0;
	power_info->power_down_setting_size = 0;
	s_ctrl->streamon_count = 0;
	s_ctrl->streamoff_count = 0;
	s_ctrl->is_probe_succeed = 0;
	s_ctrl->last_flush_req = 0;
	s_ctrl->sensor_state = CAM_SENSOR_INIT;
}

int cam_sensor_match_id(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t chipid = 0;
	struct cam_camera_slave_info *slave_info;

	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!slave_info) {
		CAM_ERR(CAM_SENSOR, " failed: %pK",
			 slave_info);
		return -EINVAL;
	}

	/* Skip hw probe if either hw_no_io_ops or hw_no_ops flag is enabled */
	if (s_ctrl->hw_no_io_ops || s_ctrl->hw_no_ops) {
		CAM_DBG(CAM_SENSOR, "%s[0x%x] probe with hw_no_io_ops[%d] hw_no_ops[%d]",
				s_ctrl->sensor_name,
				slave_info->sensor_id,
				s_ctrl->hw_no_io_ops,
				s_ctrl->hw_no_ops);
		return rc;
	}

	rc = camera_io_dev_read(
		&(s_ctrl->io_master_info),
		slave_info->sensor_id_reg_addr,
		&chipid, CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD);

	CAM_DBG(CAM_SENSOR, "%s read id: 0x%x expected id 0x%x:",
		s_ctrl->sensor_name, chipid, slave_info->sensor_id);

	if (cam_sensor_id_by_mask(s_ctrl, chipid) != slave_info->sensor_id) {
		CAM_WARN(CAM_SENSOR, "%s read id: 0x%x expected id 0x%x:",
				s_ctrl->sensor_name, chipid,
				slave_info->sensor_id);
		return -ENODEV;
	}
	return rc;
}

int32_t cam_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int rc = 0, pkt_opcode = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;
	struct timespec64 ts;
	uint64_t ms, sec, min, hrs;

	if (!s_ctrl || !arg) {
		CAM_ERR(CAM_SENSOR, "s_ctrl is NULL");
		return -EINVAL;
	}

	if (cmd->op_code != CAM_SENSOR_PROBE_CMD) {
		if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
			CAM_ERR(CAM_SENSOR, "Invalid handle type: %d",
				cmd->handle_type);
			return -EINVAL;
		}
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	switch (cmd->op_code) {
	case CAM_SENSOR_PROBE_CMD: {
		if (s_ctrl->is_probe_succeed == 1) {
			CAM_WARN(CAM_SENSOR,
				"Sensor %s already Probed in the slot",
				s_ctrl->sensor_name);
			break;
		}

		if (cmd->handle_type ==
			CAM_HANDLE_MEM_HANDLE) {
			rc = cam_handle_mem_ptr(cmd->handle, cmd->op_code,
				s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Get Buffer Handle Failed");
				goto release_mutex;
			}
		} else {
			CAM_ERR(CAM_SENSOR, "Invalid Command Type: %d",
				cmd->handle_type);
			rc = -EINVAL;
			goto release_mutex;
		}

		/* Parse and fill vreg params for powerup settings */
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_setting,
			s_ctrl->sensordata->power_info.power_setting_size);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Fail in filling vreg params for %s PUP rc %d",
				s_ctrl->sensor_name, rc);
			goto free_power_settings;
		}

		/* Parse and fill vreg params for powerdown settings*/
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_down_setting,
			s_ctrl->sensordata->power_info.power_down_setting_size);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Fail in filling vreg params for %s PDOWN rc %d",
				s_ctrl->sensor_name, rc);
			goto free_power_settings;
		}

		if (s_ctrl->is_aon_user) {
			CAM_DBG(CAM_SENSOR,
				"Setup for Main Camera with csiphy index: %d",
				s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY]);
			rc = cam_sensor_util_aon_ops(true,
				s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY]);
			if (rc) {
				CAM_WARN(CAM_SENSOR,
					"Main camera access operation is not successful rc: %d",
					rc);
				goto free_power_settings;
			}
		}

		/* Power up and probe sensor */

		if (!(s_ctrl->hw_no_probe_pw_ops)) {
			rc = cam_sensor_power_up(s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Power up failed for %s sensor_id: 0x%x, slave_addr: 0x%x",
					s_ctrl->sensor_name,
					s_ctrl->sensordata->slave_info.sensor_id,
					s_ctrl->sensordata->slave_info.sensor_slave_addr
					);
				goto free_power_settings;
			}
			rc = cam_sensor_match_id(s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Probe failed for %s slot:%d, slave_addr:0x%x, sensor_id:0x%x",
					s_ctrl->sensor_name,
					s_ctrl->soc_info.index,
					s_ctrl->sensordata->slave_info.sensor_slave_addr,
					s_ctrl->sensordata->slave_info.sensor_id);

				if (!(s_ctrl->hw_no_probe_pw_ops))
					cam_sensor_power_down(s_ctrl);
				msleep(20);
				goto free_power_settings;
			}
		} else {
			CAM_DBG(CAM_SENSOR, "%s-slot[%d] probe with hw_no_probe_pw_ops[%d]",
				s_ctrl->sensor_name,
				s_ctrl->soc_info.index,
				s_ctrl->hw_no_probe_pw_ops);
		}

		if (!(s_ctrl->hw_no_probe_pw_ops)) {
			rc = cam_sensor_power_down(s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Fail in %s sensor Power Down",
					s_ctrl->sensor_name);
				goto free_power_settings;
			}
		}

		if (s_ctrl->is_aon_user) {
			CAM_DBG(CAM_SENSOR,
				"Setup for AON FW with csiphy index: %d",
				s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY]);
			rc = cam_sensor_util_aon_ops(false,
				s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY]);
			if (rc) {
				CAM_WARN(CAM_SENSOR,
					"AON FW access operation is not successful rc: %d",
					rc);
				goto free_power_settings;
			}
		}

		/*
		 * Set probe succeeded flag to 1 so that no other camera shall
		 * probed on this slot
		 */
		s_ctrl->is_probe_succeed = 1;
		s_ctrl->sensor_state = CAM_SENSOR_INIT;

		CAM_INFO(CAM_SENSOR,
				"Probe success for %s slot:%d,slave_addr:0x%x,sensor_id:0x%x",
				s_ctrl->sensor_name,
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_slave_addr,
				s_ctrl->sensordata->slave_info.sensor_id);

	}
		break;
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev sensor_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if ((s_ctrl->is_probe_succeed == 0) ||
			(s_ctrl->sensor_state != CAM_SENSOR_INIT)) {
			CAM_WARN(CAM_SENSOR,
				"Not in right state to aquire %s state: %d",
				s_ctrl->sensor_name, s_ctrl->sensor_state);
			rc = -EINVAL;
			goto release_mutex;
		}

		if (s_ctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_SENSOR,
				"%s Device is already acquired",
				s_ctrl->sensor_name);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = copy_from_user(&sensor_acq_dev,
			u64_to_user_ptr(cmd->handle),
			sizeof(sensor_acq_dev));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed Copying from user");
			goto release_mutex;
		}

		bridge_params.session_hdl  = sensor_acq_dev.session_handle;
		bridge_params.ops          = &s_ctrl->bridge_intf.ops;
		bridge_params.no_crm_ops  = NULL;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = s_ctrl;
		bridge_params.no_crm_priv = NULL;
		bridge_params.dev_id = CAM_SENSOR;
		s_ctrl->bridge_intf.enable_crm = 1;
		/* add crm callbacks only in case of with crm is enabled */
		if (sensor_acq_dev.info_handle & WITH_NO_CRM_MASK) {
			s_ctrl->bridge_intf.enable_crm = 0;
			bridge_params.no_crm_ops  = &s_ctrl->bridge_intf.no_crm_ops;
			bridge_params.no_crm_priv = s_ctrl;
		}

		/* no_hw_io_ops does'nt need to apply anything to hw */
		if (s_ctrl->hw_no_io_ops || s_ctrl->hw_no_ops) {
			bridge_params.no_crm_ops  = NULL;
			bridge_params.no_crm_priv = NULL;
		}

		sensor_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		if (sensor_acq_dev.device_handle <= 0) {
			rc = -EFAULT;
			CAM_ERR(CAM_SENSOR, "Can not create device handle");
			goto release_mutex;
		}
		s_ctrl->bridge_intf.device_hdl = sensor_acq_dev.device_handle;
		s_ctrl->bridge_intf.session_hdl = sensor_acq_dev.session_handle;

		CAM_DBG(CAM_SENSOR, "%s Device Handle: %d",
			s_ctrl->sensor_name, sensor_acq_dev.device_handle);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&sensor_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}

		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Sensor Power up failed for %s sensor_id:0x%x, slave_addr:0x%x",
				s_ctrl->sensor_name,
				s_ctrl->sensordata->slave_info.sensor_id,
				s_ctrl->sensordata->slave_info.sensor_slave_addr
				);
			goto release_mutex;
		}

		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
		s_ctrl->last_flush_req = 0;
		CAM_INFO(CAM_SENSOR,
			"CAM_ACQUIRE_DEV Success for %s id:0x%x,slave-addr:0x%x crm:[%d]",
			s_ctrl->sensor_name,
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr,
			s_ctrl->bridge_intf.enable_crm);
	}
		break;
	case CAM_RELEASE_DEV: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_START)) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
				"Not in right state to release %s state: %d",
				s_ctrl->sensor_name, s_ctrl->sensor_state);
			goto release_mutex;
		}

		if (s_ctrl->bridge_intf.link_hdl != -1) {
			CAM_ERR(CAM_SENSOR,
				"%s Device [%d] still active on link 0x%x",
				s_ctrl->sensor_name,
				s_ctrl->sensor_state,
				s_ctrl->bridge_intf.link_hdl);
			rc = -EAGAIN;
			goto release_mutex;
		}

		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Sensor Power Down failed for %s sensor_id: 0x%x, slave_addr:0x%x",
				s_ctrl->sensor_name,
				s_ctrl->sensordata->slave_info.sensor_id,
				s_ctrl->sensordata->slave_info.sensor_slave_addr
				);
			goto release_mutex;
		}

		cam_sensor_release_per_frame_resource(s_ctrl);
		cam_sensor_release_stream_rsc(s_ctrl);
		if (s_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_SENSOR,
				"Invalid Handles: %s link hdl: %d device hdl: %d",
				s_ctrl->sensor_name,
				s_ctrl->bridge_intf.device_hdl,
				s_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"Failed in destroying %s device hdl",
				s_ctrl->sensor_name);
		s_ctrl->bridge_intf.device_hdl = -1;
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.session_hdl = -1;

		s_ctrl->sensor_state = CAM_SENSOR_INIT;
		CAM_INFO(CAM_SENSOR,
			"CAM_RELEASE_DEV Success for %s sensor_id:0x%x, slave_addr:0x%x",
			s_ctrl->sensor_name,
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
		s_ctrl->streamon_count = 0;
		s_ctrl->streamoff_count = 0;
		s_ctrl->last_flush_req = 0;
		s_ctrl->last_applied_req = 0;
	}
		break;
	case CAM_QUERY_CAP: {
		struct  cam_sensor_query_cap sensor_cap;

		CAM_DBG(CAM_SENSOR, "%s Sensor Queried", s_ctrl->sensor_name);
		cam_sensor_query_cap(s_ctrl, &sensor_cap);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&sensor_cap, sizeof(struct  cam_sensor_query_cap))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		break;
	}
	case CAM_QUERY_CAP_V2: {
		struct  cam_sensor_query_cap_v2 sensor_cap;

		CAM_DBG(CAM_SENSOR, "%s Sensor Queried V2", s_ctrl->sensor_name);
		cam_sensor_query_cap_v2(s_ctrl, &sensor_cap);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&sensor_cap, sizeof(struct  cam_sensor_query_cap_v2))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		break;
	}

	case CAM_START_DEV: {
		struct cam_req_mgr_timer_notify timer;
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_START)) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to start %s state: %d",
			s_ctrl->sensor_name,
			s_ctrl->sensor_state);
			goto release_mutex;
		}

		if (s_ctrl->i2c_data.streamon_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamon_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply streamon settings for %s",
					s_ctrl->sensor_name);
				goto release_mutex;
			}
		}
		s_ctrl->sensor_state = CAM_SENSOR_START;

		if (s_ctrl->bridge_intf.crm_cb &&
			s_ctrl->bridge_intf.crm_cb->notify_timer) {
			timer.link_hdl = s_ctrl->bridge_intf.link_hdl;
			timer.dev_hdl = s_ctrl->bridge_intf.device_hdl;
			timer.state = true;
			rc = s_ctrl->bridge_intf.crm_cb->notify_timer(&timer);
			if (rc) {
				CAM_ERR(CAM_SENSOR,
					"%s Enable CRM SOF freeze timer failed rc: %d",
					s_ctrl->sensor_name, rc);
				return rc;
			}
		}

		CAM_GET_TIMESTAMP(ts);
		CAM_CONVERT_TIMESTAMP_FORMAT(ts, hrs, min, sec, ms);

		CAM_INFO(CAM_SENSOR,
			"%llu:%llu:%llu.%llu CAM_START_DEV Success for %s sensor_id:0x%x,sensor_slave_addr:0x%x",
			hrs, min, sec, ms,
			s_ctrl->sensor_name,
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
	break;
	case CAM_STOP_DEV: {
		if (s_ctrl->sensor_state != CAM_SENSOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to stop %s state: %d",
			s_ctrl->sensor_name, s_ctrl->sensor_state);
			goto release_mutex;
		}

		if (s_ctrl->i2c_data.streamoff_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamoff_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
				"cannot apply streamoff settings for %s",
				s_ctrl->sensor_name);
			}
		}

		cam_sensor_release_per_frame_resource(s_ctrl);
		s_ctrl->last_flush_req = 0;
		s_ctrl->last_applied_req = 0;
		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;

		CAM_GET_TIMESTAMP(ts);
		CAM_CONVERT_TIMESTAMP_FORMAT(ts, hrs, min, sec, ms);

		CAM_INFO(CAM_SENSOR,
			"%llu:%llu:%llu.%llu CAM_STOP_DEV Success for %s sensor_id:0x%x,sensor_slave_addr:0x%x",
			hrs, min, sec, ms,
			s_ctrl->sensor_name,
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
		break;
	case CAM_CONFIG_DEV: {
		if (s_ctrl->hw_no_io_ops) {
			rc = cam_sensor_handle_config_no_io(s_ctrl, arg);
			CAM_DBG(CAM_SENSOR, "Ignore packet parse for hw_no_ops sensor: %d",
					s_ctrl->soc_info.index);
			break;
		}
		rc = cam_sensor_i2c_pkt_parse(s_ctrl, arg);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "%s:Failed i2c pkt parse. rc: %d",
				s_ctrl->sensor_name, rc);
			goto release_mutex;
		}
		if (s_ctrl->i2c_data.init_settings.is_settings_valid &&
			(s_ctrl->i2c_data.init_settings.request_id == 0)) {

			pkt_opcode =
				CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG;
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				pkt_opcode);

			if ((rc == -EAGAIN) &&
			(s_ctrl->io_master_info.master_type == CCI_MASTER)) {
				/* If CCI hardware is resetting we need to wait
				 * for sometime before reapply
				 */
				CAM_WARN(CAM_SENSOR,
					"%s: Reapplying the Init settings due to cci hw reset",
					s_ctrl->sensor_name);
				usleep_range(1000, 1010);
				rc = cam_sensor_apply_settings(s_ctrl, 0,
					pkt_opcode);
			}
			s_ctrl->i2c_data.init_settings.request_id = -1;

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"%s: cannot apply init settings rc= %d",
					s_ctrl->sensor_name, rc);
				delete_request(&s_ctrl->i2c_data.init_settings);
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"%s: Fail in deleting the Init settings",
					s_ctrl->sensor_name);
				goto release_mutex;
			}
		}

		if (s_ctrl->i2c_data.config_settings.is_settings_valid &&
			(s_ctrl->i2c_data.config_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG);

			s_ctrl->i2c_data.config_settings.request_id = -1;

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"%s: cannot apply config settings",
					s_ctrl->sensor_name);
				delete_request(
					&s_ctrl->i2c_data.config_settings);
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.config_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"%s: Fail in deleting the config settings",
					s_ctrl->sensor_name);
				goto release_mutex;
			}
			s_ctrl->sensor_state = CAM_SENSOR_CONFIG;
		}

		if (s_ctrl->i2c_data.read_settings.is_settings_valid) {
			if (!s_ctrl->hw_no_ops)
				rc = cam_sensor_i2c_read_data(
					&s_ctrl->i2c_data.read_settings,
					&s_ctrl->io_master_info);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "%s: cannot read data: %d",
					s_ctrl->sensor_name, rc);
				delete_request(&s_ctrl->i2c_data.read_settings);
				goto release_mutex;
			}
			rc = delete_request(
				&s_ctrl->i2c_data.read_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"%s: Fail in deleting the read settings",
					s_ctrl->sensor_name);
				goto release_mutex;
			}
		}
	}
		break;
	case CAM_FLUSH_REQ: {
		CAM_DBG(CAM_SENSOR,
				"Invalid Flush request for %s slot: %d, sensor_id:0x%x, slave_addr:0x%x. ",
				s_ctrl->sensor_name,
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_id,
				s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
		break;
	default:
		CAM_ERR(CAM_SENSOR, "%s: Invalid Opcode: %d",
			s_ctrl->sensor_name, cmd->op_code);
		rc = -EINVAL;
		goto release_mutex;
	}

release_mutex:
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_down_setting_size = 0;
	power_info->power_setting_size = 0;
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int cam_sensor_publish_dev_info(struct cam_req_mgr_device_info *info)
{
	int rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	if (!info)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(info->dev_hdl);

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	info->dev_id = CAM_REQ_MGR_DEVICE_SENSOR;
	strlcpy(info->name, CAM_SENSOR_NAME, sizeof(info->name));
	if (s_ctrl->pipeline_delay >= 1 && s_ctrl->pipeline_delay <= 3)
		info->p_delay = s_ctrl->pipeline_delay;
	else
		info->p_delay = 2;
	info->trigger = CAM_TRIGGER_POINT_SOF;

	return rc;
}

int cam_sensor_no_crm_handshake(
		struct cam_req_mgr_no_crm_handshake_data *info)
{
	int rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	if (!info) {
		CAM_ERR(CAM_SENSOR, "handshake data: NULL");
		return -EINVAL;
	}

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_no_crm_priv(info->dev_hdl);

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	if (s_ctrl->pipeline_delay >= MIN_PD &&
					s_ctrl->pipeline_delay <= MAX_PD)
		info->pipeline_delay = s_ctrl->pipeline_delay;
	else
		info->pipeline_delay = DEFAULT_PD;

	info->trigger = CAM_TRIGGER_POINT_SOF;
	s_ctrl->bridge_intf.frame_skip_cb = info->frame_skip_cb;
	s_ctrl->anchor_pd = info->anchor_pd;

	return rc;
}

static int dump_settings_array(
	int index,
	uint64_t req_id,
	struct cam_sensor_i2c_reg_setting *s_array)
{
	int i = 0;
	struct cam_sensor_i2c_reg_array *setting_array = NULL;

	if (!s_array)
		return 0;

	setting_array = s_array->reg_setting;

	if (!setting_array)
		return 0;

	for (i = 0; i < s_array->size; i++) {
		CAM_INFO(CAM_SENSOR,
				"slot[%d] req[%llu] reg = %x data = %x\n",
				index,
				req_id,
				setting_array[i].reg_addr,
				setting_array[i].reg_data);
	}

	return 0;
}

static uint64_t cam_sensor_find_latest_req(
	struct cam_sensor_ctrl_t *s_ctrl,
	uint64_t request_id,
	uint64_t last_applied_req_id)
{
	int i, offset = -1;
	uint64_t latest_request_id = 0;
	struct i2c_settings_array *i2c_set = NULL;
	i2c_set = s_ctrl->i2c_data.per_frame;
	offset = request_id % MAX_PER_FRAME_ARRAY;

	/* if current request is non nop send it anyways */
	if ((i2c_set[offset].request_id == request_id) &&
		(i2c_set[offset].is_settings_valid) &&
		(!list_empty(&i2c_set[offset].list_head))) {
		return i2c_set[offset].request_id;
	}

	/* TODO: Better to search in reverse order from request */
	/* find the latest non nop request id which is valid */
	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		if ((i2c_set[i].request_id > latest_request_id) &&
			(i2c_set[i].is_settings_valid) &&
			(i2c_set[i].request_id <= request_id) &&
			(i2c_set[i].request_id > last_applied_req_id) &&
			(!list_empty(&i2c_set[i].list_head))) {
			latest_request_id = i2c_set[i].request_id;
		}
	}

	/*
	 * not able to find any request id in queue
	 * We can send current request id if its in queue
	 * even if its nop as we are not able to find any
	 * non nop in the queue anyways, if non nop is still
	 * not available in kmd send req id as 0
	 **/
	if ((!latest_request_id) &&
		(i2c_set[offset].request_id == request_id) &&
		(i2c_set[offset].is_settings_valid))
		latest_request_id = request_id;

	return latest_request_id;
}

static int cam_sensor_apply_settings_no_crm(
	struct cam_sensor_ctrl_t *s_ctrl,
	struct cam_req_mgr_no_crm_apply_request *notify)
{

	int rc                                = 0;
	uint64_t isp_req_id                   = 0;
	int sensor_pd                         = 2;
	int isp_pd                            = 1;
	uint64_t sensor_req_id                = 0;
	struct i2c_settings_array *i2c_set    = NULL;
	enum cam_sensor_packet_opcodes opcode;

	if (!s_ctrl || !notify) {
		CAM_ERR(CAM_SENSOR, "Invalid params");
		return -EINVAL;
	}

	isp_req_id    = notify->anchor_req_id;
	sensor_pd     = s_ctrl->pipeline_delay;
	isp_pd        = s_ctrl->anchor_pd;
	sensor_req_id = isp_req_id + (sensor_pd - isp_pd);
	opcode        = CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE;
	i2c_set       = s_ctrl->i2c_data.per_frame;

	CAM_DBG(CAM_SENSOR,
				"slot[%d] isp[%llu] sensor[%llu] sensor_pd[%d]",
				s_ctrl->soc_info.index, isp_req_id, sensor_req_id, sensor_pd);

	/* detected a skip */
	if ((sensor_req_id - s_ctrl->last_applied_req) > 1) {
		uint64_t new_req_id = 0;

		new_req_id = cam_sensor_find_latest_req(
							s_ctrl,
							sensor_req_id,
							s_ctrl->last_applied_req);
		if (new_req_id > 0) {
			rc = cam_sensor_apply_settings(
					s_ctrl,
					new_req_id,
					opcode);
			if (!rc) {
				s_ctrl->last_applied_req = new_req_id;
				CAM_ERR(CAM_SENSOR,
							"slot[%d] skiped apply[%llu]",
							s_ctrl->soc_info.index,
							s_ctrl->last_applied_req);
			}
		} else {
			CAM_INFO(CAM_SENSOR,
					"slot[%d] RequestId[%d] not in queue ",
					s_ctrl->soc_info.index,
					sensor_req_id);
		}
	} else {
		/* This is a no skip case */
		int offset = sensor_req_id % MAX_PER_FRAME_ARRAY;
		struct i2c_settings_array *i2c_set = s_ctrl->i2c_data.per_frame;

		if (i2c_set[offset].request_id != sensor_req_id) {
			CAM_INFO(CAM_SENSOR,
						"slot[%d] RequestId[%d] not in queue ",
						s_ctrl->soc_info.index,
						sensor_req_id);
		} else {
			rc = cam_sensor_apply_settings(s_ctrl,
						sensor_req_id,
						opcode);
			if (!rc) {
				s_ctrl->last_applied_req = sensor_req_id;
				CAM_DBG(CAM_SENSOR, "slot[%d] apply[%llu]",
								s_ctrl->soc_info.index,
								s_ctrl->last_applied_req);
			}
		}
	}
	return rc;
}

int cam_sensor_no_crm_apply_req(
	struct cam_req_mgr_no_crm_apply_request *apply)
{
	int rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	s_ctrl = cam_get_device_no_crm_priv(apply->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Invalid private data req[%llu]", apply->anchor_req_id);
		return -EINVAL;
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	rc = cam_sensor_apply_settings_no_crm(s_ctrl, apply);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int cam_sensor_establish_link(struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	if (!link)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(link->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&s_ctrl->cam_sensor_mutex);
	if (link->link_enable) {
		s_ctrl->bridge_intf.link_hdl = link->link_hdl;
		s_ctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.crm_cb = NULL;
	}
	mutex_unlock(&s_ctrl->cam_sensor_mutex);

	return 0;
}

int cam_sensor_power(struct v4l2_subdev *sd, int on)
{
	struct cam_sensor_ctrl_t *s_ctrl = v4l2_get_subdevdata(sd);

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	if (!on && s_ctrl->sensor_state == CAM_SENSOR_START) {
		cam_sensor_power_down(s_ctrl);
		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
	}
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));

	return 0;
}

int cam_sensor_power_up(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_camera_slave_info *slave_info;
	struct cam_hw_soc_info *soc_info =
		&s_ctrl->soc_info;

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: %pK", s_ctrl);
		return -EINVAL;
	}

	if (s_ctrl->hw_no_ops)
		return rc;

	power_info = &s_ctrl->sensordata->power_info;
	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!power_info || !slave_info) {
		CAM_ERR(CAM_SENSOR, "failed: %pK %pK", power_info, slave_info);
		return -EINVAL;
	}

	if (s_ctrl->bob_pwm_switch) {
		rc = cam_sensor_bob_pwm_mode_switch(soc_info,
			s_ctrl->bob_reg_index, true);
		if (rc) {
			CAM_WARN(CAM_SENSOR,
			"BoB PWM setup failed rc: %d", rc);
			rc = 0;
		}
	}

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "core power up failed:%d", rc);
		return rc;
	}

	/*
	 * Allow to do power on however don't initialize cci
	 * if hw no io ops is enabled
	 */
	if (!s_ctrl->hw_no_io_ops) {
		rc = camera_io_init(&(s_ctrl->io_master_info));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "cci_init failed: rc: %d", rc);
			goto cci_failure;
		}
	}

	return rc;
cci_failure:
	if (cam_sensor_util_power_down(power_info, soc_info))
		CAM_ERR(CAM_SENSOR, "power down failure");

	return rc;

}

int cam_sensor_power_down(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info *soc_info;
	int rc = 0;

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: s_ctrl %pK", s_ctrl);
		return -EINVAL;
	}

	if (s_ctrl->hw_no_ops)
		return rc;

	power_info = &s_ctrl->sensordata->power_info;
	soc_info = &s_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_SENSOR, "failed: %s power_info %pK",
			s_ctrl->sensor_name, power_info);
		return -EINVAL;
	}

	rc = cam_sensor_util_power_down(power_info, soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "%s core power down failed:%d",
			s_ctrl->sensor_name, rc);
		return rc;
	}

	if (s_ctrl->bob_pwm_switch) {
		rc = cam_sensor_bob_pwm_mode_switch(soc_info,
			s_ctrl->bob_reg_index, false);
		if (rc) {
			CAM_WARN(CAM_SENSOR,
				"%s BoB PWM setup failed rc: %d",
				s_ctrl->sensor_name, rc);
			rc = 0;
		}
	}

	/*
	 * Don't de init cci in case of hw_no_io_ops
	 */
	if (!s_ctrl->hw_no_io_ops)
		camera_io_release(&(s_ctrl->io_master_info));

	return rc;
}

int cam_sensor_apply_settings(struct cam_sensor_ctrl_t *s_ctrl,
	uint64_t req_id, enum cam_sensor_packet_opcodes opcode)
{
	int rc = 0, offset, i;
	uint64_t top = 0, del_req_id = 0;
	struct i2c_settings_array *i2c_set = NULL;
	struct i2c_settings_list *i2c_list;

	if (req_id == 0) {
		switch (opcode) {
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
			i2c_set = &s_ctrl->i2c_data.streamon_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG: {
			i2c_set = &s_ctrl->i2c_data.init_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG: {
			i2c_set = &s_ctrl->i2c_data.config_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
			i2c_set = &s_ctrl->i2c_data.streamoff_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE_V2:
		default:
			return 0;
		}

		if (s_ctrl->hw_no_io_ops || s_ctrl->hw_no_ops) {
			CAM_DBG(CAM_SENSOR, "Skip apply for no-op sensor: req_id: %d opcode: %d",
					req_id,
					opcode);
			rc = 0;
		} else if (i2c_set->is_settings_valid == 1) {
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
				rc = cam_sensor_i2c_modes_util(
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d",
						rc);
					return rc;
				}
			}
		}
	} else if (req_id > 0) {
		offset = req_id % MAX_PER_FRAME_ARRAY;

		if (opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE)
			i2c_set = s_ctrl->i2c_data.frame_skip;
		else
			i2c_set = s_ctrl->i2c_data.per_frame;

		if (s_ctrl->hw_no_io_ops || s_ctrl->hw_no_ops) {
			CAM_DBG(CAM_SENSOR, "Skip apply for no-op sensor: req_id: %d opcode: %d",
					req_id,
					opcode);
			rc = 0;
		} else if (i2c_set[offset].is_settings_valid == 1 &&
			i2c_set[offset].request_id == req_id) {
			list_for_each_entry(i2c_list,
				&(i2c_set[offset].list_head), list) {
				struct cam_sensor_i2c_reg_setting *s_array =
						&(i2c_list->i2c_settings);

				if (s_ctrl->en_perframe_reg_dump)
					dump_settings_array(
						s_ctrl->soc_info.index,
						req_id,
						s_array);
				rc = cam_sensor_i2c_modes_util(
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d",
						rc);
					return rc;
				}
			}
			CAM_DBG(CAM_SENSOR, "applied req_id: %llu", req_id);
		} else {
			CAM_DBG(CAM_SENSOR,
				"Invalid/NOP request to apply: %lld", req_id);
		}

		/* Change the logic dynamically */
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			if ((req_id >=
				i2c_set[i].request_id) &&
				(top <
				i2c_set[i].request_id) &&
				(i2c_set[i].is_settings_valid
					== 1)) {
				del_req_id = top;
				top = i2c_set[i].request_id;
			}
		}

		if (top < req_id) {
			if ((((top % MAX_PER_FRAME_ARRAY) - (req_id %
				MAX_PER_FRAME_ARRAY)) >= BATCH_SIZE_MAX) ||
				(((top % MAX_PER_FRAME_ARRAY) - (req_id %
				MAX_PER_FRAME_ARRAY)) <= -BATCH_SIZE_MAX))
				del_req_id = req_id;
		}

		if (!del_req_id)
			return rc;

		CAM_DBG(CAM_SENSOR, "top: %llu, del_req_id:%llu",
			top, del_req_id);

		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			if ((del_req_id >
				 i2c_set[i].request_id) && (
				 i2c_set[i].is_settings_valid
					== 1)) {
				i2c_set[i].request_id = 0;
				rc = delete_request(
					&(i2c_set[i]));
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"Delete request Fail:%lld rc:%d",
						del_req_id, rc);
			}
		}
	}

	return rc;
}

int32_t cam_sensor_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	enum cam_sensor_packet_opcodes opcode =
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE;

	if (!apply)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	CAM_DBG(CAM_REQ, " Sensor[%d] update req id: %lld",
		s_ctrl->soc_info.index, apply->request_id);
	trace_cam_apply_req("Sensor", s_ctrl->soc_info.index, apply->request_id, apply->link_hdl);
	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	rc = cam_sensor_apply_settings(s_ctrl, apply->request_id,
		opcode);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int32_t cam_sensor_notify_frame_skip(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	enum cam_sensor_packet_opcodes opcode =
		CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE;

	if (!apply)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	CAM_DBG(CAM_REQ, " Sensor[%d] handle frame skip for req id: %lld",
		s_ctrl->soc_info.index, apply->request_id);
	trace_cam_notify_frame_skip("Sensor", apply->request_id);
	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	rc = cam_sensor_apply_settings(s_ctrl, apply->request_id,
		opcode);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int32_t cam_sensor_flush_request(struct cam_req_mgr_flush_request *flush_req)
{
	int32_t rc = 0, i;
	uint32_t cancel_req_id_found = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	struct i2c_settings_array *i2c_set = NULL;

	if (!flush_req)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(flush_req->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	if ((s_ctrl->sensor_state != CAM_SENSOR_START) &&
		(s_ctrl->sensor_state != CAM_SENSOR_CONFIG)) {
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return rc;
	}

	if (s_ctrl->i2c_data.per_frame == NULL) {
		CAM_ERR(CAM_SENSOR, "i2c frame data is NULL");
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return -EINVAL;
	}

	if (s_ctrl->i2c_data.frame_skip == NULL) {
		CAM_ERR(CAM_SENSOR, "i2c not ready data is NULL");
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return -EINVAL;
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		s_ctrl->last_flush_req = flush_req->req_id;
		CAM_DBG(CAM_SENSOR, "last reqest to flush is %lld",
			flush_req->req_id);
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		i2c_set = &(s_ctrl->i2c_data.per_frame[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (i2c_set->request_id != flush_req->req_id))
			continue;

		if (i2c_set->is_settings_valid == 1) {
			rc = delete_request(i2c_set);
			if (rc < 0)
				CAM_ERR(CAM_SENSOR,
					"delete request: %lld rc: %d",
					i2c_set->request_id, rc);

			if (flush_req->type ==
				CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
				cancel_req_id_found = 1;
				break;
			}
		}
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		i2c_set = &(s_ctrl->i2c_data.frame_skip[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (i2c_set->request_id != flush_req->req_id))
			continue;

		if (i2c_set->is_settings_valid == 1) {
			rc = delete_request(i2c_set);
			if (rc < 0)
				CAM_ERR(CAM_SENSOR,
					"delete request for not ready packet: %lld rc: %d",
					i2c_set->request_id, rc);

			if (flush_req->type ==
				CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
				cancel_req_id_found = 1;
				break;
			}
		}
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ &&
		!cancel_req_id_found)
		CAM_DBG(CAM_SENSOR,
			"Flush request id:%lld not found in the pending list",
			flush_req->req_id);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}
