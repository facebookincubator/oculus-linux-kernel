/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __V4l2_VIDC_EXTENSIONS_H__
#define __V4l2_VIDC_EXTENSIONS_H__

#include <linux/types.h>
#include <linux/v4l2-controls.h>

/*
 * supported standard color formats
 * V4L2_PIX_FMT_NV12   Y/CbCr 4:2:0
 * V4L2_PIX_FMT_RGBA32  RGBA-8-8-8-8
 */
 /* Below are additional color formats */
/* 12  Y/CbCr 4:2:0  compressed */
#define V4L2_PIX_FMT_VIDC_NV12C                 v4l2_fourcc('Q', '1', '2', 'C')
/* Y/CbCr 4:2:0, 10 bits per channel compressed */
#define V4L2_PIX_FMT_VIDC_TP10C                 v4l2_fourcc('Q', '1', '0', 'C')
/* Y/CbCr 4:2:0, 10 bits per channel */
#define V4L2_PIX_FMT_VIDC_P010                  v4l2_fourcc('P', '0', '1', '0')
/* 32  RGBA-8-8-8-8 compressed */
#define V4L2_PIX_FMT_VIDC_ARGB32C               v4l2_fourcc('Q', '2', '4', 'C')
#define V4L2_META_FMT_VIDC                      v4l2_fourcc('Q', 'M', 'E', 'T')
/* HEIC encoder and decoder */
#define V4L2_PIX_FMT_HEIC                       v4l2_fourcc('H', 'E', 'I', 'C')
/* AV1 */
#define V4L2_PIX_FMT_AV1                        v4l2_fourcc('A', 'V', '1', '0')
/* start of vidc specific colorspace definitions */
#define V4L2_COLORSPACE_VIDC_GENERIC_FILM    101
#define V4L2_COLORSPACE_VIDC_EG431           102
#define V4L2_COLORSPACE_VIDC_EBU_TECH        103

#define V4L2_XFER_FUNC_VIDC_BT470_SYSTEM_M   201
#define V4L2_XFER_FUNC_VIDC_BT470_SYSTEM_BG  202
#define V4L2_XFER_FUNC_VIDC_BT601_525_OR_625 203
#define V4L2_XFER_FUNC_VIDC_LINEAR           204
#define V4L2_XFER_FUNC_VIDC_XVYCC            205
#define V4L2_XFER_FUNC_VIDC_BT1361           206
#define V4L2_XFER_FUNC_VIDC_BT2020           207
#define V4L2_XFER_FUNC_VIDC_ST428            208
#define V4L2_XFER_FUNC_VIDC_HLG              209

/* should be 255 or below due to u8 limitation */
#define V4L2_YCBCR_VIDC_SRGB_OR_SMPTE_ST428  241
#define V4L2_YCBCR_VIDC_FCC47_73_682         242

/* end of vidc specific colorspace definitions */
#ifndef V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10_STILL_PICTURE
#define V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10_STILL_PICTURE    (3)
#endif

/* vendor controls start */
#ifdef V4L2_CTRL_CLASS_CODEC
#define V4L2_CID_MPEG_VIDC_BASE (V4L2_CTRL_CLASS_CODEC | 0x2000)
#else
#define V4L2_CID_MPEG_VIDC_BASE (V4L2_CTRL_CLASS_MPEG | 0x2000)
#endif

#define V4L2_MPEG_MSM_VIDC_DISABLE 0
#define V4L2_MPEG_MSM_VIDC_ENABLE 1

#define V4L2_CID_MPEG_VIDC_SECURE               (V4L2_CID_MPEG_VIDC_BASE + 0x1)
#define V4L2_CID_MPEG_VIDC_LOWLATENCY_REQUEST   (V4L2_CID_MPEG_VIDC_BASE + 0x3)
/* FIXme: */
#define V4L2_CID_MPEG_VIDC_CODEC_CONFIG         (V4L2_CID_MPEG_VIDC_BASE + 0x4)
#define V4L2_CID_MPEG_VIDC_FRAME_RATE           (V4L2_CID_MPEG_VIDC_BASE + 0x5)
#define V4L2_CID_MPEG_VIDC_OPERATING_RATE       (V4L2_CID_MPEG_VIDC_BASE + 0x6)

