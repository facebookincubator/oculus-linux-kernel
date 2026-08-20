#include <stp/common/stp_pipeline.h>
#include <stp/common/stp_common.h>
#include <stp/common/stp_logging.h>

/* Initialize a STP pipeline */
void stp_pl_init(PL_TYPE *pl, uint8_t *buffer, uint32_t size)
{
	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(buffer, "Invalid buffer pointer");
	STP_ASSERT(size, "Invalid size parameter");

	pl->buffer = buffer;
	pl->size = size;
	// The inital state when there is no data
	pl->head = 0;
	pl->tail = 0;
}

/* Initialize a STP pipeline lock*/
void stp_pl_init_lock(PL_TYPE *pl)
{
	STP_ASSERT(pl, "Invalid parameter(s)");

	STP_LOCK_INIT(pl->lock);
}

/* De-Initialize a STP pipeline */
void stp_pl_deinit(PL_TYPE *pl)
{
	STP_ASSERT(pl, "Invalid parameter(s)");
}

/* De-Initialize a STP pipeline lock */
void stp_pl_deinit_lock(PL_TYPE *pl)
{
	STP_ASSERT(pl, "Invalid parameter(s)");

	STP_LOCK_DEINIT(pl->lock);
}

/* Check if the pipeline has been initialized*/
bool stp_pl_check_initialized(PL_TYPE *pl)
{
	STP_ASSERT(pl, "Invalid pl pointer");
	return pl->size != 0;
}

/* Get the size of data available in the pipeline */
static void stp_pl_get_data_size_rel(PL_TYPE *pl, const uint32_t rel_head,
				     const uint32_t rel_tail, uint32_t *len)
{
	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(len, "Invalid len pointer");

	if (!stp_pl_check_initialized(pl)) {
		STP_LOG_ERROR("Pipeline uninitialized. Assigned size as 0");
		*len = 0;
		return;
	}

	STP_ASSERT(rel_head < pl->size, "Invalid head");
	STP_ASSERT(rel_tail < pl->size, "Invalid tail");

	if (rel_head == rel_tail) {
		*len = 0;
	} else if (rel_head < rel_tail) {
		*len = rel_tail - rel_head;
	} else {
		*len = pl->size - rel_head;
		*len += rel_tail;
	}
}

/* Get the size of data available in the pipeline */
void stp_pl_get_data_size(PL_TYPE *pl, uint32_t *len)
{
	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(len, "Invalid len pointer");

	stp_pl_get_data_size_rel(pl, pl->head, pl->tail, len);
}

/* Get the size of the pipeline */
void stp_pl_get_size(PL_TYPE *pl, uint32_t *len)
{
	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(len, "Invalid len pointer");

	*len = pl->size;
}

/* Get data from pipeline, without updating the head
 * The update is done after confirmation of data received
 * The size is expected to be less or equal with data available
 */
