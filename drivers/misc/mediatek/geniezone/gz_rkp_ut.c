/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/sizes.h>

#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>

#include <gz-trusty/trusty_ipc.h>
#include <tz_cross/trustzone.h>
#include <tz_cross/ta_test.h>
#include <tz_cross/ta_system.h>
#include <kree/system.h>
#include <kree/mem.h>
#include <kree/tz_mod.h>
#include "unittest.h"

#include "gz_rkp_ut.h"

#define KREE_DEBUG(fmt...) pr_debug("[CM_kUT]" fmt)
#define KREE_INFO(fmt...) pr_info("[CM_kUT]" fmt)
#define KREE_ERR(fmt...) pr_info("[CM_kUT][ERR]" fmt)


/*vreg test UT items*/
#define test_vreg_basic 0
#define test_vreg_dyn_ipa_mmu_basic 1
#define test_vreg_dyn_ipa_mmu_threads 1
/*not supported now*/
#define test_vreg_from_gzha 0
#define test_Vmemory 0


/*the same in vm-common.h*/
enum vm_type_t {
	vm_invalid = 0,    /* invalid */
	vm_ram_rw,         /* RAM, read/write */
	vm_ram_ro,         /* RAM, read-only */
	vm_io_direct,      /* Device, read/write */
	vm_map_foreign,    /* RAM from foreign domain */
	vm_grant_map_rw,   /* Read/write grant mapping */
	vm_grant_map_ro,   /* Read-only grant mapping */
	/* set attribute only */
	vm_iommu_map_rw,   /* Read/write iommu mapping */
	vm_iommu_map_ro,   /* Read-only iommu mapping */
	vm_ram_rw_xn,      /* RAM, read/write, no execution*/
	vm_ram_ro_xn,      /* RAM, read only, no execution*/
	vm_ram_wo,         /* RAM, write-only */
	vm_io_rw,          /* Device, read/write, execute(xn=0) */
	vm_io_ro,          /* Device, read-only , execute(xn=0) */
	vm_io_rw_xn,       /* Device, read/write, no execute(xn=1) */
	vm_io_ro_xn,       /* Device, read-only , no execute(xn=1) */
	vm_max_real_type,  /* type end */
};

enum s1_attrs_t {
	MM_MEM_ATTRIB_INVALID = 0x0,
	MM_MEMATTRIB_READONLY = 0x1,
	MM_MEMATTRIB_WRITEONLY = 0x2,
	MM_MEM_ATTRIB_EXECUTE = 0x4,
	MM_MEM_ATTRIB_NORMAL = 0x8,
	MM_MEM_ATTRIB_DEVICE = 0x10,
	MM_MEM_ATTRIB_NONCACHEABLE = 0x20,
};

/*the same setting in gz*/
#define test_vreg_src_pa  0x1070A000  /*can modify*/
#define test_vreg_basic_size  0x1000  /*can modify*/

#define VREG_BASE_dyn_map (test_vreg_src_pa + 1 * test_vreg_basic_size)
#define VREG_BASE_gzha    (test_vreg_src_pa + 2 * test_vreg_basic_size)
#define VREG_BASE_basic   (test_vreg_src_pa + 3 * test_vreg_basic_size)
#define VREG_BASE_vmem    (test_vreg_src_pa + 4 * test_vreg_basic_size)

#define WORD_WIDTH 4 /*32-bit*/



#if test_vreg_from_gzha
/*trap a vreg by a GZ HA*/
int _vreg_test_from_gzha(void)
{
	KREE_SESSION echo_sn;
	int ret;
	union MTEEC_PARAM p[4];
	uint32_t paramTypes;

	/*session: echo svr */
	ret = _create_session(echo_srv_name, &echo_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("echo_sn create fail\n");
		return ret;
	}

	paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);

	/*TZCMD_TEST_VREG_FROM_HA*/
	ret = KREE_TeeServiceCall(echo_sn, 0x9004, paramTypes, p);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("[%s] Fail(0x%x)\n", __func__, ret);
		return ret;
	}

	/*close session */
	ret = _close_session(echo_sn);
	if (ret != TZ_RESULT_SUCCESS) {
		KREE_ERR("echo_sn close fail\n");
		return ret;
	}

	return TZ_RESULT_SUCCESS;
}
#endif


