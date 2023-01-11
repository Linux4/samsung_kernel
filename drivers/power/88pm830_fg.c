#include "88pm830_fg.h"
#include <linux/i2c.h>
#include <linux/regmap.h>

const int vbat_correction_table[] = {
	3300, 3500, 3700, 3900, 4100, 4350
};

const int ccnt_correction_table[] = {
	992, 996, 1000, 1004, 1006, 1010
};

#define PM800_SLPCNT_BK_LSB	(0xb4)
#define PM800_SLPCNT_BK_MSB	(0xb5)
unsigned int get_extern_data(struct pm830_battery_info *info, int flag)
{
	u8 buf[2];
	unsigned int val, data;
	struct pm80x_chip *pmic;
	static int times;

	if (!info) {
		pr_err("88pm830 device info is empty!\n");
		return 0;
	}
	pmic = info->extern_chip;

	if (!pmic) {
		dev_err(info->dev, "extern pmic handler is empty!\n");
		return 0;
	}

	switch (flag) {
	case PMIC_PWRDOWN_LOG:
		val = ((pmic->powerdown2 & 0x1f) << 8) | ((pmic->powerdown1) & 0xff);
		break;
	case PMIC_STORED_DATA:
		regmap_bulk_read(pmic->regmap, PM800_USER_DATA5, buf, 2);
		val = (buf[0] & 0xff) | ((buf[1] & 0xff) << 8);
		break;
	case PMIC_SLP_CNT:
		/* 88pm860 A0 case */
		switch (pmic->type) {
		case CHIP_PM86X:
			if (pmic->chip_id == CHIP_PM86X_ID_A0) {
				regmap_bulk_read(pmic->regmap, PM860_A0_SLP_CNT1, buf, 2);
				val = (buf[0] | ((buf[1] & 0x3) << 8));
				dev_dbg(info->dev, "sleep_counter = %d, 0x%x\n", val, val);
				/* clear sleep counter */
				regmap_update_bits(pmic->regmap, PM860_A0_SLP_CNT2,
						   PM860_A0_SLP_CNT_RST, PM860_A0_SLP_CNT_RST);
				return val;
			}
			/*
			 * only 88pm860 A0 chip has this change,
			 * so no need _else_ branch
			 */
			break;
		default:
			break;
		}
		/*
		 * the first time when system boots up
		 * we need to get information from sleep_counter backup registers
		 */
		if (!times) {
			/* power page */
			regmap_read(pmic->subchip->regmap_power, PM800_SLPCNT_BK_LSB, &data);
			val = data & 0xf;
			regmap_read(pmic->subchip->regmap_power, PM800_SLPCNT_BK_MSB, &data);
			val |= (data & 0xf) << 4;
			/* we only care about slp_cnt[11:4] */
			val <<= 3;

			times = -1;
			dev_info(info->dev, "boot up: sleep_counter = %d, 0x%x\n", val, val);
		} else {
			/* get msb of sleep_counter */
			regmap_read(pmic->regmap, PM800_RTC_MISC7, &data);

			/* we only care about slp_cnt[11:4] */
			val = data << 3;
			dev_dbg(info->dev, "%s: msb[0xe9]=0x%x, slp_cnt=%d\n", __func__, data, val);
		}

		break;
	case PMIC_VBAT_SLP:
		/* gpadc page */
		regmap_bulk_read(pmic->subchip->regmap_gpadc, PM800_VBAT_SLP, buf, 2);
		val = ((buf[0] & 0xff) << 4) | (buf[1] & 0xf);
		break;
	default:
		val = 0;
		dev_err(info->dev, "%s: case %d: unexpected case.\n", __func__, flag);
		break;
	}

	return val;
}

void set_extern_data(struct pm830_battery_info *info, int flag, int data)
{
	u8 buf[2];
	unsigned int val;
	struct pm80x_chip *pmic;

	if (!info) {
		pr_err("88pm830 device info is empty!\n");
		return;
	}

	pmic = info->extern_chip;
	if (!pmic) {
		dev_err(info->dev, "extern pmic handler is empty!\n");
		return;
	}

	switch (flag) {
	case PMIC_PWRDOWN_LOG:
		dev_err(info->dev, "no need to clear power off log.\n");

		break;
	case PMIC_STORED_DATA:
		buf[0] = data & 0xff;
		regmap_read(pmic->regmap, PM800_USER_DATA6, &val);
		buf[1] = ((data >> 8) & 0xfc) | (val & 0x3);
		regmap_bulk_write(pmic->regmap, PM800_USER_DATA5, buf, 2);

		break;
	case PMIC_SLP_CNT:
	case PMIC_VBAT_SLP:
	default:
		dev_err(info->dev, "%s: case %d: unexpected case.\n", __func__, flag);
		break;
	}
	return;
}

bool sys_is_reboot(struct pm830_battery_info *info)
{
	return !!(get_extern_data(info, PMIC_PWRDOWN_LOG) & 0x1eff);
}
