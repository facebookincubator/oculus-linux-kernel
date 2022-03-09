/* (c) Facebook, Inc. and its affiliates. Confidential and proprietary. */
#include "stp_router.h"

#include "stp_master.h"
#include "stp_master_common.h"
#include "stp_pipeline.h"
#include "stp_os.h"

struct stp_router_data_t {
	struct pipeline_type rx_pipeline;
	bool connected;
	bool slave_connected;
	bool valid_session;
} stp_router_data_t;

struct stp_router_metadata_t {
	uint8_t magic;
	uint8_t client_id;
	uint16_t size;   /* TODO: union with notification */
} stp_router_metadata_t;

#define STP_ROUTER_MAGIC_DATA   0xDA
#define STP_ROUTER_MAGIC_NOTIFICATION   0x80

enum {
	STP_ROUTER_NOTIFICATION_CONNECT = 0xC088,
	STP_ROUTER_NOTIFICATION_DISCONNECT = 0xDC08,
};

static struct stp_router_data_t stp_router_data[STP_ROUTER_NUM_HANDLES];
static STP_LOCK_TYPE stp_router_lock;

/* Initialize with false by default */
static bool stp_valid_session;

static struct stp_router_wait_signal_table wait_signal_table;

static void do_stp_open(void)
{
	int ret;

	do {
		ret = stp_open();
		if (ret == STP_SUCCESS)
			break;

		msleep(100);
	} while (1);

	stp_valid_session = true;
}

void stp_router_resume_all_reads(void)
{
	int index;

	for (index = 0; index < STP_ROUTER_NUM_HANDLES; index++) {
		wait_signal_table.signal_read(index);
	}
}

void stp_router_invalidate_session(void)
{
	int index;

	STP_LOCK(stp_router_lock);

	for (index = 0; index < STP_ROUTER_NUM_HANDLES; index++) {
		stp_router_data[index].valid_session = false;
		stp_router_data[index].slave_connected = false;
		stp_router_data[index].connected = false;
	}

	stp_valid_session = false;

	stp_router_resume_all_reads();

	STP_LOG_INFO("STP Router - invalidate session %d\n", stp_valid_session);

	STP_UNLOCK(stp_router_lock);
}

int32_t stp_router_check_for_rw_errors(stp_router_handle handle)
{
	int32_t ret;

	STP_ASSERT(handle < STP_ROUTER_NUM_HANDLES, "Invalid parameter(s)");

	if (!stp_router_data[handle].valid_session)
		return STP_ERROR_INVALID_SESSION;
	else if (!stp_router_data[handle].slave_connected)
		return STP_ERROR_SLAVE_NOT_CONNECTED;
	else if (!stp_router_data[handle].connected)
		return STP_ERROR_MASTER_NOT_CONNECTED;

	ret = stp_check_for_rw_errors();

	if (ret == STP_ERROR_INVALID_SESSION)
		STP_LOG_INFO("STP Router INVALID SESSION\n");

	return ret;
}

static bool stp_router_valid_magic(uint8_t magic)
{
	if ((magic == STP_ROUTER_MAGIC_DATA) ||
		(magic == STP_ROUTER_MAGIC_NOTIFICATION))
		return true;
	else
		return false;
}

static int stp_router_send_notification(stp_router_handle handle,
	uint16_t notification)
{
	struct stp_router_metadata_t metadata;
	unsigned int size_written_metadata;
	int32_t ret = STP_ROUTER_SUCCESS;

	STP_ASSERT(handle < STP_ROUTER_NUM_HANDLES, "Invalid parameter(s)");

	metadata.size = notification;
	metadata.client_id = handle;
	metadata.magic = STP_ROUTER_MAGIC_NOTIFICATION;

	ret = stp_write((const U8 *)&metadata, sizeof(metadata),
		&size_written_metadata);
	/* TODO: call stp_write() */
	STP_ASSERT(size_written_metadata == sizeof(metadata),
		"stp_write_nb error!");

	return ret;
}

static int stp_router_receive_notification(stp_router_handle handle,
	uint16_t notification)
{
	int32_t ret = STP_ROUTER_SUCCESS;

	STP_ASSERT(handle < STP_ROUTER_NUM_HANDLES, "Invalid parameter(s)");

	STP_LOCK(stp_router_lock);

	switch (notification) {
	case STP_ROUTER_NOTIFICATION_CONNECT:
		STP_LOG_INFO("STP Router notification: Slave connected %d\n",
			handle);
		stp_router_data[handle].slave_connected = true;
		break;
	case STP_ROUTER_NOTIFICATION_DISCONNECT:
		STP_LOG_INFO("STP Router notification: Slave disconnected %d\n",
			handle);
		stp_router_data[handle].slave_connected = false;
		break;
	default:
		STP_LOG_ERROR("STP Router unknown notification: %d!\n",
			notification);
		break;
	}

	STP_UNLOCK(stp_router_lock);

	return ret;
}

