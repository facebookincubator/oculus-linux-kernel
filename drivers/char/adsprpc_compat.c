// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 */
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/msm_ion.h>

#include "adsprpc_compat.h"
#include "adsprpc_shared.h"

#define COMPAT_FASTRPC_IOCTL_INVOKE \
		_IOWR('R', 1, struct compat_fastrpc_ioctl_invoke)
#define COMPAT_FASTRPC_IOCTL_MMAP \
		_IOWR('R', 2, struct compat_fastrpc_ioctl_mmap)
#define COMPAT_FASTRPC_IOCTL_MUNMAP \
		_IOWR('R', 3, struct compat_fastrpc_ioctl_munmap)
#define COMPAT_FASTRPC_IOCTL_INVOKE_FD \
		_IOWR('R', 4, struct compat_fastrpc_ioctl_invoke_fd)
#define COMPAT_FASTRPC_IOCTL_INIT \
		_IOWR('R', 6, struct compat_fastrpc_ioctl_init)
#define COMPAT_FASTRPC_IOCTL_INVOKE_ATTRS \
		_IOWR('R', 7, struct compat_fastrpc_ioctl_invoke_attrs)
#define COMPAT_FASTRPC_IOCTL_GETPERF \
		_IOWR('R', 9, struct compat_fastrpc_ioctl_perf)
#define COMPAT_FASTRPC_IOCTL_INIT_ATTRS \
		_IOWR('R', 10, struct compat_fastrpc_ioctl_init_attrs)
#define COMPAT_FASTRPC_IOCTL_INVOKE_CRC \
		_IOWR('R', 11, struct compat_fastrpc_ioctl_invoke_crc)
#define COMPAT_FASTRPC_IOCTL_CONTROL \
		_IOWR('R', 12, struct compat_fastrpc_ioctl_control)
#define COMPAT_FASTRPC_IOCTL_MMAP_64 \
		_IOWR('R', 14, struct compat_fastrpc_ioctl_mmap_64)
#define COMPAT_FASTRPC_IOCTL_MUNMAP_64 \
		_IOWR('R', 15, struct compat_fastrpc_ioctl_munmap_64)
#define COMPAT_FASTRPC_IOCTL_GET_DSP_INFO \
		_IOWR('R', 16, struct compat_fastrpc_ioctl_dsp_capabilities)

struct compat_remote_buf {
	compat_uptr_t pv;	/* buffer pointer */
	compat_size_t len;	/* length of buffer */
};

union compat_remote_arg {
	struct compat_remote_buf buf;
	compat_uint_t h;
};

struct compat_fastrpc_ioctl_invoke {
	compat_uint_t handle;	/* remote handle */
	compat_uint_t sc;	/* scalars describing the data */
	compat_uptr_t pra;	/* remote arguments list */
};

struct compat_fastrpc_ioctl_invoke_fd {
	struct compat_fastrpc_ioctl_invoke inv;
	compat_uptr_t fds;	/* fd list */
};

struct compat_fastrpc_ioctl_invoke_attrs {
	struct compat_fastrpc_ioctl_invoke inv;
	compat_uptr_t fds;	/* fd list */
	compat_uptr_t attrs;	/* attribute list */
};

struct compat_fastrpc_ioctl_invoke_crc {
	struct compat_fastrpc_ioctl_invoke inv;
	compat_uptr_t fds;	/* fd list */
	compat_uptr_t attrs;	/* attribute list */
	compat_uptr_t crc;	/* crc list */
};

struct compat_fastrpc_ioctl_mmap {
	compat_int_t fd;	/* ion fd */
	compat_uint_t flags;	/* flags for dsp to map with */
	compat_uptr_t vaddrin;	/* optional virtual address */
	compat_size_t size;	/* size */
	compat_uptr_t vaddrout;	/* dsps virtual address */
};

struct compat_fastrpc_ioctl_mmap_64 {
	compat_int_t fd;	/* ion fd */
	compat_uint_t flags;	/* flags for dsp to map with */
	compat_u64 vaddrin;	/* optional virtual address */
	compat_size_t size;	/* size */
	compat_u64 vaddrout;	/* dsps virtual address */
};

