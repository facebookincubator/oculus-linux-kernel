// SPDX-License-Identifier: GPL-2.0
#include "syncboss_devfs_clients.h"

#include "syncboss_debugfs.h"
#include "syncboss_sequence_number.h"

void syncboss_devfs_clients_init(struct syncboss_devfs_clients *clients,
	struct device *dev, struct syncboss_seq *seq,
	struct syncboss_debugfs *debugfs)
{
	clients->dev = dev;
	clients->seq = seq;
	clients->debugfs = debugfs;
	clients->client_data_index = 0;
	INIT_LIST_HEAD(&clients->client_data_list);
}

int syncboss_devfs_client_create_locked(struct syncboss_devfs_clients *clients,
		struct file *file, struct task_struct *task,
		struct syncboss_devfs_client **client)
{
	struct device *dev = clients->dev;
	struct syncboss_devfs_client *data;
	int status;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	INIT_LIST_HEAD(&data->list_entry);
	data->file = file;
	data->task = task;
	data->index = clients->client_data_index++;

	status = syncboss_sequence_number_client_create_locked(clients->seq,
				&data->seq, task);
	if (status) {
		dev_err(dev, "failed to init sequence number client data: %d", status);
		goto error;
	}

	status = syncboss_debugfs_devfs_client_add_locked(clients->debugfs, data);
	if (status && status != -ENODEV)
		dev_warn(dev, "failed to add client data to debugfs for %d: %d",
			data->task->pid, status);

	list_add_tail(&data->list_entry, &clients->client_data_list);

	*client = data;

	return 0;

error:
	devm_kfree(dev, data);

	return status;
}

struct syncboss_devfs_client *syncboss_devfs_client_get_locked(
	struct syncboss_devfs_clients *clients, struct file *file)
{
	struct syncboss_devfs_client *data;

	list_for_each_entry(data, &clients->client_data_list, list_entry) {
		if (data->file == file)
			return data;
	}

	return NULL;
}

void syncboss_devfs_client_destroy_locked(struct syncboss_devfs_clients *clients,
	struct syncboss_devfs_client *client)
{
	struct device *dev = clients->dev;

	syncboss_debugfs_devfs_client_remove_locked(clients->debugfs, client);

	list_del(&client->list_entry);

	syncboss_sequence_number_client_destroy_locked(clients->seq, client->seq);

	devm_kfree(dev, client);
}
