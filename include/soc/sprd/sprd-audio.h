/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef __ASM_ARCH_AUDIO_GLB_H
#define __ASM_ARCH_AUDIO_GLB_H

#if defined(CONFIG_ARCH_SCX35L)
#include "__sprd_audio_sc8835l.h"
#elif defined(CONFIG_ARCH_SCX35)
#include "__sprd_audio_sc8830.h"
#elif defined(CONFIG_ARCH_SC8825)
#include "__sprd_audio_sc8825.h"
#else
#error "Unknown architecture specification"
#endif

#include "__sprd_audio_power.h"

typedef struct glb_reg_dump {
	char *reg_name;
	unsigned long reg;
	int count;
	int (*func)(void *);
} glb_reg_dump_t;

#define REG_PAIR(t, r, c, f)  {t->reg_name = #r; t->reg = r; t->count = c; t->func = (void*)f; t++; }

#ifndef AUDIO_VBC_REG_DUMP_LIST
#define AUDIO_VBC_REG_DUMP_LIST(t)
#endif
#ifndef AUDIO_CODEC_REG_DUMP_LIST
#define AUDIO_CODEC_REG_DUMP_LIST(t)
#endif
#ifndef AUDIO_IIS_REG_DUMP_LIST
#define AUDIO_IIS_REG_DUMP_LIST(t)
#endif

#define AUDIO_GLB_REG_DUMP_LIST(t) { \
	AUDIO_VBC_REG_DUMP_LIST(t) \
	AUDIO_CODEC_REG_DUMP_LIST(t) \
	AUDIO_IIS_REG_DUMP_LIST(t) \
}

#endif
