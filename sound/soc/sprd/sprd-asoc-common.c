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
#define pr_fmt(fmt) pr_sprd_fmt(" COM ") fmt
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/info.h>
#include <sound/sprd-audio-hook.h>

#include "sprd-asoc-common.h"

static struct sprd_audio_ext_hook *sprd_audio_hook = 0;

int sprd_ext_hook_register(struct sprd_audio_ext_hook *hook)
{
	if (sprd_audio_hook) {
		pr_err("ERR:Already Registed Hook\n");
		return -1;
	}
	sprd_audio_hook = hook;
	return 0;
}

EXPORT_SYMBOL(sprd_ext_hook_register);

int sprd_ext_hook_unregister(struct sprd_audio_ext_hook *hook)
{
	if (sprd_audio_hook != hook) {
		pr_err("ERR:Maybe Unregister other's Hook?\n");
		return -1;
	}
	sprd_audio_hook = 0;
	return 0;
}

EXPORT_SYMBOL(sprd_ext_hook_unregister);

#define SAFE_CALL(func) do { \
	if (func) { \
		ret = func(id, on); \
	} \
} while(0)

int sprd_ext_speaker_ctrl(int id, int on)
{
	int ret = NO_HOOK;
	sp_asoc_pr_dbg("external Speaker(%d) Hook; on=%d\n", id, on);
	if (sprd_audio_hook)
		SAFE_CALL(sprd_audio_hook->ext_speaker_ctrl);
	return ret;
}

EXPORT_SYMBOL(sprd_ext_speaker_ctrl);

int sprd_ext_headphone_ctrl(int id, int on)
{
	int ret = NO_HOOK;
	sp_asoc_pr_dbg("external Headphone(%d) Hook; on=%d\n", id, on);
	if (sprd_audio_hook)
		SAFE_CALL(sprd_audio_hook->ext_headphone_ctrl);
	return ret;
}

EXPORT_SYMBOL(sprd_ext_headphone_ctrl);

int sprd_ext_earpiece_ctrl(int id, int on)
{
	int ret = NO_HOOK;
	sp_asoc_pr_dbg("external Earpiece(%d) Hook; on=%d\n", id, on);
	if (sprd_audio_hook)
		SAFE_CALL(sprd_audio_hook->ext_earpiece_ctrl);
	return ret;
}

EXPORT_SYMBOL(sprd_ext_earpiece_ctrl);

int sprd_ext_mic_ctrl(int id, int on)
{
	int ret = NO_HOOK;
	sp_asoc_pr_dbg("external MIC(%d) Hook; on=%d\n", id, on);
	if (sprd_audio_hook)
		SAFE_CALL(sprd_audio_hook->ext_mic_ctrl);
	return ret;
}

EXPORT_SYMBOL(sprd_ext_mic_ctrl);

int sprd_ext_fm_ctrl(int id, int on)
{
	int ret = NO_HOOK;
	sp_asoc_pr_dbg("external FM(%d) Hook; on=%d\n", id, on);
	if (sprd_audio_hook)
		SAFE_CALL(sprd_audio_hook->ext_fm_ctrl);
	return ret;
}

EXPORT_SYMBOL(sprd_ext_fm_ctrl);

/* spreadtrum audio debug */

static int sp_audio_debug_flag = SP_AUDIO_DEBUG_DEFAULT;

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
	if (!snd_info_get_line(buffer, line, sizeof(line)))
		*p_sp_audio_debug_flag = simple_strtoul(line, NULL, 16);
}

int sprd_audio_debug_init(struct snd_card *card)
{
	struct snd_info_entry *entry;
	if ((entry = snd_info_create_card_entry(card, "asoc-sprd-debug",
						card->proc_root)) != NULL) {
		entry->c.text.read = snd_pcm_sprd_debug_read;
		entry->c.text.write = snd_pcm_sprd_debug_write;
		entry->mode |= S_IWUSR;
		entry->private_data = &sp_audio_debug_flag;
		if (snd_info_register(entry) < 0) {
			snd_info_free_entry(entry);
			entry = NULL;
		}
	}
	return 0;
}

EXPORT_SYMBOL(sprd_audio_debug_init);
