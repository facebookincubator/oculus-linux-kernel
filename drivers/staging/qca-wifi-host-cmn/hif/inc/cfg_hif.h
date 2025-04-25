/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _CFG_HIF_H_
#define _CFG_HIF_H_

/* Min/Max/default CE status srng timer threshold */
#define WLAN_CFG_CE_STATUS_RING_TIMER_THRESH_MIN 0
#define WLAN_CFG_CE_STATUS_RING_TIMER_THRESH_MAX 4096
#ifdef WLAN_WAR_CE_DISABLE_SRNG_TIMER_IRQ
#define WLAN_CFG_CE_STATUS_RING_TIMER_THRESH_DEFAULT 0
#else
#define WLAN_CFG_CE_STATUS_RING_TIMER_THRESH_DEFAULT 4096
#endif

/* Min/Max/default CE status srng batch count threshold */
#define WLAN_CFG_CE_STATUS_RING_BATCH_COUNT_THRESH_MIN 0
#define WLAN_CFG_CE_STATUS_RING_BATCH_COUNT_THRESH_MAX 512
#define WLAN_CFG_CE_STATUS_RING_BATCH_COUNT_THRESH_DEFAULT 1

#ifdef WLAN_CE_INTERRUPT_THRESHOLD_CONFIG
/*
 * <ini>
 * ce_status_ring_timer_thresh - ce status srng timer threshold
 * @Min: 0
 * @Max: 4096
 * @Default: 0
 *
 * This ini specifies the timer threshold for CE status srng to
 * indicate the interrupt to be fired whenever the timer threshold
 * runs out.
 *
 * Supported Feature: interrupt threshold for CE status srng
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_CE_STATUS_RING_TIMER_THRESHOLD \
	CFG_INI_UINT("ce_status_ring_timer_threshold", \
		     WLAN_CFG_CE_STATUS_RING_TIMER_THRESH_MIN, \
		     WLAN_CFG_CE_STATUS_RING_TIMER_THRESH_MAX, \
		     WLAN_CFG_CE_STATUS_RING_TIMER_THRESH_DEFAULT, \
		     CFG_VALUE_OR_DEFAULT, \
		     "CE Status ring timer threshold")

#define CFG_RING_TIMER_THRESHOLD CFG(CFG_CE_STATUS_RING_TIMER_THRESHOLD)

/*
 * <ini>
 * ce_status_ring_batch_count_thresh - ce status srng batch count threshold
 * @Min: 0
 * @Max: 512
 * @Default: 1
 *
 * This ini specifies the batch count threshold for CE status srng to
 * indicate the interrupt to be fired for a given number of packets in
 * the ring.
 *
 * Supported Feature: interrupt threshold for CE status srng
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_CE_STATUS_RING_BATCH_COUNT_THRESHOLD \
	CFG_INI_UINT("ce_status_ring_batch_count_threshold", \
		     WLAN_CFG_CE_STATUS_RING_BATCH_COUNT_THRESH_MIN, \
		     WLAN_CFG_CE_STATUS_RING_BATCH_COUNT_THRESH_MAX, \
		     WLAN_CFG_CE_STATUS_RING_BATCH_COUNT_THRESH_DEFAULT, \
		     CFG_VALUE_OR_DEFAULT, \
		     "CE Status ring batch count threshold")

#define CFG_BATCH_COUNT_THRESHOLD CFG(CFG_CE_STATUS_RING_BATCH_COUNT_THRESHOLD)

#else
#define CFG_RING_TIMER_THRESHOLD
#define CFG_BATCH_COUNT_THRESHOLD
#endif /* WLAN_CE_INTERRUPT_THRESHOLD_CONFIG */

/*
 * <ini>
 * gDisableWakeIrq - Disable wake IRQ or not
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini controls driver to disable wake IRQ or not.
 * Disable wake IRQ for one MSI mode.
 * If you want to support wake IRQ. Please allocate at least
 * 2 MSI vector. The first is for wake IRQ while the others
 * share the second vector.
 *
 * Related: None.
 *
 * Supported Feature: wake IRQ
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_DISABLE_WAKE_IRQ CFG_INI_BOOL( \
	"gDisableWakeIrq", \
	0, \
	"Disable wake IRQ")

/*
 * <ini>
 * irq_affine_audio_use_case - IRQ affinity for audio use case supported
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini controls driver to enable IRQ affinity for Pro audio use case.
 *
 * Related: None.
 *
 * Supported Feature: IRQ Affinity
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_IRQ_AFFINE_AUDIO_USE_CASE CFG_INI_BOOL( \
	"irq_affine_audio_use_case", \
	0, \
	"Enable IRQ affinity for audio use case")

#define CFG_HIF \
	CFG_RING_TIMER_THRESHOLD \
	CFG_BATCH_COUNT_THRESHOLD \
	CFG(CFG_DISABLE_WAKE_IRQ) \
	CFG(CFG_IRQ_AFFINE_AUDIO_USE_CASE)

#endif /* _CFG_HIF_H_ */
