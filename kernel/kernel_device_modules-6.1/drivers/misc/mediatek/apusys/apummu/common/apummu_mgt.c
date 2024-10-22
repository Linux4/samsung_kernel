// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/kref.h>
#include <linux/workqueue.h>

#include "apummu_drv.h"
#include "apummu_mgt.h"
#include "apummu_mem.h"
#include "apummu_remote_cmd.h"
#include "apummu_cmn.h"
#include "apummu_trace.h"

extern struct apummu_dev_info *g_adv;

struct apummu_tbl {
	struct list_head g_stable_head;
	struct kref session_tbl_cnt;
	struct mutex table_lock;
	struct mutex DRAM_FB_lock;
	bool is_stable_exist;
	bool is_SLB_set;
	bool is_work_canceled;
	bool is_free_job_set;
	bool is_SLB_alloc; // Since SLB state might not sync with APU
};

struct apummu_tbl g_ammu_table_set;
struct apummu_session_tbl *g_ammu_stable_ptr; // stable stand for session table

#define AMMU_FREE_DRAM_DELAY_MS	(5 * 1000)
static struct workqueue_struct *ammu_workq;
static struct delayed_work DRAM_free_work;

#define IOVA2EVA_ENCODE_EN	(1)
#define PAGE_ARRAY_CNT_EN	(1)
#define SHIFT_BITS			(12)

#if IOVA2EVA_ENCODE_EN
#define ENCODE_OFFSET	(0x20000000)
#define IOVA2EVA(input_addr)	(input_addr - ENCODE_OFFSET) // -512M
#define EVA2IOVA(input_addr)	(input_addr + ENCODE_OFFSET) // +512M
#else
#define IOVA2EVA(input_addr)	(input_addr)
#define EVA2IOVA(input_addr)	(input_addr)
#endif

/**
 * @input:
 *  type -> buffer type
 *  input_addr -> addr to encode (IOVA)
 *  output_addr -> encoded address (EVA)
 * @output:
 *  if encode succeeded
 * @description:
 *  encode input addr according to type
 */
static int addr_encode(uint64_t input_addr, enum AMMU_BUF_TYPE type, uint64_t *output_addr)
{
	int ret = 0;
	uint64_t ret_addr;

	switch (type) {
	case AMMU_DATA_BUF:
		ret_addr = IOVA2EVA(input_addr);
		break;
	case AMMU_CMD_BUF:
	case AMMU_VLM_BUF:
		ret_addr = input_addr;
		break;
	default:
		AMMU_LOG_ERR("APUMMU encode invalid buffer type(%u)\n", type);
		ret = -EINVAL;
		goto out;
	}

	*output_addr = ret_addr;
out:
	return ret;
}

static int addr_decode(uint64_t input_addr, enum AMMU_BUF_TYPE type, uint64_t *output_addr)
{
	int ret = 0;
	uint64_t ret_addr;

	switch (type) {
	case AMMU_DATA_BUF:
		ret_addr = EVA2IOVA(input_addr);
		break;
	case AMMU_CMD_BUF:
	case AMMU_VLM_BUF:
		ret_addr = input_addr;
		break;
	default:
		AMMU_LOG_ERR("APUMMU decode invalid buffer type(%u)\n", type);
		ret = -EINVAL;
		goto out;
	}

	*output_addr = ret_addr;
out:
	return ret;
}

int apummu_eva_decode(uint64_t eva, uint64_t *iova, enum AMMU_BUF_TYPE type)
{
	return addr_decode(eva, type, iova);
}

/**
 * @input:
 *  session -> for session check
 * @output:
 *  if the stable of input session is exist
 * @description:
 *  Check if session table of input session exist
 *  also bind exist stable to g_ammu_stable_ptr
 */
static bool is_session_table_exist(uint64_t session)
{
	bool isExist = false;
	struct list_head *list_ptr;

	list_for_each(list_ptr, &g_ammu_table_set.g_stable_head) {
		g_ammu_stable_ptr = list_entry(list_ptr, struct apummu_session_tbl, list);
		if (g_ammu_stable_ptr->session == session) {
			isExist = true;
			break;
		}
	}

	return isExist;
}

