// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "clkbuf-util.h"
#include "clkbuf-ctrl.h"
#include "clkbuf-pmif.h"

struct match_pmif {
	char *name;
	struct clkbuf_hdlr *hdlr;
	int (*init)(struct clkbuf_dts *array, struct match_pmif *match);
	struct clkbuf_dts *(*parse_dts)(struct clkbuf_dts *array,
			struct device_node *clkbuf_node, int nums);
};

static int read_with_ofs(struct clkbuf_hw *hw, struct reg_t *reg, u32 *val,
			 u32 ofs)
{
	int ret = 0;

	if (!reg)
		return -EREG_IS_NULL;

	if (!reg->mask)
		return -EREG_NO_MASK;

	*val = 0;

	switch (hw->hw_type) {
	case PMIF_M:
		(*val) = (readl(hw->base.pmif_m + reg->ofs + ofs) &
			  (reg->mask << reg->shift)) >>
			 reg->shift;
		break;
	case PMIF_P:
		(*val) = (readl(hw->base.pmif_p + reg->ofs + ofs) &
			  (reg->mask << reg->shift)) >>
			 reg->shift;
		break;

	default:
		ret = -EREG_READ_FAIL;
		break;
	}

	return ret;
}

static int write_with_ofs(struct clkbuf_hw *hw, struct reg_t *reg, u32 val,
			  u32 ofs)
{
	int ret = 0;

	if (!reg)
		return -EREG_IS_NULL;

	if (!reg->mask)
		return -EREG_NO_MASK;

	switch (hw->hw_type) {
	case PMIF_M:
		writel((readl(hw->base.pmif_m + reg->ofs + ofs) &
			(~(reg->mask << reg->shift))) |
			       (val << reg->shift),
		       hw->base.pmif_m + reg->ofs + ofs);
		break;

	case PMIF_P:
		writel((readl(hw->base.pmif_p + reg->ofs + ofs) &
			(~(reg->mask << reg->shift))) |
			       (val << reg->shift),
		       hw->base.pmif_p + reg->ofs + ofs);
		break;

	default:
		ret = -EREG_WRITE_FAIL;
		break;
	}

	return ret;
}

static int pmif_read(struct clkbuf_hw *hw, struct reg_t *reg, u32 *val)
{
	return read_with_ofs(hw, reg, val, 0);
}

static int pmif_write(struct clkbuf_hw *hw, struct reg_t *reg, u32 val)
{
	return write_with_ofs(hw, reg, val, 0);
}

static int pmif_init_v1(struct clkbuf_dts *array, struct match_pmif *match)
{
	struct clkbuf_hdlr *hdlr = match->hdlr;
	struct plat_pmifdata *pd;
	static DEFINE_SPINLOCK(lock);

	CLKBUF_DBG("array<%lx>,%s %d, id<%d>\n", (unsigned long)array, array->pmif_name,
		   array->hw.hw_type, array->pmif_id);

	pd = (struct plat_pmifdata *)(hdlr->data);
	pd->hw = array->hw;
	pd->lock = &lock;

	/* hook hdlr to array */
	array->hdlr = hdlr;
	return 0;
}

