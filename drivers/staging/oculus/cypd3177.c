// SPDX-License-Identifier: GPL+
/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "cypd.h"

/* Status Registers */
#define DEVICE_MODE	0x0000
#define SILICON_ID	0x0002
#define INTERRUPT	0x0006	/* Interrupt for which INTR pin */
#define PD_STATUS	0x1008
#define TYPE_C_STATUS	0x100C
#define BUS_VOLTAGE	0x100D
#define CURRENT_PDO	0x1010
#define CURRENT_RDO	0x1014
#define SWAP_RESPONSE	0x1028
#define EVENT_STATUS	0x1044
#define READ_GPIO_LEVEL	0x0082
#define SAMPLE_GPIO	0x0083

/* Command Registers */
#define RESET		0x0008
#define EVENT_MASK	0x1024
#define DM_CONTROL	0x1000
#define SELECT_SINK_PDO	0x1005
#define PD_CONTROL	0x1006
#define REQUEST		0x1050
#define SET_GPIO_MODE	0x0080
#define SET_GPIO_LEVEL	0x0081
#define VDM_EC_CONTROL	0x102A

/* Response Registers */
#define DEV_RESPONSE	0x007E
#define PD_RESPONSE	0x1400

/* Data Region */
#define READ_DATA_REGION 0x1404
#define WRITE_DATA_REGION 0x1800

/* PD RESPONSE EVENT */
#define PD_PORT_BUSY		0x12
#define TYPEC_CONNECT		0x84
#define TYPEC_DISCONNECT	0x85
#define PD_CONTRACT_COMPLETE	0x86
#define VDM_RECEIVED		0x90
#define HARD_RESET		0x9A
#define DEV_EVENT		BIT(0)
#define PD_EVENT		BIT(1)

#define OUT_EN_L_BIT		BIT(0)	/* Port Partner Connection status */
#define EVENT_MASK_OPEN		(BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7) | BIT(11))
#define CYPD3177_REG_8BIT	1
#define CYPD3177_REG_16BIT	2
#define CYPD3177_REG_32BIT	4
#define VOLTAGE_UNIT		100000	/* in 100000uv units */
#define PDO_BIT10_BIT19		0xffc00
#define PDO_VOLTAGE_UNIT	50000
#define RDO_BIT0_BIT9		0x3ff
#define RDO_CURRENT_UNIT	10000
#define RESET_DEV		0x152
#define VOLTAGE_9V		9000000
#define VDM_ENABLE		0x1
#define VDM_DISABLE		0
#define PD_3_0_REQ		BIT(2)
#define MAX_DATA_LENGTH	32
#define DM_CTRL_MSG(sop, len) \
	(sop | (len & 0x100) | ((len & 0xFF) << 8))
#define VDM_DATA_LEN(msg) \
	((msg & 0xFF00) >> 8)

/* EVENT_STATUS Fields */
#define EVENT_STATUS_TYPEC_ATTACHED		BIT(0)
#define EVENT_STATUS_PD_CONTRACT_COMPLETE	BIT(2)

/* TYPE_C_STATUS Fields */
#define TYPE_C_STATUS_TYPE_SOURCE_VAL		2
#define TYPE_C_STATUS_ATTACHED_TYPE_SHIFT	2
#define TYPE_C_STATUS_ATTACHED_TYPE_MASK	(0x7 << TYPE_C_STATUS_ATTACHED_TYPE_SHIFT)

/* PD_STATUS Fields */
#define PD_STATUS_CONTRACT_MASK			BIT(10)

/* SNKP Signature */
#define SNKP	0x534E4B50

/* Sink PDO Mask */
#define PDO_MASK_MAX	0x7F
#define PDO_PACKAGE_LENGTH	32

#define TOTAL_CAPS	7
#define MAX_NUM_SINK_CAPS	14
#define PD_SNK_PDO_FIXED(prs, hc, uc, usb_comm, drs, volt, curr) \
	(((prs) << 29) | ((hc) << 28) | ((uc) << 27) | ((usb_comm) << 26) | \
	 ((drs) << 25) | ((volt) << 10) | (curr))

struct cypd3177 {
	struct device		*dev;
	struct i2c_client	*client;
	struct power_supply	*psy;
	struct power_supply	*batt_psy;
	struct power_supply	*dc_psy;
	struct notifier_block	 psy_nb;
	struct delayed_work	 sink_cap_work;
	struct wakeup_source	*cypd_ws;
	struct dentry		*dfs_root;
	unsigned int hpi_irq;
	unsigned int fault_irq;
	bool typec_status;
	int pd_attach;
	struct cypd *cypd;
	bool is_opened;
	void (*msg_rx_cb)(struct cypd *pd, enum pd_sop_type sop,
			  u8 *buf, size_t len);
	bool pdo_update_flag;
	int num_sink_caps;
	unsigned long last_conn_time_jiffies;
	u32 last_conn_debounce_ms;
	u32 sink_caps[TOTAL_CAPS];
	u32 num_5v_sink_caps;
	bool sink_cap_5v;
	int charge_status;
	bool handle_port_busy;
	struct mutex state_lock;
};

