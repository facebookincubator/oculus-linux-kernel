#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscfifo.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <uapi/linux/oculus/lis2mdl.h>

#define REG_WHO_AM_I			0x4f
#define VAL_WHO_AM_I			0x40

#define I2C_AUTO_INCREMENT	0x80
#define I2C_PROBE_RETRY_MAX	5

const size_t BUF_SZ_RX = 16;
const size_t BUF_SZ_TX = 16;

struct regulator_info {
	const char *name;
	struct regulator *regulator;
	u32 min_uV; /* in uV */
	u32 max_uV; /* in uV */
};

struct lis2mdl_device {
	struct device *dev; /* shortcut to misc device */
	struct i2c_client *client;
	struct miscdevice misc;
	struct miscfifo mf;

	int irq_gpio;
	enum of_gpio_flags irq_flags;

	struct regulator_info vdd;
	struct regulator_info vio;

	atomic_long_t irq_timestamp;

	struct {
		struct mutex lock;
		struct lis2mdl_interrupt_config interrupt_config;
		struct lis2mdl_pm_config suspend_cfg;
		struct lis2mdl_pm_config resume_cfg;
	} state;

	struct {
		struct mutex lock;
		/* DMA safe buffers for I2C transfer */
		u8 *rx_buf;
		u8 *tx_buf;
	} io;
};

enum reg_op_type {
	REG_OP_READ,
	REG_OP_WRITE,
};

static inline struct lis2mdl_device *from_miscdevice(struct miscdevice *p)
{
	return container_of(p, struct lis2mdl_device, misc);
}

static inline struct lis2mdl_device *from_miscfifo(struct miscfifo *p)
{
	return container_of(p, struct lis2mdl_device, mf);
}

static inline struct lis2mdl_device *from_i2c_client(struct i2c_client *client)
{
	return i2c_get_clientdata(client);
}

/**
 * Reads len bytes from the device and return a pointer to the read data.
 * Must call with ddata->io.lock held
 *
 * @param  ddata driver instance
 * @param  addr  register address
 * @param  len   length of data to read
 * @return	   pointer to data or error pointer
 */
static u8 *i2c_reg_read_locked(struct lis2mdl_device *ddata,
				u8 addr, size_t len)
{
	int rc;
	u8 addr_out = addr | I2C_AUTO_INCREMENT;
	struct i2c_msg xfers[] = {
		{
			.addr = ddata->client->addr,
			.flags = ddata->client->flags,
			.len = 1,
			.buf = &addr_out,
		},
		{
			.addr = ddata->client->addr,
			.flags = ddata->client->flags | I2C_M_RD,
			.len = len,
			.buf = ddata->io.rx_buf,
		}
	};

	if (WARN_ON(len > BUF_SZ_RX)) {
		dev_err(ddata->dev, "invalid read size: %zu", len);
		return ERR_PTR(-EINVAL);
	}

	rc = i2c_transfer(ddata->client->adapter, xfers, 2);

	if (rc < 0)
		return ERR_PTR(rc);

	if (rc != 2) {
		dev_err(ddata->dev, "i2c_transfer returned %d", rc);
		return ERR_PTR(-EIO);
	}

	return ddata->io.rx_buf;
}

/**
 * Write len bytes to the device
 * Must call with ddata->io.lock held
 *
 * @param  ddata pointer to device instance
 * @param  addr  register address
 * @param  buf   data to write
 * @param  len   length of data to write
 * @param  usr   if true, buf is from user
 * @return	   0 on success -errno
 */
static int i2c_reg_write_usr_locked(struct lis2mdl_device *ddata,
				u8 addr, const u8 *buf, size_t len, bool usr)
{

	int rc;
	struct i2c_msg xfer = {
		.addr = ddata->client->addr,
		.flags = ddata->client->flags,
		.len = len + 1,
		.buf = ddata->io.tx_buf,
	};

	dev_vdbg(ddata->dev, "write addr:%02x, len:%zu data: %*phN\n",
			addr, len, (int) len, buf);
	if (WARN_ON(len > (BUF_SZ_TX - 1))) {
		dev_err(ddata->dev, "invalid write size: %zu", len);
		return -EINVAL;
	}

	ddata->io.tx_buf[0] = addr;
	if (!usr) {
		memcpy(&ddata->io.tx_buf[1], buf, len);
	} else {
		rc = copy_from_user(&ddata->io.tx_buf[1], buf, len);
		if (rc)
			return -EFAULT;
	}

	rc = i2c_transfer(ddata->client->adapter, &xfer, 1);
	if (rc < 0)
		return rc;

	if (rc == 1)
		return 0;

	return -EIO;
}

