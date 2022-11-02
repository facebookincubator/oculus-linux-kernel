/*
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2010-2014, The Linux Foundation. All rights reserved.
 * Author: Nick Pelly <npelly@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_GENI_SERIAL_H
#define __ASM_ARCH_MSM_GENI_SERIAL_H

#include <linux/serial_core.h>

/**
 * struct msm_serial_hs_platform_data - platform device data
 *					for msm hsuart device
 * @wakeup_irq : IRQ line to be configured as Wakeup source.
 * @inject_rx_on_wakeup : Set 1 if specific character to be inserted on wakeup
 * @rx_to_inject : Character to be inserted on wakeup
 * @gpio_config : Configure gpios that are used for uart communication
 * @userid : User-defined number to be used to enumerate device as tty<userid>
 * @uart_tx_gpio: GPIO number for UART Tx Line.
 * @uart_rx_gpio: GPIO number for UART Rx Line.
 * @uart_cts_gpio: GPIO number for UART CTS Line.
 * @uart_rfr_gpio: GPIO number for UART RFR Line.
 * @bam_tx_ep_pipe_index : BAM TX Endpoint Pipe Index for HSUART
 * @bam_tx_ep_pipe_index : BAM RX Endpoint Pipe Index for HSUART
 * @no_suspend_delay : Flag used to make system go to suspend
 * immediately or not
 * @obs: Flag to enable out of band sleep feature support
 */

struct uart_port *msm_geni_serial_get_uart_port(int port_index);
int msm_geni_serial_get_clock_count(struct uart_port *uport);
int msm_geni_serial_get_client_count(struct uart_port *uport);

int vote_clock_on(struct uart_port *uport);
int vote_clock_off(struct uart_port *uport);
void msm_geni_serial_set_mctrl(struct uart_port *uport, unsigned int mctrl);
unsigned int msm_geni_serial_tx_empty(struct uart_port *uport);
#endif
