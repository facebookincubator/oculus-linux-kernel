/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_cfg80211_afc.c
 *
 * Defines AFC cfg80211 vendor command interface handles
 */

#include <wlan_cfg80211.h>
#include <wlan_cfg80211_afc.h>
#include <wlan_reg_ucfg_api.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_osif_priv.h>
#include <wlan_afc_ucfg_api.h>

/* Maximum AFC data length can pass to target limited by platform driver */
#define IF_AFC_RESPONSE_MAX_LEN  4096

/*
 * JSON format AFC response data maximum length, limited by interface,
 * struct wlan_afc_host_resp is AFC response format pass to target.
 */
#define QCA_NL80211_AFC_RESP_DATA_MAX_SIZE  \
	(IF_AFC_RESPONSE_MAX_LEN - sizeof(struct wlan_afc_host_resp))

/**
 * struct frange_obj - Structure of channel frequency range with psd
 * @freq_start: Frequency range start in MHz
 * @freq_end: Frequency range end in MHz
 * @psd: The PSD power info (dBm/MHz) multiplied by a factor of 100 to
 * preserve granularity up to 2 decimal places
 */
struct frange_obj {
	qdf_freq_t freq_start;
	qdf_freq_t freq_end;
	uint32_t psd;
};

/**
 * struct channel_eirp - Structure of channel with eirp
 * @channel_cfi: Channel center frequency index
 * @eirp: The EIRP power info (dBm) multiplied by a factor of 100 to
 * preserve granularity up to 2 decimal places
 */
struct channel_eirp {
	uint8_t channel_cfi;
	uint32_t eirp;
};

/**
 * struct opclass_eirp_obj - Structure of operation class eirp object
 * @opclass: Operation class number
 * @num_channel: Number of channels belongs to this opclass
 * @chan_eirp: Channel eirp structure list
 */
struct opclass_eirp_obj {
	uint8_t opclass;
	uint8_t num_channel;
	struct channel_eirp chan_eirp[REG_MAX_CHANNELS_PER_OPERATING_CLASS];
};

/**
 * struct afc_resp_extracted - Structure of AFC response extracted from
 * AFC vendor response
 * @json_data: Pointer to JSON data buffer
 * @json_len: JSON data length
 * @time_to_live: Time to live of AFC response in seconds
 * @request_id: Request ID
 * @avail_exp_date: Expire date
 * Date format: bits 7:0   - DD (Day 1-31)
 *              bits 15:8  - MM (Month 1-12)
 *              bits 31:16 - YYYY (Year)
 * @avail_exp_time: Expire time
 * Time format: bits 7:0   - SS (Seconds 0-59)
 *              bits 15:8  - MM (Minutes 0-59)
 *              bits 23:16 - HH (Hours 0-23)
 *              bits 31:24 - Reserved
 * @afc_serv_resp_code: AFC server respond code
 * -1: General Failure.
 * 0: Success.
 * 100 - 199: General errors related to protocol.
 * 300 - 399: Error events specific to message exchange
 *            for the Available Spectrum Inquiry.
 * @num_frange_obj: Number of frequency range objects
 * @frange: Array of frequency range object
 * @num_opclass: Number of operation class channel eirp objects
 * @op_obj: Array of operation class channel eirp objects
 */
struct afc_resp_extracted {
	uint8_t *json_data;
	uint32_t json_len;
	uint32_t time_to_live;
	uint32_t request_id;
	uint32_t avail_exp_date;
	uint32_t avail_exp_time;
	int32_t  afc_serv_resp_code;
	uint32_t num_frange_obj;
	struct frange_obj frange[NUM_6GHZ_CHANNELS];
	uint32_t num_opclass;
	struct opclass_eirp_obj op_obj[REG_MAX_SUPP_OPER_CLASSES];
};

const struct nla_policy
wlan_cfg80211_afc_response_policy[QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA] = { .type = NLA_STRING,
				.len = QCA_NL80211_AFC_RESP_DATA_MAX_SIZE },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE] = {
							.type = NLA_S32 },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO] = {
							.type = NLA_NESTED },
};

#define nla_nest_end_checked(skb, start) do {		\
	if ((skb) && (start))				\
		nla_nest_end(skb, start);		\
} while (0)

