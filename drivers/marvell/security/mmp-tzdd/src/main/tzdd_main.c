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

MODULE_LICENSE("GPL");

/*
 * Parameters which can be set at load time.
 */

#define TZDD_VERSION    "TEEC Drvier Version 1.0.4"

int tzdd_major;
int tzdd_minor;

module_param(tzdd_major, int, S_IRUGO);
module_param(tzdd_minor, int, S_IRUGO);

/* driver data struct */
tzdd_dev_t *tzdd_dev;
static struct class *_tzdd_class;

#define OP_PARAM_NUM        4

static int tzdd_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int tzdd_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static long tzdd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	TEEC_Result result;

	TZDD_DBG("tzdd_ioctl\n");

	OSA_ASSERT(arg);

	switch (cmd) {
	case TEEC_INIT_CONTEXT:
	{
		teec_init_context_args teec_init_context;
		if (copy_from_user
		    (&teec_init_context, (void __user *)arg,
		     sizeof(teec_init_context_args))) {
			return -EFAULT;
		}

		result =
		    _teec_initialize_context(teec_init_context.name,
					     &(teec_init_context.
					       tee_ctx_ntw));
		teec_init_context.ret = result;

		if (copy_to_user
		    ((void __user *)arg, &teec_init_context,
		     sizeof(teec_init_context_args))) {
			return -EFAULT;
		}
	}
	break;
	case TEEC_FINAL_CONTEXT:
	{
		teec_final_context_args teec_final_context;
		if (copy_from_user
		    (&teec_final_context, (void __user *)arg,
		     sizeof(teec_final_context_args))) {
			return -EFAULT;
		}

		OSA_ASSERT(teec_final_context.tee_ctx_ntw);

		result =
		    _teec_final_context(teec_final_context.tee_ctx_ntw);
		teec_final_context.ret = result;

		if (copy_to_user
		    ((void __user *)arg, &teec_final_context,
		     sizeof(teec_final_context_args))) {
			return -EFAULT;
		}
	}
	break;
	case TEEC_REGIST_SHARED_MEM:
	case TEEC_ALLOC_SHARED_MEM:
	{
		teec_map_shared_mem_args teec_map_shared_mem;
		void *buf_local_virt;
		tee_memm_ss_t memm_handle;

		if (copy_from_user
			(&teec_map_shared_mem, (void __user *)arg,
			sizeof(teec_map_shared_mem_args))) {
			return -EFAULT;
		}

		memm_handle = tee_memm_create_ss();
		tee_memm_get_user_mem(memm_handle,
				      teec_map_shared_mem.buffer,
				      teec_map_shared_mem.size,
				      &buf_local_virt);
		if (!buf_local_virt) {
			TZDD_DBG("Error in tee_memm_get_user_mem\n");
			return -EFAULT;
		}
		tee_memm_destroy_ss(memm_handle);

		teec_map_shared_mem.buffer = buf_local_virt;

		OSA_ASSERT(teec_map_shared_mem.tee_ctx_ntw);
		result =
		    _teec_map_shared_mem(teec_map_shared_mem,
					 &(teec_map_shared_mem.
					   tee_shm_ntw));
		teec_map_shared_mem.ret = result;

		if (copy_to_user
		    ((void __user *)arg, &teec_map_shared_mem,
		     sizeof(teec_map_shared_mem_args))) {
			return -EFAULT;
		}
	}
	break;
	case TEEC_RELEASE_SHARED_MEM:
	{
		teec_unmap_shared_mem_args teec_unmap_shared_mem;
		if (copy_from_user
		    (&teec_unmap_shared_mem, (void __user *)arg,
		     sizeof(teec_unmap_shared_mem_args))) {
			return -EFAULT;
		}

		OSA_ASSERT(teec_unmap_shared_mem.tee_shm_ntw);
		result =
		    _teec_unmap_shared_mem(teec_unmap_shared_mem,
					   teec_unmap_shared_mem.
					   tee_shm_ntw);
		teec_unmap_shared_mem.ret = result;

		if (copy_to_user
		    ((void __user *)arg, &teec_unmap_shared_mem,
		     sizeof(teec_unmap_shared_mem_args))) {
			return -EFAULT;
		}
	}
	break;
	case TEEC_OPEN_SS:
	{
		uint32_t param_types, tmp_types;
		uint32_t i;
		uint8_t *tmpref_buf[OP_PARAM_NUM];
		void *save_buf_addr[OP_PARAM_NUM];

		teec_open_ss_args teec_open_ss;
		TEEC_UUID destination;
		uint32_t _connectionData;
		void *connectionData;

		for (i = 0; i < OP_PARAM_NUM; i++) {
			tmpref_buf[i] = NULL;
			save_buf_addr[i] = NULL;
		}

		if (copy_from_user
		    (&teec_open_ss, (void __user *)arg,
		     sizeof(teec_open_ss_args))) {
			return -EFAULT;
		}

		if (teec_open_ss.destination) {
			if (copy_from_user
			    (&destination,
			     (void __user *)teec_open_ss.destination,
			     sizeof(TEEC_UUID))) {
				return -EFAULT;
			}
		}

		if (teec_open_ss.connectionData) {
			if (copy_from_user
			    (&_connectionData,
			     (void __user *)teec_open_ss.connectionData,
			     sizeof(uint32_t))) {
				return -EFAULT;
			}
			connectionData = (void *)&_connectionData;
		} else {
			connectionData = NULL;
		}

		if (teec_open_ss.flags == OPERATION_EXIST) {
			param_types = teec_open_ss.paramTypes;
			for (i = 0; i < OP_PARAM_NUM; i++) {
				tmp_types =
				    TEEC_PARAM_TYPE_GET(param_types, i);
				switch (tmp_types) {
				case TEEC_MEMREF_TEMP_INPUT:
				case TEEC_MEMREF_TEMP_OUTPUT:
				case TEEC_MEMREF_TEMP_INOUT:
				{
					if (NULL !=
						teec_open_ss.params[i]
						.tmpref.buffer) {
						tmpref_buf[i] = kmalloc(
							teec_open_ss.params[i]
							.tmpref.size,
							GFP_KERNEL);
						if (NULL == tmpref_buf[i]) {
							TZDD_DBG
							("%s(%d): Failed to \
							allocate buffer\n",
							__func__,
							__LINE__);
							return -ENOMEM;
						}
						if (copy_from_user(
							tmpref_buf[i],
							(void __user *)
							(teec_open_ss.params[i].tmpref.buffer),
							teec_open_ss.params[i].tmpref.size)) {
							for (i = 0; i < OP_PARAM_NUM; i++) {
								if (NULL != tmpref_buf[i])
									kfree(tmpref_buf[i]);
							}
								return
								    -EFAULT;
						}
							save_buf_addr[i] =
							    teec_open_ss.params[i].tmpref.buffer;
							teec_open_ss.params[i].tmpref.buffer =
							    tmpref_buf[i];
					}
				}
				break;
				case TEEC_MEMREF_WHOLE:
				case TEEC_MEMREF_PARTIAL_INPUT:
				case TEEC_MEMREF_PARTIAL_OUTPUT:
				case TEEC_MEMREF_PARTIAL_INOUT:
				{
					TEEC_MRVL_SharedMemory *teec_mrvl_shared_mem;

					save_buf_addr[i] =
						teec_open_ss.params[i].memref.parent;
					teec_mrvl_shared_mem =
						(TEEC_MRVL_SharedMemory *) (save_buf_addr[i]);
					teec_open_ss.params[i].memref.parent =
						container_of(teec_mrvl_shared_mem,
						TEEC_SharedMemory,
						imp);
				}
				break;
				default:
					break;
				}
			}
		}
		OSA_ASSERT(teec_open_ss.tee_ctx_ntw);
		result = _teec_open_session(
					    teec_open_ss.tee_ctx_ntw,
					    &(teec_open_ss.tee_ss_ntw),
					    (const TEEC_UUID
					     *)(&destination),
					    teec_open_ss.connectionMethod,
					    connectionData,
					    teec_open_ss.flags,
					    teec_open_ss.paramTypes,
					    &(teec_open_ss.params),
					    &(teec_open_ss.tee_op_ntw),
					    teec_open_ss.tee_op_ntw_for_cancel,
					    &(teec_open_ss.returnOrigin), arg,
					    teec_open_ss);
		teec_open_ss.ret = result;

		if (teec_open_ss.flags == OPERATION_EXIST) {
			if (TEEC_SUCCESS == teec_open_ss.ret) {
				param_types = teec_open_ss.paramTypes;
				for (i = 0; i < OP_PARAM_NUM; i++) {
					tmp_types =
					    TEEC_PARAM_TYPE_GET
					    (param_types, i);
					switch (tmp_types) {
					case TEEC_MEMREF_TEMP_INPUT:
					case TEEC_MEMREF_TEMP_OUTPUT:
					case TEEC_MEMREF_TEMP_INOUT:
					{
						if (NULL !=
							teec_open_ss.params[i].tmpref.buffer) {
							teec_open_ss.params[i].tmpref.buffer =
								save_buf_addr[i];
							if (copy_to_user((void __user *)
								(teec_open_ss.params[i].tmpref.buffer),
								tmpref_buf[i],
								teec_open_ss.params[i].tmpref.size)) {
								for (i = 0; i < OP_PARAM_NUM; i++)
									if (NULL != tmpref_buf[i])
										kfree(tmpref_buf[i]);
								return -EFAULT;
							}
						}
					}
					break;
					default:
						break;
					}
				}
			}
		}
		if (copy_to_user
		    ((void __user *)arg, &teec_open_ss,
		     sizeof(teec_open_ss_args))) {
			for (i = 0; i < OP_PARAM_NUM; i++)
				if (NULL != tmpref_buf[i])
					kfree(tmpref_buf[i]);

			return -EFAULT;
		}
		for (i = 0; i < OP_PARAM_NUM; i++)
			if (NULL != tmpref_buf[i])
				kfree(tmpref_buf[i]);
	}
		break;
	case TEEC_CLOSE_SS:
	{
		teec_close_ss_args teec_close_ss;
		if (copy_from_user
			(&teec_close_ss, (void __user *)arg,
			sizeof(teec_close_ss_args)))
			return -EFAULT;

		OSA_ASSERT(teec_close_ss.tee_ss_ntw);

		result = _teec_close_session(teec_close_ss.tee_ss_ntw);
		teec_close_ss.ret = result;

		if (copy_to_user
			((void __user *)arg, &teec_close_ss,
			sizeof(teec_close_ss_args)))
			return -EFAULT;

	}
	break;
	case TEEC_INVOKE_CMD:
	{
		uint32_t param_types, tmp_types;
		uint32_t i;
		uint8_t *tmpref_buf[OP_PARAM_NUM];
		void *save_buf_addr[OP_PARAM_NUM];

		teec_invoke_cmd_args teec_invoke_cmd;
		if (copy_from_user(&teec_invoke_cmd,
			(void __user *)arg,
			sizeof(teec_invoke_cmd_args)))
			return -EFAULT;

		for (i = 0; i < OP_PARAM_NUM; i++) {
			tmpref_buf[i] = NULL;
			save_buf_addr[i] = NULL;
		}

		if (teec_invoke_cmd.flags == OPERATION_EXIST) {
			param_types = teec_invoke_cmd.paramTypes;
			for (i = 0; i < OP_PARAM_NUM; i++) {
				tmp_types =
					TEEC_PARAM_TYPE_GET(param_types, i);
				switch (tmp_types) {
				case TEEC_MEMREF_TEMP_INPUT:
				case TEEC_MEMREF_TEMP_OUTPUT:
				case TEEC_MEMREF_TEMP_INOUT:
				{
					if (NULL !=
						teec_invoke_cmd.params[i].tmpref.buffer) {
						tmpref_buf[i] = kmalloc
							(teec_invoke_cmd.
							 params[i].
							 tmpref.
							 size,
							 GFP_KERNEL);
						if (NULL == tmpref_buf[i]) {
							TZDD_DBG
								("%s(%d): Failed to allocate buffer\n",
								 __func__,
								 __LINE__);
							return -ENOMEM;
						}

						if (copy_from_user(tmpref_buf[i],
							(void __user *)
							(teec_invoke_cmd.params[i].tmpref.buffer),
							teec_invoke_cmd.params[i].tmpref.size)) {
							for (i = 0; i < OP_PARAM_NUM; i++)
								if (NULL != tmpref_buf[i])
									kfree(tmpref_buf[i]);

							return -EFAULT;
						}
						save_buf_addr[i] =
							teec_invoke_cmd.params[i].tmpref.buffer;
						teec_invoke_cmd.params[i].tmpref.buffer =
							tmpref_buf[i];
					}
				}
				break;
				case TEEC_MEMREF_WHOLE:
				case TEEC_MEMREF_PARTIAL_INPUT:
				case TEEC_MEMREF_PARTIAL_OUTPUT:
				case TEEC_MEMREF_PARTIAL_INOUT:
				{
					TEEC_MRVL_SharedMemory *teec_mrvl_shared_mem;

					save_buf_addr[i] =
						teec_invoke_cmd.params[i].memref.parent;
					teec_mrvl_shared_mem =
						(TEEC_MRVL_SharedMemory *) (save_buf_addr[i]);
					teec_invoke_cmd.params[i].memref.parent =
						container_of(teec_mrvl_shared_mem,
							TEEC_SharedMemory,
							imp);
				}
				break;
				default:
					break;
				}
			}
		}
		OSA_ASSERT(teec_invoke_cmd.tee_ss_ntw);
		result = _teec_invoke_command(teec_invoke_cmd.tee_ss_ntw,
						teec_invoke_cmd.cmd_ID,
						teec_invoke_cmd.started,
						teec_invoke_cmd.flags,
						teec_invoke_cmd.paramTypes,
						&(teec_invoke_cmd.params),
						&(teec_invoke_cmd.tee_op_ntw),
						teec_invoke_cmd.tee_op_ntw_for_cancel,
						&(teec_invoke_cmd.returnOrigin),
						arg,
						teec_invoke_cmd);
		teec_invoke_cmd.ret = result;

		if (teec_invoke_cmd.flags == OPERATION_EXIST) {
			if (TEEC_SUCCESS == teec_invoke_cmd.ret) {
				param_types =
				    teec_invoke_cmd.paramTypes;
				for (i = 0; i < OP_PARAM_NUM; i++) {
					tmp_types =
						TEEC_PARAM_TYPE_GET(param_types, i);
					switch (tmp_types) {
					case TEEC_MEMREF_TEMP_INPUT:
					case TEEC_MEMREF_TEMP_OUTPUT:
					case TEEC_MEMREF_TEMP_INOUT:
					{
						if (NULL !=
							teec_invoke_cmd.params[i].tmpref.buffer) {
							teec_invoke_cmd.params[i].tmpref.buffer =
								save_buf_addr[i];
							if (copy_to_user(
								(void __user *)
								(teec_invoke_cmd.params[i].tmpref.buffer),
								tmpref_buf[i],
								teec_invoke_cmd.params[i].tmpref.size)) {
								for (i = 0; i < OP_PARAM_NUM; i++)
									if (NULL != tmpref_buf[i])
										kfree(tmpref_buf[i]);

								return -EFAULT;
							}
						}
					}
					break;
					default:
						break;
					}
				}
			}
		}
		if (copy_to_user
			((void __user *)arg, &teec_invoke_cmd,
			sizeof(teec_invoke_cmd_args))) {
			for (i = 0; i < OP_PARAM_NUM; i++)
				if (NULL != tmpref_buf[i])
					kfree(tmpref_buf[i]);

			return -EFAULT;
		}
		for (i = 0; i < OP_PARAM_NUM; i++)
			if (NULL != tmpref_buf[i])
				kfree(tmpref_buf[i]);
	}
	break;
	case TEEC_REQUEST_CANCEL:
	{
		teec_requeset_cancel_args teec_requeset_cancel;
		if (copy_from_user
			(&teec_requeset_cancel, (void __user *)arg,
			sizeof(teec_requeset_cancel_args)))
			return -EFAULT;

		OSA_ASSERT(teec_requeset_cancel.tee_op_ntw);

		result =
			_teec_request_cancellation(teec_requeset_cancel.tee_op_ntw);
		teec_requeset_cancel.ret = result;

		if (copy_to_user
			((void __user *)arg, &teec_requeset_cancel,
			sizeof(teec_requeset_cancel_args))) {
			return -EFAULT;
		}
	}
	break;
	default:
		TZDD_DBG("default\n");
		TZDD_DBG("GEU IOCTL invald command %x\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

extern osa_sem_t _g_pm_lock;
static int32_t tzdd_suspend(struct platform_device *dev, pm_message_t state)
{
	int32_t ret;
	ret = (osa_try_to_wait_for_sem(_g_pm_lock) ? -1 : 0);
	return ret;
}

static int32_t tzdd_resume(struct platform_device *dev)
{
	osa_release_sem(_g_pm_lock);
	return 0;
}

/* driver file ops */
static struct file_operations _tzdd_fops = {
	.open = tzdd_open,
	.release = tzdd_close,
	.unlocked_ioctl = tzdd_ioctl,
	.owner = THIS_MODULE,
};

static struct platform_driver _tzdd_drv = {
	.suspend = tzdd_suspend,
	.resume = tzdd_resume,
	.driver = {
			.name = "tzdd",
			.owner = THIS_MODULE,
			},
};

static int32_t tzdd_cdev_init(tzdd_dev_t *dev)
{
	int32_t err, dev_nr;

	OSA_ASSERT(dev);

	if (tzdd_major) {
		dev_nr = MKDEV(tzdd_major, tzdd_minor);
		err = register_chrdev_region(dev_nr, 1, "tzdd");
	} else {
		err = alloc_chrdev_region(&dev_nr, 0, 1, "tzdd");
		tzdd_major = MAJOR(dev_nr);
		tzdd_minor = MINOR(dev_nr);
	}

	if (err < 0)
		goto _err0;

	cdev_init(&dev->cdev, &_tzdd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &_tzdd_fops;

	err = cdev_add(&dev->cdev, dev_nr, 1);

	/* Fail gracefully if need be */
	if (err)
		goto _err1;

	err = platform_driver_register(&_tzdd_drv);
	if (err)
		goto _err2;

	_tzdd_class = class_create(THIS_MODULE, "tzdd");
	if (IS_ERR(_tzdd_class))
		goto _err3;
	device_create(_tzdd_class, NULL,
			MKDEV(tzdd_major, tzdd_minor), NULL, "tzdd");

	return 0;

_err3:
	platform_driver_unregister(&_tzdd_drv);
_err2:
	cdev_del(&dev->cdev);
_err1:
	unregister_chrdev_region(MKDEV(tzdd_major, tzdd_minor), 1);
_err0:
	return -1;
}

static void tzdd_cdev_cleanup(tzdd_dev_t *dev)
{
	OSA_ASSERT(dev);

	device_destroy(_tzdd_class, MKDEV(tzdd_major, tzdd_minor));
	class_destroy(_tzdd_class);
	platform_driver_unregister(&_tzdd_drv);
	cdev_del(&dev->cdev);
	unregister_chrdev_region(MKDEV(tzdd_major, tzdd_minor), 1);

	return;
}

static int32_t __init tzdd_init(void)
{
	unsigned int ret = 0;

	TZDD_DBG("tzdd init\n");

	/* allocate driver struct */
	tzdd_dev = kmalloc(sizeof(tzdd_dev_t), GFP_KERNEL);
	if (NULL == tzdd_dev) {
		TZDD_DBG("%s(%d): Failed to allocate buffer\n", __func__,
			 __LINE__);
		return -ENOMEM;
	}
	memset(tzdd_dev, 0, sizeof(tzdd_dev_t));

	ret = tzdd_cdev_init(tzdd_dev);
	if (ret < 0) {
		TZDD_DBG("Failed to init cdev\n");
		goto _fail_in_adding_cdev;
	}

	ret = tzdd_init_core(tzdd_dev);
	if (ret) {
		TZDD_DBG("%s: failed to init core\n", __func__);
		goto _fail_in_init_core;
	}

	printk(KERN_ERR "%s\n", TZDD_VERSION);
	return 0;

_fail_in_init_core:
	tzdd_cdev_cleanup(tzdd_dev);
_fail_in_adding_cdev:
	if (NULL != tzdd_dev)
		kfree(tzdd_dev);

	return 0;
}

static void __exit tzdd_cleanup(void)
{
	TZDD_DBG("tzdd cleanup\n");
	tzdd_cleanup_core(tzdd_dev);
	tzdd_cdev_cleanup(tzdd_dev);
	if (NULL != tzdd_dev)
		kfree(tzdd_dev);

	return;
}

module_init(tzdd_init);
module_exit(tzdd_cleanup);
