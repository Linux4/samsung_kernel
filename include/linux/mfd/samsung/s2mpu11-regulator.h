/*
 * s2mpu11-regulator.h - Voltage regulator driver for the s2mpu11
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

#ifndef __LINUX_MFD_S2MPU11_PRIV_H
#define __LINUX_MFD_S2MPU11_PRIV_H

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define S2MPU11_REG_INVALID			(0xff)
#define S2MPU11_CHANNEL				1

/* PMIC Top-Level Registers */
#define	S2MPU11_PMIC_REG_VGPIO0			0x00
#define	S2MPU11_PMIC_REG_VGPIO1			0x01
#define	S2MPU11_PMIC_REG_VGPIO2			0x02
#define	S2MPU11_PMIC_REG_VGPIO3			0x03

#define	S2MPU11_PMIC_REG_CHIP_ID		0x04
#define S2MPU11_PMIC_REG_I3C_CONFIG		0x05
#define S2MPU11_PMIC_REG_I3C_SATA		0x06
#define S2MPU11_PMIC_REG_IRQM			0x07
#define S2MPU11_PMIC_REG_SEQ_CTRL		0x08

#define S2MPU11_PMIC_REG_CDC_IRQM_MASK		0x02
#define S2MPU11_PMIC_REG_PM_IRQM_MASK		0x01

/* Slave addr = 0xCE */
/* PMIC Registers */
#define S2MPU11_PMIC_REG_INT1			0x00
#define S2MPU11_PMIC_REG_INT2			0x01
#define S2MPU11_PMIC_REG_INT3			0x02
#define S2MPU11_PMIC_REG_INT1M			0x03
#define S2MPU11_PMIC_REG_INT2M			0x04
#define S2MPU11_PMIC_REG_INT3M			0x05

#define S2MPU11_PMIC_REG_STATUS1		0x06
#define S2MPU11_PMIC_REG_OFFSRC			0x08

#define S2MPU11_PMIC_REG_CTRL1			0x09
#define S2MPU11_PMIC_REG_CTRL2			0x0A
#define S2MPU11_PMIC_REG_CTRL3			0x0B

#define S2MPU11_PMIC_REG_CFG_PM			0x0C
#define S2MPU11_PMIC_REG_TIME_CTRL		0x0D
#define S2MPU11_PMIC_REG_UVLO_CTRL		0x0E

#define S2MPU11_PMIC_REG_B1CTRL			0x0F
#define S2MPU11_PMIC_REG_B1OUT			0x10
#define S2MPU11_PMIC_REG_B2CTRL			0x11
#define S2MPU11_PMIC_REG_B2OUT			0x12
#define S2MPU11_PMIC_REG_B3CTRL			0x13
#define S2MPU11_PMIC_REG_B3OUT			0x14
#define S2MPU11_PMIC_REG_B4CTRL			0x15
#define S2MPU11_PMIC_REG_B4OUT			0x16
#define S2MPU11_PMIC_REG_B5CTRL			0x17
#define S2MPU11_PMIC_REG_B5OUT			0x18

#define S2MPU11_PMIC_REG_BUCK_RISE_RAMP		0x19
#define S2MPU11_PMIC_REG_BUCK_FALL_RAMP		0x1A

#define S2MPU11_PMIC_REG_L1CTRL			0x1B
#define S2MPU11_PMIC_REG_L2CTRL			0x1C
#define S2MPU11_PMIC_REG_L3CTRL			0x1D
#define S2MPU11_PMIC_REG_L4CTRL			0x1E
#define S2MPU11_PMIC_REG_L5CTRL			0x1F
#define S2MPU11_PMIC_REG_L6CTRL			0x20
#define S2MPU11_PMIC_REG_L7CTRL			0x21
#define S2MPU11_PMIC_REG_L8CTRL			0x22
#define S2MPU11_PMIC_REG_L9CTRL			0x23
#define S2MPU11_PMIC_REG_L10CTRL		0x24
#define S2MPU11_PMIC_REG_L11CTRL		0x25
#define S2MPU11_PMIC_REG_L12CTRL		0x26
#define S2MPU11_PMIC_REG_L13CTRL		0x27
#define S2MPU11_PMIC_REG_L14CTRL		0x28
#define S2MPU11_PMIC_REG_L15CTRL		0x29

