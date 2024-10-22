// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <asm/mman.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/sysinfo.h>
#include <linux/seq_file.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>
#include <linux/proc_fs.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#include <mt-plat/mrdump.h>
#endif

#include "apu.h"
#include "apusys_secure.h"
#include "apusys_core.h"
#include "hw_logger.h"

#include <linux/sched/clock.h>
#include <linux/sched/signal.h>

#define HW_LOG_DUMP_RAW_BUF (1)
#define APUSYS_RV_DEBUG_INFO_DUMP (1)

/* debug log level */
static unsigned char g_hw_logger_log_lv = DBG_LOG_INFO;

/* dev */
#define HW_LOGGER_DEV_NAME "apu_hw_logger"
static struct device *hw_logger_dev;

#ifdef HW_LOG_SYSFS_BIN
/* sysfs related */
static struct kobject *root_dir;
#endif

/* procfs related */
static struct proc_dir_entry *log_root;
static struct proc_dir_entry *log_devinfo;
static struct proc_dir_entry *log_devattr;
static struct proc_dir_entry *log_seqlog;
static struct proc_dir_entry *log_seqlogL;
static struct proc_dir_entry *log_aov_log;
#ifdef HW_LOG_DUMP_RAW_BUF
static struct proc_dir_entry *log_raw_log;
static struct proc_dir_entry *log_aov_tcm_log;
#endif

/* logtop mmap address */
static void *apu_logtop;
static void *apu_mbox;
static void *apu_rpc;

#define LOG_W_OFS_MBOX    (apu_mbox + 0x40)
#define LOG_R_OFS_MBOX    (apu_mbox + 0x44)

/* hw log buffer related */
static unsigned int hw_log_lbc_size;
static unsigned int aov_tcm_buf_init;
static char *hw_log_buf;
static char *aov_hw_log_buf;
static char *aov_tcm_buf;
static dma_addr_t hw_log_buf_addr;
static dma_addr_t aov_hw_log_buf_addr;

/* local buffer related */
DEFINE_MUTEX(hw_logger_mutex);
DEFINE_SPINLOCK(hw_logger_spinlock);
static DEFINE_SPINLOCK(smc_call_spinlock);
static char *local_log_buf;

/* for local buffer tracking */
static unsigned int __loc_log_w_ofs;
static unsigned int __loc_log_ov_flg;
static unsigned int __loc_log_sz;

/* for irq disable */
static unsigned int __hw_log_r_reg;

static unsigned int access_rcx_in_atf;
static unsigned int enable_interrupt;
static int hw_logger_irq_number;

#ifdef HW_LOG_SYSFS_BIN
/* for sysfs normal dump */
static unsigned int g_dump_log_r_ofs;
#endif

uint32_t aov_log_buf_sz;

/* for interrupt mode disable only */
atomic_t apu_toplog_deep_idle;

struct hw_logger_seq_data {
	unsigned int w_ptr;
	unsigned int r_ptr;
	unsigned int ov_flg;
	unsigned int not_rd_sz;
	int is_debug_info_dump;
	int is_debug_info_dump_finished;
	bool nonblock;
};

/* for procfs normal dump */
static struct hw_logger_seq_data g_log;
/* for procfs lock dump */
static struct hw_logger_seq_data g_log_l;
/* for procfs mobile logger lock dump */
static struct hw_logger_seq_data g_log_lm;

/* for interrupt mode disable only */
static struct workqueue_struct *apusys_hwlog_wq;
static struct work_struct apusys_hwlog_task;
static struct delayed_work apusys_mtklog_task;

static wait_queue_head_t apusys_hwlog_wait;

#define MTKLOG_WAKEUP_MS (1000)

static struct mtk_apu *g_apu;

struct hw_log_level_data {
	unsigned int level;
};

static struct hw_log_level_data hw_ipi_loglv_data = {IPI_DEBUG_LEVEL};

#define APUSYS_HWLOG_WQ_NAME "apusys_hwlog_wq"

/* debug log */
#define PROC_WRITE_BUFSIZE 16
#define CLEAR_LOG_CMD "clear"

static void hw_logger_buf_invalidate(void)
{
	dma_sync_single_for_cpu(
		hw_logger_dev, hw_log_buf_addr,
		HWLOGR_LOG_SIZE, DMA_FROM_DEVICE);
}

static int hw_logger_buf_alloc(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct arm_smccc_res smc_res;
	int ret = 0;

	ret = dma_set_coherent_mask(dev, DMA_BIT_MASK(64));
	if (ret) {
		HWLOGR_ERR("dma_set_coherent_mask fail (%d)\n", ret);
		ret = -ENOMEM;
		goto out;
	}

	/* local buffer for dump */
	local_log_buf = kzalloc(LOCAL_LOG_SIZE, GFP_KERNEL);
	if (!local_log_buf) {
		ret = -ENOMEM;
		goto out;
	}

	/* memory used by hw logger */
	hw_log_buf = kzalloc(HWLOGR_LOG_SIZE, GFP_KERNEL);
	if (!hw_log_buf) {
		ret = -ENOMEM;
		goto out;
	}

	hw_log_buf_addr = dma_map_single(dev, hw_log_buf,
		HWLOGR_LOG_SIZE, DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, hw_log_buf_addr);
	if (ret) {
		HWLOGR_ERR("dma_map_single fail for hw_log_buf_addr (%d)\n", ret);
		ret = -ENOMEM;
		goto out;
	}

	ret = of_property_read_u32(np, "aov-log-buf-sz",
				   &aov_log_buf_sz);
	if (ret || (aov_log_buf_sz == 0)) {
		dev_info(dev, "not support aov_log_buf_sz(%d)(ret = %d)\n",
			aov_log_buf_sz, ret);
		ret = 0;
	} else if (aov_log_buf_sz != 0) {
		dev_info(dev, "aov_log_buf_sz = %d\n", aov_log_buf_sz);

		/* memory used by hw logger for aov */
		aov_hw_log_buf = kzalloc(aov_log_buf_sz, GFP_KERNEL);
		if (!aov_hw_log_buf) {
			ret = -ENOMEM;
			goto out;
		}

		aov_tcm_buf = kzalloc(aov_log_buf_sz, GFP_KERNEL);
		if (!aov_tcm_buf) {
			ret = -ENOMEM;
			goto out;
		}

		arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
			MTK_APUSYS_KERNEL_OP_APUSYS_SETUP_TCM_LOG_MEM,
			__pa_nodebug(aov_tcm_buf), aov_log_buf_sz, 0, 0, 0, 0, &smc_res);

		if (smc_res.a0 != 0)
			HWLOGR_ERR("arm_smccc_smc setup_tcm_log_mem error ret: 0x%lx", smc_res.a0);
		else
			aov_tcm_buf_init = 1;

		aov_hw_log_buf_addr = dma_map_single(dev, aov_hw_log_buf,
			aov_log_buf_sz, DMA_FROM_DEVICE);
		ret = dma_mapping_error(dev, aov_hw_log_buf_addr);
		if (ret) {
			HWLOGR_ERR("dma_map_single fail for aov_hw_log_buf_addr (%d)\n",
				ret);
			ret = -ENOMEM;
			goto out;
		}
	}

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	(void)mrdump_mini_add_extra_file(
		(unsigned long)local_log_buf,
		__pa_nodebug(local_log_buf),
		LOCAL_LOG_SIZE, "APUSYS_LOG");

	(void)mrdump_mini_add_extra_file(
		(unsigned long)hw_log_buf,
		__pa_nodebug(hw_log_buf),
		HWLOGR_LOG_SIZE, "APUSYS_HW_LOG");

	if (aov_hw_log_buf && aov_log_buf_sz != 0)
		(void)mrdump_mini_add_extra_file(
			(unsigned long)aov_hw_log_buf,
			__pa_nodebug(aov_hw_log_buf),
			aov_log_buf_sz, "APUSYS_AOV_LOG");

	if (aov_tcm_buf && aov_tcm_buf_init && aov_log_buf_sz != 0)
		(void)mrdump_mini_add_extra_file(
			(unsigned long)aov_tcm_buf,
			__pa_nodebug(aov_tcm_buf),
			aov_log_buf_sz, "APUSYS_AOV_TCM_LOG");
