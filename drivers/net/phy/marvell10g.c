/*
 * Marvell 10G 88x3310 PHY driver
 *
 * Based upon the ID registers, this PHY appears to be a mixture of IPs
 * from two different companies.
 *
 * There appears to be several different data paths through the PHY which
 * are automatically managed by the PHY.  The following has been determined
 * via observation and experimentation for a setup using single-lane Serdes:
 *
 *       SGMII PHYXS -- BASE-T PCS -- 10G PMA -- AN -- Copper (for <= 1G)
 *  10GBASE-KR PHYXS -- BASE-T PCS -- 10G PMA -- AN -- Copper (for 10G)
 *  10GBASE-KR PHYXS -- BASE-R PCS -- Fiber
 *
 * With XAUI, observation shows:
 *
 *        XAUI PHYXS -- <appropriate PCS as above>
 *
 * and no switching of the host interface mode occurs.
 *
 * If both the fiber and copper ports are connected, the first to gain
 * link takes priority and the other port is completely locked out.
 */
#include <linux/ctype.h>
#include <linux/hwmon.h>
#include <linux/marvell_phy.h>
#include <linux/phy.h>

enum {
	MV_PMA_BOOT		= 0xc050,
	MV_PMA_BOOT_FATAL	= BIT(0),

	MV_PCS_BASE_T		= 0x0000,
	MV_PCS_BASE_R		= 0x1000,
	MV_PCS_1000BASEX	= 0x2000,

	MV_PCS_PAIRSWAP		= 0x8182,
	MV_PCS_PAIRSWAP_MASK	= 0x0003,
	MV_PCS_PAIRSWAP_AB	= 0x0002,
	MV_PCS_PAIRSWAP_NONE	= 0x0003,

	/* These registers appear at 0x800X and 0xa00X - the 0xa00X control
	 * registers appear to set themselves to the 0x800X when AN is
	 * restarted, but status registers appear readable from either.
	 */
	MV_AN_CTRL1000		= 0x8000, /* 1000base-T control register */
	MV_AN_STAT1000		= 0x8001, /* 1000base-T status register */

	/* Vendor2 MMD registers */
	MV_V2_TEMP_CTRL		= 0xf08a,
	MV_V2_TEMP_CTRL_MASK	= 0xc000,
	MV_V2_TEMP_CTRL_SAMPLE	= 0x0000,
	MV_V2_TEMP_CTRL_DISABLE	= 0xc000,
	MV_V2_TEMP		= 0xf08c,
	MV_V2_TEMP_UNKNOWN	= 0x9600, /* unknown function */
};

struct mv3310_priv {
	struct device *hwmon_dev;
	char *hwmon_name;
};

static int mv3310_modify(struct phy_device *phydev, int devad, u16 reg,
			 u16 mask, u16 bits)
{
	int old, val, ret;

	old = phy_read_mmd(phydev, devad, reg);
	if (old < 0)
		return old;

	val = (old & ~mask) | (bits & mask);
	if (val == old)
		return 0;

	ret = phy_write_mmd(phydev, devad, reg, val);

	return ret < 0 ? ret : 1;
}

#ifdef CONFIG_HWMON
static umode_t mv3310_hwmon_is_visible(const void *data,
				       enum hwmon_sensor_types type,
				       u32 attr, int channel)
{
	if (type == hwmon_chip && attr == hwmon_chip_update_interval)
		return 0444;
	if (type == hwmon_temp && attr == hwmon_temp_input)
		return 0444;
	return 0;
}

static int mv3310_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
			     u32 attr, int channel, long *value)
{
	struct phy_device *phydev = dev_get_drvdata(dev);
	int temp;

	if (type == hwmon_chip && attr == hwmon_chip_update_interval) {
		*value = MSEC_PER_SEC;
		return 0;
	}

	if (type == hwmon_temp && attr == hwmon_temp_input) {
		temp = phy_read_mmd(phydev, MDIO_MMD_VEND2, MV_V2_TEMP);
		if (temp < 0)
			return temp;

		*value = ((temp & 0xff) - 75) * 1000;

		return 0;
	}

	return -EOPNOTSUPP;
}

