/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: contains nud event tracking function declarations
 */

#ifndef _WLAN_NUD_TRACKING_H_
#define _WLAN_NUD_TRACKING_H_

#ifdef WLAN_NUD_TRACKING

/**
 * hdd_nud_failure_work() - Handle NUD failuire work
 * @context: HDD context pointer
 * @vdev_id: vdev id
 *
 * Return: None
 */
void hdd_nud_failure_work(hdd_cb_handle context, uint8_t vdev_id);
#else
static inline void
hdd_nud_failure_work(hdd_cb_handle context, uint8_t vdev_id)
{
}
#endif /* WLAN_NUD_TRACKING */
#endif /* end  of _WLAN_NUD_TRACKING_H_ */
