#include <linux/version.h>

//#include <soc/samsung/exynos-smc.h>
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_HVC)
#include <soc/samsung/exynos/exynos-hvc.h>
#endif
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
#include <soc/samsung/exynos/exynos-s2mpu.h>
#endif

#include "modap/platform_mif_regmap_api.h"
#include "mif_reg.h"
#include "baaw.h"
#include "regmap_register.h"

#ifdef CONFIG_WLBT_KUNIT
#include "../kunit/kunit_platform_mif_regmap_api.c"
#endif

#define COMP_RET(x, y) do { \
	if (x != y) {\
		pr_err("%s failed at L%d", __FUNCTION__, __LINE__); \
		return -1; \
	} \
} while (0)

const char regmap_lookup_table[REGMAP_LIST_SIZE][REGMAP_LIST_LENGTH] = {
	{"samsung,syscon-phandle"},
	{"samsung,dbus_baaw-syscon-phandle"},
	{"samsung,boot_cfg-syscon-phandle"},
	{"samsung,pbus_baaw-syscon-phandle"},
	{"samsung,wlbt_remap-syscon-phandle"},
#ifdef CONFIG_SCSC_I3C
	{"samsung,i3c_apm_pmic-syscon-phandle"},
#endif
};

#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
#define WLBT_SUBSYS_NAME "WLBT"
#endif

struct regmap *platform_mif_get_regmap( struct platform_mif *platform, enum regmap_name _regmap_name)
{
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "[%s] -- regmap[%d] : %p\n", __func__, _regmap_name, platform->regmap[_regmap_name]);
	return platform->regmap[_regmap_name];
}

int platform_mif_set_regmap(struct platform_mif *platform)
{
	int i;

	platform->regmap = (struct regmap **)devm_kzalloc(platform->dev, sizeof(struct regmap *) * REGMAP_LIST_SIZE, GFP_KERNEL);

	/* Completion event and state used to indicate CFG_REQ IRQ occurred */
	init_completion(&platform->cfg_ack);
	platform->boot_state = WLBT_BOOT_IN_RESET;

	for (i = 0; i < REGMAP_LIST_SIZE; i++) {
		//struct regmap *regmap = platform_mif_get_regmap(platform, i);
		const char *lookup_table = regmap_lookup_table[i];

		platform->regmap[i] = syscon_regmap_lookup_by_phandle(
			platform->dev->of_node,
			lookup_table);

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "[%s] -- regmap[%d] : %p\n", __func__, i, platform->regmap[i]);

		if (IS_ERR(platform->regmap[i])) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"%s failed. Aborting. %ld\n",
				lookup_table,
				PTR_ERR(platform->regmap[i]));
			devm_kfree(platform->dev, platform->regmap);
			return -EINVAL;
		}
	}

	return 0;
}

static void platform_wlbt_print_mailbox(struct platform_mif *platform)
{
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR0 0x%08x\n",
			platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR0 0x%08x\n",
			platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR0 0x%08x\n",
			platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR0 0x%08x\n",
			platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR0 0x%08x\n",
			platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR0)));

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR1 0x%08x\n",
			platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR1 0x%08x\n",
			platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR1 0x%08x\n",
			platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR1 0x%08x\n",
			platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR1 0x%08x\n",
			platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR1)));

}

static void platform_wlbt_print_pmureg_register(struct platform_mif *platform)
{
	int i;
	struct regmap *regmap = platform_mif_get_regmap(platform, PMUREG);

	for (i = 0; i < sizeof(pmureg_register_offset) / sizeof(const u32); i++)
	{
		u32 val;
		regmap_read(regmap, pmureg_register_offset[i], &val);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "%s -- 0x%x\n",
			pmureg_register_name[i], val);
	}
}

static void platform_wlbt_print_boot_cfg_register(struct platform_mif *platform)
{
	int i;
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);
	for (i = 0; i < sizeof(boot_cfg_register_offset) / sizeof(const u32); i++)
	{
		u32 val;
		regmap_read(regmap, boot_cfg_register_offset[i], &val);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "%s -- 0x%x\n",
			boot_cfg_register_name[i], val);
	}
}

#ifndef CONFIG_SOC_S5E8845
#ifndef CONFIG_SOC_S5E5535
static void platform_wlbt_print_karam(struct platform_mif *platform)
{
	u32 val;
	uint32_t ka_val = 0;
	unsigned int ka_addr = PMU_BOOT_RAM_START;
	unsigned int ka_offset = 0;
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);

	SCSC_TAG_INFO(PLAT_MIF, "AP accesses KARAM\n");
	regmap_write(regmap, PMU_BOOT, PMU_BOOT_AP_ACC);
	regmap_read(regmap, PMU_BOOT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "Updated BOOT_SOURCE: 0x%x\n", val);
	SCSC_TAG_INFO(PLAT_MIF, "KARAM Info:\n");
	while (ka_addr <= PMU_BOOT_RAM_END) {
		regmap_read(regmap, ka_addr, &ka_val);
		SCSC_TAG_INFO(PLAT_MIF, "0x%08x(0x%x)", ka_val, ka_offset);
		ka_addr += (unsigned int)sizeof(unsigned int);
		ka_offset += (unsigned int)sizeof(unsigned int);
	}

	SCSC_TAG_INFO(PLAT_MIF, "WLBT PMU accesses KARAM\n");

	regmap_write(regmap, PMU_BOOT, PMU_BOOT_PMU_ACC);
	regmap_read(regmap, PMU_BOOT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "Updated BOOT_SOURCE: 0x%x\n", val);
	regmap_read(regmap, PMU_BOOT_ACK, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "BOOT_CFG_ACK 0x%x\n", val);
}
#endif
#endif

