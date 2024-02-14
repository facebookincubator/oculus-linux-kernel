// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "cam_rpmsg.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"

static int cam_rpmsg_system_dump_ping_payload(
		struct cam_rpmsg_system_ping_payload *pp)
{
	CAM_INFO(CAM_RPMSG, "Ping Packet");
	return 0;
}

static int cam_rpmsg_system_dump_sync_payload(
		struct cam_rpmsg_system_sync_payload *pp)
{
	int i;

	CAM_INFO(CAM_RPMSG, "Sync Packet Dump num_cams %d", pp->num_cams);
	for (i = 0; i < pp->num_cams; i++)
		CAM_INFO(CAM_RPMSG, "camera_id[%d]: %d", i, pp->camera_id[i]);
	return 0;
}

static int cam_rpmsg_isp_dump_acq_payload(
		struct cam_rpmsg_isp_acq_payload *ap)
{
	CAM_INFO(CAM_RPMSG, "Acq ver %d, sensor_id %d",
			ap->version, ap->sensor_id);
	return 0;
}

static int cam_rpmsg_isp_dump_rel_payload(
		struct cam_rpmsg_isp_rel_payload *rp)
{
	CAM_INFO(CAM_RPMSG, "Rel ver %d, sensor_id %d",
			rp->version, rp->sensor_id);
	return 0;
}

static int cam_rpmsg_isp_dump_start_payload(
		struct cam_rpmsg_isp_start_payload *sp)
{
	CAM_INFO(CAM_RPMSG, "start ver %d, sensor_id %d",
			sp->version, sp->sensor_id);
	return 0;
}

static int cam_rpmsg_isp_dump_stop_payload(
		struct cam_rpmsg_isp_stop_payload *sp)
{
	CAM_INFO(CAM_RPMSG, "stop ver %d, sensor_id %d",
			sp->version, sp->sensor_id);
	return 0;
}

static void cam_rpmsg_isp_dump_hw_cfg(void *cfgs, int *rsz, int len)
{
	struct cam_rpmsg_isp_init_hw_cfg *hw_cfg;
	uint32_t *cdm_data;
	int consumed = 0, i = 0;

	if (len < sizeof(struct cam_rpmsg_isp_init_hw_cfg)) {
		CAM_ERR(CAM_RPMSG, "Malformed packet len %d, expected %d",
				len, sizeof(struct cam_rpmsg_isp_init_hw_cfg));
		return;
	}

	hw_cfg = (struct cam_rpmsg_isp_init_hw_cfg *) cfgs;
	CAM_INFO(CAM_RPMSG, "HW_ID %d, size %d", hw_cfg->hw_id, hw_cfg->size);

	*rsz += sizeof(struct cam_rpmsg_isp_init_hw_cfg);
	if (hw_cfg->size % 4 != 0 || hw_cfg->size < 4 || len < hw_cfg->size) {
		CAM_ERR(CAM_RPMSG, "partial hw_cfg len %d, expected %d",
				len, hw_cfg->size);
		return;
	}

	cdm_data = U64_TO_PTR(U64_TO_PTR(cfgs) +
			sizeof(struct cam_rpmsg_isp_init_hw_cfg));
	while (consumed < hw_cfg->size) {
		int type, off, num_ent, j, val, dmi_sel, cdm_sz;

		type = *cdm_data++;
		num_ent = CAM_CDM_GET_SIZE(&type);
		type = CAM_CDM_GET_TYPE(&type);
		cdm_sz = CAM_CDM_GET_PAYLOAD_SIZE(type, num_ent);
		// TODO: cheak if we have sufficient size and add in retuend size
		if (cdm_sz == INT_MAX || consumed + cdm_sz > hw_cfg->size) {
			CAM_ERR(CAM_RPMSG,
				"Malformed Packet pld sz %d hw_cfg_sz %d",
				consumed + cdm_sz, hw_cfg->size);
			return;
		}

		CAM_INFO(CAM_RPMSG, "BL %d CDM type %d num_ent %d", i++, type,
				num_ent);
		switch(type) {
			case CAM_CDM_TYPE_REG_CONT:
				off = *cdm_data++;
				off = CAM_CDM_GET_REG_CONT_OFFSET(&off);
				for (j = 0; j < num_ent; j++) {
					val = *cdm_data++;
					CAM_INFO(CAM_RPMSG, "off %03x val %08x",
							off + j * 4, val);
				}
				break;
			case CAM_CDM_TYPE_REG_RAND:
				for (j = 0; j < num_ent; j++) {
					off = *cdm_data++;
					off = CAM_CDM_GET_REG_RAND_OFFSET(&off);
					val = *cdm_data++;
					CAM_INFO(CAM_RPMSG, "off %03x val %08x",
							off, val);
				}
				break;
			case CAM_CDM_TYPE_SW_DMI32:
				val = *cdm_data++; //reserved word
				val = *cdm_data++;
				dmi_sel = CAM_CDM_GET_SW_DMI32_SEL(&val);
				off = CAM_CDM_GET_SW_DMI32_OFFSET(&val);
				num_ent = (num_ent + 1) / 4;
				CAM_INFO(CAM_RPMSG, "dmi_sel %d, off %d num_ent %d",
						dmi_sel, off, num_ent);
				for (j = 0; j < num_ent; j++) {
					val = *cdm_data++;
					CAM_INFO(CAM_RPMSG, "%d val %08x",
							j, val);
				}
				break;
			default:
				CAM_ERR(CAM_RPMSG,
						"Unknown CDM payload type %d",
						type);
		}
		consumed += cdm_sz;
	}
	*rsz += consumed;
}

