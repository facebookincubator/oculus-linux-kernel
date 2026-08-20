/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __LINUX_AW2026_LED_H__
#define __LINUX_AW2026_LED_H__

/* The definition of each time described as shown in figure.
 *        /-----------\
 *       /      |      \
 *      /|      |      |\
 *     / |      |      | \-----------
 *       |hold_time_ms |      |
 *       |             |      |
 * rise_time_ms  fall_time_ms |
 *                       off_time_ms
 */

struct aw2026_platform_data {
	int imax;
	int led_current;
	int rise_time_ms;
	int hold_time_ms;
	int fall_time_ms;
	int off_time_ms;
	struct aw2026_led *led;
	struct kernfs_node *brightness_kn;
};

#endif
