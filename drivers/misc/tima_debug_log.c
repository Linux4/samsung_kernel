#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "tima_debug_log.h"

int read_log_from_tw(char *log_buffer, uint32_t log_size, uint32_t cmd_id)
{
    TEEC_Result teeRet = TEEC_SUCCESS;
    TEEC_SharedMemory sharedMem;
    TEEC_Session log_session;
    TEEC_Context log_context;
    TEEC_Operation op = {0,};
    TEEC_UUID log_uuid = TIMA_UTIL_SRV_UUID;
    uint32_t returnOrigin;
    int ret = 0;

	teeRet = TEEC_InitializeContext(NULL, &log_context);
	if(teeRet  != TEEC_SUCCESS) {
	    pr_err("TIMA: tima_debug_log_init--failed to initialize, teeRet = %x.\n", teeRet);
		return -1;
	}
	teeRet = TEEC_OpenSession(&log_context, &log_session, &log_uuid, TEEC_LOGIN_APPLICATION, NULL, &op, &returnOrigin);
	if(teeRet  != TEEC_SUCCESS) {
	    pr_err("TIMA: tima_debug_log_init--failed to Open Session, teeRet = %x. returnOrigin = %x \n", teeRet,returnOrigin);
        TEEC_FinalizeContext(&log_context);
		return -1;
	}
	sharedMem.buffer = log_buffer;
	sharedMem.size = log_size;
	sharedMem.flags = TEEC_MEM_OUTPUT;
	teeRet = TEEC_RegisterSharedMemory(&log_context, &sharedMem);
	if(teeRet  != TEEC_SUCCESS) {
	    pr_err("TIMA: tima_debug_log--failed to register share memory, teeRet = %x.\n", teeRet);
        TEEC_CloseSession(&log_session);
        TEEC_FinalizeContext(&log_context);
        return -1;
	}
	op.paramTypes = TEEC_PARAM_TYPES(
		 TEEC_MEMREF_WHOLE, TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sharedMem;
	op.params[0].memref.size = log_size;
	op.params[0].memref.offset = 0;

	teeRet = TEEC_InvokeCommand(&log_session, cmd_id , &op, &returnOrigin);
	if(teeRet  != TEEC_SUCCESS) {
	    pr_err("TEEC_InvokeCommand failed, teeRet = %x,returnOrigin = %x\n", teeRet, returnOrigin);
		ret = -1;
    }
    TEEC_ReleaseSharedMemory(&sharedMem);
    TEEC_CloseSession(&log_session);
    TEEC_FinalizeContext(&log_context);
    return ret;
}

ssize_t	tima_read(struct file *filep, char __user *buf, size_t size, loff_t *offset)
{
    char *log_buffer = NULL;
    uint32_t log_size;
    uint32_t cmd_id;
    int ret = 0;

    if (!strcmp(filep->f_path.dentry->d_iname, "tima_debug_log"))
    {
        cmd_id = TIMAUTIL_DEBUG_LOG_READ;
        log_size = DEBUG_LOG_SIZE;
    }
    else if (!strcmp(filep->f_path.dentry->d_iname, "tima_secure_log"))
    {
        cmd_id = TIMAUTIL_SECURE_LOG_READ;
        log_size = SECURE_LOG_SIZE;
    }
    else
    {
        cmd_id = TIMAUTIL_DEBUG_LOG_READ;
        log_size = DEBUG_LOG_SIZE;
    }

    /* First check is to get rid of integer overflow exploits */
    if (size > log_size || (*offset) + size > log_size) {
        printk(KERN_ERR"TIMA: Extra read\n");
        return -EINVAL;
    }

    log_buffer = vmalloc(log_size);
    if (!log_buffer)
    {
        printk(KERN_ERR"TIMA: log_buffer memory allocation failed\n");
        return -ENOMEM;
    }
    ret=read_log_from_tw(log_buffer,log_size,cmd_id);
    if(!ret)
    {
        if (copy_to_user(buf,log_buffer + (*offset), size)) {
            printk(KERN_ERR"TIMA: Copy to user failed\n");
            ret = -1;
        } else {
            *offset += size;
            ret = size;
        }
    }
    else
    {
        pr_warn("TIMA: read_log_from_tw is failed \n");
        ret = -1;
    }
    vfree(log_buffer);
    return ret;
}

static const struct file_operations tima_proc_fops = {
	.read		= tima_read,
};

/**
 *      tima_debug_log_read_init -  Initialization function for TIMA
 *
 *      It creates and initializes tima proc entry with initialized read handler
 */
static int __init tima_log_read_init(void)
{
    int ret = 0;

    if (proc_create("tima_debug_log", 0644,NULL, &tima_proc_fops) == NULL) {
        printk(KERN_ERR"tima_log_read_init: Error creating proc entry tima_debug_log \n");
        ret = -1;
    }
    if (proc_create("tima_secure_log", 0644,NULL, &tima_proc_fops) == NULL) {
        printk(KERN_ERR"tima_log_read_init: Error creating proc entry tima_secure_log \n");
        remove_proc_entry("tima_debug_log", NULL);
        ret = -1;
    }
    return ret;
}

late_initcall(tima_log_read_init);
MODULE_DESCRIPTION(DRIVER_DESC);