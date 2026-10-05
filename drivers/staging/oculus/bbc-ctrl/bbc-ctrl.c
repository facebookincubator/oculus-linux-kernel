// SPDX-License-Identifier: GPL-2.0-only
/**
 * This driver is responsible for starting up BBC(Battery Boost Circuit)
 * and setting up its control signals on the Oatmeal platform.
 */
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/extcon.h>

#define BBC_RAMP_VOLTAGE_STEP	50000	/* uV */

/*
 * When comparator enabled, send comp_set pulse to enable CHG
 * while battery charging current and vsys meet:
 * Ibatt < 100mA && VSYS > 3.5V
 */
#define I_BATT_TH		100000	/* uA */
#define V_SYS_TH		3500000	/* uV */

#define VOUTROOF_SETTING_UNIT	50000	/* uV */

/* Boost mode */
#define BOOST_HW_CONTROL_MODE	0x00
#define BOOST_PFM_AUTO_PWM_MODE 0x01
#define BOOST_FORCE_PWM_MODE	0x02
#define BOOST_PWM_AUTO_PFM_MODE	0x03

#define BATTERY_C_INI_REV	0x34

/*
 * Boost enable condition,
 * vsys250_avg < SWITCH_BOOST_VSYS_AVG,
 * or RSoC < SWITCH_BOOST_RSOC */
#define SWITCH_BOOST_VSYS_AVG   3600000 /* uV */
#define SWITCH_BOOST_RSOC       20

#define I2C_JITTER_DELAY_MS     500

enum {
	BOOST_MODE_SET_PWM = 0x01,
	BOOST_MODE_DISABLE_BOOST_EN,
};

struct bbc_ctrl {
	struct device *dev;
	struct notifier_block nb_psy;
	struct notifier_block nb_extcon;
	struct power_supply *usb_charger;
	struct power_supply *max17332_battery;
	struct gpio_desc *boost_nbyp_gpio;
	struct gpio_desc *nsw_en_gpio;
	struct gpio_desc *nshdn_en_gpio;
	struct gpio_desc *vsys_ref_gpio;
	struct work_struct notify_work;
	struct mutex	lock;
	struct regulator *tps_boost;
	struct regulator *max77789_regulator;
	struct iio_channel *vsys_chan;
	bool boost_state_en;
	bool charging_state;
	int rsoc_level;
	int boost_voutroof_th;
	int boost_th_vbat;
	int vsys_convert_coeff;
	u8 boost_th_rsoc;
	bool vbat_for_boost_th;
	bool is_optional_batt;
	bool enable_optional_batt;
	bool vsys_simulate_usb;
	bool has_extcon;
	struct extcon_dev *extcon_dev;
	bool boot_up;
	bool bbc_enabled;
};

static int gpio_init_helper(struct device *dev, struct gpio_desc **desc,
			const char *name, enum gpiod_flags flags)
{
	*desc = devm_gpiod_get(dev, name, flags);
	if (IS_ERR_OR_NULL(*desc)) {
		if (PTR_ERR(*desc) == -ENOENT)
			dev_warn(dev, "Could not find definition for %s gpio\n", name);
		else
			dev_err(dev, "Failed to acquire %s gpio\n", name);
		return PTR_ERR(*desc);
	}
	return 0;
}

static int get_vsys_avg(struct bbc_ctrl *bbc_data)
{
	int vsys_val;
	u64 vsys_avg;
	int vsys_sum = 0;
	int i = 0;
	int ret;

	for (; i < 10; i++) {
		ret = iio_read_channel_processed(bbc_data->vsys_chan, &vsys_val);
		if (ret < 0) {
			dev_err(bbc_data->dev, "Read vsys voltage failed %d", ret);
			return ret;
		}
		vsys_sum += vsys_val;
		msleep(10);
	}
	vsys_avg = vsys_sum / 10;
	/*
	 * Calculate vsys:
	 * Radc and R768 are connected parallel,
	 * the paralleled resistor is named Rpara,
	 * then it in series with R767
	 *
	 * R767=100Kohm, R768=220Kohm, Radc=10Mohm
	 *
	 * Rpara=R768*Radc/(R768+Radc)
	 * 	=220K*10M/(220K+10M/)
	 * 	=215264ohm
	 *
	 * vsys = vsys_adc*(R767+Rpara)/Rpara
	 *
	 * vsys = vsys_adc*(100k+215264)/215264
	 * 	= vsys_adc*1.464545
	 */
	vsys_avg *= bbc_data->vsys_convert_coeff;
	vsys_avg /= 1000000;

	/* convert vsys_avg to uV */
	vsys_avg *= 1000;
	return vsys_avg;
}

