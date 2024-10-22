// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include "mtk_battery_oc_throttling.h"
#include "mtk_low_battery_throttling.h"

#define CREATE_TRACE_POINTS
#include "mtk_peak_power_budget_trace.h"

#define STR_SIZE 512
#define MAX_VALUE 0x7FFF
#define MAX_POWER_DRAM 4000
#define MAX_POWER_DISPLAY 2000
#define SOC_ERROR 3000
#define BAT_PATH_DEFAULT_RDC 100
#define BAT_PATH_DEFAULT_RAC 50

#define NORMAL_BOOT_ID 0

static bool mt_ppb_debug;
static spinlock_t ppb_lock;
void __iomem *ppb_sram_base;
struct fg_cus_data fg_data;
struct power_budget_t pb;
static struct notifier_block ppb_nb;

struct ppb_ctrl ppb_ctrl = {
	.ppb_stop = 0,
	.ppb_drv_done = 0,
	.manual_mode = 0,
	.ppb_mode = 1,
};

struct ppb ppb = {
	.loading_flash = 0,
	.loading_audio = 0,
	.loading_camera = 0,
	.loading_display = MAX_POWER_DISPLAY,
	.loading_apu = 0,
	.loading_dram = MAX_POWER_DRAM,
	.vsys_budget = 0,
	.cg_budget_thd = 0,
	.cg_budget_cnt = 0,
};

struct ppb ppb_manual = {
	.loading_flash = 0,
	.loading_audio = 0,
	.loading_camera = 0,
	.loading_display = 0,
	.loading_apu = 0,
	.loading_dram = 0,
	.vsys_budget = 0,
	.cg_budget_thd = 0,
	.cg_budget_cnt = 0,
};

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

static int ppb_read_sram(int offset)
{
	void __iomem *addr = ppb_sram_base + offset * 4;

	return readl(addr);
}

static void ppb_write_sram(unsigned int val, int offset)
{
	writel(val, (void __iomem *)(ppb_sram_base + offset * 4));
}

static void ppb_allocate_budget_manager(void)
{
	int vsys_budget = 0, remain_budget = 0;
	int flash, audio, camera, display, apu, dram;

	if (ppb_ctrl.manual_mode == 1) {
		flash = ppb_manual.loading_flash;
		audio = ppb_manual.loading_audio;
		camera = ppb_manual.loading_camera;
		display = ppb_manual.loading_display;
		apu = ppb_manual.loading_apu;
		dram = ppb_manual.loading_dram;
		vsys_budget = ppb_manual.vsys_budget;
		remain_budget = vsys_budget - (flash + audio + camera + display + dram);
		remain_budget = (remain_budget > 0) ? remain_budget : 0;
		ppb_manual.remain_budget = remain_budget;
	} else {
		flash = ppb.loading_flash;
		audio = ppb.loading_audio;
		camera = ppb.loading_camera;
		display = ppb.loading_display;
		apu = ppb.loading_apu;
		dram = ppb.loading_dram;
		vsys_budget = ppb.vsys_budget;
		remain_budget = vsys_budget - (flash + audio + camera + display + dram);
		remain_budget = (remain_budget > 0) ? remain_budget : 0;
		ppb.remain_budget = remain_budget;
	}

	ppb_write_sram(remain_budget, PPB_VSYS_PWR);
	ppb_write_sram(flash, PPB_FLASH_PWR);
	ppb_write_sram(audio, PPB_AUDIO_PWR);
	ppb_write_sram(camera, PPB_CAMERA_PWR);
	ppb_write_sram(apu, PPB_APU_PWR);
	ppb_write_sram(display, PPB_DISPLAY_PWR);
	ppb_write_sram(dram, PPB_DRAM_PWR);
	ppb_write_sram(0, PPB_APU_PWR_ACK);

	if (mt_ppb_debug)
		pr_info("(S_BGT/R_BGT)=%u,%u (FLASH/AUD/CAM/DISP/APU/DRAM)=%u,%u,%u,%u,%u,%u\n",
			vsys_budget, remain_budget, flash, audio, camera, display, apu, dram);
	trace_peak_power_budget(&ppb);
}

static bool ppb_func_enable_check(void)
{
	if (!ppb_ctrl.ppb_drv_done)
		return false;

	return true;
}

static bool ppb_update_table_info(enum ppb_kicker kicker, struct ppb *req_ppb)
{
	bool is_update = false;

	switch (kicker) {
	case KR_BUDGET:
		if (ppb.vsys_budget != req_ppb->vsys_budget) {
			ppb.vsys_budget = req_ppb->vsys_budget;
			is_update = true;
		}
		break;
	case KR_FLASHLIGHT:
		if (ppb.loading_flash != req_ppb->loading_flash) {
			ppb.loading_flash = req_ppb->loading_flash;
			is_update = true;
		}
		break;
	case KR_AUDIO:
		if (ppb.loading_audio != req_ppb->loading_audio) {
			ppb.loading_audio = req_ppb->loading_audio;
			is_update = true;
		}
		break;
	case KR_CAMERA:
		if (ppb.loading_camera != req_ppb->loading_camera) {
			ppb.loading_camera = req_ppb->loading_camera;
			is_update = true;
		}
		break;
	case KR_DISPLAY:
		if (ppb.loading_display != req_ppb->loading_display) {
			ppb.loading_display = req_ppb->loading_display;
			is_update = true;
		}
		break;
	case KR_APU:
		if (ppb.loading_apu != req_ppb->loading_apu) {
			ppb.loading_apu = req_ppb->loading_apu;
			is_update = true;
		}
		break;
	default:
		pr_info("[%s] ERROR, unknown kicker [%d]\n", __func__, kicker);
		WARN_ON_ONCE(1);
		break;
	}

	return is_update;
}

