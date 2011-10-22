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
#include <arch/arm/mmu.h>
#include <platform.h>
#include <platform/debug.h>
#include <platform/clock.h>
#include <platform/timer.h>
#include <platform/interrupts.h>
#include <platform/msm_gpio.h>
#include <dev/fbcon.h>
#include <target.h>

void platform_early_init(void)
{
	platform_init_interrupts();
	platform_init_timer();
	msm_clock_init();
	msm_gpio_init();
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

void platform_init_mmu_mappings(void)
{
    uint32_t sections = 1152;

    /* Map io mapped peripherals as device non-shared memory */
    while (sections--)
    {
        arm_mmu_map_section(0x88000000 + (sections << 20),
                            0x88000000 + (sections << 20),
                            (MMU_MEMORY_TYPE_DEVICE_NON_SHARED |
                             MMU_MEMORY_AP_READ_WRITE));
    }
}
