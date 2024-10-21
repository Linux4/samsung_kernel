// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "util.h"

extern int usdm_snprintf_bytes_line(char *linebuf, size_t linebuflen, int rowsize,
		const u8 *bytes, size_t sz_bytes);

/*
 * This is the most fundamental element of KUnit, the test case. A test case
 * makes a set EXPECTATIONs and ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct kunit *)`. `struct kunit` is a context object that stores
 * information about the current test.
 */

/* NOTE: UML TC */
static void util_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void util_test_stringfy_macro(struct kunit *test)
{
	char arr[15] = STRINGFY(test-string);
	KUNIT_EXPECT_STREQ(test, &arr[0], "test-string");
}

static void util_test_m_nargs_macro_return_count_of_arguments(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, M_NARGS(0), 1);
	KUNIT_EXPECT_EQ(test, M_NARGS(0, 0), 2);
	KUNIT_EXPECT_EQ(test, M_NARGS(0, 0, 0), 3);
	KUNIT_EXPECT_EQ(test, M_NARGS(0, 0, 0, 0), 4);
	KUNIT_EXPECT_EQ(test, M_NARGS(0, 0, 0, 0, 0), 5);
	KUNIT_EXPECT_EQ(test, M_NARGS(0, 0, 0, 0, 0, 0), 6);
	KUNIT_EXPECT_EQ(test, M_NARGS(0, 0, 0, 0, 0, 0, 0), 7);
	KUNIT_EXPECT_EQ(test, M_NARGS(0, 0, 0, 0, 0, 0, 0, 0), 8);
}

static void util_test_m_conc_macro_return_concaternate_two_arguments(struct kunit *test)
{
	int aaa = 1;
	int bbb = 2;
	int aaabbb = 3;

	KUNIT_EXPECT_EQ(test, M_CONC(aaa,), aaa);
	KUNIT_EXPECT_EQ(test, M_CONC(,bbb), bbb);
	KUNIT_EXPECT_EQ(test, M_CONC(aaa, bbb), aaabbb);
}

static void util_test_m_get_elem_macro_return_nth_argument(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, M_GET_ELEM_1(1, 2, 3, 4, 5, 6, 7, 8), 1);
	KUNIT_EXPECT_EQ(test, M_GET_ELEM_2(1, 2, 3, 4, 5, 6, 7, 8), 2);
	KUNIT_EXPECT_EQ(test, M_GET_ELEM_3(1, 2, 3, 4, 5, 6, 7, 8), 3);
	KUNIT_EXPECT_EQ(test, M_GET_ELEM_4(1, 2, 3, 4, 5, 6, 7, 8), 4);
	KUNIT_EXPECT_EQ(test, M_GET_ELEM_5(1, 2, 3, 4, 5, 6, 7, 8), 5);
	KUNIT_EXPECT_EQ(test, M_GET_ELEM_6(1, 2, 3, 4, 5, 6, 7, 8), 6);
	KUNIT_EXPECT_EQ(test, M_GET_ELEM_7(1, 2, 3, 4, 5, 6, 7, 8), 7);
	KUNIT_EXPECT_EQ(test, M_GET_ELEM_8(1, 2, 3, 4, 5, 6, 7, 8), 8);
}

static void util_test_m_get_back_macro_return_nth_argument_from_end(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, M_GET_LAST(1, 2, 3, 4, 5, 6, 7, 8), 8);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_1(1, 2, 3, 4, 5, 6, 7, 8), 8);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_2(1, 2, 3, 4, 5, 6, 7, 8), 7);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_3(1, 2, 3, 4, 5, 6, 7, 8), 6);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_4(1, 2, 3, 4, 5, 6, 7, 8), 5);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_5(1, 2, 3, 4, 5, 6, 7, 8), 4);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_6(1, 2, 3, 4, 5, 6, 7, 8), 3);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_7(1, 2, 3, 4, 5, 6, 7, 8), 2);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_8(1, 2, 3, 4, 5, 6, 7, 8), 1);
}

static void util_test_m_get_back_macro_return_0_if_index_is_greater_than_number_of_arguments(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, M_GET_BACK_1(), 0);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_2(1), 0);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_3(1, 2), 0);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_4(1, 2, 3), 0);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_5(1, 2, 3, 4), 0);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_6(1, 2, 3, 4, 5), 0);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_7(1, 2, 3, 4, 5, 6), 0);
	KUNIT_EXPECT_EQ(test, M_GET_BACK_8(1, 2, 3, 4, 5, 6, 7), 0);
}