static struct cypd3177 *__chip;

static void dc_psy_set_icl(struct cypd3177 *chip, int icl_uA)
{
	union power_supply_propval val = { .intval = icl_uA };

	if (!chip->dc_psy || icl_uA < 0)
		return;

	dev_dbg(&chip->client->dev, "setting ICL to %d uA", icl_uA);
	power_supply_set_property(chip->dc_psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
}

static int cypd3177_i2c_read_reg(struct i2c_client *client, u16 data_length,
			       u16 reg, u32 *val)
{
	int r;
	struct i2c_msg msg[2];
	u8 addr[2];
	u8 data[4];

	if (!client->adapter)
		return -ENODEV;
	if (data_length != CYPD3177_REG_8BIT &&
		data_length != CYPD3177_REG_16BIT &&
		data_length != CYPD3177_REG_32BIT)
		return -EINVAL;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = addr;

	/* low byte goes out first */
	addr[0] = (u8) (reg & 0xff);
	addr[1] = (u8) (reg >> 8);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = data_length;
	msg[1].buf = data;

	r = i2c_transfer(client->adapter, msg, 2);
	if (r < 0)
		goto err;

	*val = 0;
	/* high byte comes first */
	if (data_length == CYPD3177_REG_8BIT)
		*val = data[0];
	else if (data_length == CYPD3177_REG_16BIT)
		*val = (data[1] << 8) | data[0];
	else
		*val = (data[3] << 24) | (data[2] << 16) | (data[1] << 8)
			| data[0];

	return 0;

err:
	dev_err(&client->dev, "read from offset 0x%x error %d\n", reg, r);

	return r;
}

static void cypd3177_i2c_create_msg(struct i2c_client *client, u16 len, u16 reg,
				  u32 val, struct i2c_msg *msg, u8 *buf)
{
	msg->addr = client->addr;
	msg->flags = 0; /* Write */
	msg->len = 2 + len;
	msg->buf = buf;

	/* low byte goes out first */
	buf[0] = (u8) (reg & 0xff);
	buf[1] = (u8) (reg >> 8);

	switch (len) {
	case CYPD3177_REG_8BIT:
		buf[2] = (u8) (val) & 0xff;
		break;
	case CYPD3177_REG_16BIT:
		buf[2] = (u8) (val) & 0xff;
		buf[3] = (u8) (val >> 8) & 0xff;
		break;
	case CYPD3177_REG_32BIT:
		buf[2] = (u8) (val) & 0xff;
		buf[3] = (u8) (val >> 8) & 0xff;
		buf[4] = (u8) (val >> 16) & 0xff;
		buf[5] = (u8) (val >> 24) & 0xff;
		break;
	default:
		dev_err(&client->dev, "CYPD3177 : %s: invalid message length.\n",
			  __func__);
		break;
	}
}

static int cypd3177_i2c_write_reg(struct i2c_client *client, u16 data_length,
				u16 reg, u32 val)
{
	int r;
	struct i2c_msg msg;
	u8 data[6];

	if (!client->adapter)
		return -ENODEV;
	if (data_length != CYPD3177_REG_8BIT &&
		data_length != CYPD3177_REG_16BIT &&
		data_length != CYPD3177_REG_32BIT)
		return -EINVAL;

	cypd3177_i2c_create_msg(client, data_length, reg, val, &msg, data);

	r = i2c_transfer(client->adapter, &msg, 1);
	if (r < 0) {
		dev_err(&client->dev,
			"wrote 0x%x to offset 0x%x error %d\n", val, reg, r);
		return r;
	}

	return 0;
}

static void cypd3177_pdo_data_package(struct cypd3177 *chip, u16 len,
			u16 reg, struct i2c_msg *msg, u8 *buf)
{
	int i = 0;
	int m = 0;

	msg->addr = chip->client->addr;
	msg->flags = 0;
	msg->len = 2 + len;
	msg->buf = buf;

	/* Low byte goes out first */
	buf[0] = (u8) (reg & 0xff);
	buf[1] = (u8) (reg >> 8);

	/* Package one byte SNKP signature data */
	buf[2] = SNKP & 0Xff;
	buf[3] = (SNKP >> 8) & 0Xff;
	buf[4] = (SNKP >> 16) & 0Xff;
	buf[5] = (SNKP >> 24) & 0Xff;

	/* Package the remaining seven bytes of PDO data */
	for (i = 0; i < 7; i++) {
		buf[6 + m] = (u8) chip->sink_caps[i] & 0xff;
		buf[7 + m] = (u8) (chip->sink_caps[i] >> 8) & 0xff;
		buf[8 + m] = (u8) (chip->sink_caps[i] >> 16) & 0xff;
		buf[9 + m] = (u8) (chip->sink_caps[i] >> 24) & 0xff;
		m = (i + 1) * 4;
	}
}

static int cypd3177_i2c_write_pdo_package(struct cypd3177 *chip, u16 reg)
{
	int r;
	struct i2c_msg msg;
	u8 data[PDO_PACKAGE_LENGTH + 2];

	if (!chip->client->adapter)
		return -ENODEV;

	cypd3177_pdo_data_package(chip, PDO_PACKAGE_LENGTH, reg, &msg, data);

	r = i2c_transfer(chip->client->adapter, &msg, 1);
	if (r < 0)
		dev_err(&chip->client->dev, "write to offset 0x%x error %d\n",
			reg, r);

	return r;
}

static bool waiting_for_debounce(struct cypd3177 *chip)
{
	const u32 time_since_conn = jiffies_to_msecs(jiffies - chip->last_conn_time_jiffies);
	const bool ret = (chip->last_conn_time_jiffies != 0) &&
		(time_since_conn < chip->last_conn_debounce_ms);

	dev_dbg(&chip->client->dev, "%s: ret=%d time_since_conn=%u",
		__func__, ret, time_since_conn);

	return ret;
}

static int cypd3177_get_online(struct cypd3177 *chip, int *val)
{
	int rc = 0;
	int stat;

	if (waiting_for_debounce(chip)) {
		*val = 1;
	} else if (chip->typec_status) {
		rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_8BIT,
			TYPE_C_STATUS, &stat);
		if (rc < 0) {
			*val = 0;
			dev_err(&chip->client->dev, "cypd3177 i2c read type-c status error: %d\n",
				rc);
			return rc;
		}
		*val = stat & OUT_EN_L_BIT;
	} else
		*val = 0;

	return rc;
}

