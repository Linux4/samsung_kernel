// SPDX-License-Identifier: GPL-2.0
/*
 * Synaptics TouchCom touchscreen driver
 *
 * Copyright (C) 2017-2020 Synaptics Incorporated. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

/**
 * @file syna_tcm2_testing.c
 *
 * This file implements the sample code to perform chip testing.
 */
#include "syna_tcm2_testing.h"
#include "syna_tcm2_testing_limits.h"
#include "synaptics_touchcom_core_dev.h"
#include "synaptics_touchcom_func_base.h"


/**
 * List the custom production test items
 */
enum test_code {
	TEST_PID22_TRANS_RAW_CAP = 0x16,
	TEST_PID25_TRANS_RX_SHORT = 0x19,
	TEST_PID30_BSC_CALIB_TEST = 0x1E,
	TEST_PID52_JITTER_DELTA_TEST = 0x34,
	TEST_PID53_SRAM_TEST = 0x35,
	TEST_PID54_BSC_DIFF_TEST = 0x36,
	TEST_PID56_TSP_SNR_NONTOUCH = 0x38,
	TEST_PID57_TSP_SNR_TOUCH = 0x39,
};

/* g_testing_dir represents the root folder of testing sysfs
 */
static struct kobject *g_testing_dir;
static struct syna_tcm *g_tcm_ptr;


#define SEC_NVM_CALIB_DATA_SIZE (28)

/**
 * syna_testing_compare_byte_vector()
 *
 * Sample code to compare the test result with limits
 * by byte vector
 *
 * @param
 *    [ in] data: target test data
 *    [ in] data_size: size of test data
 *    [ in] limit: test limit value to be compared with
 *    [ in] limit_size: size of test limit
 *
 * @return
 *    on success, true; otherwise, return false
 */
static bool syna_testing_compare_byte_vector(unsigned char *data,
		unsigned int data_size, const unsigned char *limit,
		unsigned int limit_size)
{
	bool result = false;
	unsigned char tmp;
	unsigned char p, l;
	int i, j;

	if (!data || (data_size == 0)) {
		LOGE("Invalid test data\n");
		return false;
	}
	if (!limit || (limit_size == 0)) {
		LOGE("Invalid limits\n");
		return false;
	}

	if (limit_size < data_size) {
		LOGE("Limit size mismatched, data size: %d, limits: %d\n",
			data_size, limit_size);
		return false;
	}

	result = true;
	for (i = 0; i < data_size; i++) {
		tmp = data[i];

		for (j = 0; j < 8; j++) {
			p = GET_BIT(tmp, j);
			l = GET_BIT(limit[i], j);
			if (p != l) {
				LOGE("Fail on TRX-%03d (data:%X, limit:%X)\n",
					(i*8 + j), p, l);
				result = false;
			}
		}
	}

	return result;
}

/**
 * syna_testing_compare_frame()
 *
 * Sample code to compare the test result with limits
 * by a lower-bound frame
 *
 * @param
 *    [ in] data: target test data
 *    [ in] data_size: size of test data
 *    [ in] rows: the number of rows
 *    [ in] cols: the number of column
 *    [ in] limits_hi: upper-bound test limit
 *    [ in] limits_lo: lower-bound test limit
 *
 * @return
 *    on success, true; otherwise, return false
 */
static bool syna_testing_compare_frame(unsigned char *data,
		unsigned int data_size, int rows, int cols,
		const short *limits_hi, const short *limits_lo)
{
	bool result = false;
	short *data_ptr = NULL;
	short limit;
	int i, j;

	if (!data || (data_size == 0)) {
		LOGE("Invalid test data\n");
		return false;
	}

	if (data_size < (2 * rows * cols)) {
		LOGE("Size mismatched, data:%d (exppected:%d)\n",
			data_size, (2 * rows * cols));
		result = false;
		return false;
	}

	if (LIMIT_BOUNDARY < rows) {
		LOGE("Rows mismatched, rows:%d (exppected:%d)\n",
			rows, LIMIT_BOUNDARY);
		result = false;
		return false;
	}

	if (LIMIT_BOUNDARY < cols) {
		LOGE("Columns mismatched, cols: %d (exppected:%d)\n",
			cols, LIMIT_BOUNDARY);
		result = false;
		return false;
	}

	result = true;

	if (!limits_hi)
		goto end_of_upper_bound_limit;

	data_ptr = (short *)&data[0];
	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			limit = limits_hi[i * LIMIT_BOUNDARY + j];
			if (*data_ptr > limit) {
				LOGE("Fail on (%2d,%2d)=%5d, limits_hi:%4d\n",
					i, j, *data_ptr, limit);
				result = false;
			}
			data_ptr++;
		}
	}

