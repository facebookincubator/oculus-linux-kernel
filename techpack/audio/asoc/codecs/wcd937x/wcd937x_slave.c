// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/component.h>
#include <soc/soundwire.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define SWR_SLV_MAX_REG_ADDR    0x2009
#define SWR_SLV_START_REG_ADDR  0x40
#define SWR_SLV_MAX_BUF_LEN     20
#define BYTES_PER_LINE          12
#define SWR_SLV_RD_BUF_LEN      8
#define SWR_SLV_WR_BUF_LEN      32
#define SWR_SLV_MAX_DEVICES     2
#endif /* CONFIG_DEBUG_FS */

struct wcd937x_slave_priv {
	struct swr_device *swr_slave;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_wcd937x_dent;
	struct dentry *debugfs_peek;
	struct dentry *debugfs_poke;
	struct dentry *debugfs_reg_dump;
	unsigned int read_data;
#endif
};

#ifdef CONFIG_DEBUG_FS
static int codec_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, u32 *param1, int num_of_par)
{
	char *token = NULL;
	int base = 0, cnt = 0;

	token = strsep(&buf, " ");
	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtou32(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else {
			return -EINVAL;
		}
	}
	return 0;
}

static bool is_swr_slv_reg_readable(int reg)
{
	int ret = true;

	if (((reg > 0x46) && (reg < 0x4A)) ||
	    ((reg > 0x4A) && (reg < 0x50)) ||
	    ((reg > 0x55) && (reg < 0xD0)) ||
	    ((reg > 0xD0) && (reg < 0xE0)) ||
	    ((reg > 0xE0) && (reg < 0xF0)) ||
	    ((reg > 0xF0) && (reg < 0x100)) ||
	    ((reg > 0x105) && (reg < 0x120)) ||
	    ((reg > 0x205) && (reg < 0x220)) ||
	    ((reg > 0x305) && (reg < 0x320)) ||
	    ((reg > 0x405) && (reg < 0x420)) ||
	    ((reg > 0x128) && (reg < 0x130)) ||
	    ((reg > 0x228) && (reg < 0x230)) ||
	    ((reg > 0x328) && (reg < 0x330)) ||
	    ((reg > 0x428) && (reg < 0x430)) ||
	    ((reg > 0x138) && (reg < 0x205)) ||
	    ((reg > 0x238) && (reg < 0x305)) ||
	    ((reg > 0x338) && (reg < 0x405)) ||
	    ((reg > 0x405) && (reg < 0xF00)) ||
	    ((reg > 0xF05) && (reg < 0xF20)) ||
	    ((reg > 0xF25) && (reg < 0xF30)) ||
	    ((reg > 0xF35) && (reg < 0x2000)))
		ret = false;

	return ret;
}

static ssize_t wcd937x_swrslave_reg_show(struct swr_device *pdev,
					char __user *ubuf,
					size_t count, loff_t *ppos)
{
	int i, reg_val, len;
	ssize_t total = 0;
	char tmp_buf[SWR_SLV_MAX_BUF_LEN];

	if (!ubuf || !ppos)
		return 0;

	for (i = (((int) *ppos/BYTES_PER_LINE) + SWR_SLV_START_REG_ADDR);
		i <= SWR_SLV_MAX_REG_ADDR; i++) {
		if (!is_swr_slv_reg_readable(i))
			continue;
		swr_read(pdev, pdev->dev_num, i, &reg_val, 1);
		len = snprintf(tmp_buf, sizeof(tmp_buf), "0x%.3x: 0x%.2x\n", i,
			       (reg_val & 0xFF));
		if (((total + len) >= count - 1) || (len < 0))
			break;
		if (copy_to_user((ubuf + total), tmp_buf, len)) {
			pr_err("%s: fail to copy reg dump\n", __func__);
			total = -EFAULT;
			goto copy_err;
		}
		total += len;
		*ppos += len;
	}

copy_err:
	*ppos = SWR_SLV_MAX_REG_ADDR * BYTES_PER_LINE;
	return total;
}