static int cypd3177_get_voltage_now(struct cypd3177 *chip, int *val)
{
	int rc;
	int raw;

	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_8BIT, BUS_VOLTAGE,
		&raw);
	if (rc < 0) {
		*val = 0;
		dev_dbg(&chip->client->dev, "read cypd3177 voltage error.\n");
		return 0;
	}

	*val = raw * VOLTAGE_UNIT;
	return rc;
}

static int cypd3177_get_voltage_max(struct cypd3177 *chip, int *val)
{
	int rc = 0;
	int pdo_data;

	if (chip->pd_attach) {
		rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_32BIT,
				CURRENT_PDO, &pdo_data);
		if (rc < 0) {
			dev_dbg(&chip->client->dev, "read cypd3177 PDO data failed: %d\n",
				rc);
			return rc;
		}
		*val = ((pdo_data & PDO_BIT10_BIT19) >> 10) * PDO_VOLTAGE_UNIT;
	} else
		*val = 0;

	return rc;
}

static int cypd3177_get_current_max(struct cypd3177 *chip, int *val)
{
	int rc = 0;
	int rdo_data;

	if (chip->pd_attach) {
		rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_32BIT,
				CURRENT_RDO, &rdo_data);
		if (rc < 0) {
			dev_dbg(&chip->client->dev, "read cypd3177 RDO data failed: %d\n",
				rc);
			return rc;
		}
		*val = (rdo_data & RDO_BIT0_BIT9) * RDO_CURRENT_UNIT;
	} else
		*val = 0;

	return rc;
}

static int cypd3177_dev_reset(struct cypd3177 *chip)
{
	int rc;

	rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_16BIT, RESET,
				RESET_DEV);
	if (rc < 0)
		dev_err(&chip->client->dev, "cypd3177 reset failed: %d\n", rc);

	return rc;
}

static int cypd3177_enable_vdm(struct cypd3177 *chip)
{
	int rc;

	rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_8BIT, VDM_EC_CONTROL,
				VDM_ENABLE);
	if (rc < 0)
		dev_err(&chip->client->dev, "cypd3177 enable VDM failed: %d\n", rc);

	return rc;
}

static int cypd3177_disable_vdm(struct cypd3177 *chip)
{
	int rc;

	rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_8BIT, VDM_EC_CONTROL,
				VDM_DISABLE);
	if (rc < 0)
		dev_err(&chip->client->dev, "cypd3177 disable VDM failed: %d\n", rc);

	return rc;
}

static int cypd3177_write_vdm_msg(struct cypd3177 *chip, const u8 *buf,
				size_t data_len)
{
	int rc;
	int count;

	/* Write VDM bytes */
	for (count = 0; count < data_len; count++) {
		rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_8BIT,
				WRITE_DATA_REGION + count, *buf);
		if (rc < 0) {
			dev_err(&chip->client->dev, "cypd3177 writing byte %d failed: %d\n",
				count+1, rc);
			return rc;
		}
		buf++;
	}

	return rc;
}

