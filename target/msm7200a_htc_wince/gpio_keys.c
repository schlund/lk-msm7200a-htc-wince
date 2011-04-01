/* gpio-keys.c - code for handling keys connected directly to gpios
 *
 * Copyright (C) 2011 Alexander Tarasikov <alexander.tarasikov@gmail.com>
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

#include <debug.h>
#include <kernel/thread.h>
#include <dev/gpio.h>
#include <dev/keys.h>
#include <dev/gpio_keys.h>
#include <platform/interrupts.h>

static enum handler_return gpio_key_irq(void *arg)
{
	struct gpio_key *key = arg;
	int state = !!gpio_get(key->gpio);
	keys_post_event(key->keycode, state ^ key->active_low);
	return INT_RESCHEDULE;
}

void gpio_keys_init(struct gpio_keys_pdata *pdata) {
	dprintf(INFO, "+%s\n", __func__);
	if (!pdata) {
		dprintf(CRITICAL, "%s: pdata is NULL\n", __func__);
	}

	for (int i = 0; i < pdata->nkeys; i++) {
		int gpio = pdata->keys[i].gpio;
		int cfg = gpio_config(gpio, GPIO_INPUT);
		if (cfg < 0) {
			dprintf(CRITICAL, "%s: cannot set gpio %d as input\n", __func__, gpio);
			continue;
		}
		register_gpio_int_handler(gpio, gpio_key_irq, &pdata->keys[i]);
		unmask_gpio_interrupt(gpio, GPIO_IRQF_BOTH);
	}
	dprintf(INFO, "-%s\n", __func__);
}

