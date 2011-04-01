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
#include <platform/acpuclock.h>
#include <reg.h>

static mutex_t clk_mutex;

struct mdns_clock_params {
	unsigned long freq;
	unsigned calc_freq;
	unsigned md;
	unsigned ns;
	unsigned pll_freq;
	unsigned clk_id;
};

struct msm_clock_params {
	unsigned idx;
	unsigned offset;
	int setup_mdns;
	char *name;
};

#define MSM_CLOCK_REG(frequency,M,N,D,PRE,a5,SRC,MNE,pll_frequency) { \
	.freq = (frequency), \
	.md = ((0xffff & (M)) << 16) | (0xffff & ~((D) << 1)), \
	.ns = ((0xffff & ~((N) - (M))) << 16) \
	    | ((0xff & (0xa | (MNE))) << 8) \
	    | ((0x7 & (a5)) << 5) \
	    | ((0x3 & (PRE)) << 3) \
	    | (0x7 & (SRC)), \
	.pll_freq = (pll_frequency), \
	.calc_freq = (pll_frequency*M/((PRE+1)*N)), \
}

static struct msm_clock_params msm_clock_parameters[NR_CLKS] = {
	[ADSP_CLK] = {.offset = 0x34,.name = "ADSP_CLK",},
	[GP_CLK] = {.offset = 0x5c,.name = "GP_CLK",},
	[I2C_CLK] = {.offset = 0x68,.setup_mdns = 1,.name = "I2C_CLK",},
	[MDP_CLK] = { .offset = 0x8c, .name = "MDP_CLK",},
	[PMDH_CLK] = {.offset = 0x8c,.setup_mdns = 0,.name = "PMDH_CLK",},
	[SDAC_CLK] = {.offset = 0x9c,.name = "SDAC_CLK",},
	[SDC1_CLK] = {.offset = 0xa4,.setup_mdns = 1,.name = "SDC1_CLK",},
	[SDC2_CLK] = {.offset = 0xac,.setup_mdns = 1,.name = "SDC2_CLK",},
	[SDC3_CLK] = {.offset = 0xb4,.setup_mdns = 1,.name = "SDC3_CLK",},
	[SDC4_CLK] = {.offset = 0xbc,.setup_mdns = 1,.name = "SDC4_CLK",},
	[SDC1_PCLK] = {.idx = 7,.name = "SDC1_PCLK",},
	[SDC2_PCLK] = {.idx = 8,.name = "SDC2_PCLK",},
	[SDC3_PCLK] = {.idx = 27,.name = "SDC3_PCLK",},
	[SDC4_PCLK] = {.idx = 28,.name = "SDC4_PCLK",},
	[UART1_CLK] = {.offset = 0xe0,.name = "UART1_CLK",},
	[UART2_CLK] = {.offset = 0xe0,.name = "UART2_CLK",},
	[UART3_CLK] = {.offset = 0xe0,.name = "UART3_CLK",},
	[UART1DM_CLK] = {.idx = 17,.offset = 0xd4,.setup_mdns = 1,.name =
			 "UART1DM_CLK",},
	[UART2DM_CLK] = {.idx = 26,.offset = 0xdc,.setup_mdns = 1,.name =
			 "UART2DM_CLK",},
	[USB_HS_CLK] = {.offset = 0x2c0,.name = "USB_HS_CLK",},
	[USB_HS_PCLK] = {.idx = 25,.name = "USB_HS_PCLK",},
};

static struct mdns_clock_params msm_clock_freq_parameters_pll0_245[] = {

