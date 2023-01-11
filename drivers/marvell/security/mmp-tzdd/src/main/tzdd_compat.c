/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */

#include "tzdd_internal.h"

#define IS_TYPE_NONE(_m)    (0x0 == (_m))
#define IS_TYPE_VALUE(_m)   ((_m) >= 0x1 && (_m) <= 0x3)
#define IS_TYPE_TMPREF(_m)  ((_m) >= 0x5 && (_m) <= 0x7)
#define IS_TYPE_MEMREF(_m)  ((_m) >= 0xc && (_m) <= 0xf)

#define CLEAN_HIGH_32BIT    (0xFFFFFFFF)
#define _HIGH_MEM_TRSLATION(_a) ((((_a) >> 8) & 0x000000ff) | 0xffffff00)

struct compat_teec_init_context_args {
	compat_int_t name;
	compat_u64 tee_ctx_ntw;
	compat_uint_t ret;
};

struct compat_teec_final_context_args {
	compat_u64 tee_ctx_ntw;
	compat_uint_t ret;
};

struct compat_teec_map_shared_mem_args {
	compat_u64 tee_ctx_ntw;
	compat_int_t buffer;
	compat_size_t size;
	compat_uint_t flags;
	compat_u64 tee_shm_ntw;
	compat_uint_t ret;
};

struct compat_teec_unmap_shared_mem_args {
	compat_int_t buffer;
	compat_size_t size;
	compat_uint_t flags;
	compat_u64 tee_shm_ntw;
	compat_uint_t ret;
};

struct compat_teec_temp_ref {
	compat_int_t buffer;
	compat_size_t size;
};

struct compat_teec_mem_ref {
	compat_int_t parent;
	compat_size_t size;
	compat_size_t offset;
};

struct compat_teec_value {
	compat_uint_t a;
	compat_uint_t b;
};

union compat_teec_parameter {
	struct compat_teec_temp_ref tmpref;
	struct compat_teec_mem_ref memref;
	struct compat_teec_value value;
};

struct compat_teec_open_ss_args {
	compat_u64 tee_ctx_ntw;
	compat_u64 tee_ss_ntw;
	compat_int_t destination;
	compat_uint_t connectionMethod;
	compat_int_t connectionData;
	compat_uint_t flags;
	compat_uint_t paramTypes;
	union compat_teec_parameter params[4];
	compat_u64 tee_op_ntw;
	compat_uint_t tee_op_ntw_for_cancel;
	compat_uint_t returnOrigin;
	compat_uint_t ret;
};

struct compat_teec_close_ss_args {
	compat_u64 tee_ss_ntw;
	compat_uint_t ret;
};

struct compat_teec_request_cancel_args {
	compat_u64 tee_op_ntw;
	compat_uint_t ret;
};

struct compat_teec_invoke_cmd_args {
	compat_u64 tee_ss_ntw;
	compat_uint_t cmd_ID;
	compat_uint_t started;
	compat_uint_t flags;
	compat_uint_t paramTypes;
	union compat_teec_parameter params[4];
	compat_u64 tee_op_ntw;
	compat_uint_t tee_op_ntw_for_cancel;
	compat_uint_t returnOrigin;
	compat_uint_t ret;
};

static int _tzdd_compat_get_init_context_data(
			struct compat_teec_init_context_args __user *data32,
			teec_init_context_args __user *data)
{
	data->name = compat_ptr(data32->name);

	return 0;
}

static int _tzdd_compat_put_init_context_data(
			struct compat_teec_init_context_args __user *data32,
			teec_init_context_args __user *data)
{
	compat_uint_t u;
	int err = 0;

	data32->tee_ctx_ntw = (compat_u64)data->tee_ctx_ntw;

	err |= get_user(u, &data->ret);
	err |= put_user(u, &data32->ret);

	return err;
}

static int _tzdd_compat_get_final_context_data(
			struct compat_teec_final_context_args __user *data32,
			teec_final_context_args __user *data)
{

	data->tee_ctx_ntw = (tee_impl)data32->tee_ctx_ntw;

	return 0;
}