#if test_Vmemory
int vmemory_test(void)
{
	/*test data register*/
	int i;
	char *ptr;
	void __iomem *data_io = ioremap(VREG_BASE_vmem, test_vreg_basic_size);

	ptr = (char *) data_io;

	if (ptr) {
		for (i = 0; i < 4; i++)
			KREE_INFO("[%d]r[%d] = %c\n", __LINE__, i, ptr[i]);
	}

	if (data_io)
		iounmap(data_io);

	return TZ_RESULT_SUCCESS;
}
#endif

#if test_vreg_basic
int _vreg_test_basic(void)
{
	uint32_t v;
	int i;

	void __iomem *io = ioremap(VREG_BASE_basic, test_vreg_basic_size);

	KREE_INFO("[%s][%d] runs...\n", __func__, __LINE__);
	if (io) {
		KREE_INFO("[%d]vreg: write control reg:io\n", __LINE__);
		writel(0x00000002, io + (1 * WORD_WIDTH));
		writel(0x00000001, io + (2 * WORD_WIDTH));

		KREE_INFO("[%d]check write data\n", __LINE__);
		for (i = 0; i <= 4; i++) {
			v = readl(io + (i * WORD_WIDTH));
			KREE_INFO("[%d] read offset word[%d]=0x%x\n",
				__LINE__, i, v);
		}

	} else
		KREE_ERR("[%d]vreg: null io_1\n", __LINE__);

	if (io)
		iounmap(io);

	return TZ_RESULT_SUCCESS;
}
#endif

#if test_vreg_dyn_ipa_mmu_basic

/*Test: dynamic ipa mmu unmap/map/change type operations
 * Detail:
 * (1) alloc a buffer (buf) with size 64KB and update
 *     ipa type to vm_ram_ro_xn (read-only, no execution).
 * (2) because the test buffer is allocated in the test fun,
 *     the ipa type needs to update back to vm_ram_rw before
 *     freeing the test buffer. (Or data abort will occur)
 */
