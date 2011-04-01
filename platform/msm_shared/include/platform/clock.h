#ifndef __MSM_SHARED_PLATFORM_CLOCK_H_
#define __MSM_SHARED_PLATFORM_CLOCK_H_

#define ACPU_CLK           0	/* Applications processor clock */
#define ADM_CLK            1	/* Applications data mover clock */
#define ADSP_CLK           2	/* ADSP clock */
#define EBI1_CLK           3	/* External bus interface 1 clock */
#define EBI2_CLK           4	/* External bus interface 2 clock */
#define ECODEC_CLK         5	/* External CODEC clock */
#define EMDH_CLK           6	/* External MDDI host clock */
#define GP_CLK             7	/* General purpose clock */
#define GRP_CLK            8	/* Graphics clock */
#define I2C_CLK            9	/* I2C clock */
#define ICODEC_RX_CLK     10	/* Internal CODEX RX clock */
#define ICODEC_TX_CLK     11	/* Internal CODEX TX clock */
#define IMEM_CLK          12	/* Internal graphics memory clock */
#define MDC_CLK           13	/* MDDI client clock */
#define MDP_CLK           14	/* Mobile display processor clock */
#define PBUS_CLK          15	/* Peripheral bus clock */
#define PCM_CLK           16	/* PCM clock */
#define PMDH_CLK          17	/* Primary MDDI host clock */
#define SDAC_CLK          18	/* Stereo DAC clock */
#define SDC1_CLK          19	/* Secure Digital Card clocks */
#define SDC1_PCLK         20
#define SDC2_CLK          21
#define SDC2_PCLK         22
#define SDC3_CLK          23
#define SDC3_PCLK         24
#define SDC4_CLK          25
#define SDC4_PCLK         26
#define TSIF_CLK          27	/* Transport Stream Interface clocks */
#define TSIF_REF_CLK      28
#define TV_DAC_CLK        29	/* TV clocks */
#define TV_ENC_CLK        30
#define UART1_CLK         31	/* UART clocks */
#define UART2_CLK         32
#define UART3_CLK         33
#define UART1DM_CLK       34
#define UART2DM_CLK       35
#define USB_HS_CLK        36	/* High speed USB core clock */
#define USB_HS_PCLK       37	/* High speed USB pbus clock */
#define USB_OTG_CLK       38	/* Full speed USB clock */
#define VDC_CLK           39	/* Video controller clock */
#define VFE_CLK           40	/* Camera / Video Front End clock */
#define VFE_MDC_CLK       41	/* VFE MDDI client clock */

#define NR_CLKS		  42

void msm_clock_init(void);
int clk_enable(unsigned id);
void clk_disable(unsigned id);
int clk_set_rate(unsigned id, unsigned freq);

#endif //__MSM_SHARED_PLATFORM_CLOCK_H_
