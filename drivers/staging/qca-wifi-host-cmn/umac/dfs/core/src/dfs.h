/*
 * Copyright (c) 2013, 2016-2019 The Linux Foundation.  All rights reserved.
 * Copyright (c) 2005-2006 Atheros Communications, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: This file has main dfs structures.
 */

#ifndef _DFS_H_
#define _DFS_H_

#include <qdf_types.h>       /* QDF_NBUF_EXEMPT_NO_EXEMPTION, etc. */
#include <qdf_net_types.h>   /* QDF_NBUF_EXEMPT_NO_EXEMPTION, etc. */
#include <qdf_nbuf.h>        /* qdf_nbuf_t, etc. */
#include <qdf_util.h>        /* qdf_assert */
#include <qdf_lock.h>        /* qdf_spinlock */
#include <qdf_time.h>
#include <qdf_timer.h>

#include <wlan_dfs_ioctl.h>
#include "dfs_structs.h"
#include "dfs_channel.h"
#include "dfs_ioctl_private.h"
#include <i_qdf_types.h>     /* For qdf_packed*/
#include <queue.h>           /* For STAILQ_ENTRY */
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <osdep.h>

/* File Line and Submodule String */
#define FLSM(x, str)   #str " : " FL(x)
/* Cast to dfs type */
#define DC(x)  ((struct wlan_dfs *)(x))

/**
 * dfs_log: dfs logging using submodule MASKs and
 * QDF trace level.
 * The logging is controlled by two bitmasks:
 * 1) submodule bitmask: sm
 * 2) trace level masks: level
 *
 * @dfs: The dfs object pointer or NULL if dfs is not defined.
 * @sm: Submodule BITMASK.
 * @level: QDF trace level.
 * @args...: Variable argument list.
 *
 * The submodule(sm) cannot be empty even if argument dfs is NULL.
 * Else the macro will create a  compilation  error.
 * One may provide WLAN_DEBUG_DFS_ALWAYS when  the argument dfs is NULL.
 * Example:-
 * dfs_log(NULL, WLAN_DEBUG_DFS_ALWAYS, QDF_TRACE_LEVEL_INFO,"Error pulse");
 *
 * Why DC(x) is required?
 * Since NULL is defined as ((void *)(0)), if the argument "dfs"
 * in a call to the macro "dfs_log" is NULL
 * then during compilation (NULL)->dfs_debug_mask will dereference
 * a (void *) type, which is illegal. Therefore, we need
 * the cast: (DC(dfs))->dfs_debug_mask.
 * Example:-
 * dfs_log(NULL, WLAN_DEBUG_DFS, QDF_TRACE_LEVEL_INFO,"dfs is NULL");
 */
#define dfs_log(dfs, sm, level, args...)  do {        \
	if (((dfs) == NULL) ||                            \
			((sm) == WLAN_DEBUG_DFS_ALWAYS) ||        \
			((sm) & ((DC(dfs))->dfs_debug_mask))) {   \
		QDF_TRACE(QDF_MODULE_ID_DFS, level, ## args); \
	}                                                 \
} while (0)

#define dfs_logfl(dfs, level, sm, format, args...) \
	dfs_log(dfs, sm, level, FLSM(format, sm), ## args)

#define dfs_alert(dfs, sm, format, args...) \
	dfs_logfl(dfs, QDF_TRACE_LEVEL_FATAL, sm, format, ## args)

#define dfs_err(dfs, sm, format, args...) \
	dfs_logfl(dfs, QDF_TRACE_LEVEL_ERROR, sm, format, ## args)

#define dfs_warn(dfs, sm, format, args...) \
	dfs_logfl(dfs, QDF_TRACE_LEVEL_WARN, sm, format, ## args)

#define dfs_info(dfs, sm, format, args...) \
	dfs_logfl(dfs, QDF_TRACE_LEVEL_INFO, sm, format, ## args)

#define dfs_debug(dfs, sm, format, args...) \
	dfs_logfl(dfs, QDF_TRACE_LEVEL_DEBUG, sm, format, ## args)

#define DFS_MIN(a, b) ((a) < (b)?(a):(b))
#define DFS_MAX(a, b) ((a) > (b)?(a) : (b))
#define DFS_DIFF(a, b)(DFS_MAX(a, b) - DFS_MIN(a, b))

/**
 * Maximum number of radar events to be processed in a single iteration.
 * Allows soft watchdog to run.
 */
#define MAX_EVENTS 100

/**
 * Constants to use for chirping detection.
 *
 * All are unconverted as HW reports them.
 *
 * XXX Are these constants with or without fast clock 5GHz operation?
 * XXX Peregrine reports pulses in microseconds, not hardware clocks!
 */

#define MAX_DUR_FOR_LOW_RSSI 4

/**
 * Cascade has issue with reported duration especially when there is a
 * crossover of chirp from one segment to another. It may report a value
 * of duration that is well below 50us for a valid FCC type 5 chirping
 * pulse. For now changing minimum duration as a work around. This will
 * affect all chips but since we detect chirp with Merlin+, we may be OK
 * for now. We need a more robust solution for this.
 */
#define MIN_BIN5_DUR_CAS            25 /* 50 * 1.25*/
#define MIN_BIN5_DUR_MICROSEC_CAS   20
#define MIN_BIN5_DUR                63 /* 50 * 1.25*/
#define MIN_BIN5_DUR_MICROSEC       50
#define MAYBE_BIN5_DUR              35 /* 28 * 1.25*/
#define MAYBE_BIN5_DUR_MICROSEC     28

/* Conversion is already done using dfs->dur_multiplier */
#define MAX_BIN5_DUR                145   /* use 145 for osprey */
#define MAX_BIN5_DUR_MICROSEC       105

#define DFS_MARGIN_EQUAL(a, b, margin)	((DFS_DIFF(a, b)) <= margin)
#define DFS_MAX_STAGGERED_BURSTS    3

/**
 * All filter thresholds in the radar filter tables are effective at a 50%
 * channel loading.
 */
#define DFS_CHAN_LOADING_THRESH     50
#define DFS_EXT_CHAN_LOADING_THRESH 30
#define DFS_DEFAULT_PRI_MARGIN      6
#define DFS_DEFAULT_FIXEDPATTERN_PRI_MARGIN	4

#define WLAN_DFSQ_LOCK(_dfs)         qdf_spin_lock_bh(&(_dfs)->dfs_radarqlock)
#define WLAN_DFSQ_UNLOCK(_dfs)       qdf_spin_unlock_bh(&(_dfs)->dfs_radarqlock)
#define WLAN_DFSQ_LOCK_CREATE(_dfs)  qdf_spinlock_create( \
		&(_dfs)->dfs_radarqlock)
#define WLAN_DFSQ_LOCK_DESTROY(_dfs) qdf_spinlock_destroy( \
		&(_dfs)->dfs_radarqlock)

#define WLAN_ARQ_LOCK(_dfs)          qdf_spin_lock_bh(&(_dfs)->dfs_arqlock)
#define WLAN_ARQ_UNLOCK(_dfs)        qdf_spin_unlock_bh(&(_dfs)->dfs_arqlock)
#define WLAN_ARQ_LOCK_CREATE(_dfs)   qdf_spinlock_create(&(_dfs)->dfs_arqlock)
#define WLAN_ARQ_LOCK_DESTROY(_dfs)  qdf_spinlock_destroy(&(_dfs)->dfs_arqlock)

#define WLAN_DFSEVENTQ_LOCK(_dfs)         qdf_spin_lock_bh(&(_dfs)->dfs_eventqlock)
#define WLAN_DFSEVENTQ_UNLOCK(_dfs)       qdf_spin_unlock_bh( \
		&(_dfs)->dfs_eventqlock)
#define WLAN_DFSEVENTQ_LOCK_CREATE(_dfs)  qdf_spinlock_create( \
		&(_dfs)->dfs_eventqlock)
#define WLAN_DFSEVENTQ_LOCK_DESTROY(_dfs) qdf_spinlock_destroy( \
		&(_dfs)->dfs_eventqlock)

#define WLAN_DFSNOL_LOCK(_dfs)         qdf_spin_lock_bh(&(_dfs)->dfs_nol_lock)
#define WLAN_DFSNOL_UNLOCK(_dfs)       qdf_spin_unlock_bh(&(_dfs)->dfs_nol_lock)
#define WLAN_DFSNOL_LOCK_CREATE(_dfs)  qdf_spinlock_create( \
		&(_dfs)->dfs_nol_lock)
#define WLAN_DFSNOL_LOCK_DESTROY(_dfs) qdf_spinlock_destroy( \
		&(_dfs)->dfs_nol_lock)

#define PRECAC_LIST_LOCK(_dfs)         qdf_spin_lock_irqsave( \
		&(_dfs)->dfs_precac_lock)
#define PRECAC_LIST_UNLOCK(_dfs)       qdf_spin_unlock_irqrestore( \
		&(_dfs)->dfs_precac_lock)
#define PRECAC_LIST_LOCK_CREATE(_dfs)  qdf_spinlock_create( \
		&(_dfs)->dfs_precac_lock)
#define PRECAC_LIST_LOCK_DESTROY(_dfs) qdf_spinlock_destroy( \
		&(_dfs)->dfs_precac_lock)

#define WLAN_DFS_DATA_STRUCT_LOCK(_dfs) \
	qdf_spin_lock_bh(&(_dfs)->dfs_data_struct_lock)
#define WLAN_DFS_DATA_STRUCT_UNLOCK(_dfs) \
	qdf_spin_unlock_bh(&(_dfs)->dfs_data_struct_lock)
#define WLAN_DFS_DATA_STRUCT_LOCK_CREATE(_dfs) \
	qdf_spinlock_create(&(_dfs)->dfs_data_struct_lock)
#define WLAN_DFS_DATA_STRUCT_LOCK_DESTROY(_dfs) \
	qdf_spinlock_destroy(&(_dfs)->dfs_data_struct_lock)

/* Mask for time stamp from descriptor */
#define DFS_TSMASK    0xFFFFFFFF
/* Shift for time stamp from descriptor */
#define DFS_TSSHIFT   32
/* 64 bit TSF wrap value */
#define DFS_TSF_WRAP  0xFFFFFFFFFFFFFFFFULL
/* TS mask for 64 bit value */
#define DFS_64BIT_TSFMASK 0x0000000000007FFFULL

#define DFS_AR_RADAR_RSSI_THR          5 /* in dB */
#define DFS_AR_RADAR_RESET_INT         1 /* in secs */
#define DFS_AR_RADAR_MAX_HISTORY       500
#define DFS_AR_REGION_WIDTH            128
#define DFS_AR_RSSI_THRESH_STRONG_PKTS 17 /* in dB */
#define DFS_AR_RSSI_DOUBLE_THRESHOLD   15 /* in dB */
#define DFS_AR_MAX_NUM_ACK_REGIONS     9
#define DFS_AR_ACK_DETECT_PAR_THRESH   20
#define DFS_AR_PKT_COUNT_THRESH        20

#define DFS_MAX_DL_SIZE                64
#define DFS_MAX_DL_MASK                0x3F

#define DFS_NOL_TIME DFS_NOL_TIMEOUT_US
/* 30 minutes in usecs */

#define DFS_WAIT_TIME (60*1000000) /* 1 minute in usecs */

#define DFS_DISABLE_TIME (3*60*1000000) /* 3 minutes in usecs */

#define DFS_MAX_B5_SIZE 128
#define DFS_MAX_B5_MASK 0x0000007F /* 128 */

/* Max number of overlapping filters */
#define DFS_MAX_RADAR_OVERLAP 16

/* Max number of dfs events which can be q'd */
#define DFS_MAX_EVENTS 1024

#define DFS_RADAR_EN       0x80000000 /* Radar detect is capable */
#define DFS_AR_EN          0x40000000 /* AR detect is capable */
/* Radar detect in second segment is capable */
#define DFS_SECOND_SEGMENT_RADAR_EN 0x20000000
#define DFS_MAX_RSSI_VALUE 0x7fffffff /* Max rssi value */

#define DFS_BIN_MAX_PULSES 60 /* max num of pulses in a burst */
#define DFS_BIN5_PRI_LOWER_LIMIT 990 /* us */

/**
 * To cover the single pusle burst case, change from 2010 us to
 * 2010000 us.
 */

/**
 * This is reverted back to 2010 as larger value causes false
 * bin5 detect (EV76432, EV76320)
 */
#define DFS_BIN5_PRI_HIGHER_LIMIT 2010 /* us */

#define DFS_BIN5_WIDTH_MARGIN 4 /* us */
#define DFS_BIN5_RSSI_MARGIN  5 /* dBm */

/**
 * Following threshold is not specified but should be
 * okay statistically.
 */
#define DFS_BIN5_BRI_LOWER_LIMIT 300000   /* us */
#define DFS_BIN5_BRI_UPPER_LIMIT 12000000 /* us */

/* Max number of pulses kept in buffer */
#define DFS_MAX_PULSE_BUFFER_SIZE   1024
#define DFS_MAX_PULSE_BUFFER_MASK   0x3ff

#define DFS_FAST_CLOCK_MULTIPLIER    (800/11)
#define DFS_NO_FAST_CLOCK_MULTIPLIER (80)
#define DFS_BIG_SIDX 10000

/* Min value of valid psidx diff */
#define DFS_MIN_PSIDX_DIFF 4
/* Max value of valid psidx diff */
#define DFS_MAX_PSIDX_DIFF 16