int platform_mif_set_dbus_baaw(struct platform_mif *platform)
{
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
	int ret;
#endif
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_HVC)
	unsigned int val;

	val = exynos_hvc(
		HVC_FID_SET_BAAW_WINDOW,
		0,
		((u64)(WLBT_DBUS_BAAW_0_START >> BAAW_BIT_SHIFT) << 32) | (WLBT_DBUS_BAAW_0_END >> BAAW_BIT_SHIFT),
		(platform->mem_start >> BAAW_BIT_SHIFT),
		WLBT_BAAW_ACCESS_CTRL);

	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_START, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "MOHIT : Updated WLBT_DBUS_BAAW_0_START: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_END, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_END: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_REMAP, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_REMAP: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_INIT_DONE, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_ENABLE_DONE: 0x%x.\n", val);
#else
	int i;
	unsigned int val1, val2, val3, val4;
	struct regmap *regmap = platform_mif_get_regmap(platform, DBUS_BAAW);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW begin\n");
	for (i = 0; i < DBUS_BAAW_COUNT; i++) {
        	regmap_write(regmap, dbus_baaw_offset[i][0], dbus_baaw_value[i][0] >> BAAW_BIT_SHIFT);
		regmap_read(regmap, dbus_baaw_offset[i][0], &val1);
		COMP_RET(dbus_baaw_value[i][0] >> BAAW_BIT_SHIFT, val1);

        	regmap_write(regmap, dbus_baaw_offset[i][1], dbus_baaw_value[i][1] >> BAAW_BIT_SHIFT);
		regmap_read(regmap, dbus_baaw_offset[i][1], &val2);
		COMP_RET(dbus_baaw_value[i][1] >> BAAW_BIT_SHIFT, val2);

		regmap_write(regmap, dbus_baaw_offset[i][2], platform->mem_start >> BAAW_BIT_SHIFT);
		regmap_read(regmap, dbus_baaw_offset[i][2], &val3);

		regmap_write(regmap, dbus_baaw_offset[i][3], dbus_baaw_value[i][3]);
		regmap_read(regmap, dbus_baaw_offset[i][3], &val4);
		COMP_RET(dbus_baaw_value[i][3], val4);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Updated DBUS_BAAW_%d(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
				  i, val1, val2, val3, val4);

	}
#endif
#ifdef CONFIG_SOC_S5E8845
#if IS_ENABLED(CONFIG_SCSC_MEMLOG) && IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
        if (!platform->dbus_baaw0_allowed_list_set) {
		ret = exynos_s2mpu_subsystem_set_allowlist(WLBT_SUBSYS_NAME, platform->mem_start,
							   WLBT_DBUS_BAAW_1_END - WLBT_DBUS_BAAW_1_START + 1);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "update harx BAAW0: 0x%lx, %ld %s(%d)\n",
				  platform->mem_start, dbus_baaw_value[0][1] - dbus_baaw_value[0][0] + 1,
				  ret == 0 ? "SUCCESS" : "FAIL", ret);
                platform->dbus_baaw0_allowed_list_set = 1;
        }
#endif
#endif
#if IS_ENABLED(CONFIG_SOC_S5E5535) && IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
        if (!platform->dbus_baaw0_allowed_list_set) {
		ret = exynos_s2mpu_subsystem_set_allowlist(WLBT_SUBSYS_NAME, platform->mem_start,
							   WLBT_DBUS_BAAW_0_END - WLBT_DBUS_BAAW_0_START + 1);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "update harx BAAW0: 0x%lx, %ld %s(%d)\n",
				  platform->mem_start, WLBT_DBUS_BAAW_0_END - WLBT_DBUS_BAAW_0_START + 1,
				  ret == 0 ? "SUCCESS" : "FAIL", ret);
                platform->dbus_baaw0_allowed_list_set = 1;
        }
#endif
	return 0;
}

