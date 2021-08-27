/*
 * Copyright (c) 2013-2016, 2018-2021 The Linux Foundation. All rights reserved.*
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */
#ifndef __WLAN_DEFS_H__
#define __WLAN_DEFS_H__

#include <a_osapi.h> /* A_COMPILE_TIME_ASSERT */

/*
 * This file contains WLAN definitions that may be used across both
 * Host and Target software.
 */


/*
 * MAX_SPATIAL_STREAM should be defined in a fwconfig_xxx.h file,
 * but for now provide a default value here in case it's not defined
 * in the fwconfig_xxx.h file.
 */
#ifndef MAX_SPATIAL_STREAM
#define MAX_SPATIAL_STREAM 3
#endif

/*
 * NOTE: The CONFIG_160MHZ_SUPPORT is not used consistently - some code
 * uses "#ifdef CONFIG_160MHZ_SUPPORT" while other code uses
 * "#if CONFIG_160MHZ_SUPPORT".
 * This use is being standardized in the recent versions of code to use
 * #ifdef, but is being left as is in the legacy code branches.
 * To minimize impact to legacy code branches, this file internally
 * converts CONFIG_160MHZ_SUPPORT=0 to having CONFIG_160MHZ_SUPPORT
 * undefined.
 * For builds that explicitly set CONFIG_160MHZ_SUPPORT=0, the bottom of
 * this file restores CONFIG_160MHZ_SUPPORT from being undefined to being 0.
 */
// OLD:
//#ifndef CONFIG_160MHZ_SUPPORT
//#define CONFIG_160MHZ_SUPPORT 0 /* default: 160 MHz channels not supported */
//#endif
// NEW:
#ifdef CONFIG_160MHZ_SUPPORT
  /* CONFIG_160MHZ_SUPPORT is explicitly enabled or explicitly disabled */
  #if !CONFIG_160MHZ_SUPPORT
    /* CONFIG_160MHZ_SUPPORT is explicitly disabled */
    /* Change from CONFIG_160MHZ_SUPPORT=0 to CONFIG_160MHZ_SUPPORT=<undef> */
    #undef CONFIG_160MHZ_SUPPORT
    /*
     * Set a flag to indicate this CONFIG_160MHZ_SUPPORT = 0 --> undef
     * change has been done, so we can undo the change at the bottom
     * of the file.
     */
    #define CONFIG_160MHZ_SUPPORT_UNDEF_WAR
  #endif
#else
  /*
   * For backwards compatibility, if CONFIG_160MHZ_SUPPORT is not defined,
   * default it to 0, if this is either a host build or a Rome target build.
   * This maintains the prior behavior for the host and Rome target builds.
   */
  #if defined(AR6320) || !defined(ATH_TARGET)
    /*
     * Set a flag to indicate that at the end of the file,
     * CONFIG_160MHZ_SUPPORT should be set to 0.
     */
    #define CONFIG_160MHZ_SUPPORT_UNDEF_WAR
  #endif
#endif

#ifndef SUPPORT_11AX
#define SUPPORT_11AX 0 /* 11ax not supported by default */
#endif

/*
 * MAX_SPATIAL_STREAM_ANY -
 * what is the largest number of spatial streams that any target supports
 */
#define MAX_SPATIAL_STREAM_ANY_V2 4 /* pre-hawkeye */
#define MAX_SPATIAL_STREAM_ANY_V3 8 /* includes hawkeye */
/*
 * (temporarily) leave the old MAX_SPATIAL_STREAM_ANY name in place as an alias,
 * and in case some old code is using it
 */
#define MAX_SPATIAL_STREAM_ANY MAX_SPATIAL_STREAM_ANY_V2 /* DEPRECATED */

/* defines to set Packet extension values whic can be 0 us, 8 usec or 16 usec */
/* NOTE: Below values cannot be changed without breaking WMI Compatibility */
#define MAX_HE_NSS               8
#define MAX_HE_MODULATION        8
#define MAX_HE_RU                4
#define HE_MODULATION_NONE       7
#define HE_PET_0_USEC            0
#define HE_PET_8_USEC            1
#define HE_PET_16_USEC           2

#define DEFAULT_OFDMA_RU26_COUNT 0

typedef enum {
    MODE_11A        = 0,   /* 11a Mode */
    MODE_11G        = 1,   /* 11b/g Mode */
    MODE_11B        = 2,   /* 11b Mode */
    MODE_11GONLY    = 3,   /* 11g only Mode */
    MODE_11NA_HT20   = 4,  /* 11a HT20 mode */
    MODE_11NG_HT20   = 5,  /* 11g HT20 mode */
    MODE_11NA_HT40   = 6,  /* 11a HT40 mode */
    MODE_11NG_HT40   = 7,  /* 11g HT40 mode */
    MODE_11AC_VHT20 = 8,
    MODE_11AC_VHT40 = 9,
    MODE_11AC_VHT80 = 10,
    MODE_11AC_VHT20_2G = 11,
    MODE_11AC_VHT40_2G = 12,
    MODE_11AC_VHT80_2G = 13,
#ifdef CONFIG_160MHZ_SUPPORT
    MODE_11AC_VHT80_80 = 14,
    MODE_11AC_VHT160   = 15,
#endif

#if SUPPORT_11AX
    MODE_11AX_HE20 = 16,
    MODE_11AX_HE40 = 17,
    MODE_11AX_HE80 = 18,
    MODE_11AX_HE80_80 = 19,
    MODE_11AX_HE160 = 20,
    MODE_11AX_HE20_2G = 21,
    MODE_11AX_HE40_2G = 22,
    MODE_11AX_HE80_2G = 23,
#endif

#if defined(SUPPORT_11BE) && SUPPORT_11BE
    MODE_11BE_EHT20 = 24,
    MODE_11BE_EHT40 = 25,
    MODE_11BE_EHT80 = 26,
    MODE_11BE_EHT80_80 = 27,
    MODE_11BE_EHT160 = 28,
    MODE_11BE_EHT160_160 = 29,
    MODE_11BE_EHT320 = 30,
    MODE_11BE_EHT20_2G = 31, /* For WIN */
    MODE_11BE_EHT40_2G = 32, /* For WIN */
#endif

    /*
     * MODE_UNKNOWN should not be used within the host / target interface.
     * Thus, it is permissible for MODE_UNKNOWN to be conditionally-defined,
     * taking different values when compiling for different targets.
     */
    MODE_UNKNOWN,
    MODE_UNKNOWN_NO_160MHZ_SUPPORT = 14, /* not needed? */
    MODE_UNKNOWN_160MHZ_SUPPORT = MODE_UNKNOWN, /* not needed? */

#ifdef ATHR_WIN_NWF
    PHY_MODE_MAX    = MODE_UNKNOWN,
    PHY_MODE_MAX_NO_160_MHZ_SUPPORT = MODE_UNKNOWN_NO_160MHZ_SUPPORT,
    PHY_MODE_MAX_160_MHZ_SUPPORT    = MODE_UNKNOWN_160MHZ_SUPPORT,
#else
    MODE_MAX        = MODE_UNKNOWN,
    MODE_MAX_NO_160_MHZ_SUPPORT = MODE_UNKNOWN_NO_160MHZ_SUPPORT,
    MODE_MAX_160_MHZ_SUPPORT    = MODE_UNKNOWN_160MHZ_SUPPORT,
#endif
} WLAN_PHY_MODE;

#if (!defined(CONFIG_160MHZ_SUPPORT)) && (!defined(SUPPORT_11AX))
A_COMPILE_TIME_ASSERT(
    mode_unknown_value_consistency_Check,
    MODE_UNKNOWN == MODE_UNKNOWN_NO_160MHZ_SUPPORT);
#else
/*
 * If SUPPORT_11AX is defined but CONFIG_160MHZ_SUPPORT is not defined,
 * there will be a gap in the mode values, with 14 and 15 being unused.
 * But MODE_UNKNOWN_NO_160MHZ_SUPPORT will have an invalid value, since
 * mode values 16 through 23 will be used for 11AX modes.
 * Thus, MODE_UNKNOWN would still be MODE_UNKNOWN_160MHZ_SUPPORT, for
 * cases where 160 MHz is not supported by 11AX is supported.
 * (Ideally, MODE_UNKNOWN_160MHZ_SUPPORT and NO_160MHZ_SUPPORT should be
 * renamed to cover the 4 permutations of support or no support for
 * 11AX and 160 MHZ, but that is impractical, due to backwards
 * compatibility concerns.)
 */
