/* htcphoton.h
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

#ifndef __HTCPHOTON_H__
#define __HTCPHOTON_H__

#define PHOTON_USB_RESET_PHY		100
#define PHOTON_USB_POWER_PHY		105

#define PHOTON_GPIO_CHARGE_EN_N		(23) //standard charger
#define PHOTON_FAST_CHARGER_DIS		(17) //disabled when AC cable is plugged
#define PHOTON_FAST_CHARGER_EN		(32) //enabled when AC cable is plugged

#include <target/dynboard.h>

extern struct msm7k_board htcphoton_board;
#endif // __HTCPHOTON_H__
