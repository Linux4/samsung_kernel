/*
 * s2mps22-private.h - Voltage regulator driver for the s2mps22
 *
 *  Copyright (C) 2019 Samsung Electrnoics
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
 */

#ifndef __LINUX_MFD_S2MPS22_REGULATOR_H
#define __LINUX_MFD_S2MPS22_REGULATOR_H
#include <linux/i2c.h>

#define MASK(width, shift)		(((0x1 << (width)) - 1) << shift)
#define SetBit(no)			(0x1 << (no))

#define S2MPS22_REG_INVALID		(0xFF)
#define S2MPS22_IRQSRC_PMIC		SetBit(0)

/* PMIC Top-Level Registers */
#define	S2MPS22_PMIC_REG_PMICID		0x04
#define	S2MPS22_PMIC_REG_INTSRC		0x05 /* Global Int */
#define	S2MPS22_PMIC_REG_INTSRC_MASK	0x06
#define S2MPS22_PMIC_REG_OTP_TEST	0x13

/* PMIC Registers */
#define S2MPS22_REG_INT1		0x00
#define S2MPS22_REG_INT2		0x01
#define S2MPS22_REG_INT3		0x02
#define S2MPS22_REG_INT4		0x03
#define S2MPS22_REG_INT5		0x04
#define S2MPS22_REG_INT6		0x05
#define S2MPS22_REG_INT7		0x06
#define S2MPS22_REG_INT8		0x07

#define S2MPS22_REG_INT1M		0x08
#define S2MPS22_REG_INT2M		0x09
#define S2MPS22_REG_INT3M		0x0A
#define S2MPS22_REG_INT4M		0x0B
#define S2MPS22_REG_INT5M		0x0C
#define S2MPS22_REG_INT6M		0x0D
#define S2MPS22_REG_INT7M		0x0E
#define S2MPS22_REG_INT8M		0x0F

#define S2MPS22_REG_STATUS1		0x10
#define S2MPS22_REG_STATUS2		0x11

#define S2MPS22_REG_OFFSRC		0x12
#define S2MPS22_REG_CTRL1		0x13

#define S2MPS22_REG_BUCK1S_CTRL		0x1B
#define S2MPS22_REG_BUCK1S_OUT		0x1C
#define S2MPS22_REG_BUCK2S_CTRL		0x1D
#define S2MPS22_REG_BUCK2S_OUT		0x1E
#define S2MPS22_REG_BUCK3S_CTRL		0x1F
#define S2MPS22_REG_BUCK3S_OUT		0x20
#define S2MPS22_REG_BUCK4S_CTRL		0x21
#define S2MPS22_REG_BUCK4S_OUT		0x22

#define S2MPS22_REG_BUCK_RAMP1		0x25
#define S2MPS22_REG_BUCK_RAMP2		0x26

#define S2MPS22_REG_LDO1S_DVS		0x27
#define S2MPS22_REG_LDO2S_DVS		0x28
#define S2MPS22_REG_LDO3S_DVS		0x29

#define S2MPS22_REG_LDO1S_CTRL		0x2A
#define S2MPS22_REG_LDO2S_CTRL		0x2B
#define S2MPS22_REG_LDO3S_CTRL		0x2C
#define S2MPS22_REG_LDO4S_CTRL		0x2D
#define S2MPS22_REG_LDO5S_CTRL		0x2E
#define S2MPS22_REG_LDO6S_CTRL		0x2F
#define S2MPS22_REG_LDO7S_CTRL		0x30

#define S2MPS22_REG_LDO_DSCH1		0x31
#define S2MPS22_REG_LDO_CTRL1		0x33
#define S2MPS22_REG_IOCONF		0x34
#define S2MPS22_REG_IPTAT		0x35
#define S2MPS22_REG_OFFCTRL1		0x36
#define S2MPS22_REG_OFFCTRL2		0x37
#define S2MPS22_REG_TCXO_CTRL		0x38

#define S2MPS22_REG_ONSEQ_CTRL1		0x39
#define S2MPS22_REG_ONSEQ_CTRL2		0x3A
#define S2MPS22_REG_ONSEQ_CTRL3		0x3B
#define S2MPS22_REG_ONSEQ_CTRL4		0x3C
#define S2MPS22_REG_ONSEQ_CTRL5		0x3D
#define S2MPS22_REG_ONSEQ_CTRL6		0x3E
#define S2MPS22_REG_ONSEQ_CTRL7		0x3F
#define S2MPS22_REG_ONSEQ_CTRL8		0x40

