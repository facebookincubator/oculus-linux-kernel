// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/extcon.h>
#include <linux/extcon-provider.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/usb/phy.h>
#include <linux/usb/dwc3-msm.h>
#include <linux/reset.h>
#include <linux/debugfs.h>
#include <linux/qcom_scm.h>
#include <linux/types.h>

#define USB2_PHY_USB_PHY_UTMI_CTRL0		(0x3c)
#define OPMODE_MASK				(0x3 << 3)
#define OPMODE_NONDRIVING			(0x1 << 3)
#define SLEEPM					BIT(0)

#define OPMODE_NORMAL				(0x00)
#define TERMSEL					BIT(5)

#define DCD_ENABLE				BIT(0)
#define CHG_SEL0				BIT(1)
#define VDAT_SRC_ENABLE				BIT(2)
#define VDAT_DET_ENABLE				BIT(3)
#define DM_PULLDOWN				BIT(3)

#define USB2PHY_USB_PHY_CHARGING_DET_OUTPUT	(0x24)
#define FSVPLUS0				BIT(6)
#define CHGDET0					BIT(5)

#define USB2_PHY_USB_PHY_UTMI_CTRL1		(0x40)
#define USB2_PHY_CHARGING_DET_CTRL		(0x7c)
#define XCVRSEL					BIT(0)

#define USB2_PHY_USB_PHY_UTMI_CTRL5		(0x50)
#define POR					BIT(1)

#define USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON0	(0x54)
#define RETENABLEN				BIT(3)
#define FSEL_MASK				(0x7 << 4)
#define FSEL_DEFAULT				(0x3 << 4)

#define USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON1	(0x58)
#define VBUSVLDEXTSEL0				BIT(4)
#define PLLBTUNE				BIT(5)

#define USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON2	(0x5c)
#define VREGBYPASS				BIT(0)

#define USB2_PHY_USB_PHY_HS_PHY_CTRL1		(0x60)
#define VBUSVLDEXT0				BIT(0)

#define USB2_PHY_USB_PHY_HS_PHY_CTRL2		(0x64)
#define USB2_AUTO_RESUME			BIT(0)
#define USB2_SUSPEND_N				BIT(2)
#define USB2_SUSPEND_N_SEL			BIT(3)

#define USB2_PHY_USB_PHY_CFG0			(0x94)
#define UTMI_PHY_DATAPATH_CTRL_OVERRIDE_EN	BIT(0)
#define UTMI_PHY_CMN_CTRL_OVERRIDE_EN		BIT(1)

#define USB2_PHY_USB_PHY_REFCLK_CTRL		(0xa0)
#define REFCLK_SEL_MASK				(0x3 << 0)
#define REFCLK_SEL_DEFAULT			(0x2 << 0)

#define USB2PHY_USB_PHY_RTUNE_SEL		(0xb4)
#define RTUNE_SEL				BIT(0)

#define TXPREEMPAMPTUNE0(x)			(x << 6)
#define TXPREEMPAMPTUNE0_MASK			(BIT(7) | BIT(6))
#define USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X0	0x6c
#define USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X1	0x70
#define USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X2	0x74
#define USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X3	0x78
#define TXVREFTUNE0_MASK			0xF
#define PARAM_OVRD_MASK			0xFF

#define USB2_PHY_USB_PHY_PWRDOWN_CTRL		(0xa4)
#define PWRDOWN_B				BIT(0)

#define DPSE_INTR_HIGH			BIT(0)

#define USB_HSPHY_3P3_VOL_MIN			3050000 /* uV */
#define USB_HSPHY_3P3_VOL_MAX			3300000 /* uV */
#define USB_HSPHY_3P3_HPM_LOAD			16000	/* uA */
#define USB_HSPHY_3P3_VOL_FSHOST		3150000 /* uV */

#define USB_HSPHY_1P8_VOL_MIN			1704000 /* uV */
#define USB_HSPHY_1P8_VOL_MAX			1800000 /* uV */
#define USB_HSPHY_1P8_HPM_LOAD			19000	/* uA */

#define USB2PHY_REFGEN_HPM_LOAD			1200000  /* uA */
#define USB_HSPHY_VDD_HPM_LOAD			30000	/* uA */

enum port_state {
	PORT_UNKNOWN,
	PORT_DISCONNECTED,
	PORT_DCD_IN_PROGRESS,
	PORT_PRIMARY_IN_PROGRESS,
	PORT_SECONDARY_IN_PROGRESS,
	PORT_CHG_DET_DONE,
	PORT_HOST_MODE,
};

enum chg_det_state {
	STATE_UNKNOWN,
	STATE_DCD,
	STATE_PRIMARY,
	STATE_SECONDARY,
};

struct msm_hsphy {
	struct usb_phy		phy;
	void __iomem		*base;
	phys_addr_t		eud_reg;
	void __iomem		*eud_enable_reg;
	bool			re_enable_eud;

	struct clk		*ref_clk_src;
	struct clk		*cfg_ahb_clk;
	struct clk		*ref_clk;
	struct reset_control	*phy_reset;

	struct regulator	*vdd;
	struct regulator	*vdda33;
	struct regulator	*vdda18;
	struct regulator        *refgen;
	int			vdd_levels[3]; /* none, low, high */
	int			refgen_levels[3]; /* 0, REFGEN_VOL_MIN, REFGEN_VOL_MAX */
	int			vdda18_max_uA;

	bool			clocks_enabled;
	bool			power_enabled;
	bool			suspended;
	bool			cable_connected;
	bool			dpdm_enable;

	int			*param_override_seq;
	int			param_override_seq_cnt;

	void __iomem		*phy_rcal_reg;
	u32			rcal_mask;

	struct mutex		phy_lock;
	struct regulator_desc	dpdm_rdesc;
	struct regulator_dev	*dpdm_rdev;

	struct power_supply	*usb_psy;
	unsigned int		vbus_draw;
	struct work_struct	vbus_draw_work;
	struct extcon_dev	*usb_extcon;
	bool			vbus_active;
	bool			id_state;
	struct delayed_work	port_det_w;
	enum port_state		port_state;
	unsigned int		dcd_timeout;

	/* debugfs entries */
	struct dentry		*root;
	u8			txvref_tune0;
	u8			pre_emphasis;
	u8			param_ovrd0;
	u8			param_ovrd1;
	u8			param_ovrd2;
	u8			param_ovrd3;
};

static void msm_hsphy_enable_clocks(struct msm_hsphy *phy, bool on)
{
	dev_dbg(phy->phy.dev, "%s(): clocks_enabled:%d on:%d\n",
			__func__, phy->clocks_enabled, on);

	if (!phy->clocks_enabled && on) {
		clk_prepare_enable(phy->ref_clk_src);

		if (phy->ref_clk)
			clk_prepare_enable(phy->ref_clk);

		if (phy->cfg_ahb_clk)
			clk_prepare_enable(phy->cfg_ahb_clk);

		phy->clocks_enabled = true;
	}

	if (phy->clocks_enabled && !on) {

		if (phy->ref_clk)
			clk_disable_unprepare(phy->ref_clk);

		if (phy->cfg_ahb_clk)
			clk_disable_unprepare(phy->cfg_ahb_clk);

		clk_disable_unprepare(phy->ref_clk_src);
		phy->clocks_enabled = false;
	}

}

