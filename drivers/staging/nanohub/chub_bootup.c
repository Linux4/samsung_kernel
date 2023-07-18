// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CHUB IF Driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *      Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */

#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos-smc.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/soc/samsung/chub.h>
#include "chub.h"
#include "chub_exynos.h"

#undef ALIVE_WORK

static DEFINE_MUTEX(reset_mutex);
static DEFINE_MUTEX(pmu_shutdown_mutex);
static DEFINE_MUTEX(chub_err_mutex);
static DEFINE_MUTEX(chub_power_mutex);

#define CHUB_BOOTUP_TIME_MS (3000)

const char *os_image[SENSOR_VARIATION] = {
	"os.checked_0.bin",
	"os.checked_1.bin",
	"os.checked_2.bin",
	"os.checked_3.bin",
	"os.checked_4.bin",
	"os.checked_5.bin",
	"os.checked_6.bin",
	"os.checked_7.bin",
	"os.checked_8.bin",
};

void contexthub_power_lock(void)
{
	mutex_lock(&chub_power_mutex);
}
EXPORT_SYMBOL(contexthub_power_lock);

void contexthub_power_unlock(void)
{
	mutex_unlock(&chub_power_mutex);
}
EXPORT_SYMBOL(contexthub_power_unlock);

static void contexthub_reset_token(struct contexthub_ipc_info *chub)
{
	atomic_set(&chub->atomic.in_use_ipc, 0);
}

static inline void contexthub_set_in_reset(struct contexthub_ipc_info *chub,
					   bool i)
{
	atomic_set(&chub->atomic.in_reset, i);
}

static ssize_t chub_reset(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count);

#ifdef CONFIG_CONTEXTHUB_SENSOR_DEBUG
#define SENSORTASK_KICK_MS (5000)

void sensor_alive_check_func(struct work_struct *work)
{
	int ret;
	struct contexthub_ipc_info *ipc = container_of(to_delayed_work(work),
						       struct contexthub_ipc_info,
						       sensor_alive_work);

	nanohub_info("%s\n", __func__);
	ret = ipc_sensor_alive_check();
	if (ret < 0) {
		nanohub_info("%s: contexthub no reponce -> reset\n", __func__);
		contexthub_reset(ipc, true, 0xff);
	}
	schedule_delayed_work(&ipc->sensor_alive_work, msecs_to_jiffies(SENSORTASK_KICK_MS));
}
#endif
static int contexthub_alive_check(struct contexthub_ipc_info *chub)
{
	int trycnt = 0;
	int ret = 0;

	do {
		contexthub_ipc_write_event(chub, MAILBOX_EVT_CHUB_ALIVE);
		if (++trycnt > WAIT_TRY_CNT)
			break;
	} while ((atomic_read(&chub->atomic.chub_status) != CHUB_ST_RUN));

	if (atomic_read(&chub->atomic.chub_status) == CHUB_ST_RUN) {
		nanohub_dev_info(chub->dev,
				 "%s done. contexthub status is %d\n", __func__,
				 atomic_read(&chub->atomic.chub_status));
	} else {
		nanohub_dev_warn(chub->dev,
				 "%s failed. contexthub status is %d\n",
				 __func__, atomic_read(&chub->atomic.chub_status));

		if (!atomic_read(&chub->atomic.in_reset)) {
			atomic_set(&chub->atomic.chub_status, CHUB_ST_NO_RESPONSE);
			contexthub_handle_debug(chub,
						CHUB_ERR_CHUB_NO_RESPONSE);
		} else {
			nanohub_dev_info(chub->dev,
					 "%s: skip to handle debug in reset\n",
					 __func__);
		}
		ret = -ETIMEDOUT;
	}
	return ret;
}

/* contexhub slient reset support */
void contexthub_handle_debug(struct contexthub_ipc_info *chub,
			     enum chub_err_type err)
{
	int thold = (err < CHUB_ERR_CRITICAL) ? 1 :
	    ((err <
	      CHUB_ERR_MAJER) ? CHUB_RESET_THOLD : CHUB_RESET_THOLD_MINOR);

	/* update error count */
	chub->misc.err_cnt[err]++;

