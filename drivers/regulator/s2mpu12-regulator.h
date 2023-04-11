/*
 * s2mpu12-regulator.h - Voltage regulator driver for the s2mpu12
 *
 *  Copyright (C) 2022 Samsung Electrnoics
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

#ifndef __LINUX_MFD_S2MPU12_PRIV_H
#define __LINUX_MFD_S2MPU12_PRIV_H

#include <linux/i2c.h>
#define S2MPU12_REG_INVALID			(0xff)

/* PMIC Top-Level Registers */
#define	S2MPU12_PMIC_VGPIO0			0x00
#define	S2MPU12_PMIC_VGPIO1			0x01
#define	S2MPU12_PMIC_VGPIO2			0x02
#define	S2MPU12_PMIC_VGPIO3			0x03
#define S2MPU12_PMIC_CHIPID			0x04

#define S2MPU12_PMIC_IRQM			0x07
#define S2MPU12_IRQSRC_PMIC			(1 << 0)
#define S2MPU12_IRQSRC_CDC			(1 << 1)
#define S2MPU12_IRQSRC_DCXO			(1 << 2)

/* IBI Map */
#define S2MPU12_IBI0_CODEC			(1 << 3)
#define S2MPU12_IBI0_PMIC_M			(1 << 4)

/* Slave addr = 0xCC */
/* PMIC Registers */
#define S2MPU12_PMIC_INT1			0x00
#define S2MPU12_PMIC_INT2			0x01
#define S2MPU12_PMIC_INT3			0x02
#define S2MPU12_PMIC_INT4			0x03
#define S2MPU12_PMIC_INT5			0x04
#define S2MPU12_PMIC_INT6			0x05
#define S2MPU12_PMIC_INT1M			0x06
#define S2MPU12_PMIC_INT2M			0x07
#define S2MPU12_PMIC_INT3M			0x08
#define S2MPU12_PMIC_INT4M			0x09
#define S2MPU12_PMIC_INT5M			0x0A
#define S2MPU12_PMIC_INT6M			0x0B
#define S2MPU12_PMIC_STATUS1			0x0C
#define S2MPU12_PMIC_STATUS2			0x0D
#define S2MPU12_PMIC_PWRONSRC			0x0E
#define S2MPU12_PMIC_OFFSRC			0x0F

#define S2MPU12_PMIC_BUCHG			0x10
#define S2MPU12_PMIC_RTCBUF			0x11
#define S2MPU12_PMIC_CTRL1			0x12
#define S2MPU12_PMIC_CTRL2			0x13
#define S2MPU12_PMIC_CTRL3			0x14
#define S2MPU12_PMIC_UVLO_CTRL			0x15
#define S2MPU12_PMIC_CFG1			0x16
#define S2MPU12_PMIC_CFG2			0x17

#define S2MPU12_PMIC_B1CTRL			0x18
#define S2MPU12_PMIC_B1OUT1			0x19
#define S2MPU12_PMIC_B1OUT2			0x1A
#define S2MPU12_PMIC_B2CTRL			0x1B
#define S2MPU12_PMIC_B2OUT1			0x1C
#define S2MPU12_PMIC_B2OUT2			0x1D
#define S2MPU12_PMIC_B3CTRL			0x1E
#define S2MPU12_PMIC_B3OUT			0x1F
#define S2MPU12_PMIC_B4CTRL			0x20
#define S2MPU12_PMIC_B4OUT1			0x21
#define S2MPU12_PMIC_B4OUT2			0x22
#define S2MPU12_PMIC_B5CTRL			0x23
#define S2MPU12_PMIC_B5OUT1			0x24
#define S2MPU12_PMIC_B5OUT2			0x25

#define S2MPU12_PMIC_BST_CTRL			0x26
#define S2MPU12_PMIC_BUCK_RISE_RAMP		0x27
#define S2MPU12_PMIC_BUCK_FALL_RAMP		0x28
#define S2MPU12_PMIC_DVS_LDO8_CTRL		0x29
#define S2MPU12_PMIC_DVS_LDO9_CTRL		0x2A