static void ammu_DRAM_free_work(struct work_struct *work)
{
	mutex_lock(&g_ammu_table_set.DRAM_FB_lock);

	if (g_ammu_table_set.is_work_canceled) {
		ammu_trace_begin("APUMMU: free DRAM");
		apummu_dram_remap_runtime_free(g_adv);
		ammu_trace_end();
		AMMU_LOG_INFO("Delay DRAM Free done\n");
	} else
		g_ammu_table_set.is_work_canceled = true;

	g_ammu_table_set.is_free_job_set = false;
	mutex_unlock(&g_ammu_table_set.DRAM_FB_lock);
}

static void free_memory(struct kref *kref)
{
	ammu_trace_begin("APUMMU: free memory");

	AMMU_LOG_DBG("kref destroy\n");
#if DRAM_FALL_BACK_IN_RUNTIME
	if (g_adv->rsc.vlm_dram.iova != 0) {
		queue_delayed_work(ammu_workq, &DRAM_free_work,
			msecs_to_jiffies(AMMU_FREE_DRAM_DELAY_MS));
		g_ammu_table_set.is_free_job_set = true;
	}
#endif

	if (g_ammu_table_set.is_SLB_alloc) {
		ammu_trace_begin("APUMMU: Free SLB without IPI");
		/* MDW will close session in IPI handler in some case */
		// apummu_remote_mem_free_pool(g_adv);
		if (apummu_free_general_SLB(g_adv))
			AMMU_LOG_WRN("General APU SLB free fail\n");

		g_ammu_table_set.is_SLB_alloc = false;
		ammu_trace_end();
	}

	g_ammu_table_set.is_stable_exist = false;
	g_ammu_table_set.is_SLB_set = false;

	ammu_trace_end();
}

/**
 * @input:
 *  None
 * @output:
 *  if stable alloc succeeded
 * @description:
 *  bind stable to g_ammu_stable_ptr if unused table exist
 */
static int session_table_alloc(void)
{
	int ret = 0;
	struct apummu_session_tbl *sTable_ptr = NULL;
	ammu_trace_begin("APUMMU: session table allocate");

	sTable_ptr = kvzalloc(sizeof(struct apummu_session_tbl), GFP_KERNEL);
	if (!sTable_ptr) {
		AMMU_LOG_ERR("Session table alloc failed, kvzalloc failed\n");
		ret = -ENOMEM;
		goto out;
	}

	list_add_tail(&sTable_ptr->list, &g_ammu_table_set.g_stable_head);
	g_ammu_stable_ptr = sTable_ptr;

#if DRAM_FALL_BACK_IN_RUNTIME
	mutex_lock(&g_ammu_table_set.DRAM_FB_lock);
	if (g_adv->remote.vlm_size != 0) {
		if (g_adv->rsc.vlm_dram.iova == 0) {
			ammu_trace_begin("APUMMU: Alloc DRAM");
			ret = apummu_dram_remap_runtime_alloc(g_adv);
			if (ret) {
				ammu_trace_end();
				ammu_exception("alloc DRAM FB fail\n");
				mutex_unlock(&g_ammu_table_set.DRAM_FB_lock);
				goto out;
			}
			ammu_trace_end();
		} else { // DRAM not free, cancel delay job
			if (!cancel_delayed_work(&DRAM_free_work) && g_ammu_table_set.is_free_job_set)
				g_ammu_table_set.is_work_canceled = false;
			else
				g_ammu_table_set.is_free_job_set = false;
		}
	}

	mutex_unlock(&g_ammu_table_set.DRAM_FB_lock);
#endif

	if (g_adv->plat.is_general_SLB_support)
		if (!g_ammu_table_set.is_SLB_alloc) { // SLB retry
			ammu_trace_begin("APUMMU: SLB alloc");
			/* Do not assign return value, since alloc SLB may fail */
			if (apummu_alloc_general_SLB(g_adv))
				AMMU_LOG_VERBO("general SLB alloc fail...\n");
			else
				g_ammu_table_set.is_SLB_alloc = true;

			ammu_trace_end();
		}

	if (!g_ammu_table_set.is_stable_exist) {
	#if DRAM_FALL_BACK_IN_RUNTIME
		ammu_trace_begin("APUMMU: SLB + DRAM IPI");
		if (g_adv->rsc.vlm_dram.iova != 0 || g_ammu_table_set.is_SLB_alloc) {
			ret = apummu_remote_set_hw_default_iova_one_shot(g_adv);
			if (ret) {
				ammu_trace_end();
				AMMU_LOG_ERR("Remote set hw IOVA one shot fail!!\n");
				ammu_exception("Set DRAM FB + SLB fail\n");
				goto free_DRAM;
			}
		}
		ammu_trace_end();
	#else
		if (g_ammu_table_set.is_SLB_alloc) {
			if (apummu_remote_mem_add_pool(g_adv))
				goto free_general_SLB;
		}
	#endif
		g_ammu_table_set.is_SLB_set = (g_ammu_table_set.is_SLB_alloc);

		AMMU_LOG_VERBO("kref init\n");
		kref_init(&g_ammu_table_set.session_tbl_cnt);
		g_ammu_table_set.is_stable_exist = true;
	} else {
		AMMU_LOG_VERBO("kref get\n");
		kref_get(&g_ammu_table_set.session_tbl_cnt);

		/* SLB retry IPI */
		if (!g_ammu_table_set.is_SLB_set && g_ammu_table_set.is_SLB_alloc) {
			ammu_trace_begin("APUMMU: SLB ONLY IPI");
			ret = apummu_remote_mem_add_pool(g_adv);
			if (ret) {
				ammu_trace_end();
				ammu_exception("Set SLB fail\n");
				goto free_general_SLB;
			}
			ammu_trace_end();
		}

		g_ammu_table_set.is_SLB_set = true;
	}

	ammu_trace_end();
	return ret;

#if DRAM_FALL_BACK_IN_RUNTIME
free_DRAM:
	apummu_dram_remap_runtime_free(g_adv);
#endif
free_general_SLB:
	apummu_free_general_SLB(g_adv);
out:
	ammu_trace_end();
	return ret;
}

