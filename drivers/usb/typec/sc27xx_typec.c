// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Spreadtrum SC27XX USB Type-C
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/usb/typec.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/extcon.h>
#include <linux/kernel.h>
#include <linux/nvmem-consumer.h>
#include <linux/slab.h>
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
#include <linux/power_supply.h>
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */

/* registers definitions for controller REGS_TYPEC */
#define SC27XX_EN			0x00
#define SC27XX_MODE			0x04
#define SC27XX_INT_EN			0x0c
#define SC27XX_INT_CLR			0x10
#define SC27XX_INT_RAW			0x14
#define SC27XX_INT_MASK			0x18
#define SC27XX_STATUS			0x1c
#define SC27XX_TCCDE_CNT		0x20
#define SC27XX_RTRIM			0x3c
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
#define SC27XX_DBG1 			0x60

#define SC27XX_CC_POLARITY_MASK	GENMASK(7, 0)
#define SC27XX_CC_1_DETECT		BIT(0)
#define SC27XX_CC_2_DETECT		BIT(4)
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211101 start */
#define SC27XX_OTG_CC_1_DETECT		0xf1
#define SC27XX_OTG_CC_2_DETECT		0x1f
#define SC27XX_NEWOTG_CC_1_DETECT		0xf7
#define SC27XX_NEWOTG_CC_2_DETECT		0x7f
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211101 end */
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */
/* SC27XX_TYPEC_EN */
#define SC27XX_TYPEC_USB20_ONLY		BIT(4)

/* SC27XX_TYPEC MODE */
#define SC27XX_MODE_SNK			0
#define SC27XX_MODE_SRC			1
#define SC27XX_MODE_DRP			2
#define SC27XX_MODE_MASK		3

/* SC27XX_INT_EN */
#define SC27XX_ATTACH_INT_EN		BIT(0)
#define SC27XX_DETACH_INT_EN		BIT(1)

/* SC27XX_INT_CLR */
#define SC27XX_ATTACH_INT_CLR		BIT(0)
#define SC27XX_DETACH_INT_CLR		BIT(1)

/* SC27XX_INT_MASK */
#define SC27XX_ATTACH_INT		BIT(0)
#define SC27XX_DETACH_INT		BIT(1)

#define SC27XX_STATE_MASK		GENMASK(4, 0)
#define SC27XX_EVENT_MASK		GENMASK(9, 0)

#define SC2730_EFUSE_CC1_SHIFT		5
#define SC2730_EFUSE_CC2_SHIFT		0
#define SC2721_EFUSE_CC1_SHIFT		11
#define SC2721_EFUSE_CC2_SHIFT		6
#define UMP9620_EFUSE_CC1_SHIFT		1
#define UMP9620_EFUSE_CC2_SHIFT		11

#define SC27XX_CC_MASK(n)		GENMASK((n) + 4, (n))
#define SC27XX_CC_SHIFT(n)		(n)

/* sc2721 registers definitions for controller REGS_TYPEC */
#define SC2721_EN			0x00
#define SC2721_CLR			0x04
#define SC2721_MODE			0x08

/* SC2721_INT_EN */
#define SC2721_ATTACH_INT_EN		BIT(5)
#define SC2721_DETACH_INT_EN		BIT(6)

#define SC2721_STATE_MASK		GENMASK(3, 0)
#define SC2721_EVENT_MASK		GENMASK(6, 0)

/* modify sc2730 tcc debunce */
#define SC27XX_TCC_DEBOUNCE_CNT		0xc7f

/* sc2730 registers definitions for controller REGS_TYPEC */
#define SC2730_TYPEC_PD_CFG		0x08
/* SC2730_TYPEC_PD_CFG */
#define SC27XX_VCONN_LDO_EN		BIT(13)
#define SC27XX_VCONN_LDO_RDY		BIT(12)

/* 9620 typec ate current too larger */
#define UMP9620_RESERVERED_CORE		0x234c
#define TRIM_CURRENT_FROMEFUSE		BIT(4)

/* pmic name string */
#define SC2721		0x01
#define SC2730		0x02
#define UMP9620		0x03

enum sc27xx_typec_connection_state {
	SC27XX_DETACHED_SNK,
	SC27XX_ATTACHWAIT_SNK,
	SC27XX_ATTACHED_SNK,
	SC27XX_DETACHED_SRC,
	SC27XX_ATTACHWAIT_SRC,
	SC27XX_ATTACHED_SRC,
	SC27XX_POWERED_CABLE,
	SC27XX_AUDIO_CABLE,
	SC27XX_DEBUG_CABLE,
	SC27XX_TOGGLE_SLEEP,
	SC27XX_ERR_RECOV,
	SC27XX_DISABLED,
	SC27XX_TRY_SNK,
	SC27XX_TRY_WAIT_SRC,
	SC27XX_TRY_SRC,
	SC27XX_TRY_WAIT_SNK,
	SC27XX_UNSUPOORT_ACC,
	SC27XX_ORIENTED_DEBUG,
};