static int msm_hsphy_enable_power(struct msm_hsphy *phy, bool on)
{
	int ret = 0;

	dev_dbg(phy->phy.dev, "%s turn %s regulators. power_enabled:%d\n",
			__func__, on ? "on" : "off", phy->power_enabled);

	if (phy->power_enabled == on) {
		dev_dbg(phy->phy.dev, "PHYs' regulators are already ON.\n");
		return 0;
	}

	if (!on) {
		if (phy->refgen)
			goto disable_refgen;
		else
			goto disable_vdda33;
	}

	ret = regulator_set_load(phy->vdd, USB_HSPHY_VDD_HPM_LOAD);
	if (ret < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdd:%d\n", ret);
		goto err_vdd;
	}

	ret = regulator_set_voltage(phy->vdd, phy->vdd_levels[1],
				    phy->vdd_levels[2]);
	if (ret) {
		dev_err(phy->phy.dev, "unable to set voltage for hsusb vdd\n");
		goto put_vdd_lpm;
	}

	ret = regulator_enable(phy->vdd);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to enable VDD\n");
		goto unconfig_vdd;
	}

	ret = regulator_set_load(phy->vdda18, phy->vdda18_max_uA);
	if (ret < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdda18:%d\n", ret);
		goto disable_vdd;
	}

	ret = regulator_set_voltage(phy->vdda18, USB_HSPHY_1P8_VOL_MIN,
						USB_HSPHY_1P8_VOL_MAX);
	if (ret) {
		dev_err(phy->phy.dev,
				"Unable to set voltage for vdda18:%d\n", ret);
		goto put_vdda18_lpm;
	}

	ret = regulator_enable(phy->vdda18);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to enable vdda18:%d\n", ret);
		goto unset_vdda18;
	}

	ret = regulator_set_load(phy->vdda33, USB_HSPHY_3P3_HPM_LOAD);
	if (ret < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdda33:%d\n", ret);
		goto disable_vdda18;
	}

	ret = regulator_set_voltage(phy->vdda33, USB_HSPHY_3P3_VOL_MIN,
						USB_HSPHY_3P3_VOL_MAX);
	if (ret) {
		dev_err(phy->phy.dev,
				"Unable to set voltage for vdda33:%d\n", ret);
		goto put_vdda33_lpm;
	}

	ret = regulator_enable(phy->vdda33);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to enable vdda33:%d\n", ret);
		goto unset_vdd33;
	}

	if (phy->refgen) {
		ret = regulator_set_load(phy->refgen, USB2PHY_REFGEN_HPM_LOAD);
		if (ret < 0) {
			dev_err(phy->phy.dev, "Unable to set HPM of refgen:%d\n", ret);
			goto disable_vdda33;
		}

		ret = regulator_set_voltage(phy->refgen, phy->refgen_levels[1],
						phy->refgen_levels[2]);
		if (ret) {
			dev_err(phy->phy.dev,
					"Unable to set voltage for refgen:%d\n", ret);
			goto put_refgen_lpm;
		}

		ret = regulator_enable(phy->refgen);
		if (ret) {
			dev_err(phy->phy.dev, "Unable to enable refgen:%d\n", ret);
			goto unset_refgen;
		}
	}

	phy->power_enabled = true;

	pr_debug("%s(): HSUSB PHY's regulators are turned ON.\n", __func__);
	return ret;

disable_refgen:
	ret = regulator_disable(phy->refgen);
	if (ret)
		dev_err(phy->phy.dev, "Unable to disable refgen:%d\n", ret);

unset_refgen:
	ret = regulator_set_voltage(phy->refgen, phy->refgen_levels[0], phy->refgen_levels[2]);
	if (ret)
		dev_err(phy->phy.dev,
				"Unable to set (0) voltage for refgen:%d\n", ret);

put_refgen_lpm:
	ret = regulator_set_load(phy->refgen, 0);
	if (ret < 0)
		dev_err(phy->phy.dev, "Unable to set (0) HPM of refgen\n");

disable_vdda33:
	ret = regulator_disable(phy->vdda33);
	if (ret)
		dev_err(phy->phy.dev, "Unable to disable vdda33:%d\n", ret);

unset_vdd33:
	ret = regulator_set_voltage(phy->vdda33, 0, USB_HSPHY_3P3_VOL_MAX);
	if (ret)
		dev_err(phy->phy.dev,
			"Unable to set (0) voltage for vdda33:%d\n", ret);

put_vdda33_lpm:
	ret = regulator_set_load(phy->vdda33, 0);
	if (ret < 0)
		dev_err(phy->phy.dev, "Unable to set (0) HPM of vdda33\n");

disable_vdda18:
	ret = regulator_disable(phy->vdda18);
	if (ret)
		dev_err(phy->phy.dev, "Unable to disable vdda18:%d\n", ret);

unset_vdda18:
	ret = regulator_set_voltage(phy->vdda18, 0, USB_HSPHY_1P8_VOL_MAX);
	if (ret)
		dev_err(phy->phy.dev,
			"Unable to set (0) voltage for vdda18:%d\n", ret);

put_vdda18_lpm:
	ret = regulator_set_load(phy->vdda18, 0);
	if (ret < 0)
		dev_err(phy->phy.dev, "Unable to set LPM of vdda18\n");

disable_vdd:
	ret = regulator_disable(phy->vdd);
	if (ret)
		dev_err(phy->phy.dev, "Unable to disable vdd:%d\n", ret);

unconfig_vdd:
	ret = regulator_set_voltage(phy->vdd, phy->vdd_levels[0],
				    phy->vdd_levels[2]);
	if (ret)
		dev_err(phy->phy.dev, "unable to set voltage for hsusb vdd\n");

put_vdd_lpm:
	ret = regulator_set_load(phy->vdd, 0);
	if (ret < 0)
		dev_err(phy->phy.dev, "Unable to set LPM of vdd\n");
	/*
	 * Return from here based on power_enabled. If it is not set
	 * then return -EINVAL since either set_voltage or
	 * regulator_enable failed
	 */
	if (!phy->power_enabled)
		return -EINVAL;
err_vdd:
	phy->power_enabled = false;
	dev_dbg(phy->phy.dev, "HSUSB PHY's regulators are turned OFF.\n");
	return ret;
}

static void msm_usb_write_readback(void __iomem *base, u32 offset,
					const u32 mask, u32 val)
{
	u32 write_val, tmp = readl_relaxed(base + offset);

	tmp &= ~mask;		/* retain other bits */
	write_val = tmp | val;

	writel_relaxed(write_val, base + offset);

	/* Read back to see if val was written */
	tmp = readl_relaxed(base + offset);
	tmp &= mask;		/* clear other bits */

	if (tmp != val)
		pr_err("%s: write: %x to QSCRATCH: %x FAILED\n",
			__func__, val, offset);
}

