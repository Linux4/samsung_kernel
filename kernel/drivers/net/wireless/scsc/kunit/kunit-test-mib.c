#include <kunit/test.h>
#include "../mib.h"

#include "../mib.c"

#define SLSI_MIB_MORE_MASK	0x80

static void test_slsi_mib_buf_append(struct kunit *test)
{
	struct slsi_mib_data *dst = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	size_t buffer_length = 10;
	u8 *buffer = "\x12\x23\x34\x45\x56\x56\x56\x56\xAB\xB4";

	dst->dataLength = 10;
	dst->data = kmalloc(dst->dataLength + buffer_length, GFP_KERNEL);
	memset(dst->data, 0, 10);

	slsi_mib_buf_append(dst, 1000000000, buffer);
	slsi_mib_buf_append(dst, buffer_length, buffer);
}

static void test_slsi_mib_encode_uint32(struct kunit *test)
{
	u8 *buffer = kunit_kzalloc(test, sizeof(u8) * 1, GFP_KERNEL);
	u32 value = 63;

	KUNIT_EXPECT_EQ(test, 1, slsi_mib_encode_uint32(buffer, value));
	KUNIT_EXPECT_EQ(test, value, buffer[0]);

	value = 0x12345678;

	KUNIT_EXPECT_EQ(test, 5, slsi_mib_encode_uint32(buffer, value));
	KUNIT_EXPECT_EQ(test, 4 | SLSI_MIB_MORE_MASK, buffer[0]);
}

static void test_slsi_mib_encode_int32(struct kunit *test)
{
	u8 *buffer = kunit_kzalloc(test, sizeof(u8) * 1, GFP_KERNEL);
	s32 signed_value = 0x1234567;

	KUNIT_EXPECT_EQ(test, 5, slsi_mib_encode_int32(buffer, signed_value));

	signed_value = 0x11234567;
	KUNIT_EXPECT_EQ(test, 1, slsi_mib_encode_int32(buffer, signed_value));

	signed_value = 0x90000000;
	KUNIT_EXPECT_EQ(test, 5, slsi_mib_encode_int32(buffer, signed_value));
}

static void test_slsi_mib_encode_octet_str(struct kunit *test)
{
	u8 buffer[256] = {0};
	struct slsi_mib_data *octet_value = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);

	octet_value->dataLength = 0x100;
	octet_value->data = kunit_kzalloc(test, octet_value->dataLength, GFP_KERNEL);
	snprintf(octet_value->data, 0x6, "test");

	KUNIT_EXPECT_LT(test, octet_value->dataLength, slsi_mib_encode_octet_str(buffer, octet_value));
}

static void test_slsi_mib_decode_uint32(struct kunit *test)
{
	u8 *buffer = kunit_kzalloc(test, sizeof(u8) * 10, GFP_KERNEL);
	u32 value;

	buffer[0] = 0x01;
	KUNIT_EXPECT_EQ(test, 1, slsi_mib_decode_uint32(buffer, &value));

	buffer[0] = 0x83;
	KUNIT_EXPECT_EQ(test, 4, slsi_mib_decode_uint32(buffer, &value));
}

static void test_slsi_mib_decodeUint64(struct kunit *test)
{
	u8 *buffer = kunit_kzalloc(test, sizeof(u8) * 10, GFP_KERNEL);
	u64 value;

	buffer[0] = 0x01;
	KUNIT_EXPECT_EQ(test, 1, slsi_mib_decodeUint64(buffer, &value));

	buffer[0] = 0x83;
	KUNIT_EXPECT_EQ(test, 4, slsi_mib_decodeUint64(buffer, &value));
}

static void test_slsi_mib_decodeInt32(struct kunit *test)
{
	u8 *buffer = kunit_kzalloc(test, sizeof(u8) * 20, GFP_KERNEL);
	s32 value;

	buffer[0] = 0x01;
	KUNIT_EXPECT_EQ(test, 1, slsi_mib_decodeInt32(buffer, &value));

	buffer[0] = 0x43;
	KUNIT_EXPECT_EQ(test, 1, slsi_mib_decodeInt32(buffer, &value));

	buffer[0] = 0xC3;
	KUNIT_EXPECT_EQ(test, 4, slsi_mib_decodeInt32(buffer, &value));
}

