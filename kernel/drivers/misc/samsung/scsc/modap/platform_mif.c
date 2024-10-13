#include "platform_mif.h"
#include "modap/platform_mif_intr_handler.h"
#include "modap/platform_mif_memory_api.h"
#include "modap/platform_mif_pm_api.h"
#include "modap/platform_mif_memlog_api.h"
#include "modap/platform_mif_regmap_api.h"
#include <linux/smc.h>

#if IS_ENABLED(CONFIG_WLBT_PROPERTY_READ)
#include "common/platform_mif_property_api.h"
#endif

#include "platform_mif_itmon_api.h"
#include "platform_mif_qos_api.h"
#include "ka_patch.h"
#include "mif_reg.h"

#include "linux/pm_qos.h"

#ifdef CONFIG_WLBT_KUNIT
#include "../kunit/kunit_platform_mif.c"
#endif

static bool fw_compiled_in_kernel;
module_param(fw_compiled_in_kernel, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fw_compiled_in_kernel, "Use FW compiled in kernel");

bool enable_hwbypass;
module_param(enable_hwbypass,bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_hwbypass, "Enables hwbypass");

static uint wlbt_status_timeout_value = 500;
module_param(wlbt_status_timeout_value, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(wlbt_status_timeout_value, "wlbt_status_timeout(ms)");

static bool enable_scandump_wlbt_status_timeout;
module_param(enable_scandump_wlbt_status_timeout, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_scandump_wlbt_status_timeout, "wlbt_status_timeout(ms)");

static bool force_to_wlbt_status_timeout;
module_param(force_to_wlbt_status_timeout, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(force_to_wlbt_status_timeout, "wlbt_status_timeout(ms)");

static bool keep_powered = true;
module_param(keep_powered, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(keep_powered, "Keep WLBT powered after reset, to sleep flash module");


extern void platform_mif_pcie_control_fsm_init(struct platform_mif *platform);
extern irqreturn_t platform_mif_set_wlbt_clk_handler(int irq, void *data);
extern void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated);
extern void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem);

#define COMP_RET(x, y) do { \
	if (x != y) {\
		pr_err("%s failed at L%d", __FUNCTION__, __LINE__); \
		return -1; \
	} \
} while (0)


static void platform_mif_destroy(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
}

static char *platform_mif_get_uid(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
	return "0";
}

static int platform_load_pmu_fw(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (platform->ka_patch_fw) {
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "ka_patch already allocated\n");
		devm_kfree(platform->dev, platform->ka_patch_fw);
	}

	platform->ka_patch_fw = devm_kzalloc(platform->dev, ka_patch_len, GFP_KERNEL);

	if (!platform->ka_patch_fw) {
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "Unable to allocate 0x%x\n", ka_patch_len);
		return -ENOMEM;
	}

	memcpy(platform->ka_patch_fw, ka_patch, ka_patch_len);
	platform->ka_patch_len = ka_patch_len;
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "load_pmu_fw sz %u done\n", ka_patch_len);
	return 0;
}

#ifdef CONFIG_SOC_S5E5515
static int platform_load_pmu_fw_flags(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len, u32 flags)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (platform->ka_patch_fw) {
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "ka_patch already allocated\n");
		devm_kfree(platform->dev, platform->ka_patch_fw);
	}

	platform->ka_patch_fw = devm_kzalloc(platform->dev, ka_patch_len, GFP_KERNEL);

	if (!platform->ka_patch_fw) {
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "Unable to allocate 0x%x\n", ka_patch_len);
		return -ENOMEM;
	}

	memcpy(platform->ka_patch_fw, ka_patch, ka_patch_len);
	*(uint *)platform->ka_patch_fw = flags;
	platform->ka_patch_len = ka_patch_len;
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "load_pmu_fw sz %u done, flags 0x%x\n", ka_patch_len, flags);
	return 0;
}
static void platform_mif_set_ldo_radio(struct scsc_mif_abs *interface, u8 radio)
{
#if (LINUX_VERSION_CODE == KERNEL_VERSION(4, 19, 0))
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct i2c_client *client;
	struct platform_device *pdev;
	struct device *dev;
	int ret;
	unsigned char val;

	pdev = platform->pdev;
	dev = &pdev->dev;
	client = to_i2c_client(dev);
	client->addr = 0x1;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Setting RADIO %s\n", radio == S615? "S615":"S620");
	if (radio == S620) {
		ret = s2mpw03_write_reg(client, 0x4B, 0x7C);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "write_reg ret : %d\n", ret);
		ret = s2mpw03_read_reg(client, 0x4B, &val);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "read_reg ret : %d, val : 0x%x\n", ret, val);
	} else if (radio == S615) {
		ret = s2mpw03_write_reg(client, 0x4B, 0x5C);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "write_reg ret : %d\n", ret);
		ret = s2mpw03_read_reg(client, 0x4B, &val);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "read_reg ret : %d, val : 0x%x\n", ret, val);
	}