struct compat_fastrpc_ioctl_munmap {
	compat_uptr_t vaddrout;	/* address to unmap */
	compat_size_t size;	/* size */
};

struct compat_fastrpc_ioctl_munmap_64 {
	compat_u64 vaddrout;	/* address to unmap */
	compat_size_t size;	/* size */
};

struct compat_fastrpc_ioctl_init {
	compat_uint_t flags;	/* one of FASTRPC_INIT_* macros */
	compat_uptr_t file;	/* pointer to elf file */
	compat_int_t filelen;	/* elf file length */
	compat_int_t filefd;	/* ION fd for the file */
	compat_uptr_t mem;	/* mem for the PD */
	compat_int_t memlen;	/* mem length */
	compat_int_t memfd;	/* ION fd for the mem */
};

struct compat_fastrpc_ioctl_init_attrs {
	struct compat_fastrpc_ioctl_init init;
	compat_int_t attrs;	/* attributes to init process */
	compat_int_t siglen;	/* test signature file length */
};

struct compat_fastrpc_ioctl_perf {	/* kernel performance data */
	compat_uptr_t  data;
	compat_int_t numkeys;
	compat_uptr_t keys;
};

#define FASTRPC_CONTROL_LATENCY		(1)
struct compat_fastrpc_ctrl_latency {
	compat_uint_t enable;	/* latency control enable */
	compat_uint_t latency;	/* target latency in us */
};

#define FASTRPC_CONTROL_KALLOC		(3)
struct compat_fastrpc_ctrl_kalloc {
	compat_uint_t kalloc_support; /* Remote memory allocation from kernel */
};

struct compat_fastrpc_ctrl_wakelock {
	compat_uint_t enable;	/* wakelock control enable */
};

struct compat_fastrpc_ctrl_pm {
	compat_uint_t timeout;	/* timeout(in ms) for PM to keep system awake*/
};

struct compat_fastrpc_ioctl_control {
	compat_uint_t req;
	union {
		struct compat_fastrpc_ctrl_latency lp;
		struct compat_fastrpc_ctrl_kalloc kalloc;
		struct compat_fastrpc_ctrl_wakelock wp;
		struct compat_fastrpc_ctrl_pm pm;
	};
};

struct compat_fastrpc_ioctl_dsp_capabilities {
	compat_uint_t domain;	/* DSP domain to query capabilities */
	compat_uint_t dsp_attributes[FASTRPC_MAX_DSP_ATTRIBUTES];
};

static int compat_get_fastrpc_ioctl_invoke(
			struct compat_fastrpc_ioctl_invoke_crc __user *inv32,
			struct fastrpc_ioctl_invoke_crc __user **inva,
			unsigned int cmd)
{
	compat_uint_t u, sc;
	compat_size_t s;
	compat_uptr_t p;
	struct fastrpc_ioctl_invoke_crc *inv;
	union compat_remote_arg *pra32;
	union remote_arg *pra;
	int err, len, j;

	err = get_user(sc, &inv32->inv.sc);
	if (err)
		return err;

	len = REMOTE_SCALARS_LENGTH(sc);
	VERIFY(err, NULL != (inv = compat_alloc_user_space(
				sizeof(*inv) + len * sizeof(*pra))));
	if (err)
		return -EFAULT;

	pra = (union remote_arg *)(inv + 1);
	err = put_user(pra, &inv->inv.pra);
	err |= put_user(sc, &inv->inv.sc);
	err |= get_user(u, &inv32->inv.handle);
	err |= put_user(u, &inv->inv.handle);
	err |= get_user(p, &inv32->inv.pra);
	if (err)
		return err;

	pra32 = compat_ptr(p);
	pra = (union remote_arg *)(inv + 1);
	for (j = 0; j < len; j++) {
		err |= get_user(p, &pra32[j].buf.pv);
		err |= put_user(p, (uintptr_t *)&pra[j].buf.pv);
		err |= get_user(s, &pra32[j].buf.len);
		err |= put_user(s, &pra[j].buf.len);
	}

	err |= put_user(NULL, &inv->fds);
	if (cmd != COMPAT_FASTRPC_IOCTL_INVOKE) {
		err |= get_user(p, &inv32->fds);
		err |= put_user(p, (compat_uptr_t *)&inv->fds);
	}
	err |= put_user(NULL, &inv->attrs);
	if ((cmd == COMPAT_FASTRPC_IOCTL_INVOKE_ATTRS) ||
		(cmd == COMPAT_FASTRPC_IOCTL_INVOKE_CRC)) {
		err |= get_user(p, &inv32->attrs);
		err |= put_user(p, (compat_uptr_t *)&inv->attrs);
	}
	err |= put_user(NULL, (compat_uptr_t __user **)&inv->crc);
	if (cmd == COMPAT_FASTRPC_IOCTL_INVOKE_CRC) {
		err |= get_user(p, &inv32->crc);
		err |= put_user(p, (compat_uptr_t __user *)&inv->crc);
	}

	*inva = inv;
	return err;
}