end_of_upper_bound_limit:

	if (!limits_lo)
		goto end_of_lower_bound_limit;

	data_ptr = (short *)&data[0];
	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			limit = limits_lo[i * LIMIT_BOUNDARY + j];
			if (*data_ptr < limit) {
				LOGE("Fail on (%2d,%2d)=%5d, limits_lo:%4d\n",
					i, j, *data_ptr, limit);
				result = false;
			}
			data_ptr++;
		}
	}

end_of_lower_bound_limit:
	return result;
}

/**
 * syna_testing_device_id()
 *
 * Sample code to ensure the device id is expected
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_device_id(struct syna_tcm *tcm)
{
	int retval;
	bool result;
	struct tcm_identification_info info;
	char *strptr = NULL;

	LOGI("Start testing\n");

	retval = syna_tcm_identify(tcm->tcm_dev, &info);
	if (retval < 0) {
		LOGE("Fail to get identification\n");
		result = false;
		goto exit;
	}

	strptr = strnstr(info.part_number,
					device_id_limit,
					strlen(info.part_number));
	if (strptr != NULL)
		result = true;
	else {
		LOGE("Device ID mismatched, FW: %s (limit: %s)\n",
			info.part_number, device_id_limit);
		result = false;
	}

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_config_id()
 *
 * Sample code to ensure the config id is expected
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_config_id(struct syna_tcm *tcm)
{
	int retval;
	bool result;
	struct tcm_application_info info;
	int idx;

	LOGI("Start testing\n");

	retval = syna_tcm_get_app_info(tcm->tcm_dev, &info);
	if (retval < 0) {
		LOGE("Fail to get app info\n");
		result = false;
		goto exit;
	}

	result = true;
	for (idx = 0; idx < sizeof(config_id_limit); idx++) {
		if (config_id_limit[idx] != info.customer_config_id[idx]) {
			LOGE("Fail on byte.%d (data: %02X, limit: %02X)\n",
				idx, info.customer_config_id[idx],
				config_id_limit[idx]);
			result = false;
		}
	}

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_check_id_show()
 *
 * Attribute to show the result of ID comparsion to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_check_id_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		retval = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	count = 0;

	retval = syna_testing_device_id(tcm);

	retval = snprintf(buf, PAGE_SIZE - count,
			"Device ID check: %s\n",
			(retval < 0) ? "fail" : "pass");

	buf += retval;
	count += retval;

	retval = syna_testing_config_id(tcm);

	retval = snprintf(buf, PAGE_SIZE - count,
			"Config ID check: %s\n",
			(retval < 0) ? "fail" : "pass");

	buf += retval;
	count += retval;

	retval = count;
exit:
	return retval;
}

static struct kobj_attribute kobj_attr_check_id =
	__ATTR(check_id, 0444, syna_testing_check_id_show, NULL);

/**
 * syna_testing_pt01()
 *
 * Sample code to perform PT01 testing
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt01(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID01_TRX_TRX_SHORTS,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n", TEST_PID01_TRX_TRX_SHORTS);
		result = false;
		goto exit;
	}

	result = syna_testing_compare_byte_vector(test_data.buf,
			test_data.data_length,
			pt01_limits,
			ARRAY_SIZE(pt01_limits));

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_pt01_show()
 *
 * Attribute to show the result of PT01 test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt01_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt01(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST PT$01: %s\n",
			(retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_pt01 =
	__ATTR(pt01, 0444, syna_testing_pt01_show, NULL);

/**
 * syna_testing_pt05()
 *
 * Sample code to perform PT05 testing
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt05(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID05_FULL_RAW_CAP,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n", TEST_PID05_FULL_RAW_CAP);
		result = false;
		goto exit;
	}

	result = syna_testing_compare_frame(test_data.buf,
			test_data.data_length,
			tcm->tcm_dev->rows,
			tcm->tcm_dev->cols,
			(const short *)&pt05_hi_limits[0],
			(const short *)&pt05_lo_limits[0]);

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_pt05_show()
 *
 * Attribute to show the result of PT05 test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt05_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt05(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST PT$05: %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_pt05 =
	__ATTR(pt05, 0444, syna_testing_pt05_show, NULL);

/**
 * syna_testing_pt0a()
 *
 * Sample code to perform PT0A testing
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt0a(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID10_DELTA_NOISE,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n", TEST_PID10_DELTA_NOISE);
		result = false;
		goto exit;
	}

	result = syna_testing_compare_frame(test_data.buf,
			test_data.data_length,
			tcm->tcm_dev->rows,
			tcm->tcm_dev->cols,
			(const short *)&pt0a_hi_limits[0],
			(const short *)&pt0a_lo_limits[0]);

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_pt0a_show()
 *
 * Attribute to show the result of PT0A test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt0a_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt0a(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST PT$0A: %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_pt0a =
	__ATTR(pt0a, 0444, syna_testing_pt0a_show, NULL);

/**
 * syna_testing_pt11()
 *
 * Sample code to perform PT11 testing
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt11(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID17_ADC_RANGE,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n", TEST_PID17_ADC_RANGE);
		result = false;
		goto exit;
	}

	result = syna_testing_compare_frame(test_data.buf,
			test_data.data_length,
			tcm->tcm_dev->rows,
			tcm->tcm_dev->cols,
			(const short *)&pt11_hi_limits[0],
			(const short *)&pt11_lo_limits[0]);

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_pt11_show()
 *
 * Attribute to show the result of PT11 test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt11_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt11(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST PT$11: %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_pt11 =
	__ATTR(pt11, 0444, syna_testing_pt11_show, NULL);

/**
 * syna_testing_pt12()
 *
 * Sample code to perform PT12 testing
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt12(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;
	int *data_ptr = NULL;
	int rows = tcm->tcm_dev->rows;
	int cols = tcm->tcm_dev->cols;
	int i;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID18_HYBRID_ABS_RAW,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n", TEST_PID18_HYBRID_ABS_RAW);
		result = false;
		goto exit;
	}

	if ((LIMIT_BOUNDARY < rows) || (LIMIT_BOUNDARY < cols)) {
		LOGE("Size mismatched, rows:%d cols:%d\n",
			rows, cols);
		result = false;
		goto exit;
	}

	if (test_data.data_length < 4 * (rows + cols)) {
		LOGE("Data mismatched, size:%d (expected:%d)\n",
			test_data.data_length, 4 * (rows + cols));
		result = false;
		goto exit;
	}

	result = true;

	data_ptr = (int *)&test_data.buf[0];
	for (i = 0; i < cols; i++) {
		if (*data_ptr > pt12_limits[i]) {
			LOGE("Fail on Col-%2d=%5d, limits:%4d\n",
				i, *data_ptr, pt12_limits[i]);

			result = false;
		}
		data_ptr++;
	}

	for (i = 0; i < rows; i++) {
		if (*data_ptr > pt12_limits[LIMIT_BOUNDARY + i]) {
			LOGE("Fail on Row-%2d=%5d, limits:%4d\n",
				i, *data_ptr,
				pt12_limits[LIMIT_BOUNDARY + i]);

			result = false;
		}
		data_ptr++;
	}

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_pt12_show()
 *
 * Attribute to show the result of PT12 test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt12_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt12(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST PT$12: %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_pt12 =
	__ATTR(pt12, 0444, syna_testing_pt12_show, NULL);

/**
 * syna_testing_pt16()
 *
 * Sample code to perform PT16 testing
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt16(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID22_TRANS_RAW_CAP,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n", TEST_PID22_TRANS_RAW_CAP);
		result = false;
		goto exit;
	}

	result = syna_testing_compare_frame(test_data.buf,
			test_data.data_length,
			tcm->tcm_dev->rows,
			tcm->tcm_dev->cols,
			(const short *)&pt16_hi_limits[0],
			(const short *)&pt16_lo_limits[0]);

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_pt16_show()
 *
 * Attribute to show the result of PT16 test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt16_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt16(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST PT$16: %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_pt16 =
	__ATTR(pt16, 0444, syna_testing_pt16_show, NULL);

/**
 * syna_testing_pt19()
 *
 * Sample code to perform PT19 testing
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt19(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;
	unsigned short *data_ptr = NULL;
	int rows = tcm->tcm_dev->rows;
	int cols = tcm->tcm_dev->cols;
	int i, offset;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID25_TRANS_RX_SHORT,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n", TEST_PID25_TRANS_RX_SHORT);
		result = false;
		goto exit;
	}

	if ((LIMIT_BOUNDARY < rows) || (LIMIT_BOUNDARY < cols)) {
		LOGE("Size mismatched, rows:%d cols:%d\n",
			rows, cols);
		result = false;
		goto exit;
	}

	if (test_data.data_length < 2 * (rows + cols)) {
		LOGE("Data mismatched, size:%d (expected:%d)\n",
			test_data.data_length, 2 * (rows + cols));
		result = false;
		goto exit;
	}

	result = true;

	data_ptr = (unsigned short *)&test_data.buf[0];
	for (i = 0; i < cols; i++) {
		if ((*data_ptr > pt19_hi_limits[i]) ||
			(*data_ptr < pt19_lo_limits[i])) {
			LOGE("Fail on Col-%2d=%5d, limits:%4d - %4d\n",
				i, *data_ptr, pt19_lo_limits[i],
				pt19_hi_limits[i]);

			result = false;
		}
		data_ptr++;
	}

	for (i = 0; i < rows; i++) {
		offset = LIMIT_BOUNDARY + i;
		if ((*data_ptr > pt19_hi_limits[offset]) ||
			(*data_ptr < pt19_lo_limits[offset])) {
			LOGE("Fail on Row-%2d=%5d, limits:%4d - %4d\n",
				i, *data_ptr, pt19_lo_limits[offset],
				pt19_hi_limits[offset]);

			result = false;
		}
		data_ptr++;
	}

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_pt19_show()
 *
 * Attribute to show the result of PT19 test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt19_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt19(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST PT$19: %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_pt19 =
	__ATTR(pt19, 0444, syna_testing_pt19_show, NULL);

/**
 * syna_testing_pt1d()
 *
 * Sample code to perform PT1D testing
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt1d(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;
	short *data_ptr = NULL;
	int rows = tcm->tcm_dev->rows;
	int cols = tcm->tcm_dev->cols;
	int i, offset;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID29_HYBRID_ABS_NOISE,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n", TEST_PID29_HYBRID_ABS_NOISE);
		result = false;
		goto exit;
	}

	if ((LIMIT_BOUNDARY < rows) || (LIMIT_BOUNDARY < cols)) {
		LOGE("Size mismatched, rows:%d cols:%d\n",
			rows, cols);
		result = false;
		goto exit;
	}

	if (test_data.data_length < 2 * (rows + cols)) {
		LOGE("Data mismatched, size:%d (expected:%d)\n",
			test_data.data_length, 2 * (rows + cols));
		result = false;
		goto exit;
	}

	result = true;

	data_ptr = (short *)&test_data.buf[0];
	for (i = 0; i < cols; i++) {
		if (*data_ptr > pt1d_limits[i]) {
			LOGE("Fail on Col-%2d=%5d, limits:%4d\n",
				i, *data_ptr, pt1d_limits[i]);

			result = false;
		}
		data_ptr++;
	}

	for (i = 0; i < rows; i++) {
		offset = LIMIT_BOUNDARY + i;
		if (*data_ptr > pt1d_limits[offset]) {
			LOGE("Fail on Row-%2d=%5d, limits:%4d\n",
				i, *data_ptr, pt1d_limits[offset]);

			result = false;
		}
		data_ptr++;
	}

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_pt1d_show()
 *
 * Attribute to show the result of PT1D test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt1d_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt1d(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST PT$1D: %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_pt1d =
	__ATTR(pt1d, 0444, syna_testing_pt1d_show, NULL);

/**
 * syna_testing_pt1e()
 *
 * Sample code to perform BSC Calib (PT1E) testing
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt1e(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;
	unsigned short *data_ptr = NULL;
	unsigned char parameter;
	unsigned char resp_code;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	parameter = 3;
	retval = syna_tcm_send_command(tcm->tcm_dev,
			CMD_BSC_DO_CALIBRATION,
			&parameter,
			1,
			&resp_code,
			NULL,
			0);
	if ((retval < 0) || resp_code != STATUS_OK) {
		LOGE("Fail to erase bsc data\n");
		result = false;
		goto exit;
	}

	parameter = 2;
	retval = syna_tcm_send_command(tcm->tcm_dev,
			CMD_BSC_DO_CALIBRATION,
			&parameter,
			1,
			&resp_code,
			NULL,
			0);
	if ((retval < 0) || resp_code != STATUS_OK) {
		LOGE("Fail to calibrate bsc\n");
		result = false;
		goto exit;
	}

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID30_BSC_CALIB_TEST,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n",
		TEST_PID30_BSC_CALIB_TEST);
		result = false;
		goto exit;
	}

	data_ptr = (unsigned short *)&test_data.buf[0];
	if (*data_ptr == 1)
		result = true;

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_pt36()
 *
 * Sample code to perform BSC DIFF (PT36) testing
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt36(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;
	unsigned short *data_ptr = NULL;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID54_BSC_DIFF_TEST,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n",
		TEST_PID54_BSC_DIFF_TEST);
		result = false;
		goto exit;
	}

	data_ptr = (unsigned short *)&test_data.buf[0];
	if (*data_ptr == 1)
		result = true;

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_bsc_show()
 *
 * Attribute to show the result of BSC test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_bsc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt1e(tcm);

	retval = snprintf(buf, PAGE_SIZE,
			"TEST BSC Calib. (PT$1E): %s\n", (retval < 0) ? "fail" : "pass");

	count += retval;

exit:
	return count;
}

static struct kobj_attribute kobj_attr_bsc =
	__ATTR(bsc, 0444, syna_testing_bsc_show, NULL);

/**
 * syna_testing_misc_calib_show()
 *
 * Attribute to show the result of Misc. Calib test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_misc_calib_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt36(tcm);

	retval = snprintf(buf + count, PAGE_SIZE,
			"TEST Misc. Calib. (PT$36): %s\n", (retval < 0) ? "fail" : "pass");
	count += retval;

exit:
	return count;
}

static struct kobj_attribute kobj_attr_misc_calib =
	__ATTR(misc_calib, 0444, syna_testing_misc_calib_show, NULL);

/**
 * syna_testing_pt05_gap()
 *
 * Sample code to implement GAP test based on PT05
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_pt05_gap(struct syna_tcm *tcm)
{
	int retval;
	bool result = false;
	struct tcm_buffer test_data;
	int rows = tcm->tcm_dev->rows;
	int cols = tcm->tcm_dev->cols;
	short *data_ptr = NULL;
	short *frame = NULL;
	short *gap_frame = NULL;
	int i, j;
	int idx;
	short val_1, val_2;

	syna_tcm_buf_init(&test_data);

	LOGI("Start testing\n");

	frame = syna_pal_mem_alloc(rows * cols, sizeof(short));
	if (!frame) {
		LOGE("Fail to allocate image for gap test\n");
		result = false;
		frame = NULL;
		goto exit;
	}
	gap_frame = syna_pal_mem_alloc((rows - 1) * cols,
			sizeof(short));
	if (!gap_frame) {
		LOGE("Fail to allocate gap image for gap test\n");
		result = false;
		gap_frame = NULL;
		goto exit;
	}

	retval = syna_tcm_run_production_test(tcm->tcm_dev,
			TEST_PID05_FULL_RAW_CAP,
			&test_data);
	if (retval < 0) {
		LOGE("Fail to run test %d\n", TEST_PID05_FULL_RAW_CAP);
		result = false;
		goto exit;
	}

	data_ptr = (short *)&test_data.buf[0];
	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			frame[i * cols + j] = *data_ptr;
			data_ptr++;
		}
	}

	idx = 0;
	for (i = 1; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			val_1 = frame[i * cols + j];
			val_2 = frame[(i-1) * cols + j];
			gap_frame[idx] = (abs(val_1 - val_2)) * 100 / val_2;
			idx += 1;
		}
	}

	for (i = 0; i < rows - 1; i++) {
		for (j = 0; j < cols; j++) {
			idx = i * cols + j;
			if (gap_frame[idx] > pt05_gap_limits[idx]) {
				LOGE("Fail on (%2d,%2d)=%5d, max:%4d\n",
					i, j, gap_frame[idx], pt05_gap_limits[idx]);
				result = false;
			}
		}
	}

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	if (gap_frame)
		syna_pal_mem_free(gap_frame);

	if (frame)
		syna_pal_mem_free(frame);

	syna_tcm_buf_release(&test_data);

	return ((result) ? 0 : -1);
}

/**
 * syna_testing_pt05_gap_show()
 *
 * Attribute to show the result of PT1E test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_pt05_gap_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_pt05_gap(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST GAP : %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_gap =
	__ATTR(gap, 0444, syna_testing_pt05_gap_show, NULL);


/**
 * syna_testing_sram()
 *
 * Sample code to implement SRAM test based on PT35
 *
 * Please be noted a self-reset will occur during the process.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_sram(struct syna_tcm *tcm)
{
	int retval;
	struct syna_hw_interface *hw = tcm->hw_if;
	bool result = false;
	unsigned char *buf = NULL;
	unsigned char command[4] = {CMD_PRODUCTION_TEST,
		0x01, 0x00, TEST_PID53_SRAM_TEST};

	if (!hw->ops_write_data || !hw->ops_read_data){
		LOGE("No raw access helpers\n");
		return -1;
	}

	buf = syna_pal_mem_alloc(2,
		sizeof(struct tcm_identification_info));
	if (!buf) {
		LOGE("Fail to create a buffer for test\n");
		return -1;
	}

	LOGI("Start testing\n");

	/* disable the interrupt always, otherwise the result
	 * will be read by ISR
	 */
	if (hw->ops_enable_irq)
		hw->ops_enable_irq(hw, false);

	/* send the test command through raw access helpers
	 * because ASIC will reboot and response is not available
	 */
	retval = hw->ops_write_data(hw,
		command,
		sizeof(command));
	if (retval < 0) {
		LOGE("Fail to send test command\n");
		result = false;
		goto exit;
	}

	syna_pal_sleep_ms(500);

	retval = hw->ops_read_data(hw,
		buf,
		sizeof(struct tcm_identification_info) + MESSAGE_HEADER_SIZE);
	if (retval < 0) {
		LOGE("Fail to read the test result\n");
		result = false;
		goto exit;
	}

	if (buf[0] != 0xa5) {
		LOGE("Incorrect touchcomm maker\n");
		result = false;
		goto exit;
	}

	result = true;

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	/* recovery the interrupt */
	if (hw->ops_enable_irq)
		hw->ops_enable_irq(hw, true);

	if (buf)
		syna_pal_mem_free(buf);

	return ((result) ? 0 : -1);
}
/**
 * syna_testing_sram_show()
 *
 * Attribute to show the result of SRAM test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_sram_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_sram(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST SRAM : %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_sram =
	__ATTR(sram, 0444, syna_testing_sram_show, NULL);


/**
 * syna_testing_jitter_delta()
 *
 * Sample code to implement Jitter Delta test based on PT34
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_jitter_delta(struct syna_tcm *tcm)
{
	int retval;
	struct syna_hw_interface *hw = tcm->hw_if;
	bool result = false;
	unsigned char buf[32] = { 0 };
	unsigned time = 0;
	unsigned timeout = 30;
	short min[2] = { 0 };
	short max[2] = { 0 };
	short avg[2] = { 0 };
	const int IDX_MIN = 1;
	const int IDX_MAX = 0;
	unsigned char command[4] = {CMD_PRODUCTION_TEST,
		0x01, 0x00, TEST_PID52_JITTER_DELTA_TEST};

	if (!hw->ops_write_data || !hw->ops_read_data){
		LOGE("No raw access helpers\n");
		return -1;
	}

	LOGI("Start testing\n");

	/* disable the interrupt always, otherwise the result
	 * will be read by ISR
	 */
	if (hw->ops_enable_irq)
		hw->ops_enable_irq(hw, false);

	/* send the test command through raw access helpers
	 * because this test could take a longer execution time
	 */
	retval = hw->ops_write_data(hw,
		command,
		sizeof(command));
	if (retval < 0) {
		LOGE("Fail to send test command\n");
		result = false;
		goto exit;
	}

	for (time = 0; time < timeout; time++) {
		syna_pal_sleep_ms(500);

		retval = hw->ops_read_data(hw,
			buf,
			sizeof(buf));
		if (retval < 0) {
			LOGE("Fail to read the test result\n");
			result = false;
			goto exit;
		}

		if (buf[1] == STATUS_OK)
			break;
	}

	if (time >= timeout) {
		LOGE("Timed out waiting for result of PT34\n");
		result = false;
		goto exit;
	}

	min[IDX_MAX] = (short)syna_pal_le2_to_uint(&buf[MESSAGE_HEADER_SIZE + 0]);
	min[IDX_MIN] = (short)syna_pal_le2_to_uint(&buf[MESSAGE_HEADER_SIZE + 2]);

	max[IDX_MAX] = (short)syna_pal_le2_to_uint(&buf[MESSAGE_HEADER_SIZE + 4]);
	max[IDX_MIN] = (short)syna_pal_le2_to_uint(&buf[MESSAGE_HEADER_SIZE + 6]);

	avg[IDX_MAX] = (short)syna_pal_le2_to_uint(&buf[MESSAGE_HEADER_SIZE + 8]);
	avg[IDX_MIN] = (short)syna_pal_le2_to_uint(&buf[MESSAGE_HEADER_SIZE +10]);

	LOGI("MIN:%-4d - %-4d, AVG:%-4d - %-4d, MAX:%-4d - %-4d\n",
		min[IDX_MIN], min[IDX_MAX], avg[IDX_MIN], avg[IDX_MAX],
		max[IDX_MIN], max[IDX_MAX]);

	result = true;

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	/* recovery the interrupt */
	if (hw->ops_enable_irq)
		hw->ops_enable_irq(hw, true);

	return ((result) ? 0 : -1);
}
/**
 * syna_testing_jitter_delta_show()
 *
 * Attribute to show the result of Jitter Delta test to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_jitter_delta_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_jitter_delta(tcm);

	count = snprintf(buf, PAGE_SIZE,
			"TEST Jitter Delta : %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_jitter_delta =
	__ATTR(jitter_delta, 0444, syna_testing_jitter_delta_show, NULL);

/**
 * syna_testing_erase_calibration_data_show()
 *
 * Attribute to erase the calibration data
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_erase_calibration_data_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm *tcm = g_tcm_ptr;
	unsigned int count = 0;
	unsigned char payload;
	unsigned char resp_code;
	struct tcm_buffer resp;

	LOGD("start\n");

	syna_tcm_buf_init(&resp);

	payload = 0x02;

	retval = syna_tcm_send_command(tcm->tcm_dev,
			CMD_ACCESS_CALIB_DATA_FROM_NVM,
			&payload,
			sizeof(payload),
			&resp_code,
			&resp,
			0);
	if (retval < 0) {
		LOGE("Fail to erase calib data, status:%x\n",
			resp_code);
	}

	syna_tcm_buf_release(&resp);

	count = snprintf(buf, PAGE_SIZE,
			"TEST Erase Calib. Data : %s\n", (retval < 0) ? "fail" : "pass");

	return count;
}

static struct kobj_attribute kobj_attr_erase_calib =
	__ATTR(erase_calib, 0444, syna_testing_erase_calibration_data_show, NULL);

/**
 * syna_testing_write_calibration_data_show()
 *
 * Attribute to write the calibration data
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_write_calibration_data_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;
	unsigned char payload[SEC_NVM_CALIB_DATA_SIZE + 2] = { 0 };
	unsigned char calib_data[SEC_NVM_CALIB_DATA_SIZE] = { 0 };
	unsigned char resp_code;
	struct tcm_buffer resp;
	int i;

	LOGD("start\n");

	syna_tcm_buf_init(&resp);

	/* assign calib data here */
	for (i = 0; i < SEC_NVM_CALIB_DATA_SIZE; i++)
		calib_data[i] = i;

	/*
	 * assuming writting data is stored at calib_data already
	 * copy to payload buffer
	 */
	payload[0] = 0x01;
	payload[1] = SEC_NVM_CALIB_DATA_SIZE;
	for (i = 0; i < SEC_NVM_CALIB_DATA_SIZE; i++)
		payload[2 + i] = (unsigned char)calib_data[i];

	retval = syna_tcm_send_command(tcm->tcm_dev,
			CMD_ACCESS_CALIB_DATA_FROM_NVM,
			payload,
			sizeof(payload),
			&resp_code,
			&resp,
			0);
	if (retval < 0) {
		LOGE("Fail to write calib data, status:%x\n",
			resp_code);
	}

	syna_tcm_buf_release(&resp);

	count = snprintf(buf, PAGE_SIZE,
			"TEST Write Calib. Data : %s\n", (retval < 0) ? "fail" : "pass");

	return count;
}

