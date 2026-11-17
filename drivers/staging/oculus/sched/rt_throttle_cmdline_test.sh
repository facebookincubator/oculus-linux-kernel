#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Checks that the RT throttle dump names the throttling process by argv[0]
# rather than by its thread comm.
#
# This cannot be a KUnit case, which is why it sits beside rt_throttle_test.c
# rather than inside it. Only a real process has an argv[0] -- the kthread hog
# the KUnit suite uses has no mm -- and the kernel cannot spawn one either:
# Android builds set CONFIG_STATIC_USERMODEHELPER=y with an empty path, so
# call_usermodehelper() reports success and runs nothing.
#
# The property under test is what breaks crash-report grouping in the field.
# libbinder renames the threads of a HAL to binder:<tgid>_<n>, including the
# group leader, so comm embeds the pid and yields a new grouping key on every
# boot. Here a shell spinner stands in: comm is "sh", argv[0] is the path.
#
# Usage: rt_throttle_cmdline_test.sh [adb-serial]
set -u
SYSCTL=/proc/sys/sched_rt
# Bounded by timeout(1) in the spawned command itself: if this script dies, or
# adb wedges, the spinners still exit on their own. An unbounded SCHED_FIFO
# spinner that outlives its harness can starve the device past the point where
# adb can recover it.
SPIN_SECS=12
# The marker makes the spinner identifiable. Matching on the loop text alone
# also matches this harness's own shell and timeout(1), whose argv contains it.
SPIN_MARK=rt_cmdline_spin
SPINNER="$SPIN_MARK=1; while : ; do : ; done"
# A copy of sh gives a second spinner a different argv[0].
SECOND_SH=/data/local/tmp/rt_cmdline_sh2
spawn_spinner() { # spawn_spinner <cpu-mask> [shell]
	$ADB shell "nohup timeout $SPIN_SECS taskset $1 chrt -f 40 \
		${2:-/system/bin/sh} -c '$SPINNER' >/dev/null 2>&1 &" >/dev/null 2>&1
}
ADB="adb${1:+ -s $1}"

fail() { echo "FAIL: $*"; exit 1; }
cleanup() {
	$ADB shell "pkill -f $SPIN_MARK" >/dev/null 2>&1
	$ADB shell "rm -f $SECOND_SH" >/dev/null 2>&1
	[ -n "${entry:-}" ] && $ADB shell "echo $entry > $SYSCTL/panic_on_throttle" >/dev/null 2>&1
}

$ADB wait-for-device || fail "no device"
$ADB root >/dev/null 2>&1; sleep 3; $ADB wait-for-device
[ "$($ADB shell id -u | tr -d '\r')" = "0" ] || fail "adb root unavailable"
$ADB shell "[ -e $SYSCTL/panic_on_throttle ]" || fail "$SYSCTL missing (needs RT_THROTTLING_SYSCTL)"

entry=$($ADB shell "cat $SYSCTL/panic_on_throttle" | tr -d '\r')
trap cleanup EXIT

# clear_data runs reset_sysctl_to_defaults(), which re-arms panic_on_throttle,
# so always disarm after clearing rather than before. Getting this backwards
# panics the device on the throttle below instead of reporting a failure.
arm_off() {
	$ADB shell "echo 1 > $SYSCTL/clear_data; echo 0 > $SYSCTL/panic_on_throttle"
	[ "$($ADB shell "cat $SYSCTL/panic_on_throttle" | tr -d '\r')" = "0" ] \
		|| fail "could not disarm panic_on_throttle"
}

# Pin to the last online CPU so one spinner is enough to exhaust the budget.
last=$($ADB shell 'cat /sys/devices/system/cpu/online' | tr -d '\r' | sed 's/.*-//')
mask=$(printf '%x' $((1 << last)))