int _vreg_test_dyn_ipa_mmu_basic(void)
{
	int i;
	uint32_t v;
	uint64_t pa = 0;
	uint32_t pa_high, pa_low;
	void __iomem *io;
	enum vm_type_t type = vm_ram_ro_xn;
	enum vm_type_t type_rw = vm_ram_rw;
	char *buf = NULL;

	int size = 64 * 1024; /*64KB*/

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf) {
		KREE_ERR("[%s][%d] buf kmalloc Fail.\n", __func__, __LINE__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	/*memset(buf, 0, size);*/ /*will data abort*/

	pa = (uint64_t) virt_to_phys((void *)buf);
	KREE_INFO("[%s][%d]test buf pa:0x%lx\n", __func__, __LINE__, pa);

	pa_high = (uint32_t) (pa >> 32);
	pa_low = (uint32_t) (pa & (0x00000000ffffffff));

	io = ioremap(VREG_BASE_dyn_map, test_vreg_basic_size);

	if (io) {
		/*write test ipa info*/
		writel(0x0, io + (0 * WORD_WIDTH));     /*clear vreg data*/
		KREE_INFO("[%d]write dynamic IPA memory range data\n",
			__LINE__);
		writel(pa_high, io + (1 * WORD_WIDTH)); /*pa_high*/
		writel(pa_low, io + (2 * WORD_WIDTH));  /*pa_low */
		writel(size, io + (3 * WORD_WIDTH));    /*size   */
		writel(type, io + (4 * WORD_WIDTH));    /*type   */

		/*check test ipa info*/
		KREE_INFO("[%d]check if test ipa info ready\n", __LINE__);
		for (i = 0; i <= 4; i++) {
			v = readl(io + (i * WORD_WIDTH));
			KREE_INFO("[%d] read offset word[%d]=0x%x\n",
				__LINE__, i, v);
		}

		/*control bit: 0:clear data; 1:map; 2:unmap; 3:update type*/
#if 0
		/*only unmap*/
		KREE_INFO("[%d]dynamic ipa unmap.\n", __LINE__);
		writel(0x00000002, io + (0 * WORD_WIDTH));
		v = readl(io + (0 * WORD_WIDTH));
		KREE_INFO("[%d]read control bit[0]=0x%x\n", __LINE__, v);

		/*only map*/
		KREE_INFO("[%d]dynamic ipa map\n", __LINE__);
		writel(0x00000001, io + (0 * WORD_WIDTH));
		v = readl(io + (0 * WORD_WIDTH));
		KREE_INFO("[%d]read control bit[0]=0x%x\n", __LINE__, v);
#endif

		/*update type: vm_ram_ro_xn*/
		KREE_INFO("[%d]update ipa type (vm_ram_ro_xn)\n", __LINE__);
		writel(0x00000003, io + (0 * WORD_WIDTH)); /*type: ro_xn*/
		v = readl(io + (0 * WORD_WIDTH));
		KREE_INFO("[%d]read control bit[0]=0x%x\n", __LINE__, v);

		/*update type: vm_ram_rw*/
		KREE_INFO("[%d]update ipa type (vm_ram_rw)\n", __LINE__);
		writel(type_rw, io + (4 * WORD_WIDTH)); /*type: rw*/
		v = readl(io + (4 * WORD_WIDTH));
		KREE_INFO("[%d] read offset word[4](type)=0x%x\n", __LINE__, v);

		KREE_INFO("[%d]update ipa type (vm_ram_rw)\n", __LINE__);
		writel(0x00000003, io + (0 * WORD_WIDTH));
		v = readl(io + (0 * WORD_WIDTH));
		KREE_INFO("[%d]read control bit[0]=0x%x\n", __LINE__, v);

		/*clean vreg data*/
		KREE_INFO("[%d]clean vreg (control bit+data)\n", __LINE__);
		writel(0x00000000, io + (0 * WORD_WIDTH));

		/*read vreg to check if data clean*/
		KREE_INFO("[%d]check if data clean\n", __LINE__);
		for (i = 0; i <= 4; i++) {
			v = readl(io + (i * WORD_WIDTH));
			KREE_INFO("[%d] read offset word[%d]=0x%x\n",
				__LINE__, i, v);
		}

	} else
		KREE_ERR("[%d]vreg: null io\n", __LINE__);

	if (io)
		iounmap(io);

	kfree(buf);
	KREE_INFO("[%s][%d]done.\n", __func__, __LINE__);
	return TZ_RESULT_SUCCESS;
}
#endif

int vreg_test(void *args)
{

#if test_vreg_dyn_ipa_mmu_basic
	_vreg_test_dyn_ipa_mmu_basic();   /*case 1: dynamic ipa map/unmap*/
#endif

#if test_vreg_from_gzha         /*disable now*/
	_vreg_test_from_gzha();     /*case 2: trap vreg from gz HA*/
#endif

#if test_vreg_basic             /*disable now*/
	_vreg_test_basic();         /*case 3: basic vreg test*/
#endif

#if test_Vmemory                /*disable now*/
	vmemory_test();         /*case 4: basic vmem test*/
#endif

	return TZ_RESULT_SUCCESS;
}


#if test_vreg_dyn_ipa_mmu_threads


#define test_thread_num 3
struct completion _comp[test_thread_num];

char *buf_rkp;

/*Test: test dynamic ipa mmu change type/read/write memory concurrently
 * Detail:
 * (1) alloc a buffer (buf) with size 64KB and update
 *     ipa type to vm_ram_rw_xn (read/write, no execution).
 * (2) use three threads concurrently:
 *     one is changing mmu type, the other one is reading data from that
 *     memory range, and the last one is writing data to that memory range
 * (3) because ipa type is keep rw (vm_ram_rw_xn) permission, so this UT
 *     can free the test buffer (unpredictable data write won't data abort).
 */
int _init_dyn_ipa_mmu_test_buf(void)
{
	int i;
	uint64_t pa = 0;
	uint32_t pa_high, pa_low;
	void __iomem *io;

	enum vm_type_t type = vm_ram_rw_xn;

	int size = 64 * 1024; /*64KB*/
	uint32_t v;

	/*alloc test buffer*/
	buf_rkp = kmalloc(size, GFP_KERNEL);
	if (!buf_rkp) {
		KREE_ERR("[%s][%d] buf_rkp kmalloc Fail.\n",
			__func__, __LINE__);
		return TZ_RESULT_ERROR_OUT_OF_MEMORY;
	}

	/*memset(buf_rkp, 'a', size);*/	/*will data abort*/

	pa = (uint64_t) virt_to_phys((void *)buf_rkp);
	KREE_INFO("[%s][%d]test buf_rkp pa:0x%lx\n", __func__, __LINE__, pa);

	pa_high = (uint32_t) (pa >> 32);
	pa_low = (uint32_t) (pa & (0x00000000ffffffff));

	io = ioremap(VREG_BASE_dyn_map, test_vreg_basic_size);

	/*write test ipa info*/
	writel(0x0, io + (0 * WORD_WIDTH));     /*clear vreg data*/
	KREE_INFO("[%d]write dynamic IPA memory range data\n", __LINE__);
	writel(pa_high, io + (1 * WORD_WIDTH)); /*pa_high*/
	writel(pa_low, io + (2 * WORD_WIDTH));	/*pa_low */
	writel(size, io + (3 * WORD_WIDTH));	/*size	 */
	writel(type, io + (4 * WORD_WIDTH));	/*type	 */

	/*check test ipa info*/
	KREE_INFO("[%d]check if test ipa info ready\n", __LINE__);
	for (i = 0; i <= 4; i++) {
		v = readl(io + (i * WORD_WIDTH));
		KREE_INFO("[%d] read offset word[%d]=0x%x\n", __LINE__, i, v);
	}

	if (io)
		iounmap(io);

	return TZ_RESULT_SUCCESS;
}

int _init_thread_completion(void)
{
	int i;

	for (i = 0; i < test_thread_num; i++)
		init_completion(&_comp[i]);

	return 0;
}

int _wait_thread_completion(void)
{
	int i;

	for (i = 0; i < test_thread_num; i++)
		wait_for_completion(&_comp[i]);

	return 0;
}

int _thread_mmu(void *data)
{
	void __iomem *io;
	int i;

	io = ioremap(VREG_BASE_dyn_map, test_vreg_basic_size);
	if (io) {
		KREE_INFO("[%d]test dynamic update ipa type\n", __LINE__);
		for (i = 0; i < 1000; i++)
			writel(0x00000003, io + (0 * WORD_WIDTH));
	} else
		KREE_ERR("[%d]vreg: null io\n", __LINE__);

	if (io)
		iounmap(io);

	complete(&_comp[0]);
	return TZ_RESULT_SUCCESS;
}

int _thread_write(void *data)
{
	int i;

	for (i = 0; i < 1000; i++) {
		buf_rkp[10] = 'c';
		//memset(buf_rkp, 'c', 4096);
	}

	complete(&_comp[1]);
	return 0;
}

int _thread_read(void *data)
{
	int i, j;
	char m;

	for (i = 0; i < 1000; i++)
		for (j = 0; j < 4096; j++)
			m = (char) buf_rkp[j];

	complete(&_comp[2]);
	return 0;
}

int _rkp_thread_test(void)
{
	struct task_struct *thread_mmu, *thread_wrt, *thread_rd;

	_init_dyn_ipa_mmu_test_buf();

	_init_thread_completion();

	/*thread 1: do map/unmap 1000 times*/
	thread_mmu = kthread_create(_thread_mmu, NULL, "mmu_thread");
	if (IS_ERR(thread_mmu))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread_mmu, 3);
	wake_up_process(thread_mmu);

	/*thread 2: write 1000 times*/
	thread_wrt = kthread_create(_thread_write, NULL, "thread_write");
	if (IS_ERR(thread_wrt))
		return TZ_RESULT_ERROR_GENERIC;

	kthread_bind(thread_wrt, 4);
	wake_up_process(thread_wrt);


	/*thread 3: read 1000 times*/
	thread_rd = kthread_create(_thread_read, NULL, "thread_read");
	if (IS_ERR(thread_rd))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread_rd, 3);
	wake_up_process(thread_rd);

	_wait_thread_completion();

	kfree(buf_rkp); /*test type: rw(OK). ro(will abort)*/

	return 0;
}