A_COMPILE_TIME_ASSERT(
    mode_unknown_value_consistency_Check,
    MODE_UNKNOWN == MODE_UNKNOWN_160MHZ_SUPPORT);
#endif

typedef enum {
    VHT_MODE_NONE = 0,  /* NON VHT Mode, e.g., HT, DSSS, CCK */
    VHT_MODE_20M = 1,
    VHT_MODE_40M = 2,
    VHT_MODE_80M = 3,
    VHT_MODE_160M = 4
} VHT_OPER_MODE;

typedef enum {
    WLAN_11A_CAPABILITY   = 1,
    WLAN_11G_CAPABILITY   = 2,
    WLAN_11AG_CAPABILITY  = 3,
} WLAN_CAPABILITY;

#ifdef CONFIG_160MHZ_SUPPORT
#define IS_MODE_VHT(mode) (((mode) == MODE_11AC_VHT20) || \
        ((mode) == MODE_11AC_VHT40)     || \
        ((mode) == MODE_11AC_VHT80)     || \
        ((mode) == MODE_11AC_VHT80_80)  || \
        ((mode) == MODE_11AC_VHT160))
#else
#define IS_MODE_VHT(mode) (((mode) == MODE_11AC_VHT20) || \
        ((mode) == MODE_11AC_VHT40) || \
        ((mode) == MODE_11AC_VHT80))
#endif

#if SUPPORT_11AX
#define IS_MODE_HE(mode) (((mode) == MODE_11AX_HE20) || \
        ((mode) == MODE_11AX_HE40)     || \
        ((mode) == MODE_11AX_HE80)     || \
        ((mode) == MODE_11AX_HE80_80)  || \
        ((mode) == MODE_11AX_HE160)    || \
        ((mode) == MODE_11AX_HE20_2G)  || \
        ((mode) == MODE_11AX_HE40_2G)  || \
        ((mode) == MODE_11AX_HE80_2G))
#define IS_MODE_HE_2G(mode) (((mode) == MODE_11AX_HE20_2G) || \
        ((mode) == MODE_11AX_HE40_2G) || \
        ((mode) == MODE_11AX_HE80_2G))
#endif /* SUPPORT_11AX */

#if defined(SUPPORT_11BE) && SUPPORT_11BE
#define IS_MODE_EHT(mode) (((mode) == MODE_11BE_EHT20) || \
        ((mode) == MODE_11BE_EHT40)     || \
        ((mode) == MODE_11BE_EHT80)     || \
        ((mode) == MODE_11BE_EHT80_80)  || \
        ((mode) == MODE_11BE_EHT160)    || \
        ((mode) == MODE_11BE_EHT160_160)|| \
        ((mode) == MODE_11BE_EHT320)    || \
        ((mode) == MODE_11BE_EHT20_2G)  || \
        ((mode) == MODE_11BE_EHT40_2G))
#define IS_MODE_EHT_2G(mode) (((mode) == MODE_11BE_EHT20_2G) || \
        ((mode) == MODE_11BE_EHT40_2G))
#endif /* SUPPORT_11BE */

#define IS_MODE_VHT_2G(mode) (((mode) == MODE_11AC_VHT20_2G) || \
        ((mode) == MODE_11AC_VHT40_2G) || \
        ((mode) == MODE_11AC_VHT80_2G))


#define IS_MODE_11A(mode)       (((mode) == MODE_11A) || \
                                 ((mode) == MODE_11NA_HT20) || \
                                 ((mode) == MODE_11NA_HT40) || \
                                 (IS_MODE_VHT(mode)))

#define IS_MODE_11B(mode)       ((mode) == MODE_11B)
#define IS_MODE_11G(mode)       (((mode) == MODE_11G) || \
                                 ((mode) == MODE_11GONLY) || \
                                 ((mode) == MODE_11NG_HT20) || \
                                 ((mode) == MODE_11NG_HT40) || \
                                 (IS_MODE_VHT_2G(mode)))
#define IS_MODE_11GN(mode)      (((mode) == MODE_11NG_HT20) || \
                                 ((mode) == MODE_11NG_HT40))
#define IS_MODE_11GONLY(mode)   ((mode) == MODE_11GONLY)

#define IS_MODE_LEGACY(phymode)  ((phymode == MODE_11A) || \
                                  (phymode == MODE_11G) || \
                                  (phymode == MODE_11B) || \
                                  (phymode == MODE_11GONLY))

#define IS_MODE_11N(phymode)     ((phymode >= MODE_11NA_HT20) && \
                                  (phymode <= MODE_11NG_HT40))
#ifdef CONFIG_160MHZ_SUPPORT
  #define IS_MODE_11AC(phymode)  ((phymode >= MODE_11AC_VHT20) && \
                                  (phymode <= MODE_11AC_VHT160))
#else
  #define IS_MODE_11AC(phymode)  ((phymode >= MODE_11AC_VHT20) && \
                                  (phymode <= MODE_11AC_VHT80_2G))
#endif /* CONFIG_160MHZ_SUPPORT */

#if SUPPORT_11AX
  #define IS_MODE_80MHZ(phymode) ((phymode == MODE_11AC_VHT80_2G) || \
                                  (phymode == MODE_11AC_VHT80) || \
                                  (phymode == MODE_11AX_HE80) || \
                                  (phymode == MODE_11AX_HE80_2G))
  #define IS_MODE_40MHZ(phymode) ((phymode == MODE_11AC_VHT40_2G) || \
                                  (phymode == MODE_11AC_VHT40) || \
                                  (phymode == MODE_11NG_HT40) || \
                                  (phymode == MODE_11NA_HT40) || \
                                  (phymode == MODE_11AX_HE40) || \
                                  (phymode == MODE_11AX_HE40_2G))
#else
  #define IS_MODE_80MHZ(phymode) ((phymode == MODE_11AC_VHT80_2G) || \
                                  (phymode == MODE_11AC_VHT80))
  #define IS_MODE_40MHZ(phymode) ((phymode == MODE_11AC_VHT40_2G) || \
                                  (phymode == MODE_11AC_VHT40) || \
                                  (phymode == MODE_11NG_HT40) || \
                                  (phymode == MODE_11NA_HT40))
#endif /* SUPPORT_11AX */