static void mtk_power_budget_manager(enum ppb_kicker kicker, struct ppb *req_ppb)
{
	bool ppb_enable = false;
	bool ppb_update = false;
	unsigned long flags;

	ppb_enable = ppb_func_enable_check();
	if (!ppb_enable)
		return;

	ppb_update = ppb_update_table_info(kicker, req_ppb);

	if (ppb_ctrl.ppb_stop)
		return;

	if (!ppb_update)
		return;

	spin_lock_irqsave(&ppb_lock, flags);
	ppb_allocate_budget_manager();
	spin_unlock_irqrestore(&ppb_lock, flags);
}

void kicker_ppb_request_power(enum ppb_kicker kicker, unsigned int power)
{
	bool ppb_enable = false;
	struct ppb ppb = {0};

	ppb_enable = ppb_func_enable_check();
	if (!ppb_enable)
		return;

	switch (kicker) {
	case KR_BUDGET:
		ppb.vsys_budget = power;
		break;
	case KR_FLASHLIGHT:
		ppb.loading_flash = power;
		break;
	case KR_AUDIO:
		ppb.loading_audio = power;
		break;
	case KR_CAMERA:
		ppb.loading_camera = power;
		break;
	case KR_DISPLAY:
		ppb.loading_display = power;
		break;
	case KR_APU:
		ppb.loading_apu = power;
		break;
	default:
		pr_info("[%s] ERROR, unknown kicker [%d]\n", __func__, kicker);
		break;
	}

	mtk_power_budget_manager(kicker, &ppb);
}
EXPORT_SYMBOL(kicker_ppb_request_power);


static int read_dts_val(const struct device_node *np, const char *name, int *param, int unit)
{
	static unsigned int val;

	if (!of_property_read_u32(np, name, &val))
		*param = (int)val * unit;
	else {
		pr_info("Get %s no data !!!\n", name);
		return -1;
	}
	return 0;
}

static int read_dts_val_by_idx(const struct device_node *np, const char *name, int idx, int *param,
	int unit)
{
	unsigned int val = 0;

	if (!of_property_read_u32_index(np, name, idx, &val)) {
		*param = (int)val * unit;
	}  else {
		pr_info("Get %s no data, idx %d !!!\n", name, idx);
		return -1;
	}

	return 0;
}

static int interpolation(int i1, int b1, int i2, int b2, int i)
{
	int ret;

	if (i2 == i1)
		return b1;

	ret = (b2 - b1) * (i - i1) / (i2 - i1) + b1;
	return ret;
}

static int soc_to_ocv(int soc, unsigned int table_idx)
{
	struct fg_info_t *info_p = &fg_data.fg_info[table_idx];
	struct ocv_table_t *table_p;
	int dod, ret, i;
	int high_dod, low_dod, high_volt, low_volt;

	dod = 10000 - soc + SOC_ERROR;
	if (dod > 10000)
		dod = 10000;
	else if (dod < 0)
		dod = 0;

	for (i = 0; i < info_p->ocv_table_size; i++) {
		table_p = &info_p->ocv_table[i];
		if (table_p->dod >= dod)
			break;
	}

	if (i == 0) {
		ret = info_p->ocv_table[0].voltage;
	} else if (i >= info_p->ocv_table_size) {
		i = info_p->ocv_table_size - 1;
		ret = info_p->ocv_table[i].voltage;
	} else {
		high_dod = info_p->ocv_table[i-1].dod;
		low_dod = info_p->ocv_table[i].dod;
		high_volt = info_p->ocv_table[i-1].voltage;
		low_volt = info_p->ocv_table[i].voltage;
		ret = interpolation(high_dod, high_volt, low_dod, low_volt, dod);
	}

	return ret;
}

void dump_ocv_table(unsigned int idx)
{
	int i, j, offset, cnt = 5;
	char str[256];

	if (mt_ppb_debug) {
		pr_info("table[%d] temp=%d qmax=%d table_size=%d [idx, mah, vol, soc]\n", idx,
			fg_data.fg_info[idx].temp, fg_data.fg_info[idx].qmax,
			fg_data.fg_info[idx].ocv_table_size);

		for (i = 0; i * cnt < fg_data.fg_info[idx].ocv_table_size; i++) {
			offset = 0;
			memset(str, 0, sizeof(str));
			for (j = 0; j < cnt; j++) {
				if (i*cnt+j >= fg_data.fg_info[idx].ocv_table_size)
					break;

				offset += snprintf(str + offset, 256 - offset, "(%3d %5d %5d %5d) ",
					i*cnt+j, fg_data.fg_info[idx].ocv_table[i*cnt+j].mah,
					fg_data.fg_info[idx].ocv_table[i*cnt+j].voltage,
					fg_data.fg_info[idx].ocv_table[i*cnt+j].dod);
			}
			pr_info("%s\n", str);
		}
	}
}