#define S2MPU12_PMIC_L1CTRL			0x2B
#define S2MPU12_PMIC_L2CTRL			0x2C
#define S2MPU12_PMIC_L3CTRL			0x2D
#define S2MPU12_PMIC_L4CTRL			0x2E
#define S2MPU12_PMIC_L5CTRL			0x2F
#define S2MPU12_PMIC_L6CTRL			0x30
#define S2MPU12_PMIC_L7CTRL			0x31
#define S2MPU12_PMIC_L8CTRL			0x32
#define S2MPU12_PMIC_L9CTRL			0x33
#define S2MPU12_PMIC_L10CTRL			0x34
#define S2MPU12_PMIC_L11CTRL			0x35
#define S2MPU12_PMIC_L12CTRL			0x36
#define S2MPU12_PMIC_L13CTRL			0x37
#define S2MPU12_PMIC_L14CTRL			0x38
#define S2MPU12_PMIC_L15CTRL			0x39
#define S2MPU12_PMIC_L16CTRL			0x3A
#define S2MPU12_PMIC_L17CTRL			0x3B
#define S2MPU12_PMIC_L18CTRL			0x3C
#define S2MPU12_PMIC_L19CTRL			0x3D
#define S2MPU12_PMIC_L20CTRL			0x3E
#define S2MPU12_PMIC_L21CTRL			0x3F
#define S2MPU12_PMIC_L22CTRL			0x40
#define S2MPU12_PMIC_L23CTRL			0x41
#define S2MPU12_PMIC_L24CTRL			0x42
#define S2MPU12_PMIC_L25CTRL			0x43
#define S2MPU12_PMIC_L26CTRL			0x44
#define S2MPU12_PMIC_L27CTRL			0x45
#define S2MPU12_PMIC_L28CTRL			0x46
#define S2MPU12_PMIC_L29CTRL			0x47
#define S2MPU12_PMIC_L30CTRL			0x48
#define S2MPU12_PMIC_L31CTRL			0x49
#define S2MPU12_PMIC_L32CTRL			0x4A
#define S2MPU12_PMIC_L33CTRL			0x4B /* ALDO1_CTRL */
#define S2MPU12_PMIC_L34CTRL			0x4C /* ALDO2_CTRL */
#define S2MPU12_PMIC_L35CTRL			0x4D /* DLDO_CORE_CTRL */
#define S2MPU12_PMIC_L36CTRL			0x4E /* DLDO_BUF_CTRL */

#define S2MPU12_PMIC_LDO_DSCH1			0x4F
#define S2MPU12_PMIC_LDO_DSCH2			0x50
#define S2MPU12_PMIC_LDO_DSCH3			0x51
#define S2MPU12_PMIC_LDO_DSCH4			0x52
#define S2MPU12_PMIC_LDO_DSCH5			0x53

#define S2MPU12_PMIC_SEL_VGPIO0			0x54
#define S2MPU12_PMIC_SEL_VGPIO1			0x55
#define S2MPU12_PMIC_SEL_VGPIO2			0x56
#define S2MPU12_PMIC_SEL_VGPIO3			0x57
#define S2MPU12_PMIC_SEL_VGPIO4			0x58
#define S2MPU12_PMIC_SEL_VGPIO5			0x59
#define S2MPU12_PMIC_SEL_VGPIO6			0x5A
#define S2MPU12_PMIC_SEL_VGPIO7			0x5B
#define S2MPU12_PMIC_SEL_VGPIO8			0x5C
#define S2MPU12_PMIC_SEL_VGPIO9			0x5D
#define S2MPU12_PMIC_SEL_VGPIO10		0x5E

#define S2MPU12_PMIC_DVS_SEL0			0x5F
#define S2MPU12_PMIC_DVS_SEL1			0x60
/* TODO: check off-sequence */
#define S2MPU12_PMIC_ON_SEQ_SEL1		0x61

#define S2MPU12_PMIC_BUCK_OI_EN			0x89
#define S2MPU12_PMIC_BUCK_OI_PD_EN		0x8A
#define S2MPU12_PMIC_BUCK_OI_CTRL1		0x8B
#define S2MPU12_PMIC_BUCK_OI_CTRL2		0x8C
#define S2MPU12_PMIC_BUCK_OI_CTRL3		0x8D
#define S2MPU12_PMIC_DCXO_CTRL1			0x8E
#define S2MPU12_PMIC_DCXO_CTRL2			0x8F
#define S2MPU12_PMIC_DCXO_CTRL3			0x90

/* S2MPU12 regulator ids */
enum S2MPU12_regulators {
	/* 36 LDOs */
	S2MPU12_LDO1,
	S2MPU12_LDO2,
	S2MPU12_LDO3,
	S2MPU12_LDO4,
	S2MPU12_LDO5,
	S2MPU12_LDO6,
	S2MPU12_LDO7,
	S2MPU12_LDO8,
	S2MPU12_LDO9,
	S2MPU12_LDO10,
	S2MPU12_LDO11,
/*
	S2MPU12_LDO12,
	S2MPU12_LDO13,
	S2MPU12_LDO14,
	S2MPU12_LDO15,
	S2MPU12_LDO16,
	S2MPU12_LDO17,
	S2MPU12_LDO18,
	S2MPU12_LDO19,
	S2MPU12_LDO20,
	S2MPU12_LDO21,
	S2MPU12_LDO22,
 */
	S2MPU12_LDO23,
	S2MPU12_LDO24,
	S2MPU12_LDO25,
	S2MPU12_LDO26,
	S2MPU12_LDO27,
	S2MPU12_LDO28,
	S2MPU12_LDO29,
	S2MPU12_LDO30,
	S2MPU12_LDO31,
	S2MPU12_LDO32,
	S2MPU12_LDO33,
	S2MPU12_LDO34,
	S2MPU12_LDO35,
	S2MPU12_LDO36,
	/* 5 BUCKs */
	S2MPU12_BUCK1,
	S2MPU12_BUCK2,
/*
	S2MPU12_BUCK3,
 */
	S2MPU12_BUCK4,
	S2MPU12_BUCK5,

