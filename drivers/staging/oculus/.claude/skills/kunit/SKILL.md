---
name: kunit
description: Write, build, run, and analyze KUnit kernel unit tests for drivers in kernel/drivers/staging/oculus. Use when the user mentions kunit, kernel unit tests, kernel test, or wants to test a kernel module.
allowed-tools: Read, Edit, Write, Bash, Grep, Glob, Agent
---

# KUnit Testing for Oculus Staging Drivers

## Writing Tests

- Test files go next to the driver: `foo_test.c` for `foo.c`
- Add Kconfig entry to `Kconfig.test` and Makefile entry to `Makefile.test` — follow existing entries as examples
- Also add a `select <TEST_CONFIG> if <DRIVER_DEP>` line to the `KUNIT_STAGING_OCULUS` master switch in `Kconfig.test`
- Do NOT write trivial tests that just verify assignments
- Do NOT duplicate tests for thin wrappers — test the underlying function
- DO verify exact behavior and side effects, not just return codes
- DO test boundary conditions, ordering guarantees, and resource lifecycle
- DO verify error paths return correct error codes

## Build & Run

```bash
USE_PREBUILT_KERNEL=false m kernelmodules
```

```bash
adb shell "rmmod <name>_test" 2>/dev/null
adb push out/target/product/<device>/obj/KERNEL_OBJ/drivers/staging/oculus/<name>_test.ko /data/local/tmp/
adb shell "dmesg -C"
adb shell "insmod /data/local/tmp/<name>_test.ko"
adb shell dmesg | grep -E '^\[.*\] (ok|not ok) [0-9]+ - '
```