#define S2MPS22_REG_OFFSEQ_CTRL1	0x41
#define S2MPS22_REG_OFFSEQ_CTRL2	0x42
#define S2MPS22_REG_OFFSEQ_CTRL3	0x43
#define S2MPS22_REG_OFFSEQ_CTRL4	0x44
#define S2MPS22_REG_OFFSEQ_CTRL5	0x45
#define S2MPS22_REG_OFFSEQ_CTRL6	0x46

#define S2MPS22_REG_SEQ_CTRL		0x47

#define S2MPS22_REG_CONTROL_SEL1	0x48
#define S2MPS22_REG_CONTROL_SEL2	0x49
#define S2MPS22_REG_CONTROL_SEL3	0x4A
#define S2MPS22_REG_CONTROL_SEL4	0x4B
#define S2MPS22_REG_CONTROL_SEL5	0x4C
#define S2MPS22_REG_CONTROL_SEL6	0x4D

#define S2MPS22_REG_GPIO_EXT_CTRL1	0x4E
#define S2MPS22_REG_GPIO_EXT_CTRL2	0x4F
#define S2MPS22_REG_GPIO_EXT_CTRL3	0x50
#define S2MPS22_REG_GPIO_EXT_CTRL4	0x51
#define S2MPS22_REG_GPIO_EXT_CTRL5	0x52
#define S2MPS22_REG_GPIO_EXT_CTRL6	0x53

#define S2MPS22_REG_GPIO_GPADC_CTRL1	0x55
#define S2MPS22_REG_GPIO_GPADC_CTRL2	0x56
#define S2MPS22_REG_GPIO_GPADC_CTRL3	0x57
#define S2MPS22_REG_GPIO_GPADC_CTRL4	0x58
#define S2MPS22_REG_GPIO_GPADC_CTRL5	0x59
#define S2MPS22_REG_GPIO_GPADC_CTRL6	0x5A
#define S2MPS22_REG_GPIO_GPADC_CTRL7	0x5B
/* Power Meter */
#define S2MPS22_REG_ADC_CTRL1		0x5E
#define S2MPS22_REG_ADC_CTRL2		0x5F
#define S2MPS22_REG_ADC_CTRL3		0x60
#define S2MPS22_REG_ADC_DATA		0x61
#define S2MPS22_REG_ADC_MON_VBAT	0x62

#define S2MPS22_REG_GPADC_CTRL1		0x64
#define S2MPS22_REG_GPADC_CTRL2		0x65
#define S2MPS22_REG_GPADC_DATA1		0x66
#define S2MPS22_REG_GPADC_DATA2		0x67
#define S2MPS22_REG_GPADC_SUM_DATA1	0x68
#define S2MPS22_REG_GPADC_SUM_DATA2	0x69
#define S2MPS22_REG_GPADC_SUM_DATA3	0x6A
#define S2MPS22_REG_GPADC_DEBUG_DATA1	0x6B
#define S2MPS22_REG_GPADC_DEBUG_DATA2	0x6C
#define S2MPS22_REG_GPADC_DEBUG_DATA3	0x6D

#define S2MPS22_REG_BUCK_OI_EN		0x8E
#define S2MPS22_REG_BUCK_OI_PD_CTRL1	0x8F
#define S2MPS22_REG_BUCK_OI_CTRL1	0x90
#define S2MPS22_REG_BUCK_OI_CTRL3	0x92
#define S2MPS22_REG_OVP_CTRL1		0x94

#define S2MPS22_BUCK_OI_EN_MASK		0x0F
#define S2MPS22_BUCK_OI_PD_CTRL1_MASK	0x0F
#define S2MPS22_BUCK_OI_CTRL1_MASK	0xFF
#define S2MPS22_BUCK_OI_CTRL3_MASK	0xFF
/* WTSR Mask */
#define S2MPS22_WTSREN_MASK		MASK(1,3)

/* S2MPS22 Regulator ids */
enum S2MPS22_regulators {
	S2MPS22_LDO1,
	S2MPS22_LDO2,
	S2MPS22_LDO3,
	S2MPS22_LDO4,
	S2MPS22_LDO5,
	S2MPS22_LDO6,
//	S2MPS22_LDO7,

	S2MPS22_BUCK1,
	S2MPS22_BUCK2,
	S2MPS22_BUCK3,
	S2MPS22_BUCK4,

	S2MPS22_REG_MAX,
};

