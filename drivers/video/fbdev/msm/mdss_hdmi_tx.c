// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2010-2018, 2020, 2021, The Linux Foundation. All rights reserved. */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/iopoll.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include <linux/types.h>
#include <linux/hdcp_qseecom.h>
#include <linux/msm_mdp.h>
#include <linux/msm_ext_display.h>

#define REG_DUMP 0

#include "mdss_debug.h"
#include "mdss_fb.h"
#include "mdss_hdmi_cec.h"
#include "mdss_hdmi_edid.h"
#include "mdss_hdcp.h"
#include "mdss_hdmi_tx.h"
#include "mdss_hdmi_audio.h"
#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_hdmi_mhl.h"
#include "mdss_hdmi_util.h"

#define DRV_NAME "hdmi-tx"
#define COMPATIBLE_NAME "qcom,hdmi-tx"

#define HDMI_TX_EVT_STR(x) #x
#define DEFAULT_VIDEO_RESOLUTION HDMI_VFRMT_640x480p60_4_3
#define DEFAULT_HDMI_PRIMARY_RESOLUTION HDMI_VFRMT_1920x1080p60_16_9

/* HDMI PHY/PLL bit field macros */
#define SW_RESET BIT(2)
#define SW_RESET_PLL BIT(0)

#define HPD_DISCONNECT_POLARITY 0
#define HPD_CONNECT_POLARITY    1

/*
 * Audio engine may take 1 to 3 sec to shutdown
 * in normal cases. To handle worst cases, making
 * timeout for audio engine shutdown as 5 sec.
 */
#define AUDIO_POLL_SLEEP_US   (5 * 1000)
#define AUDIO_POLL_TIMEOUT_US (AUDIO_POLL_SLEEP_US * 1000)

/* Maximum pixel clock rates for hdmi tx */
#define HDMI_DEFAULT_MAX_PCLK_RATE         148500
#define HDMI_TX_3_MAX_PCLK_RATE            297000
#define HDMI_TX_4_MAX_PCLK_RATE            600000

#define hdmi_tx_get_fd(x) ((x && (ffs(x) > 0))  ? \
			hdmi_ctrl->feature_data[ffs(x) - 1] : NULL)
#define hdmi_tx_set_fd(x, y) {if (x && (ffs(x) > 0)) \
			hdmi_ctrl->feature_data[ffs(x) - 1] = y; }

#define MAX_EDID_READ_RETRY	5
#define HPD_STRING_SIZE		30

#define HDMI_TX_MIN_FPS 20000
#define HDMI_TX_MAX_FPS 120000
#define HDMI_KHZ_TO_HZ 1000

#define HDMI_TX_VERSION_403	0x40000003
#define HDMI_GET_MSB(x)		(x >> 8)
#define HDMI_GET_LSB(x)		(x & 0xff)

/* Enable HDCP by default */
static bool hdcp_feature_on;

/*
 * CN represents IT content type, if ITC bit in infoframe data byte 3
 * is set, CN bits will represent content type as below:
 * 0b00 Graphics
 * 0b01 Photo
 * 0b10 Cinema
 * 0b11 Game
 */
#define CONFIG_CN_BITS(bits, byte) \
		(byte = (byte & ~(BIT(4) | BIT(5))) |\
			((bits & (BIT(0) | BIT(1))) << 4))

enum hdmi_tx_hpd_states {
	HPD_OFF,
	HPD_ON,
	HPD_ON_CONDITIONAL_MTP,
	HPD_DISABLE,
	HPD_ENABLE
};

static int hdmi_tx_set_mhl_hpd(struct platform_device *pdev, uint8_t on);
static int hdmi_tx_sysfs_enable_hpd(struct hdmi_tx_ctrl *hdmi_ctrl, int on);
static irqreturn_t hdmi_tx_isr(int irq, void *data);
static void hdmi_tx_hpd_off(struct hdmi_tx_ctrl *hdmi_ctrl);
static int hdmi_tx_enable_power(struct hdmi_tx_ctrl *hdmi_ctrl,
	enum hdmi_tx_power_module_type module, int enable);
static void hdmi_tx_fps_work(struct work_struct *work);
static int hdmi_tx_pinctrl_set_state(struct hdmi_tx_ctrl *hdmi_ctrl,
			enum hdmi_tx_power_module_type module, bool active);
static int hdmi_tx_audio_info_setup(struct platform_device *pdev,
	struct msm_ext_disp_audio_setup_params *params);
static int hdmi_tx_get_audio_edid_blk(struct platform_device *pdev,
	struct msm_ext_disp_audio_edid_blk *blk);
static int hdmi_tx_get_cable_status(struct platform_device *pdev, u32 vote);
static void hdmi_tx_audio_off(struct hdmi_tx_ctrl *hdmi_ctrl);

static struct mdss_hw hdmi_tx_hw = {
	.hw_ndx = MDSS_HW_HDMI,
	.ptr = NULL,
	.irq_handler = hdmi_tx_isr,
};

static struct dss_gpio hpd_gpio_config[] = {
	{0, 1, COMPATIBLE_NAME "-hpd"},
	{0, 1, COMPATIBLE_NAME "-mux-en"},
	{0, 0, COMPATIBLE_NAME "-mux-sel"},
	{0, 1, COMPATIBLE_NAME "-mux-lpm"}
};

static struct dss_gpio ddc_gpio_config[] = {
	{0, 1, COMPATIBLE_NAME "-ddc-mux-sel"},
	{0, 1, COMPATIBLE_NAME "-ddc-clk"},
	{0, 1, COMPATIBLE_NAME "-ddc-data"}
};

static struct dss_gpio core_gpio_config[] = {
};

static struct dss_gpio cec_gpio_config[] = {
	{0, 1, COMPATIBLE_NAME "-cec"}
};

const char *hdmi_pm_name(enum hdmi_tx_power_module_type module)
{
	switch (module) {
	case HDMI_TX_HPD_PM:	return "HDMI_TX_HPD_PM";
	case HDMI_TX_DDC_PM:	return "HDMI_TX_DDC_PM";
	case HDMI_TX_CORE_PM:	return "HDMI_TX_CORE_PM";
	case HDMI_TX_CEC_PM:	return "HDMI_TX_CEC_PM";
	default: return "???";
	}
} /* hdmi_pm_name */

static int hdmi_tx_get_version(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc;
	struct dss_io_data *io;

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, true);
	if (rc) {
		DEV_ERR("%s: Failed to read HDMI version\n", __func__);
		goto fail;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		rc = -EINVAL;
		goto fail;
	}

	hdmi_ctrl->hdmi_tx_version = DSS_REG_R(io, HDMI_VERSION);
	hdmi_ctrl->hdmi_tx_major_version =
		MDSS_GET_MAJOR(hdmi_ctrl->hdmi_tx_version);

	switch (hdmi_ctrl->hdmi_tx_major_version) {
	case (HDMI_TX_VERSION_3):
		hdmi_ctrl->max_pclk_khz = HDMI_TX_3_MAX_PCLK_RATE;
		break;
	case (HDMI_TX_VERSION_4):
		hdmi_ctrl->max_pclk_khz = HDMI_TX_4_MAX_PCLK_RATE;
		break;
	default:
		hdmi_ctrl->max_pclk_khz = HDMI_DEFAULT_MAX_PCLK_RATE;
		break;
	}

	/*
	 * Override max pclk frequency as indicated in dt.
	 */
	if (hdmi_ctrl->pdata.max_pclk_freq_khz)
		hdmi_ctrl->max_pclk_khz = min(hdmi_ctrl->max_pclk_khz,
				hdmi_ctrl->pdata.max_pclk_freq_khz);

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, false);
	if (rc) {
		DEV_ERR("%s: FAILED to disable power\n", __func__);
		goto fail;
	}

fail:
	return rc;
}

static struct hdmi_tx_ctrl *hdmi_tx_get_data(struct platform_device *pdev)
{
	struct msm_ext_disp_data *ext_data;
	void *audio_data;

	if (!pdev) {
		pr_err("invalid input\n");
		return ERR_PTR(-ENODEV);
	}

	ext_data = platform_get_drvdata(pdev);
	if (!ext_data) {
		pr_err("invalid ext disp data\n");
		return ERR_PTR(-EINVAL);
	}

	audio_data = ext_data->intf_data;
	if (!audio_data) {
		pr_err("invalid intf data\n");
		return ERR_PTR(-EINVAL);
	}

	return container_of(audio_data, struct hdmi_tx_ctrl, audio_data);
}

int register_hdmi_cable_notification(struct ext_disp_cable_notify *handler)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct list_head *pos;

	if (!hdmi_tx_hw.ptr) {
		DEV_WARN("%s: HDMI Tx core not ready\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!handler) {
		DEV_ERR("%s: Empty handler\n", __func__);
		return -ENODEV;
	}

	hdmi_ctrl = (struct hdmi_tx_ctrl *) hdmi_tx_hw.ptr;

	mutex_lock(&hdmi_ctrl->tx_lock);
	handler->status = hdmi_ctrl->hpd_state;
	list_for_each(pos, &hdmi_ctrl->cable_notify_handlers);
	list_add_tail(&handler->link, pos);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return handler->status;
} /* register_hdmi_cable_notification */

int unregister_hdmi_cable_notification(struct ext_disp_cable_notify *handler)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	if (!hdmi_tx_hw.ptr) {
		DEV_WARN("%s: HDMI Tx core not ready\n", __func__);
		return -ENODEV;
	}

	if (!handler) {
		DEV_ERR("%s: Empty handler\n", __func__);
		return -ENODEV;
	}

	hdmi_ctrl = (struct hdmi_tx_ctrl *) hdmi_tx_hw.ptr;

	mutex_lock(&hdmi_ctrl->tx_lock);
	list_del(&handler->link);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return 0;
} /* unregister_hdmi_cable_notification */

static void hdmi_tx_cable_notify_work(struct work_struct *work)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct ext_disp_cable_notify *pos;

	hdmi_ctrl = container_of(work, struct hdmi_tx_ctrl, cable_notify_work);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	list_for_each_entry(pos, &hdmi_ctrl->cable_notify_handlers, link) {
		if (pos->status != hdmi_ctrl->hpd_state) {
			pos->status = hdmi_ctrl->hpd_state;
			pos->hpd_notify(pos);
		}
	}
	mutex_unlock(&hdmi_ctrl->tx_lock);
} /* hdmi_tx_cable_notify_work */

static void hdmi_tx_hdcp1_set_enc(struct hdmi_tx_ctrl *hdmi_ctrl, bool enc)
{
	hdcp_1x_set_enc(hdmi_tx_get_fd(HDMI_TX_FEAT_HDCP), enc);
}

static inline bool hdmi_tx_is_hdcp_enabled(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	return hdmi_ctrl->hdcp_feature_on &&
		(hdmi_ctrl->hdcp14_present || hdmi_ctrl->hdcp22_present) &&
		hdmi_ctrl->hdcp_ops;
}

/*
 * The sink must support at least one electro-optical transfer function for
 * HDMI controller to sendi the dynamic range and mastering infoframe.
 */
static inline bool hdmi_tx_is_hdr_supported(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct hdmi_edid_hdr_data *hdr_data;

	hdmi_edid_get_hdr_data(hdmi_tx_get_fd(HDMI_TX_FEAT_EDID), &hdr_data);

	return (hdr_data->eotf & BIT(0)) || (hdr_data->eotf & BIT(1)) ||
			(hdr_data->eotf & BIT(2));
}

static inline bool hdmi_tx_metadata_type_one(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct hdmi_edid_hdr_data *hdr_data;

	hdmi_edid_get_hdr_data(hdmi_tx_get_fd(HDMI_TX_FEAT_EDID), &hdr_data);

	return hdr_data->metadata_type_one;
}

static inline bool hdmix_tx_sink_dc_support(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	void *edid_fd = hdmi_tx_get_fd(HDMI_TX_FEAT_EDID);

	if (hdmi_ctrl->panel_data.panel_info.out_format == MDP_Y_CBCR_H2V2)
		return (hdmi_edid_get_deep_color(edid_fd) & BIT(4));
	else
		return (hdmi_edid_get_deep_color(edid_fd) & BIT(1));
}

static inline bool hdmi_tx_dc_support(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	/* actual pixel clock if deep color is enabled */
	void *edid_fd = hdmi_tx_get_fd(HDMI_TX_FEAT_EDID);
	u32 tmds_clk_with_dc = hdmi_tx_setup_tmds_clk_rate(
		hdmi_ctrl->timing.pixel_freq,
		hdmi_ctrl->panel.pinfo->out_format,
		true);

	return hdmi_ctrl->dc_feature_on &&
		hdmi_ctrl->dc_support &&
		hdmix_tx_sink_dc_support(hdmi_ctrl) &&
		(tmds_clk_with_dc <= hdmi_edid_get_max_pclk(edid_fd));
}

static const char *hdmi_tx_pm_name(enum hdmi_tx_power_module_type module)
{
	switch (module) {
	case HDMI_TX_HPD_PM:	return "HDMI_TX_HPD_PM";
	case HDMI_TX_DDC_PM:	return "HDMI_TX_DDC_PM";
	case HDMI_TX_CORE_PM:	return "HDMI_TX_CORE_PM";
	case HDMI_TX_CEC_PM:	return "HDMI_TX_CEC_PM";
	default: return "???";
	}
} /* hdmi_tx_pm_name */

static const char *hdmi_tx_io_name(u32 type)
{
	switch (type) {
	case HDMI_TX_CORE_IO:	return "core_physical";
	case HDMI_TX_QFPROM_IO:	return "qfprom_physical";
	case HDMI_TX_HDCP_IO:	return "hdcp_physical";
	default:		return NULL;
	}
} /* hdmi_tx_io_name */

static void hdmi_tx_audio_setup(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (hdmi_ctrl && hdmi_ctrl->audio_ops.on) {
		u32 pclk = hdmi_tx_setup_tmds_clk_rate(
			hdmi_ctrl->timing.pixel_freq,
			hdmi_ctrl->panel.pinfo->out_format,
			hdmi_ctrl->panel.dc_enable);

		hdmi_ctrl->audio_ops.on(hdmi_ctrl->audio_data,
			pclk, &hdmi_ctrl->audio_params);
	}
}

static inline u32 hdmi_tx_is_dvi_mode(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	void *data = hdmi_tx_get_fd(HDMI_TX_FEAT_EDID);

	return hdmi_edid_is_dvi_mode(data);
} /* hdmi_tx_is_dvi_mode */

static inline u32 hdmi_tx_is_in_splash(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	return mdata->handoff_pending;
}

static inline bool hdmi_tx_is_panel_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	return hdmi_ctrl->hpd_state && hdmi_ctrl->panel_power_on;
}

static inline bool hdmi_tx_is_cec_wakeup_en(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	void *fd = NULL;

	if (!hdmi_ctrl)
		return false;

	fd = hdmi_tx_get_fd(HDMI_TX_FEAT_CEC_HW);

	if (!fd)
		return false;

	return hdmi_cec_is_wakeup_en(fd);
}

static inline void hdmi_tx_cec_device_suspend(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	void *fd = NULL;

	if (!hdmi_ctrl)
		return;

	fd = hdmi_tx_get_fd(HDMI_TX_FEAT_CEC_HW);

	if (!fd)
		return;

	hdmi_cec_device_suspend(fd, hdmi_ctrl->panel_suspend);
}

static inline void hdmi_tx_audio_notify(
		struct hdmi_tx_ctrl *hdmi_ctrl, u32 state)
{
	struct msm_ext_disp_init_data *ext;

	if (!hdmi_ctrl) {
		pr_err("invalid hdmi_ctrl data\n");
		return;
	}

	ext = &hdmi_ctrl->ext_audio_data;

	if (!ext || !ext->intf_ops.audio_config ||
			!ext->intf_ops.audio_notify) {
		pr_err("invalid ext audio ops\n");
		return;
	}

	if (state == EXT_DISPLAY_CABLE_CONNECT) {
		ext->intf_ops.audio_config(hdmi_ctrl->ext_pdev,
				&ext->codec, state);

		ext->intf_ops.audio_notify(hdmi_ctrl->ext_pdev,
				&ext->codec, state);

	} else if (state == EXT_DISPLAY_CABLE_DISCONNECT) {
		ext->intf_ops.audio_notify(hdmi_ctrl->ext_pdev,
				&ext->codec, state);

		ext->intf_ops.audio_config(hdmi_ctrl->ext_pdev,
				&ext->codec, state);
	}
}

static void hdmi_tx_send_cable_notification(
		struct hdmi_tx_ctrl *hdmi_ctrl, bool val)
{
	char name[HPD_STRING_SIZE], status[HPD_STRING_SIZE];
	char *envp[3];

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	if (hdmi_ctrl->hpd_notif_state == !!val) {
		pr_debug("%s: no change in HPD state\n", __func__);
		return;
	}

	hdmi_ctrl->hpd_notif_state = !!val;

	snprintf(name, HPD_STRING_SIZE, "name=%s", "HDMI");
	snprintf(status, HPD_STRING_SIZE, "status=%s",
			val ? "connected" : "disconnected");

	pr_debug("[%s]:[%s]\n", name, status);
	envp[0] = name;
	envp[1] = status;
	envp[2] = NULL;

	kobject_uevent_env(hdmi_ctrl->kobj, KOBJ_CHANGE, envp);
} /* hdmi_tx_send_cable_notification */

static inline void hdmi_tx_ack_state(
	struct hdmi_tx_ctrl *hdmi_ctrl, int val)
{
	if (hdmi_ctrl && !hdmi_tx_is_dvi_mode(hdmi_ctrl))
		hdmi_tx_audio_notify(hdmi_ctrl, val);
}

static struct hdmi_tx_ctrl *hdmi_tx_get_drvdata_from_panel_data(
	struct mdss_panel_data *mpd)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	if (mpd) {
		hdmi_ctrl = container_of(mpd, struct hdmi_tx_ctrl, panel_data);
		if (!hdmi_ctrl)
			DEV_ERR("%s: hdmi_ctrl = NULL\n", __func__);
	} else {
		DEV_ERR("%s: mdss_panel_data = NULL\n", __func__);
	}
	return hdmi_ctrl;
} /* hdmi_tx_get_drvdata_from_panel_data */

static struct hdmi_tx_ctrl *hdmi_tx_get_drvdata_from_sysfs_dev(
	struct device *device)
{
	struct msm_fb_data_type *mfd = NULL;
	struct mdss_panel_data *panel_data = NULL;
	struct fb_info *fbi = dev_get_drvdata(device);

	if (fbi) {
		mfd = (struct msm_fb_data_type *)fbi->par;
		panel_data = dev_get_platdata(&mfd->pdev->dev);

		return hdmi_tx_get_drvdata_from_panel_data(panel_data);
	}
	DEV_ERR("%s: fbi = NULL\n", __func__);
	return NULL;
} /* hdmi_tx_get_drvdata_from_sysfs_dev */

