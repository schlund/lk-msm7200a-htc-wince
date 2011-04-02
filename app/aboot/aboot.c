/*
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <app.h>
#include <array.h>
#include <debug.h>
#include <platform.h>
#include <string.h>
#include <target.h>
#include <arch/arm.h>
#include <arch/ops.h>
#include <dev/flash.h>
#include <dev/keys.h>
#include <dev/fbcon.h>
#include <dev/usb.h>
#include <kernel/thread.h>
#include <lib/ptable.h>

#include "recovery.h"
#include "bootimg.h"
#include "fastboot.h"

#define EXPAND(NAME) #NAME
#define TARGET(NAME) EXPAND(NAME)
#define DEFAULT_CMDLINE "force_cdma=0 no_partitions init=/init fbcon=rotate:2 rel_path=andboot";

static const char *my_cmd = "";

struct atag_ptbl_entry {
	char name[16];
	unsigned offset;
	unsigned size;
	unsigned flags;
};

unsigned *target_atag_mem(unsigned *ptr);

static void ptentry_to_tag(unsigned **ptr, struct ptentry *ptn)
{
	struct atag_ptbl_entry atag_ptn;

	if (ptn->type == TYPE_MODEM_PARTITION)
		return;
	memcpy(atag_ptn.name, ptn->name, 16);
	atag_ptn.name[15] = '\0';
	atag_ptn.offset = ptn->start;
	atag_ptn.size = ptn->length;
	atag_ptn.flags = ptn->flags;
	memcpy(*ptr, &atag_ptn, sizeof(struct atag_ptbl_entry));
	*ptr += sizeof(struct atag_ptbl_entry) / sizeof(unsigned);
}

void boot_linux(void *kernel, unsigned *tags,
	   const char *cmdline, unsigned machtype,
	   void *ramdisk, unsigned ramdisk_size)
{
	unsigned *ptr = tags;
	unsigned pcount = 0;
	void (*entry) (unsigned, unsigned, unsigned *) = kernel;
	struct ptable *ptable;
	int cmdline_len = 0;
	int have_cmdline = 0;

	/* CORE */
	*ptr++ = 2;
	*ptr++ = 0x54410001;

	if (ramdisk_size) {
		*ptr++ = 4;
		*ptr++ = 0x54420005;
		*ptr++ = (unsigned)ramdisk;
		*ptr++ = ramdisk_size;
	}

	ptr = target_atag_mem(ptr);

	if (!target_is_emmc_boot()) {
		/* Skip NAND partition ATAGS for eMMC boot */
		if ((ptable = flash_get_ptable()) && (ptable->count != 0)) {
			int i;
			for (i = 0; i < ptable->count; i++) {
				struct ptentry *ptn;
				ptn = ptable_get(ptable, i);
				if (ptn->type == TYPE_APPS_PARTITION)
					pcount++;
			}
			*ptr++ = 2 + (pcount * (sizeof(struct atag_ptbl_entry) /
						sizeof(unsigned)));
			*ptr++ = 0x4d534d70;
			for (i = 0; i < ptable->count; ++i)
				ptentry_to_tag(&ptr, ptable_get(ptable, i));
		}
	}

	if (cmdline && cmdline[0]) {
		cmdline_len = strlen(cmdline);
		have_cmdline = 1;
	}

	cmdline_len += strlen(my_cmd);


	if (cmdline_len > 0) {
		const char *src;
		char *dst;
		unsigned n;
		/* include terminating 0 and round up to a word multiple */
		n = (cmdline_len + 4) & (~3);
		*ptr++ = (n / 4) + 2;
		*ptr++ = 0x54410009;
		dst = (char *)ptr;
		if (have_cmdline) {
			src = cmdline;
			while ((*dst++ = *src++)) ;
		}

		src = my_cmd;
			if (have_cmdline)
				--dst;
			have_cmdline = 1;
			while ((*dst++ = *src++)) ;

		ptr += (n / 4);
	}

	/* END */
	*ptr++ = 0;
	*ptr++ = 0;

	dprintf(INFO, "booting linux @ %p, ramdisk @ %p (%d)\n",
		kernel, ramdisk, ramdisk_size);
	if (cmdline)
		dprintf(INFO, "cmdline: %s\n", cmdline);

	enter_critical_section();
	platform_exit();
	arch_disable_cache(UCACHE);
	arch_disable_mmu();
	entry(0, machtype, tags);
}

