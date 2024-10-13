// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <videodev2_exynos_camera.h>

#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "is-groupmgr.h"
#include "is-hw.h"
#include "is-core.h"

#include "pablo-kunit-test.h"
#include "pablo-resourcemgr.h"
#include "pablo-debug.h"

static struct pablo_resourcemgr_test_ctx {
	struct is_resourcemgr resourcemgr;
	struct is_device_ischain ischain;
	struct is_device_sensor sensor;
	struct is_hardware hardware;
	struct is_minfo minfo;
	struct reserved_mem rmem;
	struct is_priv_buf pb;
	struct is_core core;
	struct platform_device dev;
	const struct pablo_memlog_operations *pml_ops_b;
} pkt_ctx;

static struct pkt_rscmgr_ops *func;
static void *pablo_mock_get_log_dump(void)
{
#ifdef ENABLE_KERNEL_LOG_DUMP
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;

	return resourcemgr->kernel_log_buf;
#else
	return NULL;
#endif
}

static unsigned int pablo_mock_get_bts_scenidx(const char *name)
{
	return !strcmp(name, "camera_default");
}

static int pablo_mock_bts_add_scenario(unsigned int scen_idx)
{
	return 0;
}

static int pablo_mock_bts_del_scenario(unsigned int scen_idx)
{
	return 0;
}

static void *pablo_mock_vaddr(void)
{
	unsigned long mock_arr[10];
	void *vaddr;

	vaddr = mock_arr;

	return vaddr;
}

static void *pablo_mock_pablo_vmap(struct page **pages, unsigned int npages, unsigned long type, pgprot_t prot)
{
	return pablo_mock_vaddr();
}

static void pablo_mock_hw_configure_llc(bool on, void *device, ulong *llc_state)
{
	/* Define the test double. */
}

static int __debug_memlog_do_dump(struct memlog_obj *obj, int log_level)
{
	return 0;
}

static int pablo_mock_ischain_power(struct is_device_ischain *device, int on)
{
	return device->pdev ? 0 : -EBUSY;
}

static int pablo_mock_interface_open(struct is_interface *this)
{
	if (test_bit(IS_IF_STATE_OPEN, &this->state))
		return -EMFILE;
	return 0;
}

static int pablo_mock_interface_close(struct is_interface *this)
{
	if (!test_bit(IS_IF_STATE_OPEN, &this->state))
		return -EMFILE;
	return 0;
}

#define PB_SIZE 100
static struct is_priv_buf *pablo_mock_ion_alloc(void *ctx, size_t size, const char *heapname,
						unsigned int flags)
{
	struct is_priv_buf *pb = &pkt_ctx.pb;

	pb->size = PB_SIZE;

	return pb;
}

static struct is_priv_buf *pablo_mock_contig_alloc(size_t size)
{
	return &pkt_ctx.pb;
}

static ulong pkt_mem_kvaddr(struct is_priv_buf *pbuf)
{
	if (!pbuf)
		return 0;

	if (!pbuf->kva)
		pbuf->kva = pablo_mock_vaddr();

	return (ulong)pbuf->kva;
}

static phys_addr_t pkt_mem_phaddr(struct is_priv_buf *pbuf)
{
	return (phys_addr_t)pablo_mock_vaddr();
}

static dma_addr_t pkt_mem_dvaddr(struct is_priv_buf *pbuf)
{
	if (!pbuf)
		return 0;

	return (dma_addr_t)pablo_mock_vaddr();
}

static struct pablo_device_ischain_ops pkt_ischain_ops = {
	.power = pablo_mock_ischain_power,
};

static struct pablo_interface_ops pkt_itf_ops = {
	.open = pablo_mock_interface_open,
	.close = pablo_mock_interface_close,
};

static struct is_mem_ops pkt_mem_ops = {
	.alloc = pablo_mock_ion_alloc,
};

const static struct pablo_memlog_operations pml_ops_mock = {
	.do_dump = __debug_memlog_do_dump,
};

static struct is_priv_buf_ops pkt_buf_ops = {
	.kvaddr = pkt_mem_kvaddr,
	.phaddr = pkt_mem_phaddr,
	.dvaddr = pkt_mem_dvaddr,
};

