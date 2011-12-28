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
#include <dev/udc.h>
#include <dev/usb.h>
#include <kernel/thread.h>
#include <lib/ptable.h>

#include "recovery.h"
#include "bootimg.h"
#include "fastboot.h"

#define EXPAND(NAME) #NAME
#define TARGET(NAME) EXPAND(NAME)

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
	void (*entry) (unsigned, unsigned, unsigned *) = kernel;
	struct ptable *ptable;
	int cmdline_len = 0;
	bool have_cmdline = 0;
	char* t_cmdline = target_get_cmdline();
	char* cmd = NULL;


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

	if ((ptable = flash_get_ptable()) && (ptable->count != 0)) {
		int i;
		*ptr++ = 2 + (ptable->count * (sizeof(struct atag_ptbl_entry) /
					sizeof(unsigned)));
		*ptr++ = 0x4d534d70;
		for (i = 0; i < ptable->count; ++i)
			ptentry_to_tag(&ptr, ptable_get(ptable, i));
	}

	if (cmdline && cmdline[0]) {
		cmd = cmdline;
	}
	else if (t_cmdline && t_cmdline[0]) {
		cmd = t_cmdline;
	}
	cmdline_len = strlen(cmd);

	if (cmdline_len > 0) {
		unsigned n;
		/* include terminating 0 and round up to a word multiple */
		n = (cmdline_len + 4) & (~3);
		*ptr++ = (n / 4) + 2;
		*ptr++ = 0x54410009;
		memcpy(ptr, cmd, cmdline_len);
		ptr += (n / 4);
	}

	/* END */
	*ptr++ = 0;
	*ptr++ = 0;

	dprintf(INFO, "booting linux @ %p, ramdisk @ %p (%d)\n",
		kernel, ramdisk, ramdisk_size);
	if (cmd)
		dprintf(INFO, "cmdline: %s\n", cmd);

	enter_critical_section();
	//platform_uninit_timer();
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
		cmdline = target_get_cmdline();
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
	struct boot_img_hdr hdr;
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
	printf("ENTERING FASTBOOT MODE\n");
	
	fastboot_register("flash:", cmd_flash);
	fastboot_register("erase:", cmd_erase);

	fastboot_register("boot", cmd_boot);
	fastboot_register("continue", cmd_continue);
	fastboot_register("reboot", cmd_reboot);
	fastboot_register("reboot-bootloader", cmd_reboot_bootloader);
	fastboot_publish("product", TARGET(BOARD));
	fastboot_publish("kernel", "lk");

	fastboot_init(target_get_scratch_address(), target_get_scratch_size());
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
	printf("ENTERING RECOVERY MODE\n");
	
	boot_into_recovery = 1;
	boot_nand();
}

static void dump_ramconsole(void) {
	for (char* c = 0x8e0000; c != 0x900000; c++) {
		fbcon_putc(*c);
	}
}

void aboot_init(const struct app_descriptor *app)
{
	page_size = flash_page_size();
	page_mask = page_size - 1;
	
	if (!page_size) {
		page_size = 4096;
		page_mask = page_size - 1;
	}

	enum boot_reason bootreason = get_boot_reason();
	switch (bootreason) {
		case BOOT_FASTBOOT:
			enter_fastboot();
			return;
		case BOOT_RECOVERY:
			boot_recovery();
			return;
		default:
			break;
	}

	printf("press POWER for FASTBOOT or halfpress CAMERA for RECOVERY modes\n");
	for (int tmout = 0; tmout < 500; tmout += 50) 
	{
		int8_t pwr = keys_get_state(KEY_POWER);
		int8_t cmr = keys_get_state(KEY_UP);
		
		if (pwr)
		{	
			enter_fastboot();
			return;
		}
		else if (cmr)
		{
			boot_recovery();
			return;
		}
		thread_sleep(50);
	}
	printf("no user choise, defaulting to nand boot\n");
	boot_nand();
}

APP_START(aboot)
	.init = aboot_init,
APP_END
