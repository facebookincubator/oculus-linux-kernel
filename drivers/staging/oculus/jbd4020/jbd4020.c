/* SPDX-License-Identifier: GPL+ */
#include <drm/drm_bridge.h>

#include <linux/component.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/thermal.h>
#include <linux/fs.h>

#include "jbd4020.h"

/* temperature = (temp_out[9:0] - 118) / 815 * 165 - 40
 */
#define TEMP_BYTES_TO_MCELSIUS(temp_bytes) 1000 * ((temp_bytes - 118) * 165 / 815 - 40)

#define JBD4020_I2C_MAX_RETRIES 6
#define JBD4020_I2C_DELAY_US 500
#define MAX_INT_DIGITS 21
#define JBD4020_RED_PANEL_ADDR 0x59
#define JBD4020_GREEN_PANEL_ADDR 0x5A
#define JBD4020_BLUE_PANEL_ADDR 0x5B
#define JBD4020_EXPECTED_DEMURA_SIZE 167088
#define JBD4020_I2C_BLOCK_SIZE 4096

struct jbd4020_panel_offset {
  u16 h;
  u16 v;
};

struct jbd4020_data {
  struct i2c_client* i2c;
  struct device* dev;
  struct i2c_adapter* adapter;
  unsigned short addr;
  struct regulator* pmic_regulator;
  bool is_enabled;
  struct thermal_zone_device* tz;
  struct jbd4020_panel_offset panel_offset[3];
  u8 is_demura_load_enabled;
  u8 is_demura_data_verification_enabled;
  u8 jbd4020_red_demura_table[JBD4020_EXPECTED_DEMURA_SIZE];
  u8 jbd4020_green_demura_table[JBD4020_EXPECTED_DEMURA_SIZE];
  u8 jbd4020_blue_demura_table[JBD4020_EXPECTED_DEMURA_SIZE];
  size_t received_red_demura_size;
  size_t received_green_demura_size;
  size_t received_blue_demura_size;
};

struct brightness_ctrl {
  u16 red;
  u16 green;
  u16 blue;
};

static struct jbd4020_panel_offset default_panel_offset[3] = {{8, 8}, {12, 8}, {8, 8}};

static struct drm_bridge bridge;
static struct jbd4020_data* cdata;

static const char DEVICE_NOT_AVAILABLE_STRING[] = "Device not available. \n";
static const char DEVICE_NOT_ENABLED_STRING[] = "Device not enabled. \n";
static const char PANELS_IO_STRING[] = "RED:%hu GREEN:%hu BLUE:%hu";
/* min possible string length with three integers (one digit) - remove nine chars of %hu,
                                                                null termination,
                                                                new line, and add three digits */
static const size_t PANELS_IO_STRING_MIN_SIZE = sizeof(PANELS_IO_STRING) - 6 - 1;
/* max possible string length with three integers - remove six chars of %u and three max int
 * digits*/
static const size_t PANELS_IO_STRING_MAX_SIZE =
    sizeof(PANELS_IO_STRING) - 9 - 1 + 3 * MAX_INT_DIGITS;

static const char TEMPERATURE_STRING[] = "RED:%u GREEN:%u BLUE:%u";
static const size_t TEMPERATURE_STRING_MAX_SIZE =
    sizeof(TEMPERATURE_STRING) - 6 - 1 + 3 * MAX_INT_DIGITS;

static const char PANEL_OFFSET_STRING[] = "%hu %hu %hu %hu %hu %hu";
static const size_t PANEL_OFFSET_STRING_MIN_SIZE = sizeof(PANEL_OFFSET_STRING) - 12 - 1;
static const size_t PANEL_OFFSET_STRING_MAX_SIZE =
    sizeof(PANEL_OFFSET_STRING) - 18 - 1 + 6 * MAX_INT_DIGITS;

static const u16 panel_addrs[] =
    {JBD4020_RED_PANEL_ADDR, JBD4020_GREEN_PANEL_ADDR, JBD4020_BLUE_PANEL_ADDR};

static const bool panel_mirrored[] = {false, true, false};
static const char *demura_red_bin_file = "/mnt/vendor/persist/calibration/demura_r.bin";
static const char *demura_green_bin_file = "/mnt/vendor/persist/calibration/demura_g.bin";
static const char *demura_blue_bin_file = "/mnt/vendor/persist/calibration/demura_b.bin";
static const char ENABLE_VERIFY_DEMURA_STRING[] = "%hhu\n";
static const size_t ENABLE_VERIFY_DEMURA_STRING_EXPECTED_SIZE = sizeof(ENABLE_VERIFY_DEMURA_STRING) - 4 - 1 + 1; // remove %hhu and null termination, add 1 digit (0/1)

static int
jbd4020_read_brightness_ctrl(struct brightness_ctrl * brightness_ctrl_val) {
  int ret;
  u8 brightness_data[2];
  int i;

  u16* colors[] = {
      &brightness_ctrl_val->red, &brightness_ctrl_val->green, &brightness_ctrl_val->blue};

  *brightness_ctrl_val = (struct brightness_ctrl){.red = 0, .green = 0, .blue = 0};

  for (i = 0; i < 3; i++) {
    ret = jbd4020_i2c_read(panel_addrs[i], REG_BRIGHTNESS_CONTROL, brightness_data, 2);
    if (ret < 0) {
      dev_err(
          cdata->dev,
          "Failed read register brightness_ctrl for panel:%x with error %d, code: %d",
          panel_addrs[i],
          ret,
          0);
      return ret;
    }
    *colors[i] = ((brightness_data[1] << 8) | brightness_data[0]) & 0x1FFF;
    dev_info(
        cdata->dev,
        "%s panel add: %x,  jbd4020_get_brightness_ctrl: %x %x %x",
        i == 0       ? "red"
            : i == 1 ? "green"
                     : "blue",
        panel_addrs[i],
        brightness_data[1],
        brightness_data[0],
        *colors[i]);
  }
  return 0;
}

