// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "cam_clock_data_recovery.h"

struct cam_clock_data_recovery_info cdr_info;

int cam_clock_data_recovery_write_register(void __iomem *csiphybase)
{
	int i, j, k;
	int str_idx;
	int len = 0;
	int count[10] = { 0, };
	int count_idx = 0;
	int cdr_num[10][10] = { 0, };
	int final_num[10] = { 0, };

	len = strlen(cdr_info.value);

	CAM_INFO(CAM_UTIL, "[CDR_DBG] input: %s", cdr_info.value);
	sprintf(cdr_info.result, "%s\n", "");

	for (str_idx = 0; str_idx < len - 1; str_idx++)
	{
		if (count_idx > 9)
		{
			CAM_ERR(CAM_UTIL, "[CDR_DBG] input value overflow");
			return 0;
		}

		if (cdr_info.value[str_idx] != ',')
		{
			if (count[count_idx] > 9)
			{
				CAM_ERR(CAM_UTIL, "[CDR_DBG] input value overflow");
				return 0;
			}

			if (cdr_info.value[str_idx] >= 'a' && cdr_info.value[str_idx] <= 'f')
			{
				cdr_num[count_idx][count[count_idx]] = cdr_info.value[str_idx] - 'W';
				count[count_idx]++;
			}
			else if (cdr_info.value[str_idx] >= 'A' && cdr_info.value[str_idx] <= 'F')
			{
				cdr_num[count_idx][count[count_idx]] = cdr_info.value[str_idx] - '7';
				count[count_idx]++;
			}
			else if (cdr_info.value[str_idx] >= '0' && cdr_info.value[str_idx] <= '9')
			{
				cdr_num[count_idx][count[count_idx]] = cdr_info.value[str_idx] - '0';
				count[count_idx]++;
			}
			else
			{
				CAM_ERR(CAM_UTIL, "[CDR_DBG] invalid input value");
				return 0;
			}
		}
		else
		{
			count_idx++;
		}
	}

	for (i = 0; i <= count_idx; i++)
	{
		for (j = 0; j < count[i]; j++)
		{
			int temp = 1;
			for (k = count[i] - 1; k > j; k--)
				temp = temp * 16;
			final_num[i] += temp * cdr_num[i][j];
		}
	}

	for (i = 0; i < 9; i += 3) {
		cam_io_w_mb(final_num[i+1],
			csiphybase + final_num[i]);

		if (final_num[i+2])
			usleep_range(final_num[i+2], final_num[i+2] + 5);

		CAM_INFO(CAM_UTIL, "[CDR_DBG] Offset: 0x%x, Val: 0x%x Delay(us): %u",
			final_num[i],
			cam_io_r_mb(csiphybase + final_num[i]),
			final_num[i+2]);
	}
	return 1;
}

void cam_clock_data_recovery_set_value(const char* buf)
{
	cdr_info.is_requested = 1;
	scnprintf(cdr_info.value, sizeof(cdr_info.value), "%s", buf);
}

char* cam_clock_data_recovery_get_value(void)
{
	return cdr_info.value;
}

void cam_clock_data_recovery_set_result(enum cam_clock_data_recovery_error error_type)
{
	cam_clock_data_recovery_get_timestamp(CDR_END_TS);
	sprintf(cdr_info.result, "%d,%lld\n", error_type, cdr_info.timestamp[CDR_END_TS]-cdr_info.timestamp[CDR_START_TS]);
	CAM_INFO(CAM_UTIL, "[CDR_DBG] mipi_overflow, time(ms): %llu", cdr_info.timestamp[CDR_END_TS]-cdr_info.timestamp[CDR_START_TS]);
}

char* cam_clock_data_recovery_get_result(void)
{
	return cdr_info.result;
}

int cam_clock_data_recovery_is_requested(void)
{
	return cdr_info.is_requested;
}

void cam_clock_data_recovery_reset_request(void)
{
	cdr_info.is_requested = 0;
}

void cam_clock_data_recovery_get_timestamp(enum cam_clock_data_recovery_timestamp type)
{
	cdr_info.timestamp[type] = ktime_get();
	cdr_info.timestamp[type] = cdr_info.timestamp[type] / 1000 / 1000;
}