/**
 * afc_expiry_event_update_or_get_len() - Function to fill vendor event buffer
 * with info extracted from AFC request, or get required vendor buffer length.
 * @vendor_event: Pointer to vendor event SK buffer structure
 * @afc_req: Pointer to AFC request from regulatory component
 *
 * If vendor_event is NULL, to get vendor buffer length, otherwise
 * to fill vendor event buffer with info
 *
 * Return: If get vendor buffer length, return positive value as length,
 * If fill vendor event  0 if success, otherwise negative error code
 */
static int
afc_expiry_event_update_or_get_len(struct sk_buff *vendor_event,
				   struct wlan_afc_host_request *afc_req)
{
	struct nlattr *nla_attr;
	struct nlattr *freq_info;
	struct nlattr *opclass_info = NULL;
	struct nlattr *chan_list = NULL;
	struct nlattr *chan_info = NULL;
	int i, j, len = NLMSG_HDRLEN;
	struct wlan_afc_opclass_obj *afc_opclass_obj;

	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE,
		       QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY)) {
		osif_err("QCA_WLAN_VENDOR_AFC_EVENT_TYPE_EXPIRY put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event &&
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID,
			afc_req->req_id)) {
		osif_err("QCA_WLAN_VENDOR_ATTR_AFC_REQ_ID put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AFC_WFA_VERSION,
			(afc_req->version_major << 16) |
			afc_req->version_minor)) {
		osif_err("AFC EVENT WFA version put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
	    nla_put_u16(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_MIN_DES_POWER,
			afc_req->min_des_power)) {
		osif_err("QCA_WLAN_VENDOR_ATTR_AFC_REQ_MIN_DES_PWR put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u16));
	}

	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_AP_DEPLOYMENT,
		       afc_req->afc_location->deployment_type)) {
		osif_err("AFC EVENT AP deployment put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event) {
		/* Update the frequency range list from the Expiry event */
		nla_attr = nla_nest_start(vendor_event,
					  QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST);
		if (!nla_attr) {
			osif_err("AFC FREQ RANGE LIST start put fail");
			goto fail;
		}
	} else {
		len += nla_total_size(0);
	}

	for (i = 0; i < afc_req->freq_lst->num_ranges; i++) {
		if (vendor_event) {
			freq_info = nla_nest_start(vendor_event, i);
			if (!freq_info) {
				osif_err("Fail to put freq list nest %d", i);
				goto fail;
			}
		} else {
			len += nla_total_size(0);
		}

		if (vendor_event &&
		    (nla_put_u32(vendor_event,
				 QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START,
				 afc_req->freq_lst->range_objs[i].lowfreq) ||
		    nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END,
				afc_req->freq_lst->range_objs[i].highfreq))) {
			osif_err("AFC REQ FREQ RANGE LIST put fail, num %d",
				 afc_req->freq_lst->num_ranges);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u32)) * 2;
		}
		nla_nest_end_checked(vendor_event, freq_info);
	}
	nla_nest_end_checked(vendor_event, nla_attr);

	if (vendor_event) {
		/* Update the Operating class and channel list */
		nla_attr = nla_nest_start(vendor_event,
					  QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST);
		if (!nla_attr) {
			osif_err("AFC OPCLASS CHAN LIST start put fail");
			goto fail;
		}
	} else {
		len += nla_total_size(0);
	}

	for (i = 0; i < afc_req->opclass_obj_lst->num_opclass_objs; i++) {
		if (vendor_event) {
			opclass_info = nla_nest_start(vendor_event, i);
			if (!opclass_info) {
				osif_err("Fail to put opclass nest %d", i);
				goto fail;
			}
		} else {
			len += nla_total_size(0);
		}

		afc_opclass_obj = &afc_req->opclass_obj_lst->opclass_objs[i];

		if (vendor_event &&
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS,
			       afc_opclass_obj->opclass)) {
			osif_err("AFC OPCLASS INFO OPCLASS put fail, num %d",
				 afc_req->opclass_obj_lst->num_opclass_objs);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u8));
		}

		if (vendor_event) {
			chan_list = nla_nest_start(vendor_event,
						   QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST);
			if (!chan_list) {
				osif_err("AFC OPCLASS INFO CHAN LIST start put fail");
				goto fail;
			}
		} else {
			len += nla_total_size(0);
		}

		for (j = 0; j < afc_opclass_obj->opclass_num_cfis; j++) {
			if (vendor_event) {
				chan_info = nla_nest_start(vendor_event, j);
				if (!chan_info) {
					osif_err("Fail to put opclass cfis nest %d", j);
					goto fail;
				}
			} else {
				len += nla_total_size(0);
			}

			if (vendor_event &&
			    nla_put_u8(vendor_event,
				       QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM,
				       afc_opclass_obj->cfis[j])) {
				osif_err("AFC EIRP INFO CHAN NUM put fail, num %d",
					 afc_opclass_obj->opclass_num_cfis);
				goto fail;
			} else {
				len += nla_total_size(sizeof(u8));
			}
			nla_nest_end_checked(vendor_event, chan_info);
		}
		nla_nest_end_checked(vendor_event, chan_list);
		nla_nest_end_checked(vendor_event, opclass_info);
	}
	nla_nest_end_checked(vendor_event, nla_attr);

	return vendor_event ? 0 : len;