static void util_test_m_get_last_macro_return_last_argument(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, M_GET_LAST(1, 2, 3, 4, 5, 6, 7, 8), 8);
	KUNIT_EXPECT_EQ(test, M_GET_LAST(1, 2, 3, 4, 5, 6, 7), 7);
	KUNIT_EXPECT_EQ(test, M_GET_LAST(1, 2, 3, 4, 5, 6), 6);
	KUNIT_EXPECT_EQ(test, M_GET_LAST(1, 2, 3, 4, 5), 5);
	KUNIT_EXPECT_EQ(test, M_GET_LAST(1, 2, 3, 4), 4);
	KUNIT_EXPECT_EQ(test, M_GET_LAST(1, 2, 3), 3);
	KUNIT_EXPECT_EQ(test, M_GET_LAST(1, 2), 2);
	KUNIT_EXPECT_EQ(test, M_GET_LAST(1), 1);
	KUNIT_EXPECT_EQ(test, M_GET_LAST(), 0);
}

static void util_test_copy_from_sliced_byte_array_fail_with_invalid_argument(struct kunit *test)
{
	const unsigned char src[6] = { 1, 2, 3, 4, 5, 6 };
	unsigned char dst[3];

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, NULL, 0, 0, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(NULL, src, 0, 0, 0), -EINVAL);
}

static void util_test_copy_from_sliced_byte_array_return_0_if_slicing_position_under_zero(struct kunit *test)
{
	const unsigned char src[6] = { 1, 2, 3, 4, 5, 6 };
	unsigned char dst[3];

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, -1, ARRAY_SIZE(src), 1), 0);
	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, -1, -6, 1), 0);
}

static void util_test_copy_from_sliced_byte_array_return_0_if_start_to_stop_is_forward_but_step_is_minus(struct kunit *test)
{
	const unsigned char src[6] = { 1, 2, 3, 4, 5, 6 };
	unsigned char dst[3];

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, 0, ARRAY_SIZE(src), -2), 0);
}

static void util_test_copy_from_sliced_byte_array_return_0_if_start_to_stop_is_reverse_but_step_is_plus(struct kunit *test)
{
	const unsigned char src[6] = { 1, 2, 3, 4, 5, 6 };
	unsigned char dst[3];

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, ARRAY_SIZE(src) - 1, -1, 2), 0);
}

static void util_test_copy_from_sliced_byte_array_forward_order_success(struct kunit *test)
{
	const unsigned char src[6] = { 1, 2, 3, 4, 5, 6 };
	unsigned char dst[3];

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, 0, ARRAY_SIZE(src), 2), 3);
	KUNIT_EXPECT_EQ(test, dst[0], src[0]);
	KUNIT_EXPECT_EQ(test, dst[1], src[2]);
	KUNIT_EXPECT_EQ(test, dst[2], src[4]);

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, 1, ARRAY_SIZE(src), 2), 3);
	KUNIT_EXPECT_EQ(test, dst[0], src[1]);
	KUNIT_EXPECT_EQ(test, dst[1], src[3]);
	KUNIT_EXPECT_EQ(test, dst[2], src[5]);

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, 2, ARRAY_SIZE(src), 2), 2);
	KUNIT_EXPECT_EQ(test, dst[0], src[2]);
	KUNIT_EXPECT_EQ(test, dst[1], src[4]);

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, 3, ARRAY_SIZE(src), 2), 2);
	KUNIT_EXPECT_EQ(test, dst[0], src[3]);
	KUNIT_EXPECT_EQ(test, dst[1], src[5]);

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, 4, ARRAY_SIZE(src), 2), 1);
	KUNIT_EXPECT_EQ(test, dst[0], src[4]);

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, 5, ARRAY_SIZE(src), 2), 1);
	KUNIT_EXPECT_EQ(test, dst[0], src[5]);
}

