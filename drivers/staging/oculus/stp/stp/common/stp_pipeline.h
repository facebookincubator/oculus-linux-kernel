#ifndef STP_PIPELINE_H
#define STP_PIPELINE_H

#include <stp/common/stp_os.h>

/* TX/RX pipeline */
struct pipeline_type {
	// pipeline buffer
	uint8_t *buffer;
	// buffer size
	_Atomic uint32_t size;
	// head of pipeline (consumer)
	_Atomic unsigned int head;
	// tail of pipeline (producer)
	_Atomic unsigned int tail;

	STP_LOCK_TYPE lock;
};

#define PL_TYPE struct pipeline_type

/* Initialize a STP pipeline */
void stp_pl_init(PL_TYPE *pl, uint8_t *buffer, uint32_t size);

/* De-Initialize a STP pipeline */
void stp_pl_deinit(PL_TYPE *pl);

/* Get the size of data available in the pipeline */
void stp_pl_get_data_size(PL_TYPE *pl, uint32_t *len);

/* Get the size of the pipeline */
void stp_pl_get_size(PL_TYPE *pl, uint32_t *len);

/* At this point we know that we have at least len_buffer in pipeline */
void stp_pl_get_data(PL_TYPE *pl, uint8_t *buffer, uint32_t buffer_size);

/* Get the size of available space in pipeline */
void stp_pl_get_available_space(PL_TYPE *pl, uint32_t *size);

/* At this point we know we have enough space to add data to pipeline */
void stp_pl_add_data(PL_TYPE *pl, const uint8_t *buffer, uint32_t buffer_size);

/* Check if pipeline is empty */
void stp_pl_is_empty(PL_TYPE *pl, bool *is_empty);

/* Get the size of data available in the pipeline */
void stp_pl_get_data_size(PL_TYPE *pl, uint32_t *len);

/* Reset pipeline */
void stp_pl_reset(PL_TYPE *pl);

/* Get the size of data available in the pipeline in percentage 0-100*/
void stp_pl_get_percentage_filled(PL_TYPE *pl, uint32_t *percentage);

/* Get data from pipeline */
int32_t stp_pl_get_data_nb(PL_TYPE *pl, uint8_t *buffer, uint32_t buffer_size,
			   uint32_t *data_size);

#endif