static ssize_t brightness_ctrl_show(struct device* dev, struct device_attribute* attr, char* buf) {
  struct brightness_ctrl brightness_ctrl_val;
  int ret;

  if (!cdata) {
    return snprintf(buf, sizeof(DEVICE_NOT_AVAILABLE_STRING), DEVICE_NOT_AVAILABLE_STRING);
  }

  if (!cdata->is_enabled) {
    return snprintf(buf, sizeof(DEVICE_NOT_ENABLED_STRING), DEVICE_NOT_ENABLED_STRING);
  }

  ret = jbd4020_read_brightness_ctrl(&brightness_ctrl_val);
  if (ret < 0) {
    const char* errmsg = "Failed during jbd4020_read_brightness_ctrl";
    dev_err(cdata->dev, "%s", errmsg);
    return snprintf(buf, sizeof(buf), "%s", errmsg);
  }
  return snprintf(
      buf,
      PANELS_IO_STRING_MAX_SIZE,
      PANELS_IO_STRING,
      brightness_ctrl_val.red,
      brightness_ctrl_val.green,
      brightness_ctrl_val.blue);
}

static int jbd4020_write_brightness_ctrl(struct brightness_ctrl* brightness_ctrl_val) {
  int ret;
  u8 brightness_data[4] = {0};
  int i;

  u16* colors[] = {
      &brightness_ctrl_val->red, &brightness_ctrl_val->green, &brightness_ctrl_val->blue};

  for (i = 0; i < 3; i++) {
    ret = jbd4020_i2c_read(panel_addrs[i], REG_BRIGHTNESS_CONTROL, brightness_data, 2);
    if (ret < 0) {
      dev_err(
          cdata->dev,
          "Failed read register brightness_ctrl for panel:%x with error %d, code: %d",
          panel_addrs[i],
          ret,
          1);
      return ret;
    }
    brightness_data[0] = (*colors[i] & 0xFF);
    brightness_data[1] = ((brightness_data[1] & (~0x1F)) | ((*colors[i] >> 8) & 0x1F));
    ret = jbd4020_i2c_write(panel_addrs[i], REG_BRIGHTNESS_CONTROL, brightness_data, 4);
    if (ret < 0) {
      dev_err(
          cdata->dev,
          "Failed write register brightness_ctrl for for panel:%x with error %d",
          panel_addrs[i],
          ret);
      return ret;
    }
  }
  return ret;
}

static ssize_t brightness_ctrl_store(
    struct device* dev,
    struct device_attribute* attr,
    const char* buf,
    size_t count) {
  struct brightness_ctrl brightness_ctrl_val;
  int ret;

  if (!cdata)
    return 0;

  if (!cdata->is_enabled) {
    return 0;
  }

  if (count < PANELS_IO_STRING_MIN_SIZE || count > PANELS_IO_STRING_MAX_SIZE) {
    dev_err(cdata->dev, "Invalid buffer size for brightness_ctrl: %zu\n", count);
    return -EINVAL;
  }

  ret = sscanf(
      buf,
      PANELS_IO_STRING,
      &brightness_ctrl_val.red,
      &brightness_ctrl_val.green,
      &brightness_ctrl_val.blue);
  if (ret != 3) {
    dev_err(cdata->dev, "Failed to get brightness_ctrl value from string: %s\n", buf);
    return -EINVAL;
  }

  ret = jbd4020_write_brightness_ctrl(&brightness_ctrl_val);
  if (ret < 0) {
    dev_err(cdata->dev, "Failed during jbd4020_write_brightness_ctrl");
    return -EIO;
  }
  return count;
}

static struct device_attribute attr_brightness_ctrl =
    __ATTR(brightness_ctrl, 0660, brightness_ctrl_show, brightness_ctrl_store);

static ssize_t mipi_interface_store(
    struct device* dev,
    struct device_attribute* attr,
    const char* buf,
    size_t count) {
  int ret;
  u16 panel_mipi_interface[3] = {0};
  int i;
  u8 mipi_data[4]={0};

  if (!cdata)
    return 0;

  if (!cdata->is_enabled) {
    return 0;
  }

  if (count < PANELS_IO_STRING_MIN_SIZE || count > PANELS_IO_STRING_MAX_SIZE) {
    dev_err(cdata->dev, "Invalid buffer size for mipi_interface: %zu\n", count);
    return -EINVAL;
  }

  ret = sscanf(
      buf,
      PANELS_IO_STRING,
      &panel_mipi_interface[0], //red
      &panel_mipi_interface[1], //green
      &panel_mipi_interface[2]); //blue
  if (ret != 3) {
    dev_err(cdata->dev, "Failed to get mipi interface value from string: %s\n", buf);
    return -EINVAL;
  }
  for (i = 0; i < 3; i++) {
    // check valid values for al panels mipi interface 0(turn off) or 1(turn on)
    if(panel_mipi_interface[i] > 1) {
      dev_err(cdata->dev, "Invalid mipi interface value from string, excepted value is either 0(turn off) or 1(turn on)): %s\n", buf);
      return -EINVAL;
    }
  }

  for(i = 0; i < 3; i++) {
	ret = jbd4020_i2c_read(panel_addrs[i], REG_MIPI_INTERFACE, mipi_data, 4);
	if (ret < 0) {
		dev_err(
			cdata->dev,
			"Failed read register mipi_interface for panel:%x with error %d, code: %d",
			panel_addrs[i],
			ret,
			0);
		return ret;
	}

    if(panel_mipi_interface[i] == 0) {
	    mipi_data[0] = (mipi_data[0] & ~0x80);
      mipi_data[1] = (mipi_data[1] & ~0x0F);
      ret = jbd4020_i2c_write(panel_addrs[i], REG_MIPI_INTERFACE, mipi_data, 4);
    } else {
      mipi_data[0] = (mipi_data[0] | 0x80);
      mipi_data[1] = (mipi_data[1] | 0x0F);
      ret = jbd4020_i2c_write(panel_addrs[i], REG_MIPI_INTERFACE, mipi_data, 4);
    }
    if (ret < 0) {
      dev_err(
          cdata->dev,
          "Failed write register mipi_configuration for for panel:%x with error %d",
          panel_addrs[i],
          ret);
      return ret;
    }
  }
  return count;
}


