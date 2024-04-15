// SPDX-License-Identifier: GPL-2.0
#include "syncboss_spi_fastpath.h"

static int fastpath_transfer_one_message(struct spi_master *master,
				    struct spi_message *msg)
{
	struct spi_transfer *xfer;
	int ret = 0;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		ret = master->transfer_one(master, msg->spi, xfer);
		if (ret < 0) {
			dev_err(&msg->spi->dev,
				"SPI fastpath transfer_one failed: %d\n", ret);
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

int spi_fastpath_transfer(struct spi_device *spi, struct spi_message *msg)
{
	int status;
	struct spi_master *master = spi->master;

	msg->spi = spi;
	master->cur_msg = msg;

	status = fastpath_transfer_one_message(master, master->cur_msg);
	if (status) {
		dev_err(&master->dev,
			"fastpath transfer_one_message failed\n");
	}

	if (!status)
		status = msg->status;

	return status;
}