/**
 * Read the register at addr and return it's value
 * @param  ddata driver instance
 * @param  addr  register address
 * @return	   error if < 0, or register value
 */
static int i2c_reg_read1_locked(struct lis2mdl_device *ddata, u8 addr)
{
	u8 *buf = i2c_reg_read_locked(ddata, addr, 1);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	return buf[0];
}

static int probe_i2c_bus(struct lis2mdl_device *ddata)
{
	int rc;

	mutex_lock(&ddata->io.lock);
	rc = i2c_reg_read1_locked(ddata, REG_WHO_AM_I);
	if (rc < 0) {
		dev_err(ddata->dev,
			"failed probing device (could not read WAI): %d", rc);
		goto exit;
	}

	if (rc != VAL_WHO_AM_I) {
		dev_err(ddata->dev,
			"failed probing device (WAI value does not match, expect: %02x, got: %02x)",
			VAL_WHO_AM_I, rc);
		rc = -ENODEV;
		goto exit;
	}
	rc = 0;
	dev_dbg(ddata->dev, "Probe OK");

exit:
	mutex_unlock(&ddata->io.lock);
	return rc;
}

static int set_interrupt_config_locked(struct lis2mdl_device *ddata,
				struct lis2mdl_interrupt_config *cfg)
{
	if (cfg->mode == LIS2MDL_INTR_DISABLED) {
		if (ddata->state.interrupt_config.mode == LIS2MDL_INTR_DISABLED)
			return 0;

		disable_irq(ddata->client->irq);
		ddata->state.interrupt_config = *cfg;
		dev_dbg(ddata->dev, "disable interrupts\n");
		return 0;
	}

	if (cfg->mode == LIS2MDL_INTR_READ_RANGE) {
		if (ddata->state.interrupt_config.mode != LIS2MDL_INTR_DISABLED)
			return -EBUSY;

		if (cfg->range.len == 0 || cfg->range.len > BUF_SZ_RX)
			return -EINVAL;

		dev_dbg(ddata->dev, "enable interrupts: read addr:%02x len:%d\n",
				cfg->range.reg, (unsigned) cfg->range.len);
		ddata->state.interrupt_config = *cfg;
		enable_irq(ddata->client->irq);
		return 0;
	}

	return -EINVAL;
}

static irqreturn_t isr_primary(int irq, void *p)
{
	struct lis2mdl_device *ddata = p;

	atomic_long_set(&ddata->irq_timestamp, ktime_get_ns());

	return IRQ_WAKE_THREAD;
}

static irqreturn_t isr_thread_fn(int irq, void *p)
{
	struct lis2mdl_device *ddata = p;

	int rc;
	u64 irq_timestamp = atomic_long_read(&ddata->irq_timestamp);
	/* ddata->state.interrupt_config will not change while the
		 isr is pending */
	const struct lis2mdl_interrupt_config *cfg =
		&ddata->state.interrupt_config;


	if (cfg->mode == LIS2MDL_INTR_DISABLED) {
		dev_err_ratelimited(ddata->dev,
					 "received isr, but intr config is disabled\n");
		goto exit;
	}

	if (cfg->mode == LIS2MDL_INTR_READ_RANGE) {
		u8 *buf;
		struct lis2mdl_event event = {
			.timestamp = irq_timestamp,
			.len = cfg->range.len
		};

		mutex_lock(&ddata->io.lock);
		buf = i2c_reg_read_locked(
			ddata, cfg->range.reg, cfg->range.len);
		if (IS_ERR(buf)) {
			rc = PTR_ERR(buf);
			dev_err_ratelimited(ddata->dev,
				"error reading device (in isr): %d\n", rc);
			goto exit_io_unlock;
		}

		rc = miscfifo_send_header_payload(&ddata->mf,
				(void *) &event, sizeof(event),
				buf, cfg->range.len);
		goto exit_io_unlock;
	}

exit_io_unlock:
	mutex_unlock(&ddata->io.lock);

exit:
	return IRQ_HANDLED;
}

