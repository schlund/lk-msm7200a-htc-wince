#ifndef __DEV_GPIO_KEYS_H__
#define __DEV_GPIO_KEYS_H__

struct gpio_key {
		int gpio;
		int keycode;
		bool active_low;
		char *name;
};

struct gpio_keys_pdata {
		struct gpio_key *keys;
		int nkeys;
};

extern void gpio_keys_init(struct gpio_keys_pdata *);

#endif //__DEV_GPIO_KEYS__