/**
 * Software use: channel interference used for as AR as well as RADAR
 * interference detection.
 */
#define CHANNEL_INTERFERENCE    0x01

#define CHANNEL_2GHZ      0x00080 /* 2 GHz spectrum channel. */
#define CHANNEL_OFDM      0x00040 /* OFDM channel */
#define CHANNEL_TURBO     0x00010 /* Turbo Channel */
#define CHANNEL_108G (CHANNEL_2GHZ|CHANNEL_OFDM|CHANNEL_TURBO)

/* qdf_packed - denotes structure is packed. */
#define qdf_packed __qdf_packed

#define SEG_ID_PRIMARY         0
#define SEG_ID_SECONDARY       1

/* MIN and MAX width for different regions */
#define REG0_MIN_WIDTH 33
#define REG0_MAX_WIDTH 38
#define REG1_MIN_WIDTH 39
#define REG1_MAX_WIDTH 44
#define REG2_MIN_WIDTH 53
#define REG2_MAX_WIDTH 58
#define REG3_MIN_WIDTH 126
#define REG3_MAX_WIDTH 140
#define REG4_MIN_WIDTH 141
#define REG4_MAX_WIDTH 160
#define REG5_MIN_WIDTH 189
#define REG5_MAX_WIDTH 210
#define REG6_MIN_WIDTH 360
#define REG6_MAX_WIDTH 380
#define REG7_MIN_WIDTH 257
#define REG7_MAX_WIDTH 270
#define REG8_MIN_WIDTH 295
#define REG8_MAX_WIDTH 302

#define OVER_SAMPLING_FREQ 44000
#define SAMPLING_FREQ 40000
#define HUNDRED 100
#define NUM_BINS 128
#define THOUSAND 1000

/* ETSI11_WORLD regdmn pair id */
#define ETSI11_WORLD_REGDMN_PAIR_ID 0x26
#define ETSI12_WORLD_REGDMN_PAIR_ID 0x28
#define ETSI13_WORLD_REGDMN_PAIR_ID 0x27
#define ETSI14_WORLD_REGDMN_PAIR_ID 0x29

/* Array offset to ETSI legacy pulse */
#define ETSI_LEGACY_PULSE_ARR_OFFSET 2

#define ETSI_RADAR_EN302_502_FREQ_LOWER 5725
#define ETSI_RADAR_EN302_502_FREQ_UPPER 5865

#define DFS_NOL_ADD_CHAN_LOCKED(dfs, freq, timeout)         \
	do {                                                \
		WLAN_DFSNOL_LOCK(dfs);                      \
		dfs_nol_addchan(dfs, freq, timeout);        \
		WLAN_DFSNOL_UNLOCK(dfs);                    \
	} while (0)

/*
 * Free the NOL element in a thread. This is to avoid freeing the
 * timer object from within timer callback function . The nol element
 * contains the timer Object.
 */
#define DFS_NOL_DELETE_CHAN_LOCKED(dfs, freq, chwidth)      \
	do {                                                \
		WLAN_DFSNOL_LOCK(dfs);                      \
		dfs_nol_delete(dfs, freq, chwidth);         \
		qdf_sched_work(NULL, &dfs->dfs_nol_elem_free_work); \
		WLAN_DFSNOL_UNLOCK(dfs);                    \
	} while (0)

#define DFS_GET_NOL_LOCKED(dfs, dfs_nol, nchan)             \
	do {                                                \
		WLAN_DFSNOL_LOCK(dfs);                      \
		dfs_get_nol(dfs, dfs_nol, nchan);           \
		WLAN_DFSNOL_UNLOCK(dfs);                    \
	} while (0)

#define DFS_PRINT_NOL_LOCKED(dfs)                           \
	do {                                                \
		WLAN_DFSNOL_LOCK(dfs);                      \
		dfs_print_nol(dfs);                         \
		WLAN_DFSNOL_UNLOCK(dfs);                    \
	} while (0)

#define DFS_NOL_FREE_LIST_LOCKED(dfs)                       \
	do {                                                \
		WLAN_DFSNOL_LOCK(dfs);                      \
		dfs_nol_free_list(dfs);                     \
		WLAN_DFSNOL_UNLOCK(dfs);                    \
	} while (0)

/* Host sends the average parameters of the radar pulses and starts the status
 * wait timer with this timeout.
 */
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
#define HOST_DFS_STATUS_WAIT_TIMER_MS 200
#endif

/*
 * USENOL_DISABLE_NOL_HOST_AND_FW : Do not add radar hit channel to NOL
 * in host and FW. Enable CSA on the same channel.
 */
#define USENOL_DISABLE_NOL_HOST_AND_FW 0
/*
 * USENOL_ENABLE_NOL_HOST_AND_FW : Add the radar hit channel to NOL in
 * host and FW (in case of FO). NOL timer cannot be configured by the user
 * as FW does not allow manipulating NOL timeout. If noltimeout is configured,
 * (say 1 min) FW will not be intimated about the configuration and hence NOL
 * timer may elapse at different instances in host (after 1 min) and FW (after
 * default 30 min) which could lead to DFS Violation if host tries to come up
 * on the channel after host NOL timeout (of 1 min) as the FW would still
 * have the channel in NOL list.
 */
#define USENOL_ENABLE_NOL_HOST_AND_FW 1
/*
 * USENOL_ENABLE_NOL_HOST_DISABLE_NOL_FW : Add the radar hit channel to NOL
 * in host. NOL timer can be configured by user. NOL in FW (for FO) is disabled.
 */
#define USENOL_ENABLE_NOL_HOST_DISABLE_NOL_FW 2
#define AGILE_DETECTOR_ID 2

/**
 * struct dfs_pulseparams - DFS pulse param structure.
 * @p_time:        Time for start of pulse in usecs.
 * @p_dur:         Duration of pulse in usecs.
 * @p_rssi:        RSSI of pulse.
 * @p_seg_id:      Segment id.
 * @p_sidx:        Sidx value.
 * @p_delta_peak:  Delta peak value.
 * @p_psidx_diff:  The difference in the FFT peak index between the short FFT
 *                 and the first long FFT.
 * @p_seq_num:     Sequence number.
 */
struct dfs_pulseparams {
	uint64_t p_time;
	uint8_t  p_dur;
	uint8_t  p_rssi;
	uint8_t  p_seg_id;
	int16_t  p_sidx;
	int8_t   p_delta_peak;
	int16_t  p_psidx_diff;
	uint32_t p_seq_num;
} qdf_packed;

/**
 * struct dfs_pulseline - Pulseline structure.
 * @pl_elems[]:     array of pulses in delay line.
 * @pl_firstelem:   Index of the first element.
 * @pl_lastelem:    Index of the last element.
 * @pl_numelems:    Number of elements in the delay line.
 */
struct dfs_pulseline {
	struct dfs_pulseparams pl_elems[DFS_MAX_PULSE_BUFFER_SIZE];
	uint32_t pl_firstelem;
	uint32_t pl_lastelem;
	uint32_t pl_numelems;
} qdf_packed;

#define DFS_EVENT_CHECKCHIRP  0x01 /* Whether to check the chirp flag */
#define DFS_EVENT_HW_CHIRP    0x02 /* hardware chirp */
#define DFS_EVENT_SW_CHIRP    0x04 /* software chirp */
/* Whether the event contains valid psidx diff value*/
#define DFS_EVENT_VALID_PSIDX_DIFF 0x08

/* Use this only if the event has CHECKCHIRP set. */
#define DFS_EVENT_ISCHIRP(e) \
	((e)->re_flags & (DFS_EVENT_HW_CHIRP | DFS_EVENT_SW_CHIRP))

/**
 * Check if the given event is to be rejected as not possibly
 * a chirp.  This means:
 *   (a) it's a hardware or software checked chirp, and
 *   (b) the HW/SW chirp bits are both 0.
 */
#define DFS_EVENT_NOTCHIRP(e) \
	(((e)->re_flags & (DFS_EVENT_CHECKCHIRP)) && (!DFS_EVENT_ISCHIRP((e))))

/**
 * struct dfs_event - DFS event structure.
 * @re_full_ts:          64-bit full timestamp from interrupt time.
 * @re_ts:               Original 15 bit recv timestamp.
 * @re_rssi:             Rssi of radar event.
 * @re_dur:              Duration of radar pulse.
 * @re_chanindex:        Channel of event.
 * @re_flags:            Event flags.
 * @re_freq:             Centre frequency of event, KHz.
 * @re_freq_lo:          Lower bounds of frequency, KHz.
 * @re_freq_hi:          Upper bounds of frequency, KHz.
 * @re_seg_id:           HT80_80/HT160 use.
 * @re_sidx:             Seg index.
 * @re_freq_offset_khz:  Freq offset in KHz
 * @re_peak_mag:         Peak mag.
 * @re_total_gain:       Total gain.
 * @re_mb_gain:          Mb gain.
 * @re_relpwr_db:        Relpower in db.
 * @re_delta_diff:       Delta diff.
 * @re_delta_peak:       Delta peak.
 * @re_psidx_diff:       Psidx diff.
 * @re_list:             List of radar events.
 */
struct dfs_event {
	uint64_t  re_full_ts;
	uint32_t  re_ts;
	uint8_t   re_rssi;
	uint8_t   re_dur;
	uint8_t   re_chanindex;
	uint8_t   re_flags;
	uint32_t  re_freq;
	uint32_t  re_freq_lo;
	uint32_t  re_freq_hi;
	uint8_t   re_seg_id;
	int       re_sidx;
	u_int     re_freq_offset_khz;
	int       re_peak_mag;
	int       re_total_gain;
	int       re_mb_gain;
	int       re_relpwr_db;
	uint8_t   re_delta_diff;
	int8_t    re_delta_peak;
	int16_t   re_psidx_diff;

	STAILQ_ENTRY(dfs_event) re_list;
} qdf_packed;

#define DFS_AR_MAX_ACK_RADAR_DUR   511
#define DFS_AR_MAX_NUM_PEAKS       3
#define DFS_AR_ARQ_SIZE            2048 /* 8K AR events for buffer size */
#define DFS_AR_ARQ_SEQSIZE         2049 /* Sequence counter wrap for AR */

#define DFS_RADARQ_SIZE      512 /* 1K radar events for buffer size */
#define DFS_RADARQ_SEQSIZE   513 /* Sequence counter wrap for radar */
/* Number of radar channels we keep state for */
#define DFS_NUM_RADAR_STATES 64
/* Max number radar filters for each type */
#define DFS_MAX_NUM_RADAR_FILTERS 10
/* Number of different radar types */
#define DFS_MAX_RADAR_TYPES  32
/* Number of filter index table rows */
#define DFS_NUM_FT_IDX_TBL_ROWS  256

/* RADAR filter pattern type 1*/
#define WLAN_DFS_RF_PATTERN_TYPE_1 1

/**
 * struct dfs_ar_state - DFS AR state structure.
 * @ar_prevwidth:         Previous width.
 * @ar_phyerrcount[]:     Phy error count.
 * @ar_acksum:            Acksum.
 * @ar_packetthreshold:   Thresh to determine traffic load.
 * @ar_parthreshold:      Thresh to determine peak.
 * @ar_radarrssi:         Rssi threshold for AR event.
 * @ar_prevtimestamp:     Prev time stamp.
 * @ar_peaklist[]:        Peak list.
 */
struct dfs_ar_state {
	uint32_t ar_prevwidth;
	uint32_t ar_phyerrcount[DFS_AR_MAX_ACK_RADAR_DUR];
	uint32_t ar_acksum;
	uint32_t ar_packetthreshold;
	uint32_t ar_parthreshold;
	uint32_t ar_radarrssi;
	uint16_t ar_prevtimestamp;
	uint16_t ar_peaklist[DFS_AR_MAX_NUM_PEAKS];
};

/**
 * struct dfs_delayelem - Delay Element.
 * @de_time:       Current "filter" time for start of pulse in usecs.
 * @de_dur:        Duration of pulse in usecs.
 * @de_rssi:       Rssi of pulse in dB.
 * @de_ts:         Time stamp for this delay element.
 * @de_seg_id:     Segment id for HT80_80/HT160 use.
 * @de_sidx:       Sidx value.
 * @de_delta_peak: Delta peak.
 * @de_psidx_diff: Psidx diff.
 * @de_seq_num:    Sequence number.
 */
struct dfs_delayelem {
	uint32_t de_time;
	uint8_t  de_dur;
	uint8_t  de_rssi;
	uint64_t de_ts;
	uint8_t  de_seg_id;
	int16_t  de_sidx;
	int8_t   de_delta_peak;
	int16_t  de_psidx_diff;
	uint32_t de_seq_num;
} qdf_packed;