static void clear_pm_cfg_locked(struct lis2mdl_pm_config *cfg)
{
	size_t i;

	WARN_ON((cfg->num_ops == 0) != (cfg->ops == NULL));

	if (!cfg->ops)
		return;

	for (i = 0; i < cfg->num_ops; i++)
		kfree(cfg->ops[i].buf);

	kfree(cfg->ops);

	cfg->ops = NULL;
	cfg->num_ops = 0;
}

/* deep-copy user_cfg in to cfg (kernel mode pointers) */
static int copy_pm_cfg_locked(struct lis2mdl_device *ddata,
	struct lis2mdl_pm_config *cfg, void __user *uptr)
{
	size_t i;
	int rc;
	struct lis2mdl_pm_config user_cfg = {};


	WARN_ON(cfg->num_ops != 0);
	WARN_ON(cfg->ops != NULL);

	rc = copy_from_user(&user_cfg, uptr, sizeof(user_cfg));
	if (rc)
		return -EFAULT;

	if (user_cfg.num_ops == 0)
		return 0;

	cfg->ops = kcalloc(user_cfg.num_ops, sizeof(cfg->ops[0]), GFP_KERNEL);
	if (!cfg->ops)
		return -ENOMEM;
	cfg->num_ops = user_cfg.num_ops;

	for (i = 0; i < cfg->num_ops; i++) {
		void *ubuf;
		struct lis2mdl_reg_operation *op = &cfg->ops[i];

		if (copy_from_user(op, user_cfg.ops + i, sizeof(*op))) {
			dev_warn(ddata->dev,
				"fault copying user_cfg.ops[%zu]", i);
			rc = -EFAULT;
			goto exit_free;
		}

		/* copy op->buf to kernel */
		ubuf = op->buf;
		op->buf = kzalloc(op->len, GFP_KERNEL);
		if (!op->buf) {
			rc = -ENOMEM;
			goto exit_free;
		}

		if (copy_from_user(op->buf, ubuf, op->len)) {
			dev_dbg(ddata->dev, "fault copying ubuf: %p", ubuf);
			rc = -EFAULT;
			goto exit_free;
		}

		dev_dbg(ddata->dev, " op[%zu] write addr:%02x len:%d [%*phN]\n",
				i,
				op->reg, (int) op->len,
				(int) op->len, op->buf);
	}

	return 0;

exit_free:
	clear_pm_cfg_locked(cfg);
	return rc;
}

static long lis2mdl_fop_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct miscfifo_client *client = file->private_data;
	struct lis2mdl_device *ddata = from_miscfifo(client->mf);
	ssize_t ulen = _IOC_SIZE(cmd);
	void __user *uptr = (void __user *) arg;
	long rc;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(LIS2MDL_IOC_REG_READ): {
		struct lis2mdl_reg_operation op;
		u8 *rx;

		if (sizeof(op) != ulen)
			return -EINVAL;

		if (copy_from_user(&op, uptr, sizeof(op)))
			return -EFAULT;

		mutex_lock(&ddata->io.lock);
		rx = i2c_reg_read_locked(ddata, op.reg, op.len);
		if (IS_ERR(rx)) {
			rc = PTR_ERR(rx);
		} else {
			rc = copy_to_user(op.buf, rx, op.len);
			rc = rc ? -EFAULT : 0;
		}
		mutex_unlock(&ddata->io.lock);
		return rc;
	}

	case _IOC_NR(LIS2MDL_IOC_REG_WRITE): {
		struct lis2mdl_reg_operation op;

		if (sizeof(op) != ulen)
			return -EINVAL;

		if (copy_from_user(&op, uptr, sizeof(op)))
			return -EFAULT;

		mutex_lock(&ddata->io.lock);
		rc = i2c_reg_write_usr_locked(
				ddata, op.reg, op.buf, op.len, true);
		mutex_unlock(&ddata->io.lock);
		return rc;
	}

	case _IOC_NR(LIS2MDL_IOC_INTR_GET_CFG):
		if (sizeof(struct lis2mdl_interrupt_config) != ulen)
			return -EINVAL;

		mutex_lock(&ddata->state.lock);
		rc = copy_to_user(uptr, &ddata->state.interrupt_config, ulen);
		mutex_unlock(&ddata->state.lock);
		return rc ? -EFAULT : 0;

	case _IOC_NR(LIS2MDL_IOC_INTR_SET_CFG): {
		struct lis2mdl_interrupt_config cfg;

		if (sizeof(cfg) != ulen)
			return -EINVAL;

		rc = copy_from_user(&cfg, uptr, sizeof(cfg));
		if (rc)
			return -EFAULT;

		mutex_lock(&ddata->state.lock);
		rc = set_interrupt_config_locked(ddata, &cfg);
		mutex_unlock(&ddata->state.lock);
		return rc;
	}

	case _IOC_NR(LIS2MDL_IOC_PM_SET_RESUME):
	case _IOC_NR(LIS2MDL_IOC_PM_SET_SUSPEND): {
		struct lis2mdl_pm_config *cfg;

		if (sizeof(struct lis2mdl_pm_config) != ulen)
			return -EINVAL;

		mutex_lock(&ddata->state.lock);

		if (_IOC_NR(cmd) == _IOC_NR(LIS2MDL_IOC_PM_SET_SUSPEND)) {
			dev_dbg(ddata->dev, "set resume ops");
			cfg = &ddata->state.resume_cfg;
		} else {
			dev_dbg(ddata->dev, "set suspend ops");
			cfg = &ddata->state.suspend_cfg;
		}
		clear_pm_cfg_locked(cfg);
		rc = copy_pm_cfg_locked(ddata, cfg, uptr);

		mutex_unlock(&ddata->state.lock);
		return rc;
	}


	default:
		return -ENOTTY;
	}
	return 0;
}