#endif

	HWLOGR_INFO("local_log_buf = 0x%llx\n", (unsigned long long)local_log_buf);
	HWLOGR_INFO("hw_log_buf = 0x%llx, hw_log_buf_addr = 0x%llx\n",
		(unsigned long long)hw_log_buf, hw_log_buf_addr);
	HWLOGR_INFO("aov_hw_log_buf = 0x%llx, aov_hw_log_buf_addr = 0x%llx\n",
		(unsigned long long)aov_hw_log_buf, aov_hw_log_buf_addr);

out:
	return ret;
}

int hw_logger_dump_tcm_log(void)
{
	struct arm_smccc_res res;

	if (aov_tcm_buf_init == 0)
		return -1;

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
		MTK_APUSYS_KERNEL_OP_APUSYS_DUMP_TCM_LOG,
		0, 0, 0, 0, 0, 0, &res);

	if (res.a0 != 0) {
		HWLOGR_ERR("arm_smccc_smc dump_tcm_log error ret: 0x%lx", res.a0);
		return -1;
	}
	return 0;
}

static int ioread32_atf(uint8_t op_num, void **addr, uint32_t **ret_val)
{
	int i, op = 0;
	uint8_t smc_op;
	struct arm_smccc_res res;
	unsigned long flags = 0;

	if (op_num == 0 || op_num > MAX_SMC_OP_NUM) {
		HWLOGR_ERR("op_num invalid: %d\n", op_num);
		return -EINVAL;
	}

	for (i = 0; i < op_num; i++) {
		if (addr[i] == APU_LOG_BUF_T_SIZE)
			smc_op = SMC_OP_APU_LOG_BUF_T_SIZE;
		else if (addr[i] == APU_LOG_BUF_W_PTR)
			smc_op = SMC_OP_APU_LOG_BUF_W_PTR;
		else if (addr[i] == APU_LOG_BUF_R_PTR)
			smc_op = SMC_OP_APU_LOG_BUF_R_PTR;
		else if (addr[i] == APU_LOGTOP_CON)
			smc_op = SMC_OP_APU_LOG_BUF_CON;
		else {
			HWLOGR_ERR("Not support addr: 0x%llx", (unsigned long long)addr[i]);
			return -EINVAL;
		}
		op |= smc_op << (8 * i);
	}
	HWLOGR_DBG("arm_smccc_smc reg_dump op: 0x%08x\n", op);

	spin_lock_irqsave(&smc_call_spinlock, flags);
	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
		MTK_APUSYS_KERNEL_OP_APUSYS_LOGTOP_REG_DUMP,
		op, 0, 0, 0, 0, 0, &res);
	spin_unlock_irqrestore(&smc_call_spinlock, flags);

	HWLOGR_DBG("arm_smccc_smc reg_dump a0 a1 a2 a3 / 0x%lx 0x%lx 0x%lx 0x%lx\n",
		res.a0, res.a1, res.a2, res.a3);

	if (res.a0 != 0) {
		HWLOGR_ERR("arm_smccc_smc reg_dump error ret: 0x%lx", res.a0);
		HWLOGR_ERR("arm_smccc_smc reg_dump op: 0x%08x\n", op);
		HWLOGR_ERR("arm_smccc_smc reg_dump a0 a1 a2 a3 / 0x%lx 0x%lx 0x%lx 0x%lx\n",
					res.a0, res.a1, res.a2, res.a3);
		return res.a0;
	}

	*ret_val[0] = res.a1;
	if (op_num > 1)
		*ret_val[1] = res.a2;
	if (op_num > 2)
		*ret_val[2] = res.a3;

	return 0;
}

static int iowrite32_atf(uint32_t write_val, void *addr)
{
	int op;
	struct arm_smccc_res res;
	unsigned long flags = 0;

	if (addr == APU_LOG_BUF_R_PTR)
		op = SMC_OP_APU_LOG_BUF_R_PTR;
	else {
		HWLOGR_ERR("Not support addr: 0x%llx", (unsigned long long)addr);
		return -EINVAL;
	}

	HWLOGR_DBG("arm_smccc_smc reg_write op val / 0x%x 0x%x\n", op, write_val);

	spin_lock_irqsave(&smc_call_spinlock, flags);
	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
		MTK_APUSYS_KERNEL_OP_APUSYS_LOGTOP_REG_WRITE,
		op, write_val, 0, 0, 0, 0, &res);
	spin_unlock_irqrestore(&smc_call_spinlock, flags);

	HWLOGR_DBG("arm_smccc_smc reg_write a0 a1 / 0x%lx 0x%lx\n", res.a0, res.a1);

	if (res.a0 != 0) {
		HWLOGR_ERR("arm_smccc_smc reg_write error ret: 0x%lx", res.a0);
		HWLOGR_ERR("arm_smccc_smc reg_write op val / 0x%x 0x%x\n", op, write_val);
		HWLOGR_ERR("arm_smccc_smc reg_write a0 a1 / 0x%lx 0x%lx\n", res.a0, res.a1);
		return res.a0;
	}

	return 0;
}

static int w1c32_atf(void *addr, uint32_t *ret_val)
{
	int op;
	struct arm_smccc_res res;
	unsigned long flags = 0;

	if (addr == APU_LOGTOP_CON)
		op = SMC_OP_APU_LOG_BUF_CON;
	else {
		HWLOGR_ERR("Not support addr: 0x%llx", (unsigned long long)addr);
		return -EINVAL;
	}

	HWLOGR_DBG("arm_smccc_smc reg_w1c op / 0x%x\n", op);

	spin_lock_irqsave(&smc_call_spinlock, flags);
	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL,
		MTK_APUSYS_KERNEL_OP_APUSYS_LOGTOP_REG_W1C,
		op, 0, 0, 0, 0, 0, &res);
	spin_unlock_irqrestore(&smc_call_spinlock, flags);

	HWLOGR_DBG("arm_smccc_smc reg_w1c a0 a1 / 0x%lx 0x%lx\n", res.a0, res.a1);

	if (res.a0 != 0) {
		HWLOGR_ERR("arm_smccc_smc reg_w1c error ret: 0x%lx", res.a0);
		HWLOGR_ERR("arm_smccc_smc reg_w1c op / 0x%x\n", op);
		HWLOGR_ERR("arm_smccc_smc reg_w1c a0 a1 / 0x%lx 0x%lx\n", res.a0, res.a1);
		return res.a0;
	}

	*ret_val = res.a1;
	return 0;
}

unsigned long long get_st_addr(void)
{
	return (unsigned long long) hw_log_buf_addr;
}

unsigned long long get_t_size(void)
{
	return HWLOGR_LOG_SIZE;
}

static int get_r_w_ptr(unsigned long long *r_ptr, unsigned long long *w_ptr)
{
	int ret = 0;
	uint32_t r_ptr_reg = 0, w_ptr_reg = 0;

	if (enable_interrupt) {
		if (access_rcx_in_atf) {
			ret = ioread32_atf(2,
				(void*[]){APU_LOG_BUF_R_PTR, APU_LOG_BUF_W_PTR},
				(uint32_t*[]){&r_ptr_reg, &w_ptr_reg}
			);
			if (ret)
				goto out;
		} else {
			r_ptr_reg = ioread32(APU_LOG_BUF_R_PTR);
			w_ptr_reg = ioread32(APU_LOG_BUF_W_PTR);
		}
	} else {
		r_ptr_reg = __hw_log_r_reg;
		if (access_rcx_in_atf) {
			ret = ioread32_atf(1,
				(void*[]){APU_LOG_BUF_W_PTR},
				(uint32_t*[]){&w_ptr_reg}
			);
			if (ret)
				goto out;
		} else {
			w_ptr_reg = ioread32(APU_LOG_BUF_W_PTR);
		}
	}

	/* hw log w/r_ptr is a 34bit addr but store in a 32bit feild */
	if (r_ptr != NULL)
		*r_ptr = (unsigned long long)r_ptr_reg << 2;
	if (w_ptr != NULL)
		*w_ptr = (unsigned long long)w_ptr_reg << 2;

out:
	return ret;
}

static int set_r_ptr(unsigned long long r_ptr)
{
	int ret = 0;

	if (enable_interrupt) {
		if (access_rcx_in_atf)
			ret = iowrite32_atf(lower_32_bits(r_ptr >> 2), APU_LOG_BUF_R_PTR);
		else
			iowrite32(lower_32_bits(r_ptr >> 2), APU_LOG_BUF_R_PTR);
	} else
		__hw_log_r_reg = lower_32_bits(r_ptr >> 2);

	return ret;
}