void update_ocv_table(int temp, int qmax)
{
	int i, j, ht, lt;
	struct fg_info_t *h_info_p, *l_info_p, *c_info_p;
	struct ocv_table_t *h_table_p, *l_table_p, *c_table_p;

	c_info_p = &fg_data.fg_info[0];

	if (c_info_p->temp == temp)
		return;

	for (i = 1; i <= fg_data.fg_info_size; i++) {
		if(temp > fg_data.fg_info[i].temp)
			break;
	}

	c_info_p->temp = temp;
	if (i == 1) {
		c_info_p->qmax = fg_data.fg_info[1].qmax;
		c_info_p->ocv_table_size = fg_data.fg_info[1].ocv_table_size;
		memcpy(&c_info_p->ocv_table[0], &fg_data.fg_info[1].ocv_table[0],
			sizeof(struct ocv_table_t) * c_info_p->ocv_table_size);
		if (qmax != 0) {
			c_info_p->qmax = qmax;
			for (j = 0; j < c_info_p->ocv_table_size; j++) {
				c_table_p = &c_info_p->ocv_table[j];
				c_table_p->dod = c_table_p->mah * 10000 / qmax;
			}
		}
		goto out;
	} else if (i == fg_data.fg_info_size + 1) {
		c_info_p->qmax = fg_data.fg_info[fg_data.fg_info_size].qmax;
		c_info_p->ocv_table_size = fg_data.fg_info[fg_data.fg_info_size].ocv_table_size;
		memcpy(&c_info_p->ocv_table[0],
			&fg_data.fg_info[fg_data.fg_info_size].ocv_table[0],
			sizeof(struct ocv_table_t) * c_info_p->ocv_table_size);
		if (qmax != 0) {
			c_info_p->qmax = qmax;
			for (j = 0; j < c_info_p->ocv_table_size; j++) {
				c_table_p = &c_info_p->ocv_table[j];
				c_table_p->dod = c_table_p->mah * 10000 / qmax;
			}
		}
		goto out;
	}

	h_info_p = &fg_data.fg_info[i-1];
	l_info_p = &fg_data.fg_info[i];
	ht = h_info_p->temp;
	lt = l_info_p->temp;

	if (qmax != 0)
		c_info_p->qmax = qmax;
	else
		c_info_p->qmax = interpolation(ht, h_info_p->qmax, lt, l_info_p->qmax, temp);

	for (i = 0; i < h_info_p->ocv_table_size; i++) {
		h_table_p = &h_info_p->ocv_table[i];
		l_table_p = &l_info_p->ocv_table[i];
		c_table_p = &c_info_p->ocv_table[i];

		c_table_p->mah = interpolation(ht, h_table_p->mah, lt, l_table_p->mah, temp);
		c_table_p->voltage = interpolation(ht, h_table_p->voltage, lt, l_table_p->voltage,
			temp);
		if (qmax != 0) {
			c_table_p->dod = c_table_p->mah * 10000 / qmax;
		} else {
			c_table_p->dod = interpolation(ht, h_table_p->dod, lt, l_table_p->dod,
				temp);
		}
	}
out:
	dump_ocv_table(0);
}

static int cal_imax(int vbat, int uvlo, int ocp, int rdc, int rac)
{
	int ret;

	if (vbat - ocp * (rac + rdc) / 2000 < uvlo)
		ret = (vbat - uvlo) * 2000 / (rdc + rac);
	else
		ret = ocp;

	return ret;
}

static int cal_uvlo(int vbat, int uvlo, int ocp, int rdc, int rac)
{
	int ret;

	if (vbat - ocp * (rdc + rac) / 2000 < uvlo)
		ret = uvlo;
	else
		ret = vbat - ocp * (rdc + rac) / 2000;

	return ret;
}

static int cal_max_bat_power(int vbat, int uvlo, int ocp, int rdc, int rac, int i)
{
	int ret;

	if (vbat - ocp * (rdc + rac) / 2000 <= uvlo)
		ret = vbat * i / 1000;
	else
		ret = vbat * ocp / 1000;

	return ret;
}

static int cal_max_sys_power(int bat_pwr, int imax, int rdc, int rac)
{
	int ret;

	ret = bat_pwr - (imax >> 2) * imax / 1000 * (rdc + rac) / 1000;

	return ret;
}

static int get_sys_power_budget(int ocv, int rdc, int rac, int ocp, int uvlo)
{
	int imax, uv, bat_pwr, sys_pwr;

	imax = cal_imax(ocv, uvlo, ocp, rdc, rac);
	uv = cal_uvlo(ocv, uvlo, ocp, rdc, rac);
	bat_pwr = cal_max_bat_power(ocv, uv, ocp, rdc, rac, imax);
	sys_pwr = cal_max_sys_power(bat_pwr, imax, rdc, rac);

	return sys_pwr;
}