/* todo: Fix this. Right now this is declared in hdmi_util.h */
void *hdmi_get_featuredata_from_sysfs_dev(struct device *device,
	u32 feature_type)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	if (!device) {
		DEV_ERR("%s: invalid device\n", __func__);
		return NULL;
	}

	if (feature_type >= HDMI_TX_FEAT_MAX) {
		DEV_ERR("%s: feature type exceeded max value\n", __func__);
		return NULL;
	}

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(device);
	if (hdmi_ctrl)
		return hdmi_tx_get_fd(feature_type);
	else
		return NULL;

} /* hdmi_tx_get_featuredata_from_sysfs_dev */
EXPORT_SYMBOL(hdmi_get_featuredata_from_sysfs_dev);

static int hdmi_tx_config_5v(struct hdmi_tx_ctrl *ctrl, bool enable)
{
	int ret = 0;
	struct dss_module_power *pd = NULL;

	if (!ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	if (ctrl->hdmi_tx_version >= HDMI_TX_VERSION_403)
		ret = hdmi_tx_pinctrl_set_state(ctrl, HDMI_TX_HPD_PM, enable);
	else {
		pd = &ctrl->pdata.power_data[HDMI_TX_HPD_PM];
	if (!pd || !pd->gpio_config) {
		DEV_ERR("%s: Invalid power data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	gpio_set_value(pd->gpio_config->gpio, enable);
	}

end:
	return ret;
}

static ssize_t connected_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdm ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", hdmi_ctrl->hpd_state);
	DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->hpd_state);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return ret;
} /* hdmi_tx_sysfs_rda_connected */

static ssize_t edid_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	int i = 0;
	const char *buf_t = buf;
	const int char_to_nib = 2;
	int edid_size = count / char_to_nib;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	if (!hdmi_ctrl->edid_buf) {
		DEV_ERR("%s: invalid edid buffer\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	if ((edid_size < EDID_BLOCK_SIZE) ||
		(edid_size > hdmi_ctrl->edid_buf_size)) {
		DEV_DBG("%s: disabling custom edid\n", __func__);

		ret = -EINVAL;
		hdmi_ctrl->custom_edid = false;
		goto end;
	}

	memset(hdmi_ctrl->edid_buf, 0, hdmi_ctrl->edid_buf_size);

	while (edid_size--) {
		char t[3]; /* char_to_nib + 1; */
		int d;

		memcpy(t, buf_t, sizeof(char) * char_to_nib);
		t[char_to_nib] = '\0';

		ret = kstrtoint(t, 16, &d);
		if (ret) {
			pr_err("kstrtoint error %d\n", ret);
			goto end;
		}

		memcpy(hdmi_ctrl->edid_buf + i++, &d,
			sizeof(*hdmi_ctrl->edid_buf));

		buf_t += char_to_nib;
	}

	ret = strnlen(buf, PAGE_SIZE);
	hdmi_ctrl->custom_edid = true;
end:
	mutex_unlock(&hdmi_ctrl->tx_lock);
	return ret;
}

static ssize_t edid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	u32 size;
	u32 cea_blks;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	if (!hdmi_ctrl->edid_buf) {
		DEV_ERR("%s: invalid edid buffer\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	cea_blks = hdmi_ctrl->edid_buf[EDID_BLOCK_SIZE - 2];
	if (cea_blks >= MAX_EDID_BLOCKS) {
		DEV_ERR("%s: invalid cea blocks\n", __func__);
		mutex_unlock(&hdmi_ctrl->tx_lock);
		return -EINVAL;
	}
	size = (cea_blks + 1) * EDID_BLOCK_SIZE;
	size = min_t(u32, size, PAGE_SIZE);

	DEV_DBG("%s: edid size %d\n", __func__, size);

	memcpy(buf, hdmi_ctrl->edid_buf, size);

	print_hex_dump(KERN_DEBUG, "HDMI EDID: ", DUMP_PREFIX_NONE,
		16, 1, buf, size, false);

	mutex_unlock(&hdmi_ctrl->tx_lock);
	return size;
}

static int hdmi_tx_update_pixel_clk(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_module_power *power_data = NULL;
	struct mdss_panel_info *pinfo;
	u32 new_clk_rate = 0;
	int rc = 0;

	if (!hdmi_ctrl) {
		pr_err("invalid hdmi ctrl data\n");
		rc = -EINVAL;
		goto end;
	}

	pinfo = &hdmi_ctrl->panel_data.panel_info;

	power_data = &hdmi_ctrl->pdata.power_data[HDMI_TX_CORE_PM];
	if (!power_data) {
		pr_err("Error: invalid power data\n");
		rc = -EINVAL;
		goto end;
	}

	new_clk_rate = hdmi_tx_setup_tmds_clk_rate(pinfo->clk_rate,
				pinfo->out_format, hdmi_ctrl->panel.dc_enable);

	if (power_data->clk_config->rate == new_clk_rate)
		goto end;

	power_data->clk_config->rate = new_clk_rate;

	pr_debug("rate %ld\n", power_data->clk_config->rate);

	rc = msm_dss_clk_set_rate(power_data->clk_config, power_data->num_clk);
	if (rc < 0)
		pr_err("failed to set clock rate %lu\n",
			power_data->clk_config->rate);
end:
	return rc;
}

static ssize_t hot_plug_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int hot_plug, rc;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);

	rc = kstrtoint(buf, 10, &hot_plug);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}

	hdmi_ctrl->hpd_state = !!hot_plug;

	queue_work(hdmi_ctrl->workq, &hdmi_ctrl->hpd_int_work);

	rc = strnlen(buf, PAGE_SIZE);
end:
	mutex_unlock(&hdmi_ctrl->tx_lock);
	return rc;
}

static ssize_t sim_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", hdmi_ctrl->sim_mode);
	DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->sim_mode);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return ret;
}

static ssize_t sim_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int sim_mode, rc;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct dss_io_data *io = NULL;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io is not initialized\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	if (!hdmi_ctrl->hpd_initialized) {
		DEV_ERR("%s: hpd not enabled\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	rc = kstrtoint(buf, 10, &sim_mode);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}

	hdmi_ctrl->sim_mode = !!sim_mode;

	if (hdmi_ctrl->sim_mode) {
		DSS_REG_W(io, HDMI_HPD_INT_CTRL, BIT(0));
	} else {
		int cable_sense = DSS_REG_R(io, HDMI_HPD_INT_STATUS) & BIT(1);

		DSS_REG_W(io, HDMI_HPD_INT_CTRL, BIT(0) | BIT(2) |
			(cable_sense ? 0 : BIT(1)));
	}

	rc = strnlen(buf, PAGE_SIZE);
end:
	mutex_unlock(&hdmi_ctrl->tx_lock);
	return rc;
}

static ssize_t video_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", hdmi_ctrl->vic);
	DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->vic);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return ret;
} /* hdmi_tx_sysfs_rda_video_mode */

static ssize_t hpd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", hdmi_ctrl->hpd_feature_on);
	DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->hpd_feature_on);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return ret;
} /* hdmi_tx_sysfs_rda_hpd */

static ssize_t hpd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int hpd, rc = 0;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);

	rc = kstrtoint(buf, 10, &hpd);
	if (rc) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}

	DEV_DBG("%s: %d\n", __func__, hpd);

	if (hdmi_ctrl->ds_registered && hpd &&
	    (!hdmi_ctrl->mhl_hpd_on || hdmi_ctrl->hpd_feature_on)) {
		DEV_DBG("%s: DS registered, HPD on not allowed\n", __func__);
		goto end;
	}

	switch (hpd) {
	case HPD_OFF:
	case HPD_DISABLE:
		if (hpd == HPD_DISABLE)
			hdmi_ctrl->hpd_disabled = true;

		if (!hdmi_ctrl->hpd_feature_on) {
			DEV_DBG("%s: HPD is already off\n", __func__);
			goto end;
		}

		if (hdmi_ctrl->panel_power_on) {
			hdmi_ctrl->hpd_off_pending = true;
			hdmi_tx_config_5v(hdmi_ctrl, false);
		} else {
			hdmi_tx_hpd_off(hdmi_ctrl);

			hdmi_tx_send_cable_notification(hdmi_ctrl, 0);
			hdmi_tx_audio_notify(hdmi_ctrl, 0);
		}

		break;
	case HPD_ON:
		if (hdmi_ctrl->hpd_disabled) {
			DEV_ERR("%s: hpd is disabled, state %d not allowed\n",
				__func__, hpd);
			goto end;
		}

		if (hdmi_ctrl->pdata.cond_power_on) {
			DEV_ERR("%s: hpd state %d not allowed w/ cond. hpd\n",
				__func__, hpd);
			goto end;
		}

		if (hdmi_ctrl->hpd_feature_on) {
			DEV_DBG("%s: HPD is already on\n", __func__);
			goto end;
		}

		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
		break;
	case HPD_ON_CONDITIONAL_MTP:
		if (hdmi_ctrl->hpd_disabled) {
			DEV_ERR("%s: hpd is disabled, state %d not allowed\n",
				__func__, hpd);
			goto end;
		}

		if (!hdmi_ctrl->pdata.cond_power_on) {
			DEV_ERR("%s: hpd state %d not allowed w/o cond. hpd\n",
				__func__, hpd);
			goto end;
		}

		if (hdmi_ctrl->hpd_feature_on) {
			DEV_DBG("%s: HPD is already on\n", __func__);
			goto end;
		}

		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
		break;
	case HPD_ENABLE:
		hdmi_ctrl->hpd_disabled = false;

		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
		break;
	default:
		DEV_ERR("%s: Invalid HPD state requested\n", __func__);
		goto end;
	}

	if (!rc) {
		hdmi_ctrl->hpd_feature_on =
			(~hdmi_ctrl->hpd_feature_on) & BIT(0);
		DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->hpd_feature_on);
	} else {
		DEV_ERR("%s: failed to '%s' hpd. rc = %d\n", __func__,
			hpd ? "enable" : "disable", rc);
		ret = rc;
	}

end:
	mutex_unlock(&hdmi_ctrl->tx_lock);
	return ret;
} /* hdmi_tx_sysfs_wta_hpd */

static ssize_t vendor_name_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret, sz;
	u8 *s = (u8 *) buf;
	u8 *d = NULL;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	d = hdmi_ctrl->spd_vendor_name;
	ret = strnlen(buf, PAGE_SIZE);
	ret = (ret > 8) ? 8 : ret;

	sz = sizeof(hdmi_ctrl->spd_vendor_name);
	memset(hdmi_ctrl->spd_vendor_name, 0, sz);
	while (*s) {
		if (*s & 0x60 && *s ^ 0x7f) {
			*d = *s;
		} else {
			/* stop copying if control character found */
			break;
		}

		if (++s > (u8 *) (buf + ret))
			break;

		d++;
	}
	hdmi_ctrl->spd_vendor_name[sz - 1] = 0;

	DEV_DBG("%s: '%s'\n", __func__, hdmi_ctrl->spd_vendor_name);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return ret;
} /* hdmi_tx_sysfs_wta_vendor_name */

static ssize_t vendor_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%s\n", hdmi_ctrl->spd_vendor_name);
	DEV_DBG("%s: '%s'\n", __func__, hdmi_ctrl->spd_vendor_name);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return ret;
} /* hdmi_tx_sysfs_rda_vendor_name */

static ssize_t product_description_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret, sz;
	u8 *s = (u8 *) buf;
	u8 *d = NULL;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	d = hdmi_ctrl->spd_product_description;
	ret = strnlen(buf, PAGE_SIZE);
	ret = (ret > 16) ? 16 : ret;

	sz = sizeof(hdmi_ctrl->spd_product_description);
	memset(hdmi_ctrl->spd_product_description, 0, sz);
	while (*s) {
		if (*s & 0x60 && *s ^ 0x7f) {
			*d = *s;
		} else {
			/* stop copying if control character found */
			break;
		}

		if (++s > (u8 *) (buf + ret))
			break;

		d++;
	}
	hdmi_ctrl->spd_product_description[sz - 1] = 0;

	DEV_DBG("%s: '%s'\n", __func__, hdmi_ctrl->spd_product_description);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return ret;
} /* hdmi_tx_sysfs_wta_product_description */

static ssize_t product_description_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%s\n",
		hdmi_ctrl->spd_product_description);
	DEV_DBG("%s: '%s'\n", __func__, hdmi_ctrl->spd_product_description);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return ret;
} /* hdmi_tx_sysfs_rda_product_description */

static ssize_t avi_itc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	int itc = 0;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);

	ret = kstrtoint(buf, 10, &itc);
	if (ret) {
		DEV_ERR("%s: kstrtoint failed. rc =%d\n", __func__, ret);
		goto end;
	}

	if (itc < 0 || itc > 1) {
		DEV_ERR("%s: Invalid ITC %d\n", __func__, itc);
		ret = -EINVAL;
		goto end;
	}

	hdmi_ctrl->panel.is_it_content = itc ? true : false;

	ret = strnlen(buf, PAGE_SIZE);
end:
	mutex_unlock(&hdmi_ctrl->tx_lock);
	return ret;
} /* hdmi_tx_sysfs_wta_avi_itc */

static ssize_t avi_cn0_1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	int cn_bits = 0;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);

	ret = kstrtoint(buf, 10, &cn_bits);
	if (ret) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, ret);
		goto end;
	}

	/* As per CEA-861-E, CN is a positive number and can be max 3 */
	if (cn_bits < 0 || cn_bits > 3) {
		DEV_ERR("%s: Invalid CN %d\n", __func__, cn_bits);
		ret = -EINVAL;
		goto end;
	}

	hdmi_ctrl->panel.content_type = cn_bits;

	ret = strnlen(buf, PAGE_SIZE);
end:
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return ret;
} /* hdmi_tx_sysfs_wta_cn_bits */

static ssize_t s3d_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, s3d_mode;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	void *pdata;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	pdata = hdmi_tx_get_fd(HDMI_TX_FEAT_PANEL);

	mutex_lock(&hdmi_ctrl->tx_lock);

	ret = kstrtoint(buf, 10, &s3d_mode);
	if (ret) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, ret);
		goto end;
	}

	if (s3d_mode < HDMI_S3D_NONE || s3d_mode >= HDMI_S3D_MAX) {
		DEV_ERR("%s: invalid s3d mode = %d\n", __func__, s3d_mode);
		ret = -EINVAL;
		goto end;
	}

	if (s3d_mode > HDMI_S3D_NONE &&
		!hdmi_edid_is_s3d_mode_supported(
		    hdmi_tx_get_fd(HDMI_TX_FEAT_EDID),
			hdmi_ctrl->vic, s3d_mode)) {
		DEV_ERR("%s: s3d mode not supported in current video mode\n",
			__func__);
		ret = -EPERM;
		hdmi_ctrl->panel.s3d_support = false;
		goto end;
	}

	hdmi_ctrl->panel.s3d_mode = s3d_mode;
	hdmi_ctrl->panel.s3d_support = true;

	if (hdmi_ctrl->panel_ops.vendor)
		hdmi_ctrl->panel_ops.vendor(pdata);

	ret = strnlen(buf, PAGE_SIZE);
	DEV_DBG("%s: %d\n", __func__, hdmi_ctrl->s3d_mode);
end:
	mutex_unlock(&hdmi_ctrl->tx_lock);
	return ret;
}

static ssize_t s3d_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_sysfs_dev(dev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", hdmi_ctrl->s3d_mode);
	DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->s3d_mode);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	return ret;
}

static ssize_t fv_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int read, ret;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct dss_module_power *pd = NULL;

	hdmi_ctrl = hdmi_tx_get_drvdata_from_sysfs_dev(dev);
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	pd = &hdmi_ctrl->pdata.power_data[HDMI_TX_HPD_PM];

	if (!pd || !pd->gpio_config) {
		DEV_ERR("%s: Error: invalid power data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	pd = &hdmi_ctrl->pdata.power_data[HDMI_TX_HPD_PM];
	if (!pd || !pd->gpio_config) {
		DEV_ERR("%s: Error: invalid power data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ret = kstrtoint(buf, 10, &read);
	if (ret) {
		DEV_ERR("%s: kstrtoint failed. rc=%d\n", __func__, ret);
		goto end;
	}

	read = ~(!!read ^ pd->gpio_config->value) & BIT(0);

	ret = hdmi_tx_config_5v(hdmi_ctrl, read);
	if (ret)
		goto end;

	ret = strnlen(buf, PAGE_SIZE);
end:
	mutex_unlock(&hdmi_ctrl->tx_lock);
	return ret;
}

static DEVICE_ATTR_RO(connected);
static DEVICE_ATTR_WO(hot_plug);
static DEVICE_ATTR_RW(sim_mode);
static DEVICE_ATTR_RW(edid);
static DEVICE_ATTR_RO(video_mode);
static DEVICE_ATTR_RW(hpd);
static DEVICE_ATTR_RW(vendor_name);
static DEVICE_ATTR_RW(product_description);
static DEVICE_ATTR_WO(avi_itc);
static DEVICE_ATTR_WO(avi_cn0_1);
static DEVICE_ATTR_RW(s3d_mode);
static DEVICE_ATTR_WO(fv);

static struct attribute *hdmi_tx_fs_attrs[] = {
	&dev_attr_connected.attr,
	&dev_attr_hot_plug.attr,
	&dev_attr_sim_mode.attr,
	&dev_attr_edid.attr,
	&dev_attr_video_mode.attr,
	&dev_attr_hpd.attr,
	&dev_attr_vendor_name.attr,
	&dev_attr_product_description.attr,
	&dev_attr_avi_itc.attr,
	&dev_attr_avi_cn0_1.attr,
	&dev_attr_s3d_mode.attr,
	&dev_attr_fv.attr,
	NULL,
};
static struct attribute_group hdmi_tx_fs_attrs_group = {
	.attrs = hdmi_tx_fs_attrs,
};

static int hdmi_tx_sysfs_create(struct hdmi_tx_ctrl *hdmi_ctrl,
	struct fb_info *fbi)
{
	int rc;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	if (!fbi) {
		DEV_ERR("%s: invalid fb info\n", __func__);
		return -ENODEV;
	}

	rc = sysfs_create_group(&fbi->dev->kobj,
		&hdmi_tx_fs_attrs_group);
	if (rc) {
		DEV_ERR("%s: failed, rc=%d\n", __func__, rc);
		return rc;
	}
	hdmi_ctrl->kobj = &fbi->dev->kobj;
	DEV_DBG("%s: sysfs group %pK\n", __func__, hdmi_ctrl->kobj);

	return 0;
} /* hdmi_tx_sysfs_create */

static void hdmi_tx_sysfs_remove(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}
	if (hdmi_ctrl->kobj)
		sysfs_remove_group(hdmi_ctrl->kobj, &hdmi_tx_fs_attrs_group);
	hdmi_ctrl->kobj = NULL;
} /* hdmi_tx_sysfs_remove */

static int hdmi_tx_config_avmute(struct hdmi_tx_ctrl *hdmi_ctrl, bool set)
{
	struct dss_io_data *io;
	u32 av_mute_status;
	bool av_pkt_en = false;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return -EINVAL;
	}

	av_mute_status = DSS_REG_R(io, HDMI_GC);

	if (set) {
		if (!(av_mute_status & BIT(0))) {
			DSS_REG_W(io, HDMI_GC, av_mute_status | BIT(0));
			av_pkt_en = true;
		}
	} else {
		if (av_mute_status & BIT(0)) {
			DSS_REG_W(io, HDMI_GC, av_mute_status & ~BIT(0));
			av_pkt_en = true;
		}
	}

	/* Enable AV Mute tranmission here */
	if (av_pkt_en)
		DSS_REG_W(io, HDMI_VBI_PKT_CTRL,
			DSS_REG_R(io, HDMI_VBI_PKT_CTRL) | (BIT(4) & BIT(5)));

	DEV_DBG("%s: AVMUTE %s\n", __func__, set ? "set" : "cleared");

	return 0;
} /* hdmi_tx_config_avmute */

static bool hdmi_tx_is_encryption_set(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_io_data *io;
	bool enc_en = true;
	u32 reg_val;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		goto end;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		goto end;
	}

	reg_val = DSS_REG_R_ND(io, HDMI_HDCP_CTRL2);
	if ((reg_val & BIT(0)) && (reg_val & BIT(1)))
		goto end;

	if (DSS_REG_R_ND(io, HDMI_CTRL) & BIT(2))
		goto end;

	return false;

end:
	return enc_en;
} /* hdmi_tx_is_encryption_set */