/* Define the test cases. */
static void pablo_rscmgr_init_log_rmem_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	int test_result;

	pkt_ctx.rmem.size = 5;
	test_result = pablo_rscmgr_init_log_rmem(resourcemgr, &pkt_ctx.rmem);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_rscmgr_secure_mem_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	int test_result;

	resourcemgr->scenario = IS_SCENARIO_SECURE;
	test_result = is_resourcemgr_init_secure_mem(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	resourcemgr->minfo->pb_taaisp_s = NULL;
	resourcemgr->minfo->pb_tnr_s = NULL;
	test_result = is_resourcemgr_deinit_secure_mem(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_rscmgr_init_dynamic_mem_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	int test_result;

	resourcemgr->mem.is_mem_ops = &pkt_mem_ops;

	pkt_mem_ops.alloc = NULL;

	test_result = is_resourcemgr_init_dynamic_mem(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	pkt_mem_ops.alloc = pablo_mock_ion_alloc;
	test_result = is_resourcemgr_init_dynamic_mem(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_result, 0);
	KUNIT_EXPECT_NULL(test, (void *)resourcemgr->minfo->pb_taaisp);
	KUNIT_EXPECT_NULL(test, (void *)resourcemgr->minfo->pb_tnr);
	KUNIT_EXPECT_NULL(test, (void *)resourcemgr->minfo->pb_orbmch);
}

static void pablo_rscmgr_bts_scen_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	char scen_name[][20] = { "camera_default", "camera_heavy" };
	int num_scen = 2;
	int i;

	resourcemgr->bts_scen = NULL;
	is_bts_scen(resourcemgr, 0, false);

	resourcemgr->bts_scen = kunit_kzalloc(test, sizeof(struct is_bts_scen) * num_scen, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, resourcemgr->bts_scen);

	for (i = 0; i < num_scen; i++) {
		resourcemgr->bts_scen[i].index = i;
		resourcemgr->bts_scen[i].name = scen_name[i];
	}

	is_bts_scen(resourcemgr, 0, false);
	is_bts_scen(resourcemgr, 0, true);

	kunit_kfree(test, resourcemgr->bts_scen);
	resourcemgr->bts_scen = NULL;
}

static void pablo_rscmgr_resource_ioctl_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	struct v4l2_control *ctrl;
	int i;
	int ret;
	struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();
	u32 save_cpu_cluster[PABLO_CPU_CLUSTER_MAX];
	u32 test_cpu_cluster[PABLO_CPU_CLUSTER_MAX] = {0, 4, 99, 7};	// LIT, MID, MID_HF, BIG

	ctrl = kunit_kzalloc(test, sizeof(struct v4l2_control), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctrl);

	mutex_init(&resourcemgr->qos_lock);

	ctrl->id = V4L2_CID_IS_DVFS_CLUSTER0;
	ctrl->value = 0x10001;
	for (i = 0; i < PABLO_CPU_CLUSTER_MAX; i++)
		resourcemgr->cluster[i] = 0;
	is_resource_ioctl(resourcemgr, ctrl);

	for (i = 0; i < PABLO_CPU_CLUSTER_MAX; i++)
		resourcemgr->cluster[i] = 0x10001;
	is_resource_ioctl(resourcemgr, ctrl);

	ctrl->value = 0;
	is_resource_ioctl(resourcemgr, ctrl);

	for (i = 0; i < PABLO_CPU_CLUSTER_MAX; i++) {
		save_cpu_cluster[i] = plpd->cpu_cluster[i];
		plpd->cpu_cluster[i] = test_cpu_cluster[i];
	}
	ctrl->id = V4L2_CID_IS_DVFS_CLUSTER1_HF;
	ret = is_resource_ioctl(resourcemgr, ctrl);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	ctrl->id = V4L2_CID_IS_DVFS_CLUSTER1;
	ret = is_resource_ioctl(resourcemgr, ctrl);
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < PABLO_CPU_CLUSTER_MAX; i++) {
		plpd->cpu_cluster[i] = save_cpu_cluster[i];
	}

	kunit_kfree(test, ctrl);
}