static int32_t stp_router_discard_data_from_stp(stp_router_handle handle,
	uint32_t size)
{
#define STP_ROUTER_DISCARD_DATA_BUFFER_LEN 128
	static uint8_t buffer[STP_ROUTER_DISCARD_DATA_BUFFER_LEN];
	uint32_t len_to_read;
	unsigned int len_read;
	int32_t ret = STP_ROUTER_SUCCESS;
	bool log_once = false;

	if (!log_once) {
		STP_LOG_INFO("STP Router discard data %d client_id=%d\n",
			size, handle);
		log_once = true;
	}

	do {
		if (size > STP_ROUTER_DISCARD_DATA_BUFFER_LEN)
			len_to_read = STP_ROUTER_DISCARD_DATA_BUFFER_LEN;
		else
			len_to_read = size;

		ret = stp_read((uint8_t *)buffer, len_to_read, &len_read);
		if (ret != STP_SUCCESS) {
			STP_LOG_ERROR("STP Router discard data error!\n");
			break;
		}

		size -= len_read;

	} while (size > 0);

	return ret;
}

static void stp_router_stp_callback(int stp_event)
{
	switch (stp_event) {
	case STP_EVENT_INIT:
		stp_router_invalidate_session();
		break;
	default:
		STP_LOG_ERROR("stp_router_stp_callback unknown event %d",
			stp_event);
		break;
	}
}

int32_t stp_router_init(struct stp_router_init_t *init)
{
	int i;

	STP_ASSERT(init && init->wait_signal, "Invalid parameter(s)");

	STP_LOCK_INIT(stp_router_lock);

	for (i = 0; i < STP_ROUTER_NUM_HANDLES; i++) {
		stp_router_data[i].connected = false;
		stp_router_data[i].slave_connected = false;
	}

	wait_signal_table = *(init->wait_signal);

	stp_set_callback(&stp_router_stp_callback);

	return STP_ROUTER_SUCCESS;
}

int32_t stp_router_open(stp_router_handle *handle,
	struct stp_router_open_t *obj)
{
	uint8_t client_id = 0;
	int32_t ret = STP_ROUTER_SUCCESS;

	STP_ASSERT(handle && obj && obj->rx_buffer && obj->rx_buffer_size,
		"Invalid parameter(s)");
	STP_ASSERT(obj->client_id < STP_ROUTER_NUM_HANDLES,
		"Invalid parameter(s)");

	ret = stp_check_for_rw_errors();
	if (ret != STP_SUCCESS)
		return ret;

	STP_LOCK(stp_router_lock);

	client_id = obj->client_id;

	if (stp_router_data[client_id].connected) {
		STP_LOG_ERROR("STP Router error: client %d already connected\n",
			client_id);
		STP_UNLOCK(stp_router_lock);
		return STP_ERROR_MASTER_ALREADY_OPEN;
	}

	stp_router_data[client_id].connected = true;
	*handle = client_id;
	stp_router_data[client_id].valid_session = true;

	stp_pl_init(&stp_router_data[client_id].rx_pipeline, obj->rx_buffer,
		obj->rx_buffer_size);

	STP_LOG_INFO("STP Router open client ID %d\n", client_id);

	ret = stp_router_send_notification(*handle,
		STP_ROUTER_NOTIFICATION_CONNECT);

	STP_UNLOCK(stp_router_lock);

	return ret;
}

int32_t stp_router_close(stp_router_handle handle)
{
	int32_t ret = STP_ROUTER_SUCCESS;

	STP_ASSERT(handle < STP_ROUTER_NUM_HANDLES, "Invalid parameter(s)");

	if (!stp_router_data[handle].connected)
		return STP_ERROR_INVALID_PARAMETERS;

	STP_LOCK(stp_router_lock);

	stp_router_data[handle].connected = false;

	stp_pl_reset(&stp_router_data[handle].rx_pipeline);

	ret = stp_router_send_notification(handle,
		STP_ROUTER_NOTIFICATION_DISCONNECT);

	STP_UNLOCK(stp_router_lock);

	STP_LOG_INFO("STP Router close client ID %d\n", handle);

	return ret;
}