static int __set_pmif_inf(void *data, int cmd, int pmif_id, int onoff)
{
	struct plat_pmifdata *pd = (struct plat_pmifdata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct pmif_m *pmif_m;
	struct reg_t reg;
	unsigned long flags = 0;
	int ret = 0;
	spinlock_t *lock = pd->lock;

	CLKBUF_DBG("cmd: %x\n", cmd);

	spin_lock_irqsave(lock, flags);

	pmif_m = pd->pmif_m;
	if (!pmif_m) {
		ret = -EHW_NO_PM_DATA;
		goto WRITE_FAIL;
	}

	switch (cmd) {
	case SET_PMIF_CONN_INF: // = 0x0001,

		reg = pmif_m->_conn_inf_en;
		ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
		if (ret)
			goto WRITE_FAIL;
		break;

	case SET_PMIF_NFC_INF: // = 0x0002,

		reg = pmif_m->_nfc_inf_en;
		ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
		if (ret)
			goto WRITE_FAIL;
		break;

	case SET_PMIF_RC_INF: // = 0x0004,

		reg = pmif_m->_rc_inf_en;
		ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
		if (ret)
			goto WRITE_FAIL;
		break;

	default:
		ret = -EUSER_INPUT_INVALID;
		goto WRITE_FAIL;
	}

	spin_unlock_irqrestore(lock, flags);
	return ret;
WRITE_FAIL:
	spin_unlock_irqrestore(lock, flags);
	return ret;
}

static int __set_pmif_inf_v2(void *data, int cmd, int pmif_id, int onoff)
{
	struct plat_pmifdata *pd = (struct plat_pmifdata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct pmif_m *pmif_m = pd->pmif_m;
	struct pmif_p *pmif_p = pd->pmif_p;
	struct reg_t reg;
	unsigned long flags = 0;
	int ret = 0;
	spinlock_t *lock = pd->lock;

	CLKBUF_DBG("cmd: %x\n", cmd);

	spin_lock_irqsave(lock, flags);

	if (pmif_id == PMIF_M_ID) {
		if (pmif_m) {
			/*switch to PMIF_M*/
			hw.hw_type = PMIF_M;
			switch (cmd) {
			case SET_PMIF_CONN_INF: // = 0x0001,
				reg = pmif_m->_conn_inf_en;
				ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
				if (ret)
					goto V2_WRITE_FAIL;
				break;
			case SET_PMIF_NFC_INF: // = 0x0002,
				reg = pmif_m->_nfc_inf_en;
				ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
				if (ret)
					goto V2_WRITE_FAIL;
				break;
			case SET_PMIF_RC_INF: // = 0x0004,
				reg = pmif_m->_rc_inf_en;
				ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
				if (ret)
					goto V2_WRITE_FAIL;
				break;
			default:
				ret = -EUSER_INPUT_INVALID;
				goto V2_WRITE_FAIL;
			}
		} else {
			ret = -EHW_NO_PM_DATA;
			goto V2_WRITE_FAIL;
		}
	} else if (pmif_id == PMIF_P_ID) {
		if (pmif_p) {
			/*switch to PMIF_P*/
			hw.hw_type = PMIF_P;
			switch (cmd) {
			case SET_PMIF_CONN_INF: // = 0x0001,
				reg = pmif_p->_conn_inf_en;
				ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
				if (ret)
					goto V2_WRITE_FAIL;
				break;
			case SET_PMIF_NFC_INF: // = 0x0002,
				reg = pmif_p->_nfc_inf_en;
				ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
				if (ret)
					goto V2_WRITE_FAIL;
				break;
			case SET_PMIF_RC_INF: // = 0x0004,
				reg = pmif_p->_rc_inf_en;
				ret = pmif_write(&hw, &reg, (onoff == 1) ? 1 : 0);
				if (ret)
					goto V2_WRITE_FAIL;
				break;
			default:
				ret = -EUSER_INPUT_INVALID;
				goto V2_WRITE_FAIL;
			}
		} else {
			ret = -EHW_NO_PP_DATA;
			goto V2_WRITE_FAIL;
		}
	}

	spin_unlock_irqrestore(lock, flags);
	return ret;
V2_WRITE_FAIL:
	spin_unlock_irqrestore(lock, flags);
	return ret;
}

static ssize_t __dump_pmif_status(void *data, char *buf)
{
	struct plat_pmifdata *pd = (struct plat_pmifdata *)data;
	struct clkbuf_hw hw;
	struct pmif_m *pmif_m;
	struct reg_t *reg_p;
	int len = 0, i;
	u32 out;

	if (!IS_PMIF_HW((pd->hw).hw_type))
		goto DUMP_FAIL;

	if (buf) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"/***PMIF CMD usage:***/\n");
		len += snprintf(buf + len, PAGE_SIZE - len,
				"SET_PMIF_CONN_INF = 0x0001\n");
		len += snprintf(buf + len, PAGE_SIZE - len,
				"SET_PMIF_NFC_INF = 0x0002\n");
		len += snprintf(buf + len, PAGE_SIZE - len,
				"SET_PMIF_RC_INF = 0x0004\n");
		len += snprintf(buf + len, PAGE_SIZE - len,
				"echo \"[CMD(hex)] [val(hex)] [pmif_id(hex)]\" > pmif_status\n");
		len += snprintf(
			buf + len, PAGE_SIZE - len,
			"ex. echo \"0x0001 0 0\" > /sys/kernel/clkbuf/pmif_status\n");
		len += snprintf(
			buf + len, PAGE_SIZE - len,
			"===================================================\n");
	}

	hw = pd->hw;
	/*switch to PMIF_M*/
	hw.hw_type = PMIF_M;
	pmif_m = pd->pmif_m;
	if (!pmif_m) {
		CLKBUF_DBG("pmif_m is null");
		goto DUMP_FAIL;
	}

	for (i = 0; i < sizeof(struct pmif_m) / sizeof(struct reg_t); ++i) {
		if (!((((struct reg_t *)pmif_m) + i)->mask))
			continue;

		reg_p = ((struct reg_t *)pmif_m) + i;

		if (pmif_read(&hw, reg_p, &out))
			goto DUMP_FAIL;

		if (!buf)
			CLKBUF_DBG(
				"PMIF_M reg: %s Addr: 0x%08x Val: 0x%08x\n",
				reg_p->name, reg_p->ofs, out);
		else
			len += snprintf(
				buf + len, PAGE_SIZE - len,
				"PMIF_M reg: %s Addr: 0x%08x Val: 0x%08x\n",
				reg_p->name, reg_p->ofs, out);
	}
	return len;
DUMP_FAIL:
	CLKBUF_DBG("HW_TYPE is not PMIF HW or READ FAIL\n");
	return len;
}

