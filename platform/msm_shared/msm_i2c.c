/* msm-i2c.c
 *
 * LK port: Copyright (C) 2011 Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * Copyright (C) 2007 Google, Inc.
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
 */

#include <err.h>
#include <debug.h>
#include <reg.h>
#include <dev/gpio.h>
#include <kernel/thread.h>
#include <platform/msm_i2c.h>
#include <platform/interrupts.h>
#include <platform/timer.h>
#include <platform/iomap.h>
#include <platform/clock.h>


#define DEBUG_I2C 0
#if DEBUG_I2C
	#define I2C_DBG(lvl, fmt, x...) dprintf(lvl, fmt, ##x)
#else
	#define I2C_DBG(lvl, fmt, x...) do {} while (0)
#endif

#define I2C_ERR(fmt, x...) dprintf(ALWAYS, fmt, ##x)


#define I2C_DBG_FUNC_LINE() I2C_DBG("%s: %d\n", __func__, __LINE__)


#define I2C_M_TEN		0x0010	/* this is a ten bit chip address */
#define I2C_M_RD		0x0001	/* read data, from slave to master */
#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */

struct i2c_msg {
	uint16_t addr;	/* slave address			*/
	uint16_t flags;
	uint16_t len;		/* msg length				*/
	uint8_t *buf;		/* pointer to msg data			*/
};

enum {
	I2C_WRITE_DATA = 0x00,
	I2C_CLK_CTL = 0x04,
	I2C_STATUS = 0x08,
	I2C_READ_DATA = 0x0c,
	I2C_INTERFACE_SELECT = 0x10,

	I2C_WRITE_DATA_DATA_BYTE = 0xff,
	I2C_WRITE_DATA_ADDR_BYTE = 1U << 8,
	I2C_WRITE_DATA_LAST_BYTE = 1U << 9,

	I2C_CLK_CTL_FS_DIVIDER_VALUE = 0xff,
	I2C_CLK_CTL_HS_DIVIDER_VALUE = 7U << 8,

	I2C_STATUS_WR_BUFFER_FULL = 1U << 0,
	I2C_STATUS_RD_BUFFER_FULL = 1U << 1,
	I2C_STATUS_BUS_ERROR = 1U << 2,
	I2C_STATUS_PACKET_NACKED = 1U << 3,
	I2C_STATUS_ARB_LOST = 1U << 4,
	I2C_STATUS_INVALID_WRITE = 1U << 5,
	I2C_STATUS_FAILED = 3U << 6,
	I2C_STATUS_BUS_ACTIVE = 1U << 8,
	I2C_STATUS_BUS_MASTER = 1U << 9,
	I2C_STATUS_ERROR_MASK = 0xfc,

	I2C_INTERFACE_SELECT_INTF_SELECT = 1U << 0,
	I2C_INTERFACE_SELECT_SCL = 1U << 8,
	I2C_INTERFACE_SELECT_SDA = 1U << 9,
};

static struct msm_i2c_dev {
	struct i2c_msg *msg;
	struct msm_i2c_pdata *pdata;
	int rem;
	int pos;
	int cnt;
	int ret;
	bool need_flush;
	int flush_cnt;
} dev;

#if DEBUG_I2C
static void
dump_status(uint32_t status)
{
	I2C_DBG(VDEBUG, "STATUS (0x%.8x): ", status);
	if (status & I2C_STATUS_BUS_MASTER)
		I2C_DBG(VDEBUG, "MST ");
	if (status & I2C_STATUS_BUS_ACTIVE)
		I2C_DBG(VDEBUG, "ACT ");
	if (status & I2C_STATUS_INVALID_WRITE)
		I2C_DBG(VDEBUG, "INV_WR ");
	if (status & I2C_STATUS_ARB_LOST)
		I2C_DBG(VDEBUG, "ARB_LST ");
	if (status & I2C_STATUS_PACKET_NACKED)
		I2C_DBG(VDEBUG, "NAK ");
	if (status & I2C_STATUS_BUS_ERROR)
		I2C_DBG(VDEBUG, "BUS_ERR ");
	if (status & I2C_STATUS_RD_BUFFER_FULL)
		I2C_DBG(VDEBUG, "RD_FULL ");
	if (status & I2C_STATUS_WR_BUFFER_FULL)
		I2C_DBG(VDEBUG, "WR_FULL ");
	if (status & I2C_STATUS_FAILED)
		I2C_DBG(VDEBUG, "FAIL 0x%x", (status & I2C_STATUS_FAILED));
	I2C_DBG(VDEBUG, "\n");
}
#else
static inline void dump_status(uint32_t status) {}
#endif