int32_t stp_router_write(stp_router_handle handle, const uint8_t *buffer,
	uint32_t size, uint32_t *data_size)
{
	struct stp_router_metadata_t metadata;
	unsigned int size_written, size_written_metadata;
	int32_t ret = STP_ROUTER_SUCCESS;
	struct stp_write_object list[2];

	STP_ASSERT(handle < STP_ROUTER_NUM_HANDLES, "Invalid parameter(s)");
	STP_ASSERT(buffer && size && data_size, "Invalid parameter(s)");

	STP_LOCK(stp_router_lock);

	ret = stp_router_check_for_rw_errors(handle);
	if (ret != STP_ROUTER_SUCCESS)
		goto error;

	metadata.size = size;
	metadata.client_id = (uint8_t)handle;
	metadata.magic = STP_ROUTER_MAGIC_DATA;

	list[0].buffer = (const U8 *)&metadata;
	list[0].size = sizeof(metadata);
	list[0].data_size = &size_written_metadata;

	list[1].buffer = buffer;
	list[1].size = size;
	list[1].data_size = &size_written;

	ret = stp_write_list(list, 2);
	if (ret == STP_SUCCESS)
		*data_size = size_written;

error:
	STP_UNLOCK(stp_router_lock);

	return ret;
}

/* Read data non-blocking mode */
static int32_t stp_router_read_nb_private(stp_router_handle handle,
	uint8_t *buffer, uint32_t buffer_size, uint32_t *data_size)
{
	int32_t ret = STP_ROUTER_SUCCESS;

	/* No locking since this is done by the caller(s) */

	ret = stp_router_check_for_rw_errors(handle);
	if (ret != STP_SUCCESS) {
		STP_LOG_ERROR("stp_router_read_nb_private error %d\n", ret);
		return ret;
	}

	stp_pl_get_data_nb(&stp_router_data[handle].rx_pipeline,
		buffer, buffer_size, data_size);

	return STP_ROUTER_SUCCESS;
}

/* Read data blocking mode */
int32_t stp_router_read(stp_router_handle handle, uint8_t *buffer,
	uint32_t buffer_size, uint32_t *data_size)
{
	int32_t ret = STP_ROUTER_SUCCESS;
	uint8_t *p;
	uint32_t remaining_data;
	uint32_t data_read;

	STP_ASSERT(handle < STP_ROUTER_NUM_HANDLES, "Invalid parameter(s)");
	STP_ASSERT(buffer && buffer_size && data_size, "Invalid parameter(s)");

	STP_LOCK(stp_router_lock);

	ret = stp_router_check_for_rw_errors(handle);
	if (ret != STP_ROUTER_SUCCESS) {
		STP_UNLOCK(stp_router_lock);
		goto error;
	}

	p = buffer;
	remaining_data = buffer_size;

	while (true) {
		ret = stp_router_read_nb_private(handle, p, remaining_data,
			&data_read);
		if (ret != STP_ROUTER_SUCCESS)
			break;

		if (data_read > 0) {
			p += data_read;
			remaining_data -= data_read;
		}

		if (remaining_data == 0)
			break;

		STP_UNLOCK(stp_router_lock);

		ret = wait_signal_table.wait_read(handle);
		if (ret < 0) {
			/*
			 * check if the error should be returned
			 * back to upper layer
			 */
			if (wait_signal_table.check_error(ret))
				goto error;

			ret = stp_router_check_for_rw_errors(handle);

			if (ret == STP_ROUTER_SUCCESS)
				ret = STP_ERROR_IO_INTRERRUPT;
			goto error;
		} else {
			ret = stp_router_check_for_rw_errors(handle);
			if (ret < 0)
				goto error;
		}

		STP_LOCK(stp_router_lock);
	}

	*data_size = buffer_size;

	STP_UNLOCK(stp_router_lock);

error:
	return ret;
}

