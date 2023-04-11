/* SPDX-License-Identifier: GPL-2.0 */

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

#include <gpexbe_power_cycle_wa.h>

u32 gpexbe_power_cycle_wa_get_reset_ticks(void)
{
	return 0;
}

u32 gpexbe_power_cycle_wa_get_hard_stop_ticks(void)
{
	return 0;
}

void gpexbe_power_cycle_wa_set_hard_stopped(void)
{
	return;
}

bool gpexbe_power_cycle_wa_is_hard_stopped(void)
{
	return false;
}

void gpexbe_power_cycle_wa_clear_hard_stopped(void)
{
	return;
}

void gpexbe_power_cycle_wa_execute_dummy_job(struct kbase_device *kbdev, u64 cores)
{
	return;
}
