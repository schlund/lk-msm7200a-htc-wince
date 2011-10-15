/*
 * init.c - contains common code for msm7227 htc phones
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
#include <target/htcphoton.h>
#include <target/dex_comm.h>
#include <platform/acpuclock.h>
#include <platform/clock.h>
#include <platform/irqs.h>
#include <platform/iomap.h>
#include <platform/pcom.h>
#include <platform/msm_i2c.h>
#include <platform/timer.h>

static enum mtype mtype = 0;
static struct msm7k_board *board = NULL;

#define MSM7227_GPIO_I2C_CLK 60
#define MSM7227_GPIO_I2C_DAT 61
static void i2c_set_mux(int to_i2c) {
	if (to_i2c) {
		pcom_gpio_tlmm_config(PCOM_GPIO_CFG(MSM7227_GPIO_I2C_CLK, 1, PCOM_GPIO_CFG_OUTPUT,
				PCOM_GPIO_CFG_NO_PULL, PCOM_GPIO_CFG_8MA), 0);
		pcom_gpio_tlmm_config(PCOM_GPIO_CFG(MSM7227_GPIO_I2C_DAT , 1, PCOM_GPIO_CFG_OUTPUT,
				PCOM_GPIO_CFG_NO_PULL, PCOM_GPIO_CFG_8MA), 0);
	} else {
		pcom_gpio_tlmm_config(PCOM_GPIO_CFG(MSM7227_GPIO_I2C_CLK, 1, PCOM_GPIO_CFG_INPUT,
				PCOM_GPIO_CFG_NO_PULL, PCOM_GPIO_CFG_2MA), 0);
		pcom_gpio_tlmm_config(PCOM_GPIO_CFG(MSM7227_GPIO_I2C_DAT, 1, PCOM_GPIO_CFG_INPUT,
				PCOM_GPIO_CFG_NO_PULL, PCOM_GPIO_CFG_2MA), 0);
	}
}

static struct msm_i2c_pdata i2c_pdata = {
	.clk_nr	= I2C_CLK,
	.irq_nr = INT_PWB_I2C,
	.scl_gpio = MSM7227_GPIO_I2C_CLK,
	.sda_gpio = MSM7227_GPIO_I2C_DAT,
	.set_mux_to_i2c = i2c_set_mux,
	.i2c_base = (void*)MSM_I2C_BASE,
};

static void setup_board(void) {
	//TODO: add real detection if needed
	mtype = MACH_TYPE_HTCPHOTON,
	board = &htcphoton_board;
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
	dprintf(INFO, "%s\n", __func__);
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