static void hdmi_tx_hdcp_cb(void *ptr, enum hdcp_states status)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = (struct hdmi_tx_ctrl *)ptr;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	hdmi_ctrl->hdcp_status = status;

	queue_delayed_work(hdmi_ctrl->workq, &hdmi_ctrl->hdcp_cb_work, HZ/4);
}

static inline bool hdmi_tx_is_stream_shareable(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	bool ret;

	switch (hdmi_ctrl->enc_lvl) {
	case HDCP_STATE_AUTH_ENC_NONE:
		ret = true;
		break;
	case HDCP_STATE_AUTH_ENC_1X:
		ret = hdmi_tx_is_hdcp_enabled(hdmi_ctrl) &&
			hdmi_ctrl->auth_state;
		break;
	case HDCP_STATE_AUTH_ENC_2P2:
		ret = hdmi_ctrl->hdcp_feature_on &&
			hdmi_ctrl->hdcp22_present &&
			hdmi_ctrl->auth_state;
		break;
	default:
		ret = false;
	}

	return ret;
}

static void hdmi_tx_hdcp_cb_work(struct work_struct *work)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct delayed_work *dw = to_delayed_work(work);
	int rc = 0;

	hdmi_ctrl = container_of(dw, struct hdmi_tx_ctrl, hdcp_cb_work);
	if (!hdmi_ctrl) {
		DEV_DBG("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);

	switch (hdmi_ctrl->hdcp_status) {
	case HDCP_STATE_AUTHENTICATED:
		hdmi_ctrl->auth_state = true;

		if (hdmi_tx_is_panel_on(hdmi_ctrl) &&
			hdmi_tx_is_stream_shareable(hdmi_ctrl)) {
			rc = hdmi_tx_config_avmute(hdmi_ctrl, false);
		}

		if (hdmi_ctrl->hdcp1_use_sw_keys && hdmi_ctrl->hdcp14_present) {
			if (!hdmi_ctrl->hdcp22_present)
				hdmi_tx_hdcp1_set_enc(hdmi_ctrl, true);
		}
		break;
	case HDCP_STATE_AUTH_FAIL:
		if (hdmi_ctrl->hdcp1_use_sw_keys && hdmi_ctrl->hdcp14_present) {
			if (hdmi_ctrl->auth_state && !hdmi_ctrl->hdcp22_present)
				hdmi_tx_hdcp1_set_enc(hdmi_ctrl, false);
		}

		hdmi_ctrl->auth_state = false;

			if (hdmi_tx_is_encryption_set(hdmi_ctrl) ||
				!hdmi_tx_is_stream_shareable(hdmi_ctrl)) {
				rc = hdmi_tx_config_avmute(hdmi_ctrl, true);
			}

		if (hdmi_tx_is_panel_on(hdmi_ctrl)) {
			pr_debug("%s: Reauthenticating\n", __func__);
			if (hdmi_ctrl->hdcp_ops && hdmi_ctrl->hdcp_data) {
				rc = hdmi_ctrl->hdcp_ops->reauthenticate(
				hdmi_ctrl->hdcp_data);
			if (rc)
				pr_err("%s: HDCP reauth failed. rc=%d\n",
					__func__, rc);
			} else
				pr_err("%s: NULL HDCP Ops and Data\n",
					__func__);
		} else {
			pr_debug("%s: Not reauthenticating. Cable not conn\n",
				__func__);
		}

		break;
	case HDCP_STATE_AUTH_FAIL_NOREAUTH:
		if (hdmi_ctrl->hdcp1_use_sw_keys && hdmi_ctrl->hdcp14_present) {
			if (hdmi_ctrl->auth_state && !hdmi_ctrl->hdcp22_present)
				hdmi_tx_hdcp1_set_enc(hdmi_ctrl, false);
		}

		hdmi_ctrl->auth_state = false;

		break;
	case HDCP_STATE_AUTH_ENC_NONE:
		hdmi_ctrl->enc_lvl = HDCP_STATE_AUTH_ENC_NONE;

		if (hdmi_tx_is_panel_on(hdmi_ctrl))
			rc = hdmi_tx_config_avmute(hdmi_ctrl, false);
		break;
	case HDCP_STATE_AUTH_ENC_1X:
	case HDCP_STATE_AUTH_ENC_2P2:
		hdmi_ctrl->enc_lvl = hdmi_ctrl->hdcp_status;

		if (hdmi_tx_is_panel_on(hdmi_ctrl) &&
			hdmi_tx_is_stream_shareable(hdmi_ctrl)) {
			rc = hdmi_tx_config_avmute(hdmi_ctrl, false);
		} else {
			rc = hdmi_tx_config_avmute(hdmi_ctrl, true);
		}
		break;
	default:
		break;
		/* do nothing */
	}

	mutex_unlock(&hdmi_ctrl->tx_lock);
}

static u32 hdmi_tx_ddc_read(struct hdmi_tx_ddc_ctrl *ddc_ctrl,
	u32 block, u8 *edid_buf)
{
	u32 block_size = EDID_BLOCK_SIZE;
	struct hdmi_tx_ddc_data ddc_data;
	u32 status = 0, retry_cnt = 0, i;

	if (!ddc_ctrl) {
		DEV_ERR("%s: invalid ddc ctrl data\n", __func__);
		return -EINVAL;
	}

	if (!edid_buf) {
		DEV_ERR("%s: invalid edid buffer\n", __func__);
		return -EINVAL;
	}

	do {
		DEV_DBG("EDID: reading block(%d) with block-size=%d\n",
				block, block_size);

		for (i = 0; i < EDID_BLOCK_SIZE; i += block_size) {
			memset(&ddc_data, 0, sizeof(ddc_data));

			ddc_data.dev_addr    = EDID_BLOCK_ADDR;
			ddc_data.offset      = block * EDID_BLOCK_SIZE + i;
			ddc_data.data_buf    = edid_buf + i;
			ddc_data.data_len    = block_size;
			ddc_data.request_len = block_size;
			ddc_data.retry       = 1;
			ddc_data.what        = "EDID";
			ddc_data.retry_align = true;

			ddc_ctrl->ddc_data = ddc_data;

			/* Read EDID twice with 32bit alighnment too */
			if (block < 2)
				status = hdmi_ddc_read(ddc_ctrl);
			else
				status = hdmi_ddc_read_seg(ddc_ctrl);

			if (status)
				break;
		}
		if (retry_cnt++ >= MAX_EDID_READ_RETRY)
			block_size /= 2;

	} while (status && (block_size >= 16));

	return status;
}

static int hdmi_tx_read_edid_retry(struct hdmi_tx_ctrl *hdmi_ctrl, u8 block)
{
	u32 checksum_retry = 0;
	u8 *ebuf;
	int ret = 0;
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ebuf = hdmi_ctrl->edid_buf;
	if (!ebuf) {
		DEV_ERR("%s: invalid edid buf\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ddc_ctrl = &hdmi_ctrl->ddc_ctrl;

	while (checksum_retry++ < MAX_EDID_READ_RETRY) {
		ret = hdmi_tx_ddc_read(ddc_ctrl, block,
			ebuf + (block * EDID_BLOCK_SIZE));
		if (ret)
			continue;
		else
			break;
	}
end:
	return ret;
}

static int hdmi_tx_read_edid(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int ndx, check_sum;
	int cea_blks = 0, block = 0, total_blocks = 0;
	int ret = 0;
	u8 *ebuf;
	struct hdmi_tx_ddc_ctrl *ddc_ctrl;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ebuf = hdmi_ctrl->edid_buf;
	if (!ebuf) {
		DEV_ERR("%s: invalid edid buf\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	memset(ebuf, 0, hdmi_ctrl->edid_buf_size);

	ddc_ctrl = &hdmi_ctrl->ddc_ctrl;

	do {
		if (block * EDID_BLOCK_SIZE > hdmi_ctrl->edid_buf_size) {
			DEV_ERR("%s: no mem for block %d, max mem %d\n",
				__func__, block, hdmi_ctrl->edid_buf_size);
			ret = -ENOMEM;
			goto end;
		}

		ret = hdmi_tx_read_edid_retry(hdmi_ctrl, block);
		if (ret) {
			DEV_ERR("%s: edid read failed\n", __func__);
			goto end;
		}

		/* verify checksum to validate edid block */
		check_sum = 0;
		for (ndx = 0; ndx < EDID_BLOCK_SIZE; ++ndx)
			check_sum += ebuf[ndx];

		if (check_sum & 0xFF) {
			DEV_ERR("%s: checksum mismatch\n", __func__);
			ret = -EINVAL;
			goto end;
		}

		/* get number of cea extension blocks as given in block 0*/
		if (block == 0) {
			cea_blks = ebuf[EDID_BLOCK_SIZE - 2];
			if (cea_blks < 0 || cea_blks >= MAX_EDID_BLOCKS) {
				cea_blks = 0;
				DEV_ERR("%s: invalid cea blocks %d\n",
					__func__, cea_blks);
				ret = -EINVAL;
				goto end;
			}

			total_blocks = cea_blks + 1;
		}
	} while ((cea_blks-- > 0) && (block++ < MAX_EDID_BLOCKS));
end:

	return ret;
}

/* Enable HDMI features */
static int hdmi_tx_init_panel(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct hdmi_panel_init_data panel_init_data = {0};
	void *panel_data;
	int rc = 0;

	hdmi_ctrl->panel.pinfo = &hdmi_ctrl->panel_data.panel_info;

	panel_init_data.io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	panel_init_data.ds_data = &hdmi_ctrl->ds_data;
	panel_init_data.ops = &hdmi_ctrl->panel_ops;
	panel_init_data.panel_data = &hdmi_ctrl->panel;
	panel_init_data.spd_vendor_name = hdmi_ctrl->spd_vendor_name;
	panel_init_data.spd_product_description =
		hdmi_ctrl->spd_product_description;
	panel_init_data.version = hdmi_ctrl->hdmi_tx_major_version;
	panel_init_data.ddc = &hdmi_ctrl->ddc_ctrl;
	panel_init_data.timing = &hdmi_ctrl->timing;

	panel_data = hdmi_panel_init(&panel_init_data);
	if (IS_ERR_OR_NULL(panel_data)) {
		DEV_ERR("%s: panel init failed\n", __func__);
		rc = -EINVAL;
	} else {
		hdmi_tx_set_fd(HDMI_TX_FEAT_PANEL, panel_data);
		DEV_DBG("%s: panel initialized\n", __func__);
	}

	return rc;
}

static int hdmi_tx_init_edid(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct hdmi_edid_init_data edid_init_data = {0};
	void *edid_data;
	int rc = 0;

	edid_init_data.kobj = hdmi_ctrl->kobj;
	edid_init_data.ds_data = hdmi_ctrl->ds_data;
	edid_init_data.max_pclk_khz = hdmi_ctrl->max_pclk_khz;
	edid_init_data.yc420_support = true;

	edid_data = hdmi_edid_init(&edid_init_data);
	if (!edid_data) {
		DEV_ERR("%s: edid init failed\n", __func__);
		rc = -ENODEV;
		goto end;
	}

	hdmi_ctrl->panel_data.panel_info.edid_data = edid_data;
	hdmi_tx_set_fd(HDMI_TX_FEAT_EDID, edid_data);

	/* get edid buffer from edid parser */
	hdmi_ctrl->edid_buf = edid_init_data.buf;
	hdmi_ctrl->edid_buf_size = edid_init_data.buf_size;

	hdmi_edid_set_video_resolution(edid_data, hdmi_ctrl->vic, true);
end:
	return rc;
}

static int hdmi_tx_init_hdcp(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct hdcp_init_data hdcp_init_data = {0};
	struct resource *res;
	void *hdcp_data;
	int rc = 0;

	res = platform_get_resource_byname(hdmi_ctrl->pdev,
		IORESOURCE_MEM, hdmi_tx_io_name(HDMI_TX_CORE_IO));
	if (!res) {
		DEV_ERR("%s: Error getting HDMI tx core resource\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	hdcp_init_data.phy_addr      = res->start;
	hdcp_init_data.core_io       = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	hdcp_init_data.qfprom_io     = &hdmi_ctrl->pdata.io[HDMI_TX_QFPROM_IO];
	hdcp_init_data.hdcp_io       = &hdmi_ctrl->pdata.io[HDMI_TX_HDCP_IO];
	hdcp_init_data.mutex         = &hdmi_ctrl->mutex;
	hdcp_init_data.sysfs_kobj    = hdmi_ctrl->kobj;
	hdcp_init_data.ddc_ctrl      = &hdmi_ctrl->ddc_ctrl;
	hdcp_init_data.workq         = hdmi_ctrl->workq;
	hdcp_init_data.notify_status = hdmi_tx_hdcp_cb;
	hdcp_init_data.cb_data       = (void *)hdmi_ctrl;
	hdcp_init_data.hdmi_tx_ver   = hdmi_ctrl->hdmi_tx_major_version;
	hdcp_init_data.sec_access    = true;
	hdcp_init_data.timing        = &hdmi_ctrl->timing;
	hdcp_init_data.client_id     = HDCP_CLIENT_HDMI;

	if (hdmi_ctrl->hdcp14_present) {
		hdcp_data = hdcp_1x_init(&hdcp_init_data);

		if (IS_ERR_OR_NULL(hdcp_data)) {
			DEV_ERR("%s: hdcp 1.4 init failed\n", __func__);
			rc = -EINVAL;
			goto end;
		} else {
			hdmi_tx_set_fd(HDMI_TX_FEAT_HDCP, hdcp_data);
			hdmi_ctrl->panel_data.panel_info.hdcp_1x_data =
				hdcp_data;
			DEV_DBG("%s: HDCP 1.4 initialized\n", __func__);
		}
	}

	hdcp_data = hdmi_hdcp2p2_init(&hdcp_init_data);

	if (IS_ERR_OR_NULL(hdcp_data)) {
		DEV_ERR("%s: hdcp 2.2 init failed\n", __func__);
		rc = -EINVAL;
		goto end;
	} else {
		hdmi_tx_set_fd(HDMI_TX_FEAT_HDCP2P2, hdcp_data);
		DEV_DBG("%s: HDCP 2.2 initialized\n", __func__);
	}
end:
	return rc;
}

static int hdmi_tx_init_cec_hw(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct hdmi_cec_init_data cec_init_data = {0};
	void *cec_hw_data;
	int rc = 0;

	cec_init_data.io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	cec_init_data.workq = hdmi_ctrl->workq;
	cec_init_data.pinfo = &hdmi_ctrl->panel_data.panel_info;
	cec_init_data.ops = &hdmi_ctrl->hdmi_cec_ops;
	cec_init_data.cbs = &hdmi_ctrl->hdmi_cec_cbs;

	cec_hw_data = hdmi_cec_init(&cec_init_data);
	if (IS_ERR_OR_NULL(cec_hw_data)) {
		DEV_ERR("%s: cec init failed\n", __func__);
		rc = -EINVAL;
	} else {
		hdmi_ctrl->panel_data.panel_info.is_cec_supported = true;
		hdmi_tx_set_fd(HDMI_TX_FEAT_CEC_HW, cec_hw_data);
		DEV_DBG("%s: cec hw initialized\n", __func__);
	}

	return rc;
}

static int hdmi_tx_init_cec_abst(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct cec_abstract_init_data cec_abst_init_data = {0};
	void *cec_abst_data;
	int rc = 0;

	cec_abst_init_data.kobj  = hdmi_ctrl->kobj;
	cec_abst_init_data.ops   = &hdmi_ctrl->hdmi_cec_ops;
	cec_abst_init_data.cbs   = &hdmi_ctrl->hdmi_cec_cbs;

	cec_abst_data = cec_abstract_init(&cec_abst_init_data);
	if (IS_ERR_OR_NULL(cec_abst_data)) {
		DEV_ERR("%s: cec abst init failed\n", __func__);
		rc = -EINVAL;
	} else {
		hdmi_tx_set_fd(HDMI_TX_FEAT_CEC_ABST, cec_abst_data);
		hdmi_ctrl->panel_data.panel_info.cec_data = cec_abst_data;
		DEV_DBG("%s: cec abst initialized\n", __func__);
	}

	return rc;
}

static int hdmi_tx_init_audio(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct hdmi_audio_init_data audio_init_data = {0};
	void *audio_data;
	int rc = 0;

	audio_init_data.io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	audio_init_data.ops = &hdmi_ctrl->audio_ops;

	audio_data = hdmi_audio_register(&audio_init_data);
	if (!audio_data) {
		rc = -EINVAL;
		DEV_ERR("%s: audio init failed\n", __func__);
	} else {
		hdmi_ctrl->audio_data = audio_data;
		DEV_DBG("%s: audio initialized\n", __func__);
	}

	return rc;
}

static int hdmi_tx_get_intf_id(struct platform_device *pdev)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = hdmi_tx_get_data(pdev);

	if (!hdmi_ctrl) {
		pr_err("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	return EXT_DISPLAY_TYPE_HDMI;
}

static void hdmi_tx_audio_teardown_done(struct platform_device *pdev)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = hdmi_tx_get_data(pdev);

	if (!hdmi_ctrl) {
		pr_err("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);
	hdmi_tx_audio_off(hdmi_ctrl);
	mutex_unlock(&hdmi_ctrl->tx_lock);

	pr_debug("audio engine disabled\n");
}

static int hdmi_tx_audio_ack_done(struct platform_device *pdev, u32 ack)
{
	int rc = 0;
	struct hdmi_tx_ctrl *hdmi_ctrl = hdmi_tx_get_data(pdev);

	if (!hdmi_ctrl) {
		pr_err("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	pr_debug("acknowledging audio (%d)\n", ack);

	return rc;
}

static int hdmi_tx_audio_codec_ready(struct platform_device *pdev)
{
	int rc = 0;
	struct hdmi_tx_ctrl *hdmi_ctrl = hdmi_tx_get_data(pdev);

	if (!hdmi_ctrl) {
		pr_err("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	return rc;
}

static int hdmi_tx_init_ext_disp(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int ret = 0;
	struct msm_ext_disp_init_data *ext;
	struct msm_ext_disp_audio_codec_ops *ops;
	struct device_node *pd_np;
	const char *phandle = "qcom,msm_ext_disp";

	if (!hdmi_ctrl) {
		pr_err("%s: invalid hdmi ctrl data\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	ext = &hdmi_ctrl->ext_audio_data;
	ops = &ext->codec_ops;

	ext->codec.type = EXT_DISPLAY_TYPE_HDMI;
	ext->codec.ctrl_id = 0;
	ext->codec.stream_id = 0;
	ext->pdev = hdmi_ctrl->pdev;
	ext->intf_data = &hdmi_ctrl->audio_data;

	ops->audio_info_setup	= hdmi_tx_audio_info_setup;
	ops->get_audio_edid_blk	= hdmi_tx_get_audio_edid_blk;
	ops->cable_status	= hdmi_tx_get_cable_status;
	ops->get_intf_id        = hdmi_tx_get_intf_id;
	ops->teardown_done      = hdmi_tx_audio_teardown_done;
	ops->acknowledge        = hdmi_tx_audio_ack_done;
	ops->ready              = hdmi_tx_audio_codec_ready;

	if (!hdmi_ctrl->pdev->dev.of_node) {
		pr_err("%s cannot find hdmi_ctrl dev.of_node\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	pd_np = of_parse_phandle(hdmi_ctrl->pdev->dev.of_node, phandle, 0);
	if (!pd_np) {
		pr_err("%s cannot find %s dev\n", __func__, phandle);
		ret = -ENODEV;
		goto end;
	}

	hdmi_ctrl->ext_pdev = of_find_device_by_node(pd_np);
	if (!hdmi_ctrl->ext_pdev) {
		pr_err("%s cannot find %s pdev\n", __func__, phandle);
		ret = -ENODEV;
		goto end;
	}

	ret = msm_ext_disp_register_intf(hdmi_ctrl->ext_pdev, ext);
	if (ret)
		pr_err("%s: failed to register disp\n", __func__);

end:
	return ret;
}

static void hdmi_tx_deinit_features(struct hdmi_tx_ctrl *hdmi_ctrl,
		u32 features)
{
	void *fd;

	if (features & HDMI_TX_FEAT_CEC_ABST) {
		fd = hdmi_tx_get_fd(HDMI_TX_FEAT_CEC_ABST);

		cec_abstract_deinit(fd);

		hdmi_ctrl->panel_data.panel_info.cec_data = NULL;
		hdmi_tx_set_fd(HDMI_TX_FEAT_CEC_ABST, 0);
	}

	if (features & HDMI_TX_FEAT_CEC_HW) {
		fd = hdmi_tx_get_fd(HDMI_TX_FEAT_CEC_HW);

		hdmi_cec_deinit(fd);
		hdmi_ctrl->panel_data.panel_info.is_cec_supported = false;
		hdmi_tx_set_fd(HDMI_TX_FEAT_CEC_HW, 0);
	}

	if (features & HDMI_TX_FEAT_HDCP2P2) {
		fd = hdmi_tx_get_fd(HDMI_TX_FEAT_HDCP2P2);

		hdmi_hdcp2p2_deinit(fd);
		hdmi_tx_set_fd(HDMI_TX_FEAT_HDCP2P2, 0);
	}

	if (features & HDMI_TX_FEAT_HDCP) {
		fd = hdmi_tx_get_fd(HDMI_TX_FEAT_HDCP);

		hdcp_1x_deinit(fd);
		hdmi_tx_set_fd(HDMI_TX_FEAT_HDCP, 0);
	}

	if (features & HDMI_TX_FEAT_EDID) {
		fd = hdmi_tx_get_fd(HDMI_TX_FEAT_EDID);

		hdmi_edid_deinit(fd);
		hdmi_ctrl->edid_buf = NULL;
		hdmi_ctrl->edid_buf_size = 0;
		hdmi_tx_set_fd(HDMI_TX_FEAT_EDID, 0);
	}
} /* hdmi_tx_init_features */

static int hdmi_tx_init_features(struct hdmi_tx_ctrl *hdmi_ctrl,
	struct fb_info *fbi)
{
	int ret = 0;
	u32 deinit_features = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	if (!fbi) {
		DEV_ERR("%s: invalid fb info\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ret = hdmi_tx_init_panel(hdmi_ctrl);
	if (ret)
		goto end;

	ret = hdmi_tx_init_edid(hdmi_ctrl);
	if (ret) {
		deinit_features |= HDMI_TX_FEAT_PANEL;
		goto err;
	}

	ret = hdmi_tx_init_hdcp(hdmi_ctrl);
	if (ret) {
		deinit_features |= HDMI_TX_FEAT_EDID;
		goto err;
	}

	ret = hdmi_tx_init_cec_hw(hdmi_ctrl);
	if (ret) {
		deinit_features |= HDMI_TX_FEAT_HDCP;
		goto err;
	}

	ret = hdmi_tx_init_cec_abst(hdmi_ctrl);
	if (ret) {
		deinit_features |= HDMI_TX_FEAT_CEC_HW;
		goto err;
	}

	ret = hdmi_tx_init_audio(hdmi_ctrl);
	if (ret) {
		deinit_features |= HDMI_TX_FEAT_CEC_ABST;
		goto err;
	}

	ret = hdmi_tx_init_ext_disp(hdmi_ctrl);
	if (ret) {
		hdmi_audio_unregister(hdmi_ctrl->audio_data);
		goto err;
	}

	return 0;
err:
	hdmi_tx_deinit_features(hdmi_ctrl, deinit_features);
end:
	return ret;
}

static inline u32 hdmi_tx_is_controller_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_io_data *io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];

	return DSS_REG_R_ND(io, HDMI_CTRL) & BIT(0);
} /* hdmi_tx_is_controller_on */

static int hdmi_tx_init_panel_info(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct mdss_panel_info *pinfo;
	struct msm_hdmi_mode_timing_info timing = {0};
	u32 ret;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	ret = hdmi_get_supported_mode(&timing, &hdmi_ctrl->ds_data,
		hdmi_ctrl->vic);
	pinfo = &hdmi_ctrl->panel_data.panel_info;

	if (ret || !timing.supported || !pinfo) {
		DEV_ERR("%s: invalid timing data\n", __func__);
		return -EINVAL;
	}

	pinfo->xres = timing.active_h;
	pinfo->yres = timing.active_v;
	pinfo->clk_rate = timing.pixel_freq * 1000;

	pinfo->lcdc.h_back_porch = timing.back_porch_h;
	pinfo->lcdc.h_front_porch = timing.front_porch_h;
	pinfo->lcdc.h_pulse_width = timing.pulse_width_h;
	pinfo->lcdc.v_back_porch = timing.back_porch_v;
	pinfo->lcdc.v_front_porch = timing.front_porch_v;
	pinfo->lcdc.v_pulse_width = timing.pulse_width_v;
	pinfo->lcdc.frame_rate = timing.refresh_rate;
	pinfo->lcdc.h_polarity = timing.active_low_h;
	pinfo->lcdc.v_polarity = timing.active_low_v;

	pinfo->type = DTV_PANEL;
	pinfo->pdest = DISPLAY_3;
	pinfo->wait_cycle = 0;
	pinfo->out_format = MDP_RGB_888;
	pinfo->bpp = 24;
	pinfo->fb_num = 1;

	pinfo->min_fps = HDMI_TX_MIN_FPS;
	pinfo->max_fps = HDMI_TX_MAX_FPS;

	pinfo->lcdc.border_clr = 0; /* blk */
	pinfo->lcdc.underflow_clr = 0xff; /* blue */
	pinfo->lcdc.hsync_skew = 0;

	pinfo->is_pluggable = hdmi_ctrl->pdata.pluggable;

	hdmi_ctrl->timing = timing;

	return 0;
} /* hdmi_tx_init_panel_info */

static int hdmi_tx_read_sink_info(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int status = 0;
	void *data;
	struct dss_io_data *io;
	u32 sink_max_pclk;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	data = hdmi_tx_get_fd(HDMI_TX_FEAT_EDID);
	if (!data) {
		DEV_ERR("%s: invalid edid fd\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];

	if (!hdmi_tx_is_controller_on(hdmi_ctrl)) {
		DEV_ERR("%s: failed: HDMI controller is off", __func__);
		status = -ENXIO;
		goto error;
	}

	if (hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, true)) {
		DEV_ERR("%s: Failed to enable ddc power\n", __func__);
		status = -EINVAL;
		goto error;
	}

	/* Enable SW DDC before EDID read */
	DSS_REG_W_ND(io, HDMI_DDC_ARBITRATION,
		DSS_REG_R(io, HDMI_DDC_ARBITRATION) & ~(BIT(4)));

	/* Set/Reset HDMI max TMDS clock supported by source */
	hdmi_edid_set_max_pclk_rate(data, hdmi_ctrl->max_pclk_khz);

	if (!hdmi_ctrl->custom_edid && !hdmi_ctrl->sim_mode) {
		hdmi_ddc_config(&hdmi_ctrl->ddc_ctrl);

		status = hdmi_tx_read_edid(hdmi_ctrl);
		if (status) {
			DEV_ERR("%s: error reading edid\n", __func__);
			status = -EINVAL;
			goto bail;
		}
	}

	/* parse edid if a valid edid buffer is present */
	if (hdmi_ctrl->custom_edid || !hdmi_ctrl->sim_mode) {
		status = hdmi_edid_parser(data);
		if (status) {
			DEV_ERR("%s: edid parse failed\n", __func__);
		} else {
			/*
			 * Update HDMI max supported TMDS clock, consider
			 * both sink and source capacity. For DVI sink,
			 * could not get max TMDS clock from EDID, so just
			 * use source capacity.
			 */
			sink_max_pclk =
				hdmi_edid_get_sink_caps_max_tmds_clk(data);
			if (sink_max_pclk != 0)
				hdmi_edid_set_max_pclk_rate(data,
				  min(sink_max_pclk / 1000,
				      hdmi_ctrl->max_pclk_khz));
		}
	}
bail:
	if (hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, false))
		DEV_ERR("%s: Failed to disable ddc power\n", __func__);
error:
	return status;
} /* hdmi_tx_read_sink_info */

static void hdmi_tx_update_hdcp_info(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	void *fd = NULL;
	struct hdcp_ops *ops = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	/* check first if hdcp2p2 is supported */
	fd = hdmi_tx_get_fd(HDMI_TX_FEAT_HDCP2P2);
	if (fd)
		ops = hdmi_hdcp2p2_start(fd);

	if (ops && ops->feature_supported)
		hdmi_ctrl->hdcp22_present = ops->feature_supported(fd);
	else
		hdmi_ctrl->hdcp22_present = false;

	if (!hdmi_ctrl->hdcp22_present) {
		fd = hdmi_tx_get_fd(HDMI_TX_FEAT_HDCP);
		if (fd)
			ops = hdcp_1x_start(fd);
	}

	/* update internal data about hdcp */
	hdmi_ctrl->hdcp_data = fd;
	hdmi_ctrl->hdcp_ops = ops;
}

static void hdmi_tx_update_deep_color(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct mdss_panel_info *pinfo;
	u8 deep_color = hdmi_edid_get_deep_color(
		hdmi_tx_get_fd(HDMI_TX_FEAT_EDID));

	pinfo = &hdmi_ctrl->panel_data.panel_info;

	pinfo->deep_color = 0;
	hdmi_ctrl->dc_support = false;
	pinfo->bpp = 24;

	if (deep_color & BIT(0))
		pinfo->deep_color |= MDP_DEEP_COLOR_YUV444;

	if (deep_color & BIT(1)) {
		pinfo->deep_color |= MDP_DEEP_COLOR_RGB30B;
		hdmi_ctrl->dc_support = true;
		pinfo->bpp = 30;
	}

	if (deep_color & BIT(2)) {
		pinfo->deep_color |= MDP_DEEP_COLOR_RGB36B;
		hdmi_ctrl->dc_support = true;
		pinfo->bpp = 36;
	}

	if (deep_color & BIT(3)) {
		pinfo->deep_color |= MDP_DEEP_COLOR_RGB48B;
		hdmi_ctrl->dc_support = true;
		pinfo->bpp = 48;
	}
}

static void hdmi_tx_update_hdr_info(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct mdss_panel_info *pinfo = &hdmi_ctrl->panel_data.panel_info;
	struct mdss_panel_hdr_properties *hdr_prop = &pinfo->hdr_properties;
	struct hdmi_edid_hdr_data *hdr_data = NULL;

	/* CEA-861.3 4.2 */
	hdr_prop->hdr_enabled = hdmi_tx_is_hdr_supported(hdmi_ctrl);
	/* no display primaries in EDID, so skip it */
	memset(hdr_prop->display_primaries, 0,
		sizeof(hdr_prop->display_primaries));

	hdmi_edid_get_hdr_data(hdmi_tx_get_fd(HDMI_TX_FEAT_EDID), &hdr_data);

	if (hdr_prop->hdr_enabled) {
		hdr_prop->peak_brightness = hdr_data->max_luminance * 10000;
		if (hdr_data->avg_luminance != 0)
			hdr_prop->avg_brightness = 50 *
				(BIT(0) << (int)(hdr_data->avg_luminance / 32));
		hdr_prop->blackness_level = (hdr_data->min_luminance *
					hdr_data->min_luminance *
					hdr_data->max_luminance * 100) / 65025;
	}
}

static void hdmi_tx_hpd_int_work(struct work_struct *work)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	int rc = -EINVAL;
	int retry = MAX_EDID_READ_RETRY;

	hdmi_ctrl = container_of(work, struct hdmi_tx_ctrl, hpd_int_work);
	if (!hdmi_ctrl) {
		DEV_DBG("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);

	if (!hdmi_ctrl->hpd_initialized) {
		DEV_DBG("hpd not initialized\n");
		mutex_unlock(&hdmi_ctrl->tx_lock);
		return;
	}

	DEV_DBG("%s: %s\n", __func__,
		hdmi_ctrl->hpd_state ? "CONNECT" : "DISCONNECT");

	if (hdmi_ctrl->hpd_state) {
		while (rc && retry--)
			rc = hdmi_tx_read_sink_info(hdmi_ctrl);
		if (!retry && rc)
			pr_warn_ratelimited("%s: EDID read failed\n", __func__);
		hdmi_tx_update_deep_color(hdmi_ctrl);
		hdmi_tx_update_hdr_info(hdmi_ctrl);
	}

	mutex_unlock(&hdmi_ctrl->tx_lock);

	if (hdmi_ctrl->hpd_state) {
		hdmi_tx_send_cable_notification(hdmi_ctrl, true);
	} else {
		hdmi_tx_audio_notify(hdmi_ctrl, 0);
		hdmi_tx_send_cable_notification(hdmi_ctrl, false);
	}
} /* hdmi_tx_hpd_int_work */

static int hdmi_tx_check_capability(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 hdmi_disabled, hdcp_disabled, reg_val;
	struct dss_io_data *io = NULL;
	int ret = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_QFPROM_IO];
	if (!io->base) {
		DEV_ERR("%s: QFPROM io is not initialized\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	/* check if hdmi and hdcp are disabled */
	if (hdmi_ctrl->hdmi_tx_major_version < HDMI_TX_VERSION_4) {
		hdcp_disabled = DSS_REG_R_ND(io,
			QFPROM_RAW_FEAT_CONFIG_ROW0_LSB) & BIT(31);

		hdmi_disabled = DSS_REG_R_ND(io,
			QFPROM_RAW_FEAT_CONFIG_ROW0_MSB) & BIT(0);
	} else {
		reg_val = DSS_REG_R_ND(io,
			QFPROM_RAW_FEAT_CONFIG_ROW0_LSB + QFPROM_RAW_VERSION_4);
		hdcp_disabled = reg_val & BIT(12);
		hdmi_disabled = reg_val & BIT(13);

		reg_val = DSS_REG_R_ND(io, SEC_CTRL_HW_VERSION);
		/*
		 * With HDCP enabled on capable hardware, check if HW
		 * or SW keys should be used.
		 */
		if (!hdcp_disabled && (reg_val >= HDCP_SEL_MIN_SEC_VERSION)) {
			reg_val = DSS_REG_R_ND(io,
				QFPROM_RAW_FEAT_CONFIG_ROW0_MSB +
				QFPROM_RAW_VERSION_4);
			if (!(reg_val & BIT(23)))
				hdmi_ctrl->hdcp1_use_sw_keys = true;
		}
	}

	hdmi_ctrl->dc_feature_on = false;

	DEV_DBG("%s: Features <HDMI:%s, HDCP:%s, Deep Color:%s>\n", __func__,
		hdmi_disabled ? "OFF" : "ON", hdcp_disabled ? "OFF" : "ON",
		!hdmi_ctrl->dc_feature_on ? "OFF" : "ON");

	if (hdmi_disabled) {
		DEV_ERR("%s: HDMI disabled\n", __func__);
		ret = -ENODEV;
		goto end;
	}

	hdmi_ctrl->hdcp14_present = !hdcp_disabled;
end:
	return ret;
} /* hdmi_tx_check_capability */

static void hdmi_tx_set_mode(struct hdmi_tx_ctrl *hdmi_ctrl, u32 power_on)
{
	struct dss_io_data *io = NULL;
	/* Defaults: Disable block, HDMI mode */
	u32 hdmi_ctrl_reg = BIT(1);
	void *data = hdmi_tx_get_fd(HDMI_TX_FEAT_EDID);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		return;
	}

	if (power_on) {
		/* Enable the block */
		hdmi_ctrl_reg |= BIT(0);

		/**
		 * HDMI Encryption, if HDCP is enabled
		 * The ENC_REQUIRED bit is only available on HDMI Tx major
		 * version less than 4. From 4 onwards, this bit is controlled
		 * by TZ
		 */
		if (hdmi_ctrl->hdmi_tx_major_version < 4 &&
			hdmi_tx_is_hdcp_enabled(hdmi_ctrl) &&
			!hdmi_ctrl->pdata.primary)
			hdmi_ctrl_reg |= BIT(2);

		/* Set transmission mode to DVI based in EDID info */
		if (hdmi_edid_is_dvi_mode(data))
			hdmi_ctrl_reg &= ~BIT(1); /* DVI mode */

		/*
		 * Use DATAPATH_MODE as 1 always, the new mode that also
		 * supports scrambler and HDCP 2.2. The legacy mode should no
		 * longer be used
		 */
		hdmi_ctrl_reg |= BIT(31);
	}

	DSS_REG_W(io, HDMI_CTRL, hdmi_ctrl_reg);

	DEV_DBG("HDMI Core: %s, HDMI_CTRL=0x%08x\n",
		power_on ? "Enable" : "Disable", hdmi_ctrl_reg);
} /* hdmi_tx_set_mode */

static int hdmi_tx_pinctrl_set_state(struct hdmi_tx_ctrl *hdmi_ctrl,
			enum hdmi_tx_power_module_type module, bool active)
{
	struct pinctrl_state *pin_state = NULL;
	int rc = -EFAULT;
	u64 cur_pin_states;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.pinctrl))
		return 0;

	cur_pin_states = active ? (hdmi_ctrl->pdata.pin_states | BIT(module))
				: (hdmi_ctrl->pdata.pin_states & ~BIT(module));

	if (cur_pin_states & BIT(HDMI_TX_HPD_PM)) {
		if (cur_pin_states & BIT(HDMI_TX_DDC_PM)) {
			if (cur_pin_states & BIT(HDMI_TX_CEC_PM))
				pin_state = hdmi_ctrl->pin_res.state_active;
			else
				pin_state =
					hdmi_ctrl->pin_res.state_ddc_active;
		} else if (cur_pin_states & BIT(HDMI_TX_CEC_PM)) {
			pin_state = hdmi_ctrl->pin_res.state_cec_active;
		} else {
			pin_state = hdmi_ctrl->pin_res.state_hpd_active;
		}
	} else {
		pin_state = hdmi_ctrl->pin_res.state_suspend;
	}

	if (!IS_ERR_OR_NULL(pin_state)) {
		rc = pinctrl_select_state(hdmi_ctrl->pin_res.pinctrl,
				pin_state);
		if (rc)
			pr_err("%s: cannot set pins\n", __func__);
		else
			hdmi_ctrl->pdata.pin_states = cur_pin_states;
	} else {
		pr_err("%s: pinstate not found\n", __func__);
	}

	return rc;
}

static int hdmi_tx_pinctrl_init(struct platform_device *pdev)
{
	struct hdmi_tx_ctrl *hdmi_ctrl;

	hdmi_ctrl = platform_get_drvdata(pdev);
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	hdmi_ctrl->pin_res.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.pinctrl)) {
		pr_err("%s: failed to get pinctrl\n", __func__);
		return PTR_ERR(hdmi_ctrl->pin_res.pinctrl);
	}

	hdmi_ctrl->pin_res.state_active =
		pinctrl_lookup_state(hdmi_ctrl->pin_res.pinctrl, "hdmi_active");
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.state_active))
		pr_debug("%s: cannot get active pinstate\n", __func__);

	hdmi_ctrl->pin_res.state_hpd_active =
		pinctrl_lookup_state(hdmi_ctrl->pin_res.pinctrl,
							"hdmi_hpd_active");
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.state_hpd_active))
		pr_debug("%s: cannot get hpd active pinstate\n", __func__);

	hdmi_ctrl->pin_res.state_cec_active =
		pinctrl_lookup_state(hdmi_ctrl->pin_res.pinctrl,
							"hdmi_cec_active");
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.state_cec_active))
		pr_debug("%s: cannot get cec active pinstate\n", __func__);

	hdmi_ctrl->pin_res.state_ddc_active =
		pinctrl_lookup_state(hdmi_ctrl->pin_res.pinctrl,
							"hdmi_ddc_active");
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.state_ddc_active))
		pr_debug("%s: cannot get ddc active pinstate\n", __func__);

	hdmi_ctrl->pin_res.state_suspend =
		pinctrl_lookup_state(hdmi_ctrl->pin_res.pinctrl, "hdmi_sleep");
	if (IS_ERR_OR_NULL(hdmi_ctrl->pin_res.state_suspend))
		pr_debug("%s: cannot get sleep pinstate\n", __func__);

	return 0;
}

static int hdmi_tx_config_power(struct hdmi_tx_ctrl *hdmi_ctrl,
	enum hdmi_tx_power_module_type module, int config)
{
	int rc = 0;
	struct dss_module_power *power_data = NULL;
	char name[MAX_CLIENT_NAME_LEN];

	if (!hdmi_ctrl) {
		DEV_ERR("%s: Error: invalid hdmi ctrl data\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	if (module >= HDMI_TX_MAX_PM) {
		DEV_ERR("%s: Error: invalid power module type\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	power_data = &hdmi_ctrl->pdata.power_data[module];
	if (!power_data) {
		DEV_ERR("%s: Error: invalid power data\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	if (config) {
		rc = msm_dss_config_vreg(&hdmi_ctrl->pdev->dev,
			power_data->vreg_config, power_data->num_vreg, 1);
		if (rc) {
			DEV_ERR("%s: Failed to config %s vreg. Err=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto exit;
		}

		snprintf(name, MAX_CLIENT_NAME_LEN, "hdmi:%u", module);
		hdmi_ctrl->pdata.reg_bus_clt[module] =
			mdss_reg_bus_vote_client_create(name);
		if (IS_ERR(hdmi_ctrl->pdata.reg_bus_clt[module])) {
			pr_err("reg bus client create failed\n");
			msm_dss_config_vreg(&hdmi_ctrl->pdev->dev,
			power_data->vreg_config, power_data->num_vreg, 0);
			rc = PTR_ERR(hdmi_ctrl->pdata.reg_bus_clt[module]);
			goto exit;
		}

		rc = msm_dss_get_clk(&hdmi_ctrl->pdev->dev,
			power_data->clk_config, power_data->num_clk);
		if (rc) {
			DEV_ERR("%s: Failed to get %s clk. Err=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);

			mdss_reg_bus_vote_client_destroy(
				hdmi_ctrl->pdata.reg_bus_clt[module]);
			hdmi_ctrl->pdata.reg_bus_clt[module] = NULL;
			msm_dss_config_vreg(&hdmi_ctrl->pdev->dev,
			power_data->vreg_config, power_data->num_vreg, 0);
		}
	} else {
		msm_dss_put_clk(power_data->clk_config, power_data->num_clk);
		mdss_reg_bus_vote_client_destroy(
			hdmi_ctrl->pdata.reg_bus_clt[module]);
		hdmi_ctrl->pdata.reg_bus_clt[module] = NULL;

		rc = msm_dss_config_vreg(&hdmi_ctrl->pdev->dev,
			power_data->vreg_config, power_data->num_vreg, 0);
		if (rc)
			DEV_ERR("%s: Fail to deconfig %s vreg. Err=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
	}

exit:
	return rc;
} /* hdmi_tx_config_power */

static int hdmi_tx_enable_power(struct hdmi_tx_ctrl *hdmi_ctrl,
	enum hdmi_tx_power_module_type module, int enable)
{
	int rc = 0;
	struct dss_module_power *power_data = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: Error: invalid hdmi ctrl data\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	if (module >= HDMI_TX_MAX_PM) {
		DEV_ERR("%s: Error: invalid power module type\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	power_data = &hdmi_ctrl->pdata.power_data[module];
	if (!power_data) {
		DEV_ERR("%s: Error: invalid power data\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	if (hdmi_ctrl->panel_data.panel_info.cont_splash_enabled) {
		DEV_DBG("%s: %s enabled by splash.\n",
				__func__, hdmi_pm_name(module));
		return 0;
	}

	if (enable && !hdmi_ctrl->power_data_enable[module]) {
		rc = msm_dss_enable_vreg(power_data->vreg_config,
			power_data->num_vreg, 1);
		if (rc) {
			DEV_ERR("%s: Failed to enable %s vreg. Error=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto error;
		}

		rc = hdmi_tx_pinctrl_set_state(hdmi_ctrl, module, enable);
		if (rc) {
			DEV_ERR("%s: Failed to set %s pinctrl state\n",
				__func__, hdmi_tx_pm_name(module));
			goto error;
		}

		rc = msm_dss_enable_gpio(power_data->gpio_config,
			power_data->num_gpio, 1);
		if (rc) {
			DEV_ERR("%s: Failed to enable %s gpio. Error=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto disable_vreg;
		}
		mdss_update_reg_bus_vote(hdmi_ctrl->pdata.reg_bus_clt[module],
			VOTE_INDEX_LOW);

		rc = msm_dss_clk_set_rate(power_data->clk_config,
			power_data->num_clk);
		if (rc) {
			DEV_ERR("%s: failed to set clks rate for %s. err=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto disable_gpio;
		}

		rc = msm_dss_enable_clk(power_data->clk_config,
			power_data->num_clk, 1);
		if (rc) {
			DEV_ERR("%s: Failed to enable clks for %s. Error=%d\n",
				__func__, hdmi_tx_pm_name(module), rc);
			goto disable_gpio;
		}
		hdmi_ctrl->power_data_enable[module] = true;
	} else if (!enable && hdmi_ctrl->power_data_enable[module] &&
		(!hdmi_tx_is_cec_wakeup_en(hdmi_ctrl) ||
		((module != HDMI_TX_HPD_PM) && (module != HDMI_TX_CEC_PM)))) {
		msm_dss_enable_clk(power_data->clk_config,
			power_data->num_clk, 0);
		mdss_update_reg_bus_vote(hdmi_ctrl->pdata.reg_bus_clt[module],
			VOTE_INDEX_DISABLE);
		msm_dss_enable_gpio(power_data->gpio_config,
			power_data->num_gpio, 0);
		hdmi_tx_pinctrl_set_state(hdmi_ctrl, module, 0);
		msm_dss_enable_vreg(power_data->vreg_config,
			power_data->num_vreg, 0);
		hdmi_ctrl->power_data_enable[module] = false;
	}

	return rc;

disable_gpio:
	mdss_update_reg_bus_vote(hdmi_ctrl->pdata.reg_bus_clt[module],
		VOTE_INDEX_DISABLE);
	msm_dss_enable_gpio(power_data->gpio_config, power_data->num_gpio, 0);
disable_vreg:
	msm_dss_enable_vreg(power_data->vreg_config, power_data->num_vreg, 0);
error:
	return rc;
} /* hdmi_tx_enable_power */

static void hdmi_tx_core_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_CEC_PM, 0);
	hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_CORE_PM, 0);
} /* hdmi_tx_core_off */

static int hdmi_tx_core_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	if (hdmi_ctrl->pdata.ext_hdmi_pixel_clk
			&& hdmi_ctrl->pdata.hdmi_pclk_rcg) {
		rc = clk_set_parent(hdmi_ctrl->pdata.hdmi_pclk_rcg,
				hdmi_ctrl->pdata.ext_hdmi_pixel_clk);
		if (rc) {
			pr_err("%s: set_parent for hdmi pclk failed. rc=%d\n",
					__func__, rc);
			return -EINVAL;
		}
	}

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_CORE_PM, 1);
	if (rc) {
		DEV_ERR("%s: core hdmi_msm_enable_power failed rc = %d\n",
			__func__, rc);
		return rc;
	}
	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_CEC_PM, 1);
	if (rc) {
		DEV_ERR("%s: cec hdmi_msm_enable_power failed rc = %d\n",
			__func__, rc);
		goto disable_core_power;
	}

	return rc;
disable_core_power:
	hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_CORE_PM, 0);
	return rc;
} /* hdmi_tx_core_on */

static void hdmi_tx_phy_reset(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	unsigned int phy_reset_polarity = 0x0;
	unsigned int pll_reset_polarity = 0x0;
	unsigned int val;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return;
	}

	val = DSS_REG_R_ND(io, HDMI_PHY_CTRL);

	phy_reset_polarity = val >> 3 & 0x1;
	pll_reset_polarity = val >> 1 & 0x1;

	if (phy_reset_polarity == 0)
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val | SW_RESET);
	else
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val & (~SW_RESET));

	if (pll_reset_polarity == 0)
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val | SW_RESET_PLL);
	else
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val & (~SW_RESET_PLL));

	if (phy_reset_polarity == 0)
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val & (~SW_RESET));
	else
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val | SW_RESET);

	if (pll_reset_polarity == 0)
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val & (~SW_RESET_PLL));
	else
		DSS_REG_W_ND(io, HDMI_PHY_CTRL, val | SW_RESET_PLL);
} /* hdmi_tx_phy_reset */

static int hdmi_tx_audio_info_setup(struct platform_device *pdev,
	struct msm_ext_disp_audio_setup_params *params)
{
	int rc = 0;
	struct hdmi_tx_ctrl *hdmi_ctrl = hdmi_tx_get_data(pdev);
	u32 is_mode_dvi;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	if (!params) {
		DEV_ERR("%s: invalid audio params\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);

	is_mode_dvi = hdmi_tx_is_dvi_mode(hdmi_ctrl);

	if (!is_mode_dvi && hdmi_tx_is_panel_on(hdmi_ctrl)) {
		memcpy(&hdmi_ctrl->audio_params, params,
			sizeof(struct msm_ext_disp_audio_setup_params));

		hdmi_tx_audio_setup(hdmi_ctrl);
	} else {
		rc = -EPERM;
	}

	mutex_unlock(&hdmi_ctrl->tx_lock);
	return rc;
}

static int hdmi_tx_get_audio_edid_blk(struct platform_device *pdev,
	struct msm_ext_disp_audio_edid_blk *blk)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = hdmi_tx_get_data(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	return hdmi_edid_get_audio_blk(
		hdmi_tx_get_fd(HDMI_TX_FEAT_EDID), blk);
} /* hdmi_tx_get_audio_edid_blk */

static u8 hdmi_tx_tmds_enabled(struct platform_device *pdev)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	/* status of tmds */
	return (hdmi_ctrl->timing_gen_on == true);
}

static int hdmi_tx_set_mhl_max_pclk(struct platform_device *pdev, u32 max_val)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}
	if (max_val) {
		hdmi_ctrl->ds_data.ds_max_clk = max_val;
		hdmi_ctrl->ds_data.ds_registered = true;
	} else {
		DEV_ERR("%s: invalid max pclk val\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int msm_hdmi_register_mhl(struct platform_device *pdev,
			  struct msm_hdmi_mhl_ops *ops, void *data)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	if (!ops) {
		DEV_ERR("%s: invalid ops\n", __func__);
		return -EINVAL;
	}

	ops->tmds_enabled = hdmi_tx_tmds_enabled;
	ops->set_mhl_max_pclk = hdmi_tx_set_mhl_max_pclk;
	ops->set_upstream_hpd = hdmi_tx_set_mhl_hpd;

	hdmi_ctrl->ds_registered = true;

	return 0;
}

static int hdmi_tx_get_cable_status(struct platform_device *pdev, u32 vote)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = hdmi_tx_get_data(pdev);
	unsigned long flags;
	u32 hpd;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -ENODEV;
	}

	spin_lock_irqsave(&hdmi_ctrl->hpd_state_lock, flags);
	hpd = hdmi_tx_is_panel_on(hdmi_ctrl);
	spin_unlock_irqrestore(&hdmi_ctrl->hpd_state_lock, flags);

	hdmi_ctrl->vote_hdmi_core_on = false;

	if (vote && hpd)
		hdmi_ctrl->vote_hdmi_core_on = true;

	return hpd;
}

static inline bool hdmi_tx_hw_is_cable_connected(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	return DSS_REG_R(&hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO],
			HDMI_HPD_INT_STATUS) & BIT(1) ? true : false;
}

static void hdmi_tx_hpd_polarity_setup(struct hdmi_tx_ctrl *hdmi_ctrl,
	bool polarity)
{
	struct dss_io_data *io = NULL;
	bool cable_sense;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}
	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io is not initialized\n", __func__);
		return;
	}

	if (hdmi_ctrl->sim_mode) {
		DEV_DBG("%s: sim mode enabled\n", __func__);
		return;
	}

	if (polarity)
		DSS_REG_W(io, HDMI_HPD_INT_CTRL, BIT(2) | BIT(1));
	else
		DSS_REG_W(io, HDMI_HPD_INT_CTRL, BIT(2));

	cable_sense = hdmi_tx_hw_is_cable_connected(hdmi_ctrl);
	DEV_DBG("%s: listen = %s, sense = %s\n", __func__,
		polarity ? "connect" : "disconnect",
		cable_sense ? "connect" : "disconnect");

	if (cable_sense == polarity) {
		u32 reg_val = DSS_REG_R(io, HDMI_HPD_CTRL);

		/* Toggle HPD circuit to trigger HPD sense */
		DSS_REG_W(io, HDMI_HPD_CTRL, reg_val & ~BIT(28));
		DSS_REG_W(io, HDMI_HPD_CTRL, reg_val | BIT(28));
	}
} /* hdmi_tx_hpd_polarity_setup */

