// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2017, STMicroelectronics - All Rights Reserved
 *
 * This file is part "VD6281 API" and is dual licensed, either 'STMicroelectronics Proprietary license'
 * or 'BSD 3-clause "New" or "Revised" License' , at your option.
 *
 ********************************************************************************
 *
 * 'STMicroelectronics Proprietary license'
 *
 ********************************************************************************
 *
 * License terms STMicroelectronics Proprietary in accordance with licensing terms at www.st.com/sla0044
 *
 * STMicroelectronics confidential
 * Reproduction and Communication of this document is strictly prohibited unless
 * specifically authorized in writing by STMicroelectronics.
 *
 *
 ********************************************************************************
 *
 * Alternatively, "VD6281 API" may be distributed under the terms of
 * 'BSD 3-clause "New" or "Revised" License', in which case the following provisions apply instead of the ones
 * mentioned above
 *
 ********************************************************************************
 *
 * License terms BSD 3-clause "New" or "Revised" License.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 ********************************************************************************
 *
 */

#include <linux/atomic.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

#include "vd6281_adapter_ioctl.h"

#define VD6281_ADAPTER_DEV_NAME "vd6281_adapter"

struct vd6281_adapter {
	struct device *dev;
	struct i2c_client *i2c;
	struct i2c_adapter *adapter;
	unsigned short addr;
	struct miscdevice misc;
	struct mutex i2c_mutex;
	atomic_t in_use;
	struct regulator *vdd;
};

struct vd6281_spidev_data {
	struct spi_device *pdev;
	struct mutex spi_mutex;
	struct miscdevice misc;
	u8 *pbuffer;
	int16_t *psamples;
	u32 spi_max_frequency;
	u32 spi_buffer_size;
	u32 spi_speed_hz;
	u16 samples_nb_per_chunk;
	u16 pdm_data_sample_width_in_bytes;
	// CIC filter internal registers
	// max CIC stage is 4
	// integrate registers
	int32_t integrate_register[MAX_CIC_STAGE];
	// difference registers
	int32_t difference_register[MAX_CIC_STAGE];
	// CIC compensation filter registers
	int32_t compensation_register[2];
};

#ifdef CONFIG_PM_SLEEP
static int vd6281_suspend(struct device *dev)
{
	struct vd6281_adapter *adp = dev_get_drvdata(dev);
	int ret = 0;

	if (adp->vdd) {
		ret = regulator_disable(adp->vdd);
		if (ret < 0)
			dev_err(dev, "vdd regulator disable failed: %d\n", ret);
	}

	return ret;
}

static int vd6281_resume(struct device *dev)
{
	struct vd6281_adapter *adp = dev_get_drvdata(dev);
	int ret = 0;

	if (adp->vdd) {
		ret = regulator_enable(adp->vdd);
		if (ret < 0) {
			dev_err(dev, "vdd regulator enable failed: %d\n", ret);
			return ret;
		}
	}

	return 0;
}
#endif

static const struct dev_pm_ops vd6281_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(vd6281_suspend, vd6281_resume)
};

// legacy chunk transfer function. To be used by user part as a backup
static ssize_t vd6281_spi_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t status = 0;
	unsigned long missing;
	struct vd6281_spidev_data	*pdata = container_of(file->private_data,
		struct vd6281_spidev_data, misc);

	struct spi_transfer t = {
			.rx_buf = pdata->pbuffer,
			.len		= count,
		};
	struct spi_message m;

	if (count > pdata->spi_buffer_size)
		return -EMSGSIZE;

	// set the speed set by user, or the max_frequency one if not set by user
	t.speed_hz = pdata->spi_speed_hz;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(pdata->pdev, &m);
	if (status != 0) {
		pr_err("[%ld] spi read failed\n", status);
		return status;
	}

	status = m.actual_length;
	missing = copy_to_user(buf, pdata->pbuffer, status);
	if (missing == status)
		status = -EFAULT;
	else
		status = status - missing;

	return status;
}