static void bat_handler(struct work_struct *work)
{
	struct power_supply *psy = pb.psy, *psy_mtk;
	union power_supply_propval val;
	static int last_soc = MAX_VALUE, last_temp = MAX_VALUE;
	unsigned int temp_stage;
	int ret = 0, soc, temp, sys_power, volt, qmax;
	bool loop;

	if (!pb.psy)
		return;

	psy_mtk = power_supply_get_by_name("mtk-gauge");
	if (!psy_mtk)
		return;

	ret = power_supply_get_property(psy_mtk, POWER_SUPPLY_PROP_ENERGY_NOW, &val);
	if (ret)
		return;

	soc = val.intval / 100;
	if (soc == 0)
		return;

	ret = power_supply_get_property(psy_mtk, POWER_SUPPLY_PROP_ENERGY_FULL, &val);
	if (ret)
		qmax = 0;
	else
		qmax = val.intval;

	if (strcmp(psy->desc->name, "battery") != 0)
		return;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ret)
		return;

	temp = val.intval / 10;
	temp_stage = pb.temp_cur_stage;

	if (temp != last_temp) {
		do {
			loop = false;
			if (temp < last_temp && temp_stage < pb.temp_max_stage) {
				if (temp < pb.temp_thd[temp_stage]) {
					temp_stage++;
					loop = true;
				}
			} else if (temp > last_temp && temp_stage > 0) {
				if (temp >= pb.temp_thd[temp_stage-1]) {
					temp_stage--;
					loop = true;
				}
			}
		} while (loop);
		pb.cur_rdc = pb.rdc[temp_stage];
		pb.cur_rac = pb.rac[temp_stage];
		pb.temp_cur_stage = temp_stage;
		update_ocv_table(temp, qmax);
	}

	if (temp != last_temp || soc != last_soc) {
		volt = soc_to_ocv(soc * 100, 0);
		pb.ocv = volt / 10;
		sys_power = get_sys_power_budget(pb.ocv, pb.cur_rdc, pb.cur_rac, pb.ocp, pb.uvlo);
		last_temp = temp;
		last_soc = soc;
		kicker_ppb_request_power(KR_BUDGET, sys_power);
		if (mt_ppb_debug)
			pr_info("%s: power=%d ocv=%d soc=%d Rdc,Rac=%d,%d temp,stage=%d,%d\n",
				__func__, sys_power, pb.ocv, soc, pb.cur_rdc, pb.cur_rac, temp,
				pb.temp_cur_stage);
	}
}

int ppb_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;

	if (!ppb_ctrl.ppb_stop) {
		pb.psy = psy;
		schedule_work(&pb.bat_work);
		return NOTIFY_DONE;
	}

	return NOTIFY_DONE;
}

static int read_mtk_gauge_dts(struct platform_device *pdev)
{
	int i, j, ret, num;
	struct ocv_table_t *table_p;
	struct fg_info_t *info_p;
	struct device_node *np;
	char str[STR_SIZE];

	np = of_find_node_by_name(NULL, "mtk-gauge");
	if (!np)
		dev_notice(&pdev->dev, "get gauge node fail\n");

	read_dts_val(np, "active-table", &fg_data.fg_info_size, 1);
	if (fg_data.fg_info_size > 10)
		fg_data.fg_info_size = 10;

#ifdef DYNAMIC_ALLOC_PB_INFO
	fg_data.fg_info = devm_kmalloc_array(&pdev->dev, fg_data.fg_info_size + 1,
		sizeof(struct fg_info_t), GFP_KERNEL);
	if (!fg_data.fg_info)
		return -ENOMEM;
#endif
	fg_data.fg_info[0].temp = MAX_VALUE;

	for (i = 0; i < fg_data.fg_info_size; i++) {
		info_p = &fg_data.fg_info[i+1];

		num = 4;
		read_dts_val(np, "g-q-max-row", &num, 1);
		read_dts_val_by_idx(np, "g-q-max", i*num+fg_data.bat_type,
			&info_p->qmax, 1);

		ret = snprintf(str, STR_SIZE, "temperature-t%d", i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &info_p->temp, 1);

		ret = snprintf(str, STR_SIZE, "battery%d-profile-t%d-num", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &info_p->ocv_table_size, 1);

		if (info_p->ocv_table_size > 100)
			info_p->ocv_table_size = 100;

		if (i == 0) {
#ifdef DYNAMIC_ALLOC_PB_INFO
			fg_data.fg_info[0].ocv_table = devm_kmalloc_array(&pdev->dev,
				info_p->ocv_table_size, sizeof(struct ocv_table_t), GFP_KERNEL);
			if (!fg_data.fg_info[0].ocv_table)
				return -ENOMEM;
#endif
			fg_data.fg_info[0].ocv_table_size = info_p->ocv_table_size;
		} else{
			if (info_p->ocv_table_size != fg_data.fg_info[i].ocv_table_size) {
				pr_info("%s: table size not align %d %d !!!\n", __func__,
					info_p->ocv_table_size, fg_data.fg_info[i].ocv_table_size);
					break;
			}
		}

		num = 3;
		ret = snprintf(str, STR_SIZE, "battery%d-profile-t%d-col", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &num, 1);
#ifdef DYNAMIC_ALLOC_PB_INFO
		info_p->ocv_table = devm_kmalloc_array(&pdev->dev, info_p->ocv_table_size,
			sizeof(struct ocv_table_t), GFP_KERNEL);
		if (!info_p->ocv_table)
			return -ENOMEM;
#endif
		ret = snprintf(str, STR_SIZE, "battery%d-profile-t%d", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}

		for (j = 0; j < info_p->ocv_table_size; j++) {
			table_p = &info_p->ocv_table[j];
			read_dts_val_by_idx(np, str, j*num, &table_p->mah, 1);
			read_dts_val_by_idx(np, str, j*num+1, &table_p->voltage, 1);
			if (info_p->ocv_table[j].dod == 0 && info_p->qmax > 0)
				info_p->ocv_table[j].dod = info_p->ocv_table[j].mah * 1000 /
					info_p->qmax;

		}
	}
	return 0;
}

