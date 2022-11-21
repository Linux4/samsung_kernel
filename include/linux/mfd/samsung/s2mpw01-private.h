/*
 * s2mpw01-private.h - Voltage regulator driver for the s2mpw01
 *
 *  Copyright (C) 2015 Samsung Electrnoics
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

#ifndef __LINUX_MFD_S2MPW01_PRIV_H
#define __LINUX_MFD_S2MPW01_PRIV_H

#include <linux/i2c.h>


#define S2MPW01_I2C_ADDR		(0x66)
#define S2MPW01_REG_INVALID		(0xff)

#define S2MPW01_IRQSRC_PMIC		(1 << 0)
#define S2MPW01_IRQSRC_CHG		(1 << 1)
#define S2MPW01_IRQSRC_FG       	(1 << 2)


/* Slave addr = 0x66 */
/* PMIC Top-Level Registers */
#define	S2MPW01_PMIC_REG_PMICID		(0x00)
#define	S2MPW01_PMIC_REG_INTSRC		(0x05)
#define	S2MPW01_PMIC_REG_INTSRC_MASK	(0x06)


/* Slave addr = 0xCC */
/* PMIC Registers */
#define S2MPW01_PMIC_REG_INT1		(0x00)
#define S2MPW01_PMIC_REG_INT2	0x01
#define S2MPW01_PMIC_REG_INT3	0x02
#define S2MPW01_PMIC_REG_INT1M	0x03
#define S2MPW01_PMIC_REG_INT2M	0x04
#define S2MPW01_PMIC_REG_INT3M	0x05
#define S2MPW01_PMIC_REG_STATUS1	0x06
#define S2MPW01_PMIC_REG_STATUS2	0x07
#define S2MPW01_PMIC_REG_PWRONSRC	0x08
#define S2MPW01_PMIC_REG_OFFSRC	0x09

#define S2MPW01_PMIC_REG_RTCBUF	0x0B
#define S2MPW01_PMIC_REG_CTRL1	0x0C

#define S2MPW01_PMIC_REG_CTRL3	0x0E

#define S2MPW01_PMIC_REG_B1CTRL		0x13
#define S2MPW01_PMIC_REG_B1OUT1		0x14
#define S2MPW01_PMIC_REG_B1OUT2		0x15
#define S2MPW01_PMIC_REG_B1OUT3		0x16
#define S2MPW01_PMIC_REG_B2CTRL1	0x17
#define S2MPW01_PMIC_REG_B2CTRL2	0x18
#define S2MPW01_PMIC_REG_B3CTRL1	0x19
#define S2MPW01_PMIC_REG_B3CTRL2	0x1A
#define S2MPW01_PMIC_REG_B4CTRL1	0x1B
#define S2MPW01_PMIC_REG_B4CTRL2	0x1C
#define S2MPW01_PMIC_REG_RAMP	0x1D
#define S2MPW01_PMIC_REG_BSTCTRL 0x1E
#define S2MPW01_PMIC_REG_L1DVS 0x1F
#define S2MPW01_PMIC_REG_L1CTRL1 0x20
#define S2MPW01_PMIC_REG_L1CTRL2 0x21
#define S2MPW01_PMIC_REG_L2CTRL1 0x22
#define S2MPW01_PMIC_REG_L2CTRL2 0x23
#define S2MPW01_PMIC_REG_L3CTRL 0x24
#define S2MPW01_PMIC_REG_L4CTRL 0x25
#define S2MPW01_PMIC_REG_L5CTRL 0x26
#define S2MPW01_PMIC_REG_L6CTRL 0x27
#define S2MPW01_PMIC_REG_L7CTRL 0x28
#define S2MPW01_PMIC_REG_L8CTRL 0x29
#define S2MPW01_PMIC_REG_L9CTRL 0x2A
#define S2MPW01_PMIC_REG_L10CTRL 0x2B
#define S2MPW01_PMIC_REG_L11CTRL 0x2C
#define S2MPW01_PMIC_REG_L12CTRL 0x2D
#define S2MPW01_PMIC_REG_L13CTRL 0x2E
#define S2MPW01_PMIC_REG_L14CTRL 0x2F
#define S2MPW01_PMIC_REG_L15CTRL 0x30
#define S2MPW01_PMIC_REG_L16CTRL 0x31
#define S2MPW01_PMIC_REG_L17CTRL 0x32
#define S2MPW01_PMIC_REG_L18CTRL 0x33
#define S2MPW01_PMIC_REG_L19CTRL 0x34
#define S2MPW01_PMIC_REG_L20CTRL 0x35
#define S2MPW01_PMIC_REG_L21CTRL 0x36
#define S2MPW01_PMIC_REG_L22CTRL 0x37
#define S2MPW01_PMIC_REG_L23CTRL 0x38
#define S2MPW01_PMIC_REG_LDO_DSCH1 0x39
#define S2MPW01_PMIC_REG_LDO_DSCH2 0x3A
#define S2MPW01_PMIC_REG_LDO_DSCH3 0x3B
#define S2MPW01_PMIC_REG_EXT_CTRL	0xFF