static void hdmi_tx_audio_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	if (hdmi_ctrl->audio_ops.off)
		hdmi_ctrl->audio_ops.off(hdmi_ctrl->audio_data);

	memset(&hdmi_ctrl->audio_params, 0,
		sizeof(struct msm_ext_disp_audio_setup_params));
}

static int hdmi_tx_power_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	struct dss_io_data *io = NULL;
	void *pdata =  NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	pdata = hdmi_tx_get_fd(HDMI_TX_FEAT_PANEL);
	if (!pdata) {
		DEV_ERR("%s: invalid panel data\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: Core io is not initialized\n", __func__);
		goto end;
	}

	if (!hdmi_ctrl->panel_power_on) {
		DEV_DBG("%s: hdmi_ctrl is already off\n", __func__);
		goto end;
	}

	if (!hdmi_tx_is_dvi_mode(hdmi_ctrl))
		hdmi_tx_audio_off(hdmi_ctrl);

	if (hdmi_ctrl->panel_ops.off)
		hdmi_ctrl->panel_ops.off(pdata);

	hdmi_tx_set_mode(hdmi_ctrl, false);
	hdmi_tx_set_mode(hdmi_ctrl, true);

	hdmi_tx_core_off(hdmi_ctrl);

	hdmi_ctrl->panel_power_on = false;
	hdmi_ctrl->dc_support = false;

	if (hdmi_ctrl->hpd_off_pending || hdmi_ctrl->panel_suspend)
		hdmi_tx_hpd_off(hdmi_ctrl);

	if (hdmi_ctrl->hdmi_tx_hpd_done)
		hdmi_ctrl->hdmi_tx_hpd_done(
			hdmi_ctrl->downstream_data);
