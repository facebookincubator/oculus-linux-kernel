/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2017, STMicroelectronics - All Rights Reserved
 *
 * This file is part "VD6281 API" and is dual licensed, either 'STMicroelectronics Proprietary license'
 * or 'BSD 3-clause "New" or "Revised" License' , at your option.\
 *
 ********************************************************************************
 *
 * 'STMicroelectronics Proprietary license'
 *
 ********************************************************************************
 *
 * License terms STMicroelectronics Proprietary in accordance with licensing terms at www.st.com/sla0044
 *
 * STMicroelectronics confidential
 * Reproduction and Communication of this document is strictly prohibited unless
 * specifically authorized in writing by STMicroelectronics.
 *
 *
 ********************************************************************************
 *
 * Alternatively, "VD6281 API" may be distributed under the terms of
 * 'BSD 3-clause "New" or "Revised" License', in which case the following provisions apply instead of the ones
 * mentioned above
 *
 ********************************************************************************
 *
 * License terms BSD 3-clause "New" or "Revised" License.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 ********************************************************************************
 *
 */

#ifndef __VD6281_ADAPTER_IOCTL__
#define __VD6281_ADAPTER_IOCTL__ 1

#define VD6281_MULTI_REG_RD_MAX 8

#define VD6281_IOCTL_REG_WR		_IOW('r', 0x01, struct vd6281_reg)
#define VD6281_IOCTL_REG_RD		_IOWR('r', 0x02, struct vd6281_reg)
#define VD6281_IOCTL_REG_RD_MULTI  _IOWR('r', 0x03, struct vd6281_read_multi_regs)

#define VD6281_IOCTL_GET_SPI_INFO	_IOWR('r', 0x01, struct vd6281_spi_info)
#define VD6281_IOCTL_SET_SPI_PARAMS	_IOW('r', 0x02, struct vd6281_spi_params)
#define VD6281_IOCTL_GET_CHUNK_SAMPLES	_IOWR('r', 0x03, __u16)

struct vd6281_reg {
	__u8 index;
	__u8 data;
};

struct vd6281_read_multi_regs {
	__u8 index;
	__u8 len;
	__u8 data[VD6281_MULTI_REG_RD_MAX];
};

struct vd6281_spi_info {
	__u32 chunk_size;
	__u32 spi_max_frequency;
};

struct vd6281_spi_params {
	__u32 speed_hz;
	__u16 samples_nb_per_chunk;
	__u16 pdm_data_sample_width_in_bytes;
};

#define MAX_CIC_STAGE (4)

#endif