static void msm_hsphy_reset(struct msm_hsphy *phy)
{
	int ret;

	ret = reset_control_assert(phy->phy_reset);
	if (ret)
		dev_err(phy->phy.dev, "%s: phy_reset assert failed\n",
								__func__);
	usleep_range(100, 150);

	ret = reset_control_deassert(phy->phy_reset);
	if (ret)
		dev_err(phy->phy.dev, "%s: phy_reset deassert failed\n",
							__func__);
}

static void hsusb_phy_write_seq(void __iomem *base, u32 *seq, int cnt,
		unsigned long delay)
{
	int i;

	pr_debug("Seq count:%d\n", cnt);
	for (i = 0; i < cnt; i = i+2) {
		pr_debug("write 0x%02x to 0x%02x\n", seq[i], seq[i+1]);
		writel_relaxed(seq[i], base + seq[i+1]);
		if (delay)
			usleep_range(delay, (delay + 2000));
	}
}

#define EUD_EN2 BIT(0)
static int msm_hsphy_init(struct usb_phy *uphy)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);
	int ret;
	u32 rcal_code = 0, eud_csr_reg = 0;

	dev_dbg(uphy->dev, "%s phy_flags:0x%x\n", __func__, phy->phy.flags);
	if (phy->eud_enable_reg) {
		eud_csr_reg = readl_relaxed(phy->eud_enable_reg);
		if (eud_csr_reg & EUD_EN2) {
			dev_dbg(phy->phy.dev, "csr:0x%x eud is enabled\n",
							eud_csr_reg);
			/* if in host mode, disable EUD */
			if (phy->phy.flags & PHY_HOST_MODE) {
				qcom_scm_io_writel(phy->eud_reg, 0x0);
				phy->re_enable_eud = true;
			} else {
				msm_hsphy_enable_clocks(phy, true);
				ret = msm_hsphy_enable_power(phy, true);
				/* On some targets 3.3V LDO which acts as EUD power
				 * up (which in turn reset the USB PHY) is shared
				 * with EMMC so that it won't be turned off even
				 * though we remove our vote as part of disconnect
				 * so power up this regulator is actually not
				 * resetting the PHY next time when cable is
				 * connected. So we explicitly bring
				 * it out of power down state by writing
				 * to POWER DOWN register,powering on the EUD
				 * will bring EUD as well as phy out of reset state.
				 */
				msm_usb_write_readback(phy->base,
					USB2_PHY_USB_PHY_PWRDOWN_CTRL, PWRDOWN_B, 1);
				return ret;
			}
		}
	}

	ret = msm_hsphy_enable_power(phy, true);
	if (ret)
		return ret;

	msm_hsphy_enable_clocks(phy, true);

	msm_hsphy_reset(phy);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_CFG0,
				UTMI_PHY_CMN_CTRL_OVERRIDE_EN,
				UTMI_PHY_CMN_CTRL_OVERRIDE_EN);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL5,
				POR, POR);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON0,
				FSEL_MASK, 0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON1,
				PLLBTUNE, PLLBTUNE);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_REFCLK_CTRL,
				REFCLK_SEL_MASK, REFCLK_SEL_DEFAULT);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON1,
				VBUSVLDEXTSEL0, VBUSVLDEXTSEL0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL1,
				VBUSVLDEXT0, VBUSVLDEXT0);

	/* set parameter ovrride  if needed */
	if (phy->param_override_seq)
		hsusb_phy_write_seq(phy->base, phy->param_override_seq,
				phy->param_override_seq_cnt, 0);

	if (phy->pre_emphasis) {
		u8 val = TXPREEMPAMPTUNE0(phy->pre_emphasis) &
				TXPREEMPAMPTUNE0_MASK;
		if (val)
			msm_usb_write_readback(phy->base,
				USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X1,
				TXPREEMPAMPTUNE0_MASK, val);
	}

	if (phy->txvref_tune0) {
		u8 val = phy->txvref_tune0 & TXVREFTUNE0_MASK;

		msm_usb_write_readback(phy->base,
			USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X1,
			TXVREFTUNE0_MASK, val);
	}

	if (phy->param_ovrd0) {
		msm_usb_write_readback(phy->base,
			USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X0,
			PARAM_OVRD_MASK, phy->param_ovrd0);
	}

	if (phy->param_ovrd1) {
		msm_usb_write_readback(phy->base,
			USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X1,
			PARAM_OVRD_MASK, phy->param_ovrd1);
	}

	if (phy->param_ovrd2) {
		msm_usb_write_readback(phy->base,
			USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X2,
			PARAM_OVRD_MASK, phy->param_ovrd2);
	}

	if (phy->param_ovrd3) {
		msm_usb_write_readback(phy->base,
			USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X3,
			PARAM_OVRD_MASK, phy->param_ovrd3);
	}

	dev_dbg(uphy->dev, "x0:%08x x1:%08x x2:%08x x3:%08x\n",
	readl_relaxed(phy->base + USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X0),
	readl_relaxed(phy->base + USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X1),
	readl_relaxed(phy->base + USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X2),
	readl_relaxed(phy->base + USB2PHY_USB_PHY_PARAMETER_OVERRIDE_X3));

	if (phy->phy_rcal_reg) {
		rcal_code = readl_relaxed(phy->phy_rcal_reg) & phy->rcal_mask;

		dev_dbg(uphy->dev, "rcal_mask:%08x reg:%pK code:%08x\n",
				phy->rcal_mask, phy->phy_rcal_reg, rcal_code);
	}

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON2,
				VREGBYPASS, VREGBYPASS);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL2,
				USB2_SUSPEND_N_SEL | USB2_SUSPEND_N,
				USB2_SUSPEND_N_SEL | USB2_SUSPEND_N);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL0,
				SLEEPM, SLEEPM);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL5,
				POR, 0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL2,
				USB2_SUSPEND_N_SEL, 0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_CFG0,
				UTMI_PHY_CMN_CTRL_OVERRIDE_EN, 0);

	return 0;
}

static int msm_hsphy_set_suspend(struct usb_phy *uphy, int suspend)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);

	if (phy->suspended && suspend) {
		if (phy->phy.flags & PHY_SUS_OVERRIDE)
			goto suspend;

		dev_dbg(uphy->dev, "%s: USB PHY is already suspended\n",
								__func__);
		return 0;
	}