enum {
    REGDMN_MODE_11A_BIT                = 0,  /* 11a channels */
    REGDMN_MODE_TURBO_BIT              = 1,  /* 11a turbo-only channels */
    REGDMN_MODE_11B_BIT                = 2,  /* 11b channels */
    REGDMN_MODE_PUREG_BIT              = 3,  /* 11g channels (OFDM only) */
    REGDMN_MODE_11G_BIT                = 3,  /* XXX historical */
    /* bit 4 is reserved */
    REGDMN_MODE_108G_BIT               = 5,  /* 11g+Turbo channels */
    REGDMN_MODE_108A_BIT               = 6,  /* 11a+Turbo channels */
    /* bit 7 is reserved */
    REGDMN_MODE_XR_BIT                 = 8,  /* XR channels */
    REGDMN_MODE_11A_HALF_RATE_BIT      = 9,  /* 11A half rate channels */
    REGDMN_MODE_11A_QUARTER_RATE_BIT   = 10, /* 11A quarter rate channels */
    REGDMN_MODE_11NG_HT20_BIT          = 11, /* 11N-G HT20 channels */
    REGDMN_MODE_11NA_HT20_BIT          = 12, /* 11N-A HT20 channels */
    REGDMN_MODE_11NG_HT40PLUS_BIT      = 13, /* 11N-G HT40 + channels */
    REGDMN_MODE_11NG_HT40MINUS_BIT     = 14, /* 11N-G HT40 - channels */
    REGDMN_MODE_11NA_HT40PLUS_BIT      = 15, /* 11N-A HT40 + channels */
    REGDMN_MODE_11NA_HT40MINUS_BIT     = 16, /* 11N-A HT40 - channels */
    REGDMN_MODE_11AC_VHT20_BIT         = 17, /* 5Ghz, VHT20 */
    REGDMN_MODE_11AC_VHT40PLUS_BIT     = 18, /* 5Ghz, VHT40 + channels */
    REGDMN_MODE_11AC_VHT40MINUS_BIT    = 19, /* 5Ghz  VHT40 - channels */
    REGDMN_MODE_11AC_VHT80_BIT         = 20, /* 5Ghz, VHT80 channels */
    REGDMN_MODE_11AC_VHT20_2G_BIT      = 21, /* 2Ghz, VHT20 */
    REGDMN_MODE_11AC_VHT40_2G_BIT      = 22, /* 2Ghz, VHT40 */
    REGDMN_MODE_11AC_VHT80_2G_BIT      = 23, /* 2Ghz, VHT80 */
    REGDMN_MODE_11AC_VHT160_BIT        = 24, /* 5Ghz, VHT160 */
    REGDMN_MODE_11AC_VHT40_2GPLUS_BIT  = 25, /* 2Ghz, VHT40+ */
    REGDMN_MODE_11AC_VHT40_2GMINUS_BIT = 26, /* 2Ghz, VHT40- */
    REGDMN_MODE_11AC_VHT80_80_BIT      = 27, /* 5GHz, VHT80+80 */
    /* bits 28 to 31 are reserved */
    REGDMN_MODE_11AXG_HE20_BIT         = 32, /* 2Ghz, HE20 */
    REGDMN_MODE_11AXA_HE20_BIT         = 33, /* 5Ghz, HE20 */
    REGDMN_MODE_11AXG_HE40PLUS_BIT     = 34, /* 2Ghz, HE40+ */
    REGDMN_MODE_11AXG_HE40MINUS_BIT    = 35, /* 2Ghz, HE40- */
    REGDMN_MODE_11AXA_HE40PLUS_BIT     = 36, /* 5Ghz, HE40+ */
    REGDMN_MODE_11AXA_HE40MINUS_BIT    = 37, /* 5Ghz, HE40- */
    REGDMN_MODE_11AXA_HE80_BIT         = 38, /* 5Ghz, HE80 */
    REGDMN_MODE_11AXA_HE160_BIT        = 39, /* 5Ghz, HE160 */
    REGDMN_MODE_11AXA_HE80_80_BIT      = 40, /* 5Ghz, HE80+80 */
    REGDMN_MODE_11BEG_EHT20_BIT        = 41, /* 2Ghz, EHT20 */
    REGDMN_MODE_11BEA_EHT20_BIT        = 42, /* 5Ghz, EHT20 */
    REGDMN_MODE_11BEG_EHT40PLUS_BIT    = 43, /* 2Ghz, EHT40+ */
    REGDMN_MODE_11BEG_EHT40MINUS_BIT   = 44, /* 2Ghz, EHT40- */
    REGDMN_MODE_11BEA_EHT40PLUS_BIT    = 45, /* 5Ghz, EHT40+ */
    REGDMN_MODE_11BEA_EHT40MINUS_BIT   = 46, /* 5Ghz, EHT40- */
    REGDMN_MODE_11BEA_EHT80_BIT        = 47, /* 5Ghz, EHT80 */
    REGDMN_MODE_11BEA_EHT160_BIT       = 48, /* 5Ghz, EHT160 */
    REGDMN_MODE_11BEA_EHT320_BIT       = 49, /* 5Ghz, EHT320 */
};

enum {
    REGDMN_MODE_11A                = 1 << REGDMN_MODE_11A_BIT,                /* 11a channels */
    REGDMN_MODE_TURBO              = 1 << REGDMN_MODE_TURBO_BIT,              /* 11a turbo-only channels */
    REGDMN_MODE_11B                = 1 << REGDMN_MODE_11B_BIT,                /* 11b channels */
    REGDMN_MODE_PUREG              = 1 << REGDMN_MODE_PUREG_BIT,              /* 11g channels (OFDM only) */
    REGDMN_MODE_11G                = 1 << REGDMN_MODE_11G_BIT,                /* XXX historical */
    REGDMN_MODE_108G               = 1 << REGDMN_MODE_108G_BIT,               /* 11g+Turbo channels */
    REGDMN_MODE_108A               = 1 << REGDMN_MODE_108A_BIT,               /* 11a+Turbo channels */
    REGDMN_MODE_XR                 = 1 << REGDMN_MODE_XR_BIT,                 /* XR channels */
    REGDMN_MODE_11A_HALF_RATE      = 1 << REGDMN_MODE_11A_HALF_RATE_BIT,      /* 11A half rate channels */
    REGDMN_MODE_11A_QUARTER_RATE   = 1 << REGDMN_MODE_11A_QUARTER_RATE_BIT,   /* 11A quarter rate channels */
    REGDMN_MODE_11NG_HT20          = 1 << REGDMN_MODE_11NG_HT20_BIT,          /* 11N-G HT20 channels */
    REGDMN_MODE_11NA_HT20          = 1 << REGDMN_MODE_11NA_HT20_BIT,          /* 11N-A HT20 channels */
    REGDMN_MODE_11NG_HT40PLUS      = 1 << REGDMN_MODE_11NG_HT40PLUS_BIT,      /* 11N-G HT40 + channels */
    REGDMN_MODE_11NG_HT40MINUS     = 1 << REGDMN_MODE_11NG_HT40MINUS_BIT,     /* 11N-G HT40 - channels */
    REGDMN_MODE_11NA_HT40PLUS      = 1 << REGDMN_MODE_11NA_HT40PLUS_BIT,      /* 11N-A HT40 + channels */
    REGDMN_MODE_11NA_HT40MINUS     = 1 << REGDMN_MODE_11NA_HT40MINUS_BIT,     /* 11N-A HT40 - channels */
    REGDMN_MODE_11AC_VHT20         = 1 << REGDMN_MODE_11AC_VHT20_BIT,         /* 5Ghz, VHT20 */
    REGDMN_MODE_11AC_VHT40PLUS     = 1 << REGDMN_MODE_11AC_VHT40PLUS_BIT,     /* 5Ghz, VHT40 + channels */
    REGDMN_MODE_11AC_VHT40MINUS    = 1 << REGDMN_MODE_11AC_VHT40MINUS_BIT,    /* 5Ghz  VHT40 - channels */
    REGDMN_MODE_11AC_VHT80         = 1 << REGDMN_MODE_11AC_VHT80_BIT,         /* 5Ghz, VHT80 channels */
    REGDMN_MODE_11AC_VHT20_2G      = 1 << REGDMN_MODE_11AC_VHT20_2G_BIT,      /* 2Ghz, VHT20 */
    REGDMN_MODE_11AC_VHT40_2G      = 1 << REGDMN_MODE_11AC_VHT40_2G_BIT,      /* 2Ghz, VHT40 */
    REGDMN_MODE_11AC_VHT80_2G      = 1 << REGDMN_MODE_11AC_VHT80_2G_BIT,      /* 2Ghz, VHT80 */
    REGDMN_MODE_11AC_VHT160        = 1 << REGDMN_MODE_11AC_VHT160_BIT,        /* 5Ghz, VHT160 */
    REGDMN_MODE_11AC_VHT40_2GPLUS  = 1 << REGDMN_MODE_11AC_VHT40_2GPLUS_BIT,  /* 2Ghz, VHT40+ */
    REGDMN_MODE_11AC_VHT40_2GMINUS = 1 << REGDMN_MODE_11AC_VHT40_2GMINUS_BIT, /* 2Ghz, VHT40- */
    REGDMN_MODE_11AC_VHT80_80      = 1 << REGDMN_MODE_11AC_VHT80_80_BIT,      /* 5GHz, VHT80+80 */
};

enum {
    REGDMN_MODE_U32_11AXG_HE20      = 1 << (REGDMN_MODE_11AXG_HE20_BIT - 32),
    REGDMN_MODE_U32_11AXA_HE20      = 1 << (REGDMN_MODE_11AXA_HE20_BIT - 32),
    REGDMN_MODE_U32_11AXG_HE40PLUS  = 1 << (REGDMN_MODE_11AXG_HE40PLUS_BIT - 32),
    REGDMN_MODE_U32_11AXG_HE40MINUS = 1 << (REGDMN_MODE_11AXG_HE40MINUS_BIT - 32),
    REGDMN_MODE_U32_11AXA_HE40PLUS  = 1 << (REGDMN_MODE_11AXA_HE40PLUS_BIT - 32),
    REGDMN_MODE_U32_11AXA_HE40MINUS = 1 << (REGDMN_MODE_11AXA_HE40MINUS_BIT - 32),
    REGDMN_MODE_U32_11AXA_HE80      = 1 << (REGDMN_MODE_11AXA_HE80_BIT - 32),
    REGDMN_MODE_U32_11AXA_HE160     = 1 << (REGDMN_MODE_11AXA_HE160_BIT - 32),
    REGDMN_MODE_U32_11AXA_HE80_80   = 1 << (REGDMN_MODE_11AXA_HE80_80_BIT - 32),
    REGDMN_MODE_U32_11BEG_EHT20      = 1 << (REGDMN_MODE_11BEG_EHT20_BIT - 32),
    REGDMN_MODE_U32_11BEA_EHT20      = 1 << (REGDMN_MODE_11BEA_EHT20_BIT - 32),
    REGDMN_MODE_U32_11BEG_EHT40PLUS  = 1 << (REGDMN_MODE_11BEG_EHT40PLUS_BIT - 32),
    REGDMN_MODE_U32_11BEG_EHT40MINUS = 1 << (REGDMN_MODE_11BEG_EHT40MINUS_BIT - 32),
    REGDMN_MODE_U32_11BEA_EHT40PLUS  = 1 << (REGDMN_MODE_11BEA_EHT40PLUS_BIT - 32),
    REGDMN_MODE_U32_11BEA_EHT40MINUS = 1 << (REGDMN_MODE_11BEA_EHT40MINUS_BIT - 32),
    REGDMN_MODE_U32_11BEA_EHT80      = 1 << (REGDMN_MODE_11BEA_EHT80_BIT - 32),
    REGDMN_MODE_U32_11BEA_EHT160     = 1 << (REGDMN_MODE_11BEA_EHT160_BIT - 32),
    REGDMN_MODE_U32_11BEA_EHT320     = 1 << (REGDMN_MODE_11BEA_EHT320_BIT - 32),
};