static const struct hwmon_ops mv3310_hwmon_ops = {
	.is_visible = mv3310_hwmon_is_visible,
	.read = mv3310_hwmon_read,
};

static u32 mv3310_hwmon_chip_config[] = {
	HWMON_C_REGISTER_TZ | HWMON_C_UPDATE_INTERVAL,
	0,
};

static const struct hwmon_channel_info mv3310_hwmon_chip = {
	.type = hwmon_chip,
	.config = mv3310_hwmon_chip_config,
};

static u32 mv3310_hwmon_temp_config[] = {
	HWMON_T_INPUT,
	0,
};

static const struct hwmon_channel_info mv3310_hwmon_temp = {
	.type = hwmon_temp,
	.config = mv3310_hwmon_temp_config,
};

static const struct hwmon_channel_info *mv3310_hwmon_info[] = {
	&mv3310_hwmon_chip,
	&mv3310_hwmon_temp,
	NULL,
};

static const struct hwmon_chip_info mv3310_hwmon_chip_info = {
	.ops = &mv3310_hwmon_ops,
	.info = mv3310_hwmon_info,
};

static int mv3310_hwmon_config(struct phy_device *phydev, bool enable)
{
	u16 val;
	int ret;

	ret = phy_write_mmd(phydev, MDIO_MMD_VEND2, MV_V2_TEMP,
			    MV_V2_TEMP_UNKNOWN);
	if (ret < 0)
		return ret;

	val = enable ? MV_V2_TEMP_CTRL_SAMPLE : MV_V2_TEMP_CTRL_DISABLE;
	ret = mv3310_modify(phydev, MDIO_MMD_VEND2, MV_V2_TEMP_CTRL,
			    MV_V2_TEMP_CTRL_MASK, val);

	return ret < 0 ? ret : 0;
}

static void mv3310_hwmon_disable(void *data)
{
	struct phy_device *phydev = data;

	mv3310_hwmon_config(phydev, false);
}

static int mv3310_hwmon_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	struct mv3310_priv *priv = dev_get_drvdata(&phydev->mdio.dev);
	int i, j, ret;

	priv->hwmon_name = devm_kstrdup(dev, dev_name(dev), GFP_KERNEL);
	if (!priv->hwmon_name)
		return -ENODEV;

	for (i = j = 0; priv->hwmon_name[i]; i++) {
		if (isalnum(priv->hwmon_name[i])) {
			if (i != j)
				priv->hwmon_name[j] = priv->hwmon_name[i];
			j++;
		}
	}
	priv->hwmon_name[j] = '\0';

	ret = mv3310_hwmon_config(phydev, true);
	if (ret)
		return ret;

	ret = devm_add_action_or_reset(dev, mv3310_hwmon_disable, phydev);
	if (ret)
		return ret;

	priv->hwmon_dev = devm_hwmon_device_register_with_info(dev,
				priv->hwmon_name, phydev,
				&mv3310_hwmon_chip_info, NULL);

	return PTR_ERR_OR_ZERO(priv->hwmon_dev);
}
#else
static inline int mv3310_hwmon_config(struct phy_device *phydev, bool enable)
{
	return 0;
}

static int mv3310_hwmon_probe(struct phy_device *phydev)
{
	return 0;
}
#endif

static int mv3310_probe(struct phy_device *phydev)
{
	struct mv3310_priv *priv;
	u32 mmd_mask = MDIO_DEVS_PMAPMD | MDIO_DEVS_AN;
	int ret;

	if (!phydev->is_c45 ||
	    (phydev->c45_ids.devices_in_package & mmd_mask) != mmd_mask)
		return -ENODEV;

	ret = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MV_PMA_BOOT);
	if (ret < 0)
		return ret;

	if (ret & MV_PMA_BOOT_FATAL) {
		dev_warn(&phydev->mdio.dev,
			 "PHY failed to boot firmware, status=%04x\n", ret);
		return -ENODEV;
	}

	priv = devm_kzalloc(&phydev->mdio.dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(&phydev->mdio.dev, priv);

	ret = mv3310_hwmon_probe(phydev);
	if (ret)
		return ret;

	return 0;
}

