/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#if !defined(_GSI_EMULATION_STUBS_H_)
# define _GSI_EMULATION_STUBS_H_

# include <asm/barrier.h>
# define __iormb()       rmb() /* used in gsi.h */
# define __iowmb()       wmb() /* used in gsi.h */

#endif /* #if !defined(_GSI_EMULATION_STUBS_H_) */
