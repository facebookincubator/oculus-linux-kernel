// SPDX-License-Identifier: GPL-2.0

#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <soc/qcom/smci_appclient.h>
#include <soc/qcom/smci_appcontroller.h>
#include <soc/qcom/smci_apploader.h>
#include <soc/qcom/smci_clientenv.h>
#include <soc/qcom/smci_object.h>
#include <uapi/linux/sched/types.h>

#define HANDLE_CMD_OP 0
#define SENSORLOCK_CMDS_VERSION 1
#define SENSORLOCK_EVENT_TIMEOUT_MS 5000
#define SENSORLOCK_WATCHDOG_PET_MS 1000
#define SENSORLOCK_MAX_LED_WAIT_TIME_MS 3000
#define SENSORLOCK_APP_NAME "sensorlock64"
#define SENSORLOCK_FIRMWARE_NAME "sensorlock64.mbn"

#define IS_SMCI_ERROR(rc, rsp) \
	((rc) != SMCI_OBJECT_OK || (rsp).status != SMCI_OBJECT_OK)

#define STOP_WATCHDOG_IOCTL _IO(MISC_MAJOR, 0)
#define START_WATCHDOG_IOCTL _IO(MISC_MAJOR, 1)

enum sensorlock_state {
	STATE_DISABLED = 1,
	STATE_DISENGAGED = 2,
	STATE_ENGAGED = 3,
};

enum sensorlock_cmds {
	CMD_CHECK_SIGNAL = 0,
	CMD_GET_CURRENT_STATE = 1,
	CMD_CHECK_LED = 2,
	CMD_TOGGLE_MIC = 3,
	CMD_GET_LED_INFO = 4,
};

struct sl_basic_cmd {
	uint32_t version;
	uint32_t cmd_id;
} __attribute__((__packed__));

struct sl_toggle_mic_cmd {
	uint32_t version;
	uint32_t cmd_id;
	bool enabled;
} __attribute__((__packed__));

struct sl_basic_rsp {
	uint32_t version;
	int32_t status;
} __attribute__((__packed__));

struct sl_state_rsp {
	uint32_t version;
	int32_t status;
	enum sensorlock_state state;
} __attribute__((__packed__));

struct sl_led_info_rsp {
	uint32_t version;
	int32_t status;
	bool is_led_on;
	int32_t wait_time_ms;
} __attribute__((__packed__));

struct sensorlock_reg_data {
	bool dmic_enabled;
	int dmic_status;
	struct mutex dmic_lock;
};

struct sensorlock_dev_data {
	struct device *dev;
	struct miscdevice misc;
	struct smci_object sensorlock_app;
	struct kthread_worker *kworker;
	struct mutex event_lock;
	struct task_struct *watchdog_thread;
	wait_queue_head_t watchdog_queue;
	wait_queue_head_t sensorlock_wait_queue;
	atomic_t event_available;
	ktime_t event_timestamp;
	atomic_t state;
	int app_load_status;
	bool is_app_connected;
	int sensorlock_irq;
	struct sensorlock_reg_data reg_data;
};

enum sensorlock_work_action {
	WORK_ACT_LOAD,
	WORK_ACT_CHECK_SIGNALS,
	WORK_ACT_DMIC_ENABLE,
	WORK_ACT_DMIC_DISABLE,
	WORK_ACT_CHECK_LED,
};

struct sensorlock_work {
	struct sensorlock_dev_data *dev_data;
	struct kthread_delayed_work kthread_work;
	enum sensorlock_work_action action;
};

static int sensorlock_watchdog_pet(void *data);
static void sensorlock_work_handler(struct kthread_work *work);

static void sensorlock_set_event(struct sensorlock_dev_data *dev_data, enum sensorlock_state state)
{
	mutex_lock(&dev_data->event_lock);
	atomic_set(&dev_data->state, state);
	dev_data->event_timestamp = ktime_get_boottime();
	atomic_set(&dev_data->event_available, 1);

	mutex_unlock(&dev_data->event_lock);
	wake_up_all(&dev_data->sensorlock_wait_queue);
}