#endif
}
#endif

static struct device *platform_mif_get_mif_device(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	return platform->dev;
}

static void platform_mif_irq_clear(void)
{
	/* Implement if required */
}

static int platform_mif_read_register(struct scsc_mif_abs *interface, u64 id, u32 *val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct regmap *regmap = platform_mif_get_regmap(platform, PMUREG);

	if (id == SCSC_REG_READ_WLBT_STAT) {
		regmap_read(regmap, WLBT_STAT, val);
		return 0;
	}

	return -EIO;
}

static void platform_mif_cleanup(struct scsc_mif_abs *interface)
{
}

static void platform_mif_restart(struct scsc_mif_abs *interface)
{
}


static int platform_mif_set_remap(struct platform_mif *platform)
{
	unsigned int val;
	struct regmap *regmap = platform_mif_get_regmap(platform, WLBT_REMAP);

	regmap_write(regmap,
		 WLAN_PROC_RMP_BOOT,
		(WLBT_DBUS_BAAW_0_START + platform->remap_addr_wlan) >> 12);
	regmap_read(regmap, WLAN_PROC_RMP_BOOT, &val);
	COMP_RET(((WLBT_DBUS_BAAW_0_START + platform->remap_addr_wlan) >> 12), val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLAN_REMAP: 0x%x.\n", val);

	regmap_write(regmap,
		 WPAN_PROC_RMP_BOOT,
		(WLBT_DBUS_BAAW_0_START + platform->remap_addr_wpan) >> 12);
	regmap_read(regmap, WPAN_PROC_RMP_BOOT, &val);
	COMP_RET(((WLBT_DBUS_BAAW_0_START + platform->remap_addr_wpan) >> 12), val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WPAN_REMAP: 0x%x.\n", val);

	return 0;
}

static int platform_mif_set_chip_id(struct platform_mif *platform)
{
	unsigned int id;
	unsigned int val;
	struct regmap *regmap = platform_mif_get_regmap(platform, WLBT_REMAP);

	/* CHIP_VERSION_ID - update with AP view of SOC revision */
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "CHIP_VERSION_ID begin\n");
	regmap_read(regmap, CHIP_VERSION_ID_OFFSET, &id);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Read CHIP_VERSION_ID 0x%x\n", id);

#if IS_ENABLED(CONFIG_SOC_S5E8535) || IS_ENABLED(CONFIG_SOC_S5E8835) || IS_ENABLED(CONFIG_SOC_S5E8845)
	if ((id & CHIP_VERSION_ID_IP_ID) >> CHIP_VERSION_ID_IP_ID_SHIFT == 0xA8)
                id = (id & ~CHIP_VERSION_ID_IP_ID) | (CURRENT_CHIP_IP_ID << CHIP_VERSION_ID_IP_ID_SHIFT);
#endif

	id &= ~(CHIP_VERSION_ID_IP_MAJOR | CHIP_VERSION_ID_IP_MINOR);
	id |= ((exynos_soc_info.revision << CHIP_VERSION_ID_IP_MINOR_SHIFT)
		 & (CHIP_VERSION_ID_IP_MAJOR | CHIP_VERSION_ID_IP_MINOR));
	regmap_write(regmap, CHIP_VERSION_ID_OFFSET, id);
	regmap_read(regmap, CHIP_VERSION_ID_OFFSET, &val);
	COMP_RET(id, val);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated CHIP_VERSION_ID 0x%x\n", id);

	return 0;
}

#if 0
inline void platform_mif_set_ka_patch_addr(
	struct platform_mif *platform,
	uint32_t *ka_patch_addr,
	uint32_t *ka_patch_addr_end)
{
	size_t ka_patch_len;

	if (platform->ka_patch_fw && !fw_compiled_in_kernel) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch present in FW image\n");
		ka_patch_addr = platform->ka_patch_fw;
		pr_info("[%s] -- platform->ka_patch_fw = 0x%x\n", __func__, platform->ka_patch_fw);
		pr_info("[%s] -- ka_patch_addr = 0x%x\n", __func__, ka_patch_addr);
		ka_patch_addr_end = ka_patch_addr + (platform->ka_patch_len / sizeof(uint32_t));
		ka_patch_len = platform->ka_patch_len;
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch not present in FW image. ARRAY_SIZE %d Use default\n", ARRAY_SIZE(ka_patch));
		ka_patch_addr = &ka_patch[0];
		ka_patch_addr_end = ka_patch_addr + ARRAY_SIZE(ka_patch);
		ka_patch_len = ARRAY_SIZE(ka_patch);
	}
}
#endif
static int platform_mif_set_ka_patch(
	struct platform_mif *platform,
	uint32_t *ka_patch_addr,
	uint32_t *ka_patch_addr_end)
{

	int i;
	unsigned int ka_addr = PMU_BOOT_RAM_START;
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);

