// SPDX-License-Identifier: GPL-2.0

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/usb/pd.h>
#include <linux/usb/pdfu.h>
#include <linux/workqueue.h>

#include "../usbvdm.h"
#include "../usbvdm/subscriber.h"

#define MAX_DEVICES_SUPPORTED 8
#define MAX_FW_PATH_LEN 20

enum pdfu_state {
	ENUMERATION,
	ACQUISITION,
	RECONFIGURATION,
	TRANSFER,
	VALIDATION,
	MANIFESTATION,
	COMPLETE,
};

static const char *const pdfu_states[] = {
	[ENUMERATION] = "Enumeration",
	[ACQUISITION] = "Acquisition",
	[RECONFIGURATION] = "Reconfiguration",
	[TRANSFER] = "Transfer",
	[VALIDATION] = "Validation",
	[MANIFESTATION] = "Manifestation",
	[COMPLETE] = "Complete",
};

struct pdfu_data {
	struct device *dev;
	struct usbvdm_subscription *conn_sub;
	struct usbvdm_subscription *subs[MAX_DEVICES_SUPPORTED];
	u16 pid;

	enum pdfu_state state;
	enum pdfu_state prev_state;
	bool sm_running;
	struct work_struct state_machine_work;

	struct mutex rx_lock;
	struct mutex state_lock;
	bool connected;

	struct completion rx_complete;
	struct pdfu_message *rx_msg;

	char fw_path[MAX_FW_PATH_LEN];
	bool fw_manual_override;
	const struct firmware *fw;

	u16 fw_version1;
	u16 fw_version2;
	u16 fw_version3;
	u16 fw_version4;
};

typedef int (*pdfu_state_handler)(struct pdfu_data *);

static int handle_state_enumeration(struct pdfu_data *pdfu);
static int handle_state_acquisition(struct pdfu_data *pdfu);
static int handle_state_reconfiguration(struct pdfu_data *pdfu);
static int handle_state_transfer(struct pdfu_data *pdfu);
static int handle_state_validation(struct pdfu_data *pdfu);
static int handle_state_manifestation(struct pdfu_data *pdfu);

static pdfu_state_handler state_handlers[] = {
	[ENUMERATION] = handle_state_enumeration,
	[ACQUISITION] = handle_state_acquisition,
	[RECONFIGURATION] = handle_state_reconfiguration,
	[TRANSFER] = handle_state_transfer,
	[VALIDATION] = handle_state_validation,
	[MANIFESTATION] = handle_state_manifestation,
};

static int transmit_pdfu_message(struct pdfu_data *pdfu, u8 *payload,
				 size_t payload_len,
				 enum pdfu_req_msg_type msg_type)
{
	struct pdfu_message msg = { 0 };

	if (payload_len > sizeof(msg.payload))
		return -EINVAL;

	msg.header.protocol_version = PDFU_REV10;
	msg.header.msg_type = msg_type;
	memcpy(&msg.payload, payload, payload_len);

	return usbvdm_subscriber_ext_msg(
		pdfu->conn_sub, PD_EXT_FW_UPDATE_REQUEST, (u8 *)&msg,
		payload_len + sizeof(struct pdfu_header));
}

static int transmit_data_block(struct pdfu_data *pdfu, u16 db_index, bool is_nr)
{
	struct pdfu_data_request_payload payload = { 0 };
	size_t db_len;
	const struct firmware *fw = pdfu->fw;
	const size_t fw_index = db_index * PDFU_DATA_BLOCK_MAX_SIZE;

	if (fw_index >= fw->size)
		return -EINVAL;

	db_len = min((size_t)PDFU_DATA_BLOCK_MAX_SIZE, fw->size - fw_index);

	payload.data_block_index = db_index;
	memcpy(&payload.data_block, &fw->data[fw_index], db_len);

	dev_info(pdfu->dev, "Transmitting DataBlockNum=%d, isNR=%d", db_index,
		 is_nr);

	return transmit_pdfu_message(pdfu, (u8 *)&payload,
				     sizeof(db_index) + db_len,
				     is_nr ? REQ_PDFU_DATA_NR : REQ_PDFU_DATA);
}

