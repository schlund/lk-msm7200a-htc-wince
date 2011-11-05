/*
 * board_htcphoton.c
 * provides support for the HTC Kovsky board for LK bootloader
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

#include <array.h>
#include <bootreason.h>
#include <debug.h>
#include <reg.h>
#include <target.h>
#include <dev/fbcon.h>
#include <dev/flash.h>
#include <dev/gpio.h>
#include <dev/gpio_keys.h>
#include <dev/keys.h>
#include <dev/udc.h>
#include <lib/ptable.h>
#include <platform/clock.h>
#include <platform/timer.h>
#include <platform/msm_i2c.h>
#include <platform/mddi.h>
#include <platform/hsusb.h>
#include <platform/pcom.h>
#include <target/dynboard.h>
#include <target/dex_comm.h>
#include <target/htcphoton.h>

static struct msm_mddi_pdata htcphoton_mddi_pdata = {
	.resx = 320,
	.resy = 480,
};

static void htcphoton_display_init(void)
{
	struct fbcon_config* fb_config = mddi_init(&htcphoton_mddi_pdata);
	ASSERT(fb_config);
	fbcon_setup(fb_config);
}

static void htcphoton_display_exit(void) {
	fbcon_teardown();
	mddi_deinit(&htcphoton_mddi_pdata);
}

/******************************************************************************
 * USB
 *****************************************************************************/
static void msm_hsusb_set_state(int state)
{
	if (state) {
		msm_proc_comm(PCOM_MSM_HSUSB_PHY_RESET, 0, 0);	
	}
}

static void htcphoton_set_charger(enum psy_charger_state state) {
	return;
	if (state == CHG_OFF) {
		gpio_set(PHOTON_FAST_CHARGER_DIS, 1);
		gpio_set(PHOTON_FAST_CHARGER_EN, 0);
	}
	else {
		gpio_set(PHOTON_FAST_CHARGER_DIS, 0);
		gpio_set(PHOTON_FAST_CHARGER_EN, 1);
	}
}

static bool htcphoton_usb_online(void) {
	return dex_get_vbus_state();
}

static bool htcphoton_ac_online(void) {
	return 0;//!gpio_get(PHOTON_GPIO_CHARGE_EN_N);
}

static bool htcphoton_want_charging(void) {
	return true;
}

static struct pda_power_supply htcphoton_power_supply = {
	.is_ac_online = htcphoton_ac_online,
	.is_usb_online = htcphoton_usb_online,
	.set_charger_state = htcphoton_set_charger,
	.want_charging = htcphoton_want_charging,
};

static struct msm_hsusb_pdata htcphoton_hsusb_pdata = {
	.set_ulpi_state = msm_hsusb_set_state,
	.power_supply = &htcphoton_power_supply,
};

static struct udc_device htcphoton_udc_device = {
	.vendor_id = 0x18D1,
	.product_id = 0xD00D,
	.version_id = 0x0100,
	.manufacturer = "HTC Corporation",
	.product = "HD Mini",
};

static void htcphoton_usb_init(void) {
	msm_hsusb_init(&htcphoton_hsusb_pdata);
	int ret = udc_init(&htcphoton_udc_device);
	dprintf(ALWAYS, "udc_init done with ret=%d\n", ret);
}

/******************************************************************************
 * NAND
 *****************************************************************************/
/* CE starts at 0x02820000
 * block size is 0x20000
 * offset is the size of all the data we don't want to mess with,
 * namely, bootloader and calibration
 */

#define HTCPHOTON_RESERVED_SECTORS 321
#define HTCPHOTON_FLASH_SIZE 0x1000

#define HTCPHOTON_FLASH_OFFSET 0

#define HTCPHOTON_FLASH_LK_START (HTCPHOTON_FLASH_OFFSET)
#define HTCPHOTON_FLASH_LK_SIZE 1

#define HTCPHOTON_FLASH_RECOVERY_START (HTCPHOTON_FLASH_LK_START + \
	HTCPHOTON_FLASH_LK_SIZE)
#define HTCPHOTON_FLASH_RECOVERY_SIZE 0x50

