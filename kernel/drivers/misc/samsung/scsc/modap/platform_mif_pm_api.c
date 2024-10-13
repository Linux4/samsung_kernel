#include "modap/platform_mif_pm_api.h"
#include "modap/platform_mif_regmap_api.h"
#include "mif_reg.h"
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
#include <soc/samsung/exynos/exynos-s2mpu.h>
#endif

#ifdef CONFIG_WLBT_KUNIT
#include "../kunit/kunit_platform_mif_pm_api.c"
#endif

static bool disable_apm_setup = true;
module_param(disable_apm_setup, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_apm_setup, "Disable host APM setup");

static bool enable_platform_mif_arm_reset = true;
module_param(enable_platform_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables WIFIBT ARM cores reset");

static bool init_done;

/* Temporary workaround to power up slave PMIC LDOs before FW APM/WLBT signalling
 * is complete
 */
static void power_supplies_on(struct platform_mif *platform)
{
	//struct i2c_client i2c;

	/* HACK: Note only addr field is needed by s2mpu11_write_reg() */
	//i2c.addr = 0x1;

	/* The APM IPC in FW will be used instead */
	if (disable_apm_setup) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT LDOs firmware controlled\n");
		return;
	}

	//SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT LDOs on (PMIC i2c_addr = 0x%x)\n", i2c.addr);

	/* SLAVE PMIC
	 * echo 0x22 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xE0 > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x23 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xE8 > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x24 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xEC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x25 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xEC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x26 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xFC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x27 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xFC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 */

	/*s2mpu11_write_reg(&i2c, 0x22, 0xe0);
	s2mpu11_write_reg(&i2c, 0x23, 0xe8);
	s2mpu11_write_reg(&i2c, 0x24, 0xec);
	s2mpu11_write_reg(&i2c, 0x25, 0xec);
	s2mpu11_write_reg(&i2c, 0x26, 0xfc);
	s2mpu11_write_reg(&i2c, 0x27, 0xfc);*/
}

static int platform_mif_start_wait_for_cfg_ack_completion(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* done as part of platform_mif_pmu_reset_release() init_done sequence */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "start wait for completion\n");
	/* At this point WLBT should assert the CFG_REQ IRQ, so wait for it */
	if (wait_for_completion_timeout(&platform->cfg_ack, WLBT_BOOT_TIMEOUT) == 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Timeout waiting for CFG_REQ IRQ\n");
		platform_wlbt_regdump(interface);
		return -ETIMEDOUT;
	}

	/* only continue if CFG_REQ IRQ configured WLBT/PMU correctly */
	if (platform->boot_state == WLBT_BOOT_CFG_ERROR) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "CFG_REQ failed to configure WLBT.\n");
		return -EIO;
	}

	return 0;
}

static int platform_mif_pmu_reset_release(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
#ifdef CONFIG_WLBT_AUTOGEN_PMUCAL
	int		ret = 0;
	/* We're now ready for the IRQ */
	platform->boot_state = WLBT_BOOT_WAIT_CFG_REQ;
	smp_wmb(); /* commit before irq */

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "on boot_state = WLBT_BOOT_WAIT_CFG_REQ\n");
	if(!init_done) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "init\n");
		ret = pmu_cal_progress(
			platform,
			pmucal_wlbt.init,
			pmucal_wlbt.init_size);
		if(ret < 0)
			goto done;
		init_done = 1;
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "release\n");
		ret = pmu_cal_progress(
			platform,	
			pmucal_wlbt.reset_release,
			pmucal_wlbt.reset_release_size);
		if(ret < 0)
			goto done;
	}

	/* Now handle the CFG_REQ IRQ */
	enable_irq(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	/* Wait for CFG_ACK completion */
	ret = platform_mif_start_wait_for_cfg_ack_completion(interface);
done:
	return ret;
#else
	return platform_mif_manual_pmu_reset_release(interface, &init_done);
#endif
}

static int platform_mif_pmu_reset_assert(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
	int ret = exynos_s2mpu_backup_subsystem("WLBT");

	if (ret)
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "exynos_s2mpu_backup_subsystem failed err:%d\n", ret);
#endif
#ifdef CONFIG_WLBT_AUTOGEN_PMUCAL
	return pmu_cal_progress(
		platform,
		pmucal_wlbt.reset_assert,
		pmucal_wlbt.reset_assert_size);
#else
	return platform_mif_manual_pmu_reset_assert(interface);
#endif
}

/* reset=0 - release from reset */
/* reset=1 - hold reset */
int platform_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 ret = 0;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (enable_platform_mif_arm_reset || !reset) {
		if (!reset) { /* Release from reset */
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"SOC_VERSION: product_id 0x%x, rev 0x%x\n",
				exynos_soc_info.product_id, exynos_soc_info.revision);
#endif
			power_supplies_on(platform);
			ret = platform_mif_pmu_reset_release(interface);
		} else {
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
			cancel_work_sync(&platform->cfgreq_wq);
			flush_workqueue(platform->cfgreq_workq);
#endif
			/* Put back into reset */
			ret = platform_mif_pmu_reset_assert(interface);

			/* Free pmu array */
			if (platform->ka_patch_fw) {
				devm_kfree(platform->dev, platform->ka_patch_fw);
				platform->ka_patch_fw = NULL;
			}
		}
	} else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Not resetting ARM Cores - enable_platform_mif_arm_reset: %d\n",
			 enable_platform_mif_arm_reset);

	return ret;
}

static void platform_mif_reg_save(struct platform_mif *platform)
{
	platform->mif_preserve.irq_bit_mask = __platform_mif_irq_bit_mask_read(platform);
	platform->mif_preserve.irq_bit_mask_wpan = __platform_mif_irq_bit_mask_read_wpan(platform);
}

/* Restore MIF registers that may have been lost during suspend */
static void platform_mif_reg_restore(struct platform_mif *platform)
{
	__platform_mif_irq_bit_mask_write(platform, platform->mif_preserve.irq_bit_mask);
	__platform_mif_irq_bit_mask_write_wpan(platform, platform->mif_preserve.irq_bit_mask_wpan);
}

int platform_mif_suspend(struct scsc_mif_abs *interface)
{
	int r = 0;
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	enable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num);
	enable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num);

	if (platform->suspend_handler)
		r = platform->suspend_handler(interface, platform->suspendresume_data);

	/* Save the MIF registers.
	 * This must be done last as the suspend_handler may use the MIF
	 */
	platform_mif_reg_save(platform);

	return r;
}

void platform_mif_resume(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct regmap *regmap = platform_mif_get_regmap(platform, PMUREG);
	s32 ret;

	disable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num);
	disable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num);

	/* Restore the MIF registers.
	 * This must be done first as the resume_handler may use the MIF.
	 */
	platform_mif_reg_restore(platform);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Clear WLBT_ACTIVE_CLR flag\n");
	/* Clear WLBT_ACTIVE_CLR flag in WLBT_CTRL_NS */
	ret = regmap_write_bits(regmap, WLBT_CTRL_NS | 0xc000, 8, 8);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_CLR]: %d\n", ret);
	}

	if (platform->resume_handler)
		platform->resume_handler(interface, platform->suspendresume_data);
}


