#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Run KUnit tests for kernel/drivers/staging/oculus/ using the
# kernel/meta/qemu UML kernel. No device or emulator needed.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
AOSP_ROOT="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"

cd "$AOSP_ROOT"
cp "$SCRIPT_DIR/qemu/kunitconfig" "$SCRIPT_DIR/qemu/.kunitconfig"
exec python3 kernel/meta/qemu/tools/testing/kunit/kunit.py run \
    --build_dir "$SCRIPT_DIR/qemu" \
    --make_options 'CC=gcc' \
    "$@"
