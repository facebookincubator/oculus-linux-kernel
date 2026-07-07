---
name: build-module
description: Build a kernel module from kernel/drivers/staging/oculus and optionally push it to a connected device. Use this when the user wants to build, rebuild, or push a kernel module (.ko).
argument-hint: <module-name>
---

# Build Kernel Module Skill

Build an individual kernel module from `kernel/drivers/staging/oculus` and
optionally push it to a connected device.

## Prerequisites

- AOSP checkout with `build/envsetup.sh` available
- The current working directory must be within the AOSP tree

## Workflow

### 1. Determine the lunch target and device

Check for connected devices first:

```bash
adb devices -l 2>/dev/null | grep -v "^List" | grep -v "^$"
```

- **Device(s) connected and all the same product**: Read the product name with
  `adb shell getprop ro.product.device` and use `<product>-mainline-userdebug`
  as the lunch target.
- **Multiple different devices**: Ask the user which device to target. Set
  `ANDROID_SERIAL=<serial>` for all subsequent adb commands so they target the
  correct device.
- **No devices connected**: Ask the user what the target product should be for
  `lunch` (e.g. eureka, unknown6, hollywood, seacliff, stanley, starlet).

### 2. Build the module

The AOSP root is at `../../../../../` relative to `kernel/drivers/staging/oculus`.

Source envsetup, lunch, and build in a single shell invocation:

```bash
cd <aosp-root> && source build/envsetup.sh && lunch <target> && \
  EXTRA_KERNEL_OPTS='M=drivers/staging/oculus modules' m kernelcompdb
```

This builds all modules under `kernel/drivers/staging/oculus/`. The resulting
`.ko` files are placed at a known path:

```
<aosp-root>/out/target/product/<device>/obj/KERNEL_OBJ/drivers/staging/oculus/<module>.ko
```

Use a long timeout (10+ minutes) for the build command.

### 3. Push and reload on device

Only do this if `adb devices` shows a connected device.

1. Root the device and remount filesystems as writable:
   ```bash
   adb root && adb remount
   ```
2. Push the `.ko` file:
   ```bash
   adb push <aosp-root>/out/target/product/<device>/obj/KERNEL_OBJ/drivers/staging/oculus/<module>.ko /vendor/lib/modules/
   ```
3. Reload the module:
   ```bash
   adb shell "rmmod <module_name>" 2>/dev/null
   adb shell "insmod /vendor/lib/modules/<module_name>.ko"
   ```

The `rmmod` may fail if the module wasn't previously loaded; that is fine.

## Notes

- If `adb root` or `adb remount` fails, ask the user to check their device
  setup.
- Do NOT push or reload modules if no device is connected.
- Some modules live in subdirectories (e.g. `usbvdm/usbvdm.ko`,
  `mcu/swd/meta_swd.ko`). Use the correct subpath when locating the `.ko`.