static int compat_get_fastrpc_ioctl_mmap(
			struct compat_fastrpc_ioctl_mmap __user *map32,
			struct fastrpc_ioctl_mmap __user *map)
{
	compat_uint_t u;
	compat_int_t i;
	compat_size_t s;
	compat_uptr_t p;
	int err;

	err = get_user(i, &map32->fd);
	err |= put_user(i, &map->fd);
	err |= get_user(u, &map32->flags);
	err |= put_user(u, &map->flags);
	err |= get_user(p, &map32->vaddrin);
	err |= put_user(p, (uintptr_t *)&map->vaddrin);
	err |= get_user(s, &map32->size);
	err |= put_user(s, &map->size);

	return err;
}

static int compat_get_fastrpc_ioctl_mmap_64(
			struct compat_fastrpc_ioctl_mmap_64 __user *map32,
			struct fastrpc_ioctl_mmap __user *map)
{
	compat_uint_t u;
	compat_int_t i;
	compat_size_t s;
	compat_u64 p;
	int err;

	err = get_user(i, &map32->fd);
	err |= put_user(i, &map->fd);
	err |= get_user(u, &map32->flags);
	err |= put_user(u, &map->flags);
	err |= get_user(p, &map32->vaddrin);
	err |= put_user(p, &map->vaddrin);
	err |= get_user(s, &map32->size);
	err |= put_user(s, &map->size);

	return err;
}

static int compat_put_fastrpc_ioctl_mmap(
			struct compat_fastrpc_ioctl_mmap __user *map32,
			struct fastrpc_ioctl_mmap __user *map)
{
	compat_uptr_t p;
	int err;

	err = get_user(p, &map->vaddrout);
	err |= put_user(p, &map32->vaddrout);

	return err;
}

static int compat_put_fastrpc_ioctl_mmap_64(
			struct compat_fastrpc_ioctl_mmap_64 __user *map32,
			struct fastrpc_ioctl_mmap __user *map)
{
	compat_u64 p;
	int err;

	err = get_user(p, &map->vaddrout);
	err |= put_user(p, &map32->vaddrout);

	return err;
}

static int compat_get_fastrpc_ioctl_munmap(
			struct compat_fastrpc_ioctl_munmap __user *unmap32,
			struct fastrpc_ioctl_munmap __user *unmap)
{
	compat_uptr_t p;
	compat_size_t s;
	int err;

	err = get_user(p, &unmap32->vaddrout);
	err |= put_user(p, &unmap->vaddrout);
	err |= get_user(s, &unmap32->size);
	err |= put_user(s, &unmap->size);

	return err;
}

static int compat_get_fastrpc_ioctl_munmap_64(
			struct compat_fastrpc_ioctl_munmap_64 __user *unmap32,
			struct fastrpc_ioctl_munmap __user *unmap)
{
	compat_u64 p;
	compat_size_t s;
	int err;