static int lis2mdl_fop_open(struct inode *inode, struct file *file)
{
	struct lis2mdl_device *ddata = from_miscdevice(file->private_data);

	return miscfifo_fop_open(file, &ddata->mf);
}

static const struct file_operations lis2mdl_fops = {
	.owner = THIS_MODULE,
	.open = lis2mdl_fop_open,
	.release = miscfifo_fop_release,
	.read = miscfifo_fop_read,
	.poll = miscfifo_fop_poll,
	.unlocked_ioctl = lis2mdl_fop_ioctl,
};

static int configure_regulator(struct lis2mdl_device *ddata,
	struct regulator_info *info)
{
	int rc;
	char prop_name[128] = "";

	snprintf(prop_name, sizeof(prop_name) - 1,
		"%s-voltage-min", info->name);
	if (of_property_read_u32(ddata->client->dev.of_node,
		prop_name, &info->min_uV)) {
		dev_err(ddata->dev, "error getting property '%s' in of config\n",
			prop_name);
		return -EINVAL;
	}

	snprintf(prop_name, sizeof(prop_name) - 1,
		"%s-voltage-max", info->name);
	if (of_property_read_u32(ddata->client->dev.of_node,
		prop_name, &info->max_uV)) {
		dev_err(ddata->dev, "error getting property '%s' in of config\n",
			prop_name);
		return -EINVAL;
	}

	info->regulator = devm_regulator_get(
		&ddata->client->dev, info->name);
	if (!info->regulator) {
		dev_err(ddata->dev, "error getting regulator %s\n", info->name);
		return -EINVAL;
	}

	dev_dbg(ddata->dev, "regulator %s min: %d uV max: %d uV",
		info->name, info->min_uV, info->max_uV);

	rc = regulator_set_voltage(info->regulator, info->min_uV, info->max_uV);
	if (rc) {
		dev_warn(ddata->dev, "failed setting '%s' voltage (%d-%d): %d",
			info->name, info->min_uV, info->max_uV, rc);
		return rc;
	}

	rc = regulator_enable(info->regulator);
	if (rc) {
		dev_err(ddata->dev, "error enabling regulator");
		devm_regulator_put(info->regulator);
		return rc;
	}

	return 0;
}

