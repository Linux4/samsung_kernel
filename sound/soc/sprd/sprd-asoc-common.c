/*
 * sound/soc/sprd/sprd-asoc-common.c
 *
 * SPRD ASoC Common implement -- SpreadTrum ASOC Common.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt(" COM ")""fmt

#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <sound/soc.h>
#include <sound/info.h>

#include "sprd-asoc-common.h"

/* spreadtrum audio debug */
static int sp_audio_debug_flag = SP_AUDIO_DEBUG_DEFAULT;

struct regmap *aon_apb_gpr;

u32 agcp_ahb_set_offset;

u32 agcp_ahb_clr_offset;

struct regmap *g_agcp_ahb_gpr;
/*
 * pmu apb global registers operating interfaces
 */
struct regmap *pmu_apb_gpr;

/*
 * pmu completement apb global registers operating interfaces
 */
struct regmap *pmu_com_apb_gpr;

/*
 * ap apb global registers operating interfaces
 * ap_apb_gpr will be set by i2s.c in its probe func.
 */
struct regmap *g_ap_apb_gpr;

/* anlg_phy_g_controller */
struct regmap *anlg_phy_g;


inline int get_sp_audio_debug_flag(void)
{
	return sp_audio_debug_flag;
}
EXPORT_SYMBOL(get_sp_audio_debug_flag);

static void snd_pcm_sprd_debug_read(struct snd_info_entry *entry,
				    struct snd_info_buffer *buffer)
{
	int *p_sp_audio_debug_flag = entry->private_data;

	snd_iprintf(buffer, "0x%08x\n", *p_sp_audio_debug_flag);
}

static void snd_pcm_sprd_debug_write(struct snd_info_entry *entry,
				     struct snd_info_buffer *buffer)
{
	int *p_sp_audio_debug_flag = entry->private_data;
	char line[64];
	unsigned long long flag;

	if (!snd_info_get_line(buffer, line, sizeof(line))) {
		if (kstrtoull(line, 16, &flag) != 0) {
			pr_err("ERR: %s kstrtoull failed!\n", __func__);
			return;
		}
		*p_sp_audio_debug_flag = (int)flag;
	}
}

int sprd_audio_debug_init(struct snd_card *card)
{
	struct snd_info_entry *entry;

	entry = snd_info_create_card_entry(card, "asoc-sprd-debug",
							card->proc_root);
	if (entry != NULL) {
		entry->c.text.read = snd_pcm_sprd_debug_read;
		entry->c.text.write = snd_pcm_sprd_debug_write;
		entry->mode |= 0200;
		entry->private_data = &sp_audio_debug_flag;
		if (snd_info_register(entry) < 0) {
			snd_info_free_entry(entry);
			entry = NULL;
		}
	}

	return 0;
}
EXPORT_SYMBOL(sprd_audio_debug_init);

int aon_apb_gpr_null_check(void)
{
	if (aon_apb_gpr == NULL) {
		pr_err("ERR: %saon_apb_gpr is not initialized!\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

void arch_audio_set_aon_apb_gpr(struct regmap *gpr)
{
	aon_apb_gpr = gpr;
}

struct regmap *arch_audio_get_aon_apb_gpr(void)
{
	return aon_apb_gpr;
}

/*
 * agcp ahb global registers operating interfaces
 */

void set_agcp_ahb_offset(u32 set_ahb_offset, u32 clr_ahb_offset)
{
	agcp_ahb_set_offset = set_ahb_offset;
	agcp_ahb_clr_offset = clr_ahb_offset;
	pr_info("%s agcp_ahb_set_offset 0x%x, agcp_ahb_clr_offset 0x%x", __func__,
		agcp_ahb_set_offset, agcp_ahb_clr_offset);
}

int agcp_ahb_gpr_null_check(void)
{
	if (g_agcp_ahb_gpr == NULL) {
		pr_err("ERR: %s g_agcp_ahb_gpr isn't initialized!\n",
			__func__);
		return -1;
	}

	return 0;
}



void arch_audio_set_agcp_ahb_gpr(struct regmap *gpr)
{
	g_agcp_ahb_gpr = gpr;
}

struct regmap *arch_audio_get_agcp_ahb_gpr(void)
{
	return g_agcp_ahb_gpr;
}


int pmu_apb_gpr_null_check(void)
{
	if (pmu_apb_gpr == NULL) {
		pr_err("ERR: %s pmu_apb_gpr is not initialized!\n",
			__func__);
		return -1;
	}

	return 0;
}

void arch_audio_set_pmu_apb_gpr(struct regmap *gpr)
{
	pmu_apb_gpr = gpr;
}


int pmu_com_apb_gpr_null_check(void)
{
	if (pmu_com_apb_gpr == NULL) {
		pr_err("ERR: %s pmu_com_apb_gpr is not initialized!\n",
			__func__);
		return -1;
	}

	return 0;
}

void arch_audio_set_pmu_com_apb_gpr(struct regmap *gpr)
{
	pmu_com_apb_gpr = gpr;
}

int ap_apb_gpr_null_check(void)
{
	if (g_ap_apb_gpr == NULL) {
		pr_err("ERR: %s g_ap_apb_gpr is not initialized!\n",
			__func__);
		return -1;
	}

	return 0;
}

void arch_audio_set_ap_apb_gpr(struct regmap *gpr)
{
	g_ap_apb_gpr = gpr;
}

struct regmap *arch_audio_get_ap_apb_gpr(void)
{
	return g_ap_apb_gpr;
}
int anlg_phy_g_null_check(void)
{
	if (anlg_phy_g == NULL) {
		pr_err("ERR: %s anlg_phy_g2 is not initialized!\n",
			__func__);
		return -EFAULT;
	}

	return 0;
}

void arch_audio_set_anlg_phy_g(struct regmap *gpr)
{
	anlg_phy_g = gpr;
}

struct regmap *arch_audio_get_anlg_phy_g(void)
{
	return anlg_phy_g;
}