static void store_r_ofs(unsigned int pwr_status, unsigned int r_ofs)
{
	/* set read pointer */
	if (pwr_status != 0) {
		HWLOGR_DBG("set_r_ptr r_ofs = 0x%x\n", r_ofs);
		set_r_ptr(get_st_addr() + r_ofs);
	}

	if (enable_interrupt) {
		HWLOGR_DBG("set_r_ofs_mbox r_ofs = 0x%x\n", r_ofs);
		iowrite32(r_ofs, LOG_R_OFS_MBOX);
	}
}

static void apu_hw_log_level_ipi_handler(void *data, unsigned int len, void *priv)
{
	int ret;
	unsigned int log_level = *(unsigned int *)data;

	HWLOGR_INFO("log_level = 0x%x (%d)\n", log_level, len);
	ret = apu_power_on_off(g_apu->pdev, APU_IPI_LOG_LEVEL, 0, 1);
	if (ret && ret != -EOPNOTSUPP)
		HWLOGR_ERR("apu_power_on_off fail(%d)\n", ret);
}

int hw_logger_config_init(struct mtk_apu *apu)
{
	unsigned long flags;
	struct logger_init_info *st_logger_init_info;

	if (!apu || !apu->conf_buf) {
		HWLOGR_ERR("invalid argument: apu\n");
		return -EINVAL;
	}

	if (!apu_logtop)
		return 0;

	st_logger_init_info = (struct logger_init_info *)
		get_apu_config_user_ptr(apu->conf_buf, eLOGGER_INIT_INFO);

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	g_log.w_ptr = 0;
	g_log.r_ptr = U32_MAX;
	g_log.ov_flg = 0;
	g_log_l.w_ptr = 0;
	g_log_l.r_ptr = U32_MAX;
	g_log_l.ov_flg = 0;
	g_log_lm.w_ptr = 0;
	g_log_lm.r_ptr = U32_MAX;
	g_log_lm.ov_flg = 0;
#ifdef HW_LOG_SYSFS_BIN
	g_dump_log_r_ofs = U32_MAX;
#endif
	__loc_log_sz = 0;

	atomic_set(&apu_toplog_deep_idle, 0);

	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	if (hw_log_buf_addr) {
		st_logger_init_info->iova =
			lower_32_bits(hw_log_buf_addr);
		st_logger_init_info->iova_h =
			upper_32_bits(hw_log_buf_addr);
		apu->apummu_hwlog_buf_da = hw_log_buf_addr;
		st_logger_init_info->lbc_sz = hw_log_lbc_size;
	}

	if (aov_hw_log_buf_addr) {
		st_logger_init_info->aov_iova =
			lower_32_bits(aov_hw_log_buf_addr);
		st_logger_init_info->aov_iova_h =
			upper_32_bits(aov_hw_log_buf_addr);
		st_logger_init_info->aov_buf_sz =
			aov_log_buf_sz;
	}

	HWLOGR_DBG("set st_logger_init_info iova = 0x%x, iova_h = 0x%x, lbc_sz = 0x%x\n",
		st_logger_init_info->iova,
		st_logger_init_info->iova_h,
		st_logger_init_info->lbc_sz);

	if (aov_hw_log_buf_addr)
		HWLOGR_DBG("set st_logger_init_info aov_iova = 0x%x, aov_iova_h = 0x%x\n",
			st_logger_init_info->aov_iova,
			st_logger_init_info->aov_iova_h);

	return 0;
}
EXPORT_SYMBOL(hw_logger_config_init);

int hw_logger_ipi_init(struct mtk_apu *apu)
{
	int ret = 0;

	/* do nothing if not probed */
	if (!apu_logtop)
		return 0;

	g_apu = apu;

	ret = apu_ipi_register(g_apu, APU_IPI_LOG_LEVEL, NULL,
			apu_hw_log_level_ipi_handler, NULL);
	if (ret)
		HWLOGR_ERR("Fail in hw_log_level_ipi_init\n");
	return 0;
}

void hw_logger_ipi_remove(struct mtk_apu *apu)
{
	apu_ipi_unregister(apu, APU_IPI_LOG_LEVEL);
}

static int __apu_logtop_copy_buf(unsigned int r_ofs, unsigned int w_ofs, unsigned int pwr_status)
{
	unsigned long flags;
	unsigned int t_size, r_size;
	unsigned int log_w_ofs, log_ov_flg;
	int ret = 0;
	static bool err_log;

	if (!apu_logtop || !hw_log_buf || !local_log_buf)
		return 0;

	if (w_ofs == r_ofs)
		goto out;

	t_size = get_t_size();

	HWLOGR_DBG("w_ofs = 0x%x, r_ofs = 0x%x, t_size = 0x%x\n", w_ofs, r_ofs, t_size);

	/* check copy size */
	if (w_ofs > r_ofs)
		r_size = w_ofs - r_ofs;
	else
		r_size = t_size - r_ofs + w_ofs;

	/* sanity check */
	if (r_size >= t_size)
		r_size = t_size;

	ret = r_size;

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	log_w_ofs = __loc_log_w_ofs;
	log_ov_flg = __loc_log_ov_flg;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	if (w_ofs + HWLOG_LINE_MAX_LENS > t_size ||
		r_ofs + HWLOG_LINE_MAX_LENS > t_size || t_size == 0 || t_size > HWLOGR_LOG_SIZE) {
		/* only print the first error */
		if (!err_log)
			HWLOGR_WARN("w_ofs = 0x%x, r_ofs = 0x%x, t_size = 0x%x\n",
				w_ofs, r_ofs, t_size);
		err_log = true;
		return 0;
	}

	if (log_w_ofs + HWLOG_LINE_MAX_LENS > LOCAL_LOG_SIZE) {
		/* only print the first error */
		if (!err_log)
			HWLOGR_WARN("log_w_ofs = 0x%x\n", log_w_ofs);
		err_log = true;
		return 0;
	}

	/* print when back to normal */
	if (err_log) {
		HWLOGR_INFO("[ok] w_ofs = 0x%x, r_ofs = 0x%x, t_size = 0x%x\n",
			w_ofs, r_ofs, t_size);
		err_log = false;
	}

	/* invalidate hw logger buf */
	hw_logger_buf_invalidate();

	while (r_size > 0) {
#ifdef HW_LOG_DEBUG
		HWLOGR_DBG("(%x)(%03x):(%d) %s",
			r_ofs, r_size, (int)strlen(hw_log_buf + r_ofs), hw_log_buf + r_ofs);
		if (DBG_ON())
			hwlogr_hex_dump(" ", hw_log_buf + r_ofs, HWLOG_LINE_MAX_LENS);
#endif
		/* copy hw logger buffer to local buffer */
		memcpy(local_log_buf + log_w_ofs,
			hw_log_buf + r_ofs, HWLOG_LINE_MAX_LENS);

		/* move local write pointer */
		log_w_ofs = (log_w_ofs + HWLOG_LINE_MAX_LENS) % LOCAL_LOG_SIZE;

		/* move hw logger read pointer */
		r_ofs = (r_ofs + HWLOG_LINE_MAX_LENS) % t_size;
		r_size -= HWLOG_LINE_MAX_LENS;

		__loc_log_sz += HWLOG_LINE_MAX_LENS;
		if (__loc_log_sz >= LOCAL_LOG_SIZE)
			log_ov_flg = 1;
	};

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	__loc_log_w_ofs = log_w_ofs;
	__loc_log_ov_flg = log_ov_flg;
	g_log_l.not_rd_sz += ret;
	g_log_lm.not_rd_sz += ret;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	store_r_ofs(pwr_status, r_ofs);
out:
	return ret;
}