#define S2MPU11_PMIC_REG_LDO_DSCH1		0x2A
#define S2MPU11_PMIC_REG_LDO_DSCH2		0x2B

#define S2MPU11_PMIC_REG_SEL_VGPIO0		0x2C
#define S2MPU11_PMIC_REG_SEL_VGPIO1		0x2D
#define S2MPU11_PMIC_REG_SEL_VGPIO2		0x2E
#define S2MPU11_PMIC_REG_SEL_VGPIO3		0x2F
#define S2MPU11_PMIC_REG_SEL_VGPIO4		0x30
#define S2MPU11_PMIC_REG_SEL_VGPIO5		0x31
#define S2MPU11_PMIC_REG_SEL_VGPIO6		0x32
#define S2MPU11_PMIC_REG_SEL_VGPIO7		0x33
#define S2MPU11_PMIC_REG_SEL_VGPIO8		0x34
#define S2MPU11_PMIC_REG_SEL_VGPIO9		0x35

#define S2MPU11_PMIC_REG_ON_SEQ_SEL1		0x36
#define S2MPU11_PMIC_REG_ON_SEQ_SEL2		0x37
#define S2MPU11_PMIC_REG_ON_SEQ_SEL3		0x38
#define S2MPU11_PMIC_REG_ON_SEQ_SEL4		0x39
#define S2MPU11_PMIC_REG_ON_SEQ_SEL5		0x3A
#define S2MPU11_PMIC_REG_ON_SEQ_SEL6		0x3B
#define S2MPU11_PMIC_REG_ON_SEQ_SEL7		0x3C
#define S2MPU11_PMIC_REG_ON_SEQ_SEL8		0x3D
#define S2MPU11_PMIC_REG_ON_SEQ_SEL9		0x3E
#define S2MPU11_PMIC_REG_ON_SEQ_SEL10		0x3F

#define S2MPU11_PMIC_REG_HOLD_SEQ_SEL1		0x40
#define S2MPU11_PMIC_REG_HOLD_SEQ_SEL2		0x41

#define S2MPU11_PMIC_REG_OFF_SEQ_CTRL1		0x44
#define S2MPU11_PMIC_REG_OFF_SEQ_CTRL2		0x45
#define S2MPU11_PMIC_REG_OFF_SEQ_CTRL3		0x46
#define S2MPU11_PMIC_REG_OFF_SEQ_CTRL4		0x47
#define S2MPU11_PMIC_REG_OFF_SEQ_CTRL5		0x48
#define S2MPU11_PMIC_REG_OFF_SEQ_CTRL6		0x49
#define S2MPU11_PMIC_REG_OFF_SEQ_CTRL7		0x4A
#define S2MPU11_PMIC_REG_OFF_SEQ_CTRL8		0x4B
#define S2MPU11_PMIC_REG_OFF_SEQ_CTRL9		0x4C
#define S2MPU11_PMIC_REG_OFF_SEQ_CTRL10		0x4D

#define S2MPU11_PMIC_REG_OCP_WARN_BUCK2		0x4E
#define S2MPU11_PMIC_REG_BUCK_OI_EN		0x4F
#define S2MPU11_PMIC_REG_BUCK_OI_PD_EN		0x50
#define S2MPU11_PMIC_REG_BUCK_OI_CTRL1		0x51
#define S2MPU11_PMIC_REG_BUCK_OI_CTRL2		0x52
#define S2MPU11_PMIC_REG_BUCK_OI_CTRL3		0x53
#define S2MPU11_PMIC_REG_SUB_CTRL1		0x54
#define S2MPU11_PMIC_REG_SUB_CTRL2		0x55
#define S2MPU11_PMIC_REG_SEQ_CTRL2		0x56
#define S2MPU11_PMIC_REG_EXT_CTRL		0xFE