#endif

int test_rkp_by_buffer(void *args)
{

#if test_vreg_dyn_ipa_mmu_threads
	/*GZ side log (io.c) needs to be off*/
	_rkp_thread_test();
#endif

	return TZ_RESULT_SUCCESS;
}

struct gz_driver_ipa_info {
	char *name;
	uint64_t pa;
	int size;
	enum s1_attrs_t type;
} gz_driver_ipa_info;

static struct gz_driver_ipa_info gz_driver_ipa[] = {
	[0] = { .name = "tipc_chan_queue_msg",
			.size = PAGE_SIZE,
			.type = MM_MEMATTRIB_READONLY
		  },
	[1] = { .name = "tipc_create_channel",
			.size = PAGE_SIZE,
			.type = MM_MEMATTRIB_READONLY
		  },
	[2] = { .name = "tipc_k_connect",
			.size = PAGE_SIZE,
			.type = MM_MEMATTRIB_READONLY
		  },
	[3] = { .name = "tipc_k_write",
			.size = PAGE_SIZE,
			.type = MM_MEMATTRIB_READONLY
		  },
	[4] = { .name = "rxbuf",
			.size = PAGE_SIZE,
			.type = MM_MEMATTRIB_READONLY
		  },
	{.type = MM_MEM_ATTRIB_INVALID } /*end condition*/
};