static int mv3310_suspend(struct phy_device *phydev)
{
	return 0;
}

static int mv3310_resume(struct phy_device *phydev)
{
	return mv3310_hwmon_config(phydev, true);
}

static int mv3310_config_init(struct phy_device *phydev)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(supported) = { 0, };
	u32 mask;
	int val;

	/* Check that the PHY interface type is compatible */
	if (phydev->interface != PHY_INTERFACE_MODE_SGMII &&
	    phydev->interface != PHY_INTERFACE_MODE_XAUI &&
	    phydev->interface != PHY_INTERFACE_MODE_RXAUI &&
	    phydev->interface != PHY_INTERFACE_MODE_10GKR)
		return -ENODEV;

	__set_bit(ETHTOOL_LINK_MODE_Pause_BIT, supported);
	__set_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, supported);

	if (phydev->c45_ids.devices_in_package & MDIO_DEVS_AN) {
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_STAT1);
		if (val < 0)
			return val;

		if (val & MDIO_AN_STAT1_ABLE)
			__set_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, supported);
	}

	val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_STAT2);
	if (val < 0)
		return val;

	/* Ethtool does not support the WAN mode bits */
	if (val & (MDIO_PMA_STAT2_10GBSR | MDIO_PMA_STAT2_10GBLR |
		   MDIO_PMA_STAT2_10GBER | MDIO_PMA_STAT2_10GBLX4 |
		   MDIO_PMA_STAT2_10GBSW | MDIO_PMA_STAT2_10GBLW |
		   MDIO_PMA_STAT2_10GBEW))
		__set_bit(ETHTOOL_LINK_MODE_FIBRE_BIT, supported);
	if (val & MDIO_PMA_STAT2_10GBSR)
		__set_bit(ETHTOOL_LINK_MODE_10000baseSR_Full_BIT, supported);
	if (val & MDIO_PMA_STAT2_10GBLR)
		__set_bit(ETHTOOL_LINK_MODE_10000baseLR_Full_BIT, supported);
	if (val & MDIO_PMA_STAT2_10GBER)
		__set_bit(ETHTOOL_LINK_MODE_10000baseER_Full_BIT, supported);

	if (val & MDIO_PMA_STAT2_EXTABLE) {
		val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_EXTABLE);
		if (val < 0)
			return val;

		if (val & (MDIO_PMA_EXTABLE_10GBT | MDIO_PMA_EXTABLE_1000BT |
			   MDIO_PMA_EXTABLE_100BTX | MDIO_PMA_EXTABLE_10BT))
			__set_bit(ETHTOOL_LINK_MODE_TP_BIT, supported);
		if (val & MDIO_PMA_EXTABLE_10GBLRM)
			__set_bit(ETHTOOL_LINK_MODE_FIBRE_BIT, supported);
		if (val & (MDIO_PMA_EXTABLE_10GBKX4 | MDIO_PMA_EXTABLE_10GBKR |
			   MDIO_PMA_EXTABLE_1000BKX))
			__set_bit(ETHTOOL_LINK_MODE_Backplane_BIT, supported);
		if (val & MDIO_PMA_EXTABLE_10GBLRM)
			__set_bit(ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT,
				  supported);
		if (val & MDIO_PMA_EXTABLE_10GBT)
			__set_bit(ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
				  supported);
		if (val & MDIO_PMA_EXTABLE_10GBKX4)
			__set_bit(ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
				  supported);
		if (val & MDIO_PMA_EXTABLE_10GBKR)
			__set_bit(ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
				  supported);
		if (val & MDIO_PMA_EXTABLE_1000BT)
			__set_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
				  supported);
		if (val & MDIO_PMA_EXTABLE_1000BKX)
			__set_bit(ETHTOOL_LINK_MODE_1000baseKX_Full_BIT,
				  supported);
		if (val & MDIO_PMA_EXTABLE_100BTX) {
			__set_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT,
				  supported);
			__set_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
				  supported);
		}
		if (val & MDIO_PMA_EXTABLE_10BT) {
			__set_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT,
				  supported);
			__set_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT,
				  supported);
		}
	}

	if (!ethtool_convert_link_mode_to_legacy_u32(&mask, supported))
		dev_warn(&phydev->mdio.dev,
			 "PHY supports (%*pb) more modes than phylib supports, some modes not supported.\n",
			 __ETHTOOL_LINK_MODE_MASK_NBITS, supported);

	phydev->supported &= mask;
	phydev->advertising &= phydev->supported;

	return 0;
}

