// SPDX-License-Identifier: GPL-2.0

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>

#include "molokini.h"

#define DEFAULT_MOLOKINI_VID 0x2833

/* Vendor Defined Message Header Macros */
#define VDMH_PARAMETER(h) (h & 0xFF)
#define VDMH_SIZE(h) ((h & 0x700) >> 8)
#define VDMH_HIGH(h) ((h & 0x800) >> 11)
#define VDMH_PROTOCOL(h) ((h & 0x3000) >> 12)
#define VDMH_ACK(h) ((h & 0x4000) >> 14)

static const size_t molokini_size_bytes[] = { 1, 2, 4, 8, 16, 32 };

static void vdm_connect(struct usbpd_svid_handler *hdlr,
		bool supports_usb_comm)
{
	struct molokini_pd *mpd =
		container_of(hdlr, struct molokini_pd, vdm_handler);

	dev_dbg(mpd->dev, "enter: usb-com=%d\n", supports_usb_comm);

	/* Client notifications only occur during connect/disconnect
	 * state transitions. This should not be called when Molokini
	 * is already connected.
	 */
	BUG_ON(mpd->connected);

	mutex_lock(&mpd->lock);
	mpd->connected = true;
	mutex_unlock(&mpd->lock);
}

static void vdm_disconnect(struct usbpd_svid_handler *hdlr)
{
	struct molokini_pd *mpd =
		container_of(hdlr, struct molokini_pd, vdm_handler);

	dev_dbg(mpd->dev, "enter\n");

	/* Client notifications only occur during connect/disconnect
	 * state transitions. This should not be called when Molokini
	 * is already disconnected.
	 */
	BUG_ON(!mpd->connected);

	mutex_lock(&mpd->lock);
	mpd->connected = false;
	mutex_unlock(&mpd->lock);
}