int _thread_mmu_gzdriver_body(uint64_t pa, int size, enum s1_attrs_t type)
{
	uint32_t pa_high, pa_low;
	uint32_t v;
	void __iomem *io;
	int i;

	KREE_INFO("[%s][%d]gz driver pa:0x%lx, size=0x%x, type=0x%x\n",
		__func__, __LINE__, pa, size, type);

	if (pa == 0 || size == 0 || type == MM_MEM_ATTRIB_INVALID) {
		KREE_INFO("[%d]Invalid param[%d]\n", __LINE__, i);
		return TZ_RESULT_ERROR_BAD_PARAMETERS;
	}

	pa_high = (uint32_t) (pa >> 32);
	pa_low = (uint32_t) (pa & (0x00000000ffffffff));

	io = ioremap(VREG_BASE_dyn_map, test_vreg_basic_size);
	if (!io) {
		KREE_ERR("[%d]vreg: null io\n", __LINE__);
		return TZ_RESULT_ERROR_GENERIC;
	}

	/*write test ipa info*/
	writel(0x0, io + (0 * WORD_WIDTH)); /*clear vreg data*/
	KREE_INFO("[%d]write dynamic IPA memory range data\n", __LINE__);
	writel(pa_high, io + (1 * WORD_WIDTH)); /*pa_high*/
	writel(pa_low, io + (2 * WORD_WIDTH));	/*pa_low */
	writel(size, io + (3 * WORD_WIDTH));	/*size	 */
	writel(type, io + (4 * WORD_WIDTH));	/*type	 */

	/*check test ipa info*/
	KREE_INFO("[%d]check if test ipa info ready\n", __LINE__);
	for (i = 0; i <= 4; i++) {
		v = readl(io + (i * WORD_WIDTH));
		KREE_INFO("[%d] read offset word[%d]=0x%x\n", __LINE__, i, v);

		if (((i > 1) && (i <= 4)) && (v == 0)) {
			KREE_INFO("[%d] writel fail.\n", __LINE__, i, v);
			if (io)
				iounmap(io);
			return TZ_RESULT_ERROR_BAD_PARAMETERS;
		}
	}

	KREE_INFO("[%d]test dynamic update ipa type\n", __LINE__);
	/*for (i = 0; i < 100; i++)*/
	for (i = 0; i < 1; i++)
		writel(0x00000003, io + (0 * WORD_WIDTH));

	if (io)
		iounmap(io);

	return TZ_RESULT_SUCCESS;
}

