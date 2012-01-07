/* DEX (legacy version of pcom) interface for HTC MSM7200A wince PDAs.
 *
 * Copyright (C) 2011 htc-linux.org
 * Original implementation for linux kernel is copyright (C) maejrep
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Based on proc_comm.c by Brian Swetland
 */

#include <debug.h>
#include <reg.h>
#include <string.h>
#include <dev/gpio.h>
#include <kernel/mutex.h>
#include <platform/iomap.h>
#include <platform/msm_gpio.h>
#include <platform/timer.h>
#include <target/dex_comm.h>

#if defined(DEX_DEBUG) && DEX_DEBUG
	#define DDEX(fmt, arg...) dprintf(INFO, "[DEX] %s: " fmt "\n", __FUNCTION__, ## arg)
#else
	#define DDEX(fmt, arg...) do {} while (0)
#endif

#define EDEX(fmt, arg...) dprintf(ERROR, "[DEX] %s: " fmt "\n", __FUNCTION__, ## arg)

#define MSM_A2M_INT(n)	(MSM_CSR_BASE + 0x400 + (n) * 4)
#define DEX_COMMAND		0x00
#define DEX_STATUS		0x04
#define DEX_SERIAL		0x08
#define DEX_SERIAL_CHECK	0x0C
#define DEX_DATA		0x20
#define DEX_DATA_RESULT		0x24

#define DEX_TIMEOUT (10000000) /* 10s in microseconds */

#define DEX_INIT_DONE (MSM_SHARED_BASE + 0xfc13c)

static mutex_t dex_mutex;

static inline void notify_other_dex_comm(void)
{
	writel(1, MSM_A2M_INT(6));
}

static void wait_dex_ready(void) {
	int status = 0;
	
	if (readl(DEX_INIT_DONE)) {
		return;
	}

	while (!(status = readl(DEX_INIT_DONE))) {
		dprintf(INFO, "[DEX] waiting for ARM9... state = %d\n", status);
		mdelay(500);
	}
	dprintf(INFO, "%s: WinCE DEX initialized.\n", __func__);
}

static void init_dex_locked(void) {
	unsigned base = (unsigned)(MSM_SHARED_BASE + 0xfc100);

	if (readl(DEX_INIT_DONE)) {
		return;
	}

	writel(0, base + DEX_DATA);
	writel(0, base + DEX_DATA_RESULT);
	writel(0, base + DEX_SERIAL);
	writel(0, base + DEX_SERIAL_CHECK);
	writel(0, base + DEX_STATUS);
}

int msm_dex_comm(struct msm_dex_command * in, unsigned *out)
{
	unsigned base = (unsigned)(MSM_SHARED_BASE + 0xfc100);
	unsigned dex_timeout;
	unsigned status;
	unsigned num;
	unsigned base_cmd, base_status;

	mutex_acquire(&dex_mutex);

	wait_dex_ready();

	DDEX("waiting for modem; command=0x%02x data=0x%x", in->cmd, in->data);

	// Store original cmd byte
	base_cmd = in->cmd & 0xff;

	// Write only lowest byte
	writeb(base_cmd, base + DEX_COMMAND);

	// If we have data to pass, add 0x100 bit and store the data
	if ( in->has_data )
	{
		writel(readl(base + DEX_COMMAND) | DEX_HAS_DATA, base + DEX_COMMAND);
		writel(in->data, base + DEX_DATA);
	} else {
		writel(readl(base + DEX_COMMAND) & ~DEX_HAS_DATA, base + DEX_COMMAND);
		writel(0, base + DEX_DATA);
	}

	// Increment last serial counter
	num = readl(base + DEX_SERIAL) + 1;
	writel(num, base + DEX_SERIAL);

	DDEX("command and data sent (cntr=0x%x) ...", num);

	// Notify ARM9 with int6
	notify_other_dex_comm();

	// Wait for response... XXX: check irq stat?
	dex_timeout = DEX_TIMEOUT;
	while ( --dex_timeout && readl(base + DEX_SERIAL_CHECK) != num )
		udelay(1);

	if ( ! dex_timeout )
	{
		EDEX("%s: DEX cmd timed out. status=0x%x, A2Mcntr=%x, M2Acntr=%x\n",
			__func__, readl(base + DEX_STATUS), num, readl(base + DEX_SERIAL_CHECK));
		goto end;
	}

	DDEX("command result status = 0x%08x", readl(base + DEX_STATUS));

	// Read status of command
	status = readl(base + DEX_STATUS);
	writeb(0, base + DEX_STATUS);
	base_status = status & 0xff;
	DDEX("status new = 0x%x; status base = 0x%x",
		readl(base + DEX_STATUS), base_status);


	if ( base_status == base_cmd )
	{
		if ( status & DEX_STATUS_FAIL )
		{
			EDEX("DEX cmd failed; status=%x, result=%x",
				readl(base + DEX_STATUS),
				readl(base + DEX_DATA_RESULT));

			writel(readl(base + DEX_STATUS) & ~DEX_STATUS_FAIL, base + DEX_STATUS);
		}
		else if ( status & DEX_HAS_DATA )
		{
			writel(readl(base + DEX_STATUS) & ~DEX_HAS_DATA, base + DEX_STATUS);
			if (out)
				*out = readl(base + DEX_DATA_RESULT);
			DDEX("DEX output data = 0x%x",
				readl(base + DEX_DATA_RESULT));
		}
	} else {
		dprintf(CRITICAL "%s: DEX Code not match! a2m[0x%x], m2a[0x%x], a2m_num[0x%x], m2a_num[0x%x]\n",
			__func__, base_cmd, base_status, num, readl(base + DEX_SERIAL_CHECK));
	}

end:
	writel(0, base + DEX_DATA_RESULT);
	writel(0, base + DEX_STATUS);

	mutex_release(&dex_mutex);
	return 0;
}