/**
 * struct dfs_delayline - DFS Delay Line.
 * @dl_elems[]:    Array of pulses in delay line.
 * @dl_last_ts:    Last timestamp the delay line was used (in usecs).
 * @dl_firstelem:  Index of the first element.
 * @dl_lastelem:   Index of the last element.
 * @dl_numelems:   Number of elements in the delay line.
 * The following is to handle fractional PRI pulses that can cause false
 * detection.
 * @dl_seq_num_start: Sequence number of first pulse that was part of
 *                    threshold match.
 * @dl_seq_num_stop:  Sequence number of last pulse that was part of threshold
 *                    match.
 * The following is required because the first pulse may or may not be in the
 * delay line but we will find it iin the pulse line using dl_seq_num_second's
 * diff_ts value.
 * @dl_seq_num_second: Sequence number of second pulse that was part of
 *                     threshold match.
 * @dl_search_pri:     We need final search PRI to identify possible fractional
 *                     PRI issue.
 * @dl_min_sidx:       Minimum sidx value of pulses used to match thershold.
 *                     Used for sidx spread check.
 * @dl_max_sidx:       Maximum sidx value of pulses used to match thershold.
 *                     Used for sidx spread check.
 * @dl_delta_peak_match_count: Number of pulse in the delay line that had valid
 *                             delta peak value.
 * @dl_psidx_diff_match_count: Number of pulse in the delay line that had valid
 *                             psidx diff value.
 */
struct dfs_delayline {
	struct dfs_delayelem dl_elems[DFS_MAX_DL_SIZE];
	uint64_t dl_last_ts;
	uint32_t dl_firstelem;
	uint32_t dl_lastelem;
	uint32_t dl_numelems;
	uint32_t dl_seq_num_start;
	uint32_t dl_seq_num_stop;
	uint32_t dl_seq_num_second;
	uint32_t dl_search_pri;
	int16_t  dl_min_sidx;
	int8_t   dl_max_sidx;
	uint8_t  dl_delta_peak_match_count;
	uint8_t  dl_psidx_diff_match_count;
} qdf_packed;

/**
 * struct dfs_filter - Dfs filter.
 * @rf_dl:              Delay line of pulses for this filter.
 * @rf_numpulses:       Number of pulses in the filter.
 * @rf_minpri:          Min pri to be considered for this filter.
 * @rf_maxpri:          Max pri to be considered for this filter.
 * @rf_threshold:       Match filter output threshold for radar detect.
 * @rf_filterlen:       Length (in usecs) of the filter.
 * @rf_patterntype:     Fixed or variable pattern type.
 * @rf_fixed_pri_radar_pulse: indicates if it is a fixed pri pulse.
 * @rf_mindur:          Min duration for this radar filter.
 * @rf_maxdur:          Max duration for this radar filter.
 * @rf_ignore_pri_window: Ignore pri window.
 * @rf_pulseid:         Unique ID corresponding to the original filter ID.
 * To reduce false detection, look at frequency spread. For now we will use
 * sidx spread. But for HT160 frequency spread will be a better measure.
 * @rf_sidx_spread:     Maximum SIDX value spread in a matched sequence
 *                      excluding FCC Bin 5.
 * @rf_check_delta_peak: Minimum allowed delta_peak value for a pulse to be
 *                       considetred for this filter's match.
 */
struct dfs_filter {
	struct dfs_delayline rf_dl;
	uint32_t  rf_numpulses;
	uint32_t  rf_minpri;
	uint32_t  rf_maxpri;
	uint32_t  rf_threshold;
	uint32_t  rf_filterlen;
	uint32_t  rf_patterntype;
	uint32_t  rf_fixed_pri_radar_pulse;
	uint32_t  rf_mindur;
	uint32_t  rf_maxdur;
	uint32_t  rf_ignore_pri_window;
	uint32_t  rf_pulseid;
	uint16_t  rf_sidx_spread;
	int8_t    rf_check_delta_peak;
} qdf_packed;

/**
 * struct dfs_filtertype - Structure of DFS Filter type.
 * @ft_filters[]:      Array of ptrs storing addresses for struct of dfs_filter.
 * @ft_filterdur:      Duration of pulse which specifies filter type.
 * @ft_numfilters:     Num filters of this type.
 * @ft_last_ts:        Last timestamp this filtertype was used (in usecs).
 * @ft_mindur:         Min pulse duration to be considered for this filter type.
 * @ft_maxdur:         Max pulse duration to be considered for this filter type.
 * @ft_rssithresh:     Min rssi to be considered for this filter type.
 * @ft_numpulses:      Num pulses in each filter of this type.
 * @ft_patterntype:    Fixed or variable pattern type.
 * @ft_minpri:         Min pri to be considered for this type.
 * @ft_rssimargin:     Rssi threshold margin. In Turbo Mode HW reports rssi 3dB
 *                     lower than in non TURBO mode. This will offset that diff.
 */
struct dfs_filtertype {
	struct dfs_filter *ft_filters[DFS_MAX_NUM_RADAR_FILTERS];
	uint32_t  ft_filterdur;
	uint32_t  ft_numfilters;
	uint64_t  ft_last_ts;
	uint32_t  ft_mindur;
	uint32_t  ft_maxdur;
	uint32_t  ft_rssithresh;
	uint32_t  ft_numpulses;
	uint32_t  ft_patterntype;
	uint32_t  ft_minpri;
	uint32_t  ft_rssimargin;
};

/**
 * struct dfs_channel - Channel structure for dfs component.
 * @dfs_ch_freq:                Frequency in Mhz.
 * @dfs_ch_flags:               Channel flags.
 * @dfs_ch_flagext:             Extended channel flags.
 * @dfs_ch_ieee:                IEEE channel number.
 * @dfs_ch_vhtop_ch_freq_seg1:  Channel Center frequency.
 * @dfs_ch_vhtop_ch_freq_seg2:  Channel Center frequency applicable for 80+80MHz
 *                          mode of operation.
 */
struct dfs_channel {
	uint16_t       dfs_ch_freq;
	uint64_t       dfs_ch_flags;
	uint16_t       dfs_ch_flagext;
	uint8_t        dfs_ch_ieee;
	uint8_t        dfs_ch_vhtop_ch_freq_seg1;
	uint8_t        dfs_ch_vhtop_ch_freq_seg2;
};

/**
 * struct dfs_state - DFS state.
 * @rs_chan:            Channel info.
 * @rs_chanindex:       Channel index in radar structure.
 * @rs_numradarevents:  Number of radar events.
 * @rs_param:           Phy param.
 */
struct dfs_state {
	struct dfs_channel rs_chan;
	uint8_t  rs_chanindex;
	uint32_t rs_numradarevents;
	struct wlan_dfs_phyerr_param rs_param;
};

#define DFS_NOL_TIMEOUT_S  (30*60)    /* 30 minutes in seconds */
#define DFS_NOL_TIMEOUT_MS (DFS_NOL_TIMEOUT_S * 1000)
#define DFS_NOL_TIMEOUT_US (DFS_NOL_TIMEOUT_MS * 1000)

/**
 * struct dfs_nolelem - DFS NOL element.
 * @nol_dfs           Back pointer to dfs object.
 * @nol_freq:         Centre frequency.
 * @nol_chwidth:      Event width (MHz).
 * @nol_start_ticks:  NOL start time in OS ticks.
 * @nol_timeout_ms:   NOL timeout value in msec.
 * @nol_timer:        Per element NOL timer.
 * @nol_next:         Next element pointer.
 */
struct dfs_nolelem {
	TAILQ_ENTRY(dfs_nolelem) nolelem_list;
	struct wlan_dfs *nol_dfs;
	uint32_t       nol_freq;
	uint32_t       nol_chwidth;
	unsigned long  nol_start_ticks;
	uint32_t       nol_timeout_ms;
	qdf_timer_t    nol_timer;
	struct dfs_nolelem *nol_next;
} qdf_packed;


/**
 * struct dfs_info - DFS Info.
 * @rn_ftindex:            Number of different types of radars.
 * @rn_lastfull_ts:        Last 64 bit timstamp from recv interrupt.
 * @rn_last_ts:            last 15 bit ts from recv descriptor.
 * @rn_last_unique_ts:     last unique 32 bit ts from recv descriptor.
 * @rn_ts_prefix:          Prefix to prepend to 15 bit recv ts.
 * @rn_numbin5radars:      Number of bin5 radar pulses to search for.
 * @rn_fastdivGCval:       Value of fast diversity gc limit from init file.
 * @rn_minrssithresh:      Min rssi for all radar types.
 * @rn_maxpulsedur:        Max pulse width in TSF ticks.
 * @dfs_ext_chan_busy:     Ext chan busy.
 * @ext_chan_busy_ts:      Ext chan busy time.
 * @dfs_bin5_chirp_ts:     Ext bin5 chrip time.
 * @dfs_last_bin5_dur:     Last bin5 during.
 */
struct dfs_info {
	uint32_t  rn_ftindex;
	uint64_t  rn_lastfull_ts;
	uint16_t  rn_last_ts;
	uint32_t  rn_last_unique_ts;
	uint64_t  rn_ts_prefix;
	uint32_t  rn_numbin5radars;
	uint32_t  rn_fastdivGCval;
	int32_t   rn_minrssithresh;
	uint32_t  rn_maxpulsedur;
	uint8_t   dfs_ext_chan_busy;
	uint64_t  ext_chan_busy_ts;
	uint64_t  dfs_bin5_chirp_ts;
	uint8_t   dfs_last_bin5_dur;
} qdf_packed;

/**
 * struct dfs_bin5elem - BIN5 elements.
 * @be_ts:   Timestamp for the bin5 element.
 * @be_rssi: Rssi for the bin5 element.
 * @be_dur:  Duration of bin5 element.
 */
struct dfs_bin5elem {
	uint64_t  be_ts;
	uint32_t  be_rssi;
	uint32_t  be_dur;
};

/**
 * struct dfs_bin5radars - BIN5 radars.
 * @br_elems[]:    List of bin5 elems that fall within the time window.
 * @br_firstelem:  Index of the first element.
 * @br_lastelem:   Index of the last element.
 * @br_numelems:   Number of elements in the delay line.
 * @br_pulse:      Original info about bin5 pulse.
 */
struct dfs_bin5radars {
	struct dfs_bin5elem br_elems[DFS_MAX_B5_SIZE];
	uint32_t  br_firstelem;
	uint32_t  br_lastelem;
	uint32_t  br_numelems;
	struct dfs_bin5pulse br_pulse;
};

/**
 * struct dfs_stats - DFS stats.
 * @num_radar_detects:    Total num. of radar detects.
 * @num_seg_two_radar_detects: Total num. of radar detected in secondary segment
 * @total_phy_errors:     Total PHY errors.
 * @owl_phy_errors:       OWL PHY errors.
 * @pri_phy_errors:       Primary channel phy errors.
 * @ext_phy_errors:       Extension channel phy errors.
 * @dc_phy_errors:        DC PHY errors.
 * @early_ext_phy_errors: Extension channel early radar found error.
 * @bwinfo_errors:        Bogus bandwidth info received in descriptor.
 * @datalen_discards:     data length at least three bytes of payload.
 * @rssi_discards:        RSSI is not accurate.
 * @last_reset_tstamp:    Last reset timestamp.
 */
struct dfs_stats {
	uint32_t       num_radar_detects;
	uint32_t  num_seg_two_radar_detects;
	uint32_t  total_phy_errors;
	uint32_t  owl_phy_errors;
	uint32_t  pri_phy_errors;
	uint32_t  ext_phy_errors;
	uint32_t  dc_phy_errors;
	uint32_t  early_ext_phy_errors;
	uint32_t  bwinfo_errors;
	uint32_t  datalen_discards;
	uint32_t  rssi_discards;
	uint64_t  last_reset_tstamp;
};

#define DFS_EVENT_LOG_SIZE      256

/**
 * struct dfs_event_log - DFS event log.
 * @ts:               64-bit full timestamp from interrupt time.
 * @diff_ts:          Diff timestamp.
 * @rssi:             Rssi of radar event.
 * @dur:              Duration of radar pulse.
 * @is_chirp:         Chirp flag.
 * @seg_id:           HT80_80/HT160 use.
 * @sidx:             Seg index.
 * @freq_offset_khz:  Freq offset in KHz
 * @peak_mag:         Peak mag.
 * @total_gain:       Total gain.
 * @mb_gain:          Mb gain.
 * @relpwr_db:        Relpower in db.
 * @delta_diff:       Delta diff.
 * @delta_peak:       Delta peak.
 * @psidx_diff:       Psidx diff.
 */

struct dfs_event_log {
	uint64_t  ts;
	uint32_t  diff_ts;
	uint8_t   rssi;
	uint8_t   dur;
	int       is_chirp;
	uint8_t   seg_id;
	int       sidx;
	u_int     freq_offset_khz;
	int       peak_mag;
	int       total_gain;
	int       mb_gain;
	int       relpwr_db;
	uint8_t   delta_diff;
	int8_t    delta_peak;
	int16_t   psidx_diff;
};

#define WLAN_DFS_WEATHER_CHANNEL_WAIT_MIN 10 /*10 minutes*/
#define WLAN_DFS_WEATHER_CHANNEL_WAIT_S (WLAN_DFS_WEATHER_CHANNEL_WAIT_MIN * 60)
#define WLAN_DFS_WEATHER_CHANNEL_WAIT_MS  \
	((WLAN_DFS_WEATHER_CHANNEL_WAIT_S) * 1000) /*in MS*/

#define WLAN_DFS_WAIT_POLL_PERIOD 2  /* 2 seconds */
#define WLAN_DFS_WAIT_POLL_PERIOD_MS  \
	((WLAN_DFS_WAIT_POLL_PERIOD) * 1000)  /*in MS*/

#define DFS_DEBUG_TIMEOUT_S     30 /* debug timeout is 30 seconds */
#define DFS_DEBUG_TIMEOUT_MS    (DFS_DEBUG_TIMEOUT_S * 1000)