static int sensorlock_get_event(struct sensorlock_dev_data *dev_data, enum sensorlock_state *state)
{
	int rc = 0;
	s64 delta_ms;

	mutex_lock(&dev_data->event_lock);
	delta_ms = ktime_ms_delta(ktime_get_boottime(), dev_data->event_timestamp);
		if (delta_ms > SENSORLOCK_EVENT_TIMEOUT_MS) {
			dev_err(dev_data->dev, "expired request called after %lldms\n", delta_ms);
			rc = -ETIME;
		goto timeout;
	}

	*state = atomic_read(&dev_data->state);
timeout:
	atomic_set(&dev_data->event_available, 0);
	mutex_unlock(&dev_data->event_lock);
	return rc;
}

/* Character device read operation blocks until a signal has been sent to the Trustzone application */
static ssize_t sensorlock_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	int rc;
	enum sensorlock_state state;
	struct sensorlock_dev_data *dev_data = container_of(filp->private_data, struct sensorlock_dev_data, misc);

	if (len < sizeof(enum sensorlock_state)) {
		dev_err(dev_data->dev, "output buffer state must be at least %lu bytes", sizeof(enum sensorlock_state));
		return -EINVAL;
	}

	rc = wait_event_interruptible(dev_data->sensorlock_wait_queue, atomic_read(&dev_data->event_available));
	if (rc) {
		dev_warn_ratelimited(dev_data->dev, "%s aborted due to signal. rc: %d", __func__, rc);
		return rc;
	}

	rc = sensorlock_get_event(dev_data, &state);
	if (rc) {
		dev_err(dev_data->dev, "failed to get sensorlock event. rc: %d", rc);
		return rc;
	}

	if (copy_to_user(buf, &state, sizeof(enum sensorlock_state))) {
		dev_err(dev_data->dev, "could not copy sensorlock state to userspace");
		return -EFAULT;
	}

	return sizeof(enum sensorlock_state);
}

static long sensorlock_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct sensorlock_dev_data *dev_data = container_of(filp->private_data, struct sensorlock_dev_data, misc);
	struct device *dev = dev_data->dev;

	switch (cmd) {
	case STOP_WATCHDOG_IOCTL:
		dev_info(dev, "%s stopping sensorlock watchdog heartbeat", __func__);
		kthread_stop(dev_data->watchdog_thread);
		break;
	case START_WATCHDOG_IOCTL:
		dev_info(dev, "%s starting sensorlock watchdog heartbeat", __func__);
		kthread_stop(dev_data->watchdog_thread);
		dev_data->watchdog_thread = kthread_run(sensorlock_watchdog_pet, dev, "sensorlock_watchdog_pet");
		if (!dev_data->watchdog_thread) {
			dev_err(dev, "error creating watchdog pet thread");
			return -EINVAL;
		}

		break;
	default:
		dev_err(dev, "%s invalid ioctl command: %d", __func__, cmd);
		break;
	}

	return 0;
}

static const struct file_operations fops = {
	.owner          = THIS_MODULE,
	.read           = sensorlock_read,
	.unlocked_ioctl = sensorlock_ioctl,
};

static void queue_sensorlock_delayed_work(struct device *dev, enum sensorlock_work_action action, unsigned long jiffies)
{
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev);
	struct sensorlock_work *sl_work = devm_kmalloc(dev, sizeof(*sl_work), GFP_ATOMIC);

	if (sl_work == NULL)
		return;

	sl_work->dev_data = dev_data;
	sl_work->action = action;
	kthread_init_delayed_work(&sl_work->kthread_work, sensorlock_work_handler);
	kthread_queue_delayed_work(dev_data->kworker, &sl_work->kthread_work, jiffies);
}

