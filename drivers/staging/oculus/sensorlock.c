// SPDX-License-Identifier: GPL-2.0

#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <soc/qcom/smci_appclient.h>
#include <soc/qcom/smci_appcontroller.h>
#include <soc/qcom/smci_apploader.h>
#include <soc/qcom/smci_clientenv.h>
#include <soc/qcom/smci_object.h>

#define HANDLE_CMD_OP 0
#define SENSORLOCK_CMDS_VERSION 1
#define SENSORLOCK_CMD_CHECK_SIGNAL 0
#define SENSORLOCK_APP_NAME "sensorlock64"
#define SENSORLOCK_FIRMWARE_NAME "sensorlock64.mbn"

enum sensorlock_state {
	STATE_DISABLED = 1,
	STATE_DISENGAGED = 2,
	STATE_ENGAGED = 3,
};

struct qsc_send_cmd {
	uint32_t version;
	uint32_t cmd_id;
} __attribute__((__packed__));

struct qsc_send_cmd_rsp {
	uint32_t version;
	int32_t status;
	enum sensorlock_state state;
} __attribute__((__packed__));

struct sensorlock_dev_data {
	struct device *dev;
	struct miscdevice misc;
	struct mutex lock;
	struct smci_object sensorlock_app;
	struct work_struct sensorlock_event;
	wait_queue_head_t sensorlock_wait_queue;
	atomic_t waiting_for_event;
	atomic_t state;
	bool is_app_connected;
};

/* Character device read operation blocks until a signal has been sent to the Trustzone application */
static ssize_t sensorlock_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	enum sensorlock_state state;
	struct sensorlock_dev_data *dev_data = container_of(filp->private_data, struct sensorlock_dev_data, misc);

	if (len < sizeof(enum sensorlock_state)) {
		dev_err(dev_data->dev, "output buffer state must be at least %lu bytes", sizeof(enum sensorlock_state));
		return -EINVAL;
	}

	atomic_set(&dev_data->waiting_for_event, 1);
	wait_event_interruptible(dev_data->sensorlock_wait_queue, !atomic_read(&dev_data->waiting_for_event));
	state = atomic_read(&dev_data->state);
	if (copy_to_user(buf, &state, sizeof(enum sensorlock_state))) {
		dev_err(dev_data->dev, "could not copy sensorlock state to userspace");
		return -EFAULT;
	}

	return sizeof(enum sensorlock_state);
}

static const struct file_operations fops = {
	.owner          = THIS_MODULE,
	.read           = sensorlock_read,
};

/* Call the sensorlock trustzone function check_signals */
static int sensorlock_check_signals_locked(struct sensorlock_dev_data *dev_data)
{
	int rc = 0;
	union smci_object_arg args[2];
	struct qsc_send_cmd cmd = {SENSORLOCK_CMDS_VERSION, SENSORLOCK_CMD_CHECK_SIGNAL};
	struct qsc_send_cmd_rsp rsp = {};

	if (!dev_data) {
		pr_err("%s: dev_data is null\n", __func__);
		return -EINVAL;
	}

	args[0].b.ptr = &cmd;
	args[0].b.size = sizeof(cmd);
	args[1].b.ptr = &rsp;
	args[1].b.size = sizeof(rsp);

	rc = smci_object_invoke(dev_data->sensorlock_app, HANDLE_CMD_OP, args, SMCI_OBJECT_COUNTS_PACK(1, 1, 0, 0));
	if (rc == SMCI_OBJECT_OK) {
		atomic_set(&dev_data->state, rsp.state);
		atomic_set(&dev_data->waiting_for_event, 0);
		wake_up_all(&dev_data->sensorlock_wait_queue);
	}

	return rc;
}

