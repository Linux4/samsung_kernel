/*
 * es705-veq-params.h  --  Audience eS705 ALSA SoC Audio driver
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Jeremy Pi <jpi@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES705_VEQ_PARAMS_H
#define _ES705_VEQ_PARAMS_H

enum {
	VEQ_CT,
	VEQ_FT,
	MAX_VEQ_USE_CASE
};

#if defined(CONFIG_MACH_TRLTE_VZW) || defined(CONFIG_MACH_TBLTE_VZW) || \
	defined(CONFIG_MACH_TRLTE_USC) || defined(CONFIG_MACH_TBLTE_USC) 
#define MAX_VOLUME_INDEX 8

/* index 0 means min. volume level */
static u32 veq_max_gains_nb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180005,
	0x90180005,
	0x90180005, 
	0x90180007,
	0x90180009,
	0x90180007,
	0x90180005,
	0x90180003
	},
	{
	0x90180003,
	0x90180004, 
	0x90180006,
	0x90180008,
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};


/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_nb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x9018001e
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f,
	0x9018000f
	}
};

static u32 veq_max_gains_extra[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180002,
	0x90180002,
	0x90180002,
	0x90180004,
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180001
	},
	{
	0x90180003,
	0x90180004,	
	0x90180006,
	0x90180008,
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_extra[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x9018001e
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f,
	0x9018000f
	}
};

static u32 veq_max_gains_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180005,
	0x90180005,
	0x90180005, 
	0x90180007,
	0x90180009,
	0x90180007,
	0x90180005,
	0x90180003
	},
	{
	0x90180003,
	0x90180004, 
	0x90180006,
	0x90180008,
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x9018001e
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f,
	0x9018000f
	}
};

static u32 veq_max_gains_extra_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180002,
	0x90180002,
	0x90180002,
	0x90180004,
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180001
	},
	{
	0x90180003,
	0x90180004,	
	0x90180006,
	0x90180008,
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_extra_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x9018001e
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f,
	0x9018000f
	}
};

#elif defined(CONFIG_MACH_TRLTE_ATT) || defined(CONFIG_MACH_TBLTE_ATT) || \
	defined(CONFIG_MACH_TRLTE_TMO) || defined(CONFIG_MACH_TBLTE_TMO) || \
	defined(CONFIG_MACH_TRLTE_SPR) || defined(CONFIG_MACH_TBLTE_SPR) || \
	defined(CONFIG_MACH_TRLTE_CAN)

#define MAX_VOLUME_INDEX 6

/* index 0 means min. volume level */
static u32 veq_max_gains_nb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180003,
	0x90180003,
	0x90180005,
	0x90180007,
	0x90180005,
	0x90180003
	},
	{
	0x90180003,
	0x90180004,	
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_nb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x9018001e
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f
	}
};

static u32 veq_max_gains_extra[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180001,
	0x90180001,
	0x90180003,
	0x90180005,
	0x90180003,
	0x90180001
	},
	{
	0x90180003,
	0x90180004,	
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_extra[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x9018001e
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f
	}
};

static u32 veq_max_gains_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180004,
	0x90180004,
	0x90180004,
	0x90180006,
	0x90180004,
	0x90180002
	},
	{
	0x90180003,
	0x90180004,	
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f
	}
};

static u32 veq_max_gains_extra_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180001,
	0x90180001,
	0x90180003,
	0x90180005,
	0x90180003,
	0x90180001
	},
	{
	0x90180003,
	0x90180004,	
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_extra_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f
	}
};

#else
#define MAX_VOLUME_INDEX 6

/* index 0 means min. volume level */
static u32 veq_max_gains_nb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180003,
	0x90180003,
	0x90180005,
	0x90180007,
	0x90180005,
	0x90180003
	},
	{
	0x90180003,
	0x90180004,	
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_nb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x9018001e
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f
	}
};

static u32 veq_max_gains_extra[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180001,
	0x90180001,
	0x90180003,
	0x90180005,
	0x90180003,
	0x90180001
	},
	{
	0x90180003,
	0x90180004,	
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_extra[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x9018001e
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f
	}
};

static u32 veq_max_gains_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180004,
	0x90180004,
	0x90180004,
	0x90180006,
	0x90180004,
	0x90180002
	},
	{
	0x90180003,
	0x90180004,	
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f
	}
};

static u32 veq_max_gains_extra_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180001,
	0x90180001,
	0x90180003,
	0x90180005,
	0x90180003,
	0x90180001
	},
	{
	0x90180003,
	0x90180004,	
	0x90180006,
	0x90180004,
	0x90180002,
	0x90180000
	}
};

/* index 0 means min. volume level */
static u32 veq_noise_estimate_adjs_extra_wb[MAX_VEQ_USE_CASE][MAX_VOLUME_INDEX] = {
	{
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014,
	0x90180014
	},
	{
	0x90180009,
	0x90180009,
	0x90180009,
	0x90180009,
	0x9018000b,
	0x9018000f
	}
};

#endif 
#endif