static int bbc_ctrl_ramp_voltage(struct bbc_ctrl *bbc_data,
			int vroof_set, int vroof_term)
{
	int ret = 0;

	if (vroof_set >= vroof_term) {
		dev_err(bbc_data->dev, "%s:vroof_set >= vroof_term \n",
				__func__);
		return ret;
	}
	while (vroof_set < vroof_term) {
		vroof_set += BBC_RAMP_VOLTAGE_STEP;
		ret = regulator_set_voltage(bbc_data->tps_boost, vroof_set, vroof_set);
		if (ret < 0) {
			dev_err(bbc_data->dev, "%s:Failed to set voutroof to %d: %d\n",
					__func__, vroof_set, ret);
		}
		msleep(10);
	}
	return ret;
}

static void bbc_control_set_boost_mode(struct bbc_ctrl *bbc_data,
					bool enable_boost)
{
	int vsys_avg;
	int vroof_set;
	int remainder;
	int ret;

	if (enable_boost) {
		/*
		 * Refer to "Battery Boost Control Algorithm (Meta proposal)":
		 * Enable and config BBC follow the sequence:
		 *	Set nSHDN = 0
		 *	Set nBYP = 0
		 *	Set boostEN = 1
		 *	Read vsys voltage with the ADS1115 (average of 10 samples)
		 *	Set VOUTFLOOR_TH = MIN (vsys, 3.75V)
		 *	Set nSW_EN = 1
		 *	Set nBYP = 1
		 *	Ramp VOUTFLOOR_TH until it reaches 3.75V (1 step/10ms)
		 * Add delays between the signals.
		 */
		if (bbc_data->boost_state_en)
			return;
		vsys_avg = get_vsys_avg(bbc_data);
		if (vsys_avg < 0) {
			dev_err(bbc_data->dev, "Get average vsys voltage failed");
			return;
		}
		/*
		 * It was found that, when vbat low and usb disconnected,
		 * boost enabled and boost vbat, it will change mode
		 * from FWM mode to PWM mode auto, the vbat will drop and device
		 * will shutdown.To avoid it, we change the mode to PWM before
		 * setting vroof voltage.
		 */
		ret = regulator_set_mode(bbc_data->tps_boost, BOOST_MODE_SET_PWM);
		if (ret) {
			dev_err(bbc_data->dev, "%s:Failed to set pwm mode: %d\n",
					__func__, ret);
		}
		/* We must set the voutroof to a multiple of 50mV */
		remainder = vsys_avg % VOUTROOF_SETTING_UNIT;
		if (remainder)
			vsys_avg = vsys_avg - remainder + VOUTROOF_SETTING_UNIT;
		vroof_set = min(vsys_avg, bbc_data->boost_voutroof_th);
		ret = regulator_set_voltage(bbc_data->tps_boost, vroof_set, vroof_set);
		if (ret < 0) {
			dev_err(bbc_data->dev, "%s:, Failed to set voutroof to %d: %d\n",
					__func__, vroof_set, ret);
		}
		gpiod_set_value_cansleep(bbc_data->nsw_en_gpio,1);
		msleep(50);
		gpiod_set_value_cansleep(bbc_data->boost_nbyp_gpio,1);
		msleep(10);
		/* if vsys_set < boost_voutroof_th, make it change slowly */
		if (vroof_set < bbc_data->boost_voutroof_th) {
			ret = bbc_ctrl_ramp_voltage(bbc_data, vroof_set, bbc_data->boost_voutroof_th);
			if (ret < 0)
				dev_err(bbc_data->dev, "%s:, Failed to ramp voutroof from %d to: %d\n",
					__func__, vroof_set, bbc_data->boost_voutroof_th);
		}
		bbc_data->boost_state_en = true;
		return;
	}
	if (!bbc_data->boost_state_en)
		return;
	gpiod_set_value_cansleep(bbc_data->nshdn_en_gpio,0);
	msleep(50);
	gpiod_set_value_cansleep(bbc_data->boost_nbyp_gpio,0);
	msleep(50);
	gpiod_set_value_cansleep(bbc_data->nsw_en_gpio,0);
	msleep(50);
	ret = regulator_set_mode(bbc_data->tps_boost, BOOST_MODE_DISABLE_BOOST_EN);
	if (ret) {
		dev_err(bbc_data->dev, "%s:Failed to disable boost_en: %d\n",
				__func__, ret);
	}
	bbc_data->boost_state_en = false;
	return;
}