/* Allows the driver to connect to a loaded trustzone application in order to call commands */
static int sensorlock_app_connect(struct sensorlock_dev_data *dev_data)
{
	int rc = 0;
	struct smci_object client_env = {NULL, NULL};
	struct smci_object app_client = {NULL, NULL};
	const size_t name_len = strlen(SENSORLOCK_APP_NAME);

	if (!dev_data) {
		pr_err("%s: dev_data is null\n", __func__);
		return -EINVAL;
	}

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

/* Loads the sensorlock trustzone application into the trustzone driver */
static int sensorlock_app_load(struct sensorlock_dev_data *dev_data)
{
	int rc = 0;
	const struct firmware *app_data;
	struct smci_object client_env = {NULL, NULL};
	struct smci_object app_loader = {NULL, NULL};
	struct smci_object app_controller = {NULL, NULL};
	struct smci_object app_obj = {NULL, NULL};

	if (!dev_data) {
		pr_err("%s: dev_data is null\n", __func__);
		return -EINVAL;
	}

	/* Nothing to do */
	if (dev_data->is_app_connected) {
		dev_info(dev_data->dev, "sensorlock trustzone application is already loaded\n");
		return 0;
	}

	rc = request_firmware(&app_data, SENSORLOCK_FIRMWARE_NAME, dev_data->dev);
	if (rc) {
		dev_err(dev_data->dev, "sensorlock request_firmware failed: %d\n", rc);
		return -EIO;
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
	return rc;
}

static irqreturn_t sensorlock_irq_handler(int irq, void *dev_id)
{
	struct sensorlock_dev_data *dev_data = dev_id;

	if (!dev_data) {
		pr_err("%s: invalid dev_data state\n", __func__);
		return -EINVAL;
	}

	queue_work(system_highpri_wq, &dev_data->sensorlock_event);
	return IRQ_HANDLED;
}

static void sensorlock_work_handler(struct work_struct *work)
{
	struct sensorlock_dev_data *dev_data = container_of(work, struct sensorlock_dev_data, sensorlock_event);

	if (!dev_data) {
		pr_err("%s: invalid dev_data state\n", __func__);
		return;
	}

	/* We should only need to connect to the Trustzone application once */
	mutex_lock(&dev_data->lock);
	if (!dev_data->is_app_connected && sensorlock_app_connect(dev_data))
		dev_err(dev_data->dev, "Could not connect to the sensorlock trustzone application");

	if (dev_data->is_app_connected)
		sensorlock_check_signals_locked(dev_data);

	mutex_unlock(&dev_data->lock);
}

static int __init sensorlock_probe(struct platform_device *pdev)
{
	int rc = 0;
	unsigned int sensorlock_power_irq = 0;
	unsigned int sensorlock_voltage_sense_irq = 0;
	struct sensorlock_dev_data *dev_data = NULL;
	struct device *dev = &pdev->dev;

	dev_data = devm_kzalloc(dev, sizeof(struct sensorlock_dev_data), GFP_KERNEL);
	if (!dev_data)
		return -ENOMEM;

	dev_data->dev = dev;
	dev_data->is_app_connected = false;
	mutex_init(&dev_data->lock);
	atomic_set(&dev_data->waiting_for_event, 0);
	SMCI_OBJECT_ASSIGN_NULL(dev_data->sensorlock_app);
	INIT_WORK(&dev_data->sensorlock_event, sensorlock_work_handler);
	init_waitqueue_head(&dev_data->sensorlock_wait_queue);
	dev_set_drvdata(dev, dev_data);

	dev_data->misc.name = "sensorlock";
	dev_data->misc.minor = MISC_DYNAMIC_MINOR;
	dev_data->misc.fops = &fops;

	rc = misc_register(&dev_data->misc);
	if (rc) {
		dev_err(dev, "error creating misc device");
		goto error_misc;
	}

	rc = sensorlock_app_load(dev_data);
	if (rc) {
		dev_err(dev, "error loading the trustzone application: %d", rc);
		goto error_trustzone;
	}

	sensorlock_power_irq = platform_get_irq_byname(pdev, "sensorlock_power_irq");
	if (sensorlock_power_irq < 0) {
		dev_err(dev, "Unable to find sensorlock power button IRQ");
		rc = -EINVAL;
		goto error_trustzone;
	}

	sensorlock_voltage_sense_irq = platform_get_irq_byname(pdev, "sensorlock_voltage_sense_irq");
	if (sensorlock_voltage_sense_irq < 0) {
		dev_err(dev, "Unable to find sensorlock voltage sense IRQ");
		rc = -EINVAL;
		goto error_trustzone;
	}

	rc = devm_request_threaded_irq(dev,
				sensorlock_power_irq,
				NULL, sensorlock_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"sensorlock_power_irq", dev_data);
	if (rc) {
		dev_err(dev, "failed to set sensorlock power irq handler");
		goto error_trustzone;
	}

	rc = devm_request_threaded_irq(dev,
				sensorlock_voltage_sense_irq,
				NULL, sensorlock_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"sensorlock_voltage_sense_irq", dev_data);
	if (rc) {
		dev_err(dev, "failed to set sensorlock voltage sense irq handler");
		goto error_trustzone;
	}

	return 0;

error_trustzone:
	misc_deregister(&dev_data->misc);
error_misc:
	mutex_destroy(&dev_data->lock);
	return rc;
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
};

builtin_platform_driver_probe(sensorlock_driver, sensorlock_probe);
MODULE_DESCRIPTION("Sensorlock interrupt handler for trustzone input signals");
MODULE_LICENSE("GPL v2");