static int mv3310_config_aneg(struct phy_device *phydev)
{
	bool changed = false;
	u32 advertising;
	int ret;

	/* We don't support manual MDI control */
	phydev->mdix_ctrl = ETH_TP_MDI_AUTO;

	if (phydev->autoneg == AUTONEG_DISABLE) {
		ret = genphy_c45_pma_setup_forced(phydev);
		if (ret < 0)
			return ret;

		return genphy_c45_an_disable_aneg(phydev);
	}

	phydev->advertising &= phydev->supported;
	advertising = phydev->advertising;

	ret = mv3310_modify(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE,
			    ADVERTISE_ALL | ADVERTISE_100BASE4 |
			    ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM,
			    ethtool_adv_to_mii_adv_t(advertising));
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	ret = mv3310_modify(phydev, MDIO_MMD_AN, MV_AN_CTRL1000,
			    ADVERTISE_1000FULL | ADVERTISE_1000HALF,
			    ethtool_adv_to_mii_ctrl1000_t(advertising));
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	/* 10G control register */
	ret = mv3310_modify(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL,
			    MDIO_AN_10GBT_CTRL_ADV10G,
			    advertising & ADVERTISED_10000baseT_Full ?
				MDIO_AN_10GBT_CTRL_ADV10G : 0);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	if (changed)
		ret = genphy_c45_restart_aneg(phydev);

	return ret;
}

static int mv3310_aneg_done(struct phy_device *phydev)
{
	int val;

	val = phy_read_mmd(phydev, MDIO_MMD_PCS, MV_PCS_BASE_R + MDIO_STAT1);
	if (val < 0)
		return val;

	if (val & MDIO_STAT1_LSTATUS)
		return 1;

	return genphy_c45_aneg_done(phydev);
}

static void mv3310_update_interface(struct phy_device *phydev)
{
	if ((phydev->interface == PHY_INTERFACE_MODE_SGMII ||
	     phydev->interface == PHY_INTERFACE_MODE_10GKR) && phydev->link) {
		/* The PHY automatically switches its serdes interface (and
		 * active PHYXS instance) between Cisco SGMII and 10GBase-KR
		 * modes according to the speed.  Florian suggests setting
		 * phydev->interface to communicate this to the MAC. Only do
		 * this if we are already in either SGMII or 10GBase-KR mode.
		 */
		if (phydev->speed == SPEED_10000)
			phydev->interface = PHY_INTERFACE_MODE_10GKR;
		else if (phydev->speed >= SPEED_10 &&
			 phydev->speed < SPEED_10000)
			phydev->interface = PHY_INTERFACE_MODE_SGMII;
	}
}

/* 10GBASE-ER,LR,LRM,SR do not support autonegotiation. */
static int mv3310_read_10gbr_status(struct phy_device *phydev)
{
	phydev->link = 1;
	phydev->speed = SPEED_10000;
	phydev->duplex = DUPLEX_FULL;

	mv3310_update_interface(phydev);

	return 0;
}