static int cypd3177_read_vdm_msg(struct cypd3177 *chip, enum pd_sop_type *sop,
				 u8 *buf, size_t data_len)
{
	int rc;
	int count;
	int raw;

	/* Read message header */
	for (count = 0; count < sizeof(u16); count++) {
		rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_8BIT,
			READ_DATA_REGION + count, &raw);
		if (rc < 0) {
			dev_err(&chip->client->dev, "cypd3177 reading header failed: %d\n", rc);
			return rc;
		}
		*buf = (u8) raw;
		buf++;
	}

	/* Read sop type */
	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_8BIT,
			READ_DATA_REGION + count, &raw);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 reading sop failed: %d\n", rc);
		return rc;
	}
	*sop = (u8) raw;

	/* Account for sop and reserved bytes */
	count += sizeof(u16);

	/* Read VDM bytes */
	for (; count < data_len; count++) {
		rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_8BIT,
				READ_DATA_REGION + count, &raw);
		if (rc < 0) {
			dev_err(&chip->client->dev, "cypd3177 reading byte %d failed: %d\n",
				count+1, rc);
			return rc;
		}
		*buf = (u8) raw;
		buf++;
	}

	return rc;
}

int cypd_phy_open(struct cypd_phy_params *params)
{
	struct cypd3177 *chip = __chip;
	int ret = 0;
	if (!chip) {
		pr_err("%s: Invalid handle\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&chip->state_lock);

	if (chip->is_opened) {
		dev_err(&chip->client->dev, "cypd3177 already opened\n");
		ret = -EBUSY;
		goto unlock_out;
	}

	cypd3177_enable_vdm(chip);

	chip->msg_rx_cb = params->msg_rx_cb;

	chip->is_opened = true;
unlock_out:
	mutex_unlock(&chip->state_lock);
	return ret;
}

void cypd_phy_close(void)
{
	struct cypd3177 *chip = __chip;

	if (!chip) {
		pr_err("%s: Invalid handle\n", __func__);
		return;
	}

	mutex_lock(&chip->state_lock);

	if (!chip->is_opened) {
		dev_err(&chip->client->dev, "cypd3177 not opened\n");
		goto unlock_out;
	}

	cypd3177_disable_vdm(chip);

	chip->msg_rx_cb = NULL;

	chip->is_opened = false;

unlock_out:
	mutex_unlock(&chip->state_lock);
}

int cypd_phy_write(u16 hdr, const u8 *data, size_t data_len, enum pd_sop_type sop)
{
	int rc = 0;
	u16 raw = 0;
	struct cypd3177 *chip = __chip;

	if (!chip) {
		pr_err("%s: Invalid handle\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&chip->state_lock);

	if (!chip->is_opened) {
		dev_err(&chip->client->dev, "cypd3177 not opened\n");
		rc = -ENODEV;
		goto unlock_out;
	}

	if (data_len > MAX_DATA_LENGTH) {
		dev_err(&chip->client->dev, "data length %d is > max %d supported\n",
			(int) data_len, MAX_DATA_LENGTH);
		rc = -EINVAL;
		goto unlock_out;
	}

	rc = cypd3177_write_vdm_msg(chip, data, data_len);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 write to data mem failed: %d\n",
			rc);
		goto unlock_out;
	}

	raw = DM_CTRL_MSG(sop, data_len);
	rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_16BIT, DM_CONTROL,
				raw);
	if (rc < 0)
		dev_err(&chip->client->dev, "cypd3177 write to DM_control failed: %d\n", rc);

unlock_out:
	mutex_unlock(&chip->state_lock);

	return rc;

}

static int cypd3177_set_sink_caps(struct cypd3177 *chip, int num_sink_caps) {
	int rc;
	u8 pdo_mask;

	rc = cypd3177_i2c_write_pdo_package(chip,
		WRITE_DATA_REGION);
	if (rc < 0) {
		dev_err(&chip->client->dev, "failed to write cypd3177 pdo package: %d\n",
			rc);
		return rc;
	}
	pdo_mask = PDO_MASK_MAX >> (TOTAL_CAPS - num_sink_caps);

	/* Enable PDOs */
	rc = cypd3177_i2c_write_reg(chip->client,
		CYPD3177_REG_8BIT, SELECT_SINK_PDO, pdo_mask);
	if (rc < 0) {
		dev_err(&chip->client->dev, "failed to enable cypd3177 sink pdo: %d\n",
			rc);
		return rc;
	}

	return rc;
}

static int cypd3177_set_5v_sink_caps(struct cypd3177 *chip) {
	int ret = 0;

	if (!chip->pd_attach)
		return -ENODEV;

	dev_dbg(&chip->client->dev, "%s: setting 5v sink caps num=%d",
		__func__, chip->num_5v_sink_caps);
	ret = cypd3177_set_sink_caps(chip, (int) chip->num_5v_sink_caps);

	if (!ret)
		chip->sink_cap_5v = true;
	else
		dev_err(&chip->client->dev, "%s: error(%d) setting 5V sink caps\n",
			__func__, ret);

	return ret;
}

static void cypd3177_sink_cap_work(struct work_struct *work) {
	struct cypd3177 *chip = container_of(work, struct cypd3177, sink_cap_work.work);
	union power_supply_propval val = {0};
	int ret;

	mutex_lock(&chip->state_lock);

	if (!chip->pd_attach) {
		dev_dbg(&chip->client->dev, "%s: no PD attached", __func__);
		goto relax_ws;
	}

	ret = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (ret) {
		dev_err(&chip->client->dev,
			"%s: power_supply_get_property returned error %d\n",
			__func__, ret);
		goto relax_ws;
	}

	dev_dbg(&chip->client->dev,
		"%s: current charge_status %d, new charge_status %d, sink_cap_5v %d\n",
		__func__, chip->charge_status, val.intval, chip->sink_cap_5v);

	if (val.intval == POWER_SUPPLY_STATUS_FULL && !chip->sink_cap_5v) {
		cypd3177_set_5v_sink_caps(chip);
	}

relax_ws:
	__pm_relax(chip->cypd_ws);
	mutex_unlock(&chip->state_lock);
}

static void pd_handle_typec_connect(struct cypd3177 *chip)
{
	if (chip->typec_status)
		return;

	chip->typec_status = true;
	chip->pdo_update_flag = false;
	chip->sink_cap_5v = false;
	power_supply_reg_notifier(&chip->psy_nb);

	if (!waiting_for_debounce(chip)) {
		chip->last_conn_time_jiffies = jiffies;
		power_supply_changed(chip->psy);
	}
}

static void pd_handle_typec_disconnect_or_reset(struct cypd3177 *chip)
{
	if (!chip->typec_status)
		return;

	chip->typec_status = false;
	chip->pd_attach = 0;
	chip->sink_cap_5v = false;
	dc_psy_set_icl(chip, 0);
	power_supply_unreg_notifier(&chip->psy_nb);
	power_supply_changed(chip->psy);
}

static void pd_handle_contract_complete(struct cypd3177 *chip)
{
	int icl_uA;
	chip->pd_attach = 1;

	/*
	 * If custom sink PDOs are defined and not be updated, when
	 * PD negotiation is completed at the first time, these custom
	 * PDOs need to be updated and enabled.
	 */
	if ((chip->num_sink_caps > 0) && (!chip->pdo_update_flag)) {
		dev_dbg(&chip->client->dev, "setting all custom sink caps");

		chip->pdo_update_flag = true;
		cypd3177_set_sink_caps(chip, chip->num_sink_caps);
	} else {
		/*
		 * cypd3177 is powered by dock. Everytime we reconnect, we need to
		 * update it with our custom PDOs. It initially negotations PDOs with
		 * it's default values. We then update it with our custom PDOs and it
		 * renegotates a new PDO. We only want to notify power_supply_updated
		 * after this second negotation.
		 */
		dev_dbg(&chip->client->dev, "final PD negotiation");
		if (!cypd3177_get_current_max(chip, &icl_uA))
			dc_psy_set_icl(chip, icl_uA);
		power_supply_changed(chip->psy);
	}
}

static void pd_handle_pd_port_busy(struct cypd3177 *chip)
{
	dev_dbg(&chip->client->dev, "pd_handle_pd_port_busy:reset\n");
	cypd3177_dev_reset(chip);
	chip->typec_status = false;
	chip->pd_attach = 0;
	chip->sink_cap_5v = false;
	power_supply_unreg_notifier(&chip->psy_nb);
	power_supply_changed(chip->psy);
}

static void pd_handle_vdm_received(struct cypd3177 *chip, int pd_raw)
{
	int rc, data_len = VDM_DATA_LEN(pd_raw);
	enum pd_sop_type sop;
	u8 buf[MAX_DATA_LENGTH];

	dev_dbg(&chip->client->dev, "VDM received: length:%d\n", data_len);
	if (data_len < sizeof(u16) || data_len > MAX_DATA_LENGTH) {
		dev_err(&chip->client->dev, "wrong data length %d\n",
			data_len);
		return;
	}
	rc = cypd3177_read_vdm_msg(chip, &sop, buf, data_len);
	if (rc < 0)
		return;

	/* Account for the trimmed sop type and reserved byte */
	data_len = data_len - sizeof(u16);

	print_hex_dump_debug("VDM msg:", DUMP_PREFIX_NONE, 32, 4, buf, data_len,
		false);

	/* Report to cypd policy engine */
	if (chip->msg_rx_cb)
		chip->msg_rx_cb(chip->cypd, sop, buf, data_len);

}

static int dev_handle_response(struct cypd3177 *chip)
{
	int raw;
	int rc;

	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_16BIT,
				DEV_RESPONSE, &raw);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 read dev response failed: %d\n",
			rc);
		return rc;
	}

	dev_dbg(&chip->client->dev, "cypd3177 received dev response: %d\n", raw);

	/* open event mask bit3 bit4 bit5 bit6 bit7 bit11 */
	rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_32BIT,
				EVENT_MASK, EVENT_MASK_OPEN);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 write event mask failed: %d\n",
			rc);
		return rc;
	}

	/* read and process initial typec status */
	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_32BIT,
				TYPE_C_STATUS, &raw);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 read type_c_status response failed: %d\n",
			rc);
		return rc;
	}
	if (((raw & TYPE_C_STATUS_ATTACHED_TYPE_MASK) >> TYPE_C_STATUS_ATTACHED_TYPE_SHIFT) == TYPE_C_STATUS_TYPE_SOURCE_VAL)
		pd_handle_typec_connect(chip);

	// read and process initial pd status
	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_32BIT,
				PD_STATUS, &raw);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 read pd_status response failed: %d\n",
			rc);
		return rc;
	}
	if (raw & PD_STATUS_CONTRACT_MASK)
		pd_handle_contract_complete(chip);

	return rc;
}