	MSM_CLOCK_REG(144000, 3, 0x64, 0x32, 3, 3, 0, 1, 19200000),	/* SD, 144kHz */
	MSM_CLOCK_REG(7372800, 3, 0x64, 0x32, 0, 2, 4, 1, 245760000),	/*  460800*16, will be divided by 4 for 115200 */
	MSM_CLOCK_REG(12000000, 1, 0x20, 0x10, 1, 3, 1, 1, 768000000),	/* SD, 12MHz */
	MSM_CLOCK_REG(14745600, 3, 0x32, 0x19, 0, 2, 4, 1, 245760000),	/* BT, 921600 (*16) */
	MSM_CLOCK_REG(19200000, 1, 0x0a, 0x05, 3, 3, 1, 1, 768000000),	/* SD, 19.2MHz */
	MSM_CLOCK_REG(24000000, 1, 0x8, 0x04, 3, 2, 1, 1, 768000000),	/* SD, 24MHz */
	MSM_CLOCK_REG(32000000, 1, 0x0c, 0x06, 1, 3, 1, 1, 768000000),	/* SD, 32MHz */
	MSM_CLOCK_REG(58982400, 6, 0x19, 0x0c, 0, 2, 4, 1, 245760000),	/* BT, 3686400 (*16) */
//      MSM_CLOCK_REG(64000000, 0x19, 0x60, 0x30, 0, 2, 4, 1, 245760000),       /* BT, 4000000 (*16) */
	{0, 0, 0, 0, 0, 0},
};

// CDMA phones typically use a 196 MHz PLL0
static struct mdns_clock_params msm_clock_freq_parameters_pll0_196[] = {

	MSM_CLOCK_REG(144000, 3, 0x64, 0x32, 3, 3, 0, 1, 19200000),	/* SD, 144kHz */
	MSM_CLOCK_REG(7372800, 3, 0x50, 0x28, 0, 2, 4, 1, 196608000),	/*  460800*16, will be divided by 4 for 115200 */
	MSM_CLOCK_REG(12000000, 1, 0x20, 0x10, 1, 3, 1, 1, 768000000),	/* SD, 12MHz */
	MSM_CLOCK_REG(14745600, 3, 0x28, 0x14, 0, 2, 4, 1, 196608000),	/* BT, 921600 (*16) */
	MSM_CLOCK_REG(19200000, 1, 0x0a, 0x05, 3, 3, 1, 1, 768000000),	/* SD, 19.2MHz */
	MSM_CLOCK_REG(24000000, 1, 0x10, 0x08, 1, 3, 1, 1, 768000000),	/* SD, 24MHz */
	MSM_CLOCK_REG(32000000, 1, 0x0c, 0x06, 1, 3, 1, 1, 768000000),	/* SD, 32MHz */
	MSM_CLOCK_REG(58982400, 3, 0x0a, 0x05, 0, 2, 4, 1, 196608000),	/* BT, 3686400 (*16) */
//      MSM_CLOCK_REG(64000000, 0x7d, 0x180, 0xC0, 0, 2, 4, 1, 196608000),      /* BT, 4000000 (*16) */
	{0, 0, 0, 0, 0, 0},
};

static struct mdns_clock_params *msm_clock_freq_parameters = NULL;

static inline struct msm_clock_params msm_clk_get_params(unsigned id)
{
	struct msm_clock_params empty;
	if (id < NR_CLKS)
		return msm_clock_parameters[id];
	return empty;
}

static inline unsigned msm_clk_enable_bit(unsigned id)
{
	struct msm_clock_params params;
	params = msm_clk_get_params(id);
	if (!params.idx)
		return 0;
	return 1U << params.idx;
}

static int msm_clk_is_enabled(unsigned id)
{
	int is_enabled = 0;
	unsigned bit;
	bit = msm_clk_enable_bit(id);
	if (bit > 0) {
		is_enabled = (readl(MSM_CLK_CTL_BASE) & bit) != 0;
	}
	return is_enabled;
}

