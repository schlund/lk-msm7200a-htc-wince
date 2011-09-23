/*
 * init.c - contains common code for msm7200A htc phones
 *
 * Copyright (C) 2011 Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * Copyright (C) 2007-2011 htc-linux.org and XDANDROID project
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
#include <platform.h>
#include <string.h>
#include <target.h>
#include <arch/mtype.h>
#include <dev/gpio.h>
#include <kernel/thread.h>
#include <target/dynboard.h>
#include <target/htckovsky.h>
#include <target/htcrhodium.h>
#include <target/dex_comm.h>
#include <platform/acpuclock.h>
#include <platform/clock.h>
#include <platform/irqs.h>
#include <platform/iomap.h>
#include <platform/msm_gpio.h>
#include <platform/msm_i2c.h>
#include <platform/timer.h>

static enum mtype mtype = 0;
static struct msm7k_board *board = NULL;

#define MSM7200A_GPIO_I2C_CLK 60
#define MSM7200A_GPIO_I2C_DAT 61
static void i2c_set_mux(int to_i2c) {
	if (to_i2c) {
		msm_gpio_config(MSM_GPIO_CFG(MSM7200A_GPIO_I2C_CLK, 1, MSM_GPIO_CFG_OUTPUT,
				MSM_GPIO_CFG_NO_PULL, MSM_GPIO_CFG_8MA), 0);
		msm_gpio_config(MSM_GPIO_CFG(MSM7200A_GPIO_I2C_DAT , 1, MSM_GPIO_CFG_OUTPUT,
				MSM_GPIO_CFG_NO_PULL, MSM_GPIO_CFG_8MA), 0);
	} else {
		msm_gpio_config(MSM_GPIO_CFG(MSM7200A_GPIO_I2C_CLK, 1, MSM_GPIO_CFG_INPUT,
				MSM_GPIO_CFG_NO_PULL, MSM_GPIO_CFG_2MA), 0);
		msm_gpio_config(MSM_GPIO_CFG(MSM7200A_GPIO_I2C_DAT, 1, MSM_GPIO_CFG_INPUT,
				MSM_GPIO_CFG_NO_PULL, MSM_GPIO_CFG_2MA), 0);
	}
}

static struct msm_i2c_pdata i2c_pdata = {
	.clk_nr	= I2C_CLK,
	.irq_nr = INT_PWB_I2C,
	.scl_gpio = MSM7200A_GPIO_I2C_CLK,
	.sda_gpio = MSM7200A_GPIO_I2C_DAT,
	.set_mux_to_i2c = i2c_set_mux,
	.i2c_base = (void*)MSM_I2C_BASE,
};

static struct machine_map {
	char* name;
	struct msm7k_board *board;
	enum mtype mtype;
} machines[] = {
	{
		.name = "KOVS100",
		.mtype = MACH_TYPE_HTCKOVSKY,
		.board = &htckovsky_board,
	},
	{
		.name = "KOVS110",
		.mtype = MACH_TYPE_HTCKOVSKY,
		.board = &htckovsky_board,
	},
	{
		.name = "RHOD400",
		.mtype = MACH_TYPE_HTCRHODIUM,
		.board = &htcrhodium_board,
	},
	{
		.name = "RHOD210",
		.mtype = MACH_TYPE_HTCRHODIUM,
		.board = &htcrhodium_board,
	}
};

int strcmp_one_byte_vs_two(const char *onebyte, const char* twobyte) {
	if (!onebyte && !twobyte)
		return 0;

	if (!onebyte)
		return -1;

	if (!twobyte)
		return 1;

	int len = strlen(onebyte);
	for (int i = 0; (i < len) && twobyte[i << 1]; i++) {
		if (onebyte[i] != twobyte[i << 1])
			return onebyte[i] - twobyte[i << 1];
	}
	return 0;
}

static bool probe_board(char* address, bool double_byte) {
	for (unsigned i = 0; i < ARRAY_SIZE(machines); i++) {
		if (double_byte) {
			if (!strcmp_one_byte_vs_two(machines[i].name, address)) {
				board = machines[i].board;
				mtype = machines[i].mtype;
				return true;
			}
		}
		else {
			if (!strcmp(address, machines[i].name)) {
				board = machines[i].board;
				mtype = machines[i].mtype;
				return true;
			}
		}
	}
	return false;
}

static void setup_board(void) {
	if (probe_board((char*)0xdf08c, false))
		return;

	if (probe_board((char*)0x81068, true))
		return;

	if (probe_board((char*)0xda000, true))
		return;

	//TODO: add real detection and fallback
	mtype = MACH_TYPE_HTCKOVSKY,
	board = &htckovsky_board;
}

/******************************************************************************
 * Boot Status
 *****************************************************************************/
enum boot_markers {
	MARK_BUTTON = 0x42555454,
	MARK_CHARGER = 0x43515247,
	MARK_BATTERY = 0x4e424234,
	MARK_OFF = 0x4f465352,
	MARK_RESET = 0x52455354,
	MARK_SOFTRESET = 0x53525354,
	MARK_FASTBOOT = 0x46414254,
	MARK_RECOVERY = 0x52435652,
	MARK_LK_TAG = 0x504f4f50,
};

