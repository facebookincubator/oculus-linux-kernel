/*
 * Copyright (C) 2015 Free Electrons
 * Copyright (C) 2015 NextThing Co
 *
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drmP.h>

#include "sun4i_drv.h"
#include "sun4i_framebuffer.h"

static int sun4i_de_atomic_check(struct drm_device *dev,
				 struct drm_atomic_state *state)
{
	int ret;

	ret = drm_atomic_helper_check_modeset(dev, state);
	if (ret)
		return ret;

	ret = drm_atomic_normalize_zpos(dev, state);
	if (ret)
		return ret;

	return drm_atomic_helper_check_planes(dev, state);
}

static const struct drm_mode_config_funcs sun4i_de_mode_config_funcs = {
	.output_poll_changed	= drm_fb_helper_output_poll_changed,
	.atomic_check		= sun4i_de_atomic_check,
	.atomic_commit		= drm_atomic_helper_commit,
	.fb_create		= drm_gem_fb_create,
};

static struct drm_mode_config_helper_funcs sun4i_de_mode_config_helpers = {
	.atomic_commit_tail	= drm_atomic_helper_commit_tail_rpm,
};

int sun4i_framebuffer_init(struct drm_device *drm)
{
	drm_mode_config_reset(drm);

	drm->mode_config.max_width = 8192;
	drm->mode_config.max_height = 8192;

	drm->mode_config.funcs = &sun4i_de_mode_config_funcs;
	drm->mode_config.helper_private = &sun4i_de_mode_config_helpers;

	return drm_fb_cma_fbdev_init(drm, 32, 0);
}

void sun4i_framebuffer_free(struct drm_device *drm)
{
	drm_fb_cma_fbdev_fini(drm);
}