/* Read data from STP into client RX pipeline */
static int32_t stp_router_read_data_from_stp(stp_router_handle handle,
	struct pipeline_type *pl,
	uint32_t size, uint32_t *read_size)
{
	unsigned int av_space;
	unsigned int read_from_stp;
	unsigned int discard_data = 0;
	int32_t ret = STP_ROUTER_SUCCESS;
	bool log_once = false;

	STP_ASSERT(pl && size && read_size, "Invalid parameter(s)");

	STP_LOCK(pl->lock);

	stp_pl_get_available_space(pl, &av_space);
	if (av_space < size) {
		if (!log_once) {
			STP_LOG_ERROR("STP Router dropping data ch:%d size: %d",
				handle, size);
			log_once = true;
		}
		_stp_debug.router_drops_full_pipeline++;

		discard_data = size - av_space;
		size = av_space;
	}

	if (pl->size - pl->tail >= size) {
		ret = stp_read(pl->buffer + pl->tail, size, &read_from_stp);

		if (ret != STP_SUCCESS)
			goto error;

		pl->tail += size;

		if (pl->tail == pl->size)
			pl->tail = 0;
	} else {
		unsigned int len = pl->size - pl->tail;

		ret = stp_read(pl->buffer + pl->tail, len, &read_from_stp);
		if (ret != STP_SUCCESS)
			goto error;

		ret = stp_read(pl->buffer, size - len, &read_from_stp);
		if (ret != STP_SUCCESS)
			goto error;

		pl->tail = size - len;
	}

	/* TODO: notify the client if discard data? */
	if (discard_data > 0) {
		if (!log_once) {
			STP_LOG_ERROR("STP Router dropping data ch:%d size:%d",
				discard_data, handle);
			log_once = true;
		}
		_stp_debug.router_drops_full_pipeline++;
		stp_router_discard_data_from_stp(handle, discard_data);
	}

	*read_size = size;

error:
	STP_UNLOCK(pl->lock);

	return ret;
}

static int32_t stp_router_get_data_from_stp(stp_router_handle handle,
	uint32_t size_to_read)
{
	int32_t ret = STP_ROUTER_SUCCESS;
	uint32_t size_read;

	STP_LOCK(stp_router_lock);

	if (handle >= STP_ROUTER_NUM_HANDLES) {
		STP_LOG_ERROR("STP Router error: bad client_id %d!\n",
			handle);
		ret = STP_ROUTER_SUCCESS;
		goto error;
	}

	if (!stp_router_data[handle].slave_connected) {
		STP_LOG_ERROR(
			"STP Router error: Slave not connected: %d\n",
			handle);
		stp_router_discard_data_from_stp(handle, size_to_read);
		ret = STP_ROUTER_SUCCESS;
		goto error;
	}

	do {
		ret = stp_router_read_data_from_stp(handle,
			&stp_router_data[handle].rx_pipeline,
			size_to_read, &size_read);
		if (ret != STP_ROUTER_SUCCESS) {
			STP_LOG_INFO("stp_router_read_data_from_stp error %d\n",
				ret);
			break;
		}

		if (size_read > 0)
			wait_signal_table.signal_read(handle);

		size_to_read -= size_read;
	} while (size_to_read > 0);

error:
	STP_UNLOCK(stp_router_lock);

	return ret;
}

/* stp_router transaction entry. Should be called from a separate thread */
int32_t stp_router_transaction_thread(void)
{
	int32_t ret = STP_ROUTER_SUCCESS;
	struct stp_router_metadata_t metadata;
	unsigned int read_from_stp;
	uint16_t client_id;

	if (!stp_valid_session) {
		do_stp_open();
		return ret;
	}

	do {
		ret = stp_read((uint8_t *)&metadata,
			sizeof(metadata), &read_from_stp);
		if (ret != STP_SUCCESS)
			break;

		if (!stp_router_valid_magic(metadata.magic)) {
			STP_LOG_ERROR("STP Router error: bad magic: %d!\n",
				metadata.magic);
			break;
		}

		client_id = metadata.client_id;

		switch (metadata.magic) {
		case STP_ROUTER_MAGIC_DATA:
			ret = stp_router_get_data_from_stp(client_id,
				metadata.size);
			break;
		case STP_ROUTER_MAGIC_NOTIFICATION:
			ret = stp_router_receive_notification(client_id,
				metadata.size);
			break;
		default:
			STP_LOG_ERROR("STP Router error, bad magic 0x%02x!\n",
				metadata.magic);
			break;
		}

	} while (false);

	return ret;
}

int32_t stp_router_debug(char *buf, int size)
{
	int i;
	int ret = 0;

	STP_ASSERT(buf && size, "Invalid parameter(s)");

	strcpy(buf, "");
	for (i = 0; i < STP_ROUTER_NUM_HANDLES; i++) {
		ret = scnprintf(buf, PAGE_SIZE,
			"%s\n"
			"Valid session [%d]: %d\n"
			"Master connected [%d]: %d\n"
			"Slave connected [%d]: %d\n",
			buf,
			i, stp_router_data[i].valid_session,
			i, stp_router_data[i].connected,
			i, stp_router_data[i].slave_connected);
	}

	return ret;
}