static void queue_sensorlock_work(struct device *dev, enum sensorlock_work_action action)
{
	queue_sensorlock_delayed_work(dev, action, 0);
}

/* Allows the driver to connect to a loaded trustzone application in order to call commands */
static int sensorlock_app_connect(struct device *dev)
{
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev);
	struct smci_object client_env = {NULL, NULL};
	struct smci_object app_client = {NULL, NULL};
	const size_t name_len = strlen(SENSORLOCK_APP_NAME);
	int rc;

	rc = get_client_env_object(&client_env);
	if (rc) {
		dev_err(dev_data->dev, "get_client_env_object failed: %d\n", rc);
		client_env.invoke = NULL;
		client_env.context = NULL;
		goto cleanup;
	}

	rc = smci_clientenv_open(client_env, SMCI_APPCLIENT_UID, &app_client);
	if (rc) {
		dev_err(dev_data->dev, "smci_clientenv_open failed: %d\n", rc);
		app_client.invoke = NULL;
		app_client.context = NULL;
		goto cleanup;
	}

	rc = smci_appclient_getappobject(app_client, SENSORLOCK_APP_NAME, name_len, &dev_data->sensorlock_app);
	if (rc) {
		dev_err(dev_data->dev, "smci_appclient_getappobject failed: %d\n", rc);
		dev_data->sensorlock_app.invoke = NULL;
		dev_data->sensorlock_app.context = NULL;
		goto cleanup;
	}

	dev_data->is_app_connected = true;
cleanup:
	SMCI_OBJECT_ASSIGN_NULL(client_env);
	SMCI_OBJECT_ASSIGN_NULL(app_client);
	return rc;
}

static int sensorlock_try_connect(struct device *dev)
{
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev);
	int rc;

	if (dev_data->is_app_connected)
		return 0;

	rc = sensorlock_app_connect(dev);
	if (rc)
		dev_err(dev, "Could not connect to the sensorlock trustzone application");

	return rc;
}

static int sensorlock_send_cmd(struct device *dev, void *cmd, size_t cmd_size, void *rsp, size_t rsp_size)
{
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev);
	union smci_object_arg args[2];
	int rc;

	rc = sensorlock_try_connect(dev);
	if (rc)
		return rc;

	args[0].b.ptr = cmd;
	args[0].b.size = cmd_size;
	args[1].b.ptr = rsp;
	args[1].b.size = rsp_size;

	return smci_object_invoke(dev_data->sensorlock_app, HANDLE_CMD_OP, args, SMCI_OBJECT_COUNTS_PACK(1, 1, 0, 0));
}

/* Loads the sensorlock trustzone application into the trustzone driver */
static void sensorlock_app_load(struct device *dev)
{
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev);
	const struct firmware *app_data;
	struct smci_object client_env = {NULL, NULL};
	struct smci_object app_loader = {NULL, NULL};
	struct smci_object app_controller = {NULL, NULL};
	struct smci_object app_obj = {NULL, NULL};
	int rc;

	rc = firmware_request_nowarn(&app_data, SENSORLOCK_FIRMWARE_NAME, dev_data->dev);
	if (rc) {
		dev_err(dev_data->dev, "sensorlock firmware_request_nowarn failed: %d\n", rc);
		goto out;
	}

	rc = get_client_env_object(&client_env);
	if (rc) {
		dev_err(dev_data->dev, "get_client_env_object failed: %d\n", rc);
		client_env.invoke = NULL;
		client_env.context = NULL;
		goto cleanup;
	}

	rc = smci_clientenv_open(client_env, SMCI_APPLOADER_UID, &app_loader);
	if (rc) {
		dev_err(dev_data->dev, "smci_clientenv_open failed: %d\n", rc);
		app_loader.invoke = NULL;
		app_loader.context = NULL;
		goto cleanup;
	}

	rc = smci_apploader_loadfrombuffer(app_loader, app_data->data, app_data->size, &app_controller);
	if (rc) {
		dev_err(dev_data->dev, "smci_apploader_loadfrombuffer failed: %d\n", rc);
		app_controller.invoke = NULL;
		app_controller.context = NULL;
		goto cleanup;
	}

	rc = smci_appcontroller_getappobject(app_controller, &app_obj);
	if (rc) {
		dev_err(dev_data->dev, "smci_appcontroller_getappobject failed: %d\n", rc);
		app_obj.invoke = NULL;
		app_obj.context = NULL;
		goto cleanup;
	}

