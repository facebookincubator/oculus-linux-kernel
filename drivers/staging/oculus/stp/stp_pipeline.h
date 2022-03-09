/*
 * SPI STP Pipeline header
 *
 * Copyright (C) 2020 Eugen Pirvu
 * Copyright (C) 2020 Facebook, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef STP_PIPELINE_H
#define STP_PIPELINE_H

#include "stp_os.h"

/* TX/RX pipeline */
struct pipeline_type {
	// pipeline buffer
	U8 *buffer;
	// buffer size
	unsigned int size;
	// head of pipeline (consumer)
	unsigned int head;
	// tail of pipeline (producer)
	unsigned int tail;

	STP_LOCK_TYPE lock;
};

#define PL_TYPE struct pipeline_type

/* Initialize a STP pipeline */
void stp_pl_init(PL_TYPE *pl, U8 *buffer, unsigned int size);

/* Get the size of data available in the pipeline */
void stp_pl_get_data_size(PL_TYPE *pl, UINT *len);

/* Get data from pipeline, without updating the head */
/* The update is done after confirmation of data received */
void stp_pl_get_data_no_update(PL_TYPE *pl, U8 *buffer,
	UINT size, UINT *new_head);

/* At this point we know that we have at least len_buffer in pipeline */
void stp_pl_get_data(PL_TYPE *pl, U8 *buffer, UINT buffer_size);

/* Get the size of available space in pipeline */
void stp_pl_get_available_space(PL_TYPE *pl, unsigned int *size);

/* At this point we know we have enough space to add data to pipeline */
void stp_pl_add_data(PL_TYPE *pl, const U8 *buffer, UINT buffer_size);

/* Check if pipeline is empty */
void  stp_pl_is_empty(PL_TYPE *pl, bool *is_empty);

/* Update pipeline head (after data received confirmation */
void stp_pl_update_head(PL_TYPE *pl, unsigned int head);

/* Check if pipeline is empty */
void
stp_pl_is_empty_rel(PL_TYPE *pl, unsigned int rel_head, bool *is_empty);

void
stp_pl_get_data_no_update_rel(PL_TYPE *pl, UINT rel_head,
	U8 *buffer, UINT size, UINT *new_head);

/* Get the size of data available in the pipeline */
void
stp_pl_get_data_size_rel(PL_TYPE *pl, UINT rel_head, UINT *len);

/* Reset pipeline */
void stp_pl_reset(PL_TYPE *pl);

/* Get data from pipeline in non-blocking mode */
void
stp_pl_get_data_nb(PL_TYPE *pl, U8 *buffer, UINT buffer_size, UINT *data_size);

#endif
