/* htcrhodium.h
 *
 * Copyright (C) 2011 Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * Copyright (C) htc-linux.org and XDANDROID project
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

#ifndef __HTCRHODIUM_H__
#define __HTCRHODIUM_H__

#include <target/dynboard.h>

#define RHODIUM_BAT_IRQ			28
#define RHODIUM_USB_AC_PWR		32
#define RHODIUM_CHARGE_EN_N		44
#define RHODIUM_KPD_IRQ			27

#define RHODIUM_END_KEY			18
#define RHODIUM_VOLUMEUP_KEY	39
#define RHODIUM_VOLUMEDOWN_KEY	40
#define RHODIUM_POWER_KEY		83

#define RHOD_LCD_PWR			0x63
#define RHOD_LCD_VSYNC			97
#define RHODIUM_USBPHY_RST		100

extern struct msm7k_board htcrhodium_board;
#endif // __HTCRHODIUM_H__