static void msm_i2c_write_delay(void)
{
	/* If scl is still high we have >4us (for 100kbps) to write the data
	 * register before we risk hitting a bug where the controller releases
	 * scl to soon after driving sda low. Writing the data after the
	 * scheduled release time for scl also avoids the bug.
	 */
	if (readl(dev.pdata->i2c_base + I2C_INTERFACE_SELECT) & I2C_INTERFACE_SELECT_SCL)
		return;
	udelay(6);
}

static bool msm_i2c_fill_write_buffer(void)
{
	uint16_t val;
	if (dev.pos < 0) {
		val = I2C_WRITE_DATA_ADDR_BYTE | dev.msg->addr << 1;
		if (dev.msg->flags & I2C_M_RD)
			val |= 1;
		if (dev.rem == 1 && dev.msg->len == 0)
			val |= I2C_WRITE_DATA_LAST_BYTE;
		msm_i2c_write_delay();
		writel(val, dev.pdata->i2c_base + I2C_WRITE_DATA);
		dev.pos++;
		return true;
	}

	if (dev.msg->flags & I2C_M_RD)
		return false;

	if (!dev.cnt)
		return false;

	/* Ready to take a byte */
	val = dev.msg->buf[dev.pos];
	if (dev.cnt == 1 && dev.rem == 1)
		val |= I2C_WRITE_DATA_LAST_BYTE;

	msm_i2c_write_delay();
	writel(val, dev.pdata->i2c_base + I2C_WRITE_DATA);
	dev.pos++;
	dev.cnt--;
	return true;
}

static void msm_i2c_read_buffer(void)
{
	/*
	 * Theres something in the FIFO.
	 * Are we expecting data or flush crap?
	 */
	 I2C_DBG_FUNC_LINE();
	if ((dev.msg->flags & I2C_M_RD) && dev.pos >= 0 && dev.cnt) {
		I2C_DBG_FUNC_LINE();
		switch (dev.cnt) {
		case 1:
			if (dev.pos != 0)
				break;
			dev.need_flush = true;
			/* fall-trough */
		case 2:
			writel(I2C_WRITE_DATA_LAST_BYTE,
			 dev.pdata->i2c_base + I2C_WRITE_DATA);
		}
		dev.msg->buf[dev.pos] = readl(dev.pdata->i2c_base + I2C_READ_DATA);
		dev.cnt--;
		dev.pos++;
	} else { /* FLUSH */
		I2C_DBG_FUNC_LINE();
		if (dev.flush_cnt & 1) {
			/*
			* Stop requests are sometimes ignored, but writing
			* more than one request generates a write error.
			*/
			writel(I2C_WRITE_DATA_LAST_BYTE,
				dev.pdata->i2c_base + I2C_WRITE_DATA);
		}
		readl(dev.pdata->i2c_base + I2C_READ_DATA);
		if (dev.need_flush)
			dev.need_flush = false;
		else
			dev.flush_cnt++;
	}
}

static void msm_i2c_interrupt_locked(void)
{
	uint32_t status	= readl(dev.pdata->i2c_base + I2C_STATUS);
	bool not_done = true;

	if (!dev.msg) {
		I2C_DBG(INFO,
			"IRQ but nothing to do!, status %x\n", status);
		dump_status(status);
		return;
	}
	if (status & I2C_STATUS_ERROR_MASK)
		goto out_err;

	if (!(status & I2C_STATUS_WR_BUFFER_FULL))
		not_done = msm_i2c_fill_write_buffer();
	if (status & I2C_STATUS_RD_BUFFER_FULL)
		msm_i2c_read_buffer();

	if (dev.pos >= 0 && dev.cnt == 0) {
		if (dev.rem > 1) {
			dev.rem--;
			dev.msg++;
			dev.pos = -1;
			dev.cnt = dev.msg->len;
		} else if (!not_done && !dev.need_flush)
			return;
	}
	return;

out_err:
	I2C_ERR("error, status %x\n", status);
	dev.ret = ERROR;
}

static enum handler_return msm_i2c_isr(void *arg) {
	enter_critical_section();
	msm_i2c_interrupt_locked();
	exit_critical_section();
	return INT_RESCHEDULE;
}

static int
msm_i2c_poll_notbusy(int warn)
{
	I2C_DBG(VDEBUG, "%s\n", __func__);
	uint32_t retries = 0;
	while (retries != 200) {
		uint32_t status = readl(dev.pdata->i2c_base + I2C_STATUS);

		if (!(status & I2C_STATUS_BUS_ACTIVE)) {
			if (retries && warn)
				I2C_DBG(INFO,
					"Warning bus was busy (%d)\n", retries);
			return 0;
		}
		if (retries++ > 100)
			thread_sleep(10);
	}
	I2C_ERR("Error waiting for notbusy\n");
	return ERR_TIMED_OUT;
}