static ssize_t __dump_pmif_status_v2(void *data, char *buf)
{
	struct plat_pmifdata *pd = (struct plat_pmifdata *)data;
	struct clkbuf_hw hw = pd->hw;
	struct pmif_m *pmif_m = pd->pmif_m;
	struct pmif_p *pmif_p = pd->pmif_p;
	struct reg_t *reg_p;
	int len = 0, i;
	u32 out;

	if (!IS_PMIF_HW((pd->hw).hw_type))
		goto V2_DUMP_FAIL;

	if (buf) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"/***PMIF CMD usage:***/\n");
		len += snprintf(buf + len, PAGE_SIZE - len,
				"SET_PMIF_CONN_INF = 0x0001\n");
		len += snprintf(buf + len, PAGE_SIZE - len,
				"SET_PMIF_NFC_INF = 0x0002\n");
		len += snprintf(buf + len, PAGE_SIZE - len,
				"SET_PMIF_RC_INF = 0x0004\n");
		len += snprintf(buf + len, PAGE_SIZE - len,
				"echo \"[CMD(hex)] [val(hex)] [pmif_id(hex)]\" > pmif_status\n");
		len += snprintf(
			buf + len, PAGE_SIZE - len,
			"ex. echo \"0x0001 0 0\" > /sys/kernel/clkbuf/pmif_status\n");
		len += snprintf(
			buf + len, PAGE_SIZE - len,
			"===================================================\n");
	}

	if (pmif_m && hw.base.pmif_m) {
		/*switch to PMIF_M*/
		hw.hw_type = PMIF_M;
		for (i = 0; i < sizeof(struct pmif_m) / sizeof(struct reg_t); ++i) {
			if (!((((struct reg_t *)pmif_m) + i)->mask))
				continue;

			reg_p = ((struct reg_t *)pmif_m) + i;

			if (pmif_read(&hw, reg_p, &out))
				goto V2_DUMP_FAIL;

			if (!buf)
				CLKBUF_DBG(
					"PMIF_M reg: %s Addr: 0x%08x Val: 0x%08x\n",
					reg_p->name, reg_p->ofs, out);
			else
				len += snprintf(
					buf + len, PAGE_SIZE - len,
					"PMIF_M reg: %s Addr: 0x%08x Val: 0x%08x\n",
					reg_p->name, reg_p->ofs, out);
		}
	} else {
		CLKBUF_DBG("pmif_m is null");
	}

	if (pmif_p && hw.base.pmif_p) {
		/*switch to PMIF_P*/
		hw.hw_type = PMIF_P;
		for (i = 0; i < sizeof(struct pmif_p) / sizeof(struct reg_t); ++i) {
			if (!((((struct reg_t *)pmif_p) + i)->mask))
				continue;

			reg_p = ((struct reg_t *)pmif_p) + i;

			if (pmif_read(&hw, reg_p, &out))
				goto V2_DUMP_FAIL;

			if (!buf)
				CLKBUF_DBG(
					"PMIF_P reg: %s Addr: 0x%08x Val: 0x%08x\n",
					reg_p->name, reg_p->ofs, out);
			else
				len += snprintf(
					buf + len, PAGE_SIZE - len,
					"PMIF_P reg: %s Addr: 0x%08x Val: 0x%08x\n",
					reg_p->name, reg_p->ofs, out);
		}
	} else {
		CLKBUF_DBG("pmif_p is null");
	}

	return len;

