/*
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_DEVICE_H
#define __ASM_DEVICE_H

struct dev_archdata {
	const struct dma_map_ops *dma_ops;
#ifdef CONFIG_IOMMU_API
	void *iommu;			/* private IOMMU data */
#endif
	bool dma_coherent;
#ifdef CONFIG_ARM64_DMA_USE_IOMMU
	struct dma_iommu_mapping	*mapping;
#endif
};

struct pdev_archdata {
	u64 dma_mask;
};

#ifdef CONFIG_ARM64_DMA_USE_IOMMU
#define to_dma_iommu_mapping(dev) ((dev)->archdata.mapping)
#else
#define to_dma_iommu_mapping(dev) NULL
#endif

#endif