static int pdfu_await_response(struct pdfu_data *pdfu, unsigned int timeout_ms,
			       enum pdfu_resp_msg_type msg_type, u8 *payload,
			       size_t payload_len)
{
	const unsigned long timeout_j = msecs_to_jiffies(timeout_ms);
	struct pdfu_message response = { 0 };

	if (payload_len > sizeof(response.payload))
		return -EINVAL;

	if (!wait_for_completion_timeout(&pdfu->rx_complete, timeout_j)) {
		dev_err(pdfu->dev, "Timed out waiting for msg_type=%02x",
			msg_type);
		return -ETIMEDOUT;
	}

	mutex_lock(&pdfu->rx_lock);
	memcpy(&response, pdfu->rx_msg, sizeof(struct pdfu_message));
	kfree(pdfu->rx_msg);
	pdfu->rx_msg = NULL;
	mutex_unlock(&pdfu->rx_lock);

	if (response.header.protocol_version != PDFU_REV10 ||
	    response.header.msg_type != msg_type) {
		dev_err(pdfu->dev,
			"Received unexpected PDFU Message, rev=0x%02x msg_type=0x%02x",
			response.header.protocol_version,
			response.header.msg_type);
		return -EBADMSG;
	}

	memcpy(payload, &response.payload, payload_len);
	return 0;
}

static int handle_state_enumeration(struct pdfu_data *pdfu)
{
	struct get_fw_id_response_payload resp_payload = { 0 };
	int i, rc;
	/* TODO(T180734326) Determine acceptable timing parameters */
	const unsigned int timeout = 1000;

	for (i = 0; i < PDFU_N_ENUMERATE_RESEND; i++) {
		msleep(PDFU_T_NEXT_REQUEST_SENT);

		reinit_completion(&pdfu->rx_complete);
		rc = transmit_pdfu_message(pdfu, NULL, 0, REQ_GET_FW_ID);
		if (rc)
			return rc;

		rc = pdfu_await_response(pdfu, timeout, RESP_GET_FW_ID,
					 (u8 *)&resp_payload,
					 sizeof(resp_payload));
		if (!rc)
			break;
		else if (rc != -ETIMEDOUT)
			return rc;
	}

	if (i == PDFU_N_ENUMERATE_RESEND) {
		dev_err(pdfu->dev, "No response after max GET_FW_ID attempts");
		return -ENOMSG;
	}

	pdfu->fw_version1 = resp_payload.fw_version1;
	pdfu->fw_version2 = resp_payload.fw_version2;
	pdfu->fw_version3 = resp_payload.fw_version3;
	pdfu->fw_version4 = resp_payload.fw_version4;

	dev_info(pdfu->dev,
		 "PDFU Enumeration successful, FW_Version=%04x.%04x.%04x.%04x",
		 pdfu->fw_version1, pdfu->fw_version2, pdfu->fw_version3,
		 pdfu->fw_version4);

	return 0;
}

static int handle_state_acquisition(struct pdfu_data *pdfu)
{
	int rc = 0;

	if (!pdfu->fw_manual_override) {
		if (pdfu->pid == VDM_PID_MOKU_BOOTLOADER) {
			/* TODO(T186537706) Replace with parsing function */
			sprintf(pdfu->fw_path, "moku.bin");
		} else if (pdfu->pid == VDM_PID_LEHUA_BOOTLOADER) {
			sprintf(pdfu->fw_path, "lehua.bin");
		} else {
			dev_err(pdfu->dev, "fw_path is null");
			return -EINVAL;
		}
	}

	rc = request_firmware(&pdfu->fw, pdfu->fw_path, pdfu->dev);
	if (rc) {
		dev_err(pdfu->dev, "Failed to get fw for '%s': %d",
			pdfu->fw_path, rc);
		return rc;
	}

	dev_info(pdfu->dev, "Successfully loaded fw image '%s'", pdfu->fw_path);

	return 0;
}

static int handle_state_reconfiguration(struct pdfu_data *pdfu)
{
	struct pdfu_initiate_request_payload req_payload = { 0 };
	struct pdfu_initiate_response_payload resp_payload = { 0 };
	u8 wait_time = 0;
	u32 max_image_size;
	int i, rc;
	/* TODO(T180734326) Determine acceptable timing parameters */
	const unsigned int timeout = 1000;

	for (i = 0; i < PDFU_N_RECONFIGURE_RESEND; i++) {
		msleep(wait_time + PDFU_T_NEXT_REQUEST_SENT);

		/*
		 * TODO(T181477675) include the FW version. Will this come from
		 * userspace or should the driver parse the PDFU File Prefix?
		 */
		reinit_completion(&pdfu->rx_complete);
		rc = transmit_pdfu_message(pdfu, (u8 *)&req_payload,
					   sizeof(req_payload),
					   REQ_PDFU_INITIATE);
		if (rc)
			return rc;

		rc = pdfu_await_response(pdfu, timeout, RESP_PDFU_INITIATE,
					 (u8 *)&resp_payload,
					 sizeof(resp_payload));
		if (rc == -ETIMEDOUT)
			continue;
		else if (rc)
			return rc;

		wait_time = resp_payload.wait_time;
		if (!wait_time || wait_time == PDFU_WAIT_TIME_ERROR)
			break;

		memset(&resp_payload, 0, sizeof(resp_payload));
	}

	if (i == PDFU_N_RECONFIGURE_RESEND) {
		dev_err(pdfu->dev,
			"No response after max PDFU_INITIATE attempts");
		return -ENOMSG;
	}

	if (wait_time != 0 || resp_payload.status != 0) {
		dev_err(pdfu->dev,
			"PDFU Reconfiguration failed: WaitTime=%d, Status=0x%01x",
			wait_time, resp_payload.status);
		return -EBADMSG;
	}

	memcpy(&max_image_size, resp_payload.max_image_size,
	       sizeof(resp_payload.max_image_size));

	dev_info(pdfu->dev,
		 "Responder reconfiguration successful, MaxImageSize=%d",
		 max_image_size);

	return 0;
}