/* BUCKs 1S_2S_3S_4S */
#define S2MPS22_BUCK_MIN1		300000
#define S2MPS22_BUCK_STEP1		6250
/* LDOs 1S_2S_3S */
#define S2MPS22_LDO_MIN1		300000
#define S2MPS22_LDO_STEP1		25000
/* LDOs 4S_6S_7S */
#define S2MPS22_LDO_MIN2		700000
#define S2MPS22_LDO_STEP2		25000
/* LDOs 5S */
#define S2MPS22_LDO_MIN3		700000
#define S2MPS22_LDO_STEP3		12500
/* LDO/BUCK output voltage control */
#define S2MPS22_LDO_VSEL_MASK		0x3F
#define S2MPS22_BUCK_VSEL_MASK		0xFF
#define S2MPS22_LDO_N_VOLTAGES		(S2MPS22_LDO_VSEL_MASK + 1)
#define S2MPS22_BUCK_N_VOLTAGES 	(S2MPS22_BUCK_VSEL_MASK + 1)
/* Enable control[7:6] */
#define S2MPS22_ENABLE_SHIFT		0x06
#define S2MPS22_ENABLE_MASK		(0x03 << S2MPS22_ENABLE_SHIFT)

#define S2MPS22_REGULATOR_MAX		(S2MPS22_REG_MAX)
#define SEC_PMIC_REV(iodev)		(iodev)->pmic_rev
/* Set LDO/BUCK time */
#define S2MPS22_ENABLE_TIME_LDO		128
#define S2MPS22_ENABLE_TIME_BUCK	130
/* unit : nA */
#define CURRENT_B1				31250000
#define CURRENT_B2				46875000
#define CURRENT_B3_4			15625000
#define CURRENT_L1_3		 	2343750
#define CURRENT_L2			 	3515625
#define CURRENT_L4_5		 	4687500
#define CURRENT_L6_7		 	1171875

/* unit : pW */
#define POWER_B1			195313000
#define POWER_B2			292969000
#define POWER_B3_4			97656250
#define POWER_L1_3			58593750
#define POWER_L2			87890630
#define POWER_L4			117187500
#define POWER_L5_6_7		29296880

/* ADC_CTRL3 MASK */
#define ADC_EN_MASK			MASK(1,7)
#define ADC_ASYNCRD_MASK		MASK(1,7)
#define ADC_CAL_EN_MASK			MASK(1,6)
#define ADC_PTR_MASK			MASK(6,0)
/* ADC MUX/CURRENT/POWER Base */
#define MUX_PTR_BASE			0x20
#define CURRENT_PTR_BASE		0x00
#define POWER_PTR_BASE			0x10
/* LDO 1~7 */
#define S2MPS22_LDO_START		0x41
#define S2MPS22_LDO_END			0x47
/* BUCK 1~4 */
#define S2MPS22_BUCK_START		0x01
#define S2MPS22_BUCK_END		0x04
#define S2MPS22_LDO_CNT			7
#define S2MPS22_BUCK_CNT		4
#define S2MPS22_BUCK_OCP_IRQ_NUM	4	/* S2MPS22_IRQ_OCP_Bx_INT3 */

#define SMP_NUM_MASK			MASK(4,0)

#define S2MPS22_MAX_ADC_CHANNEL		8
/*
 * sec_opmode_data - regulator operation mode data
 * @id: regulator id
 * @mode: regulator operation mode
 */

enum s2mps22_temperature_source {
	S2MPS22_TEMP_120 = 0,	/* 120 degree */
	S2MPS22_TEMP_140,	/* 140 degree */

	S2MPS22_TEMP_NR,
};

enum s2mps22_irq_source {
	S2MPS22_PMIC_INT1 = 0,
	S2MPS22_PMIC_INT2,
	S2MPS22_PMIC_INT3,
	S2MPS22_PMIC_INT4,
	S2MPS22_PMIC_INT5,
	S2MPS22_PMIC_INT6,
	S2MPS22_PMIC_INT7,
	S2MPS22_PMIC_INT8,

	S2MPS22_IRQ_GROUP_NR,
};

#define S2MPS22_NUM_IRQ_PMIC_REGS	8
#define S2MPS22_BUCK_OI_MAX		4

enum s2mps22_irq {
	/* PMIC */
	S2MPS22_IRQ_PWRONF_INT1,
	S2MPS22_IRQ_PWRONR_INT1,
	S2MPS22_IRQ_INT120C_INT1,
	S2MPS22_IRQ_INT140C_INT1,
	S2MPS22_IRQ_TSD_INT1,
	S2MPS22_IRQ_WTSR_INT1,
	S2MPS22_IRQ_WRSTB_INT1,
	S2MPS22_IRQ_ADCDONE_INT1,

	S2MPS22_IRQ_OC1_INT2,
	S2MPS22_IRQ_OC2_INT2,
	S2MPS22_IRQ_OC3_INT2,
	S2MPS22_IRQ_OC4_INT2,
	S2MPS22_IRQ_OC5_INT2,
	S2MPS22_IRQ_OC6_INT2,
	S2MPS22_IRQ_OC7_INT2,
	S2MPS22_IRQ_OC8_INT2,

