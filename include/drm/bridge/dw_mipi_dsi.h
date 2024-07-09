/*
 * Copyright (C) STMicroelectronics SA 2017
 *
 * Authors: Philippe Cornu <philippe.cornu@st.com>
 *          Yannick Fertre <yannick.fertre@st.com>
 *
 * License terms:  GNU General Public License (GPL), version 2
 */

#ifndef __DW_MIPI_DSI__
#define __DW_MIPI_DSI__

struct dw_mipi_dsi;

struct dw_mipi_dsi_phy_ops {
	int (*init)(void *priv_data);
	int (*get_lane_mbps)(void *priv_data, struct drm_display_mode *mode,
			     unsigned long mode_flags, u32 lanes, u32 format,
			     unsigned int *lane_mbps);
};

struct dw_mipi_dsi_plat_data {
	void __iomem *base;
	unsigned int max_data_lanes;

	enum drm_mode_status (*mode_valid)(void *priv_data,
					   const struct drm_display_mode *mode);

	const struct dw_mipi_dsi_phy_ops *phy_ops;

	void *priv_data;
};

struct dw_mipi_dsi *dw_mipi_dsi_probe(struct platform_device *pdev,
				      const struct dw_mipi_dsi_plat_data
				      *plat_data);
void dw_mipi_dsi_remove(struct dw_mipi_dsi *dsi);
struct dw_mipi_dsi *dw_mipi_dsi_bind(struct platform_device *pdev,
				     struct drm_encoder *encoder,
				     const struct dw_mipi_dsi_plat_data
				     *plat_data);
void dw_mipi_dsi_unbind(struct dw_mipi_dsi *dsi);

#endif /* __DW_MIPI_DSI__ */