static struct kobj_attribute kobj_attr_write_calib =
	__ATTR(write_calib, 0444, syna_testing_write_calibration_data_show, NULL);

/**
 * syna_testing_read_calibration_data_show()
 *
 * Attribute to read the calibration data
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_read_calibration_data_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm *tcm = g_tcm_ptr;
	unsigned int count = 0;
	unsigned char payload[2];
	unsigned char resp_code;
	struct tcm_buffer resp;
	int i;

	LOGD("start\n");

	syna_tcm_buf_init(&resp);

	payload[0] = 0x00;
	payload[1] = SEC_NVM_CALIB_DATA_SIZE;

	retval = syna_tcm_send_command(tcm->tcm_dev,
			CMD_ACCESS_CALIB_DATA_FROM_NVM,
			payload,
			sizeof(payload),
			&resp_code,
			&resp,
			0);
	if (retval < 0) {
		LOGE("Fail to read calib data, status:%x\n",
			resp_code);
		goto exit;
	}

	/*
	 * retrieved data is stored at resp.buf
	 * and size is equal to resp.data_length
	 */
	for (i = 0; i < resp.data_length; i++) {
		retval = snprintf(buf, PAGE_SIZE - count,
			"%02x", resp.buf[i]);

		buf += retval;
		count += retval;
	}

