/* msm7200a_hsusb.c
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

#include <platform/msm_gpio.h>
#include <dev/gpio.h>

void msm7200a_ulpi_config(int enable)
{
	int n;
	/* Configure ULPI DATA pins */
	for (n = 0x6f; n <= 0x76; n++) {
		msm_gpio_config(MSM_GPIO_CFG(n, 1,
					     enable ? MSM_GPIO_CFG_INPUT :
					     MSM_GPIO_CFG_OUTPUT,
					     enable ? MSM_GPIO_CFG_NO_PULL :
					     MSM_GPIO_CFG_PULL_DOWN,
					     MSM_GPIO_CFG_2MA), 0);
	}
	msm_gpio_config(MSM_GPIO_CFG(0x77, 1, MSM_GPIO_CFG_INPUT,
				     enable ? MSM_GPIO_CFG_NO_PULL :
				     MSM_GPIO_CFG_PULL_DOWN, MSM_GPIO_CFG_2MA),
			0);
	msm_gpio_config(MSM_GPIO_CFG
			(0x78, 1, MSM_GPIO_CFG_INPUT,
			 enable ? MSM_GPIO_CFG_NO_PULL : MSM_GPIO_CFG_PULL_DOWN,
			 MSM_GPIO_CFG_2MA), 0);
	msm_gpio_config(MSM_GPIO_CFG
			(0x79, 1, MSM_GPIO_CFG_OUTPUT,
			 enable ? MSM_GPIO_CFG_NO_PULL : MSM_GPIO_CFG_PULL_UP,
			 MSM_GPIO_CFG_2MA), 0);
}