static void stp_pl_get_data_no_update_rel(PL_TYPE *pl, const uint32_t rel_head,
					  const uint32_t rel_tail,
					  uint8_t *buffer, uint32_t size,
					  uint32_t *new_head)
{
	uint32_t data_size;

	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(buffer, "Invalid buffer pointer");
	STP_ASSERT(new_head, "Invalid new_head pointer");

	stp_pl_get_data_size_rel(pl, rel_head, rel_tail, &data_size);
	STP_ASSERT(data_size >= size, "Not enough data");

	*new_head = rel_head;

	if (rel_head < rel_tail) {
		uint8_t *p = pl->buffer + rel_head;

		memcpy(buffer, p, size);
		*new_head += size;
	} else {
		uint32_t len = pl->size - rel_head;

		if (len >= size) {
			uint8_t *p = pl->buffer + rel_head;

			memcpy(buffer, p, size);
			*new_head = rel_head + size;

			/* new_head is not the actual atomic index
			 * which the other process could see. So it is
			 * safe for it to temporarily hold an invalid
			 * index.
			 */
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

static void stp_pl_get_data_no_update(PL_TYPE *pl, uint8_t *buffer,
				      uint32_t size, uint32_t *new_head)
{
	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(buffer, "Invalid buffer pointer");
	STP_ASSERT(new_head, "Invalid new_head pointer");

	stp_pl_get_data_no_update_rel(pl, pl->head, pl->tail, buffer, size,
				      new_head);
}

void stp_pl_get_data(PL_TYPE *pl, uint8_t *buffer, uint32_t buffer_size)
{
	uint32_t new_head;

	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(buffer, "Invalid buffer pointer");
	STP_ASSERT(buffer_size, "Invalid buffer_size parameter");

	stp_pl_get_data_no_update(pl, buffer, buffer_size, &new_head);
	pl->head = new_head;
}

/* Get the size of available space in pipeline */
void stp_pl_get_available_space(PL_TYPE *pl, uint32_t *size)
{
	uint32_t len;
	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(size, "Invalid size pointer");

	// TODO: Make this an STP_ASSERT once T158902664 is fixed.
	if (pl->size == 0) {
		*size = 0;
		return;
	}

	stp_pl_get_data_size(pl, &len);
	// Available size is decremented by 1, to avoid head = tail
	// head = tail only when pipeline is empty only
	*size = pl->size - len - 1;
}

void stp_pl_add_data(PL_TYPE *pl, const uint8_t *buffer, uint32_t buffer_size)
{
	uint32_t av_space;

	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(buffer, "Invalid buffer pointer");
	STP_ASSERT(buffer_size, "Invalid buffer_size pointer");

	stp_pl_get_available_space(pl, &av_space);
	STP_ASSERT(av_space >= buffer_size, "Not enough space");

	uint32_t tmp_tail = pl->tail;

	if (pl->size - tmp_tail >= buffer_size) {
		memcpy(pl->buffer + tmp_tail, buffer, buffer_size);

		/* Use a temporary variable when calculating the new tail,
		 * in order to avoid the other side seeing an invalid
		 * "pl->tail == pl->size" value.
		 */
		tmp_tail += buffer_size;

		if (tmp_tail == pl->size)
			tmp_tail = 0;

	} else {
		uint32_t len = pl->size - tmp_tail;

		memcpy(pl->buffer + tmp_tail, buffer, len);
		memcpy(pl->buffer, buffer + len, buffer_size - len);
		tmp_tail = buffer_size - len;
	}
	pl->tail = tmp_tail;

	STP_ASSERT(pl->tail < pl->size, "Invalid pipeline tail");
}

/* Check if pipeline is empty */
void stp_pl_is_empty(PL_TYPE *pl, bool *is_empty)
{
	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(is_empty, "Invalid is_empty pointer");

	*is_empty = (pl->head == pl->tail);
}

// From here on, pipeline functions should lock inside them
// TBD: Migrate the above functions to this

/* Reset pipeline */
void stp_pl_reset(PL_TYPE *pl)
{
	STP_ASSERT(pl, "Invalid parameter(s)");

	STP_LOCK(pl->lock);
	pl->head = 0;
	pl->tail = 0;
	STP_UNLOCK(pl->lock);
}

void stp_pl_get_percentage_filled(PL_TYPE *pl, uint32_t *percentage)
{
	uint32_t len;

	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(percentage, "Invalid percentage pointer");

	stp_pl_get_data_size(pl, &len);

	*percentage = (100 * len) / pl->size;
}

int32_t stp_pl_get_data_nb(PL_TYPE *pl, uint8_t *buffer, uint32_t buffer_size,
			   uint32_t *data_size)
{
	uint32_t data_av;

	STP_ASSERT(pl, "Invalid pl pointer");
	STP_ASSERT(buffer, "Invalid buffer pointer");
	STP_ASSERT(data_size, "Invalid data_size pointer");

	STP_LOCK(pl->lock);

	*data_size = buffer_size;

	stp_pl_get_data_size(pl, &data_av);

	if (data_av < buffer_size)
		*data_size = data_av;

	if (*data_size > 0)
		stp_pl_get_data(pl, buffer, *data_size);

	STP_UNLOCK(pl->lock);

	return STP_SUCCESS;
}
