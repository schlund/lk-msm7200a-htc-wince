/*
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

#ifndef __DYNBOARD_H__
#define __DYNBOARD_H__

struct msm7k_board {
	void (*early_init)(void);
	void (*init)(void);
	void (*exit)(void);
	void (*wait_for_charge)(void);
	void *scratch_addr;
	unsigned scratch_size;
	char *cmdline;
};

#endif //__DYNBOARD_H__