#ifndef CONFIG_SOC_S5E5535
	unsigned int val;
	/* PMU boot patch */
	regmap_write(regmap, PMU_BOOT, PMU_BOOT_AP_ACC);
	regmap_read(regmap, PMU_BOOT, &val);
#if 0
	if (PMU_BOOT_AP_ACC != val) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PMU_BOOT_AP_ACC w/r error. Write : 0x%x, Read : 0x%x\n", PMU_BOOT_AP_ACC, val);
		return -1;
	}
#endif
#endif
	//platform_mif_set_ka_patch_addr(platform, ka_patch_addr, ka_patch_addr_end);

	i = 0;
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "KA patch start\n");
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch_addr : 0x%x\n", ka_patch_addr);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch_addr_end: 0x%x\n", ka_patch_addr_end);

	while (ka_patch_addr < ka_patch_addr_end) {
		regmap_write(regmap, ka_addr, *ka_patch_addr);
		ka_addr += (unsigned int)sizeof(unsigned int);
		ka_patch_addr++;
		i++;
	}
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "KA patch done\n");
	return 0;
}

static int platform_mif_set_cfg_done(struct platform_mif *platform)
{

#ifndef CONFIG_SOC_S5E5535
	unsigned int val;
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);

	/* Notify PMU of configuration done */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT PMU accesses KARAM\n");
	regmap_write(regmap, PMU_BOOT, PMU_BOOT_PMU_ACC);
	regmap_read(regmap, PMU_BOOT, &val);


        if (PMU_BOOT_PMU_ACC != val) {
                SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PMU_BOOT_PMU_ACC w/r error. Write : 0x%x, Read : 0x%x\n", PMU_BOOT_PMU_ACC, val);
                return -1;
        }
#endif

	/* WLBT FW could panic as soon as CFG_ACK is set, so change state.
	 * This allows early FW panic to be dumped.
	 */
	platform->boot_state = WLBT_BOOT_ACK_CFG_REQ;

	return 0;
}

static int platform_mif_set_cfg_ack(struct platform_mif *platform)
{
	unsigned int val;
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);

#ifdef CONFIG_SOC_S5E5535
	regmap_write(regmap, WLBT_PBUS_BOOT, PMU_BOOT_COMPLETE);
	regmap_read(regmap, WLBT_PBUS_BOOT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "lulu: Updated BOOT_CFG_ACK: 0x%x\n", val);
#else
	/* BOOT_CFG_ACK */
	regmap_write(regmap, PMU_BOOT_ACK, PMU_BOOT_COMPLETE);
	regmap_read(regmap, PMU_BOOT_ACK, &val);

        if (PMU_BOOT_COMPLETE != val) {
                SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PMU_BOOT_COMPLETE w/r error. Write : 0x%x, Read : 0x%x\n", PMU_BOOT_COMPLETE, val);
                return -1;
        }
#endif
	/* Mark as CFQ_REQ handled, so boot may continue */
	platform->boot_state = WLBT_BOOT_CFG_DONE;

	return 0;
}

bool is_ka_patch_in_fw(struct platform_mif *platform)
{
	return (platform->ka_patch_fw && !fw_compiled_in_kernel);
}
/* WARNING! this function might be
 * running in IRQ context if CONFIG_SCSC_WLBT_CFG_REQ_WQ is not defined */