suspend:
	if (suspend) { /* Bus suspend */
		/*
		 * The HUB class drivers calls usb_phy_notify_disconnect() upon a device
		 * disconnect. Consider a scenario where a USB device is disconnected without
		 * detaching the OTG cable. phy->cable_connected is marked false due to above
		 * mentioned call path. Now, while entering low power mode (host bus suspend),
		 * we come here and turn off regulators thinking no cable is connected. Prevent
		 * this by not turning off regulators while in host mode.
		 */
		if (phy->cable_connected || (phy->phy.flags & PHY_HOST_MODE)) {
			/* Enable auto-resume functionality during host mode
			 * bus suspend with some FS/HS peripheral connected.
			 */
			if ((phy->phy.flags & PHY_HOST_MODE) &&
				(phy->phy.flags & PHY_HSFS_MODE)) {
				/* Enable auto-resume functionality by pulsing
				 * signal
				 */
				msm_usb_write_readback(phy->base,
					USB2_PHY_USB_PHY_HS_PHY_CTRL2,
					USB2_AUTO_RESUME, USB2_AUTO_RESUME);
				usleep_range(500, 1000);
				msm_usb_write_readback(phy->base,
					USB2_PHY_USB_PHY_HS_PHY_CTRL2,
					USB2_AUTO_RESUME, 0);
			}
			msm_hsphy_enable_clocks(phy, false);
		} else {/* Cable disconnect */
			mutex_lock(&phy->phy_lock);
			dev_dbg(uphy->dev, "phy->flags:0x%x\n", phy->phy.flags);
			if (phy->re_enable_eud) {
				dev_dbg(uphy->dev, "re-enabling EUD\n");
				qcom_scm_io_writel(phy->eud_reg, 0x1);
				phy->re_enable_eud = false;
			}

			if (!phy->dpdm_enable) {
				if (!(phy->phy.flags & EUD_SPOOF_DISCONNECT)) {
					dev_dbg(uphy->dev, "turning off clocks/ldo\n");
					msm_hsphy_enable_clocks(phy, false);
					msm_hsphy_enable_power(phy, false);
				}
			} else {
				dev_dbg(uphy->dev, "dpdm reg still active.  Keep clocks/ldo ON\n");
			}
			mutex_unlock(&phy->phy_lock);
		}
		phy->suspended = true;
	} else { /* Bus resume and cable connect */
		msm_hsphy_enable_clocks(phy, true);
		phy->suspended = false;
	}

	return 0;
}

static int msm_hsphy_notify_connect(struct usb_phy *uphy,
				    enum usb_device_speed speed)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);

	phy->cable_connected = true;

	return 0;
}

static int msm_hsphy_notify_disconnect(struct usb_phy *uphy,
				       enum usb_device_speed speed)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);

	phy->cable_connected = false;

	return 0;
}

static void msm_hsphy_vbus_draw_work(struct work_struct *w)
{
	struct msm_hsphy *phy = container_of(w, struct msm_hsphy,
			vbus_draw_work);
	union power_supply_propval val = {0};
	int ret;

	if (!phy->usb_psy) {
		phy->usb_psy = power_supply_get_by_name("usb");
		if (!phy->usb_psy) {
			dev_err(phy->phy.dev, "Could not get usb psy\n");
			return;
		}
	}

	dev_info(phy->phy.dev, "Avail curr from USB = %u\n", phy->vbus_draw);

	/* Set max current limit in uA */
	val.intval = 1000 * phy->vbus_draw;
	ret = power_supply_set_property(phy->usb_psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	if (ret) {
		dev_dbg(phy->phy.dev, "Error (%d) setting input current limit\n", ret);
		return;
	}
}

static int msm_hsphy_set_power(struct usb_phy *uphy, unsigned int mA)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);

	if (phy->cable_connected && (mA == 0))
		return 0;

	phy->vbus_draw = mA;
	schedule_work(&phy->vbus_draw_work);

	return 0;
}

static void msm_hsphy_put_phy_in_non_driving_mode(struct msm_hsphy *phy,
							int state)
{
	if (state) {
		/* set utmi_phy_cmn_cntrl_override_en &
		 * utmi_phy_datapath_ctrl_override_en
		 */
		msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL2,
					USB2_SUSPEND_N_SEL | USB2_SUSPEND_N,
					USB2_SUSPEND_N_SEL | USB2_SUSPEND_N);
		msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL0,
					OPMODE_MASK, OPMODE_NONDRIVING);
		msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_CFG0,
					UTMI_PHY_DATAPATH_CTRL_OVERRIDE_EN,
					UTMI_PHY_DATAPATH_CTRL_OVERRIDE_EN);
	} else {
		/* clear utmi_phy_cmn_cntrl_override_en &
		 * utmi_phy_datapath_ctrl_override_en
		 */
		msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_CFG0,
					UTMI_PHY_CMN_CTRL_OVERRIDE_EN, 0x00);
		msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_CFG0,
					UTMI_PHY_DATAPATH_CTRL_OVERRIDE_EN, 0x00);
	}
}

#define DP_PULSE_WIDTH_MSEC 200
static enum usb_charger_type usb_phy_drive_dp_pulse(struct usb_phy *uphy)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);
	int ret;

	ret = msm_hsphy_enable_power(phy, true);
	if (ret < 0) {
		dev_dbg(phy->phy.dev,
			"dpdm regulator enable failed:%d\n", ret);
		return 0;
	}
	msm_hsphy_enable_clocks(phy, true);
	msm_hsphy_put_phy_in_non_driving_mode(phy, 1);

	/* set opmode to normal i.e. 0x0 & termsel to fs */
	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL0,
				OPMODE_MASK, OPMODE_NORMAL);
	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL0,
				TERMSEL, TERMSEL);
	/* set xcvrsel to fs */
	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL1,
					XCVRSEL, XCVRSEL);

	msleep(DP_PULSE_WIDTH_MSEC);

	/* clear termsel to fs */
	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL0,
				TERMSEL, 0x00);
	/* clear xcvrsel */
	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL1,
					XCVRSEL, 0x00);
	msm_hsphy_put_phy_in_non_driving_mode(phy, 0);

	msleep(20);
	msm_hsphy_enable_clocks(phy, false);
	ret = msm_hsphy_enable_power(phy, false);
	if (ret < 0) {
		dev_dbg(phy->phy.dev,
			"dpdm regulator disable failed:%d\n", ret);
	}

	return 0;
}

static int msm_hsphy_dpdm_regulator_enable(struct regulator_dev *rdev)
{
	int ret = 0;
	struct msm_hsphy *phy = rdev_get_drvdata(rdev);

	dev_dbg(phy->phy.dev, "%s dpdm_enable:%d\n",
				__func__, phy->dpdm_enable);

	if (phy->eud_enable_reg && readl_relaxed(phy->eud_enable_reg)) {
		dev_err(phy->phy.dev, "eud is enabled\n");
		phy->dpdm_enable = true;
		return 0;
	}

	mutex_lock(&phy->phy_lock);
	if (!phy->dpdm_enable) {
		ret = msm_hsphy_enable_power(phy, true);
		if (ret) {
			mutex_unlock(&phy->phy_lock);
			return ret;
		}

		msm_hsphy_enable_clocks(phy, true);

		msm_hsphy_reset(phy);

		/*
		 * For PMIC charger detection, place PHY in UTMI non-driving
		 * mode which leaves Dp and Dm lines in high-Z state.
		 */
		msm_hsphy_put_phy_in_non_driving_mode(phy, 1);

		phy->dpdm_enable = true;
	}
	mutex_unlock(&phy->phy_lock);

	return ret;
}