#define SINK_CAPS_DELAY_MS 200
static int psy_changed(struct notifier_block *nb, unsigned long evt, void *ptr) {
	struct cypd3177 *chip = container_of(nb, struct cypd3177, psy_nb);
	struct power_supply *psy = ptr;

	if (!chip->pd_attach || !chip->batt_psy ||
	    strcmp(psy->desc->name, "battery") != 0 ||
	    evt != PSY_EVENT_PROP_CHANGED) {
		return 0;
	}

	__pm_stay_awake(chip->cypd_ws);
	schedule_delayed_work(&chip->sink_cap_work, msecs_to_jiffies(SINK_CAPS_DELAY_MS));

	return 0;
}

static int pd_handle_response(struct cypd3177 *chip)
{
	int rc;
	int pd_raw, pd_event;

	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_32BIT,
				PD_RESPONSE, &pd_raw);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 read pd response failed: %d\n",
			rc);
		return rc;
	}
	dev_dbg(&chip->client->dev, "cypd3177 pd_raw = 0x%x\n", pd_raw);

	pd_event = pd_raw & 0xFF;
	switch (pd_event) {
	case TYPEC_CONNECT:
		pd_handle_typec_connect(chip);
		break;
	case PD_CONTRACT_COMPLETE:
		pd_handle_contract_complete(chip);
		break;
	case TYPEC_DISCONNECT:
	case HARD_RESET:
		pd_handle_typec_disconnect_or_reset(chip);
		break;
	case VDM_RECEIVED:
		pd_handle_vdm_received(chip, pd_raw);
		break;
	case PD_PORT_BUSY:
		if (chip->handle_port_busy)
			pd_handle_pd_port_busy(chip);
		break;
	default:
		break;
	}

	return rc;
}

