#include <stp/common/stp_pipeline.h>
#include <stp/common/stp_common.h>
#include <stp/common/stp_logging.h>

/* Initialize a STP pipeline */
void stp_pl_init(PL_TYPE *pl, uint8_t *buffer, uint32_t size)
{
	STP_ASSERT(pl && buffer && size, "Invalid parameter(s)");

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

/* Get the size of data available in the pipeline */
static void stp_pl_get_data_size_rel(PL_TYPE *pl, uint32_t rel_head,
				     uint32_t *len)
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
void stp_pl_get_data_size(PL_TYPE *pl, uint32_t *len)
{
	STP_ASSERT(pl && len, "Invalid parameter(s)");

	stp_pl_get_data_size_rel(pl, pl->head, len);
}

/* Get the size of the pipeline */
void stp_pl_get_size(PL_TYPE *pl, uint32_t *len)
{
	STP_ASSERT(pl && len, "Invalid parameter(s)");

	*len = pl->size;
}

/* Get data from pipeline, without updating the head
 * The update is done after confirmation of data received
 * The size is expected to be less or equal with data available
 */
static void stp_pl_get_data_no_update_rel(PL_TYPE *pl, uint32_t rel_head,
					  uint8_t *buffer, uint32_t size,
					  uint32_t *new_head)
{
	uint32_t data_size;

	STP_ASSERT(pl && buffer && new_head, "Invalid parameter(s)");

	stp_pl_get_data_size_rel(pl, rel_head, &data_size);
	STP_ASSERT(data_size >= size, "Not enough data");

	*new_head = rel_head;

	if (rel_head < pl->tail) {
		uint8_t *p = pl->buffer + rel_head;

		memcpy(buffer, p, size);
		*new_head += size;
	} else {
		uint32_t len = pl->size - rel_head;

		if (len >= size) {
			uint8_t *p = pl->buffer + rel_head;

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

static void stp_pl_get_data_no_update(PL_TYPE *pl, uint8_t *buffer,
				      uint32_t size, uint32_t *new_head)
{
	STP_ASSERT(pl && buffer && new_head, "Invalid parameter(s)");

	stp_pl_get_data_no_update_rel(pl, pl->head, buffer, size, new_head);
}

void stp_pl_get_data(PL_TYPE *pl, uint8_t *buffer, uint32_t buffer_size)
{
	uint32_t new_head;

	STP_ASSERT(pl && buffer && buffer_size, "Invalid parameter(s)");

	stp_pl_get_data_no_update(pl, buffer, buffer_size, &new_head);
	pl->head = new_head;
}

/* Get the size of available space in pipeline */
void stp_pl_get_available_space(PL_TYPE *pl, uint32_t *size)
{
	uint32_t len;

	STP_ASSERT(pl && size, "Invalid parameter(s)");

	stp_pl_get_data_size(pl, &len);
	// Available size is decremented by 1, to avoid head = tail
	// head = tail only when pipeline is empty only
	*size = pl->size - len - 1;
}

void stp_pl_add_data(PL_TYPE *pl, const uint8_t *buffer, uint32_t buffer_size)
{
	uint32_t av_space;

	STP_ASSERT(pl, "Invalid pl");
	STP_ASSERT(buffer, "Invalid buffer");
	STP_ASSERT(buffer_size, "Invalid buffer_size");

	stp_pl_get_available_space(pl, &av_space);
	STP_ASSERT(av_space >= buffer_size, "Not enough space");

	if (pl->size - pl->tail >= buffer_size) {
		memcpy(pl->buffer + pl->tail, buffer, buffer_size);
		pl->tail += buffer_size;

		if (pl->tail == pl->size)
			pl->tail = 0;
	} else {
		uint32_t len = pl->size - pl->tail;

		memcpy(pl->buffer + pl->tail, buffer, len);
		memcpy(pl->buffer, buffer + len, buffer_size - len);
		pl->tail = buffer_size - len;
	}

	STP_ASSERT(pl->tail < pl->size, "Invalid pipeline tail");
}

/* Check if pipeline is empty */
void stp_pl_is_empty(PL_TYPE *pl, bool *is_empty)
{
	STP_ASSERT(pl && is_empty, "Invalid parameter(s)");

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

	STP_ASSERT(pl && percentage, "Invalid parameter(s)");

	stp_pl_get_data_size(pl, &len);

	*percentage = (100 * len) / pl->size;
}

int32_t stp_pl_get_data_nb(PL_TYPE *pl, uint8_t *buffer, uint32_t buffer_size,
			   uint32_t *data_size)
{
	uint32_t data_av;

	STP_ASSERT(pl && buffer && data_size, "Invalid parameter(s)");

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
