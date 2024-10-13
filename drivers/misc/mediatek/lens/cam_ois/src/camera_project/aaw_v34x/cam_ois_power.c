#include <linux/regulator/consumer.h>
#include <linux/delay.h>

#include "cam_ois_power.h"
#include "cam_ois_define.h"
#include "cam_ois_mcu_fw.h"
#include "cam_ois_common.h"

#define CAM_OIS_GPIO_MCU_BOOT_HIGH      "cam0_ois_mcu_boot_on"
#define CAM_OIS_GPIO_MCU_BOOT_LOW       "cam0_ois_mcu_boot_off"
#define CAM_OIS_GPIO_MCU_RST_HIGH       "cam0_ois_mcu_rst_1"
#define CAM_OIS_GPIO_MCU_RST_LOW        "cam0_ois_mcu_rst_0"
#define CAM_OIS_GPIO_MCU_LDO_HIGH       "cam0_ois_mcu_ldo_on"
#define CAM_OIS_GPIO_MCU_LDO_LOW        "cam0_ois_mcu_ldo_off"
#define CAM_OIS_GPIO_OIS_VDD_HIGH       "cam0_ois_vdd_ldo_on"
#define CAM_OIS_GPIO_OIS_VDD_LOW        "cam0_ois_vdd_ldo_off"
#define CAM_OIS_REGULATOR_AF            "cam0_vcamaf"
#define CAM_OIS_REGULATOR_OIS           "cam0_oisvdd"
#define CAM_OIS_IMGSENSOR_DRV_COMP      "mediatek,imgsensor"

static int cam_ois_regulator_enable(struct regulator *regulator, int voltage)
{
	int ret = 0;

	LOG_INF("E");

	if (regulator) {
		ret = regulator_set_voltage(regulator, voltage, voltage);
		if (ret)
			LOG_ERR("fail to regulator_set_voltage power level:%d", voltage);
		else
			LOG_DBG("set regulator power level:%d", voltage);

		ret = regulator_enable(regulator);
		if (ret)
			LOG_ERR("regulator_enable fail, power level: %d", voltage);

	} else {
		LOG_INF("regulator is null");
		ret = -1;
	}

	return ret;
}

static int cam_ois_regulator_disable(struct regulator *regulator)
{
	int ret = 0;

	LOG_INF("E");
	if (regulator) {
		if (regulator_is_enabled(regulator)) {
			LOG_INF("regulator is enabled");

			ret = regulator_disable(regulator);
			if (ret)
				LOG_ERR("regulator_disable fail");
		}

	} else {
		LOG_INF("regulator is null");
		ret = -1;
	}

	return ret;
}

#if ENABLE_AOIS == 0
static int cam_ois_get_gpio_pinctrl(struct pinctrl *pctrl, struct pinctrl_state **pstate,
	char *pctrl_name)
{
	int ret = 0;

	LOG_INF("pinctrl init %x %s\n", pctrl, pctrl_name);
	*pstate = pinctrl_lookup_state(pctrl, pctrl_name);
	if (IS_ERR(*pstate)) {
		LOG_INF("Failed to init (%s)\n", pctrl_name);
		ret = PTR_ERR(*pstate);
		*pstate = NULL;
	}

	if (*pstate)
		LOG_INF("sccuess to init (%s) %x\n", pctrl_name, *pstate);

	return ret;
}
#endif

int cam_ois_pinctrl_get_state_done(struct mcu_info *mcu_info)
{
	if (mcu_info->af_regulator != NULL) {
		LOG_INF("done before %x\n", mcu_info->pinctrl);
		return 0;
	}
	return -1;
}