/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
enum typec_cc_state {
	TYPEC_STATUS_UNKNOWN,
	TYPEC_STATUS_CC1,
	TYPEC_STATUS_CC2,
};
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */

struct sprd_typec_variant_data {
	u8 pmic_name;
	u32 efuse_cc1_shift;
	u32 efuse_cc2_shift;
	u32 int_en;
	u32 int_clr;
	u32 mode;
	u32 attach_en;
	u32 detach_en;
	u32 state_mask;
	u32 event_mask;
};

static const struct sprd_typec_variant_data sc2730_data = {
	.pmic_name = SC2730,
	.efuse_cc1_shift = SC2730_EFUSE_CC1_SHIFT,
	.efuse_cc2_shift = SC2730_EFUSE_CC2_SHIFT,
	.int_en = SC27XX_INT_EN,
	.int_clr = SC27XX_INT_CLR,
	.mode = SC27XX_MODE,
	.attach_en = SC27XX_ATTACH_INT_EN,
	.detach_en = SC27XX_DETACH_INT_EN,
	.state_mask = SC27XX_STATE_MASK,
	.event_mask = SC27XX_EVENT_MASK,
};

static const struct sprd_typec_variant_data sc2721_data = {
	.pmic_name = SC2721,
	.efuse_cc1_shift = SC2721_EFUSE_CC1_SHIFT,
	.efuse_cc2_shift = SC2721_EFUSE_CC2_SHIFT,
	.int_en = SC2721_EN,
	.int_clr = SC2721_CLR,
	.mode = SC2721_MODE,
	.attach_en = SC2721_ATTACH_INT_EN,
	.detach_en = SC2721_DETACH_INT_EN,
	.state_mask = SC2721_STATE_MASK,
	.event_mask = SC2721_EVENT_MASK,
};

static const struct sprd_typec_variant_data ump9620_data = {
	.pmic_name = UMP9620,
	.efuse_cc1_shift = UMP9620_EFUSE_CC1_SHIFT,
	.efuse_cc2_shift = UMP9620_EFUSE_CC2_SHIFT,
	.int_en = SC27XX_INT_EN,
	.int_clr = SC27XX_INT_CLR,
	.mode = SC27XX_MODE,
	.attach_en = SC27XX_ATTACH_INT_EN,
	.detach_en = SC27XX_DETACH_INT_EN,
	.state_mask = SC27XX_STATE_MASK,
	.event_mask = SC27XX_EVENT_MASK,
};

struct sc27xx_typec {
	struct device *dev;
	struct regmap *regmap;
	u32 base;
	int irq;
	struct extcon_dev *edev;
	bool usb20_only;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	u8 dr_mode;
	u8 pr_mode;
	u8 pd_swap_evt;

	enum sc27xx_typec_connection_state state;
	enum sc27xx_typec_connection_state pre_state;
	struct typec_port *port;
	struct typec_partner *partner;
	struct typec_capability typec_cap;
	const struct sprd_typec_variant_data *var_data;
	/* delayed work for handling dr swap */
	struct delayed_work swap_work;
	/* Tab A8_s code(Unisoc Patch 1816562) for AX6300SDEV-158 by wenyaqi at 20220406 start */
	struct delayed_work  typec_work;
	/* Tab A8_s code(Unisoc Patch 1816562) for AX6300SDEV-158 by wenyaqi at 20220406 end */
	bool use_pdhub_c2c;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
	struct power_supply *typec_psy;
	enum typec_cc_state cc_state;
	struct power_supply_desc psd;
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */
};
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
struct sc27xx_typec *typec_sc;
static bool pd_dr_swap_flag;

void sc27xx_set_dr_swap_flag(bool flag)
{
	pd_dr_swap_flag = flag;
}

bool sc27xx_get_dr_swap_flag(void)
{
	return pd_dr_swap_flag;
}
EXPORT_SYMBOL_GPL(sc27xx_get_dr_swap_flag);