static void pablo_rscmgr_logsync_kunit_test(struct kunit *test)
{
	int test_result;

	test_result = is_logsync(0);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_rscmgr_update_lic_sram_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	int test_result;
	int taa_sram_sum = atomic_read(&resourcemgr->lic_sram.taa_sram_sum);

	resourcemgr->dev_resource.taa_id = 0;
	resourcemgr->dev_resource.width = 320;

	taa_sram_sum += resourcemgr->dev_resource.width;
	is_resource_update_lic_sram(resourcemgr, true);
	test_result = atomic_read(&resourcemgr->lic_sram.taa_sram_sum);
	KUNIT_EXPECT_EQ(test, test_result, taa_sram_sum);

	is_resource_update_lic_sram(resourcemgr, false);
	test_result = atomic_read(&resourcemgr->lic_sram.taa_sram_sum);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_rscmgr_kernel_log_dump_kunit_test(struct kunit *test)
{
#ifdef ENABLE_KERNEL_LOG_DUMP
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	int test_result;

	test_result = is_kernel_log_dump(resourcemgr, false);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	test_result = is_kernel_log_dump(resourcemgr, false);
	KUNIT_EXPECT_LT(test, test_result, 0);
#endif
}

static void pablo_rscmgr_get_shot_timeout_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	int test_result;
	u32 shot_timeout;

#if IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	shot_timeout = 10000;
#else
	shot_timeout = 3000;
#endif
	resourcemgr->shot_timeout = shot_timeout;
	test_result = is_get_shot_timeout(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_result, (int)shot_timeout);
}

static void pablo_rscmgr_init_mem_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	int test_result;
	int i;

	resourcemgr->mem.is_mem_ops = &pkt_mem_ops;
	pkt_mem_ops.alloc = NULL;
	test_result = is_resourcemgr_init_mem(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_result, -ENOMEM);

	pkt_mem_ops.alloc = pablo_mock_ion_alloc;
	resourcemgr->mem.contig_alloc = &pablo_mock_contig_alloc;
	resourcemgr->minfo->pb_debug = &pkt_ctx.pb;
	resourcemgr->minfo->kvaddr_debug = 0;
	resourcemgr->minfo->pb_sfr_dump_addr = &pkt_ctx.pb;
	resourcemgr->minfo->pb_sfr_dump_value = &pkt_ctx.pb;
	for (i = 0; i < SENSOR_POSITION_MAX; i++)
		resourcemgr->minfo->pb_cal[i] = &pkt_ctx.pb;

	resourcemgr->minfo->pb_debug->ops = &pkt_buf_ops;
	test_result = is_resourcemgr_init_mem(resourcemgr);
	KUNIT_EXPECT_EQ(test, (u32)pkt_ctx.pb.size, (u32)PB_SIZE);
	KUNIT_EXPECT_GT(test, resourcemgr->minfo->kvaddr_debug, (unsigned long)0);
	KUNIT_EXPECT_GT(test, (unsigned long)resourcemgr->minfo->phaddr_debug, (unsigned long)0);
	KUNIT_EXPECT_GT(test, resourcemgr->minfo->kvaddr_debug_cnt, (unsigned long)0);
}

static void pablo_rscmgr_set_global_param_kunit_test(struct kunit *test)
{
	int state;
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	struct is_device_ischain *device = &pkt_ctx.ischain;
	void *backup = pablo_get_rscmgr_ops()->configure_llc;

	mutex_init(&resourcemgr->global_param.lock);
	resourcemgr->global_param.state = 1;
	device->llc_mode = 0;
	pablo_get_rscmgr_ops()->configure_llc = pablo_mock_hw_configure_llc;

	is_resource_set_global_param(resourcemgr, device);
	state = test_bit(device->instance, &resourcemgr->global_param.state);
	KUNIT_EXPECT_EQ(test, state, (int)1);

	pablo_get_rscmgr_ops()->configure_llc = backup;
}