static irqreturn_t cypd3177_hpi_irq_handler(int irq, void *dev_id)
{
	struct cypd3177 *chip = dev_id;
	int rc;
	int irq_raw;

	mutex_lock(&chip->state_lock);

	rc = cypd3177_i2c_read_reg(chip->client, CYPD3177_REG_8BIT,
				INTERRUPT, &irq_raw);
	if (rc < 0) {
		dev_err(&chip->client->dev, "cypd3177 read interrupt event failed: %d\n",
			rc);
		goto unlock_out;
	}

	if (irq_raw & DEV_EVENT)
		dev_handle_response(chip);

	if (irq_raw & PD_EVENT)
		pd_handle_response(chip);

	if (irq_raw & ~(DEV_EVENT | PD_EVENT))
		dev_err(&chip->client->dev, "cypd3177 read interrupt event irq_raw=%d\n",
			irq_raw);

	rc = cypd3177_i2c_write_reg(chip->client, CYPD3177_REG_8BIT,
			INTERRUPT, irq_raw);
	if (rc < 0)
		dev_err(&chip->client->dev, "cypd3177 write interrupt failed: %d\n",
			rc);

unlock_out:
	mutex_unlock(&chip->state_lock);

	/*
	 * From BCR HPI Guide:
	 * There can be a delay of up to 50 µsec from the time system controller clears the
	 * interrupt to the time the INTR signal is de-asserted. The system controller should
	 * take care to not sample the INTR signal in this period.
	 */
	usleep_range(50, 200);

	return IRQ_HANDLED;
}