void platform_set_wlbt_regs(struct platform_mif *platform)
{
#define CHECK(x) do { \
	if (x != 0) { \
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "%s failed at L%d", __FUNCTION__, __LINE__); \
		goto cfg_error; \
	} \
	else { \
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "pass L%d", __LINE__); \
	} \
} while (0)
	u64 ret64 = 0;
	const u64 EXYNOS_WLBT = 0x1;
	uint32_t *ka_patch_addr;
	uint32_t *ka_patch_addr_end;
	size_t ka_patch_len;

	if (is_ka_patch_in_fw(platform)){
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch present in FW image\n");
		ka_patch_addr = platform->ka_patch_fw;
		pr_info("[%s] -- platform->ka_patch_fw = %p\n", __func__, platform->ka_patch_fw);
		pr_info("[%s] -- ka_patch_addr = %p\n", __func__, ka_patch_addr);
		ka_patch_addr_end = ka_patch_addr + (platform->ka_patch_len / sizeof(uint32_t));
		ka_patch_len = platform->ka_patch_len;
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch not present in FW image. ARRAY_SIZE %d Use default\n", ARRAY_SIZE(ka_patch));
		ka_patch_addr = &ka_patch[0];
		ka_patch_addr_end = ka_patch_addr + ARRAY_SIZE(ka_patch);
		ka_patch_len = ARRAY_SIZE(ka_patch);
	}


	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");
	//platform_mif_set_ka_patch_addr(platform, ka_patch_addr, ka_patch_addr_end);
	//CHECK(platform_mif_set_ka_patch(platform));

	/* Set TZPC to non-secure mode */
	/* TODO : check the tz registers */

	ret64 = exynos_smc(SMC_CMD_CONN_IF, (EXYNOS_WLBT << 32) | EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret64)
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
		"Failed to set TZPC to non-secure mode: %llu\n", ret64);
	else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"SMC_CMD_CONN_IF run successfully : %llu\n", ret64);

	CHECK(platform_mif_set_remap(platform));
	CHECK(platform_mif_set_chip_id(platform));
	CHECK(platform_mif_set_dbus_baaw(platform));
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	platform_mif_set_memlog_baaw(platform);
#endif
	CHECK(platform_mif_set_pbus_baaw(platform));
	CHECK(platform_mif_set_ka_patch(platform, ka_patch_addr, ka_patch_addr_end));
	CHECK(platform_mif_set_cfg_done(platform));
	CHECK(platform_mif_set_cfg_ack(platform));

	goto done;

cfg_error:
	platform->boot_state = WLBT_BOOT_CFG_ERROR;
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "ERROR: WLBT Config failed. WLBT will not work\n");
done:
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Complete CFG_ACK\n");
	complete(&platform->cfg_ack);
	return;
}

void platform_mif_irq_interface_init(struct scsc_mif_abs *platform_if)
{
	platform_if->irq_bit_set = platform_mif_irq_bit_set;
	platform_if->irq_get = platform_mif_irq_get;
	platform_if->irq_bit_mask_status_get = platform_mif_irq_bit_mask_status_get;
	platform_if->irq_bit_clear = platform_mif_irq_bit_clear;
	platform_if->irq_bit_mask = platform_mif_irq_bit_mask;
	platform_if->irq_bit_unmask = platform_mif_irq_bit_unmask;
}

struct scsc_mif_abs *platform_mif_create(struct platform_device *pdev)
{
	struct scsc_mif_abs *platform_if;
	struct platform_mif *platform =
		(struct platform_mif *)devm_kzalloc(&pdev->dev, sizeof(struct platform_mif), GFP_KERNEL);
	int                 err = 0;

	if (!platform)
		return NULL;

	SCSC_TAG_INFO_DEV(PLAT_MIF, &pdev->dev, "Creating MIF platform device\n");

	platform_if = &platform->interface;

	/* initialise interface structure */
	platform_if->destroy = platform_mif_destroy;
	platform_if->get_uid = platform_mif_get_uid;
	platform_if->reset = platform_mif_reset;
	platform_if->wlbt_regdump = platform_wlbt_regdump;

