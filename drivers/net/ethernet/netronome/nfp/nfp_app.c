/*
 * Copyright (C) 2017-2018 Netronome Systems, Inc.
 *
 * This software is dual licensed under the GNU General License Version 2,
 * June 1991 as shown in the file COPYING in the top-level directory of this
 * source tree or the BSD 2-Clause License provided below.  You have the
 * option to license this software under the complete terms of either license.
 *
 * The BSD 2-Clause License:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      1. Redistributions of source code must retain the above
 *         copyright notice, this list of conditions and the following
 *         disclaimer.
 *
 *      2. Redistributions in binary form must reproduce the above
 *         copyright notice, this list of conditions and the following
 *         disclaimer in the documentation and/or other materials
 *         provided with the distribution.
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

#include <linux/bug.h>
#include <linux/lockdep.h>
#include <linux/rcupdate.h>
#include <linux/skbuff.h>
#include <linux/slab.h>

#include "nfpcore/nfp_cpp.h"
#include "nfpcore/nfp_nffw.h"
#include "nfp_app.h"
#include "nfp_main.h"
#include "nfp_net.h"
#include "nfp_net_repr.h"
#include "nfp_port.h"

static const struct nfp_app_type *apps[] = {
	[NFP_APP_CORE_NIC]	= &app_nic,
#ifdef CONFIG_BPF_SYSCALL
	[NFP_APP_BPF_NIC]	= &app_bpf,
#else
	[NFP_APP_BPF_NIC]	= &app_nic,
#endif
#ifdef CONFIG_NFP_APP_FLOWER
	[NFP_APP_FLOWER_NIC]	= &app_flower,
#endif
#ifdef CONFIG_NFP_APP_ABM_NIC
	[NFP_APP_ACTIVE_BUFFER_MGMT_NIC] = &app_abm,
#endif
};

struct nfp_app *nfp_app_from_netdev(struct net_device *netdev)
{
	if (nfp_netdev_is_nfp_net(netdev)) {
		struct nfp_net *nn = netdev_priv(netdev);

		return nn->app;
	}

	if (nfp_netdev_is_nfp_repr(netdev)) {
		struct nfp_repr *repr = netdev_priv(netdev);

		return repr->app;
	}

	WARN(1, "Unknown netdev type for nfp_app\n");

	return NULL;
}

const char *nfp_app_mip_name(struct nfp_app *app)
{
	if (!app || !app->pf->mip)
		return "";
	return nfp_mip_name(app->pf->mip);
}

int nfp_app_ndo_init(struct net_device *netdev)
{
	struct nfp_app *app = nfp_app_from_netdev(netdev);

	if (!app || !app->type->ndo_init)
		return 0;
	return app->type->ndo_init(app, netdev);
}

void nfp_app_ndo_uninit(struct net_device *netdev)
{
	struct nfp_app *app = nfp_app_from_netdev(netdev);

	if (app && app->type->ndo_uninit)
		app->type->ndo_uninit(app, netdev);
}

u64 *nfp_app_port_get_stats(struct nfp_port *port, u64 *data)
{
	if (!port || !port->app || !port->app->type->port_get_stats)
		return data;
	return port->app->type->port_get_stats(port->app, port, data);
}

int nfp_app_port_get_stats_count(struct nfp_port *port)
{
	if (!port || !port->app || !port->app->type->port_get_stats_count)
		return 0;
	return port->app->type->port_get_stats_count(port->app, port);
}

u8 *nfp_app_port_get_stats_strings(struct nfp_port *port, u8 *data)
{
	if (!port || !port->app || !port->app->type->port_get_stats_strings)
		return data;
	return port->app->type->port_get_stats_strings(port->app, port, data);
}

struct sk_buff *
nfp_app_ctrl_msg_alloc(struct nfp_app *app, unsigned int size, gfp_t priority)
{
	struct sk_buff *skb;

	if (nfp_app_ctrl_has_meta(app))
		size += 8;

	skb = alloc_skb(size, priority);
	if (!skb)
		return NULL;

	if (nfp_app_ctrl_has_meta(app))
		skb_reserve(skb, 8);

	return skb;
}

struct nfp_reprs *
nfp_reprs_get_locked(struct nfp_app *app, enum nfp_repr_type type)
{
	return rcu_dereference_protected(app->reprs[type],
					 lockdep_is_held(&app->pf->lock));
}

struct nfp_reprs *
nfp_app_reprs_set(struct nfp_app *app, enum nfp_repr_type type,
		  struct nfp_reprs *reprs)
{
	struct nfp_reprs *old;

	old = nfp_reprs_get_locked(app, type);
	rcu_assign_pointer(app->reprs[type], reprs);

	return old;
}

struct nfp_app *nfp_app_alloc(struct nfp_pf *pf, enum nfp_app_id id)
{
	struct nfp_app *app;

	if (id >= ARRAY_SIZE(apps) || !apps[id]) {
		nfp_err(pf->cpp, "unknown FW app ID 0x%02hhx, driver too old or support for FW not built in\n", id);
		return ERR_PTR(-EINVAL);
	}

	if (WARN_ON(!apps[id]->name || !apps[id]->vnic_alloc))
		return ERR_PTR(-EINVAL);
	if (WARN_ON(!apps[id]->ctrl_msg_rx && apps[id]->ctrl_msg_rx_raw))
		return ERR_PTR(-EINVAL);

	app = kzalloc(sizeof(*app), GFP_KERNEL);
	if (!app)
		return ERR_PTR(-ENOMEM);

	app->pf = pf;
	app->cpp = pf->cpp;
	app->pdev = pf->pdev;
	app->type = apps[id];

	return app;
}

void nfp_app_free(struct nfp_app *app)
{
	kfree(app);
}