unsigned page_size = 0;
unsigned page_mask = 0;

#define ROUND_TO_PAGE(x,y) (((x) + (y)) & (~(y)))

static unsigned char buf[4096];	//Equal to max-supported pagesize

int boot_linux_from_flash(void)
{
	struct boot_img_hdr *hdr = (void *)buf;
	unsigned n;
	struct ptentry *ptn;
	struct ptable *ptable;
	unsigned offset = 0;
	const char *cmdline;

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		dprintf(CRITICAL, "ERROR: Partition table not found\n");
		return -1;
	}

	if (!boot_into_recovery) {
		ptn = ptable_find(ptable, "boot");
		if (ptn == NULL) {
			dprintf(CRITICAL, "ERROR: No boot partition found\n");
			return -1;
		}
	} else {
		ptn = ptable_find(ptable, "recovery");
		if (ptn == NULL) {
			dprintf(CRITICAL,
				"ERROR: No recovery partition found\n");
			return -1;
		}
	}

	if (flash_read(ptn, offset, buf, page_size)) {
		dprintf(CRITICAL, "ERROR: Cannot read boot image header\n");
		return -1;
	}
	offset += page_size;

	if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		dprintf(CRITICAL, "ERROR: Invaled boot image heador\n");
		return -1;
	}

	if (hdr->page_size != page_size) {
		dprintf(CRITICAL,
			"ERROR: Invaled boot image pagesize. Device pagesize: %d, Image pagesize: %d\n",
			page_size, hdr->page_size);
		return -1;
	}

	n = ROUND_TO_PAGE(hdr->kernel_size, page_mask);
	if (flash_read(ptn, offset, (void *)hdr->kernel_addr, n)) {
		dprintf(CRITICAL, "ERROR: Cannot read kernel image\n");
		return -1;
	}
	offset += n;

	n = ROUND_TO_PAGE(hdr->ramdisk_size, page_mask);
	if (flash_read(ptn, offset, (void *)hdr->ramdisk_addr, n)) {
		dprintf(CRITICAL, "ERROR: Cannot read ramdisk image\n");
		return -1;
	}
	offset += n;

	dprintf(INFO, "\nkernel  @ %x (%d bytes)\n", hdr->kernel_addr,
		hdr->kernel_size);
	dprintf(INFO, "ramdisk @ %x (%d bytes)\n", hdr->ramdisk_addr,
		hdr->ramdisk_size);

	if (hdr->cmdline[0]) {
		cmdline = (char *)hdr->cmdline;
	} else {
		cmdline = DEFAULT_CMDLINE;
	}
	dprintf(INFO, "cmdline = '%s'\n", cmdline);

	/* TODO: create/pass atags to kernel */

	dprintf(INFO, "\nBooting Linux\n");
	boot_linux((void *)hdr->kernel_addr, (void *)TAGS_ADDR,
		   (const char *)cmdline, target_machtype(),
		   (void *)hdr->ramdisk_addr, hdr->ramdisk_size);

	return 0;
}