exit:

	syna_tcm_buf_release(&resp);

	count = snprintf(buf, PAGE_SIZE,
			"TEST Read Calib. Data : %s\n", (retval < 0) ? "fail" : "pass");

	return count;
}

static struct kobj_attribute kobj_attr_read_calib =
	__ATTR(read_calib, 0444, syna_testing_read_calibration_data_show, NULL);

/**
 * syna_testing_set_tsp_snr_frames_store()
 *
 * Attribute to set up TSP SNR test frames
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [ in] buf:   string buffer input
 *    [ in] count: size of buffer input
 *
 * @return
 *    on success, return count; otherwise, return error code
 */
static ssize_t syna_testing_set_tsp_snr_frames_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	unsigned int input;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (kstrtouint(buf, 10, &input))
		return -EINVAL;

	/* set up ear detection mode */
	retval = syna_tcm_set_dynamic_config(tcm->tcm_dev,
			DC_TSP_SNR_TEST_FRAMES,
			(unsigned short)input);
	if (retval < 0) {
		LOGE("Fail to set %d with dynamic command 0x%x\n",
			input, DC_TSP_SNR_TEST_FRAMES);
		goto exit;
	}

	retval = count;

exit:
	return retval;
}

static struct kobj_attribute kobj_attr_tsp_snr_frames =
	__ATTR(tsp_snr_frames, 0220, NULL, syna_testing_set_tsp_snr_frames_store);