end:
	DEV_INFO("%s: HDMI Core: OFF\n", __func__);
	return 0;
} /* hdmi_tx_power_off */

static int hdmi_tx_power_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int ret, vic;
	u32 pixel_clk;
	struct mdss_panel_data *panel_data = &hdmi_ctrl->panel_data;
	void *pdata = hdmi_tx_get_fd(HDMI_TX_FEAT_PANEL);
	void *edata = hdmi_tx_get_fd(HDMI_TX_FEAT_EDID);

	hdmi_ctrl->hdcp_feature_on = hdcp_feature_on;
	vic = hdmi_panel_get_vic(&panel_data->panel_info,
				&hdmi_ctrl->ds_data);

	if (vic <= 0) {
		DEV_ERR("%s: invalid vic\n", __func__);
		return -EINVAL;
	}

	hdmi_ctrl->vic = vic;
	ret = hdmi_get_supported_mode(&hdmi_ctrl->timing,
		&hdmi_ctrl->ds_data, hdmi_ctrl->vic);
	if (ret || !hdmi_ctrl->timing.supported) {
		DEV_ERR("%s: invalid timing data\n", __func__);
		return -EINVAL;
	}

	hdmi_ctrl->panel.vic = hdmi_ctrl->vic;
	if (!hdmi_tx_is_dvi_mode(hdmi_ctrl))
		hdmi_ctrl->panel.infoframe = true;
	else
		hdmi_ctrl->panel.infoframe = false;

	hdmi_ctrl->panel.scan_info = hdmi_edid_get_sink_scaninfo(edata,
					hdmi_ctrl->vic);
	hdmi_ctrl->panel.scrambler = hdmi_edid_get_sink_scrambler_support(
					edata);
	hdmi_ctrl->panel.dc_enable = hdmi_tx_dc_support(hdmi_ctrl);

	if (hdmi_ctrl->panel_ops.on)
		hdmi_ctrl->panel_ops.on(pdata);

	pixel_clk = hdmi_tx_setup_tmds_clk_rate(hdmi_ctrl->timing.pixel_freq,
		hdmi_ctrl->panel.pinfo->out_format,
		hdmi_ctrl->panel.dc_enable) * 1000;

	DEV_DBG("%s: setting pixel clk %d\n", __func__, pixel_clk);

	hdmi_ctrl->pdata.power_data[HDMI_TX_CORE_PM].clk_config[0].rate =
		pixel_clk;

	hdmi_edid_set_video_resolution(hdmi_tx_get_fd(HDMI_TX_FEAT_EDID),
		hdmi_ctrl->vic, false);

	hdmi_tx_core_on(hdmi_ctrl);

	if (!hdmi_tx_is_encryption_set(hdmi_ctrl) &&
	    hdmi_tx_is_stream_shareable(hdmi_ctrl)) {
		hdmi_tx_config_avmute(hdmi_ctrl, false);
	}

	hdmi_ctrl->panel_power_on = true;

	hdmi_tx_hpd_polarity_setup(hdmi_ctrl, HPD_DISCONNECT_POLARITY);

	if (hdmi_ctrl->hdmi_tx_hpd_done)
		hdmi_ctrl->hdmi_tx_hpd_done(hdmi_ctrl->downstream_data);

	DEV_DBG("%s: hdmi_ctrl core on\n", __func__);
	return 0;
}