/* device_va == iova */
int addr_encode_and_write_stable(enum AMMU_BUF_TYPE type, uint64_t session, uint64_t device_va,
								uint32_t buf_size, uint64_t *eva)
{
	int ret = 0;
	uint64_t ret_eva;
	uint32_t SLB_type, cross_page_array_num = 0;
	uint8_t mask_idx;

	ammu_trace_begin("APUMMU: add table");

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		goto out_before_lock;
	}

	if (device_va & 0xFFF) {
		AMMU_LOG_ERR("device_va is not 4K alignment!!!\n");
		ret = -EINVAL;
		goto out_before_lock;
	}

	AMMU_LOG_VERBO("session = 0x%llx, device_va = 0x%llx, size = 0x%x\n",
		session, device_va, buf_size);

	if (device_va < 0x40000000) {
		if (((device_va >= g_adv->remote.vlm_addr) &&
			(device_va <= (g_adv->remote.vlm_addr + g_adv->remote.vlm_size)))) {
			ret_eva = device_va;
			goto out;
		} else if ((device_va >= g_adv->remote.SLB_base_addr) &&
			((device_va < (g_adv->remote.SLB_base_addr + g_adv->remote.SLB_size)))) {
			SLB_type = (device_va == g_adv->rsc.internal_SLB.iova)
				? APUMMU_MEM_TYPE_RSV_S
				: APUMMU_MEM_TYPE_EXT;

			ret = ammu_session_table_add_SLB(session, SLB_type);
			if (ret) {
				AMMU_LOG_ERR("IOVA 2 EVA SLB fail!!\n");
				goto out_before_lock;
			}

			ret_eva = (device_va == g_adv->rsc.internal_SLB.iova)
				? g_adv->remote.vlm_addr
				: device_va;

			goto out;
		} else {
			AMMU_LOG_ERR("Invalid input VA 0x%llx\n", device_va);
			ret = -EINVAL;
			goto out_before_lock;
		}
	}

	/* addr encode and CHECK input type */
	ret = addr_encode(device_va, type, &ret_eva);
	if (ret)
		goto out_before_lock;

	/* lock for g_ammu_stable_ptr */
	mutex_lock(&g_ammu_table_set.table_lock);

	/* check if session table exist by session */
	if (!is_session_table_exist(session)) {
		/* if session table not exist alloc a session table */
		ret = session_table_alloc();
		if (ret)
			goto out_after_lock;

		g_ammu_stable_ptr->session = session;
	}

	/* Hint for RV APUMMU fill VSID table */
	/* NOTE: cross_page_array_num is use when the given buffer cross differnet page array */
	if ((device_va >> SHIFT_BITS) & 0x300000) { // mask for 34-bit
		cross_page_array_num = (((device_va + buf_size) / (0x20000000))
							- ((device_va) / (0x20000000)));
		do {
			/* >> 29 = 512M / 0x20000000 */
			mask_idx = (((device_va - 0x100000000) >> 29)
						+ cross_page_array_num) & (0x1f);

			g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[1] |=
				(1 << mask_idx);
			g_ammu_stable_ptr->DRAM_4_16G_mask_cnter[mask_idx] += 1;

		} while (cross_page_array_num--);
		g_ammu_stable_ptr->stable_info.mem_mask |= (1 << DRAM_4_16G);
	} else {
		cross_page_array_num =
			(((device_va + buf_size) / (0x8000000)) - ((device_va) / (0x8000000)));
		do {
		#if IOVA2EVA_ENCODE_EN
			/* - (0x40000000) because mapping start from 1G */
			/* >> 27 = 128M / 0x20000000 */
			mask_idx = (((device_va - (0x40000000)) >> 27)
						+ cross_page_array_num) & (0x1f);
		#else
			mask_idx = ((device_va >> 27) + cross_page_array_num) &
						(0x1f);
		#endif

			g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[0] |=
				(1 << mask_idx);
			g_ammu_stable_ptr->DRAM_1_4G_mask_cnter[mask_idx] += 1;

		} while (cross_page_array_num--);
		g_ammu_stable_ptr->stable_info.mem_mask |= (1 << DRAM_1_4G);
	}

