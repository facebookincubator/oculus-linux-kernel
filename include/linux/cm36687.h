/* include/linux/CM36687.h
 *
 * Copyright (C) 2017 Vishay Capella Microsystems Limited
 * Author: Frank Hsieh <Frank.Hsieh@vishay.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_CM36687_H
#define __LINUX_CM36687_H

#define CM36687_I2C_NAME "cm36687"

/* Define Slave Address*/
#define	CM36687_slave_add	0xC0>>1

/*Define Command Code*/
#define		PS_CONF1      0x03
#define		PS_CONF3      0x04
#define		PS_THDL       0x05
#define		PS_THDH       0x06
#define		PS_CANC       0x07
#define		PS_CONF5      0x08

#define		PS_DATA       0xF2
#define		INT_FLAG      0xF3
#define		ID_REG        0xF4

/*cm36687*/
/*for PS CONF1 command*/
#define CM36687_PS_INT_IN_AND_OUT  (2 << 2) /*enable/disable Interrupt*/
#define CM36687_PS_INT_MASK        0xFFF3   /*enable/disable Interrupt*/

#define CM36687_PS_PERIOD_8   (0 << 6)
#define CM36687_PS_PERIOD_16  (1 << 6)
#define CM36687_PS_PERIOD_32  (2 << 6)
#define CM36687_PS_PERIOD_64  (3 << 6)
#define CM36687_PS_PERS_1 	 (0 << 4)
#define CM36687_PS_PERS_2 	 (1 << 4)
#define CM36687_PS_PERS_3 	 (2 << 4)
#define CM36687_PS_PERS_4 	 (3 << 4)
#define CM36687_PS_IT_1T 	   (0 << 14)
#define CM36687_PS_IT_2T 	   (1 << 14)
#define CM36687_PS_IT_4T 		 (2 << 14)
#define CM36687_PS_IT_8T 		 (3 << 14)
#define CM36687_PS_START 		 (1 << 11)

#define CM36687_PS_SD	       (1 << 0)/*enable/disable PS func, 1:disable , 0: enable*/
#define CM36687_PS_SD_MASK   0xFFFE

/*for PS CONF3 command*/
#define CM36687_LED_I_8               (0 << 8)
#define CM36687_LED_I_12              (1 << 8)
#define CM36687_LED_I_15              (2 << 8)
#define CM36687_LED_I_18              (3 << 8)
#define CM36687_LED_I_21              (4 << 8)
#define CM36687_LED_I_25              (5 << 8)
#define CM36687_LED_I_28              (6 << 8)
#define CM36687_LED_I_30              (7 << 8)
#define CM36687_PS_ACTIVE_FORCE_MODE  (1 << 6)
#define CM36687_PS_ACTIVE_FORCE_TRIG  (1 << 5)

/*for PS CONF5 command*/
#define CM36687_POR_S 		            (1 << 0)

/*for INT FLAG*/
#define INT_FLAG_PS_SPFLAG           (1<<12)

#define INT_FLAG_PS_IF_CLOSE         (1<<9)
#define INT_FLAG_PS_IF_AWAY          (1<<8)  

extern unsigned int ps_kparam1;
extern unsigned int ps_kparam2;

struct cm36687_platform_data {
	int intr;
	int (*power)(int, uint8_t); /* power to the chip */
	uint8_t slave_addr;
	uint16_t ps_close_thd_set;
	uint16_t ps_away_thd_set;	
	uint16_t ps_conf1_val;
	uint16_t ps_conf3_val;	
};

#endif