	err = get_user(p, &unmap32->vaddrout);
	err |= put_user(p, &unmap->vaddrout);
	err |= get_user(s, &unmap32->size);
	err |= put_user(s, &unmap->size);

	return err;
}

static int compat_get_fastrpc_ioctl_perf(
			struct compat_fastrpc_ioctl_perf __user *perf32,
			struct fastrpc_ioctl_perf __user *perf)
{
	compat_uptr_t p;
	int err;

	err = get_user(p, &perf32->data);
	err |= put_user(p, &perf->data);
	err |= get_user(p, &perf32->keys);
	err |= put_user(p, &perf->keys);

	return err;
}

static int compat_get_fastrpc_ioctl_control(
			struct compat_fastrpc_ioctl_control __user *ctrl32,
			struct fastrpc_ioctl_control __user *ctrl)
{
	compat_uptr_t p;
	int err;

	err = get_user(p, &ctrl32->req);
	err |= put_user(p, &ctrl->req);
	if (p == FASTRPC_CONTROL_LATENCY) {
		err |= get_user(p, &ctrl32->lp.enable);
		err |= put_user(p, &ctrl->lp.enable);
		err |= get_user(p, &ctrl32->lp.latency);
		err |= put_user(p, &ctrl->lp.latency);
	} else if (p == FASTRPC_CONTROL_WAKELOCK) {
		err |= get_user(p, &ctrl32->wp.enable);
		err |= put_user(p, &ctrl->wp.enable);
	} else if (p == FASTRPC_CONTROL_PM) {
		err |= get_user(p, &ctrl32->pm.timeout);
		err |= put_user(p, &ctrl->pm.timeout);
	}

	return err;
}

static int compat_get_fastrpc_ioctl_init(
			struct compat_fastrpc_ioctl_init_attrs __user *init32,
			struct fastrpc_ioctl_init_attrs __user *init,
			unsigned int cmd)
{
	compat_uint_t u;
	compat_uptr_t p;
	compat_int_t i;
	int err;

	err = get_user(u, &init32->init.flags);
	err |= put_user(u, &init->init.flags);
	err |= get_user(p, &init32->init.file);
	err |= put_user(p, &init->init.file);
	err |= get_user(i, &init32->init.filelen);
	err |= put_user(i, &init->init.filelen);
	err |= get_user(i, &init32->init.filefd);
	err |= put_user(i, &init->init.filefd);
	err |= get_user(p, &init32->init.mem);
	err |= put_user(p, &init->init.mem);
	err |= get_user(i, &init32->init.memlen);
	err |= put_user(i, &init->init.memlen);
	err |= get_user(i, &init32->init.memfd);
	err |= put_user(i, &init->init.memfd);

	err |= put_user(0, &init->attrs);
	if (cmd == COMPAT_FASTRPC_IOCTL_INIT_ATTRS) {
		err |= get_user(i, &init32->attrs);
		err |= put_user(i, (compat_uptr_t *)&init->attrs);
	}

	err |= put_user(0, &init->siglen);
	if (cmd == COMPAT_FASTRPC_IOCTL_INIT_ATTRS) {
		err |= get_user(i, &init32->siglen);
		err |= put_user(i, (compat_uptr_t *)&init->siglen);
	}

	return err;
}

static int compat_put_fastrpc_ioctl_get_dsp_info(
		struct compat_fastrpc_ioctl_dsp_capabilities __user *info32,
		struct fastrpc_ioctl_dsp_capabilities __user *info)
{
	compat_uint_t u, *dsp_attr, *dsp_attr_32;
	int err, ii;

	dsp_attr = info->dsp_attributes;
	dsp_attr_32 = info32->dsp_attributes;
	for (ii = 0, err = 0; ii < FASTRPC_MAX_DSP_ATTRIBUTES; ii++) {
		err |= get_user(u, dsp_attr++);
		err |= put_user(u, dsp_attr_32++);
	}

	return err;
}

