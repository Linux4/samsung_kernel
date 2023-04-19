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

/* Implements */
#include <gpexwa_wakeup_clock.h>

/* Uses */
#include <gpex_clock.h>
#include <gpex_utils.h>

/* This workaround is only used by 8835. If more devices end up using this WA, this middle clock
 * should be read from devicetree
 */
#define MIDDLE_CLOCK (455000)

void gpexwa_wakeup_clock_suspend(void)
{
	gpex_clock_set(MIDDLE_CLOCK);
	GPU_LOG(MALI_EXYNOS_DEBUG, "suspend clock(%d)", gpex_clock_get_cur_clock());
}

void gpexwa_wakeup_clock_set(void)
{
	gpex_clock_set(MIDDLE_CLOCK);
	GPU_LOG(MALI_EXYNOS_DEBUG, "set wakeup clock(%d)", gpex_clock_get_cur_clock());
}

void gpexwa_wakeup_clock_restore(void)
{
	gpex_clock_set(MIDDLE_CLOCK);
	GPU_LOG(MALI_EXYNOS_DEBUG, "set wakeup clock restore(%d)", gpex_clock_get_cur_clock());
}