	S2MPU12_REG_MAX,
};

/* BUCK 1M_2M_3M */
#define S2MPU12_BUCK_MIN1		300000
#define S2MPU12_BUCK_STEP1		6250
/* BUCK 4M */
#define S2MPU12_BUCK_MIN2		300000
#define S2MPU12_BUCK_STEP2		6250
/* BUCK 5M */
#define S2MPU12_BUCK_MIN3		600000
#define S2MPU12_BUCK_STEP3		12500
/* LDO 6M */
#define S2MPU12_LDO_MIN1		400000
#define S2MPU12_LDO_STEP1		12500
/* LDO 1M_3M_5M_7M_12M_13M_17M_18M_19M_29M_DLDO_BUF(36M) */
#define S2MPU12_LDO_MIN2		700000
#define S2MPU12_LDO_STEP2		12500
/* LDO 8M_9M */
#define S2MPU12_LDO_MIN3		300000
#define S2MPU12_LDO_STEP3		25000
/* LDO 2M_4M_14M_20M_28M_30M_ALDO1(33M)_DLDO_CORE(35M) */
#define S2MPU12_LDO_MIN4		700000
#define S2MPU12_LDO_STEP4		25000
/* LDO 10M_11M_15M_16M_21M_22M_23M_24M_25M_26M_27M_31M_32M_ALDO2(34M) */
#define S2MPU12_LDO_MIN5		1800000
#define S2MPU12_LDO_STEP5		25000
/* LDO/BUCK output voltage control */
#define S2MPU12_LDO_VSEL_MASK		0x3F
#define S2MPU12_BUCK_VSEL_MASK		0xFF
#define S2MPU12_LDO_N_VOLTAGES		(S2MPU12_LDO_VSEL_MASK + 1)
#define S2MPU12_BUCK_N_VOLTAGES		(S2MPU12_BUCK_VSEL_MASK + 1)
/* Enable control[7:6] */
#define S2MPU12_ENABLE_SHIFT		0x06
#define S2MPU12_ENABLE_MASK		(0x03 << S2MPU12_ENABLE_SHIFT)
/* Set LDO/BUCK time */
#define S2MPU12_ENABLE_TIME_LDO		128
#define S2MPU12_ENABLE_TIME_BUCK1	110
#define S2MPU12_ENABLE_TIME_BUCK2	110
#define S2MPU12_ENABLE_TIME_BUCK3	110
#define S2MPU12_ENABLE_TIME_BUCK4	150
#define S2MPU12_ENABLE_TIME_BUCK5	150

#define SEC_PMIC_REV(iodev)		(iodev)->pmic_rev

#define V_PWREN_CPUCL0			(0)
#define V_PWREN_MIF			(1)
#define V_PWREN_CP			(2)
#define V_PWREN_CLK			(3)

#define VGPIO_SEL_MASK			(3)
/* SELVGPIO0 */
#define B4_VGPIO_SEL			(6)
#define B3_VGPIO_SEL			(4)
#define B2_VGPIO_SEL			(2)
#define B1_VGPIO_SEL			(0)

/* SELVGPIO2 */
#define L6_VGPIO_SEL			(6)
#define L5_VGPIO_SEL			(4)
#define L4_VGPIO_SEL			(2)
#define L3_VGPIO_SEL			(0)

/* SELVGPIO3 */
#define L10_VGPIO_SEL			(6)
#define L9_VGPIO_SEL			(4)
#define L8_VGPIO_SEL			(2)
#define L7_VGPIO_SEL			(0)

/* SELVGPIO4 */
#define L14_VGPIO_SEL			(6)
#define L13_VGPIO_SEL			(4)
#define L12_VGPIO_SEL			(2)
#define L11_VGPIO_SEL			(0)

/* SELVGPIO5 */
#define L18_VGPIO_SEL			(6)
#define L17_VGPIO_SEL			(4)
#define L16_VGPIO_SEL			(2)
#define L15_VGPIO_SEL			(0)

/* SELVGPIO6 */
#define L22_VGPIO_SEL			(6)
#define L21_VGPIO_SEL			(4)
#define L20_VGPIO_SEL			(2)
#define L19_VGPIO_SEL			(0)

/* SELVGPIO10 */
#define L36_VGPIO_SEL			(2)
#define L35_VGPIO_SEL			(0)
/* Mask */