static void hdmi_tx_hpd_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;
	struct dss_io_data *io = NULL;
	unsigned long flags;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	if (!hdmi_ctrl->hpd_initialized) {
		DEV_DBG("%s: HPD is already OFF, returning\n", __func__);
		return;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return;
	}

	/* Turn off HPD interrupts */
	DSS_REG_W(io, HDMI_HPD_INT_CTRL, 0);


	if (hdmi_tx_is_cec_wakeup_en(hdmi_ctrl)) {
		hdmi_ctrl->mdss_util->enable_wake_irq(&hdmi_tx_hw);
	} else {
		hdmi_ctrl->mdss_util->disable_irq(&hdmi_tx_hw);
		hdmi_tx_set_mode(hdmi_ctrl, false);
	}
	hdmi_tx_config_5v(hdmi_ctrl, false);
	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, 0);
	if (rc)
		DEV_INFO("%s: Failed to disable hpd power. Error=%d\n",
			__func__, rc);

	spin_lock_irqsave(&hdmi_ctrl->hpd_state_lock, flags);
	hdmi_ctrl->hpd_state = false;
	spin_unlock_irqrestore(&hdmi_ctrl->hpd_state_lock, flags);

	hdmi_ctrl->hpd_initialized = false;
	hdmi_ctrl->hpd_off_pending = false;

	DEV_DBG("%s: HPD is now OFF\n", __func__);
} /* hdmi_tx_hpd_off */

static int hdmi_tx_hpd_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 reg_val;
	int rc = 0;
	struct dss_io_data *io = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_ERR("%s: core io not inititalized\n", __func__);
		return -EINVAL;
	}

	if (hdmi_ctrl->hpd_initialized) {
		DEV_DBG("%s: HPD is already ON\n", __func__);
	} else {
		rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_HPD_PM, true);
		if (rc) {
			DEV_ERR("%s: Failed to enable hpd power. rc=%d\n",
				__func__, rc);
			return rc;
		}

		dss_reg_dump(io->base, io->len, "HDMI-INIT: ", REG_DUMP);

		if (!hdmi_ctrl->panel_data.panel_info.cont_splash_enabled) {
			hdmi_tx_set_mode(hdmi_ctrl, false);
			hdmi_tx_phy_reset(hdmi_ctrl);
			hdmi_tx_set_mode(hdmi_ctrl, true);
		}

		DSS_REG_W(io, HDMI_USEC_REFTIMER, 0x0001001B);

		if (hdmi_tx_is_cec_wakeup_en(hdmi_ctrl))
			hdmi_ctrl->mdss_util->disable_wake_irq(&hdmi_tx_hw);

		hdmi_ctrl->mdss_util->enable_irq(&hdmi_tx_hw);

		hdmi_ctrl->hpd_initialized = true;

		DEV_INFO("%s: HDMI HW version = 0x%x\n", __func__,
			DSS_REG_R_ND(&hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO],
				HDMI_VERSION));

		/* set timeout to 4.1ms (max) for hardware debounce */
		reg_val = DSS_REG_R(io, HDMI_HPD_CTRL) | 0x1FFF;

		/* Turn on HPD HW circuit */
		DSS_REG_W(io, HDMI_HPD_CTRL, reg_val | BIT(28));

		hdmi_tx_hpd_polarity_setup(hdmi_ctrl, HPD_CONNECT_POLARITY);
		DEV_DBG("%s: HPD is now ON\n", __func__);
	}

	return rc;
} /* hdmi_tx_hpd_on */

static int hdmi_tx_sysfs_enable_hpd(struct hdmi_tx_ctrl *hdmi_ctrl, int on)
{
	int rc = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	DEV_DBG("%s: %d\n", __func__, on);
	if (on) {
		hdmi_ctrl->hpd_off_pending = false;
		rc = hdmi_tx_hpd_on(hdmi_ctrl);
	} else {
		if (!hdmi_ctrl->panel_power_on)
			hdmi_tx_hpd_off(hdmi_ctrl);
		else
			hdmi_ctrl->hpd_off_pending = true;
	}

	return rc;
} /* hdmi_tx_sysfs_enable_hpd */

static int hdmi_tx_set_mhl_hpd(struct platform_device *pdev, uint8_t on)
{
	int rc = 0;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&hdmi_ctrl->tx_lock);

	/* mhl status should override */
	hdmi_ctrl->mhl_hpd_on = on;

	if (!on && hdmi_ctrl->hpd_feature_on) {
		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, false);
	} else if (on && !hdmi_ctrl->hpd_feature_on) {
		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
	} else {
		DEV_DBG("%s: hpd is already '%s'. return\n", __func__,
			hdmi_ctrl->hpd_feature_on ? "enabled" : "disabled");
		goto end;
	}

	if (!rc) {
		hdmi_ctrl->hpd_feature_on =
			(~hdmi_ctrl->hpd_feature_on) & BIT(0);
		DEV_DBG("%s: '%d'\n", __func__, hdmi_ctrl->hpd_feature_on);
	} else {
		DEV_ERR("%s: failed to '%s' hpd. rc = %d\n", __func__,
			on ? "enable" : "disable", rc);
	}
end:
	mutex_unlock(&hdmi_ctrl->tx_lock);
	return rc;
}

static irqreturn_t hdmi_tx_isr(int irq, void *data)
{
	struct dss_io_data *io = NULL;
	struct hdmi_tx_ctrl *hdmi_ctrl = (struct hdmi_tx_ctrl *)data;
	unsigned long flags;
	u32 hpd_current_state;
	u32 reg_val = 0;

	if (!hdmi_ctrl) {
		DEV_WARN("%s: invalid hdmi ctrl data data, ISR ignored\n",
			__func__);
		goto end;
	}

	io = &hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO];
	if (!io->base) {
		DEV_WARN("%s: core io not initialized, ISR ignored\n",
			__func__);
		goto end;
	}

	if (DSS_REG_R(io, HDMI_HPD_INT_STATUS) & BIT(0)) {
		spin_lock_irqsave(&hdmi_ctrl->hpd_state_lock, flags);
		hpd_current_state = hdmi_ctrl->hpd_state;
		hdmi_ctrl->hpd_state =
			(DSS_REG_R(io, HDMI_HPD_INT_STATUS) & BIT(1)) >> 1;
		spin_unlock_irqrestore(&hdmi_ctrl->hpd_state_lock, flags);

		if (!completion_done(&hdmi_ctrl->hpd_int_done))
			complete_all(&hdmi_ctrl->hpd_int_done);

		/*
		 * check if this is a spurious interrupt, if yes, reset
		 * interrupts and return
		 */
		if (hpd_current_state == hdmi_ctrl->hpd_state) {
			DEV_DBG("%s: spurious interrupt %d\n", __func__,
				hpd_current_state);

			/* enable interrupts */
			reg_val |= BIT(2);

			/* set polarity, reverse of current state */
			reg_val |= (~hpd_current_state << 1) & BIT(1);

			/* ack interrupt */
			reg_val |= BIT(0);

			DSS_REG_W(io, HDMI_HPD_INT_CTRL, reg_val);
			goto end;
		}

		/*
		 * Ack the current hpd interrupt and stop listening to
		 * new hpd interrupt.
		 */
		DSS_REG_W(io, HDMI_HPD_INT_CTRL, BIT(0));

		queue_work(hdmi_ctrl->workq, &hdmi_ctrl->hpd_int_work);
	}

	if (hdmi_ddc_isr(&hdmi_ctrl->ddc_ctrl,
		hdmi_ctrl->hdmi_tx_major_version))
		DEV_ERR("%s: hdmi_ddc_isr failed\n", __func__);

	if (hdmi_tx_get_fd(HDMI_TX_FEAT_CEC_HW)) {
		if (hdmi_cec_isr(hdmi_tx_get_fd(HDMI_TX_FEAT_CEC_HW)))
			DEV_ERR("%s: hdmi_cec_isr failed\n", __func__);
	}

	if (hdmi_ctrl->hdcp_ops && hdmi_ctrl->hdcp_data) {
		if (hdmi_ctrl->hdcp_ops->isr) {
			if (hdmi_ctrl->hdcp_ops->isr(
				hdmi_ctrl->hdcp_data))
				DEV_ERR("%s: hdcp_1x_isr failed\n",
					 __func__);
		}
	}
end:
	return IRQ_HANDLED;
} /* hdmi_tx_isr */

static void hdmi_tx_dev_deinit(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	hdmi_tx_deinit_features(hdmi_ctrl, HDMI_TX_FEAT_MAX);

	hdmi_ctrl->hdcp_ops = NULL;
	hdmi_ctrl->hdcp_data = NULL;

	if (hdmi_ctrl->workq)
		destroy_workqueue(hdmi_ctrl->workq);
	mutex_destroy(&hdmi_ctrl->tx_lock);
	mutex_destroy(&hdmi_ctrl->mutex);

	hdmi_tx_hw.ptr = NULL;
} /* hdmi_tx_dev_deinit */

static int hdmi_tx_dev_init(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;
	struct hdmi_tx_platform_data *pdata = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	pdata = &hdmi_ctrl->pdata;

	rc = hdmi_tx_check_capability(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: no HDMI device\n", __func__);
		goto fail_no_hdmi;
	}

	/* irq enable/disable will be handled in hpd on/off */
	hdmi_tx_hw.ptr = (void *)hdmi_ctrl;

	mutex_init(&hdmi_ctrl->mutex);
	mutex_init(&hdmi_ctrl->tx_lock);

	INIT_LIST_HEAD(&hdmi_ctrl->cable_notify_handlers);

	hdmi_ctrl->workq = create_workqueue("hdmi_tx_workq");
	if (!hdmi_ctrl->workq) {
		DEV_ERR("%s: hdmi_tx_workq creation failed.\n", __func__);
		rc = -EPERM;
		goto fail_create_workq;
	}

	hdmi_ctrl->ddc_ctrl.io = &pdata->io[HDMI_TX_CORE_IO];
	init_completion(&hdmi_ctrl->ddc_ctrl.ddc_sw_done);

	hdmi_ctrl->panel_power_on = false;
	hdmi_ctrl->panel_suspend = false;

	hdmi_ctrl->hpd_state = false;
	hdmi_ctrl->hpd_initialized = false;
	hdmi_ctrl->hpd_off_pending = false;
	init_completion(&hdmi_ctrl->hpd_int_done);

	INIT_WORK(&hdmi_ctrl->hpd_int_work, hdmi_tx_hpd_int_work);
	INIT_WORK(&hdmi_ctrl->fps_work, hdmi_tx_fps_work);
	INIT_WORK(&hdmi_ctrl->cable_notify_work, hdmi_tx_cable_notify_work);
	INIT_DELAYED_WORK(&hdmi_ctrl->hdcp_cb_work, hdmi_tx_hdcp_cb_work);

	spin_lock_init(&hdmi_ctrl->hpd_state_lock);

	return 0;

fail_create_workq:
	if (hdmi_ctrl->workq)
		destroy_workqueue(hdmi_ctrl->workq);
	mutex_destroy(&hdmi_ctrl->mutex);
fail_no_hdmi:
	return rc;
} /* hdmi_tx_dev_init */

static int hdmi_tx_start_hdcp(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	if (hdmi_ctrl->panel_data.panel_info.cont_splash_enabled ||
		!hdmi_tx_is_hdcp_enabled(hdmi_ctrl))
		return 0;

	if (hdmi_tx_is_encryption_set(hdmi_ctrl))
		hdmi_tx_config_avmute(hdmi_ctrl, true);

	rc = hdmi_ctrl->hdcp_ops->authenticate(hdmi_ctrl->hdcp_data);
	if (rc)
		DEV_ERR("%s: hdcp auth failed. rc=%d\n", __func__, rc);

	return rc;
}

static int hdmi_tx_hdcp_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	DEV_DBG("%s: Turning off HDCP\n", __func__);
	hdmi_ctrl->hdcp_ops->off(hdmi_ctrl->hdcp_data);

	flush_delayed_work(&hdmi_ctrl->hdcp_cb_work);

	hdmi_ctrl->hdcp_ops = NULL;

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM,
		false);
	if (rc)
		DEV_ERR("%s: Failed to disable ddc power\n",
			__func__);

	return rc;
}

static void hdmi_tx_update_fps(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	void *pdata = NULL;
	struct mdss_panel_info *pinfo;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	pdata = hdmi_tx_get_fd(HDMI_TX_FEAT_PANEL);
	if (!pdata) {
		DEV_ERR("%s: invalid panel data\n", __func__);
		return;
	}

	pinfo = &hdmi_ctrl->panel_data.panel_info;

	if (!pinfo->dynamic_fps) {
		DEV_DBG("%s: Dynamic fps not enabled\n", __func__);
		return;
	}

	DEV_DBG("%s: current fps %d, new fps %d\n", __func__,
		pinfo->current_fps, hdmi_ctrl->dynamic_fps);

	if (hdmi_ctrl->dynamic_fps == pinfo->current_fps) {
		DEV_DBG("%s: Panel is already at this FPS: %d\n",
			__func__, hdmi_ctrl->dynamic_fps);
		return;
	}

	if (hdmi_tx_is_hdcp_enabled(hdmi_ctrl))
		hdmi_tx_hdcp_off(hdmi_ctrl);

	if (hdmi_ctrl->panel_ops.update_fps)
		hdmi_ctrl->vic = hdmi_ctrl->panel_ops.update_fps(pdata,
			hdmi_ctrl->dynamic_fps);

	hdmi_tx_update_pixel_clk(hdmi_ctrl);

	hdmi_tx_start_hdcp(hdmi_ctrl);
}