static struct device_attribute attr_mipi_interface =
    __ATTR(mipi_interface, 0220, NULL, mipi_interface_store);

static int get_demura_data(const char *filename, int panel_index)
{
    struct file *filp;
    loff_t pos = 0;
    ssize_t ret;
    size_t total = 0;
    size_t *demura_received_size[3] = {&cdata->received_red_demura_size, &cdata->received_green_demura_size, &cdata->received_blue_demura_size};
    u8 *demura_table[3] = {cdata->jbd4020_red_demura_table, cdata->jbd4020_green_demura_table, cdata->jbd4020_blue_demura_table};
    ktime_t start_time, end_time;
    s64 elapsed_time_us;
    start_time = ktime_get();
    filp = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        dev_err(cdata->dev, "demura: Failed to open file %s, error: %ld\n", filename, PTR_ERR(filp));
        return PTR_ERR(filp);
    }
    while (total < JBD4020_EXPECTED_DEMURA_SIZE) {
        ret = kernel_read(filp, demura_table[panel_index] + total, JBD4020_EXPECTED_DEMURA_SIZE - total, &pos);
        if (ret < 0) {
            dev_err(cdata->dev, "demura: Error reading file at offset %zu, error: %zd\n", total, ret);
            filp_close(filp, NULL);
            return ret;
        }
        if (ret == 0) {
            dev_info(cdata->dev, "demura: Reached EOF at offset %zu\n", total);
            break;
        }
        dev_info(cdata->dev, "demura: Read %zd bytes at offset %zu\n", ret, total);
        total += ret;
    }
    filp_close(filp, NULL);
    dev_info(cdata->dev, "demura: File closed: %s\n", filename);
    *demura_received_size[panel_index] = total;
    if (total != JBD4020_EXPECTED_DEMURA_SIZE) {
      dev_err(cdata->dev, "demura: Data size mismatch! Expected %d, got %zu\n", JBD4020_EXPECTED_DEMURA_SIZE, *demura_received_size[panel_index]);
      return -EIO;  
    }
    dev_info(cdata->dev, "demura: Data loaded successfully. Total bytes read: %zu (expected: %d)\n", *demura_received_size[panel_index], JBD4020_EXPECTED_DEMURA_SIZE);
    end_time = ktime_get();
    elapsed_time_us = ktime_to_us(ktime_sub(end_time, start_time));
    dev_info(cdata->dev, "get_demura_data execution time: %lld us (%lld ms) for panel %d\n", 
            elapsed_time_us, elapsed_time_us / 1000, panel_index);
    return 0;
}

// Reads back a block and compares it to the expected buffer.
static void jbd4020_verify_demura_block(
    unsigned short panel_addr,
    u8 *reg,
    u8 *expected_buf,
    unsigned int len,
    int panel_index,
    int block_offset)
{
    int mismatch_count = 0;
    int i;
    int ret;
    u8 *readback_buf = kmalloc(JBD4020_I2C_BLOCK_SIZE, GFP_KERNEL);
    if (!readback_buf){
      dev_err(cdata->dev, "Failed to create readback buffer for demura block verification");
      return;
    }
        
    ret = jbd4020_i2c_read(panel_addr, reg, readback_buf, len);
    if (ret < 0) {
        dev_err(cdata->dev, "Failed to read back demura block at offset %d for panel %d", block_offset, panel_index);
        kfree(readback_buf);
        return;
    }

    for (i = 0; i < len; ++i) {
        if (readback_buf[i] != expected_buf[i]) {
            mismatch_count++;
        }
    }
    if (mismatch_count > 0) {
        dev_err(cdata->dev, "Demura block verification failed at offset %d for panel %d: %d mismatches",
                block_offset, panel_index, mismatch_count);
    }
    else {
        dev_info(cdata->dev, "Demura block verification passed at offset %d for panel %d", block_offset, panel_index);
    }
    kfree(readback_buf);
    return;
}