static int lis2mdl_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	int retry = 0;

	struct lis2mdl_device *ddata = kzalloc(sizeof(*ddata), GFP_KERNEL);

	if (!ddata) {
		rc = -ENOMEM;
		dev_err(&client->dev, "error allocating driver data\n");
		goto exit;
	}

	ddata->dev = &client->dev; /* placeholder until miscdevice is created */
	ddata->client = client;

	mutex_init(&ddata->state.lock);
	mutex_init(&ddata->io.lock);
	ddata->io.rx_buf = kmalloc(BUF_SZ_RX, GFP_KERNEL | GFP_DMA);
	ddata->io.tx_buf = kmalloc(BUF_SZ_TX, GFP_KERNEL | GFP_DMA);
	if (!(ddata->io.rx_buf && ddata->io.tx_buf)) {
		dev_err(ddata->dev, "error alocating DMA buffers\n");
		goto exit_free;
	}

	ddata->mf.config.header_payload = true;
	ddata->mf.config.kfifo_size = 4096;

	ddata->misc.minor = MISC_DYNAMIC_MINOR;
	ddata->misc.name = client->name;
	ddata->misc.fops = &lis2mdl_fops;

	ddata->vdd.name = "vdd";
	ddata->vio.name = "vio";

	rc = configure_regulator(ddata, &ddata->vdd);
	if (rc)
		goto exit_free_dma;

	rc = configure_regulator(ddata, &ddata->vio);
	if (rc)
		goto exit_vdd_disable;


	for (retry = 0; retry < I2C_PROBE_RETRY_MAX; retry++) {
		/* give device time to power on */
		msleep(10);
		rc = probe_i2c_bus(ddata);
		if (!rc)
			break;
	}
	if (retry == I2C_PROBE_RETRY_MAX)
		goto exit_vio_disable;

	rc = of_get_named_gpio_flags(
		client->dev.of_node, "int1-gpio", 0, &ddata->irq_flags);
	if (rc < 0) {
		dev_err(ddata->dev, "error locating gpio 'int1-gpio' in of config\n");
		goto exit_vio_disable;
	}

	ddata->irq_gpio = rc;
	client->irq = gpio_to_irq(ddata->irq_gpio);
	dev_dbg(ddata->dev, "irq: %d, gpio %d, flags: %08x",
		client->irq, ddata->irq_gpio, ddata->irq_flags);

	if (client->irq <= 0) {
		dev_err(ddata->dev, "failed mapping gpio to irq\n");
		goto exit_vio_disable;
	}

	irq_set_status_flags(client->irq, IRQ_DISABLE_UNLAZY);
	rc = devm_request_threaded_irq(&client->dev, client->irq,
					isr_primary, isr_thread_fn,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					ddata->misc.name, ddata);
	if (rc) {
			dev_err(ddata->dev, "error registering irq: %d", rc);
			goto exit_vio_disable;
	}
	disable_irq(ddata->client->irq);

	rc = miscfifo_register(&ddata->mf);
	if (rc) {
		dev_err(ddata->dev, "error registering misc device: %d\n", rc);
		goto exit_free_irq;
	}

	rc = misc_register(&ddata->misc);
	if (rc) {
		dev_err(ddata->dev, "error registering misc device: %d\n", rc);
		goto exit_miscfifo_destroy;
	}

	ddata->dev = ddata->misc.this_device;
	rc = sysfs_create_link(&ddata->dev->kobj, &client->dev.kobj, "i2c");
	if (rc) {
		dev_err(ddata->dev, "error creating sysfs link: %d\n", rc);
		goto exit_misc_deregister;
	}

	i2c_set_clientdata(client, ddata);

	dev_info(ddata->dev, "Oculus ST-LIS2MDL IMU ready");
	dev_dbg(ddata->dev,  "    gpio %d (irq #%d)\n",
			ddata->irq_gpio, client->irq);
	return 0;

exit_misc_deregister:
	misc_deregister(&ddata->misc);
exit_miscfifo_destroy:
	miscfifo_destroy(&ddata->mf);
exit_free_irq:
	irq_clear_status_flags(ddata->client->irq, IRQ_DISABLE_UNLAZY);
	devm_free_irq(&ddata->client->dev, ddata->client->irq, ddata);
exit_vio_disable:
	regulator_disable(ddata->vio.regulator);
	devm_regulator_put(ddata->vio.regulator);
exit_vdd_disable:
	regulator_disable(ddata->vdd.regulator);
	devm_regulator_put(ddata->vdd.regulator);
exit_free_dma:
	kfree(ddata->io.rx_buf);
	kfree(ddata->io.tx_buf);
exit_free:
	kfree(ddata);
exit:
	return rc;
}

