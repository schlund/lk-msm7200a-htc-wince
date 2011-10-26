/* linux/arch/arm/mach-msm/board-htckovsky.c
 *
 * Copyright (C) 2011 Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * Copyright (C) 2007-2011 htc-linux.org
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

#include <stdio.h>
#include <kernel/mutex.h>
#include <platform/clock.h>
#include <platform/iomap.h>
#include <platform/timer.h>
#include <platform/pcom.h>
#include <reg.h>

static mutex_t clk_mutex;

#define TCXO_RATE			19200000
#define PLLn_BASE(n)	(MSM_CLK_CTL_BASE + 0x300 + 28 * (n))
#define PLLn_L_VAL(n)	(PLLn_BASE(n) + 4)
#define PLL_FREQ(l, m, n)	(TCXO_RATE * (l) + TCXO_RATE * (m) / (n))

enum pll {
	TCXO = -1,
	PLL0,
	PLL1,
	PLL2,
	PLL3,
	PLL_COUNT = PLL3 + 1,
};

static unsigned int pll_get_rate(enum pll pll)
{
	unsigned int mode, L, M, N, freq;

	if (pll == TCXO)
		return TCXO_RATE;
	if (pll >= PLL_COUNT)
		return 0;
	else {
		mode = readl(PLLn_BASE(pll) + 0x0);
		L = readl(PLLn_BASE(pll) + 0x4);
		M = readl(PLLn_BASE(pll) + 0x8);
		N = readl(PLLn_BASE(pll) + 0xc);
		freq = PLL_FREQ(L, M, N);
	}

	dprintf(INFO, "PLL%d: MODE=%08x L=%08x M=%08x N=%08x freq=%u Hz (%u MHz)\n",
		pll, mode, L, M, N, freq, freq / 1000000);

	return freq;
}

int clk_enable(unsigned id)
{
	int rc;
	mutex_acquire(&clk_mutex);
	rc = pcom_clock_enable(id);
	mutex_release(&clk_mutex);
	return rc;
}

void clk_disable(unsigned id)
{
	mutex_acquire(&clk_mutex);
	pcom_clock_disable(id);	
	mutex_release(&clk_mutex);
}

int clk_set_rate(unsigned id, unsigned freq)
{
	int rc;
	mutex_acquire(&clk_mutex);
	rc = pcom_clock_set_rate(id, freq);
	mutex_release(&clk_mutex);
	return rc;
}

void msm_clock_init(void)
{
	mutex_init(&clk_mutex);
	mutex_acquire(&clk_mutex);
	mutex_release(&clk_mutex);
}