static int jbd4020_load_demura_table_i2c(int panel_index) {
  u8 reg[4] = {0x03, 00, 0x04, 0x00};
  int size = JBD4020_EXPECTED_DEMURA_SIZE;
  int idx = 0;
  int ret;
  u8 *demura_table[3] = {cdata->jbd4020_red_demura_table, cdata->jbd4020_green_demura_table, cdata->jbd4020_blue_demura_table};
  ktime_t start_time, end_time;
  s64 elapsed_time_us;

  dev_info(cdata->dev, "jbd4020_load_demura_table_i2c, start for panel %d", panel_index);
  start_time = ktime_get();
  while (size >= JBD4020_I2C_BLOCK_SIZE) {
    ret = jbd4020_i2c_write(panel_addrs[panel_index], reg, (u8*)(demura_table[panel_index] + idx), JBD4020_I2C_BLOCK_SIZE);
    if (ret < 0) {
      dev_err(cdata->dev, "Failed to write demura block at offset %d", idx);
      return ret;
    }
    if(cdata->is_demura_data_verification_enabled == 1){
      jbd4020_verify_demura_block(panel_addrs[panel_index], reg, demura_table[panel_index] + idx, JBD4020_I2C_BLOCK_SIZE, panel_index, idx);
    }
    idx += JBD4020_I2C_BLOCK_SIZE;
    size -= JBD4020_I2C_BLOCK_SIZE;
    if (reg[2] >= 0xF0) {
      reg[1] += 0x1;
    }
    reg[2] += 0x10;
  }
  if (size > 0) {
    ret =  jbd4020_i2c_write(panel_addrs[panel_index], reg, (u8*)(demura_table[panel_index] + idx), size);
    if (ret < 0) {
      dev_err(cdata->dev, "Failed to write demura block at offset %d", idx);
      return ret;
    }
    if(cdata->is_demura_data_verification_enabled == 1){
      jbd4020_verify_demura_block(panel_addrs[panel_index], reg, demura_table[panel_index] + idx, size, panel_index, idx);
    }
    idx += size;
  }
  end_time = ktime_get();
  dev_info(cdata->dev, "jbd4020_load_demura_table_i2c, end %d for panel %d\n", idx, panel_index);
  elapsed_time_us = ktime_to_us(ktime_sub(end_time, start_time));
  dev_info(cdata->dev, "jbd4020_load_demura_table_i2c execution time: %lld us (%lld ms) for panel %d\n", 
           elapsed_time_us, elapsed_time_us / 1000, panel_index);
  return idx;
}

static int jbd4020_get_load_demura_table() {
  int panel_idx = 0;
  int demura_size_loaded = 0;
  int ret = 0;
  const char *demura_files[] = {demura_red_bin_file, demura_green_bin_file, demura_blue_bin_file};
  size_t *demura_received_size[3] = {&cdata->received_red_demura_size, &cdata->received_green_demura_size, &cdata->received_blue_demura_size};
  if (cdata->is_demura_load_enabled == 0) {
    dev_info(cdata->dev, "Demura loading is disabled");
    return 0;
  }

  for (panel_idx = 0; panel_idx < 3; panel_idx++) {
    dev_info(cdata->dev, "available demura size for panel %d: %zu\n", panel_idx, *demura_received_size[panel_idx]);
    if(*demura_received_size[panel_idx] != JBD4020_EXPECTED_DEMURA_SIZE){
      dev_info(cdata->dev, "read demura table from binary file for panel %d: %s", panel_idx, demura_files[panel_idx]);
      if (get_demura_data(demura_files[panel_idx], panel_idx) != 0) {
        dev_err(cdata->dev, "Failed to load demura data from file for panel %d", panel_idx);
        return -EIO;
      }
    }
    // Load demura table to the specific panel
    demura_size_loaded = jbd4020_load_demura_table_i2c(panel_idx);
    if (demura_size_loaded > 0) {
      ret = jbd4020_i2c_write(panel_addrs[panel_idx], REG_DEMURA_CTL, VALUE_DEMURA_ENABLE, 4);
      if (ret < 0) {
        dev_err(cdata->dev, "Failed to enable demura for panel %d", panel_idx);
        return ret;
      } 
    }
  }
  return 0;
}

static ssize_t demura_enablement_show(struct device* dev, struct device_attribute* attr, char* buf) {
  if (!cdata || !cdata->is_enabled) {
    return snprintf(buf, sizeof(DEVICE_NOT_ENABLED_STRING), DEVICE_NOT_ENABLED_STRING);
  }

  dev_info(cdata->dev, "demura_enablement_show, cdata->is_demura_load_enabled: %d", cdata->is_demura_load_enabled);
  return snprintf(
      buf,
      ENABLE_VERIFY_DEMURA_STRING_EXPECTED_SIZE,
      ENABLE_VERIFY_DEMURA_STRING,
      cdata->is_demura_load_enabled);
}

static ssize_t demura_enablement_store(
    struct device* dev,
    struct device_attribute* attr,
    const char* buf,
    size_t count) {

  if (!cdata || !cdata->is_enabled) {
    return 0;
  }

  if (count < ENABLE_VERIFY_DEMURA_STRING_EXPECTED_SIZE) {
    dev_err(cdata->dev, "Invalid buffer size for enabling demura load: %zu, buffer string: %s\n", count, buf);
    return -EINVAL;
  }

  if (1 !=
      sscanf(
          buf,
          ENABLE_VERIFY_DEMURA_STRING,
          &cdata->is_demura_load_enabled)) {
    dev_err(cdata->dev, "Failed to get value for demura_load : %s\n", buf);
    return -EINVAL;
  }

  if((cdata->is_demura_load_enabled > 1)) {
    dev_err(cdata->dev, "Disabling demura_load invalid value for enable_demura_load from string, excepted value is either 0 or 1): %s\n", buf);
    cdata->is_demura_load_enabled = 0;
    return -EINVAL;
  }

  return count;
}

static struct device_attribute attr_enable_demura_load =
    __ATTR(enable_demura_load, 0660, demura_enablement_show, demura_enablement_store);

static ssize_t demura_verification_show(struct device* dev, struct device_attribute* attr, char* buf) {
  if (!cdata || !cdata->is_enabled) {
    return snprintf(buf, sizeof(DEVICE_NOT_ENABLED_STRING), DEVICE_NOT_ENABLED_STRING);
  }

  dev_info(cdata->dev, "demura_verification_show, cdata->is_demura_data_verification_enabled: %d", cdata->is_demura_data_verification_enabled);
  return snprintf(
      buf,
      ENABLE_VERIFY_DEMURA_STRING_EXPECTED_SIZE,
      ENABLE_VERIFY_DEMURA_STRING,
      cdata->is_demura_data_verification_enabled);
}