	platform_mif_irq_api_init(platform);
	platform_mif_memory_api_init(platform_if);
	platform_mif_intr_handler_api_init(platform);
	platform_mif_memlog_init(platform);
	platform_mif_irq_interface_init(platform_if);
#if IS_ENABLED(CONFIG_WLBT_PROPERTY_READ)
	platform_mif_wlbt_property_read_init(platform_if);
#endif
	platform_if->get_mif_device = platform_mif_get_mif_device;
	platform_if->irq_clear = platform_mif_irq_clear;
	platform_if->mif_dump_registers = platform_wlbt_regdump;
	platform_if->mif_read_register = platform_mif_read_register;
	platform_if->mif_cleanup = platform_mif_cleanup;
	platform_if->mif_restart = platform_mif_restart;
	platform_if->load_pmu_fw = platform_load_pmu_fw;
#ifdef CONFIG_SOC_S5E5515
	platform_if->load_pmu_fw_flags = platform_load_pmu_fw_flags;
	platform_if->set_ldo_radio = platform_mif_set_ldo_radio;
#endif
	platform->irq_dev_pmu = NULL;
	/* LDO voltage */
	/* Reset ka_patch pointer & size */
	platform->ka_patch_fw = NULL;
	platform->ka_patch_len = 0;
	/* Update state */
	platform->pdev = pdev;
	platform->dev = &pdev->dev;
#if IS_ENABLED(CONFIG_WLBT_PROPERTY_READ)
	platform->np = pdev->dev.of_node;
#endif
	platform->regmap = (struct regmap **)devm_kzalloc(
		platform->dev,
		sizeof(struct regmap *) * REGMAP_LIST_SIZE,
		GFP_KERNEL);

#ifdef CONFIG_OF_RESERVED_MEM
	if(!sharedmem_base) {
		struct device_node * np;
		np = of_parse_phandle(platform->dev->of_node, "memory-region", 0);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "module build register sharedmem np %x\n",np);
		if(np) {
			platform->mem_start = of_reserved_mem_lookup(np)->base;
			platform->mem_size = of_reserved_mem_lookup(np)->size;
		}
	}
	else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "built-in register sharedmem \n");
		platform->mem_start = sharedmem_base;
		platform->mem_size = sharedmem_size;
	}
#else
	/* If CONFIG_OF_RESERVED_MEM is not defined, sharedmem values should be
	 * parsed from the scsc_wifibt binding
	 */
	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-base", &sharedmem_base)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_start = sharedmem_base;

	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-size", &sharedmem_size)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_size = sharedmem_size;
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "platform->mem_start 0x%lx platform->mem_size 0x%x\n",
			(uintptr_t)platform->mem_start, (u32)platform->mem_size);
	if (platform->mem_start == 0)
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "platform->mem_start is 0");

	if (platform->mem_size == 0) {
		/* We return return if mem_size is 0 as it does not make any sense.
		 * This may be an indication of an incorrect platform device binding.
		 */
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "platform->mem_size is 0");
		err = -EINVAL;
		goto error_exit;
	}

	err = platform_mif_irq_get_ioresource_mem(pdev, platform);
	if (err)
		goto error_exit;

	err = platform_mif_irq_get_ioresource_irq(pdev, platform);
	if (err)
		goto error_exit;
SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "2\n");
	err = platform_mif_set_regmap(platform);
	if (err)
		goto error_exit;
SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "1\n");
#ifdef CONFIG_SCSC_QOS
	platform_mif_qos_init(platform);
#endif

	/* Initialize spinlock */
	spin_lock_init(&platform->mif_spinlock);

#if IS_ENABLED(WLBT_ITMON_NOTIFIER)
	platform_mif_itmon_api_init(platform);
#endif

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	platform->cfgreq_workq = create_singlethread_workqueue("wlbt_cfg_reg_work");
	if (!platform->cfgreq_workq) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Error creating CFG_REQ singlethread_workqueue\n");
		err = -ENOMEM;
		goto error_exit;
	}

	INIT_WORK(&platform->cfgreq_wq, platform_cfg_req_wq);
#endif

	return platform_if;

error_exit:
	devm_kfree(&pdev->dev, platform);
	return NULL;
}

void platform_mif_destroy_platform(struct platform_device *pdev, struct scsc_mif_abs *interface)
{
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	destroy_workqueue(platform->cfgreq_workq);
#endif
}

struct platform_device *platform_mif_get_platform_dev(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	BUG_ON(!interface || !platform);

	return platform->pdev;
}