static int vd6281_spi_open(struct inode *inode, struct file *file)
{
	int i;
	struct vd6281_spidev_data *pdata = container_of(file->private_data,
		struct vd6281_spidev_data, misc);

	if (!pdata->pbuffer) {
		if (pdata->spi_buffer_size != 0) {
			pdata->pbuffer = kmalloc(pdata->spi_buffer_size, GFP_KERNEL);
			if (!pdata->pbuffer)
				return -ENOMEM;
		} else {
			return -EFAULT;
		}
	}

	//reset the internal status
	for (i = 0; i < MAX_CIC_STAGE; i++) {
		pdata->difference_register[i] = 0;
		pdata->integrate_register[i] = 0;
	}
	pdata->compensation_register[0] = 0;
	pdata->compensation_register[1] = 0;

	return 0;
}

//Q15 format. 15 bits for decimal part.
//in should be in [0, 1)
static inline int16_t convert_to_Q15_format(float in)
{
  return (int16_t)((float)(1<<15) * in);
}

/*
structure of a CIC filter
The folowing shows a M = 3 stage, CIC filter with the downsample rate D.
z^-1 is a delay block
                                                                  ----------------
------> + ----------------> + ---------------> + --------------->| downsample D |  ---------------> + -----------------> + -----------------> + ---->
        ^           |       ^           |      ^           |      ----------------    |             ^ (-)  |             ^ (-)  |             ^ (-)
    |           |       |           |      |           |                          |             |      |             |      |             |
    |<--|z^-1|<-|       |<--|z^-1|<-|      |<--|z^-1|<-|                          |-->|z^-1|--->|      |-->|z^-1|--->|      |-->|z^-1|--->|
        integration registers                                                        decimation registers
ref:
https://www.dsprelated.com/showarticle/1337.php
*/

static inline int update_integration_registers(int32_t* p_integrate_register, int number_of_stages, int32_t input)
{
  // partial sum of the registers
  int sum_integrate_buffer = 0;
  int j;
  int sum_data = 0;

  // update the CIC integration registers
  for (j = 0; j < number_of_stages; j++) {
    // partial sum, sum of the registers up to the curent register
    sum_integrate_buffer += p_integrate_register[j];
    // update the curent register with partial sum and new input counts.
    p_integrate_register[j] = sum_integrate_buffer + input;
  }
  // sum of all registers plus the new input counts.
  sum_data = sum_integrate_buffer + input;
  return sum_data;
}

static inline void update_difference_registers(int32_t* p_difference_register, int number_of_stages, int32_t sum_data)
{
  int j;
  int cumsum_diff = 0;
  int tmp;
  for (j = 0; j < number_of_stages; j++) {
    // make a copy of the current register
    tmp = p_difference_register[j];
    // update the current register
    p_difference_register[j] = sum_data - cumsum_diff;
    cumsum_diff += tmp;
  }
}

// perform
// 1. sum of 8 bits
// 2. multi-stage CIC with downsample
// 3. compensation filter
static void downsample8_cic_compensae(
  int downsample_ratio,
  int input_PDM_size,
  uint8_t* p_input,
  int16_t* p_output,
  int32_t* p_integrate_register,
  int32_t* p_difference_register,
  int32_t* p_compensation_register,
  int32_t number_of_stages)
{
  int32_t internal_ds;
  int32_t gain = 1;
  int32_t i, j, s;
  int32_t bit_shift;
  int32_t count1s;
  int32_t sum_data, sum_diff;
  int32_t cic_output;
  int64_t comp_filter_output;
  int32_t compensate_gain;
  int16_t inverse_gain_Q15;

  // internal downsampling ratio = downsample_ratio / 8
  internal_ds = downsample_ratio >> 3;

  // CIC adds gains to the output data.
  // gain = D^M, M is the number of stages, D is the downsampling ratio
  for (i = 0; i < number_of_stages; i++) {
    gain *= internal_ds;
  }

  // bits to shift the output to keep a unit gain
  bit_shift = sizeof(int32_t) * 8 - 1 - __builtin_clz(gain);
  bit_shift = gain > 0 ? bit_shift : 0;

  // if we shift right the CIC output with bit_shift
  // we should get a unit gain on average
  // The input to the CIC is a byte, so we can have at most 8 1's per byte.
  // 4bit is enough for 8 1's.
  // So we have up to 11 bits (15-4) to shift for float to int16.
  bit_shift = bit_shift - 11;
    if (bit_shift < 0) {
      bit_shift = 0;
  }

  // coefficients for the high-pass 2 order, FIR compensation filter
  if (number_of_stages == 3) {
    compensate_gain = -10;
  } else {
    compensate_gain = -6;
  }

  // max gain for the compensation filter is 2 + abs(compensate_gain).
  // need to remove this gain from the compensation filter output.
  // to keep unit gain on average
  // convert the inverse of the compensation filter gain to Q15 format.
  inverse_gain_Q15 = convert_to_Q15_format(1.0f / (2.0f - compensate_gain));

  s = 0;
  for (i = 0; i < input_PDM_size; i++) {
    // count number of 1s in a byte (downsample by 8)
    count1s = __builtin_popcount(p_input[i]);
    sum_data = update_integration_registers(p_integrate_register, number_of_stages, count1s);
    // downsample stage
    if ((i + 1) % internal_ds == 0) {
      sum_diff = 0;
      // sum of difference registers
      for (j = 0; j < number_of_stages; j++) {
        sum_diff += p_difference_register[j];
      }
      update_difference_registers(p_difference_register, number_of_stages, sum_data);

      // CIC output
      cic_output = sum_data - sum_diff;

      // apply compensation filter, a second orfer FIR filter
      // y(n) = x(n) + compensate_gain * x(n-1) + x(n-2)
      comp_filter_output = (int64_t)cic_output + (int64_t)p_compensation_register[0] * (int64_t)compensate_gain + (int64_t)p_compensation_register[1];

      //update compensation register
      p_compensation_register[0] = cic_output;
      p_compensation_register[1] = p_compensation_register[0];

      // normalize the output to unit gain and then shift bits
      // for float to int16
	  // instead of shift 15bits for Q15 , we need to shift bit_shift more bits
      p_output[s] = -1 * (int16_t)((comp_filter_output * inverse_gain_Q15) >> (bit_shift + 15));
      s++;
    }
  }

}