static int _tzdd_compat_put_final_context_data(
			struct compat_teec_final_context_args __user *data32,
			teec_final_context_args __user *data)
{
	compat_uint_t u;
	int err = 0;

	err |= get_user(u, &data->ret);
	err |= put_user(u, &data32->ret);

	return err;
}

static int _tzdd_compat_get_map_shared_mem_data(
	struct compat_teec_map_shared_mem_args __user *data32,
	teec_map_shared_mem_args __user *data)
{
	compat_size_t s;
	compat_uint_t u;
	int err = 0;

	data->tee_ctx_ntw = (tee_impl)data32->tee_ctx_ntw;
	data->buffer = compat_ptr(data32->buffer);
	err |= get_user(s, &data32->size);
	err |= put_user(s, &data->size);
	err |= get_user(u, &data32->flags);
	err |= put_user(u, &data->flags);

	if ((data->flags >> 16) == _HIGH_MEM_MAGIC) {
		data->buffer = (void *) ((_HIGH_MEM_TRSLATION((unsigned long long)data->flags) << 32) |
			(unsigned long long) data->buffer);
	}

	/* clean the flag */
	data->flags = data->flags & 0x000000ff;

	return err;
}

static int _tzdd_compat_put_map_shared_mem_data(
		struct compat_teec_map_shared_mem_args __user *data32,
		teec_map_shared_mem_args __user *data)
{
	compat_uint_t u;
	int err = 0;

	data32->tee_shm_ntw = (compat_u64)data->tee_shm_ntw;
	err |= get_user(u, &data->ret);
	err |= put_user(u, &data32->ret);

	return err;
}

static int _tzdd_compat_get_unmap_shared_mem_data(
	struct compat_teec_unmap_shared_mem_args __user *data32,
	teec_unmap_shared_mem_args __user *data)
{
	compat_size_t s;
	compat_uint_t u;
	int err = 0;

	data->buffer = compat_ptr(data32->buffer);
	err |= get_user(s, &data32->size);
	err |= put_user(s, &data->size);
	err |= get_user(u, &data32->flags);
	err |= put_user(u, &data->flags);
	data->tee_shm_ntw = (tee_impl)data32->tee_shm_ntw;

	return err;
}

static int _tzdd_compat_put_unmap_shared_mem_data(
	struct compat_teec_unmap_shared_mem_args __user *data32,
	teec_unmap_shared_mem_args __user *data)
{
	compat_uint_t u;
	int err = 0;

	err |= get_user(u, &data->ret);
	err |= put_user(u, &data32->ret);

	return err;
}
static int _tzdd_compat_get_open_ss_data(
			struct compat_teec_open_ss_args __user *data32,
			teec_open_ss_args __user *data)
{
	compat_uint_t u;
	compat_size_t s;
	int err = 0;
	int j;
	uint32_t value;

	data->tee_ctx_ntw = (tee_impl)data32->tee_ctx_ntw;
	data->destination = (TEEC_UUID *)compat_ptr(data32->destination);
	err |= get_user(u, &data32->connectionMethod);
	err |= put_user(u, &data->connectionMethod);
	data->connectionData = compat_ptr(data32->connectionData);
	err |= get_user(u, &data32->flags);
	err |= put_user(u, &data->flags);
	err |= get_user(u, &data32->paramTypes);
	err |= put_user(u, &data->paramTypes);

	for (j = 0; j < 4; j++) {
		value = TEEC_PARAM_TYPE_GET(data->paramTypes, j);
		if (IS_TYPE_NONE(value)) {
			osa_memset(&(data->params[j]), 0, sizeof(TEEC_Parameter));
		}
		if (IS_TYPE_VALUE(value)) {
			err |= get_user(u, &data32->params[j].value.a);
			err |= put_user(u, &data->params[j].value.a);
			err |= get_user(u, &data32->params[j].value.b);
			err |= put_user(u, &data->params[j].value.b);
		}
		if (IS_TYPE_TMPREF(value)) {
			data->params[j].tmpref.buffer =
				compat_ptr(data32->params[j].tmpref.buffer);
			err |= get_user(s, &(data32->params[j].tmpref.size));
			err |= put_user(s, &(data->params[j].tmpref.size));
		}
		if (IS_TYPE_MEMREF(value)) {
			data->params[j].memref.parent =
				compat_ptr(data32->params[j].memref.parent);
			err |= get_user(s, &(data32->params[j].memref.size));
			err |= put_user(s, &(data->params[j].memref.size));
			err |= get_user(s, &(data32->params[j].memref.offset));
			err |= put_user(s, &(data->params[j].memref.offset));
		}
	}

	data->tee_op_ntw_for_cancel = compat_ptr(data32->tee_op_ntw_for_cancel);

	return err;
}