void cmd_boot(const char *arg, void *data, unsigned sz)
{
	unsigned kernel_actual;
	unsigned ramdisk_actual;
	static struct boot_img_hdr hdr;
	char *ptr = ((char *)data);

	if (sz < sizeof(hdr)) {
		fastboot_fail("invalid bootimage header");
		return;
	}

	memcpy(&hdr, data, sizeof(hdr));

	/* ensure commandline is terminated */
	hdr.cmdline[BOOT_ARGS_SIZE - 1] = 0;

	if (target_is_emmc_boot() && hdr.page_size) {
		page_size = hdr.page_size;
		page_mask = page_size - 1;
	}

	kernel_actual = ROUND_TO_PAGE(hdr.kernel_size, page_mask);
	ramdisk_actual = ROUND_TO_PAGE(hdr.ramdisk_size, page_mask);

	if (page_size + kernel_actual + ramdisk_actual < sz) {
		fastboot_fail("incomplete bootimage");
		return;
	}

	memmove((void *)KERNEL_ADDR, ptr + page_size, hdr.kernel_size);
	memmove((void *)RAMDISK_ADDR, ptr + page_size + kernel_actual,
		hdr.ramdisk_size);

	fastboot_okay("");
	udc_stop();

	boot_linux((void *)KERNEL_ADDR, (void *)TAGS_ADDR,
		   (const char *)hdr.cmdline, target_machtype(),
		   (void *)RAMDISK_ADDR, hdr.ramdisk_size);
}

void cmd_erase(const char *arg, void *data, unsigned sz)
{
	struct ptentry *ptn;
	struct ptable *ptable;

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		fastboot_fail("partition table doesn't exist");
		return;
	}

	ptn = ptable_find(ptable, arg);
	if (ptn == NULL) {
		fastboot_fail("unknown partition name");
		return;
	}

	if (flash_erase(ptn)) {
		fastboot_fail("failed to erase partition");
		return;
	}
	fastboot_okay("");
}

void cmd_flash(const char *arg, void *data, unsigned sz)
{
	struct ptentry *ptn;
	struct ptable *ptable;
	unsigned extra = 0;

	ptable = flash_get_ptable();
	if (ptable == NULL) {
		fastboot_fail("partition table doesn't exist");
		return;
	}

	ptn = ptable_find(ptable, arg);
	if (ptn == NULL) {
		fastboot_fail("unknown partition name");
		return;
	}

	if (!strcmp(ptn->name, "boot") || !strcmp(ptn->name, "recovery")) {
		if (memcmp((void *)data, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
			fastboot_fail("image is not a boot image");
			return;
		}
	}

	if (!strcmp(ptn->name, "system") || !strcmp(ptn->name, "userdata")
	    || !strcmp(ptn->name, "persist"))
		extra = ((page_size >> 9) * 16);
	else
		sz = ROUND_TO_PAGE(sz, page_mask);

	dprintf(INFO, "writing %d bytes to '%s'\n", sz, ptn->name);
	if (flash_write(ptn, extra, data, sz)) {
		fastboot_fail("flash write failure");
		return;
	}
	dprintf(INFO, "partition '%s' updated\n", ptn->name);
	fastboot_okay("");
}

void cmd_continue(const char *arg, void *data, unsigned sz)
{
	fastboot_okay("");
	udc_stop();
	boot_linux_from_flash();
}

void cmd_reboot(const char *arg, void *data, unsigned sz)
{
	dprintf(INFO, "rebooting the device\n");
	fastboot_okay("");
	reboot_device(BOOT_WARM);
}

void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz)
{
	dprintf(INFO, "rebooting the device\n");
	fastboot_okay("");
	reboot_device(BOOT_FASTBOOT);
}

static void enter_fastboot(void) {
	fastboot_register("flash:", cmd_flash);
	fastboot_register("erase:", cmd_erase);

	fastboot_register("boot", cmd_boot);
	fastboot_register("continue", cmd_continue);
	fastboot_register("reboot", cmd_reboot);
	fastboot_register("reboot-bootloader", cmd_reboot_bootloader);
	fastboot_publish("product", TARGET(BOARD));
	fastboot_publish("kernel", "lk");

	fastboot_init(target_get_scratch_address(), 120 * 1024 * 1024);
	dprintf(INFO, "starting usb\n");
	udc_start();
}

static void reboot(void) {
	reboot_device(BOOT_WARM);
}

static void reboot_bootloader(void) {
	reboot_device(BOOT_FASTBOOT);
}

static void boot_nand(void) {
	recovery_init();
	boot_linux_from_flash();

	dprintf(CRITICAL, "ERROR: Could not do normal boot. Reverting "
		"to fastboot mode.\n");

	enter_fastboot();
}

