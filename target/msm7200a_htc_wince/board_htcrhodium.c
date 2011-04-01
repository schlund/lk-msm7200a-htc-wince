/* board_htcrhodium.c
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
#include <debug.h>
#include <reg.h>
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
#include <target/dex_comm.h>
#include <target/dynboard.h>
#include <target/msm7200a_hsusb.h>
#include <target/htcrhodium.h>
/******************************************************************************
 * LCD
 *****************************************************************************/
int rhod_panel_id = 0;

#define REG_WAIT (0xffff)
struct nov_regs {
	unsigned reg;
	unsigned val;
} nov_init_seq[] = {
	/* Auo es1 */
	{0x5100, 0x00},
	{0x1100, 0x01},
	{REG_WAIT, 0x80},
	{0x4e00, 0x00},
	{0x3a00, 0x05},
	{REG_WAIT, 0x1},
	{0x3500, 0x02},
	{0x4400, 0x00},
	{0x4401, 0x00},
	{0x5e00, 0x00},
	{0x6a01, 0x00},
	{0x6a02, 0x01},
	{0x5301, 0x10},
	{0x5500, 0x02},
	{0x6a17, 0x00},
	{0x6a18, 0xff},
	{0x2900, 0x01},
	{0x5300, 0x2c},
	{0x5303, 0x01},
};

struct nov_regs nov_deinit_seq[] = {

	{0x2800, 0x00},		// display off
	{0x1000, 0x00},		// sleep-in
};

struct nov_regs nov_init_seq1[] = {
	/* EID es3 */		// Table modified by WisTilt2
	{0x1100, 0x01},		// sleep-out
	{REG_WAIT, 30},		// 30ms delay - tested safe down to 20ms on all rhods
	{0x2900, 0x01},		// display on
	{0x3500, 0x00},		// tearing effect
	{0x3a00, 0x55},		// bits per pixel
	{0x4400, 0x00},		// set tear line 1st param
	{0x4401, 0x00},		// set tear line 2nd param
	{0x4e00, 0x00},		// set spi/i2c mode
	{0x5100, 0x20},		// display brightness
	{0x5301, 0x10},		// led control pins
	{0x5302, 0x01},		// labc dimming on/off
	{0x5303, 0x01},		// labc rising dimming style
	{0x5304, 0x01},		// labc falling dimming style
	{0x5500, 0x02},		// cabc enable & image type
	{0x5e00, 0x04},		// cabc minimum brightness
	{0x5e03, 0x05},		// labc adc & hysteresis enable
	{0x5e04, 0x01},		// labc reference voltage 1.7v
	{0x6a01, 0x00},		// pwm duty/freq control
	{0x6a02, 0x01},		// pwm freq
	{0x6a17, 0x00},		// cabc pwm force off
	{0x6a18, 0xff},		// cabc pwm duty
	{0x6f00, 0x00},		// clear ls lsb
	{0x6f01, 0x00},		// clear ls msb
	{0x6f02, 0x00},		// force cabc pwm off
	{0x6f03, 0x00},		// force pwm duty
	{0x5300, 0x3c},		// set autobl bit during init
};

static void htcrhodium_mddi_panel_init(struct mddi_client_caps *client_caps)
{
	unsigned i, reg, val;

	switch (rhod_panel_id)	//Added by WisTilt2 - Panel detect
	{
	case 0x14:	/* EID - Rhod210,400,500 */
	case 0x15:	/* EID - Rhod300 */
		for (i = 0; i < ARRAY_SIZE(nov_init_seq1); i++)
		{
			reg = nov_init_seq1[i].reg;
			val = nov_init_seq1[i].val;
			if (reg == REG_WAIT)
				mdelay(val);
			else
				mddi_remote_write(val, reg);
		}
		break;

	case 0x01:	/* AUO - Rhod100 */
	case 0x13:	/* AUO - Rhod100 */
		for (i = 0; i < ARRAY_SIZE(nov_init_seq); i++)
		{
			reg = nov_init_seq[i].reg;
			val = nov_init_seq[i].val;
			if (reg == REG_WAIT)
				mdelay(val);
			else
				mddi_remote_write(val, reg);
		}
		break;

	default:
		dprintf(ALWAYS, "%s: Unknown panel_id %x detected!\n", __func__, rhod_panel_id);
		for (i = 0; i < ARRAY_SIZE(nov_init_seq1); i++)	//Default to EID on unknown
		{
			reg = nov_init_seq1[i].reg;
			val = nov_init_seq1[i].val;
			if (reg == REG_WAIT)
				mdelay(val);
			else
				mddi_remote_write(val, reg);
		}
		break;
	}
	mddi_remote_write(0x00, 0x2900);	// display on
	mddi_remote_write(0x2c, 0x5300);	// toggle autobl bit
}

static void htcrhodium_mddi_panel_deinit(void)
{
	for (unsigned i = 0; i < ARRAY_SIZE(nov_deinit_seq); i++) {
		mddi_remote_write(nov_deinit_seq[i].val, nov_deinit_seq[i].reg);
		mdelay(5);
	}
}

static void htcrhodium_panel_backlight(int light) {
	unsigned char buffer[3];
	buffer[0] = 0x24;
	buffer[1] = light ? 6 : 0;
	msm_i2c_write(0x66, buffer, 2);
}

static struct msm_mddi_pdata htcrhodium_mddi_pdata = {
	.panel_init = htcrhodium_mddi_panel_init,
	.panel_deinit = htcrhodium_mddi_panel_deinit,
	.panel_backlight = htcrhodium_panel_backlight,
	.resx = 480,
	.resy = 800,
};

