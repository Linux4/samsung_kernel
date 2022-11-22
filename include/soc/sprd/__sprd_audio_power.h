/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
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

#ifndef __ASM_ARCH_AUDIO_POWER_H
#define __ASM_ARCH_AUDIO_POWER_H

#define FIXED_AUDIO_POWER 1

/* power setting */
static inline int arch_audio_power_write_mask(int reg, int val, int mask)
{
	int ret = 0;

#if FIXED_AUDIO_POWER
	ret = arch_audio_codec_write_mask(reg, val, mask);
#endif

	return ret;
}

static inline int arch_audio_power_write(int reg, int val)
{
	int ret = 0;

#if FIXED_AUDIO_POWER
	ret = arch_audio_codec_write(reg, val);
#endif

	return ret;
}

static inline int arch_audio_power_read(int reg)
{
	int ret = 0;

#if FIXED_AUDIO_POWER
	ret = arch_audio_codec_read(reg);
#endif

	return ret;
}

#endif