#define RSSI_POSSIBLY_FALSE              50
#define SEARCH_FFT_REPORT_PEAK_MAG_THRSH 40

#define MIN_DFS_SUBCHAN_BW 20 /* Minimum bandwidth of each subchannel. */

#define FREQ_OFFSET_BOUNDARY_FOR_80MHZ 40

/**
 * struct wlan_dfs -                 The main dfs structure.
 * @dfs_debug_mask:                  Current debug bitmask.
 * @dfs_curchan_radindex:            Current channel radar index.
 * @dfs_extchan_radindex:            Extension channel radar index.
 * @dfsdomain:                       Current DFS domain.
 * @dfs_proc_phyerr:                 Flags for Phy Errs to process.
 * @dfs_eventq:                      Q of free dfs event objects.
 * @dfs_eventqlock:                  Lock for free dfs event list.
 * @dfs_radarq:                      Q of radar events.
 * @dfs_radarqlock:                  Lock for dfs q.
 * @dfs_arq:                         Q of AR events.
 * @dfs_arqlock:                     Lock for AR q.
 * @dfs_ar_state:                    AR state.
 * @dfs_radar[]:                     Per-Channel Radar detector state.
 * @dfs_radarf[]:                    One filter for each radar pulse type.
 * @dfs_rinfo:                       State vars for radar processing.
 * @dfs_b5radars:                    Array of bin5 radar events.
 * @dfs_ftindextable:                Map of radar durs to filter types.
 * @dfs_nol:                         Non occupancy list for radar.
 * @dfs_nol_count:                   How many items?
 * @dfs_defaultparams:               Default phy params per radar state.
 * @wlan_dfs_stats:                  DFS related stats.
 * @pulses:                          Pulse history.
 * @events:                          Events structure.
 * @wlan_radar_tasksched:            Radar task is scheduled.
 * @wlan_dfswait:                    Waiting on channel for radar detect.
 * @wlan_dfstest:                    Test timer in progress.
 * @dfs_caps:                        Object of wlan_dfs_caps structure.
 * @wlan_dfstest_ieeechan:           IEEE chan num to return to after a dfs mute
 *                                   test.
 * @wlan_dfs_cac_time:               CAC period.
 * @wlan_dfstesttime:                Time to stay off chan during dfs test.
 * @wlan_dfswaittimer:               Dfs wait timer.
 * @wlan_dfstesttimer:               Dfs mute test timer.
 * @wlan_dfs_debug_timer:            Dfs debug timer.
 * @dfs_bangradar_type:              Radar simulation type.
 * @is_radar_found_on_secondary_seg: Radar on second segment.
 * @dfs_radar_found_for_fo:          Radar found event for FO(Full Offload) is
 *                                   received.
 * @is_radar_during_precac:          Radar found during precac.
 * @dfs_precac_lock:                 Lock to protect precac lists.
 * @dfs_precac_enable:               Enable the precac.
 * @dfs_precac_secondary_freq:       Second segment freq for precac.
 * @dfs_precac_primary_freq:         Primary freq.
 * @dfs_defer_precac_channel_change: Defer precac channel change.
 * @dfs_precac_inter_chan:           Intermediate non-DFS channel used while
 *                                   doing precac.
 * @dfs_autoswitch_des_chan:         Desired channel which has to be used
 *                                   after precac.
 * @dfs_autoswitch_des_mode:         Desired PHY mode which has to be used
 *                                   after precac.
 * @dfs_pre_cac_timeout_channel_change: Channel change due to precac timeout.
 * @wlan_dfs_task_timer:             Dfs wait timer.
 * @dur_multiplier:                  Duration multiplier.
 * @wlan_dfs_isdfsregdomain:         True when AP is in DFS domain
 * @wlan_dfs_false_rssi_thres:       False RSSI Threshold.
 * @wlan_dfs_peak_mag:               Peak mag.
 * @radar_log[]:                     Radar log.
 * @dfs_event_log_count:             Event log count.
 * @dfs_event_log_on:                Event log on.
 * @dfs_phyerr_count:                Same as number of PHY radar interrupts.
 * @dfs_phyerr_reject_count:         When TLV is supported, # of radar events
 *                                   ignored after TLV is parsed.
 * @dfs_phyerr_queued_count:         Number of radar events queued for matching
 *                                   the filters.
 * @dfs_phyerr_freq_min:             Phyerr min freq.
 * @dfs_phyerr_freq_max:             Phyerr max freq.
 * @dfs_phyerr_w53_counter:          Phyerr w53 counter.
 * @dfs_pri_multiplier:              Allow pulse if they are within multiple of
 *                                   PRI for the radar type.
 * @wlan_dfs_nol_timeout:            NOL timeout.
 * @update_nol:                      Update NOL.
 * @dfs_seq_num:                     Sequence number.
 * @dfs_nol_event[]:                 NOL event.
 * @dfs_nol_timer:                   NOL list processing.
 * @dfs_nol_free_list:               NOL free list.
 * @dfs_nol_elem_free_work:          The work queue to free an NOL element.
 * @dfs_cac_timer:                   CAC timer.
 * @dfs_cac_valid_timer:             Ignore CAC when this timer is running.
 * @dfs_cac_timeout_override:        Overridden cac timeout.
 * @dfs_enable:                      DFS Enable.
 * @dfs_cac_timer_running:           DFS CAC timer running.
 * @dfs_ignore_dfs:                  Ignore DFS.
 * @dfs_ignore_cac:                  Ignore CAC.
 * @dfs_cac_valid:                   DFS CAC valid.
 * @dfs_cac_valid_time:              Time for which CAC will be valid and will
 *                                   not be re-done.
 * @dfs_precac_timeout_override:     Overridden precac timeout.
 * @dfs_num_precac_freqs:            Number of PreCAC VHT80 frequencies.
 * @dfs_precac_required_list:        PreCAC required list.
 * @dfs_precac_done_list:            PreCAC done list.
 * @dfs_precac_nol_list:             PreCAC NOL List.
 * @dfs_curchan:                     DFS current channel.
 * @dfs_cac_started_chan:            CAC started channel.
 * @dfs_pdev_obj:                    DFS pdev object.
 * @dfs_is_offload_enabled:          Set if DFS offload enabled.
 * @dfs_use_nol:                     Use the NOL when radar found(default: TRUE)
 * @dfs_nol_lock:                    Lock to protect nol list.
 * @tx_leakage_threshold:            Tx leakage threshold for dfs.
 * @dfs_use_nol_subchannel_marking:  Use subchannel marking logic to add only
 *                                   radar affected subchannel instead of all
 *                                   bonding channels.
 * @dfs_host_wait_timer:             The timer that is started from host after
 *                                   sending the average radar parameters.
 *                                   Before this timeout host expects its dfs
 *                                   status from fw.
 * @dfs_average_pri:                 Average pri value of the received radar
 *                                   pulses.
 * @dfs_average_duration:            Average duration of the received radar
 *                                   pulses.
 * @dfs_average_sidx:                Average sidx of the received radar pulses.
 * @dfs_is_host_wait_running:        Indicates if host dfs status wait timer is
 *                                   running.
 * @dfs_average_params_sent:         Indicates if host has sent the average
 *                                   radar parameters.
 * @dfs_no_res_from_fw:              Indicates no response from fw.
 * @dfs_spoof_check_failed:          Indicates if the spoof check has failed.
 * @dfs_spoof_test_done:             Indicates if the sppof test is done.
 * @dfs_status_timeout_override:     Used to change the timeout value of
 *                                   dfs_host_wait_timer.
 * @dfs_is_stadfs_enabled:           Is STADFS enabled.
 * @dfs_min_sidx:                    Minimum sidx of the received radar pulses.
 * @dfs_max_sidx:                    Maximum sidx of the received radar pulses.
 * @dfs_seg_id:                      Segment ID of the radar hit channel.
 * @dfs_is_chirp:                    Radar Chirp in pulse present or not.
 * @dfs_bw_reduced:                  DFS bandwidth reduced channel bit.
 * @dfs_freq_offset:                 Frequency offset where radar was found.
 * @dfs_cac_aborted:                 DFS cac is aborted.
 * @dfs_disable_radar_marking:       To mark or unmark NOL chan as radar hit.
 * @dfs_data_struct_lock:            DFS data structure lock. This is to protect
 *                                   all the filtering data structures. For
 *                                   example: dfs_bin5radars, dfs_filtertype,
 *                                   etc.
 * @dfs_nol_ie_bandwidth:            Minimum Bandwidth of subchannels that
 *                                   are added to NOL.
 * @dfs_nol_ie_startfreq:            The centre frequency of the starting
 *                                   subchannel in the current channel list
 *                                   to be sent in NOL IE with RCSA.
 * @dfs_nol_ie_bitmap:               The bitmap of radar affected subchannels
 *                                   in the current channel list
 *                                   to be sent in NOL IE with RCSA.
 * @dfs_is_rcsa_ie_sent              To send or to not send RCSA IE.
 * @dfs_is_nol_ie_sent               To send or to not send NOL IE.
 */
struct wlan_dfs {
	uint32_t       dfs_debug_mask;
	int16_t        dfs_curchan_radindex;
	int16_t        dfs_extchan_radindex;
	uint32_t       dfsdomain;
	uint32_t       dfs_proc_phyerr;

	STAILQ_HEAD(, dfs_event) dfs_eventq;
	qdf_spinlock_t dfs_eventqlock;

	STAILQ_HEAD(, dfs_event) dfs_radarq;
	qdf_spinlock_t dfs_radarqlock;

	STAILQ_HEAD(, dfs_event) dfs_arq;
	qdf_spinlock_t dfs_arqlock;

	struct dfs_ar_state   dfs_ar_state;
	struct dfs_state      dfs_radar[DFS_NUM_RADAR_STATES];
	struct dfs_filtertype *dfs_radarf[DFS_MAX_RADAR_TYPES];
	struct dfs_info       dfs_rinfo;
	struct dfs_bin5radars *dfs_b5radars;
	int8_t                **dfs_ftindextable;
	struct dfs_nolelem    *dfs_nol;
	int                   dfs_nol_count;
	struct wlan_dfs_phyerr_param dfs_defaultparams;
	struct dfs_stats      wlan_dfs_stats;
	struct dfs_pulseline  *pulses;
	struct dfs_event      *events;

	uint32_t       wlan_radar_tasksched:1,
				   wlan_dfswait:1,
				   wlan_dfstest:1;
	struct wlan_dfs_caps dfs_caps;
	uint8_t        wlan_dfstest_ieeechan;
	uint32_t       wlan_dfs_cac_time;
	uint32_t       wlan_dfstesttime;
	qdf_timer_t    wlan_dfswaittimer;
	qdf_timer_t    wlan_dfstesttimer;
	qdf_timer_t    wlan_dfs_debug_timer;
	enum dfs_bangradar_types dfs_bangradar_type;
	bool           is_radar_found_on_secondary_seg;
	bool           dfs_radar_found_for_fo;
	bool           is_radar_during_precac;
	qdf_spinlock_t dfs_precac_lock;
	bool           dfs_precac_enable;
	uint8_t        dfs_precac_secondary_freq;
	uint8_t        dfs_precac_primary_freq;
	uint8_t        dfs_defer_precac_channel_change;
#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
	uint8_t        dfs_precac_inter_chan;
	uint8_t        dfs_autoswitch_des_chan;
	enum wlan_phymode dfs_autoswitch_des_mode;
#endif
	uint8_t        dfs_pre_cac_timeout_channel_change:1;
	qdf_timer_t    wlan_dfs_task_timer;
	int            dur_multiplier;
	uint16_t       wlan_dfs_isdfsregdomain;
	int            wlan_dfs_false_rssi_thres;
	int            wlan_dfs_peak_mag;
	struct dfs_event_log radar_log[DFS_EVENT_LOG_SIZE];
	int            dfs_event_log_count;
	int            dfs_event_log_on;
	int            dfs_phyerr_count;
	int            dfs_phyerr_reject_count;
	int            dfs_phyerr_queued_count;
	int            dfs_phyerr_freq_min;
	int            dfs_phyerr_freq_max;
	int            dfs_phyerr_w53_counter;
	int            dfs_pri_multiplier;
	int            wlan_dfs_nol_timeout;
	bool           update_nol;
	uint32_t       dfs_seq_num;
	int            dfs_nol_event[DFS_CHAN_MAX];
	qdf_timer_t    dfs_nol_timer;

	TAILQ_HEAD(, dfs_nolelem) dfs_nol_free_list;
	qdf_work_t     dfs_nol_elem_free_work;

	qdf_timer_t    dfs_cac_timer;
	qdf_timer_t    dfs_cac_valid_timer;
	int            dfs_cac_timeout_override;
	uint8_t        dfs_enable:1,
				   dfs_cac_timer_running:1,
				   dfs_ignore_dfs:1,
				   dfs_ignore_cac:1,
				   dfs_cac_valid:1;
	uint32_t       dfs_cac_valid_time;
	int            dfs_precac_timeout_override;
	uint8_t        dfs_num_precac_freqs;
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
	uint8_t        dfs_disable_radar_marking;
#endif
	TAILQ_HEAD(, dfs_precac_entry) dfs_precac_required_list;
	TAILQ_HEAD(, dfs_precac_entry) dfs_precac_done_list;
	TAILQ_HEAD(, dfs_precac_entry) dfs_precac_nol_list;

#ifdef QCA_SUPPORT_ETSI_PRECAC_DFS
	TAILQ_HEAD(, dfs_etsi_precac_entry) dfs_etsiprecac_required_list;
	TAILQ_HEAD(, dfs_etsi_precac_entry) dfs_etsiprecac_done_list;
#endif

