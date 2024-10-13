/*
 * sound/soc/sprd/dai/vbc/r3p0/vbc-codec.h
 *
 * SPRD SoC VBC Codec -- SpreadTrum SOC VBC Codec function.
 *
 * Copyright (C) 2012 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY ork FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __VBC_CODEC_H
#define __VBC_CODEC_H

#include <sound/vbc-utils.h>


#define VBC_PROFILE_FIRMWARE_MAGIC_LEN	(16)
#define VBC_PROFILE_FIRMWARE_MAGIC_ID	("audio_profile")
#define VBC_PROFILE_VERSION		(0x00000002)
#define VBC_PROFILE_CNT_MAX		(0x0fffffff)
#define VBC_PROFILE_NAME_MAX		(32)
#define VBC_PROFILE_PARAS_LEN_MAX         (20+72*2)
#define VBC_PROFILE_MODE_MAX         (20)

#define MDG_STP_MAX_VAL		(0x1fff)
#define DG_MAX_VAL			(0x7f)
#define SMTHDG_MAX_VAL		(0xffff)
#define MIXERDG_MAX_VAL		(0xffff)
#define MIXERDG_STP_MAX_VAL	(0xffff)
#define OFFLOAD_DG_MAX		(4096)
#define MAX_32_BIT          (0xffffffff)
#define SRC_MAX_VAL		(48000)


#define FUN_REG(f) ((unsigned short)(-((f) + 1)))
#define SOC_REG(r) ((unsigned short)(r))

/* header of the data */
struct vbc_fw_header {
	char magic[VBC_PROFILE_FIRMWARE_MAGIC_LEN];
	u32 num_mode;	/* total mode num in profile */
	u32 len_mode; /* size of each mode in profile */
};

struct vbc_profile {
	struct device *dev;
	struct snd_soc_codec *codec;
	/*
	*16bit, 8bit ,   8bit
	*path,  mode,  offset(unit mode)
	*/
	int now_mode[SND_VBC_PROFILE_MAX];	/* the current mode */
	int is_loading[SND_VBC_PROFILE_MAX];
	struct vbc_fw_header hdr[SND_VBC_PROFILE_MAX];
	void *data[SND_VBC_PROFILE_MAX];	/* store profile from hal */
	int (*vbc_profile_apply) (struct snd_soc_codec * codec, void *data,
			int profile_type, int mode);
};

#define VBC_DG_L_SHIFT			(0)
#define VBC_DG_R_SHIFT			(16)
#define VBC_DG_VAL_MAX			(0x3FFF)

#define VBC_MDG_SWITCH_SHIFT		(0)
#define VBC_MDG_SWITCH_MASK		(0x1)
#define VBC_MDG_STEP_SHIFT		(16)
#define VBC_MDG_STEP_MASK		(0xFFFF)

#define VBC_SMTHDG_L_SHIFT			(0)
#define VBC_SMTHDG_R_SHIFT			(16)
#define VBC_SMTHDG_VAL_MAX			(0xFFFF)
#define VBC_SMTHDG_STEP_SHIFT	(0)
#define	VBC_SMTHDG_STEP_MAX	(0xFFFF)

#define VBC_MIXERDG_L_SHIFT		(0)
#define VBC_MIXERDG_R_SHIFT		(16)
#define VBC_MIXERDG_VAL_MAX		(0xFFFF)
#define VBC_MIXERDG_STEP_SHIFT	(0)
#define	VBC_MIXERDG_STEP_MAX	(0xFFFF)

#define VBC_VAL_MAX		(0xFFFFFFFF)

typedef int (*sprd_vbc_dg_set) (int id, int dg_l, int dg_r);
struct vbc_dg {
	int dg_l[VBC_DG_MAX-VBC_DG_START];
	int dg_r[VBC_DG_MAX-VBC_DG_START];
	sprd_vbc_dg_set dg_set;
};

typedef int (*sprd_vbc_mdg_set) (int id, int enable, int mdg_step);
struct vbc_mdg {
	int mute_switch[VBC_MDG_MAX-VBC_MDG_START];
	int mdg_step[VBC_MDG_MAX-VBC_MDG_START];
	sprd_vbc_mdg_set mdg_set;
};

typedef int (*sprd_vbc_smthdg_set) (int id, int smthdg_l, int smthdg_r);
typedef int (*sprd_vbc_smthdg_step_set) (int id, int smthdg_step);
struct vbc_smthdg {
	int smthdg_l[VBC_SMTHDG_MAX-VBC_SMTHDG_START];
	int smthdg_r[VBC_SMTHDG_MAX-VBC_SMTHDG_START];
	int smthdg_step[VBC_SMTHDG_MAX-VBC_SMTHDG_START];
	sprd_vbc_smthdg_set smthdg_set;
	sprd_vbc_smthdg_step_set smthdg_step_set;
};