fail:
	return -EINVAL;
}

/**
 * afc_power_event_update_or_get_len() - Function to fill vendor event buffer
 * with AFC power update event or get required vendor buffer length
 * @vendor_event: Pointer to vendor event SK buffer
 * @pwr_evt: Pointer to AFC power event
 *
 * If vendor_event is NULL, to get vendor buffer length, otherwise
 * to fill vendor event buffer with info
 *
 * Return: If get vendor buffer length, return positive value as length,
 * If fill vendor event, 0 if success, otherwise negative error code
 */
static int
afc_power_event_update_or_get_len(struct sk_buff *vendor_event,
				  struct reg_fw_afc_power_event *pwr_evt)
{
	struct afc_chan_obj *pow_evt_chan_info = NULL;
	struct chan_eirp_obj *pow_evt_eirp_info = NULL;
	struct nlattr *nla_attr;
	struct nlattr *freq_info;
	struct nlattr *opclass_info;
	struct nlattr *chan_list;
	struct nlattr *chan_info = NULL;
	int i, j, len = NLMSG_HDRLEN;

	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_TYPE,
		       QCA_WLAN_VENDOR_AFC_EVENT_TYPE_POWER_UPDATE_COMPLETE)) {
		osif_err("AFC power update complete event type put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event &&
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID,
			pwr_evt->resp_id)) {
		osif_err("QCA_WLAN_VENDOR_ATTR_AFC_EVENT_REQ_ID put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_AFC_EVENT_STATUS_CODE,
		       pwr_evt->fw_status_code)) {
		osif_err("AFC EVENT STATUS CODE put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u8));
	}

	if (vendor_event &&
	    nla_put_s32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_SERVER_RESP_CODE,
			pwr_evt->serv_resp_code)) {
		osif_err("AFC EVENT SERVER RESP CODE put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(s32));
	}

	if (vendor_event &&
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_DATE,
			pwr_evt->avail_exp_time_d)) {
		osif_err("AFC EVENT EXPIRE DATE put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event &&
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_AFC_EVENT_EXP_TIME,
			pwr_evt->avail_exp_time_t)) {
		osif_err("AFC EVENT EXPIRE TIME put fail");
		goto fail;
	} else {
		len += nla_total_size(sizeof(u32));
	}

	if (vendor_event) {
		/* Update the Frequency and corresponding PSD info */
		nla_attr = nla_nest_start(vendor_event,
					  QCA_WLAN_VENDOR_ATTR_AFC_EVENT_FREQ_RANGE_LIST);
		if (!nla_attr)
			goto fail;
	} else {
		len += nla_total_size(0);
	}

	for (i = 0; i < pwr_evt->num_freq_objs; i++) {
		if (vendor_event) {
			freq_info = nla_nest_start(vendor_event, i);
			if (!freq_info)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		if (vendor_event &&
		    (nla_put_u32(vendor_event,
				 QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START,
				 pwr_evt->afc_freq_info[i].low_freq) ||
		    nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END,
				pwr_evt->afc_freq_info[i].high_freq) ||
		    nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD,
				pwr_evt->afc_freq_info[i].max_psd))) {
			osif_err("AFC FREQUENCY PSD INFO put failed, num %d",
				 pwr_evt->num_freq_objs);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u32)) * 3;
		}
		nla_nest_end_checked(vendor_event, freq_info);
	}
	nla_nest_end_checked(vendor_event, nla_attr);

	if (vendor_event) {
		/* Update the Operating class, channel list and EIRP info */
		nla_attr = nla_nest_start(vendor_event,
					  QCA_WLAN_VENDOR_ATTR_AFC_EVENT_OPCLASS_CHAN_LIST);
		if (!nla_attr)
			goto fail;
	} else {
		len += nla_total_size(0);
	}

	pow_evt_chan_info = pwr_evt->afc_chan_info;

	for (i = 0; i < pwr_evt->num_chan_objs; i++) {
		if (vendor_event) {
			opclass_info = nla_nest_start(vendor_event, i);
			if (!opclass_info)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		if (vendor_event &&
		    nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS,
			       pow_evt_chan_info[i].global_opclass)) {
			osif_err("AFC OPCLASS INFO put fail, num %d",
				 pwr_evt->num_chan_objs);
			goto fail;
		} else {
			len += nla_total_size(sizeof(u8));
		}

		if (vendor_event) {
			chan_list = nla_nest_start(vendor_event,
						   QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST);
			if (!chan_list)
				goto fail;
		} else {
			len += nla_total_size(0);
		}

		pow_evt_eirp_info = pow_evt_chan_info[i].chan_eirp_info;

		for (j = 0; j < pow_evt_chan_info[i].num_chans; j++) {
			if (vendor_event) {
				chan_info = nla_nest_start(vendor_event, j);
				if (!chan_info)
					goto fail;
			} else {
				len += nla_total_size(0);
			}

			if (vendor_event &&
			    (nla_put_u8(vendor_event,
					QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM,
					pow_evt_eirp_info[j].cfi) ||
			    nla_put_u32(vendor_event,
					QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP,
					pow_evt_eirp_info[j].eirp_power))) {
				osif_err("AFC CHAN EIRP_INFO put fail, num %d",
					 pow_evt_chan_info[i].num_chans);
				goto fail;
			} else {
				len += nla_total_size(sizeof(u8));
				len += nla_total_size(sizeof(u32));
			}
			nla_nest_end_checked(vendor_event, chan_info);
		}
		nla_nest_end_checked(vendor_event, chan_list);
		nla_nest_end_checked(vendor_event, opclass_info);
	}

	nla_nest_end_checked(vendor_event, nla_attr);

	return vendor_event ? 0 : len;

