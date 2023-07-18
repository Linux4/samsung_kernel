/*
 * (C) COPYRIGHT 2021 Samsung Electronics Inc. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#include <gpexbe_middle_clock_wa.h>
#include <gpex_clock.h>

#define MIDDLE_CLOCK (377000)

static int previous_clock = 0;

void gpexbe_middle_clock_wa_set(void)
{
	gpex_clock_mutex_lock();
	previous_clock = gpex_clock_get_cur_clock();
	gpex_clock_set(MIDDLE_CLOCK);
	gpex_clock_mutex_unlock();
}

void gpexbe_middle_clock_wa_restore(void)
{
	gpex_clock_mutex_lock();
	if (previous_clock > MIDDLE_CLOCK) {
		gpex_clock_set(previous_clock);
	} else {
		gpex_clock_set(MIDDLE_CLOCK);
	}
	previous_clock = 0;
	gpex_clock_mutex_unlock();
}