V2_DUMP_FAIL:
	CLKBUF_DBG("HW_TYPE is not PMIF HW or READ FAIL\n");
	return len;
}

static struct clkbuf_dts *pmif_parse_dts_v1(struct clkbuf_dts *array,
				  struct device_node *clkbuf_node, int nums)
{
	struct device_node *pmif_node;
	struct platform_device *pmif_dev;
	unsigned int num_pmif = 0, iomap_idx = 0;
	const char *comp = NULL;
	void __iomem *pmif_m_base;
	int perms = 0xffff;

	pmif_node = of_parse_phandle(clkbuf_node, "pmif", 0);

	if (!pmif_node) {
		CLKBUF_DBG("find pmif_node failed, not support PMIF\n");
		return NULL;
	}
	/*count pmif numbers, assume only PMIF_M*/
	num_pmif = 1;

	pmif_dev = of_find_device_by_node(pmif_node);
	pmif_m_base = of_iomap(pmif_node, iomap_idx++);

	/*start parsing pmif dts*/

	of_property_read_string(pmif_node, "compatible", &comp);

	array->nums = nums;
	array->num_pmif = num_pmif;
	array->comp = (char *)comp;

	/*assume only PMIF_M*/
	array->pmif_name = "PMIF_M";
	array->hw.hw_type = PMIF_M;

	array->perms = perms;
	array->pmif_id = PMIF_M_ID;
	array->hw.base.pmif_m = pmif_m_base;
	array++;

	return array;
}

static int count_pmif_elements(struct device_node *clkbuf_node)
{
	int num_pmif = 0;

	if (of_parse_phandle(clkbuf_node, "pmif", 0)) {
		// -1 to exclude pmif-phandle itself
		num_pmif = of_property_count_elems_of_size(clkbuf_node, "pmif", sizeof(u32)) - 1;
		// Only pmif phandler -> PMIF_M only
		return num_pmif ? num_pmif : 1;
	} else {
		return 0;
	}
}

static struct clkbuf_dts *pmif_parse_dts_v2(struct clkbuf_dts *array,
				  struct device_node *clkbuf_node, int nums)
{
	struct device_node *pmif_node;
	unsigned int pmif_reg_idx = 0, phandle_idx = 1;
	const char *comp = NULL;//, *pmif_name = NULL;
	void __iomem *pmif_m_base = NULL, *pmif_p_base = NULL;
	int perms = 0xffff, num_pmif;
	const char *pmif_reg_name;
	struct clkbuf_dts *pmif_m_array = NULL, *pmif_p_array = NULL;

	pmif_node = of_parse_phandle(clkbuf_node, "pmif", 0);