int cam_ois_pinctrl_get_state(struct mcu_info *mcu_info)
{
	int ret = 0;
	if (!mcu_info->pinctrl) {
		mcu_info->pinctrl = devm_pinctrl_get(mcu_info->pdev);
		if (IS_ERR(mcu_info->pinctrl)) {
			LOG_INF("Failed to get pinctrl.\n");
			ret = PTR_ERR(mcu_info->pinctrl);
			mcu_info->pinctrl = NULL;
			return ret;
		}
		LOG_INF("pinctrl %x\n", mcu_info->pinctrl);
	}

	if (!cam_ois_pinctrl_get_state_done(mcu_info))
		return 0;

	mcu_info->af_regulator = regulator_get_optional(mcu_info->pdev, CAM_OIS_REGULATOR_AF);
	if (IS_ERR_OR_NULL(mcu_info->af_regulator)) {
		ret = PTR_ERR(mcu_info->af_regulator);
		LOG_ERR("fail to get AF Regulator %d", ret);
		mcu_info->af_regulator = NULL;
	}
	mcu_info->af_voltage = 2800000; //v2.8

	return ret;
}

int cam_ois_af_power_up(struct mcu_info *mcu_info)
{
	int ret = 0;

	LOG_INF("E");

	ret = cam_ois_regulator_enable(mcu_info->af_regulator, mcu_info->af_voltage);
	if (ret)
		LOG_ERR("regulator-on AF fail");
	else
		LOG_INF("regulator-on AF Success, power level %d", mcu_info->af_voltage);

	return ret;
}

int cam_ois_af_power_down(struct mcu_info *mcu_info)
{
	int ret = 0;

	ret = cam_ois_regulator_disable(mcu_info->af_regulator);
	if (ret)
		LOG_ERR("regulator-off AF fail");
	else
		LOG_INF("regulator-off AF Success");

	return ret;
}

int cam_ois_mcu_power_up(struct mcu_info *mcu_info)
{
	int ret = 0;

	LOG_INF(" - E mcu power %d", mcu_info->power_count);

	if (mcu_info->power_count > 0)
		mcu_info->power_count++;
	else if (mcu_info->power_count == 0) {
		LOG_INF("AOIS dummy");
		mcu_info->power_count++;
	}

	LOG_INF(" - X mcu power %d", mcu_info->power_count);
	return ret;
}

int cam_ois_sysfs_mcu_power_up(struct mcu_info *mcu_info)
{
	int ret = 0;

	LOG_INF(" - E mcu power %d", mcu_info->power_count);

	if (mcu_info->power_count > 0)
		mcu_info->power_count++;
	else if (mcu_info->power_count == 0) {
		LOG_INF("AOIS dummy");
		mcu_info->power_count++;
	}

	LOG_INF(" - X mcu power %d", mcu_info->power_count);
	return ret;
}

int cam_ois_mcu_power_down(struct mcu_info *mcu_info)
{
	int ret = 0;

	LOG_INF(" - E mcu power %d", mcu_info->power_count);
	if (mcu_info->power_count > 1)
		mcu_info->power_count--;
	else if (mcu_info->power_count == 1) {
		LOG_INF("AOIS dummy");
		mcu_info->power_count--;
	}

	if (mcu_info->power_count < 0)
		mcu_info->power_count = 0;

	LOG_INF(" - X mcu power %d", mcu_info->power_count);

	return ret;
}

int cam_ois_sysfs_mcu_power_down(struct mcu_info *mcu_info)
{
	int ret = 0;

	LOG_INF(" - E mcu power %d", mcu_info->power_count);
	if (mcu_info->power_count > 1)
		mcu_info->power_count--;
	else if (mcu_info->power_count == 1) {
		LOG_INF("AOIS dummy");
		mcu_info->power_count--;
	}

	if (mcu_info->power_count < 0)
		mcu_info->power_count = 0;

	LOG_INF(" - X mcu power %d", mcu_info->power_count);

	return ret;
}

void cam_ois_mcu_print_fw_version(struct mcu_info *mcu_info, char *str)
{
	if (str == NULL)
		LOG_INF("FW Ver(module %s, phone %s)", mcu_info->module_ver, mcu_info->phone_ver);
	else
		LOG_INF("%s, FW Ver(module %s, phone %s)", str, mcu_info->module_ver, mcu_info->phone_ver);
}

int cam_ois_pinctrl_init(struct mcu_info *mcu_info)
{
	int ret = 0;

	LOG_INF("E");
	LOG_INF("MCU dev %d", mcu_info->pdev);
	if (mcu_info->pdev)
		ret = cam_ois_pinctrl_get_state(mcu_info);
	else
		ret = -ENODEV;

	return ret;
}
