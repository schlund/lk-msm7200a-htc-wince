/* arch/arm/mach-msm/vreg.c
 *
 * Port to DEX interface (C) 2007-2011 htc-linux.org
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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

#include <reg.h>
#include <err.h>
#include <debug.h>
#include <array.h>
#include <target/dex_vreg.h>
#include <target/dex_comm.h>
#include <platform/iomap.h>

struct vreg {
	unsigned id;
	int status;
	unsigned refcnt;
};

static struct vreg vregs[32] = {
};

int dex_vreg_enable(unsigned id)
{
	if (id > ARRAY_SIZE(vregs)) {
		printf("[VREG] %s: incorrect index %u\n", __func__, id);
		return ERR_INVALID_ARGS;
	}
	struct vreg* vreg = vregs + id;

	struct msm_dex_command dex = {
		.cmd = DEX_PMIC_REG_ON,
		.has_data = 1,
		.data = 1U << vreg->id,
	};
	
	if (vreg->refcnt == 0)
	vreg->status = msm_dex_comm(&dex, 0);

	if ((vreg->refcnt < UINT_MAX) && (!vreg->status))
		vreg->refcnt++;

	return vreg->status;
}

int dex_vreg_disable(unsigned id)
{
	if (id > ARRAY_SIZE(vregs)) {
		printf("[VREG] %s: incorrect index %u\n", __func__, id);
		return ERR_INVALID_ARGS;
	}

	struct vreg* vreg = vregs + id;

	if (!vreg->refcnt)
		return 0;

	struct msm_dex_command dex = {
		.cmd = DEX_PMIC_REG_OFF,
		.has_data = 1,
		.data = 1U << vreg->id,
	};

	if (vreg->refcnt == 1)
	vreg->status = msm_dex_comm(&dex, 0);

	if (!vreg->status)
		vreg->refcnt--;

	return vreg->status;
}

int dex_vreg_set_level(unsigned id, unsigned mv)
{
	if (id > ARRAY_SIZE(vregs)) {
		printf("[VREG] %s: incorrect index %u\n", __func__, id);
		return ERR_INVALID_ARGS;
	}
	struct vreg* vreg = vregs + id;

	struct msm_dex_command dex = {
		.cmd = DEX_PMIC_REG_VOLTAGE,
		.has_data = 1,
		.data = (1U << vreg->id)
	};
	// This reg appears to only be used by dex_vreg_set_level()
	writel(mv, MSM_SHARED_BASE + 0xfc130);
	vreg->status = msm_dex_comm(&dex, 0);
	return vreg->status;
}