	num_pmif = count_pmif_elements(clkbuf_node);

	/* Check if >0 pmif_reg_idx specified */
	if (num_pmif > 0) {
		for (phandle_idx = 1; phandle_idx <= num_pmif; ++phandle_idx) {
			/* Check if pmif_reg_idx exists */
			if (of_property_read_u32_index(clkbuf_node, "pmif",
					phandle_idx, &pmif_reg_idx)) {
				CLKBUF_DBG("ERROR: Get PMIF attribute[%u] failed\n", phandle_idx);
				break;
			}

			/* Check reg-names[pmif_reg_idx] exists */
			if (of_property_read_string_index(pmif_node, "reg-names",
					pmif_reg_idx, &pmif_reg_name)) {
				CLKBUF_DBG("ERROR: Get PMIF reg-names[%u] failed\n", pmif_reg_idx);
				break;
			}

			/* Parse PMIF REG by reg-names */
			if (!strcmp(pmif_reg_name, "pmif")) {
				CLKBUF_DBG("PMIF_M dts found!");
				pmif_m_array = array;
				pmif_m_base = of_iomap(pmif_node, pmif_reg_idx);
				of_property_read_string(pmif_node, "compatible", &comp);
				array->hw.hw_type = PMIF_M;
				array->comp = (char *)comp;
				array->pmif_id = PMIF_M_ID;
				array->perms = perms;
				array->pmif_name = "PMIF_M";
				array->nums = nums;
				array->num_pmif = num_pmif;
				array++;
			} else if (!strcmp(pmif_reg_name, "pmif-p")) {
				CLKBUF_DBG("PMIF_P dts found!");
				pmif_p_array = array;
				pmif_p_base = of_iomap(pmif_node, pmif_reg_idx);
				of_property_read_string(pmif_node, "compatible", &comp);
				array->hw.hw_type = PMIF_P;
				array->comp = (char *)comp;
				array->pmif_id = PMIF_P_ID;
				array->perms = perms;
				array->pmif_name = "PMIF_P";
				array->nums = nums;
				array->num_pmif = num_pmif;
				array++;
			}
		}

		if (pmif_m_array != NULL) {
			pmif_m_array->hw.base.pmif_m = pmif_m_base;
			pmif_m_array->hw.base.pmif_p = pmif_p_base;
		}

		if (pmif_p_array != NULL) {
			pmif_p_array->hw.base.pmif_m = pmif_m_base;
			pmif_p_array->hw.base.pmif_p = pmif_p_base;
		}
	}

	return array;
}

void __spmi_dump_pmif_record(void)
{
	spmi_dump_pmif_record_reg();
}

static struct clkbuf_operation clkbuf_ops_v1 = {
	.dump_pmif_status = __dump_pmif_status,
	.set_pmif_inf = __set_pmif_inf,
#ifdef LOG_6985_SPMI_CMD
	.spmi_dump_pmif_record = __spmi_dump_pmif_record,
#endif
};

static struct clkbuf_operation clkbuf_ops_v2 = {
	.dump_pmif_status = __dump_pmif_status_v2,
	.set_pmif_inf = __set_pmif_inf_v2,
#ifdef LOG_6989_SPMI_CMD
	.spmi_dump_pmif_record = __spmi_dump_pmif_record,
#endif
};

static struct clkbuf_hdlr pmif_hdlr_v2 = {
	.ops = &clkbuf_ops_v1,
	.data = &pmif_data_v2,
};

static struct clkbuf_hdlr pmif_hdlr_v3 = {
	.ops = &clkbuf_ops_v2,
	.data = &pmif_data_v3,
};

static struct match_pmif mt6878_match_pmif = {
	.name = "mediatek,mt6878-spmi",
	.hdlr = &pmif_hdlr_v2,
	.init = &pmif_init_v1,
	.parse_dts = &pmif_parse_dts_v1,
};