	nanohub_dev_info(chub->dev,
			 "%s: err:%d(cnt:%d), cur_err:%d, thold:%d, chub_status:%d\n",
			 __func__, err, chub->misc.err_cnt[err], chub->misc.cur_err,
			 thold, atomic_read(&chub->atomic.chub_status));

	/* set chub_status to CHUB_ST_ERR. request silent reset */
	if ((chub->misc.err_cnt[err] >= thold) && (atomic_read(&chub->atomic.chub_status) != CHUB_ST_ERR)) {
		atomic_set(&chub->atomic.chub_status, CHUB_ST_ERR);
		nanohub_dev_info(chub->dev,
				 "%s: err:%d(cnt:%d), enter error status\n",
				 __func__, err, chub->misc.err_cnt[err]);
		/* handle err */
		chub->misc.cur_err = err;
		schedule_work(&chub->debug_work);
	}
}

int contexthub_verify_symtable(struct contexthub_ipc_info *chub, void *data, size_t size)
{
	u32 magic, symbol_size, offset = 0;

	if (!chub || chub->misc.multi_os || chub->symbol_table)
		return -EINVAL;

	magic = *(u32 *)(data + size - 4);
	if (magic != 0x626d7973) {
		offset = 528;
		magic = *(u32 *)(data + size - offset - 4);
	}

	nanohub_dev_info(chub->dev, "read magic : %x\n", magic);
	if (magic == 0x626d7973) {
		symbol_size = *(u32 *)(data + size - offset - 8);
		nanohub_dev_info(chub->dev, "symbol table size : %d\n",
				 symbol_size);

		if (symbol_size < size - offset) {
			chub->symbol_table = kmalloc(symbol_size, GFP_KERNEL);
			if (chub->symbol_table) {
				memcpy(chub->symbol_table,
				       data + size - symbol_size - offset - 8,
				       symbol_size);
				nanohub_dev_info(chub->dev,
						 "Load symbol table Done!!!\n");
			}
		}
	} else {
		chub->symbol_table = NULL;
	}

	return 0;
}

static void contexthub_set_baaw(struct contexthub_ipc_info *chub)
{
	int i, j, offset, ret;

	/* tzpc setting, if defined in dt */
	if (of_get_property(chub->dev->of_node, "smc-required", NULL)) {
		ret = exynos_smc(SMC_CMD_CONN_IF,
				 (EXYNOS_CHUB << 32) | EXYNOS_SET_CONN_TZPC,
				 0, 0);
		if (ret) {
			nanohub_err("%s: TZPC setting fail by ret:%d\n", __func__, ret);
			return;
		}
	}

	for (j = 0 ; j < BAAW_MAX; j++) {
		if (IS_ERR_OR_NULL(chub->iomem.chub_baaws[j].addr))
			break;

		for (i = 0 ; i < chub->iomem.chub_baaws[j].size * 5; i++) {
			if (!(i % 5)) {
				offset = chub->iomem.chub_baaws[j].values[i];
				continue;
			}
			nanohub_dev_debug(chub->dev, "%s: %s offset +%x %x\n", __func__,
					  chub->iomem.chub_baaws[j].name,
					  offset + (i % 5 - 1) * 4, chub->iomem.chub_baaws[j].values[i]);
			__raw_writel(chub->iomem.chub_baaws[j].values[i],
				     chub->iomem.chub_baaws[j].addr + offset + (i % 5 - 1) * 4);
		}
	}
}

static int contexthub_reset_prepare(struct contexthub_ipc_info *chub)
{
	int ret;
	/* pmu call reset-release_config */
	ret = cal_chub_reset_release_config();
	if (ret) {
		nanohub_err("%s: reset release cfg fail\n", __func__);
		return ret;
	}

	/* baaw config */
	nanohub_dev_info(chub->dev, "%s: tzpc and baaw set\n", __func__);
	contexthub_set_baaw(chub);

	contexthub_chub_ipc_reset(chub);
	contexthub_upmu_init(chub);

	return ret;
}

