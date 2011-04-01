#define DS2746_DATA_SIZE			0x12
#define DS2746_STATUS_REG			0x01
#define DS2746_AUX0_MSB				0x08
#define DS2746_AUX0_LSB				0x09
#define DS2746_AUX1_MSB				0x0a
#define DS2746_AUX1_LSB				0x0b
#define DS2746_VOLTAGE_MSB			0x0c
#define DS2746_VOLTAGE_LSB			0x0d
#define DS2746_CURRENT_MSB			0x0e
#define DS2746_CURRENT_LSB			0x0f
#define DS2746_CURRENT_ACCUM_MSB	0x10
#define DS2746_CURRENT_ACCUM_LSB	0x11
#define DS2746_OFFSET_BIAS			0x61
#define DS2746_ACCUM_BIAS			0x62

#define DEFAULT_RSNS				1500
#define DEFAULT_BATTERY_RATING		1500
#define DEFAULT_HIGH_VOLTAGE		4200
#define DEFAULT_LOW_VOLTAGE			3300

#define DS2746_CURRENT_ACCUM_RES	645
#define DS2746_VOLTAGE_RES			2440

struct ds2746_info {
	u32 batt_id;		/* Battery ID from ADC */
	u32 batt_vol;		/* Battery voltage from ADC */
	u32 batt_temp;		/* Battery Temperature (C) from formula and ADC */
	int batt_current;	/* Battery current from ADC */
	u32 level;		/* formula */
	u32 charging_source;	/* 0: no cable, 1:usb, 2:AC */
	bool charging_enabled;	/* 0: Disable, 1: Enable */
	u32 full_bat;		/* Full capacity of battery (mAh) */
	struct ds2746_platform_data bat_pdata;
};

static int i2c_read(int r)
{
	unsigned char i2c_msg[1];
	unsigned char i2c_data[2];
	i2c_msg[0] = r;
	i2c_master_send(pclient, i2c_msg, 1);
	i2c_master_recv(pclient, i2c_data, 2);
	return i2c_data[0];
}

static void i2c_write(int r, int v)
{
	unsigned char i2c_msg[3];
	i2c_msg[0] = r;
	i2c_msg[1] = v >> 8;
	i2c_msg[2] = v & 0xFF;
	i2c_master_send(pclient, i2c_msg, 3);
}

static int reading2capacity(int r)
{
	return (DS2746_CURRENT_ACCUM_RES * r) / (bi->bat_pdata.resistance);
}

static int ds2746_battery_read_status(struct ds2746_info *b)
{
	signed short s;
	int aux0, aux1;
	int aux0r, aux1r;


	if (!pclient) {
		printk(KERN_INFO "client is null\n");
		b->level = 50;
		return 0;
	}

	s = i2c_read(DS2746_VOLTAGE_LSB);
	s |= i2c_read(DS2746_VOLTAGE_MSB) << 8;
	b->batt_vol = ((s >> 4) * DS2746_VOLTAGE_RES) / 1000;

	s = i2c_read(DS2746_CURRENT_LSB);
	s |= i2c_read(DS2746_CURRENT_MSB) << 8;
	b->batt_current = ((s >> 2) * DS2746_CURRENT_ACCUM_RES) / (bi->bat_pdata.resistance);

	/* if battery voltage is < 3.3V and depleting, we assume it's almost empty! */
	if (b->batt_vol < bi->bat_pdata.low_voltage && b->batt_current < 0) {
		aux0 = ((b->batt_vol - bi->bat_pdata.low_voltage) * current_accum_capacity * 5) / 3;
		printk(KERN_INFO "ds2746: correcting ACR to %d\n", aux0);
		/* use approximate formula: 3.3V=5%, 3.0V=0% */
		/* correction-factor is (capacity * 0.05) / (3300 - 3000) */
		/*  or (capacity*5/3) */
		i2c_write(DS2746_CURRENT_ACCUM_MSB, 0);
		i2c_write(DS2746_CURRENT_ACCUM_LSB, aux0);
	}

	s = i2c_read(DS2746_CURRENT_ACCUM_LSB);
	s |= i2c_read(DS2746_CURRENT_ACCUM_MSB) << 8;

	if (s > 0) {
		if (s > current_accum_capacity) {
			/* if the battery is "fuller" than expected,
			 * update our expectations */
			printk(KERN_INFO "ds2746: old capacity == %u\n", current_accum_capacity);
			current_accum_capacity = s;
			battery_capacity = reading2capacity(s);
			printk(KERN_INFO
			       "ds2746: correcting max capacity to %d mAh (current_capacity=%u)\n",
			       battery_capacity, current_accum_capacity);
		} else {
			/* check if we're >4.2V and <5mA */
			if ((b->batt_vol >= bi->bat_pdata.high_voltage) && (b->batt_current < 5)) {
				/* that's almost full, so let's assume it is */
				current_accum_capacity = s + 20;
				battery_capacity = reading2capacity(s);
				printk(KERN_INFO
				       "ds2746: correcting capacity to %d (current capacity=%u)\n",
				       battery_capacity,
				       current_accum_capacity);
			}
		}
	}

	b->level = (s * 100) / current_accum_capacity;
	/* if we read 0%, */
	if (b->level < 1) {
		/* only report 0% if we're really down, <3.1V */
		b->level = ((b->batt_vol <= bi->bat_pdata.low_voltage - 100) ? 0 : 1);
	}

	if (b->level > 100)
		b->level = 100;

	aux0 = i2c_read(DS2746_AUX0_LSB);
	aux0 |= i2c_read(DS2746_AUX0_MSB) << 8;
	aux0 >>= 4;

	aux1 = i2c_read(DS2746_AUX1_LSB);
	aux1 |= i2c_read(DS2746_AUX1_MSB) << 8;
	aux1 >>= 4;

	// ratio
	aux0r = (1000L * aux0) / 2047;
	aux1r = (1000L * aux1) / 2047;
	// resistance in Ohm (R_AUX=10kOhm)
	//aux0r = (10000L*aux0r)/(1000-aux0r);
	//aux1r = (10000L*aux1r)/(1000-aux1r);

	b->batt_temp = (aux1r - 32L);
	b->batt_temp = ((b->batt_temp * 5) / 9) + 5;
	DBG("ds2746: %dmV %dmA charge: %d/100 (%d units) aux0: %d (%d) aux1: %d (%d) / %d\n",
	       b->batt_vol, b->batt_current, b->level, s, aux0, aux0r, aux1,
	       aux1r, b->batt_temp);

	return 0;
}

static int ds2746_probe(struct battery_pdata)
{
	int ret;
	struct ds2746_platform_data pdata = {
		.resistance = DEFAULT_RSNS,
		.capacity = DEFAULT_BATTERY_RATING,
		.high_voltage = DEFAULT_HIGH_VOLTAGE,
		.low_voltage = DEFAULT_LOW_VOLTAGE,
	};

	if (client->dev.platform_data)
		pdata = *(struct ds2746_platform_data*)(client->dev.platform_data);

	current_accum_capacity = pdata.capacity * pdata.resistance;
	current_accum_capacity /= DS2746_CURRENT_ACCUM_RES;

	bi->bat_pdata = pdata;
	bi->full_bat = 100;

	DBG("ds2746: resistance = %d, capacity = %d, "
		"high_voltage = %d, low_voltage = %d, softACR = %d\n",
		pdata.resistance, pdata.capacity,
		pdata.high_voltage, pdata.low_voltage,
		current_accum_capacity);


	return 0;
}

static struct battery ds2746_battery {
	.probe = ds2746_probe,
}