static void util_test_copy_from_sliced_byte_array_reverse_order_success(struct kunit *test)
{
	const unsigned char src[6] = { 1, 2, 3, 4, 5, 6 };
	unsigned char dst[3];

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, ARRAY_SIZE(src) - 1, -1, -2), 3);
	KUNIT_EXPECT_EQ(test, dst[0], src[5]);
	KUNIT_EXPECT_EQ(test, dst[1], src[3]);
	KUNIT_EXPECT_EQ(test, dst[2], src[1]);

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, ARRAY_SIZE(src) - 2, -1, -2), 3);
	KUNIT_EXPECT_EQ(test, dst[0], src[4]);
	KUNIT_EXPECT_EQ(test, dst[1], src[2]);
	KUNIT_EXPECT_EQ(test, dst[2], src[0]);

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, ARRAY_SIZE(src) - 3, -1, -2), 2);
	KUNIT_EXPECT_EQ(test, dst[0], src[3]);
	KUNIT_EXPECT_EQ(test, dst[1], src[1]);

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, ARRAY_SIZE(src) - 4, -1, -2), 2);
	KUNIT_EXPECT_EQ(test, dst[0], src[2]);
	KUNIT_EXPECT_EQ(test, dst[1], src[0]);

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, ARRAY_SIZE(src) - 5, -1, -2), 1);
	KUNIT_EXPECT_EQ(test, dst[0], src[1]);

	KUNIT_EXPECT_EQ(test, copy_from_sliced_byte_array(dst, src, ARRAY_SIZE(src) - 6, -1, -2), 1);
	KUNIT_EXPECT_EQ(test, dst[0], src[0]);
}

static void util_test_copy_to_sliced_byte_array_fail_with_invalid_argument(struct kunit *test)
{
	unsigned char dst[6] = { 0, };
	const unsigned char src[3] = { 1, 2, 3 };

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, NULL, 0, 0, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(NULL, src, 0, 0, 0), -EINVAL);
}

static void util_test_copy_to_sliced_byte_array_return_0_if_slicing_position_under_zero(struct kunit *test)
{
	unsigned char dst[6] = { 0, };
	const unsigned char src[3] = { 1, 2, 3 };

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, -1, ARRAY_SIZE(dst), 1), 0);
	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, -1, -6, 1), 0);
}

static void util_test_copy_to_sliced_byte_array_return_0_if_start_to_stop_is_forward_but_step_is_minus(struct kunit *test)
{
	unsigned char dst[6] = { 0, };
	const unsigned char src[3] = { 1, 2, 3 };

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, 0, ARRAY_SIZE(dst), -2), 0);
}

static void util_test_copy_to_sliced_byte_array_return_0_if_start_to_stop_is_reverse_but_step_is_plus(struct kunit *test)
{
	unsigned char dst[6] = { 0, };
	const unsigned char src[3] = { 1, 2, 3 };

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, ARRAY_SIZE(dst) - 1, -1, 2), 0);
}

static void util_test_copy_to_sliced_byte_array_forward_order_success(struct kunit *test)
{
	unsigned char dst[6] = { 0, };
	const unsigned char src[3] = { 1, 2, 3 };

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, 0, ARRAY_SIZE(dst), 2), 3);
	KUNIT_EXPECT_EQ(test, dst[0], src[0]);
	KUNIT_EXPECT_EQ(test, dst[2], src[1]);
	KUNIT_EXPECT_EQ(test, dst[4], src[2]);

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, 1, ARRAY_SIZE(dst), 2), 3);
	KUNIT_EXPECT_EQ(test, dst[1], src[0]);
	KUNIT_EXPECT_EQ(test, dst[3], src[1]);
	KUNIT_EXPECT_EQ(test, dst[5], src[2]);

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, 2, ARRAY_SIZE(dst), 2), 2);
	KUNIT_EXPECT_EQ(test, dst[2], src[0]);
	KUNIT_EXPECT_EQ(test, dst[4], src[1]);

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, 3, ARRAY_SIZE(dst), 2), 2);
	KUNIT_EXPECT_EQ(test, dst[3], src[0]);
	KUNIT_EXPECT_EQ(test, dst[5], src[1]);

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, 4, ARRAY_SIZE(dst), 2), 1);
	KUNIT_EXPECT_EQ(test, dst[4], src[0]);

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, 5, ARRAY_SIZE(dst), 2), 1);
	KUNIT_EXPECT_EQ(test, dst[5], src[0]);
}