cleanup:
	release_firmware(app_data);
	SMCI_OBJECT_ASSIGN_NULL(app_obj);
	SMCI_OBJECT_ASSIGN_NULL(app_controller);
	SMCI_OBJECT_ASSIGN_NULL(app_loader);
	SMCI_OBJECT_ASSIGN_NULL(client_env);
out:
	dev_data->app_load_status = rc;
	return;
}

static void sensorlock_check_led(struct device *dev)
{
	struct sl_basic_cmd cmd = {SENSORLOCK_CMDS_VERSION, CMD_CHECK_LED};
	struct sl_basic_rsp rsp = {};
	int rc;

	rc = sensorlock_send_cmd(dev, &cmd, sizeof(cmd), &rsp, sizeof(rsp));
	if (IS_SMCI_ERROR(rc, rsp)) {
		dev_err(dev, "%s failed. rc: %d status: %d\n", __func__, rc, rsp.status);
		return;
	}
}

static void sensorlock_get_led_info(struct device *dev)
{
	struct sl_basic_cmd cmd = {SENSORLOCK_CMDS_VERSION, CMD_GET_LED_INFO};
	struct sl_led_info_rsp rsp = {};
	unsigned long wait_time_ms;
	int rc;

	rc = sensorlock_send_cmd(dev, &cmd, sizeof(cmd), &rsp, sizeof(rsp));
	if (IS_SMCI_ERROR(rc, rsp)) {
		dev_err(dev, "%s failed. rc: %d status: %d\n", __func__, rc, rsp.status);
		return;
	}

	/* If the LED is off there is no more work to do */
	if (!rsp.is_led_on)
		return;

	wait_time_ms = min(rsp.wait_time_ms, SENSORLOCK_MAX_LED_WAIT_TIME_MS);
	queue_sensorlock_delayed_work(dev, WORK_ACT_CHECK_LED, msecs_to_jiffies(wait_time_ms));
}

static void sensorlock_dmic_toggle(struct device *dev, bool enable)
{
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev);
	struct sensorlock_reg_data *rdata = &dev_data->reg_data;
	struct sl_toggle_mic_cmd cmd = {SENSORLOCK_CMDS_VERSION, CMD_TOGGLE_MIC, enable};
	struct sl_basic_rsp rsp = {};
	int rc;

	rc = sensorlock_send_cmd(dev, &cmd, sizeof(cmd), &rsp, sizeof(rsp));
	if (IS_SMCI_ERROR(rc, rsp))
		dev_err(dev, "%s failed. rc: %d status: %d\n", __func__, rc, rsp.status);
	else
		rdata->dmic_enabled = enable;

	rdata->dmic_status = IS_SMCI_ERROR(rc, rsp) ? -EINVAL : 0;
}

/* Call the sensorlock trustzone function check_signals */
static void sensorlock_check_signals(struct device *dev)
{
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev);
	struct sl_basic_cmd cmd = {SENSORLOCK_CMDS_VERSION, CMD_CHECK_SIGNAL};
	struct sl_state_rsp rsp = {};
	enum sensorlock_state prev;
	int rc;

	prev = atomic_read(&dev_data->state);
	rc = sensorlock_send_cmd(dev, &cmd, sizeof(cmd), &rsp, sizeof(rsp));
	if (IS_SMCI_ERROR(rc, rsp)) {
		dev_err(dev, "%s failed. rc: %d status: %d\n", __func__, rc, rsp.status);
		return;
	}

	sensorlock_set_event(dev_data, rsp.state);
	if (prev != rsp.state) {
		if (rsp.state == STATE_DISENGAGED)
			wake_up_all(&dev_data->watchdog_queue);
		else if (rsp.state == STATE_ENGAGED)
			sensorlock_get_led_info(dev_data->dev);
	}
}