static void bbc_ctrl_get_charging_state(struct bbc_ctrl *bbc_data)
{
	int ret;
	int curr_ilim;
	union power_supply_propval val;

	/*
	 * Refer "Battery Boost Control Algorithm (Meta proposal)":
	 * When RSoC < 15%:
	 * 	IF Host Current Capability > 0:
	 * 		we treat it as a charging state
	 * 	Else:
	 * 		we treat it as a discharging state
	 * We treat curr_capability == 0 when:
	 * 	usb disconnected
	 * 	or
	 * 	usb connected && curr_ilim < 500mA
	 *
	 * For factory test, we change boost threshold to VBAT,
	 * it changed to:
	 * When VBAT < 3.6V:
	 * 	IF Host Current Capability > 0:
	 * 		we treat it as a charging state
	 * 	Else:
	 * 		we treat it as a discharging state
	 * We treat curr_capability == 0 when:
	 * 	max77789 is disabled
	 * We treat curr_capability > 0 when:
	 * 	max77789 is enabled
	 */
	if (bbc_data->vsys_simulate_usb) {
		if (regulator_is_enabled(bbc_data->max77789_regulator))
			bbc_data->charging_state = true;
		else
			bbc_data->charging_state = false;
		return;
	}

	ret = power_supply_get_property(bbc_data->usb_charger,
			POWER_SUPPLY_PROP_ONLINE, &val);
	if (ret) {
		dev_err(bbc_data->dev, "Unable to read USB power supply status: %d\n", ret);
		return;
	}
	if (!val.intval) {
		bbc_data->charging_state = false;
		return;
	}
	curr_ilim = regulator_get_current_limit(bbc_data->max77789_regulator);
	if (curr_ilim < 500) {
		bbc_data->charging_state = false;
		return;
	}
	bbc_data->charging_state = true;
}

static void bbc_ctrl_send_comp_set_pulse(struct bbc_ctrl *bbc_data)
{
	gpiod_direction_output_raw(bbc_data->vsys_ref_gpio, 1);
	msleep(50);
	gpiod_direction_input(bbc_data->vsys_ref_gpio);
	dev_info(bbc_data->dev, "%s: bbc_ctrl send vsys_ref pulse done",
                        __func__);
}

static void bbc_ctrl_set_comparator_state(struct bbc_ctrl *bbc_data)
{
	int ret;
	int vsys_iio;
	u64 vsys_val;
	union power_supply_propval prop_max17332;

	ret = power_supply_get_property(bbc_data->max17332_battery,
			POWER_SUPPLY_PROP_CURRENT_NOW, &prop_max17332);

	if (ret) {
		dev_err(bbc_data->dev,
			"%s : Failed to get max17332 battery current_avg :%d\n", __func__, ret);
		return;
	}
	ret = iio_read_channel_processed(bbc_data->vsys_chan, &vsys_iio);
	if (ret < 0) {
		dev_err(bbc_data->dev, "Read vsys voltage failed %d", ret);
		return;
	}
	vsys_val = vsys_iio * bbc_data->vsys_convert_coeff;
	vsys_val /= 1000000;

        /* convert vsys_avg to uV */
	vsys_val *= 1000;
	/*
	 * If charging, enable comparator,
	 * If discharging, disable comparator
	 */
	if (bbc_data->charging_state) {
		/*
		 * When bootup with low battery level and charging,
		 * we should send a pulse to vsys_ref to ensure
		 * battery can be charged
		 * */
		gpiod_set_value_cansleep(bbc_data->nshdn_en_gpio, 1);
		if (bbc_data->boot_up) {
			bbc_ctrl_send_comp_set_pulse(bbc_data);
			return;
		}
		/*
		 * IF battery_current < 100mA and VSYS > 3.9V:
		 * 	Send COMP_SET pulse
		 */
		if (prop_max17332.intval < I_BATT_TH && vsys_val > V_SYS_TH) {
			msleep(50);
			bbc_ctrl_send_comp_set_pulse(bbc_data);
		}
		return;
	}
	gpiod_set_value_cansleep(bbc_data->nshdn_en_gpio, 0);
}

static void config_bbc_state(struct bbc_ctrl *bbc_data, bool enable_bbc)
{
	if (bbc_data->is_optional_batt && !bbc_data->enable_optional_batt) {
		dev_info(bbc_data->dev,
			"%s: optional battery, but did not enable BBC operation\n",
			__func__);
		return;
	}
	if (enable_bbc) {
		bbc_control_set_boost_mode(bbc_data, true);
		bbc_ctrl_get_charging_state(bbc_data);
		bbc_ctrl_set_comparator_state(bbc_data);
	} else {
		bbc_control_set_boost_mode(bbc_data, false);
	}
}