#define LK_BOOTREASON_ADDR 0x8dfff0
#define SPL_BOOT_REASON_ADDR 0x8105c

enum boot_reason get_boot_reason() {
	unsigned bootcode = readl(SPL_BOOT_REASON_ADDR);
	enum boot_reason ret = BOOT_UNKNOWN;
	switch (bootcode) {
		case MARK_BUTTON:
			ret = BOOT_COLD;
			dprintf(INFO, "Boot reason: power button\n");
			break;
		case MARK_CHARGER:
			ret = BOOT_CHARGING;
			dprintf(INFO, "Boot reason: power up for charging\n");
			break;
		case MARK_BATTERY:
			ret = BOOT_BATTERY;
			dprintf(INFO, "Boot reason: no battery before\n");
			break;
		case MARK_OFF:
			ret = BOOT_UNEXPECTED;
			dprintf(INFO, "Boot reason: off soft reset\n");
			break;
		case MARK_RESET:
			ret = BOOT_WARM;
			dprintf(INFO, "Boot reason: reset\n");
			break;
		case MARK_SOFTRESET:
			ret = BOOT_WARM;
			dprintf(INFO, "Boot reason: soft reset\n");
			break;

		default:
			dprintf(INFO, "Boot reason unknown %08x\n", bootcode);
	}

	unsigned sign = readl(LK_BOOTREASON_ADDR);
	unsigned xsign = readl(LK_BOOTREASON_ADDR + 4);
	if ((sign ^ xsign) == MARK_LK_TAG) {
		switch (sign) {
			case MARK_FASTBOOT:
			dprintf(INFO, "Found fastboot marker\n");
			return BOOT_FASTBOOT;

			case MARK_RECOVERY:
			dprintf(INFO, "Found recovery marker\n");
			return BOOT_RECOVERY;
		}
	}
	return ret;
}

void target_shutdown(void) {
	enter_critical_section();
	platform_exit();
	dex_power_off();
}

void reboot_device(enum boot_reason reboot_mode)
{
	enter_critical_section();
	switch (reboot_mode) {
		case BOOT_FASTBOOT:
			writel(MARK_FASTBOOT, LK_BOOTREASON_ADDR);
			writel(MARK_FASTBOOT ^ MARK_LK_TAG, LK_BOOTREASON_ADDR + 4);
			break;

		case BOOT_RECOVERY:
			writel(MARK_RECOVERY, LK_BOOTREASON_ADDR);
			writel(MARK_RECOVERY ^ MARK_LK_TAG, LK_BOOTREASON_ADDR + 4);
			break;
		default:
			writel(0, LK_BOOTREASON_ADDR);
			writel(0, LK_BOOTREASON_ADDR + 4);
			break;
	}
	switch (reboot_mode) {
		case BOOT_CHARGING:
			writel(MARK_CHARGER, SPL_BOOT_REASON_ADDR);
			break;
		default:
			writel(MARK_SOFTRESET, SPL_BOOT_REASON_ADDR);
	}
	platform_exit();
	dex_reboot();
}

void target_init(void)
{
	setup_board();
	if (board && board->early_init)
		board->early_init();
	int ret = msm_dex_comm_init();
	dprintf(INFO, "DEX init with ret = %d\n", ret);
	acpu_clock_init();
	ret = msm_i2c_probe(&i2c_pdata);
	dprintf(INFO, "I2C init with ret = %d\n", ret);
	if (board && board->init)
		board->init();
}

static void msm_prepare_clocks(void) {
	static int clocks_off[] = {
		MDC_CLK,
		SDC1_CLK,
		SDC2_CLK,
		SDC3_CLK,
		SDC4_CLK,
		USB_HS_CLK,
	};

	static int clocks_on[] = {
		IMEM_CLK,
		MDP_CLK,
		PMDH_CLK,
	};

	for (int i = 0; i < (int)ARRAY_SIZE(clocks_off); i++)
		clk_disable(clocks_off[i]);

	for (int i = 0; i < (int)ARRAY_SIZE(clocks_on); i++)
		clk_enable(clocks_on[i]);
}

void target_exit(void) {
	if (board && board->exit)
		board->exit();
	msm_i2c_remove();
	msm_prepare_clocks();
	writel(0, LK_BOOTREASON_ADDR);
	writel(0, LK_BOOTREASON_ADDR + 4);
}

void* target_get_scratch_addr(void) {
	if (board && board->scratch_addr)
		return board->scratch_addr;

	return (void*)SCRATCH_ADDR;
}

unsigned target_get_scratch_size(void) {
	if (board && board->scratch_addr)
		return board->scratch_size;

	//least common value supported by msm72k htc boards
	return (98 << 20) - SCRATCH_ADDR;
}

char* target_get_cmdline(void) {
	if (board && board->cmdline)
		return board->cmdline;

	return "";
}

enum mtype target_machtype(void)
{
	return mtype;
}