static int handle_state_transfer(struct pdfu_data *pdfu)
{
	struct pdfu_data_response_payload response;
	u8 wait_time = 0;
	u8 num_nr = 0;
	u16 prev_db = 0, next_db = 0;
	const u16 final_db =
		DIV_ROUND_UP(pdfu->fw->size, PDFU_DATA_BLOCK_MAX_SIZE) - 1;
	/* TODO(T180734326) Determine acceptable timing parameters */
	const unsigned int timeout = 5000;
	int rc = 0;

	while (prev_db != final_db && wait_time != PDFU_WAIT_TIME_DONE) {
		msleep(PDFU_T_NEXT_REQUEST_SENT + wait_time);

		/*
		 * Send as many PDFU_DATA_NR messages as allowed by Responder.
		 * First and final blocks must be PDFU_DATA messages.
		 */
		while (num_nr > 0 && next_db != 0 && next_db != final_db) {
			rc = transmit_data_block(pdfu, next_db, true);
			if (rc)
				return rc;

			--num_nr;
			++next_db;
			msleep(PDFU_T_NEXT_REQUEST_SENT);
		}

		reinit_completion(&pdfu->rx_complete);
		rc = transmit_data_block(pdfu, next_db, false);
		if (rc)
			return rc;

		rc = pdfu_await_response(pdfu, timeout, RESP_PDFU_DATA,
					 (u8 *)&response, sizeof(response));
		if (rc)
			/* TODO(T181477573) Implement retries */
			// Supposed to retry on timeout here, but just quit
			return rc;

		if (response.status != 0) {
			dev_err(pdfu->dev,
				"Received PDFU_DATA Response with err status=%x",
				response.status);
			return -EINVAL;
		}

		wait_time = response.wait_time;
		num_nr = response.num_data_nr;

		prev_db = next_db;
		next_db = response.data_block_num;
	}

	return 0;
}

static int handle_state_validation(struct pdfu_data *pdfu)
{
	struct pdfu_validate_response_payload resp_payload = { 0 };
	u8 wait_time = 0;
	int i, rc;
	/* TODO(T180734326) Determine acceptable timing parameters */
	const unsigned int timeout = 1000;

	for (i = 0; i < PDFU_N_VALIDATE_RESEND; i++) {
		msleep(PDFU_T_NEXT_REQUEST_SENT + wait_time);

		reinit_completion(&pdfu->rx_complete);
		rc = transmit_pdfu_message(pdfu, NULL, 0, REQ_PDFU_VALIDATE);
		if (rc)
			return rc;

		rc = pdfu_await_response(pdfu, timeout, RESP_PDFU_VALIDATE,
					 (u8 *)&resp_payload,
					 sizeof(resp_payload));
		if (rc == -ETIMEDOUT)
			continue;
		else if (rc)
			return rc;

		wait_time = resp_payload.wait_time;
		if (!wait_time || wait_time == PDFU_WAIT_TIME_ERROR)
			break;

		memset(&resp_payload, 0, sizeof(resp_payload));
	}

	if (i == PDFU_N_VALIDATE_RESEND) {
		dev_err(pdfu->dev,
			"No response after max PDFU_VALIDATE attempts");
		return -ENOMSG;
	}

	if (wait_time != 0 || resp_payload.status != 0 ||
	    resp_payload.flags != 1) {
		dev_err(pdfu->dev,
			"PDFU Validation failed: WaitTime=%d, Status=0x%01x, Flags=%d",
			wait_time, resp_payload.status, resp_payload.flags);
		return -EBADMSG;
	}

	dev_info(pdfu->dev, "Responder validation successful");

	return 0;
}