	S2MPS22_IRQ_OCP_B1_INT3,
	S2MPS22_IRQ_OCP_B2_INT3,
	S2MPS22_IRQ_OCP_B3_INT3,
	S2MPS22_IRQ_OCP_B4_INT3,

	S2MPS22_IRQ_OI_B1_INT4,
	S2MPS22_IRQ_OI_B2_INT4,
	S2MPS22_IRQ_OI_B3_INT4,
	S2MPS22_IRQ_OI_B4_INT4,

	S2MPS22_IRQ_SC_LDO1_INT5,
	S2MPS22_IRQ_SC_LDO2_INT5,
	S2MPS22_IRQ_SC_LDO3_INT5,
	S2MPS22_IRQ_SC_LDO4_INT5,
	S2MPS22_IRQ_SC_LDO5_INT5,
	S2MPS22_IRQ_SC_LDO6_INT5,
	S2MPS22_IRQ_SC_LDO7_INT5,

	S2MPS22_IRQ_GPIO_EXT0_F_INT6,
	S2MPS22_IRQ_GPIO_EXT0_R_INT6,
	S2MPS22_IRQ_GPIO_EXT1_F_INT6,
	S2MPS22_IRQ_GPIO_EXT1_R_INT6,
	S2MPS22_IRQ_GPIO_EXT2_F_INT6,
	S2MPS22_IRQ_GPIO_EXT2_R_INT6,

	S2MPS22_IRQ_GPIO_GPADC0_F_INT7,
	S2MPS22_IRQ_GPIO_GPADC0_R_INT7,
	S2MPS22_IRQ_GPIO_GPADC1_F_INT7,
	S2MPS22_IRQ_GPIO_GPADC1_R_INT7,
	S2MPS22_IRQ_GPIO_GPADC2_F_INT7,
	S2MPS22_IRQ_GPIO_GPADC2_R_INT7,
	S2MPS22_IRQ_GPIO_GPADC3_F_INT7,
	S2MPS22_IRQ_GPIO_GPADC3_R_INT7,

	S2MPS22_IRQ_GPIO_GPADC4_F_INT8,
	S2MPS22_IRQ_GPIO_GPADC4_R_INT8,
	S2MPS22_IRQ_GPIO_GPADC5_F_INT8,
	S2MPS22_IRQ_GPIO_GPADC5_R_INT8,
	S2MPS22_IRQ_GPIO_GPADC6_F_INT8,
	S2MPS22_IRQ_GPIO_GPADC6_R_INT8,

	S2MPS22_IRQ_NR,
};


enum sec_device_type {
	S2MPS22X,
};

struct s2mps22_dev {
	/* pmic VER/REV register */
	u8 pmic_rev;	/* pmic Rev */

	bool wakeup;

	int adc_mode;
	int adc_sync_mode;
	int type;
	int device_type;

	int irq;
	int irq_base;
	int irq_gpio;
	int irq_masks_cur[S2MPS22_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPS22_IRQ_GROUP_NR];

	struct device *dev;
	struct i2c_client *i2c;
	struct i2c_client *pmic;
	struct i2c_client *debug_i2c;
	struct mutex i2c_lock;

	struct apm_ops *ops;
	struct mutex irqlock;
	struct s2mps22_platform_data *pdata;
#ifdef CONFIG_DRV_SAMSUNG_PMIC
	struct device *powermeter_dev;
#endif
#ifdef CONFIG_SEC_PM
	struct device *ap_sub_pmic_dev;
#endif /* CONFIG_SEC_PM */
};

enum s2mps22_types {
	TYPE_S2MPS22,
};

extern int s2mps22_irq_init(struct s2mps22_dev *s2mps22);
extern void s2mps22_irq_exit(struct s2mps22_dev *s2mps22);

extern void s2mps22_powermeter_init(struct s2mps22_dev *s2mps22);
extern void s2mps22_powermeter_deinit(struct s2mps22_dev *s2mps22);

/* S2MPS22 shared i2c API function */
extern int s2mps22_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int s2mps22_bulk_read(struct i2c_client *i2c, u8 reg, int count,
			     u8 *buf);
extern int s2mps22_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int s2mps22_bulk_write(struct i2c_client *i2c, u8 reg, int count,
			      u8 *buf);
extern int s2mps22_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int s2mps22_read_word(struct i2c_client *i2c, u8 reg);

extern int s2mps22_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#ifdef CONFIG_EXYNOS_OCP
extern void get_s2mps22_i2c(struct i2c_client **i2c);
#endif

#endif /* __LINUX_MFD_S2MPS22_REGULATOR_H */