static int msm_clk_enable(unsigned id)
{
	struct msm_clock_params params;
	int done = 0;

	params = msm_clk_get_params(id);
	switch (id) {
	case MDP_CLK:
		writel((readl(MSM_CLK_CTL_BASE) & 0x3ffffdff) | 0x200,
		       MSM_CLK_CTL_BASE);
		done = 1;
		break;

	case PMDH_CLK:
		writel((readl(MSM_CLK_CTL_BASE + params.offset) & 0x67f) |
		       0x800, MSM_CLK_CTL_BASE + params.offset);
		writel((readl(MSM_CLK_CTL_BASE + params.offset) & 0xc7f) |
		       0x200, MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case I2C_CLK:
		writel((readl(MSM_CLK_CTL_BASE + params.offset) & 0x600) |
		       0x800, MSM_CLK_CTL_BASE + params.offset);
		writel((readl(MSM_CLK_CTL_BASE + params.offset) & 0xc00) |
		       0x200, MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case ADSP_CLK:
		writel((readl(MSM_CLK_CTL_BASE + params.offset) & 0x7ff7ff) |
		       0x800, MSM_CLK_CTL_BASE + params.offset);
		writel((readl(MSM_CLK_CTL_BASE + params.offset) & 0x6fffff) |
		       0x100000, MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case UART1_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) | 0x10,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) | 0x20,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case UART2_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) | 0x400,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) | 0x800,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case UART3_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) | 0x10000,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) | 0x20000,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case SDAC_CLK:
	case SDC1_CLK:
	case SDC2_CLK:
	case SDC3_CLK:
	case SDC4_CLK:
	case USB_HS_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) | 0x800,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) | 0x100,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) | 0x200,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case SDC1_PCLK:
	case SDC2_PCLK:
	case SDC3_PCLK:
	case SDC4_PCLK:
	case USB_HS_PCLK:
		done = 1;
		break;

	default:
		break;
	}

	if (params.idx) {
		writel(readl(MSM_CLK_CTL_BASE) | (1U << params.idx),
		       MSM_CLK_CTL_BASE);
		done = 1;
	}

	if (done)
		return 0;

	printf
	    ("%s: FIXME! enabling a clock that doesn't have an ena bit or ns-only offset: %u\n",
	     __func__, id);

	return 0;
}

static void msm_clk_disable(unsigned id)
{
	int done = 0;
	struct msm_clock_params params;
	params = msm_clk_get_params(id);

	switch (id) {
	case MDP_CLK:
		writel(readl(MSM_CLK_CTL_BASE) & 0x3ffffdff, MSM_CLK_CTL_BASE);
		done = 1;
		return;
		break;

	case UART1_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & ~0x20,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & ~0x10,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case UART2_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & ~0x800,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & ~0x400,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case UART3_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & ~0x20000,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & ~0x10000,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case SDAC_CLK:
	case SDC1_CLK:
	case SDC2_CLK:
	case SDC3_CLK:
	case SDC4_CLK:
	case USB_HS_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & ~0x200,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & ~0x800,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & ~0x100,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case I2C_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & 0xc00,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & 0x600,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case ADSP_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & 0x6fffff,
		       MSM_CLK_CTL_BASE + params.offset);
		if (readl(MSM_CLK_CTL_BASE + params.offset) & 0x280) {
			done = 1;
			break;
		}
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & 0x7ff7ff,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case PMDH_CLK:
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & 0xc7f,
		       MSM_CLK_CTL_BASE + params.offset);
		writel(readl(MSM_CLK_CTL_BASE + params.offset) & 0x67f,
		       MSM_CLK_CTL_BASE + params.offset);
		done = 1;
		break;

	case SDC1_PCLK:
	case SDC2_PCLK:
	case SDC3_PCLK:
	case SDC4_PCLK:
	case USB_HS_PCLK:
		done = 1;
		break;
	default:
		break;
	}

	if (done)
		return;

	printf
	    ("%s: FIXME! disabling a clock that doesn't have an ena bit: %u\n",
	     __func__, id);
	return;
}