static int sensorlock_get_current_state(struct device *dev, enum sensorlock_state *state)
{
	struct sl_basic_cmd cmd = {SENSORLOCK_CMDS_VERSION, CMD_GET_CURRENT_STATE};
	struct sl_state_rsp rsp = {};
	int rc;

	rc = sensorlock_send_cmd(dev, &cmd, sizeof(cmd), &rsp, sizeof(rsp));
	if (IS_SMCI_ERROR(rc, rsp)) {
		dev_err(dev, "%s failed. rc: %d status: %d\n", __func__, rc, rsp.status);
		return -EINVAL;
	}

	*state = rsp.state;
	return 0;
}

static void sensorlock_init_state(struct device *dev)
{
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev);
	enum sensorlock_state state;
	int rc;

	rc = sensorlock_get_current_state(dev, &state);
	if (rc) {
		dev_err(dev, "%s failed to get state: %d\n", __func__, rc);
		return;
	}

	atomic_set(&dev_data->state, state);
	if (state == STATE_DISENGAGED)
		wake_up_all(&dev_data->watchdog_queue);
}

static void sensorlock_work_handler(struct kthread_work *work)
{
	struct sensorlock_work *sl_work = container_of(work, struct sensorlock_work, kthread_work.work);
	struct device *dev = sl_work->dev_data->dev;

	switch (sl_work->action) {
	case WORK_ACT_LOAD:
		sensorlock_app_load(dev);
		sensorlock_init_state(dev);
		break;
	case WORK_ACT_CHECK_SIGNALS:
		sensorlock_check_signals(dev);
		break;
	case WORK_ACT_DMIC_ENABLE:
		sensorlock_dmic_toggle(dev, true);
		break;
	case WORK_ACT_DMIC_DISABLE:
		sensorlock_dmic_toggle(dev, false);
		break;
	case WORK_ACT_CHECK_LED:
		sensorlock_check_led(dev);
		break;
	default:
		dev_err(dev, "%s: invalid action %d\n", __func__, sl_work->action);
		break;
	}

	devm_kfree(dev, sl_work);
}

static int sensorlock_watchdog_pet(void *data)
{
	struct device *dev = data;
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev);
	const int WAIT_TIME = msecs_to_jiffies(SENSORLOCK_WATCHDOG_PET_MS);
	int rc;

	while (!kthread_should_stop()) {
		do {
			rc = wait_event_interruptible(dev_data->watchdog_queue, atomic_read(&dev_data->state) == STATE_DISENGAGED);
		} while (rc != 0 && !kthread_should_stop());

		do {
			rc = wait_event_interruptible_timeout(dev_data->watchdog_queue, kthread_should_stop(), WAIT_TIME);
		} while (rc != 0 && rc != 1 && !kthread_should_stop());

		if (!kthread_should_stop())
			queue_sensorlock_work(dev, WORK_ACT_CHECK_SIGNALS);
	}

	return 0;
}

static irqreturn_t sensorlock_irq_handler(int irq, void *data)
{
	struct sensorlock_dev_data *dev_data = data;

	queue_sensorlock_work(dev_data->dev, WORK_ACT_CHECK_SIGNALS);
	return IRQ_HANDLED;
}