static void test_slsi_mib_decodeInt64(struct kunit *test)
{
	u8 *buffer = kunit_kzalloc(test, sizeof(u8) * 20, GFP_KERNEL);
	s64 value;

	buffer[0] = 0x01;
	KUNIT_EXPECT_EQ(test, 1, slsi_mib_decodeInt64(buffer, &value));

	buffer[0] = 0x43;
	KUNIT_EXPECT_EQ(test, 1, slsi_mib_decodeInt64(buffer, &value));

	buffer[0] = 0xC3;
	KUNIT_EXPECT_EQ(test, 4, slsi_mib_decodeInt64(buffer, &value));
}

static void test_slsi_mib_decode_octet_str(struct kunit *test)
{
	u8 *buffer = kunit_kzalloc(test, sizeof(u8) * 20, GFP_KERNEL);
	struct slsi_mib_data octet_value;

	buffer[0] = 0x0A;
	buffer[8] = 0x01;
	buffer[9] = 0x01;
	buffer[10] = 0x01;
	KUNIT_EXPECT_EQ(test, 65804, slsi_mib_decode_octet_str(buffer, &octet_value));
}

static void test_slsi_mib_decode_type_length(struct kunit *test)
{
	u8 *buffer = kunit_kzalloc(test, sizeof(u8) * 20, GFP_KERNEL);
	size_t length;

	buffer[0] = 0xC2;
	buffer[1] = 0x01;
	buffer[2] = 0x01;
	buffer[3] = 0x01;
	KUNIT_EXPECT_EQ(test, SLSI_MIB_TYPE_INT, slsi_mib_decode_type_length(buffer, &length));

	buffer[0] = 0xA2;
	KUNIT_EXPECT_EQ(test, SLSI_MIB_TYPE_OCTET, slsi_mib_decode_type_length(buffer, &length));

	buffer[0] = 0x82;
	KUNIT_EXPECT_EQ(test, SLSI_MIB_TYPE_UINT, slsi_mib_decode_type_length(buffer, &length));
}

static void test_slsi_mib_encode_psid_indexs(struct kunit *test)
{
	struct slsi_mib_get_entry *value = kunit_kzalloc(test, sizeof(struct slsi_mib_get_entry), GFP_KERNEL);
	u8 *buffer = kunit_kzalloc(test, sizeof(u8) * 15, GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 4, slsi_mib_encode_psid_indexs(buffer, value));

	value->index[0] = 2;
	KUNIT_EXPECT_EQ(test, 6, slsi_mib_encode_psid_indexs(buffer, value));
}

static void test_slsi_mib_encode(struct kunit *test)
{
	struct slsi_mib_data *buffer = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	struct slsi_mib_entry *value = kunit_kzalloc(test, sizeof(struct slsi_mib_entry), GFP_KERNEL);

	buffer->data = kmalloc(100, GFP_KERNEL);
	value->value.type = SLSI_MIB_TYPE_OCTET;
	value->value.u.octetValue.data = kunit_kzalloc(test, sizeof(u8), GFP_KERNEL);
	*value->value.u.octetValue.data = 0x12;
	value->value.u.octetValue.dataLength = 0x1000000;
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_FAILURE, slsi_mib_encode(buffer, value));

	value->index[0] = 2;
	value->value.u.octetValue.dataLength = 0;
	value->psid = 10;
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, slsi_mib_encode(buffer, value));

	value->index[0] = 0;
	value->value.type = SLSI_MIB_TYPE_UINT;
	value->value.u.uintValue = 63;
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, slsi_mib_encode(buffer, value));

	value->value.type = SLSI_MIB_TYPE_INT;
	value->value.u.intValue = 0x11234567;
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, slsi_mib_encode(buffer, value));

	value->value.type = SLSI_MIB_TYPE_BOOL;
	value->value.u.boolValue = false;
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, slsi_mib_encode(buffer, value));

	value->value.type = SLSI_MIB_TYPE_NONE;
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, slsi_mib_encode(buffer, value));

	value->value.type = 5;
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_FAILURE, slsi_mib_encode(buffer, value));

	if (buffer->data)
		kfree(buffer->data);
}