static int mv3310_read_status(struct phy_device *phydev)
{
	u32 mmd_mask = phydev->c45_ids.devices_in_package;
	int val;

	/* The vendor devads do not report link status.  Avoid the PHYXS
	 * instance as there are three, and its status depends on the MAC
	 * being appropriately configured for the negotiated speed.
	 */
	mmd_mask &= ~(BIT(MDIO_MMD_VEND1) | BIT(MDIO_MMD_VEND2) |
		      BIT(MDIO_MMD_PHYXS));

	phydev->speed = SPEED_UNKNOWN;
	phydev->duplex = DUPLEX_UNKNOWN;
	phydev->lp_advertising = 0;
	phydev->link = 0;
	phydev->pause = 0;
	phydev->asym_pause = 0;
	phydev->mdix = 0;

	val = phy_read_mmd(phydev, MDIO_MMD_PCS, MV_PCS_BASE_R + MDIO_STAT1);
	if (val < 0)
		return val;

	if (val & MDIO_STAT1_LSTATUS)
		return mv3310_read_10gbr_status(phydev);

	val = genphy_c45_read_link(phydev, mmd_mask);
	if (val < 0)
		return val;

	phydev->link = val > 0 ? 1 : 0;

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_STAT1);
	if (val < 0)
		return val;

	if (val & MDIO_AN_STAT1_COMPLETE) {
		val = genphy_c45_read_lpa(phydev);
		if (val < 0)
			return val;

		/* Read the link partner's 1G advertisement */
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MV_AN_STAT1000);
		if (val < 0)
			return val;

		phydev->lp_advertising |= mii_stat1000_to_ethtool_lpa_t(val);

		if (phydev->autoneg == AUTONEG_ENABLE)
			phy_resolve_aneg_linkmode(phydev);
	}

	if (phydev->autoneg != AUTONEG_ENABLE) {
		val = genphy_c45_read_pma(phydev);
		if (val < 0)
			return val;
	}

	if (phydev->speed == SPEED_10000) {
		val = genphy_c45_read_mdix(phydev);
		if (val < 0)
			return val;
	} else {
		val = phy_read_mmd(phydev, MDIO_MMD_PCS, MV_PCS_PAIRSWAP);
		if (val < 0)
			return val;

		switch (val & MV_PCS_PAIRSWAP_MASK) {
		case MV_PCS_PAIRSWAP_AB:
			phydev->mdix = ETH_TP_MDI_X;
			break;
		case MV_PCS_PAIRSWAP_NONE:
			phydev->mdix = ETH_TP_MDI;
			break;
		default:
			phydev->mdix = ETH_TP_MDI_INVALID;
			break;
		}
	}

	mv3310_update_interface(phydev);

	return 0;
}

static struct phy_driver mv3310_drivers[] = {
	{
		.phy_id		= 0x002b09aa,
		.phy_id_mask	= MARVELL_PHY_ID_MASK,
		.name		= "mv88x3310",
		.features	= SUPPORTED_10baseT_Full |
				  SUPPORTED_10baseT_Half |
				  SUPPORTED_100baseT_Full |
				  SUPPORTED_100baseT_Half |
				  SUPPORTED_1000baseT_Full |
				  SUPPORTED_Autoneg |
				  SUPPORTED_TP |
				  SUPPORTED_FIBRE |
				  SUPPORTED_10000baseT_Full |
				  SUPPORTED_Backplane,
		.soft_reset	= gen10g_no_soft_reset,
		.config_init	= mv3310_config_init,
		.probe		= mv3310_probe,
		.suspend	= mv3310_suspend,
		.resume		= mv3310_resume,
		.config_aneg	= mv3310_config_aneg,
		.aneg_done	= mv3310_aneg_done,
		.read_status	= mv3310_read_status,
	},
};

module_phy_driver(mv3310_drivers);

static struct mdio_device_id __maybe_unused mv3310_tbl[] = {
	{ 0x002b09aa, MARVELL_PHY_ID_MASK },
	{ },
};
MODULE_DEVICE_TABLE(mdio, mv3310_tbl);
MODULE_DESCRIPTION("Marvell Alaska X 10Gigabit Ethernet PHY driver (MV88X3310)");
MODULE_LICENSE("GPL");