int contexthub_shutdown(struct contexthub_ipc_info *chub)
{
	int ret;
	int status = atomic_read(&chub->atomic.chub_status);

	if (atomic_read(&chub->atomic.chub_shutdown)) {
		nanohub_dev_info(chub->dev, "%s: chub already shutdown!\n", __func__);
		return 0;
	}
	/* mutex in ISR(itmon) is not allowed */
	if (status != CHUB_ST_ITMON)
		mutex_lock(&pmu_shutdown_mutex);
	/* pmu call assert */
	ret = cal_chub_reset_assert();
	if (ret) {
		nanohub_err("%s: reset assert fail\n", __func__);
		if (status != CHUB_ST_ITMON)
			mutex_unlock(&pmu_shutdown_mutex);
		return ret;
	}

	/* pmu call reset-release_config */
	ret = contexthub_reset_prepare(chub);
	if (ret) {
		nanohub_err("%s: reset prepare fail\n", __func__);
		if (status != CHUB_ST_ITMON)
			mutex_unlock(&pmu_shutdown_mutex);
		return ret;
	}
	atomic_set(&chub->atomic.chub_shutdown, 1);
	if (status != CHUB_ST_ITMON)
		mutex_unlock(&pmu_shutdown_mutex);

	return ret;
}

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
static int contexthub_s2mpu_notifier(struct s2mpu_notifier_block *nb,
				     struct s2mpu_notifier_info *nb_data)
{
	struct contexthub_ipc_info *data =
	    container_of(nb, struct contexthub_ipc_info, ext.s2mpu_nb);
	struct s2mpu_notifier_info *s2mpu_data = nb_data;

	(void)data;
	(void)s2mpu_data;

	nanohub_info("%s called!\n", __func__);
	contexthub_handle_debug(data, CHUB_ERR_S2MPU);
	return S2MPU_NOTIFY_OK;
}

static void contexthub_register_s2mpu_notifier(struct contexthub_ipc_info *chub)
{
	chub->ext.s2mpu_nb.notifier_call = contexthub_s2mpu_notifier;
	chub->ext.s2mpu_nb.subsystem = "CHUB";
	exynos_s2mpu_notifier_call_register(&chub->ext.s2mpu_nb);
}
#endif // IS_ENABLED(CONFIG_EXYNOS_S2MPU)

#define MAX_FIRMWARE_NUM 3
#define SRAM_OFFSET_OS SZ_4K

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU) && IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
static void contexthub_imgloader_checker(struct imgloader_desc *desc, phys_addr_t fw_phys_base, size_t fw_bin_size, size_t fw_mem_size) {
	const struct firmware *entry;
	int ret = 0;

	nanohub_dev_warn(desc->dev, "=====compare SRAM and F/W binary=====\n");
	nanohub_dev_warn(desc->dev, "loaded fw name : %s\n", desc->fw_name);
	nanohub_dev_warn(desc->dev, "loaded fw size : 0x%X\n", fw_bin_size);

	ret = request_firmware(&entry, desc->fw_name, desc->dev);

	if (ret) {
		nanohub_dev_err(desc->dev, "request_firmware failed for checking after s2mpu verification failed\n");
		release_firmware(entry);
	} else {
		int diff_check = 0, i;
		void *sram_phy = ioremap(fw_phys_base, fw_mem_size);
		uint8_t *sram = (uint8_t*)kmalloc(fw_bin_size, GFP_KERNEL);

		memcpy_fromio(sram, sram_phy, fw_bin_size);

		for (i = 0; i < fw_bin_size; i++) {
			if (sram[i] != entry->data[i]) {
				nanohub_dev_warn(desc->dev, "diff offset : 0x%x\n", i);
				nanohub_dev_warn(desc->dev, "sram val : 0x%x, bin val : 0x%x\n", sram[i], entry->data[i]);
				diff_check++;
			}

		}

		if (diff_check == 0)
			nanohub_dev_warn(desc->dev, "no diff between chub sram and binary\n");
		else
			nanohub_dev_warn(desc->dev, "has %d diff between chub sram and binary\n", diff_check);

		iounmap(sram);
		release_firmware(entry);
	}
	nanohub_dev_warn(desc->dev, "===compare SRAM and F/W binary end===\n");
}