#define HTCPHOTON_FLASH_MISC_START (HTCPHOTON_FLASH_RECOVERY_START + \
	HTCPHOTON_FLASH_RECOVERY_SIZE)
#define HTCPHOTON_FLASH_MISC_SIZE 5

#define HTCPHOTON_FLASH_BOOT_START (HTCPHOTON_FLASH_MISC_START + \
	HTCPHOTON_FLASH_MISC_SIZE)
#define HTCPHOTON_FLASH_BOOT_SIZE 0x50

#define HTCPHOTON_FLASH_SYS_START (HTCPHOTON_FLASH_BOOT_START + \
	HTCPHOTON_FLASH_BOOT_SIZE)
#define HTCPHOTON_FLASH_SYS_SIZE 0x390

#define HTCPHOTON_FLASH_DATA_START (HTCPHOTON_FLASH_SYS_START + \
	HTCPHOTON_FLASH_SYS_SIZE)
#define HTCPHOTON_FLASH_DATA_SIZE 0xa00

#define HTCPHOTON_FLASH_CACHE_SIZE 0x80
#define HTCPHOTON_FLASH_CACHE_START (HTCPHOTON_FLASH_SIZE - \
	(HTCPHOTON_FLASH_OFFSET + HTCPHOTON_FLASH_CACHE_SIZE))

#if HTCPHOTON_FLASH_CACHE_START < HTCPHOTON_FLASH_OFFSET
	#error Cache partition starts in protected zone
#endif

#if (HTCPHOTON_FLASH_DATA_START + HTCPHOTON_FLASH_DATA_SIZE) \
	> HTCPHOTON_FLASH_CACHE_START
	#error Incorrect flash layout. Verify partition sizes manually
#endif

static struct ptable flash_ptable;
static struct ptentry htcphoton_part_list[] = {
	{
		.start = HTCPHOTON_FLASH_LK_START,
		.length = HTCPHOTON_FLASH_LK_SIZE,
		.name = "lkbootloader",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = HTCPHOTON_FLASH_RECOVERY_START,
		.length = HTCPHOTON_FLASH_RECOVERY_SIZE,
		.name = "recovery",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = HTCPHOTON_FLASH_MISC_START,
		.length = HTCPHOTON_FLASH_MISC_SIZE,
		.name = "misc",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = HTCPHOTON_FLASH_BOOT_START,
		.length = HTCPHOTON_FLASH_BOOT_SIZE,
		.name = "boot",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = HTCPHOTON_FLASH_SYS_START,
		.length = HTCPHOTON_FLASH_SYS_SIZE,
		.name = "system",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = HTCPHOTON_FLASH_DATA_START,
		.length = HTCPHOTON_FLASH_DATA_SIZE,
		.name = "userdata",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = HTCPHOTON_FLASH_CACHE_START,
		.length = HTCPHOTON_FLASH_CACHE_SIZE,
		.name = "cache",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
};

static void htcphoton_nand_init(void) {
	struct flash_info *flash_info;
	int nparts = ARRAY_SIZE(htcphoton_part_list);
	unsigned offset = HTCPHOTON_FLASH_OFFSET;
	ptable_init(&flash_ptable);
	flash_init();
	flash_info = flash_get_info();
	if (!flash_info) {
		printf("%s: error initializing flash");
		return;
	}

	for (int i = 0; i < nparts; i++) {
		struct ptentry *ptn = &htcphoton_part_list[i];
		unsigned len = ptn->length;

		if ((len == 0) && (i == nparts - 1)) {
			len = flash_info->num_blocks - offset - ptn->start;
		}
		ptable_add(&flash_ptable, ptn->name, offset + ptn->start,
			len, ptn->flags, ptn->type, ptn->perm);
		break;
	}

	flash_set_ptable(&flash_ptable);
}

/******************************************************************************
 * Exports
 *****************************************************************************/
static void htcphoton_early_init(void) {
	photon_vibe(200);
	htcphoton_display_init();
}

static void htcphoton_init(void) {
	htcphoton_usb_init();
	htcphoton_nand_init();
}

struct msm7k_board htcphoton_board = {
	.early_init = htcphoton_early_init,
	.init = htcphoton_init,
	.cmdline = "rw",
};
