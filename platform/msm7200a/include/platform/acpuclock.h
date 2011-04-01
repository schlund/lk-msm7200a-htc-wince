/*
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM7K_PLATFORM_ACPUCLOCK_H_
#define __MSM7K_PLATFORM_ACPUCLOCK_H_

#include <platform/iomap.h>

#define TCXO_RATE			19200000
#define PLLn_BASE(n)	(MSM_CLK_CTL_BASE + 0x300 + 28 * (n))
#define PLLn_L_VAL(n)	(PLLn_BASE(n) + 4)
#define PLL_FREQ(l, m, n)	(TCXO_RATE * (l) + TCXO_RATE * (m) / (n))

#define A11S_CLK_CNTL_ADDR			(MSM_CSR_BASE + 0x100)
#define A11S_CLK_SEL_ADDR			(MSM_CSR_BASE + 0x104)
#define A11S_VDD_SVS_PLEVEL_ADDR	(MSM_CSR_BASE + 0x124)

enum pll {
	TCXO = -1,
	PLL0,
	PLL1,
	PLL2,
	PLL3,
	PLL_COUNT = PLL3 + 1,
};

enum pll_src {
	SRC_TCXO = 0,
	SRC_PLL_1 = 1,
	SRC_PLL_2 = 2,
	SRC_PLL_3 = 3,
	SRC_PLL_0 = 4,
};

extern void acpu_clock_init(void);
unsigned int pll_get_rate(enum pll, bool dump);

#endif //__MSM7K_PLATFORM_ACPUCLOCK_H_