static int _tzdd_compat_put_open_ss_data(
			struct compat_teec_open_ss_args __user *data32,
			teec_open_ss_args __user *data)
{
	compat_uint_t u;
	compat_size_t s;
	int err = 0;
	int j;
	uint32_t value;

	data32->tee_ss_ntw = (compat_u64)data->tee_ss_ntw;

	for (j = 0; j < 4; j++) {
		value = TEEC_PARAM_TYPE_GET(data->paramTypes, j);
		if (IS_TYPE_NONE(value)) {
			/* nothing */;
		}
		if (IS_TYPE_VALUE(value)) {
			err |= get_user(u, &data->params[j].value.a);
			err |= put_user(u, &data32->params[j].value.a);
			err |= get_user(u, &data->params[j].value.b);
			err |= put_user(u, &data32->params[j].value.b);
		}
		if (IS_TYPE_TMPREF(value)) {
			/* buffer address should not be changed */
			err |= get_user(s, &(data->params[j].tmpref.size));
			err |= put_user(s, &(data32->params[j].tmpref.size));
		}
		if (IS_TYPE_MEMREF(value)) {
			/* buffer address should not be changed */
			err |= get_user(s, &(data->params[j].memref.size));
			err |= put_user(s, &(data32->params[j].memref.size));
			err |= get_user(s, &(data->params[j].memref.offset));
			err |= put_user(s, &(data32->params[j].memref.offset));
		}
	}

	data32->tee_op_ntw = (compat_u64)data->tee_op_ntw;

	err |= get_user(u, &data->returnOrigin);
	err |= put_user(u, &data32->returnOrigin);
	err |= get_user(u, &data->ret);
	err |= put_user(u, &data32->ret);

	return err;
}

static int _tzdd_compat_get_close_ss_data(
			struct compat_teec_close_ss_args __user *data32,
			teec_close_ss_args __user *data)
{
	data->tee_ss_ntw = (tee_impl)(data32->tee_ss_ntw);

	return 0;
}

static int _tzdd_compat_put_close_ss_data(
			struct compat_teec_close_ss_args __user *data32,
			teec_close_ss_args __user *data)
{
	compat_uint_t u;
	int err = 0;

	err |= get_user(u, &data->ret);
	err |= put_user(u, &data32->ret);

	return err;
}

static int _tzdd_compat_get_request_cancel_data(
			struct compat_teec_request_cancel_args __user *data32,
			teec_request_cancel_args __user *data)
{
	data->tee_op_ntw = (tee_impl)(data32->tee_op_ntw);

	return 0;
}

static int _tzdd_compat_put_request_cancel_data(
			struct compat_teec_request_cancel_args __user *data32,
			teec_request_cancel_args __user *data)
{
	compat_uint_t u;
	int err = 0;

	err |= get_user(u, &data->ret);
	err |= put_user(u, &data32->ret);

	return err;
}