static int handle_state_manifestation(struct pdfu_data *pdfu)
{
	return 0;
}

static inline void advance_state(struct pdfu_data *pdfu)
{
	pdfu->prev_state = pdfu->state;
	if (pdfu->state < COMPLETE)
		pdfu->state += 1;
}

static int run_state_machine(struct pdfu_data *pdfu)
{
	int rc = 0;

	dev_info(pdfu->dev, "Launching PDFU state machine");

	pdfu->sm_running = true;
	pdfu->state = ENUMERATION;
	do {
		rc = state_handlers[pdfu->state](pdfu);
		if (rc)
			break;

		advance_state(pdfu);

		dev_info(pdfu->dev, "PDFU State Machine: [%s] -> [%s]",
			 pdfu_states[pdfu->prev_state],
			 pdfu_states[pdfu->state]);
	} while (pdfu->state < COMPLETE);

	dev_info(pdfu->dev, "Exiting PDFU state machine, rc=%d", rc);

	/* TODO(T181477454) create a reset function */
	kfree(pdfu->rx_msg);
	release_firmware(pdfu->fw);
	pdfu->rx_msg = NULL;
	pdfu->sm_running = false;
	pdfu->fw = NULL;
	pdfu->fw_manual_override = false;

	return rc;
}

static void state_machine_work_fn(struct work_struct *work)
{
	struct pdfu_data *pdfu =
		container_of(work, struct pdfu_data, state_machine_work);

	run_state_machine(pdfu);
}

static void pdfu_usbvdm_connect(struct usbvdm_subscription *sub, u16 vid,
				u16 pid)
{
	struct pdfu_data *pdfu = usbvdm_subscriber_get_drvdata(sub);

	if (pdfu->connected) {
		dev_warn(pdfu->dev,
			 "Already connected: ignoring connect callback\n");
		return;
	}

	mutex_lock(&pdfu->state_lock);
	pdfu->connected = true;
	pdfu->conn_sub = sub;
	pdfu->pid = pid;
	mutex_unlock(&pdfu->state_lock);

	pm_stay_awake(pdfu->dev);

	sysfs_notify(&pdfu->dev->kobj, NULL, "connected");
}

static void pdfu_usbvdm_disconnect(struct usbvdm_subscription *sub)
{
	struct pdfu_data *pdfu = usbvdm_subscriber_get_drvdata(sub);

	if (!pdfu->connected) {
		dev_warn(
			pdfu->dev,
			"Already disconnected: ignoring disconnect callback\n");
		return;
	}

	mutex_lock(&pdfu->state_lock);
	pdfu->connected = false;
	pdfu->conn_sub = NULL;
	pdfu->pid = 0;
	mutex_unlock(&pdfu->state_lock);

	pm_relax(pdfu->dev);

	sysfs_notify(&pdfu->dev->kobj, NULL, "connected");
}

static void pdfu_rx_ext_msg(struct usbvdm_subscription *sub, u8 msg_type,
			    const u8 *data, size_t data_len)
{
	struct pdfu_data *pdfu = usbvdm_subscriber_get_drvdata(sub);
	struct device *dev = pdfu->dev;
	struct pdfu_message *rx_msg;

	if (msg_type != PD_EXT_FW_UPDATE_RESPONSE) {
		dev_dbg(dev, "Received non-FW_Update msg, MsgType=%02x",
			msg_type);
		return;
	}

	if (data_len < sizeof(struct pdfu_header) ||
	    data_len > PD_MAX_EXT_MSG_LEN) {
		dev_err(dev, "Received PDFU message of incorrect size=%zu",
			data_len);
		return;
	}

	if (pdfu->rx_msg) {
		dev_warn(dev,
			 "Received FW_Update msg before prev msg was handled");
		return;
	}

	rx_msg = kzalloc(sizeof(struct pdfu_message), GFP_KERNEL);
	if (!rx_msg)
		return;

	memcpy(rx_msg, data, data_len);

	mutex_lock(&pdfu->rx_lock);
	pdfu->rx_msg = rx_msg;
	mutex_unlock(&pdfu->rx_lock);

	complete(&pdfu->rx_complete);

	dev_info(dev, "Received FW_Update msg");
}

static const struct usbvdm_subscriber_ops ops = {
	.connect = pdfu_usbvdm_connect,
	.disconnect = pdfu_usbvdm_disconnect,
	.ext_msg = pdfu_rx_ext_msg,
};

static ssize_t connected_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct pdfu_data *pdfu = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", pdfu->connected);
}
static DEVICE_ATTR_RO(connected);