	struct dfs_channel *dfs_curchan;
	struct dfs_channel dfs_cac_started_chan;
	struct wlan_objmgr_pdev *dfs_pdev_obj;
	struct dfs_soc_priv_obj *dfs_soc_obj;
#if defined(QCA_SUPPORT_AGILE_DFS) || defined(ATH_SUPPORT_ZERO_CAC_DFS)
	uint8_t dfs_psoc_idx;
	uint8_t        dfs_agile_precac_freq;
#endif
	bool           dfs_is_offload_enabled;
	int            dfs_use_nol;
	qdf_spinlock_t dfs_nol_lock;
	uint16_t tx_leakage_threshold;
	bool dfs_use_nol_subchannel_marking;
	uint8_t        dfs_spoof_test_done:1;
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
	qdf_timer_t    dfs_host_wait_timer;
	uint32_t       dfs_average_pri;
	uint32_t       dfs_average_duration;
	uint32_t       dfs_average_sidx;
	uint8_t        dfs_is_host_wait_running:1,
				   dfs_average_params_sent:1,
				   dfs_no_res_from_fw:1,
				   dfs_spoof_check_failed:1;
	struct dfs_channel dfs_radar_found_chan;
	int            dfs_status_timeout_override;
#endif
	bool           dfs_is_stadfs_enabled;
	int32_t        dfs_min_sidx;
	int32_t        dfs_max_sidx;
	uint8_t        dfs_seg_id;
	uint8_t        dfs_is_chirp;
	bool           dfs_bw_reduced;
	int32_t        dfs_freq_offset;
	bool           dfs_cac_aborted;
	qdf_spinlock_t dfs_data_struct_lock;
	uint8_t        dfs_nol_ie_bandwidth;
	uint16_t       dfs_nol_ie_startfreq;
	uint8_t        dfs_nol_ie_bitmap;
	bool           dfs_is_rcsa_ie_sent;
	bool           dfs_is_nol_ie_sent;
	bool           dfs_agile_precac_enable;
};

#if defined(QCA_SUPPORT_AGILE_DFS) || defined(ATH_SUPPORT_ZERO_CAC_DFS)
/**
 * struct wlan_dfs_priv - dfs private struct with agile capability info
 * @wlan_dfs: pointer to wlan_dfs object.
 * @agile_precac_active: agile precac active information for wlan_dfs_priv obj
 */
struct wlan_dfs_priv {
	struct wlan_dfs *dfs;
	bool agile_precac_active;
};
#endif

/**
 * struct dfs_soc_priv_obj - dfs private data
 * @psoc: pointer to PSOC object information
 * @pdev: pointer to PDEV object information
 * @dfs_is_phyerr_filter_offload: For some chip like Rome indicates too many
 *                                phyerr packets in a short time, which causes
 *                                OS hang. If this feild is configured as true,
 *                                FW will do the pre-check, filter out some
 *                                kinds of invalid phyerrors and indicate
 *                                radar detection related information to host.
 * @dfs_priv: array of dfs private structs with agile capability info
 * @num_dfs_privs: array size of dfs private structs for given psoc.
 * @cur_precac_dfs_index: current precac dfs index
 * @dfs_precac_timer: agile precac timer
 * @dfs_precac_timer_running: precac timer running flag
 * @ocac_status: Off channel CAC complete status
 */
struct dfs_soc_priv_obj {
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	bool dfs_is_phyerr_filter_offload;
#if defined(QCA_SUPPORT_AGILE_DFS) || defined(ATH_SUPPORT_ZERO_CAC_DFS)
	struct wlan_dfs_priv dfs_priv[WLAN_UMAC_MAX_PDEVS];
	uint8_t num_dfs_privs;
	uint8_t cur_precac_dfs_index;
	qdf_timer_t     dfs_precac_timer;
	uint8_t dfs_precac_timer_running;
	bool precac_state_started;
	bool ocac_status;
#endif
};

/**
 * enum DFS debug - This should match the table from if_ath.c.
 * @WLAN_DEBUG_DFS:             Minimal DFS debug.
 * @WLAN_DEBUG_DFS1:            Normal DFS debug.
 * @WLAN_DEBUG_DFS2:            Maximal DFS debug.
 * @WLAN_DEBUG_DFS3:            Matched filterID display.
 * @WLAN_DEBUG_DFS_PHYERR:      Phy error parsing.
 * @WLAN_DEBUG_DFS_NOL:         NOL related entries.
 * @WLAN_DEBUG_DFS_PHYERR_SUM:  PHY error summary.
 * @WLAN_DEBUG_DFS_PHYERR_PKT:  PHY error payload.
 * @WLAN_DEBUG_DFS_BIN5:        BIN5 checks.
 * @WLAN_DEBUG_DFS_BIN5_FFT:    BIN5 FFT check.
 * @WLAN_DEBUG_DFS_BIN5_PULSE:  BIN5 pulse check.
 * @WLAN_DEBUG_DFS_FALSE_DET:   False detection debug related prints.
 * @WLAN_DEBUG_DFS_FALSE_DET2:  Second level check to confirm poisitive
 *                              detection.
 * @WLAN_DEBUG_DFS_RANDOM_CHAN: Random channel selection.
 */
enum {
	WLAN_DEBUG_DFS  = 0x00000100,
	WLAN_DEBUG_DFS1 = 0x00000200,
	WLAN_DEBUG_DFS2 = 0x00000400,
	WLAN_DEBUG_DFS3 = 0x00000800,
	WLAN_DEBUG_DFS_PHYERR = 0x00001000,
	WLAN_DEBUG_DFS_NOL    = 0x00002000,
	WLAN_DEBUG_DFS_PHYERR_SUM = 0x00004000,
	WLAN_DEBUG_DFS_PHYERR_PKT = 0x00008000,
	WLAN_DEBUG_DFS_BIN5       = 0x00010000,
	WLAN_DEBUG_DFS_BIN5_FFT   = 0x00020000,
	WLAN_DEBUG_DFS_BIN5_PULSE = 0x00040000,
	WLAN_DEBUG_DFS_FALSE_DET  = 0x00080000,
	WLAN_DEBUG_DFS_FALSE_DET2 = 0x00100000,
	WLAN_DEBUG_DFS_RANDOM_CHAN = 0x00200000,
	WLAN_DEBUG_DFS_MAX        = 0x80000000,
	WLAN_DEBUG_DFS_ALWAYS     = WLAN_DEBUG_DFS_MAX
};

/**
 * enum host dfs spoof check status.
 * @HOST_DFS_CHECK_PASSED: Host indicates RADAR detected and the FW
 *                         confirms it to be spoof radar to host.
 * @HOST_DFS_CHECK_FAILED: Host doesn't indicate RADAR detected or spoof
 *                         radar parameters by
 *                         WMI_HOST_DFS_RADAR_FOUND_CMDID doesn't match.
 * @HOST_DFS_STATUS_CHECK_HW_RADAR: Host indicates RADAR detected and the
 *                             FW confirms it to be real HW radar to host.
 */
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
enum {
	HOST_DFS_STATUS_CHECK_PASSED = 0,
	HOST_DFS_STATUS_CHECK_FAILED = 1,
	HOST_DFS_STATUS_CHECK_HW_RADAR = 2
};
#endif

/**
 * struct dfs_phy_err - DFS phy error.
 * @fulltsf:             64-bit TSF as read from MAC.
 * @is_pri:              Detected on primary channel.
 * @is_ext:              Detected on extension channel.
 * @is_dc:               Detected at DC.
 * @is_early:            Early detect.
 * @do_check_chirp:      Whether to check hw_chirp/sw_chirp.
 * @is_hw_chirp:         Hardware-detected chirp.
 * @is_sw_chirp:         Software detected chirp.
 * @rs_tstamp:           32 bit TSF from RX descriptor (event).
 * @freq:                Centre frequency of event - KHz.
 * @freq_lo:             Lower bounds of frequency - KHz.
 * @freq_hi:             Upper bounds of frequency - KHz.
 * @rssi:                Pulse RSSI.
 * @dur:                 Pulse duration, raw (not uS).
 * @seg_id:              HT80_80/HT160 use.
 * @sidx:                Seg index.
 * @freq_offset_khz:     Freq offset in KHz.
 * @peak_mag:            Peak mag.
 * @total_gain:          Total gain.
 * @mb_gain:             Mb gain.
 * @relpwr_db:           Relpower in DB.
 * @pulse_delta_diff:    Pulse delta diff.
 * @pulse_delta_peak:    Pulse delta peak.
 * @pulse_psidx_diff:    Pulse psidx diff.
 *
 * Chirp notes!
 *
 * Pre-Sowl chips don't do FFT reports, so chirp pulses simply show up
 * as long duration pulses.
 *
 * The bin5 checking code would simply look for a chirp pulse of the correct
 * duration (within MIN_BIN5_DUR and MAX_BIN5_DUR) and add it to the "chirp"
 * pattern.
 *
 * For Sowl and later, an FFT was done on longer duration frames.  If those
 * frames looked like a chirp, their duration was adjusted to fall within
 * the chirp duration limits.  If the pulse failed the chirp test (it had
 * no FFT data or the FFT didn't meet the chirping requirements) then the
 * pulse duration was adjusted to be greater than MAX_BIN5_DUR, so it
 * would always fail chirp detection.
 *
 * This is pretty horrible.
 *
 * The eventual goal for chirp handling is thus:
 *
 * 1)In case someone ever wants to do chirp detection with this code on
 *   chips that don't support chirp detection, you can still do it based
 *   on pulse duration.  That's your problem to solve.
 *
 * 2)For chips that do hardware chirp detection or FFT, the "do_check_chirp"
 *   bit should be set.
 *
 * 3)Then, either is_hw_chirp or is_sw_chirp is set, indicating that
 *   the hardware or software post-processing of the chirp event found
 *   that indeed it was a chirp.
 *
 * 4)Finally, the bin5 code should just check whether the chirp bits are
 *   set and behave appropriately, falling back onto the duration checks
 *   if someone wishes to use this on older hardware (or with disabled
 *   FFTs, for whatever reason.)
 *
 * XXX TODO:
 *
 * 1)add duration in uS and raw duration, so the PHY error parsing
 *   code is responsible for doing the duration calculation;
 * 2)add ts in raw and corrected, so the PHY error parsing
 *   code is responsible for doing the offsetting, not the radar
 *   event code.
 */
struct dfs_phy_err {
	uint64_t fulltsf;
	uint32_t is_pri:1,
			 is_ext:1,
			 is_dc:1,
			 is_early:1,
			 do_check_chirp:1,
			 is_hw_chirp:1,
			 is_sw_chirp:1;
	uint32_t rs_tstamp;
	uint32_t freq;
	uint32_t freq_lo;
	uint32_t freq_hi;
	uint8_t  rssi;
	uint8_t  dur;
	uint8_t  seg_id;
	int      sidx;
	u_int    freq_offset_khz;
	int      peak_mag;
	int      total_gain;
	int      mb_gain;
	int      relpwr_db;
	uint8_t  pulse_delta_diff;
	int8_t   pulse_delta_peak;
	int16_t  pulse_psidx_diff;
};

/**
 * struct rx_radar_status - Parsed radar status
 * @raw_tsf:           Raw tsf
 * @tsf_offset:        TSF offset.
 * @rssi:              RSSI.
 * @pulse_duration:    Pulse duration.
 * @is_chirp:          Is chirp.
 * @delta_peak:        Delta peak.
 * @delta_diff:        Delta diff.
 * @sidx:              Starting frequency.
 * @freq_offset:       Frequency offset.
 * @agc_total_gain:    AGC total gain.
 * @agc_mb_gain:       AGC MB gain.
 */
struct rx_radar_status {
	uint32_t raw_tsf;
	uint32_t tsf_offset;
	int      rssi;
	int      pulse_duration;
	int      is_chirp:1;
	int      delta_peak;
	int      delta_diff;
	int      sidx;
	int      freq_offset; /* in KHz */
	int      agc_total_gain;
	int      agc_mb_gain;
};

/**
 * struct rx_search_fft_report - FFT report.
 * @total_gain_db:     Total gain in Db.
 * @base_pwr_db:       Base power in Db.
 * @fft_chn_idx:       FFT channel index.
 * @peak_sidx:         Peak sidx.
 * @relpwr_db:         Real power in Db.
 * @avgpwr_db:         Average power in Db.
 * @peak_mag:          Peak Mag.
 * @num_str_bins_ib:   Num dtr BINs IB
 * @seg_id:            Segment ID
 */
struct rx_search_fft_report {
	uint32_t total_gain_db;
	uint32_t base_pwr_db;
	int      fft_chn_idx;
	int      peak_sidx;
	int      relpwr_db;
	int      avgpwr_db;
	int      peak_mag;
	int      num_str_bins_ib;
	int      seg_id;
};