static int msm_hsphy_dpdm_regulator_disable(struct regulator_dev *rdev)
{
	int ret = 0, val = 0;
	struct msm_hsphy *phy = rdev_get_drvdata(rdev);

	dev_dbg(phy->phy.dev, "%s dpdm_enable:%d\n",
				__func__, phy->dpdm_enable);

	if (phy->eud_enable_reg) {
		val = readl_relaxed(phy->eud_enable_reg);
		if (val & EUD_EN2) {
			dev_err(phy->phy.dev, "eud is enabled\n");
			phy->dpdm_enable = false;
			return 0;
		}
	}

	mutex_lock(&phy->phy_lock);
	if (phy->dpdm_enable) {
		if (!phy->cable_connected) {
			msm_hsphy_enable_clocks(phy, false);
			ret = msm_hsphy_enable_power(phy, false);
			if (ret < 0) {
				mutex_unlock(&phy->phy_lock);
				return ret;
			}
		}
		phy->dpdm_enable = false;
	}
	mutex_unlock(&phy->phy_lock);

	return ret;
}

static int msm_hsphy_dpdm_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct msm_hsphy *phy = rdev_get_drvdata(rdev);

	dev_dbg(phy->phy.dev, "%s dpdm_enable:%d\n",
			__func__, phy->dpdm_enable);

	return phy->dpdm_enable;
}

static struct regulator_ops msm_hsphy_dpdm_regulator_ops = {
	.enable		= msm_hsphy_dpdm_regulator_enable,
	.disable	= msm_hsphy_dpdm_regulator_disable,
	.is_enabled	= msm_hsphy_dpdm_regulator_is_enabled,
};

static int msm_hsphy_regulator_init(struct msm_hsphy *phy)
{
	struct device *dev = phy->phy.dev;
	struct regulator_config cfg = {};
	struct regulator_init_data *init_data;

	init_data = devm_kzalloc(dev, sizeof(*init_data), GFP_KERNEL);
	if (!init_data)
		return -ENOMEM;

	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS;
	phy->dpdm_rdesc.owner = THIS_MODULE;
	phy->dpdm_rdesc.type = REGULATOR_VOLTAGE;
	phy->dpdm_rdesc.ops = &msm_hsphy_dpdm_regulator_ops;
	phy->dpdm_rdesc.name = kbasename(dev->of_node->full_name);

	cfg.dev = dev;
	cfg.init_data = init_data;
	cfg.driver_data = phy;
	cfg.of_node = dev->of_node;

	phy->dpdm_rdev = devm_regulator_register(dev, &phy->dpdm_rdesc, &cfg);
	return PTR_ERR_OR_ZERO(phy->dpdm_rdev);
}

static int msm_hsphy_vbus_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct usb_phy *usb_phy = container_of(nb, struct usb_phy, vbus_nb);
	struct msm_hsphy *phy = container_of(usb_phy, struct msm_hsphy, phy);

	if (!phy || !data) {
		pr_err("Failed to get PHY for vbus_notifier\n");
		return NOTIFY_DONE;
	}

	phy->vbus_active = !!event;
	dev_dbg(phy->phy.dev, "Got VBUS notification: %u\n", event);
	queue_delayed_work(system_freezable_wq, &phy->port_det_w, 0);

	return NOTIFY_DONE;
}

static void msm_hsphy_create_debugfs(struct msm_hsphy *phy)
{
	phy->root = debugfs_create_dir(dev_name(phy->phy.dev), NULL);
	debugfs_create_x8("pre_emphasis", 0644, phy->root, &phy->pre_emphasis);
	debugfs_create_x8("txvref_tune0", 0644, phy->root, &phy->txvref_tune0);
	debugfs_create_x8("param_ovrd0", 0644, phy->root, &phy->param_ovrd0);
	debugfs_create_x8("param_ovrd1", 0644, phy->root, &phy->param_ovrd1);
	debugfs_create_x8("param_ovrd2", 0644, phy->root, &phy->param_ovrd2);
	debugfs_create_x8("param_ovrd3", 0644, phy->root, &phy->param_ovrd3);
}

static int msm_hsphy_id_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct usb_phy *usb_phy = container_of(nb, struct usb_phy, vbus_nb);
	struct msm_hsphy *phy = container_of(usb_phy, struct msm_hsphy, phy);

	if (!phy || !data) {
		pr_err("Failed to get PHY for vbus_notifier\n");
		return NOTIFY_DONE;
	}

	phy->id_state = !event;
	dev_dbg(phy->phy.dev, "Got id notification: %u\n", event);
	queue_delayed_work(system_freezable_wq, &phy->port_det_w, 0);

	return NOTIFY_DONE;
}

static const unsigned int msm_hsphy_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static int msm_hsphy_notify_charger(struct msm_hsphy *phy,
					enum power_supply_type charger_type)
{
	union power_supply_propval pval = {0};

	dev_dbg(phy->phy.dev, "Notify charger type: %d\n", charger_type);

	if (!phy->usb_psy) {
		phy->usb_psy = power_supply_get_by_name("usb");
		if (!phy->usb_psy) {
			dev_err(phy->phy.dev, "Could not get usb psy\n");
			return -ENODEV;
		}
	}

	pval.intval = charger_type;
	power_supply_set_property(phy->usb_psy, POWER_SUPPLY_PROP_USB_TYPE,
									&pval);
	return 0;
}

static void msm_hsphy_notify_extcon(struct msm_hsphy *phy,
						int extcon_id, int event)
{
	struct extcon_dev *edev = phy->phy.edev;
	union extcon_property_value val;
	int ret;

	dev_dbg(phy->phy.dev, "Notify event: %d for extcon_id: %d\n",
					event, extcon_id);

	if (event) {
		ret = extcon_get_property(edev, extcon_id,
					EXTCON_PROP_USB_TYPEC_POLARITY, &val);
		if (ret)
			dev_err(phy->phy.dev, "Failed to get TYPEC POLARITY\n");
		else
			extcon_set_property(phy->usb_extcon, extcon_id,
					EXTCON_PROP_USB_TYPEC_POLARITY, val);

		ret = extcon_get_property(edev, extcon_id,
						EXTCON_PROP_USB_SS, &val);
		if (ret)
			dev_err(phy->phy.dev, "Failed to get USB_SS property\n");
		else
			extcon_set_property(phy->usb_extcon, extcon_id,
						EXTCON_PROP_USB_SS, val);
	}

	extcon_set_state_sync(phy->usb_extcon, extcon_id, event);
}

static bool msm_hsphy_chg_det_status(struct msm_hsphy *phy,
						enum chg_det_state state)
{
	u32 reg, status = false;

	reg = readl_relaxed(phy->base
				+ USB2PHY_USB_PHY_CHARGING_DET_OUTPUT);
	dev_dbg(phy->phy.dev, "state: %d reg: 0x%x\n", state, reg);

	switch (state) {
	case STATE_DCD:
		status = reg & FSVPLUS0;
		break;
	case STATE_PRIMARY:
		status = reg & CHGDET0;
		break;
	case STATE_SECONDARY:
		status = reg & CHGDET0;
		break;
	case STATE_UNKNOWN:
	default:
		break;
	}