static int apu_logtop_copy_buf(void)
{
	int ret = 0;
	static bool lock_fail;
	unsigned int r_ofs, w_ofs, pwr_status;
	unsigned long long r_ptr = 0, w_ptr = 0;

	if (!mutex_trylock(&hw_logger_mutex)) {
		if (!lock_fail) {
			HWLOGR_DBG("lock fail, skip copy buf\n");
			lock_fail = true;
		}
		return ret;
	} else if (lock_fail) {
		HWLOGR_DBG("lock success\n");
		lock_fail = false;
	}

	ret = get_r_w_ptr(&r_ptr, &w_ptr);

	if (ret != 0) {
		HWLOGR_ERR("skip copy buf, get_r_w_ptr() fail, ret = %d\n", ret);
		goto out;
	}

	r_ofs = r_ptr - get_st_addr();
	w_ofs = w_ptr - get_st_addr();
	HWLOGR_DBG("w_ofs = 0x%x, r_ofs = 0x%x, st_addr = 0x%llx, t_size = 0x%llx\n",
		w_ofs, r_ofs, get_st_addr(), get_t_size());

	if (enable_interrupt) {
		/* uP power done, get r/w_ptr from mbox */
		pwr_status = GET_MASK_BITS(APU_RPC_PWR_STATUS);
		HWLOGR_DBG("APU_RPC_PWR_STATUS : 0x%x\n", pwr_status);

		if (pwr_status == 0 || r_ptr == 0 || w_ptr == 0) {
			r_ofs = ioread32(LOG_R_OFS_MBOX);
			w_ofs = ioread32(LOG_W_OFS_MBOX);
			HWLOGR_DBG("mbox w_ofs = 0x%x, r_ofs = 0x%x\n", w_ofs, r_ofs);
		}
	} else {
		/* If interrupt disable, no need to copy while uP power down */
		if (atomic_read(&apu_toplog_deep_idle) || r_ptr == 0 || w_ptr == 0) {
			HWLOGR_INFO("in deep idle skip copy");
			goto out;
		}
		pwr_status = 1;
	}

	ret = __apu_logtop_copy_buf(r_ofs, w_ofs, pwr_status);
out:
	mutex_unlock(&hw_logger_mutex);

	return ret;
}

static void apu_logtop_copy_buf_wq(struct work_struct *work)
{
	unsigned int r_ofs, w_ofs;
	unsigned long long r_ptr = 0;

	HWLOGR_DBG("in\n");

	mutex_lock(&hw_logger_mutex);

	get_r_w_ptr(&r_ptr, NULL);

	r_ofs = r_ptr - get_st_addr();
	w_ofs = ioread32(LOG_W_OFS_MBOX);
	HWLOGR_DBG("w_ofs = 0x%x, r_ofs = 0x%x\n", w_ofs, r_ofs);

	__apu_logtop_copy_buf(r_ofs, w_ofs, 0);

	/* force set 0 here to prevent racing between power up */
	set_r_ptr(0);

	mutex_unlock(&hw_logger_mutex);

	HWLOGR_DBG("out\n");
}

static void apu_logtop_mtklog_wq(struct work_struct *work)
{
	HWLOGR_DBG("in\n");

	wake_up_all(&apusys_hwlog_wait);
}

int hw_logger_copy_buf(void)
{
	int ret = 0;

	HWLOGR_DBG("in\n");

	if (!apu_logtop || !g_apu)
		return 0;

	if (enable_interrupt || !atomic_read(&apu_toplog_deep_idle))
		ret = apu_logtop_copy_buf();

	/* return copied size */
	return ret;
}

int hw_logger_power_on(void)
{
	HWLOGR_DBG("in power on\n");

	wake_up_all(&apusys_hwlog_wait);

	return 0;
}

int hw_logger_deep_idle_enter_pre(void)
{
	HWLOGR_DBG("in deep idel pre\n");

	if (!apu_logtop || enable_interrupt)
		return 0;

	atomic_set(&apu_toplog_deep_idle, 1);
	return 0;
}



int hw_logger_deep_idle_enter_post(void)
{
	HWLOGR_DBG("in deep idle post\n");

	if (!apu_logtop || enable_interrupt)
		return 0;

	queue_work(apusys_hwlog_wq, &apusys_hwlog_task);

	return 0;
}

int hw_logger_deep_idle_leave(void)
{

	HWLOGR_DBG("in deep idle leave\n");

	if (!apu_logtop || enable_interrupt)
		return 0;

	/* reset read pointer */
	set_r_ptr(get_st_addr());

	atomic_set(&apu_toplog_deep_idle, 0);

	return 0;
}

static irqreturn_t apu_logtop_irq_handler(int irq, void *priv)
{
	bool lbc_full_flg, ovwrite_flg;
	unsigned int ctrl_flag = 0;
	unsigned long long irq_start_time;

	irq_start_time = sched_clock();

	apu_logtop_copy_buf();

	if (access_rcx_in_atf) {
		w1c32_atf(APU_LOGTOP_CON_FLAG_ADDR, &ctrl_flag);
	} else {
		ctrl_flag = ioread32(APU_LOGTOP_CON_FLAG_ADDR);
		iowrite32(ctrl_flag, APU_LOGTOP_CON_FLAG_ADDR);
	}

	HWLOGR_DBG("w1c apu_logtop_irq_handler = 0x%x\n",
		(ctrl_flag & APU_LOGTOP_CON_FLAG_MASK) >> APU_LOGTOP_CON_FLAG_SHIFT);

	lbc_full_flg = !!(ctrl_flag & LBC_FULL_FLAG);
	ovwrite_flg = !!(ctrl_flag & OVWRITE_FLAG);

	if (!lbc_full_flg && !ovwrite_flg)
		HWLOGR_INFO("intr status = 0x%x should not occur\n", ctrl_flag);

	wake_up_all(&apusys_hwlog_wait);
	HWLOGR_DBG("irq_time = %lld ns\n", sched_clock() - irq_start_time);

	return IRQ_HANDLED;
}

static int apusys_debug_dump(struct seq_file *s, void *unused)
{
	DBG_HWLOG_CON(s, "%d\n", g_hw_logger_log_lv);
	return 0;
}

static int hw_logger_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_debug_dump, inode->i_private);
}

static ssize_t show_debuglv(struct file *filp, char __user *buffer,
					size_t count, loff_t *ppos)
{
	char buf[512];
	unsigned int len = 0;
	unsigned long flags;

	len += scnprintf(buf + len, sizeof(buf) - len,
			"uP_hw_logger_log_lv = 0x%x\n",
			hw_ipi_loglv_data.level);

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	len += scnprintf(buf + len, sizeof(buf) - len,
		"__loc_log_w_ofs = %d,__loc_log_ov_flg = %d __loc_log_sz = %d\n",
		__loc_log_w_ofs, __loc_log_ov_flg, __loc_log_sz);
	len += scnprintf(buf + len, sizeof(buf) - len,
		"g_log: w_ptr = %d, r_ptr = %d, ov_flg = %d\n",
		g_log.w_ptr, g_log.r_ptr, g_log.ov_flg);
	len += scnprintf(buf + len, sizeof(buf) - len,
		"g_log_l: w_ptr = %d, r_ptr = %d, ov_flg = %d, not_rd_sz = %d\n",
		g_log_l.w_ptr, g_log_l.r_ptr,
		g_log_l.ov_flg, g_log_l.not_rd_sz);
	len += scnprintf(buf + len, sizeof(buf) - len,
		"g_log_lm: w_ptr = %d, r_ptr = %d, ov_flg = %d, not_rd_sz = %d\n",
		g_log_lm.w_ptr, g_log_lm.r_ptr,
		g_log_lm.ov_flg, g_log_lm.not_rd_sz);
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);
	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static ssize_t set_debuglv(struct file *flip,
						   const char __user *buffer,
						   size_t count, loff_t *f_pos)
{
	char tmp[16] = {0};
	int ret;
	unsigned int input = 0;

	if (count + 1 >= 16)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		HWLOGR_ERR("copy_from_user failed (%d)\n", ret);
		goto out;
	}

	tmp[count] = '\0';
	ret = kstrtouint(tmp, 16, &input);
	if (ret) {
		HWLOGR_ERR("kstrtouint failed (%d)\n", ret);
		goto out;
	}

	HWLOGR_INFO("set uP debug lv = 0x%x\n", input);

	hw_ipi_loglv_data.level = input;

	ret = apu_power_on_off(g_apu->pdev, APU_IPI_LOG_LEVEL, 1, 0);
	if (ret && ret != -EOPNOTSUPP) {
		HWLOGR_ERR("apu_power_on_off fail(%d)\n", ret);
		goto out;
	}

	ret = apu_ipi_send(g_apu, APU_IPI_LOG_LEVEL,
			&hw_ipi_loglv_data, sizeof(hw_ipi_loglv_data), 1000);
	if (ret)
		HWLOGR_ERR("Failed for hw_logger log level send.\n");
