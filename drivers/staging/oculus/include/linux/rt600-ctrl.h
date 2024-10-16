/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef _RT600_CTRL_H_
#define _RT600_CTRL_H_

enum rt600_boot_state {
	normal = 0,
	flashing
};

int rt600_event_register(struct notifier_block *nb);
int rt600_event_unregister(struct notifier_block *nb);

#endif /* _RT600_CTRL_H_ */