/* Encoder Intra refresh period */
#define V4L2_CID_MPEG_VIDC_INTRA_REFRESH_PERIOD (V4L2_CID_MPEG_VIDC_BASE + 0xB)
/* Encoder Intra refresh type */
#define V4L2_CID_MPEG_VIDEO_VIDC_INTRA_REFRESH_TYPE                           \
	(V4L2_CID_MPEG_VIDC_BASE + 0xC)
enum v4l2_mpeg_vidc_ir_type {
	V4L2_MPEG_VIDEO_VIDC_INTRA_REFRESH_RANDOM = 0x0,
	V4L2_MPEG_VIDEO_VIDC_INTRA_REFRESH_CYCLIC = 0x1,
};
#define V4L2_CID_MPEG_VIDC_TIME_DELTA_BASED_RC  (V4L2_CID_MPEG_VIDC_BASE + 0xD)
/* Encoder quality controls */
#define V4L2_CID_MPEG_VIDC_CONTENT_ADAPTIVE_CODING                            \
	(V4L2_CID_MPEG_VIDC_BASE + 0xE)
#define V4L2_CID_MPEG_VIDC_QUALITY_BITRATE_BOOST                              \
	(V4L2_CID_MPEG_VIDC_BASE + 0xF)
#define V4L2_CID_MPEG_VIDC_VIDEO_BLUR_TYPES                                   \
	(V4L2_CID_MPEG_VIDC_BASE + 0x10)
enum v4l2_mpeg_vidc_blur_types {
	VIDC_BLUR_NONE               = 0x0,
	VIDC_BLUR_EXTERNAL           = 0x1,
	VIDC_BLUR_ADAPTIVE           = 0x2,
};
/* (blur width) << 16 | (blur height) */
#define V4L2_CID_MPEG_VIDC_VIDEO_BLUR_RESOLUTION                              \
	(V4L2_CID_MPEG_VIDC_BASE + 0x11)
/* TODO: jdas: compound control for matrix */
#define V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX                        \
	(V4L2_CID_MPEG_VIDC_BASE + 0x12)

/* various Metadata - encoder & decoder */
enum v4l2_mpeg_vidc_metadata_bits {
	V4L2_MPEG_VIDC_META_DISABLE          = 0x0,
	V4L2_MPEG_VIDC_META_ENABLE           = 0x1,
	V4L2_MPEG_VIDC_META_TX_INPUT         = 0x2,
	V4L2_MPEG_VIDC_META_TX_OUTPUT        = 0x4,
	V4L2_MPEG_VIDC_META_RX_INPUT         = 0x8,
	V4L2_MPEG_VIDC_META_RX_OUTPUT        = 0x10,
	V4L2_MPEG_VIDC_META_MAX              = 0x20,
};

#define V4L2_CID_MPEG_VIDC_METADATA_LTR_MARK_USE_DETAILS                      \
	(V4L2_CID_MPEG_VIDC_BASE + 0x13)
#define V4L2_CID_MPEG_VIDC_METADATA_SEQ_HEADER_NAL                            \
	(V4L2_CID_MPEG_VIDC_BASE + 0x14)
#define V4L2_CID_MPEG_VIDC_METADATA_DPB_LUMA_CHROMA_MISR                      \
	(V4L2_CID_MPEG_VIDC_BASE + 0x15)
#define V4L2_CID_MPEG_VIDC_METADATA_OPB_LUMA_CHROMA_MISR                      \
	(V4L2_CID_MPEG_VIDC_BASE + 0x16)