static void util_test_copy_to_sliced_byte_array_reverse_order_success(struct kunit *test)
{
	unsigned char dst[6] = { 0, };
	const unsigned char src[3] = { 1, 2, 3 };

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, ARRAY_SIZE(dst) - 1, -1, -2), 3);
	KUNIT_EXPECT_EQ(test, dst[5], src[0]);
	KUNIT_EXPECT_EQ(test, dst[3], src[1]);
	KUNIT_EXPECT_EQ(test, dst[1], src[2]);

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, ARRAY_SIZE(dst) - 2, -1, -2), 3);
	KUNIT_EXPECT_EQ(test, dst[4], src[0]);
	KUNIT_EXPECT_EQ(test, dst[2], src[1]);
	KUNIT_EXPECT_EQ(test, dst[0], src[2]);

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, ARRAY_SIZE(dst) - 3, -1, -2), 2);
	KUNIT_EXPECT_EQ(test, dst[3], src[0]);
	KUNIT_EXPECT_EQ(test, dst[1], src[1]);

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, ARRAY_SIZE(dst) - 4, -1, -2), 2);
	KUNIT_EXPECT_EQ(test, dst[2], src[0]);
	KUNIT_EXPECT_EQ(test, dst[0], src[1]);

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, ARRAY_SIZE(dst) - 5, -1, -2), 1);
	KUNIT_EXPECT_EQ(test, dst[1], src[0]);

	KUNIT_EXPECT_EQ(test, copy_to_sliced_byte_array(dst, src, ARRAY_SIZE(dst) - 6, -1, -2), 1);
	KUNIT_EXPECT_EQ(test, dst[0], src[0]);
}

static void util_test_hextos32(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, hextos32(0x3FF, 10), -511);
	KUNIT_EXPECT_EQ(test, hextos32(0x1FF, 10), 511);
}

static void util_test_s32tohex(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, s32tohex(-511, 10), (u32)0x3FF);
	KUNIT_EXPECT_EQ(test, s32tohex(511, 10), (u32)0x1FF);
}

static void util_test_calc_checksum_16bit_boundardy_check(struct kunit *test)
{
	u8 *arr = kunit_kzalloc(test, sizeof(*arr) * 258, GFP_KERNEL);

	memset(arr, 0xFF, 258);
	KUNIT_EXPECT_EQ(test, calc_checksum_16bit(arr, 257), (u16)0xFFFF);
	KUNIT_EXPECT_EQ(test, calc_checksum_16bit(arr, 258), (u16)0xFE);
}

static void util_test_usdm_snprintf_bytes_line_success(struct kunit *test)
{
	u8 *bytes = kunit_kzalloc(test, 256, GFP_KERNEL);
	u8 *linebuf = kunit_kzalloc(test, 1024, GFP_KERNEL);
	int i, sz_bytes;

	sz_bytes = 33;
	for (i = 0; i < sz_bytes; i++)
		bytes[i] = i;

	/* do nothing if there's no remaining linebuflen */
	linebuf[0] = 'c';
	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes_line(linebuf, 0, 32, bytes, sz_bytes), 0);
	KUNIT_EXPECT_EQ(test, (u8)linebuf[0], (u8)'c');

	/* set 'null terminator' in case of sz_bytes is 0 */
	linebuf[0] = 'c';
	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes_line(linebuf, 1, 32, bytes, 0), 0);
	KUNIT_EXPECT_EQ(test, (u8)linebuf[0], (u8)'\0');

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes_line(linebuf, 96, 16, bytes, sz_bytes), 47);
	KUNIT_EXPECT_STREQ(test, linebuf, "00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f");

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes_line(linebuf, 96, 32, bytes, sz_bytes), 95);
	KUNIT_EXPECT_STREQ(test, linebuf, "00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f");
}