typedef int (*sprd_vbc_mixerdg_mainpath_set) (int id, int mixerdg_main_l, int mixerdg_main_r);
typedef int (*sprd_vbc_mixerdg_mixpath_set) (int id, int mixerdg_mix_l, int mixerdg_mix_r);
typedef int (*sprd_vbc_mixerdg_step_set) (int smthdg_step);
struct vbc_mixerdg {
	int mixerdg_main_l[VBC_MIXERDG_MAX-VBC_MIXERDG_START];
	int mixerdg_main_r[VBC_MIXERDG_MAX-VBC_MIXERDG_START];
	int mixerdg_mix_l[VBC_MIXERDG_MAX-VBC_MIXERDG_START];
	int mixerdg_mix_r[VBC_MIXERDG_MAX-VBC_MIXERDG_START];
	int mixerdg_step;
	sprd_vbc_mixerdg_mainpath_set mixerdg_mainpath_set;
	sprd_vbc_mixerdg_mixpath_set mixerdg_mixpath_set;
	sprd_vbc_mixerdg_step_set mixerdg_step_set;
};

typedef int (*sprd_vbc_src_set) (int id, int enable, VBC_SRC_MODE_E src_mode);
struct vbc_src {
	int enable[VBC_SRC_MAX - VBC_SRC_DAC0];
	VBC_SRC_MODE_E src_mode[VBC_SRC_MAX - VBC_SRC_DAC0];
	sprd_vbc_src_set src_set;
};

typedef int (*sprd_vbc_bt_call_set) (int id, int enable, VBC_SRC_MODE_E src_mode);
struct vbc_bt_call {
	int enable;
	VBC_SRC_MODE_E src_mode;
	sprd_vbc_bt_call_set bt_call_set;
};


struct vbc_codec_priv {
	struct snd_soc_codec *codec;
	struct vbc_profile vbc_profile_setting;
	struct mutex load_mutex;
	struct vbc_mdg mdg;
	struct vbc_dg dg;
	struct vbc_dg offload_dg;
	struct vbc_smthdg smthdg;
	struct vbc_mixerdg mixerdg;
	int vbc_mux[VBC_MUX_MAX-VBC_MUX_START];
	int vbc_adder[VBC_ADDER_MAX-VBC_ADDER_START];
	struct vbc_src vbc_src;
	struct vbc_bt_call bt_call;
	int volume;
	VBC_LOOPBACK_TYPE_E loopback_type;
	int voice_fmt;
	int arm_rate;
    unsigned short vbc_dp_en[VBC_DP_EN_MAX-VBC_DP_EN_START];
    unsigned int dsp_reg;
};

#define IS_SPRD_VBC_PROFILE_RANG(reg) ((reg) >= SND_VBC_PROFILE_START && (reg) < (SND_VBC_PROFILE_MAX))
#define SPRD_VBC_PROFILE_IDX(reg) ((reg) - SND_VBC_PROFILE_START)
#define IS_SPRD_VBC_MDG_RANG(reg) ((reg) >= VBC_MDG_START && (reg) < (VBC_MDG_MAX))
#define SPRD_VBC_MDG_IDX(reg) ((reg) - VBC_MDG_START)
#define SPRD_VBC_DG_IDX(reg) ((reg) - VBC_DG_START)
#define IS_SPRD_VBC_DG_RANG(reg) ((reg) >= VBC_DG_START && (reg) < (VBC_DG_MAX))
#define SPRD_VBC_SMTHDG_IDX(reg) ((reg) - VBC_SMTHDG_START)
#define IS_SPRD_VBC_SMTHDG_RANG(reg) ((reg) >= VBC_SMTHDG_START && (reg) < (VBC_SMTHDG_MAX))
#define SPRD_VBC_MIXERDG_IDX(reg) ((reg) - VBC_MIXERDG_START)
#define IS_SPRD_VBC_MIXERDG_RANG(reg) ((reg) >= VBC_MIXERDG_START && (reg) < (VBC_MIXERDG_MAX))
#define IS_SPRD_VBC_MUX_RANG(reg) ((reg) >= VBC_MUX_START && (reg) < (VBC_MUX_MAX))
#define SPRD_VBC_MUX_IDX(reg) ((reg) - VBC_MUX_START)
#define IS_SPRD_VBC_ADDER_MUX_RANG(id) (((id) >= VBC_ADDER_START) && ((id) < VBC_ADDER_MAX))
#define SPRD_VBC_ADDER_IDX(reg)	((reg) - VBC_ADDER_START)

int vbc_codec_getpathinfo(struct device *dev, struct snd_pcm_statup_paras *para,
			struct sprd_vbc_priv *vbc_priv);
int sprd_vbc_codec_remove(struct platform_device *pdev);
int sprd_vbc_codec_probe(struct platform_device *pdev);

#endif /* __VBC_CODEC_H */
