#ifndef _REGS_DPU_H
#define _REGS_DPU_H

#define DPU_TSC_RED(_v) 	(((_v) & 0xff) << 0)
#define DPU_TSC_GREEN(_v)	(((_v) & 0xff) << 8)
#define DPU_TSC_BLUE(_v)	(((_v) & 0xff) << 16)
#define DPU_TSC_CYAN(_v)	(((_v) & 0xff) << 0)
#define DPU_TSC_MAGENTA(_v)	(((_v) & 0xff) << 8)
#define DPU_TSC_YELLOW(_v)	(((_v) & 0xff) << 16)
#define DPU_TSC_ONOFF(_v)	(((_v) & 0x1) << 3)
#define DPU_TSC_SCALE(_v)	(((_v) & 0xf) << 0)
#define DPU_TSC_SHIFT(_v)	(((_v) & 0x1ff) << 8)
#define DPU_TSC_GAIN(_v)	(((_v) & 0xff) << 0)

/* SCR : Red */
#define DPU_SCR_MASK	 	(0xffffff << 0)
#define DPUSCR_RED		0x0008
#define DPU_SCR_RED(_v) 	(((_v) & 0xffffff) << 0)
/* SCR : Green */
#define DPUSCR_GREEN		0x000C
#define DPU_SCR_GREEN(_v) 	(((_v) & 0xffffff) << 0)
/* SCR : Blue */
#define DPUSCR_BLUE		0x0010
#define DPU_SCR_BLUE(_v) 	(((_v) & 0xffffff) << 0)
/* SCR : Cyan */
#define DPUSCR_CYAN		0x0014
#define DPU_SCR_CYAN(_v) 	(((_v) & 0xffffff) << 0)
/* SCR : Magenta */
#define DPUSCR_MAGENTA		0x0018
#define DPU_SCR_MAGENTA(_v) 	(((_v) & 0xffffff) << 0)
/* SCR : Yellow */
#define DPUSCR_YELLOW		0x001C
#define DPU_SCR_YELLOW(_v) 	(((_v) & 0xffffff) << 0)
/* SCR : White */
#define DPUSCR_WHITE		0x0020
#define DPU_SCR_WHITE(_v) 	(((_v) & 0xffffff) << 0)
/* SCR : Black */
#define DPUSCR_BLACK		0x0024
#define DPU_SCR_BLACK(_v) 	(((_v) & 0xffffff) << 0)

#define DPU_HUE_RED(_v) 	(((_v) & 0x1ff) << 0)
#define DPU_HUE_GREEN(_v)	(((_v) & 0x1ff) << 9)
#define DPU_HUE_BLUE(_v)	(((_v) & 0x1ff) << 18)
#define DPU_HUE_CYAN(_v)	(((_v) & 0x1ff) << 0)
#define DPU_HUE_MAGENTA(_v)	(((_v) & 0x1ff) << 9)
#define DPU_HUE_YELLOW(_v)	(((_v) & 0x1ff) << 18)
#define DPU_HUE_ONOFF(_v)	(((_v) & 0x1) << 1)

#define DPUGAMMALUT_X_Y_BASE	0x0028
#define DPUGAMMALUT_MAX		65*3
#define DPU_GAMMA_LUT_Y(_v)	(((_v) & 0x1ff) << 2)
#define DPU_GAMMA_LUT_X(_v)	(((_v) & 0x1ff) << 18)

#define DPUCON			0x0000
#define DPU_MASK_CTRL		0x500
#define DPU_MASK_CTRL_MASK	0x1

#define DPU_DITHER_ON		0
#define DPU_DITHER_ON_MASK	(0x1 << DPU_DITHER_ON)
#define DPU_SCR_ON		1
#define DPU_SCR_ON_MASK		(0x1 << DPU_SCR_ON)
#define DPU_GAMMA_ON		2
#define DPU_GAMMA_ON_MASK	(0x1 << DPU_GAMMA_ON)
#define DPU_HSC_ON		3
#define DPU_HSC_ON_MASK		(0x1 << DPU_HSC_ON)

#define DPUIMG_SIZESET		0x0004

/* Contrast : Gamma */
#define DPU_GAMMA_LUT_Y_MASK	(0x1ff << 2)
#define DPU_GAMMA_LUT_X_MASK	(0x1ff << 18)

/* Saturation : TSC */
#define DPUHSC_CONTROL		0x01B4
#define PAIM_ON 		(0x1 << 5)
#define Y_COMP_ON		(0x1 << 4)
#define TSC_ON			(0x1 << 3)
#define GP_ON			(0x1 << 2)
#define PPHC_ON 		(0x1 << 1)
#define SKIN_ON 		(0x1 << 0)

/* Saturation : Blue, Green, Red */
#define DPUHSC_PAIMGAIN2_1_0	0x01B8
#define PAIM_GAIN2		(0xff << 16)
#define PAIM_GAIN1		(0xff << 8)
#define PAIM_GAIN0		(0xff << 0)

/* Saturation : Yellow, Magenta */
#define DPUHSC_PAIMGAIN5_4_3	0x01BC
#define PAIM_GAIN5		(0xff << 16)
#define PAIM_GAIN4		(0xff << 8)
#define PAIM_GAIN3		(0xff << 0)

/* Saturation : Decreasing, Enhancement */
#define DPUHSC_PAIMSCALE_SHIFT	0x01C0
#define PAIM_SHIFT		(0x1ff << 8)
#define PAIM_SCALE		(0xf << 0)

/* Hue : Blue, Green, Red */
#define DPUHSC_PPHCGAIN2_1_0	0x01C4
#define DPU_PPHC_GAIN2_MASK	(0x1ff << 18)
#define DPU_PPHC_GAIN1_MASK	(0x1ff << 9)
#define DPU_PPHC_GAIN0_MASK	(0x1ff << 0)

/* Hue : Yellow, Magenta, Cyan */
#define DPUHSC_PPHCGAIN5_4_3	0x01C8
#define DPU_PPHC_GAIN5_MASK	(0x1ff << 18)
#define DPU_PPHC_GAIN4_MASK	(0x1ff << 9)
#define DPU_PPHC_GAIN3_MASK	(0x1ff << 0)

/* Y compensation ratio, Total saturation control gain */
#define DPUHSC_TSCGAIN_YRATIO	0x01CC
#define YCOM_RATIO		(0xf << 16)
#define TSC_GAIN		(0xff << 0)

#define OPTION_SELECT_TRIGGER			(0xf << 21)
#define OPTION_SELECT_FUNCTION			(0xf << 16)

#endif