static void util_test_usdm_snprintf_bytes_success(struct kunit *test)
{
	u8 *bytes = kunit_kzalloc(test, 256, GFP_KERNEL);
	u8 *linebuf = kunit_kzalloc(test, 1024, GFP_KERNEL);
	int i, sz_bytes;

	sz_bytes = 33;
	for (i = 0; i < sz_bytes; i++)
		bytes[i] = i;

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes(linebuf, 2, bytes, sz_bytes), 0);

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes(linebuf, 3, bytes, sz_bytes), 2);
	KUNIT_EXPECT_STREQ(test, linebuf, "00");

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes(linebuf, 4, bytes, sz_bytes), 3);
	KUNIT_EXPECT_STREQ(test, linebuf, "00 ");

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes(linebuf, 5, bytes, sz_bytes), 4);
	KUNIT_EXPECT_STREQ(test, linebuf, "00 0");

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes(linebuf, 6, bytes, sz_bytes), 5);
	KUNIT_EXPECT_STREQ(test, linebuf, "00 01");

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes(linebuf, 96, bytes, sz_bytes), 95);
	KUNIT_EXPECT_STREQ(test, linebuf, "00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f");

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes(linebuf, 97, bytes, sz_bytes), 95);
	KUNIT_EXPECT_STREQ(test, linebuf, "00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f");

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes(linebuf, 98, bytes, sz_bytes), 97);
	KUNIT_EXPECT_STREQ(test, linebuf, "00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f\n2");

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes(linebuf, 99, bytes, sz_bytes), 98);
	KUNIT_EXPECT_STREQ(test, linebuf, "00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f\n20");

	KUNIT_EXPECT_EQ(test, usdm_snprintf_bytes(linebuf, 1024, bytes, sz_bytes), sz_bytes * 3 - 1);
	KUNIT_EXPECT_STREQ(test, linebuf, "00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f\n20");
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int util_test_init(struct kunit *test)
{
	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void util_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case util_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(util_test_test),

	KUNIT_CASE(util_test_stringfy_macro),
	KUNIT_CASE(util_test_m_nargs_macro_return_count_of_arguments),
	KUNIT_CASE(util_test_m_conc_macro_return_concaternate_two_arguments),
	KUNIT_CASE(util_test_m_get_elem_macro_return_nth_argument),
	KUNIT_CASE(util_test_m_get_back_macro_return_nth_argument_from_end),
	KUNIT_CASE(util_test_m_get_back_macro_return_0_if_index_is_greater_than_number_of_arguments),
	KUNIT_CASE(util_test_m_get_last_macro_return_last_argument),

	KUNIT_CASE(util_test_copy_from_sliced_byte_array_fail_with_invalid_argument),
	KUNIT_CASE(util_test_copy_from_sliced_byte_array_return_0_if_slicing_position_under_zero),
	KUNIT_CASE(util_test_copy_from_sliced_byte_array_return_0_if_start_to_stop_is_forward_but_step_is_minus),
	KUNIT_CASE(util_test_copy_from_sliced_byte_array_return_0_if_start_to_stop_is_reverse_but_step_is_plus),
	KUNIT_CASE(util_test_copy_from_sliced_byte_array_forward_order_success),
	KUNIT_CASE(util_test_copy_from_sliced_byte_array_reverse_order_success),

	KUNIT_CASE(util_test_copy_to_sliced_byte_array_fail_with_invalid_argument),
	KUNIT_CASE(util_test_copy_to_sliced_byte_array_return_0_if_slicing_position_under_zero),
	KUNIT_CASE(util_test_copy_to_sliced_byte_array_return_0_if_start_to_stop_is_forward_but_step_is_minus),
	KUNIT_CASE(util_test_copy_to_sliced_byte_array_return_0_if_start_to_stop_is_reverse_but_step_is_plus),
	KUNIT_CASE(util_test_copy_to_sliced_byte_array_forward_order_success),
	KUNIT_CASE(util_test_copy_to_sliced_byte_array_reverse_order_success),
	KUNIT_CASE(util_test_hextos32),
	KUNIT_CASE(util_test_s32tohex),
	KUNIT_CASE(util_test_calc_checksum_16bit_boundardy_check),
	KUNIT_CASE(util_test_usdm_snprintf_bytes_line_success),
	KUNIT_CASE(util_test_usdm_snprintf_bytes_success),
	{},
};

/*
 * This defines a suite or grouping of tests.
 *
 * Test cases are defined as belonging to the suite by adding them to
 * `test_cases`.
 *
 * Often it is desirable to run some function which will set up things which
 * will be used by every test; this is accomplished with an `init` function
 * which runs before each test case is invoked. Similarly, an `exit` function
 * may be specified which runs after every test case and can be used to for
 * cleanup. For clarity, running tests in a test module would behave as follows:
 *
 * module.init(test);
 * module.test_case[0](test);
 * module.exit(test);
 * module.init(test);
 * module.test_case[1](test);
 * module.exit(test);
 * ...;
 */
static struct kunit_suite util_test_module = {
	.name = "util_test",
	.init = util_test_init,
	.exit = util_test_exit,
	.test_cases = util_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&util_test_module);

MODULE_LICENSE("GPL v2");