static int read_mtk_ppb_bat_dts(struct platform_device *pdev, struct device_node *np)
{
	int i, j, ret, num;
	struct ocv_table_t *table_p;
	struct fg_info_t *info_p;
	char str[STR_SIZE];

#ifdef DYNAMIC_ALLOC_PB_INFO
	fg_data.fg_info = devm_kmalloc_array(&pdev->dev, fg_data.fg_info_size + 1,
		sizeof(struct fg_info_t), GFP_KERNEL);
	if (!fg_data.fg_info)
		return -ENOMEM;
#endif
	fg_data.fg_info[0].temp = MAX_VALUE;

	for (i = 0; i < fg_data.fg_info_size; i++) {
		info_p = &fg_data.fg_info[i+1];

		ret = snprintf(str, STR_SIZE, "bat%d-ocv-table-t%d-qmax", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &info_p->qmax, 1);

		ret = snprintf(str, STR_SIZE, "bat%d-ocv-table-t%d-temperature", fg_data.bat_type,
				i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &info_p->temp, 1);

		ret = snprintf(str, STR_SIZE, "bat%d-ocv-table-t%d-size", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		read_dts_val(np, str, &info_p->ocv_table_size, 1);

		if (info_p->ocv_table_size > 100)
			info_p->ocv_table_size = 100;

		if (i == 0) {
#ifdef DYNAMIC_ALLOC_PB_INFO
			fg_data.fg_info[0].ocv_table = devm_kmalloc_array(&pdev->dev,
				info_p->ocv_table_size, sizeof(struct ocv_table_t), GFP_KERNEL);
			if (!fg_data.fg_info[0].ocv_table)
				return -ENOMEM;
#endif
			fg_data.fg_info[0].ocv_table_size = info_p->ocv_table_size;
		} else{
			//fixme
			if (info_p->ocv_table_size != fg_data.fg_info[i].ocv_table_size) {
				pr_info("%s: table size not align %d %d !!!\n", __func__,
					info_p->ocv_table_size, fg_data.fg_info[i].ocv_table_size);
					break;
			}
		}

		num = 3;
#ifdef DYNAMIC_ALLOC_PB_INFO
		info_p->ocv_table = devm_kmalloc_array(&pdev->dev, info_p->ocv_table_size,
			sizeof(struct ocv_table_t), GFP_KERNEL);
		if (!info_p->ocv_table)
			return -ENOMEM;
#endif
		ret = snprintf(str, STR_SIZE, "bat%d-ocv-table-t%d", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}

		for (j = 0; j < info_p->ocv_table_size; j++) {
			table_p = &info_p->ocv_table[j];
			read_dts_val_by_idx(np, str, j*num, &table_p->mah, 1);
			read_dts_val_by_idx(np, str, j*num+1, &table_p->voltage, 1);
			read_dts_val_by_idx(np, str, j*num+2, &table_p->dod, 1);
			if (info_p->ocv_table[j].dod == 0 && info_p->qmax > 0)
				info_p->ocv_table[j].dod = info_p->ocv_table[j].mah * 1000 /
					info_p->qmax;

		}
	}
	return 0;
}

static int read_power_budget_dts(struct platform_device *pdev)
{
	int i, ret;
	int num, offset = 0;
	struct device_node *np;
	char str[STR_SIZE];

	np = of_find_node_by_name(NULL, "mtk-gauge");
	if (!np)
		dev_notice(&pdev->dev, "get gauge node fail\n");

	if (read_dts_val(np, "bat_type", &fg_data.bat_type, 1)) {
		fg_data.bat_type = 0;
		pr_info("warning: can't get bat_type in dts, set to 0\n");
	}

	np = pdev->dev.of_node;
	if (!np) {
		dev_notice(&pdev->dev, "get peak power budget dts fail\n");
		return -ENODATA;
	}

	read_dts_val(np, "max-temperature-stage", &num, 1);
	if (num > 4 || num < 0)
		num = 0;

	if (num > 0)
		of_property_read_u32_array(np, "temperature-threshold", &pb.temp_thd[0], num);

	pb.temp_max_stage = num;

	for (i = 0; i <= pb.temp_max_stage; i++) {
		ret = snprintf(str, STR_SIZE, "battery%d-path-rdc-t%d", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		if (read_dts_val(np, str, &pb.rdc[i], 1))
			pb.rdc[i] = BAT_PATH_DEFAULT_RDC;

		ret = snprintf(str, STR_SIZE, "battery%d-path-rac-t%d", fg_data.bat_type, i);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
		if (read_dts_val(np, str, &pb.rac[i], 1))
			pb.rac[i] = BAT_PATH_DEFAULT_RAC;
	}

	read_dts_val(np, "system-ocp", &pb.ocp, 1);
	read_dts_val(np, "system-uvlo", &pb.uvlo, 1);

	ret = snprintf(str, STR_SIZE, "ocp=%d uvlo=%d temp_max_stage=%d",
		pb.ocp, pb.uvlo, pb.temp_max_stage);
	if (ret < 0)
		pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);

	for (i = 0; i <= pb.temp_max_stage; i++) {
		offset += snprintf(str + offset, STR_SIZE - offset, "[%d](rdc, rac)=(%d, %d)",
			i, pb.rdc[i], pb.rac[i]);
		if (ret < 0) {
			pr_info("%s:%d: snprintf error %d\n", __func__, __LINE__, ret);
			break;
		}
	}
	pr_info("%s\n", str);

	ret = read_dts_val(np, "bat-ocv-table-num", &fg_data.fg_info_size, 1);

	if (ret || fg_data.fg_info_size == 0)
		read_mtk_gauge_dts(pdev);
	else
		read_mtk_ppb_bat_dts(pdev, np);

	return 0;
}

static int mt_ppb_debug_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "(MODE/CG_MIN/VSYS_BUDGET/VSYS_ACK)=%u,%u,%u,%u\n",
		ppb_read_sram(PPB_MODE),
		ppb_read_sram(PPB_CG_PWR),
		ppb_read_sram(PPB_VSYS_PWR),
		ppb_read_sram(PPB_VSYS_ACK));

	seq_printf(m, "(FLASH/AUDIO/CAMERA/DISPLAY/APU/DRAM)=%u,%u,%u,%u,%u,%u\n",
		ppb_read_sram(PPB_FLASH_PWR),
		ppb_read_sram(PPB_AUDIO_PWR),
		ppb_read_sram(PPB_CAMERA_PWR),
		ppb_read_sram(PPB_DISPLAY_PWR),
		ppb_read_sram(PPB_APU_PWR),
		ppb_read_sram(PPB_DRAM_PWR));

	seq_printf(m, "(MD/WIFI/APU_ACK/BOOT_MODE)=%u,%u,%u,%u\n",
		ppb_read_sram(PPB_MD_PWR),
		ppb_read_sram(PPB_WIFI_PWR),
		ppb_read_sram(PPB_APU_PWR_ACK),
		ppb_read_sram(PPB_BOOT_MODE));

	return 0;
}