static int _tzdd_compat_get_invoke_cmd_data(
			struct compat_teec_invoke_cmd_args __user *data32,
			teec_invoke_cmd_args __user *data)
{
	compat_uint_t u;
	compat_size_t s;
	int err = 0;
	int j;
	uint32_t value;

	data->tee_ss_ntw = (tee_impl)(data32->tee_ss_ntw);
	err |= get_user(u, &data32->cmd_ID);
	err |= put_user(u, &data->cmd_ID);
	err |= get_user(u, &data32->started);
	err |= put_user(u, &data->started);
	err |= get_user(u, &data32->flags);
	err |= put_user(u, &data->flags);
	err |= get_user(u, &data32->paramTypes);
	err |= put_user(u, &data->paramTypes);

	for (j = 0; j < 4; j++) {
		value = TEEC_PARAM_TYPE_GET(data->paramTypes, j);
		if (IS_TYPE_NONE(value)) {
			osa_memset(&(data->params[j]), 0, sizeof(TEEC_Parameter));
		}
		if (IS_TYPE_VALUE(value)) {
			err |= get_user(u, &data32->params[j].value.a);
			err |= put_user(u, &data->params[j].value.a);
			err |= get_user(u, &data32->params[j].value.b);
			err |= put_user(u, &data->params[j].value.b);
		}
		if (IS_TYPE_TMPREF(value)) {
			data->params[j].tmpref.buffer =
				compat_ptr(data32->params[j].tmpref.buffer);
			err |= get_user(s, &(data32->params[j].tmpref.size));
			err |= put_user(s, &(data->params[j].tmpref.size));
		}
		if (IS_TYPE_MEMREF(value)) {
			data->params[j].memref.parent =
				compat_ptr(data32->params[j].memref.parent);
			err |= get_user(s, &(data32->params[j].memref.size));
			err |= put_user(s, &(data->params[j].memref.size));
			err |= get_user(s, &(data32->params[j].memref.offset));
			err |= put_user(s, &(data->params[j].memref.offset));
		}
	}

	data->tee_op_ntw_for_cancel = compat_ptr(data32->tee_op_ntw_for_cancel);

	return err;
}

static int _tzdd_compat_put_invoke_cmd_data(
			struct compat_teec_invoke_cmd_args __user *data32,
			teec_invoke_cmd_args __user *data)
{
	compat_uint_t u;
	compat_size_t s;
	int err = 0;
	int j;
	uint32_t value;

	for (j = 0; j < 4; j++) {
		value = TEEC_PARAM_TYPE_GET(data->paramTypes, j);
		if (IS_TYPE_NONE(value)) {
			/* nothing */;
		}
		if (IS_TYPE_VALUE(value)) {
			err |= get_user(u, &data->params[j].value.a);
			err |= put_user(u, &data32->params[j].value.a);
			err |= get_user(u, &data->params[j].value.b);
			err |= put_user(u, &data32->params[j].value.b);
		}
		if (IS_TYPE_TMPREF(value)) {
			/* buffer address should not be changed */
			err |= get_user(s, &(data->params[j].tmpref.size));
			err |= put_user(s, &(data32->params[j].tmpref.size));
		}
		if (IS_TYPE_MEMREF(value)) {
			/* buffer address should not be changed */
			err |= get_user(s, &(data->params[j].memref.size));
			err |= put_user(s, &(data32->params[j].memref.size));
			err |= get_user(s, &(data->params[j].memref.offset));
			err |= put_user(s, &(data32->params[j].memref.offset));
		}
	}

	data32->tee_op_ntw = (compat_u64)data->tee_op_ntw;

	err |= get_user(u, &data->returnOrigin);
	err |= put_user(u, &data32->returnOrigin);
	err |= get_user(u, &data->ret);
	err |= put_user(u, &data32->ret);

	return err;
}

