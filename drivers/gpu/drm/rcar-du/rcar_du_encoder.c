/*
 * rcar_du_encoder.c  --  R-Car Display Unit Encoder
 *
 * Copyright (C) 2013-2014 Renesas Electronics Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/export.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>

#include "rcar_du_drv.h"
#include "rcar_du_encoder.h"
#include "rcar_du_kms.h"
#include "rcar_lvds.h"

/* -----------------------------------------------------------------------------
 * Encoder
 */

static const struct drm_encoder_helper_funcs encoder_helper_funcs = {
};

static const struct drm_encoder_funcs encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

int rcar_du_encoder_init(struct rcar_du_device *rcdu,
			 enum rcar_du_output output,
			 struct device_node *enc_node)
{
	struct rcar_du_encoder *renc;
	struct drm_encoder *encoder;
	struct drm_bridge *bridge = NULL;
	int ret;

	renc = devm_kzalloc(rcdu->dev, sizeof(*renc), GFP_KERNEL);
	if (renc == NULL)
		return -ENOMEM;

	renc->output = output;
	encoder = rcar_encoder_to_drm_encoder(renc);

	dev_dbg(rcdu->dev, "initializing encoder %pOF for output %u\n",
		enc_node, output);

	/* Locate the DRM bridge from the encoder DT node. */
	bridge = of_drm_find_bridge(enc_node);
	if (!bridge) {
		ret = -EPROBE_DEFER;
		goto done;
	}

	if (output == RCAR_DU_OUTPUT_LVDS0 ||
	    output == RCAR_DU_OUTPUT_LVDS1)
		rcdu->lvds[output - RCAR_DU_OUTPUT_LVDS0] = bridge;

	/*
	 * On Gen3 skip the LVDS1 output if the LVDS1 encoder is used as a
	 * companion for LVDS0 in dual-link mode.
	 */
	if (rcdu->info->gen >= 3 && output == RCAR_DU_OUTPUT_LVDS1) {
		if (rcar_lvds_dual_link(bridge)) {
			ret = -ENOLINK;
			goto done;
		}
	}

	ret = drm_encoder_init(rcdu->ddev, encoder, &encoder_funcs,
			       DRM_MODE_ENCODER_NONE, NULL);
	if (ret < 0)
		goto done;

	drm_encoder_helper_add(encoder, &encoder_helper_funcs);

	/*
	 * Attach the bridge to the encoder. The bridge will create the
	 * connector.
	 */
	ret = drm_bridge_attach(encoder, bridge, NULL);
	if (ret) {
		drm_encoder_cleanup(encoder);
		return ret;
	}

done:
	if (ret < 0) {
		if (encoder->name)
			encoder->funcs->destroy(encoder);
		devm_kfree(rcdu->dev, renc);
	}

	return ret;
}