static int vd6281_spi_chunk_transfer_and_get_samples(struct vd6281_spidev_data *pdata)
{

	ssize_t status = 0;
	int input_PDM_size;

	struct spi_transfer t = {
			.rx_buf = pdata->pbuffer,
			.len	= pdata->spi_buffer_size,
		};
	struct spi_message m;

	// set the speed set by user, or the max_frequency one if not set by user
	t.speed_hz = pdata->spi_speed_hz;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(pdata->pdev, &m);
	if (status != 0) {
		pr_err("[%ld] spi read failed\n", status);
		return status;
	}

	if (m.actual_length != pdata->spi_buffer_size) {
		pr_info("vd6281. spi transfer error");
		return -EFAULT;
	}

	input_PDM_size = pdata->samples_nb_per_chunk * pdata->pdm_data_sample_width_in_bytes;
	downsample8_cic_compensae(
		pdata->pdm_data_sample_width_in_bytes * 8,
		input_PDM_size,
		pdata->pbuffer,
		pdata->psamples,
		pdata->integrate_register,
		pdata->difference_register,
		pdata->compensation_register,
		MAX_CIC_STAGE);
	return 0;
}

static int vd6281_spi_ioctl_handler(struct vd6281_spidev_data *pdata, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct vd6281_spi_info spi_info;
	struct vd6281_spi_params spi_params;

	if (!pdata)
		return -EINVAL;

	mutex_lock(&pdata->spi_mutex);
	switch (cmd) {
	case VD6281_IOCTL_GET_SPI_INFO:
		spi_info.chunk_size = pdata->spi_buffer_size;
		spi_info.spi_max_frequency = pdata->spi_max_frequency;
		ret = copy_to_user((void __user *) arg, &spi_info, sizeof(struct vd6281_spi_info));
		break;

	case VD6281_IOCTL_SET_SPI_PARAMS:
		ret = copy_from_user(&spi_params, (void __user *)arg, sizeof(struct vd6281_spi_params));
		if (ret != 0)
			break;
		if ((!spi_params.speed_hz) || (!spi_params.samples_nb_per_chunk) || (!spi_params.pdm_data_sample_width_in_bytes)) {
			ret = -EINVAL;
			break;
		}
		pdata->spi_speed_hz = spi_params.speed_hz;
		pdata->samples_nb_per_chunk = spi_params.samples_nb_per_chunk;
		pdata->pdm_data_sample_width_in_bytes = spi_params.pdm_data_sample_width_in_bytes;

		kfree(pdata->psamples);
		pdata->psamples = kmalloc_array(pdata->samples_nb_per_chunk, sizeof(int16_t), GFP_KERNEL);
		pr_info("vd6281 : spi speed set : %d", pdata->spi_speed_hz);
		pr_info("vd6281 : nb of samples per chunk  : %d", pdata->samples_nb_per_chunk);
		pr_info("vd6281 : sample width in bytes: %d", pdata->pdm_data_sample_width_in_bytes);
		break;

	case VD6281_IOCTL_GET_CHUNK_SAMPLES:
		ret = vd6281_spi_chunk_transfer_and_get_samples(pdata);
		if (ret != 0)
			break;
		ret = copy_to_user((void __user *) arg, pdata->psamples, pdata->samples_nb_per_chunk * sizeof(int16_t));
		break;

	default:
		ret = -EINVAL;
	}
	mutex_unlock(&pdata->spi_mutex);

	return ret;
}