static void vdm_received(struct usbpd_svid_handler *hdlr, u32 vdm_hdr,
		const u32 *vdos, int num_vdos)
{
	u32 protocol_type, parameter_type, sb, high_bytes;
	struct molokini_pd *mpd =
		container_of(hdlr, struct molokini_pd, vdm_handler);

	dev_dbg(mpd->dev,
		"enter: vdm_hdr=0x%x, vdos?=%d, num_vdos=%d\n",
		vdm_hdr, vdos != NULL, num_vdos);

	protocol_type = VDMH_PROTOCOL(vdm_hdr);
	if (protocol_type != MOLOKINI_FW_BROADCAST) {
		dev_warn(mpd->dev, "Usupported Protocol=%d\n", protocol_type);
		return;
	}

	if (num_vdos == 0) {
		dev_warn(mpd->dev, "Empty VDO packets vdm_hdr=0x%x\n", vdm_hdr);
		return;
	}

	sb = VDMH_SIZE(vdm_hdr);
	high_bytes = VDMH_HIGH(vdm_hdr);
	parameter_type = VDMH_PARAMETER(vdm_hdr);
	switch (parameter_type) {
	case MOLOKINI_FW_SERIAL:
		memcpy(mpd->params.serial,
			vdos, sizeof(mpd->params.serial));
		break;
	case MOLOKINI_FW_SERIAL_BATTERY:
		memcpy(mpd->params.serial_battery,
			vdos, sizeof(mpd->params.serial_battery));
		break;
	case MOLOKINI_FW_TEMP_FG:
		mpd->params.temp_fg = vdos[0];
		break;
	case MOLOKINI_FW_VOLTAGE:
		mpd->params.voltage = vdos[0];
		break;
	case MOLOKINI_FW_BATT_STATUS:
		mpd->params.battery_status = vdos[0];
		break;
	case MOLOKINI_FW_CURRENT:
		mpd->params.icurrent = vdos[0]; /* negative range */
		break;
	case MOLOKINI_FW_REMAINING_CAPACITY:
		mpd->params.remaining_capacity = vdos[0];
		break;
	case MOLOKINI_FW_FCC:
		mpd->params.fcc = vdos[0];
		break;
	case MOLOKINI_FW_CYCLE_COUNT:
		mpd->params.cycle_count = vdos[0];
		break;
	case MOLOKINI_FW_RSOC:
		mpd->params.rsoc = vdos[0];
		break;
	case MOLOKINI_FW_SOH:
		mpd->params.soh = vdos[0];
		break;
	case MOLOKINI_FW_DEVICE_NAME:
		if (mpd->params.device_name[0] == '\0' &&
			molokini_size_bytes[sb] <=
				sizeof(u32) * num_vdos &&
			molokini_size_bytes[sb] <=
				sizeof(mpd->params.device_name)) {
			memcpy(mpd->params.device_name,
				vdos, molokini_size_bytes[sb]);
		}
		break;
	case MOLOKINI_FW_LDB1:
		memcpy(mpd->params.lifetime1_lower, vdos,
			sizeof(mpd->params.lifetime1_lower));
		memcpy(mpd->params.lifetime1_higher,
			vdos + LIFETIME_1_LOWER_LEN,
			sizeof(mpd->params.lifetime1_higher));
		break;
	case MOLOKINI_FW_LDB3:
		mpd->params.lifetime3 = vdos[0];
		break;
	case MOLOKINI_FW_LDB4:
		memcpy(mpd->params.lifetime4,
			vdos, sizeof(mpd->params.lifetime4));
		break;
	case MOLOKINI_FW_LDB6:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[0],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[0] +
				VDOS_MAX_BYTES,
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB7:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[1],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[1] +
				VDOS_MAX_BYTES,
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB8:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[2],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[2] +
				VDOS_MAX_BYTES,
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB9:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[3],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[3] +
				VDOS_MAX_BYTES,
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB10:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[4],
					vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[4] +
				VDOS_MAX_BYTES,
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB11:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[5],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[5] +
				VDOS_MAX_BYTES,
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_LDB12:
		if (!high_bytes)
			memcpy(mpd->params.temp_zones[6],
				vdos, VDOS_MAX_BYTES);
		else
			memcpy(mpd->params.temp_zones[6] +
				VDOS_MAX_BYTES,
				vdos, VDOS_MAX_BYTES);
		break;
	case MOLOKINI_FW_MANUFACTURER_INFO1:
		mpd->params.manufacturer_info_a = vdos[0];
		break;
	case MOLOKINI_FW_MANUFACTURER_INFO2:
		mpd->params.manufacturer_info_b = vdos[0];
		break;
	case MOLOKINI_FW_MANUFACTURER_INFO3:
		mpd->params.manufacturer_info_c = vdos[0];
		break;
	}
}

static ssize_t mpd_dbg_device_name(struct file *file,
			char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct molokini_pd *mpd = file->private_data;

	return simple_read_from_buffer(user_buf, count, ppos,
		mpd->params.device_name, strlen(mpd->params.device_name));
}

static const struct file_operations device_name_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = mpd_dbg_device_name,
};

static int molokini_serial(struct seq_file *s, void *p)
{
	struct molokini_pd *mpd = s->private;

	seq_printf(s, "%s\n", mpd->params.serial);
	return 0;
}

static int molokini_serial_battery(struct seq_file *s, void *p)
{
	struct molokini_pd *mpd = s->private;

	seq_printf(s, "%s\n", mpd->params.serial_battery);
	return 0;
}

static int molokini_serial_open(struct inode *inode, struct file *file)
{
	return single_open(file, molokini_serial, inode->i_private);
}

static int molokini_serial_battery_open(struct inode *inode, struct file *file)
{
	return single_open(file, molokini_serial_battery, inode->i_private);
}

static const struct file_operations serial_ops = {
	.owner = THIS_MODULE,
	.open = molokini_serial_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.release = single_release,
};

static const struct file_operations serial_battery_ops = {
	.owner = THIS_MODULE,
	.open = molokini_serial_battery_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.release = single_release,
};