static int fastrpc_setmode(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	return filp->f_op->unlocked_ioctl(filp, cmd,
					(unsigned long)compat_ptr(arg));
}

static int compat_fastrpc_control(struct file *filp,
		unsigned long arg)
{
	int err = 0;
	struct compat_fastrpc_ioctl_control __user *ctrl32;
	struct fastrpc_ioctl_control __user *ctrl;
	compat_uptr_t p;

	ctrl32 = compat_ptr(arg);
	VERIFY(err, NULL != (ctrl = compat_alloc_user_space(
						sizeof(*ctrl))));
	if (err)
		return -EFAULT;
	VERIFY(err, 0 == compat_get_fastrpc_ioctl_control(ctrl32,
						ctrl));
	if (err)
		return err;
	err = filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_CONTROL,
						(unsigned long)ctrl);
	if (err)
		return err;
	err = get_user(p, &ctrl32->req);
	if (err)
		return err;
	if (p == FASTRPC_CONTROL_KALLOC) {
		err = get_user(p, &ctrl->kalloc.kalloc_support);
		err |= put_user(p, &ctrl32->kalloc.kalloc_support);
	}
	return err;
}

static int compat_fastrpc_getperf(struct file *filp,
		unsigned long arg)
{
	int err = 0;
	struct compat_fastrpc_ioctl_perf __user *perf32;
	struct fastrpc_ioctl_perf *perf;
	compat_uint_t u;
	long ret;

	perf32 = compat_ptr(arg);
	VERIFY(err, NULL != (perf = compat_alloc_user_space(
						sizeof(*perf))));
	if (err)
		return -EFAULT;
	VERIFY(err, 0 == compat_get_fastrpc_ioctl_perf(perf32,
						perf));
	if (err)
		return err;
	ret = filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_GETPERF,
						(unsigned long)perf);
	if (ret)
		return ret;
	err = get_user(u, &perf->numkeys);
	err |= put_user(u, &perf32->numkeys);
	return err;
}

static int compat_fastrpc_get_dsp_info(struct file *filp,
		unsigned long arg)
{
	struct compat_fastrpc_ioctl_dsp_capabilities __user *info32;
	struct fastrpc_ioctl_dsp_capabilities *info;
	compat_uint_t u;
	long ret;
	int err = 0;

	info32 = compat_ptr(arg);
	VERIFY(err, NULL != (info = compat_alloc_user_space(
						sizeof(*info))));
	if (err)
		return -EFAULT;

	err = get_user(u, &info32->domain);
	err |= put_user(u, &info->domain);
	if (err)
		return err;

	ret = filp->f_op->unlocked_ioctl(filp,
			FASTRPC_IOCTL_GET_DSP_INFO,
			(unsigned long)info);
	if (ret)
		return ret;

	err = compat_put_fastrpc_ioctl_get_dsp_info(info32, info);
	return err;
}