static int
msm_i2c_recover_bus_busy(void)
{
	I2C_DBG(VDEBUG, "%s\n", __func__);
	int i;
	bool gpio_clk_status = false;
	uint32_t status = readl(dev.pdata->i2c_base + I2C_STATUS);
	I2C_DBG_FUNC_LINE();

	if (!(status & (I2C_STATUS_BUS_ACTIVE | I2C_STATUS_WR_BUFFER_FULL)))
		return 0;

	if (dev.pdata->set_mux_to_i2c)
		dev.pdata->set_mux_to_i2c(0);

	if (status & I2C_STATUS_RD_BUFFER_FULL) {
		I2C_DBG(INFO, "Read buffer full, status %x, intf %x\n",
			 status, readl(dev.pdata->i2c_base + I2C_INTERFACE_SELECT));
		writel(I2C_WRITE_DATA_LAST_BYTE, dev.pdata->i2c_base + I2C_WRITE_DATA);
		readl(dev.pdata->i2c_base + I2C_READ_DATA);
	}
	else if (status & I2C_STATUS_BUS_MASTER) {
		I2C_DBG(INFO, "Still the bus master, status %x, intf %x\n",
			 status, readl(dev.pdata->i2c_base + I2C_INTERFACE_SELECT));
		writel(I2C_WRITE_DATA_LAST_BYTE | 0xff,
		 dev.pdata->i2c_base + I2C_WRITE_DATA);
	}

	I2C_DBG(INFO, "i2c_scl: %d, i2c_sda: %d\n",
		 gpio_get(dev.pdata->scl_gpio), gpio_get(dev.pdata->sda_gpio));

	for (i = 0; i < 9; i++) {
		if (gpio_get(dev.pdata->sda_gpio) && gpio_clk_status)
			break;
		gpio_set(dev.pdata->scl_gpio, 0);
		udelay(5);
		gpio_set(dev.pdata->sda_gpio, 0);
		udelay(5);
		gpio_config(dev.pdata->scl_gpio, GPIO_INPUT);
		udelay(5);
		if (!gpio_get(dev.pdata->scl_gpio))
			udelay(20);
		if (!gpio_get(dev.pdata->scl_gpio))
			mdelay(10);
		gpio_clk_status = gpio_get(dev.pdata->scl_gpio);
		gpio_config(dev.pdata->sda_gpio, GPIO_INPUT);
		udelay(5);
	}
	if (dev.pdata->set_mux_to_i2c)
		dev.pdata->set_mux_to_i2c(1);

	udelay(10);

	status = readl(dev.pdata->i2c_base + I2C_STATUS);
	if (!(status & I2C_STATUS_BUS_ACTIVE)) {
		I2C_DBG(VDEBUG, "Bus busy cleared after %d clock cycles, "
			 "status %x, intf %x\n",
			 i, status, readl(dev.pdata->i2c_base + I2C_INTERFACE_SELECT));
		return 0;
	}

	I2C_DBG(INFO, "Bus still busy, status %x, intf %x\n",
		 status, readl(dev.pdata->i2c_base + I2C_INTERFACE_SELECT));
	return ERR_NOT_READY;
}


static int
msm_i2c_xfer(struct i2c_msg msgs[], int num)
{
	int ret;
	int timeout;

	clk_enable(dev.pdata->clk_nr);
	unmask_interrupt(dev.pdata->irq_nr);
	I2C_DBG_FUNC_LINE();

	ret = msm_i2c_poll_notbusy(1);

	if (ret) {
		ret = msm_i2c_recover_bus_busy();
		if (ret)
			goto err;
	}
	I2C_DBG_FUNC_LINE();

	enter_critical_section();
	I2C_DBG_FUNC_LINE();

	if (dev.flush_cnt) {
		I2C_DBG(INFO, "%d unrequested bytes read\n",
			 dev.flush_cnt);
	}
	dev.msg = msgs;
	dev.rem = num;
	dev.pos = -1;
	dev.ret = num;
	dev.need_flush = false;
	dev.flush_cnt = 0;
	dev.cnt = msgs->len;

	msm_i2c_interrupt_locked();
	exit_critical_section();

	/*
	 * Now that we've setup the xfer, the ISR will transfer the data
	 * and wake us up with dev.err set if there was an error
	 */

	timeout = msm_i2c_poll_notbusy(0); /* Read may not have stopped in time */
	I2C_DBG_FUNC_LINE();

	enter_critical_section();
		I2C_DBG_FUNC_LINE();

	if (dev.flush_cnt) {
		I2C_DBG(INFO, "%d unrequested bytes read\n",
			 dev.flush_cnt);
	}
	ret = dev.ret;
	dev.msg = NULL;
	dev.rem = 0;
	dev.pos = 0;
	dev.ret = 0;
	dev.flush_cnt = 0;
	dev.cnt = 0;
	exit_critical_section();

	if (timeout == ERR_TIMED_OUT) {
		I2C_DBG(INFO, "Transaction timed out\n");
		ret = ERR_TIMED_OUT;
	}

	if (ret < 0) {
		I2C_ERR("Error during data xfer (%d)\n", ret);
		msm_i2c_recover_bus_busy();
	}
err:
	mask_interrupt(dev.pdata->irq_nr);
	clk_disable(dev.pdata->clk_nr);
	return ret;
}