static void sc27xx_connect_set_status_use_pdhubc2c(struct sc27xx_typec *sc)
{
	sc27xx_set_dr_swap_flag(false);
	switch (sc->state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		sc->pre_state = SC27XX_ATTACHED_SNK;
		sc->pr_mode = EXTCON_SINK;
		sc->dr_mode = EXTCON_USB;
		extcon_set_state_sync(sc->edev, EXTCON_SINK, true);
		extcon_set_state_sync(sc->edev, EXTCON_USB, true);
		dev_info(sc->dev, "%s sink connect!\n", __func__);
		break;
	case SC27XX_ATTACHED_SRC:
		sc->pre_state = SC27XX_ATTACHED_SRC;
		sc->pr_mode = EXTCON_SOURCE;
		sc->dr_mode = EXTCON_USB_HOST;
		extcon_set_state_sync(sc->edev, EXTCON_SOURCE, true);
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, true);
		dev_info(sc->dev, "%s source connect!\n", __func__);
		break;
	case SC27XX_AUDIO_CABLE:
		sc->pre_state = SC27XX_AUDIO_CABLE;
		extcon_set_state_sync(sc->edev, EXTCON_JACK_HEADPHONE, true);
		break;
	default:
		break;
	}

	return;
}

static void sc27xx_connect_set_status_no_pdhubc2c(struct sc27xx_typec *sc)
{
	switch (sc->state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		sc->pre_state = SC27XX_ATTACHED_SNK;
		extcon_set_state_sync(sc->edev, EXTCON_USB, true);
		break;
	case SC27XX_ATTACHED_SRC:
		sc->pre_state = SC27XX_ATTACHED_SRC;
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, true);
		break;
	case SC27XX_AUDIO_CABLE:
		sc->pre_state = SC27XX_AUDIO_CABLE;
		extcon_set_state_sync(sc->edev, EXTCON_JACK_HEADPHONE, true);
		break;
	default:
		break;
	}

	return;
}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
static int typec_get_property(struct power_supply *psy, enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	struct sc27xx_typec *typec_data = container_of(psy->desc, struct sc27xx_typec, psd);

	switch (psp) {
		case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
			val->intval = typec_data->cc_state;
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

static enum power_supply_property typec_props[] = {
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
};
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */

static int sc27xx_typec_connect(struct sc27xx_typec *sc, u32 status)
{
	enum typec_data_role data_role = TYPEC_DEVICE;
	enum typec_role power_role = TYPEC_SOURCE;
	enum typec_role vconn_role = TYPEC_SOURCE;
	struct typec_partner_desc desc;
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
	u32 val;
	int ret;
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */

	if (sc->partner)
		return 0;

	switch (sc->state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		power_role = TYPEC_SINK;
		data_role = TYPEC_DEVICE;
		vconn_role = TYPEC_SINK;
		break;
	case SC27XX_ATTACHED_SRC:
	case SC27XX_AUDIO_CABLE:
		power_role = TYPEC_SOURCE;
		data_role = TYPEC_HOST;
		vconn_role = TYPEC_SOURCE;
		break;
	default:
		/* Tab A8 code for AX6300DEV-1201 by qiaodan at 20211002 start */
		// break;
		return 0;
		/* Tab A8 code for AX6300DEV-1201 by qiaodan at 20211002 end */
	}

	desc.usb_pd = 0;
	/* Tab A8 code for P221116-05713  by  xuliqin at 20221123 start */
	if(sc->state == SC27XX_AUDIO_CABLE)
		desc.accessory = TYPEC_ACCESSORY_AUDIO;
	else
		desc.accessory = TYPEC_ACCESSORY_NONE;
	/* Tab A8 code for  P221116-05713   by  xuliqin at 20221123 end */
	desc.identity = NULL;

	sc->partner = typec_register_partner(sc->port, &desc);
	if (!sc->partner)
		return -ENODEV;

	typec_set_pwr_opmode(sc->port, TYPEC_PWR_MODE_USB);
	typec_set_pwr_role(sc->port, power_role);
	typec_set_data_role(sc->port, data_role);
	typec_set_vconn_role(sc->port, vconn_role);
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	if (sc->use_pdhub_c2c)
		sc27xx_connect_set_status_use_pdhubc2c(sc);
	else
		sc27xx_connect_set_status_no_pdhubc2c(sc);

/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
	ret = regmap_read(sc->regmap, sc->base + SC27XX_DBG1, &val);
	dev_info(sc->dev, "typec cc val =%x\n",val);
	if (ret < 0) {
		dev_err(sc->dev, "failed to read DBG1 register.\n");
		sc->cc_state = TYPEC_STATUS_UNKNOWN;
	} else {
		val &= SC27XX_CC_POLARITY_MASK;

		switch (val) {
		case SC27XX_CC_1_DETECT:
		/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211101 start */
		case SC27XX_OTG_CC_1_DETECT:
		case SC27XX_NEWOTG_CC_1_DETECT:
		/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211101 end */
			sc->cc_state = TYPEC_STATUS_CC1;
			break;
		case SC27XX_CC_2_DETECT:
		/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211101 start */
		case SC27XX_OTG_CC_2_DETECT:
		case SC27XX_NEWOTG_CC_2_DETECT:
		/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211101 end */
			sc->cc_state = TYPEC_STATUS_CC2;
			break;
		default:
			sc->cc_state = TYPEC_STATUS_UNKNOWN;
			break;
		}
	}
	dev_err(sc->dev, "sc->cc_state =%d\n",sc->cc_state);
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */
	return 0;
}

static void sc27xx_disconnect_set_status_use_pdhubc2c(struct sc27xx_typec *sc)
{
	sc27xx_set_dr_swap_flag(false);

	switch (sc->pre_state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
	case SC27XX_ATTACHED_SRC:
		dev_emerg(sc->dev, "%s sc->dr_mode:%d  sc->pr_mode:%d!\n",
				__func__, sc->dr_mode, sc->pr_mode);
		if (sc->dr_mode == EXTCON_USB_HOST)
			extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, false);
		else if (sc->dr_mode == EXTCON_USB)
			extcon_set_state_sync(sc->edev, EXTCON_USB, false);

		sc->dr_mode = EXTCON_NONE;

		if (sc->pr_mode == EXTCON_SOURCE)
			extcon_set_state_sync(sc->edev, EXTCON_SOURCE, false);
		else if (sc->pr_mode == EXTCON_SINK)
			extcon_set_state_sync(sc->edev, EXTCON_SINK, false);

		sc->pr_mode = EXTCON_NONE;
		break;
	case SC27XX_AUDIO_CABLE:
		extcon_set_state_sync(sc->edev, EXTCON_JACK_HEADPHONE, false);
		break;
	default:
		break;
	}
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */

	return;
}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
static void sc27xx_disconnect_set_status_no_pdhubc2c(struct sc27xx_typec *sc)
{
	switch (sc->pre_state) {
	case SC27XX_ATTACHED_SNK:
	case SC27XX_DEBUG_CABLE:
		extcon_set_state_sync(sc->edev, EXTCON_USB, false);
		break;
	case SC27XX_ATTACHED_SRC:
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, false);
		break;
	case SC27XX_AUDIO_CABLE:
		extcon_set_state_sync(sc->edev, EXTCON_JACK_HEADPHONE, false);
		break;
	default:
		break;
	}
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	return;
}