static irqreturn_t cypd3177_fault_irq_handler(int irq, void *dev_id)
{
	struct cypd3177 *chip = dev_id;

	mutex_lock(&chip->state_lock);

	dev_info(&chip->client->dev, "cypd3177 input voltage fault (unplugged/undocked?)\n");

	if (!chip->typec_status)
		goto out;

	chip->typec_status = false;
	chip->pd_attach = 0;
	chip->sink_cap_5v = false;
	dc_psy_set_icl(chip, 0);
	power_supply_unreg_notifier(&chip->psy_nb);
	if (chip->psy)
		power_supply_changed(chip->psy);

out:
	mutex_unlock(&chip->state_lock);

	return IRQ_HANDLED;
}

static enum power_supply_property cypd3177_psy_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_STATUS,
};

static int cypd3177_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	int rc = 0, *val = &pval->intval;
	struct cypd3177 *chip = power_supply_get_drvdata(psy);

	mutex_lock(&chip->state_lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		rc = cypd3177_get_online(chip, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = cypd3177_get_voltage_now(chip, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = cypd3177_get_voltage_max(chip, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = cypd3177_get_current_max(chip, val);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		*val = chip->pd_attach ? POWER_SUPPLY_STATUS_CHARGING :
				POWER_SUPPLY_STATUS_NOT_CHARGING;
		rc = 0;
		break;
	default:
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		dev_err_ratelimited(chip->dev, "property %d unavailable: %d\n", psp, rc);
		rc = -ENODATA;
	}
	mutex_unlock(&chip->state_lock);

	return rc;
}

static const struct power_supply_desc cypd3177_psy_desc = {
	.name = "cypd3177",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = cypd3177_psy_props,
	.num_properties = ARRAY_SIZE(cypd3177_psy_props),
	.get_property = cypd3177_get_prop,
};

/*
 * Some android arch platforms backported wakelock APIs from kernel 5.4..0
 * Since their minor versions are changed in the Android R OS
 * Added defines for these platforms
 * 4.19.81 -> 4.19.110, 4.14.78 -> 4.14.170
 */
#if (defined(CONFIG_ARCH_QCOM) && (((LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 170)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0))) || (LINUX_VERSION_CODE >= \
	KERNEL_VERSION(4, 19, 110))))
#define WAKELOCK_BACKPORT
#endif