static void notify_work(struct work_struct *work)
{
	int vsys_avg, ret;
	union power_supply_propval prop_max17332;
	struct bbc_ctrl *bbc_data =
		container_of(work, struct bbc_ctrl, notify_work);

	if (bbc_data->is_optional_batt && !bbc_data->enable_optional_batt) {
		dev_info(bbc_data->dev,
			"%s: optional battery, but did not enable BBC operation\n",
			__func__);
		return;
	}
	/*
	 * Refer "Battery Boost Control Algorithm (Meta proposal)":
	 *
	 * When any of these events happen:
	 * 1.During power on
	 * 2.Receive an alert from MAX17332 indicating a RSoC change
	 * 3.Cable is inserted
	 * 4.Cable is removed
	 *
	 * We check the RSoC(check VBat in factory test), vsys250_avg,
	 * and charging state, then set the boost state as below:
	 * If RSoC > 20:
	 * 	Enable power switch, disable boost
	 * If RSoC < 15 or vsys250_avg < 3.6V:
	 * 	Set boost auto mode, disable power switch
	 * 	If charging:
	 * 		Enable comparator(max40009)
	 * 	If discharging:
	 * 		Disable comparator(max40009)
	 * Else(15 < RSoC < 20, vsys250_avg > 3.6V):
	 * 	If last state is boost enabled:
	 * 		If charging:
	 * 			Enable comparator(max40009)
	 * 		If discharging:
	 * 			Disable comparator(max40009)
	 * 	If last state is boost disabled:
	 * 		Ignore.
	 */
	if (bbc_data->vbat_for_boost_th) {
		ret = power_supply_get_property(bbc_data->max17332_battery,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop_max17332);
		if (ret) {
			dev_err(bbc_data->dev,
				"%s : Failed to get max17332 battery voltage :%d\n",
				__func__, ret);
			return;
		}
		mutex_lock(&bbc_data->lock);
		config_bbc_state(bbc_data, (prop_max17332.intval < bbc_data->boost_th_vbat));
		mutex_unlock(&bbc_data->lock);
		return;
	} else {
		ret = power_supply_get_property(bbc_data->max17332_battery,
			POWER_SUPPLY_PROP_CAPACITY, &prop_max17332);
		if (ret) {
			dev_err(bbc_data->dev,
				"%s : Failed to get max17332 battery capacity :%d\n",
				__func__, ret);
			return;
		}
		mutex_lock(&bbc_data->lock);
		if (prop_max17332.intval > SWITCH_BOOST_RSOC) {
			config_bbc_state(bbc_data, false);
			if (bbc_data->boot_up)
				bbc_data->boot_up = false;
			mutex_unlock(&bbc_data->lock);
			return;
		}
		vsys_avg = get_vsys_avg(bbc_data);
		if (vsys_avg < 0) {
			dev_err(bbc_data->dev, "%s:Get average vsys voltage failed: %d",
					__func__, vsys_avg);
			return;
		}
		if ((vsys_avg < SWITCH_BOOST_VSYS_AVG) ||
				(prop_max17332.intval < bbc_data->boost_th_rsoc)) {
			config_bbc_state(bbc_data, true);
		} else {
			if (bbc_data->boost_state_en) {
				bbc_ctrl_get_charging_state(bbc_data);
				bbc_ctrl_set_comparator_state(bbc_data);
			}
		}
		if (bbc_data->boot_up)
			bbc_data->boot_up = false;
		mutex_unlock(&bbc_data->lock);
	}
}

