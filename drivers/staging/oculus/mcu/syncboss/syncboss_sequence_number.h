/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_SEQUENCE_NUMBER_H
#define _SYNCBOSS_SEQUENCE_NUMBER_H

#include "syncboss.h"

void syncboss_sequence_number_reset_locked(struct syncboss_dev_data *devdata);
int syncboss_sequence_number_allocate_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data, uint8_t *seq);
int syncboss_sequence_number_release_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data, uint8_t seq);
void syncboss_sequence_number_release_client_locked(
	struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data);

#endif