#define V4L2_CID_MPEG_VIDC_METADATA_INTERLACE                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x17)
#define V4L2_CID_MPEG_VIDC_METADATA_CONCEALED_MB_COUNT                        \
	(V4L2_CID_MPEG_VIDC_BASE + 0x18)
#define V4L2_CID_MPEG_VIDC_METADATA_HISTOGRAM_INFO                            \
	(V4L2_CID_MPEG_VIDC_BASE + 0x19)
#define V4L2_CID_MPEG_VIDC_METADATA_SEI_MASTERING_DISPLAY_COLOUR              \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1A)
#define V4L2_CID_MPEG_VIDC_METADATA_SEI_CONTENT_LIGHT_LEVEL                   \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1B)
#define V4L2_CID_MPEG_VIDC_METADATA_HDR10PLUS                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1C)
#define V4L2_CID_MPEG_VIDC_METADATA_EVA_STATS                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1D)
#define V4L2_CID_MPEG_VIDC_METADATA_BUFFER_TAG                                \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1E)
#define V4L2_CID_MPEG_VIDC_METADATA_SUBFRAME_OUTPUT                           \
	(V4L2_CID_MPEG_VIDC_BASE + 0x1F)
#define V4L2_CID_MPEG_VIDC_METADATA_ROI_INFO                                  \
	(V4L2_CID_MPEG_VIDC_BASE + 0x20)
#define V4L2_CID_MPEG_VIDC_METADATA_TIMESTAMP                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x21)
#define V4L2_CID_MPEG_VIDC_METADATA_ENC_QP_METADATA                           \
	(V4L2_CID_MPEG_VIDC_BASE + 0x22)
#define V4L2_CID_MPEG_VIDC_MIN_BITSTREAM_SIZE_OVERWRITE                       \
	(V4L2_CID_MPEG_VIDC_BASE + 0x23)
#define V4L2_CID_MPEG_VIDC_METADATA_BITSTREAM_RESOLUTION                      \
	(V4L2_CID_MPEG_VIDC_BASE + 0x24)
#define V4L2_CID_MPEG_VIDC_METADATA_CROP_OFFSETS                              \
	(V4L2_CID_MPEG_VIDC_BASE + 0x25)
#define V4L2_CID_MPEG_VIDC_METADATA_SALIENCY_INFO                             \
	(V4L2_CID_MPEG_VIDC_BASE + 0x26)
#define V4L2_CID_MPEG_VIDC_METADATA_TRANSCODE_STAT_INFO                       \
	(V4L2_CID_MPEG_VIDC_BASE + 0x27)

/* Encoder Super frame control */
#define V4L2_CID_MPEG_VIDC_SUPERFRAME           (V4L2_CID_MPEG_VIDC_BASE + 0x28)
/* Thumbnail Mode control */
#define V4L2_CID_MPEG_VIDC_THUMBNAIL_MODE       (V4L2_CID_MPEG_VIDC_BASE + 0x29)
/* Priority control */
#define V4L2_CID_MPEG_VIDC_PRIORITY             (V4L2_CID_MPEG_VIDC_BASE + 0x2A)
/* Metadata DPB Tag List*/
#define V4L2_CID_MPEG_VIDC_METADATA_DPB_TAG_LIST                             \
	(V4L2_CID_MPEG_VIDC_BASE + 0x2B)
/* Encoder Input Compression Ratio control */
#define V4L2_CID_MPEG_VIDC_ENC_INPUT_COMPRESSION_RATIO                       \
	(V4L2_CID_MPEG_VIDC_BASE + 0x2C)
#define V4L2_CID_MPEG_VIDC_METADATA_DEC_QP_METADATA                           \
	(V4L2_CID_MPEG_VIDC_BASE + 0x2E)
/* Encoder Complexity control */
#define V4L2_CID_MPEG_VIDC_VENC_COMPLEXITY                                   \
	(V4L2_CID_MPEG_VIDC_BASE + 0x2F)