fail:
	return -EINVAL;
}

int wlan_cfg80211_afc_send_request(struct wlan_objmgr_pdev *pdev,
				   struct wlan_afc_host_request *afc_req)
{
	struct sk_buff *vendor_event;
	struct pdev_osif_priv *osif_priv;
	int ret, vendor_buffer_len;

	osif_priv = wlan_pdev_get_ospriv(pdev);
	if (!osif_priv) {
		osif_err("PDEV OS private structure is NULL");
		return -EINVAL;
	}

	if (!afc_req) {
		osif_err("afc host request is NULL");
		return -EINVAL;
	}

	vendor_buffer_len = afc_expiry_event_update_or_get_len(NULL, afc_req);

	vendor_event = wlan_cfg80211_vendor_event_alloc(osif_priv->wiphy,
							NULL,
							vendor_buffer_len,
							QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX,
							GFP_ATOMIC);
	if (!vendor_event) {
		osif_err("cfg80211 vendor event alloc failed");
		return -ENOMEM;
	}

	ret = afc_expiry_event_update_or_get_len(vendor_event, afc_req);

	if (ret) {
		osif_err("Failed to update AFC request vendor event");
		goto fail;
	}

	osif_debug("Sending AFC expiry event to user application");
	wlan_cfg80211_vendor_event(vendor_event, GFP_ATOMIC);

	return 0;

fail:
	wlan_cfg80211_vendor_free_skb(vendor_event);
	return -EINVAL;
}

int
wlan_cfg80211_afc_send_update_complete(struct wlan_objmgr_pdev *pdev,
				       struct reg_fw_afc_power_event *afc_evt)
{
	struct sk_buff *vendor_event;
	struct pdev_osif_priv *osif_priv;
	int vendor_buffer_len;