long tzdd_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	OSA_ASSERT(arg);

	switch (cmd) {
	case TEEC_INIT_CONTEXT:
	{
		struct compat_teec_init_context_args __user *data32;
		teec_init_context_args __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		ret = _tzdd_compat_get_init_context_data(data32, data);
		if (ret)
			return ret;

		ret = filp->f_op->unlocked_ioctl(filp, cmd,
					(unsigned long)data);
		if (ret)
			return ret;

		ret = _tzdd_compat_put_init_context_data(data32, data);
		if (ret)
			return ret;
	}
	break;
	case TEEC_FINAL_CONTEXT:
	{
		struct compat_teec_final_context_args __user *data32;
		teec_final_context_args __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		ret = _tzdd_compat_get_final_context_data(data32, data);
		if (ret)
			return ret;

		ret = filp->f_op->unlocked_ioctl(filp, cmd,
					(unsigned long)data);
		if (ret)
			return ret;

		ret = _tzdd_compat_put_final_context_data(data32, data);
		if (ret)
			return ret;
	}
	break;
	case TEEC_REGIST_SHARED_MEM:
	/* fall through */;
	case TEEC_ALLOC_SHARED_MEM:
	{
		struct compat_teec_map_shared_mem_args __user *data32;
		teec_map_shared_mem_args __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		ret = _tzdd_compat_get_map_shared_mem_data(data32, data);
		if (ret)
			return ret;

		ret = filp->f_op->unlocked_ioctl(filp, cmd,
					(unsigned long)data);
		if (ret)
			return ret;

		ret = _tzdd_compat_put_map_shared_mem_data(data32, data);
		if (ret)
			return ret;
	}
	break;
	case TEEC_RELEASE_SHARED_MEM:
	{
		struct compat_teec_unmap_shared_mem_args __user *data32;
		teec_unmap_shared_mem_args __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (NULL == data)
			return -EFAULT;

		ret = _tzdd_compat_get_unmap_shared_mem_data(data32, data);
		if (ret)
			return ret;

		ret = filp->f_op->unlocked_ioctl(filp, cmd,
					(unsigned long)data);
		if (ret)
			return ret;

		ret = _tzdd_compat_put_unmap_shared_mem_data(data32, data);
		if (ret)
			return ret;
	}
	break;
	case TEEC_OPEN_SS:
	{
		struct compat_teec_open_ss_args __user *data32;
		teec_open_ss_args __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		ret = _tzdd_compat_get_open_ss_data(data32, data);
		if (ret)
			return ret;

		ret = filp->f_op->unlocked_ioctl(filp, cmd,
					(unsigned long)data);
		if (ret)
			return ret;

		ret = _tzdd_compat_put_open_ss_data(data32, data);
		if (ret)
			return ret;
	}
	break;
	case TEEC_CLOSE_SS:
	{
		struct compat_teec_close_ss_args __user *data32;
		teec_close_ss_args __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		ret = _tzdd_compat_get_close_ss_data(data32, data);
		if (ret)
			return ret;

		ret = filp->f_op->unlocked_ioctl(filp, cmd,
					(unsigned long)data);
		if (ret)
			return ret;

		ret = _tzdd_compat_put_close_ss_data(data32, data);
		if (ret)
			return ret;
	}
	break;
	case TEEC_INVOKE_CMD:
	{
		struct compat_teec_invoke_cmd_args __user *data32;
		teec_invoke_cmd_args __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		ret = _tzdd_compat_get_invoke_cmd_data(data32, data);
		if (ret)
			return ret;

		ret = filp->f_op->unlocked_ioctl(filp, cmd,
					(unsigned long)data);
		if (ret)
			return ret;

		ret = _tzdd_compat_put_invoke_cmd_data(data32, data);
		if (ret)
			return ret;
	}
	break;
	case TEEC_REQUEST_CANCEL:
	{
		struct compat_teec_request_cancel_args __user *data32;
		teec_request_cancel_args __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		ret = _tzdd_compat_get_request_cancel_data(data32, data);
		if (ret)
			return ret;

		ret = filp->f_op->unlocked_ioctl(filp, cmd,
					(unsigned long)data);
		if (ret)
			return ret;

		ret = _tzdd_compat_put_request_cancel_data(data32, data);
		if (ret)
			return ret;
	}
	break;
	default:
		TZDD_DBG("TZDD COMPAT IOCTL invald command %x\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}