static int bbc_control_psy_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *ptr)
{
	struct power_supply *psy = ptr;
	struct bbc_ctrl *bbc_data = NULL;
	union power_supply_propval prop_max17332;
	int ret;

	if (IS_ERR_OR_NULL(nb))
		return NOTIFY_BAD;

	bbc_data = container_of(nb, struct bbc_ctrl, nb_psy);
	if (IS_ERR_OR_NULL(bbc_data) ||
			IS_ERR_OR_NULL(bbc_data->max17332_battery)) {
		dev_err(bbc_data->dev, "PSY notifier provided object handle is invalid\n");
		return NOTIFY_BAD;
	}
	/* Only process max17332 status change events */
	if (psy != bbc_data->max17332_battery && ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_DONE;

	/* For max17332 psy, we only processed RSoC changed */
	ret = power_supply_get_property(bbc_data->max17332_battery,
		POWER_SUPPLY_PROP_CAPACITY, &prop_max17332);
	if (ret) {
		dev_err(bbc_data->dev,
			"%s : Failed to get max17332 battery capacity :%d\n",
			__func__, ret);
		return NOTIFY_OK;
	}
	if (bbc_data->rsoc_level == prop_max17332.intval)
		return NOTIFY_OK;
	else
		bbc_data->rsoc_level = prop_max17332.intval;

	/* when usb disconnected, and RSoC changed
	 * wake up system to read vsys */
	ret = power_supply_get_property(bbc_data->usb_charger,
		POWER_SUPPLY_PROP_ONLINE, &prop_max17332);
	if (!ret && !prop_max17332.intval)
		pm_wakeup_event(bbc_data->dev, I2C_JITTER_DELAY_MS);

	schedule_work(&bbc_data->notify_work);
	return NOTIFY_OK;
}

static int usbd_connect_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *ptr)
{
	struct extcon_dev *edev = ptr;
	struct bbc_ctrl *bbc_data = NULL;

	if (IS_ERR_OR_NULL(nb))
		return NOTIFY_BAD;

	bbc_data = container_of(nb, struct bbc_ctrl, nb_extcon);

	if (!bbc_data->has_extcon)
		return NOTIFY_BAD;

	if (!extcon_get_state(edev, EXTCON_USB)) {
		if (bbc_data->boost_state_en)
			gpiod_set_value_cansleep(bbc_data->nshdn_en_gpio, 0);
	} else
		schedule_work(&bbc_data->notify_work);
	return NOTIFY_OK;
}

static int bbc_ctrl_parse_dt(struct bbc_ctrl *data)
{
	int ret;
	const char *temp_string = NULL;

	/* get gpio state */
	ret = gpio_init_helper(data->dev, &data->boost_nbyp_gpio, "boost-nbyp",
							GPIOD_OUT_LOW);
	if (ret) {
		dev_err(data->dev, "failed to init BOOST-NBYP gpio: %d", ret);
		return ret;
	}
	ret = gpio_init_helper(data->dev, &data->nsw_en_gpio, "nsw-en",
							GPIOD_OUT_LOW);
	if (ret) {
		dev_err(data->dev, "failed to init NLS-EN gpio: %d", ret);
		return ret;
	}
	ret = gpio_init_helper(data->dev, &data->nshdn_en_gpio, "nshdn-en",
							GPIOD_OUT_LOW);
	if (ret) {
		dev_err(data->dev, "failed to init NSHDN-EN gpio: %d", ret);
		return ret;
	}
	ret = gpio_init_helper(data->dev, &data->vsys_ref_gpio, "vsys-ref",
							GPIOD_IN);
	if (ret) {
		dev_err(data->dev, "failed to init VSYS-REF gpio: %d", ret);
		return ret;
	}

	/* get vsys voltage channel */
	ret = of_property_read_string(data->dev->of_node, "io-channel-names",
			&temp_string);
	if (ret < 0) {
		dev_err(data->dev, "Failed to read vsys io-channel-names: %d",
				ret);
		return ret;
	}

	data->vsys_chan = iio_channel_get(data->dev, temp_string);
	if (IS_ERR(data->vsys_chan)) {
		dev_err(data->dev, "channel %s iio_channel_get error: %ld",
			temp_string, PTR_ERR(data->vsys_chan));
		return ret;
	}

	/* get boost voutroof */
	ret = of_property_read_u32(data->dev->of_node, "boost-voutroof-th-uv",
			&data->boost_voutroof_th);
	if (ret) {
		dev_err(data->dev, "Failed to read boost-voutroof-th-uv: %d\n",
			ret);
		return ret;
	}

	/* for factory test, check vbat and decide whether enable boost */
	ret = of_property_read_u32(data->dev->of_node, "boost-th-battery-voltage-uv",
			&data->boost_th_vbat);
	if (ret) {
		dev_err(data->dev, "Failed to read boost-th-battery-voltage-uv: %d\n",
			ret);
		return ret;
	}

	ret = of_property_read_u8(data->dev->of_node, "boost-th-rsoc",
			&data->boost_th_rsoc);
	if (ret) {
		dev_err(data->dev, "Failed to read boost-th-rsoc: %d\n",
			ret);
		return ret;
	}

	/* get vsys convert coefficient */
	ret = of_property_read_u32(data->dev->of_node, "vsys-convert-coeff",
			&data->vsys_convert_coeff);
	if (ret) {
		dev_err(data->dev, "Failed to read vsys-convert-coeff: %d\n",
			ret);
		return ret;
	}

	return 0;
}

static ssize_t enable_check_vbat_for_boost_th_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bbc_ctrl *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", data->vbat_for_boost_th);
}

static ssize_t enable_check_vbat_for_boost_th_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bbc_ctrl *data = dev_get_drvdata(dev);
	int ret, val;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;

	data->vbat_for_boost_th = val;
	return count;
}
static DEVICE_ATTR_RW(enable_check_vbat_for_boost_th);