int _thread_mmu_gzdriver(void *data)
{
	int i = 0;

	while (1) {
		if (gz_driver_ipa[i].type == MM_MEM_ATTRIB_INVALID) {
			KREE_INFO("[%d] test done\n", __LINE__);
			i++;
			break;
		}

		if (gz_driver_ipa[i].pa == 0 || gz_driver_ipa[i].size == 0) {
			KREE_INFO("[%d] Invalid param[%d] skip test.\n",
				__LINE__, i);
			i++;
			continue;
		}

		KREE_INFO("[%d]test: name=%s, pa=0x%lx\n",
			__LINE__, gz_driver_ipa[i].name,
			gz_driver_ipa[i].pa);

		KREE_INFO("[%d]test: size=0x%x, type=0x%x\n",
			__LINE__, gz_driver_ipa[i].size,
			gz_driver_ipa[i].type);

		/*chage ipa type*/
		/*set --> read-only*/
		_thread_mmu_gzdriver_body(gz_driver_ipa[i].pa,
			gz_driver_ipa[i].size, gz_driver_ipa[i].type);

		/*reset to orig type*/
		_thread_mmu_gzdriver_body(gz_driver_ipa[i].pa,
			gz_driver_ipa[i].size,
			(MM_MEMATTRIB_READONLY | MM_MEMATTRIB_WRITEONLY));

		i++;
	}

	return TZ_RESULT_SUCCESS;
}

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))
void _set_test_data(void)
{
	/*trusty-ipc.c APIs & data buffer for testing*/

	gz_driver_ipa[0].pa = (uint64_t)
		virt_to_phys((void *) pa_tipc_chan_queue_msg);
	gz_driver_ipa[1].pa = (uint64_t)
		virt_to_phys((void *) pa_tipc_create_channel);
	gz_driver_ipa[2].pa = (uint64_t)
		virt_to_phys((void *) pa_tipc_k_connect);
	gz_driver_ipa[3].pa = (uint64_t)
		virt_to_phys((void *) pa_tipc_k_write);
	gz_driver_ipa[4].pa = (uint64_t)
		virt_to_phys((void *) pa_rxbuf);

	gz_driver_ipa[0].pa = ROUNDUP(gz_driver_ipa[0].pa, PAGE_SIZE);
	gz_driver_ipa[1].pa = ROUNDUP(gz_driver_ipa[1].pa, PAGE_SIZE);
	gz_driver_ipa[2].pa = ROUNDUP(gz_driver_ipa[2].pa, PAGE_SIZE);
	gz_driver_ipa[3].pa = ROUNDUP(gz_driver_ipa[3].pa, PAGE_SIZE);
	gz_driver_ipa[4].pa = ROUNDUP(gz_driver_ipa[4].pa, PAGE_SIZE);

	KREE_INFO("[%d]&tipc_chan_queue_msg:0x%lx, [ROUNDUP PA]=0x%lx,\n",
		__LINE__, pa_tipc_chan_queue_msg, gz_driver_ipa[0].pa);
	KREE_INFO("PA=0x%lx\n",
		(uint64_t) virt_to_phys((void *) pa_tipc_create_channel));

	KREE_INFO("[%d]&tipc_create_channel:0x%lx, [ROUNDUP PA]=0x%lx,",
		__LINE__, pa_tipc_create_channel, gz_driver_ipa[1].pa);
	KREE_INFO("PA=0x%lx\n",
		(uint64_t) virt_to_phys((void *) pa_tipc_create_channel));

	KREE_INFO("[%d]&tipc_k_connect:0x%lx, [ROUNDUP PA]=0x%lx,",
		__LINE__, pa_tipc_k_connect, gz_driver_ipa[2].pa);
	KREE_INFO("PA=0x%lx\n",
		(uint64_t) virt_to_phys((void *) pa_tipc_k_connect));

	KREE_INFO("[%d]&tipc_k_write:0x%lx, [ROUNDUP PA]=0x%lx,",
		__LINE__, pa_tipc_k_write, gz_driver_ipa[3].pa);
	KREE_INFO("PA=0x%lx\n",
		(uint64_t) virt_to_phys((void *) pa_tipc_k_write));

	KREE_INFO("[%d]&rxbuf:0x%lx, [ROUNDUP PA]=0x%lx,",
		__LINE__, pa_rxbuf, gz_driver_ipa[4].pa);
	KREE_INFO("PA=0x%lx\n",
		(uint64_t) virt_to_phys((void *) pa_rxbuf));

}

int _rkp_gzdriver_test(void)
{
	struct task_struct *thread_mmu;

	_set_test_data();

	/*thread 1: do map/unmap 100 times*/
	thread_mmu = kthread_create(_thread_mmu_gzdriver, NULL, "mmu_gzdrv");
	if (IS_ERR(thread_mmu))
		return TZ_RESULT_ERROR_GENERIC;
	kthread_bind(thread_mmu, 3);
	wake_up_process(thread_mmu);

	return 0;
}

int test_rkp_by_gzdriver(void *args)
{
#if test_vreg_dyn_ipa_mmu_threads
	/*GZ side log (io.c) needs to be off*/
	_rkp_gzdriver_test();
#endif

	return TZ_RESULT_SUCCESS;
}

/*map/unmap GZ kernel memory with RO and RW types*/
int test_rkp_gzkernel_memory(void *args)
{
	void __iomem *io;

	io = ioremap(VREG_BASE_dyn_map, test_vreg_basic_size);

	/*unmap/map RO*/
	writel(0x00000004, io + (0 * WORD_WIDTH));

	/*unmap/map RW*/
	writel(0x00000005, io + (0 * WORD_WIDTH));

	if (io)
		iounmap(io);

	return TZ_RESULT_SUCCESS;
}


/*map/unmap GZ kernel memory*/
int test_rkp(void *args)
{
	uint64_t size = (uint64_t)_etext - (uint64_t)_text;
	uint64_t pa = (uint64_t) virt_to_phys((void *)_text);

	KREE_INFO("[%s][%d] test param: _etext=0x%llx, _text=0x%llx,",
		__func__, __LINE__, (uint64_t)_etext, (uint64_t)_text);
	KREE_INFO("start_pa=0x%llx, size=0x%llx\n", pa, size);

	return _thread_mmu_gzdriver_body((uint64_t)pa, (uint64_t)size,
		0x2); /*RO, EXECUTE*/
}