#define REGDMN_MODE_ALL       (0xFFFFFFFF)       /* REGDMN_MODE_ALL is defined out of the enum
                                                  * to prevent the ARM compile "warning #66:
                                                  * enumeration value is out of int range"
                                                  * Anyway, this is a BIT-OR of all possible values.
                                                  */

#define REGDMN_CAP1_CHAN_HALF_RATE        0x00000001
#define REGDMN_CAP1_CHAN_QUARTER_RATE     0x00000002
#define REGDMN_CAP1_CHAN_HAL49GHZ         0x00000004


/* regulatory capabilities */
#define REGDMN_EEPROM_EEREGCAP_EN_FCC_MIDBAND   0x0040
#define REGDMN_EEPROM_EEREGCAP_EN_KK_U1_EVEN    0x0080
#define REGDMN_EEPROM_EEREGCAP_EN_KK_U2         0x0100
#define REGDMN_EEPROM_EEREGCAP_EN_KK_MIDBAND    0x0200
#define REGDMN_EEPROM_EEREGCAP_EN_KK_U1_ODD     0x0400
#define REGDMN_EEPROM_EEREGCAP_EN_KK_NEW_11A    0x0800

typedef struct {
    A_UINT32 tlv_header;     /* TLV tag and len; tag equals WMI_TLVTAG_STRUC_HAL_REG_CAPABILITIES */
    A_UINT32 eeprom_rd;      /* regdomain value specified in EEPROM */
    A_UINT32 eeprom_rd_ext;  /* regdomain */
    A_UINT32 regcap1;        /* CAP1 capabilities bit map. */
    A_UINT32 regcap2;        /* REGDMN EEPROM CAP. */
    A_UINT32 wireless_modes; /* REGDMN MODE */
    A_UINT32 low_2ghz_chan;
    A_UINT32 high_2ghz_chan;
    A_UINT32 low_5ghz_chan;
    A_UINT32 high_5ghz_chan;
    A_UINT32 wireless_modes_ext; /* REGDMN MODE ext */
} HAL_REG_CAPABILITIES;

#ifdef NUM_SPATIAL_STREAM
/*
 * The rate control definitions below are only used in the target.
 * (Host-based rate control is no longer applicable.)
 * Maintain the defs in wlanfw_cmn for the sake of existing Rome / Helium
 * targets, but for Lithium targets remove them from wlanfw_cmn and define
 * them in a target-only location instead.
 * SUPPORT_11AX is essentially used as a condition to identify Lithium targets.
 * Some host drivers would also have SUPPORT_11AX defined, and thus would lose
 * the definition of RATE_CODE, RC_TX_DONE_PARAMS, and related macros, but
 * that's okay because the host should have no references to these
 * target-only data structures.
 */
#if !((NUM_SPATIAL_STREAM > 4) || SUPPORT_11AX) /* following N/A for Lithium */

/*
 * Used to update rate-control logic with the status of the tx-completion.
 * In host-based implementation of the rate-control feature, this struture is used to
 * create the payload for HTT message/s from target to host.
 */
#ifndef CONFIG_MOVE_RC_STRUCT_TO_MACCORE
  #if (NUM_SPATIAL_STREAM > 3)
    #define A_RATEMASK A_UINT64
  #else
    #define A_RATEMASK A_UINT32
  #endif
#endif /* CONFIG_MOVE_RC_STRUCT_TO_MACCORE */

typedef A_UINT8 A_RATE;
typedef A_UINT8 A_RATECODE;

#define A_RATEMASK_NUM_OCTET (sizeof (A_RATEMASK))
#define A_RATEMASK_NUM_BITS ((sizeof (A_RATEMASK)) << 3)

typedef struct {
    A_RATECODE rateCode;
    A_UINT8 flags;
} RATE_CODE;

typedef struct {
    RATE_CODE ptx_rc; /* rate code, bw, chain mask sgi */
    A_UINT8 reserved[2];
    A_UINT32 flags;       /* Encodes information such as excessive
                             retransmission, aggregate, some info
                             from .11 frame control,
                             STBC, LDPC, (SGI and Tx Chain Mask
                             are encoded in ptx_rc->flags field),
                             AMPDU truncation (BT/time based etc.),
                             RTS/CTS attempt  */
    A_UINT32 num_enqued;  /* # of MPDUs (for non-AMPDU 1) for this rate */
    A_UINT32 num_retries; /* Total # of transmission attempt for this rate */
    A_UINT32 num_failed;  /* # of failed MPDUs in A-MPDU, 0 otherwise */
    A_UINT32 ack_rssi;    /* ACK RSSI: b'7..b'0 avg RSSI across all chain */
    A_UINT32 time_stamp ; /* ACK timestamp (helps determine age) */
    A_UINT32 is_probe;    /* Valid if probing. Else, 0 */
    A_UINT32 ba_win_size; /* b'7..b0, block Ack Window size, b'31..b8 Resvd */
    A_UINT32 failed_ba_bmap_0_31; /* failed BA bitmap 0..31 */
    A_UINT32 failed_ba_bmap_32_63; /* failed BA bitmap 32..63 */
    A_UINT32 bmap_tried_0_31; /* enqued bitmap 0..31 */
    A_UINT32 bmap_tried_32_63; /* enqued bitmap 32..63 */
} RC_TX_DONE_PARAMS;


#define RC_SET_TX_DONE_INFO(_dst, _rc, _f, _nq, _nr, _nf, _rssi, _ts) \
    do {                                                              \
        (_dst).ptx_rc.rateCode = (_rc).rateCode;                      \
        (_dst).ptx_rc.flags    = (_rc).flags;                         \
        (_dst).flags           = (_f);                                \
        (_dst).num_enqued      = (_nq);                               \
        (_dst).num_retries     = (_nr);                               \
        (_dst).num_failed      = (_nf);                               \
        (_dst).ack_rssi        = (_rssi);                             \
        (_dst).time_stamp      = (_ts);                               \
    } while (0)

#define RC_SET_TXBF_DONE_INFO(_dst, _f)                                 \
    do {                                                                \
        (_dst).flags           |= (_f);                                 \
    } while (0)

/*
 * NOTE: NUM_SCHED_ENTRIES is not used in the host/target interface, but for
 * historical reasons has been defined in the host/target interface files.
 * The NUM_SCHED_ENTRIES definition is being moved into a target-only
 * header file for newer (Lithium) targets, but is being left here for
 * non-Lithium cases, to avoid having to rework legacy targets to move
 * the NUM_SCHED_ENTRIES definition into a target-only header file.
 * Moving the NUM_SCHED_ENTRIES definition into a non-Lithium conditional
 * block should have no impact on the host, since the host does not use
 * NUM_SCHED_ENTRIES.
 */
#define NUM_SCHED_ENTRIES           2

#endif /* !((NUM_SPATIAL_STREAM > 4) || SUPPORT_11AX) */ /* above N/A for Lithium */
#endif /* NUM_SPATIAL_STREAM */

/* NOTE: NUM_DYN_BW cannot be changed without breaking WMI Compatibility */
#define NUM_DYN_BW_MAX              4

