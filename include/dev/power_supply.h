#ifndef __POWER_SUPPLY_H__
#define __POWER_SUPPLY_H__

enum psy_charger_state {
	CHG_OFF,
	CHG_USB_LOW,
	CHG_USB_HIGH,
	CHG_AC,
};

struct pda_power_supply {
	bool (*is_usb_online) (void);
	bool (*is_ac_online) (void);
	void (*set_charger_state) (enum psy_charger_state);
	bool (*want_charging) (void);
};

#endif //__POWER_SUPPLY_H__
