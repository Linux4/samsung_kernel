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

#include <runtime_test_runner.h>
#include "clock_runtime_tests.h"
#include "pm_state_runtime_tests.h"

int run_tests_gpex_clock()
{
	int clk = 403000;
	test_gpex_clock_init();
	test_gpex_clock_term();
	test_gpex_clock_prepare_runtime_off();
	test_gpex_clock_set(clk);
	test_gpex_clock_lock_clock();
	test_gpex_clock_get_clock_slow();
	test_gpex_clock_get_table_idx(clk);
	test_gpex_clock_get_table_idx_cur();
	test_gpex_clock_get_boot_clock();
	test_gpex_clock_get_max_clock();
	test_gpex_clock_get_max_clock_limit();
	test_gpex_clock_get_min_clock();
	test_gpex_clock_get_cur_clock();
	test_gpex_clock_get_min_lock();
	test_gpex_clock_get_max_lock();
	test_gpex_clock_mutex_lock();
	test_gpex_clock_mutex_unlock();
	test_gpex_clock_get_voltage(clk);
	return 0;
}

int run_tests_gpex_pm_state()
{
	if (test_gpex_pm_state_init())
		return 1;

	if (test_gpex_pm_state_set_bad_state())
		return 1;

	if (test_gpex_pm_state_set_good_state())
		return 1;

	if (test_gpex_pm_state_term())
		return 1;

	return 0;
}