static void sc27xx_typec_disconnect(struct sc27xx_typec *sc, u32 status)
{
	typec_unregister_partner(sc->partner);
	sc->partner = NULL;
	typec_set_pwr_opmode(sc->port, TYPEC_PWR_MODE_USB);
	typec_set_pwr_role(sc->port, TYPEC_SINK);
	typec_set_data_role(sc->port, TYPEC_DEVICE);
	typec_set_vconn_role(sc->port, TYPEC_SINK);
	sc->pd_swap_evt = TYPEC_NO_SWAP;

	if (sc->use_pdhub_c2c)
		sc27xx_disconnect_set_status_use_pdhubc2c(sc);
	else
		sc27xx_disconnect_set_status_no_pdhubc2c(sc);
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
	sc->cc_state = TYPEC_STATUS_UNKNOWN;
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */
	return;
}

static int sc27xx_typec_dr_set(const struct typec_capability *cap,
				enum typec_data_role role)
{
	/* TODO: Data role set */
	return 0;
}

static int sc27xx_typec_pr_set(const struct typec_capability *cap,
				enum typec_role role)
{
	/* TODO: Power role set */
	return 0;
}

static int sc27xx_typec_vconn_set(const struct typec_capability *cap,
				enum typec_role role)
{
	/* TODO: Vconn set */
	return 0;
}

static irqreturn_t sc27xx_typec_interrupt(int irq, void *data)
{
	struct sc27xx_typec *sc = data;
	u32 event;
	int ret;

	ret = regmap_read(sc->regmap, sc->base + SC27XX_INT_MASK, &event);
	if (ret)
		return ret;

	event &= sc->var_data->event_mask;

	ret = regmap_read(sc->regmap, sc->base + SC27XX_STATUS, &sc->state);
	if (ret)
		goto clear_ints;

	sc->state &= sc->var_data->state_mask;

	if (event & SC27XX_ATTACH_INT) {
		ret = sc27xx_typec_connect(sc, sc->state);
		if (ret)
			dev_warn(sc->dev, "failed to register partner\n");
	} else if (event & SC27XX_DETACH_INT) {
		sc27xx_typec_disconnect(sc, sc->state);
	}

clear_ints:
	regmap_write(sc->regmap, sc->base + sc->var_data->int_clr, event);

	dev_emerg(sc->dev, "now works as DRP and is in %d state, event %d\n",
		sc->state, event);
	return IRQ_HANDLED;
}