static void hdmi_tx_fps_work(struct work_struct *work)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;

	hdmi_ctrl = container_of(work, struct hdmi_tx_ctrl, fps_work);
	if (!hdmi_ctrl) {
		DEV_DBG("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	hdmi_tx_update_fps(hdmi_ctrl);
}

static int hdmi_tx_evt_handle_register(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;

	rc = hdmi_tx_sysfs_create(hdmi_ctrl, hdmi_ctrl->evt_arg);
	if (rc) {
		DEV_ERR("%s: hdmi_tx_sysfs_create failed.rc=%d\n",
			__func__, rc);
		goto sysfs_err;
	}
	rc = hdmi_tx_init_features(hdmi_ctrl, hdmi_ctrl->evt_arg);
	if (rc) {
		DEV_ERR("%s: init_features failed.rc=%d\n", __func__, rc);
		goto init_err;
	}

	if (hdmi_ctrl->pdata.primary || !hdmi_ctrl->pdata.pluggable) {
		reinit_completion(&hdmi_ctrl->hpd_int_done);
		rc = hdmi_tx_sysfs_enable_hpd(hdmi_ctrl, true);
		if (rc) {
			DEV_ERR("%s: hpd_enable failed. rc=%d\n", __func__, rc);
			goto primary_err;
		} else {
			hdmi_ctrl->hpd_feature_on = true;
		}
	}

	return 0;

primary_err:
	hdmi_tx_deinit_features(hdmi_ctrl, HDMI_TX_FEAT_MAX);
init_err:
	hdmi_tx_sysfs_remove(hdmi_ctrl);
sysfs_err:
	return rc;
}

static int hdmi_tx_evt_handle_check_param(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int new_vic = -1;
	int rc = 0;

	new_vic = hdmi_panel_get_vic(hdmi_ctrl->evt_arg, &hdmi_ctrl->ds_data);

	if ((new_vic < 0) || (new_vic > HDMI_VFRMT_MAX)) {
		DEV_ERR("%s: invalid or not supported vic\n", __func__);
		goto end;
	}

	/*
	 * return value of 1 lets mdss know that panel
	 * needs a reconfig due to new resolution and
	 * it will issue close and open subsequently.
	 */
	if (new_vic != hdmi_ctrl->vic) {
		rc = 1;
		DEV_DBG("%s: res change %d ==> %d\n", __func__,
			hdmi_ctrl->vic, new_vic);
	}
end:
	return rc;
}

static int hdmi_tx_evt_handle_resume(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;

	hdmi_ctrl->panel_suspend = false;
	hdmi_tx_cec_device_suspend(hdmi_ctrl);

	if (!hdmi_ctrl->hpd_feature_on)
		goto end;

	rc = hdmi_tx_hpd_on(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: hpd_on failed. rc=%d\n", __func__, rc);
		goto end;
	}

end:
	return rc;
}

static int hdmi_tx_evt_handle_reset(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl->panel_data.panel_info.cont_splash_enabled &&
	    hdmi_ctrl->hpd_initialized) {
		hdmi_tx_set_mode(hdmi_ctrl, false);
		hdmi_tx_phy_reset(hdmi_ctrl);
		hdmi_tx_set_mode(hdmi_ctrl, true);
	}

	return 0;
}

static int hdmi_tx_evt_handle_unblank(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc;

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, true);
	if (rc) {
		DEV_ERR("%s: ddc power on failed. rc=%d\n", __func__, rc);
		goto end;
	}

	rc = hdmi_tx_power_on(hdmi_ctrl);
	if (rc)
		DEV_ERR("%s: hdmi_tx_power_on failed. rc=%d\n", __func__, rc);
end:
	return rc;
}

static int hdmi_tx_evt_handle_panel_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;

	if (!hdmi_ctrl->sim_mode) {
		hdmi_tx_update_hdcp_info(hdmi_ctrl);

		rc = hdmi_tx_start_hdcp(hdmi_ctrl);
		if (rc)
			DEV_ERR("%s: hdcp start failed rc=%d\n", __func__, rc);
	}

	hdmi_ctrl->timing_gen_on = true;

	return rc;
}

static int hdmi_tx_evt_handle_suspend(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl->hpd_feature_on)
		goto end;

	if (!hdmi_ctrl->hpd_state && !hdmi_ctrl->panel_power_on)
		hdmi_tx_hpd_off(hdmi_ctrl);

	hdmi_ctrl->panel_suspend = true;
	hdmi_tx_cec_device_suspend(hdmi_ctrl);
end:
	return 0;
}

static int hdmi_tx_evt_handle_blank(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (hdmi_tx_is_hdcp_enabled(hdmi_ctrl))
		hdmi_tx_hdcp_off(hdmi_ctrl);

	return 0;
}

static int hdmi_tx_evt_handle_panel_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc;

	rc = hdmi_tx_enable_power(hdmi_ctrl, HDMI_TX_DDC_PM, false);
	if (rc) {
		DEV_ERR("%s: Failed to disable ddc power\n", __func__);
		goto end;
	}

	if (hdmi_ctrl->panel_power_on) {
		hdmi_tx_config_avmute(hdmi_ctrl, 1);
		rc = hdmi_tx_power_off(hdmi_ctrl);
		if (rc)
			DEV_ERR("%s: hdmi_tx_power_off failed.rc=%d\n",
				__func__, rc);
	} else {
		DEV_DBG("%s: hdmi_ctrl is already powered off\n", __func__);
	}

	hdmi_ctrl->timing_gen_on = false;
end:
	return rc;
}

static int hdmi_tx_evt_handle_close(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (hdmi_ctrl->hpd_feature_on && hdmi_ctrl->hpd_initialized &&
		!hdmi_ctrl->hpd_state)
		hdmi_tx_hpd_polarity_setup(hdmi_ctrl, HPD_CONNECT_POLARITY);

	return 0;
}

static int hdmi_tx_evt_handle_deep_color(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	u32 deep_color = (int) (unsigned long) hdmi_ctrl->evt_arg;
	struct mdss_panel_info *pinfo = &hdmi_ctrl->panel_data.panel_info;

	hdmi_ctrl->dc_support = true;

	if (deep_color & BIT(1)) {
		pinfo->deep_color |= MDP_DEEP_COLOR_RGB30B;
		pinfo->bpp = 30;
	} else if (deep_color & BIT(2)) {
		pinfo->deep_color |= MDP_DEEP_COLOR_RGB36B;
		pinfo->bpp = 36;
	} else if (deep_color & BIT(3)) {
		pinfo->deep_color |= MDP_DEEP_COLOR_RGB48B;
		pinfo->bpp = 48;
	} else {
		hdmi_ctrl->dc_support = false;
	}

	return 0;
}

static int hdmi_tx_pre_evt_handle_panel_off(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	hdmi_tx_ack_state(hdmi_ctrl, false);
	return 0;
}

static int hdmi_tx_pre_evt_handle_update_fps(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	hdmi_ctrl->dynamic_fps = (u32) (unsigned long)hdmi_ctrl->evt_arg;
	queue_work(hdmi_ctrl->workq, &hdmi_ctrl->fps_work);
	return 0;
}

static int hdmi_tx_post_evt_handle_unblank(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	hdmi_tx_ack_state(hdmi_ctrl, true);
	return 0;
}

static int hdmi_tx_post_evt_handle_resume(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (!hdmi_ctrl->hpd_feature_on)
		return 0;

	if (!hdmi_tx_hw_is_cable_connected(hdmi_ctrl)) {
		u32 timeout;

		reinit_completion(&hdmi_ctrl->hpd_int_done);
		timeout = wait_for_completion_timeout(
			&hdmi_ctrl->hpd_int_done, HZ/10);
		if (!timeout) {
			pr_debug("cable removed during suspend\n");
		hdmi_tx_audio_notify(hdmi_ctrl, 0);
		hdmi_tx_send_cable_notification(hdmi_ctrl, 0);
		}
	}

	return 0;
}

static int hdmi_tx_post_evt_handle_panel_on(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	if (hdmi_ctrl->panel_suspend) {
		pr_debug("panel suspend has triggered\n");
		hdmi_tx_audio_notify(hdmi_ctrl, 0);
		hdmi_tx_send_cable_notification(hdmi_ctrl, 0);
	}

	return 0;
}

static int hdmi_tx_event_handler(struct mdss_panel_data *panel_data,
	int event, void *arg)
{
	int rc = 0;
	hdmi_tx_evt_handler handler;
	struct hdmi_tx_ctrl *hdmi_ctrl =
		hdmi_tx_get_drvdata_from_panel_data(panel_data);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	hdmi_ctrl->evt_arg = arg;

	pr_debug("event = %s suspend=%d, hpd_feature=%d\n",
		mdss_panel_intf_event_to_string(event),
		hdmi_ctrl->panel_suspend,
		hdmi_ctrl->hpd_feature_on);

	/* event handlers prior to tx_lock */
	handler = hdmi_ctrl->pre_evt_handler[event];
	if (handler) {
		rc = handler(hdmi_ctrl);
		if (rc) {
			pr_err("pre handler failed: event = %s, rc = %d\n",
				mdss_panel_intf_event_to_string(event), rc);
		return rc;
	}
	}

	mutex_lock(&hdmi_ctrl->tx_lock);

	handler = hdmi_ctrl->evt_handler[event];
	if (handler) {
		rc = handler(hdmi_ctrl);
		if (rc) {
			pr_err("handler failed: event = %s, rc = %d\n",
				mdss_panel_intf_event_to_string(event), rc);
			mutex_unlock(&hdmi_ctrl->tx_lock);
			return rc;
		}
	}

	mutex_unlock(&hdmi_ctrl->tx_lock);

	/* event handlers post to tx_lock */
	handler = hdmi_ctrl->post_evt_handler[event];
	if (handler) {
		rc = handler(hdmi_ctrl);
		if (rc)
			pr_err("post handler failed: event = %s, rc = %d\n",
				mdss_panel_intf_event_to_string(event), rc);
	}

	return rc;
}

static int hdmi_tx_register_panel(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int rc = 0;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	hdmi_ctrl->panel_data.event_handler = hdmi_tx_event_handler;

	if (!hdmi_ctrl->pdata.primary)
		hdmi_ctrl->vic = DEFAULT_VIDEO_RESOLUTION;

	rc = hdmi_tx_init_panel_info(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: hdmi_init_panel_info failed\n", __func__);
		return rc;
	}

	rc = mdss_register_panel(hdmi_ctrl->pdev, &hdmi_ctrl->panel_data);
	if (rc) {
		DEV_ERR("%s: FAILED: to register HDMI panel\n", __func__);
		return rc;
	}

	rc = hdmi_ctrl->mdss_util->register_irq(&hdmi_tx_hw);
	if (rc)
		DEV_ERR("%s: mdss_register_irq failed.\n", __func__);

	return rc;
} /* hdmi_tx_register_panel */

static void hdmi_tx_deinit_resource(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int i;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return;
	}

	/* VREG & CLK */
	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--) {
		if (hdmi_tx_config_power(hdmi_ctrl, i, 0))
			DEV_ERR("%s: '%s' power deconfig fail\n",
				__func__, hdmi_tx_pm_name(i));
	}

	/* IO */
	for (i = HDMI_TX_MAX_IO - 1; i >= 0; i--) {
		if (hdmi_ctrl->pdata.io[i].base)
			msm_dss_iounmap(&hdmi_ctrl->pdata.io[i]);
	}
} /* hdmi_tx_deinit_resource */

static int hdmi_tx_init_resource(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	int i, rc = 0;
	struct hdmi_tx_platform_data *pdata = NULL;

	if (!hdmi_ctrl) {
		DEV_ERR("%s: invalid hdmi ctrl data\n", __func__);
		return -EINVAL;
	}

	pdata = &hdmi_ctrl->pdata;

	hdmi_tx_pinctrl_init(hdmi_ctrl->pdev);

	/* IO */
	for (i = 0; i < HDMI_TX_MAX_IO; i++) {
		rc = msm_dss_ioremap_byname(hdmi_ctrl->pdev, &pdata->io[i],
			hdmi_tx_io_name(i));
		if (rc) {
			DEV_DBG("%s: '%s' remap failed or not available\n",
				__func__, hdmi_tx_io_name(i));
		}
		DEV_INFO("%s: '%s': start = 0x%pK, len=0x%x\n", __func__,
			hdmi_tx_io_name(i), pdata->io[i].base,
			pdata->io[i].len);
	}

	/* VREG & CLK */
	for (i = 0; i < HDMI_TX_MAX_PM; i++) {
		rc = hdmi_tx_config_power(hdmi_ctrl, i, 1);
		if (rc) {
			DEV_ERR("%s: '%s' power config failed.rc=%d\n",
				__func__, hdmi_tx_pm_name(i), rc);
			goto error;
		}
	}

	return rc;

error:
	hdmi_tx_deinit_resource(hdmi_ctrl);
	return rc;
} /* hdmi_tx_init_resource */

static void hdmi_tx_put_dt_clk_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		DEV_ERR("%s: invalid power module\n", __func__);
		return;
	}

	module_power->num_clk = 0;
} /* hdmi_tx_put_dt_clk_data */

static int hdmi_tx_is_core_clk(const char *clk_name)
{
	bool is_core_clk = false;

	if (!strcmp(clk_name, "core_extp_clk"))
		is_core_clk = true;
	else
		is_core_clk = false;

	return is_core_clk;
}

static int hdmi_tx_is_hpd_clk(const char *clk_name)
{
	bool is_hpd_clk = false;

	if (!strcmp(clk_name, "hpd_iface_clk"))
		is_hpd_clk = true;
	else if (!strcmp(clk_name, "hpd_mdp_core_clk"))
		is_hpd_clk = true;
	else if (!strcmp(clk_name, "hpd_core_clk"))
		is_hpd_clk = true;
	else
		is_hpd_clk = false;

	return is_hpd_clk;
}

static int hdmi_tx_init_power_data(struct device *dev,
		struct hdmi_tx_platform_data *pdata)
{
	int num_clk = 0, i = 0, rc = 0;
	int hpd_clk_count = 0, core_clk_count = 0;
	struct dss_module_power *hpd_power_data = NULL;
	struct dss_module_power *core_power_data = NULL;
	const char *clk_name;

	num_clk = of_property_count_strings(dev->of_node,
			"clock-names");
	if (num_clk <= 0) {
		pr_err("%s: no clocks are defined\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	hpd_power_data = &pdata->power_data[HDMI_TX_HPD_PM];
	core_power_data = &pdata->power_data[HDMI_TX_CORE_PM];

	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(dev->of_node, "clock-names",
				i, &clk_name);

		if (hdmi_tx_is_hpd_clk(clk_name))
			hpd_clk_count++;
		if (hdmi_tx_is_core_clk(clk_name))
			core_clk_count++;
	}

	/* Initialize the HPD power module */
	if (hpd_clk_count <= 0) {
		pr_err("%s: no hpd clocks are defined\n", __func__);
		rc = -EINVAL;
		goto exit;
		}

	hpd_power_data->num_clk = hpd_clk_count;
	hpd_power_data->clk_config = devm_kzalloc(dev, sizeof(struct dss_clk) *
			hpd_power_data->num_clk, GFP_KERNEL);
	if (!hpd_power_data->clk_config) {
		rc = -EINVAL;
		goto exit;
	}

	/* Initialize the CORE power module */
	if (core_clk_count <= 0) {
		pr_err("%s: no core clocks are defined\n", __func__);
		rc = -EINVAL;
		goto core_clock_error;
	}

	core_power_data->num_clk = core_clk_count;
	core_power_data->clk_config = devm_kzalloc(dev, sizeof(struct dss_clk) *
			core_power_data->num_clk, GFP_KERNEL);
	if (!core_power_data->clk_config) {
		core_power_data->num_clk = 0;
		rc = -EINVAL;
		goto core_clock_error;
	}

	return rc;

core_clock_error:
	hdmi_tx_put_dt_clk_data(dev, hpd_power_data);
exit:
	return rc;
		}

static int hdmi_tx_get_dt_clk_data(struct device *dev,
		struct hdmi_tx_platform_data *pdata)
{
	int rc = 0, i = 0;
	const char *clk_name;
	int num_clk = 0;
	int hpd_clk_index = 0, core_clk_index = 0;
	int hpd_clk_count = 0, core_clk_count = 0;
	struct dss_module_power *hpd_power_data = NULL;
	struct dss_module_power *core_power_data = NULL;

	if (!dev) {
		pr_err("%s: invalid device\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	rc =  hdmi_tx_init_power_data(dev, pdata);
	if (rc) {
		pr_err("%s: failed to initialize power data\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	hpd_power_data = &pdata->power_data[HDMI_TX_HPD_PM];
	hpd_clk_count = hpd_power_data->num_clk;
	core_power_data = &pdata->power_data[HDMI_TX_CORE_PM];
	core_clk_count = core_power_data->num_clk;

	num_clk = hpd_clk_count + core_clk_count;

	for (i = 0; i < num_clk; i++) {
		of_property_read_string_index(dev->of_node, "clock-names",
				i, &clk_name);

		if (hdmi_tx_is_hpd_clk(clk_name)
				&& hpd_clk_index < hpd_clk_count) {
			struct dss_clk *clk =
				&hpd_power_data->clk_config[hpd_clk_index];
			strlcpy(clk->clk_name, clk_name, sizeof(clk->clk_name));
			clk->type = DSS_CLK_AHB;
			hpd_clk_index++;
		} else if (hdmi_tx_is_core_clk(clk_name)
				&& core_clk_index < core_clk_count) {
			struct dss_clk *clk =
				&core_power_data->clk_config[core_clk_index];
			strlcpy(clk->clk_name, clk_name, sizeof(clk->clk_name));
			clk->type = DSS_CLK_PCLK;
			core_clk_index++;
		}
	}

	pdata->ext_hdmi_pixel_clk = devm_clk_get(dev, "ext_hdmi_pixel_clk");
	if (IS_ERR(pdata->ext_hdmi_pixel_clk)) {
		pr_debug("%s: can't find ext hdmi pclk. rc=%d\n", __func__, rc);
		pdata->ext_hdmi_pixel_clk = NULL;
	}

	pdata->hdmi_pclk_rcg = devm_clk_get(dev, "hdmi_pclk_rcg");
	if (IS_ERR(pdata->hdmi_pclk_rcg)) {
		pr_debug("%s: can't find hdmi pclk rcg. rc=%d\n", __func__, rc);
		pdata->hdmi_pclk_rcg = NULL;
	}

	pr_debug("%s: HDMI clock parsing successful\n", __func__);

	return rc;

exit:
	return rc;
} /* hdmi_tx_get_dt_clk_data */

static void hdmi_tx_put_dt_vreg_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		DEV_ERR("%s: invalid power module\n", __func__);
		return;
	}

	module_power->num_vreg = 0;
} /* hdmi_tx_put_dt_vreg_data */

static int hdmi_tx_get_dt_vreg_data(struct device *dev,
	struct dss_module_power *mp, u32 module_type)
{
	int i, j, rc = 0;
	int dt_vreg_total = 0, mod_vreg_total = 0;
	u32 ndx_mask = 0;
	u32 *val_array = NULL;
	const char *mod_name = NULL;
	struct device_node *of_node = NULL;

	if (!dev) {
		DEV_ERR("%s: invalid device\n", __func__);
		return -EINVAL;
	}

	if (!mp) {
		DEV_ERR("%s: invalid power module\n", __func__);
		return -EINVAL;
	}

	switch (module_type) {
	case HDMI_TX_HPD_PM:
		mod_name = "hpd";
		break;
	case HDMI_TX_DDC_PM:
		mod_name = "ddc";
		break;
	case HDMI_TX_CORE_PM:
		mod_name = "core";
		break;
	case HDMI_TX_CEC_PM:
		mod_name = "cec";
		break;
	default:
		DEV_ERR("%s: invalid module type=%d\n", __func__,
			module_type);
		return -EINVAL;
	}

	DEV_DBG("%s: module: '%s'\n", __func__, hdmi_tx_pm_name(module_type));

	of_node = dev->of_node;

	dt_vreg_total = of_property_count_strings(of_node, "qcom,supply-names");
	if (dt_vreg_total < 0) {
		DEV_ERR("%s: vreg not found. rc=%d\n", __func__,
			dt_vreg_total);
		rc = dt_vreg_total;
		goto error;
	}

	/* count how many vreg for particular hdmi module */
	for (i = 0; i < dt_vreg_total; i++) {
		const char *st = NULL;

		rc = of_property_read_string_index(of_node,
			"qcom,supply-names", i, &st);
		if (rc) {
			DEV_ERR("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto error;
		}

		if (strnstr(st, mod_name, strlen(st))) {
			ndx_mask |= BIT(i);
			mod_vreg_total++;
		}
	}

	if (mod_vreg_total > 0) {
		mp->num_vreg = mod_vreg_total;
		mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
			mod_vreg_total, GFP_KERNEL);
		if (!mp->vreg_config) {
			DEV_ERR("%s: can't alloc '%s' vreg mem\n", __func__,
				hdmi_tx_pm_name(module_type));
			goto error;
		}
	} else {
		DEV_DBG("%s: no vreg\n", __func__);
		return 0;
	}

	val_array = devm_kzalloc(dev, sizeof(u32) * dt_vreg_total, GFP_KERNEL);
	if (!val_array) {
		DEV_ERR("%s: can't allocate vreg scratch mem\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	for (i = 0, j = 0; (i < dt_vreg_total) && (j < mod_vreg_total); i++) {
		const char *st = NULL;

		if (!(ndx_mask & BIT(0))) {
			ndx_mask >>= 1;
			continue;
		}

		/* vreg-name */
		rc = of_property_read_string_index(of_node,
			"qcom,supply-names", i, &st);
		if (rc) {
			DEV_ERR("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto error;
		}
		snprintf(mp->vreg_config[j].vreg_name, 32, "%s", st);

		/* vreg-min-voltage */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,min-voltage-level", val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' min volt. rc=%d\n",
				__func__, hdmi_tx_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].min_voltage = val_array[i];

		/* vreg-max-voltage */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,max-voltage-level", val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' max volt. rc=%d\n",
				__func__, hdmi_tx_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].max_voltage = val_array[i];

		/* vreg-op-mode */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,enable-load", val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' enable load. rc=%d\n",
				__func__, hdmi_tx_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].enable_load = val_array[i];

		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,disable-load", val_array,
			dt_vreg_total);
		if (rc) {
			DEV_ERR("%s: error read '%s' disable load. rc=%d\n",
				__func__, hdmi_tx_pm_name(module_type), rc);
			goto error;
		}
		mp->vreg_config[j].disable_load = val_array[i];

		DEV_DBG("%s: %s min=%d, max=%d, enable=%d disable=%d\n",
			__func__,
			mp->vreg_config[j].vreg_name,
			mp->vreg_config[j].min_voltage,
			mp->vreg_config[j].max_voltage,
			mp->vreg_config[j].enable_load,
			mp->vreg_config[j].disable_load);

		ndx_mask >>= 1;
		j++;
	}