/* VGPIO_RX_MONITOR ADDR. */
#define VGPIO_I3C_BASE			0x11A50000
#define VGPIO_MONITOR_ADDR		0x1704

/* POWER-KEY MASK */
#define S2MPU12_STATUS1_PWRON		(1 << 0)
#define S2MPU12_RISING_EDGE		(1 << 1)
#define S2MPU12_FALLING_EDGE		(1 << 0)

/* MAX */
#define S2MPU12_REGULATOR_MAX 		(S2MPU12_REG_MAX)
#define S2MPU12_NUM_IRQ_PMIC_REGS	6
#define S2MPU12_NUM_SC_LDO_IRQ          4
#define S2MPU12_BUCK_MAX		5
#define S2MPU12_TEMP_MAX		2
/*
 * sec_opmode_data - regulator operation mode data
 * @id: regulator id
 * @mode: regulator operation mode
 */

enum s2mpu12_irq_source {
	PMIC_INT1 = 0,
	PMIC_INT2,
	PMIC_INT3,
	PMIC_INT4,
	PMIC_INT5,
	PMIC_INT6,

	S2MPU12_IRQ_GROUP_NR,
};

enum s2mpu12_irq {
	S2MPU12_PMIC_IRQ_PWRONR_INT1,
	S2MPU12_PMIC_IRQ_PWRONF_INT1,
	S2MPU12_PMIC_IRQ_JIGONBF_INT1,
	S2MPU12_PMIC_IRQ_JIGONBR_INT1,
	S2MPU12_PMIC_IRQ_ACOKBF_INT1,
	S2MPU12_PMIC_IRQ_ACOKBR_INT1,
	S2MPU12_PMIC_IRQ_PWRON1S_INT1,
	S2MPU12_PMIC_IRQ_MRB_INT1,

	S2MPU12_PMIC_IRQ_RTC60S_INT2,
	S2MPU12_PMIC_IRQ_RTCA1_INT2,
	S2MPU12_PMIC_IRQ_RTCA0_INT2,
	S2MPU12_PMIC_IRQ_SMPL_INT2,
	S2MPU12_PMIC_IRQ_RTC1S_INT2,
	S2MPU12_PMIC_IRQ_WTSR_INT2,
	S2MPU12_PMIC_IRQ_WRSTB_INT2,

	S2MPU12_PMIC_IRQ_OCPB1_INT3,
	S2MPU12_PMIC_IRQ_OCPB2_INT3,
	S2MPU12_PMIC_IRQ_OCPB3_INT3,
	S2MPU12_PMIC_IRQ_OCPB4_INT3,
	S2MPU12_PMIC_IRQ_OCPB5_INT3,

	S2MPU12_PMIC_IRQ_OI_B1_INT5,
	S2MPU12_PMIC_IRQ_OI_B2_INT5,
	S2MPU12_PMIC_IRQ_OI_B3_INT5,
	S2MPU12_PMIC_IRQ_OI_B4_INT5,
	S2MPU12_PMIC_IRQ_OI_B5_INT5,

	S2MPU12_PMIC_IRQ_INT120C_INT6,
	S2MPU12_PMIC_IRQ_INT140C_INT6,
	S2MPU12_PMIC_IRQ_TSD_INT6,

	S2MPU12_IRQ_NR,
};


enum sec_device_type {
	S2MPU12X,
};

struct s2mpu12_dev {
	struct device *dev;
	struct i2c_client *i2c;
	struct i2c_client *pmic;
	struct i2c_client *rtc;
	struct i2c_client *close;
	struct mutex i2c_lock;
	struct apm_ops *ops;

	int type;
	int device_type;
	int irq;
	int irq_base;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[S2MPU12_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPU12_IRQ_GROUP_NR];

	/* pmic REV register */
	u8 pmic_rev;	/* pmic Rev */

	struct s2mpu12_platform_data *pdata;

	/* VGPIO_RX_MONITOR */
	void __iomem *mem_base;

	/* Work queue */
	struct workqueue_struct *irq_wqueue;
	struct delayed_work irq_work;
};

enum s2mpu12_types {
	TYPE_S2MPU12,
};

extern int s2mpu12_irq_init(struct s2mpu12_dev *s2mpu12);
extern void s2mpu12_irq_exit(struct s2mpu12_dev *s2mpu12);

/* S2MPU12 shared i2c API function */
extern int s2mpu12_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int s2mpu12_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mpu12_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int s2mpu12_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mpu12_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int s2mpu12_read_word(struct i2c_client *i2c, u8 reg);

extern int s2mpu12_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

extern int s2mpu12_read_pwron_status(void);
extern int exynos_power_key_pressed_chk(void);

#endif /* __LINUX_MFD_S2MPU12_PRIV_H */