arm_off
spawn_spinner "$mask"
# Confirm it really is SCHED_FIFO and confined, before any multi-spinner phase
# adds RT load. Only the spinner is FIFO; the wrappers are SCHED_OTHER and their
# argv contains the loop text, so match on policy rather than on the text alone.
sleep 1
spin_pid=$($ADB shell "for p in \$(pgrep -f $SPIN_MARK); do
	chrt -p \$p 2>/dev/null | grep -q FIFO && { echo \$p; break; }; done" | tr -d '\r')
[ -n "$spin_pid" ] || fail "no SCHED_FIFO spinner started"
aff=$($ADB shell "taskset -p $spin_pid" 2>/dev/null | tr -d '\r' | sed 's/.*: //')
echo "spinner pid $spin_pid affinity $aff (want $mask)"
[ "$aff" = "$mask" ] || fail "spinner not pinned to cpu $last (mask $aff)"

# Assert on the *first* throttle. The name is read at dump time from the
# address space we are already in, so there is no warm-up: the first throttle
# is the only kind panic_on_throttle=1 ever produces, and it must resolve.
sleep 3

name=$($ADB shell "cat $SYSCTL/process_name" | tr -d '\r')
thread=$($ADB shell "cat $SYSCTL/thread_name" | tr -d '\r')
rows=$($ADB shell "cat $SYSCTL/top_contributors" | tr -d '\r')
pid=$($ADB shell "cat $SYSCTL/pid" | tr -d '\r')

echo "process_name: '$name'"
echo "thread_name:  '$thread'"
echo "rows:         '$rows'"

[ -n "$name" ] || fail "no throttle latched; the spinner did not run RT"
[ "$name" != "unavailable" ] || fail "process name was 'unavailable'"
[ "$name" != "$thread" ] || fail "process named by thread comm '$name', not argv[0]"
case "$name" in
	/*) ;;
	*)  fail "process name is not a path: '$name'" ;;
esac
# The report is resolved in a worker, so rows carry argv[0] too and must agree
# with the hog line. Rows for processes that cannot be resolved (kthreads) keep
# their comm, which is why this only checks the hog's own row.
echo "$rows" | grep -q "^$name " \
	|| fail "contributor row disagrees with the hog line: '$rows'"

# Two blame strings exist. The tick prints one immediately, so the dump
# survives a crash before the worker runs, and it can only carry comms. The
# worker prints the resolved name on its own line, and panic() -- when armed --
# uses that one, so the crash key is the resolved name.
blame=$($ADB shell "dmesg | grep -F 'Throttling triggered by thread' | tail -1" | tr -d '\r')
[ -n "$blame" ] || fail "no blame line in dmesg"
blame_thread=$(echo "$blame" | sed -n 's/.*thread "\([^"]*\)".*/\1/p')
[ "$blame_thread" = "$thread" ] \
	|| fail "blame thread '$blame_thread' != latched '$thread'"

resolved=$($ADB shell "dmesg | grep -F 'rt_throttle: process' | tail -1" | tr -d '\r')
[ -n "$resolved" ] || fail "worker never emitted a resolved name"
res_name=$(echo "$resolved" | sed -n 's/.*process "\([^"]*\)".*/\1/p')
[ "$res_name" = "$name" ] \
	|| fail "resolved '$res_name' != latched '$name'"

# The latch belongs to the first throttle after a clear. A later throttle by a
# process with another argv[0] still gets its own worker, but must not mix its
# name into the latched pid and thread.
$ADB shell "pkill -f $SPIN_MARK" >/dev/null 2>&1
sleep 2
$ADB shell "cp /system/bin/sh $SECOND_SH && chmod 755 $SECOND_SH" \
	|| fail "could not stage $SECOND_SH"
spawn_spinner "$mask" "$SECOND_SH"
sleep 3
$ADB shell "dmesg | grep -qF 'rt_throttle: process \"$SECOND_SH\"'" \
	|| fail "second throttle by $SECOND_SH never reported"
name2=$($ADB shell "cat $SYSCTL/process_name" | tr -d '\r')
thread2=$($ADB shell "cat $SYSCTL/thread_name" | tr -d '\r')
pid2=$($ADB shell "cat $SYSCTL/pid" | tr -d '\r')
rows2=$($ADB shell "cat $SYSCTL/top_contributors" | tr -d '\r')
echo "after a second throttle: process_name '$name2' pid $pid2"
[ "$name2" = "$name" ] && [ "$thread2" = "$thread" ] && [ "$pid2" = "$pid" ] \
	|| fail "latch overwritten: pid $pid2 thread '$thread2' process '$name2'"
echo "$rows2" | grep -q "^$name " \
	|| fail "latched rows overwritten by the second throttle: '$rows2'"

echo "PASS: '$name', thread comm '$thread', rows and latch agree, second throttle left the latch alone"