	return status;
}

/*
 * Different circuit blocks are enabled on DP and DM lines as part
 * of different phases of charger detection. Then the state of
 * DP and DM lines are monitored to identify different type of
 * chargers.
 * These circuit blocks can be enabled with the configuration of
 * the CHARGING_DET_CTRL register and the DP/DM lines can be
 * monitored with the status of the CHARGING_DET_OUTPUT register.
 */
static void msm_hsphy_chg_det_enable_seq(struct msm_hsphy *phy, int state)
{
	dev_dbg(phy->phy.dev, "state: %d\n", state);

	switch (state) {
	case STATE_DCD:
		msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL1,
						DM_PULLDOWN, DM_PULLDOWN);
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						DCD_ENABLE, DCD_ENABLE);
		break;
	case STATE_PRIMARY:
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						CHG_SEL0, 0);
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						VDAT_SRC_ENABLE, VDAT_SRC_ENABLE);
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						VDAT_DET_ENABLE, VDAT_DET_ENABLE);
		break;
	case STATE_SECONDARY:
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						CHG_SEL0, CHG_SEL0);
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						VDAT_SRC_ENABLE, VDAT_SRC_ENABLE);
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						VDAT_DET_ENABLE, VDAT_DET_ENABLE);
		break;
	case STATE_UNKNOWN:
	default:
		break;
	}
}

static void msm_hsphy_chg_det_disable_seq(struct msm_hsphy *phy, int state)
{
	dev_dbg(phy->phy.dev, "state: %d\n", state);

	switch (state) {
	case STATE_DCD:
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						DCD_ENABLE, 0);
		msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL1,
						DM_PULLDOWN, 0);
		/* Delay 10ms for DCD circuit to turn off */
		usleep_range(10000, 11000);
		break;
	case STATE_PRIMARY:
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						VDAT_SRC_ENABLE, 0);
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						VDAT_DET_ENABLE, 0);
		break;
	case STATE_SECONDARY:
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						CHG_SEL0, 0);
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						VDAT_SRC_ENABLE, 0);
		msm_usb_write_readback(phy->base, USB2_PHY_CHARGING_DET_CTRL,
						VDAT_DET_ENABLE, 0);
		break;
	case STATE_UNKNOWN:
	default:
		break;
	}
}

#define CHG_DCD_TIMEOUT_MSEC		750
#define CHG_DCD_POLL_TIME_MSEC		50

/* Wait 50ms per BC 1.2 TVDPSRC_ON until output reads 1 */
#define CHG_PRIMARY_DET_TIME_MSEC	50
#define CHG_SECONDARY_DET_TIME_MSEC	50

static int msm_hsphy_prepare_chg_det(struct msm_hsphy *phy)
{
	int ret;

	/*
	 * Set dpdm_enable to indicate charger detection
	 * is in progress. This also prevents the core
	 * driver from doing the set_suspend and init
	 * calls of the PHY which inteferes with the charger
	 * detection during bootup.
	 */
	phy->dpdm_enable = true;
	ret = msm_hsphy_enable_power(phy, true);
	if (ret)
		return ret;

	msm_hsphy_enable_clocks(phy, true);
	msm_hsphy_reset(phy);

	msm_hsphy_put_phy_in_non_driving_mode(phy, 1);
	return 0;
}

static void msm_hsphy_unprepare_chg_det(struct msm_hsphy *phy)
{
	int ret;

	ret = reset_control_assert(phy->phy_reset);
	if (ret)
		dev_err(phy->phy.dev, "phyassert failed\n");

	usleep_range(100, 150);

	ret = reset_control_deassert(phy->phy_reset);
	if (ret)
		dev_err(phy->phy.dev, "deassert failed\n");

	msm_hsphy_enable_clocks(phy, false);
	msm_hsphy_enable_power(phy, false);

	phy->dpdm_enable = false;
}