/**
 * syna_testing_tsp_snr()
 *
 * Sample code to issue TSP SNR Measurement
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_testing_tsp_snr(struct syna_tcm *tcm, unsigned char pid)
{
	int retval;
	struct syna_hw_interface *hw = tcm->hw_if;
	bool result = false;
	unsigned char buf[2 * 7 * (3 * 3) + 4] = { 0 };
	unsigned time = 0;
	unsigned timeout = 30;
	unsigned char command[4] = {CMD_PRODUCTION_TEST,
		0x01, 0x00, 0x00};

	if (!hw->ops_write_data || !hw->ops_read_data){
		LOGE("No raw access helpers\n");
		return -1;
	}

	command[3] = (unsigned char) pid;

	LOGI("Start %s\n",
		(command[3] == TEST_PID57_TSP_SNR_TOUCH)? "TSP_SNR_TOUCH" : "TSP_SNR_NONTOUCH");

	/* disable the interrupt always, otherwise the result
	 * will be read by ISR
	 */
	if (hw->ops_enable_irq)
		hw->ops_enable_irq(hw, false);

	/* send the test command through raw access helpers
	 * because this test could take a longer execution time
	 */
	retval = hw->ops_write_data(hw,
		command,
		sizeof(command));
	if (retval < 0) {
		LOGE("Fail to send test command\n");
		result = false;
		goto exit;
	}

	for (time = 0; time < timeout; time++) {
		syna_pal_sleep_ms(500);

		retval = hw->ops_read_data(hw,
			buf,
			sizeof(buf));
		if (retval < 0) {
			LOGE("Fail to read the test result\n");
			result = false;
			goto exit;
		}

		if (buf[1] == STATUS_OK)
			break;
	}

	if (time >= timeout) {
		LOGE("Timed out waiting for result of PT%X\n", command[3]);
		result = false;
		goto exit;
	}

	/* result should be placed at buf[] */

	result = true;