/**
 * dfs_process_radarevent() - process the radar event generated for a pulse.
 * @dfs: Pointer to wlan_dfs structure.
 * @chan: Current channel.
 *
 * There is currently no way to specify that a radar event has occurred on
 * a specific channel, so the current methodology is to mark both the pri
 * and ext channels as being unavailable. This should be fixed for 802.11ac
 * or we'll quickly run out of valid channels to use.
 *
 * If Radar found, this marks the channel (and the extension channel, if HT40)
 * as having seen a radar event. It marks CHAN_INTERFERENCE and will add it to
 * the local NOL implementation. This is only done for 'usenol=1', as the other
 * two modes don't do radar notification or CAC/CSA/NOL; it just notes there
 * was a radar.
 */
void  dfs_process_radarevent(struct wlan_dfs *dfs,
		struct dfs_channel *chan);

/**
 * dfs_nol_addchan() - Add channel to NOL.
 * @dfs: Pointer to wlan_dfs structure.
 * @freq: frequency to add to NOL.
 * @dfs_nol_timeout: NOL timeout.
 */
void dfs_nol_addchan(struct wlan_dfs *dfs,
		uint16_t freq,
		uint32_t dfs_nol_timeout);

/**
 * dfs_get_nol() - Get NOL.
 * @dfs: Pointer to wlan_dfs structure.
 * @dfs_nol: Pointer to dfsreq_nolelem structure to save the channels from NOL.
 * @nchan: Number of channels.
 */
void dfs_get_nol(struct wlan_dfs *dfs,
		struct dfsreq_nolelem *dfs_nol,
		int *nchan);

/**
 * dfs_set_nol() - Set NOL.
 * @dfs: Pointer to wlan_dfs structure.
 * @dfs_nol: Pointer to dfsreq_nolelem structure.
 * @nchan: Number of channels.
 */
void dfs_set_nol(struct wlan_dfs *dfs,
		struct dfsreq_nolelem *dfs_nol,
		int nchan);

/**
 * dfs_nol_update() - NOL update
 * @dfs: Pointer to wlan_dfs structure.
 *
 * Notify the driver/umac that it should update the channel radar/NOL flags
 * based on the current NOL list.
 */
void dfs_nol_update(struct wlan_dfs *dfs);

/**
 * dfs_nol_timer_cleanup() - NOL timer cleanup.
 * @dfs: Pointer to wlan_dfs structure.
 *
 * Cancels the NOL timer and frees the NOL elements.
 */
void dfs_nol_timer_cleanup(struct wlan_dfs *dfs);

/**
 * dfs_nol_timer_detach() - Free NOL timer.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_nol_timer_detach(struct wlan_dfs *dfs);

/**
 * dfs_nol_workqueue_cleanup() - Flushes NOL workqueue.
 * @dfs: Pointer to wlan_dfs structure.
 *
 * Flushes the NOL workqueue.
 */
void dfs_nol_workqueue_cleanup(struct wlan_dfs *dfs);

/**
 * dfs_retain_bin5_burst_pattern() - Retain the BIN5 burst pattern.
 * @dfs: Pointer to wlan_dfs structure.
 * @diff_ts: Timestamp diff.
 * @old_dur: Old duration.
 */
uint8_t dfs_retain_bin5_burst_pattern(struct wlan_dfs *dfs,
		uint32_t diff_ts,
		uint8_t old_dur);

/**
 * dfs_bin5_check_pulse() - BIN5 check pulse.
 * @dfs: Pointer to wlan_dfs structure.
 * @re: Pointer to dfs_event structure.
 * @br: Pointer to dfs_bin5radars structure.
 *
 * Reject the pulse if:
 * 1) It's outside the RSSI threshold;
 * 2) It's outside the pulse duration;
 * 3) It's been verified by HW/SW chirp checking
 *    and neither of those found a chirp.
 */
int dfs_bin5_check_pulse(struct wlan_dfs *dfs,
		struct dfs_event *re,
		struct dfs_bin5radars *br);

/**
 * dfs_bin5_addpulse() - BIN5 add pulse.
 * @dfs: Pointer to wlan_dfs structure.
 * @br: Pointer to dfs_bin5radars structure.
 * @re: Pointer to dfs_event structure.
 * @thists: Timestamp.
 */
int dfs_bin5_addpulse(struct wlan_dfs *dfs,
		struct dfs_bin5radars *br,
		struct dfs_event *re,
		uint64_t thists);

/**
 * dfs_bin5_check() - BIN5 check.
 * @dfs: Pointer to wlan_dfs structure.
 *
 * If the dfs structure is NULL (which should be illegal if everyting is working
 * properly, then signify that a bin5 radar was found.
 */
int dfs_bin5_check(struct wlan_dfs *dfs);

/**
 * dfs_check_chirping() - Check chirping.
 * @dfs: Pointer to wlan_dfs structure.
 * @buf: Phyerr buffer
 * @datalen: Phyerr buf length
 * @is_ctl: detected on primary channel.
 * @is_ext: detected on extension channel.
 * @slope: Slope
 * @is_dc: DC found
 *
 * This examines the FFT data contained in the PHY error information to figure
 * out whether the pulse is moving across frequencies.
 */
int dfs_check_chirping(struct wlan_dfs *dfs,
		void *buf,
		uint16_t datalen,
		int is_ctl,
		int is_ext,
		int *slope,
		int *is_dc);

/**
 * dfs_get_random_bin5_dur() - Get random BIN5 duration.
 * @dfs: Pointer to wlan_dfs structure.
 * @tstamp: Timestamp.
 *
 * Chirping pulses may get cut off at DC and report lower durations.
 * This function will compute a suitable random duration for each pulse.
 * Duration must be between 50 and 100 us, but remember that in
 * wlan_process_phyerr() which calls this function, we are dealing with the
 * HW reported duration (unconverted). dfs_process_radarevent() will
 * actually convert the duration into the correct value.
 * This function doesn't take into account whether the hardware
 * is operating in 5GHz fast clock mode or not.
 * And this function doesn't take into account whether the hardware
 * is peregrine or not.
 */
int dfs_get_random_bin5_dur(struct wlan_dfs *dfs,
		uint64_t tstamp);

/**
 * dfs_print_delayline() - Prints delayline.
 * @dfs: Pointer to wlan_dfs structure.
 * @dl: Pointer to dfs_delayline structure.
 */
void dfs_print_delayline(struct wlan_dfs *dfs,
		struct dfs_delayline *dl);

/**
 * dfs_print_nol() - Print NOL elements.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_print_nol(struct wlan_dfs *dfs);

/**
 * dfs_print_filter() - Prints the filter.
 * @dfs: Pointer to wlan_dfs structure.
 * @rf: Pointer to dfs_filter structure.
 */
void dfs_print_filter(struct wlan_dfs *dfs,
		struct dfs_filter *rf);

/**
 * dfs_getchanstate() - Get chan state.
 * @dfs: Pointer to wlan_dfs structure.
 * @index: To save the index of dfs_radar[]
 * @ext_chan_flag: Extension channel flag;
 */
struct dfs_state *dfs_getchanstate(struct wlan_dfs *dfs,
		uint8_t *index,
		int ext_ch_flag);

/**
 * dfs_round() - DFS found.
 * @val: Convert durations to TSF ticks.
 *
 * Return: TSF ticks.
 */
uint32_t dfs_round(int32_t val);

/**
 * dfs_reset_alldelaylines() - Reset alldelaylines.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
void dfs_reset_alldelaylines(struct wlan_dfs *dfs);
#else
static inline void dfs_reset_alldelaylines(struct wlan_dfs *dfs)
{
}
#endif

/**
 * dfs_reset_delayline() - Clear only a single delay line.
 * @dl: Pointer to dfs_delayline structure.
 */
void dfs_reset_delayline(struct dfs_delayline *dl);

/**
 * dfs_reset_filter_delaylines() - Reset filter delaylines.
 * @dft: Pointer to dfs_filtertype structure.
 */
void dfs_reset_filter_delaylines(struct dfs_filtertype *dft);

/**
 * dfs_reset_radarq() - Reset radar queue.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
void dfs_reset_radarq(struct wlan_dfs *dfs);
#else
static inline void dfs_reset_radarq(struct wlan_dfs *dfs)
{
}
#endif

/**
 * dfs_add_pulse() - Adds pulse to the queue.
 * @dfs: Pointer to wlan_dfs structure.
 * @rf: Pointer to dfs_filter structure.
 * @re: Pointer to dfs_event structure.
 * @deltaT: deltaT value.
 * @this_ts: Last time stamp.
 */
void dfs_add_pulse(struct wlan_dfs *dfs,
		struct dfs_filter *rf,
		struct dfs_event *re,
		uint32_t deltaT,
		uint64_t this_ts);

/**
 * dfs_bin_check() - BIN check
 * @dfs: Pointer to wlan_dfs structure.
 * @rf: Pointer to dfs_filter structure.
 * @deltaT: deltaT value.
 * @width: Width
 * @ext_chan_flag: Extension channel flag.
 */
int dfs_bin_check(struct wlan_dfs *dfs,
		struct dfs_filter *rf,
		uint32_t deltaT,
		uint32_t dur,
		int ext_chan_flag);

/**
 * dfs_bin_pri_check() - BIN PRI check
 * @dfs: Pointer to wlan_dfs structure.
 * @rf: Pointer to dfs_filter structure.
 * @dl: Pointer to dfs_delayline structure.
 * @score: Primary score.
 * @refpri: Current "filter" time for start of pulse in usecs.
 * @refdur: Duration value.
 * @ext_chan_flag: Extension channel flag.
 * @fundamentalpri: Highest PRI.
 */
int dfs_bin_pri_check(struct wlan_dfs *dfs,
		struct dfs_filter *rf,
		struct dfs_delayline *dl,
		uint32_t score,
		uint32_t refpri,
		uint32_t refdur,
		int ext_chan_flag,
		int fundamentalpri);

/**
 * dfs_staggered_check() - Detection implementation for staggered PRIs.
 * @dfs: Pointer to wlan_dfs structure.
 * @rf: Pointer to dfs_filter structure.
 * @deltaT: Delta of the Timestamp.
 * @width: Duration of radar pulse.
 *
 * Return: 1 on success and 0 on failure.
 */
int dfs_staggered_check(struct wlan_dfs *dfs,
		struct dfs_filter *rf,
		uint32_t deltaT,
		uint32_t width);

/**
 * dfs_get_pri_margin() - Get Primary margin.
 * @dfs: Pointer to wlan_dfs structure.
 * @is_extchan_detect: Extension channel detect.
 * @is_fixed_pattern: Fixed pattern.
 *
 * For the extension channel, if legacy traffic is present, we see a lot of
 * false alarms, so make the PRI margin narrower depending on the busy % for
 * the extension channel.
 *
 * Return: Returns pri_margin.
 */
int dfs_get_pri_margin(struct wlan_dfs *dfs,
		int is_extchan_detect,
		int is_fixed_pattern);

/**
 * dfs_get_filter_threshold() - Get filter threshold.
 * @dfs: Pointer to wlan_dfs structure.
 * @rf: Pointer to dfs_filter structure.
 * @is_extchan_detect: Extension channel detect.
 *
 * For the extension channel, if legacy traffic is present, we see a lot of
 * false alarms, so make the thresholds higher depending on the busy % for the
 * extension channel.
 *
 * Return: Returns threshold.
 */
int dfs_get_filter_threshold(struct wlan_dfs *dfs,
		struct dfs_filter *rf,
		int is_extchan_detect);

#if defined(QCA_MCL_DFS_SUPPORT)
/**
 * dfs_process_ar_event() - Process the ar event.
 * @dfs: Pointer to wlan_dfs structure.
 * @chan: Current channel structure.
 */
static inline void dfs_process_ar_event(struct wlan_dfs *dfs,
					struct dfs_channel *chan)
{
}

/**
 * dfs_reset_ar() - resets the ar state.
 * @dfs: pointer to wlan_dfs structure.
 */
static inline void dfs_reset_ar(struct wlan_dfs *dfs)
{
}

/**
 * dfs_reset_arq() - resets the ar queue.
 * @dfs: pointer to wlan_dfs structure.
 */
static inline void dfs_reset_arq(struct wlan_dfs *dfs)
{
}

#else
void dfs_process_ar_event(struct wlan_dfs *dfs,
			  struct dfs_channel *chan);

void dfs_reset_ar(struct wlan_dfs *dfs);
void dfs_reset_arq(struct wlan_dfs *dfs);
#endif

/**
 * dfs_is_radar_enabled() - check if radar detection is enabled.
 * @dfs: Pointer to wlan_dfs structure.
 * @ignore_dfs: if 1 then radar detection is disabled..
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
void dfs_is_radar_enabled(struct wlan_dfs *dfs,
			  int *ignore_dfs);
#else
static inline void dfs_is_radar_enabled(struct wlan_dfs *dfs,
					int *ignore_dfs)
{
}
#endif

/**
 * dfs_process_phyerr_bb_tlv() - Parses the PHY error and populates the
 *                               dfs_phy_err struct.
 * @dfs: Pointer to wlan_dfs structure.
 * @buf: Phyerr buffer
 * @datalen: Phyerr buf len
 * @rssi: RSSI
 * @ext_rssi: Extension RSSI.
 * @rs_tstamp: Time stamp.
 * @fulltsf: TSF64.
 * @e: Pointer to dfs_phy_err structure.
 *
 * Return: Returns 1.
 */