	osif_priv = wlan_pdev_get_ospriv(pdev);
	if (!osif_priv) {
		osif_err("PDEV OS private structure is NULL");
		return -EINVAL;
	}

	if (!afc_evt) {
		osif_err("afc power event is NULL");
		return -EINVAL;
	}

	vendor_buffer_len = afc_power_event_update_or_get_len(NULL, afc_evt);

	vendor_event = wlan_cfg80211_vendor_event_alloc(osif_priv->wiphy,
							NULL,
							vendor_buffer_len,
							QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX,
							GFP_ATOMIC);
	if (!vendor_event) {
		osif_err("cfg80211 vendor event alloc failed");
		return -ENOMEM;
	}

	if (afc_power_event_update_or_get_len(vendor_event, afc_evt)) {
		osif_err("Failed to update AFC power vendor event");
		goto fail;
	}

	osif_debug("Sending AFC update complete event to user application");
	wlan_cfg80211_vendor_event(vendor_event, GFP_ATOMIC);

	return 0;

fail:
	wlan_cfg80211_vendor_free_skb(vendor_event);
	return -EINVAL;
}

/**
 * afc_response_display() - Function to display AFC response information
 * @rsp: Pointer to AFC response structure which is extracted from vendor
 * command
 *
 * Return: None
 */
static void afc_response_display(struct afc_resp_extracted *rsp)
{
	int iter, j;

	if (rsp->json_data)
		return;

	osif_debug("Req ID: %u TTL: %u Date: 0x%x Time: 0x%x Resp code: %u Freq objs: %u Opclass objs: %u",
		   rsp->request_id,
		   rsp->time_to_live,
		   rsp->avail_exp_date,
		   rsp->avail_exp_time,
		   rsp->afc_serv_resp_code,
		   rsp->num_frange_obj,
		   rsp->num_opclass);

	for (iter = 0; iter < rsp->num_frange_obj; iter++)
		osif_debug("Freq Info[%d]: start %u end %u PSD %u",
			   iter,
			   rsp->frange[iter].freq_start,
			   rsp->frange[iter].freq_end,
			   rsp->frange[iter].psd);

	for (iter = 0; iter < rsp->num_opclass; iter++) {
		osif_debug("Opclass[%d]: %u Num channels: %u",
			   iter,
			   rsp->op_obj[iter].opclass,
			   rsp->op_obj[iter].num_channel);

		for (j = 0; j < rsp->op_obj[iter].num_channel; j++)
			osif_debug("Channel Info[%d]:CFI: %u EIRP: %u",
				   j,
				   rsp->op_obj[iter].chan_eirp[j].channel_cfi,
				   rsp->op_obj[iter].chan_eirp[j].eirp);
	}
}

/**
 * wlan_parse_afc_rsp_freq_psd() - Function to parse AFC response channel
 * frequency range PSD information from NL attribute.
 * @attr: Pointer to NL AFC frequency PSD attributes
 * @rsp: Pointer to AFC extracted response
 *
 * Return: Negative error number if failed, otherwise success
 */
static int
wlan_parse_afc_rsp_freq_psd(struct nlattr *attr, struct afc_resp_extracted *rsp)
{
	int ret = -EINVAL;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_MAX + 1];
	struct nlattr *cur_attr = NULL, *tb2;
	uint32_t rem;
	qdf_size_t i = 0;

	nla_for_each_nested(cur_attr, attr, rem) {
		if (i >= NUM_6GHZ_CHANNELS) {
			osif_err("Ignore exceed");
			break;
		}
		if (wlan_cfg80211_nla_parse(tb,
					    QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_MAX,
					    nla_data(cur_attr),
					    nla_len(cur_attr),
					    NULL)) {
			osif_err("Invalid ATTR");
			return ret;
		}

		tb2 = tb[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_START];
		if (tb2)
			rsp->frange[i].freq_start = nla_get_u32(tb2);

		tb2 = tb[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_RANGE_END];
		if (tb2)
			rsp->frange[i].freq_end = nla_get_u32(tb2);

		tb2 = tb[QCA_WLAN_VENDOR_ATTR_AFC_FREQ_PSD_INFO_PSD];
		if (tb2)
			rsp->frange[i].psd = nla_get_u32(tb2);

		i++;
	}

	rsp->num_frange_obj = i;
	return i;
}