static void pablo_rscmgr_clear_global_param_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	struct is_device_ischain *device = &pkt_ctx.ischain;
	atomic_t sensor_cnt;
	int llc_state_bit = 1;
	int rsc_value, value;
	void *backup = pablo_get_rscmgr_ops()->configure_llc;

	mutex_init(&resourcemgr->global_param.lock);
	resourcemgr->global_param.state = 0;

	rsc_value = atomic_read(&resourcemgr->global_param.sensor_cnt);
	atomic_set(&sensor_cnt, rsc_value);

	pablo_get_rscmgr_ops()->configure_llc = pablo_mock_hw_configure_llc;

	is_resource_clear_global_param(resourcemgr, device);
	llc_state_bit = test_bit(device->instance, &resourcemgr->global_param.state);
	KUNIT_EXPECT_EQ(test, llc_state_bit, (int)0);

	atomic_dec(&sensor_cnt);
	rsc_value = atomic_read(&resourcemgr->global_param.sensor_cnt);
	value = atomic_read(&sensor_cnt);
	KUNIT_EXPECT_EQ(test, value, rsc_value);

	pablo_get_rscmgr_ops()->configure_llc = backup;
}

static void pablo_rscmgr_clear_cluster_qos_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	int i;
	int min_max_value = (1 << CLUSTER_MAX_SHIFT | 1 << CLUSTER_MIN_SHIFT);

	for (i = 0; i < PABLO_CPU_CLUSTER_MAX; i++)
		resourcemgr->cluster[i] = min_max_value;

	pablo_resource_clear_cluster_qos(resourcemgr);

	for (i = 0; i < PABLO_CPU_CLUSTER_MAX; i++)
		KUNIT_EXPECT_EQ(test, resourcemgr->cluster[i] , (u32)0);
}

static void pablo_rscmgr_dump_kunit_test(struct kunit *test)
{
	int test_result;

	test_result = is_resource_dump();
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static void pablo_rscmgr_cdump_kunit_test(struct kunit *test)
{
	struct is_core *core = is_get_is_core();
	int rsccount = atomic_read(&core->resourcemgr.rsccount);
	struct is_device_ischain *device = &core->ischain[0];
	unsigned long state = device->state;
	struct is_debug *debug;
	int test_result;

	debug = is_debug_get();
	pkt_ctx.pml_ops_b = debug->memlog.ops;
	debug->memlog.ops = &pml_ops_mock;

	atomic_set(&core->resourcemgr.rsccount, 0);
	test_result = is_resource_cdump();
	KUNIT_EXPECT_EQ(test, test_result, (int)0);

	atomic_set(&core->resourcemgr.rsccount, 1);
	test_result = is_resource_cdump();
	KUNIT_EXPECT_EQ(test, test_result, (int)0);

	atomic_set(&core->resourcemgr.rsccount, rsccount);
	device->state = state;
	debug->memlog.ops = pkt_ctx.pml_ops_b;
}

static void pablo_rscmgr_sensor_kunit_test(struct kunit *test, bool get)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	struct is_resource *resource;
	int i;

	resource = kunit_kzalloc(test, sizeof(struct is_resource), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, resource);

	resource->pdev = kunit_kzalloc(test, sizeof(struct platform_device), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, resource->pdev);

	for (i = RESOURCE_TYPE_SENSOR0; i < RESOURCE_TYPE_ISCHAIN; i++) {
		if (get) {
			func->get_sensor(resourcemgr, i, resource);
			KUNIT_EXPECT_EQ(test, test_bit(IS_RM_SS0_POWER_ON + i, &resourcemgr->state),
					1);
		} else {
			func->put_sensor(resourcemgr, i, resource);
			KUNIT_EXPECT_EQ(test, test_bit(IS_RM_SS0_POWER_ON + i, &resourcemgr->state),
					0);
		}
	}

	kunit_kfree(test, resource->pdev);
	kunit_kfree(test, resource);
	resource->pdev = NULL;
	resource = NULL;
}
static void pablo_rscmgr_get_sensor_kunit_test(struct kunit *test)
{
	pablo_rscmgr_sensor_kunit_test(test, true);
}

