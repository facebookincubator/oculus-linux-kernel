#include <linux/oculus/minisensor.h>

enum minisensor_reg_op_type {
	REG_OP_READ,
	REG_OP_WRITE,
};

static inline struct minisensor_device *from_spidevice(struct spi_device *spi)
{
	return spi_get_drvdata(spi);
}

static inline struct minisensor_device *from_miscdevice(struct miscdevice *p)
{
	return container_of(p, struct minisensor_device, misc);
}

static inline struct minisensor_device *from_miscfifo(struct miscfifo *p)
{
	return container_of(p, struct minisensor_device, mf);
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
static u8 *spi_reg_read_locked(struct minisensor_device *ddata,
				u8 addr, size_t len)
{
	int rc;

	/* submit a combined transfer which includes reg addr write operation */
	struct spi_transfer xfer = {
		.tx_buf = ddata->io.tx_buf,
		.rx_buf = ddata->io.rx_buf,
		.bits_per_word = 8,
		.len = len + 1,
	};

	if (WARN_ON(len > (SPI_RX_LIMIT - 1))) {
		dev_err(ddata->dev, "invalid read size: %zu", len);
		return ERR_PTR(-EINVAL);
	}

	/* tx_buf[1...] is expected to have been cleared in reg_write() */
	ddata->io.tx_buf[0] = addr | REG_READ_FLAG;
	rc = spi_sync_transfer(ddata->spi, &xfer, 1);
	if (rc)
		return ERR_PTR(rc);

	/* read data begins after the reg addr written in xfer[0] */
	return ddata->io.rx_buf + 1;
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
static int spi_reg_write_usr_locked(struct minisensor_device *ddata,
				u8 addr, const u8 *buf, size_t len, bool usr)
{
	int rc;
	struct spi_transfer xfer = {
		.tx_buf = ddata->io.tx_buf,
		.bits_per_word = 8,
		.len = len + 1,
	};

	dev_vdbg(ddata->dev, "write addr:%02x, len:%zu data: %*phN\n",
			addr, len, (int) len, buf);
	if (xfer.len > SPI_TX_LIMIT) {
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

	rc = spi_sync_transfer(ddata->spi, &xfer, 1);
	/* zero out tx_buf so we don't resend anything in reg_read() */
	memset(ddata->io.tx_buf, 0, len);
	return rc;
}

/**
 * Read the register at addr and return it's value
 * @param  ddata driver instance
 * @param  addr  register address
 * @return	   error if < 0, or register value
 */
static int spi_reg_read1_locked(struct minisensor_device *ddata, u8 addr)
{
	u8 *buf = spi_reg_read_locked(ddata, addr, 1);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	return buf[0];
}

/**
 * Write 1 byte to the register at addr
 * @param  ddata driver instance
 * @param  addr  register address
 * @param  val   value to write
 * @return	   error if < 0, or 0
 */
static int spi_reg_write1_locked(
	struct minisensor_device *ddata, u8 addr, u8 val)
{
	return spi_reg_write_usr_locked(ddata, addr, &val, 1, false);
}

static int do_reg_op_l(struct minisensor_device *ddata,
	struct minisensor_reg_operation *op, enum minisensor_reg_op_type type,
	bool user)
{

	int rc;
	u8 *rx;
	u16 flags = op->reg & 0xff00;
	u8 reg = op->reg & 0xff;

	if (op->delay_us)
		usleep_range(op->delay_us, op->delay_us + 1);

	if (ddata->has_lsm6dsl_embedded_pages &&
		(flags & LSM6DSL_REG_BANK_A_FLAG)) {
		rc = spi_reg_write1_locked(ddata,
			REG_FUNC_CFG_ACCESS, VAL_FUNC_CFG_EMB_BANK_A);
		if (rc)
			return rc;
		udelay(REG_BANK_SWITCH_DELAY_US);
	} else if (flags) {
		dev_warn(ddata->dev, "invalid register flags: %04x",
			(unsigned) op->reg);
		return -EINVAL;
	}

	if (type == REG_OP_WRITE) {
		rc = spi_reg_write_usr_locked(
				ddata, op->reg, op->buf, op->len, user);
	} else {
		rx = spi_reg_read_locked(ddata, reg, op->len);
		if (IS_ERR(rx)) {
			rc = PTR_ERR(rx);
			goto exit;
		}

		if (user) {
			if (copy_to_user(op->buf, rx, op->len)) {
				rc = -EFAULT;
				goto exit;
			}
		} else {
			memcpy(op->buf, rx, op->len);
		}

		rc = 0;
	}

exit:
	if (ddata->has_lsm6dsl_embedded_pages &&
		(flags & LSM6DSL_REG_BANK_A_FLAG)) {
		spi_reg_write1_locked(ddata,
			REG_FUNC_CFG_ACCESS, VAL_FUNC_CFG_NORMAL);
		udelay(REG_BANK_SWITCH_DELAY_US);
	}

	return rc;
}

static int probe_spi_bus(struct minisensor_device *ddata)
{
	int rc;

	mutex_lock(&ddata->io.lock);
	rc = spi_reg_read1_locked(ddata, ddata->wai_addr);
	if (rc < 0) {
		dev_err(ddata->dev,
			"%s: failed probing device (could not read WAI): %d",
			ddata->misc.name, rc);
		goto exit;
	}

	if (rc != ddata->wai_value) {
		dev_err(ddata->dev,
			"%s: failed probing device (WAI addr=%04x value does not match, expect: %02x, got: %02x)",
			ddata->misc.name,
			(int) ddata->wai_addr, (int) ddata->wai_value, rc);
		rc = -ENODEV;
		goto exit;
	}
	rc = 0;
	dev_dbg(ddata->dev, "Probe OK (got WAI %04x=%02x)",
		ddata->wai_addr, ddata->wai_value);

exit:
	mutex_unlock(&ddata->io.lock);
	return rc;
}

static int set_interrupt_config_locked(struct minisensor_device *ddata,
				struct minisensor_interrupt_config *cfg)
{
	if (cfg->mode == MINI_SENSOR_INTR_DISABLED) {
		if (ddata->state.interrupt_config.mode ==
			MINI_SENSOR_INTR_DISABLED)
			return 0;

		disable_irq(ddata->spi->irq);
		ddata->state.interrupt_config = *cfg;
		dev_dbg(ddata->dev, "disable interrupts\n");
		return 0;
	}

	if (ddata->state.interrupt_config.mode != MINI_SENSOR_INTR_DISABLED)
		return -EBUSY;

	if (cfg->mode == MINI_SENSOR_INTR_READ_FIXED_RANGE) {
		if (cfg->range.len == 0 || cfg->range.len > SPI_RX_LIMIT)
			return -EINVAL;

		dev_dbg(ddata->dev, "enable interrupts: read addr:%02x len:%d\n",
				cfg->range.reg, (unsigned) cfg->range.len);
		ddata->state.interrupt_config = *cfg;
		enable_irq(ddata->spi->irq);
		return 0;
	}

	if (cfg->mode == MINI_SENSOR_INTR_READ_FIFO) {
		/* setup default values to skip checks later */
		if (!cfg->fifo.len_mask)
			cfg->fifo.len_mask = 0xff;
		if (!cfg->fifo.len_multiplier)
			cfg->fifo.len_multiplier = 1;

		dev_dbg(ddata->dev, "enable interrupts: status addr:%02x mask:%02x shift:%02x multiplier:%02x data addr:%02x\n",
			(unsigned) cfg->fifo.status_reg,
			(unsigned) cfg->fifo.len_mask,
			(unsigned) cfg->fifo.len_right_shift,
			(unsigned) cfg->fifo.len_multiplier,
			(unsigned) cfg->fifo.data_reg);
		ddata->state.interrupt_config = *cfg;
		enable_irq(ddata->spi->irq);
		return 0;
	}

	return -EINVAL;
}

static irqreturn_t isr_primary(int irq, void *p)
{
	struct minisensor_device *ddata = p;

	atomic_long_set(&ddata->irq_timestamp, ktime_get_ns());

	return IRQ_WAKE_THREAD;
}

static irqreturn_t isr_thread_fn(int irq, void *p)
{
	struct minisensor_device *ddata = p;

	int rc;
	u64 irq_timestamp = atomic_long_read(&ddata->irq_timestamp);
	/* ddata->state.interrupt_config will not change while the
	   isr is pending */
	const struct minisensor_interrupt_config *cfg =
		&ddata->state.interrupt_config;


	if (cfg->mode == MINI_SENSOR_INTR_DISABLED) {
		dev_err_ratelimited(ddata->dev,
				   "received isr, but intr config is disabled\n");
		goto exit;
	}

	if (cfg->mode == MINI_SENSOR_INTR_READ_FIXED_RANGE) {
		u8 *buf;
		struct minisensor_event event = {
			.timestamp = irq_timestamp,
			.len = cfg->range.len
		};

		mutex_lock(&ddata->io.lock);
		buf = spi_reg_read_locked(
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

	if (cfg->mode == MINI_SENSOR_INTR_READ_FIFO) {
		u8 *buf;
		int len;

		struct minisensor_event event = {
			.timestamp = irq_timestamp,
		};

		mutex_lock(&ddata->io.lock);

		/* read from the 'length register' (e.g. fifo status) */
		len = spi_reg_read1_locked(ddata, cfg->fifo.status_reg);
		if (len < 0) {
			rc = len;
			dev_err_ratelimited(ddata->dev,
				"error reading len-reg (in isr): %d\n", rc);
			goto exit_io_unlock;
		}

		event.fifo_status = len;

		/* manipulate the value to get the number of bytes to read */
		len &= cfg->fifo.len_mask;
		len >>= cfg->fifo.len_right_shift;
		len *= cfg->fifo.len_multiplier;
		event.len = len;
		if (len == 0) {
			dev_err_ratelimited(ddata->dev,
				"got 0 length read from raw value: %02x\n",
				event.fifo_status);
			goto exit_io_unlock;
		}

		/* read from the 'data' register (e.g. fifo) */
		buf = spi_reg_read_locked(
			ddata, cfg->fifo.data_reg, len);
		if (IS_ERR(buf)) {
			rc = PTR_ERR(buf);
			dev_err_ratelimited(ddata->dev,
				"error reading src-reg (in isr): %d\n", rc);
			goto exit_io_unlock;
		}

		miscfifo_send_header_payload(&ddata->mf,
				(void *) &event, sizeof(event), buf, len);
		goto exit_io_unlock;
	}

exit_io_unlock:
	mutex_unlock(&ddata->io.lock);

exit:
	return IRQ_HANDLED;
}


static void clear_pm_cfg_locked(struct minisensor_pm_config *cfg)
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
static int copy_pm_cfg_locked(struct minisensor_device *ddata,
	struct minisensor_pm_config *cfg, void __user *uptr)
{
	size_t i;
	int rc;
	struct minisensor_pm_config user_cfg = {};


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
		struct minisensor_reg_operation *op = &cfg->ops[i];

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

static long minisensor_fop_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct miscfifo_client *client = file->private_data;
	struct minisensor_device *ddata = from_miscfifo(client->mf);
	ssize_t ulen = _IOC_SIZE(cmd);
	void __user *uptr = (void __user *) arg;
	long rc;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(MINI_SENSOR_IOC_REG_WRITE):
	case _IOC_NR(MINI_SENSOR_IOC_REG_READ): {
		struct minisensor_reg_operation op;
		enum minisensor_reg_op_type type =
			_IOC_NR(cmd) == _IOC_NR(MINI_SENSOR_IOC_REG_WRITE) ?
			REG_OP_WRITE :
			REG_OP_READ;

		if (sizeof(op) != ulen)
			return -EINVAL;

		if (copy_from_user(&op, uptr, sizeof(op)))
			return -EFAULT;

		mutex_lock(&ddata->io.lock);
		rc  = do_reg_op_l(ddata, &op, type, true);
		mutex_unlock(&ddata->io.lock);
		return rc;
	}

	case _IOC_NR(MINI_SENSOR_IOC_INTR_GET_CFG):
		if (sizeof(struct minisensor_interrupt_config) != ulen)
			return -EINVAL;

		mutex_lock(&ddata->state.lock);
		rc = copy_to_user(uptr, &ddata->state.interrupt_config, ulen);
		mutex_unlock(&ddata->state.lock);
		return rc ? -EFAULT : 0;

	case _IOC_NR(MINI_SENSOR_IOC_INTR_SET_CFG): {
		struct minisensor_interrupt_config cfg;

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

	case _IOC_NR(MINI_SENSOR_IOC_PM_SET_RESUME):
	case _IOC_NR(MINI_SENSOR_IOC_PM_SET_SUSPEND): {
		struct minisensor_pm_config *cfg;

		if (sizeof(struct minisensor_pm_config) != ulen)
			return -EINVAL;

		mutex_lock(&ddata->state.lock);

		if (_IOC_NR(cmd) == _IOC_NR(MINI_SENSOR_IOC_PM_SET_RESUME)) {
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

static int minisensor_fop_open(struct inode *inode, struct file *file)
{
	struct minisensor_device *ddata = from_miscdevice(file->private_data);

	return miscfifo_fop_open(file, &ddata->mf);
}

static const struct file_operations minisensor_fops = {
	.owner = THIS_MODULE,
	.open = minisensor_fop_open,
	.release = miscfifo_fop_release,
	.read = miscfifo_fop_read,
	.poll = miscfifo_fop_poll,
	.unlocked_ioctl = minisensor_fop_ioctl,
};

static int minisensor_spi_probe(struct spi_device *spi)
{
	int rc = 0;
	u32 irq_flags = 0;
	u32 wai_addr;
	u32 wai_value;
	u32 startup_time_ms;

	struct minisensor_device *ddata = kzalloc(sizeof(*ddata), GFP_KERNEL);

	if (!ddata) {
		rc = -ENOMEM;
		dev_err(&spi->dev, "error allocating driver data\n");
		goto exit;
	}

	ddata->dev = &spi->dev; /* placeholder until miscdevice is created */
	ddata->spi = spi;

	mutex_init(&ddata->state.lock);
	mutex_init(&ddata->io.lock);
	ddata->io.rx_buf = kmalloc(SPI_BUF_SZ, GFP_KERNEL | GFP_DMA);
	ddata->io.tx_buf = kzalloc(SPI_BUF_SZ, GFP_KERNEL | GFP_DMA);
	if (!(ddata->io.rx_buf && ddata->io.tx_buf)) {
		dev_err(ddata->dev, "error alocating DMA buffers\n");
		goto exit_free;
	}

	ddata->mf.config.header_payload = true;
	ddata->mf.config.kfifo_size = 4096;

	ddata->misc.minor = MISC_DYNAMIC_MINOR;
	ddata->misc.name = spi->dev.of_node->name;
	ddata->misc.fops = &minisensor_fops;

	if (of_find_property(spi->dev.of_node, "has-lsm6dsl-emb-pages", NULL))
		ddata->has_lsm6dsl_embedded_pages = true;

	if (of_property_read_u32(spi->dev.of_node, "reg-wai-addr", &wai_addr) ||
		of_property_read_u32(
			spi->dev.of_node, "reg-wai-value", &wai_value)) {
		dev_err(ddata->dev, "error getting 'reg-wai-addr' or 'reg-wai-value' properties\n");
		rc = -EINVAL;
		goto exit_free_dma;
	}

	ddata->wai_addr = wai_addr & 0xffff;
	ddata->wai_value = wai_value & 0xff;

	if (of_property_read_u32(spi->dev.of_node,
			"vdd-voltage-min", &ddata->vdd_io_min_uV)) {
			dev_err(ddata->dev, "error getting property 'vdd-voltage-min' in of config\n");
			rc = -EINVAL;
			goto exit_free_dma;
	}

	if (of_property_read_u32(spi->dev.of_node,
			"vdd-voltage-max", &ddata->vdd_io_max_uV)) {
			dev_err(ddata->dev, "error getting property 'vdd-voltage-max' in of config\n");
			rc = -EINVAL;
			goto exit_free_dma;
	}

	if (of_property_read_u32(spi->dev.of_node,
			"startup-time-ms", &startup_time_ms)) {
		startup_time_ms = 100;
	}

	ddata->reg_vdd_io = devm_regulator_get(&spi->dev, "vdd");
	if (!ddata->reg_vdd_io) {
			dev_err(ddata->dev, "error getting vdd regulator\n");
			rc = -EINVAL;
			goto exit_free_dma;
	}

	dev_dbg(ddata->dev, "regulator 'vdd' min: %d uV max: %d uV",
		ddata->vdd_io_min_uV, ddata->vdd_io_max_uV);

	rc = regulator_set_voltage(ddata->reg_vdd_io,
		ddata->vdd_io_min_uV, ddata->vdd_io_max_uV);
	if (rc) {
		dev_err(ddata->dev,
			"error setting regulator voltage: %d", rc);
		goto exit_reg_put;
	}

	rc = regulator_enable(ddata->reg_vdd_io);
	if (rc) {
		dev_err(ddata->dev, "error enabling regulator");
		goto exit_reg_put;
	}

	/* give device time to power on */
	msleep(startup_time_ms);

	rc = probe_spi_bus(ddata);
	if (rc)
		goto exit_reg_disable;

	rc = of_property_read_u32(
		spi->dev.of_node, "interrupts-flags", &irq_flags);
	if (rc < 0) {
		dev_err(ddata->dev, "error locating irq flags, missing 'interrupts-flags' property\n");
		goto exit_reg_disable;
	}

	irq_set_status_flags(spi->irq, IRQ_DISABLE_UNLAZY);
	rc = devm_request_threaded_irq(&spi->dev, spi->irq,
					isr_primary, isr_thread_fn,
					IRQF_ONESHOT | irq_flags,
					ddata->misc.name, ddata);
	if (rc) {
		dev_err(ddata->dev, "error registering irq: %d", rc);
		goto exit_reg_disable;
	}
	disable_irq(ddata->spi->irq);

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
	rc = sysfs_create_link(&ddata->dev->kobj, &spi->dev.kobj, "spi");
	if (rc) {
		dev_err(ddata->dev, "error creating sysfs link: %d\n", rc);
		goto exit_misc_deregister;
	}

	spi_set_drvdata(spi, ddata);

	dev_info(ddata->dev, "Oculus %s sensor driver ready (irq #%d)\n",
			ddata->misc.name, spi->irq);
	return 0;

exit_misc_deregister:
	misc_deregister(&ddata->misc);
exit_miscfifo_destroy:
	miscfifo_destroy(&ddata->mf);
exit_free_irq:
	irq_clear_status_flags(ddata->spi->irq, IRQ_DISABLE_UNLAZY);
	devm_free_irq(&ddata->spi->dev, ddata->spi->irq, ddata);
exit_reg_disable:
	regulator_disable(ddata->reg_vdd_io);
exit_reg_put:
	devm_regulator_put(ddata->reg_vdd_io);
exit_free_dma:
	kfree(ddata->io.rx_buf);
	kfree(ddata->io.tx_buf);
exit_free:
	kfree(ddata);
exit:
	return rc;
}

static int minisensor_spi_remove(struct spi_device *spi)
{
	struct minisensor_device *ddata = spi_get_drvdata(spi);

	spi_set_drvdata(spi, NULL);

	sysfs_remove_link(&ddata->dev->kobj, "spi");
	misc_deregister(&ddata->misc);
	miscfifo_destroy(&ddata->mf);

	mutex_destroy(&ddata->io.lock);
	mutex_destroy(&ddata->state.lock);

	clear_pm_cfg_locked(&ddata->state.resume_cfg);
	clear_pm_cfg_locked(&ddata->state.suspend_cfg);

	irq_clear_status_flags(ddata->spi->irq, IRQ_DISABLE_UNLAZY);
	devm_free_irq(&ddata->spi->dev, ddata->spi->irq, ddata);

	regulator_disable(ddata->reg_vdd_io);
	devm_regulator_put(ddata->reg_vdd_io);
	kfree(ddata->io.tx_buf);
	kfree(ddata->io.rx_buf);
	kfree(ddata);

	return 0;
}

static int run_pm_commands(struct minisensor_device *ddata, bool suspend)
{
	u32 i;
	struct minisensor_pm_config *cfg;
	u8 buf[4] = { 0, 0, 0, 0 };
	struct minisensor_event event = {
		.type = (suspend ?
				minisensor_event_pm_suspend_fence :
				minisensor_event_pm_resume_fence),
		.timestamp = ktime_get_ns(),
		.len = sizeof(buf)
	};
	bool interrupts_enabled;

	mutex_lock(&ddata->state.lock);

	interrupts_enabled =
		ddata->state.interrupt_config.mode != MINI_SENSOR_INTR_DISABLED;

	if (suspend) {
		dev_dbg(ddata->dev, "suspending");
		cfg = &ddata->state.suspend_cfg;
		if (interrupts_enabled)
			disable_irq(ddata->spi->irq);
	} else {
		dev_dbg(ddata->dev, "resuming");
		cfg = &ddata->state.resume_cfg;
	}

	for (i = 0; i < cfg->num_ops; i++)
		do_reg_op_l(ddata, &cfg->ops[i], REG_OP_WRITE, false);


	if (!suspend && interrupts_enabled)
		enable_irq(ddata->spi->irq);

	miscfifo_send_header_payload(&ddata->mf,
			(void *) &event, sizeof(event),
			buf, sizeof(buf));

	mutex_unlock(&ddata->state.lock);

	return 0;
}

static int minisensor_suspend(struct device *dev)
{
	struct minisensor_device *ddata = from_spidevice(to_spi_device(dev));

	return run_pm_commands(ddata, true);
}

static int minisensor_resume(struct device *dev)
{
	struct minisensor_device *ddata = from_spidevice(to_spi_device(dev));

	return run_pm_commands(ddata, false);
}

static const struct dev_pm_ops minisensor_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(minisensor_suspend, minisensor_resume)
};

static const struct spi_device_id minisensor_id_table[] = {
	{ "minisensor" },
	{ },
};
MODULE_DEVICE_TABLE(spi, minisensor_id_table);

static const struct of_device_id minisensor_of_match[] = {
	{
		.compatible = "oculus,minisensor",
		.data = "minisensor",
	},
	{}
};
MODULE_DEVICE_TABLE(of, minisensor_of_match);

static struct spi_driver minisensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "oculus-minisensor-spi",
		.pm = &minisensor_pm_ops,
		.of_match_table = of_match_ptr(minisensor_of_match),
	},
	.probe = minisensor_spi_probe,
	.remove = minisensor_spi_remove,
	.id_table = minisensor_id_table,
};
module_spi_driver(minisensor_driver);

MODULE_AUTHOR("Khalid Zubair <kzubair@oculus.com>");
MODULE_DESCRIPTION("ST LSM6DSL IMU Driver");
MODULE_LICENSE("GPL");