static ssize_t enable_vsys_for_factory_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val;
	struct bbc_ctrl *pdata = dev_get_drvdata(dev);
	val = regulator_is_enabled(pdata->max77789_regulator);
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t enable_vsys_for_factory_test_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret, val;
	struct bbc_ctrl *pdata = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;

	if (val) {
		if (!regulator_is_enabled(pdata->max77789_regulator)) {
			ret = regulator_enable(pdata->max77789_regulator);
			if (ret < 0) {
				dev_err(pdata->dev, "Failed to enable max77789: %d\n",
						ret);
				return ret;
			}
		}
	} else {
		if (regulator_is_enabled(pdata->max77789_regulator)) {
			ret = regulator_disable(pdata->max77789_regulator);
			if (ret < 0) {
				dev_err(pdata->dev, "Failed to disable max77789: %d\n",
						ret);
				return ret;
			}
		}
	}
	/* Add delay time to make vbat reading accurately*/
	msleep(1000);
	schedule_work(&pdata->notify_work);
	return count;
}
static DEVICE_ATTR_RW(enable_vsys_for_factory_test);

static ssize_t boost_th_vbat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bbc_ctrl *pdata = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->boost_th_vbat);
}

static ssize_t boost_th_vbat_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int val, ret;
	struct bbc_ctrl *pdata = dev_get_drvdata(dev);
	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;
	pdata->boost_th_vbat = val;
	return count;
}
static DEVICE_ATTR_RW(boost_th_vbat);

static ssize_t boost_th_rsoc_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        struct bbc_ctrl *pdata = dev_get_drvdata(dev);
        return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->boost_th_rsoc);
}

static ssize_t boost_th_rsoc_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
        int val, ret;
        struct bbc_ctrl *pdata = dev_get_drvdata(dev);
        ret = kstrtoint(buf, 0, &val);
        if (ret)
                return ret;
        pdata->boost_th_rsoc = val;
        return count;
}
static DEVICE_ATTR_RW(boost_th_rsoc);

static ssize_t boost_voutroof_th_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        struct bbc_ctrl *pdata = dev_get_drvdata(dev);
        return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->boost_voutroof_th);
}

static ssize_t boost_voutroof_th_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
	int val, ret;
	struct bbc_ctrl *pdata = dev_get_drvdata(dev);
	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;

	/* check if boost enabled */
	if (!pdata->boost_state_en) {
		dev_err(pdata->dev, "%s:boost disabled, can't set voutroof\n",
				__func__);
		return ret;
	}
	ret = regulator_set_voltage(pdata->tps_boost, val, val);
	if (ret < 0) {
		dev_err(pdata->dev, "%s:Failed to set voutroof to %d: %d\n",
			 __func__, val, ret);
		return ret;
	}
	pdata->boost_voutroof_th = val;
	return count;
}
static DEVICE_ATTR_RW(boost_voutroof_th);

static ssize_t boost_state_en_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct bbc_ctrl *pdata = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->boost_state_en);
}

static ssize_t boost_state_en_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
	int ret;
	bool val;

	struct bbc_ctrl *pdata = dev_get_drvdata(dev);
	ret = kstrtobool(buf, &val);
	if (ret)
		return ret;
	config_bbc_state(pdata, val);
	return count;
}
static DEVICE_ATTR_RW(boost_state_en);

static ssize_t optional_batt_enable_bbc_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        struct bbc_ctrl *pdata = dev_get_drvdata(dev);
        return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->enable_optional_batt);
}

static ssize_t optional_batt_enable_bbc_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
	int ret;
	bool val;

	struct bbc_ctrl *pdata = dev_get_drvdata(dev);
	ret = kstrtobool(buf, &val);
	if (ret)
		return ret;
	if (val) {
		//if enable bbc, first set state, then launch notify_work
		pdata->enable_optional_batt = val;
		schedule_work(&pdata->notify_work);
	} else {
		//if disable bbc, first switch to ps, then set state
		config_bbc_state(pdata, false);
		pdata->enable_optional_batt = val;
	}
	return count;
}
static DEVICE_ATTR_RW(optional_batt_enable_bbc);

static ssize_t vsys_simulate_usb_en_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        struct bbc_ctrl *pdata = dev_get_drvdata(dev);
        return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->vsys_simulate_usb);
}

static ssize_t vsys_simulate_usb_en_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
        int ret;
        bool val;

        struct bbc_ctrl *pdata = dev_get_drvdata(dev);
        ret = kstrtobool(buf, &val);
        if (ret)
                return ret;
        pdata->vsys_simulate_usb = val;
        return count;
}
static DEVICE_ATTR_RW(vsys_simulate_usb_en);