long compat_fastrpc_device_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	int err = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_FASTRPC_IOCTL_INVOKE:
	case COMPAT_FASTRPC_IOCTL_INVOKE_FD:
	case COMPAT_FASTRPC_IOCTL_INVOKE_ATTRS:
	case COMPAT_FASTRPC_IOCTL_INVOKE_CRC:
	{
		struct compat_fastrpc_ioctl_invoke_crc __user *inv32;
		struct fastrpc_ioctl_invoke_crc __user *inv;

		inv32 = compat_ptr(arg);
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_invoke(inv32,
							&inv, cmd));
		if (err)
			return err;
		return filp->f_op->unlocked_ioctl(filp,
				FASTRPC_IOCTL_INVOKE_CRC, (unsigned long)inv);
	}
	case COMPAT_FASTRPC_IOCTL_MMAP:
	{
		struct compat_fastrpc_ioctl_mmap __user *map32;
		struct fastrpc_ioctl_mmap __user *map;
		long ret;

		map32 = compat_ptr(arg);
		VERIFY(err, NULL != (map = compat_alloc_user_space(
							sizeof(*map))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_mmap(map32, map));
		if (err)
			return err;
		ret = filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_MMAP,
							(unsigned long)map);
		if (ret)
			return ret;
		VERIFY(err, 0 == compat_put_fastrpc_ioctl_mmap(map32, map));
		return err;
	}
	case COMPAT_FASTRPC_IOCTL_MMAP_64:
	{
		struct compat_fastrpc_ioctl_mmap_64  __user *map32;
		struct fastrpc_ioctl_mmap __user *map;
		long ret;

		map32 = compat_ptr(arg);
		VERIFY(err, NULL != (map = compat_alloc_user_space(
							sizeof(*map))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_mmap_64(map32, map));
		if (err)
			return err;
		ret = filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_MMAP_64,
							(unsigned long)map);
		if (ret)
			return ret;
		VERIFY(err, 0 == compat_put_fastrpc_ioctl_mmap_64(map32, map));
		return err;
	}
	case COMPAT_FASTRPC_IOCTL_MUNMAP:
	{
		struct compat_fastrpc_ioctl_munmap __user *unmap32;
		struct fastrpc_ioctl_munmap __user *unmap;

		unmap32 = compat_ptr(arg);
		VERIFY(err, NULL != (unmap = compat_alloc_user_space(
							sizeof(*unmap))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_munmap(unmap32,
							unmap));
		if (err)
			return err;
		return filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_MUNMAP,
							(unsigned long)unmap);
	}
	case COMPAT_FASTRPC_IOCTL_MUNMAP_64:
	{
		struct compat_fastrpc_ioctl_munmap_64 __user *unmap32;
		struct fastrpc_ioctl_munmap __user *unmap;

		unmap32 = compat_ptr(arg);
		VERIFY(err, NULL != (unmap = compat_alloc_user_space(
							sizeof(*unmap))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_munmap_64(unmap32,
							unmap));
		if (err)
			return err;
		return filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_MUNMAP_64,
							(unsigned long)unmap);
	}
	case COMPAT_FASTRPC_IOCTL_INIT:
		/* fall through */
	case COMPAT_FASTRPC_IOCTL_INIT_ATTRS:
	{
		struct compat_fastrpc_ioctl_init_attrs __user *init32;
		struct fastrpc_ioctl_init_attrs __user *init;

		init32 = compat_ptr(arg);
		VERIFY(err, NULL != (init = compat_alloc_user_space(
							sizeof(*init))));
		if (err)
			return -EFAULT;
		VERIFY(err, 0 == compat_get_fastrpc_ioctl_init(init32,
							init, cmd));
		if (err)
			return err;
		return filp->f_op->unlocked_ioctl(filp,
			 FASTRPC_IOCTL_INIT_ATTRS, (unsigned long)init);
	}
	case FASTRPC_IOCTL_GETINFO:
	{
		compat_uptr_t __user *info32;
		uint32_t __user *info;
		compat_uint_t u;
		long ret;

		info32 = compat_ptr(arg);
		VERIFY(err, NULL != (info = compat_alloc_user_space(
							sizeof(*info))));
		if (err)
			return -EFAULT;
		err = get_user(u, info32);
		err |= put_user(u, info);
		if (err)
			return err;
		ret = filp->f_op->unlocked_ioctl(filp, FASTRPC_IOCTL_GETINFO,
							(unsigned long)info);
		if (ret)
			return ret;
		err = get_user(u, info);
		err |= put_user(u, info32);
		return err;
	}
	case FASTRPC_IOCTL_SETMODE:
		return fastrpc_setmode(filp, cmd, arg);
	case COMPAT_FASTRPC_IOCTL_CONTROL:
	{
		return compat_fastrpc_control(filp, arg);
	}
	case COMPAT_FASTRPC_IOCTL_GETPERF:
	{
		return compat_fastrpc_getperf(filp, arg);
	}
	case COMPAT_FASTRPC_IOCTL_GET_DSP_INFO:
	{
		return compat_fastrpc_get_dsp_info(filp, arg);
	}
	default:
		return -ENOIOCTLCMD;
	}
}