static long vd6281_spi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	struct vd6281_spidev_data *pdata = container_of(file->private_data,
		struct vd6281_spidev_data, misc);

return vd6281_spi_ioctl_handler(pdata, cmd, arg);
}

static int vd6281_spi_release(struct inode *inode, struct file *file)
{
	struct vd6281_spidev_data *pdata = container_of(file->private_data,
		struct vd6281_spidev_data, misc);

	kfree(pdata->pbuffer);
	pdata->pbuffer = NULL;

	kfree(pdata->psamples);
	pdata->psamples = NULL;

	return 0;
}

static int vd6281_spi_parse_dt(struct vd6281_spidev_data *pdata)
{
	int ret = 0;
	struct device_node *of_node = pdata->pdev->dev.of_node;

	ret = of_property_read_u32(of_node, "spi-max-frequency", &pdata->spi_max_frequency);
	if (ret) {
		pr_err("[%s] failed to read spi-frequency", __func__);
		return ret;
	}

	// set the spi speed to max frequency by default
	pdata->spi_speed_hz = pdata->spi_max_frequency;

	pr_info("vd6281 : [%s] spi (max) frequency=%d", __func__, pdata->spi_max_frequency);

	ret = of_property_read_u32(of_node, "chunk-size", &pdata->spi_buffer_size);
	if (ret) {
		pr_err("[%s] failed to read spi-frequency", __func__);
		return ret;
	}

	// set the spi speed to max frequency by default
	pr_info("vd6281 : [%s] spi chunk size=%d", __func__, pdata->spi_buffer_size);

	return 0;
}

static const struct file_operations vd6281_spi_fops = {
	.owner		= THIS_MODULE,
	.open		= vd6281_spi_open,
	.release	= vd6281_spi_release,
	.read		= vd6281_spi_read,
	.unlocked_ioctl	= vd6281_spi_ioctl
};

int vd6281_spi_driver_probe(struct spi_device *pdev)
{
	int ret = 0;
	struct vd6281_spidev_data *pdata;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->pdev = pdev;
	mutex_init(&pdata->spi_mutex);
	spi_set_drvdata(pdev, pdata);

	vd6281_spi_parse_dt(pdata);

	//pr_info("[%s] spi mode=%d, cs=%d, bits_per_word=%d, speed=%d, csgpio=%d, modalias=%s",
	//	___func__, pdev->mode, pdev->chip_select, pdev->bits_per_word,
	//	pdev->max_speed_hz, pdev->cs_gpio, pdev->modalias);

	pdata->misc.minor = MISC_DYNAMIC_MINOR;
	pdata->misc.name = "vd6281_spi";
	pdata->misc.fops = &vd6281_spi_fops;
	ret = misc_register(&pdata->misc);

	if (ret)
		pr_info("vd6281_spi_probe failed");
	else
		pr_info("vd6281_spi_probe successfully");

	return ret;
}


int vd6281_spi_driver_remove(struct spi_device *pdev)
{
	struct vd6281_spidev_data *pdata;

	pdata = spi_get_drvdata(pdev);
	if (!pdata) {
		pr_err("[%s] can't remove %p", __func__, pdev);
		return 0;
	}

	misc_deregister(&pdata->misc);

	return 0;
}

