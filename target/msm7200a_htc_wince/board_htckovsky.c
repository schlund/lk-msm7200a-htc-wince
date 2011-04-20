/*
 * board_htckovsky.c
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
#include <dev/battery/ds2746.h>
#include <dev/fbcon.h>
#include <dev/flash.h>
#include <dev/gpio.h>
#include <dev/gpio_keys.h>
#include <dev/keys.h>
#include <dev/udc.h>
#include <lib/ptable.h>
#include <platform/clock.h>
#include <platform/timer.h>
#include <platform/msm_gpio.h>
#include <platform/msm_i2c.h>
#include <platform/mddi.h>
#include <platform/hsusb.h>
#include <target/dynboard.h>
#include <target/msm7200a_hsusb.h>
#include <target/dex_comm.h>
#include <target/htckovsky.h>
/******************************************************************************
 * LCD
 *****************************************************************************/
struct mddi_table {
	unsigned int reg;
	unsigned int value;
};

struct spi_table {
	unsigned short reg;
	unsigned short value;
};

static struct spi_table epson_spi_init_table[] = {
	{2, 0},
	{3, 0},
	{4, 0},
	{0x10, 0x250},
	{0x20, 0},
	{0x21, 0x2831},
	{0x22, 0x3e},
	{0x23, 0x7400},
	{0x24, 0x7400},
	{0x25, 0x6a06},
	{0x26, 0x7400},
	{0x27, 0x1c06},
	{0x28, 0x1c28},
	{0x29, 0x1c4a},
	{0x2a, 0x666},
	{0x100, 0x33},
	{0x101, 0x3},
	{0x102, 0x3700},
	{0x300, 0x7777},
	{0x301, 0x2116},
	{0x302, 0xc114},
	{0x303, 0x2177},
	{0x304, 0x7110},
	{0x305, 0xc316},
	{0x402, 0},
	{0x501, 0xffff},
	{0x502, 0xffff},
	{0x503, 0xffff},
	{0x504, 0xff},
};

static struct mddi_table mddi_epson_deinit_table[] = {
	{0x504, 0x8001},
	{0x324, 0x3800},
	{1, 0x64},
};

