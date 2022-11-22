/*
 * nxp_tfa9897.h -- NXP ALSA SoC Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/version.h>

enum nxp_type {
	NXP,
};
struct nxp_cdata {
	unsigned int rate;
	unsigned int fmt;
};
struct nxp_priv {
	struct snd_soc_codec *codec;
	enum nxp_type devtype;
	void *control_data;
	struct nxp_pdata *pdata;
};