static int mt_ppb_dump_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u\n",
		ppb_read_sram(PPB_MODE),
		ppb_read_sram(PPB_CG_PWR),
		ppb_read_sram(PPB_VSYS_PWR),
		ppb_read_sram(PPB_VSYS_ACK),
		ppb_read_sram(PPB_FLASH_PWR),
		ppb_read_sram(PPB_AUDIO_PWR),
		ppb_read_sram(PPB_CAMERA_PWR),
		ppb_read_sram(PPB_DISPLAY_PWR),
		ppb_read_sram(PPB_APU_PWR),
		ppb_read_sram(PPB_DRAM_PWR),
		ppb_read_sram(PPB_MD_PWR),
		ppb_read_sram(PPB_WIFI_PWR),
		ppb_read_sram(PPB_APU_PWR_ACK),
		ppb_read_sram(PPB_CG_PWR_THD),
		ppb_read_sram(PPB_CG_PWR_CNT));

	return 0;
}

static int mt_ppb_debug_log_proc_show(struct seq_file *m, void *v)
{
	if (mt_ppb_debug)
		seq_puts(m, "ppb debug log enabled\n");
	else
		seq_puts(m, "ppb debug log disabled\n");

	return 0;
}

static ssize_t mt_ppb_debug_log_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	unsigned int len = 0;
	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &debug) == 0) {
		if (debug == 0)
			mt_ppb_debug = 0;
		else if (debug == 1)
			mt_ppb_debug = 1;
		else
			pr_notice("should be [0:disable,1:enable]\n");
	} else
		pr_notice("should be [0:disable,1:enable]\n");

	return count;
}

static int mt_ppb_manual_mode_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "manual_mode: %d\n", ppb_ctrl.manual_mode);
	if (ppb_ctrl.manual_mode > 0) {
		seq_printf(m, "(VSYS_BUDGET/REMAIN_BUDGET)=%u,%u\n",
			ppb_manual.vsys_budget,
			ppb_manual.remain_budget);

		seq_printf(m, "(FLASH/AUDIO/CAMERA/DISPLAY/APU/DRAM)=%u,%u,%u,%u,%u,%u\n",
			ppb_manual.loading_flash,
			ppb_manual.loading_audio,
			ppb_manual.loading_camera,
			ppb_manual.loading_display,
			ppb_manual.loading_apu,
			ppb_manual.loading_dram);
	} else {
		seq_printf(m, "(VSYS_BUDGET/REMAIN_BUDGET)=%u,%u\n",
			ppb.vsys_budget,
			ppb.remain_budget);

		seq_printf(m, "(FLASH/AUDIO/CAMERAC/DISPLAY/APU/DRAM)=%u,%u,%u,%u,%u,%u\n",
			ppb.loading_flash,
			ppb.loading_audio,
			ppb.loading_camera,
			ppb.loading_display,
			ppb.loading_apu,
			ppb.loading_dram);
	}

	return 0;
}