out:

	return count;
}

static const struct proc_ops apusys_debug_fops = {
	.proc_open = hw_logger_open,
	.proc_read = show_debuglv,
	.proc_write = set_debuglv,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t show_debugAttr(struct file *filp, char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[512];
	unsigned int len = 0;
	unsigned long flags;

	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"hw_log_buf = 0x%llx\n",
		(unsigned long long)hw_log_buf);

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"__loc_log_w_ofs = %d\n",
		__loc_log_w_ofs);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"__loc_log_ov_flg = %d\n",
		__loc_log_ov_flg);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"__loc_log_sz = %d\n",
		__loc_log_sz);

	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log.w_ptr = %d\n",
		g_log.w_ptr);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log.r_ptr = %d\n",
		g_log.r_ptr);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log.ov_flg = %d\n",
		g_log.ov_flg);

	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_l.w_ptr = %d\n",
		g_log_l.w_ptr);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_l.r_ptr = %d\n",
		g_log_l.r_ptr);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_l.ov_flg = %d\n",
		g_log_l.ov_flg);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_l.not_rd_sz = %d\n",
		g_log_l.not_rd_sz);

	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_lm.w_ptr = %d\n",
		g_log_lm.w_ptr);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_lm.r_ptr = %d\n",
		g_log_lm.r_ptr);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_lm.ov_flg = %d\n",
		g_log_lm.ov_flg);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_lm.not_rd_sz = %d\n",
		g_log_lm.not_rd_sz);

	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_hw_logger_log_lv = %d:\n",
		g_hw_logger_log_lv);

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static ssize_t set_debugAttr(struct file *flip,
							const char __user *buffer,
							size_t count, loff_t *f_pos)
{
	char tmp[16] = {0};
	int ret;
	unsigned int input = 0;

	if (count + 1 >= 16)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		HWLOGR_ERR("copy_from_user failed (%d)\n", ret);
		goto out;
	}

	tmp[count] = '\0';
	ret = kstrtouint(tmp, 10, &input);
	if (ret) {
		HWLOGR_ERR("kstrtouint failed (%d)\n", ret);
		goto out;
	}

	if (input <= DBG_LOG_DEBUG)
		g_hw_logger_log_lv = input;
out:

	return count;
}

static const struct proc_ops hw_logger_attr_fops = {
	.proc_open = hw_logger_open,
	.proc_read = show_debugAttr,
	.proc_write = set_debugAttr,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/**
 * seq_start() takes a position as an argument and returns an iterator which
 * will start reading at that position.
 * start->show->next->show...->next->show->next->stop->start->stop
 */
static void *seq_start(struct seq_file *s, loff_t *pos)
{
	struct hw_logger_seq_data *pSData;
	unsigned int w_ptr, r_ptr, ov_flg = 0;
	unsigned long flags;

	HWLOGR_DBG("in");

	hw_logger_copy_buf();

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	if (g_log.r_ptr == U32_MAX) {
		w_ptr = __loc_log_w_ofs;
		ov_flg = __loc_log_ov_flg;
		g_log.w_ptr = w_ptr;
		g_log.ov_flg = ov_flg;
		if (ov_flg)
			/* avoid stuck at while loop move one forward */
			r_ptr = (w_ptr + HWLOG_LINE_MAX_LENS) % HWLOGR_LOG_SIZE;
		else
			r_ptr = 0;
		g_log.r_ptr = r_ptr;
	} else {
		w_ptr = g_log.w_ptr;
		r_ptr = g_log.r_ptr;
	}
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	HWLOGR_DBG("w_ptr = %d, r_ptr = %d, ov_flg = %d, *pos = %d\n",
		w_ptr, r_ptr, ov_flg, (unsigned int)*pos);

	if (w_ptr == r_ptr) {
		if (!APUSYS_RV_DEBUG_INFO_DUMP || s->size == PAGE_SIZE) {
			spin_lock_irqsave(&hw_logger_spinlock, flags);
			g_log.r_ptr = U32_MAX;
			spin_unlock_irqrestore(&hw_logger_spinlock, flags);
		} else {
			s->size = PAGE_SIZE;
		}
		return NULL;
	}

	pSData = kzalloc(sizeof(struct hw_logger_seq_data),
		GFP_KERNEL);
	if (!pSData)
		return NULL;

	pSData->w_ptr = w_ptr;
	pSData->ov_flg = ov_flg;
	if (ov_flg == 0)
		pSData->r_ptr = r_ptr;
	else
		pSData->r_ptr = w_ptr;

	return pSData;
}

/**
 * seq_start() takes a position as an argument and returns an iterator which
 * will start reading at that position.
 */
static void *seq_startl(struct seq_file *s, loff_t *pos)
{
	uint32_t w_ptr, r_ptr, ov_flg;
	struct hw_logger_seq_data *pSData, *gpSData;
	unsigned long flags;

	HWLOGR_DBG("in");

	pSData = kzalloc(sizeof(struct hw_logger_seq_data),
		GFP_KERNEL);
	if (!pSData)
		return NULL;

	/* force flush last hw buffer */
	hw_logger_copy_buf();

	if (s->file &&
		s->file->f_flags & O_NONBLOCK) {
		pSData->nonblock = true;
		gpSData = &g_log_lm;
	} else {
		pSData->nonblock = false;
		gpSData = &g_log_l;
	}

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	w_ptr = __loc_log_w_ofs;
	gpSData->w_ptr = w_ptr;
	if (gpSData->r_ptr == U32_MAX) {
		ov_flg = __loc_log_ov_flg;
		gpSData->ov_flg = ov_flg;
		if (ov_flg)
			/* avoid stuck at while loop move one forward */
			r_ptr = (w_ptr + HWLOG_LINE_MAX_LENS) % HWLOGR_LOG_SIZE;
		else
			r_ptr = 0;
		gpSData->r_ptr = r_ptr;
	} else {
		r_ptr = gpSData->r_ptr;
		/* check if overflow occurs */
		if (gpSData->not_rd_sz >= LOCAL_LOG_SIZE - HWLOG_LINE_MAX_LENS) {
			r_ptr = (w_ptr + HWLOG_LINE_MAX_LENS) % HWLOGR_LOG_SIZE;
			gpSData->not_rd_sz = LOCAL_LOG_SIZE - HWLOG_LINE_MAX_LENS;
		}
	}
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	/* for ctrl-c to force exit the loop */
	while (!signal_pending(current) && w_ptr == r_ptr) {
		if (w_ptr != r_ptr)
			break;
		 /* return if file is open as non blocking mode */
		else if (pSData->nonblock) {
			/* free here and return NULL, seq will stop */
			kfree(pSData);
			HWLOGR_DBG("END\n");
			return NULL;
		}
		usleep_range(10000, 15000);

		/* force flush last hw buffer */
		hw_logger_copy_buf();

		spin_lock_irqsave(&hw_logger_spinlock, flags);
		w_ptr = __loc_log_w_ofs;
		ov_flg = __loc_log_ov_flg;
		spin_unlock_irqrestore(&hw_logger_spinlock, flags);
	};

	HWLOGR_DBG("w_ptr = %d, r_ptr = %d, ov_flg = %d, *pos = %d\n",
		w_ptr, r_ptr, ov_flg, (unsigned int)*pos);

	pSData->w_ptr = w_ptr;
	pSData->r_ptr = r_ptr;
	pSData->ov_flg = ov_flg;

	if (signal_pending(current)) {
		HWLOGR_DBG("BREAK w_ptr = %d, r_ptr = %d, ov_flg = %d\n",
			w_ptr, r_ptr, ov_flg);
		spin_lock_irqsave(&hw_logger_spinlock, flags);
		gpSData->r_ptr = U32_MAX;
		spin_unlock_irqrestore(&hw_logger_spinlock, flags);
		/* free here and return NULL, seq will stop */
		kfree(pSData);
		return NULL;
	}

	return pSData;
}

/**
 * move the iterator forward to the next position in the sequence
 */
static void *seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct hw_logger_seq_data *pSData = v;
	unsigned long flags;
	struct mtk_apu_hw_ops *hw_ops;

	/*
	HWLOGR_DBG(
		"w_ptr = %d, r_ptr = %d, ov_flg = %d, *pos = %d\n",
		pSData->w_ptr, pSData->r_ptr,
		pSData->ov_flg, (unsigned int)*pos);
	*/

	if (APUSYS_RV_DEBUG_INFO_DUMP && pSData->is_debug_info_dump_finished) {
		/* just prevent warning */
		*pos = *pos + 1;
		HWLOGR_DBG("%s: is_debug_info_dump = %d, is_debug_info_dump_finished = %d\n",
			__func__, pSData->is_debug_info_dump, pSData->is_debug_info_dump_finished);
		goto out;
	}

	pSData->r_ptr = (pSData->r_ptr + HWLOG_LINE_MAX_LENS) % LOCAL_LOG_SIZE;
	spin_lock_irqsave(&hw_logger_spinlock, flags);
	g_log.r_ptr = pSData->r_ptr;
	/* just prevent warning */
	*pos = pSData->r_ptr + 1;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	if (pSData->r_ptr != pSData->w_ptr)
		return pSData;

	HWLOGR_DBG(
		"END g_log.w_ptr = %d, g_log.r_ptr = %d, g_log_l.ov_flg = %d\n",
		g_log.w_ptr, g_log.r_ptr,
		g_log_l.ov_flg);

	if (APUSYS_RV_DEBUG_INFO_DUMP && g_apu) {
		hw_ops = &g_apu->platdata->ops;
		if (hw_ops->debug_info_dump && pSData->is_debug_info_dump == 0) {
			pSData->is_debug_info_dump = 1;
			return pSData;
		}
	}

out:
	kfree(pSData);
	return NULL;
}