static int vd6281_read_reg8(struct vd6281_adapter *adp, void __user *p)
{
	struct vd6281_reg reg;
	int ret;
	struct i2c_msg msg;
	uint8_t data;

	if (copy_from_user(&reg, p, sizeof(struct vd6281_reg)))
		return -EFAULT;

	data = reg.index;

	msg.addr = adp->addr;
	/* write the reg first */
	msg.flags = I2C_M_STOP | I2C_M_DMA_SAFE;
	msg.len = 1;
	msg.buf = &data;

	ret = i2c_transfer(adp->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(adp->dev, "Failed to write register %x with error %d", reg.index, ret);
		return ret;
	}

	/* then read value */
	msg.flags = I2C_M_RD | I2C_M_STOP | I2C_M_DMA_SAFE;

	ret = i2c_transfer(adp->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(adp->dev, "Failed to read register %x with error %d", reg.index, ret);
		return ret;
	}

	reg.data = data;
	return copy_to_user(p, &reg, sizeof(struct vd6281_reg));
}

static int vd6281_write_reg8(struct vd6281_adapter *adp, void __user *p)
{
	struct i2c_msg msg;
	uint8_t write_buf[2];
	struct vd6281_reg reg;
	int ret;

	if (copy_from_user(&reg, p, sizeof(struct vd6281_reg)))
		return -EFAULT;

	write_buf[0] = reg.index;
	write_buf[1] = reg.data;

	msg.addr = adp->addr;
	msg.flags = I2C_M_STOP | I2C_M_DMA_SAFE;
	msg.len = 2;
	msg.buf = write_buf;

	ret = i2c_transfer(adp->adapter, &msg, 1);

	// QUP/I2C case, ret in the number of messages sucessfully sent
	// CCI : 0 is returned if camera_io_dev_read is successfull
	if (ret < 0) {
		dev_err(adp->dev, "Failed to write register %x with error %d", reg.index, ret);
		return ret;
	}

	return 0;
}

static int vd6281_read_reg8_multi(struct vd6281_adapter *adp, void __user *p)
{
	struct vd6281_read_multi_regs reg;
	int ret;
	struct i2c_msg msg;
	uint8_t index;

	if (copy_from_user(&reg, p, sizeof(struct vd6281_read_multi_regs)))
		return -EFAULT;

	if (reg.len > VD6281_MULTI_REG_RD_MAX)
		return -EMSGSIZE;

	index = reg.index;

	/* write the reg first */
	msg.addr = adp->addr;
	msg.flags = I2C_M_STOP | I2C_M_DMA_SAFE;
	msg.len = 1;
	msg.buf = &index;

	ret = i2c_transfer(adp->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(adp->dev, "Failed to write register %x with error %d", reg.index, ret);
		return ret;
	}

	msg.addr = adp->addr;
	msg.flags = I2C_M_RD | I2C_M_STOP | I2C_M_DMA_SAFE;
	msg.buf = reg.data;
	msg.len = reg.len;

	ret = i2c_transfer(adp->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(adp->dev, "Failed to read register %x with error %d", reg.index, ret);
		return ret;
	}

	return copy_to_user(p, &reg, sizeof(struct vd6281_read_multi_regs));
}

static int vd6281_ioctl_handler(struct vd6281_adapter *adp, unsigned int cmd,
	unsigned long arg)
{
	int ret;

	if (!adp)
		return -EINVAL;

	mutex_lock(&adp->i2c_mutex);
	switch (cmd) {
	case VD6281_IOCTL_REG_WR:
		ret = vd6281_write_reg8(adp, (void __user *) arg);
		break;

	case VD6281_IOCTL_REG_RD:
		ret = vd6281_read_reg8(adp, (void __user *) arg);
		break;

	case VD6281_IOCTL_REG_RD_MULTI:
		ret = vd6281_read_reg8_multi(adp, (void __user *) arg);
		break;

	default:
		ret = -EINVAL;
	}
	mutex_unlock(&adp->i2c_mutex);

	return ret;
}

static int vd6281_open(struct inode *inode, struct file *file)
{
	struct vd6281_adapter *adp = container_of(file->private_data,
		struct vd6281_adapter, misc);

	if (atomic_cmpxchg(&adp->in_use, 0, 1) != 0)
		return -EBUSY;

	pr_info("%s %p", __func__, adp);

	return 0;
}

static int vd6281_release(struct inode *inode, struct file *file)
{
	struct vd6281_adapter *adp = container_of(file->private_data,
		struct vd6281_adapter, misc);

	atomic_set(&adp->in_use, 0);
	pr_info("%s %p", __func__, adp);

	return 0;
}

static long vd6281_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct vd6281_adapter *adp = container_of(file->private_data,
		struct vd6281_adapter, misc);

	return vd6281_ioctl_handler(adp, cmd, arg);
}

