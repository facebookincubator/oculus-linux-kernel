#include "spi_fastpath.h"
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/delay.h>

int spi_fastpath_init(struct spi_device* spi)
{
	struct spi_master *master = spi->master;
	struct spi_message msg;
	struct spi_message *current_msg_backup = NULL;
	int status = 0;

	spi_message_init(&msg);
	msg.spi = spi;

	current_msg_backup = master->cur_msg;
	master->cur_msg = &msg;

	status = master->prepare_transfer_hardware(master);
	if (status) {
		dev_err(&master->dev,
			"failed to prepare transfer hardware\n");
		goto error;
	}

	// prepare msg
	status = master->prepare_message(master, master->cur_msg);
	if (status) {
		dev_err(&master->dev, "failed to prepare message: %d\n",
			status);
		master->cur_msg->status = status;
		spi_finalize_current_message(master);
		goto error;
	}
	master->cur_msg_prepared = true;

error:
	// Restore current message
	master->cur_msg = current_msg_backup;

	return status;
}

int spi_fastpath_deinit(struct spi_device* spi)
{
	struct spi_master *master = spi->master;
	struct spi_message msg;

	spi_message_init(&msg);
	msg.spi = spi;
	master->cur_msg = &msg;

	spi_finalize_current_message(master);
	return 0;
}

static int fastpath_transfer_one_message(struct spi_master *master,
				    struct spi_message *msg)
{
	struct spi_transfer *xfer;
	int ret = 0;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		ret = master->transfer_one(master, msg->spi, xfer);
		if (ret < 0) {
			dev_err(&msg->spi->dev,
				"SPI transfer failed: %d\n", ret);
			goto out;
		}

		if (msg->status != -EINPROGRESS)
			goto out;

		msg->actual_length += xfer->len;
	}

out:
	if (msg->status == -EINPROGRESS)
		msg->status = ret;

	if (msg->status && master->handle_err)
		master->handle_err(master, msg);

	return ret;
}

int spi_fastpath_transfer(struct spi_device* spi, struct spi_message* msg)
{
	int status;
	struct spi_master *master = spi->master;
	msg->spi = spi;
	master->cur_msg = msg;

	status = fastpath_transfer_one_message(master, master->cur_msg);
	if (status) {
		dev_err(&master->dev,
			"failed to transfer one message from queue\n");
	}

	if (!status) {
		status = msg->status;
	}
	return status;
}
