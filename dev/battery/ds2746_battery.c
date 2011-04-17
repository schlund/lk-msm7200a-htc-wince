#include <platform/msm_i2c.h>
#include <dev/battery/ds2746.h>

uint32_t ds2746_read_voltage_mv(uint8_t addr) {
	uint16_t vol;
	uint8_t s0, s1;
	msm_i2c_read(addr, DS2746_VOLTAGE_MSB, &s0, 1);
	msm_i2c_read(addr, DS2746_VOLTAGE_LSB, &s1, 1);

	vol = s0 << 8;
	vol |= s1;

	uint32_t ret = ((vol >> 4) * DS2746_VOLTAGE_RES) / 1000;
	return ret;
}