static struct mddi_table mddi_epson_init_table_1[] = {
	{0x30, 0},
	{0x18, 0x3BF},
	{0x20, 0x3034},
	{0x24, 0x4011},
	{0x28, 0x32a},
	{0x2c, 0x1},
	{0x2c, 0x1},
	{0x1, 0x2},
	{0x84, 0x208},
	{0x88, 0x37},
	{0x8C, 0x2113},
	{0x90, 0x0},
	{0x2c, 0x2},
	{0x1, 0x2},
	{0x2c, 0x3},
	{0x100, 0x3402},
	{0x104, 0x380},
	{0x140, 0x3E},
	{0x144, 0xEF},
	{0x148, 0x9},
	{0x14c, 0x4},
	{0x150, 0x0},
	{0x154, 0x333},
	{0x158, 0x31F},
	{0x15c, 0x8},
	{0x160, 0x4},
	{0x164, 0x0},
	{0x168, 0xE3},
	{0x180, 0x57},
	{0x184, 0xDB},
	{0x188, 0xE3},
	{0x18c, 0x0},
	{0x190, 0x0},
	{0x200, 0xA6},
	{0x204, 0x0},
	{0x208, 0x13F},
	{0x20c, 0x0},
	{0x210, 0x0},
	{0x214, 0x0},
	{0x218, 0x0},
	{0x21C, 0x1DF},
	{0x220, 0x0},
	{0x224, 0x0},
	{0x400, 0x8000},
	{0x404, 0x10FF},
	{0x480, 0x4001},
	{0x484, 0x62},
	{0x500, 0x0},
	{0x504, 0x8000},
	{0x508, 0x0},
	{0x50c, 0x0},
	{0x510, 0x0},
	{0x514, 0x0},
	{0x518, 0x1E},
	{0x51C, 0xC7},
	{0x520, 0x1DF},
	{0x524, 0x31F},
	{0x528, 0x0},
	{0x52c, 0x0},
	{0x1, 0x1},
	{0x580, 0x0},
	{0x584, 0x0},
	{0x588, 0xD2C},
	{0x58C, 0xFA0},
	{0x590, 0x10CC},
	{0x594, 0x34},
	{0x598, 0x77},
	{0x59C, 0x13F},
	{0x5A0, 0xEF},
	{0x5a4, 0x0},
	{0x5a8, 0x0},
	{0x1, 0x1},
	{0x600, 0x0},
	{0x604, 0x101},
	{0x608, 0x0},
	{0x60C, 0x80},
	{0x610, 0x0},
	{0x648, 0x0},
	{0x680, 0x8000},
	{0x684, 0x0},
	{0x688, 0x1E},
	{0x68C, 0xC7},
	{0x690, 0x1DF},
	{0x694, 0x31F},
	{0x698, 0x0},
	{0x69c, 0x0},
	{0x6a0, 0x0},
	{0x6a4, 0x0},
	{0x6a8, 0x0},
	{0x6AC, 0x34},
	{0x6B0, 0x77},
	{0x6B4, 0x12B},
	{0x6B8, 0x31},
	{0x6BC, 0xF},
	{0x6C0, 0xF0},
	{0x6c4, 0x0},
	{0x6C8, 0x96},
	{0x700, 0x0},
	{0x704, 0x20A},
	{0x708, 0x400},
	{0x70C, 0x400},
	{0x714, 0xC00},
	{0x718, 0xD2C},
	{0x71C, 0xFA0},
	{0x720, 0x10CC},
	{0x724, 0x14},
	{0x728, 0x3B},
	{0x72C, 0x63},
	{0x730, 0x63},
	{0x734, 0},
	{0x738, 0},
	{0x73c, 0x32},
	{0x740, 0x32},
	{0x744, 0x95},
	{0x748, 0x95},
	{0x800, 0x0},
	{0x804, 0x20A},
	{0x808, 0x400},
	{0x80C, 0x400},
	{0x814, 0xC00},
	{0x818, 0xD2C},
	{0x81C, 0xFA0},
	{0x820, 0x10CC},
	{0x824, 0x14},
	{0x828, 0x3B},
	{0x82C, 0x63},
	{0x830, 0x63},
	{0x834, 0x64},
	{0x838, 0x64},
	{0x83C, 0x64},
	{0x840, 0x64},
	{0x844, 0xC7},
	{0x848, 0xC7},
	{0x900, 0x0},
	{0x904, 0x0},
	{0x908, 0x13F},
	{0x90C, 0xEF},
	{0x910, 0x0},
	{0x914, 0x13F},
	{0x918, 0x0},
	{0x91C, 0xEF},
	{0x600, 0x4000},
	{0xb00, 0x3},
	{0x300, 0x3000},
	{0x304, 0x0},
	{0x308, 0x0},
	{0x30C, 0x4FFF},
	{0x310, 0x7FFF},
	{0x314, 0xFF},
	{0x318, 0x7FFF},
	{0x31C, 0xFFFF},
	{0x320, 0xFF},
	{0x324, 0x6FFF},
	{0x328, 0x0},
	{0x32c, 0x0},
	{0x600, 0x4000}, {1, 0x2},
	{0x614, 0},
	{0x618, 0},
	{0x61c, 0},
	{0x620, 0},
	{0x628, 0},
	{0x62c, 0},
	{0x630, 0},
	{0x634, 0},
	{0x638, 0},
	{0x63c, 0},
	{0x640, 0},
	{0x644, 0},
	{0x324, 0x3800},
	{1, 0x64},
};

static struct mddi_table mddi_epson_init_table_2[] = {
	{0x324, 0x2800},
	{1, 0x104},
	{0x504, 0x1},
	{1, 0xa},
};

