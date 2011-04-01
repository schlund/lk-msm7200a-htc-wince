#include <app.h>
#include <debug.h>
#include <lib/console.h>
#include <kernel/thread.h>

static void pooploader_init(const struct app_descriptor *app)
{
	dprintf(ALWAYS, "%s\n", __func__);
}

static void pooploader_entry(const struct app_descriptor *app, void *args)
{
	dprintf(ALWAYS, "%s\n", __func__);
	while (1) {
		dprintf(ALWAYS, "poop\n");
		thread_sleep(1000);
	}
}


APP_START(pooploader).init = pooploader_init,.entry = pooploader_entry, APP_END