int msm_i2c_write(int chip, void *buf, size_t count) {
	int rc;
	int retry;

	if (!dev.pdata) {
		I2C_DBG(ALWAYS, "[MSM-I2C]: %s: called when driver is not installed\n",
				__func__);
		return ERR_NOT_READY;
	}

	struct i2c_msg msg[] = {
		{
		 .addr = chip,
		 .flags = 0,
		 .len = count,
		 .buf = buf,
		 },
	};
	I2C_DBG(VDEBUG, "+%s\n", __func__);
	for (retry = 0; retry <= MSM_I2C_WRITE_RETRY_TIMES; retry++) {
		rc = msm_i2c_xfer(msg, 1);
		if (rc == 1) {
			rc = 0;
			goto ret;
		}
		mdelay(10);
		I2C_DBG(INFO, "%s, i2c write retry\n", __func__);
	}
	I2C_DBG(INFO, "micropklt_write, i2c_write_block retry over %d\n",
	       MSM_I2C_WRITE_RETRY_TIMES);

ret:
	I2C_DBG(VDEBUG, "-%s\n", __func__);
	return rc;
}

int msm_i2c_read(int chip, uint8_t reg, void *buf, size_t count) {
	int retry;
	int rc;

	if (!dev.pdata) {
		I2C_DBG(ALWAYS, "[MSM-I2C]: %s: called when driver is not installed\n",
				__func__);
		return ERR_NOT_READY;
	}

	struct i2c_msg msgs[] = {
		{
		 .addr = chip,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg,
		 },
		{
		 .addr = chip,
		 .flags = I2C_M_RD,
		 .len = count,
		 .buf = buf,
		 }
	};
	for (retry = 0; retry <= MSM_I2C_READ_RETRY_TIMES; retry++) {
		rc = msm_i2c_xfer(msgs, 2);
		if (rc == 2) {
			return 0;
		}
		mdelay(10);
		I2C_DBG(INFO, "%s, i2c read retry\n", __func__);
	}
	I2C_DBG(INFO, "i2c_read_block retry over %d\n",
		MSM_I2C_READ_RETRY_TIMES);
	return ERR_NOT_READY;
}

int msm_i2c_probe(struct msm_i2c_pdata* pdata)
{
	if (dev.pdata) {
		I2C_DBG(ALWAYS, "[MSM-I2C]: already installed\n");
		return -1;
	}

	if (!pdata->set_mux_to_i2c) {
		I2C_DBG(ALWAYS, "[MSM-I2C]: pdata->set_mut_to_i2c is NULL\n");
		return -1;
	}

	dev.pdata = pdata;

	enter_critical_section();
	mask_interrupt(dev.pdata->irq_nr);
	dev.pdata->set_mux_to_i2c(0);
	clk_enable(dev.pdata->clk_nr);

	/* I2C_HS_CLK = I2C_CLK/(3*(HS_DIVIDER_VALUE+1) */
	/* I2C_FS_CLK = I2C_CLK/(2*(FS_DIVIDER_VALUE+3) */
	/* FS_DIVIDER_VALUE = ((I2C_CLK / I2C_FS_CLK) / 2) - 3 */
	int i2c_clk = 19200000; /* input clock */
	int target_clk = 100000;
	/* target_clk = 200000; */
	int fs_div = ((i2c_clk / target_clk) / 2) - 3;
	int hs_div = 3;
	int clk_ctl = ((hs_div & 0x7) << 8) | (fs_div & 0xff);
	writel(clk_ctl, pdata->i2c_base + I2C_CLK_CTL);
	I2C_DBG(VDEBUG, "msm_i2c_probe: clk_ctl %x, %d Hz\n",
	 clk_ctl, i2c_clk / (2 * ((clk_ctl & 0xff) + 3)));
	clk_disable(pdata->clk_nr);

	register_int_handler(pdata->irq_nr, msm_i2c_isr, NULL);
	unmask_interrupt(pdata->irq_nr);
	exit_critical_section();

	return 0;
}

void msm_i2c_remove() {
	if (!dev.pdata)
		return;
	I2C_DBG(VDEBUG, "+%s\n", __func__);
	enter_critical_section();
	mask_interrupt(dev.pdata->irq_nr);
	clk_disable(dev.pdata->clk_nr);
	dev.pdata->set_mux_to_i2c(0);
	dev.pdata = NULL;
	exit_critical_section();
	I2C_DBG(VDEBUG, "-%s\n", __func__);
}