static ssize_t demura_verification_store(
    struct device* dev,
    struct device_attribute* attr,
    const char* buf,
    size_t count) {

  if (!cdata || !cdata->is_enabled) {
    return 0;
  }

  if (count < ENABLE_VERIFY_DEMURA_STRING_EXPECTED_SIZE) {
    dev_err(cdata->dev, "Invalid buffer size for demura data verification: %zu\n, buffer string: %s\n", count, buf);
    return -EINVAL;
  }

  if (1 !=
      sscanf(
          buf,
          ENABLE_VERIFY_DEMURA_STRING,
          &cdata->is_demura_data_verification_enabled)) {
    dev_err(cdata->dev, "Failed to get value for demura_verification : %s\n", buf);
    return -EINVAL;
  }

  if((cdata->is_demura_data_verification_enabled > 1)) {
    dev_err(cdata->dev, "Disabling demura_verification invalid value for enable_demura_verification from string, excepted value is either 0 or 1): %s\n", buf);
    cdata->is_demura_data_verification_enabled = 0;
    return -EINVAL;
  }

  return count;
}

static struct device_attribute attr_enable_demura_verification =
    __ATTR(enable_demura_verification, 0660, demura_verification_show, demura_verification_store);

static ssize_t panel_offset_show(struct device* dev, struct device_attribute* attr, char* buf) {
  return snprintf(
      buf,
      PANEL_OFFSET_STRING_MAX_SIZE,
      PANEL_OFFSET_STRING,
      cdata->panel_offset[0].h,
      cdata->panel_offset[0].v,
      cdata->panel_offset[1].h,
      cdata->panel_offset[1].v,
      cdata->panel_offset[2].h,
      cdata->panel_offset[2].v);
}

static ssize_t panel_offset_store(
    struct device* dev,
    struct device_attribute* attr,
    const char* buf,
    size_t count) {
  if (!cdata || !cdata->is_enabled) {
    return 0;
  }

  if (count < PANEL_OFFSET_STRING_MIN_SIZE || count > PANEL_OFFSET_STRING_MAX_SIZE) {
    dev_err(cdata->dev, "Invalid buffer size for panel offsets: %zu\n", count);
    return -EINVAL;
  }

  if (6 !=
      sscanf(
          buf,
          PANEL_OFFSET_STRING,
          &cdata->panel_offset[0].h,
          &cdata->panel_offset[0].v,
          &cdata->panel_offset[1].h,
          &cdata->panel_offset[1].v,
          &cdata->panel_offset[2].h,
          &cdata->panel_offset[2].v)) {
    dev_err(cdata->dev, "Failed to get panel offsets value from string: %s\n", buf);
    return -EINVAL;
  }

  return count;
}

static struct device_attribute attr_panel_offset =
    __ATTR(panel_offset, 0660, panel_offset_show, panel_offset_store);

int jbd4020_i2c_write(unsigned short addr, u8* reg, u8* buffer, unsigned int len) {
  struct i2c_msg msg;
  u8* write_buf;
  int retry = 0, ret = 0;

  write_buf = devm_kzalloc(cdata->dev, len + 4, GFP_KERNEL);
  write_buf[0] = reg[0];
  write_buf[1] = reg[1];
  write_buf[2] = reg[2];
  write_buf[3] = reg[3];
  memcpy(write_buf + 4, buffer, len);

  msg.addr = addr;
  msg.flags = I2C_M_STOP | I2C_M_DMA_SAFE;
  msg.len = len + 4;
  msg.buf = write_buf;

  do {
    ++retry;

    ret = i2c_transfer(cdata->adapter, &msg, 1);
    if (ret < 0) {
      dev_info(
          cdata->dev,
          "Failed writing register [0x%x, 0x%x, 0x%x, 0x%x] with error %d",
          reg[0],
          reg[1],
          reg[2],
          reg[3],
          ret);

      if (retry < JBD4020_I2C_MAX_RETRIES)
        udelay(JBD4020_I2C_DELAY_US);
    }
  } while (ret < 0 && retry < JBD4020_I2C_MAX_RETRIES);

  if (ret < 0)
    dev_err(
        cdata->dev,
        "Failed to write register [0x%x, 0x%x, 0x%x, 0x%x] after %d retries",
        reg[0],
        reg[1],
        reg[2],
        reg[3],
        JBD4020_I2C_MAX_RETRIES);

  devm_kfree(cdata->dev, write_buf);
  return ret;
}

int jbd4020_i2c_read(unsigned short addr, u8* reg, u8* buffer, unsigned int len) {
  struct i2c_msg msg;
  int retry = 0, ret = 0;

  ret = jbd4020_i2c_write(addr, reg, NULL, 0);
  if (ret < 0)
    return ret;

  msg.addr = addr;
  msg.flags = I2C_M_RD | I2C_M_STOP | I2C_M_DMA_SAFE;
  msg.len = len;
  msg.buf = (u8*)buffer;

  do {
    ++retry;

    ret = i2c_transfer(cdata->adapter, &msg, 1);
    if (ret < 0) {
      dev_info(
          cdata->dev,
          "Failed reading register [0x%x, 0x%x, 0x%x, 0x%x]with error %d",
          reg[0],
          reg[1],
          reg[2],
          reg[3],
          ret);

      if (retry < JBD4020_I2C_MAX_RETRIES)
        udelay(JBD4020_I2C_DELAY_US);
    }
  } while (ret < 0 && retry < JBD4020_I2C_MAX_RETRIES);

  if (ret < 0)
    dev_err(
        cdata->dev,
        "Failed to read register [0x%x, 0x%x, 0x%x, 0x%x] after %d retries",
        reg[0],
        reg[1],
        reg[2],
        reg[3],
        JBD4020_I2C_MAX_RETRIES);

  return ret;
}