static ssize_t fw_path_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct pdfu_data *pdfu = dev_get_drvdata(dev);

	if (count > MAX_FW_PATH_LEN)
		return -EINVAL;

	pdfu->fw_manual_override = true;

	strncpy(pdfu->fw_path, buf, count);

	dev_info(pdfu->dev, "Updated fw_path to '%s'", pdfu->fw_path);

	return count;
}
static DEVICE_ATTR_WO(fw_path);

static ssize_t responder_fw_version_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct pdfu_data *pdfu = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%04x.%04x.%04x.%04x\n",
			 pdfu->fw_version1, pdfu->fw_version2,
			 pdfu->fw_version3, pdfu->fw_version4);
}
static DEVICE_ATTR_RO(responder_fw_version);

static ssize_t start_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct pdfu_data *pdfu = dev_get_drvdata(dev);
	int rc;

	if (pdfu->sm_running)
		return -EBUSY;

	rc = run_state_machine(pdfu);
	if (rc)
		return rc;

	return scnprintf(buf, PAGE_SIZE,
			 "PDFU update completed successfully\n");
}
static DEVICE_ATTR_RO(start);

static ssize_t start_async_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pdfu_data *pdfu = dev_get_drvdata(dev);

	if (pdfu->sm_running)
		return -EBUSY;

	schedule_work(&pdfu->state_machine_work);
	return scnprintf(buf, PAGE_SIZE,
			 "Started PDFU update asynchronously\n");
}
static DEVICE_ATTR_RO(start_async);

static struct attribute *pdfu_initiator_attrs[] = {
	&dev_attr_connected.attr,
	&dev_attr_fw_path.attr,
	&dev_attr_responder_fw_version.attr,
	&dev_attr_start.attr,
	&dev_attr_start_async.attr,
	NULL,
};
ATTRIBUTE_GROUPS(pdfu_initiator);

static int pdfu_initiator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pdfu_data *pdfu = NULL;
	const char *pids_prop = "usb,product-ids";
	u16 pids[MAX_DEVICES_SUPPORTED];
	int i, num_pids, rc = 0;

	pdfu = devm_kzalloc(dev, sizeof(struct pdfu_data), GFP_KERNEL);
	if (!pdfu)
		return -ENOMEM;

	pdfu->dev = dev;
	dev_set_drvdata(dev, pdfu);

	INIT_WORK(&pdfu->state_machine_work, state_machine_work_fn);
	init_completion(&pdfu->rx_complete);
	mutex_init(&pdfu->rx_lock);
	mutex_init(&pdfu->state_lock);

	rc = num_pids = device_property_count_u16(dev, pids_prop);
	if (rc < 0)
		return rc;
	else if (num_pids > MAX_DEVICES_SUPPORTED)
		return -EINVAL;

	rc = device_property_read_u16_array(dev, pids_prop, pids, num_pids);
	if (rc < 0)
		return rc;

	rc = device_init_wakeup(pdfu->dev, true);
	if (rc < 0)
		return rc;

	for (i = 0; i < num_pids; i++) {
		pdfu->subs[i] =
			usbvdm_subscribe(dev, VDM_SVID_META, pids[i], ops);
		if (IS_ERR_OR_NULL(pdfu->subs[i]))
			return IS_ERR(pdfu->subs[i]) ? PTR_ERR(pdfu) : -ENODEV;

		usbvdm_subscriber_set_drvdata(pdfu->subs[i], pdfu);
	}

	rc = sysfs_create_groups(&dev->kobj, pdfu_initiator_groups);
	if (rc)
		return rc;

	return 0;
}

static int pdfu_initiator_remove(struct platform_device *pdev)
{
	struct pdfu_data *pdfu = platform_get_drvdata(pdev);
	int i;

	device_init_wakeup(pdfu->dev, false);
	for (i = 0; i < MAX_DEVICES_SUPPORTED; i++)
		usbvdm_unsubscribe(pdfu->subs[i]);
	kfree(pdfu->rx_msg);
	release_firmware(pdfu->fw);
	sysfs_remove_groups(&pdfu->dev->kobj, pdfu_initiator_groups);

	return 0;
}

static const struct of_device_id pdfu_initiator_match_table[] = {
	{ .compatible = "meta,pdfu-initiator" },
	{},
};

static struct platform_driver pdfu_initiator_driver = {
	.driver	= {
		.name = "pdfu_initiator",
		.of_match_table = pdfu_initiator_match_table,
	},
	.probe	= pdfu_initiator_probe,
	.remove	= pdfu_initiator_remove,
};

module_platform_driver(pdfu_initiator_driver);

MODULE_DESCRIPTION("PDFU Initiator driver");
MODULE_LICENSE("GPL v2");