static void pablo_rscmgr_put_sensor_kunit_test(struct kunit *test)
{
	pablo_rscmgr_sensor_kunit_test(test, false);
}

static void pablo_rscmgr_get_ischain_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	struct is_core *core;
	struct is_interface *itf;
	struct is_device_ischain *ischain;
	int test_result;

	/* TC : resourcemgr->state is IS_RM_ISC_POWER_ON */
	set_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state);
	test_result = func->get_ischain(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
	clear_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state);

	/* TC : interface_open is failed */
	resourcemgr->private_data = &pkt_ctx.core;
	core = (struct is_core *)resourcemgr->private_data;
	core->noti_register_state = false;
	itf = &core->interface;
	itf->ops = &pkt_itf_ops;
	set_bit(IS_IF_STATE_OPEN, &itf->state);
	test_result = func->get_ischain(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_result, -EMFILE);
	clear_bit(IS_IF_STATE_OPEN, &itf->state);

	/* TC : is_ischain_power is fail */
	ischain = &core->ischain[0];
	ischain->ops = &pkt_ischain_ops;
	test_result = func->get_ischain(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_result, -EBUSY);

	/* TC : After is_ischain_power is passed */
	core->ischain[0].pdev = &pkt_ctx.dev;
	resourcemgr->cur_bts_scen_idx = 1; /* to check if this value is set zero after running */
	test_result = func->get_ischain(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state), 1);
	KUNIT_EXPECT_EQ(test, resourcemgr->cur_bts_scen_idx, 0);
}

static void pablo_rscmgr_put_ischain_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &pkt_ctx.resourcemgr;
	struct is_core *core;
	struct is_device_ischain *ischain;
	struct is_interface *itf;

	resourcemgr->minfo->pb_taaisp_s = NULL;
	resourcemgr->minfo->pb_tnr_s = NULL;
	resourcemgr->private_data = &pkt_ctx.core;
	core = (struct is_core *)resourcemgr->private_data;
	ischain = &core->ischain[0];
	itf = &core->interface;
	ischain->ops = &pkt_ischain_ops;
	itf->ops = &pkt_itf_ops;
	set_bit(IS_IF_STATE_OPEN, &itf->state);
	set_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state);
	resourcemgr->cur_bts_scen_idx = 1;
	func->put_ischain(resourcemgr);
	KUNIT_EXPECT_EQ(test, test_bit(IS_RM_ISC_POWER_ON, &resourcemgr->state), 0);
	KUNIT_EXPECT_EQ(test, resourcemgr->cur_bts_scen_idx, 0);
}

static struct pablo_rscmgr_sys_ops pkt_sys_ops;
static void pablo_rscmgr_set_sys_ops(struct pablo_rscmgr_sys_ops *dst_ops,
				      struct pablo_rscmgr_sys_ops *src_ops, bool init)
{
	dst_ops->get_log_kernel = src_ops->get_log_kernel;
	dst_ops->bts_scenidx = src_ops->bts_scenidx;
	dst_ops->bts_add_scen = src_ops->bts_add_scen;
	dst_ops->bts_del_scen = src_ops->bts_del_scen;
	dst_ops->vmap = src_ops->vmap;

	if (init) {
		src_ops->get_log_kernel = pablo_mock_get_log_dump;
		src_ops->bts_scenidx = pablo_mock_get_bts_scenidx;
		src_ops->bts_add_scen = pablo_mock_bts_add_scenario;
		src_ops->bts_del_scen = pablo_mock_bts_del_scenario;
		src_ops->vmap = pablo_mock_pablo_vmap;
	}
}

static int pablo_resourcemgr_kunit_test_init(struct kunit *test)
{
	int i;

	memset(&pkt_ctx, 0, sizeof(pkt_ctx));

	for (i = 0; i < GROUP_SLOT_MAX; i++) {
		pkt_ctx.ischain.group[i] = kunit_kzalloc(test, sizeof(struct is_group), GFP_KERNEL);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pkt_ctx.ischain.group[i]);
	}

	pkt_ctx.ischain.sensor = &pkt_ctx.sensor;