/* status1 register fields */
#define S2MPW01_STATUS1_PWRON_M		(0x1 << 0)
#define S2MPW01_STATUS1_JIGONB_M	(0x1 << 1)
#define S2MPW01_STATUS1_ACOKB_M		(0x1 << 2)
#define S2MPW01_STATUS1_MRB_M		(0x1 << 4)
#define S2MPW01_STATUS1_PWRON1S_M	(0x1 << 5)
#define S2MPW01_STATUS1_INT120CS_M	(0x1 << 6)
#define S2MPW01_STATUS1_INT140CS_M	(0x1 << 7)


/* S2MPW01regulator ids */
enum S2MPW01_regulators {
	S2MPW01_LDO1,
	S2MPW01_LDO2,
	S2MPW01_LDO3,
	S2MPW01_LDO4,
	S2MPW01_LDO5,
	S2MPW01_LDO6,
	S2MPW01_LDO7,
	S2MPW01_LDO8,
	S2MPW01_LDO9,
	S2MPW01_LDO10,
	S2MPW01_LDO11,
	S2MPW01_LDO12,
	S2MPW01_LDO13,
	S2MPW01_LDO14,
	S2MPW01_LDO15,
	S2MPW01_LDO16,
	S2MPW01_LDO17,
	S2MPW01_LDO18,
	S2MPW01_LDO19,
	S2MPW01_LDO20,
	S2MPW01_LDO21,
	S2MPW01_LDO22,
	S2MPW01_LDO23,
	S2MPW01_BUCK1,
	S2MPW01_BUCK3,
	S2MPW01_BUCK4,
	S2MPW01_REG_MAX,
};

#define S2MPW01_BUCK_MIN1	400000
#define S2MPW01_BUCK_MIN2	600000
#define S2MPW01_LDO_MIN1	400000
#define S2MPW01_LDO_MIN2	800000
#define S2MPW01_LDO_MIN3	1800000
#define S2MPW01_BUCK_STEP1	6250
#define S2MPW01_BUCK_STEP2	12500
#define S2MPW01_LDO_STEP1	12500
#define S2MPW01_LDO_STEP2	25000
#define S2MPW01_LDO_VSEL_MASK	0x3F
#define S2MPW01_BUCK_VSEL_MASK	0xFF
#define S2MPW01_ENABLE_MASK	(0x03 << S2MPW01_ENABLE_SHIFT)
#define S2MPW01_SW_ENABLE_MASK	0x03
#define S2MPW01_RAMP_DELAY	12000

#define S2MPW01_ENABLE_TIME_LDO		128
#define S2MPW01_ENABLE_TIME_BUCK1	95
#define S2MPW01_ENABLE_TIME_BUCK2	106
#define S2MPW01_ENABLE_TIME_BUCK3	150
#define S2MPW01_ENABLE_TIME_BUCK4	150

#define S2MPW01_ENABLE_SHIFT	0x06
#define S2MPW01_LDO_N_VOLTAGES	(S2MPW01_LDO_VSEL_MASK + 1)
#define S2MPW01_BUCK_N_VOLTAGES (S2MPW01_BUCK_VSEL_MASK + 1)

#define S2MPW01_PMIC_EN_SHIFT	6
#define S2MPW01_REGULATOR_MAX (S2MPW01_REG_MAX)


#define SEC_PMIC_REV(iodev)	((iodev)->pmic_rev)


/*
 * sec_opmode_data - regulator operation mode data
 * @id: regulator id
 * @mode: regulator operation mode
 */

enum s2mpw01_irq_source {
	PMIC_INT1 = 0,
	PMIC_INT2,
	PMIC_INT3,
#if defined(CONFIG_SEC_CHARGER_S2MPW01)
	CHG_INT1,
	CHG_INT2,
	CHG_INT3,
#endif
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
	FG_INT,
#endif
	S2MPW01_IRQ_GROUP_NR,
};

