#include "platform_mif_pm_api.h"
#include "platform_mif_pcie_api.h"
#include "mif_reg.h"

#ifdef CONFIG_SCSC_BB_PAEAN
extern int exynos_acpm_write_reg(struct device_node *acpm_mfd_node, u8 sid, u16 type, u8 reg, u8 value);
#endif

#ifdef CONFIG_SCSC_BB_REDWOOD
static uint on_pinctrl_delay = 5000;
module_param(on_pinctrl_delay, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(on_pinctrl_delay, "wlbt_pinctrl on delay(us)");

static uint off_pinctrl_delay = 160;
module_param(off_pinctrl_delay, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(off_pinctrl_delay, "wlbt_pinctrl off delay(us)");
#endif

#ifdef CONFIG_SCSC_BB_PAEAN
static bool keep_powered = true;
#else
static bool keep_powered = false;
#endif
module_param(keep_powered, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(keep_powered, "Keep WLBT powered after reset, to sleep flash module");

static bool disable_apm_setup = true;
module_param(disable_apm_setup, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_apm_setup, "Disable host APM setup");

static bool enable_platform_mif_arm_reset = true;
module_param(enable_platform_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables WIFIBT ARM cores reset");

static uint scan2mem_timeout_value = 5000;
module_param(scan2mem_timeout_value, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scan2mem_timeout_value, "scan2mem_timeout(ms)");

bool platform_mif_get_scan2mem_mode(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	return pcie_mif_get_scan2mem_mode(platform->pcie);
}

void platform_mif_set_scan2mem_mode(struct scsc_mif_abs *interface, bool enable)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	pcie_mif_set_scan2mem_mode(platform->pcie, enable);
}

void platform_mif_set_s2m_dram_offset(struct scsc_mif_abs *interface, u32 offset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	pcie_set_s2m_dram_offset(platform->pcie, offset);
}

u32 platform_mif_get_s2m_size_octets(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pcie_get_s2m_size_octets(platform->pcie);
}

unsigned long platform_mif_get_mem_start(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	return platform->mem_start;
}

/* reset=0 - release from reset */
/* reset=1 - hold reset */
int platform_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long timeout;
	bool init;
	u32 ret = 0;

	if (enable_platform_mif_arm_reset) {
		if (!reset) { /* Release from reset */
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, " - reset : false\n");

			if (!platform_mif_get_scan2mem_mode(interface)) {
				if (keep_powered) {
					/* If WLBT was kept powered after reset, to allow the PMU to
					* sleep the flash module, power cycle it now to reset the HW.
					*/
					SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "power cycle WLBT\n");

					gpio_set_value(platform->pmic_gpio, 0);
					msleep(200);
				}
			}

			gpio_set_value(platform->pmic_gpio, 1);
#ifdef CONFIG_SCSC_BB_REDWOOD
			udelay(on_pinctrl_delay);
			gpio_set_value(platform->reset_gpio, 1);
#endif
			/* Start always with fw_user to one */
			platform_mif_pcie_control_fsm_prepare_fw(platform);
			platform_mif_send_event_to_fsm_wait_completion(platform, PCIE_EVT_RST_ON);
			ret = scsc_pcie_complete();
			if (ret)
				return ret;

			pcie_register_driver();

			/* We many need to wait until pcie is ready */
			timeout = jiffies + msecs_to_jiffies(500);
			do {
				init = pcie_is_initialized();
				if (init) {
					SCSC_TAG_INFO(PLAT_MIF, "PCIe has been initialized\n");
					break;
				}
			} while (time_before(jiffies, timeout));

			if (!init) {
				SCSC_TAG_INFO(PLAT_MIF, "PCIe has not been initialized\n");
				if (pcie_is_on(platform->pcie)) {
					SCSC_TAG_INFO(PLAT_MIF, "Trying to link down\n");
					platform_mif_send_event_to_fsm_wait_completion(platform, PCIE_EVT_RST_OFF);
					scsc_pcie_complete();
				}
				pcie_unregister_driver();
				return -EIO;
			}

			pcie_prepare_boot(platform->pcie);

			/* Enable L1.2 substate, l1ss is abbreviation for L1 PM Substates */
			pcie_mif_l1ss_ctrl(platform->pcie, 1);
		} else {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, " - reset : true\n");
			platform_mif_send_event_to_fsm_wait_completion(platform, PCIE_EVT_RST_OFF);
			scsc_pcie_complete();

			pcie_unregister_driver();

			if (platform_mif_get_scan2mem_mode(interface)) {
				SCSC_TAG_INFO(PLAT_MIF, "Wait for %dmsec to guarantee time for scandump\n", scan2mem_timeout_value);
				msleep(scan2mem_timeout_value);
			}

#ifdef CONFIG_SCSC_BB_REDWOOD
			gpio_set_value(platform->reset_gpio, 0);
#endif
			if (!platform_mif_get_scan2mem_mode(interface)) {
				if (!keep_powered) {
#ifdef CONFIG_SCSC_BB_REDWOOD
					udelay(off_pinctrl_delay);
#endif
					SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
							"pmic_gpio is set as low\n");
					gpio_set_value(platform->pmic_gpio, 0);
				} else {
					SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
							"pmic_gpio power preserved\n");
				}
			}
		}
	} else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Not resetting ARM Cores - enable_platform_mif_arm_reset: %d\n",
				enable_platform_mif_arm_reset);
	return ret;
}

#ifdef CONFIG_SCSC_BB_PAEAN
int platform_mif_acpm_write_reg(struct platform_mif *platform, u8 reg, u8 value)
{
	int ret = 0;
	const int wlbt_channel = 8;

	mutex_lock(&platform->acpm_lock);
	ret = exynos_acpm_write_reg(platform->np, wlbt_channel, 0, reg, value);
	mutex_unlock(&platform->acpm_lock);
	return ret;
}
#endif

#ifdef CONFIG_SCSC_BB_REDWOOD
void platform_mif_control_suspend_gpio(struct scsc_mif_abs *interface, u8 value)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u8 ret;
	SCSC_TAG_DEBUG_DEV(PLAT_MIF,platform->dev, "set suspend_gpio %d\n", value);
	gpio_set_value(platform->suspend_gpio, value);
	ret = gpio_get_value(platform->suspend_gpio);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF,platform->dev, "set suspend_gpio %d\n", ret);
}
#endif

int platform_mif_suspend(struct scsc_mif_abs *interface)
{
	int r = 0;
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (platform->suspend_handler)
		r = platform->suspend_handler(interface, platform->suspendresume_data);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "suspending platform driver\n");

	return r;
}

void platform_mif_resume(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (platform->resume_handler)
		platform->resume_handler(&platform->interface, platform->suspendresume_data);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "resuming platform driver\n");
}


