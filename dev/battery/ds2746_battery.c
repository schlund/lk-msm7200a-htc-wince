#include <platform/msm_i2c.h>
#include <dev/battery/ds2746.h>

uint16_t ds2746_read_voltage_mv(uint8_t addr) {
	uint16_t vol;
	uint8_t s0, s1;
	msm_i2c_read(addr, DS2746_VOLTAGE_MSB, &s0, 1);
	msm_i2c_read(addr, DS2746_VOLTAGE_LSB, &s1, 1);

	vol = s0 << 8;
	vol |= s1;

	return ((vol >> 4) * DS2746_VOLTAGE_RES) / 1000;
}

int16_t ds2746_read_current_ma(uint8_t addr, uint16_t resistance) {
	int16_t cur;
	int8_t s0, s1;
	msm_i2c_read(addr, DS2746_CURRENT_MSB, &s0, 1);
	msm_i2c_read(addr, DS2746_CURRENT_LSB, &s1, 1);

	cur = s0 << 8;
	cur |= s1;

	return ((cur >> 2) * DS2746_CURRENT_ACCUM_RES) / resistance;
}