out:
	*eva = ret_eva;

	AMMU_LOG_VERBO("apummu add 0x%llx -> 0x%llx in 0x%llx stable done\n",
		ret_eva, device_va, session);

out_after_lock:
	mutex_unlock(&g_ammu_table_set.table_lock);
out_before_lock:
	ammu_trace_end();
	return ret;
}

static void count_page_array_en_num(void)
{
	int i;
	uint32_t idx;

	for (idx = 0; idx < 2; idx++) {
	#if PAGE_ARRAY_CNT_EN
		if (g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[idx] != 0) {
			for (i = 31; i >= 0; i--) {
				if (g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[idx]
					& (1 << i))
					break;
			}

			g_ammu_stable_ptr->stable_info.DRAM_page_array_en_num[idx] =
				i + 1;
		} else {
			g_ammu_stable_ptr->stable_info.DRAM_page_array_en_num[idx] = 0;
		}
	#else
		g_ammu_stable_ptr->stable_info.DRAM_page_array_en_num[idx] = 32;
	#endif
	}
}

/* get session table by session */
int get_session_table(uint64_t session, void **tbl_kva, uint32_t *size)
{
	int ret = 0;
	ammu_trace_begin("APUMMU: Get session table");

	mutex_lock(&g_ammu_table_set.table_lock);

	if (!is_session_table_exist(session)) {
		AMMU_LOG_ERR("Session table NOT exist!!!(0x%llx)\n", session);
		ret = -ENOMEM;
		goto out;
	}

	count_page_array_en_num();

	AMMU_LOG_VERBO("stable session(%llx), mem_mask = 0x%08x\n",
		g_ammu_stable_ptr->session,
		g_ammu_stable_ptr->stable_info.mem_mask);
	AMMU_LOG_VERBO("stable DRAM_page_array_mask 1~4G = 0x%08x, enable num = 0x%08x\n",
		g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[0],
		g_ammu_stable_ptr->stable_info.DRAM_page_array_en_num[0]);
	AMMU_LOG_VERBO("stable DRAM_page_array_mask 4~16G = 0x%08x, enable num = 0x%08x\n",
		g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[1],
		g_ammu_stable_ptr->stable_info.DRAM_page_array_en_num[1]);
	AMMU_LOG_VERBO("stable EXT_SLB_addr = 0x%08x, RSV_S (start, page) = (%u, %u)\n",
		g_ammu_stable_ptr->stable_info.EXT_SLB_addr,
		g_ammu_stable_ptr->stable_info.RSV_S_SLB_page_array_start,
		g_ammu_stable_ptr->stable_info.RSV_S_SLB_page);

	*tbl_kva = (void *) (&g_ammu_stable_ptr->stable_info);
	*size = sizeof(struct ammu_stable_info);

out:
	mutex_unlock(&g_ammu_table_set.table_lock);
	ammu_trace_end();
	return ret;
}

