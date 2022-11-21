/*
 * linux/sound/soc/pxa/mmp3-sspa-zsp.h
 *
 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _MMP3_SSPA_ZSP_H
#define _MMP3_SSPA_ZSP_H

#include <plat/ssp.h>

enum {
	MMP_SSPA1,
	MMP_SSPA2,
};

/* sspa clock sources */
#define MMP_SSPA_CLK_PLL	0
#define MMP_SSPA_CLK_VCXO	1
#define MMP_SSPA_CLK_AUDIO	3



/* sspa pll id */
#define MMP_SYSCLK		0

struct zsp_sspa {
	struct ssp_device *sspa;
	unsigned int sspa_id;
	unsigned int sspa_pll_clk;
	ssp_config_t zsp_sspa_conf;
};


extern struct ssp_device *sspa_request(int port, const char *label);
extern void sspa_free(struct ssp_device *);


#endif /* _MMP3_SSPA_ZSP_H */