static int lis2mdl_i2c_remove(struct i2c_client *client)
{
	struct lis2mdl_device *ddata = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);

	sysfs_remove_link(&ddata->dev->kobj, "i2c");
	misc_deregister(&ddata->misc);
	miscfifo_destroy(&ddata->mf);

	mutex_destroy(&ddata->io.lock);
	mutex_destroy(&ddata->state.lock);

	clear_pm_cfg_locked(&ddata->state.resume_cfg);
	clear_pm_cfg_locked(&ddata->state.suspend_cfg);

	irq_clear_status_flags(ddata->client->irq, IRQ_DISABLE_UNLAZY);
	devm_free_irq(&ddata->client->dev, ddata->client->irq, ddata);

	regulator_disable(ddata->vio.regulator);
	devm_regulator_put(ddata->vio.regulator);
	regulator_disable(ddata->vdd.regulator);
	devm_regulator_put(ddata->vdd.regulator);

	kfree(ddata->io.tx_buf);
	kfree(ddata->io.rx_buf);
	kfree(ddata);

	return 0;
}

static int run_pm_commands(struct lis2mdl_device *ddata, bool suspend)
{
	u32 i;
	struct lis2mdl_pm_config *cfg;
	u8 buf[4] = { 0, 0, 0, 0 };
	struct lis2mdl_event event = {
		.type = (suspend ?
			lis2mdl_event_pm_suspend_fence :
			lis2mdl_event_pm_resume_fence),
		.timestamp = ktime_get_ns(),
		.len = sizeof(buf)
	};
	bool interrupts_enabled;

	mutex_lock(&ddata->state.lock);

	interrupts_enabled =
		ddata->state.interrupt_config.mode == LIS2MDL_INTR_READ_RANGE;
	if (suspend) {
		dev_dbg(ddata->dev, "suspending");
		cfg = &ddata->state.suspend_cfg;
		if (interrupts_enabled)
			disable_irq(ddata->client->irq);
	} else {
		dev_dbg(ddata->dev, "resuming");
		cfg = &ddata->state.resume_cfg;
	}

	for (i = 0; i < cfg->num_ops; i++) {
		struct lis2mdl_reg_operation *op = &cfg->ops[i];
		int rc = i2c_reg_write_usr_locked(
				ddata, op->reg, op->buf, op->len, false);
		if (rc)
			dev_err(ddata->dev, "error %d running %s command",
				rc, suspend ? "suspend" : "resume");
	}


	if (!suspend && interrupts_enabled)
		enable_irq(ddata->client->irq);

	miscfifo_send_header_payload(&ddata->mf,
		(void *) &event, sizeof(event),
		buf, sizeof(buf));

	mutex_unlock(&ddata->state.lock);

	return 0;
}

static int lis2mdl_suspend(struct device *dev)
{
	struct lis2mdl_device *ddata = from_i2c_client(to_i2c_client(dev));

	return run_pm_commands(ddata, true);
}

static int lis2mdl_resume(struct device *dev)
{
	struct lis2mdl_device *ddata = from_i2c_client(to_i2c_client(dev));

	return run_pm_commands(ddata, false);
}

static const struct dev_pm_ops lis2mdl_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lis2mdl_suspend, lis2mdl_resume)
};

static const struct i2c_device_id lis2mdl_id_table[] = {
	{ "lis2mdl" },
	{ },
};
MODULE_DEVICE_TABLE(i2c, lis2mdl_id_table);

static const struct of_device_id lis2mdl_of_match[] = {
	{
		.compatible = "oculus,lis2mdl",
		.data = "lis2mdl",
	},
	{}
};
MODULE_DEVICE_TABLE(of, lis2mdl_of_match);

static struct i2c_driver lis2mdl_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "oculus-lis2mdl-i2c",
		.pm = &lis2mdl_pm_ops,
		.of_match_table = of_match_ptr(lis2mdl_of_match),
	},
	.probe = lis2mdl_i2c_probe,
	.remove = lis2mdl_i2c_remove,
	.id_table = lis2mdl_id_table,
};
module_i2c_driver(lis2mdl_driver);

MODULE_AUTHOR("Khalid Zubair <kzubair@oculus.com>");
MODULE_DESCRIPTION("ST LIS2MDL IMU Driver");
MODULE_LICENSE("GPL");