static ssize_t mt_ppb_manual_mode_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0;
	int vsys_budget, manual_mode = 0;
	int loading_flash, loading_audio, loading_camera;
	int loading_display, loading_apu, loading_dram;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d %d %d %d %d %d %d %d", cmd, &manual_mode, &vsys_budget,
		&loading_flash, &loading_audio, &loading_camera, &loading_display,
		&loading_apu, &loading_dram) != 9) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "manual", 6))
		return -EINVAL;

	if (manual_mode == 1) {
		ppb_ctrl.manual_mode = manual_mode;
		ppb_manual.vsys_budget = vsys_budget;
		ppb_manual.loading_flash = loading_flash;
		ppb_manual.loading_audio = loading_audio;
		ppb_manual.loading_camera = loading_camera;
		ppb_manual.loading_display = loading_display;
		ppb_manual.loading_apu = loading_apu;
		ppb_manual.loading_dram = loading_dram;
		ppb_allocate_budget_manager();
	} else if (manual_mode == 0) {
		ppb_ctrl.manual_mode = manual_mode;
		ppb_allocate_budget_manager();
	} else
		pr_notice("ppb manual setting should be 0 or 1\n");

	return count;
}

static int mt_ppb_stop_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "ppb stop: %d\n", ppb_ctrl.ppb_stop);
	return 0;
}

static ssize_t mt_ppb_stop_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, stop = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &stop) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "stop", 4))
		return -EINVAL;

	if (stop == 0 || stop == 1)
		ppb_ctrl.ppb_stop = stop;
	else
		pr_notice("ppb stop should be 0 or 1\n");

	return count;
}

static int mt_peak_power_mode_proc_show(struct seq_file *m, void *v)
{
	int mode;

	mode = ppb_read_sram(PPB_MODE);
	seq_printf(m, "ppb_mode: %d\n", mode);
	return 0;
}

static ssize_t mt_peak_power_mode_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, mode = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &mode) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (strncmp(cmd, "mode", 4))
		return -EINVAL;

	if (mode == 0 || mode == 1 || mode == 2) {
		ppb_ctrl.ppb_mode = mode;
		ppb_write_sram(mode, PPB_MODE);
		lbat_set_ppb_mode(mode);
		bat_oc_set_ppb_mode(mode);
	} else
		pr_notice("ppb mode should be 0 or 1 or 2\n");

	return count;
}

static int mt_ppb_cg_min_power_proc_show(struct seq_file *m, void *v)
{
	unsigned int power;

	power = ppb_read_sram(PPB_CG_PWR);
	seq_printf(m, "ppb CG min power: %u\n", power);
	return 0;
}

static ssize_t mt_ppb_cg_min_power_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0, power = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %u", cmd, &power) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	ppb_write_sram(power, PPB_CG_PWR);

	return count;
}

static int mt_ppb_camera_power_proc_show(struct seq_file *m, void *v)
{
	int manual_mode = ppb_ctrl.manual_mode;

	if (manual_mode == 0)
		seq_printf(m, "ppb manual mode: %d, camera power: %d\n",
						manual_mode, ppb.loading_camera);
	else
		seq_printf(m, "ppb manual mode: %d, camera power: %d\n",
						manual_mode, ppb_manual.loading_camera);

	return 0;
}

static ssize_t mt_ppb_camera_power_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64], cmd[21];
	unsigned int len = 0;
	int power = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%20s %d", cmd, &power) != 2) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (power < 0) {
		pr_notice("ppb camera power should not be negative value\n");
		return count;
	}

	kicker_ppb_request_power(KR_CAMERA, power);
	return count;
}

static int mt_ppb_cg_budget_thd_proc_show(struct seq_file *m, void *v)
{
	int thd = 0;

	thd = ppb_read_sram(PPB_CG_PWR_THD);
	seq_printf(m, "CG budget threshold: %d\n", thd);
	return 0;
}

static ssize_t mt_ppb_cg_budget_thd_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64];
	unsigned int len = 0;
	int thd = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &thd) != 0) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}
	if (thd < 0) {
		pr_notice("ppb camera power should not be negative value\n");
		return count;
	}

	ppb_write_sram(thd, PPB_CG_PWR_THD);
	return count;
}

static int mt_ppb_cg_budget_cnt_proc_show(struct seq_file *m, void *v)
{
	int cnt = 0;

	cnt = ppb_read_sram(PPB_CG_PWR_CNT);
	seq_printf(m, "CG budget threshold count: %d\n", cnt);
	return 0;
}

static ssize_t mt_ppb_cg_budget_cnt_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[64];
	unsigned int len = 0;
	int cnt = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &cnt) != 0) {
		pr_notice("parameter number not correct\n");
		return -EPERM;
	}

	if (cnt < 0) {
		pr_notice("ppb budget count should not be negative value\n");
		return count;
	}

	ppb_write_sram(cnt, PPB_CG_PWR_CNT);
	return count;
}

