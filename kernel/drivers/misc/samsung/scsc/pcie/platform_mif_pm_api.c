#include "platform_mif_pm_api.h"
#include "platform_mif_pcie_api.h"
#include "mif_reg.h"

extern int exynos_acpm_write_reg(struct device_node *acpm_mfd_node, u8 sid, u16 type, u8 reg, u8 value);

static bool keep_powered = true;
module_param(keep_powered, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(keep_powered, "Keep WLBT powered after reset, to sleep flash module");

static bool disable_apm_setup = true;
module_param(disable_apm_setup, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_apm_setup, "Disable host APM setup");

static bool enable_platform_mif_arm_reset = true;
module_param(enable_platform_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables WIFIBT ARM cores reset");

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

			if (keep_powered) {
				/* If WLBT was kept powered after reset, to allow the PMU to
				 * sleep the flash module, power cycle it now to reset the HW.
				 */
				SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "power cycle WLBT\n");
				gpio_set_value(platform->pmic_gpio, 0);
				msleep(200);
			}
			gpio_set_value(platform->pmic_gpio, 1);
			/* Start always with fw_user to one */
			platform_mif_pcie_control_fsm_prepare_fw(platform);
			platform_mif_send_event_to_fsm_wait_completion(platform, PCIE_EVT_RST_ON);
			scsc_pcie_complete();

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

			if (!keep_powered) {
				SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
						"pmic_gpio is set as low\n");
				gpio_set_value(platform->pmic_gpio, 0);
			} else {
				SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
						"pmic_gpio power preserved\n");
			}
		}
	} else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Not resetting ARM Cores - enable_platform_mif_arm_reset: %d\n",
				enable_platform_mif_arm_reset);
	return ret;
}


int platform_mif_acpm_write_reg(struct platform_mif *platform, u8 reg, u8 value)
{
	int ret;
	const int wlbt_channel = 8;

	mutex_lock(&platform->acpm_lock);
	ret = exynos_acpm_write_reg(platform->np, wlbt_channel, 0, reg, value);
	mutex_unlock(&platform->acpm_lock);

	return ret;
}

int platform_mif_suspend(struct scsc_mif_abs *interface)
{
	int r = 0;
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO(PLAT_MIF, "setting SPMI register 0x0\n");
	platform_mif_acpm_write_reg(platform, 0x0, 0x0);

	if (platform->suspend_handler)
		r = platform->suspend_handler(interface, platform->suspendresume_data);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "suspending platform driver\n");

	return r;
}

void platform_mif_resume(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "resuming platform driver\n");

	SCSC_TAG_INFO(PLAT_MIF, "setting SPMI register 0x1\n");
	platform_mif_acpm_write_reg(platform, 0x0, 0x1);

}