static void *seq_nextl(struct seq_file *s, void *v, loff_t *pos)
{
	struct hw_logger_seq_data *pSData = v, *gpSData;
	unsigned long flags;

	if (pSData->nonblock)
		gpSData = &g_log_lm;
	else
		gpSData = &g_log_l;

	HWLOGR_DBG(
		"w_ptr = %d, r_ptr = %d, ov_flg = %d, nonblock = %d, *pos = %d\n",
		pSData->w_ptr, pSData->r_ptr,
		pSData->ov_flg, pSData->nonblock, (unsigned int)*pos);

	pSData->r_ptr = (pSData->r_ptr + HWLOG_LINE_MAX_LENS) % LOCAL_LOG_SIZE;
	spin_lock_irqsave(&hw_logger_spinlock, flags);
	gpSData->r_ptr = pSData->r_ptr;
	gpSData->not_rd_sz -= HWLOG_LINE_MAX_LENS;
	/* just prevent warning */
	*pos = pSData->r_ptr + 1;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	if (pSData->r_ptr != pSData->w_ptr)
		return pSData;

	HWLOGR_DBG(
		"END gpSData w_ptr = %d, r_ptr = %d, nonblock = %d, ov_flg = %d\n",
		gpSData->w_ptr, gpSData->r_ptr, gpSData->nonblock,
		gpSData->ov_flg);

	kfree(pSData);
	return NULL;
}

/**
 * stop() is called when iteration is complete (clean up)
 */
static void seq_stop(struct seq_file *s, void *v)
{
	HWLOGR_DBG("in");
	kfree(v);
}

/**
 * success return 0, otherwise return error code
 */
static int seq_show(struct seq_file *s, void *v)
{
	struct hw_logger_seq_data *pSData = v;
	static unsigned int prevIsBinary;
	struct mtk_apu_hw_ops *hw_ops;
	char *buf;
	size_t offs = s->count;

#ifdef HW_LOG_DEBUG
	HWLOGR_DBG("(%04d)(%d) %s", pSData->r_ptr,
		(int)strlen(local_log_buf + pSData->r_ptr), local_log_buf + pSData->r_ptr);
#endif

	if (APUSYS_RV_DEBUG_INFO_DUMP && pSData->is_debug_info_dump) {
		if (g_apu) {
			hw_ops = &g_apu->platdata->ops;
			if (hw_ops->debug_info_dump) {
				HWLOGR_DBG(
					"%s: is_debug_info_dump = %d, is_debug_info_dump_finished = %d\n",
					__func__, pSData->is_debug_info_dump,
					pSData->is_debug_info_dump_finished);
				hw_ops->debug_info_dump(g_apu, s);
			}
		}
		if (seq_has_overflowed(s)) {
			/* pr_info("%s: seq_has_overflowed\n", __func__); */
			/* need to allocate more buffer */
			buf = kvmalloc(s->size <<= 1, GFP_KERNEL_ACCOUNT);
			if (!buf)
				return -ENOMEM;
			memcpy(buf, s->buf, offs);

			kvfree(s->buf);
			/* restore original count before calling debug_info_dump */
			s->count = offs;
			s->buf = buf;

			if (g_apu) {
				hw_ops = &g_apu->platdata->ops;
				if (hw_ops->debug_info_dump) {
					HWLOGR_DBG(
						"%s: is_debug_info_dump = %d, is_debug_info_dump_finished = %d\n",
						__func__, pSData->is_debug_info_dump,
						pSData->is_debug_info_dump_finished);
					hw_ops->debug_info_dump(g_apu, s);
				}
			}

		}

		pSData->is_debug_info_dump_finished = 1;
		return 0;
	}

	if ((local_log_buf[pSData->r_ptr] == 0xA5) &&
		(local_log_buf[pSData->r_ptr+1] == 0xA5)) {
		prevIsBinary = 1;
		seq_write(s, local_log_buf + pSData->r_ptr, HWLOG_LINE_MAX_LENS);
	} else {
		if (prevIsBinary)
			seq_puts(s, "\n");
		prevIsBinary = 0;
		/*
		 * force null-terminated
		 * prevent overflow from printing non-string content
		 */
		*(local_log_buf + pSData->r_ptr + HWLOG_LINE_MAX_LENS - 1) = '\0';
		seq_printf(s, "%s",
			local_log_buf + pSData->r_ptr);
	}

	return 0;
}

static unsigned int seq_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	HWLOGR_DBG("in");

	if (!(file->f_mode & FMODE_READ))
		return ret;

	/* force flush last hw buffer */
	hw_logger_copy_buf();

	poll_wait(file, &apusys_hwlog_wait, wait);

	if (g_log_lm.r_ptr != __loc_log_w_ofs)
		ret = POLLIN | POLLRDNORM;

	if (!enable_interrupt) {
		if (!pm_runtime_suspended(g_apu->dev)) {
			cancel_delayed_work_sync(&apusys_mtklog_task);
			queue_delayed_work(apusys_hwlog_wq,
				&apusys_mtklog_task,
				msecs_to_jiffies(MTKLOG_WAKEUP_MS));
		}
	}

	HWLOGR_DBG("out (%d)\n", ret);

	return ret;
}

static const struct seq_operations seq_ops = {
	.start = seq_start,
	.next  = seq_next,
	.stop  = seq_stop,
	.show  = seq_show
};

static const struct seq_operations seq_ops_lock = {
	.start = seq_startl,
	.next  = seq_nextl,
	.stop  = seq_stop,
	.show  = seq_show
};

static int debug_sqopen_lock(struct inode *inode, struct file *file)
{
	return seq_open(file, &seq_ops_lock);
}

static int debug_sqopen(struct inode *inode, struct file *file)
{
	return seq_open(file, &seq_ops);
}

static void clear_local_log_buf(void)
{
	unsigned long flags;

	HWLOGR_INFO("in\n");

	/* Before reset local buffer, copy (flush) latest log from raw buffer */
	hw_logger_copy_buf();

	mutex_lock(&hw_logger_mutex);

	memset(local_log_buf, 0, LOCAL_LOG_SIZE);

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	__loc_log_w_ofs = 0;
	__loc_log_ov_flg = 0;
	__loc_log_sz = 0;

	g_log.w_ptr = 0;
	g_log.r_ptr = U32_MAX;
	g_log.ov_flg = 0;
	g_log.not_rd_sz = 0;
	g_log_l.w_ptr = 0;
	g_log_l.r_ptr = U32_MAX;
	g_log_l.ov_flg = 0;
	g_log_l.not_rd_sz = 0;

	g_log_lm.not_rd_sz = 0;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	mutex_unlock(&hw_logger_mutex);
}

