/*
* Copyright (c) 2008, Google Inc.
* All rights reserved.
*
* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*  * Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

#include <debug.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <dev/fbcon.h>
#include <kernel/thread.h>

#include "font_8x8.h"

struct pos {
	int x;
	int y;
};

static void fbcon_draw_glyph_0(unsigned char c);
static void fbcon_scroll_up_0(void);


static struct fbcon_private {
	struct fbcon_config *config;
	void (*draw_glyph)(unsigned char c);
	void (*scroll_up)(void);
	uint16_t bg_color;
	uint16_t fg_color;
	struct pos position;
	struct pos max_position;
} fbcon = {
	.config = NULL,
	.draw_glyph = fbcon_draw_glyph_0,
	.scroll_up = fbcon_scroll_up_0,
};

static void fbcon_flush(void)
{
	if (fbcon.config->update_start)
		fbcon.config->update_start();
	if (fbcon.config->update_done)
		while (!fbcon.config->update_done()) ;
}

static void fbcon_draw_glyph_180(unsigned char c)
{
	uint16_t *pixels =
	fbcon.config->base +
	fbcon.config->width * fbcon.config->height * (fbcon.config->bpp >> 3);
	pixels -= fbcon.position.y * FONT_HEIGHT * fbcon.config->width;
	pixels -= (fbcon.position.x * (FONT_WIDTH + 1) + 1);
	unsigned stride = fbcon.config->stride;
	stride -= FONT_WIDTH;

	int x, y;

	for (y = 0; y < FONT_HEIGHT; y++) {
		for (x = FONT_WIDTH - 1; x >= 0; x--) {
			if (fontdata_8x8[(c << 3) + y] & (1 << x))
				*pixels = fbcon.fg_color;
			pixels--;
		}
		pixels -= stride;
	}
}

static void fbcon_draw_glyph_0(unsigned char c) {
	uint16_t *pixels = fbcon.config->base;
	pixels += fbcon.position.y * FONT_HEIGHT * fbcon.config->width;
	pixels += (fbcon.position.x * (FONT_WIDTH + 1) + 1);
	unsigned stride = fbcon.config->stride;
	stride -= FONT_WIDTH;

	int x, y;

	for (y = 0; y < FONT_HEIGHT; y++) {
		for (x = FONT_WIDTH - 1; x >= 0; x--) {
			if (fontdata_8x8[(c << 3) + y] & (1 << x))
				*pixels = fbcon.fg_color;
			pixels++;
		}
		pixels += stride;
	}
}

/* TODO: Take optimize copying by using memmove or assembly */
static void fbcon_scroll_up_180(void)
{
	unsigned short *dst =
	fbcon.config->base +
	fbcon.config->width * fbcon.config->height * (fbcon.config->bpp >> 3);
	unsigned short *src = dst - (fbcon.config->width * FONT_HEIGHT);
	unsigned count = fbcon.config->width * (fbcon.config->height - FONT_HEIGHT);
	while (count--) {
		*dst-- = *src--;
	}

	dst = fbcon.config->base + fbcon.config->width * (fbcon.config->bpp >> 3);
	while (dst > (unsigned short *)fbcon.config->base) {
		*dst-- = fbcon.bg_color;
	}
}

static void fbcon_scroll_up_0(void) {
	unsigned short *dst = fbcon.config->base;
	unsigned short *src = dst + (fbcon.config->width * FONT_HEIGHT);
	unsigned count = fbcon.config->width * (fbcon.config->height - FONT_HEIGHT);

	while(count--) {
		*dst++ = *src++;
	}

	count = fbcon.config->width * FONT_HEIGHT;
	while(count--) {
		*dst++ = fbcon.bg_color;
	}
}

/* TODO: take stride into account */
void fbcon_clear(void)
{
	enter_critical_section();
	unsigned short *start = fbcon.config->base;
	unsigned short *end =
	fbcon.config->base +
	fbcon.config->width * fbcon.config->height * (fbcon.config->bpp >> 3);
	while (start < end) {
		*start++ = fbcon.bg_color;
	}
	fbcon_flush();
	fbcon.position.x = fbcon.position.y = 0;
	exit_critical_section();
}

void fbcon_putc(char c)
{
	/* ignore anything that happens before fbcon is initialized */
	if (!fbcon.config)
		return;

	if ((unsigned char)c > 127)
		return;
	if ((unsigned char)c < 32) {
		if (c == '\n')
			goto newline;
		else if (c == '\r')
			fbcon.position.x = 0;
		return;
	}
	c -= 32;

	if (fbcon.config->rotation != ROTATE_0 && fbcon.draw_glyph)
		fbcon.draw_glyph(c);
	else
		fbcon_draw_glyph_0(c);

	fbcon.position.x++;
	if (fbcon.position.x < fbcon.max_position.x)
		return;

newline:
	fbcon.position.y++;
	fbcon.position.x = 0;
	if (fbcon.position.y >= fbcon.max_position.y) {
		fbcon.position.y = fbcon.max_position.y - 1;
		if (fbcon.config->rotation != ROTATE_0 && fbcon.scroll_up)
			fbcon.scroll_up();
		else
			fbcon_scroll_up_0();
	}
	fbcon_flush();
}

#if DISPLAY_SPLASH_SCREEN
static void diplay_image_on_screen(void)
{
	fbcon_clear();
	fbcon_flush();
}
#endif

void fbcon_setup(struct fbcon_config *_config)
{
	ASSERT(_config);
	if (fbcon.config)
		return;

	fbcon.config = _config;

	switch (fbcon.config->format) {
	case FB_FORMAT_RGB565:
		fbcon.fg_color = RGB565_GREEN;
		fbcon.bg_color = RGB565_BLACK;
		break;

	default:
		dprintf(CRITICAL, "unknown framebuffer pixel format\n");
		ASSERT(0);
		break;
	}

	if (fbcon.config->rotation == ROTATE_180) {
		fbcon.scroll_up = fbcon_scroll_up_180;
		fbcon.draw_glyph = fbcon_draw_glyph_180;
	}

	fbcon.position.x = 0;
	fbcon.position.y = 0;
	fbcon.max_position.x = fbcon.config->width / (FONT_WIDTH + 1);
	fbcon.max_position.y = (fbcon.config->height - 1) / FONT_HEIGHT;
	fbcon_clear();
}

void fbcon_teardown(void) {
	enter_critical_section();
	fbcon.config = NULL;
	exit_critical_section();
}