/* Decoder Max Number of Reorder Frames */
#define V4L2_CID_MPEG_VIDC_METADATA_MAX_NUM_REORDER_FRAMES                   \
	(V4L2_CID_MPEG_VIDC_BASE + 0x30)
/* Control IDs for AV1 */
#define V4L2_CID_MPEG_VIDEO_AV1_PROFILE        (V4L2_CID_MPEG_VIDC_BASE + 0x31)
enum v4l2_mpeg_video_av1_profile {
	V4L2_MPEG_VIDEO_AV1_PROFILE_MAIN            = 0,
	V4L2_MPEG_VIDEO_AV1_PROFILE_HIGH            = 1,
	V4L2_MPEG_VIDEO_AV1_PROFILE_PROFESSIONAL    = 2,
};

#define V4L2_CID_MPEG_VIDEO_AV1_LEVEL           (V4L2_CID_MPEG_VIDC_BASE + 0x32)
enum v4l2_mpeg_video_av1_level {
	V4L2_MPEG_VIDEO_AV1_LEVEL_2_0  = 0,
	V4L2_MPEG_VIDEO_AV1_LEVEL_2_1  = 1,
	V4L2_MPEG_VIDEO_AV1_LEVEL_2_2  = 2,
	V4L2_MPEG_VIDEO_AV1_LEVEL_2_3  = 3,
	V4L2_MPEG_VIDEO_AV1_LEVEL_3_0  = 4,
	V4L2_MPEG_VIDEO_AV1_LEVEL_3_1  = 5,
	V4L2_MPEG_VIDEO_AV1_LEVEL_3_2  = 6,
	V4L2_MPEG_VIDEO_AV1_LEVEL_3_3  = 7,
	V4L2_MPEG_VIDEO_AV1_LEVEL_4_0  = 8,
	V4L2_MPEG_VIDEO_AV1_LEVEL_4_1  = 9,
	V4L2_MPEG_VIDEO_AV1_LEVEL_4_2  = 10,
	V4L2_MPEG_VIDEO_AV1_LEVEL_4_3  = 11,
	V4L2_MPEG_VIDEO_AV1_LEVEL_5_0  = 12,
	V4L2_MPEG_VIDEO_AV1_LEVEL_5_1  = 13,
	V4L2_MPEG_VIDEO_AV1_LEVEL_5_2  = 14,
	V4L2_MPEG_VIDEO_AV1_LEVEL_5_3  = 15,
	V4L2_MPEG_VIDEO_AV1_LEVEL_6_0  = 16,
	V4L2_MPEG_VIDEO_AV1_LEVEL_6_1  = 17,
	V4L2_MPEG_VIDEO_AV1_LEVEL_6_2  = 18,
	V4L2_MPEG_VIDEO_AV1_LEVEL_6_3  = 19,
	V4L2_MPEG_VIDEO_AV1_LEVEL_7_0  = 20,
	V4L2_MPEG_VIDEO_AV1_LEVEL_7_1  = 21,
	V4L2_MPEG_VIDEO_AV1_LEVEL_7_2  = 22,
	V4L2_MPEG_VIDEO_AV1_LEVEL_7_3  = 23,
};

#define V4L2_CID_MPEG_VIDEO_AV1_TIER        (V4L2_CID_MPEG_VIDC_BASE + 0x33)
enum v4l2_mpeg_video_av1_tier {
	V4L2_MPEG_VIDEO_AV1_TIER_MAIN  = 0,
	V4L2_MPEG_VIDEO_AV1_TIER_HIGH  = 1,
};
/* Decoder Timestamp Reorder control */
#define V4L2_CID_MPEG_VIDC_TS_REORDER           (V4L2_CID_MPEG_VIDC_BASE + 0x34)
/* AV1 Decoder Film Grain */
#define V4L2_CID_MPEG_VIDC_AV1D_FILM_GRAIN_PRESENT                           \
	(V4L2_CID_MPEG_VIDC_BASE + 0x35)