int jbd4020_i2c_init(void) {
  int ret = 0;
  int i;
  struct i2c_msg msg;
  u8 payload[8] = {0};

  dev_info(cdata->dev, "jbd4020_i2c_init\n");
  msg.flags = I2C_M_STOP | I2C_M_DMA_SAFE;
  msg.len = 8;

  for (i = 0; i < JBD4020_DEFAULT_REGMAP_SIZE; i++) {
    msg.buf = (unsigned char*)&(jbd4020_default_regmap_patch[i].data);

    if (jbd4020_default_regmap_patch[i].des_bm & 0x01) {
      msg.addr = JBD4020_RED_PANEL_ADDR;
      ret = i2c_transfer(cdata->adapter, &msg, 1);
      if (ret < 0) {
        break;
        dev_info(
            cdata->dev,
            "Failed writing register [0x%x, 0x%x, 0x%x, 0x%x] with error %d",
            msg.buf[0],
            msg.buf[1],
            msg.buf[2],
            msg.buf[3],
            ret);
      }
    }
    if (jbd4020_default_regmap_patch[i].des_bm & 0x02) {
      msg.addr = JBD4020_GREEN_PANEL_ADDR;
      ret = i2c_transfer(cdata->adapter, &msg, 1);
      if (ret < 0) {
        break;
        dev_info(
            cdata->dev,
            "Failed writing register [0x%x, 0x%x, 0x%x, 0x%x] with error %d",
            msg.buf[0],
            msg.buf[1],
            msg.buf[2],
            msg.buf[3],
            ret);
      }
    }

    if (jbd4020_default_regmap_patch[i].des_bm & 0x04) {
      msg.addr = JBD4020_BLUE_PANEL_ADDR;
      ret = i2c_transfer(cdata->adapter, &msg, 1);
      if (ret < 0) {
        break;
        dev_info(
            cdata->dev,
            "Failed writing register [0x%x, 0x%x, 0x%x, 0x%x] with error %d",
            msg.buf[0],
            msg.buf[1],
            msg.buf[2],
            msg.buf[3],
            ret);
      }
    }

    if (jbd4020_default_regmap_patch[i].delayms) {
      mdelay(jbd4020_default_regmap_patch[i].delayms);
    }
  }

  // end with dynamic reg and endding
  msg.buf = payload;
  payload[0] = REG_SRAM_MIR_FLIP[0];
  payload[1] = REG_SRAM_MIR_FLIP[1];
  payload[2] = REG_SRAM_MIR_FLIP[2];
  payload[3] = REG_SRAM_MIR_FLIP[3];
  for (i = 0; i < 3; i++) {
    payload[4] = panel_mirrored[i] ? 0x01 : 0x00;
    payload[5] = 0x00;
    payload[6] = 0x00;
    payload[7] = 0x00;
    msg.addr = panel_addrs[i];
    if (0 > i2c_transfer(cdata->adapter, &msg, 1)) {
      dev_info(cdata->dev, "Failed writing register REG_SRAM_MIR_FLIP with address %d", msg.addr);
    }
  }

  payload[0] = REG_SRAM_OFFSET[0];
  payload[1] = REG_SRAM_OFFSET[1];
  payload[2] = REG_SRAM_OFFSET[2];
  payload[3] = REG_SRAM_OFFSET[3];
  for (i = 0; i < 3; i++) {
    payload[4] = (cdata->panel_offset[i].h & 0xF) << 4 | 0x01;
    payload[5] = (cdata->panel_offset[i].v & 0xF) << 4 | (cdata->panel_offset[i].h & 0x100) >> 8;
    payload[6] = (cdata->panel_offset[i].v & 0x100) >> 8;
    payload[7] = 0x00;
    msg.addr = panel_addrs[i];
    if (0 > i2c_transfer(cdata->adapter, &msg, 1)) {
      dev_info(cdata->dev, "Failed writing register REG_SRAM_OFFSET with address %d", msg.addr);
    }
  }

  payload[0] = REG_VO_MIR_FLIP_OFFSET[0];
  payload[1] = REG_VO_MIR_FLIP_OFFSET[1];
  payload[2] = REG_VO_MIR_FLIP_OFFSET[2];
  payload[3] = REG_VO_MIR_FLIP_OFFSET[3];
  for (i = 0; i < 3; i++) {
    payload[4] = cdata->panel_offset[i].h & 0xFF;
    payload[5] = (cdata->panel_offset[i].h >> 8) & 0x0F;
    payload[6] = cdata->panel_offset[i].v & 0xFF;
    payload[7] = (cdata->panel_offset[i].v >> 8) & 0x0F;
    payload[7] |= panel_mirrored[i] ? 0x60 : 0x20;
    msg.addr = panel_addrs[i];
    if (0 > i2c_transfer(cdata->adapter, &msg, 1)) {
      dev_info(
          cdata->dev, "Failed writing register REG_VO_MIR_FLIP_OFFSET with address %d", msg.addr);
    }
  }

  payload[0] = REG_CFG_END[0];
  payload[1] = REG_CFG_END[1];
  payload[2] = REG_CFG_END[2];
  payload[3] = REG_CFG_END[3];
  payload[4] = 0x02;
  payload[5] = 0x00;
  payload[6] = 0x00;
  payload[7] = 0x00;
  for (i = 0; i < 3; i++) {
    msg.addr = panel_addrs[i];
    if (0 > i2c_transfer(cdata->adapter, &msg, 1)) {
      dev_info(cdata->dev, "Failed writing register REG_CFG_END with address %d", msg.addr);
    }
  }

  return ret == 1 ? 0 : ret;
}