static const struct file_operations vd6281_fops = {
	.owner		= THIS_MODULE,
	.open		= vd6281_open,
	.release	= vd6281_release,
	.unlocked_ioctl	= vd6281_ioctl,
};

static int vd6281_adapter_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret;
	struct vd6281_adapter *adp;

	pr_info("vd6281_adapter_i2c_probe");
	adp = devm_kzalloc(&client->dev, sizeof(*adp), GFP_KERNEL);
	if (!adp)
		return -ENOMEM;

	adp->i2c = client;
	adp->dev = &client->dev;
	adp->adapter = client->adapter;
	adp->addr = client->addr;
	mutex_init(&adp->i2c_mutex);

	adp->vdd = devm_regulator_get_optional(&client->dev, "vdd");
	if (IS_ERR(adp->vdd)) {
		ret = PTR_ERR(adp->vdd);
		dev_err(&client->dev, "Unable to get vdd regulator: %d\n", ret);
		return ret;
	}
	if (adp->vdd) {
		ret = regulator_enable(adp->vdd);
		if (ret < 0) {
			dev_err(&client->dev, "vdd enable failed: %d\n", ret);
			return ret;
		}
	}

	i2c_set_clientdata(client, adp);

	adp->misc.minor = MISC_DYNAMIC_MINOR;
	adp->misc.name = "vd6281";
	adp->misc.fops = &vd6281_fops;
	ret = misc_register(&adp->misc);

	if (ret)
		pr_info("%s failed", __func__);
	else
		pr_info("%s successfully", __func__);

	return ret;
}

static int vd6281_adapter_i2c_remove(struct i2c_client *client)
{
	struct vd6281_adapter *adp;

	adp = i2c_get_clientdata(client);

	misc_deregister(&adp->misc);

	if (adp->vdd) {
		int ret = regulator_disable(adp->vdd);

		if (ret < 0) {
			dev_err(&client->dev, "vdd regulator disable failed: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static const struct of_device_id vd6281_adapter_dt_ids[] = {
	{ .compatible = "st,vd6281_adapter" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, vd6281_adapter_dt_ids);

static const struct i2c_device_id vd6281_i2c_id[] = {
	{"vd6281_adapter", 0},
	{},
};

static struct i2c_driver vd6281_adapter_i2c_driver = {
	.driver = {
		.name  = "vd6281_adapter",
		.owner = THIS_MODULE,
		.of_match_table = vd6281_adapter_dt_ids,
		.pm = &vd6281_pm,
	},
	.probe = vd6281_adapter_i2c_probe,
	.remove = vd6281_adapter_i2c_remove,
	.id_table = vd6281_i2c_id,
};

static const struct of_device_id vd6281_spi_dt_ids[] = {
	{ .compatible = "st,vd6281_spi" },
	{ /* sentinel */ }
};

static struct spi_driver vd6281_spi_driver = {
	.driver = {
		.name = "vd6281_spi",
		.owner = THIS_MODULE,
		.of_match_table = vd6281_spi_dt_ids,
	},
	.probe = vd6281_spi_driver_probe,
	.remove = vd6281_spi_driver_remove,
};

MODULE_DEVICE_TABLE(of, vd6281_spi_dt_ids);

static int __init vd6281_module_init(void)
{
	int ret = 0;

	pr_info("vd6281: module init\n");

	/* register as a i2c client device */
	ret = i2c_add_driver(&vd6281_adapter_i2c_driver);
	if (ret) {
		i2c_del_driver(&vd6281_adapter_i2c_driver);
		pr_info("vd6281: could not add i2c driver\n");
		return ret;
	}

	ret = spi_register_driver(&vd6281_spi_driver);
	if (ret < 0) {
		pr_err("spi_register_driver failed => %d", ret);
		return ret;
	}

	return ret;
}

static void __exit vd6281_module_exit(void)
{

	pr_debug("vd6281 : module exit\n");

	spi_unregister_driver(&vd6281_spi_driver);
	i2c_del_driver(&vd6281_adapter_i2c_driver);
}

module_init(vd6281_module_init);
module_exit(vd6281_module_exit);

MODULE_AUTHOR("Philippe LEGEARD <philippe.legeard@st.com>");
MODULE_DESCRIPTION("vd6281 adapter driver");
MODULE_LICENSE("GPL v2");
