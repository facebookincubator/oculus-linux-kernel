# Realsense Card Controller `rscc`



## Modes

### OFF

- `rs_fpga_spi` - disabled
- `rs_ds5` - disabled
- `rs_ds5_flash` - enabled

### ON

- `rs_fpga_spi` - enabled
- `rs_ds5` - enabled
- `rs_ds5_flash` - disabled

### FLASH

- `rs_fpga_spi` - disabled
- `rs_ds5` - disabled
- `rs_ds5_flash` - enabled

## Userspace API/access & endpoints

## `sysfs` attributes

NOTE: Only available in `userdebug` & `eng` builds

| Attribute               | Permissions | Known Values                                                 |
|-------------------------|-------------|--------------------------------------------------------------|
| `rscc_mode`             | `rw`        | `off&#124;on&#124;flash`                                     |
| `rscc_ds5_flash_device` | `r`         | Outputs the device name of the `spi_flash` used by the `ds5` |
| `rscc_ds5_fw`           | `rw`        | Read firmware version or write firmware absolute path        |

## IOCTL (Not implemented yet)

> Currently when `rscc_mode` is set to `on`, you can directly interact
> with `/dev/d4xx`.  This will change to `/dev/rscc`, which will proxy to
> the `d4xx` driver ensuring that both `mode` & `power` settings are respected

The `ioctl` device will be `/dev/rscc`. The header to use will be `uapi/linux/rscc.h` (`linux/rscc.h`) Commands will include:
* 
* `ds5_stream_enable(bool)`
* `ds5_fw_update(char[256])`
* `ds5_fw_version()`
* `ds5_reg_set(u16,u16)`
* `ds5_reg_get(u16,u16*)`
* `fpga_fw_update(char[256])`
* `fpga_fw_version()`

This is a work in progress, more will be added as required


## Examples

### How to change RSCC mode manually

```shell
# run in adb shell

# Turn all modules/resources on card OFF
echo off > /sys/module/realsense_card_controller/drivers/platform:oculus,rscc/vendor:oculus,rscc/rscc_mode

# Turn all modules/resources on card ON
echo on > /sys/module/realsense_card_controller/drivers/platform:oculus,rscc/vendor:oculus,rscc/rscc_mode

# Configure modules to be ready for flashing
echo flash > /sys/module/realsense_card_controller/drivers/platform:oculus,rscc/vendor:oculus,rscc/rscc_mode

# To check the current mode
cat /sys/module/realsense_card_controller/drivers/platform:oculus,rscc/vendor:oculus,rscc/rscc_mode

```

### How to flash DS-5 manually

> The FW must reside in `/vendor/firmware`

```shell
# run in adb shell

# 1. Put the realsense daughter card into `flash` mode
echo flash > /sys/module/realsense_card_controller/drivers/platform:oculus,rscc/vendor:oculus,rscc/rscc_mode

# 2. to use the default firmware /vendor/firmware/realsense-fw.bin
echo '' > /sys/module/realsense_card_controller/drivers/platform:oculus,rscc/vendor:oculus,rscc/rscc_ds5_fw
# OR - to use the some other firmware /vendor/firmware/realsense-example-123-fw.bin 
echo realsense-example-123-fw.bin >  /sys/module/realsense_card_controller/drivers/platform:oculus,rscc/vendor:oculus,rscc/rscc_ds5_fw

# 3.  Currently the FW flash is done async & to see the status, you must follow the kernel
# logs in either `logcat` or the serial console or `dmesg`
# the process takes ~30s

# 4. Once complete, set the mode to either `on` and read the FW version
# NOTE: as mentioned this will change when the end point is added to the `rscc` driver
cat /sys/bus/i2c/devices/3-0010/ds5_fw_ver
```