static int sc27xx_typec_enable(struct sc27xx_typec *sc)
{
	int ret;
	u32 val;

	/* Set typec mode */
	ret = regmap_read(sc->regmap, sc->base + sc->var_data->mode, &val);
	if (ret)
		return ret;

	val &= ~SC27XX_MODE_MASK;
	switch (sc->typec_cap.type) {
	case TYPEC_PORT_SRC:
		val |= SC27XX_MODE_SRC;
		break;
	case TYPEC_PORT_SNK:
		val |= SC27XX_MODE_SNK;
		break;
	case TYPEC_PORT_DRP:
		val |= SC27XX_MODE_DRP;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_write(sc->regmap, sc->base + sc->var_data->mode, val);
	if (ret)
		return ret;

	/* typec USB20 only flag, only work in snk mode */
	if (sc->typec_cap.data == TYPEC_PORT_UFP && sc->usb20_only) {
		ret = regmap_update_bits(sc->regmap, sc->base + SC27XX_EN,
					 SC27XX_TYPEC_USB20_ONLY,
					 SC27XX_TYPEC_USB20_ONLY);
		if (ret)
			return ret;
	}

	/* modify sc2730 tcc debounce to 100ms while PD signal occur at 150ms
	 * and effect tccde reginize.Reason is hardware signal and clk not
	 * accurate.
	 */
	if (sc->var_data->efuse_cc2_shift == SC2730_EFUSE_CC2_SHIFT) {
		ret = regmap_write(sc->regmap, sc->base + SC27XX_TCCDE_CNT,
				SC27XX_TCC_DEBOUNCE_CNT);
		if (ret)
			return ret;
	}

	if (sc->var_data->pmic_name == UMP9620) {
		ret = regmap_read(sc->regmap, UMP9620_RESERVERED_CORE, &val);
		if (ret)
			return ret;

		val |= TRIM_CURRENT_FROMEFUSE;
		regmap_write(sc->regmap, UMP9620_RESERVERED_CORE, val);
	}

	/* Enable typec interrupt and enable typec */
	ret = regmap_read(sc->regmap, sc->base + sc->var_data->int_en, &val);
	if (ret)
		return ret;

	val |= sc->var_data->attach_en | sc->var_data->detach_en;
	return regmap_write(sc->regmap, sc->base + sc->var_data->int_en, val);
}

static const u32 sc27xx_typec_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	EXTCON_SOURCE,
	EXTCON_SINK,
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	EXTCON_JACK_HEADPHONE,
	EXTCON_NONE,
};

static int sc27xx_typec_get_cc1_efuse(struct sc27xx_typec *sc)
{
	struct nvmem_cell *cell;
	u32 calib_data = 0;
	void *buf;
	size_t len = 0;

	cell = nvmem_cell_get(sc->dev, "typec_cc1_cal");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	calib_data = (calib_data & SC27XX_CC_MASK(sc->var_data->efuse_cc1_shift))
			>> SC27XX_CC_SHIFT(sc->var_data->efuse_cc1_shift);
	kfree(buf);

	return calib_data;
}

static int sc27xx_typec_get_cc2_efuse(struct sc27xx_typec *sc)
{
	struct nvmem_cell *cell;
	u32 calib_data = 0;
	void *buf;
	size_t len = 0;

	cell = nvmem_cell_get(sc->dev, "typec_cc2_cal");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	calib_data = (calib_data & SC27XX_CC_MASK(sc->var_data->efuse_cc2_shift))
			>> SC27XX_CC_SHIFT(sc->var_data->efuse_cc2_shift);
	kfree(buf);

	return calib_data;
}