static int reg_dmic_toggle(struct regulator_dev *rdev, bool enable)
{
	struct device *dev = rdev_get_dev(rdev);
	struct sensorlock_reg_data *rdata = rdev_get_drvdata(rdev);
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev->parent);
	int rc = 0;

	mutex_lock(&rdata->dmic_lock);
	queue_sensorlock_work(dev_data->dev, enable ? WORK_ACT_DMIC_ENABLE : WORK_ACT_DMIC_DISABLE);
	kthread_flush_worker(dev_data->kworker);

	rc = rdata->dmic_status;
	mutex_unlock(&rdata->dmic_lock);

	return rc;
}

static int reg_dmic_enable(struct regulator_dev *rdev)
{
	return reg_dmic_toggle(rdev, true);
}

static int reg_dmic_disable(struct regulator_dev *rdev)
{
	return reg_dmic_toggle(rdev, false);
}

static int reg_dmic_is_enabled(struct regulator_dev *rdev)
{
	struct sensorlock_reg_data *rdata = rdev_get_drvdata(rdev);

	return rdata->dmic_enabled;
}

static const struct regulator_ops sensorlock_dmic_reg_ops = {
	.enable = reg_dmic_enable,
	.disable = reg_dmic_disable,
	.is_enabled = reg_dmic_is_enabled,
};

static const struct regulator_desc sensorlock_dmic_reg_desc = {
	.name = "dmic",
	.supply_name = "vin",
	.type = REGULATOR_VOLTAGE,
	.regulators_node = of_match_ptr("regulators"),
	.of_match = of_match_ptr("dmic"),
	.ops = &sensorlock_dmic_reg_ops,
	.owner = THIS_MODULE,
};

static int sensorlock_regulator_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct regulator_config config = { 0 };
	const struct regulator_desc *desc = &sensorlock_dmic_reg_desc;
	struct sensorlock_reg_data *rdata = &((struct sensorlock_dev_data *)dev_get_drvdata(dev))->reg_data;
	struct regulator_dev *rdev;
	int ret;

	mutex_init(&rdata->dmic_lock);

	config.dev = dev;
	config.driver_data = rdata;
	config.init_data = of_get_regulator_init_data(dev, np, desc);

	rdev = devm_regulator_register(dev, desc, &config);
	if (IS_ERR(rdev)) {
		ret = PTR_ERR(rdev);
		dev_err(dev, "failed to register regulator: %d\n", ret);
		return ret;
	}

	ret = devm_regulator_debug_register(dev, rdev);
	if (ret) {
		dev_err(dev, "failed to register debug regulator: %d\n", ret);
		return ret;
	}

	return 0;
}