static int jbd4020_pmic_enable() {
  dev_info(cdata->dev, "jbd4020 pmic_enable\n");
  return regulator_enable(cdata->pmic_regulator);
}

static void jbd4020_thermal_enable() {
  if (cdata->tz) {
    if (jbd4020_i2c_write(cdata->addr, REG_THERMAL_CTRL, VALUE_THERMAL_ENABLE, 4) < 0) {
      dev_info(cdata->dev, "jbd4020_thermal_enable failed\n");
    } else {
      thermal_zone_device_enable(cdata->tz);
      dev_info(cdata->dev, "jbd4020_thermal_enable done\n");
    }
  }
}

static void jbd4020_thermal_disable() {
  if (cdata->tz) {
    thermal_zone_device_disable(cdata->tz);
    dev_info(cdata->dev, "jbd4020_thermal_disable done\n");
  }
}

static int jbd4020_pmic_disable() {
  dev_info(cdata->dev, "jbd4020 pmic_disable\n");

  return regulator_disable(cdata->pmic_regulator);
}

static void jbd4020_drm_pre_enable(struct drm_bridge* bridge) {
  if (cdata->is_enabled) {
    dev_info(cdata->dev, "jbd4020 is already enabled. Skipping pre-enable\n");
    return;
  }

  jbd4020_pmic_enable();
  mdelay(100);
  jbd4020_i2c_init();
  jbd4020_thermal_enable();
  dev_info(cdata->dev, "jbd4020_drm_pre_enable set flag done\n");
}

static void jbd4020_drm_enable(struct drm_bridge* bridge) {
  if (cdata->is_enabled) {
    dev_info(cdata->dev, "jbd4020 already enabled. Skipping clearscreen (DRM enable)\n");
    return;
  }

  // TODO: clear screen or enable port
  // load and enable demura
  jbd4020_get_load_demura_table();
  cdata->is_enabled = true;
  dev_info(cdata->dev, "jbd4020_drm_enable set flag done\n");
}

static void jbd4020_drm_disable(struct drm_bridge* bridge) {
  if (!cdata->is_enabled) {
    dev_info(cdata->dev, "jbd4020 not enabled, no need to disable.\n");
    return;
  }

  jbd4020_thermal_disable();
  cdata->is_enabled = false;
  jbd4020_pmic_disable();
  dev_info(cdata->dev, "jbd4020_drm_disable set flag done\n");
}

static const struct drm_bridge_funcs jbd4020_bridge_funcs = {
    .pre_enable = jbd4020_drm_pre_enable,
    .enable = jbd4020_drm_enable,
    .disable = jbd4020_drm_disable,
};

static int jbd4020_bind(struct device* dev, struct device* master, void* data) {
  dev_err(dev, "jbd4020 bind successful\n");
  return 0;
}

static void jbd4020_unbind(struct device* dev, struct device* master, void* data) {
  dev_err(dev, "jbd4020 unbind successful\n");
}

static void read_temperature(u16 panel_addr, int* temp) {
  int ret;
  u8 temp_data[2];
  int temp_out;
  *temp = 0;
  ret = jbd4020_i2c_read(panel_addr, REG_THERMAL_READ, temp_data, 2);
  if (ret < 0) {
    dev_err(cdata->dev, "Failed read register thermal_enable with error %d", ret);
    return;
  }

  temp_out = ((temp_data[1] << 8) | temp_data[0]) & 0x3FF;
  *temp = TEMP_BYTES_TO_MCELSIUS(temp_out);
  dev_err(
      cdata->dev, " jbd4020_get_temp: %x %x %d %d", temp_data[1], temp_data[0], temp_out, *temp);

  return;
}

static ssize_t temp_ctrl_show(struct device* dev, struct device_attribute* attr, char* buf) {
  int panel_temp[3] = {0};
  int i;
  if (!cdata) {
    return snprintf(buf, sizeof(DEVICE_NOT_AVAILABLE_STRING), DEVICE_NOT_AVAILABLE_STRING);
  }

  if (!cdata->is_enabled) {
    return snprintf(buf, sizeof(DEVICE_NOT_ENABLED_STRING), DEVICE_NOT_ENABLED_STRING);
  }
  
  for (i = 0; i < 3; i++) {
    read_temperature(panel_addrs[i], &panel_temp[i]);
  }

  return snprintf(
      buf,
      TEMPERATURE_STRING_MAX_SIZE,
      TEMPERATURE_STRING,
      (unsigned int)(panel_temp[0]),
      (unsigned int)(panel_temp[1]),
      (unsigned int)(panel_temp[2]));
}

static struct device_attribute attr_temp_ctrl =
    __ATTR(temperature_ctrl, 0440, temp_ctrl_show, NULL);

static int jbd4020_get_temp(void* data, int* temp) {
  struct jbd4020_data* cdata = data;

  if (!cdata || !cdata->tz)
    return -EINVAL;

  if (!cdata->is_enabled || cdata->tz->mode == THERMAL_DEVICE_DISABLED) {
    return 0;
  }

  read_temperature(cdata->addr, temp);
  return 0;
}

static struct thermal_zone_of_device_ops jbd4020_thermal_ops = {
    .get_temp = jbd4020_get_temp,
};

static const struct component_ops jbd4020_bridge_comp_ops = {
    .bind = jbd4020_bind,
    .unbind = jbd4020_unbind,
};

