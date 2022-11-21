/*
 * s2mpu06-private.h - Voltage regulator driver for the s2mpu06
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

#ifndef __LINUX_MFD_S2MPU06_PRIV_H
#define __LINUX_MFD_S2MPU06_PRIV_H

#include <linux/i2c.h>
#define S2MPU06_REG_INVALID             (0xff)
#define S2MPU06_IRQSRC_PMIC		(1 << 0)
#define S2MPU06_IRQSRC_CHG		(1 << 2)
#define S2MPU06_IRQSRC_FG		(1 << 4)
#define S2MPU06_IRQSRC_CODEC		(1 << 1)

/* Slave addr = 0x66 */
/* PMIC Top-Level Registers */
#define	S2MPU06_PMIC_REG_PMICID	0x00
#define	S2MPU06_PMIC_REG_INTSRC 0x05
#define	S2MPU06_PMIC_REG_INTSRC_MASK	0x06
/* Slave addr = 0xCC */
/* PMIC Registers */
#define S2MPU06_PMIC_REG_INT1	0x00
#define S2MPU06_PMIC_REG_INT2	0x01
#define S2MPU06_PMIC_REG_INT3	0x02
#define S2MPU06_PMIC_REG_INT1M	0x03
#define S2MPU06_PMIC_REG_INT2M	0x04
#define S2MPU06_PMIC_REG_INT3M	0x05
#define S2MPU06_PMIC_REG_STATUS1	0x06
#define S2MPU06_PMIC_REG_STATUS2	0x07
#define S2MPU06_PMIC_REG_PWRONSRC	0x08
#define S2MPU06_PMIC_REG_OFFSRC	0x09

#define S2MPU06_PMIC_REG_RTCBUF	0x0B
#define S2MPU06_PMIC_REG_CTRL1	0x0C

#define S2MPU06_PMIC_REG_CTRL3	0x0E

#define S2MPU06_PMIC_REG_B1CTRL		0x13
#define S2MPU06_PMIC_REG_B1OUT1		0x14
#define S2MPU06_PMIC_REG_B1OUT2		0x15
#define S2MPU06_PMIC_REG_B1OUT3		0x16
#define S2MPU06_PMIC_REG_B2CTRL1	0x17
#define S2MPU06_PMIC_REG_B2CTRL2	0x18
#define S2MPU06_PMIC_REG_B3CTRL1	0x19
#define S2MPU06_PMIC_REG_B3CTRL2	0x1A
#define S2MPU06_PMIC_REG_RAMP		0x1B
#define S2MPU06_PMIC_REG_BSTCTRL	0x1C
#define S2MPU06_PMIC_REG_L6DVS		0x1D
#define S2MPU06_PMIC_REG_L1CTRL1	0x1E
#define S2MPU06_PMIC_REG_L1CTRL2	0x1F
#define S2MPU06_PMIC_REG_L2CTRL		0x20
#define S2MPU06_PMIC_REG_L3CTRL		0x21
#define S2MPU06_PMIC_REG_L4CTRL		0x22
#define S2MPU06_PMIC_REG_L5CTRL		0x23
#define S2MPU06_PMIC_REG_L6CTRL1	0x24
#define S2MPU06_PMIC_REG_L6CTRL2	0x25
#define S2MPU06_PMIC_REG_L7CTRL		0x26
#define S2MPU06_PMIC_REG_L8CTRL		0x27
#define S2MPU06_PMIC_REG_L9CTRL		0x28
#define S2MPU06_PMIC_REG_L10CTRL	0x29
#define S2MPU06_PMIC_REG_L11CTRL	0x2A
#define S2MPU06_PMIC_REG_L12CTRL	0x2B
#define S2MPU06_PMIC_REG_L13CTRL	0x2C
#define S2MPU06_PMIC_REG_L14CTRL	0x2D
#define S2MPU06_PMIC_REG_L15CTRL	0x2E
#define S2MPU06_PMIC_REG_L16CTRL	0x2F
#define S2MPU06_PMIC_REG_L17CTRL	0x30
#define S2MPU06_PMIC_REG_L18CTRL	0x31
#define S2MPU06_PMIC_REG_L19CTRL	0x32
#define S2MPU06_PMIC_REG_L20CTRL	0x33
#define S2MPU06_PMIC_REG_L21CTRL	0x34
#define S2MPU06_PMIC_REG_L22CTRL	0x35
#define S2MPU06_PMIC_REG_L23CTRL	0x36
#define S2MPU06_PMIC_REG_L24CTRL	0x37
#define S2MPU06_PMIC_REG_LDO_DSCH1	0x38
#define S2MPU06_PMIC_REG_LDO_DSCH2	0x39
#define S2MPU06_PMIC_REG_LDO_DSCH3	0x3A
#define S2MPU06_PMIC_REG_EXT_CTRL	0xFF