/* Some products only use 20/40/80; some use 20/40/80/160 */
#ifndef NUM_DYN_BW
#define NUM_DYN_BW                  3 /* default: support up through 80 MHz */
#endif

#define NUM_DYN_BW_MASK             0x3

#define PROD_SCHED_BW_ENTRIES       (NUM_SCHED_ENTRIES * NUM_DYN_BW)

#if NUM_DYN_BW  > 5
/* Extend rate table module first */
#error "Extend rate table module first"
#endif

#define MAX_IBSS_PEERS 32

#ifdef NUM_SPATIAL_STREAM
/*
 * RC_TX_RATE_SCHEDULE and RC_TX_RATE_INFO defs are used only in the target.
 * (Host-based rate control is no longer applicable.)
 * Maintain the defs in wlanfw_cmn for the sake of existing Rome / Helium
 * targets, but for Lithium targets remove them from wlanfw_cmn and define
 * them in a target-only location instead.
 * SUPPORT_11AX is essentially used as a condition to identify Lithium targets.
 * Some host drivers would also have SUPPORT_11AX defined, and thus would lose
 * the definition of RC_TX_RATE_SCHEDULE and RC_TX_RATE_INFO, but that's okay
 * because the host should have no references to these target-only data
 * structures.
 */
#ifndef CONFIG_MOVE_RC_STRUCT_TO_MACCORE
#if !((NUM_SPATIAL_STREAM > 4) || SUPPORT_11AX)
  #if defined(CONFIG_AR900B_SUPPORT) || defined(AR900B)
  typedef struct{
      A_UINT32    psdu_len[NUM_DYN_BW * NUM_SCHED_ENTRIES];
      A_UINT16    flags[NUM_SCHED_ENTRIES][NUM_DYN_BW];
      A_RATE      rix[NUM_SCHED_ENTRIES][NUM_DYN_BW];
      A_UINT8     tpc[NUM_SCHED_ENTRIES][NUM_DYN_BW];
      A_UINT32    antmask[NUM_SCHED_ENTRIES];
      A_UINT8     num_mpdus[NUM_DYN_BW * NUM_SCHED_ENTRIES];
      A_UINT16    txbf_cv_len;
      A_UINT32    txbf_cv_ptr;
      A_UINT16    txbf_flags;
      A_UINT16    txbf_cv_size;
      A_UINT8     txbf_nc_idx;
      A_UINT8     tries[NUM_SCHED_ENTRIES];
      A_UINT8     bw_mask[NUM_SCHED_ENTRIES];
      A_UINT8     max_bw[NUM_SCHED_ENTRIES];
      A_UINT8     num_sched_entries;
      A_UINT8     paprd_mask;
      A_RATE      rts_rix;
      A_UINT8     sh_pream;
      A_UINT8     min_spacing_1_4_us;
      A_UINT8     fixed_delims;
      A_UINT8     bw_in_service;
      A_RATE      probe_rix;
      A_UINT8     num_valid_rates;
      A_UINT8     rtscts_tpc;
      A_UINT8     dd_profile;
  } RC_TX_RATE_SCHEDULE;
  #else
  typedef struct{
      A_UINT32    psdu_len[NUM_DYN_BW * NUM_SCHED_ENTRIES];
      A_UINT16    flags[NUM_DYN_BW * NUM_SCHED_ENTRIES];
      A_RATE      rix[NUM_DYN_BW * NUM_SCHED_ENTRIES];
      A_UINT8     tpc[NUM_DYN_BW * NUM_SCHED_ENTRIES];
      A_UINT8     num_mpdus[NUM_DYN_BW * NUM_SCHED_ENTRIES];
      A_UINT32    antmask[NUM_SCHED_ENTRIES];
      A_UINT32    txbf_cv_ptr;
      A_UINT16    txbf_cv_len;
      A_UINT8     tries[NUM_SCHED_ENTRIES];
      A_UINT8     num_valid_rates;
      A_UINT8     paprd_mask;
      A_RATE      rts_rix;
      A_UINT8     sh_pream;
      A_UINT8     min_spacing_1_4_us;
      A_UINT8     fixed_delims;
      A_UINT8     bw_in_service;
      A_RATE      probe_rix;
  } RC_TX_RATE_SCHEDULE;
  #endif

  typedef struct{
      A_UINT16    flags[NUM_DYN_BW * NUM_SCHED_ENTRIES];
      A_RATE      rix[NUM_DYN_BW * NUM_SCHED_ENTRIES];
  #ifdef DYN_TPC_ENABLE
      A_UINT8     tpc[NUM_DYN_BW * NUM_SCHED_ENTRIES];
  #endif
  #ifdef SECTORED_ANTENNA
      A_UINT32    antmask[NUM_SCHED_ENTRIES];
  #endif
      A_UINT8     tries[NUM_SCHED_ENTRIES];
      A_UINT8     num_valid_rates;
      A_RATE      rts_rix;
      A_UINT8     sh_pream;
      A_UINT8     bw_in_service;
      A_RATE      probe_rix;
      A_UINT8     dd_profile;
  } RC_TX_RATE_INFO;
#endif /* !((NUM_SPATIAL_STREAM > 4) || SUPPORT_11AX) */
#endif /* CONFIG_MOVE_RC_STRUCT_TO_MACCORE */
#endif

/*
 * Temporarily continue to provide the WHAL_RC_INIT_RC_MASKS def in wlan_defs.h
 * for older targets.
 * The WHAL_RX_INIT_RC_MASKS macro def needs to be moved into ratectrl_11ac.h
 * for all targets, but until this is complete, the WHAL_RC_INIT_RC_MASKS def
 * will be maintained here in its old location.
 */
#ifndef CONFIG_160MHZ_SUPPORT
#define WHAL_RC_INIT_RC_MASKS(_rm) do {                                     \
        _rm[WHAL_RC_MASK_IDX_NON_HT] = A_RATEMASK_OFDM_CCK;                 \
        _rm[WHAL_RC_MASK_IDX_HT_20] = A_RATEMASK_HT_20;                     \
        _rm[WHAL_RC_MASK_IDX_HT_40] = A_RATEMASK_HT_40;                     \
        _rm[WHAL_RC_MASK_IDX_VHT_20] = A_RATEMASK_VHT_20;                   \
        _rm[WHAL_RC_MASK_IDX_VHT_40] = A_RATEMASK_VHT_40;                   \
        _rm[WHAL_RC_MASK_IDX_VHT_80] = A_RATEMASK_VHT_80;                   \
        } while (0)
#endif

/**
 * strucutre describing host memory chunk.
 */
typedef struct {
   A_UINT32   tlv_header;     /* TLV tag and len; tag equals WMITLV_TAG_STRUC_wlan_host_memory_chunk */
   /** id of the request that is passed up in service ready */
   A_UINT32 req_id;
   /** the physical address the memory chunk */
   A_UINT32 ptr;
   /** size of the chunk */
   A_UINT32 size;
    /** ptr_high
     * most significant bits of physical address of the memory chunk
     * Only applicable for addressing more than 32 bit.
     * This will only be non-zero if the target has set
     * WMI_SERVICE_SUPPORT_EXTEND_ADDRESS flag.
     */
   A_UINT32 ptr_high;
} wlan_host_memory_chunk;

#define NUM_UNITS_IS_NUM_VDEVS        0x1
#define NUM_UNITS_IS_NUM_PEERS        0x2
#define NUM_UNITS_IS_NUM_ACTIVE_PEERS 0x4
/* request host to allocate memory contiguously */
#define REQ_TO_HOST_FOR_CONT_MEMORY   0x8

/**
 * structure used by FW for requesting host memory
 */
typedef struct {
    A_UINT32    tlv_header;     /* TLV tag and len; tag equals WMI_TLVTAG_STRUC_wlan_host_mem_req */

    /** ID of the request */
    A_UINT32    req_id;
    /** size of the  of each unit */
    A_UINT32    unit_size;
    /**
     * flags to  indicate that
     * the number units is dependent
     * on number of resources(num vdevs num peers .. etc)
     */
    A_UINT32    num_unit_info;
    /*
     * actual number of units to allocate . if flags in the num_unit_info
     * indicate that number of units is tied to number of a particular
     * resource to allocate then  num_units filed is set to 0 and host
     * will derive the number units from number of the resources it is
     * requesting.
     */
    A_UINT32    num_units;
} wlan_host_mem_req;

typedef enum {
    IGNORE_DTIM = 0x01,
    NORMAL_DTIM = 0x02,
    STICK_DTIM  = 0x03,
    AUTO_DTIM   = 0x04,
} BEACON_DTIM_POLICY;

