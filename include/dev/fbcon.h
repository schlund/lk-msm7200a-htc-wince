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

#ifndef __DEV_FBCON_H
#define __DEV_FBCON_H

enum fb_format {
	FB_FORMAT_RGB565 = 0,
	FB_FORMAT_RGB888 = 1,
};

enum rgb565_colors {
	RGB565_BLACK = 0,
	RGB565_WHITE = 0xffff,
	RGB565_RED = 0xf800,
	RGB565_GREEN = 0x7e0,
	RGB565_BLUE = 0x1f,
	RGB565_YELLOW = RGB565_GREEN | RGB565_RED,
	RGB565_CYAN = RGB565_GREEN | RGB565_BLUE,
	RGB565_PINK = RGB565_BLUE | RGB565_RED,
	RGB565_ORANGE = 0xfc00,
	RGB565_VIOLET = 0x8000,
};

enum fbcon_rotation {
	ROTATE_0,
	ROTATE_180,
};

struct fbcon_config {
	void *base;
	unsigned width;
	unsigned height;
	unsigned stride;
	unsigned bpp;
	unsigned format;
	enum fbcon_rotation rotation;

	void (*update_start) (void);
	int (*update_done) (void);
};

void fbcon_setup(struct fbcon_config *cfg);
void fbcon_teardown(void);
void fbcon_putc(char c);

#endif				/* __DEV_FBCON_H */