/**
 * wlan_parse_afc_rsp_opclass_eirp() - Function to parse AFC response operation
 * class EIRP information from NL attributes.
 * @attr: Pointer to NL AFC operation class EIRP attributes
 * @rsp: Pointer to AFC extracted response
 *
 * Return: Negative error number if failed, otherwise success
 */
static int
wlan_parse_afc_rsp_opclass_eirp(struct nlattr *attr,
				struct afc_resp_extracted *rsp)
{
	int ret = -EINVAL;
	struct nlattr *tb1[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX + 1];
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_MAX + 1];
	struct nlattr *cur_attr = NULL, *sub_attr = NULL, *tb;
	uint32_t rem, sub_rem;
	int i = 0, ch_idx;

	nla_for_each_nested(cur_attr, attr, rem) {
		if (i >= REG_MAX_SUPP_OPER_CLASSES) {
			osif_err("Ignore opclass list exceed");
			break;
		}
		if (wlan_cfg80211_nla_parse(tb1,
					    QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_MAX,
					    nla_data(cur_attr),
					    nla_len(cur_attr),
					    NULL)) {
			osif_err("Invalid ATTR");
			return ret;
		}
		tb = tb1[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_OPCLASS];
		if (tb)
			rsp->op_obj[i].opclass = nla_get_u8(tb);

		tb = tb1[QCA_WLAN_VENDOR_ATTR_AFC_OPCLASS_INFO_CHAN_LIST];
		if (!tb) {
			osif_err("No opclass channel list");
			return ret;
		}

		ch_idx = 0;

		nla_for_each_nested(sub_attr, tb, sub_rem) {
			if (ch_idx >= NUM_6GHZ_CHANNELS) {
				osif_err("Ignore eirp list exceed");
				break;
			}
			if (wlan_cfg80211_nla_parse(tb2,
						    QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_MAX,
						    nla_data(sub_attr),
						    nla_len(sub_attr),
						    NULL)) {
				osif_err("Invalid ATTR");
				return ret;
			}
			tb = tb2[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_CHAN_NUM];
			if (tb)
				rsp->op_obj[i].chan_eirp[ch_idx].channel_cfi =
						nla_get_u8(tb);
			tb = tb2[QCA_WLAN_VENDOR_ATTR_AFC_CHAN_EIRP_INFO_EIRP];
			if (tb)
				rsp->op_obj[i].chan_eirp[ch_idx].eirp =
						nla_get_u32(tb);
			ch_idx++;
		}
		rsp->op_obj[i].num_channel = ch_idx;

		i++;
	}
	rsp->num_opclass = i;
	return i;
}

/**
 * free_extract_afc_rsp() - Function to free AFC extracted response
 * @rsp: Pointer to AFC extracted response
 *
 * Return: None
 */
static inline void free_extract_afc_rsp(struct afc_resp_extracted *rsp)
{
	if (!rsp)
		return;

	qdf_mem_free(rsp->json_data);
	qdf_mem_free(rsp);
}

/**
 * extract_afc_resp() - Function to extract AFC response
 * @attr: Pointer to NL attribute array
 *
 * Return: Pointer to AFC response axtracted
 */
static struct afc_resp_extracted *extract_afc_resp(struct nlattr **attr)
{
	struct afc_resp_extracted *afc_rsp;
	struct nlattr *nl;