/* Control to set input metadata buffer fd */
#define V4L2_CID_MPEG_VIDC_INPUT_METADATA_FD                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x36)
/* Control to enable input metadata via request api */
#define V4L2_CID_MPEG_VIDC_INPUT_METADATA_VIA_REQUEST_ENABLE                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x37)
/* Enables Output buffer fence id via input metadata */
#define V4L2_CID_MPEG_VIDC_METADATA_OUTBUF_FENCE                             \
	(V4L2_CID_MPEG_VIDC_BASE + 0x38)
/* Control to set fence id to driver in order get corresponding fence fd */
#define V4L2_CID_MPEG_VIDC_SW_FENCE_ID                                       \
	(V4L2_CID_MPEG_VIDC_BASE + 0x39)
/*
 * Control to get fence fd from driver for the fence id
 * set via V4L2_CID_MPEG_VIDC_SW_FENCE_ID
 */
#define V4L2_CID_MPEG_VIDC_SW_FENCE_FD                                       \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3A)
#define V4L2_CID_MPEG_VIDC_METADATA_PICTURE_TYPE                             \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3B)

/* Encoder Slice Delivery Mode
 * set format has a dependency on this control
 * and gets invoked when this control is updated.
 */
#define V4L2_CID_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE                          \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3C)
enum v4l2_hevc_encode_delivery_mode {
	V4L2_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE_FRAME_BASED = 0,
	V4L2_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE_SLICE_BASED = 1,
};

#define V4L2_CID_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE                          \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3D)
enum v4l2_h264_encode_delivery_mode {
	V4L2_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE_FRAME_BASED = 0,
	V4L2_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE_SLICE_BASED = 1,
};

#define V4L2_CID_MPEG_VIDC_CRITICAL_PRIORITY                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3E)
#define V4L2_CID_MPEG_VIDC_RESERVE_DURATION                                  \
	(V4L2_CID_MPEG_VIDC_BASE + 0x3F)

#define V4L2_CID_MPEG_VIDC_METADATA_DOLBY_RPU                                 \
	(V4L2_CID_MPEG_VIDC_BASE + 0x40)

#define V4L2_CID_MPEG_VIDC_CLIENT_ID                                          \
	(V4L2_CID_MPEG_VIDC_BASE + 0x41)

#define V4L2_CID_MPEG_VIDC_LAST_FLAG_EVENT_ENABLE                             \
	(V4L2_CID_MPEG_VIDC_BASE + 0x42)

#define V4L2_CID_MPEG_VIDC_VUI_TIMING_INFO                                    \
	(V4L2_CID_MPEG_VIDC_BASE + 0x43)

#define V4L2_CID_MPEG_VIDC_EARLY_NOTIFY_ENABLE                                \
	(V4L2_CID_MPEG_VIDC_BASE + 0x44)

#define V4L2_CID_MPEG_VIDC_EARLY_NOTIFY_LINE_COUNT                            \
	(V4L2_CID_MPEG_VIDC_BASE + 0x45)

/* add new controls above this line */
/* Deprecate below controls once availble in gki and gsi bionic header */
#ifndef V4L2_CID_MPEG_VIDEO_BASELAYER_PRIORITY_ID
#define V4L2_CID_MPEG_VIDEO_BASELAYER_PRIORITY_ID                            \
	(V4L2_CID_MPEG_BASE + 230)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_AU_DELIMITER
#define V4L2_CID_MPEG_VIDEO_AU_DELIMITER                                     \
	(V4L2_CID_MPEG_BASE + 231)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_LTR_COUNT
#define V4L2_CID_MPEG_VIDEO_LTR_COUNT                                        \
	(V4L2_CID_MPEG_BASE + 232)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_FRAME_LTR_INDEX
#define V4L2_CID_MPEG_VIDEO_FRAME_LTR_INDEX                                  \
	(V4L2_CID_MPEG_BASE + 233)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_USE_LTR_FRAMES
