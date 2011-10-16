/*
 * Copyright (c) 2008, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <debug.h>
#include <reg.h>

#include <platform/iomap.h>
#include <platform/clock.h>
#include <platform/pcom.h>

#define MSM_A2M_INT(n) (MSM_CSR_BASE + 0x400 + (n) * 4)
static inline void notify_other_proc_comm(void)
{
	writel(1, MSM_A2M_INT(6));
}

#define APP_COMMAND (MSM_SHARED_BASE + 0x00)
#define APP_STATUS  (MSM_SHARED_BASE + 0x04)
#define APP_DATA1   (MSM_SHARED_BASE + 0x08)
#define APP_DATA2   (MSM_SHARED_BASE + 0x0C)

#define MDM_COMMAND (MSM_SHARED_BASE + 0x10)
#define MDM_STATUS  (MSM_SHARED_BASE + 0x14)
#define MDM_DATA1   (MSM_SHARED_BASE + 0x18)
#define MDM_DATA2   (MSM_SHARED_BASE + 0x1C)

int msm_proc_comm(unsigned cmd, unsigned *data1, unsigned *data2)
{
	int ret = -1;
	unsigned status;

	while (readl(MDM_STATUS) != PCOM_READY) {
		/* XXX check for A9 reset */
	}

	writel(cmd, APP_COMMAND);
	writel(data1 ? *data1 : 0, APP_DATA1);
	writel(data2 ? *data2 : 0, APP_DATA2);

	notify_other_proc_comm();
	while (readl(APP_COMMAND) != PCOM_CMD_DONE) {
		/* XXX check for A9 reset */
	}

	status = readl(APP_STATUS);

	if (status != PCOM_CMD_FAIL) {
		if (data1)
			*data1 = readl(APP_DATA1);
		if (data2)
			*data2 = readl(APP_DATA2);
		ret = 0;
	}
	else {
		dprintf(INFO, "[PCOM]: FAIL [%x %x %x]\n", cmd, data1, data2);
	}

	return ret;
}

int pcom_clock_enable(unsigned id)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_ENABLE, &id, 0);
}

int pcom_clock_disable(unsigned id)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_DISABLE, &id, 0);
}

int pcom_clock_set_rate(unsigned id, unsigned rate)
{
	return msm_proc_comm(PCOM_CLKCTL_RPC_SET_RATE, &id, &rate);
}

int pcom_clock_get_rate(unsigned id)
{
	if (msm_proc_comm(PCOM_CLKCTL_RPC_RATE, &id, 0)) {
		return -1;
	} else {
		return (int)id;
	}
}

void pcom_reboot(unsigned reboot_reason)
{
	msm_proc_comm(PCOM_RESET_CHIP, &reboot_reason, 0);
	for (;;) ;
}

int pcom_gpio_tlmm_config(unsigned config, unsigned disable)
{
	return msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &config, &disable);
}

int pcom_vreg_set_level(unsigned id, unsigned mv)
{
	return msm_proc_comm(PCOM_VREG_SET_LEVEL, &id, &mv);
}

int pcom_vreg_enable(unsigned id)
{
	int enable = 1;
	return msm_proc_comm(PCOM_VREG_SWITCH, &id, &enable);

}

int pcom_vreg_disable(unsigned id)
{
	int enable = 0;
	return msm_proc_comm(PCOM_VREG_SWITCH, &id, &enable);
}