/* During test it is observed that 6 * 400 = 2400 can
 * be alloced in addition to CFG_TGT_NUM_MSDU_DESC.
 * If there is any change memory requirement, this number
 * needs to be revisited. */
#define TOTAL_VOW_ALLOCABLE 2400
#define VOW_DESC_GRAB_MAX 800

#define VOW_GET_NUM_VI_STA(vow_config) (((vow_config) & 0xffff0000) >> 16)
#define VOW_GET_DESC_PER_VI_STA(vow_config) ((vow_config) & 0x0000ffff)

/***TODO!!! Get these values dynamically in WMI_READY event and use it to calculate the mem req*/
/* size in bytes required for msdu descriptor. If it changes, this should be updated. LARGE_AP
 * case is not considered. LARGE_AP is disabled when VoW is enabled.*/
#define MSDU_DESC_SIZE 20

/* size in bytes required to support a peer in target.
 * This obtained by considering Two tids per peer.
 * peer structure = 168 bytes
 * tid = 96 bytes (per sta 2 means we need 192 bytes)
 * peer_cb = 16 * 2
 * key = 52 * 2
 * AST = 12 * 2
 * rate, reorder.. = 384
 * smart antenna = 50
 */
#define MEMORY_REQ_FOR_PEER 800
/*
 * NB: it is important to keep all the fields in the structure dword long
 * so that it is easy to handle the statistics in BE host.
 */

/*
 * wlan_dbg_tx_stats_v1, _v2:
 * differing versions of the wlan_dbg_tx_stats struct used by different
 * targets
 */
struct wlan_dbg_tx_stats_v1 {
    /* Num HTT cookies queued to dispatch list */
    A_INT32 comp_queued;
    /* Num HTT cookies dispatched */
    A_INT32 comp_delivered;
    /* Num MSDU queued to WAL */
    A_INT32 msdu_enqued;
    /* Num MPDU queue to WAL */
    A_INT32 mpdu_enqued;
    /* Num MSDUs dropped by WMM limit */
    A_INT32 wmm_drop;
    /* Num Local frames queued */
    A_INT32 local_enqued;
    /* Num Local frames done */
    A_INT32 local_freed;
    /* Num queued to HW */
    A_INT32 hw_queued;
    /* Num PPDU reaped from HW */
    A_INT32 hw_reaped;
    /* Num underruns */
    A_INT32 underrun;
    /* Num PPDUs cleaned up in TX abort */
    A_INT32 tx_abort;
    /* Num MPDUs requed by SW */
    A_INT32 mpdus_requed;
    /* excessive retries */
    A_UINT32 tx_ko;
    /* data hw rate code */
    A_UINT32 data_rc;
    /* Scheduler self triggers */
    A_UINT32 self_triggers;
    /* frames dropped due to excessive sw retries */
    A_UINT32 sw_retry_failure;
    /* illegal rate phy errors  */
    A_UINT32 illgl_rate_phy_err;
    /* wal pdev continous xretry */
    A_UINT32 pdev_cont_xretry;
    /* wal pdev continous xretry */
    A_UINT32 pdev_tx_timeout;
    /* wal pdev resets  */
    A_UINT32 pdev_resets;
    /* frames dropped due to non-availability of stateless TIDs */
    A_UINT32 stateless_tid_alloc_failure;
    /* PhY/BB underrun */
    A_UINT32 phy_underrun;
    /* MPDU is more than txop limit */
    A_UINT32 txop_ovf;
};

struct wlan_dbg_tx_stats_v2 {
    /* Num HTT cookies queued to dispatch list */
    A_INT32 comp_queued;
    /* Num HTT cookies dispatched */
    A_INT32 comp_delivered;
    /* Num MSDU queued to WAL */
    A_INT32 msdu_enqued;
    /* Num MPDU queue to WAL */
    A_INT32 mpdu_enqued;
    /* Num MSDUs dropped by WMM limit */
    A_INT32 wmm_drop;
    /* Num Local frames queued */
    A_INT32 local_enqued;
    /* Num Local frames done */
    A_INT32 local_freed;
    /* Num queued to HW */
    A_INT32 hw_queued;
    /* Num PPDU reaped from HW */
    A_INT32 hw_reaped;
    /* Num underruns */
    A_INT32 underrun;
    /* HW Paused. */
    A_UINT32 hw_paused;
    /* Num PPDUs cleaned up in TX abort */
    A_INT32 tx_abort;
    /* Num MPDUs requed by SW */
    A_INT32 mpdus_requed;
    /* excessive retries */
    A_UINT32 tx_ko;
    A_UINT32 tx_xretry;
    /* data hw rate code */
    A_UINT32 data_rc;
    /* Scheduler self triggers */
    A_UINT32 self_triggers;
    /* frames dropped due to excessive sw retries */
    A_UINT32 sw_retry_failure;
    /* illegal rate phy errors  */
    A_UINT32 illgl_rate_phy_err;
    /* wal pdev continous xretry */
    A_UINT32 pdev_cont_xretry;
    /* wal pdev continous xretry */
    A_UINT32 pdev_tx_timeout;
    /* wal pdev resets  */
    A_UINT32 pdev_resets;
    /* frames dropped due to non-availability of stateless TIDs */
    A_UINT32 stateless_tid_alloc_failure;
    /* PhY/BB underrun */
    A_UINT32 phy_underrun;
    /* MPDU is more than txop limit */
    A_UINT32 txop_ovf;
    /* Number of Sequences posted */
    A_UINT32 seq_posted;
    /* Number of Sequences failed queueing */
    A_UINT32 seq_failed_queueing;
    /* Number of Sequences completed */
    A_UINT32 seq_completed;
    /* Number of Sequences restarted */
    A_UINT32 seq_restarted;
    /* Number of MU Sequences posted */
    A_UINT32 mu_seq_posted;
    /* Num MPDUs flushed by SW, HWPAUSED, SW TXABORT (Reset,channel change) */
    A_INT32 mpdus_sw_flush;
    /* Num MPDUs filtered by HW, all filter condition (TTL expired) */
    A_INT32 mpdus_hw_filter;
    /* Num MPDUs truncated by PDG (TXOP, TBTT, PPDU_duration based on rate, dyn_bw) */
    A_INT32 mpdus_truncated;
    /* Num MPDUs that was tried but didn't receive ACK or BA */
    A_INT32 mpdus_ack_failed;
    /* Num MPDUs that was dropped du to expiry. */
    A_INT32 mpdus_expired;
};

#if defined(AR900B)
#define wlan_dbg_tx_stats wlan_dbg_tx_stats_v2
#else
#define wlan_dbg_tx_stats wlan_dbg_tx_stats_v1
#endif

/*
 * wlan_dbg_rx_stats_v1, _v2:
 * differing versions of the wlan_dbg_rx_stats struct used by different
 * targets
 */
struct wlan_dbg_rx_stats_v1 {
    /* Cnts any change in ring routing mid-ppdu */
    A_INT32 mid_ppdu_route_change;
    /* Total number of statuses processed */
    A_INT32 status_rcvd;
    /* Extra frags on rings 0-3 */
    A_INT32 r0_frags;
    A_INT32 r1_frags;
    A_INT32 r2_frags;
    A_INT32 r3_frags;
    /* MSDUs / MPDUs delivered to HTT */
    A_INT32 htt_msdus;
    A_INT32 htt_mpdus;
    /* MSDUs / MPDUs delivered to local stack */
    A_INT32 loc_msdus;
    A_INT32 loc_mpdus;
    /* AMSDUs that have more MSDUs than the status ring size */
    A_INT32 oversize_amsdu;
    /* Number of PHY errors */
    A_INT32 phy_errs;
    /* Number of PHY errors drops */
    A_INT32 phy_err_drop;
    /* Number of mpdu errors - FCS, MIC, ENC etc. */
    A_INT32 mpdu_errs;
};

struct wlan_dbg_rx_stats_v2 {
    /* Cnts any change in ring routing mid-ppdu */
    A_INT32 mid_ppdu_route_change;
    /* Total number of statuses processed */
    A_INT32 status_rcvd;
    /* Extra frags on rings 0-3 */
    A_INT32 r0_frags;
    A_INT32 r1_frags;
    A_INT32 r2_frags;
    A_INT32 r3_frags;
    /* MSDUs / MPDUs delivered to HTT */
    A_INT32 htt_msdus;
    A_INT32 htt_mpdus;
    /* MSDUs / MPDUs delivered to local stack */
    A_INT32 loc_msdus;
    A_INT32 loc_mpdus;
    /* AMSDUs that have more MSDUs than the status ring size */
    A_INT32 oversize_amsdu;
    /* Number of PHY errors */
    A_INT32 phy_errs;
    /* Number of PHY errors drops */
    A_INT32 phy_err_drop;
    /* Number of mpdu errors - FCS, MIC, ENC etc. */
    A_INT32 mpdu_errs;
    /* Number of rx overflow errors. */
    A_INT32 rx_ovfl_errs;
};

