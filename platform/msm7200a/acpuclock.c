/* acpuclock.c - controls and prints CPU and PLL frequencies
 *
 * Copyright (C) 2011 Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * Copyright (C) htc-linux.org and XDANDROID project
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
#include <debug.h>
#include <platform/timer.h>
#include <platform/acpuclock.h>

//TODO: add setting run mode speed and power saving for charging

unsigned int pll_get_rate(enum pll pll, bool dump)
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

	if (dump)
		dprintf(INFO, "PLL%d: MODE=%08x L=%08x M=%08x N=%08x freq=%u Hz (%u MHz)\n",
				pll, mode, L, M, N, freq, freq / 1000000);

	return freq;
}

static void acpuclk_set_vdd_level(unsigned vdd)
{
	uint32_t current_vdd;

	current_vdd = readl(A11S_VDD_SVS_PLEVEL_ADDR) & 0x07;

	dprintf(INFO, "[ACPUCLOCK]: Switching VDD from %u -> %d\n",
	       current_vdd, vdd);

	writel((1 << 7) | (vdd << 3), A11S_VDD_SVS_PLEVEL_ADDR);
	udelay(62);

	if ((readl(A11S_VDD_SVS_PLEVEL_ADDR) & 0x7) != vdd) {
		dprintf(INFO, "[ACPUCLOCK]: VDD set failed\n");
		return;
	}
}

void acpu_clock_init(void)
{
	unsigned long pll0_l, pll1_l, pll2_l;
	do {
		pll0_l = readl(PLLn_L_VAL(PLL0)) & 0x3f;
		udelay(50);
	} while (pll0_l == 0);
	do {
		pll1_l = readl(PLLn_L_VAL(PLL1)) & 0x3f;
		udelay(50);
	} while (pll1_l == 0);
	do {
		pll2_l = readl(PLLn_L_VAL(PLL2)) & 0x3f;
		udelay(50);
	} while (pll2_l == 0);

	for (int i = PLL0; i < PLL_COUNT; i++) {
		pll_get_rate(i, true);
	}
	acpuclk_set_vdd_level(7);
}