static int typec_set_rtrim(struct sc27xx_typec *sc)
{
	int calib_cc1;
	int calib_cc2;
	u32 rtrim;

	calib_cc1 = sc27xx_typec_get_cc1_efuse(sc);
	if (calib_cc1 < 0)
		return calib_cc1;
	calib_cc2 = sc27xx_typec_get_cc2_efuse(sc);
	if (calib_cc2 < 0)
		return calib_cc2;

	rtrim = calib_cc1 | calib_cc2<<5;

	return regmap_write(sc->regmap, sc->base + SC27XX_RTRIM, rtrim);
}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
static void sc27xx_typec_work(struct work_struct *work)
{
	struct sc27xx_typec *sc = typec_sc;
	u32 event;
	int ret;

	/* pd swap,cc need debounce to report interrupt */
	/* Tab A8_s code(Unisoc Patch 1816562) for AX6300SDEV-158 by wenyaqi at 20220406 start */
	// msleep(200);
	/* Tab A8_s code(Unisoc Patch 1816562) for AX6300SDEV-158 by wenyaqi at 20220406 end */

	ret = regmap_read(sc->regmap, sc->base + SC27XX_INT_RAW, &event);
	if (ret) {
		return;
	}

	event &= sc->var_data->event_mask;

	dev_info(sc->dev, "%s SC27XX_INT_RAW:0x%x!\n", __func__, event);

	regmap_write(sc->regmap, sc->base + sc->var_data->int_clr, event);

	ret = regmap_read(sc->regmap, sc->base + SC27XX_INT_RAW, &event);
	if (ret) {
		return;
	}

	event &= sc->var_data->event_mask;
	dev_info(sc->dev, "%s SC27XX_INT_RAW:0x%x!\n", __func__, event);
	sc27xx_set_typec_int_enable();
 }
 /* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */

static void sc27xx_typec_swap_work(struct work_struct *work)
{
	struct sc27xx_typec *sc = typec_sc;

	dev_info(sc->dev, "%s pd_swap_evt:%d!\n", __func__, sc->pd_swap_evt);

	switch (sc->pd_swap_evt) {
	case TYPEC_HOST_TO_DEVICE:
		sc27xx_set_dr_swap_flag(true);
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, false);
		extcon_set_state_sync(sc->edev, EXTCON_USB, true);
		typec_set_data_role(sc->port, TYPEC_DEVICE);
		break;
	case TYPEC_DEVICE_TO_HOST:
		sc27xx_set_dr_swap_flag(true);
		extcon_set_state_sync(sc->edev, EXTCON_USB, false);
		extcon_set_state_sync(sc->edev, EXTCON_USB_HOST, true);
		typec_set_data_role(sc->port, TYPEC_HOST);
		break;
	default:
		return;
	}
}


