/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef USB_QMI_SVC_H
#define USB_QMI_SVC_H

#ifdef CONFIG_SND_USB_AUDIO_QMI
int uaudio_qmi_plat_init(void);
void uaudio_qmi_plat_exit(void);
#else
static inline int uaudio_qmi_plat_init(void)
{
	return 0;
}

static inline void uaudio_qmi_plat_exit(void)
{
}
#endif

#endif