static void msm_hsphy_port_state_work(struct work_struct *w)
{
	struct msm_hsphy *phy = container_of(w, struct msm_hsphy,
							port_det_w.work);
	unsigned long delay = 0;
	int ret;
	u32 status;

	dev_dbg(phy->phy.dev, "state: %d\n", phy->port_state);

	switch (phy->port_state) {
	case PORT_UNKNOWN:
		if (!phy->id_state) {
			phy->port_state = PORT_HOST_MODE;
			msm_hsphy_notify_extcon(phy, EXTCON_USB_HOST, 1);
			return;
		}

		if (phy->vbus_active) {
			if (phy->eud_enable_reg &&
					readl_relaxed(phy->eud_enable_reg)) {
				pr_err("usb: EUD is enabled, no charger detection\n");
				msm_hsphy_notify_charger(phy,
							POWER_SUPPLY_TYPE_USB);
				msm_hsphy_notify_extcon(phy, EXTCON_USB, 1);
				phy->port_state = PORT_CHG_DET_DONE;
				return;
			}

			/* Enable DCD sequence */
			ret = msm_hsphy_prepare_chg_det(phy);
			if (ret)
				return;

			msm_hsphy_chg_det_enable_seq(phy, STATE_DCD);
			phy->port_state = PORT_DCD_IN_PROGRESS;
			phy->dcd_timeout = 0;
			delay = CHG_DCD_POLL_TIME_MSEC;
			break;
		}
		return;
	case PORT_DISCONNECTED:
		msm_hsphy_unprepare_chg_det(phy);
		msm_hsphy_notify_charger(phy, POWER_SUPPLY_TYPE_UNKNOWN);
		phy->port_state = PORT_UNKNOWN;
		break;
	case PORT_DCD_IN_PROGRESS:
		if (!phy->vbus_active) {
			/* Disable PHY sequence */
			phy->port_state = PORT_DISCONNECTED;
			break;
		}

		status = msm_hsphy_chg_det_status(phy, STATE_DCD);

		/*
		 * Floating or non compliant charger which pull D+ all the time
		 * will cause DCD timeout and end up being detected as SDP. This
		 * is an acceptable behavior compared to false negative of
		 * slower insertion of SDP/CDP detection
		 */
		if (!status || phy->dcd_timeout >= CHG_DCD_TIMEOUT_MSEC) {
			dev_dbg(phy->phy.dev, "DCD status=%d timeout=%d\n",
							status, phy->dcd_timeout);
			msm_hsphy_chg_det_disable_seq(phy, STATE_DCD);
			msm_hsphy_chg_det_enable_seq(phy, STATE_PRIMARY);
			phy->port_state = PORT_PRIMARY_IN_PROGRESS;
			delay = CHG_PRIMARY_DET_TIME_MSEC;
		} else {
			delay = CHG_DCD_POLL_TIME_MSEC;
			phy->dcd_timeout += delay;
		}

		break;
	case PORT_PRIMARY_IN_PROGRESS:
		if (!phy->vbus_active) {
			phy->port_state = PORT_DISCONNECTED;
			break;
		}

		status = msm_hsphy_chg_det_status(phy, STATE_PRIMARY);

		if (status) {
			msm_hsphy_chg_det_disable_seq(phy, STATE_PRIMARY);
			/*
			 * Delay 20ms TVDMSRC_DIS for Charging Port to
			 * disable D-. This is needed between primary
			 * detection shutoff and secondary detection start
			 */
			msleep(20);
			msm_hsphy_chg_det_enable_seq(phy, STATE_SECONDARY);
			phy->port_state = PORT_SECONDARY_IN_PROGRESS;
			delay = CHG_SECONDARY_DET_TIME_MSEC;

		} else {
			msm_hsphy_chg_det_disable_seq(phy, STATE_PRIMARY);
			msm_hsphy_unprepare_chg_det(phy);
			msm_hsphy_notify_charger(phy, POWER_SUPPLY_TYPE_USB);
			msm_hsphy_notify_extcon(phy, EXTCON_USB, 1);
			dev_info(phy->phy.dev, "Connected to SDP\n");
			phy->port_state = PORT_CHG_DET_DONE;
		}
		break;
	case PORT_SECONDARY_IN_PROGRESS:
		if (!phy->vbus_active) {
			phy->port_state = PORT_DISCONNECTED;
			break;
		}

		status = msm_hsphy_chg_det_status(phy, STATE_SECONDARY);

		msm_hsphy_chg_det_disable_seq(phy, STATE_SECONDARY);
		msm_hsphy_unprepare_chg_det(phy);
		phy->port_state = PORT_CHG_DET_DONE;

		if (status) {
			msm_hsphy_notify_charger(phy,
						POWER_SUPPLY_TYPE_USB_DCP);
			dev_info(phy->phy.dev, "Connected to DCP\n");
		} else {
			msm_hsphy_notify_charger(phy,
						POWER_SUPPLY_TYPE_USB_CDP);
			msm_hsphy_notify_extcon(phy, EXTCON_USB, 1);
			/*
			 * Drive a pulse on DP to ensure proper CDP detection
			 */
			dev_info(phy->phy.dev, "Connected to CDP, pull DP up\n");
			usb_phy_drive_dp_pulse(&phy->phy);
		}
		/*
		 * Fall through to check if cable got disconnected
		 * during detection.
		 */
	case PORT_CHG_DET_DONE:
		if (!phy->vbus_active) {
			phy->port_state = PORT_DISCONNECTED;
			msm_hsphy_notify_extcon(phy, EXTCON_USB, 0);
			break;
		}

		return;
	case PORT_HOST_MODE:
		if (phy->id_state) {
			phy->port_state = PORT_UNKNOWN;
			msm_hsphy_notify_extcon(phy, EXTCON_USB_HOST, 0);
		}

		if (!phy->vbus_active)
			return;

		break;
	default:
		return;
	}

	dev_dbg(phy->phy.dev, "%s status:%d vbus_state:%d delay:%d\n",
				__func__, status, phy->vbus_active, delay);

	queue_delayed_work(system_freezable_wq,
			&phy->port_det_w, msecs_to_jiffies(delay));
}

static int msm_hsphy_extcon_register(struct msm_hsphy *phy)
{
	int ret;

	/* Register extcon for notifications from charger driver */
	phy->phy.vbus_nb.notifier_call = msm_hsphy_vbus_notifier;

	phy->phy.id_nb.notifier_call = msm_hsphy_id_notifier;

	/* Register extcon to notify USB driver */
	phy->usb_extcon = devm_extcon_dev_allocate(phy->phy.dev,
						msm_hsphy_extcon_cable);
	if (IS_ERR(phy->usb_extcon)) {
		dev_err(phy->phy.dev, "failed to allocate extcon device\n");
		return PTR_ERR(phy->usb_extcon);
	}

	ret = devm_extcon_dev_register(phy->phy.dev, phy->usb_extcon);
	if (ret) {
		dev_err(phy->phy.dev, "failed to register extcon device\n");
		return ret;
	}

	extcon_set_property_capability(phy->usb_extcon, EXTCON_USB,
			EXTCON_PROP_USB_TYPEC_POLARITY);
	extcon_set_property_capability(phy->usb_extcon, EXTCON_USB,
			EXTCON_PROP_USB_SS);
	extcon_set_property_capability(phy->usb_extcon, EXTCON_USB_HOST,
			EXTCON_PROP_USB_TYPEC_POLARITY);
	extcon_set_property_capability(phy->usb_extcon, EXTCON_USB_HOST,
			EXTCON_PROP_USB_SS);
	return 0;
}

static int usb2_get_regulators(struct msm_hsphy *phy)
{
	struct device *dev = phy->phy.dev;

	phy->refgen = NULL;

	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get vdd supply\n");
		return PTR_ERR(phy->vdd);
	}

	phy->vdda33 = devm_regulator_get(dev, "vdda33");
	if (IS_ERR(phy->vdda33)) {
		dev_err(dev, "unable to get vdda33 supply\n");
		return PTR_ERR(phy->vdda33);
	}

	phy->vdda18 = devm_regulator_get(dev, "vdda18");
	if (IS_ERR(phy->vdda18)) {
		dev_err(dev, "unable to get vdda18 supply\n");
		return PTR_ERR(phy->vdda18);
	}

	if (of_property_read_bool(dev->of_node, "refgen-supply")) {
		phy->refgen = devm_regulator_get_optional(dev, "refgen");
		if (IS_ERR(phy->refgen))
			dev_err(dev, "unable to get refgen supply\n");
	}

	return 0;
}