static int contexthub_imgloader_verify_fw(struct imgloader_desc *desc,
						 phys_addr_t fw_phys_base,
						 size_t fw_bin_size,
						 size_t fw_mem_size)
{
	u64 ret64 = exynos_verify_subsystem_fw(desc->name, desc->fw_id,
					       fw_phys_base, fw_bin_size,
					       ALIGN(fw_mem_size, SZ_4K));
	if (ret64) {
		nanohub_dev_warn(desc->dev,
				 "Failed F/W verification, ret=%llu\n", ret64);

		contexthub_imgloader_checker(desc, fw_phys_base, fw_bin_size, fw_mem_size);

		return -EIO;
	}
	ret64 = exynos_request_fw_stage2_ap(desc->name);
	if (ret64) {
		nanohub_dev_warn(desc->dev,
				 "Failed F/W verification to S2MPU, ret=%llu\n",
				 ret64);
		return -EIO;
	}
	return 0;
}
#else
static int contexthub_imgloader_verify_fw(struct imgloader_desc *desc,
						 phys_addr_t fw_phys_base,
						 size_t fw_bin_size,
						 size_t fw_mem_size)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
static int contexthub_imgloader_mem_setup(struct imgloader_desc *desc, const u8 *metadata,
						 size_t size, phys_addr_t *fw_phys_base,
						 size_t *fw_bin_size, size_t *fw_mem_size)
{
	void *addr = NULL;
	struct contexthub_ipc_info *chub = (struct contexthub_ipc_info *)desc->data;

	*fw_phys_base = chub->iomem.sram_phys;
	*fw_bin_size = size;
	*fw_mem_size = chub->iomem.sram_size;
	addr = chub->iomem.sram;
	memcpy_toio(addr, metadata, size);
	nanohub_info("%s chub image %s loaded by imgloader\n", __func__,
		     desc->fw_name);
	contexthub_verify_symtable(chub, (void *)metadata, size);

	return 0;
}

static int contexthub_download_image(struct contexthub_ipc_info *chub, int idx)
{
	return imgloader_boot(&chub->ext.chub_img_desc[idx]);
}

static int contexthub_imgloader_desc_init(struct contexthub_ipc_info *chub,
						 struct imgloader_desc *desc,
						 const char *name, int id)
{
	static struct imgloader_ops contexthub_imgloader_ops = {
		.mem_setup = contexthub_imgloader_mem_setup,
		.verify_fw = contexthub_imgloader_verify_fw
	};

	desc->dev = chub->dev;
	desc->owner = THIS_MODULE;
	desc->ops = &contexthub_imgloader_ops;
	desc->data = (void *)chub;
	desc->name = "CHUB";
	desc->s2mpu_support = true;
	desc->fw_name = name;
	desc->fw_id = id;

	return imgloader_desc_init(desc);
}

static int contexthub_imgloader_init(struct contexthub_ipc_info *chub)
{
	int ret = 0;
	int i = 0, j;

	if (!chub->misc.multi_os) {
		ret |= contexthub_imgloader_desc_init(chub,
				&chub->ext.chub_img_desc[i], OS_IMAGE_DEFAULT, i);
		i++;
	} else {
		do {
			ret |= contexthub_imgloader_desc_init(chub,
					&chub->ext.chub_img_desc[i], os_image[i], i);
			i++;
		} while (i < MAX_FIRMWARE_NUM);
	}

	for (j = 0 ; j < i ; j++)
		nanohub_dev_debug(chub->dev, "%s fw name: %s\n", __func__,
				  chub->ext.chub_img_desc[j].fw_name);

	return ret;
}
#else
static int contexthub_download_image(struct contexthub_ipc_info *chub, int idx)
{
	const struct firmware *entry;
	int ret = 0;
	void *chub_addr = NULL;

	if (!chub->misc.multi_os) {
		nanohub_dev_info(chub->dev, "%s: download %s\n", __func__, OS_IMAGE_DEFAULT);
		ret = request_firmware(&entry, OS_IMAGE_DEFAULT, chub->dev);
		chub_addr = chub->iomem.sram;
	} else {
		nanohub_dev_info(chub->dev, "%s: download %s\n", __func__, os_image[idx]);
		ret = request_firmware(&entry, os_image[idx], chub->dev);
		chub_addr = chub->iomem.sram;
	}

	if (ret) {
		nanohub_dev_err(chub->dev,
				"%s: request_firmware failed (%d)\n", __func__, idx);
		release_firmware(entry);
		return ret;
	}
	memcpy_toio(chub_addr, entry->data, entry->size);

	nanohub_dev_info(chub->dev, "%s: idx:%d, bin(size:%d)\n",
			 __func__, idx, (int)entry->size);
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
	if (idx > 0)
		ret = exynos_verify_subsystem_fw("CHUB", 1,
						 chub->sram_phys + SZ_4K,
						 entry->size, chub->sram_size - SZ_4K);
	else
		ret = exynos_verify_subsystem_fw("CHUB", 0,
						 chub->sram_phys, entry->size, SZ_4K);

	if (ret) {
		nanohub_dev_err(chub->dev, "%s: verify fail!:%d\n", __func__,
				ret);
		release_firmware(entry);
		return -1;
	}
#endif
	if (idx)
		contexthub_verify_symtable(chub, (void *)entry->data, entry->size);

	release_firmware(entry);

	return 0;
}
#endif