/* free session table by session */
int session_table_free(uint64_t session)
{
	int ret = 0;

	ammu_trace_begin("APUMMU: free session table");

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&g_ammu_table_set.table_lock);
	if (!is_session_table_exist(session)) {
		ret = -EFAULT;
		AMMU_LOG_ERR("free session table FAILED!!!, session table 0x%llx not found\n",
				session);
		goto out;
	}

	list_del(&g_ammu_stable_ptr->list);
	kvfree(g_ammu_stable_ptr);
	AMMU_LOG_VERBO("kref put\n");
	kref_put(&g_ammu_table_set.session_tbl_cnt, free_memory);

out:
	mutex_unlock(&g_ammu_table_set.table_lock);
	ammu_trace_end();
	return ret;
}

void dump_session_table_set(void)
{
	struct list_head *list_ptr;
	uint32_t i = 0;

	mutex_lock(&g_ammu_table_set.table_lock);

	AMMU_LOG_DBG("== APUMMU dump session table Start ==\n");
	AMMU_LOG_DBG("Total stable cnt = %u\n", kref_read(&g_ammu_table_set.session_tbl_cnt));
	AMMU_LOG_DBG("----------------------------------\n");

	list_for_each(list_ptr, &g_ammu_table_set.g_stable_head) {
		g_ammu_stable_ptr = list_entry(list_ptr, struct apummu_session_tbl, list);
		AMMU_LOG_DBG("== dump session table %u info ==\n", i++);
		AMMU_LOG_DBG("session              = 0x%llx\n",
			g_ammu_stable_ptr->session);
		AMMU_LOG_DBG("mem_mask             = 0x%x\n",
			g_ammu_stable_ptr->stable_info.mem_mask);
		AMMU_LOG_DBG("DRAM_page_array_mask = 0x%x 0x%x\n",
			g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[0],
			g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[1]);
		AMMU_LOG_DBG("DRAM_page_array_en_num = %u, %u\n",
			g_ammu_stable_ptr->stable_info.DRAM_page_array_en_num[0],
			g_ammu_stable_ptr->stable_info.DRAM_page_array_en_num[1]);
		AMMU_LOG_DBG("EXT_SLB_addr = 0x%x, RSV_S_SLB PA start = %u, page = %u\n",
			g_ammu_stable_ptr->stable_info.EXT_SLB_addr,
			g_ammu_stable_ptr->stable_info.RSV_S_SLB_page_array_start,
			g_ammu_stable_ptr->stable_info.RSV_S_SLB_page);
	}

	mutex_unlock(&g_ammu_table_set.table_lock);
	AMMU_LOG_DBG("== APUMMU dump session table End ==\n");
}

int ammu_session_table_add_SLB(uint64_t session, uint32_t type)
{
	int ret = 0;

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&g_ammu_table_set.table_lock);
	if (!is_session_table_exist(session)) {
		ret = -EINVAL;
		AMMU_LOG_ERR("Add SLB to stable FAILED!!!, session table 0x%llx not found\n",
				session);
		goto out;
	}

	if (type == APUMMU_MEM_TYPE_EXT) {
		if (!g_adv->plat.external_SLB_cnt) {
			ret = -ENOMEM;
			AMMU_LOG_ERR("External SLB is not alloced\n");
			goto out;
		}

		g_ammu_stable_ptr->stable_info.EXT_SLB_addr = g_adv->rsc.external_SLB.iova;
		g_ammu_stable_ptr->stable_info.mem_mask |= (1 << SLB_EXT);
	} else if (type == APUMMU_MEM_TYPE_RSV_S) {
		if (!g_adv->plat.internal_SLB_cnt) {
			ret = -ENOMEM;
			AMMU_LOG_ERR("Internal SLB is not alloced\n");
			goto out;
		}

		g_ammu_stable_ptr->stable_info.RSV_S_SLB_page_array_start =
			(g_adv->rsc.internal_SLB.iova - g_adv->remote.SLB_base_addr) >> 19;
		g_ammu_stable_ptr->stable_info.RSV_S_SLB_page =
			g_adv->rsc.internal_SLB.size >> 19; // / 512K
		g_ammu_stable_ptr->stable_info.mem_mask |= (1 << SLB_RSV_S);
	} else {
		AMMU_LOG_ERR("Invalid apu memory type\n");
		ret = -EINVAL;
		goto out;
	}