#define V4L2_CID_MPEG_VIDEO_USE_LTR_FRAMES                                   \
	(V4L2_CID_MPEG_BASE + 234)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_B_FRAME_MIN_QP
#define V4L2_CID_MPEG_VIDEO_H264_B_FRAME_MIN_QP                              \
	(V4L2_CID_MPEG_BASE + 389)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_B_FRAME_MAX_QP
#define V4L2_CID_MPEG_VIDEO_H264_B_FRAME_MAX_QP                              \
	(V4L2_CID_MPEG_BASE + 390)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L0_BR
#define V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L0_BR                           \
	(V4L2_CID_MPEG_BASE + 391)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L1_BR
#define V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L1_BR                           \
	(V4L2_CID_MPEG_BASE + 392)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L2_BR
#define V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L2_BR                           \
	(V4L2_CID_MPEG_BASE + 393)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L3_BR
#define V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L3_BR                           \
	(V4L2_CID_MPEG_BASE + 394)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L4_BR
#define V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L4_BR                           \
	(V4L2_CID_MPEG_BASE + 395)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L5_BR
#define V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L5_BR                           \
	(V4L2_CID_MPEG_BASE + 396)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L6_BR
#define V4L2_CID_MPEG_VIDEO_H264_HIER_CODING_L6_BR                           \
	(V4L2_CID_MPEG_BASE + 397)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_MIN_QP
#define V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_MIN_QP                              \
	(V4L2_CID_MPEG_BASE + 647)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_MAX_QP
#define V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_MAX_QP                              \
	(V4L2_CID_MPEG_BASE + 648)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_MIN_QP
#define V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_MIN_QP                              \
	(V4L2_CID_MPEG_BASE + 649)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_MAX_QP
#define V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_MAX_QP                              \
	(V4L2_CID_MPEG_BASE + 650)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_MIN_QP
#define V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_MIN_QP                              \
	(V4L2_CID_MPEG_BASE + 651)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_MAX_QP
#define V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_MAX_QP                              \
	(V4L2_CID_MPEG_BASE + 652)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_DEC_DISPLAY_DELAY
#define V4L2_CID_MPEG_VIDEO_DEC_DISPLAY_DELAY                                \
	(V4L2_CID_MPEG_BASE + 653)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_DEC_DISPLAY_DELAY_ENABLE
#define V4L2_CID_MPEG_VIDEO_DEC_DISPLAY_DELAY_ENABLE                         \
	(V4L2_CID_MPEG_BASE + 654)
#endif

enum v4l2_mpeg_vidc_metapayload_header_flags {
	METADATA_FLAGS_NONE             = 0,
	METADATA_FLAGS_TOP_FIELD        = (1 << 0),
	METADATA_FLAGS_BOTTOM_FIELD     = (1 << 1),
};

enum saliency_roi_info {
	METADATA_SALIENCY_NONE,
	METADATA_SALIENCY_TYPE0,
};