int dfs_process_phyerr_bb_tlv(struct wlan_dfs *dfs,
		void *buf,
		uint16_t datalen,
		uint8_t rssi,
		uint8_t ext_rssi,
		uint32_t rs_tstamp,
		uint64_t fulltsf,
		struct dfs_phy_err *e);

/**
 * dfs_reset() - DFS reset
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_reset(struct wlan_dfs *dfs);

/**
 * dfs_radar_enable() - Enables the radar.
 * @dfs: Pointer to wlan_dfs structure.
 * @no_cac: If no_cac is 0, it cancels the CAC.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
void dfs_radar_enable(struct wlan_dfs *dfs,
		int no_cac, uint32_t opmode);
#else
static inline void dfs_radar_enable(struct wlan_dfs *dfs,
		int no_cac, uint32_t opmode)
{
}
#endif

/**
 * dfs_process_phyerr() - Process phyerr.
 * @dfs: Pointer to wlan_dfs structure.
 * @buf: Phyerr buffer.
 * @datalen: phyerr buffer length.
 * @r_rssi: RSSI.
 * @r_ext_rssi: Extension channel RSSI.
 * @r_rs_tstamp: Timestamp.
 * @r_fulltsf: TSF64.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
void dfs_process_phyerr(struct wlan_dfs *dfs,
		void *buf,
		uint16_t datalen,
		uint8_t r_rssi,
		uint8_t r_ext_rssi,
		uint32_t r_rs_tstamp,
		uint64_t r_fulltsf);
#else
static inline void dfs_process_phyerr(struct wlan_dfs *dfs,
		void *buf,
		uint16_t datalen,
		uint8_t r_rssi,
		uint8_t r_ext_rssi,
		uint32_t r_rs_tstamp,
		uint64_t r_fulltsf)
{
}
#endif

#ifdef QCA_MCL_DFS_SUPPORT
/**
 * dfs_process_phyerr_filter_offload() - Process radar event.
 * @dfs: Pointer to wlan_dfs structure.
 * @wlan_radar_event: Pointer to radar_event_info structure.
 *
 * Return: None
 */
#if defined(WLAN_DFS_PARTIAL_OFFLOAD)
void dfs_process_phyerr_filter_offload(struct wlan_dfs *dfs,
		struct radar_event_info *wlan_radar_event);
#else
static inline void dfs_process_phyerr_filter_offload(
		struct wlan_dfs *dfs,
		struct radar_event_info *wlan_radar_event)
{
}
#endif
#endif

/**
 * dfs_get_radars() - Based on the chipset, calls init radar table functions.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
void dfs_get_radars(struct wlan_dfs *dfs);
#else
static inline void dfs_get_radars(struct wlan_dfs *dfs)
{
}
#endif

/**
 * dfs_attach() - Wrapper function to allocate memory for wlan_dfs members.
 * @dfs: Pointer to wlan_dfs structure.
 */
int dfs_attach(struct wlan_dfs *dfs);


/**
 * dfs_create_object() - Creates DFS object.
 * @dfs: Pointer to wlan_dfs structure.
 */
int dfs_create_object(struct wlan_dfs **dfs);

/**
 * dfs_destroy_object() - Destroys the DFS object.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_destroy_object(struct wlan_dfs *dfs);

/**
 * dfs_detach() - Wrapper function to free dfs variables.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_detach(struct wlan_dfs *dfs);

/**
 * dfs_cac_valid_reset() - Cancels the dfs_cac_valid_timer timer.
 * @dfs: Pointer to wlan_dfs structure.
 * @prevchan_ieee: Prevchan number.
 * @prevchan_flags: Prevchan flags.
 */
void dfs_cac_valid_reset(struct wlan_dfs *dfs,
		uint8_t prevchan_ieee,
		uint32_t prevchan_flags);

/**
 * dfs_cac_stop() - Clear the AP CAC timer.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_cac_stop(struct wlan_dfs *dfs);

/**
 * dfs_cancel_cac_timer() - Cancels the CAC timer.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_cancel_cac_timer(struct wlan_dfs *dfs);

/**
 * dfs_start_cac_timer() - Starts the CAC timer.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_start_cac_timer(struct wlan_dfs *dfs);

/**
 * dfs_is_subset_channel() - Check if the new_chan is subset of the old_chan.
 * @dfs: Pointer to wlan_dfs structure.
 * @old_chan: Pointer to old channel.
 * @new_chan: Pointer to new channel.
 *
 * Return: true if the new channel is subset of or same as the old channel,
 * else false.
 */
bool dfs_is_subset_channel(struct wlan_dfs *dfs,
			   struct dfs_channel *old_chan,
			   struct dfs_channel *new_chan);

/**
 * dfs_is_curchan_subset_of_cac_started_chan() - Check if the dfs current
 * channel is subset of cac started channel.
 * @dfs: Pointer to wlan_dfs structure.
 *
 * If the current channel and the cac_started_chan is same or
 * if the current channel is subset of the cac_started_chan then
 * this function returns true.
 *
 * Return: true if current channel is same or subset of  cac started channel,
 * else false.
 */
bool dfs_is_curchan_subset_of_cac_started_chan(struct wlan_dfs *dfs);

/**
 * dfs_clear_cac_started_chan() - Clear dfs cac started channel.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_clear_cac_started_chan(struct wlan_dfs *dfs);

/**
 * dfs_set_update_nol_flag() - Sets update_nol flag.
 * @dfs: Pointer to wlan_dfs structure.
 * @val: update_nol flag.
 */
void dfs_set_update_nol_flag(struct wlan_dfs *dfs,
		bool val);

/**
 * dfs_get_update_nol_flag() - Returns update_nol flag.
 * @dfs: Pointer to wlan_dfs structure.
 */
bool dfs_get_update_nol_flag(struct wlan_dfs *dfs);

/**
 * dfs_get_use_nol() - Get usenol.
 * @dfs: Pointer to wlan_dfs structure.
 */
int dfs_get_use_nol(struct wlan_dfs *dfs);

/**
 * dfs_get_nol_timeout() - Get NOL timeout.
 * @dfs: Pointer to wlan_dfs structure.
 */
int dfs_get_nol_timeout(struct wlan_dfs *dfs);

/**
 * dfs_is_ap_cac_timer_running() - Returns the dfs cac timer.
 * @dfs: Pointer to wlan_dfs structure.
 */
int dfs_is_ap_cac_timer_running(struct wlan_dfs *dfs);

/**
 * dfs_control()- Used to process ioctls related to DFS.
 * @dfs: Pointer to wlan_dfs structure.
 * @id: Command type.
 * @indata: Input buffer.
 * @insize: size of the input buffer.
 * @outdata: A buffer for the results.
 * @outsize: Size of the output buffer.
 */
int dfs_control(struct wlan_dfs *dfs,
		u_int id,
		void *indata,
		uint32_t insize,
		void *outdata,
		uint32_t *outsize);

/**
 * dfs_getnol() - Wrapper function for dfs_get_nol()
 * @dfs: Pointer to wlan_dfs structure.
 * @dfs_nolinfo: Pointer to dfsreq_nolinfo structure.
 */
void dfs_getnol(struct wlan_dfs *dfs,
		void *dfs_nolinfo);

/**
 * dfs_get_override_cac_timeout() -  Get override CAC timeout value.
 * @dfs: Pointer to DFS object.
 * @cac_timeout: Pointer to save the CAC timeout value.
 */
int dfs_get_override_cac_timeout(struct wlan_dfs *dfs,
		int *cac_timeout);

/**
 * dfs_override_cac_timeout() -  Override the default CAC timeout.
 * @dfs: Pointer to DFS object.
 * @cac_timeout: CAC timeout value.
 */
int dfs_override_cac_timeout(struct wlan_dfs *dfs,
		int cac_timeout);

/**
 * dfs_clear_nolhistory() - unmarks WLAN_CHAN_CLR_HISTORY_RADAR flag for
 *                          all the channels in dfs_ch_channels.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_clear_nolhistory(struct wlan_dfs *dfs);

/**
 * ol_if_dfs_configure() - Initialize the RADAR table for offload chipsets.
 * @dfs: Pointer to wlan_dfs structure.
 *
 * This is called during a channel change or regulatory domain
 * reset; in order to fetch the new configuration information and
 * program the DFS pattern matching module.
 *
 * Eventually this should be split into "fetch config" (which can
 * happen at regdomain selection time) and "configure DFS" (which
 * can happen at channel config time) so as to minimise overheads
 * when doing channel changes.  However, this'll do for now.
 */
void ol_if_dfs_configure(struct wlan_dfs *dfs);

/**
 * dfs_init_radar_filters() - Init Radar filters.
 * @dfs: Pointer to wlan_dfs structure.
 * @radar_info: Pointer to wlan_dfs_radar_tab_info structure.
 */
int dfs_init_radar_filters(struct wlan_dfs *dfs,
		struct wlan_dfs_radar_tab_info *radar_info);

/**
 * dfs_get_radars_for_ar5212() - Initialize radar table for AR5212 chipsets.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_get_radars_for_ar5212(struct wlan_dfs *dfs);

/**
 * dfs_get_radars_for_ar5416() - Initialize radar table for AR5416 chipsets.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_get_radars_for_ar5416(struct wlan_dfs *dfs);

/**
 * dfs_get_radars_for_ar9300() - Initialize radar table for AR9300 chipsets.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_get_radars_for_ar9300(struct wlan_dfs *dfs);

/**
 * dfs_print_filters() - Print the filters.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_print_filters(struct wlan_dfs *dfs);

/**
 * dfs_clear_stats() - Clear stats.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_clear_stats(struct wlan_dfs *dfs);

/**
 * dfs_radar_disable() - Disables the radar.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
int dfs_radar_disable(struct wlan_dfs *dfs);
#else
static inline int dfs_radar_disable(struct wlan_dfs *dfs)
{
	return 0;
}
#endif

/**
 * dfs_get_debug_info() - Get debug info.
 * @dfs: Pointer to wlan_dfs structure.
 * @data: void pointer to the data to save dfs_proc_phyerr.
 */
int dfs_get_debug_info(struct wlan_dfs *dfs,
		void *data);

/**
 * dfs_cac_timer_init() - Initialize cac timers.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_cac_timer_init(struct wlan_dfs *dfs);

/**
 * dfs_cac_attach() - Initialize dfs cac variables.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_cac_attach(struct wlan_dfs *dfs);

/**
 * dfs_cac_timer_reset() - Cancel dfs cac timers.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_cac_timer_reset(struct wlan_dfs *dfs);

/**
 * dfs_cac_timer_detach() - Free dfs cac timers.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_cac_timer_detach(struct wlan_dfs *dfs);

/**
 * dfs_nol_timer_init() - Initialize NOL timers.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_nol_timer_init(struct wlan_dfs *dfs);

/**
 * dfs_nol_attach() - Initialize NOL variables.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_nol_attach(struct wlan_dfs *dfs);

/**
 * dfs_nol_detach() - Detach NOL variables.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_nol_detach(struct wlan_dfs *dfs);

/**
 * dfs_print_nolhistory() - Print NOL history.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_print_nolhistory(struct wlan_dfs *dfs);

/**
 * dfs_stacac_stop() - Clear the STA CAC timer.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_stacac_stop(struct wlan_dfs *dfs);

/**
 * dfs_find_precac_secondary_vht80_chan() - Get a VHT80 channel with the
 *                                          precac primary center frequency.
 * @dfs: Pointer to wlan_dfs structure.
 * @chan: Pointer to dfs channel structure.
 */
void dfs_find_precac_secondary_vht80_chan(struct wlan_dfs *dfs,
		struct dfs_channel *chan);

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
/**
 * dfs_precac_csa() - Automatically switch the channel to the DFS channel
 *			on which PreCAC was completed without finding a RADAR.
 *			Use CSA with TBTT_COUNT to switch the channel.
 * @dfs: Pointer to dfs handler.
 *
 * Return: Void
 */
void dfs_precac_csa(struct wlan_dfs *dfs);
#endif

/**
 * dfs_phyerr_param_copy() - Function to copy src buf to dest buf.
 * @dst: dest buf.
 * @src: src buf.
 */
void dfs_phyerr_param_copy(struct wlan_dfs_phyerr_param *dst,
		struct wlan_dfs_phyerr_param *src);