static int cypd3177_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	int rc;
	struct cypd3177 *chip;
	struct power_supply_config cfg = {0};
	int i;
	int pdo_voltage, pdo_current;
	u32 sink_caps[14];

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "I2C functionality not supported\n");
		return -ENODEV;
	}

	chip = devm_kzalloc(&i2c->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = i2c;
	chip->dev = &i2c->dev;
	i2c_set_clientdata(i2c, chip);

	chip->batt_psy = power_supply_get_by_name("battery");
	if (!chip->batt_psy) {
		dev_dbg(&i2c->dev, "cypd3177 failed to get batt_psy, defer probe\n");
		i2c_set_clientdata(i2c, NULL);
		return -EPROBE_DEFER;
	}

	if (device_property_read_bool(chip->dev, "cypd3177,set-icl")) {
		chip->dc_psy = power_supply_get_by_name("wireless");
		if (!chip->dc_psy) {
			dev_dbg(&i2c->dev, "cypd3177 failed to get dc_psy, defer probe\n");
			i2c_set_clientdata(i2c, NULL);
			return -EPROBE_DEFER;
		}
	}

	mutex_init(&chip->state_lock);

	/* Create PSY */
	cfg.drv_data = chip;
	cfg.of_node = chip->dev->of_node;

	chip->psy = devm_power_supply_register(chip->dev, &cypd3177_psy_desc,
			&cfg);
	if (IS_ERR(chip->psy)) {
		dev_err(&i2c->dev, "psy registration failed: %ld\n",
				PTR_ERR(chip->psy));
		rc = PTR_ERR(chip->psy);

		return rc;
	}

	chip->num_sink_caps = device_property_read_u32_array(chip->dev,
			"cypd3177,custom-sink-caps", NULL, 0);
	if (chip->num_sink_caps > 0) {
		if (chip->num_sink_caps % 2 || chip->num_sink_caps > MAX_NUM_SINK_CAPS) {
			dev_err(&i2c->dev, "default-sink-caps must be be specified as voltage/current, max 7 pairs\n");
			return -EINVAL;
		}

		rc = device_property_read_u32_array(chip->dev,
				"cypd3177,custom-sink-caps", sink_caps,
				chip->num_sink_caps);
		if (rc) {
			dev_err(&i2c->dev, "error reading default-sink-caps\n");
			return rc;
		}
		chip->num_sink_caps /= 2;

		for (i = 0; i < chip->num_sink_caps; i++) {
			pdo_voltage = sink_caps[i * 2] / 50;
			pdo_current = sink_caps[i * 2 + 1] / 10;

			chip->sink_caps[i] = PD_SNK_PDO_FIXED(0, 0, 0, 0, 0,
						pdo_voltage, pdo_current);
		}
	}

	rc = of_property_read_u32(chip->dev->of_node, "cypd3177,num-5v-sink-caps",
		&chip->num_5v_sink_caps);
	if (rc < 0) {
		dev_err(&i2c->dev,
			"cypd3177 failed reading cypd3177,num-5v-sink-caps, rc = %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(chip->dev->of_node, "cypd3177,initial-connection-debounce-ms",
		&chip->last_conn_debounce_ms);
	if (rc < 0) {
		dev_err(&i2c->dev,
			"cypd3177 failed reading initial-connection-debounce-ms, rc = %d\n", rc);
		return rc;
	}

	chip->handle_port_busy = device_property_read_bool(chip->dev,
			"cypd3177,handle-port-busy");

	cypd3177_dev_reset(chip);

	/* cypd could call back to us, so have reference ready */
	__chip = chip;

	chip->cypd = cypd_create(chip->dev);
	if (IS_ERR(chip->cypd)) {
		dev_err(&i2c->dev, "cypd_create failed: %ld\n",
				PTR_ERR(chip->cypd));
		return PTR_ERR(chip->cypd);
	}

	INIT_DELAYED_WORK(&chip->sink_cap_work, cypd3177_sink_cap_work);
	chip->psy_nb.notifier_call = psy_changed;

	chip->hpi_irq = of_irq_get_byname(chip->dev->of_node, "cypd3177_hpi_irq");
	rc = devm_request_threaded_irq(chip->dev,
				chip->hpi_irq,
				NULL, cypd3177_hpi_irq_handler,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"cypd3177_hpi_irq", chip);
	if (rc) {
		dev_err(&i2c->dev, "cypd3177 failed to request hpi interrupt\n");
		return rc;
	}

	chip->fault_irq = of_irq_get_byname(chip->dev->of_node, "cypd3177_fault_irq");
	rc = devm_request_threaded_irq(chip->dev,
				chip->fault_irq,
				NULL, cypd3177_fault_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"cypd3177_fault_irq", chip);
	if (rc) {
		dev_err(&i2c->dev, "cypd3177 failed to request fault interrupt\n");
		return rc;
	}

#if ((LINUX_VERSION_CODE  >= KERNEL_VERSION(5, 4, 0)) || defined(WAKELOCK_BACKPORT))
	chip->cypd_ws = wakeup_source_register(NULL, "cypd-ws");
#else
	chip->cypd_ws = wakeup_source_register("cypd-ws");
#endif
	dev_dbg(&i2c->dev, "cypd3177 probe successful\n");
	return 0;
}

static int cypd3177_remove(struct i2c_client *i2c)
{
	struct cypd3177 *chip = i2c_get_clientdata(i2c);

	wakeup_source_unregister(chip->cypd_ws);
	power_supply_unreg_notifier(&chip->psy_nb);
	mutex_destroy(&chip->state_lock);
	cypd_destroy(chip->cypd);
	return 0;
}

static int cypd3177_suspend(struct device *dev)
{
	struct cypd3177 *chip = (struct cypd3177 *) dev_get_drvdata(dev);

	disable_irq(chip->hpi_irq);
	disable_irq(chip->fault_irq);

	return 0;
}

static int cypd3177_resume(struct device *dev)
{
	struct cypd3177 *chip = (struct cypd3177 *) dev_get_drvdata(dev);
	int ret_val = 0;
	int rc;

	mutex_lock(&chip->state_lock);

	enable_irq(chip->fault_irq);
	enable_irq(chip->hpi_irq);

	/* check if we were previously online */
	if (!chip->typec_status)
		goto unlock_out;

	rc = cypd3177_get_online(chip, &ret_val);

	if (rc < 0 || ret_val == 0) {
		dev_dbg(&chip->client->dev, "cypd offline upon resume");
		chip->typec_status = false;
		chip->pd_attach = 0;
		chip->sink_cap_5v = false;

		dc_psy_set_icl(chip, 0);
		power_supply_changed(chip->psy);
	} else {
		dev_dbg(&chip->client->dev, "still online upon resume");
	}

unlock_out:
	mutex_unlock(&chip->state_lock);

	return 0;
}

static SIMPLE_DEV_PM_OPS(cypd3177_pm, cypd3177_suspend, cypd3177_resume);

static const struct of_device_id match_table[] = {
	{ .compatible = "cy,cypd3177", },
	{ }
};

static struct i2c_driver cypd3177_driver = {
	.driver = {
		.name = "cypd3177-driver",
		.pm = &cypd3177_pm,
		.of_match_table = match_table,
	},
	.probe =	cypd3177_probe,
	.remove =	cypd3177_remove,
};

module_i2c_driver(cypd3177_driver);

MODULE_DESCRIPTION("CYPD3177 driver");
MODULE_LICENSE("GPL");

