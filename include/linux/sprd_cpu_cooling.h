/*
 *  linux/include/linux/sprd_cpu_cooling.h
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 */

#ifndef __SPRD_CPU_COOLING_H__
#define __SPRD_CPU_COOLING_H__

#define MAX_CPU_STATE	8

struct cooling_state {
	int max_freq;
	int max_core;
};

struct sprd_cpu_cooling_platform_data {
	struct cooling_state cpu_state[MAX_CPU_STATE];
	int state_num;
};

#endif