#define CHUB_RESET_WAIT_TIME_MS (300)
int contexthub_reset(struct contexthub_ipc_info *chub, bool force_load,
		     enum chub_err_type err)
{
	int ret = 0;
	int trycnt = 0;
	bool irq_disabled;

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	if (chub->ext.sysevent_dev) {
		if (chub->ext.sysevent_status == NULL || chub->ext.sysevent_status == PTR_ERR) {
			nanohub_dev_info(chub->dev,
					"sysevent_get doesn't called. skip this.\n");
		} else {
			sysevent_put((void *)chub->ext.sysevent_dev);
		}
	}
#endif
	mutex_lock(&reset_mutex);
	nanohub_dev_info(chub->dev,
			 "%s: force:%d, status:%d, in-reset:%d, err:%d, user:%d\n",
			 __func__, force_load, atomic_read(&chub->atomic.chub_status),
			 contexthub_read_in_reset(chub), err,
			 contexthub_read_token(chub));
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	/* debug dump */
	if (!chub->misc.itmon_dumped) {
		if (chub->log.mlog.memlog_sram_chub)
			memlog_do_dump(chub->log.mlog.memlog_sram_chub, MEMLOG_LEVEL_EMERG);
		else
			nanohub_dev_err(chub->dev, "%s: memlog_sram_chub is NULL\n", __func__);
	}
#endif
	chub_dbg_dump_hw(chub, err);

	if (!force_load && (atomic_read(&chub->atomic.chub_status) == CHUB_ST_RUN)) {
		mutex_unlock(&reset_mutex);
		nanohub_dev_info(chub->dev, "%s: out status:%d\n", __func__,
				 atomic_read(&chub->atomic.chub_status));
		return 0;
	}

	/* disable mailbox interrupt to prevent sram access during chub reset */
	disable_irq(chub->irq.irq_mailbox);
	disable_irq(chub->irq.irq_wdt);
	irq_disabled = true;

	atomic_inc(&chub->atomic.in_reset);
	chub_wake_lock(chub->ws_reset);
	contexthub_notifier_call(chub, CHUB_FW_ST_OFF);

	/* wait for ipc free */
	while (atomic_read(&chub->atomic.in_use_ipc)) {
		chub_wait_event(&chub->event.reset_lock, CHUB_RESET_WAIT_TIME_MS);
		if (++trycnt > RESET_WAIT_TRY_CNT) {
			nanohub_dev_info(chub->dev,
					 "%s: can't get lock. in_use_ipc: %d\n",
					 __func__, contexthub_read_token(chub));
			ret = -EINVAL;
			goto out;
		}
		nanohub_dev_info(chub->dev, "%s: wait for ipc user free: %d\n",
				 __func__, contexthub_read_token(chub));
	}
	if (!trycnt)
		msleep(CHUB_RESET_WAIT_TIME_MS);	/* wait for subsystem release */

	contexthub_set_in_reset(chub, true);

	nanohub_dev_info(chub->dev, "%s: start reset status:%d\n", __func__,
			 atomic_read(&chub->atomic.chub_status));