	return rc;

error:
	mp->num_vreg = 0;

	return rc;
} /* hdmi_tx_get_dt_vreg_data */

static void hdmi_tx_put_dt_gpio_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		DEV_ERR("%s: invalid power module\n", __func__);
		return;
	}

	module_power->num_gpio = 0;
} /* hdmi_tx_put_dt_gpio_data */

static int hdmi_tx_get_dt_gpio_data(struct device *dev,
	struct dss_module_power *mp, u32 module_type)
{
	int i, j;
	int mp_gpio_cnt = 0, gpio_list_size = 0;
	struct dss_gpio *gpio_list = NULL;
	struct device_node *of_node = NULL;

	DEV_DBG("%s: module: '%s'\n", __func__, hdmi_tx_pm_name(module_type));

	if (!dev) {
		DEV_ERR("%s: invalid device\n", __func__);
		return -EINVAL;
	}

	if (!mp) {
		DEV_ERR("%s: invalid power module\n", __func__);
		return -EINVAL;
	}

	of_node = dev->of_node;

	switch (module_type) {
	case HDMI_TX_HPD_PM:
		gpio_list_size = ARRAY_SIZE(hpd_gpio_config);
		gpio_list = hpd_gpio_config;
		break;
	case HDMI_TX_DDC_PM:
		gpio_list_size = ARRAY_SIZE(ddc_gpio_config);
		gpio_list = ddc_gpio_config;
		break;
	case HDMI_TX_CORE_PM:
		gpio_list_size = ARRAY_SIZE(core_gpio_config);
		gpio_list = core_gpio_config;
		break;
	case HDMI_TX_CEC_PM:
		gpio_list_size = ARRAY_SIZE(cec_gpio_config);
		gpio_list = cec_gpio_config;
		break;
	default:
		DEV_ERR("%s: invalid module type=%d\n", __func__,
			module_type);
		return -EINVAL;
	}

	for (i = 0; i < gpio_list_size; i++)
		if (of_find_property(of_node, gpio_list[i].gpio_name, NULL))
			mp_gpio_cnt++;

	if (!mp_gpio_cnt) {
		DEV_DBG("%s: no gpio\n", __func__);
		return 0;
	}

	DEV_DBG("%s: mp_gpio_cnt = %d\n", __func__, mp_gpio_cnt);
	mp->num_gpio = mp_gpio_cnt;

	mp->gpio_config = devm_kzalloc(dev, sizeof(struct dss_gpio) *
		mp_gpio_cnt, GFP_KERNEL);
	if (!mp->gpio_config) {
		DEV_ERR("%s: can't alloc '%s' gpio mem\n", __func__,
			hdmi_tx_pm_name(module_type));

		mp->num_gpio = 0;
		return -ENOMEM;
	}

	for (i = 0, j = 0; i < gpio_list_size; i++) {
		int gpio = of_get_named_gpio(of_node,
			gpio_list[i].gpio_name, 0);
		if (gpio < 0) {
			DEV_DBG("%s: no gpio named %s\n", __func__,
				gpio_list[i].gpio_name);
			continue;
		}
		memcpy(&mp->gpio_config[j], &gpio_list[i],
			sizeof(struct dss_gpio));

		mp->gpio_config[j].gpio = (unsigned int)gpio;

		DEV_DBG("%s: gpio num=%d, name=%s, value=%d\n",
			__func__, mp->gpio_config[j].gpio,
			mp->gpio_config[j].gpio_name,
			mp->gpio_config[j].value);
		j++;
	}

	return 0;
} /* hdmi_tx_get_dt_gpio_data */

static void hdmi_tx_put_dt_data(struct device *dev,
	struct hdmi_tx_platform_data *pdata)
{
	int i;

	if (!dev) {
		DEV_ERR("%s: invalid device\n", __func__);
		return;
	}

	if (!pdata) {
		DEV_ERR("%s: invalid platform data\n", __func__);
		return;
	}

	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--)
		hdmi_tx_put_dt_clk_data(dev, &pdata->power_data[i]);

	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--)
		hdmi_tx_put_dt_vreg_data(dev, &pdata->power_data[i]);

	for (i = HDMI_TX_MAX_PM - 1; i >= 0; i--)
		hdmi_tx_put_dt_gpio_data(dev, &pdata->power_data[i]);
} /* hdmi_tx_put_dt_data */

static int hdmi_tx_get_dt_data(struct platform_device *pdev,
	struct hdmi_tx_platform_data *pdata)
{
	int i, rc = 0, len = 0;
	struct device_node *of_node = NULL;
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);
	const char *data;

	if (!pdev) {
		DEV_ERR("%s: invalid platform device\n", __func__);
		return -EINVAL;
	}

	if (!pdata) {
		DEV_ERR("%s: invalid platform data\n", __func__);
		return -EINVAL;
	}

	of_node = pdev->dev.of_node;

	rc = of_property_read_u32(of_node, "cell-index", &pdev->id);
	if (rc) {
		DEV_ERR("%s: dev id from dt not found.rc=%d\n",
			__func__, rc);
		goto error;
	}
	DEV_DBG("%s: id=%d\n", __func__, pdev->id);

	/* GPIO */
	for (i = 0; i < HDMI_TX_MAX_PM; i++) {
		rc = hdmi_tx_get_dt_gpio_data(&pdev->dev,
			&pdata->power_data[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' get_dt_gpio_data failed.rc=%d\n",
				__func__, hdmi_tx_pm_name(i), rc);
			goto error;
		}
	}

	/* VREG */
	for (i = 0; i < HDMI_TX_MAX_PM; i++) {
		rc = hdmi_tx_get_dt_vreg_data(&pdev->dev,
			&pdata->power_data[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' get_dt_vreg_data failed.rc=%d\n",
				__func__, hdmi_tx_pm_name(i), rc);
			goto error;
		}
	}

	/* CLK */
	rc = hdmi_tx_get_dt_clk_data(&pdev->dev, pdata);
	if (rc) {
		DEV_ERR("%s: get_dt_clk_data failed.rc=%d\n",
				__func__, rc);
		goto error;
	}

	if (!hdmi_ctrl->pdata.primary)
		hdmi_ctrl->pdata.primary = of_property_read_bool(
			pdev->dev.of_node, "qcom,primary_panel");

	pdata->cond_power_on = of_property_read_bool(pdev->dev.of_node,
		"qcom,conditional-power-on");

	pdata->pluggable = of_property_read_bool(pdev->dev.of_node,
		"qcom,pluggable");

	rc = of_property_read_u32(pdev->dev.of_node,
		"qcom,max-pclk-frequency-khz", &pdata->max_pclk_freq_khz);
	if (rc) {
		pdata->max_pclk_freq_khz = 0;
		rc = 0;
	}

	data = of_get_property(pdev->dev.of_node, "qcom,display-id", &len);
	if (!data || len <= 0)
		pr_err("%s:%d Unable to read qcom,display-id, data=%pK,len=%d\n",
			__func__, __LINE__, data, len);
	else
		snprintf(hdmi_ctrl->panel_data.panel_info.display_id,
			MDSS_DISPLAY_ID_MAX_LEN, "%s", data);

	return rc;

error:
	hdmi_tx_put_dt_data(&pdev->dev, pdata);
	return rc;
} /* hdmi_tx_get_dt_data */

static int hdmi_tx_init_event_handler(struct hdmi_tx_ctrl *hdmi_ctrl)
{
	hdmi_tx_evt_handler *handler;

	if (!hdmi_ctrl)
		return -EINVAL;

	handler = hdmi_ctrl->evt_handler;

	handler[MDSS_EVENT_FB_REGISTERED] = hdmi_tx_evt_handle_register;
	handler[MDSS_EVENT_CHECK_PARAMS]  = hdmi_tx_evt_handle_check_param;
	handler[MDSS_EVENT_RESUME]        = hdmi_tx_evt_handle_resume;
	handler[MDSS_EVENT_RESET]         = hdmi_tx_evt_handle_reset;
	handler[MDSS_EVENT_UNBLANK]       = hdmi_tx_evt_handle_unblank;
	handler[MDSS_EVENT_PANEL_ON]      = hdmi_tx_evt_handle_panel_on;
	handler[MDSS_EVENT_SUSPEND]       = hdmi_tx_evt_handle_suspend;
	handler[MDSS_EVENT_BLANK]         = hdmi_tx_evt_handle_blank;
	handler[MDSS_EVENT_PANEL_OFF]     = hdmi_tx_evt_handle_panel_off;
	handler[MDSS_EVENT_CLOSE]         = hdmi_tx_evt_handle_close;
	handler[MDSS_EVENT_DEEP_COLOR]    = hdmi_tx_evt_handle_deep_color;

	handler = hdmi_ctrl->pre_evt_handler;
	handler[MDSS_EVENT_PANEL_UPDATE_FPS] =
		hdmi_tx_pre_evt_handle_update_fps;
	handler[MDSS_EVENT_PANEL_OFF]     = hdmi_tx_pre_evt_handle_panel_off;

	handler = hdmi_ctrl->post_evt_handler;
	handler[MDSS_EVENT_UNBLANK]       = hdmi_tx_post_evt_handle_unblank;
	handler[MDSS_EVENT_RESUME]        = hdmi_tx_post_evt_handle_resume;
	handler[MDSS_EVENT_PANEL_ON]      = hdmi_tx_post_evt_handle_panel_on;

	return 0;
}

static int hdmi_tx_probe(struct platform_device *pdev)
{
	int rc = 0, i;
	struct device_node *of_node = pdev->dev.of_node;
	struct hdmi_tx_ctrl *hdmi_ctrl = NULL;
	struct mdss_panel_cfg *pan_cfg = NULL;

	if (!of_node) {
		DEV_ERR("%s: FAILED: of_node not found\n", __func__);
		rc = -ENODEV;
		return rc;
	}

	hdmi_ctrl = devm_kzalloc(&pdev->dev, sizeof(*hdmi_ctrl), GFP_KERNEL);
	if (!hdmi_ctrl) {
		DEV_ERR("%s: FAILED: cannot alloc hdmi tx ctrl\n", __func__);
		rc = -ENOMEM;
		goto failed_no_mem;
	}

	hdmi_ctrl->mdss_util = mdss_get_util_intf();
	if (hdmi_ctrl->mdss_util == NULL) {
		pr_err("Failed to get mdss utility functions\n");
		rc = -ENODEV;
		goto failed_dt_data;
	}

	platform_set_drvdata(pdev, hdmi_ctrl);
	hdmi_ctrl->pdev = pdev;
	hdmi_ctrl->enc_lvl = HDCP_STATE_AUTH_ENC_NONE;

	pan_cfg = mdss_panel_intf_type(MDSS_PANEL_INTF_HDMI);
	if (IS_ERR(pan_cfg)) {
		return PTR_ERR(pan_cfg);
	} else if (pan_cfg) {
		int vic;

		if (kstrtoint(pan_cfg->arg_cfg, 10, &vic) ||
			vic <= HDMI_VFRMT_UNKNOWN || vic >= HDMI_VFRMT_MAX)
			vic = DEFAULT_HDMI_PRIMARY_RESOLUTION;

		hdmi_ctrl->pdata.primary = true;
		hdmi_ctrl->vic = vic;
		hdmi_ctrl->panel_data.panel_info.is_prim_panel = true;
		hdmi_ctrl->panel_data.panel_info.cont_splash_enabled =
			hdmi_ctrl->mdss_util->panel_intf_status(DISPLAY_1,
					MDSS_PANEL_INTF_HDMI) ? true : false;
	}

	hdmi_tx_hw.irq_info = mdss_intr_line();
	if (hdmi_tx_hw.irq_info == NULL) {
		pr_err("Failed to get mdss irq information\n");
		return -ENODEV;
	}

	rc = hdmi_tx_get_dt_data(pdev, &hdmi_ctrl->pdata);
	if (rc) {
		DEV_ERR("%s: FAILED: parsing device tree data. rc=%d\n",
			__func__, rc);
		goto failed_dt_data;
	}
	rc = hdmi_tx_init_resource(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: FAILED: resource init. rc=%d\n",
			__func__, rc);
		goto failed_res_init;
	}

	rc = hdmi_tx_get_version(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: FAILED: hdmi_tx_get_version. rc=%d\n",
			__func__, rc);
		goto failed_res_init;
	}

	rc = hdmi_tx_dev_init(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: FAILED: hdmi_tx_dev_init. rc=%d\n", __func__, rc);
		goto failed_dev_init;
	}

	rc = hdmi_tx_init_event_handler(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: FAILED: hdmi_tx_init_event_handler. rc=%d\n",
			__func__, rc);
		goto failed_dev_init;
	}

	rc = hdmi_tx_register_panel(hdmi_ctrl);
	if (rc) {
		DEV_ERR("%s: FAILED: register_panel. rc=%d\n", __func__, rc);
		goto failed_reg_panel;
	}

	rc = of_platform_populate(of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		DEV_ERR("%s: Failed to add child devices. rc=%d\n",
			__func__, rc);
		goto failed_reg_panel;
	} else {
		DEV_DBG("%s: Add child devices.\n", __func__);
	}

	if (mdss_debug_register_io("hdmi",
		&hdmi_ctrl->pdata.io[HDMI_TX_CORE_IO], NULL))
		DEV_WARN("%s: hdmi_tx debugfs register failed\n", __func__);

	if (hdmi_ctrl->panel_data.panel_info.cont_splash_enabled) {
		for (i = 0; i < HDMI_TX_MAX_PM; i++) {
			msm_dss_enable_vreg(
				hdmi_ctrl->pdata.power_data[i].vreg_config,
				hdmi_ctrl->pdata.power_data[i].num_vreg, 1);

			hdmi_tx_pinctrl_set_state(hdmi_ctrl, i, 1);

			msm_dss_enable_gpio(
				hdmi_ctrl->pdata.power_data[i].gpio_config,
				hdmi_ctrl->pdata.power_data[i].num_gpio, 1);

			msm_dss_enable_clk(
				hdmi_ctrl->pdata.power_data[i].clk_config,
				hdmi_ctrl->pdata.power_data[i].num_clk, 1);

			hdmi_ctrl->power_data_enable[i] = true;
		}
	}

	return rc;

failed_reg_panel:
	hdmi_tx_dev_deinit(hdmi_ctrl);
failed_dev_init:
	hdmi_tx_deinit_resource(hdmi_ctrl);
failed_res_init:
	hdmi_tx_put_dt_data(&pdev->dev, &hdmi_ctrl->pdata);
failed_dt_data:
failed_no_mem:
	return rc;
} /* hdmi_tx_probe */

static int hdmi_tx_remove(struct platform_device *pdev)
{
	struct hdmi_tx_ctrl *hdmi_ctrl = platform_get_drvdata(pdev);

	if (!hdmi_ctrl) {
		DEV_ERR("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	hdmi_tx_sysfs_remove(hdmi_ctrl);
	hdmi_tx_dev_deinit(hdmi_ctrl);
	hdmi_tx_deinit_resource(hdmi_ctrl);
	hdmi_tx_put_dt_data(&pdev->dev, &hdmi_ctrl->pdata);

	return 0;
} /* hdmi_tx_remove */

static const struct of_device_id hdmi_tx_dt_match[] = {
	{.compatible = COMPATIBLE_NAME,},
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, hdmi_tx_dt_match);

static struct platform_driver this_driver = {
	.probe = hdmi_tx_probe,
	.remove = hdmi_tx_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = hdmi_tx_dt_match,
	},
};

static int __init hdmi_tx_drv_init(void)
{
	int rc;

	rc = platform_driver_register(&this_driver);
	if (rc)
		DEV_ERR("%s: FAILED: rc=%d\n", __func__, rc);

	return rc;
} /* hdmi_tx_drv_init */

static void __exit hdmi_tx_drv_exit(void)
{
	platform_driver_unregister(&this_driver);
} /* hdmi_tx_drv_exit */

module_init(hdmi_tx_drv_init);
module_exit(hdmi_tx_drv_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("HDMI MSM TX driver");
