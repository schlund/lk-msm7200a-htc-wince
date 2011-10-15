/* platform/msm7200a/platform.c - common initialization code for msm7200a
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
 */

#include <debug.h>
#include <platform.h>
#include <platform/debug.h>
#include <platform/clock.h>
#include <platform/timer.h>
#include <platform/interrupts.h>
#include <target.h>

void platform_early_init(void)
{
	platform_init_interrupts();
	platform_init_timer();
	msm_clock_init();
}

void platform_init(void)
{
	dprintf(INFO, "platform_init()\n");
}

void platform_exit(void) {
	target_exit();
	platform_uninit_timer();
	platform_deinit_interrupts();
}

