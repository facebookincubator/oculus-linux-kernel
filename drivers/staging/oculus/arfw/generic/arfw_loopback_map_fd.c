// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file arfw_loopback_map_fd.c
 *
 * @brief Helper for mapping a region to loopback_map_dataspace via file descriptor + mmap.
 *
 * @details
 *
 *******************************************************************************/

#include <linux/anon_inodes.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "ar_common.h"
#include "arfw_loopback_map_fd.h"

static int arfw_loopback_map_fd_release(struct inode *inode, struct file *file)
{
	struct arfw_loopback_map_fd *loopback_map_data;

	(void)inode;
	AR_ASSERT(file);

	loopback_map_data = (struct arfw_loopback_map_fd *)file->private_data;

	AR_ASSERT(loopback_map_data);
	AR_ASSERT(loopback_map_data->ops);
	if (loopback_map_data->ops->release)
		loopback_map_data->ops->release(loopback_map_data);

	// since we are being called from fput, we can clear the fd
	arfw_loopback_map_fd_destroy(loopback_map_data);

	return 0;
}

static int arfw_loopback_map_fd_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct arfw_loopback_map_fd *loopback_map_data;

	AR_ASSERT(file);

	loopback_map_data = (struct arfw_loopback_map_fd *)file->private_data;

	AR_ASSERT(loopback_map_data);
	AR_ASSERT(loopback_map_data->ops);
	AR_ASSERT(loopback_map_data->ops->mmap);

	return loopback_map_data->ops->mmap(loopback_map_data, vma);
}

static struct file_operations arfw_loopback_map_fd_fops = {
	.owner = THIS_MODULE,
	.release = arfw_loopback_map_fd_release,
	.mmap = arfw_loopback_map_fd_mmap,
};

int arfw_loopback_map_fd_create(const char *name, void *context, void *vmaddr,
			    const size_t size, struct arfw_loopback_map_fd_ops *ops)
{
	int err, fd;
	struct arfw_loopback_map_fd *map_fd_data;

	if (vmaddr == NULL || size == 0 || ops == NULL || ops->mmap == NULL)
		return -EINVAL;

	map_fd_data = kzalloc(sizeof(*map_fd_data), GFP_KERNEL);
	if (map_fd_data == NULL)
		return -ENOMEM;

	map_fd_data->vmaddr = vmaddr;
	map_fd_data->size = size;
	map_fd_data->context = context;
	map_fd_data->ops = ops;

	// create anonymous file for this aperture mapping
	fd = anon_inode_getfd(name, &arfw_loopback_map_fd_fops, map_fd_data,
			      O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		err = fd;
		goto free_map_fd;
	}

	return fd;

free_map_fd:
	kfree(map_fd_data);
	return err;
}

void arfw_loopback_map_fd_destroy(struct arfw_loopback_map_fd *loopback_map_data)
{
	AR_ASSERT(loopback_map_data);

	loopback_map_data->vmaddr = NULL;
	loopback_map_data->size = 0;
	loopback_map_data->ops = NULL;
	loopback_map_data->context = NULL;
	kfree(loopback_map_data);
}
