/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This file is released under the GPL v2 or later.
 */

#ifndef _RT600_CTRL_H_
#define _RT600_CTRL_H_

enum rt600_boot_state {
	normal = 0,
	flashing,
	soc_active
};

int rt600_event_register(struct notifier_block *nb);
int rt600_event_unregister(struct notifier_block *nb);

#endif /* _RT600_CTRL_H_ */