static int sc27xx_typec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct sc27xx_typec *sc;
	const struct sprd_typec_variant_data *pdata;
	int mode, ret;

	pdata = of_device_get_match_data(dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	sc = devm_kzalloc(&pdev->dev, sizeof(*sc), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->edev = devm_extcon_dev_allocate(&pdev->dev, sc27xx_typec_cable);
	if (IS_ERR(sc->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return PTR_ERR(sc->edev);
	}

	ret = devm_extcon_dev_register(&pdev->dev, sc->edev);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't register extcon device: %d\n", ret);
		return ret;
	}

	sc->dev = &pdev->dev;
	sc->irq = platform_get_irq(pdev, 0);
	if (sc->irq < 0) {
		dev_err(sc->dev, "failed to get typec interrupt.\n");
		return sc->irq;
	}

	sc->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!sc->regmap) {
		dev_err(sc->dev, "failed to get regmap.\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(node, "reg", &sc->base);
	if (ret) {
		dev_err(dev, "failed to get reg offset!\n");
		return ret;
	}

	ret = of_property_read_u32(node, "mode", &mode);
	if (ret) {
		dev_err(dev, "failed to get typec port mode type\n");
		return ret;
	}

	sc->use_pdhub_c2c = of_property_read_bool(node, "use_pdhub_c2c");

	if (mode < TYPEC_PORT_DFP || mode > TYPEC_PORT_DRP
	    || mode == TYPEC_PORT_UFP) {
		mode = TYPEC_PORT_UFP;
		sc->usb20_only = true;
		dev_info(dev, "usb 2.0 only is enabled\n");
	}

	sc->var_data = pdata;
	sc->typec_cap.type = mode;
	sc->typec_cap.data = TYPEC_PORT_DRD;
	sc->typec_cap.dr_set = sc27xx_typec_dr_set;
	sc->typec_cap.pr_set = sc27xx_typec_pr_set;
	sc->typec_cap.vconn_set = sc27xx_typec_vconn_set;
	sc->typec_cap.revision = USB_TYPEC_REV_1_2;
	sc->typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
	sc->port = typec_register_port(&pdev->dev, &sc->typec_cap);
	if (!sc->port) {
		dev_err(sc->dev, "failed to register port!\n");
		return -ENODEV;
	}

/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
	sc->psd.name = "typec-sc27xx";
	sc->psd.type = POWER_SUPPLY_TYPE_USB_TYPE_C;
	sc->psd.properties = typec_props;
	sc->psd.num_properties = ARRAY_SIZE(typec_props);
	sc->psd.get_property = typec_get_property;
	sc->typec_psy = devm_power_supply_register(&pdev->dev, &sc->psd, NULL);

	if (IS_ERR(sc->typec_psy)) {
		dev_err(&pdev->dev, "Cannot register sc->typec_psy with name \"%s\"\n",
			sc->psd.name);
		return PTR_ERR(sc->typec_psy);
	}
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
	typec_sc = sc;
	pd_dr_swap_flag = false;
	/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
	ret = typec_set_rtrim(sc);
	if (ret < 0) {
		dev_err(sc->dev, "failed to set typec rtrim %d\n", ret);
		goto error;
	}

	ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
					sc27xx_typec_interrupt,
					IRQF_EARLY_RESUME | IRQF_ONESHOT,
					dev_name(sc->dev), sc);
	if (ret) {
		dev_err(sc->dev, "failed to request irq %d\n", ret);
		goto error;
	}

	ret = sc27xx_typec_enable(sc);
	if (ret)
		goto error;

	sc->pd_swap_evt = TYPEC_NO_SWAP;
	INIT_DELAYED_WORK(&sc->swap_work, sc27xx_typec_swap_work);

	/* Tab A8_s code(Unisoc Patch 1816562) for AX6300SDEV-158|AX6300DEV-2368 by wenyaqi at 20220406 start */
	INIT_DELAYED_WORK(&sc->typec_work, sc27xx_typec_work);
	/* Tab A8_s code(Unisoc Patch 1816562) for AX6300SDEV-158|AX6300DEV-2368 by wenyaqi at 20220406 end */
	platform_set_drvdata(pdev, sc);
	return 0;

error:
	typec_unregister_port(sc->port);
	return ret;
}

static int sc27xx_typec_remove(struct platform_device *pdev)
{
	struct sc27xx_typec *sc = platform_get_drvdata(pdev);

	sc27xx_typec_disconnect(sc, 0);
	typec_unregister_port(sc->port);

	return 0;
}
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 start */
int sc27xx_set_typec_int_clear(void)
{
	struct sc27xx_typec *sc = typec_sc;
	u32 event;
	int ret;

	ret = regmap_read(sc->regmap, sc->base + SC27XX_INT_RAW, &event);
	if (ret) {
		return ret;
	}

	event &= sc->var_data->event_mask;

	dev_info(sc->dev, "%s SC27XX_INT_RAW:0x%x!\n", __func__, event);

	regmap_write(sc->regmap, sc->base + sc->var_data->int_clr, event);

	/* Tab A8_s code(Unisoc Patch 1816562) for AX6300SDEV-158 by wenyaqi at 20220406 start */
	/* pd swap,cc need debounce to report interrupt */
	queue_delayed_work(system_unbound_wq, &sc->typec_work, msecs_to_jiffies(200));
	/* Tab A8_s code(Unisoc Patch 1816562) for AX6300SDEV-158 by wenyaqi at 20220406 end */

	return 0;
}
EXPORT_SYMBOL_GPL(sc27xx_set_typec_int_clear);

int sc27xx_set_typec_int_disable(void)
{
	struct sc27xx_typec *sc = typec_sc;
	int ret = 0;
	u32 val;

	dev_info(sc->dev, "%s entered!\n", __func__);
	/* Tab A8_s code(Unisoc Patch 1816562) for AX6300SDEV-158 by wenyaqi at 20220406 start */
	if (!cancel_delayed_work_sync(&sc->typec_work)) {
		ret = regmap_read(sc->regmap,
				sc->base + sc->var_data->int_en, &val);
		if (ret)
			return ret;

		ret = val & (sc->var_data->attach_en | sc->var_data->detach_en);
		if (!ret)
			msleep(20);
	}
	/* Tab A8_s code(Unisoc Patch 1816562) for AX6300SDEV-158 by wenyaqi at 20220406 end */

	/* disable typec interrupt for attach/detach */
	ret = regmap_read(sc->regmap, sc->base + sc->var_data->int_en, &val);
	if (ret) {
		return ret;
	}

	val &= ~(sc->var_data->attach_en | sc->var_data->detach_en);
	return regmap_write(sc->regmap, sc->base + sc->var_data->int_en, val);
}
EXPORT_SYMBOL_GPL(sc27xx_set_typec_int_disable);

int sc27xx_set_typec_int_enable(void)
{
	struct sc27xx_typec *sc = typec_sc;
	int ret = 0;
	u32 val;

	dev_info(sc->dev, "%s entered!\n", __func__);

	/* Enable typec interrupt and enable typec */
	ret = regmap_read(sc->regmap, sc->base + sc->var_data->int_en, &val);
	if (ret) {
		return ret;
	}

	val |= sc->var_data->attach_en | sc->var_data->detach_en;
	return regmap_write(sc->regmap, sc->base + sc->var_data->int_en, val);

}
EXPORT_SYMBOL_GPL(sc27xx_set_typec_int_enable);

/*
 *  TYPEC_DEVICE_HOST	device->host
 *  TYPEC_HOST_DEVICE	host->device
 */
int sc27xx_typec_set_pd_dr_swap_flag(u8 flag)
{
	struct sc27xx_typec *sc = typec_sc;

	dev_info(sc->dev, "%s entered!\n", __func__);

	if (flag == TYPEC_HOST_TO_DEVICE) {
		sc->dr_mode = EXTCON_USB;
	} else if (flag == TYPEC_DEVICE_TO_HOST) {
		sc->dr_mode = EXTCON_USB_HOST;
	} else {
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sc27xx_typec_set_pd_dr_swap_flag);

/*
 * SINK_TO_SOURCE	sink->source
 * SOURCE_TO_SINK	source->sink
 */
int sc27xx_typec_set_pr_swap_flag(u8 flag)
{
	struct sc27xx_typec *sc = typec_sc;

	dev_info(sc->dev, "%s flag:%d!\n", __func__, flag);

	if (flag == TYPEC_SOURCE_TO_SINK) {
		sc->pr_mode = EXTCON_SINK;
	} else if (flag == TYPEC_SINK_TO_SOURCE) {
		sc->pr_mode = EXTCON_SOURCE;
	} else {
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sc27xx_typec_set_pr_swap_flag);

/*
 * if pd pr/dr swap ok,send extcon event by set api.
 */
int sc27xx_typec_set_pd_swap_event(u8 pd_swap_flag)
{
	struct sc27xx_typec *sc = typec_sc;

	dev_info(sc->dev, "%s pd_swap_flag:%d!\n", __func__, pd_swap_flag);

	switch (pd_swap_flag) {
	case TYPEC_SOURCE_TO_SINK:
		extcon_set_state_sync(sc->edev, EXTCON_SOURCE, false);
		extcon_set_state_sync(sc->edev, EXTCON_SINK, true);
		typec_set_pwr_role(sc->port, TYPEC_SINK);
		break;
	case TYPEC_SINK_TO_SOURCE:
		extcon_set_state_sync(sc->edev, EXTCON_SINK, false);
		extcon_set_state_sync(sc->edev, EXTCON_SOURCE, true);
		typec_set_pwr_role(sc->port, TYPEC_SOURCE);
		break;
	case TYPEC_HOST_TO_DEVICE:
	case TYPEC_DEVICE_TO_HOST:
		if (pd_swap_flag != sc->pd_swap_evt) {
			sc->pd_swap_evt = pd_swap_flag;
			/* delay one jiffies for scheduling purpose */
			schedule_delayed_work(&sc->swap_work,
						msecs_to_jiffies(5));
		} else {
			dev_err(sc->dev, "%s igore pd_swap_flag:%d!\n",
						__func__, pd_swap_flag);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sc27xx_typec_set_pd_swap_event);
/* Tab A8 code for AX6300DEV-2368 by qiaodan at 20211028 end */
static const struct of_device_id typec_sprd_match[] = {
	{.compatible = "sprd,sc2730-typec", .data = &sc2730_data},
	{.compatible = "sprd,sc2721-typec", .data = &sc2721_data},
	{.compatible = "sprd,ump96xx-typec", .data = &ump9620_data},
	{},
};
MODULE_DEVICE_TABLE(of, typec_sprd_match);

static struct platform_driver sc27xx_typec_driver = {
	.probe = sc27xx_typec_probe,
	.remove = sc27xx_typec_remove,
	.driver = {
		.name = "sc27xx-typec",
		.of_match_table = typec_sprd_match,
	},
};

static int __init sc27xx_typec_init(void)
{
	return platform_driver_register(&sc27xx_typec_driver);
}

static void __exit sc27xx_typec_exit(void)
{
	platform_driver_unregister(&sc27xx_typec_driver);
}

module_init(sc27xx_typec_init);
module_exit(sc27xx_typec_exit);

MODULE_LICENSE("GPL v2");