	/* shutdown */
	contexthub_power_lock();
	ret = contexthub_shutdown(chub);
	contexthub_power_unlock();
	if (ret) {
		nanohub_dev_err(chub->dev, "%s: shutdown fails, ret:%d\n",
				__func__, ret);
		goto out;
	}

	/* image download */
	ret = contexthub_download_image(chub, chub->misc.num_os);
	if (ret) {
		nanohub_dev_err(chub->dev, "%s: download os fails\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	/* enable mailbox interrupt to get 'alive' event */
	enable_irq(chub->irq.irq_mailbox);
	irq_disabled = false;

	/* set boottime */
	ipc_set_chub_boottime(chub->misc.alive_mct);

	/* reset release */
	if (atomic_read(&chub->atomic.chub_shutdown)) {
		contexthub_status_reset(chub);
		ret = cal_chub_reset_release();
		if (ret)
			goto out;
		atomic_set(&chub->atomic.chub_shutdown, 0);
	} else {
		nanohub_dev_err(chub->dev,
				"contexthub status isn't shutdown. fails to reset: %d, %d\n",
				atomic_read(&chub->atomic.chub_shutdown),
				atomic_read(&chub->atomic.chub_status));
		ret = -EINVAL;
		goto out;
	}

	ret = contexthub_alive_check(chub);
	if (ret) {
		nanohub_dev_err(chub->dev, "%s: chub reset fail! (ret:%d)\n",
				__func__, ret);
	} else {
		nanohub_dev_info(chub->dev, "%s: chub reset done! (cnt:%d)\n",
				 __func__, chub->misc.err_cnt[CHUB_ERR_RESET_CNT]);
		chub->misc.err_cnt[CHUB_ERR_RESET_CNT]++;
		contexthub_reset_token(chub);
	}

out:
	if (ret) {
		atomic_set(&chub->atomic.chub_status, CHUB_ST_RESET_FAIL);
		if (irq_disabled)
			enable_irq(chub->irq.irq_mailbox);
		nanohub_dev_err(chub->dev,
				"%s: chub reset fail! should retry to reset (ret:%d), irq_disabled:%d\n",
				__func__, ret, irq_disabled);
	}
	msleep(100);		/* wakeup delay */
	chub_wake_event(&chub->event.reset_lock);
	contexthub_set_in_reset(chub, false);
	contexthub_notifier_call(chub, CHUB_FW_ST_ON);
	enable_irq(chub->irq.irq_wdt);
	chub_wake_unlock(chub->ws_reset);
	mutex_unlock(&reset_mutex);
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	if (chub->ext.sysevent_dev) {
		chub->ext.sysevent_status = sysevent_get("CHB");
		if (chub->ext.sysevent_status == PTR_ERR || chub->ext.sysevent_status == NULL)
			nanohub_warn("%s sysevent_get fail!", __func__);
	}
#endif
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	if (chub->log.mlog.memlog_printf_chub)
		memlog_sync_to_file(chub->log.mlog.memlog_printf_chub);
#endif
#ifdef CONFIG_CONTEXTHUB_DEBUG
	if (err)
		panic("contexthub crash");
#endif
	return ret;
}

int contexthub_poweron(struct contexthub_ipc_info *chub)
{
	int ret = 0;
	struct device *dev = chub->dev;

	if (atomic_read(&chub->atomic.chub_status) == CHUB_ST_NO_POWER) {
		nanohub_dev_info(dev, "%s sram size: 0x%lx %s\n",
				 __func__, chub->iomem.sram_size,
				 chub->misc.multi_os ? "multi os" : "");

		memset_io(chub->iomem.sram, 0, chub->iomem.sram_size);
		ret = contexthub_download_image(chub, chub->misc.num_os); // num_os is 0 in non-multios

		if (ret) {
			nanohub_dev_warn(dev, "fails to download bootloader\n");
			return ret;
		}

		ret = contexthub_ipc_drv_init(chub);
		if (ret) {
			dev_warn(dev, "fails to init ipc\n");
			return ret;
		}

		if (chub->misc.chub_dfs_gov) {
			nanohub_dev_info(dev, "%s: set dfs gov:%d\n", __func__, chub->misc.chub_dfs_gov);
			ipc_set_dfs_gov(chub->misc.chub_dfs_gov);
		}
		nanohub_dev_info(dev, "%s: get dfs gov:%d\n", __func__, ipc_get_dfs_gov());

		contexthub_status_reset(chub);

#ifdef NEED_TO_RTC_SYNC
		contexthub_check_time();
#endif
		/* enable Dump gpr */
		IPC_HW_WRITE_DUMPGPR_CTRL(chub->iomem.chub_dumpgpr, 0x1);

		/* set boottime */
		ipc_set_chub_boottime(chub->misc.alive_mct);

		ret = contexthub_soc_poweron(chub);
		if (ret) {
			nanohub_dev_err(chub->dev, "%s soc poweron fail\n", __func__);
			return ret;
		}

		atomic_set(&chub->atomic.chub_status, CHUB_ST_POWER_ON);
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
		ret = exynos_request_fw_stage2_ap("CHUB");
		if (ret)
			nanohub_dev_err(chub->dev, "%s fw stage2 fail %d\n", __func__, ret);
#endif
		/* don't send alive with first poweron of multi-os */
		if (!chub->misc.multi_os) {
			ret = contexthub_alive_check(chub);
			if (ret) {
				dev_warn(dev, "fails to poweron\n");
				return ret;
			}
		}

		if (atomic_read(&chub->atomic.chub_status) == CHUB_ST_RUN) {
			/* chub driver directly gets alive event without multi-os */
			nanohub_dev_info(dev, "%s: contexthub power-on\n", __func__);
		} else {
			if (chub->misc.multi_os) {	/* with multi-os */
				nanohub_dev_info(dev, "%s: wait for multi-os poweron\n", __func__);
				ret = chub_wait_event(&chub->event.poweron_lock, WAIT_TIMEOUT_MS * 2);
				nanohub_dev_info(dev,
						 "%s: multi-os poweron %s, status:%d, ret:%d, flag:%d\n",
						 __func__,
						 (atomic_read(&chub->atomic.chub_status) == CHUB_ST_RUN) ?
						 "success" : "fails",
						 atomic_read(&chub->atomic.chub_status), ret,
						 chub->event.poweron_lock.flag);
				chub->misc.multi_os = true;
			} else	/* without multi-os */
				nanohub_dev_warn(dev, "contexthub failed to power-on\n");
		}
	} else {
		/* CHUB already went through poweron sequence */
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	if (chub->ext.sysevent_dev) {
		chub->ext.sysevent_status = sysevent_get("CHB");
		if (chub->ext.sysevent_status == PTR_ERR || chub->ext.sysevent_status == NULL)
			nanohub_warn("%s sysevent_get fail!", __func__);
	}
#endif
	contexthub_notifier_call(chub, CHUB_FW_ST_POWERON);
#ifdef IPC_DEBUG
	ipc_test(chub);
#endif

#ifdef CONFIG_CONTEXTHUB_SENSOR_DEBUG
#ifdef ALIVE_WORK
	nanohub_dev_info(dev, "contexthub schedule sensor_alive_work\n");
	schedule_delayed_work(&chub->sensor_alive_work, msecs_to_jiffies(SENSORTASK_KICK_MS * 4));
	chub->sensor_alive_work_run = true;
#else
	nanohub_dev_info(dev, "skip sensor_alive_work\n");
#endif
#endif
	contexthub_upmu_init(chub);

	nanohub_info("%s done!\n", __func__);

	return 0;
}

static void contexthub_select_os(struct contexthub_ipc_info *chub)
{
	int ret;
	u8 val = (u8)ipc_read_hw_value(IPC_VAL_HW_DEBUG);

	if (!val) {
		nanohub_dev_warn(chub->dev, "%s os number is invalid\n", __func__);
		val = 1;
	}

	chub->misc.num_os = val;
	nanohub_dev_info(chub->dev, "%s selected name: %s\n", __func__, os_image[chub->misc.num_os]);
	ipc_set_chub_bootmode(BOOTMODE_COLD, chub->log.chub_rt_log.loglevel);
	contexthub_shutdown(chub);
	ret = contexthub_download_image(chub, chub->misc.num_os);
	if (ret)
		nanohub_warn("%s: os download fail! %d", __func__, ret);
	chub->misc.multi_os = false;
	ipc_write_hw_value(IPC_VAL_HW_BOOTMODE, chub->misc.os_load);
	ipc_set_chub_boottime(chub->misc.alive_mct);
	cal_chub_reset_release();
	//contexthub_core_reset(chub);
	contexthub_alive_check(chub);
	nanohub_dev_info(chub->dev, "%s done: wakeup interrupt\n", __func__);
	chub_wake_event(&chub->event.poweron_lock);
}

static void handle_debug_work_func(struct work_struct *work)
{
	struct contexthub_ipc_info *chub =
	    container_of(work, struct contexthub_ipc_info, debug_work);

	/* 1st work: os select on booting */
	if ((atomic_read(&chub->atomic.chub_status) == CHUB_ST_POWER_ON) && chub->misc.multi_os) {
		contexthub_select_os(chub);
		return;
	}

	mutex_lock(&chub_err_mutex);
	/* 2nd work: slient reset */
	if (chub->misc.cur_err) {
		int ret;
		int err = chub->misc.cur_err;

		chub->misc.cur_err = 0;
		nanohub_dev_info(chub->dev,
				 "%s: request silent reset. err:%d, status:%d, in-reset:%d\n",
				 __func__, err, __raw_readl(&chub->atomic.chub_status),
				 __raw_readl(&chub->atomic.in_reset));
		ret = chub_reset(chub->dev, NULL, NULL, 0);
		if (ret) {
			nanohub_dev_warn(chub->dev,
					 "%s: fails to reset:%d. status:%d\n",
					 __func__, ret,
					 __raw_readl(&chub->atomic.chub_status));
		} else {
			chub->misc.cur_err = 0;
			nanohub_dev_info(chub->dev, "%s: done(chub reset)\n", __func__);
		}
	}
	mutex_unlock(&chub_err_mutex);
}

static ssize_t chub_poweron(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct contexthub_ipc_info *chub = dev_get_drvdata(dev);
	int ret = contexthub_poweron(chub);

	return ret < 0 ? ret : count;
}

static ssize_t chub_reset(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct contexthub_ipc_info *chub = dev_get_drvdata(dev);
	int ret;

#ifdef CONFIG_SENSOR_DRV
	ret = nanohub_hw_reset(chub->data);
#else
	ret = contexthub_reset(chub, 1, CHUB_ERR_NONE);
#endif

	return ret < 0 ? ret : count;
}

static struct device_attribute attributes[] = {
	__ATTR(poweron, 0220, NULL, chub_poweron),
	__ATTR(reset, 0220, NULL, chub_reset),
};

int contexthub_bootup_init(struct contexthub_ipc_info *chub)
{
	int i;
	int ret = 0;

	contexthub_set_baaw(chub);
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
	contexthub_register_s2mpu_notifier(chub);
#endif
	chub->ws_reset = chub_wake_lock_init(chub->dev, "chub_reboot");
	nanohub_dev_info(chub->dev, "%s clk [%lu]\n", __func__, chub->misc.clkrate);
	init_waitqueue_head(&chub->event.poweron_lock.event);
	init_waitqueue_head(&chub->event.reset_lock.event);
	init_waitqueue_head(&chub->event.log_lock.event);
	init_waitqueue_head(&chub->event.chub_alive_lock.event);
	atomic_set(&chub->event.poweron_lock.flag, 0);
	atomic_set(&chub->event.chub_alive_lock.flag, 0);
	contexthub_reset_token(chub);
	contexthub_set_in_reset(chub, 0);
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	contexthub_imgloader_init(chub);
#endif
	INIT_WORK(&chub->debug_work, handle_debug_work_func);
#ifdef CONFIG_CONTEXTHUB_SENSOR_DEBUG
	INIT_DELAYED_WORK(&chub->sensor_alive_work, sensor_alive_check_func);
#endif

	for (i = 0, ret = 0; i < ARRAY_SIZE(attributes); i++) {
		ret = device_create_file(chub->dev, &attributes[i]);
		if (ret)
			nanohub_dev_warn(chub->dev,
					 "Failed to create file: %s\n",
					 attributes[i].attr.name);
	}
	return ret;
}
