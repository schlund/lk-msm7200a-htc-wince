/* linux/arch/arm/mach-msm/board-htckovsky.c
 *
 * Copyright (C) 2011 Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * Copyright (C) 2007-2011 htc-linux.org
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

#include <stdio.h>
#include <kernel/mutex.h>
#include <platform/clock.h>
#include <platform/timer.h>
#include <platform/pcom.h>

static mutex_t clk_mutex;

int clk_enable(unsigned id)
{
	int rc;
	mutex_acquire(&clk_mutex);
	rc = pcom_clock_enable(id);
	mutex_release(&clk_mutex);
	return rc;
}

void clk_disable(unsigned id)
{
	mutex_acquire(&clk_mutex);
	pcom_clock_disable(id);	
	mutex_release(&clk_mutex);
}

int clk_set_rate(unsigned id, unsigned freq)
{
	int rc;
	mutex_acquire(&clk_mutex);
	rc = pcom_clock_set_rate(id, freq);
	mutex_release(&clk_mutex);
	return rc;
}

void msm_clock_init(void)
{
	mutex_init(&clk_mutex);
	mutex_acquire(&clk_mutex);
	mutex_release(&clk_mutex);
}