static void msm_set_rate_simple_div_pll(unsigned id, unsigned freq) {
	if (!freq)
		return;
	int pll_freqs[PLL_COUNT];
	int divs[PLL_COUNT];
	enum pll pll_idx = TCXO;

	for (int i = 0; i < PLL_COUNT; i++) {
		pll_freqs[i] = pll_get_rate(i, false);
		divs[i] = pll_freqs[i] / freq;
		if (divs[i] == 0)
			divs[i] = 1;
	}
	unsigned freq0 = pll_freqs[pll_idx] / divs[pll_idx];
	unsigned delta = freq0 > freq ? freq0 - freq : freq - freq0;

	for (int i = 1; i < PLL_COUNT; i++) {
		unsigned res = pll_freqs[i] / divs[i];
		unsigned d = res > freq ? res - freq : freq - res;
		if (d < delta) {
			pll_idx = i;
			break;
		}
	}
	unsigned ns_val = 0;

	switch (pll_idx) {
		case TCXO:
				ns_val = ((divs[pll_idx] - 1) << 3) | SRC_TCXO;
		break;

		case PLL0:
				ns_val = ((divs[pll_idx] - 1) << 3) | SRC_PLL_0;
		break;

		case PLL1:
				ns_val = ((divs[pll_idx] - 1) << 3) | SRC_PLL_1;
		break;

		case PLL2:
				ns_val = ((divs[pll_idx] - 1) << 3) | SRC_PLL_2;
		break;

		case PLL3:
				ns_val = ((divs[pll_idx] - 1) << 3) | SRC_PLL_3;
		break;

		default:
		break;
	}
	unsigned addr = MSM_CLK_CTL_BASE + msm_clock_parameters[id].offset;
	ns_val = (0x7f & ns_val) | (readl(addr) & ~0x7f);
	writel(ns_val, addr);
}

static int msm_clk_set_freq(unsigned id, unsigned freq)
{
	int n = 0;
	unsigned offset;
	int found, enabled, needs_setup;
	struct msm_clock_params params;
	found = 0;

	params = msm_clk_get_params(id);
	offset = params.offset;
	needs_setup = params.setup_mdns;

	if (id == EBI1_CLK)
		return 0;

	if (!params.offset) {
		printf
		    ("%s: FIXME! Don't know how to set clock %u - no known Md/Ns reg\n",
		     __func__, id);
		return 0;
	}

	enabled = msm_clk_is_enabled(id);
	if (enabled)
		msm_clk_disable(id);

	switch (id) {
		case MDP_CLK:
		case PMDH_CLK:
		case EBI1_CLK:
		case EBI2_CLK:
		case ADSP_CLK:
		case EMDH_CLK:
		case GP_CLK:
		case GRP_CLK:
			msm_set_rate_simple_div_pll(id, freq);
			break;

		default:
		if (needs_setup) {
			while (msm_clock_freq_parameters[n].freq) {
				n++;
			}

			for (n--; n >= 0; n--) {
				if (freq >= msm_clock_freq_parameters[n].freq) {
					writel(msm_clock_freq_parameters[n].md,
						   MSM_CLK_CTL_BASE + offset - 4);
					if (id == VFE_CLK) {
						writel((msm_clock_freq_parameters[n].ns
							& ~0x6a06) |
							   (readl(MSM_CLK_CTL_BASE + offset)
							& 0x6a06),
							   MSM_CLK_CTL_BASE + offset);
					} else {
						writel(msm_clock_freq_parameters[n].ns,
							   MSM_CLK_CTL_BASE + offset);
					}
					found = 1;
					break;
				}
			}
		}
		break;
	}

	if (enabled)
		msm_clk_enable(id);

	if ((!found && needs_setup)) {
		printf("FIXME! could not "
		       "find suitable parameter for freq %lu\n", freq);
	}

	return 0;
}

int clk_enable(unsigned id)
{
	int rc;
	mutex_acquire(&clk_mutex);
	rc = msm_clk_enable(id);
	mutex_release(&clk_mutex);
	return rc;
}

void clk_disable(unsigned id)
{
	mutex_acquire(&clk_mutex);
	msm_clk_disable(id);
	mutex_release(&clk_mutex);
}

int clk_set_rate(unsigned id, unsigned freq)
{
	int rc;
	mutex_acquire(&clk_mutex);
	rc = msm_clk_set_freq(id, freq);
	mutex_release(&clk_mutex);
	return rc;
}

void msm_clock_init(void)
{
	mutex_init(&clk_mutex);
	mutex_acquire(&clk_mutex);
	if (pll_get_rate(PLL0, false) == 196608000) {
		// cdma pll0 = 196 MHz
		msm_clock_freq_parameters = msm_clock_freq_parameters_pll0_196;
	} else {
		// default gsm pll0 = 245 MHz
		msm_clock_freq_parameters = msm_clock_freq_parameters_pll0_245;
	}
	mutex_release(&clk_mutex);
}
