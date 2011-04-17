#ifndef __MSM_I2C_H__
#define __MSM_I2C_H__

#include <sys/types.h>

#define MSM_I2C_READ_RETRY_TIMES 10
#define MSM_I2C_WRITE_RETRY_TIMES 10

struct msm_i2c_pdata {
	int scl_gpio;
	int sda_gpio;
	int clk_nr;
	int irq_nr;
	void *i2c_base;
	void (*set_mux_to_i2c)(int mux_to_i2c);
};

int msm_i2c_probe(struct msm_i2c_pdata*);
void msm_i2c_remove(void);
int msm_i2c_write(int chip, void *buf, size_t count);
int msm_i2c_read(int chip, uint8_t reg, void *buf, size_t count);

#endif //__MSM_I2C_H__