#if defined(AR900B)
#define wlan_dbg_rx_stats wlan_dbg_rx_stats_v2
#else
#define wlan_dbg_rx_stats wlan_dbg_rx_stats_v1
#endif

struct wlan_dbg_mem_stats {
    A_UINT32 iram_free_size;
    A_UINT32 dram_free_size;
};

struct wlan_dbg_peer_stats {

	A_INT32 dummy; /* REMOVE THIS ONCE REAL PEER STAT COUNTERS ARE ADDED */
};

/*
 * wlan_dbg_rx_rate_info_v1a_t, _v1b_t:
 * differing versions of the wlan_dbg_rx_rate_info struct used by different
 * targets
 */
typedef struct {
    A_UINT32 mcs[10];
    A_UINT32 sgi[10];
    A_UINT32 nss[4];
    A_UINT32 nsts;
    A_UINT32 stbc[10];
    A_UINT32 bw[3];
    A_UINT32 pream[6];
    A_UINT32 ldpc;
    A_UINT32 txbf;
    A_UINT32 mgmt_rssi;
    A_UINT32 data_rssi;
    A_UINT32 rssi_chain0;
    A_UINT32 rssi_chain1;
    A_UINT32 rssi_chain2;
} wlan_dbg_rx_rate_info_v1a_t;

typedef struct {
    A_UINT32 mcs[10];
    A_UINT32 sgi[10];
    A_UINT32 nss[4];
    A_UINT32 nsts;
    A_UINT32 stbc[10];
    A_UINT32 bw[3];
    A_UINT32 pream[6];
    A_UINT32 ldpc;
    A_UINT32 txbf;
    A_UINT32 mgmt_rssi;
    A_UINT32 data_rssi;
    A_UINT32 rssi_chain0;
    A_UINT32 rssi_chain1;
    A_UINT32 rssi_chain2;
/*
 * TEMPORARY: leave rssi_chain3 in place for AR900B builds until code using
 * rssi_chain3 has been converted to use wlan_dbg_rx_rate_info_v2_t.
 */
	A_UINT32 rssi_chain3;
} wlan_dbg_rx_rate_info_v1b_t;

#if defined(AR900B)
#define wlan_dbg_rx_rate_info_t wlan_dbg_rx_rate_info_v1b_t
#else
#define wlan_dbg_rx_rate_info_t wlan_dbg_rx_rate_info_v1a_t
#endif

typedef struct {
    A_UINT32 mcs[10];
    A_UINT32 sgi[10];
/*
 * TEMPORARY: leave nss conditionally defined, until all code that
 * requires nss[4] is converted to use wlan_dbg_tx_rate_info_v2_t.
 * At that time, this nss array will be made length = 3 unconditionally.
 */
#if defined(CONFIG_AR900B_SUPPORT) || defined(AR900B)
    A_UINT32 nss[4];
#else
    A_UINT32 nss[3];
#endif
    A_UINT32 stbc[10];
    A_UINT32 bw[3];
    A_UINT32 pream[4];
    A_UINT32 ldpc;
    A_UINT32 rts_cnt;
    A_UINT32 ack_rssi;
} wlan_dbg_tx_rate_info_t ;

#define WLAN_MAX_MCS 10

typedef struct {
    A_UINT32 mcs[WLAN_MAX_MCS];
    A_UINT32 sgi[WLAN_MAX_MCS];
    A_UINT32 nss[MAX_SPATIAL_STREAM_ANY_V2];
    A_UINT32 nsts;
    A_UINT32 stbc[WLAN_MAX_MCS];
    A_UINT32 bw[NUM_DYN_BW_MAX];
    A_UINT32 pream[6];
    A_UINT32 ldpc;
    A_UINT32 txbf;
    A_UINT32 mgmt_rssi;
    A_UINT32 data_rssi;
    A_UINT32 rssi_chain0;
    A_UINT32 rssi_chain1;
    A_UINT32 rssi_chain2;
    A_UINT32 rssi_chain3;
    A_UINT32 reserved[8];
} wlan_dbg_rx_rate_info_v2_t;

typedef struct {
    A_UINT32 mcs[WLAN_MAX_MCS];
    A_UINT32 sgi[WLAN_MAX_MCS];
    A_UINT32 nss[MAX_SPATIAL_STREAM_ANY_V2];
    A_UINT32 stbc[WLAN_MAX_MCS];
    A_UINT32 bw[NUM_DYN_BW_MAX];
    A_UINT32 pream[4];
    A_UINT32 ldpc;
    A_UINT32 rts_cnt;
    A_UINT32 ack_rssi;
    A_UINT32 reserved[8];
} wlan_dbg_tx_rate_info_v2_t;

typedef struct {
    A_UINT32 mcs[WLAN_MAX_MCS];
    A_UINT32 sgi[WLAN_MAX_MCS];
    A_UINT32 nss[MAX_SPATIAL_STREAM_ANY_V3];
    A_UINT32 nsts;
    A_UINT32 stbc[WLAN_MAX_MCS];
    A_UINT32 bw[NUM_DYN_BW_MAX];
    A_UINT32 pream[6];
    A_UINT32 ldpc;
    A_UINT32 txbf;
    A_UINT32 mgmt_rssi;
    A_UINT32 data_rssi;
    A_UINT32 rssi_chain0;
    A_UINT32 rssi_chain1;
    A_UINT32 rssi_chain2;
    A_UINT32 rssi_chain3;
    A_UINT32 reserved[8];
} wlan_dbg_rx_rate_info_v3_t;

typedef struct {
    A_UINT32 mcs[WLAN_MAX_MCS];
    A_UINT32 sgi[WLAN_MAX_MCS];
    A_UINT32 nss[MAX_SPATIAL_STREAM_ANY_V3];
    A_UINT32 stbc[WLAN_MAX_MCS];
    A_UINT32 bw[NUM_DYN_BW_MAX];
    A_UINT32 pream[4];
    A_UINT32 ldpc;
    A_UINT32 rts_cnt;
    A_UINT32 ack_rssi;
    A_UINT32 reserved[8];
} wlan_dbg_tx_rate_info_v3_t;

#define WHAL_DBG_PHY_ERR_MAXCNT 18
#define WHAL_DBG_SIFS_STATUS_MAXCNT 8
#define WHAL_DBG_SIFS_ERR_MAXCNT 8
#define WHAL_DBG_CMD_RESULT_MAXCNT 11
#define WHAL_DBG_CMD_STALL_ERR_MAXCNT 4
#define WHAL_DBG_FLUSH_REASON_MAXCNT 40

typedef enum {
    WIFI_URRN_STATS_FIRST_PKT,
    WIFI_URRN_STATS_BETWEEN_MPDU,
    WIFI_URRN_STATS_WITHIN_MPDU,
    WHAL_MAX_URRN_STATS
} wifi_urrn_type_t;

typedef struct wlan_dbg_txbf_snd_stats {
    A_UINT32 cbf_20[4];
    A_UINT32 cbf_40[4];
    A_UINT32 cbf_80[4];
    A_UINT32 sounding[9];
    A_UINT32 cbf_160[4];
} wlan_dbg_txbf_snd_stats_t;

typedef struct wlan_dbg_wifi2_error_stats {
    A_UINT32 urrn_stats[WHAL_MAX_URRN_STATS];
    A_UINT32 flush_errs[WHAL_DBG_FLUSH_REASON_MAXCNT];
    A_UINT32 schd_stall_errs[WHAL_DBG_CMD_STALL_ERR_MAXCNT];
    A_UINT32 schd_cmd_result[WHAL_DBG_CMD_RESULT_MAXCNT];
    A_UINT32 sifs_status[WHAL_DBG_SIFS_STATUS_MAXCNT];
    A_UINT8  phy_errs[WHAL_DBG_PHY_ERR_MAXCNT];
    A_UINT32 rx_rate_inval;
} wlan_dbg_wifi2_error_stats_t;