	if (!attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE]) {
		osif_err("ATTR AFC RESP TIME TO LIVE is required");
		return NULL;
	}

	if (!attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID]) {
		osif_err("ATTR AFC RESP REQ ID is required");
		return NULL;
	}

	if (!attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE]) {
		osif_err("ATTR AFC RESP EXP DATE is required");
		return NULL;
	}

	if (!attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME]) {
		osif_err("ATTR AFC RESP EXP TIME is required");
		return NULL;
	}

	if (!attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE]) {
		osif_err("ATTR AFC RESP SERVER RESP CODE is required");
		return NULL;
	}

	if (!attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO]) {
		osif_err("ATTR AFC RESP FREQ PSD INFO is required");
		return NULL;
	}

	if (!attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO]) {
		osif_err("ATTR AFC RESP OPCLASS CHAN EIRP INFO is required");
		return NULL;
	}

	afc_rsp = qdf_mem_malloc(sizeof(*afc_rsp));
	if (!afc_rsp)
		return NULL;

	if (attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA]) {
		nl = attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_DATA];
		afc_rsp->json_data = qdf_mem_malloc(nla_len(nl));
		if (!afc_rsp->json_data)
			goto fail;

		afc_rsp->json_len = nla_len(nl);
		nla_memcpy(afc_rsp->json_data, nl, afc_rsp->json_len);
	}

	afc_rsp->time_to_live =
		nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_TIME_TO_LIVE]);

	afc_rsp->request_id =
		nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_REQ_ID]);

	afc_rsp->avail_exp_date =
		nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_DATE]);

	afc_rsp->avail_exp_time =
		nla_get_u32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_EXP_TIME]);

	afc_rsp->afc_serv_resp_code =
		nla_get_s32(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_AFC_SERVER_RESP_CODE]);

	if (wlan_parse_afc_rsp_freq_psd(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_FREQ_PSD_INFO],
					afc_rsp) <= 0) {
		osif_err("parse freq psd err");
		goto fail;
	}

	if (wlan_parse_afc_rsp_opclass_eirp(attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_OPCLASS_CHAN_EIRP_INFO],
					    afc_rsp) <= 0) {
		osif_err("parse opclass eirp err");
		goto fail;
	}

	return afc_rsp;
fail:
	osif_err("Error parsing the AFC response from application");
	free_extract_afc_rsp(afc_rsp);
	return NULL;
}

/**
 * is_target_support_json_format() - API to get whether target support JSON
 * format AFC response.
 * @psoc: Pointer to PSOC object
 *
 * Return: Boolean
 */