exit:
	LOGI("Result = %s\n", (result)?"pass":"fail");

	/* recovery the interrupt */
	if (hw->ops_enable_irq)
		hw->ops_enable_irq(hw, true);

	return ((result) ? 0 : -1);
}
/**
 * syna_testing_tsp_snr_nontouch_show()
 *
 * Attribute to show the result of TSP SNR Measurement NonTouch to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_tsp_snr_nontouch_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_tsp_snr(tcm, TEST_PID56_TSP_SNR_NONTOUCH);

	count = snprintf(buf, PAGE_SIZE,
			"TEST TSP SNR NonTouch : %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_tsp_snr_nontouch =
	__ATTR(tsp_snr_nontouch, 0444, syna_testing_tsp_snr_nontouch_show, NULL);

/**
 * syna_testing_tsp_snr_touch_show()
 *
 * Attribute to show the result of TSP SNR Measurement Touch to the console.
 *
 * @param
 *    [ in] kobj:  an instance of kobj
 *    [ in] attr:  an instance of kobj attribute structure
 *    [out] buf:  string buffer shown on console
 *
 * @return
 *    on success, number of characters being output;
 *    otherwise, negative value on error.
 */
static ssize_t syna_testing_tsp_snr_touch_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int retval;
	unsigned int count = 0;
	struct syna_tcm *tcm = g_tcm_ptr;

	if (!tcm->is_connected) {
		count = snprintf(buf, PAGE_SIZE,
				"Device is NOT connected\n");
		goto exit;
	}

	retval = syna_testing_tsp_snr(tcm, TEST_PID57_TSP_SNR_TOUCH);

	count = snprintf(buf, PAGE_SIZE,
			"TEST TSP SNR NonTouch : %s\n", (retval < 0) ? "fail" : "pass");