static const struct of_device_id jbd4020_of_match[] = {
    {.compatible = "meta,jbd4020-i2c"},
    {},
};

static int jbd4020_thermal_init() {
  cdata->tz = devm_thermal_zone_of_sensor_register(cdata->dev, 0, cdata, &jbd4020_thermal_ops);
  if (IS_ERR(cdata->tz)) {
    dev_err(cdata->dev, "Failed to register thermal zone device\n");
    return PTR_ERR(cdata->tz);
  }
  // Disable the thermal zone by default. enable it when display is on.
  thermal_zone_device_disable(cdata->tz);

  return 0;
}

static int jbd4020_probe(struct i2c_client* i2c, const struct i2c_device_id* id) {
  int i, ret = 0;
  struct device* dev = &i2c->dev;
  bool is_cont_splash_enabled;
  char* regulator_name = "pmicDA9172";
  dev_err(dev, "jbd4020_probe\n");

  if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
    dev_err(dev, "No I2C functionality present\n");
    return -ENODEV;
  }

  is_cont_splash_enabled = of_property_read_bool(i2c->dev.of_node, "continuous-splash");
  cdata = devm_kzalloc(dev, sizeof(struct jbd4020_data), GFP_KERNEL);
  if (cdata == NULL)
    return -ENOMEM;

  cdata->i2c = i2c;
  cdata->dev = dev;
  cdata->adapter = i2c->adapter;
  cdata->addr = i2c->addr;
  cdata->pmic_regulator = regulator_get(cdata->dev, regulator_name);
  if (IS_ERR(cdata->pmic_regulator)) {
    dev_err(dev, "Failed to get DA9172 PMIC regulator. Deferring\n");
    ret = -EPROBE_DEFER;
    goto error;
  }

  ret = jbd4020_thermal_init();
  if (ret != 0) {
    dev_err(dev, "Failed to init jbd4020 thermal %d\n", ret);
    goto error;
  }

  cdata->is_enabled =
      is_cont_splash_enabled; // jbd4020 is already enabled by UEFI if cont splash is enabled
  dev_err(dev, "jbd4020_probe, enable=%d\n", is_cont_splash_enabled);
  if (cdata->is_enabled) {
    jbd4020_pmic_enable(); // Enable PMIC to resolve unbalanced PMIC disables
    jbd4020_thermal_enable();
  }

  if (device_create_file(dev, &attr_brightness_ctrl)) {
    dev_err(dev, "device_create_file failed for brightness_ctrl\n");
    goto error;
  };

  if (device_create_file(dev, &attr_panel_offset)) {
    dev_err(dev, "device_create_file failed for panel_offset\n");
    goto error;
  };
  for (i = 0; i < 3; i++) {
    cdata->panel_offset[i].h = default_panel_offset[i].h;
    cdata->panel_offset[i].v = default_panel_offset[i].v;
  }

  if (device_create_file(dev, &attr_temp_ctrl)) {
    dev_err(dev, "device_create_file failed for temp_ctrl\n");
    goto error;
  };

  if (device_create_file(dev, &attr_mipi_interface)) {
    dev_err(dev, "device_create_file failed for brightness_ctrl\n");
    goto error;
  };

  if (device_create_file(dev, &attr_enable_demura_load)) {
    dev_err(dev, "device_create_file failed for enable_demura_load\n");
    goto error;
  };

  if (device_create_file(dev, &attr_enable_demura_verification)) {
    dev_err(dev, "device_create_file failed for enable_demura_data_verification\n");
    goto error;
  };
  
  bridge.funcs = &jbd4020_bridge_funcs;
  bridge.of_node = i2c->dev.of_node;
  drm_bridge_add(&bridge);

  ret = component_add(&i2c->dev, &jbd4020_bridge_comp_ops);
  if (ret)
    dev_err(dev, "component add failed, rc=%d\n", ret);

  i2c_set_clientdata(i2c, cdata);

  //prepare demura table for loading
  cdata->received_red_demura_size = 0;
  cdata->received_green_demura_size = 0;
  cdata->received_blue_demura_size = 0;

  //disable demura load by default
  cdata->is_demura_load_enabled = 0;

  //disable demura data verification by default
  cdata->is_demura_data_verification_enabled = 0;

  dev_err(dev, "jbd4020__probe done\n");
  dev_info(dev, "jbd4020 init driver probe successful\n");
  return 0;

error:
  devm_kfree(dev, cdata);
  return ret;
}

static int jbd4020_remove(struct i2c_client* i2c) {
  dev_err(&i2c->dev, "jbd4020_remove\n");
  if (!IS_ERR(cdata->pmic_regulator))
    regulator_put(cdata->pmic_regulator);
  i2c_set_clientdata(i2c, NULL);
  device_remove_file(cdata->dev, &attr_brightness_ctrl);
  device_remove_file(cdata->dev, &attr_temp_ctrl);
  device_remove_file(cdata->dev, &attr_mipi_interface);
  device_remove_file(cdata->dev, &attr_panel_offset);
  devm_kfree(cdata->dev, cdata);
  return 0;
}

static const struct i2c_device_id jbd4020_id_table[] = {
    {
        "jbd4020",
    },
    {}};
MODULE_DEVICE_TABLE(i2c, jbd4020_id_table);

static struct i2c_driver jbd4020_driver = {
    .driver =
        {
            .name = "jbd4020-i2c-driver",
            .of_match_table = jbd4020_of_match,
        },
    .probe = jbd4020_probe,
    .remove = jbd4020_remove,
    .id_table = jbd4020_id_table,
};

module_i2c_driver(jbd4020_driver);

MODULE_DESCRIPTION("JBD4020 I2C driver");