int platform_mif_set_pbus_baaw(struct platform_mif *platform)
{
	struct regmap *regmap = platform_mif_get_regmap(platform, PBUS_BAAW);
	unsigned int val1, val2, val3, val4;
	int i;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "CBUS_BAAW begin\n");
	for (i = 0; i < PBUS_BAAW_COUNT; i++) {
        	regmap_write(regmap,
			pbus_baaw_offset[i][0],
			pbus_baaw_value[i][0] >> BAAW_BIT_SHIFT);
		regmap_read(regmap,
			pbus_baaw_offset[i][0],
			&val1);
		COMP_RET(pbus_baaw_value[i][0] >> BAAW_BIT_SHIFT, val1);

        	regmap_write(regmap,
			pbus_baaw_offset[i][1],
			pbus_baaw_value[i][1] >> BAAW_BIT_SHIFT);
		regmap_read(regmap,
			pbus_baaw_offset[i][1],
			&val2);
		COMP_RET(pbus_baaw_value[i][1] >> BAAW_BIT_SHIFT, val2);

        	regmap_write(regmap,
			pbus_baaw_offset[i][2],
			pbus_baaw_value[i][2] >> BAAW_BIT_SHIFT);
		regmap_read(regmap,
			pbus_baaw_offset[i][2],
			&val3);
		COMP_RET(pbus_baaw_value[i][2] >> BAAW_BIT_SHIFT, val3);

        	regmap_write(regmap,
			pbus_baaw_offset[i][3],
			pbus_baaw_value[i][3]);
		regmap_read(regmap,
			pbus_baaw_offset[i][3],
			&val4);
		COMP_RET(pbus_baaw_value[i][3], val4);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_%d(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  i, val1, val2, val3, val4);

	}
	return 0;
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
void platform_mif_set_memlog_baaw(struct platform_mif *platform)
{
	unsigned int val1, val2, val3, val4;
	dma_addr_t paddr = platform->paddr;
#ifdef CONFIG_SOC_S5E8845
	int ret;
#endif


#if IS_ENABLED(CONFIG_WLBT_EXYNOS_HVC)
	exynos_hvc(
		HVC_FID_SET_BAAW_WINDOW,
		1,
		((u64)(WLBT_DBUS_BAAW_1_START >> BAAW_BIT_SHIFT) << 32) | (WLBT_DBUS_BAAW_1_END >> BAAW_BIT_SHIFT),
		(paddr >> BAAW_BIT_SHIFT),
		WLBT_BAAW_ACCESS_CTRL);

	val1 = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + MEMLOG_BAAW_WLBT_START, 0, 0, 0);
	val2 = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + MEMLOG_BAAW_WLBT_END, 0, 0, 0);
	val3 = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + MEMLOG_BAAW_WLBT_REMAP, 0, 0, 0);
	val4 = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + MEMLOG_BAAW_WLBT_INIT_DONE, 0, 0, 0);
#else
	struct regmap *regmap = platform_mif_get_regmap(platform, DBUS_BAAW);

	regmap_write(regmap, MEMLOG_BAAW_WLBT_START, WLBT_DBUS_BAAW_1_START >> BAAW_BIT_SHIFT);
	regmap_write(regmap, MEMLOG_BAAW_WLBT_END, WLBT_DBUS_BAAW_1_END >> BAAW_BIT_SHIFT);
	regmap_write(regmap, MEMLOG_BAAW_WLBT_REMAP, paddr >> BAAW_BIT_SHIFT);
	regmap_write(regmap, MEMLOG_BAAW_WLBT_INIT_DONE, WLBT_BAAW_ACCESS_CTRL);

	regmap_read(regmap, BAAW1_D_WLBT_START, &val1);
	regmap_read(regmap, BAAW1_D_WLBT_END, &val2);
	regmap_read(regmap, BAAW1_D_WLBT_REMAP, &val3);
	regmap_read(regmap, BAAW1_D_WLBT_INIT_DONE, &val4);
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated DBUS_BAAW_1(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
	if (!platform->dbus_baaw1_allowed_list_set) {
		ret = exynos_s2mpu_subsystem_set_allowlist(WLBT_SUBSYS_NAME, paddr,
							   WLBT_DBUS_BAAW_1_END - WLBT_DBUS_BAAW_1_START + 1);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "update harx BAAW1: 0x%lx, %ld %s(%d)\n", paddr,
				  WLBT_DBUS_BAAW_1_END - WLBT_DBUS_BAAW_1_START + 1,
				  ret == 0 ? "SUCCESS" : "FAIL", ret);
		platform->dbus_baaw1_allowed_list_set = 1;
	}
#endif
}
#endif

void platform_wlbt_regdump(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;
	int i;
	unsigned int val;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform_wlbt_print_mailbox(platform);
	platform_wlbt_print_pmureg_register(platform);
	platform_wlbt_print_boot_cfg_register(platform);

	for (i = 0; i < NUM_MBOX_PLAT; i++) {
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(ISSR(i)));
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLAN MBOX[%d]: ISSR(%d) val = 0x%x\n", i, i, val);
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(ISSR(i)));
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WPAN MBOX[%d]: ISSR(%d) val = 0x%x\n", i, i, val);
		val = platform_mif_reg_read_pmu(platform, MAILBOX_WLBT_REG(ISSR(i)));
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PMU MBOX[%d]: ISSR(%d) val = 0x%x\n", i, i, val);
	}

#ifndef CONFIG_SOC_S5E8845
#ifndef CONFIG_SOC_S5E5535
	/* From Here, KARAM Info*/
	platform_wlbt_print_karam(platform);
#endif
#endif

	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}
