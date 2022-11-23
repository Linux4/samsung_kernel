#ifndef __LINUX_SC2730_USB_CHARGER_INCLUDED
#define __LINUX_SC2730_USB_CHARGER_INCLUDED

#include <linux/delay.h>
#include <linux/regmap.h>
#include <uapi/linux/usb/charger.h>

#define CHGR_DET_FGU_CTRL		0x1ba0
#define DP_DM_FS_ENB			BIT(14)
#define DP_DM_BC_ENB			BIT(0)

#define SC2730_CHARGE_STATUS		0x1b9c
#define BIT_CHG_DET_DONE		BIT(11)
#define BIT_SDP_INT			BIT(7)
#define BIT_DCP_INT			BIT(6)
#define BIT_CDP_INT			BIT(5)

static enum usb_charger_type sc27xx_charger_detect(struct regmap *regmap)
{
	enum usb_charger_type type;
	u32 status = 0, val;
	int ret, cnt = 10;

	do {
		ret = regmap_read(regmap, SC2730_CHARGE_STATUS, &val);
		if (ret)
			return UNKNOWN_TYPE;

		if (val & BIT_CHG_DET_DONE) {
			status = val & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);
			break;
		}

		msleep(200);
	} while (--cnt > 0);

	switch (status) {
	case BIT_CDP_INT:
		type = CDP_TYPE;
		break;
	case BIT_DCP_INT:
		type = DCP_TYPE;
		break;
	case BIT_SDP_INT:
		type = SDP_TYPE;
		break;
	default:
		type = UNKNOWN_TYPE;
	}
	/* Tab A7 Lite T618 code for AX6189DEV-126 by shixuanxuan at 20220113 start */
	pr_err("BC1.2 done!, type = %d\n", type);
	/* Tab A7 Lite T618 code for AX6189DEV-126 by shixuanxuan at 20220113 end */
	return type;
}

static void sc27xx_dpdm_switch_to_phy(struct regmap *regmap, bool enable)
{
	int ret;
	u32 val;

	pr_info(" %s enter\n", __func__);
	ret = regmap_read(regmap, CHGR_DET_FGU_CTRL, &val);
	if (ret) {
		pr_err("%s, dp/dm switch reg read failed:%d\n",
				__func__, ret);
		return;
	}

	/*
	 * bit14: 1 switch to USB phy, 0 switch to fast charger
	 * bit0 : 1 switch to USB phy, 0 switch to BC1P2
	 */
	if (enable)
		val = val | DP_DM_FS_ENB | DP_DM_BC_ENB;
	else
		val = val & ~(DP_DM_FS_ENB | DP_DM_BC_ENB);

	ret = regmap_write(regmap, CHGR_DET_FGU_CTRL, val);
	if (ret)
		pr_err("%s, dp/dm switch reg write failed:%d\n",
				__func__, ret);
	/* Tab A7 Lite T618 code for AX6189DEV-126 by shixuanxuan at 20220113 start */
	pr_err("%s enable = %d\n", __func__, enable);
  	/* Tab A7 Lite T618 code for AX6189DEV-126 by shixuanxuan at 20220113 end */
}

#endif