static inline void
htckovsky_process_epson_spi_table(struct spi_table *table, size_t count)
{
	unsigned i;
	for (i = 0; i < count; i++) {
		mddi_remote_write(0x10, 0x110);
		mddi_remote_write(table[i].reg, 0x108);
		mdelay(1);
		mddi_remote_write(0x12, 0x110);
		mddi_remote_write(table[i].value, 0x108);
	}
}

static void htckovsky_process_mddi_table(struct mddi_table *table, size_t count)
{
	unsigned i;
	for (i = 0; i < count; i++) {
		uint32_t reg = table[i].reg;
		uint32_t value = table[i].value;

		switch (reg) {
		case 0:
			udelay(value);
			break;

		case 1:
			mdelay(value);
			break;

		default:
			mddi_remote_write(value, reg);
		}
	}
}

static void htckovsky_mddi_bridge_reset(int reset_asserted) {
		if (reset_asserted) {
				gpio_set(KOVS100_MDDI_PWR, 0);
				mdelay(10);
				gpio_set(KOVS100_MDDI_PWR, 1);
				mdelay(15);
		}
		else {
				gpio_set(KOVS100_MDDI_PWR, 0);
				mdelay(1);
		}
}

static void htckovsky_mddi_power_client(int on) {
	return;
	if (on) {
		gpio_set(KOVS100_LCD_PWR, 1);
		mdelay(5);
		htckovsky_mddi_bridge_reset(1);
	} else {
		htckovsky_mddi_bridge_reset(0);
		gpio_set(KOVS100_LCD_PWR, 0);
		mdelay(10);
	}
}

static void htckovsky_mddi_panel_blank() {
	htckovsky_process_mddi_table(ARRAY_AND_SIZE(mddi_epson_deinit_table));
}

static void htckovsky_mddi_panel_unblank()
{
	htckovsky_mddi_panel_blank();
	htckovsky_process_mddi_table(ARRAY_AND_SIZE(mddi_epson_init_table_1));
	htckovsky_process_epson_spi_table(ARRAY_AND_SIZE(epson_spi_init_table));
	htckovsky_process_mddi_table(ARRAY_AND_SIZE(mddi_epson_init_table_2));
}

static void htckovsky_panel_init(struct mddi_client_caps *client_caps)
{
	htckovsky_mddi_panel_unblank();
}

static void htckovsky_panel_deinit(void) {
	htckovsky_mddi_panel_blank();
}

static void htckovsky_panel_backlight(int light) {
	unsigned char buffer[3];
	buffer[0] = 0x12;
	buffer[1] = light ? 255 : 0;
	buffer[2] = light ? 200 : 0;
	msm_i2c_write(0x66, buffer, 3);
}

static struct msm_mddi_pdata htckovsky_mddi_pdata = {
	.panel_init = htckovsky_panel_init,
	.panel_deinit = htckovsky_panel_deinit,
	.panel_power = htckovsky_mddi_power_client,
	.panel_backlight = htckovsky_panel_backlight,
	.resx = 480,
	.resy = 800,
};

static void htckovsky_display_init(void)
{
	struct fbcon_config* fb_config = mddi_init(&htckovsky_mddi_pdata);
	ASSERT(fb_config);
	fb_config->rotation = ROTATE_180;
	fbcon_setup(fb_config);
}

static void htckovsky_display_exit(void) {
	fbcon_teardown();
	mddi_deinit(&htckovsky_mddi_pdata);
}

/******************************************************************************
 * LEDs
 *****************************************************************************/
static void htckovsky_set_key_light(int brightness) {
	unsigned char buffer[3] = {};

	//Front key light
	buffer[0] = 0x14;
	buffer[1] = brightness ? 0x80 : 0;
	buffer[2] = brightness;
	msm_i2c_write(0x66, buffer, 3);
}

static void htckovsky_set_color_leds(bool red, bool green, bool blue) {
	unsigned char buffer[2] = {};
	buffer[0] = 0x20;
	buffer[1] = red | (green << 1) | (blue << 2);
	msm_i2c_write(0x66, buffer, 2);
}

/******************************************************************************
 * USB
 *****************************************************************************/
