/*
 * Copyright (c) 2018, Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "port.h"

/* speed in units of 1Mb */
static const u32 mlx5e_link_speed[MLX5E_LINK_MODES_NUMBER] = {
	[MLX5E_1000BASE_CX_SGMII] = 1000,
	[MLX5E_1000BASE_KX]       = 1000,
	[MLX5E_10GBASE_CX4]       = 10000,
	[MLX5E_10GBASE_KX4]       = 10000,
	[MLX5E_10GBASE_KR]        = 10000,
	[MLX5E_20GBASE_KR2]       = 20000,
	[MLX5E_40GBASE_CR4]       = 40000,
	[MLX5E_40GBASE_KR4]       = 40000,
	[MLX5E_56GBASE_R4]        = 56000,
	[MLX5E_10GBASE_CR]        = 10000,
	[MLX5E_10GBASE_SR]        = 10000,
	[MLX5E_10GBASE_ER]        = 10000,
	[MLX5E_40GBASE_SR4]       = 40000,
	[MLX5E_40GBASE_LR4]       = 40000,
	[MLX5E_50GBASE_SR2]       = 50000,
	[MLX5E_100GBASE_CR4]      = 100000,
	[MLX5E_100GBASE_SR4]      = 100000,
	[MLX5E_100GBASE_KR4]      = 100000,
	[MLX5E_100GBASE_LR4]      = 100000,
	[MLX5E_100BASE_TX]        = 100,
	[MLX5E_1000BASE_T]        = 1000,
	[MLX5E_10GBASE_T]         = 10000,
	[MLX5E_25GBASE_CR]        = 25000,
	[MLX5E_25GBASE_KR]        = 25000,
	[MLX5E_25GBASE_SR]        = 25000,
	[MLX5E_50GBASE_CR2]       = 50000,
	[MLX5E_50GBASE_KR2]       = 50000,
};

u32 mlx5e_port_ptys2speed(u32 eth_proto_oper)
{
	unsigned long temp = eth_proto_oper;
	u32 speed = 0;
	int i;

	i = find_first_bit(&temp, MLX5E_LINK_MODES_NUMBER);
	if (i < MLX5E_LINK_MODES_NUMBER)
		speed = mlx5e_link_speed[i];

	return speed;
}

int mlx5e_port_linkspeed(struct mlx5_core_dev *mdev, u32 *speed)
{
	u32 out[MLX5_ST_SZ_DW(ptys_reg)] = {};
	u32 eth_proto_oper;
	int err;

	err = mlx5_query_port_ptys(mdev, out, sizeof(out), MLX5_PTYS_EN, 1);
	if (err)
		return err;

	eth_proto_oper = MLX5_GET(ptys_reg, out, eth_proto_oper);
	*speed = mlx5e_port_ptys2speed(eth_proto_oper);
	if (!(*speed))
		err = -EINVAL;

	return err;
}

int mlx5e_port_max_linkspeed(struct mlx5_core_dev *mdev, u32 *speed)
{
	u32 max_speed = 0;
	u32 proto_cap;
	int err;
	int i;

	err = mlx5_query_port_proto_cap(mdev, &proto_cap, MLX5_PTYS_EN);
	if (err)
		return err;

	for (i = 0; i < MLX5E_LINK_MODES_NUMBER; ++i)
		if (proto_cap & MLX5E_PROT_MASK(i))
			max_speed = max(max_speed, mlx5e_link_speed[i]);

	*speed = max_speed;
	return 0;
}

u32 mlx5e_port_speed2linkmodes(u32 speed)
{
	u32 link_modes = 0;
	int i;

	for (i = 0; i < MLX5E_LINK_MODES_NUMBER; ++i) {
		if (mlx5e_link_speed[i] == speed)
			link_modes |= MLX5E_PROT_MASK(i);
	}

	return link_modes;
}

int mlx5e_port_query_pbmc(struct mlx5_core_dev *mdev, void *out)
{
	int sz = MLX5_ST_SZ_BYTES(pbmc_reg);
	void *in;
	int err;

	in = kzalloc(sz, GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	MLX5_SET(pbmc_reg, in, local_port, 1);
	err = mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PBMC, 0, 0);

	kfree(in);
	return err;
}

int mlx5e_port_set_pbmc(struct mlx5_core_dev *mdev, void *in)
{
	int sz = MLX5_ST_SZ_BYTES(pbmc_reg);
	void *out;
	int err;

	out = kzalloc(sz, GFP_KERNEL);
	if (!out)
		return -ENOMEM;

	MLX5_SET(pbmc_reg, in, local_port, 1);
	err = mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PBMC, 0, 1);

	kfree(out);
	return err;
}

/* buffer[i]: buffer that priority i mapped to */
int mlx5e_port_query_priority2buffer(struct mlx5_core_dev *mdev, u8 *buffer)
{
	int sz = MLX5_ST_SZ_BYTES(pptb_reg);
	u32 prio_x_buff;
	void *out;
	void *in;
	int prio;
	int err;

	in = kzalloc(sz, GFP_KERNEL);
	out = kzalloc(sz, GFP_KERNEL);
	if (!in || !out) {
		err = -ENOMEM;
		goto out;
	}

	MLX5_SET(pptb_reg, in, local_port, 1);
	err = mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPTB, 0, 0);
	if (err)
		goto out;

	prio_x_buff = MLX5_GET(pptb_reg, out, prio_x_buff);
	for (prio = 0; prio < 8; prio++) {
		buffer[prio] = (u8)(prio_x_buff >> (4 * prio)) & 0xF;
		mlx5_core_dbg(mdev, "prio %d, buffer %d\n", prio, buffer[prio]);
	}
out:
	kfree(in);
	kfree(out);
	return err;
}

int mlx5e_port_set_priority2buffer(struct mlx5_core_dev *mdev, u8 *buffer)
{
	int sz = MLX5_ST_SZ_BYTES(pptb_reg);
	u32 prio_x_buff;
	void *out;
	void *in;
	int prio;
	int err;

	in = kzalloc(sz, GFP_KERNEL);
	out = kzalloc(sz, GFP_KERNEL);
	if (!in || !out) {
		err = -ENOMEM;
		goto out;
	}

	/* First query the pptb register */
	MLX5_SET(pptb_reg, in, local_port, 1);
	err = mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPTB, 0, 0);
	if (err)
		goto out;

	memcpy(in, out, sz);
	MLX5_SET(pptb_reg, in, local_port, 1);

	/* Update the pm and prio_x_buff */
	MLX5_SET(pptb_reg, in, pm, 0xFF);

	prio_x_buff = 0;
	for (prio = 0; prio < 8; prio++)
		prio_x_buff |= (buffer[prio] << (4 * prio));
	MLX5_SET(pptb_reg, in, prio_x_buff, prio_x_buff);

	err = mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPTB, 0, 1);

out:
	kfree(in);
	kfree(out);
	return err;
}