typedef struct wlan_dbg_wifi2_error2_stats {
    A_UINT32 schd_errs[WHAL_DBG_CMD_STALL_ERR_MAXCNT];
    A_UINT32 sifs_errs[WHAL_DBG_SIFS_ERR_MAXCNT];
} wlan_dbg_wifi2_error2_stats_t;

#define WLAN_DBG_STATS_SIZE_TXBF_VHT 10
#define WLAN_DBG_STATS_SIZE_TXBF_HT 8
#define WLAN_DBG_STATS_SIZE_TXBF_OFDM 8
#define WLAN_DBG_STATS_SIZE_TXBF_CCK 7

typedef struct wlan_dbg_txbf_data_stats {
    A_UINT32 tx_txbf_vht[WLAN_DBG_STATS_SIZE_TXBF_VHT];
    A_UINT32 rx_txbf_vht[WLAN_DBG_STATS_SIZE_TXBF_VHT];
    A_UINT32 tx_txbf_ht[WLAN_DBG_STATS_SIZE_TXBF_HT];
    A_UINT32 tx_txbf_ofdm[WLAN_DBG_STATS_SIZE_TXBF_OFDM];
    A_UINT32 tx_txbf_cck[WLAN_DBG_STATS_SIZE_TXBF_CCK];
} wlan_dbg_txbf_data_stats_t;

struct wlan_dbg_tx_mu_stats {
    A_UINT32 mu_sch_nusers_2;
    A_UINT32 mu_sch_nusers_3;
    A_UINT32 mu_mpdus_queued_usr[4];
    A_UINT32 mu_mpdus_tried_usr[4];
    A_UINT32 mu_mpdus_failed_usr[4];
    A_UINT32 mu_mpdus_requeued_usr[4];
    A_UINT32 mu_err_no_ba_usr[4];
    A_UINT32 mu_mpdu_underrun_usr[4];
    A_UINT32 mu_ampdu_underrun_usr[4];
};

struct wlan_dbg_tx_selfgen_stats {
    A_UINT32 su_ndpa;
    A_UINT32 su_ndp;
    A_UINT32 mu_ndpa;
    A_UINT32 mu_ndp;
    A_UINT32 mu_brpoll_1;
    A_UINT32 mu_brpoll_2;
    A_UINT32 mu_bar_1;
    A_UINT32 mu_bar_2;
    A_UINT32 cts_burst;
    A_UINT32 su_ndp_err;
    A_UINT32 su_ndpa_err;
    A_UINT32 mu_ndp_err;
    A_UINT32 mu_brp1_err;
    A_UINT32 mu_brp2_err;
};

typedef struct wlan_dbg_sifs_resp_stats {
    A_UINT32 ps_poll_trigger;       /* num ps-poll trigger frames */
    A_UINT32 uapsd_trigger;         /* num uapsd trigger frames */
    A_UINT32 qb_data_trigger[2];    /* num data trigger frames; idx 0: explicit and idx 1: implicit */
    A_UINT32 qb_bar_trigger[2];     /* num bar trigger frames;  idx 0: explicit and idx 1: implicit */
    A_UINT32 sifs_resp_data;        /* num ppdus transmitted at SIFS interval */
    A_UINT32 sifs_resp_err;         /* num ppdus failed to meet SIFS resp timing */
} wlan_dgb_sifs_resp_stats_t;



/** wlan_dbg_wifi2_error_stats_t is not grouped with the
 *  following structure as it is allocated differently and only
 *  belongs to whal
 */
typedef struct wlan_dbg_stats_wifi2 {
    wlan_dbg_txbf_snd_stats_t txbf_snd_info;
    wlan_dbg_txbf_data_stats_t txbf_data_info;
    struct wlan_dbg_tx_selfgen_stats tx_selfgen;
    struct wlan_dbg_tx_mu_stats tx_mu;
    wlan_dgb_sifs_resp_stats_t sifs_resp_info;
} wlan_dbg_wifi2_stats_t;

/*
 * wlan_dbg_rx_rate_info_v1a, _v1b:
 * differing versions of the wlan_dbg_rx_rate_info struct used by different
 * targets
 */
typedef struct {
    wlan_dbg_rx_rate_info_v1a_t rx_phy_info;
    wlan_dbg_tx_rate_info_t tx_rate_info;
} wlan_dbg_rate_info_v1a_t;

typedef struct {
    wlan_dbg_rx_rate_info_v1b_t rx_phy_info;
    wlan_dbg_tx_rate_info_t tx_rate_info;
} wlan_dbg_rate_info_v1b_t;

#if defined(AR900B)
#define wlan_dbg_rate_info_t wlan_dbg_rate_info_v1b_t
#else
#define wlan_dbg_rate_info_t wlan_dbg_rate_info_v1a_t
#endif

typedef struct {
    wlan_dbg_rx_rate_info_v2_t rx_phy_info;
    wlan_dbg_tx_rate_info_v2_t tx_rate_info;
} wlan_dbg_rate_info_v2_t;

/*
 * wlan_dbg_stats_v1, _v2:
 * differing versions of the wlan_dbg_stats struct used by different
 * targets
 */
struct wlan_dbg_stats_v1 {
    struct wlan_dbg_tx_stats_v1 tx;
    struct wlan_dbg_rx_stats_v1 rx;
    struct wlan_dbg_peer_stats peer;
};

struct wlan_dbg_stats_v2 {
    struct wlan_dbg_tx_stats_v2 tx;
    struct wlan_dbg_rx_stats_v2 rx;
    struct wlan_dbg_mem_stats mem;
    struct wlan_dbg_peer_stats peer;
};

#if defined(AR900B)
#define wlan_dbg_stats wlan_dbg_stats_v2
#else
#define wlan_dbg_stats wlan_dbg_stats_v1
#endif

#define DBG_STATS_MAX_HWQ_NUM 10
#define DBG_STATS_MAX_TID_NUM 20
#define DBG_STATS_MAX_CONG_NUM 16
struct wlan_dbg_txq_stats {
    A_UINT16 num_pkts_queued[DBG_STATS_MAX_HWQ_NUM];
    A_UINT16 tid_hw_qdepth[DBG_STATS_MAX_TID_NUM]; /* WAL_MAX_TID is 20 */
    A_UINT16 tid_sw_qdepth[DBG_STATS_MAX_TID_NUM]; /* WAL_MAX_TID is 20 */
};

struct wlan_dbg_tidq_stats {
    A_UINT32 wlan_dbg_tid_txq_status;
    struct wlan_dbg_txq_stats txq_st;
};

typedef enum {
    WLAN_DBG_DATA_STALL_NONE = 0,
    WLAN_DBG_DATA_STALL_VDEV_PAUSE,         /* 1 */
    WLAN_DBG_DATA_STALL_HWSCHED_CMD_FILTER, /* 2 */
    WLAN_DBG_DATA_STALL_HWSCHED_CMD_FLUSH,  /* 3 */
    WLAN_DBG_DATA_STALL_RX_REFILL_FAILED,   /* 4 */
    WLAN_DBG_DATA_STALL_RX_FCS_LEN_ERROR,   /* 5 */
    WLAN_DBG_DATA_STALL_MAC_WDOG_ERRORS,    /* 6 */ /* Mac watch dog */
    WLAN_DBG_DATA_STALL_PHY_BB_WDOG_ERROR,  /* 7 */ /* PHY watch dog */
    WLAN_DBG_DATA_STALL_POST_TIM_NO_TXRX_ERROR, /* 8 */
    WLAN_DBG_DATA_STALL_MAX,
} wlan_dbg_data_stall_type_e;

typedef enum {
    WLAN_DBG_DATA_STALL_RECOVERY_NONE = 0,
    WLAN_DBG_DATA_STALL_RECOVERY_CONNECT_DISCONNECT,
    WLAN_DBG_DATA_STALL_RECOVERY_CONNECT_MAC_PHY_RESET,
    WLAN_DBG_DATA_STALL_RECOVERY_CONNECT_PDR,
    WLAN_DBG_DATA_STALL_RECOVERY_CONNECT_SSR,
} wlan_dbg_data_stall_recovery_type_e;

/*
 * NOTE: If necessary, restore the explicit disabling of CONFIG_160MHZ_SUPPORT
 * See the corresponding comment + pre-processor block at the top of the file.
 */
#ifdef CONFIG_160MHZ_SUPPORT_UNDEF_WAR
    #define CONFIG_160MHZ_SUPPORT 0
    #undef CONFIG_160MHZ_SUPPORT_UNDEF_WAR
#endif

#endif /* __WLANDEFS_H__ */