static void bbc_create_attr(struct bbc_ctrl *data)
{
	/* Add sysfs node for switch boost threshold between vbat and RSoC */
	device_create_file(data->dev, &dev_attr_enable_check_vbat_for_boost_th);
	/*
	 * This sysfs node is for factory test, we can not plug\unplug
	 * usb when test BBC. So we simulate usb disconnect\concect by
	 * disable\enable vsys. The sysfs is for switching vsys between
	 * enable and disable and then config BBC state.
	 * Default state is enable vsys */
	device_create_file(data->dev, &dev_attr_enable_vsys_for_factory_test);
	/* For modify boost threshold of vbat */
	device_create_file(data->dev, &dev_attr_boost_th_vbat);
	/* For modify boost threshold of rsoc */
	device_create_file(data->dev, &dev_attr_boost_th_rsoc);
	/* For modify boost voutroof */
	device_create_file(data->dev, &dev_attr_boost_voutroof_th);
	/* For manually change boost enable\disable state */
	device_create_file(data->dev, &dev_attr_boost_state_en);
	/* For enable\disable operation of BBC with optional battery */
	device_create_file(data->dev, &dev_attr_optional_batt_enable_bbc);
	/* This is for enable\disable using vsys(max77789 vout) to
	 * simulate usb connction
	 *
	 * if enable it, we can enable\disable "enable_vsys_for_factory_test"
	 * to simulate usb connection and disconnection
	 *
	 * if disable it, we just use old method to decide whether usb
	 * is connected or not */
	device_create_file(data->dev, &dev_attr_vsys_simulate_usb_en);
}
static int bbc_ctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bbc_ctrl *pdata;
	union power_supply_propval prop;
	u16 *default_support_bbc_ini_rev_data = NULL;
	u16 *optional_support_bbc_ini_rev_data = NULL;
	struct property *ini_prop = NULL;
	int i, ret, ini_num;


	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->dev = dev;

	pdata->usb_charger = power_supply_get_by_name("usb-charger");
	if (IS_ERR_OR_NULL(pdata->usb_charger)) {
		dev_err(dev,
			"%s : Failed to find usb power supply device\n", __func__);
		ret = -EPROBE_DEFER;
		return ret;
	}

	pdata->max17332_battery = power_supply_get_by_name("max17332-battery");
	if (IS_ERR_OR_NULL(pdata->max17332_battery)) {
		dev_err(dev,
			"%s : Failed to find max17332_battery power supply device\n", __func__);
		ret = -EPROBE_DEFER;
		return ret;
	}

	ret = bbc_ctrl_parse_dt(pdata);
	if (ret != 0) {
		dev_err(dev,
			"%s : Failed to parse dt:%d\n", __func__, ret);
		return ret;
	}

	dev_set_drvdata(dev, pdata);
	mutex_init(&pdata->lock);
	ret = power_supply_get_property(pdata->max17332_battery,
			POWER_SUPPLY_PROP_INI_VERSION, &prop);
	if (ret) {
		dev_err(dev,
			"%s : Failed to get ini version :%d\n", __func__, ret);
		goto probe_done;
	}

	ini_prop = of_find_property(pdata->dev->of_node,
			"default-support-bbc-batt-ini", NULL);
	if (ini_prop == NULL || ini_prop->length <= 0) {
		dev_err(pdata->dev, "Failed to get default support bbc battery ini: %d", ret);
		return -EINVAL;
	}

	ini_num = ini_prop->length / 2;
	default_support_bbc_ini_rev_data = devm_kzalloc(dev, ini_num * sizeof(u16), GFP_KERNEL);
	if (default_support_bbc_ini_rev_data == NULL) {
		dev_err(dev, "%s: failed to alloc memory %d\n", __func__, ret);
		return -ENOMEM;
	}

	ret = of_property_read_u16_array(pdata->dev->of_node, "default-support-bbc-batt-ini",
			default_support_bbc_ini_rev_data, ini_num);
	if (ret < 0) {
		dev_err(pdata->dev, "Failed to get default support bbc battery ini array: %d", ret);
		return ret;
	}

	ini_prop = of_find_property(pdata->dev->of_node,
			"optional-support-bbc-batt-ini", NULL);
	if (ini_prop == NULL || ini_prop->length <= 0) {
		dev_err(pdata->dev, "Failed to get optional support bbc battery ini: %d", ret);
		return -EINVAL;
	}

	ini_num = ini_prop->length / 2;
	optional_support_bbc_ini_rev_data = devm_kzalloc(dev, ini_num * sizeof(u16), GFP_KERNEL);
	if (optional_support_bbc_ini_rev_data == NULL) {
		dev_err(dev, "%s: failed to alloc memory %d\n", __func__, ret);
		return -ENOMEM;
	}

	ret = of_property_read_u16_array(pdata->dev->of_node, "optional-support-bbc-batt-ini",
			optional_support_bbc_ini_rev_data, ini_num);
	if (ret < 0) {
		dev_err(pdata->dev, "Failed to get optional support bbc battery ini array: %d", ret);
		return ret;
	}
	pdata->is_optional_batt = false;
	for (i = 0; i < ini_num; i++) {
		if (optional_support_bbc_ini_rev_data[i] == prop.intval) {
			pdata->is_optional_batt	= true;
			break;
		}
	}
	pdata->bbc_enabled = false;
	for (i = 0; i < ini_num; i++) {
		if (default_support_bbc_ini_rev_data[i] == prop.intval) {
			pdata->bbc_enabled = true;
			break;
		}
	}

	if (!pdata->bbc_enabled && !pdata->is_optional_batt) {
		dev_info(dev,
			"battery ini not match, stop probing bbc driver\n");
		goto probe_done;
	}
	ret = power_supply_get_property(pdata->max17332_battery,
			POWER_SUPPLY_PROP_CAPACITY, &prop);
	if (ret) {
		dev_err(dev,
			"%s : Failed to get battery capacity :%d\n",__func__, ret);
		return ret;
	}
	pdata->rsoc_level = prop.intval;

	pdata->nb_psy.notifier_call = bbc_control_psy_notifier_call;
	ret = power_supply_reg_notifier(&pdata->nb_psy);
	if (ret) {
		dev_err(dev,
			"%s : Failed to register psy notifier\n", __func__);
		return ret;
	}
	pdata->has_extcon = of_property_read_bool(pdata->dev->of_node, "extcon");
	if (!pdata->has_extcon) {
		dev_err(pdata->dev, "%s: extcon not exist\n", __func__);
		goto probe_done;
	}
	pdata->extcon_dev = extcon_get_edev_by_phandle(pdata->dev, 0);
	pdata->nb_extcon.notifier_call = usbd_connect_notifier_call;
	ret = extcon_register_notifier(pdata->extcon_dev, EXTCON_USB,
			&pdata->nb_extcon);

	INIT_WORK(&pdata->notify_work, notify_work);
	pdata->tps_boost = devm_regulator_get(pdata->dev, "boost");
	if (IS_ERR(pdata->tps_boost)) {
		ret = PTR_ERR(pdata->tps_boost);
		dev_err(dev, "Failed to get TPS61280 regulator: %d\n", ret);
		ret = -EPROBE_DEFER;
		return ret;
	}
	pdata->max77789_regulator = devm_regulator_get(pdata->dev, "vout");
	if (IS_ERR(pdata->max77789_regulator)) {
		ret = PTR_ERR(pdata->max77789_regulator);
		dev_err(dev, "Failed to get max77789 regulator: %d\n", ret);
		ret = -EPROBE_DEFER;
		return ret;
	}

	pdata->boost_state_en = false;
	pdata->enable_optional_batt = false;
	pdata->vbat_for_boost_th = false;
	pdata->vsys_simulate_usb = false;
	pdata->boot_up = true;

	/* set device able to wake up system */
	device_init_wakeup(pdata->dev, true);
	schedule_work(&pdata->notify_work);
	bbc_create_attr(pdata);
	dev_info(dev, "BBC driver probe done\n");
probe_done:
	return 0;
};

static void bbc_ctrl_shutdown(struct platform_device *pdev)
{
	struct bbc_ctrl *pdata = platform_get_drvdata(pdev);

	if (!pdata->bbc_enabled && !pdata->is_optional_batt)
		return;
	/* Enable power switch while system off */
	bbc_control_set_boost_mode(pdata, false);
};

static const struct of_device_id bbc_ctrl_of_match[] = {
	{.compatible = "meta,bbc-ctrl"},
	{},
};

MODULE_DEVICE_TABLE(of, bbc_ctrl_of_match);

static struct platform_driver bbc_ctrl_driver = {
	.driver = {
		.name = "bbc_ctrl",
		.of_match_table = of_match_ptr(bbc_ctrl_of_match),
	},
	.probe = bbc_ctrl_probe,
	.shutdown = bbc_ctrl_shutdown,
};

module_platform_driver(bbc_ctrl_driver);

MODULE_DESCRIPTION("BBC control driver");
MODULE_LICENSE("GPL v2");
