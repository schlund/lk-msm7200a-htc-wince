#ifndef __HSUSB_H__
#define __HSUSB_H__

#include <dev/power_supply.h>

struct msm_hsusb_pdata {
	void (*set_ulpi_state) (int);
	struct pda_power_supply* power_supply;
};

void msm_hsusb_init(struct msm_hsusb_pdata*);

#endif	// __HSUSB_H__