#define S2MPW01_NUM_IRQ_PMIC_REGS	3
#define S2MPW01_NUM_IRQ_CHG_REGS	3
#define S2MPW01_NUM_IRQ_FG_REGS		1

enum s2mpw01_irq {
	/* PMIC */
	S2MPW01_PMIC_IRQ_PWRONR_INT1,
	S2MPW01_PMIC_IRQ_PWRONF_INT1,
	S2MPW01_PMIC_IRQ_JIGONBF_INT1,
	S2MPW01_PMIC_IRQ_JIGONBR_INT1,
	S2MPW01_PMIC_IRQ_ACOKBF_INT1,
	S2MPW01_PMIC_IRQ_ACOKBR_INT1,
	S2MPW01_PMIC_IRQ_PWRON1S_INT1,
	S2MPW01_PMIC_IRQ_MRB_INT1,

	S2MPW01_PMIC_IRQ_RTC60S_INT2,
	S2MPW01_PMIC_IRQ_RTCA1_INT2,
	S2MPW01_PMIC_IRQ_RTCA0_INT2,
	S2MPW01_PMIC_IRQ_SMPL_INT2,
	S2MPW01_PMIC_IRQ_RTC1S_INT2,
	S2MPW01_PMIC_IRQ_WTSR_INT2,
	S2MPW01_PMIC_IRQ_WRSTB_INT2,

	S2MPW01_PMIC_IRQ_120C_INT3,
	S2MPW01_PMIC_IRQ_140C_INT3,
	S2MPW01_PMIC_IRQ_TSD_INT3,
#ifdef CONFIG_SEC_CHARGER_S2MPW01
	/* Charger */
	S2MPW01_CHG_IRQ_RECHG_INT1,
	S2MPW01_CHG_IRQ_CHGDONE_INT1,
	S2MPW01_CHG_IRQ_TOPOFF_INT1,
	S2MPW01_CHG_IRQ_PREECHG_INT1,
	S2MPW01_CHG_IRQ_CHGSTS_INT1,
	S2MPW01_CHG_IRQ_CIN2BAT_INT1,
	S2MPW01_CHG_IRQ_CHGVINOVP_INT1,
	S2MPW01_CHG_IRQ_CHGVIN_INT1,

	S2MPW01_CHG_IRQ_ADPATH_INT2,
	S2MPW01_CHG_IRQ_FCHG_INT2,
	S2MPW01_CHG_IRQ_BATDET_INT2,

	S2MPW01_CHG_IRQ_CVOK_INT3,
	S2MPW01_CHG_IRQ_TMROUT_INT3,
	S2MPW01_CHG_IRQ_CHGTSDINT3,
#endif
#ifdef CONFIG_SEC_FUELGAUGE_S2MPW01
	/* Fuelgauge */
	S2MPW01_FG_IRQ_VBAT_L_INT,
	S2MPW01_FG_IRQ_SOC_L_INT,
	S2MPW01_FG_IRQ_IDLE_ST_INT,
	S2MPW01_FG_IRQ_INIT_ST_INT,
#endif
	S2MPW01_IRQ_NR,
};


enum sec_device_type {

	S2MPW01X,
};

struct s2mpw01_dev {
	struct device *dev;
	struct i2c_client *i2c; /* 0x66; TOP */
	struct i2c_client *pmic; /* 0xCC; Regulator */
	struct i2c_client *rtc; /* 0x0C; RTC */
	struct i2c_client *charger; /* 0x68; Charger */
	struct i2c_client *fuelgauge; /* 0xCE; Fuelgauge */
	struct mutex i2c_lock;
	struct apm_ops *ops;

	int type;
	int device_type;
	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[S2MPW01_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPW01_IRQ_GROUP_NR];

	/* pmic VER/REV register */
	u8 pmic_rev;	/* pmic Rev */
	u8 pmic_ver;	/* pmic version */

	struct s2mpw01_platform_data *pdata;
};

enum s2mpw01_types {
	TYPE_S2MPW01,
};

extern int s2mpw01_irq_init(struct s2mpw01_dev *s2mpw01);
extern void s2mpw01_irq_exit(struct s2mpw01_dev *s2mpw01);

/* S2MPW01 shared i2c API function */
extern int s2mpw01_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int s2mpw01_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mpw01_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int s2mpw01_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mpw01_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int s2mpw01_read_word(struct i2c_client *i2c, u8 reg);

extern int s2mpw01_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#endif /* __LINUX_MFD_S2MPW01_PRIV_H */

