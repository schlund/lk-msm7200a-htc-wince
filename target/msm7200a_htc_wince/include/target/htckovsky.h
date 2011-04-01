/* htckovsky.h
 *
 * Copyright (C) 2011 Alexander Tarasikov <alexander.tarasikov@gmail.com>
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

#ifndef __HTCKOVSKY_H__
#define __HTCKOVSKY_H__

#include <target/dynboard.h>

#define KOVS100_CAM_RESET	0
#define KOVS100_EXT_MIC		17
#define KOVS100_BAT_PRESENT	18
#define KOVS100_UART2DM_RTS	19
#define KOVS100_UART2DM_CTS	20
#define KOVS100_UART2DM_RX	21
#define KOVS100_REARCAM_PWR	23
#define KOVS100_WIFI_IRQ	29
#define KOVS100_HEADSET_IN	30
#define KOVS100_BT_POWER	32
#define KOVS100_AC_DETECT	37
#define KOVS100_SLIDER_IRQ_GPIO	38
#define KOVS100_CAM_FULL_KEY	41
#define KOVS100_CAM_HALF_KEY	42
#define KOVS100_JOYSTICK_IRQ	43
#define KOVS100_N_CHG_ENABLE	44
#define KOVS100_CHARGER_USB	49
#define KOVS100_SPK_UNK_0	57
#define KOVS100_SPK_UNK_1	58
#define KOVS100_BT_ROUTER	63
#define KOVS100_SPK_UNK_2	64
#define KOVS100_SPK_AMP		65
#define KOVS100_HP_AMP		66
#define KOVS100_CHARGER_FULL	67
#define KOVS100_MDDI_PWR	82
#define KOVS100_POWER_KEY	83
#define KOVS100_N_SD_STATUS	94
#define KOVS100_LCD_PWR		98
#define KOVS100_LCD_VSYNC	97
#define KOVS100_VFEMUX_FRONT	99
#define KOVS100_USB_RESET_PHY		100
#define KOVS100_WIFI_PWR	102
#define KOVS100_USB_POWER_PHY		105
#define KOVS100_VCM_PD		107
#define KOVS100_UART2DM_TX	108


extern struct msm7k_board htckovsky_board;
#endif // __HTCKOVSKY_H__
