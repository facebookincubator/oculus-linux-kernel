/* SPDX-License-Identifier: GPL-2.0 */

/*
 * API for recording hibernation and boot events
 * from core and driver subsystems.
 */

/*
 * Kernel hibernation events.
 *
 * KERN1 are events from the first stage kernel, booted much
 * like a normal boot. These events are special because they are
 * temporarily stored in IMEM, which is preserved when hibernation
 * image is loaded into main memory.
 */
enum hibernation_kernel1_event {
	/* First-stage kernel started loading memory contents from disk. */
	HIBEVENT_KERN1_LOAD_MEM_FROM_DISK_STARTED,

	/* First-stage kernel finished loading memory contents from disk. */
	HIBEVENT_KERN1_LOAD_MEM_FROM_DISK_FINISHED,

	/* First-stage kernel failed to restore from hibernation, so
	 * it proceeds with a normal boot. */
	HIBEVENT_KERN1_HIBERNATION_RESTORE_FAILED,

	/* Number of valid first-stage kernel hibernation events. */
	HIBEVENT_KERN1_MAX
};

/*
 * Kernel hibernation events.
 *
 * KERN2 are events from the hibernated kernel, invoked (jumped to)
 * by first stage kernel once memory is restored from disk.
 */
enum hibernation_kernel2_event {
	/* Second-stage kernel is now executing. */
	HIBEVENT_KERN2_ARCH_RESUME,

	/* Second-stage kernel is resuming device drivers. */
	HIBEVENT_KERN2_START_DEVICE_RESUME,

	/* Second-stage kernel finished resuming device drivers. */
	HIBEVENT_KERN2_END_DEVICE_RESUME,

	/* Second-stage kernel is now fully restored and running. */
	HIBEVENT_KERN2_IMAGE_RESTORED,

	/* All userspace processes are now thawed and ready for running. */
	HIBEVENT_KERN2_HIBERNATION_PROCESSES_THAW_DONE,

	/* Second-stage kernel has declared hibernation restore finished. */
	HIBEVENT_KERN2_HIBERNATION_EXIT,

	/* Number of valid hibernation events. */
	HIBEVENT_KERN2_MAX
};


void bootstat_reset_hibernation_stats(void);

/*
 * Record the timestamp of the currently running SoC clock for
 * the given hibernation/boot event.
 */
void bootstat_record_kernel1_event(const enum hibernation_kernel1_event e);
void bootstat_record_kernel2_event(const enum hibernation_kernel2_event e);