static void test_slsi_mib_decode(struct kunit *test)
{
	struct slsi_mib_data *buffer = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	struct slsi_mib_entry *value = kunit_kzalloc(test, sizeof(struct slsi_mib_entry), GFP_KERNEL);

	buffer->data = kunit_kzalloc(test, sizeof(u8) * 50, GFP_KERNEL);
	buffer->dataLength = 3;
	KUNIT_EXPECT_EQ(test, 0, slsi_mib_decode(buffer, value));

	buffer->dataLength = 10;
	buffer->data[2] = 10;
	KUNIT_EXPECT_EQ(test, 0, slsi_mib_decode(buffer, value));

	buffer->dataLength = 4;
	buffer->data[2] = 5;
	buffer->data[0] = 0x00;
	buffer->data[1] = 0x00;
	buffer->data[4] = 0xA0;
	buffer->data[5] = 0xA0;
	KUNIT_EXPECT_EQ(test, 0, slsi_mib_decode(buffer, value));

	buffer->dataLength = 10;
	buffer->data[4] = 0xAA;
	buffer->data[12] = 0x01;
	buffer->data[13] = 0x01;
	buffer->data[14] = 0x01;
	KUNIT_EXPECT_EQ(test, 0, slsi_mib_decode(buffer, value));

	buffer->dataLength = 10;
	buffer->data[4] = 0x81;
	buffer->data[9] = 0x11;
	KUNIT_EXPECT_EQ(test, 10, slsi_mib_decode(buffer, value));

	buffer->data[4] = 0x83;
	KUNIT_EXPECT_EQ(test, 10, slsi_mib_decode(buffer, value));

	buffer->dataLength = 10;
	buffer->data[4] = 0xC2;
	KUNIT_EXPECT_EQ(test, 10, slsi_mib_decode(buffer, value));

	buffer->data[4] = 0xA0;
	buffer->data[5] = 0x04;
	KUNIT_EXPECT_EQ(test, 10, slsi_mib_decode(buffer, value));
}

static void test_slsi_mib_encode_get_list(struct kunit *test)
{
	struct slsi_mib_get_entry *psids = kunit_kzalloc(test, sizeof(struct slsi_mib_get_entry), GFP_KERNEL);
	struct slsi_mib_data *buffer = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	u16 psids_length = 1;

	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, slsi_mib_encode_get_list(buffer, psids_length, psids));

	if (buffer->data)
		kfree(buffer->data);
}

static void test_slsi_mib_encode_get(struct kunit *test)
{
	struct slsi_mib_data *buffer = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	u16 psid = 12;
	u16 idx = 13;

	buffer->data = kmalloc(130, GFP_KERNEL);

	slsi_mib_encode_get(buffer, psid, idx);
}

static void test_slsi_mib_find(struct kunit *test)
{
	struct slsi_mib_data *buffer = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	struct slsi_mib_get_entry *entry = kunit_kzalloc(test, sizeof(struct slsi_mib_get_entry), GFP_KERNEL);

	buffer->data = kunit_kzalloc(test, sizeof(u8) * 50, GFP_KERNEL);
	buffer->dataLength = 1;
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_mib_find(buffer, entry));

	buffer->dataLength = 10;
	entry->psid = 10;
	buffer->data[0] = 0x0A;
	entry->index[0] = 1;
	buffer->data[4] = 0x0A;
	buffer->data[5] = 0x00;
	buffer->data[6] = 0x01;
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_mib_find(buffer, entry));

	buffer->dataLength = 10;
	entry->psid = 10;
	buffer->data[0] = 0x0A;
	entry->index[0] = 0;
	buffer->data[4] = 0x0A;
	buffer->data[5] = 0x00;
	buffer->data[6] = 0x01;
	KUNIT_EXPECT_PTR_EQ(test, buffer->data, slsi_mib_find(buffer, entry));

	buffer->dataLength = 10;
	entry->psid = 10;
	buffer->data[0] = 0x0A;
	entry->index[0] = 1;
	buffer->data[4] = 0x0A;
	buffer->data[5] = 0x00;
	buffer->data[6] = 0x01;
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_mib_find(buffer, entry));
}

