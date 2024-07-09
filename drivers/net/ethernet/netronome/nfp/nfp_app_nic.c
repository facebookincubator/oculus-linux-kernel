/*
 * Copyright (C) 2017 Netronome Systems, Inc.
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

#include "nfpcore/nfp_cpp.h"
#include "nfpcore/nfp_nsp.h"
#include "nfp_app.h"
#include "nfp_main.h"
#include "nfp_net.h"
#include "nfp_port.h"

int nfp_app_nic_vnic_init_phy_port(struct nfp_pf *pf, struct nfp_app *app,
				   struct nfp_net *nn, unsigned int id)
{
	int err;

	if (!pf->eth_tbl)
		return 0;

	nn->port = nfp_port_alloc(app, NFP_PORT_PHYS_PORT, nn->dp.netdev);
	if (IS_ERR(nn->port))
		return PTR_ERR(nn->port);

	err = nfp_port_init_phy_port(pf, app, nn->port, id);
	if (err) {
		nfp_port_free(nn->port);
		return err;
	}

	return nn->port->type == NFP_PORT_INVALID;
}

int nfp_app_nic_vnic_alloc(struct nfp_app *app, struct nfp_net *nn,
			   unsigned int id)
{
	int err;

	err = nfp_app_nic_vnic_init_phy_port(app->pf, app, nn, id);
	if (err)
		return err < 0 ? err : 0;

	nfp_net_get_mac_addr(app->pf, nn->dp.netdev, nn->port);

	return 0;
}