static ssize_t debug_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char buf[PROC_WRITE_BUFSIZE];

	if (*pos > 0 || count > PROC_WRITE_BUFSIZE)
		return -EFAULT;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[PROC_WRITE_BUFSIZE - 1] = '\0';

	HWLOGR_INFO("cmd = %s\n", buf);

	if (!strncmp(buf, CLEAR_LOG_CMD, strlen(CLEAR_LOG_CMD)))
		clear_local_log_buf();

	return count;
}

static const struct proc_ops hw_loggerSeqLog_ops = {
	.proc_open    = debug_sqopen,
	.proc_read    = seq_read,
	.proc_write   = debug_write,
	.proc_lseek  = seq_lseek,
	.proc_release = seq_release
};

static const struct proc_ops hw_loggerSeqLogL_ops = {
	.proc_open    = debug_sqopen_lock,
	.proc_poll    = seq_poll,
	.proc_read    = seq_read,
	.proc_write   = debug_write,
	.proc_lseek  = seq_lseek,
	.proc_release = seq_release
};

static int aov_seq_show(struct seq_file *s, void *v)
{
	if (aov_hw_log_buf) {
		dma_sync_single_for_cpu(
			hw_logger_dev, aov_hw_log_buf_addr,
			aov_log_buf_sz, DMA_FROM_DEVICE);
		seq_write(s, aov_hw_log_buf, HWLOGR_LOG_SIZE);
	}

	return 0;
}

static int aov_log_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, aov_seq_show, NULL);
}

static const struct proc_ops aov_log_ops = {
	.proc_open		= aov_log_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

#ifdef HW_LOG_DUMP_RAW_BUF
static int raw_seq_show(struct seq_file *s, void *v)
{
	hw_logger_buf_invalidate();
	seq_write(s, hw_log_buf, HWLOGR_LOG_SIZE);
	return 0;
}

static int raw_log_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, raw_seq_show, NULL);
}

static const struct proc_ops raw_log_ops = {
	.proc_open		= raw_log_sqopen,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};

static int aov_tcm_seq_show(struct seq_file *s, void *v)
{
	seq_write(s, aov_tcm_buf, aov_log_buf_sz);
	return 0;
}

static int aov_tcm_log_sqopen(struct inode *inode, struct file *file)
{
	return single_open(file, aov_tcm_seq_show, NULL);
}

static ssize_t aov_tcm_seq_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char tmp[16] = {0};
	int ret;
	unsigned int input = 0;

	if (count + 1 >= 16)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		HWLOGR_WARN("copy_from_user failed (%d)\n", ret);
		goto out;
	}

	tmp[count] = '\0';
	ret = kstrtouint(tmp, 16, &input);
	if (ret) {
		HWLOGR_WARN("kstrtouint failed (%d)\n", ret);
		goto out;
	}

	HWLOGR_INFO("dump ops (0x%x)\n", input);

	if (input & (0x1)) {
		if (hw_logger_dump_tcm_log() == 0) {
			HWLOGR_INFO("dump tcm log success\n");
			goto out;
		} else {
			HWLOGR_WARN("dump tcm log smc call fail\n");
			goto out;
		}
	}
out:
	return count;
}

static const struct proc_ops aov_tcm_log_ops = {
	.proc_open		= aov_tcm_log_sqopen,
	.proc_write     = aov_tcm_seq_write,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release
};
#endif

#ifdef HW_LOG_SYSFS_BIN
static ssize_t apusys_log_dump(struct file *filep,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;
	uint32_t w_ptr, r_ptr, ov_flg, pr_ptr;
	uint32_t print_sz;
	unsigned long flags;

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	w_ptr = __loc_log_w_ofs;
	ov_flg = __loc_log_ov_flg;
	if (g_dump_log_r_ofs == U32_MAX) {
		if (ov_flg)
			r_ptr = (w_ptr + HWLOG_LINE_MAX_LENS) % LOCAL_LOG_SIZE;
		else
			r_ptr = 0;
	} else {
		r_ptr = g_dump_log_r_ofs;
	}

	pr_ptr = r_ptr;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	if (w_ptr == r_ptr) {
		g_dump_log_r_ofs = U32_MAX;
		HWLOGR_DBG("end\n");
		return length;
	}

	do {
		print_sz = strlen(local_log_buf + pr_ptr);
		if ((length + print_sz + 1) <= size) {
			scnprintf(buf + length, print_sz, "%s", local_log_buf + pr_ptr);
			/* replace trailing null character with new line
			 * for log readability
			 */
			buf[length + print_sz] = '\n';
		} else
			break;
		length += (print_sz + 1); /* include '\n' */
		pr_ptr = (pr_ptr + HWLOG_LINE_MAX_LENS) % LOCAL_LOG_SIZE;
	} while (pr_ptr != w_ptr);

	g_dump_log_r_ofs = pr_ptr;

	HWLOGR_DBG("length = %d, g_dump_log_r_ofs = %d\n", length, g_dump_log_r_ofs);

	return length;
}

struct bin_attribute bin_attr_apusys_hwlog = {
	.attr = {
		.name = "apusys_hwlog.txt",
		.mode = 0444,
	},
	.size = 0,
	.read = apusys_log_dump,
};

static int hw_logger_create_sysfs(struct device *dev)
{
	int ret = 0;

	/* create /sys/kernel/apusys_logger */
	root_dir = kobject_create_and_add("apusys_hwlogger",
		kernel_kobj);
	if (!root_dir) {
		HWLOGR_ERR("kobject_create_and_add fail (%d)\n",
			ret);
		return -EINVAL;
	}

	ret = sysfs_create_bin_file(root_dir,
		&bin_attr_apusys_hwlog);
	if (ret)
		HWLOGR_ERR("sysfs create fail (%d)\n",
			ret);

	return ret;
}

static void hw_logger_remove_sysfs(struct device *dev)
{
	sysfs_remove_bin_file(root_dir, &bin_attr_apusys_hwlog);
}
#endif

static void hw_logger_remove_procfs(struct device *dev)
{
	remove_proc_entry("log", log_root);
	remove_proc_entry("seq_log", log_root);
	remove_proc_entry("seq_logl", log_root);
	remove_proc_entry("aov_log", log_root);
#ifdef HW_LOG_DUMP_RAW_BUF
	remove_proc_entry("raw_log", log_root);
	remove_proc_entry("aov_tcm_log", log_root);
#endif
	remove_proc_entry("attr", log_root);
	remove_proc_entry(APUSYS_HWLOGR_DIR, NULL);
}

static int hw_logger_create_procfs(struct device *dev)
{
	int ret = 0;

	log_root = proc_mkdir(APUSYS_HWLOGR_DIR, NULL);
	ret = IS_ERR_OR_NULL(log_root);
	if (ret) {
		HWLOGR_ERR("create dir fail (%d)\n", ret);
		goto out;
	}

	/* create device table info */
	log_devinfo = proc_create("log", 0440,
		log_root, &apusys_debug_fops);
	ret = IS_ERR_OR_NULL(log_devinfo);
	if (ret) {
		HWLOGR_ERR("create devinfo fail (%d)\n", ret);
		goto out;
	}

	log_seqlog = proc_create("seq_log", 0440,
		log_root, &hw_loggerSeqLog_ops);
	ret = IS_ERR_OR_NULL(log_seqlog);
	if (ret) {
		HWLOGR_ERR("create seqlog fail (%d)\n", ret);
		goto out;
	}

	log_seqlogL = proc_create("seq_logl", 0440,
		log_root, &hw_loggerSeqLogL_ops);
	ret = IS_ERR_OR_NULL(log_seqlogL);
	if (ret) {
		HWLOGR_ERR("create seqlogL fail (%d)\n", ret);
		goto out;
	}

	log_aov_log = proc_create("aov_log", 0440,
		log_root, &aov_log_ops);
	ret = IS_ERR_OR_NULL(log_aov_log);
	if (ret) {
		HWLOGR_ERR("create aov_log fail (%d)\n", ret);
		goto out;
	}

#ifdef HW_LOG_DUMP_RAW_BUF
	log_raw_log = proc_create("raw_log", 0440,
		log_root, &raw_log_ops);
	ret = IS_ERR_OR_NULL(log_raw_log);
	if (ret) {
		HWLOGR_ERR("create raw_log fail (%d)\n", ret);
		goto out;
	}

	log_aov_tcm_log = proc_create("aov_tcm_log", 0440,
		log_root, &aov_tcm_log_ops);
	ret = IS_ERR_OR_NULL(log_aov_tcm_log);
	if (ret) {
		HWLOGR_ERR("create aov_tcm_log fail (%d)\n", ret);
		goto out;
	}
#endif

	log_devattr = proc_create("attr", 0440,
		log_root, &hw_logger_attr_fops);

	ret = IS_ERR_OR_NULL(log_devattr);
	if (ret) {
		HWLOGR_ERR("create attr fail (%d)\n", ret);
		goto out;
	}

	return 0;

out:
	hw_logger_remove_procfs(dev);
	return ret;
}