out:
	mutex_unlock(&g_ammu_table_set.table_lock);
	return ret;
}

static int ammu_remove_stable_SLB_status(uint32_t type)
{
	int ret = 0;

	if (type == APUMMU_MEM_TYPE_EXT) {
		g_ammu_stable_ptr->stable_info.EXT_SLB_addr = 0;
		g_ammu_stable_ptr->stable_info.mem_mask &= ~(1 << SLB_EXT);
	} else if (type == APUMMU_MEM_TYPE_RSV_S) {
		g_ammu_stable_ptr->stable_info.RSV_S_SLB_page_array_start = 0;
		g_ammu_stable_ptr->stable_info.RSV_S_SLB_page = 0;
		g_ammu_stable_ptr->stable_info.mem_mask &= ~(1 << SLB_RSV_S);
	} else {
		AMMU_LOG_ERR("Invalid apu memory type %u\n", type);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}


int apummu_stable_buffer_remove(uint64_t session, uint64_t device_va, uint32_t buf_size)
{
	int ret = 0;
	uint32_t SLB_type, cross_page_array_num = 0;
	uint8_t mask_idx;
	bool is_34bit;

	if (device_va < 0x40000000) {
		if (((device_va >= g_adv->remote.vlm_addr) &&
			(device_va <= (g_adv->remote.vlm_addr + g_adv->remote.vlm_size))))
			goto out;
		else if ((device_va >= g_adv->remote.SLB_base_addr) &&
			((device_va < (g_adv->remote.SLB_base_addr + g_adv->remote.SLB_size)))) {
			SLB_type = (device_va == g_adv->rsc.internal_SLB.iova)
				? APUMMU_MEM_TYPE_RSV_S
				: APUMMU_MEM_TYPE_EXT;

			ret = ammu_session_table_remove_SLB(session, SLB_type);
			if (ret) {
				AMMU_LOG_ERR("Remove SLB buffer fail!!\n");
				goto out;
			}

			goto out;
		} else {
			AMMU_LOG_ERR("Invalid input VA 0x%llx\n", device_va);
			ret = -EINVAL;
			goto out;
		}
	}

	mutex_lock(&g_ammu_table_set.table_lock);

	if (!is_session_table_exist(session)) {
		AMMU_LOG_ERR("Session table NOT exist!!!(0x%llx)\n", session);
		ret = -ENOMEM;
		goto out;
	}

	is_34bit = device_va & (0x300000000);

	if (is_34bit) {
		cross_page_array_num = (((device_va + buf_size) / (0x20000000))
							- ((device_va) / (0x20000000)));
		do {
			/* >> 29 = 512M / 0x20000000 */
			mask_idx = ((device_va >> 29) + cross_page_array_num) &
						(0x1f);

			g_ammu_stable_ptr->DRAM_4_16G_mask_cnter[mask_idx] -= 1;
			if (g_ammu_stable_ptr->DRAM_4_16G_mask_cnter[mask_idx] == 0) {
				g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[1] &=
					~(1 << mask_idx);
			}
		} while (cross_page_array_num--);

		if (g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[1] == 0)
			g_ammu_stable_ptr->stable_info.mem_mask &= ~(1 << DRAM_4_16G);
	} else {
		cross_page_array_num =
			(((device_va + buf_size) / (0x8000000)) - ((device_va) / (0x8000000)));
		do {
		#if IOVA2EVA_ENCODE_EN
			/* - (0x40000000) because mapping start from 1G */
			/* >> 27 = 128M / 0x20000000 */
			mask_idx = (((device_va - (0x40000000)) >> 27)
						+ cross_page_array_num) & (0x1f);
		#else
			mask_idx = ((device_va >> 27) + cross_page_array_num) &
						(0x1f);
		#endif

			g_ammu_stable_ptr->DRAM_1_4G_mask_cnter[mask_idx] -= 1;
			if (g_ammu_stable_ptr->DRAM_1_4G_mask_cnter[mask_idx] == 0) {
				g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[0] &=
					~(1 << mask_idx);
			}
		} while (cross_page_array_num--);

		if (g_ammu_stable_ptr->stable_info.DRAM_page_array_mask[0] == 0)
			g_ammu_stable_ptr->stable_info.mem_mask &= ~(1 << DRAM_1_4G);
	}

out:
	mutex_unlock(&g_ammu_table_set.table_lock);
	return ret;
}

int ammu_session_table_remove_SLB(uint64_t session, uint32_t type)
{
	int ret = 0;

	if (g_adv == NULL) {
		AMMU_LOG_ERR("Invalid apummu_device\n");
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&g_ammu_table_set.table_lock);
	if (!is_session_table_exist(session)) {
		ret = -EINVAL;
		AMMU_LOG_ERR("Remove SLB to stable FAILED!!!, session table 0x%llx not found\n",
				session);
		goto out;
	}

	ret = ammu_remove_stable_SLB_status(type);

out:
	mutex_unlock(&g_ammu_table_set.table_lock);
	return ret;
}

void ammu_session_table_check_SLB(uint32_t type)
{
	int ret = 0;
	struct list_head *list_ptr;

	mutex_lock(&g_ammu_table_set.table_lock);
	list_for_each(list_ptr, &g_ammu_table_set.g_stable_head) {
		g_ammu_stable_ptr = list_entry(list_ptr, struct apummu_session_tbl, list);

		if (type == APUMMU_MEM_TYPE_EXT) {
			if (g_ammu_stable_ptr->stable_info.EXT_SLB_addr != 0) {
				// AMMU_LOG_WRN("0x%llx is still using EXT_SLB after free\n",
				//		g_ammu_stable_ptr->session);

				ret = ammu_remove_stable_SLB_status(APUMMU_MEM_TYPE_EXT);
				if (ret)
					AMMU_LOG_ERR("ammu_remove_stable_SLB_status fail\n");
			}
		} else if (type == APUMMU_MEM_TYPE_RSV_S) {
			if (g_ammu_stable_ptr->stable_info.RSV_S_SLB_page_array_start != 0) {
				// AMMU_LOG_WRN("0x%llx is still using RSV_S_SLB after free\n",
				//		g_ammu_stable_ptr->session);

				ret = ammu_remove_stable_SLB_status(APUMMU_MEM_TYPE_RSV_S);
				if (ret)
					AMMU_LOG_ERR("ammu_remove_stable_SLB_status fail\n");
			}
		}
	}

	mutex_unlock(&g_ammu_table_set.table_lock);
}

/* Init lust head, lock */
void apummu_mgt_init(void)
{
	char wq_name[] = "ammu_dram_free";

	g_ammu_table_set.is_stable_exist = false;
	g_ammu_table_set.is_SLB_set = false;
	g_ammu_table_set.is_work_canceled = true;
	g_ammu_table_set.is_SLB_alloc = false;
	INIT_LIST_HEAD(&g_ammu_table_set.g_stable_head);
	mutex_init(&g_ammu_table_set.table_lock);
	mutex_init(&g_ammu_table_set.DRAM_FB_lock);

	INIT_DELAYED_WORK(&DRAM_free_work, ammu_DRAM_free_work);
	ammu_workq = alloc_ordered_workqueue(wq_name, WQ_MEM_RECLAIM);
}

/* apummu_mgt_destroy session table set */
void apummu_mgt_destroy(void)
{
	struct list_head *list_ptr1, *list_ptr2;

	mutex_lock(&g_ammu_table_set.table_lock);
	list_for_each_safe(list_ptr1, list_ptr2, &g_ammu_table_set.g_stable_head) {
		g_ammu_stable_ptr = list_entry(list_ptr1, struct apummu_session_tbl, list);
		list_del(&g_ammu_stable_ptr->list);
		kvfree(g_ammu_stable_ptr);
		g_ammu_stable_ptr = NULL;
		AMMU_LOG_VERBO("kref put\n");
		kref_put(&g_ammu_table_set.session_tbl_cnt, free_memory);
	}

	mutex_unlock(&g_ammu_table_set.table_lock);

	/* Flush free DRAM job */
	flush_delayed_work(&DRAM_free_work);
}