#define PROC_FOPS_RW(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, pde_data(inode));\
}									\
static const struct proc_ops mt_ ## name ## _proc_fops = {	\
	.proc_open		= mt_ ## name ## _proc_open,			\
	.proc_read		= seq_read,					\
	.proc_lseek		= seq_lseek,					\
	.proc_release		= single_release,				\
	.proc_write		= mt_ ## name ## _proc_write,			\
}

#define PROC_FOPS_RO(name)						\
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mt_ ## name ## _proc_show, pde_data(inode));\
}									\
static const struct proc_ops mt_ ## name ## _proc_fops = {	\
	.proc_open		= mt_ ## name ## _proc_open,		\
	.proc_read		= seq_read,				\
	.proc_lseek		= seq_lseek,				\
	.proc_release	= single_release,			\
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}
PROC_FOPS_RO(ppb_debug);
PROC_FOPS_RO(ppb_dump);
PROC_FOPS_RW(ppb_debug_log);
PROC_FOPS_RW(ppb_manual_mode);
PROC_FOPS_RW(ppb_stop);
PROC_FOPS_RW(peak_power_mode);
PROC_FOPS_RW(ppb_cg_min_power);
PROC_FOPS_RW(ppb_camera_power);
PROC_FOPS_RW(ppb_cg_budget_thd);
PROC_FOPS_RW(ppb_cg_budget_cnt);


static int mt_ppb_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(ppb_debug),
		PROC_ENTRY(ppb_dump),
		PROC_ENTRY(ppb_debug_log),
		PROC_ENTRY(ppb_manual_mode),
		PROC_ENTRY(ppb_stop),
		PROC_ENTRY(peak_power_mode),
		PROC_ENTRY(ppb_cg_min_power),
		PROC_ENTRY(ppb_camera_power),
		PROC_ENTRY(ppb_cg_budget_thd),
		PROC_ENTRY(ppb_cg_budget_cnt),
	};

	dir = proc_mkdir("ppb", NULL);

	if (!dir) {
		pr_notice("fail to create /proc/ppb @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0660, dir, entries[i].fops))
			pr_notice("@%s: create /proc/ppb/%s failed\n", __func__,
				    entries[i].name);
	}

	return 0;
}

static void get_md_dbm_info(void)
{
	int ret;
	u64 of_find;
	struct device_node *mddriver = NULL;

	mddriver = of_find_compatible_node(NULL, NULL, "mediatek,mddriver");
	if (!mddriver) {
		pr_info("mddriver not found in DTS\n");
		return;
	}

	ret =  of_property_read_u64(mddriver, "md_dbm_addr", &of_find);

	if (ret) {
		pr_info("address not found in DTS");
		return;
	}
	pr_info("%s md_dbm_addr: 0x%llx\n", __func__, of_find);
	ppb_write_sram((unsigned int)of_find, PPB_MD_SMEM_ADDR);

}

static int peak_power_budget_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	void __iomem *addr;
	struct device_node *np;
	struct tag_bootmode *tag;

	np = of_find_compatible_node(NULL, NULL, "mediatek,peak_power_budget");
	if (!np) {
		dev_notice(&pdev->dev, "get peak_power_budget node fail\n");
		return -ENODATA;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ppb_sram");
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		return PTR_ERR(addr);

	ppb_sram_base = addr;

	spin_lock_init(&ppb_lock);

	mt_ppb_create_procfs();

	INIT_WORK(&pb.bat_work, bat_handler);
	read_power_budget_dts(pdev);
	ppb_nb.notifier_call = ppb_psy_event;
	ret = power_supply_reg_notifier(&ppb_nb);
	if (ret) {
		dev_notice(&pdev->dev, "power_supply_reg_notifier fail\n");
		return ret;
	}

	np = of_parse_phandle(pdev->dev.of_node, "bootmode", 0);
	if (!np)
		dev_notice(&pdev->dev, "get bootmode fail\n");
	else {
		tag = (struct tag_bootmode *)of_get_property(np, "atag,boot", NULL);
		if (!tag)
			dev_notice(&pdev->dev, "failed to get atag,boot\n");
		else {
			dev_notice(&pdev->dev, "bootmode:0x%x\n", tag->bootmode);
			ppb_write_sram((int)tag->bootmode, PPB_BOOT_MODE);
		}
	}
	get_md_dbm_info();

	ppb_ctrl.ppb_drv_done = 1;

	return 0;
}

static int peak_power_budget_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id peak_power_budget_of_match[] = {
	{ .compatible = "mediatek,peak_power_budget", },
	{},
};
MODULE_DEVICE_TABLE(of, peak_power_budget_of_match);

static struct platform_driver peak_power_budget_driver = {
	.probe = peak_power_budget_probe,
	.remove = peak_power_budget_remove,
	.driver = {
		.name = "mtk_peak_power_budget",
		.of_match_table = peak_power_budget_of_match,
	},
};
module_platform_driver(peak_power_budget_driver);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK peak power budget");
MODULE_LICENSE("GPL");