static void htckovsky_usb_disable(void)
{
	gpio_set(KOVS100_USB_RESET_PHY, 0);
	gpio_set(KOVS100_USB_POWER_PHY, 0);
}

static void htckovsky_usb_enable(void)
{
	gpio_set(0x54, 1);
	gpio_set(KOVS100_BT_ROUTER, 0);

	gpio_set(KOVS100_USB_POWER_PHY, 1);
	gpio_set(KOVS100_USB_RESET_PHY, 0);
	mdelay(3);
	gpio_set(KOVS100_USB_RESET_PHY, 1);
	mdelay(3);
}

static void msm_hsusb_set_state(int state)
{
	if (state) {
		htckovsky_usb_enable();
		msm7200a_ulpi_config(1);
	} else {
		msm7200a_ulpi_config(0);
		htckovsky_usb_disable();
	}
}

static void htckovsky_set_charger(enum psy_charger_state state) {
	gpio_set(KOVS100_N_CHG_ENABLE, state == CHG_OFF);
}

static bool htckovsky_usb_online(void) {
	return dex_get_vbus_state();
}

static bool htckovsky_ac_online(void) {
	return 0;
}

static bool htckovsky_want_charging(void) {
	uint32_t voltage = ds2746_read_voltage_mv(0x36);
	return voltage < DS2746_HIGH_VOLTAGE;
}

static void htckovsky_wait_for_charge(void) {
	uint32_t voltage;
	uint8_t no_charger_cycles = 0;
	do {
		htckovsky_set_charger(CHG_OFF);
		mdelay(10);
		voltage = ds2746_read_voltage_mv(0x36);
		bool power = htckovsky_usb_online();
		if (power) {
			htckovsky_set_charger(CHG_USB_HIGH);
			no_charger_cycles = 0;
			htckovsky_set_color_leds(0, 1, 0);
		}
		else {
			no_charger_cycles++;
			htckovsky_set_charger(CHG_OFF);
			htckovsky_set_color_leds(1, 0, 0);

			//If no charger connected for 6 seconds and we're low on battery
			if (no_charger_cycles > 12) {
				target_shutdown();
			}
		}
		printf("%s: voltage=%d\n", __func__, voltage);
		mdelay(500);
	} while (voltage < 3650);
	if (get_boot_reason() == BOOT_CHARGING)
		reboot_device(BOOT_WARM);
}

static struct pda_power_supply htckovsky_power_supply = {
	.is_ac_online = htckovsky_ac_online,
	.is_usb_online = htckovsky_usb_online,
	.set_charger_state = htckovsky_set_charger,
	.want_charging = htckovsky_want_charging,
};

static struct msm_hsusb_pdata htckovsky_hsusb_pdata = {
	.set_ulpi_state = msm_hsusb_set_state,
	.power_supply = &htckovsky_power_supply,
};

static struct udc_device htckovsky_udc_device = {
	.vendor_id = 0x18d1,
	.product_id = 0xD00D,
	.version_id = 0x0100,
	.manufacturer = "Sony Ericsson",
	.product = "Xperia X1",
};

static void htckovsky_usb_init(void) {
	msm_hsusb_init(&htckovsky_hsusb_pdata);
	int ret = udc_init(&htckovsky_udc_device);
	dprintf(ALWAYS, "udc_init done with ret=%d\n", ret);
}

/******************************************************************************
 * GPIO Keys
 *****************************************************************************/
static struct gpio_key htckovsky_gpio_keys[] = {
	{KOVS100_POWER_KEY, KEY_POWER, true, "Power"},
	{KOVS100_CAM_FULL_KEY, KEY_DOWN, true, "Camera full press"},
	{KOVS100_CAM_HALF_KEY, KEY_UP, true, "Camera half press"},
};

static struct gpio_keys_pdata htckovsky_gpio_keys_pdata = {
	.keys = htckovsky_gpio_keys,
	.nkeys = ARRAY_SIZE(htckovsky_gpio_keys),
};