struct msm_vidc_metabuf_header {
	__u32 count;
	__u32 size;
	__u32 version;
	__u32 reserved[5];
};
struct msm_vidc_metapayload_header {
	__u32 type;
	__u32 size;
	__u32 version;
	__u32 offset;
	__u32 flags;
	__u32 reserved[3];
};
enum v4l2_mpeg_vidc_metadata {
	METADATA_BITSTREAM_RESOLUTION         = 0x03000103,
	METADATA_CROP_OFFSETS                 = 0x03000105,
	METADATA_LTR_MARK_USE_DETAILS         = 0x03000137,
	METADATA_SEQ_HEADER_NAL               = 0x0300014a,
	METADATA_DPB_LUMA_CHROMA_MISR         = 0x03000153,
	METADATA_OPB_LUMA_CHROMA_MISR         = 0x03000154,
	METADATA_INTERLACE                    = 0x03000156,
	METADATA_TIMESTAMP                    = 0x0300015c,
	METADATA_CONCEALED_MB_COUNT           = 0x0300015f,
	METADATA_HISTOGRAM_INFO               = 0x03000161,
	METADATA_PICTURE_TYPE                 = 0x03000162,
	METADATA_SEI_MASTERING_DISPLAY_COLOUR = 0x03000163,
	METADATA_SEI_CONTENT_LIGHT_LEVEL      = 0x03000164,
	METADATA_HDR10PLUS                    = 0x03000165,
	METADATA_EVA_STATS                    = 0x03000167,
	METADATA_BUFFER_TAG                   = 0x0300016b,
	METADATA_SUBFRAME_OUTPUT              = 0x0300016d,
	METADATA_ENC_QP_METADATA              = 0x0300016e,
	METADATA_DEC_QP_METADATA              = 0x0300016f,
	METADATA_ROI_INFO                     = 0x03000173,
	METADATA_DPB_TAG_LIST                 = 0x03000179,
	METADATA_MAX_NUM_REORDER_FRAMES       = 0x03000127,
	METADATA_SALIENCY_INFO                = 0x0300018A,
	METADATA_FENCE                        = 0x0300018B,
	METADATA_TRANSCODING_STAT_INFO        = 0x03000191,
	METADATA_DV_RPU                       = 0x03000192,
};
enum meta_interlace_info {
	META_INTERLACE_INFO_NONE                            = 0x00000000,
	META_INTERLACE_FRAME_PROGRESSIVE                    = 0x00000001,
	META_INTERLACE_FRAME_MBAFF                          = 0x00000002,
	META_INTERLACE_FRAME_INTERLEAVE_TOPFIELD_FIRST      = 0x00000004,
	META_INTERLACE_FRAME_INTERLEAVE_BOTTOMFIELD_FIRST   = 0x00000008,
	META_INTERLACE_FRAME_INTERLACE_TOPFIELD_FIRST       = 0x00000010,
	META_INTERLACE_FRAME_INTERLACE_BOTTOMFIELD_FIRST    = 0x00000020,
};

/*
 * enum meta_picture_type - specifies input picture type
 * @META_PICTURE_TYPE_START: start of a frame or first slice in a frame
 * @META_PICTURE_TYPE_END: end of a frame or last slice in a frame
 */
enum meta_picture_type {
	META_PICTURE_TYPE_IDR                            = 0x00000001,
	META_PICTURE_TYPE_P                              = 0x00000002,
	META_PICTURE_TYPE_B                              = 0x00000004,
	META_PICTURE_TYPE_I                              = 0x00000008,
	META_PICTURE_TYPE_CRA                            = 0x00000010,
	META_PICTURE_TYPE_BLA                            = 0x00000020,
	META_PICTURE_TYPE_NOSHOW                         = 0x00000040,
	META_PICTURE_TYPE_START                          = 0x00000080,
	META_PICTURE_TYPE_END                            = 0x00000100,
};

/* vendor controls end */

/* vendor events start */

/*
 * Vendor event structure looks like below (reference videodev2.h)
 * struct v4l2_event {
 *      __u32                             type;
 *      union {
 *              struct v4l2_event_src_change    src_change;
 *              ...
 *              / ********** vendor event structure ******** /
 *              __u8                            data[64];
 *      } u;
 *      __u32                             pending;
 *      ...
 *  }
 */

/* vendor events end */

/* Default metadata size (align to 4KB) */
#define MSM_VIDC_METADATA_SIZE           (4 * 4096) /* 16 KB */
#define ENCODE_INPUT_METADATA_SIZE       (512 * 4096) /* 2 MB */
#define DECODE_INPUT_METADATA_SIZE       MSM_VIDC_METADATA_SIZE
#define MSM_VIDC_METADATA_DOLBY_RPU_SIZE  (41 * 1024) /* 41 KB */

#endif