/* Mask */
#define S2MPU11_WTSREN_MASK			0x04
#define S2MPU11_PM_IRQM_MASK			0x01

#define S2MPU11_OCP_WARN_MASK			0xE0
#define S2MPU11_OCP_WARN_EN			7
#define S2MPU11_OCP_WARN_CNT			6
#define S2MPU11_OCP_WARN_DVS_MASK		5

/* S2MPU11 regulator ids */
enum S2MPU11_regulators {
	/* 15 LDOs */
/*
	S2MPU11_LDO1,
	S2MPU11_LDO2,
	S2MPU11_LDO3,
	S2MPU11_LDO4,
	S2MPU11_LDO5,
	S2MPU11_LDO6,
	S2MPU11_LDO7,
	S2MPU11_LDO8,
	S2MPU11_LDO9,
	S2MPU11_LDO10,
	S2MPU11_LDO11,
	S2MPU11_LDO12,
	S2MPU11_LDO13,
 */
	S2MPU11_LDO14,
	S2MPU11_LDO15,
	/* 5 BUCKs */
	S2MPU11_BUCK1,
	S2MPU11_BUCK2,
	S2MPU11_BUCK3,
/*
	S2MPU11_BUCK4,
	S2MPU11_BUCK5,
 */
	S2MPU11_REG_MAX,
};
/* BUCK 1S_2S_3S_4S_5S */
#define S2MPU11_BUCK_MIN1		300000
#define S2MPU11_BUCK_STEP1		6250
/* LDO 1S_2S_6S_8S_9S_ALDO2(LDO15) */
#define S2MPU11_LDO_MIN1		700000
#define S2MPU11_LDO_STEP1		12500
/* LDO 3S_4S_7S_10S_11S_ALDO1(LDO14) */
#define S2MPU11_LDO_MIN2		700000
#define S2MPU11_LDO_STEP2		25000
/* LDO 5S_12S_13S */
#define S2MPU11_LDO_MIN3		1800000
#define S2MPU11_LDO_STEP3		25000
/* LDO/BUCK output voltage control */
#define S2MPU11_LDO_VSEL_MASK		0x3F
#define S2MPU11_BUCK_VSEL_MASK		0xFF
#define S2MPU11_LDO_N_VOLTAGES		(S2MPU11_LDO_VSEL_MASK + 1)
#define S2MPU11_BUCK_N_VOLTAGES		(S2MPU11_BUCK_VSEL_MASK + 1)
/* Enable control[7:6] */
#define S2MPU11_ENABLE_SHIFT		0x06
#define S2MPU11_ENABLE_MASK		(0x03 << S2MPU11_ENABLE_SHIFT)

#define S2MPU11_REGULATOR_MAX           (S2MPU11_REG_MAX)
/* TODO:Set LDO/BUCK time */
#define S2MPU11_ENABLE_TIME_LDO		128
#define S2MPU11_ENABLE_TIME_BUCK1	110
#define S2MPU11_ENABLE_TIME_BUCK2	110
#define S2MPU11_ENABLE_TIME_BUCK3	110
#define S2MPU11_ENABLE_TIME_BUCK4	150
#define S2MPU11_ENABLE_TIME_BUCK5	150

#define S2MPU11_PMIC_IRQ_OI_B1_SHIFT	0
#define S2MPU11_PMIC_IRQ_OI_B1_MASK	BIT(S2MPU11_PMIC_IRQ_OI_B1_SHIFT)
#define S2MPU11_PMIC_IRQ_OI_B2_SHIFT	1
#define S2MPU11_PMIC_IRQ_OI_B2_MASK	BIT(S2MPU11_PMIC_IRQ_OI_B2_SHIFT)
#define S2MPU11_PMIC_IRQ_OI_B3_SHIFT	2
#define S2MPU11_PMIC_IRQ_OI_B3_MASK	BIT(S2MPU11_PMIC_IRQ_OI_B3_SHIFT)
#define S2MPU11_PMIC_IRQ_OI_B4_SHIFT	3
#define S2MPU11_PMIC_IRQ_OI_B4_MASK	BIT(S2MPU11_PMIC_IRQ_OI_B4_SHIFT)
#define S2MPU11_PMIC_IRQ_OI_B5_SHIFT	4
#define S2MPU11_PMIC_IRQ_OI_B5_MASK	BIT(S2MPU11_PMIC_IRQ_OI_B5_SHIFT)