static ssize_t codec_debug_dump(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	struct swr_device *pdev;

	if (!count || !file || !ppos || !ubuf)
		return -EINVAL;

	pdev = file->private_data;
	if (!pdev)
		return -EINVAL;

	if (*ppos < 0)
		return -EINVAL;

	return wcd937x_swrslave_reg_show(pdev, ubuf, count, ppos);
}

static ssize_t codec_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[SWR_SLV_RD_BUF_LEN];
	struct swr_device *pdev = NULL;
	struct wcd937x_slave_priv *wcd937x_slave = NULL;

	if (!count || !file || !ppos || !ubuf)
		return -EINVAL;

	pdev = file->private_data;
	if (!pdev)
		return -EINVAL;

	wcd937x_slave = swr_get_dev_data(pdev);
	if (!wcd937x_slave)
		return -EINVAL;

	if (*ppos < 0)
		return -EINVAL;

	snprintf(lbuf, sizeof(lbuf), "0x%x\n",
			(wcd937x_slave->read_data & 0xFF));

	return simple_read_from_buffer(ubuf, count, ppos, lbuf,
					       strnlen(lbuf, 7));
}

static ssize_t codec_debug_peek_write(struct file *file,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char lbuf[SWR_SLV_WR_BUF_LEN];
	int rc = 0;
	u32 param[5];
	struct swr_device *pdev = NULL;
	struct wcd937x_slave_priv *wcd937x_slave = NULL;

	if (!cnt || !file || !ppos || !ubuf)
		return -EINVAL;

	pdev = file->private_data;
	if (!pdev)
		return -EINVAL;

	wcd937x_slave = swr_get_dev_data(pdev);
	if (!wcd937x_slave)
		return -EINVAL;

	if (*ppos < 0)
		return -EINVAL;

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';
	rc = get_parameters(lbuf, param, 1);
	if (!((param[0] <= SWR_SLV_MAX_REG_ADDR) && (rc == 0)))
		return -EINVAL;
	swr_read(pdev, pdev->dev_num, param[0], &wcd937x_slave->read_data, 1);
	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static ssize_t codec_debug_write(struct file *file,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char lbuf[SWR_SLV_WR_BUF_LEN];
	int rc = 0;
	u32 param[5];
	struct swr_device *pdev;

	if (!file || !ppos || !ubuf)
		return -EINVAL;

	pdev = file->private_data;
	if (!pdev)
		return -EINVAL;

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';
	rc = get_parameters(lbuf, param, 2);
	if (!((param[0] <= SWR_SLV_MAX_REG_ADDR) &&
		(param[1] <= 0xFF) && (rc == 0)))
		return -EINVAL;
	swr_write(pdev, pdev->dev_num, param[0], &param[1]);
	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations codec_debug_write_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
};

static const struct file_operations codec_debug_read_ops = {
	.open = codec_debug_open,
	.read = codec_debug_read,
	.write = codec_debug_peek_write,
};

static const struct file_operations codec_debug_dump_ops = {
	.open = codec_debug_open,
	.read = codec_debug_dump,
};
#endif

static int wcd937x_slave_bind(struct device *dev,
				struct device *master, void *data)
{
	int ret = 0;
	struct wcd937x_slave_priv *wcd937x_slave = NULL;
	uint8_t devnum = 0;
	struct swr_device *pdev = to_swr_device(dev);

	if (pdev == NULL) {
		dev_err(dev, "%s: pdev is NULL\n", __func__);
		return -EINVAL;
	}

	wcd937x_slave = devm_kzalloc(&pdev->dev,
				sizeof(struct wcd937x_slave_priv), GFP_KERNEL);
	if (!wcd937x_slave)
		return -ENOMEM;

	swr_set_dev_data(pdev, wcd937x_slave);

	wcd937x_slave->swr_slave = pdev;

#ifdef CONFIG_DEBUG_FS
	if (!wcd937x_slave->debugfs_wcd937x_dent) {
		wcd937x_slave->debugfs_wcd937x_dent = debugfs_create_dir(
						dev_name(&pdev->dev), 0);
		if (!IS_ERR(wcd937x_slave->debugfs_wcd937x_dent)) {
			wcd937x_slave->debugfs_peek =
					debugfs_create_file("swrslave_peek",
					S_IFREG | 0444,
					wcd937x_slave->debugfs_wcd937x_dent,
					(void *) pdev,
					&codec_debug_read_ops);

			wcd937x_slave->debugfs_poke =
					debugfs_create_file("swrslave_poke",
					S_IFREG | 0444,
					wcd937x_slave->debugfs_wcd937x_dent,
					(void *) pdev,
					&codec_debug_write_ops);

			wcd937x_slave->debugfs_reg_dump =
					debugfs_create_file(
					"swrslave_reg_dump",
					S_IFREG | 0444,
					wcd937x_slave->debugfs_wcd937x_dent,
					(void *) pdev,
					&codec_debug_dump_ops);
                }
        }
#endif

	ret = swr_get_logical_dev_num(pdev, pdev->addr, &devnum);
	if (ret) {
		dev_dbg(&pdev->dev,
				"%s get devnum %d for dev addr %llx failed\n",
				__func__, devnum, pdev->addr);
		swr_remove_device(pdev);
		return ret;
	}
	pdev->dev_num = devnum;

	return ret;
}

static void wcd937x_slave_unbind(struct device *dev,
				struct device *master, void *data)
{
	struct wcd937x_slave_priv *wcd937x_slave = NULL;
	struct swr_device *pdev = to_swr_device(dev);

	if (pdev == NULL) {
		dev_err(dev, "%s: pdev is NULL\n", __func__);
		return;
	}

	wcd937x_slave = swr_get_dev_data(pdev);
	if (!wcd937x_slave) {
		dev_err(&pdev->dev, "%s: wcd937x_slave is NULL\n", __func__);
		return;
	}

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(wcd937x_slave->debugfs_wcd937x_dent);
	wcd937x_slave->debugfs_wcd937x_dent = NULL;
#endif

	swr_set_dev_data(pdev, NULL);
}

static const struct swr_device_id wcd937x_swr_id[] = {
	{"wcd937x-slave", 0},
	{}
};

static const struct of_device_id wcd937x_swr_dt_match[] = {
	{
		.compatible = "qcom,wcd937x-slave",
	},
	{}
};

static const struct component_ops wcd937x_slave_comp_ops = {
	.bind   = wcd937x_slave_bind,
	.unbind = wcd937x_slave_unbind,
};

static int wcd937x_swr_up(struct swr_device *pdev)
{
	return 0;
}

static int wcd937x_swr_down(struct swr_device *pdev)
{
	return 0;
}

static int wcd937x_swr_reset(struct swr_device *pdev)
{
	return 0;
}

static int wcd937x_swr_probe(struct swr_device *pdev)
{
	return component_add(&pdev->dev, &wcd937x_slave_comp_ops);
}

static int wcd937x_swr_remove(struct swr_device *pdev)
{
	component_del(&pdev->dev, &wcd937x_slave_comp_ops);
	return 0;
}

static struct swr_driver wcd937x_slave_driver = {
	.driver = {
		.name = "wcd937x-slave",
		.owner = THIS_MODULE,
		.of_match_table = wcd937x_swr_dt_match,
	},
	.probe = wcd937x_swr_probe,
	.remove = wcd937x_swr_remove,
	.id_table = wcd937x_swr_id,
	.device_up = wcd937x_swr_up,
	.device_down = wcd937x_swr_down,
	.reset_device = wcd937x_swr_reset,
};

static int __init wcd937x_slave_init(void)
{
	return swr_driver_register(&wcd937x_slave_driver);
}

static void __exit wcd937x_slave_exit(void)
{
	swr_driver_unregister(&wcd937x_slave_driver);
}

module_init(wcd937x_slave_init);
module_exit(wcd937x_slave_exit);

MODULE_DESCRIPTION("WCD937X Swr Slave driver");
MODULE_LICENSE("GPL v2");