static void cam_rpmsg_isp_dump_hw_cfgs(void *cfgs, int n, int *rsz, int len) {
	int i,consumed, tcon = 0;

	for (i = 0; i < n; i++) {
		consumed = 0;
		cam_rpmsg_isp_dump_hw_cfg(cfgs + tcon, &consumed, len - tcon);
		tcon += consumed;
	}
	*rsz = tcon;
}
static void cam_rpmsg_isp_dump_pipeline_cfg(
		struct cam_rpmsg_isp_init_pipeline_cfg *cfg,
		uint32_t len, int *consumed)
{
	int sz, processed = 0, out_port_off, num_out_ports, i, num_hw_cfg;
	struct cam_rpmsg_isp_init_pipeline_cfg *pcfg;
	struct cam_rpmsg_out_port *out_ports;
	int hw_cfgs_sz = 0;

	out_port_off = offsetof(struct cam_rpmsg_isp_init_pipeline_cfg, out_port);
	pcfg = (struct cam_rpmsg_isp_init_pipeline_cfg *)
		(PTR_TO_U64(cfg) + processed);

	if (!len) {
		CAM_ERR(CAM_RPMSG, "packet with no pipeline cfg");
		return;
	} else if (len < 4) {
		CAM_ERR(CAM_RPMSG, "Malformed packet len %d", len);
		return;
	}

	sz = pcfg->size;
	if (len < sz && out_port_off <= sz) {
		CAM_ERR(CAM_RPMSG, "Malformed packet pkt_len %d sz %d off %d",
				len, sz, out_port_off);
		return;
	}
	CAM_INFO(CAM_RPMSG, "sz %d, sensor_mode %d", pcfg->size, pcfg->sensor_mode);

	for (i = 0; i < 4; i++) {
		CAM_INFO(CAM_RPMSG, "idx %d vc %d dt %d", i, pcfg->vcdt[i].vc,
				pcfg->vcdt[i].dt);
	}
	processed += out_port_off;
	num_out_ports = pcfg->num_ports;
	CAM_INFO(CAM_RPMSG, "out_ports %d", num_out_ports);
	out_ports = pcfg->out_port;
	/* size check till num_hw_config */
	if (sizeof(struct cam_rpmsg_out_port) * num_out_ports + 4 >
			len - processed) {
		CAM_ERR(CAM_RPMSG, "Malformed packet len %d, expected %d",
				len - processed,
				sizeof(struct cam_rpmsg_out_port) * num_out_ports + 4);
		return;
	}

	for(i = 0; i < num_out_ports; i++) {
		CAM_INFO(CAM_RPMSG, "type %d h %d w %d f %d", out_ports[i].type,
				out_ports[i].height, out_ports[i].width,
				out_ports[i].format);
	}
	num_hw_cfg = *(uint32_t *)&out_ports[num_out_ports];
	processed += sizeof(struct cam_rpmsg_out_port) * num_out_ports  + 4;
	CAM_INFO(CAM_RPMSG, "num_hw_cfg %d", num_hw_cfg);

	cam_rpmsg_isp_dump_hw_cfgs(U64_TO_PTR(PTR_TO_U64(cfg) + processed),
			num_hw_cfg, &hw_cfgs_sz, len - processed);
	processed += hw_cfgs_sz;
	*consumed = processed;

	return;
}

static void cam_rpmsg_isp_dump_pipeline_cfgs(
		struct cam_rpmsg_isp_init_pipeline_cfg *cfg, uint32_t n,
		uint32_t len)
{
	int i, tot_consumed = 0, consumed;
	struct cam_rpmsg_isp_init_pipeline_cfg *pipeline;

	for(i = 0; i < n; i++) {
		consumed = 0;
		pipeline = U64_TO_PTR(PTR_TO_U64(cfg) + tot_consumed);
		cam_rpmsg_isp_dump_pipeline_cfg(pipeline,
				len - tot_consumed, &consumed);
		tot_consumed += consumed;
	}

	if (len > tot_consumed) {
		CAM_ERR(CAM_RPMSG,
				"packet have more then expected bytes %d > %d",
				len, tot_consumed);
		return;
	}
}