#define SEC_PMIC_REV(iodev)		(iodev)->pmic_rev

/*
 * sec_opmode_data - regulator operation mode data
 * @id: regulator id
 * @mode: regulator operation mode
 */

enum s2mpu11_irq_source {
	S2MPU11_PMIC_INT1 = 0,
	S2MPU11_PMIC_INT2,
	S2MPU11_PMIC_INT3,

	S2MPU11_IRQ_GROUP_NR,
};

#define S2MPU11_BUCK_MAX		5
#define S2MPU11_NUM_IRQ_PMIC_REGS	3

enum s2mpu11_irq {
	/* PMIC */
	S2MPU11_PMIC_IRQ_PWRONF_INT1,
	S2MPU11_PMIC_IRQ_PWRONR_INT1,
	S2MPU11_PMIC_IRQ_INT120C_INT1,
	S2MPU11_PMIC_IRQ_INT140C_INT1,
	S2MPU11_PMIC_IRQ_TSD_INT1,
	S2MPU11_PMIC_IRQ_WTSR_INT1,
	S2MPU11_PMIC_IRQ_WRSTB_INT1,

	S2MPU11_PMIC_IRQ_OCP_B1_INT2,
	S2MPU11_PMIC_IRQ_OCP_B2_INT2,
	S2MPU11_PMIC_IRQ_OCP_B3_INT2,
	S2MPU11_PMIC_IRQ_OCP_B4_INT2,
	S2MPU11_PMIC_IRQ_OCP_B5_INT2,

	S2MPU11_PMIC_IRQ_OI_B1_INT3,
	S2MPU11_PMIC_IRQ_OI_B2_INT3,
	S2MPU11_PMIC_IRQ_OI_B3_INT3,
	S2MPU11_PMIC_IRQ_OI_B4_INT3,
	S2MPU11_PMIC_IRQ_OI_B5_INT3,

	S2MPU11_IRQ_NR,
};

enum s2mpu11_irq_type {
	S2MPU11_IRQ_OI_B1 = 1,
	S2MPU11_IRQ_OI_B2,
	S2MPU11_IRQ_OI_B3,
	S2MPU11_IRQ_OI_B4,
	S2MPU11_IRQ_OI_B5,
};

enum s2mpu11_device_type {
	S2MPU11X,
};

struct s2mpu11_dev {
	struct s2mpu11_platform_data *pdata;
	struct device *dev;
	struct i2c_client *i2c;
	struct i2c_client *pmic;
	struct mutex i2c_lock;
	struct mutex irq_lock;
	struct apm_ops *ops;

	int type;
	int device_type;
	bool wakeup;
	/* pmic REV register */
	u8 pmic_rev;	/* pmic Rev */

	/* Work queue */
	struct workqueue_struct *irq_wqueue;
	struct delayed_work irq_work;
};

enum s2mpu11_types {
	TYPE_S2MPU11,
};

/* S2MPU11 shared i2c API function */
extern int s2mpu11_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int s2mpu11_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mpu11_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int s2mpu11_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mpu11_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int s2mpu11_read_word(struct i2c_client *i2c, u8 reg);

extern int s2mpu11_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
/* notifier */
extern void s2mpu11_call_notifier(void);
extern int s2mpu11_notifier_init(struct s2mpu11_dev *s2mpu11);
#endif /* __LINUX_MFD_S2MPU11_PRIV_H */