exit:
	return count;
}

static struct kobj_attribute kobj_attr_tsp_snr_touch =
	__ATTR(tsp_snr_touch, 0444, syna_testing_tsp_snr_touch_show, NULL);

/**
 * declaration of sysfs attributes
 */
static struct attribute *attrs[] = {
	&kobj_attr_check_id.attr,
	&kobj_attr_pt01.attr,
	&kobj_attr_pt05.attr,
	&kobj_attr_pt0a.attr,
	&kobj_attr_pt11.attr,
	&kobj_attr_pt12.attr,
	&kobj_attr_pt16.attr,
	&kobj_attr_pt19.attr,
	&kobj_attr_pt1d.attr,
	&kobj_attr_bsc.attr,
	&kobj_attr_misc_calib.attr,
	&kobj_attr_gap.attr,
	&kobj_attr_jitter_delta.attr,
	&kobj_attr_sram.attr,
	&kobj_attr_read_calib.attr,
	&kobj_attr_write_calib.attr,
	&kobj_attr_erase_calib.attr,
	&kobj_attr_tsp_snr_frames.attr,
	&kobj_attr_tsp_snr_nontouch.attr,
	&kobj_attr_tsp_snr_touch.attr,
	NULL,
};

static struct attribute_group attr_testing_group = {
	.attrs = attrs,
};

/**
 * syna_testing_create_dir()
 *
 * Create a directory and register it with sysfs.
 * Then, create all defined sysfs files.
 *
 * @param
 *    [ in] tcm:  the driver handle
 *    [ in] sysfs_dir: root directory of sysfs nodes
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
int syna_testing_create_dir(struct syna_tcm *tcm,
		struct kobject *sysfs_dir)
{
	int retval = 0;

	g_testing_dir = kobject_create_and_add("testing",
			sysfs_dir);
	if (!g_testing_dir) {
		LOGE("Fail to create testing directory\n");
		return -EINVAL;
	}

	retval = sysfs_create_group(g_testing_dir, &attr_testing_group);
	if (retval < 0) {
		LOGE("Fail to create sysfs group\n");

		kobject_put(g_testing_dir);
		return retval;
	}

	g_tcm_ptr = tcm;

	return 0;
}
/**
 *syna_testing_remove_dir()
 *
 * Remove the allocate sysfs directory
 *
 * @param
 *    none
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
void syna_testing_remove_dir(void)
{
	if (g_testing_dir) {
		sysfs_remove_group(g_testing_dir, &attr_testing_group);

		kobject_put(g_testing_dir);
	}
}