/* Charger INT register */
#define S2MPU06_CHG_REG_INT1                0x00
#define S2MPU06_CHG_REG_INT2                0x01
#define S2MPU06_CHG_REG_INT3                0x02
#define S2MPU06_CHG_REG_PMIC_INT            0x03
#define S2MPU06_CHG_REG_INT1M               0x04
#define S2MPU06_CHG_REG_INT2M               0x05
#define S2MPU06_CHG_REG_INT3M               0x06
#define S2MPU06_CHG_REG_PMIC_INTM           0x07

/* FG INT register */
#define S2MPU06_FG_REG_IRQ_INT		0x02
#define S2MPU06_FG_REG_IRQ_INTM		0x03

/* S2MPU06regulator ids */
enum S2MPU06_regulators {
	S2MPU06_LDO1,
	S2MPU06_LDO2,
	S2MPU06_LDO3,
	S2MPU06_LDO4,
	S2MPU06_LDO5,
	S2MPU06_LDO6,
	S2MPU06_LDO7,
	S2MPU06_LDO8,
	S2MPU06_LDO13,
	S2MPU06_LDO14,
	S2MPU06_LDO15,
	S2MPU06_LDO16,
	S2MPU06_LDO17,
	S2MPU06_LDO18,
	S2MPU06_LDO19,
	S2MPU06_LDO20,
	S2MPU06_LDO21,
	S2MPU06_LDO22,
	S2MPU06_LDO23,
	S2MPU06_LDO24,
	S2MPU06_BUCK1,
	S2MPU06_BUCK2,
	S2MPU06_BUCK3,
	S2MPU06_REG_MAX,
};

#define S2MPU06_BUCK_MIN1	400000
#define S2MPU06_BUCK_MIN2	800000
#define S2MPU06_LDO_MIN1	800000
#define S2MPU06_LDO_MIN2	1800000
#define S2MPU06_LDO_MIN3	400000
#define S2MPU06_BUCK_STEP1	6250
#define S2MPU06_BUCK_STEP2	25000
#define S2MPU06_LDO_STEP1	12500
#define S2MPU06_LDO_STEP2	25000
#define S2MPU06_LDO_VSEL_MASK	0x3F
#define S2MPU06_BUCK_VSEL_MASK	0xFF
#define S2MPU06_ENABLE_MASK	(0x03 << S2MPU06_ENABLE_SHIFT)
#define S2MPU06_SW_ENABLE_MASK	0x03
#define S2MPU06_RAMP_DELAY	12000

#define S2MPU06_ENABLE_TIME_LDO		128
#define S2MPU06_ENABLE_TIME_BUCK1	95
#define S2MPU06_ENABLE_TIME_BUCK2	106
#define S2MPU06_ENABLE_TIME_BUCK3	150

#define S2MPU06_ENABLE_SHIFT	0x06
#define S2MPU06_LDO_N_VOLTAGES	(S2MPU06_LDO_VSEL_MASK + 1)
#define S2MPU06_BUCK_N_VOLTAGES (S2MPU06_BUCK_VSEL_MASK + 1)

#define S2MPU06_PMIC_EN_SHIFT	6
#define S2MPU06_REGULATOR_MAX (S2MPU06_REG_MAX)


#define SEC_PMIC_REV(iodev)	(iodev)->pmic_rev


/*
 * sec_opmode_data - regulator operation mode data
 * @id: regulator id
 * @mode: regulator operation mode
 */

enum s2mpu06_irq_source {
	PMIC_INT1 = 0,
	PMIC_INT2,
	PMIC_INT3,

	CHG_INT1,
	CHG_INT2,
	CHG_INT3,
	CHG_PMIC_INT,

	FG_INT,

	S2MPU06_IRQ_GROUP_NR,
};

#define S2MPU06_NUM_IRQ_PMIC_REGS	3
#define S2MPU06_NUM_IRQ_CHG_REGS	3

enum s2mpu06_irq {
	/* PMIC */
	S2MPU06_PMIC_IRQ_PWRONR_INT1,
	S2MPU06_PMIC_IRQ_PWRONF_INT1,
	S2MPU06_PMIC_IRQ_JIGONBF_INT1,
	S2MPU06_PMIC_IRQ_JIGONBR_INT1,
	S2MPU06_PMIC_IRQ_ACOKBF_INT1,
	S2MPU06_PMIC_IRQ_ACOKBR_INT1,
	S2MPU06_PMIC_IRQ_PWRON1S_INT1,
	S2MPU06_PMIC_IRQ_MRB_INT1,