static int cam_rpmsg_isp_dump_init_config(void *pkt, int len)
{
	struct cam_rpmsg_isp_init_cfg_payload *ipld;
	int consumed = 0;

	if (len < sizeof(struct cam_rpmsg_isp_init_cfg_payload)) {
		CAM_ERR(CAM_RPMSG, "Malformed packet len %d, expected %d",
			len, sizeof(struct cam_rpmsg_isp_init_cfg_payload));
		return -1;
	}

	ipld = (struct cam_rpmsg_isp_init_cfg_payload *)pkt;

	CAM_INFO(CAM_RPMSG, "major %d, minor %d", ipld->major_ver,
			ipld->minor_ver);
	CAM_INFO(CAM_RPMSG, "sensor_id %d, num_sen_mode %d", ipld->sensor_id,
			ipld->num_sensor_mode);
	consumed += offsetof(struct cam_rpmsg_isp_init_cfg_payload, cfg);

	cam_rpmsg_isp_dump_pipeline_cfgs(ipld->cfg, ipld->num_sensor_mode, len -
		offsetof(struct cam_rpmsg_isp_init_cfg_payload, cfg));

	return 0;
}

int cam_rpmsg_slave_dump_pkt(void *pkt, size_t len)
{
	struct cam_slave_pkt_hdr *hdr;
	struct cam_rpmsg_slave_payload_desc *phdr;

	hdr = (struct cam_slave_pkt_hdr *)pkt;

	CAM_INFO(CAM_RPMSG, "Header Version: %02x, Direction: %d",
			CAM_RPMSG_SLAVE_GET_HDR_VERSION(hdr),
			CAM_RPMSG_SLAVE_GET_HDR_DIRECTION(hdr));
	CAM_INFO(CAM_RPMSG, "Num Pkt: %d, Pkt Sz %d",
			CAM_RPMSG_SLAVE_GET_HDR_NUM_PACKET(hdr),
			CAM_RPMSG_SLAVE_GET_HDR_PACKET_SZ(hdr));
	if (len != CAM_RPMSG_SLAVE_GET_HDR_PACKET_SZ(hdr)) {
		WARN_ON_ONCE(1);
		CAM_ERR(CAM_RPMSG, "hdr_sz %d does not match with len %zd",
				CAM_RPMSG_SLAVE_GET_HDR_PACKET_SZ(hdr), len);
		return -1;
	}

	phdr = (struct cam_rpmsg_slave_payload_desc *)&hdr[1];

	CAM_INFO(CAM_RPMSG, "Payload Type: %d, Size: %d, reserved %d",
			CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(phdr),
			CAM_RPMSG_SLAVE_GET_PAYLOAD_SIZE(phdr),
			CAM_RPMSG_SLAVE_GET_PAYLOAD_RES(phdr));

	switch(CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(phdr)) {
		case CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_PING:
		{
			struct cam_rpmsg_system_ping_payload *pp =
				(struct cam_rpmsg_system_ping_payload *)pkt;
			return cam_rpmsg_system_dump_ping_payload(pp);
		}
		break;
		case CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_SYNC:
		{
			struct cam_rpmsg_system_sync_payload *pp =
				(struct cam_rpmsg_system_sync_payload *)pkt;
			return cam_rpmsg_system_dump_sync_payload(pp);
		}
		break;
		case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_ACQUIRE:
		{
			struct cam_rpmsg_isp_acq_payload *ap =
				(struct cam_rpmsg_isp_acq_payload *)pkt;
			return cam_rpmsg_isp_dump_acq_payload(ap);
		}
		break;
		case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_RELEASE:
		{
			struct cam_rpmsg_isp_rel_payload *rp =
				(struct cam_rpmsg_isp_rel_payload *)pkt;
			return cam_rpmsg_isp_dump_rel_payload(rp);
		}
		break;
		case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_START_DEV:
		{
			struct cam_rpmsg_isp_start_payload *sp =
				(struct cam_rpmsg_isp_start_payload *)pkt;
			return cam_rpmsg_isp_dump_start_payload(sp);
		}
		break;
		case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_STOP_DEV:
		{
			struct cam_rpmsg_isp_stop_payload *sp =
				(struct cam_rpmsg_isp_stop_payload *)pkt;
			return cam_rpmsg_isp_dump_stop_payload(sp);
		}
		break;
		case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_INIT_CONFIG:
		{
			return cam_rpmsg_isp_dump_init_config(pkt, len);
		}
		break;
		default:
			CAM_WARN(CAM_RPMSG,
				"Dump is not implemented for type %d",
				CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(phdr));
	}
	return 0;
}