static inline bool is_target_support_json_format(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

/**
 * fill_host_afc_response_buffer() - Function to fill AFC response buffer which
 * pass to target.
 * @psoc: Pointer to PSOC object
 * @afc_rsp: Pointer to AFC extracted response
 * @host_afc: Pointer to interface AFC response buffer with target
 *
 * Return: Negative error number if failed, otherwise success
 */
static int
fill_host_afc_response_buffer(struct wlan_objmgr_psoc *psoc,
			      struct afc_resp_extracted *afc_rsp,
			      struct wlan_afc_host_resp *host_afc)
{
	int ret = -EINVAL;
	uint32_t bin_len, tmp_len;
	struct wlan_afc_bin_resp_data *afc_bin;
	struct wlan_afc_resp_freq_psd_info *freq_psd;
	struct wlan_afc_resp_opclass_info *op;
	struct wlan_afc_resp_eirp_info *chan_eirp;
	int i, j;

	if (!afc_rsp || !host_afc)
		return ret;

	host_afc->time_to_live = afc_rsp->time_to_live;
	if (is_target_support_json_format(psoc)) {
		if (!afc_rsp->json_data) {
			osif_err("No JSON data");
			return ret;
		}
		if (afc_rsp->json_len >
		    IF_AFC_RESPONSE_MAX_LEN - sizeof(*host_afc)) {
			osif_err("Invalid JSON data len %d", afc_rsp->json_len);
			return ret;
		}
		host_afc->resp_format = REG_AFC_SERV_RESP_FORMAT_JSON;
		host_afc->length = sizeof(*host_afc) + afc_rsp->json_len;
		qdf_mem_copy(host_afc->afc_resp,
			     afc_rsp->json_data,
			     afc_rsp->json_len);
		return host_afc->length;
	}
	host_afc->resp_format = REG_AFC_SERV_RESP_FORMAT_BINARY;
	afc_bin = (struct wlan_afc_bin_resp_data *)host_afc->afc_resp;
	afc_bin->request_id = afc_rsp->request_id;
	afc_bin->avail_exp_time_d = afc_rsp->avail_exp_date;
	afc_bin->avail_exp_time_t = afc_rsp->avail_exp_time;
	afc_bin->afc_serv_resp_code = afc_rsp->afc_serv_resp_code;
	afc_bin->num_frequency_obj = afc_rsp->num_frange_obj;
	afc_bin->num_channel_obj = afc_rsp->num_opclass;
	bin_len = sizeof(*host_afc) + sizeof(*afc_bin);

	if (bin_len + sizeof(*freq_psd) * afc_bin->num_frequency_obj >
	    IF_AFC_RESPONSE_MAX_LEN) {
		osif_err("Invalid number frequency obj %d",
			 afc_bin->num_frequency_obj);
		return ret;
	}
	freq_psd = (struct wlan_afc_resp_freq_psd_info *)
		   ((uint8_t *)host_afc + bin_len);
	for (i = 0; i < afc_bin->num_frequency_obj; i++) {
		freq_psd->freq_info =
			(afc_rsp->frange[i].freq_start & 0x0000FFFF) |
			(afc_rsp->frange[i].freq_end << 16);
		freq_psd->max_psd = afc_rsp->frange[i].psd;
		freq_psd++;
	}
	bin_len += sizeof(*freq_psd) * afc_bin->num_frequency_obj;

	tmp_len = bin_len;
	for (i = 0; i < afc_rsp->num_opclass; i++) {
		tmp_len += sizeof(*op) +
			   sizeof(*chan_eirp) * afc_rsp->op_obj[i].num_channel;
	}
	if (tmp_len > IF_AFC_RESPONSE_MAX_LEN) {
		osif_err("Invalid opclass channel eirp info");
		return ret;
	}

	op = (struct wlan_afc_resp_opclass_info *)
	     ((uint8_t *)host_afc + bin_len);
	for (i = 0; i < afc_rsp->num_opclass; i++) {
		op->opclass = afc_rsp->op_obj[i].opclass;
		op->num_channels = afc_rsp->op_obj[i].num_channel;
		chan_eirp = (struct wlan_afc_resp_eirp_info *)
			    ((uint8_t *)op + sizeof(*op));
		for (j = 0; j < afc_rsp->op_obj[i].num_channel; j++) {
			chan_eirp->channel_cfi =
				afc_rsp->op_obj[i].chan_eirp[j].channel_cfi;
			chan_eirp->max_eirp_pwr =
				afc_rsp->op_obj[i].chan_eirp[j].eirp;
			chan_eirp++;
		}
		op = (struct wlan_afc_resp_opclass_info *)chan_eirp;
	}

	host_afc->length = tmp_len;

	return tmp_len;
}

int wlan_cfg80211_vendor_afc_response(struct wlan_objmgr_psoc *psoc,
				      struct wlan_objmgr_pdev *pdev,
				      const void *data,
				      int data_len)
{
	int ret = -EINVAL;
	struct nlattr *attr[QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX + 1];
	struct afc_resp_extracted *afc_rsp;
	struct wlan_afc_host_resp *host_afc;
	struct reg_afc_resp_rx_ind_info afc_ind_obj;
	bool is_json = is_target_support_json_format(psoc);

	if (wlan_cfg80211_nla_parse(attr, QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX,
				    data, data_len,
				    wlan_cfg80211_afc_response_policy)) {
		osif_err("Invalid AFC RESP ATTR");
		return ret;
	}

	afc_rsp = extract_afc_resp(attr);
	if (!afc_rsp)
		return ret;

	afc_response_display(afc_rsp);

	host_afc = qdf_mem_malloc(IF_AFC_RESPONSE_MAX_LEN);
	if (!host_afc)
		goto fail;

	ret = fill_host_afc_response_buffer(psoc, afc_rsp, host_afc);
	if (ret <= 0)
		goto fail;

	ret = ucfg_afc_data_send(psoc, pdev, host_afc, ret);
	if (ret) {
		osif_err("Failed to send afc data");
		goto fail;
	}

	afc_ind_obj.cmd_type = REG_AFC_CMD_SERV_RESP_READY;
	afc_ind_obj.serv_resp_format =
				is_json ? REG_AFC_SERV_RESP_FORMAT_JSON :
				REG_AFC_SERV_RESP_FORMAT_BINARY;
	if (ucfg_reg_send_afc_resp_rx_ind(pdev, &afc_ind_obj) !=
	    QDF_STATUS_SUCCESS) {
		osif_err("Failed to send afc rx indication");
		ret = -EINVAL;
	}

fail:
	free_extract_afc_rsp(afc_rsp);
	qdf_mem_free(host_afc);
	return ret;
}