int msm_dex_comm_init()
{
	mutex_init(&dex_mutex);
	mutex_acquire(&dex_mutex);

	init_dex_locked();
	wait_dex_ready();

	mutex_release(&dex_mutex);
	return 0;
}

void dex_set_vibrate(uint32_t level)
{
	struct msm_dex_command vibra;

	if (level == 0) {
		vibra.cmd = DEX_VIBRA_OFF;
		msm_dex_comm(&vibra, 0);
	} else if (level > 0) {
		if (level == 1 || level > 0xb22)
			level = 0xb22;
		writel(level, MSM_SHARED_BASE + 0xfc130);
		vibra.cmd = DEX_VIBRA_ON;
		msm_dex_comm(&vibra, 0);
	}
}

bool dex_get_vbus_state(void) {
	return !!readl(MSM_SHARED_BASE + 0xfc00c);
}

static bool halt_done = false;

void dex_power_off(void)
{
	unsigned num;
	if (halt_done) {
		dprintf(CRITICAL, "%s: poweroff already done\n", __func__);
		return;
	}
	enter_critical_section();
	halt_done = true;
	/* 1. dex 0x14 without interupts (disable irq + write 0x14 to smem)*/
	writeb(DEX_POWER_OFF, (unsigned)(MSM_SHARED_BASE + 0xfc100));
	/* 2. dex counter ++ */
	num = readl((unsigned)(MSM_SHARED_BASE + 0xfc108)) + 1;
	writel(num, (unsigned)(MSM_SHARED_BASE + 0xfc108));
	/* 3.     A2M = -1 */
	writel(-1, MSM_A2M_INT(6));
	/* 4. sleep 5s */
	mdelay(500);
	/* 5. set smem sign 0x55AA00FF */
	writel(0x55AA00FF,(unsigned)(MSM_SHARED_BASE + 0xfc08C));
	mdelay(500);
	/* 6. gpio reset */
	writel(readl(MSM_GPIOCFG2_BASE + 0x504) | (1 << 9), MSM_GPIOCFG2_BASE + 0x504);
	mdelay(50);
	gpio_set(25,0);
	exit_critical_section();

	for (;;);
}

void dex_reboot(void)
{
	if (halt_done) {
		dprintf(CRITICAL, "%s: reset already done\n", __func__);
		return;
	}
	enter_critical_section();
	halt_done = true;
	exit_critical_section();

	struct msm_dex_command dex = {.cmd = DEX_NOTIFY_ARM9_REBOOT };
	msm_dex_comm(&dex, 0);
	mdelay(350);
	msm_gpio_set_owner(25, MSM_GPIO_OWNER_ARM11);
	gpio_set(25, 0);
	dprintf(INFO "%s: Soft reset done.\n", __func__);
}