	S2MPU06_PMIC_IRQ_RTC60S_INT2,
	S2MPU06_PMIC_IRQ_RTCA1_INT2,
	S2MPU06_PMIC_IRQ_RTCA0_INT2,
	S2MPU06_PMIC_IRQ_SMPL_INT2,
	S2MPU06_PMIC_IRQ_RTC1S_INT2,
	S2MPU06_PMIC_IRQ_WTSR_INT2,
	S2MPU06_PMIC_IRQ_WRSTB_INT2,

	S2MPU06_PMIC_IRQ_120C_INT3,
	S2MPU06_PMIC_IRQ_140C_INT3,
	S2MPU06_PMIC_IRQ_TSD_INT3,

	/* Charger */
	S2MPU06_CHG_IRQ_EOC_INT1,
	S2MPU06_CHG_IRQ_CINIR_INT1,
	S2MPU06_CHG_IRQ_BATP_INT1,
	S2MPU06_CHG_IRQ_BATLV_INT1,
	S2MPU06_CHG_IRQ_TOPOFF_INT1,
	S2MPU06_CHG_IRQ_CINOVP_INT1,
	S2MPU06_CHG_IRQ_CHGTSD_INT1,

	S2MPU06_CHG_IRQ_CHGVINVR_INT2,
	S2MPU06_CHG_IRQ_CHGTR_INT2,
	S2MPU06_CHG_IRQ_TMROUT_INT2,
	S2MPU06_CHG_IRQ_RECHG_INT2,
	S2MPU06_CHG_IRQ_CHGTERM_INT2,
	S2MPU06_CHG_IRQ_BATOVP_INT2,
	S2MPU06_CHG_IRQ_CHGVIN_INT2,
	S2MPU06_CHG_IRQ_CIN2BAT_INT2,

	S2MPU06_CHG_IRQ_CHGSTS_INT3,
	S2MPU06_CHG_IRQ_OTGILIM_INT3,
	S2MPU06_CHG_IRQ_BSTINLV_INT3,
	S2MPU06_CHG_IRQ_BSTILIM_INT3,
	S2MPU06_CHG_IRQ_VMIDOVP_INT3,

	S2MPU06_CHG_IRQ_WDT_PM,
	S2MPU06_CHG_IRQ_TSD_PM,
	S2MPU06_CHG_IRQ_VDDALV_PM,

	/* Fuelgauge */
	S2MPU06_FG_IRQ_VBAT_L_INT,
	S2MPU06_FG_IRQ_SOC_L_INT,
	S2MPU06_FG_IRQ_IDLE_ST_INT,
	S2MPU06_FG_IRQ_INIT_ST_INT,

	S2MPU06_IRQ_NR,
};


enum sec_device_type {

	S2MPU06X,
};

struct s2mpu06_dev {
	struct device *dev;
	struct i2c_client *i2c; /* 0x66; TOP */
	struct i2c_client *pmic; /* 0xCC; Regulator */
	struct i2c_client *rtc; /* 0x0C; RTC */
	struct i2c_client *charger; /* 0x68; Charger */
	struct i2c_client *fuelgauge; /* 0xCE; Fuelgauge */
	struct i2c_client *codec;
	struct mutex i2c_lock;
	struct apm_ops *ops;

	int type;
	int device_type;
	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[S2MPU06_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPU06_IRQ_GROUP_NR];

	/* pmic VER/REV register */
	u8 pmic_rev;	/* pmic Rev */
	u8 pmic_ver;	/* pmic version */

	struct s2mpu06_platform_data *pdata;
};

enum s2mpu06_types {
	TYPE_S2MPU06,
};

extern int s2mpu06_irq_init(struct s2mpu06_dev *s2mpu06);
extern void s2mpu06_irq_exit(struct s2mpu06_dev *s2mpu06);

extern int s2mpu06_read_codec_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
/* S2MPU06 shared i2c API function */
extern int s2mpu06_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int s2mpu06_read_reg_non_mutex(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int s2mpu06_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mpu06_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int s2mpu06_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mpu06_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int s2mpu06_read_word(struct i2c_client *i2c, u8 reg);

extern int s2mpu06_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

extern bool s2mpu06_is_pwron(void);

extern void set_codec_notifier_flag(void);
#endif /* __LINUX_MFD_S2MPU06_PRIV_H */