static void test_slsi_mib_decode_get_list(struct kunit *test)
{
	struct slsi_mib_data *buffer = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	struct slsi_mib_get_entry *psids = kunit_kzalloc(test, sizeof(struct slsi_mib_get_entry), GFP_KERNEL);
	u16 psids_length = 2;
	struct slsi_mib_value *result;

	buffer->data = kunit_kzalloc(test, sizeof(u8) * 50, GFP_KERNEL);
	buffer->dataLength = 10;
	psids->psid = 10;
	buffer->data[0] = 0x0A;
	psids->index[0] = 0;
	buffer->data[4] = 0x0A;
	buffer->data[5] = 0x00;
	buffer->data[6] = 0x01;

	result = slsi_mib_decode_get_list(buffer, psids_length, psids);
	KUNIT_EXPECT_PTR_NE(test, NULL, result);

	if (result)
		kfree(result);

	buffer->dataLength = 4;
	psids->psid = 10;
	buffer->data[0] = 0x0A;
	psids->index[0] = 1;
	buffer->data[4] = 0x0A;
	buffer->data[5] = 0x00;
	buffer->data[6] = 0x01;
	result = slsi_mib_decode_get_list(buffer, psids_length, psids);

	if (result)
		kfree(result);
}

static void test_slsi_mib_encode_bool(struct kunit *test)
{
	struct slsi_mib_data *buffer = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	u16 psid = 12;
	bool value = false;
	u16 idx = 0;

	buffer->data = kmalloc(20, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, slsi_mib_encode_bool(buffer, psid, value, idx));

	if (buffer->data)
		kfree(buffer->data);
}

static void test_slsi_mib_encode_int(struct kunit *test)
{
	struct slsi_mib_data *buffer = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	u16 psid = 12;
	s32 value = 0x11234567;
	u16 idx = 0;

	buffer->data = kmalloc(20, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, slsi_mib_encode_int(buffer, psid, value, idx));

	if (buffer->data)
		kfree(buffer->data);
}

static void test_slsi_mib_encode_uint(struct kunit *test)
{
	struct slsi_mib_data *buffer = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	u16 psid = 12;
	u32 value = 63;
	u16 idx = 0;

	buffer->data = kmalloc(20, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, slsi_mib_encode_uint(buffer, psid, value, idx));

	if (buffer->data)
		kfree(buffer->data);
}

static void test_slsi_mib_encode_octet(struct kunit *test)
{
	struct slsi_mib_data *buffer = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	u16 psid = 12;
	size_t dataLength = 0;
	u8 *data = kunit_kzalloc(test, sizeof(u8), GFP_KERNEL);
	u16 idx = 0;

	buffer->data = kmalloc(20, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, SLSI_MIB_STATUS_SUCCESS, slsi_mib_encode_octet(buffer, psid, dataLength, data, idx));

	if (buffer->data)
		kfree(buffer->data);
}

/*
 * Test fictures
 */
static int mib_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void mib_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return;
}

/*
 * KUnit testcase definitions
 */
static struct kunit_case mib_test_cases[] = {
	KUNIT_CASE(test_slsi_mib_buf_append),
	KUNIT_CASE(test_slsi_mib_encode_uint32),
	KUNIT_CASE(test_slsi_mib_encode_int32),
	KUNIT_CASE(test_slsi_mib_encode_octet_str),
	KUNIT_CASE(test_slsi_mib_decode_uint32),
	KUNIT_CASE(test_slsi_mib_decodeUint64),
	KUNIT_CASE(test_slsi_mib_decodeInt32),
	KUNIT_CASE(test_slsi_mib_decodeInt64),
	KUNIT_CASE(test_slsi_mib_decode_octet_str),
	KUNIT_CASE(test_slsi_mib_decode_type_length),
	KUNIT_CASE(test_slsi_mib_encode_psid_indexs),
	KUNIT_CASE(test_slsi_mib_encode),
	KUNIT_CASE(test_slsi_mib_decode),
	KUNIT_CASE(test_slsi_mib_encode_get_list),
	KUNIT_CASE(test_slsi_mib_encode_get),
	KUNIT_CASE(test_slsi_mib_find),
	KUNIT_CASE(test_slsi_mib_decode_get_list),
	KUNIT_CASE(test_slsi_mib_encode_bool),
	KUNIT_CASE(test_slsi_mib_encode_int),
	KUNIT_CASE(test_slsi_mib_encode_uint),
	KUNIT_CASE(test_slsi_mib_encode_octet),
	{}
};

static struct kunit_suite mib_test_suite[] = {
	{
		.name = "kunit-mib-test",
		.test_cases = mib_test_cases,
		.init = mib_test_init,
		.exit = mib_test_exit,
	}};

kunit_test_suites(mib_test_suite);