static int sensorlock_probe(struct platform_device *pdev)
{
	int i;
	int rc = 0;
	int irq_count = 0;
	const char **irq_names;
	struct sensorlock_dev_data *dev_data = NULL;
	struct device *dev = &pdev->dev;

	dev_data = devm_kzalloc(dev, sizeof(struct sensorlock_dev_data), GFP_KERNEL);
	if (!dev_data)
		return -ENOMEM;

	irq_count = of_property_count_strings(dev->of_node, "interrupt-names");
	if (irq_count < 0) {
		dev_err(dev, "Unable to find sensorlock IRQs");
		return -EINVAL;
	}

	irq_names = devm_kcalloc(dev, irq_count, sizeof(*irq_names), GFP_KERNEL);
	if (!irq_names)
		return -ENOMEM;

	rc = of_property_read_string_array(dev->of_node, "interrupt-names", irq_names, irq_count);
	if (!rc) {
		dev_err(dev, "Unable to read sensorlock IRQ names");
		return -EINVAL;
	}

	dev_data->dev = dev;
	mutex_init(&dev_data->event_lock);
	atomic_set(&dev_data->event_available, 0);
	atomic_set(&dev_data->state, STATE_ENGAGED);
	SMCI_OBJECT_ASSIGN_NULL(dev_data->sensorlock_app);
	init_waitqueue_head(&dev_data->watchdog_queue);
	init_waitqueue_head(&dev_data->sensorlock_wait_queue);
	dev_set_drvdata(dev, dev_data);

	dev_data->misc.name = "sensorlock";
	dev_data->misc.minor = MISC_DYNAMIC_MINOR;
	dev_data->misc.fops = &fops;

	dev_data->watchdog_thread = kthread_run(sensorlock_watchdog_pet, dev, "sensorlock_watchdog_pet");
	if (IS_ERR(dev_data->watchdog_thread)) {
		dev_err(dev, "error creating watchdog pet thread");
		rc = PTR_ERR(dev_data->watchdog_thread);
		goto error_kthread;
	}

	dev_data->kworker = kthread_create_worker(0, "sensorlock");
	if (IS_ERR(dev_data->kworker)) {
		dev_err(dev, "failed to create kworker\n");
		rc = PTR_ERR(dev_data->kworker);
		goto error_kworker;
	}

	queue_sensorlock_work(dev, WORK_ACT_LOAD);
	kthread_flush_worker(dev_data->kworker);
	if (dev_data->app_load_status) {
		dev_err(dev, "Failed to load TrustZone application: %d\n", dev_data->app_load_status);

		// Only retry when the TrustZone app fails to load with SMCI_OBJECT_ERROR
		rc = dev_data->app_load_status == SMCI_OBJECT_ERROR ? -EPROBE_DEFER : -EINVAL;
		goto error_app_load;
	}

	rc = sensorlock_regulator_init(pdev);
	if (rc) {
		dev_err(dev, "regulator init failed: %d\n", rc);
		goto error_app_load;
	}

	rc = misc_register(&dev_data->misc);
	if (rc) {
		dev_err(dev, "error creating misc device");
		goto error_app_load;
	}

	for (i = 0; i < irq_count; i++) {
		dev_data->sensorlock_irq = platform_get_irq_byname(pdev, irq_names[i]);
		if (dev_data->sensorlock_irq < 0) {
			dev_err(dev, "Unable to find %s IRQ", irq_names[i]);
			rc = -EINVAL;
			goto error_irq;
		}

		rc = devm_request_irq(dev,
					dev_data->sensorlock_irq,
					sensorlock_irq_handler,
					IRQF_ONESHOT,
					irq_names[i], dev_data);
		if (rc) {
			dev_err(dev, "failed to set %s handler", irq_names[i]);
			goto error_irq;
		}
	}

	return 0;

error_irq:
	misc_deregister(&dev_data->misc);
error_app_load:
	kthread_destroy_worker(dev_data->kworker);
error_kworker:
	kthread_stop(dev_data->watchdog_thread);
error_kthread:
	return rc;
}

static int sensorlock_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sensorlock_dev_data *dev_data = dev_get_drvdata(dev);

	kthread_stop(dev_data->watchdog_thread);
	misc_deregister(&dev_data->misc);

	disable_irq(dev_data->sensorlock_irq);
	kthread_flush_worker(dev_data->kworker);
	kthread_destroy_worker(dev_data->kworker);

	SMCI_OBJECT_ASSIGN_NULL(dev_data->sensorlock_app);

	return 0;
}

/* Driver Info */
static const struct of_device_id sensorlock_match_table[] = {
	{ .compatible = "meta,sensorlock", },
	{},
};

static struct platform_driver sensorlock_driver = {
	.driver = {
		.name = "sensorlock",
		.of_match_table = sensorlock_match_table,
		.owner = THIS_MODULE,
	},
	.probe = sensorlock_probe,
	.remove = sensorlock_remove,
};

static int __init sensorlock_init(void)
{
	return platform_driver_register(&sensorlock_driver);
}

static void __exit sensorlock_exit(void)
{
	platform_driver_unregister(&sensorlock_driver);
}

module_init(sensorlock_init);
module_exit(sensorlock_exit);

MODULE_DESCRIPTION("Sensorlock interrupt handler for trustzone input signals");
MODULE_LICENSE("GPL v2");