static void molokini_create_debugfs(struct molokini_pd *mpd)
{
	static const char * const zone[] = {
		"ut", "lt", "stl", "rt", "sth", "ht", "ot" };
	static const char * const level[] = {
		"a", "b", "c", "d", "e", "f", "g", "h" };
	char temp[13];
	struct dentry *ent;
	int i, j;

	mpd->debug_root = debugfs_create_dir("molokini", NULL);
	if (!mpd->debug_root) {
		dev_warn(mpd->dev, "Couldn't create debug dir\n");
		return;
	}

	/* Manufacturing serial ndoes. */
	ent = debugfs_create_file("serial", 0400, mpd->debug_root,
		mpd, &serial_ops);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create serial file\n");

	ent = debugfs_create_file("serial_battery", 0400, mpd->debug_root,
		mpd, &serial_battery_ops);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create serial_battery file\n");

	/* Standard nodes*/
	ent = debugfs_create_bool("connected", 0400, mpd->debug_root,
		&mpd->connected);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create connected file\n");

	ent = debugfs_create_u16("temp_fg", 0400, mpd->debug_root,
		&mpd->params.temp_fg);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create temp_fg file\n");

	ent = debugfs_create_u16("voltage", 0400, mpd->debug_root,
		&mpd->params.voltage);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create voltage file\n");

	ent = debugfs_create_u16("status", 0400, mpd->debug_root,
		&mpd->params.battery_status);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create status file\n");

	ent = debugfs_create_x16("current", 0400, mpd->debug_root,
		&mpd->params.icurrent);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create current file\n");

	ent = debugfs_create_u16("remaining_capacity", 0400, mpd->debug_root,
		&mpd->params.remaining_capacity);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create remaining_capacity file\n");

	ent = debugfs_create_u16("fcc", 0400, mpd->debug_root,
		&mpd->params.fcc);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create fcc file\n");

	ent = debugfs_create_u16("cycle_count", 0400, mpd->debug_root,
		&mpd->params.cycle_count);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create cycle_count file\n");

	ent = debugfs_create_u8("rsoc", 0400, mpd->debug_root,
		&mpd->params.rsoc);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create rsoc file\n");

	ent = debugfs_create_u8("soh", 0400, mpd->debug_root,
		&mpd->params.soh);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create soh file\n");

	ent = debugfs_create_file("device_name", 0400, mpd->debug_root, mpd,
		&device_name_ops);
	if (!ent)
		dev_warn(mpd->dev, "Couldn't create device_name file\n");

	/* Lifetime nodes */
	debugfs_create_u16("cell_1_max_voltage", 0400, mpd->debug_root,
			&mpd->params.lifetime1_lower[0]);
	debugfs_create_u16("cell_1_min_voltage", 0400, mpd->debug_root,
			&mpd->params.lifetime1_lower[1]);
	debugfs_create_x16("max_charge_current", 0400, mpd->debug_root,
			&mpd->params.lifetime1_lower[2]);
	debugfs_create_x16("max_discharge_current", 0400, mpd->debug_root,
			&mpd->params.lifetime1_lower[3]);
	debugfs_create_x16("max_avg_discharge_current", 0400, mpd->debug_root,
			&mpd->params.lifetime1_lower[4]);
	debugfs_create_x16("max_avg_dsg_power", 0400, mpd->debug_root,
			&mpd->params.lifetime1_lower[5]);
	debugfs_create_x8("max_temp_cell", 0400, mpd->debug_root,
			&mpd->params.lifetime1_higher[0]);
	debugfs_create_x8("min_temp_cell", 0400, mpd->debug_root,
			&mpd->params.lifetime1_higher[1]);
	debugfs_create_x8("max_temp_int_sensor", 0400, mpd->debug_root,
			&mpd->params.lifetime1_higher[2]);
	debugfs_create_x8("min_temp_int_sensor", 0400, mpd->debug_root,
			&mpd->params.lifetime1_higher[3]);

	debugfs_create_u32("total_fw_runtime", 0400, mpd->debug_root,
			&mpd->params.lifetime3);

	debugfs_create_u16("num_valid_charge_terminations", 0400,
			mpd->debug_root, &mpd->params.lifetime4[0]);
	debugfs_create_u16("last_valid_charge_term", 0400,
			mpd->debug_root, &mpd->params.lifetime4[1]);
	debugfs_create_u16("num_qmax_updates", 0400, mpd->debug_root,
			&mpd->params.lifetime4[2]);
	debugfs_create_u16("last_qmax_update", 0400, mpd->debug_root,
			&mpd->params.lifetime4[3]);
	debugfs_create_u16("num_ra_update", 0400, mpd->debug_root,
			&mpd->params.lifetime4[4]);
	debugfs_create_u16("last_ra_update", 0400, mpd->debug_root,
			&mpd->params.lifetime4[5]);

	/* Lifetime temperature zone nodes */
	for (i = 0; i < NUM_TEMP_ZONE; i++)
		for (j = 0; j < TEMP_ZONE_LEN; j++) {
			snprintf(temp, sizeof(temp),
				"t_%s_rsoc_%s", zone[i], level[j]);
			debugfs_create_u32(temp, 0400, mpd->debug_root,
					&mpd->params.temp_zones[i][j]);
		}
}