#ifdef ENABLE_KERNEL_LOG_DUMP
	pkt_ctx.resourcemgr.kernel_log_buf = kunit_kzalloc(test, SZ_2M, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pkt_ctx.resourcemgr.kernel_log_buf);
#endif
	pkt_ctx.resourcemgr.minfo = &pkt_ctx.minfo;

	pkt_ctx.pb.ops = &pkt_buf_ops;
	pkt_ctx.resourcemgr.minfo->pb_taaisp_s = &pkt_ctx.pb;
	pkt_ctx.resourcemgr.minfo->pb_tnr_s = &pkt_ctx.pb;
	pkt_ctx.resourcemgr.minfo->pb_pregion = &pkt_ctx.pb;

	pablo_rscmgr_set_sys_ops(&pkt_sys_ops, pablo_get_rscmgr_sys_ops(), true);

	func = pablo_kunit_rscmgr();

	return 0;
}

static void pablo_resourcemgr_kunit_test_exit(struct kunit *test)
{
	int i;

#ifdef ENABLE_KERNEL_LOG_DUMP
	kunit_kfree(test, pkt_ctx.resourcemgr.kernel_log_buf);
	pkt_ctx.resourcemgr.kernel_log_buf = NULL;
#endif
	for (i = 0; i < GROUP_SLOT_MAX; i++) {
		kunit_kfree(test, pkt_ctx.ischain.group[i]);
		pkt_ctx.ischain.group[i] = NULL;
	}

	pablo_rscmgr_set_sys_ops(pablo_get_rscmgr_sys_ops(), &pkt_sys_ops, false);

	func = NULL;
}

static struct kunit_case pablo_resourcemgr_kunit_test_cases[] = {
	KUNIT_CASE(pablo_rscmgr_init_log_rmem_kunit_test),
	KUNIT_CASE(pablo_rscmgr_init_dynamic_mem_kunit_test),
	KUNIT_CASE(pablo_rscmgr_secure_mem_kunit_test),
	KUNIT_CASE(pablo_rscmgr_bts_scen_kunit_test),
	KUNIT_CASE(pablo_rscmgr_resource_ioctl_kunit_test),
	KUNIT_CASE(pablo_rscmgr_logsync_kunit_test),
	KUNIT_CASE(pablo_rscmgr_update_lic_sram_kunit_test),
	KUNIT_CASE(pablo_rscmgr_kernel_log_dump_kunit_test),
	KUNIT_CASE(pablo_rscmgr_get_shot_timeout_kunit_test),
	KUNIT_CASE(pablo_rscmgr_init_mem_kunit_test),
	KUNIT_CASE(pablo_rscmgr_set_global_param_kunit_test),
	KUNIT_CASE(pablo_rscmgr_clear_global_param_kunit_test),
	KUNIT_CASE(pablo_rscmgr_clear_cluster_qos_kunit_test),
	KUNIT_CASE(pablo_rscmgr_dump_kunit_test),
	KUNIT_CASE(pablo_rscmgr_cdump_kunit_test),
	KUNIT_CASE(pablo_rscmgr_get_sensor_kunit_test),
	KUNIT_CASE(pablo_rscmgr_put_sensor_kunit_test),
	KUNIT_CASE(pablo_rscmgr_get_ischain_kunit_test),
	KUNIT_CASE(pablo_rscmgr_put_ischain_kunit_test),
	{},
};

struct kunit_suite pablo_resourcemgr_kunit_test_suite = {
	.name = "pablo-resourcemgr-kunit-test",
	.init = pablo_resourcemgr_kunit_test_init,
	.exit = pablo_resourcemgr_kunit_test_exit,
	.test_cases = pablo_resourcemgr_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_resourcemgr_kunit_test_suite);

struct kunit_suite pablo_resourcemgr_kunit_test_suite_end = {
	.name = "pablo-resourcemgr-kunit-test-end",
	.test_cases = NULL,
};
define_pablo_kunit_test_suites_end(&pablo_resourcemgr_kunit_test_suite_end);
MODULE_LICENSE("GPL");
