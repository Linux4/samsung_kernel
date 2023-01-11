#include <asm/uaccess.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <tee_client_api.h>
#include <asm/sections.h>

#define TEE_TIMA_PKM_SRV_UUID \
     { \
         0xfcfcfcfc, 0x0000, 0x0000, \
         {  \
             0x00, 0x00, 0x00, 0x00, \
             0x00, 0x00, 0x00, 0x05, \
         }, \
     }
#define CMD_TIMA_PKM_STORE_PHYADD              0x00000011
#define KERNEL_START_VIRT_ADDR 0xFFFFFFC000000000
//#define SHARED_MEM_SIZE 12582912  //32 MB
#define SHARED_MEM_SIZE (_end - _stext)

TEEC_Context pkm_context;
TEEC_Session pkm_session;
static const TEEC_UUID pkm_uuid = TEE_TIMA_PKM_SRV_UUID;

int virt_addr_store(void)
{
	TEEC_Result teeRet = TEEC_SUCCESS;
	TEEC_Operation      op;
	TEEC_SharedMemory sharedMem;
	uint32_t returnOrigin;
	int ret = -1;

	pr_err("TIMA: Total shared memory size for TIMA PKM = 0x%x, _stext = 0x%x, _end = 0x%x\n", SHARED_MEM_SIZE, _stext, _end);
	teeRet = TEEC_InitializeContext(NULL, &pkm_context);
	if(teeRet  != TEEC_SUCCESS) {
	    pr_err("TIMA: virt_addr_store--failed to initialize, teeRet = %x.\n", teeRet);
		return ret;
	}
	memset(&op, 0, sizeof(TEEC_Operation));
	op.paramTypes = TEEC_PARAM_TYPES(
		 TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	teeRet = TEEC_OpenSession(&pkm_context, &pkm_session, &pkm_uuid, TEEC_LOGIN_APPLICATION, NULL, &op, NULL);
	if(teeRet  != TEEC_SUCCESS) {
	    pr_err("TIMA: virt_addr_store--failed to Open Session, teeRet = %x.\n", teeRet);
		goto pkm_error_ret;
	}
	sharedMem.buffer = (uint64_t *)KERNEL_START_VIRT_ADDR;
	sharedMem.size = SHARED_MEM_SIZE;
	sharedMem.flags = TEEC_MEM_INPUT;
	teeRet = TEEC_RegisterSharedMemory(&pkm_context, &sharedMem);
	if(teeRet  != TEEC_SUCCESS) {
	    pr_err("TIMA: virt_addr_store--failed to register share memory, teeRet = %x.\n", teeRet);
		goto pkm_error_ret;
	}
	op.paramTypes = TEEC_PARAM_TYPES(
		 TEEC_MEMREF_WHOLE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &sharedMem;
	op.params[0].memref.size = SHARED_MEM_SIZE;
	op.params[0].memref.offset = 0;
	teeRet = TEEC_InvokeCommand(&pkm_session, CMD_TIMA_PKM_STORE_PHYADD, &op, &returnOrigin);
	if(teeRet  != TEEC_SUCCESS) {
	    pr_err("TIMA: virt_addr_store--failed to send cmd to tw, teeRet = %x,returnOrigin = %x\n", teeRet, returnOrigin);
		goto pkm_error_ret;
	}
	pr_err("TIMA: virt_addr_store, success\n");
	ret = 0;

pkm_error_ret:
	TEEC_CloseSession(&pkm_session);
	return ret;
}

static int __init tima_virt_add_init(void)
{
	int tw_ret =0 ;
	tw_ret = virt_addr_store();
	if(tw_ret) {
       printk(KERN_ERR "virt_addr_store is failed \n");
	    return tw_ret;
	 }
	 return 0;
}

MODULE_AUTHOR("Yogesh Gupta, yogesh.gupta@samsung.com");
MODULE_DESCRIPTION("TIMA virtual address store driver");
MODULE_VERSION("0.1");

late_initcall(tima_virt_add_init);