static struct match_pmif mt6897_match_pmif = {
	.name = "mediatek,mt6897-spmi",
	.hdlr = &pmif_hdlr_v2,
	.init = &pmif_init_v1,
	.parse_dts = &pmif_parse_dts_v1,
};

static struct match_pmif mt6985_match_pmif = {
	.name = "mediatek,mt6985-spmi",
	.hdlr = &pmif_hdlr_v2,
	.init = &pmif_init_v1,
	.parse_dts = &pmif_parse_dts_v1,
};

static struct match_pmif mt6989_match_pmif = {
	.name = "mediatek,mt6989-spmi",
	.hdlr = &pmif_hdlr_v3,
	.init = &pmif_init_v1,
	.parse_dts = &pmif_parse_dts_v2,
};

static struct match_pmif *matches_pmif[] = {
	&mt6878_match_pmif,
	&mt6897_match_pmif,
	&mt6985_match_pmif,
	&mt6989_match_pmif,
	NULL,
};

/*use single pmif manager to manage pmif-m/pmif-p/pmif-q....*/
int count_pmif_node(struct device_node *clkbuf_node)
{
	return count_pmif_elements(clkbuf_node);
}

struct clkbuf_dts *parse_pmif_dts(struct clkbuf_dts *array,
				  struct device_node *clkbuf_node, int nums)
{
	struct match_pmif **match_pmif = matches_pmif;
	struct device_node *pmif_node;
	const char *comp = NULL;

	pmif_node = of_parse_phandle(clkbuf_node, "pmif", 0);

	if (!pmif_node) {
		CLKBUF_DBG("find \"pmif\" node failed, not support any PMIF\n");
	} else {
		if (of_property_read_string(pmif_node, "compatible", &comp)) {
			CLKBUF_DBG("find PMIF \"compatible\" property failed\n");
			goto PARSE_DTS_FAIL;
		}

		/* find match by compatible */
		for (; (*match_pmif) != NULL; match_pmif++)
			if (strcmp((*match_pmif)->name, comp) == 0)
				break;

		if (*match_pmif == NULL) {
			CLKBUF_DBG("no match pmif compatible!\n");
			goto PARSE_DTS_FAIL;
		}

		/* Parse dts with platform function */
		if ((*match_pmif)->parse_dts == NULL) {
			CLKBUF_DBG("parse_dts function not defined!\n");
			goto PARSE_DTS_FAIL;
		} else {
			return (*match_pmif)->parse_dts(array, clkbuf_node, nums);
		}
	}

PARSE_DTS_FAIL:
	return array;
}

int clkbuf_pmif_init(struct clkbuf_dts *array, struct device *dev)
{
	struct match_pmif **match_pmif = matches_pmif;
	struct clkbuf_hdlr *hdlr;
	struct clkbuf_hw hw;
	int i;
	int nums = array->nums;
	int num_pmif = 0;

	CLKBUF_DBG("\n");

	for (i = 0; i < nums; i++, array++) {
		hdlr = array->hdlr;
		hw = array->hw;
		/*only need one pmif element*/
		if (IS_PMIF_HW(hw.hw_type)) {
			num_pmif = array->num_pmif;
			break;
		}
	}

	if (!IS_PMIF_HW(array->hw.hw_type)) {
		CLKBUF_DBG("no dts pmif HW!\n");
		return -1;
	}

	/* find match by compatible */
	for (; (*match_pmif) != NULL; match_pmif++) {
		char *comp = (*match_pmif)->name;
		char *target = array->comp;

		if (strcmp(comp, target) == 0)
			break;
	}

	if (*match_pmif == NULL) {
		CLKBUF_DBG("no match pmif compatible!\n");
		return -1;
	}

	/* init flow: prepare pmif obj to specific array element*/
	for (i = 0; i < num_pmif; i++, array++) {
		char *src = array->comp;
		char *plat_target = (*match_pmif)->name;

		if (strcmp(src, plat_target) == 0)
			(*match_pmif)->init(array, *match_pmif);
	}

	CLKBUF_DBG("\n");
	return 0;
}
