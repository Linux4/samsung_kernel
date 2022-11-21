/*
 * dbmd2-i2s.c  --  DBMD2 I2S interface
 *
 * Copyright (C) 2014 DSP Group
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMD2_I2S_H
#define _DBMD2_I2S_H

#include <sound/soc.h>

#define DBMD2_I2S_RATES			\
		(SNDRV_PCM_RATE_8000 |	\
		SNDRV_PCM_RATE_16000 |	\
		SNDRV_PCM_RATE_48000)

#define DBMD2_I2S_FORMATS		\
		(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE)

extern struct snd_soc_dai_ops dbmd2_i2s_dai_ops;

enum dbmd2_i2s_ports {
	DBMD2_I2S0 = 1,
	DBMD2_I2S1,
	DBMD2_I2S2,
	DBMD2_I2S3,
};

#endif
