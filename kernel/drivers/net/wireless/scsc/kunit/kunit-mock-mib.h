/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_MIB_H__
#define __KUNIT_MOCK_MIB_H__

#include "../mib.h"
#include "../debug.h"

#define slsi_mib_encode_octet_str(args...)	kunit_mock_slsi_mib_encode_octet_str(args)
#define slsi_mib_encode_int32(args...)		kunit_mock_slsi_mib_encode_int32(args)
#define slsi_mib_decodeUint64(args...)		kunit_mock_slsi_mib_decodeUint64(args)
#define slsi_mib_encode(args...)		kunit_mock_slsi_mib_encode(args)
#define slsi_mib_decodeInt32(args...)		kunit_mock_slsi_mib_decodeInt32(args)
#define slsi_mib_encode_int(args...)		kunit_mock_slsi_mib_encode_int(args)
#define slsi_mib_encode_octet(args...)		kunit_mock_slsi_mib_encode_octet(args)
#define slsi_mib_decodeInt64(args...)		kunit_mock_slsi_mib_decodeInt64(args)
#define slsi_mib_encode_get(args...)		kunit_mock_slsi_mib_encode_get(args)
#define slsi_mib_encode_uint(args...)		kunit_mock_slsi_mib_encode_uint(args)
#define slsi_mib_encode_get_list(args...)	kunit_mock_slsi_mib_encode_get_list(args)
#define slsi_mib_encode_bool(args...)		kunit_mock_slsi_mib_encode_bool(args)
#define slsi_mib_decode(args...)		kunit_mock_slsi_mib_decode(args)
#define slsi_mib_decode_octet_str(args...)	kunit_mock_slsi_mib_decode_octet_str(args)
#define slsi_mib_decode_get_list(args...)	kunit_mock_slsi_mib_decode_get_list(args)
#define slsi_mib_find(args...)			kunit_mock_slsi_mib_find(args)
#define slsi_mib_encode_uint32(args...)		kunit_mock_slsi_mib_encode_uint32(args)
#define slsi_mib_buf_append(args...)		kunit_mock_slsi_mib_buf_append(args)


static size_t kunit_mock_slsi_mib_encode_octet_str(u8 *buffer, struct slsi_mib_data *octet_value)
{
	return 0;
}

static size_t kunit_mock_slsi_mib_encode_int32(u8 *buffer, s32 signed_value)
{
	return 0;
}

static size_t kunit_mock_slsi_mib_decodeUint64(u8 *buffer, u64 *value)
{
	return 0;
}

static size_t kunit_mock_slsi_mib_decodeInt32(u8 *buffer, s32 *value)
{
	return 0;
}

static u16 kunit_mock_slsi_mib_encode_int(struct slsi_mib_data *buffer, u16 psid, s32 value, u16 idx)
{
	struct slsi_mib_entry v;

	if (!buffer)
		return SLSI_MIB_STATUS_FAILURE;

	memset(&v, 0x00, sizeof(struct slsi_mib_entry));
	v.psid = psid;
	v.index[0] = idx;
	v.value.type = SLSI_MIB_TYPE_INT;
	v.value.u.intValue = value;

	buffer->dataLength = sizeof(value) + idx;
	buffer->data = kmalloc(buffer->dataLength, GFP_KERNEL);

	return SLSI_MIB_STATUS_SUCCESS;
}

static u16 kunit_mock_slsi_mib_encode_octet(struct slsi_mib_data *buffer, u16 psid, size_t dataLength,
					    const u8 *data, u16 idx)
{
	if (data) {
		if (data[0] == 'K' && data[1] == 'O') return 3;
		else {
			if (buffer) {
				if (data[0] == 'U' && data[1] == 'S')
					buffer->dataLength = 0;
				else
					buffer->dataLength = 10;
			}
		}
	}
	return 0;
}

static size_t kunit_mock_slsi_mib_decodeInt64(u8 *buffer, s64 *value)
{
	return 0;
}

static void kunit_mock_slsi_mib_encode_get(struct slsi_mib_data *buffer, u16 psid, u16 idx)
{
	return;
}