static int hw_logger_memmap(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_logtop");
	if (res == NULL) {
		HWLOGR_ERR("apu_logtop get resource fail\n");
		ret = -ENODEV;
		goto out;
	}

	apu_logtop = ioremap(res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu_logtop)) {
		HWLOGR_ERR("apu_logtop remap base fail\n");
		ret = -ENOMEM;
		goto out;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_mbox");
	if (res == NULL) {
		HWLOGR_ERR("apu_mbox get resource fail\n");
		goto out;
	}

	apu_mbox = ioremap(res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu_mbox)) {
		HWLOGR_ERR("apu_mbox remap base fail\n");
		goto out;
	}

	if (enable_interrupt) {
		apu_rpc = ioremap(APU_RPC_PHYS_BASE, APU_RPC_PHYS_SIZE);
		if (IS_ERR((void const *)apu_rpc)) {
			HWLOGR_ERR("apu_rpc remap base fail\n");
			goto out;
		}
	}
out:
	return ret;
}

static int hw_logger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;

	HWLOGR_INFO("start\n");

	hw_logger_dev = dev;

	init_waitqueue_head(&apusys_hwlog_wait);

	/* SW workaround for MDLA reset glitch on mt6897, mt6989 */
	if (of_property_read_u32(np, "access-rcx-in-atf", &access_rcx_in_atf)) {
		HWLOGR_INFO("get access-rcx-in-atf failed\n");
		access_rcx_in_atf = 0;
	}
	HWLOGR_INFO("access_rcx_in_atf : %u\n", access_rcx_in_atf);

	/* Enable interrupt mode since mt6897, mt6989 */
	if (of_property_read_u32(np, "enable-interrupt", &enable_interrupt)) {
		HWLOGR_INFO("get enable-interrupt failed\n");
		enable_interrupt = 0;
	}
	HWLOGR_INFO("enable_interrupt : %u\n", enable_interrupt);

	if (of_property_read_u32(np, "interrupt-lbc-sz", &hw_log_lbc_size)) {
		HWLOGR_INFO("get interrupt-lbc-sz failed\n");
		hw_log_lbc_size = 0;
	}
	HWLOGR_INFO("hw_log_lbc_size : %u\n", hw_log_lbc_size);

	ret = hw_logger_create_procfs(dev);
	if (ret) {
		HWLOGR_ERR("hw_logger_create_procfs fail\n");
		goto remove_procfs;
	}

#ifdef HW_LOG_SYSFS_BIN
	ret = hw_logger_create_sysfs(dev);
	if (ret) {
		HWLOGR_ERR("hw_logger_create_sysfs fail\n");
		goto remove_sysfs;
	}
#endif

	ret = hw_logger_memmap(pdev);
	if (ret) {
		HWLOGR_ERR("hw_logger_ioremap fail\n");
		goto remove_ioremap;
	}

	ret = hw_logger_buf_alloc(dev);
	if (ret) {
		HWLOGR_ERR("hw_logger_buf_alloc fail\n");
		goto remove_hw_log_buf;
	}

	if (enable_interrupt) {
		hw_logger_irq_number = platform_get_irq_byname(pdev, "apu_logtop");
		HWLOGR_INFO("hw_logger_irq_number = %d\n", hw_logger_irq_number);

		ret = devm_request_threaded_irq(dev,
			hw_logger_irq_number,
			NULL, apu_logtop_irq_handler, IRQF_ONESHOT,
			pdev->name, NULL);
		if (ret) {
			HWLOGR_ERR("failed to request IRQ (%d)\n", ret);
			goto remove_hw_log_buf;
		}
	}

	if (!enable_interrupt) {
		apusys_hwlog_wq = create_workqueue(APUSYS_HWLOG_WQ_NAME);

		/* Used for mobile log */
		INIT_DELAYED_WORK(&apusys_mtklog_task, apu_logtop_mtklog_wq);
		/* Used for deep idle */
		INIT_WORK(&apusys_hwlog_task, apu_logtop_copy_buf_wq);
		/* Init read pointer */
		set_r_ptr(get_st_addr());
	}

	HWLOGR_INFO("end\n");

	return 0;

remove_hw_log_buf:
	kfree(hw_log_buf);
	hw_log_buf = NULL;
	kfree(local_log_buf);
	local_log_buf = NULL;

remove_ioremap:
	if (apu_logtop) {
		iounmap(apu_logtop);
		apu_logtop = NULL;
	}
	if (apu_mbox) {
		iounmap(apu_mbox);
		apu_mbox = NULL;
	}

#ifdef HW_LOG_SYSFS_BIN
remove_sysfs:
	hw_logger_remove_sysfs(dev);
#endif

remove_procfs:
	hw_logger_remove_procfs(dev);

	HWLOGR_ERR("hw_logger probe error!!\n");

	return ret;
}

static int hw_logger_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	HWLOGR_INFO("in\n");

	if (enable_interrupt && hw_logger_irq_number) {
		HWLOGR_INFO("disable hw logger irq\n");
		disable_irq(hw_logger_irq_number);
	}

	if (!enable_interrupt) {
		flush_workqueue(apusys_hwlog_wq);
		destroy_workqueue(apusys_hwlog_wq);
	}
	hw_logger_remove_procfs(dev);
#ifdef HW_LOG_SYSFS_BIN
	hw_logger_remove_sysfs(dev);
#endif
	dma_unmap_single(dev, hw_log_buf_addr, HWLOGR_LOG_SIZE, DMA_FROM_DEVICE);

	kfree(hw_log_buf);
	hw_log_buf = NULL;
	kfree(local_log_buf);
	local_log_buf = NULL;

	if (aov_hw_log_buf) {
		dma_unmap_single(dev, aov_hw_log_buf_addr, aov_log_buf_sz, DMA_FROM_DEVICE);
		kfree(aov_hw_log_buf);
		aov_hw_log_buf = NULL;
	}

	kfree(aov_tcm_buf);
	aov_tcm_buf = NULL;

	iounmap(apu_logtop);
	apu_logtop = NULL;

	if (apu_mbox) {
		iounmap(apu_mbox);
		apu_mbox = NULL;
	}

	return 0;
}

static void hw_logger_shutdown(struct platform_device *pdev)
{
	HWLOGR_INFO("in\n");

	if (enable_interrupt && hw_logger_irq_number) {
		HWLOGR_INFO("disable hw logger irq\n");
		disable_irq(hw_logger_irq_number);
	}
}

static const struct of_device_id apusys_hw_logger_of_match[] = {
	{ .compatible = "mediatek,apusys_hw_logger"},
	{},
};

static struct platform_driver hw_logger_driver = {
	.probe = hw_logger_probe,
	.remove = hw_logger_remove,
	.shutdown = hw_logger_shutdown,
	.driver = {
		.name = HW_LOGGER_DEV_NAME,
		.of_match_table = of_match_ptr(apusys_hw_logger_of_match),
	}
};

int hw_logger_init(struct apusys_core_info *info)
{
	int ret = 0;

	HWLOGR_INFO("in\n");

	allow_signal(SIGKILL);

	ret = platform_driver_register(&hw_logger_driver);
	if (ret != 0) {
		HWLOGR_ERR("failed to register hw_logger driver");
		return -ENODEV;
	}

	return ret;
}

void hw_logger_exit(void)
{
	disallow_signal(SIGKILL);
	platform_driver_unregister(&hw_logger_driver);
}