static void boot_recovery(void) {
	boot_into_recovery = 1;
	boot_nand();
}

static struct menu_entry {
	char* title;
	void (*callback)(void);
	bool oneshot;
} menu[] = {
	{
		"Boot from NAND",
		boot_nand,
		true,
	},
	{
		"Boot into recovery",
		boot_recovery,
		true,
	},
	{
		"Fastboot mode",
		enter_fastboot,
		true,
	},
	{
		"Reboot",
		reboot,
		false,
	},
	{
		"Reboot to bootloader",
		reboot_bootloader,
		false,
	},
	{
		"Shutdown",
		target_shutdown,
		false,
	},
	{
		"Charge mode",
		NULL,
		false,
	},
	{
		"Dump ramconsole",
		NULL,
		false,
	}
};

enum menu_action {
	MENU_UP,
	MENU_DOWN,
	MENU_SELECT,
	MENU_REDRAW,
};

static unsigned curr_menu_idx = 0;

static bool menu_update(enum menu_action cmd) {
	unsigned idx = 0;
	enter_critical_section();
	switch (cmd) {
		case MENU_UP:
			curr_menu_idx = (curr_menu_idx + 1) % ARRAY_SIZE(menu);
			break;
		case MENU_DOWN:
			curr_menu_idx = curr_menu_idx > 0 ?
				curr_menu_idx - 1 : ARRAY_SIZE(menu) - 1;
			break;
		case MENU_SELECT:
			goto callback;
		case MENU_REDRAW:
			break;
	}
	idx = curr_menu_idx;
	exit_critical_section();

	fbcon_clear();
	for (unsigned i = 0; i < ARRAY_SIZE(menu); i++) {
		if (idx == i) {
			printf(">>>%d: %s<<<\n", i, menu[i].title);
		}
		else {
			printf("%d: %s\n", i, menu[i].title);
		}
	}
	return false;

callback:
	idx = curr_menu_idx;
	exit_critical_section();
	if (menu[idx].callback) {
		menu[idx].callback();
		return menu[idx].oneshot;
	}
	else {
		printf("No callback for menu item %d\n", curr_menu_idx);
	}
	return false;
}

static void handle_keypad(void) {
	int ret = 0;
	uint16_t last_code = 0;
	uint16_t code = 0;
	int16_t value = 0;
	int16_t last_value = -1;

	printf("press any key to enter boot menu...\n");
	ret = keys_wait_event_timeout(&last_code, &value, 5000);
	if (ret)
		return;

	menu_update(MENU_REDRAW);
	while (!(ret = keys_wait_event_timeout(&code, &value, 0))) {
		//do not handle both release and keypress
		if (last_code == code && last_value != value)
			continue;

		last_code = code;
		last_value = value;

		switch (code) {
			case KEY_UP:
			case KEY_VOLUMEUP:
				menu_update(MENU_UP);
				//printf("Key up\n");
				break;

			case KEY_DOWN:
			case KEY_VOLUMEDOWN:
				if(menu_update(MENU_DOWN));
					return;
				//printf("Key down\n");
				break;

			case KEY_ENTER:
			case KEY_POWER:
				menu_update(MENU_SELECT);
				//printf("Key enter\n");
				break;

			default:
				break;
		}
		//printf("[KEY]: code=%d value=%d\n", code, value);
	}
}

void aboot_init(const struct app_descriptor *app)
{
	enum boot_reason bootreason = get_boot_reason();
	switch (bootreason) {
		case BOOT_FASTBOOT:
			goto fastboot;
			break;
		case BOOT_RECOVERY:
			boot_into_recovery = 1;
			goto skip_keypad;
			break;
		default:
			break;
	}

	handle_keypad();
	printf("no user choice, defaulting to nand boot\n");
	if (is_usb_connected())
		goto fastboot;

skip_keypad:
	boot_nand();

fastboot:
	enter_fastboot();
}

APP_START(aboot)
	.init = aboot_init,
APP_END