static void htckovsky_gpio_keys_init(void) {
	for (unsigned i = 0; i < ARRAY_SIZE(htckovsky_gpio_keys); i++) {
		msm_gpio_config(MSM_GPIO_CFG(htckovsky_gpio_keys[i].gpio,
				0, MSM_GPIO_CFG_INPUT,
				MSM_GPIO_CFG_NO_PULL, MSM_GPIO_CFG_2MA), 0);
	}
	gpio_keys_init(&htckovsky_gpio_keys_pdata);
}
/******************************************************************************
 * NAND
 *****************************************************************************/
/* CE starts at 0x02820000
 * block size is 0x20000
 * offset is the size of all the data we don't want to mess with,
 * namely, bootloader and calibration
 * the last 16 mbytes are used for nand block markers etc so don't
 * use anything after 0x1fe00000
 *
 * TODO: make a fake mbr, specify LK as XIP
 * and the rest as imgfs or uldr and correct partitions offsets
 *
 */

#define HTCKOVSKY_FLASH_OFFSET 321
static struct ptable flash_ptable;
//static struct ptentry htckovsky_part_list[] = {
//	{
//		.start = 0,
//		.length = 8,
//		.name = "LK",
//		.type = TYPE_APPS_PARTITION,
//		.perm = PERM_WRITEABLE,
//	},
//	{
//		.start = 8,
//		.length = 72,
//		.name = "boot",
//		.type = TYPE_APPS_PARTITION,
//		.perm = PERM_WRITEABLE,
//	},
//	{
//		.start = 80,
//		.length = 0xe5f,
//		.name = "other",
//		.type = TYPE_APPS_PARTITION,
//		.perm = PERM_WRITEABLE,
//	},
//};

static struct ptentry htckovsky_part_list[] = {
	{
		.start = 0xe17,
		.length = 80,
		.name = "recovery",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = 0xe67,
		.length = 72,
		.name = "boot",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
};

static void htckovsky_nand_init(void) {
	struct flash_info *flash_info;
	int nparts = ARRAY_SIZE(htckovsky_part_list);
	unsigned offset = HTCKOVSKY_FLASH_OFFSET;
	ptable_init(&flash_ptable);
	flash_init();
	flash_info = flash_get_info();
	if (!flash_info) {
		printf("%s: error initializing flash");
		return;
	}

	for (int i = 0; i < nparts; i++) {
		struct ptentry *ptn = &htckovsky_part_list[i];
		unsigned len = ptn->length;

		if ((len == 0) && (i == nparts - 1)) {
			len = flash_info->num_blocks - offset - ptn->start;
		}
		ptable_add(&flash_ptable, ptn->name, offset + ptn->start,
			len, ptn->flags, ptn->type, ptn->perm);
	}

	ptable_dump(&flash_ptable);
	flash_set_ptable(&flash_ptable);
}
/******************************************************************************
 * Exports
 *****************************************************************************/
static void htckovsky_early_init(void) {
	htckovsky_display_init();
}

static void htckovsky_init(void) {
	if (get_boot_reason() == BOOT_CHARGING) {
		htckovsky_set_color_leds(1, 1, 0);
	}
	htckovsky_wait_for_charge();

	htckovsky_set_key_light(160);
	htckovsky_set_color_leds(1, 0, 1);
	//set half rate to reduce flicker
	clk_set_rate(MDP_CLK, 109 * 1000 * 1000);
	htckovsky_usb_init();
	htckovsky_nand_init();
	htckovsky_gpio_keys_init();
}

static void htckovsky_exit(void) {
	htckovsky_set_color_leds(0, 1, 1);
//	htckovsky_display_exit();
}

struct msm7k_board htckovsky_board = {
	.early_init = htckovsky_early_init,
	.init = htckovsky_init,
	.exit = htckovsky_exit,
	.cmdline = "fbcon=rotate:2 init=/init physkeyboard=kovsq"
	" force_cdma=0 hwrotation=180 lcd.density=240"
	" msmvkeyb_toggle=off",
};