static void htcrhodium_display_init(void)
{
	struct fbcon_config* fb_config = mddi_init(&htcrhodium_mddi_pdata);
	ASSERT(fb_config);
	fbcon_setup(fb_config);
}

static void htcrhodium_display_exit(void) {
	fbcon_teardown();
	mddi_deinit(&htcrhodium_mddi_pdata);
}

/******************************************************************************
 * USB
 *****************************************************************************/
static void htcrhodium_usb_disable(void)
{
	gpio_set(0x64, 0);
	gpio_set(0x69, 0);
}

static void htcrhodium_usb_enable(void)
{
	gpio_set(0x69, 1);
	gpio_set(0x64, 0);
	mdelay(3);
	gpio_set(0x64, 1);
	mdelay(3);
}

static void msm_hsusb_set_state(int state)
{
	if (state) {
		htcrhodium_usb_enable();
		msm7200a_ulpi_config(1);
	} else {
		msm7200a_ulpi_config(0);
		htcrhodium_usb_disable();
	}
}

static void htcrhodium_set_charger(enum psy_charger_state state) {
	gpio_set(RHODIUM_CHARGE_EN_N, state != CHG_OFF);
}

static bool htcrhodium_usb_online(void) {
	return dex_get_vbus_state();
}

static bool htcrhodium_ac_online(void) {
	return 0;
}

static bool htcrhodium_want_charging(void) {
	return true;
}

static struct pda_power_supply htcrhodium_power_supply = {
	.is_ac_online = htcrhodium_ac_online,
	.is_usb_online = htcrhodium_usb_online,
	.set_charger_state = htcrhodium_set_charger,
	.want_charging = htcrhodium_want_charging,
};

static struct msm_hsusb_pdata htcrhodium_hsusb_pdata = {
	.set_ulpi_state = msm_hsusb_set_state,
	.power_supply = &htcrhodium_power_supply,
};

static struct udc_device htcrhodium_udc_device = {
	.vendor_id = 0x18d1,
	.product_id = 0xD00D,
	.version_id = 0x0100,
	.manufacturer = "HTC Corporation",
	.product = "Touch Pro 2",
};

static void htcrhodium_usb_init(void) {
	msm_hsusb_init(&htcrhodium_hsusb_pdata);
	int ret = udc_init(&htcrhodium_udc_device);
	dprintf(ALWAYS, "udc_init done with ret=%d\n", ret);
}

/******************************************************************************
 * GPIO Keys
 *****************************************************************************/
static struct gpio_key htcrhodium_gpio_keys[] = {
	{RHODIUM_POWER_KEY, KEY_POWER, true, "Power Key"},
	{RHODIUM_VOLUMEDOWN_KEY, KEY_VOLUMEDOWN, true, "Volume Down"},
	{RHODIUM_VOLUMEUP_KEY, KEY_VOLUMEUP, true, "Volume Up"},
	{RHODIUM_END_KEY, KEY_END, true, "End Key"},
};

static struct gpio_keys_pdata htcrhodium_gpio_keys_pdata = {
	.keys = htcrhodium_gpio_keys,
	.nkeys = ARRAY_SIZE(htcrhodium_gpio_keys),
};

static void htcrhodium_gpio_keys_init(void) {
	for (unsigned i = 0; i < ARRAY_SIZE(htcrhodium_gpio_keys); i++) {
		msm_gpio_config(MSM_GPIO_CFG(htcrhodium_gpio_keys[i].gpio,
				0, MSM_GPIO_CFG_INPUT,
				MSM_GPIO_CFG_NO_PULL, MSM_GPIO_CFG_2MA), 0);
	}
	gpio_keys_init(&htcrhodium_gpio_keys_pdata);
}
/******************************************************************************
 * NAND
 *****************************************************************************/
/* CE starts at 0x02820000
 * block size is 0x20000
 * offset is the size of all the data we don't want to mess with,
 * namely, bootloader and calibration
 * the last 16 mbytes are used for nand block markers etc so don't
 * use anything after 0x1fe00000.. but ACL uses it for cache, so i don't care
 *
 */

#define HTCRHODIUM_FLASH_OFFSET 321

static struct ptable flash_ptable;
static struct ptentry htcrhodium_part_list[] = {
	{
		.start = 0,
		.length = 8,
		.name = "LK",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = 8,
		.length = 72,
		.name = "boot",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = 152,
		.length = 1200,
		.name = "system",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = 1352,
		.length = 2407,
		.name = "data",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
	{
		.start = 3759,
		.length = 160,
		.name = "cache",
		.type = TYPE_APPS_PARTITION,
		.perm = PERM_WRITEABLE,
	},
};


static void htcrhodium_nand_init(void) {
	struct flash_info *flash_info;
	int nparts = ARRAY_SIZE(htcrhodium_part_list);
	unsigned offset = HTCRHODIUM_FLASH_OFFSET;
	ptable_init(&flash_ptable);
	flash_init();
	flash_info = flash_get_info();
	if (!flash_info) {
		printf("%s: error initializing flash");
		return;
	}

	for (int i = 0; i < nparts; i++) {
		struct ptentry *ptn = &htcrhodium_part_list[i];
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
static void htcrhodium_early_init(void) {
	rhod_panel_id = readl(0x81034);
	htcrhodium_display_init();
}

static void htcrhodium_init(void) {
//	clk_set_rate(MDP_CLK, 192 * 1000 * 1000);
	htcrhodium_usb_init();
	htcrhodium_nand_init();
	htcrhodium_gpio_keys_init();
}

static void htcrhodium_exit(void) {
//	htcrhodium_set_light(0);
//	htcrhodium_display_exit();
}

struct msm7k_board htcrhodium_board = {
	.early_init = htcrhodium_early_init,
	.init = htcrhodium_init,
	.exit = htcrhodium_exit,
};