static int molokini_probe(struct platform_device *pdev)
{
	int result = 0;
	struct molokini_pd *mpd;

	dev_dbg(&pdev->dev, "enter\n");

	mpd = devm_kzalloc(
			&pdev->dev, sizeof(*mpd), GFP_KERNEL);
	if (!mpd)
		return -ENOMEM;

	mpd->upd = devm_usbpd_get_by_phandle(&pdev->dev, "battery-usbpd");
	if (IS_ERR_OR_NULL(mpd->upd)) {
		dev_err(&pdev->dev, "devm_usbpd_get_by_phandle failed: %ld\n",
				PTR_ERR(mpd->upd));
		return PTR_ERR(mpd->upd);
	}

	mpd->vdm_handler.connect = vdm_connect;
	mpd->vdm_handler.disconnect = vdm_disconnect;
	mpd->vdm_handler.vdm_received = vdm_received;

	result = of_property_read_u16(pdev->dev.of_node,
		"svid", &mpd->vdm_handler.svid);
	if (result < 0) {
		mpd->vdm_handler.svid = DEFAULT_MOLOKINI_VID;
		dev_err(&pdev->dev,
			"svid unavailable, defaulting to svid=%x, result:%d\n",
			DEFAULT_MOLOKINI_VID, result);
	}
	dev_dbg(&pdev->dev, "Property svid=%x", mpd->vdm_handler.svid);

	mutex_init(&mpd->lock);

	/* Register VDM callbacks */
	result = usbpd_register_svid(mpd->upd, &mpd->vdm_handler);
	if (result != 0) {
		dev_err(&pdev->dev, "Failed to register vdm handler");
		return result;
	}
	dev_dbg(&pdev->dev, "Registered svid 0x%04x",
		mpd->vdm_handler.svid);

	mpd->dev = &pdev->dev;

	/* Create debugfs here. */
	molokini_create_debugfs(mpd);

	return result;
}

static int molokini_remove(struct platform_device *pdev)
{
	struct molokini_pd *mpd = platform_get_drvdata(pdev);

	dev_dbg(mpd->dev, "enter\n");

	if (mpd->upd != NULL)
		usbpd_unregister_svid(mpd->upd, &mpd->vdm_handler);

	mutex_destroy(&mpd->lock);

	return 0;
}

/* Driver Info */
static const struct of_device_id molokini_match_table[] = {
		{ .compatible = "oculus,molokini", },
		{ },
};

static struct platform_driver molokini_driver = {
	.driver = {
		.name = "oculus,molokini",
		.of_match_table = molokini_match_table,
		.owner = THIS_MODULE,
	},
	.probe	= molokini_probe,
	.remove = molokini_remove,
};

static int __init molokini_init(void)
{
	platform_driver_register(&molokini_driver);
	return 0;
}

static void __exit molokini_exit(void)
{
	platform_driver_unregister(&molokini_driver);
}

module_init(molokini_init);
module_exit(molokini_exit);

MODULE_DESCRIPTION("Molokini driver");
MODULE_LICENSE("GPL");