static int msm_hsphy_probe(struct platform_device *pdev)
{
	struct msm_hsphy *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		ret = -ENOMEM;
		goto err_ret;
	}

	phy->phy.dev = dev;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"hsusb_phy_base");
	if (!res) {
		dev_err(dev, "missing memory base resource\n");
		ret = -ENODEV;
		goto err_ret;
	}

	phy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->base)) {
		dev_err(dev, "ioremap failed\n");
		ret = -ENODEV;
		goto err_ret;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"phy_rcal_reg");
	if (res) {
		phy->phy_rcal_reg = devm_ioremap(dev,
					res->start, resource_size(res));
		if (IS_ERR(phy->phy_rcal_reg)) {
			dev_err(dev, "couldn't ioremap phy_rcal_reg\n");
			phy->phy_rcal_reg = NULL;
		}
		if (of_property_read_u32(dev->of_node,
					"qcom,rcal-mask", &phy->rcal_mask)) {
			dev_err(dev, "unable to read phy rcal mask\n");
			phy->phy_rcal_reg = NULL;
		}
		dev_dbg(dev, "rcal_mask:%08x reg:%pK\n", phy->rcal_mask,
				phy->phy_rcal_reg);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"eud_enable_reg");
	if (res) {
		phy->eud_enable_reg = devm_ioremap_resource(dev, res);
		if (IS_ERR(phy->eud_enable_reg)) {
			dev_err(dev, "err getting eud_enable_reg address\n");
			return PTR_ERR(phy->eud_enable_reg);
		}
		phy->eud_reg = res->start;
	}

	/* ref_clk_src is needed irrespective of SE_CLK or DIFF_CLK usage */
	phy->ref_clk_src = devm_clk_get(dev, "ref_clk_src");
	if (IS_ERR(phy->ref_clk_src)) {
		dev_dbg(dev, "clk get failed for ref_clk_src\n");
		ret = PTR_ERR(phy->ref_clk_src);
		return ret;
	}
	phy->ref_clk = devm_clk_get_optional(dev, "ref_clk");
	if (IS_ERR(phy->ref_clk)) {
		dev_dbg(dev, "clk get failed for ref_clk\n");
		ret = PTR_ERR(phy->ref_clk);
		return ret;
	}
	if (of_property_match_string(pdev->dev.of_node,
				"clock-names", "cfg_ahb_clk") >= 0) {
		phy->cfg_ahb_clk = devm_clk_get(dev, "cfg_ahb_clk");
		if (IS_ERR(phy->cfg_ahb_clk)) {
			ret = PTR_ERR(phy->cfg_ahb_clk);
			if (ret != -EPROBE_DEFER)
				dev_err(dev,
				"clk get failed for cfg_ahb_clk ret %d\n", ret);
			return ret;
		}
	}

	phy->phy_reset = devm_reset_control_get(dev, "phy_reset");
	if (IS_ERR(phy->phy_reset))
		return PTR_ERR(phy->phy_reset);

	phy->param_override_seq_cnt = of_property_count_elems_of_size(
					dev->of_node,
					"qcom,param-override-seq",
					sizeof(*phy->param_override_seq));
	if (phy->param_override_seq_cnt > 0) {
		phy->param_override_seq = devm_kcalloc(dev,
					phy->param_override_seq_cnt,
					sizeof(*phy->param_override_seq),
					GFP_KERNEL);
		if (!phy->param_override_seq)
			return -ENOMEM;

		if (phy->param_override_seq_cnt % 2) {
			dev_err(dev, "invalid param_override_seq_len\n");
			return -EINVAL;
		}

		ret = of_property_read_u32_array(dev->of_node,
				"qcom,param-override-seq",
				phy->param_override_seq,
				phy->param_override_seq_cnt);
		if (ret) {
			dev_err(dev, "qcom,param-override-seq read failed %d\n",
				ret);
			return ret;
		}
	}

	     /*
	      * Some targets use PMOS LDOs, while others use NMOS LDOs,
	      * but there is no support for NMOS LDOs whose load current threshold
	      * for entering HPM is 30mA, which is greater than 19mA.
	      * As a result of this property being passed in dt, the value of
	      * USB_HSPHY_1P8_HPM_LOAD will be modified to meet the requirements.
	      */

	if (of_property_read_s32(dev->of_node, "qcom,vdd18-max-load-uA",
			&phy->vdda18_max_uA) || !phy->vdda18_max_uA)
		phy->vdda18_max_uA = USB_HSPHY_1P8_HPM_LOAD;

	ret = of_property_read_u32_array(dev->of_node, "qcom,vdd-voltage-level",
					 (u32 *) phy->vdd_levels,
					 ARRAY_SIZE(phy->vdd_levels));
	if (ret) {
		dev_err(dev, "error reading qcom,vdd-voltage-level property\n");
		goto err_ret;
	}

	ret = of_property_read_u32_array(dev->of_node, "qcom,refgen-voltage-level",
					(u32 *) phy->refgen_levels,
					ARRAY_SIZE(phy->refgen_levels));
	if (ret)
		dev_err(dev, "error reading qcom,refgen-voltage-level property\n");

	ret = usb2_get_regulators(phy);
	if (ret)
		return ret;

	mutex_init(&phy->phy_lock);
	platform_set_drvdata(pdev, phy);

	phy->phy.init			= msm_hsphy_init;
	phy->phy.set_suspend		= msm_hsphy_set_suspend;
	phy->phy.notify_connect		= msm_hsphy_notify_connect;
	phy->phy.notify_disconnect	= msm_hsphy_notify_disconnect;
	phy->phy.set_power		= msm_hsphy_set_power;
	phy->phy.type			= USB_PHY_TYPE_USB2;
	phy->phy.charger_detect		= usb_phy_drive_dp_pulse;

	if (of_property_read_bool(dev->of_node, "extcon")) {
		INIT_DELAYED_WORK(&phy->port_det_w, msm_hsphy_port_state_work);

		ret = msm_hsphy_extcon_register(phy);
		if (ret)
			return ret;

	}

	ret = usb_add_phy_dev(&phy->phy);
	if (ret)
		return ret;

	ret = msm_hsphy_regulator_init(phy);
	if (ret) {
		usb_remove_phy(&phy->phy);
		return ret;
	}

	INIT_WORK(&phy->vbus_draw_work, msm_hsphy_vbus_draw_work);

	if (of_property_read_bool(dev->of_node, "extcon")) {
		phy->id_state = true;
		phy->vbus_active = false;

		if (extcon_get_state(phy->phy.edev, EXTCON_USB_HOST) > 0) {
			msm_hsphy_id_notifier(&phy->phy.id_nb,
							1, phy->phy.edev);
		} else if (extcon_get_state(phy->phy.edev, EXTCON_USB) > 0) {
			msm_hsphy_vbus_notifier(&phy->phy.vbus_nb,
							1, phy->phy.edev);
		}
	}

	msm_hsphy_create_debugfs(phy);

	/*
	 * EUD may be enable in boot loader and to keep EUD session alive across
	 * kernel boot till USB phy driver is initialized based on cable status,
	 * keep LDOs on here.
	 */
	msm_hsphy_enable_clocks(phy, true);
	if (phy->eud_enable_reg && readl_relaxed(phy->eud_enable_reg)) {
		msm_hsphy_enable_power(phy, true);
	}

	return 0;

err_ret:
	return ret;
}

static int msm_hsphy_remove(struct platform_device *pdev)
{
	struct msm_hsphy *phy = platform_get_drvdata(pdev);

	if (!phy)
		return 0;

	if (phy->usb_psy)
		power_supply_put(phy->usb_psy);

	debugfs_remove_recursive(phy->root);

	usb_remove_phy(&phy->phy);
	clk_disable_unprepare(phy->ref_clk_src);

	msm_hsphy_enable_clocks(phy, false);
	msm_hsphy_enable_power(phy, false);
	return 0;
}

static const struct of_device_id msm_usb_id_table[] = {
	{
		.compatible = "qcom,usb-hsphy-snps-femto",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, msm_usb_id_table);

static struct platform_driver msm_hsphy_driver = {
	.probe		= msm_hsphy_probe,
	.remove		= msm_hsphy_remove,
	.driver = {
		.name	= "msm-usb-hsphy",
		.of_match_table = of_match_ptr(msm_usb_id_table),
	},
};

module_platform_driver(msm_hsphy_driver);

MODULE_DESCRIPTION("MSM USB HS PHY driver");
MODULE_LICENSE("GPL v2");
