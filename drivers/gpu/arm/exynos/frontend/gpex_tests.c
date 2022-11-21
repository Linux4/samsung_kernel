// SPDX-License-Identifier: GPL-2.0

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

#include <gpex_utils.h>
#include <runtime_test_runner.h>
#include <gpex_tests.h>

static int test_result = 2;

static ssize_t show_runtime_tests_result(char *buf)
{
	ssize_t ret = 0;

	switch (test_result) {
	case 0:
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "PASS\n");
		break;
	case 1:
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "FAIL\n");
		break;
	case 2:
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "NOT_RUN\n");
		break;
	default:
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "INVALID_RESULT\n");
	}

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	} else {
		buf[PAGE_SIZE - 2] = '\n';
		buf[PAGE_SIZE - 1] = '\0';
		ret = PAGE_SIZE - 1;
	}

	return ret;
}
CREATE_SYSFS_DEVICE_READ_FUNCTION(show_runtime_tests_result)
CREATE_SYSFS_KOBJECT_READ_FUNCTION(show_runtime_tests_result)

static ssize_t run_runtime_tests(const char *buf, size_t count)
{
	test_result = 0;

	GPU_LOG(MALI_EXYNOS_INFO, "gpex_test: run run_tests_gpex_clock test");
	if (run_tests_gpex_clock())
		test_result |= 1;
	else
		test_result |= 0;
	GPU_LOG(MALI_EXYNOS_INFO, "gpex_test: run_tests_gpex_clock result: %s",
		(test_result & 1) ? "FAIL" : "PASS");

	GPU_LOG(MALI_EXYNOS_INFO, "gpex_test: run run_tests_gpex_pm_state test");
	if (run_tests_gpex_pm_state())
		test_result |= 1;
	else
		test_result |= 0;
	GPU_LOG(MALI_EXYNOS_INFO, "gpex_test: run_tests_gpex_pm_state result: %s",
		(test_result & 1) ? "FAIL" : "PASS");

	return count;
}
CREATE_SYSFS_DEVICE_WRITE_FUNCTION(run_runtime_tests)
CREATE_SYSFS_KOBJECT_WRITE_FUNCTION(run_runtime_tests)

int gpex_tests_init()
{
	GPEX_UTILS_SYSFS_DEVICE_FILE_ADD(test_runner, show_runtime_tests_result, run_runtime_tests);
	GPEX_UTILS_SYSFS_KOBJECT_FILE_ADD(test_runner, show_runtime_tests_result,
					  run_runtime_tests);

	return 0;
}

void gpex_tests_term()
{
	return;
}

int gpex_tests_run()
{
	return 0;
}