static u16 kunit_mock_slsi_mib_encode_uint(struct slsi_mib_data *buffer, u16 psid, u32 value, u16 idx)
{
	struct slsi_mib_entry v;

	if (!buffer)
		return SLSI_MIB_STATUS_FAILURE;

	memset(&v, 0x00, sizeof(struct slsi_mib_entry));
	v.psid = psid;
	v.index[0] = idx;
	v.value.type = SLSI_MIB_TYPE_UINT;
	v.value.u.uintValue = value;

	buffer->dataLength = sizeof(value) + idx;
	buffer->data = kmalloc(buffer->dataLength, GFP_KERNEL);

	return SLSI_MIB_STATUS_SUCCESS;
}

static int kunit_mock_slsi_mib_encode_get_list(struct slsi_mib_data *buffer, u16 psids_length,
					       const struct slsi_mib_get_entry *psids)
{
	return 0;
}

static u16 kunit_mock_slsi_mib_encode_bool(struct slsi_mib_data *buffer, u16 psid, bool value, u16 idx)
{
	return 0;
}

static size_t kunit_mock_slsi_mib_decode(struct slsi_mib_data *data, struct slsi_mib_entry *value)
{
	if (data->data[0] == 10 || data->data[0] == 0)
		return 0;

	if (data->data[0] == 1) {
		value->value.type = SLSI_MIB_TYPE_BOOL;
		value->value.u.boolValue = *(data->data);

		return 20;
	} else if (data->data[0] == 155) {
		value->value.type = SLSI_MIB_TYPE_INT;
		value->value.u.intValue = *(data->data);

		return 20;
	} else if (data->data[0] == 101) {
		value->value.type = SLSI_MIB_TYPE_UINT;
		value->value.u.uintValue = *(data->data);

		return 20;
	} else if (data->data[0] == 159) {
		value->index[0] = 1;
		value->index[1] = 2;
		value->value.type = SLSI_MIB_TYPE_OCTET;
		value->value.u.octetValue.dataLength = 12;
		value->value.u.octetValue.data = "HELLO_KUNIT";

		return 20;
	} else if (data->data[0] == 150) {
		value->value.type = SLSI_MIB_TYPE_NONE;

		return 100;
	}

	return 0;
}

static size_t kunit_mock_slsi_mib_decode_octet_str(u8 *buffer, struct slsi_mib_data *octet_value)
{
	return 0;
}

static struct slsi_mib_value *kunit_mock_slsi_mib_decode_get_list(struct slsi_mib_data *buffer, u16 psids_length,
								  const struct slsi_mib_get_entry *psids)
{
	struct slsi_mib_value *results;

	if (psids_length == 0)
		return NULL;

	results = kmalloc_array((size_t)psids_length, sizeof(struct slsi_mib_value), GFP_KERNEL);
	results[0].type = SLSI_MIB_TYPE_INT;
	results[0].u.uintValue = 63;

	return results;
}

static u8 *kunit_mock_slsi_mib_find(struct slsi_mib_data *buffer, const struct slsi_mib_get_entry *entry)
{
	return NULL;
}

static size_t kunit_mock_slsi_mib_encode_uint32(u8 *buffer, u32 value)
{
	return 0;
}

static void kunit_mock_slsi_mib_buf_append(struct slsi_mib_data *dst, size_t buffer_length, u8 *buffer)
{
	u8 *new_buffer = kmalloc(dst->dataLength + buffer_length, GFP_KERNEL);

	if (!new_buffer) {
		SLSI_ERR_NODEV("kmalloc(%d) failed\n", (int)(dst->dataLength + buffer_length));
		return;
	}

	if (dst->data) {
		memcpy(new_buffer, dst->data, dst->dataLength);
		kfree(dst->data);
	}

	if (buffer)
		memcpy(&new_buffer[dst->dataLength], buffer, buffer_length);

	dst->dataLength += (u16)buffer_length;
	dst->data = new_buffer;
}

static u16 kunit_mock_slsi_mib_encode(struct slsi_mib_data *buffer, struct slsi_mib_entry *value)
{
	u16 ret = SLSI_MIB_STATUS_FAILURE;

	if (value && value->value.u.octetValue.dataLength > 0) {
		slsi_mib_buf_append(buffer,
				    value->value.u.octetValue.dataLength,
				    value->value.u.octetValue.data);
		ret = SLSI_MIB_STATUS_SUCCESS;
	}

	return ret;
}
#endif