/**
 * dfs_get_thresholds() - Get the threshold value.
 * @dfs: Pointer to wlan_dfs structure.
 * @param: Pointer to wlan_dfs_phyerr_param structure.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
int dfs_get_thresholds(struct wlan_dfs *dfs,
		struct wlan_dfs_phyerr_param *param);
#else
static inline int dfs_get_thresholds(struct wlan_dfs *dfs,
		struct wlan_dfs_phyerr_param *param)
{
		return 0;
}
#endif

/**
 * dfs_set_thresholds() - Sets the threshold value.
 * @dfs: Pointer to wlan_dfs structure.
 * @threshtype: DFS ioctl param type.
 * @value: Threshold value.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
int dfs_set_thresholds(struct wlan_dfs *dfs,
		const uint32_t threshtype,
		const uint32_t value);
#else
static inline int dfs_set_thresholds(struct wlan_dfs *dfs,
		const uint32_t threshtype,
		const uint32_t value)
{
		return 0;
}
#endif

/**
 * dfs_check_intersect_excl() - Check whether curfreq falls within lower_freq
 * and upper_freq, exclusively.
 * @low_freq : lower bound frequency value.
 * @high_freq: upper bound frequency value.
 * @chan_freq: Current frequency value to be checked.
 *
 * Return: returns true if overlap found, else returns false.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
bool dfs_check_intersect_excl(int low_freq, int high_freq, int chan_freq);
#else
static inline bool dfs_check_intersect_excl(int low_freq, int high_freq,
					    int chan_freq)
{
		return false;
}
#endif

/**
 * dfs_check_etsi_overlap() - Check whether given frequency centre/channel
 * width entry overlap with frequency spread in any way.
 * @center_freq         : current channel centre frequency.
 * @chan_width          : current channel width.
 * @en302_502_freq_low  : overlap frequency lower bound.
 * @en302_502_freq_high : overlap frequency upper bound.
 *
 * Return: returns 1 if overlap found, else returns 0.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
int dfs_check_etsi_overlap(int center_freq, int chan_width,
			   int en302_502_freq_low, int en302_502_freq_high);
#else
static inline int dfs_check_etsi_overlap(int center_freq, int chan_width,
					 int en302_502_freq_low,
					 int en302_502_freq_high)
{
		return 0;
}
#endif

/**
 * dfs_is_en302_502_applicable() - Check whether current channel frequecy spread
 *					overlaps with EN 302 502 radar type
 *					frequency range.
 *@dfs: Pointer to wlan_dfs structure.
 *
 * Return: returns true if overlap found, else returns false.
 */
#if defined(WLAN_DFS_DIRECT_ATTACH) || defined(WLAN_DFS_PARTIAL_OFFLOAD)
bool dfs_is_en302_502_applicable(struct wlan_dfs *dfs);
#else
static inline bool dfs_is_en302_502_applicable(struct wlan_dfs *dfs)
{
		return false;
}
#endif

/**
 * dfs_set_current_channel() - Set DFS current channel.
 * @dfs: Pointer to wlan_dfs structure.
 * @dfs_ch_freq: Frequency in Mhz.
 * @dfs_ch_flags: Channel flags.
 * @dfs_ch_flagext: Extended channel flags.
 * @dfs_ch_ieee: IEEE channel number.
 * @dfs_ch_vhtop_ch_freq_seg1: Channel Center frequency1.
 * @dfs_ch_vhtop_ch_freq_seg2: Channel Center frequency2.
 */
void dfs_set_current_channel(struct wlan_dfs *dfs,
		uint16_t dfs_ch_freq,
		uint64_t dfs_ch_flags,
		uint16_t dfs_ch_flagext,
		uint8_t dfs_ch_ieee,
		uint8_t dfs_ch_vhtop_ch_freq_seg1,
		uint8_t dfs_ch_vhtop_ch_freq_seg2);

/**
 * dfs_get_nol_chfreq_and_chwidth() - Get channel freq and width from NOL list.
 * @dfs_nol: Pointer to NOL channel entry.
 * @nol_chfreq: Pointer to save channel frequency.
 * @nol_chwidth: Pointer to save channel width.
 * @index: Index to dfs_nol list.
 */
void dfs_get_nol_chfreq_and_chwidth(struct dfsreq_nolelem *dfs_nol,
		uint32_t *nol_chfreq,
		uint32_t *nol_chwidth,
		int index);

/**
 * dfs_process_phyerr_owl() - Process an Owl-style phy error.
 * @dfs: Pointer to wlan_dfs structure.
 * @buf: Phyerr buffer
 * @datalen: Phyerr buf len
 * @rssi: RSSI
 * @ext_rssi: Extension RSSI.
 * @rs_tstamp: Time stamp.
 * @fulltsf: TSF64.
 * @e: Pointer to dfs_phy_err structure.
 *
 * Return: Returns 1.
 */
int dfs_process_phyerr_owl(struct wlan_dfs *dfs,
		void *buf,
		uint16_t datalen,
		uint8_t rssi,
		uint8_t ext_rssi,
		uint32_t rs_tstamp,
		uint64_t fulltsf,
		struct dfs_phy_err *e);

/**
 * dfs_process_phyerr_sowl() -Process a Sowl/Howl style phy error.
 * @dfs: Pointer to wlan_dfs structure.
 * @buf: Phyerr buffer
 * @datalen: Phyerr buf len
 * @rssi: RSSI
 * @ext_rssi: Extension RSSI.
 * @rs_tstamp: Time stamp.
 * @fulltsf: TSF64.
 * @e: Pointer to dfs_phy_err structure.
 *
 * Return: Returns 1.
 */
int dfs_process_phyerr_sowl(struct wlan_dfs *dfs,
		void *buf,
		uint16_t datalen,
		uint8_t rssi,
		uint8_t ext_rssi,
		uint32_t rs_tstamp,
		uint64_t fulltsf,
		struct dfs_phy_err *e);

/**
 * dfs_process_phyerr_merlin() - Process a Merlin/Osprey style phy error.
 *                               dfs_phy_err struct.
 * @dfs: Pointer to wlan_dfs structure.
 * @buf: Phyerr buffer
 * @datalen: Phyerr buf len
 * @rssi: RSSI
 * @ext_rssi: Extension RSSI.
 * @rs_tstamp: Time stamp.
 * @fulltsf: TSF64.
 * @e: Pointer to dfs_phy_err structure.
 *
 * Return: Returns 1.
 */
int dfs_process_phyerr_merlin(struct wlan_dfs *dfs,
		void *buf,
		uint16_t datalen,
		uint8_t rssi,
		uint8_t ext_rssi,
		uint32_t rs_tstamp,
		uint64_t fulltsf,
		struct dfs_phy_err *e);

/*
 * __dfs_process_radarevent() - Continuation of process a radar event function.
 * @dfs: Pointer to wlan_dfs structure.
 * @ft: Pointer to dfs_filtertype structure.
 * @re: Pointer to dfs_event structure.
 * @this_ts: Timestamp.
 *
 * There is currently no way to specify that a radar event has occurred on
 * a specific channel, so the current methodology is to mark both the pri
 * and ext channels as being unavailable.  This should be fixed for 802.11ac
 * or we'll quickly run out of valid channels to use.
 *
 * Return: If a radar event is found, return 1.  Otherwise, return 0.
 */
void __dfs_process_radarevent(struct wlan_dfs *dfs,
		struct dfs_filtertype *ft,
		struct dfs_event *re,
		uint64_t this_ts,
		int *found,
		int *false_radar_found);

/**
 * dfs_radar_found_action() - Radar found action
 * @dfs: Pointer to wlan_dfs structure.
 * @bangradar: true if radar is due to bangradar command.
 * @seg_id: Segment id.
 */
void dfs_radar_found_action(struct wlan_dfs *dfs,
			    bool bangradar,
			    uint8_t seg_id);

/**
 * bin5_rules_check_internal() - This is a extension of dfs_bin5_check().
 * @dfs: Pointer to wlan_dfs structure.
 * @br: Pointer to dfs_bin5radars structure.
 * @bursts: Bursts.
 * @numevents: Number of events.
 * @prev: prev index.
 * @i: Index.
 * @this: index to br_elems[]
 */
void bin5_rules_check_internal(struct wlan_dfs *dfs,
		struct dfs_bin5radars *br,
		uint32_t *bursts,
		uint32_t *numevents,
		uint32_t prev,
		uint32_t i,
		uint32_t this,
		int *index);

/**
 * dfs_main_task_testtimer_init() - Initialize dfs task testtimer.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_main_task_testtimer_init(struct wlan_dfs *dfs);

/**
 * dfs_stop() - Clear dfs timers.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_stop(struct wlan_dfs *dfs);

/**
 * dfs_update_cur_chan_flags() - Update DFS channel flag and flagext.
 * @dfs: Pointer to wlan_dfs structure.
 * @flags: New channel flags
 * @flagext: New Extended flags
 */
void dfs_update_cur_chan_flags(struct wlan_dfs *dfs,
		uint64_t flags,
		uint16_t flagext);

/**
 * dfs_radarevent_basic_sanity() - Check basic sanity of the radar event
 * @dfs: Pointer to wlan_dfs structure.
 * @chan: Current channel.
 *
 * Return: If a radar event found on NON-DFS channel  return 0.  Otherwise,
 * return 1.
 */
int dfs_radarevent_basic_sanity(struct wlan_dfs *dfs,
		struct dfs_channel *chan);

/**
 * wlan_psoc_get_dfs_txops() - Get dfs_tx_ops pointer
 * @psoc: Pointer to psoc structure.
 *
 * Return: Pointer to dfs_tx_ops.
 */
struct wlan_lmac_if_dfs_tx_ops *
wlan_psoc_get_dfs_txops(struct wlan_objmgr_psoc *psoc);

/**
 * dfs_nol_free_list() - Free NOL elements.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_nol_free_list(struct wlan_dfs *dfs);

/**
 * dfs_second_segment_radar_disable() - Disables the second segment radar.
 * @dfs: Pointer to wlan_dfs structure.
 *
 * This is called when AP detects the radar, to (potentially) disable
 * the radar code.
 *
 * Return: returns 0.
 */
int dfs_second_segment_radar_disable(struct wlan_dfs *dfs);

/**
 * dfs_fetch_nol_ie_info() - Fill NOL information to be sent with RCSA.
 * @dfs                    - Pointer to wlan_dfs structure.
 * @nol_ie_bandwidth       - Minimum subchannel bandwidth.
 * @nol_ie_startfreq       - Radar affected channel list's first subchannel's
 *                         - centre frequency.
 * @nol_ie_bitmap          - NOL bitmap denoting affected subchannels.
 */
void dfs_fetch_nol_ie_info(struct wlan_dfs *dfs, uint8_t *nol_ie_bandwidth,
			   uint16_t *nol_ie_startfreq, uint8_t *nol_ie_bitmap);

/**
 * dfs_set_rcsa_flags() - Set flags that are required for sending RCSA and
 * NOL IE.
 * @dfs: Pointer to wlan_dfs structure.
 * @is_rcsa_ie_sent: Boolean to check if RCSA IE should be sent or not.
 * @is_nol_ie_sent: Boolean to check if NOL IE should be sent or not.
 */
void dfs_set_rcsa_flags(struct wlan_dfs *dfs, bool is_rcsa_ie_sent,
			bool is_nol_ie_sent);

/**
 * dfs_get_rcsa_flags() - Get flags that are required for sending RCSA and
 * NOL IE.
 * @dfs: Pointer to wlan_dfs structure.
 * @is_rcsa_ie_sent: Boolean to check if RCSA IE should be sent or not.
 * @is_nol_ie_sent: Boolean to check if NOL IE should be sent or not.
 */
void dfs_get_rcsa_flags(struct wlan_dfs *dfs, bool *is_rcsa_ie_sent,
			bool *is_nol_ie_sent);

/**
 * dfs_process_nol_ie_bitmap() - Update NOL with external radar information.
 * @dfs               - Pointer to wlan_dfs structure.
 * @nol_ie_bandwidth  - Minimum subchannel bandwidth.
 * @nol_ie_starfreq   - Radar affected channel list's first subchannel's
 *                    - centre frequency.
 * @nol_ie_bitmap     - Bitmap denoting radar affected subchannels.
 *
 * Return: True if NOL IE should be propagated, else false.
 */
bool dfs_process_nol_ie_bitmap(struct wlan_dfs *dfs, uint8_t nol_ie_bandwidth,
			       uint16_t nol_ie_startfreq,
			       uint8_t nol_ie_bitmap);

/**
 * dfs_check_for_cac_start() - Check for DFS CAC start conditions.
 * @dfs: Pointer to wlan_dfs structure.
 * @continue_current_cac: If AP can start CAC then this variable indicates
 * whether to continue with the current CAC or restart the CAC. This variable
 * is valid only if this function returns true.
 *
 * Return: true if AP can start or continue the current CAC, else false.
 */
bool dfs_check_for_cac_start(struct wlan_dfs *dfs,
			     bool *continue_current_cac);

/**
 * dfs_task_testtimer_reset() - stop dfs test timer.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_task_testtimer_reset(struct wlan_dfs *dfs);

/**
 * dfs_is_freq_in_nol() - check if given channel in nol list
 * @dfs: Pointer to wlan_dfs structure
 * @freq: channel frequency
 *
 * check if given channel in nol list.
 *
 * Return: true if channel in nol, false else
 */
bool dfs_is_freq_in_nol(struct wlan_dfs *dfs, uint32_t freq);

/**
 * dfs_task_testtimer_detach() - Free dfs test timer.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_task_testtimer_detach(struct wlan_dfs *dfs);

/**
 * dfs_timer_detach() - Free dfs timers.
 * @dfs: Pointer to wlan_dfs structure.
 */
void dfs_timer_detach(struct wlan_dfs *dfs);

/**
 * dfs_is_disable_radar_marking_set() - Check if radar marking is set on
 * NOL chan.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
int dfs_is_disable_radar_marking_set(struct wlan_dfs *dfs,
				     bool *disable_radar_marking);
#else
static inline int dfs_is_disable_radar_marking_set(struct wlan_dfs *dfs,
						   bool *disable_radar_marking)
{
	return QDF_STATUS_SUCCESS;
}
#endif
/**
 * dfs_get_disable_radar_marking() - Get the value of disable radar marking.
 * @dfs: Pointer to wlan_dfs structure.
 */
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
bool dfs_get_disable_radar_marking(struct wlan_dfs *dfs);
#endif
#endif  /* _DFS_H_ */
