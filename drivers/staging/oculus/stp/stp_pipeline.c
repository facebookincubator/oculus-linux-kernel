/*
 * SPI STP Pipeline code
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

#include "stp_pipeline.h"
#include "stp_master_common.h"


/* Initialize a STP pipeline */
void
stp_pl_init(PL_TYPE *pl, U8 *buffer, unsigned int size)
{
	STP_ASSERT(pl && buffer && size, "Invalid parameter(s)");

	pl->buffer = buffer;
	pl->size = size;

	/* The inital state when there is no data */
	pl->head = 0;
	pl->tail = 0;

	STP_LOCK_INIT(pl->lock);
}

/* Get the size of data available in the pipeline */
void
stp_pl_get_data_size_rel(PL_TYPE *pl, UINT rel_head, UINT *len)
{
	STP_ASSERT(pl && len, "Invalid parameter(s)");

	if (rel_head == pl->tail) {
		*len = 0;
	} else if (rel_head < pl->tail) {
		*len = pl->tail - rel_head;
	} else {
		*len = pl->size - rel_head;
		*len += pl->tail;
	}
}

/* Get the size of data available in the pipeline */
void
stp_pl_get_data_size(PL_TYPE *pl, UINT *len)
{
	STP_ASSERT(pl && len, "Invalid parameter(s)");

	stp_pl_get_data_size_rel(pl, pl->head, len);
}

/* Get data from pipeline, without updating the head
 * The update is done after confirmation of data received
 * The size is expected to be less or equal with data available
 */
void
stp_pl_get_data_no_update_rel(PL_TYPE *pl, UINT rel_head,
	U8 *buffer, UINT size, UINT *new_head)
{
	unsigned int data_size;

	STP_ASSERT(pl && buffer && new_head, "Invalid parameter(s)");

	stp_pl_get_data_size_rel(pl, rel_head, &data_size);
	STP_ASSERT(data_size >= size, "Not enough data");

	*new_head = rel_head;

	if (rel_head < pl->tail) {
		U8 *p = pl->buffer + rel_head;

		memcpy(buffer, p, size);
		*new_head += size;
	} else {
		unsigned int len = pl->size - rel_head;

		if (len >= size) {
			U8 *p = pl->buffer + rel_head;

			memcpy(buffer, p, size);
			*new_head = rel_head + size;
			if (*new_head == pl->size)
				*new_head = 0;
		} else {
			memcpy(buffer, pl->buffer + rel_head, len);
			memcpy(buffer + len, pl->buffer, size - len);
			*new_head = size - len;
		}
	}
}

/* Get data from pipeline, without updating the head
 * The update is done after confirmation of data received
 * The size is expected to be less or equal with data available
 */
void
stp_pl_get_data_no_update(PL_TYPE *pl, U8 *buffer, UINT size, UINT *new_head)
{
	STP_ASSERT(pl && buffer && new_head, "Invalid parameter(s)");

	stp_pl_get_data_no_update_rel(pl, pl->head, buffer, size, new_head);
}

void
stp_pl_get_data(PL_TYPE *pl, U8 *buffer, UINT buffer_size)
{
	unsigned int new_head;

	STP_ASSERT(pl && buffer && buffer_size, "Invalid parameter(s)");

	stp_pl_get_data_no_update(pl, buffer, buffer_size, &new_head);

	pl->head = new_head;

	{
		unsigned int len;

		stp_pl_get_data_size(pl, &len);
		stp_ie_record(STP_IE_GET_DATA_RX_PIPELINE, len);
	}
}

/* Get the size of available space in pipeline */
void
stp_pl_get_available_space(PL_TYPE *pl, unsigned int *size)
{
	unsigned int len;

	STP_ASSERT(pl && size, "Invalid parameter(s)");

	stp_pl_get_data_size(pl, &len);
	/*
	 *Available size is decremented by 1, to avoid head = tail
	 * head = tail only when pipeline is empty only
	 */
	*size = pl->size - len - 1;
}

void
stp_pl_add_data(PL_TYPE *pl, const U8 *buffer, UINT buffer_size)
{
	unsigned int av_space;

	STP_ASSERT(pl && buffer && buffer_size, "Invalid parameter(s)");

	stp_pl_get_available_space(pl, &av_space);
	STP_ASSERT(av_space >= buffer_size, "Not enough space");

	if (pl->size - pl->tail > buffer_size) {
		memcpy(pl->buffer + pl->tail, buffer, buffer_size);
		pl->tail += buffer_size;

		if (pl->tail == pl->size)
			pl->tail = 0;

	} else {
		unsigned int len = pl->size - pl->tail;

		memcpy(pl->buffer + pl->tail, buffer, len);
		memcpy(pl->buffer, buffer + len, buffer_size - len);
		pl->tail = buffer_size - len;
	}

	STP_ASSERT(pl->tail < pl->size, "Invalid pipeline tail");
}

/* Check if pipeline is empty */
void
stp_pl_is_empty(PL_TYPE *pl, bool *is_empty)
{
	STP_ASSERT(pl && is_empty, "Invalid parameter(s)");

	*is_empty = pl->head == pl->tail;
}

/* Update pipeline head (after data received confirmation */
void
stp_pl_update_head(PL_TYPE *pl, unsigned int head)
{
	STP_ASSERT(pl, "Invalid parameter(s)");

	STP_ASSERT(head < pl->size, "Invalid pipeline head");

	pl->head = head;

	{
		unsigned int len;

		stp_pl_get_data_size(pl, &len);
		stp_ie_record(STP_IE_GET_DATA_RX_PIPELINE, len);
	}
}

/* Check if pipeline is empty */
void
stp_pl_is_empty_rel(PL_TYPE *pl, unsigned int rel_head, bool *is_empty)
{
	STP_ASSERT(pl && is_empty, "Invalid parameter(s)");

	*is_empty = (rel_head == pl->tail);
}

/* Start migrating pipeline functions to lock inside them */

void stp_pl_reset(PL_TYPE *pl)
{
	STP_LOCK(pl->lock);

	pl->head = 0;
	pl->tail = 0;

	STP_UNLOCK(pl->lock);
}

void
stp_pl_get_data_nb(PL_TYPE *pl, U8 *buffer, UINT buffer_size, UINT *data_size)
{
	unsigned int data_av;

	STP_ASSERT(pl && buffer && data_size, "Invalid parameter(s)");

	STP_LOCK(pl->lock);

	*data_size = buffer_size;

	stp_pl_get_data_size(pl, &data_av);

	if (data_av < buffer_size)
		*data_size = data_av;

	if (*data_size > 0)
		stp_pl_get_data(pl, buffer, *data_size);

	STP_UNLOCK(pl->lock);
}

