/*
 * s2mpa05-private.h - Voltage regulator driver for the s2mpa05
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

#ifndef __LINUX_MFD_S2MPA05_REGULATOR_H
#define __LINUX_MFD_S2MPA05_REGULATOR_H
#include <linux/i2c.h>

#define MASK(width, shift)		(((0x1 << (width)) - 1) << shift)
#define SetBit(no)			(0x1 << (no))

#define S2MPA05_REG_INVALID		(0xFF)

/* VGPIO ADDRESS (0x00) */
#define S2MPA05_REG_VGPIO_REG0			0x00
#define S2MPA05_REG_VGPIO_PSI			0x01
#define S2MPA05_REG_VGPIO_VGI0			0x02
#define S2MPA05_REG_VGPIO_VGI1			0x03
#define S2MPA05_REG_VGPIO_VGI2			0x04
#define S2MPA05_REG_VGPIO_VGI3			0x05
#define S2MPA05_REG_VGPIO_VGI4			0x06
#define S2MPA05_REG_VGPIO_VGI5			0x07
#define S2MPA05_REG_VGPIO_VGI6			0x08
#define S2MPA05_REG_VGPIO_VGI7			0x09
#define S2MPA05_REG_VGPIO_VGI8			0x0A
#define S2MPA05_REG_VGPIO_VGI9			0x0B
#define S2MPA05_REG_VGPIO_VGI10			0x0C
#define S2MPA05_REG_VGPIO_VGI11			0x0D
#define S2MPA05_REG_VGPIO_VGI12			0x0E
#define S2MPA05_REG_VGPIO_VGI13			0x0F
#define S2MPA05_REG_VGPIO_VGI14			0x10
#define S2MPA05_REG_VGPIO_VGI15			0x11
#define S2MPA05_REG_VGPIO_VGI16			0x12
#define S2MPA05_REG_VGPIO_VGI17			0x13
#define S2MPA05_REG_VGPIO_VGI18			0x14

/* VGPIO TX (PMIC -> AP) table check*/
#define S2MPA05_VGI0_IRQ_E		(1 << 1)	//IRQ_S3

/* COMMON ADDRESS (0x03) */
#define S2MPA05_REG_COMMON_VGPIO_REG0_1			0x00
#define S2MPA05_REG_COMMON_VGPIO_REG0_2			0x01
#define S2MPA05_REG_COMMON_CHIP_ID			0x0E
#define S2MPA05_REG_COMMON_SPMI_CFG1			0x0F
#define S2MPA05_REG_COMMON_SPMI_CFG2			0x10
#define S2MPA05_REG_COMMON_SPMI_CFG3			0x11
#define S2MPA05_REG_COMMON_COM_CTRL1			0x12
#define S2MPA05_REG_COMMON_COM_CTRL2			0x13
#define S2MPA05_REG_COMMON_COM_CTRL3			0x14
#define S2MPA05_REG_COMMON_COM_CTRL5			0x15
#define S2MPA05_REG_COMMON_TX_MASK			0x16
#define S2MPA05_REG_COMMON_IRQ			0x17
#define S2MPA05_REG_COMMON_COM_CTRL4			0x18

/* CHIP ID MASK */
#define S2MPA05_CHIP_ID_MASK			(0xFF)
#define S2MPA05_CHIP_ID_HW_MASK			(0x0F)
#define S2MPA05_CHIP_ID_SW_MASK			(0xF0)

/* TX_MASK MASK */
#define S2MPA05_GPIO_IRQM_MASK			(1 << 1)
#define S2MPA05_PM_IRQM_MASK			(1 << 0)

/* PM ADDRESS (0x05) */
#define S2MPA05_REG_PM_INT1				0x00
#define S2MPA05_REG_PM_INT2				0x01
#define S2MPA05_REG_PM_INT3				0x02
#define S2MPA05_REG_PM_INT4				0x03
#define S2MPA05_REG_PM_INT1M				0x04
#define S2MPA05_REG_PM_INT2M				0x05
#define S2MPA05_REG_PM_INT3M				0x06
#define S2MPA05_REG_PM_INT4M				0x07
#define S2MPA05_REG_PM_STATUS1				0x08
#define S2MPA05_REG_PM_STATUS2				0x09
#define S2MPA05_REG_PM_OFFSRC1_CUR			0x0A
#define S2MPA05_REG_PM_OFFSRC2_CUR			0x0B
#define S2MPA05_REG_PM_OFFSRC1_OLD1			0x0C
#define S2MPA05_REG_PM_OFFSRC2_OLD1			0x0D
#define S2MPA05_REG_PM_OFFSRC1_OLD2			0x0E
#define S2MPA05_REG_PM_OFFSRC2_OLD2			0x0F
#define S2MPA05_REG_PM_CTRL1				0x17
#define S2MPA05_REG_PM_CTRL2				0x18
#define S2MPA05_REG_PM_CTRL3				0x19
#define S2MPA05_REG_PM_ETC_OTP1				0x1A
#define S2MPA05_REG_PM_ETC_OTP2				0x1B
#define S2MPA05_REG_PM_UVLO_OTP				0x1C
#define S2MPA05_REG_PM_CFG_PM				0x1D
#define S2MPA05_REG_PM_TIME_CTRL			0x1E
#define S2MPA05_REG_PM_BUCK1_CTRL			0x1F
#define S2MPA05_REG_PM_BUCK1_OUT1			0x20
#define S2MPA05_REG_PM_BUCK1_OUT2			0x21
#define S2MPA05_REG_PM_BUCK1_OUT3			0x22
#define S2MPA05_REG_PM_BUCK1_OUT4			0x23
#define S2MPA05_REG_PM_BUCK1_DVS			0x24
#define S2MPA05_REG_PM_BUCK1_AFM			0x25
#define S2MPA05_REG_PM_BUCK1_AFMX			0x26
#define S2MPA05_REG_PM_BUCK1_AFMY			0x27
#define S2MPA05_REG_PM_BUCK1_AFMZ			0x28
#define S2MPA05_REG_PM_BUCK1_OCP			0x29
#define S2MPA05_REG_PM_BUCK1_AVP			0x2A
#define S2MPA05_REG_PM_BUCK2_CTRL			0x2B
#define S2MPA05_REG_PM_BUCK2_OUT1			0x2C
#define S2MPA05_REG_PM_BUCK2_OUT2			0x2D
#define S2MPA05_REG_PM_BUCK2_OUT3			0x2E
#define S2MPA05_REG_PM_BUCK2_OUT4			0x2F
#define S2MPA05_REG_PM_BUCK2_DVS			0x30
#define S2MPA05_REG_PM_BUCK2_AFM			0x31
#define S2MPA05_REG_PM_BUCK2_AFMX			0x32
#define S2MPA05_REG_PM_BUCK2_AFMY			0x33
#define S2MPA05_REG_PM_BUCK2_AFMZ			0x34
#define S2MPA05_REG_PM_BUCK2_OCP			0x35
#define S2MPA05_REG_PM_BUCK2_AVP			0x36
#define S2MPA05_REG_PM_BUCK3_CTRL			0x37
#define S2MPA05_REG_PM_BUCK3_OUT1			0x38
#define S2MPA05_REG_PM_BUCK3_OUT2			0x39
#define S2MPA05_REG_PM_BUCK3_OUT3			0x3A
#define S2MPA05_REG_PM_BUCK3_OUT4			0x3B
#define S2MPA05_REG_PM_BUCK3_DVS			0x3C
#define S2MPA05_REG_PM_BUCK3_AFM			0x3D
#define S2MPA05_REG_PM_BUCK3_AFMX			0x3E
#define S2MPA05_REG_PM_BUCK3_AFMY			0x3F
#define S2MPA05_REG_PM_BUCK3_AFMZ			0x40
#define S2MPA05_REG_PM_BUCK3_OCP			0x41
#define S2MPA05_REG_PM_BUCK3_AVP			0x42
#define S2MPA05_REG_PM_BUCK4_CTRL			0x43
#define S2MPA05_REG_PM_BUCK4_OUT1			0x44
#define S2MPA05_REG_PM_BUCK4_OUT2			0x45
#define S2MPA05_REG_PM_BUCK4_OUT3			0x46
#define S2MPA05_REG_PM_BUCK4_OUT4			0x47
#define S2MPA05_REG_PM_BUCK4_DVS			0x48
#define S2MPA05_REG_PM_BUCK4_AFM			0x49
#define S2MPA05_REG_PM_BUCK4_AFMX			0x4A
#define S2MPA05_REG_PM_BUCK4_AFMY			0x4B
#define S2MPA05_REG_PM_BUCK4_AFMZ			0x4C
#define S2MPA05_REG_PM_BUCK4_OCP			0x4D
#define S2MPA05_REG_PM_BUCK4_AVP			0x4E
#define S2MPA05_REG_PM_DVS_LDO4_CTRL			0x4F
#define S2MPA05_REG_PM_DVS_LDO_RAMP1			0x50
#define S2MPA05_REG_PM_LDO1_CTRL			0x51
#define S2MPA05_REG_PM_LDO2_CTRL			0x52
#define S2MPA05_REG_PM_LDO3_CTRL			0x53
#define S2MPA05_REG_PM_LDO4_CTRL			0x54
#define S2MPA05_REG_PM_LDO4_OUT1			0x55
#define S2MPA05_REG_PM_LDO4_OUT2			0x56
#define S2MPA05_REG_PM_LDO4_OUT3			0x57
#define S2MPA05_REG_PM_LDO4_OUT4			0x58
#define S2MPA05_REG_PM_LDO_DSCH1			0x59
#define S2MPA05_REG_PM_ONSEQ_CTRL1			0x5A
#define S2MPA05_REG_PM_ONSEQ_CTRL2			0x5B
#define S2MPA05_REG_PM_ONSEQ_CTRL3			0x5C
#define S2MPA05_REG_PM_ONSEQ_CTRL4			0x5D
#define S2MPA05_REG_PM_ONSEQ_CTRL5			0x5E
#define S2MPA05_REG_PM_ONSEQ_CTRL6			0x5F
#define S2MPA05_REG_PM_ONSEQ_CTRL7			0x60
#define S2MPA05_REG_PM_ONSEQ_CTRL8			0x61
#define S2MPA05_REG_PM_OFF_SEQ_CTRL1			0x62
#define S2MPA05_REG_PM_OFF_SEQ_CTRL2			0x63
#define S2MPA05_REG_PM_OFF_SEQ_CTRL3			0x64
#define S2MPA05_REG_PM_OFF_SEQ_CTRL4			0x65
#define S2MPA05_REG_PM_SEL_VGPIO1			0x66
#define S2MPA05_REG_PM_SEL_VGPIO2			0x67
#define S2MPA05_REG_PM_SEL_VGPIO3			0x68
#define S2MPA05_REG_PM_SEL_VGPIO4			0x69
#define S2MPA05_REG_PM_SEL_VGPIO5			0x6A
#define S2MPA05_REG_PM_SEL_VGPIO6			0x6B
#define S2MPA05_REG_PM_SEL_VGPIO7			0x6C
#define S2MPA05_REG_PM_SEL_VGPIO8			0x6D
#define S2MPA05_REG_PM_SEL_DVS_EN1			0x6E
#define S2MPA05_REG_PM_SEL_DVS_EN2			0x6F
#define S2MPA05_REG_PM_SEL_DVS_EN3			0x70
#define S2MPA05_REG_PM_OFF_CTRL1			0x71
#define S2MPA05_REG_PM_OFF_CTRL2			0x72
#define S2MPA05_REG_PM_OFF_CTRL3			0x73
#define S2MPA05_REG_PM_OFF_CTRL4			0x74
#define S2MPA05_REG_PM_OFF_CTRL5			0x75
#define S2MPA05_REG_PM_OFF_CTRL6			0x76
#define S2MPA05_REG_PM_OFF_CTRL7			0x77
#define S2MPA05_REG_PM_SEQ_CTRL1			0x78
#define S2MPA05_REG_PM_SEQ_CTRL2			0x79
#define S2MPA05_REG_PM_CFG_PM2				0x7A
#define S2MPA05_REG_PM_CFG_PM3				0x7B
#define S2MPA05_REG_PM_CFG_PM4				0x7C
#define S2MPA05_REG_PM_CFG_PM5				0x7D
#define S2MPA05_REG_PM_SUB_CTRL				0x7E
#define S2MPA05_REG_PM_PSI_CTRL1			0x7F
#define S2MPA05_REG_PM_PSI_CTRL2			0x80
#define S2MPA05_REG_PM_SEL_HW_GPIO			0x81

/* PM mask */
#define BUCK_RAMP_MASK			(0x03)
#define BUCK_RAMP_UP_SHIFT		6

/* CFG_PM reg WTSR_EN Mask */
#define S2MPA05_WTSREN_MASK		MASK(1,2)

/* s2mpa05 Regulator ids */
enum s2mpa05_regulators {
	S2MPA05_LDO1,
	S2MPA05_LDO2,
	S2MPA05_LDO3,
	//S2MPA05_LDO4,
	S2MPA05_BUCK1,
	S2MPA05_BUCK2,
	S2MPA05_BUCK3,
	S2MPA05_BUCK4,
	S2MPA05_REG_MAX,
};

/* BUCKs 1E ~ 4E */
#define S2MPA05_BUCK_MIN1		300000
#define S2MPA05_BUCK_STEP1		6250
/* LDOs 1E ~ 3E */
#define S2MPA05_LDO_MIN1		1800000
#define S2MPA05_LDO_STEP1		25000
/* LDO 4E */
#define S2MPA05_LDO_MIN2		300000
#define S2MPA05_LDO_STEP2		6250

/* LDO/BUCK output voltage control */
#define S2MPA05_LDO_VSEL_MASK1		0xFF	/* LDO_OUT */
#define S2MPA05_LDO_VSEL_MASK2		0x3F	/* LDO_CTRL */
#define S2MPA05_BUCK_VSEL_MASK		0xFF
#define S2MPA05_BUCK_N_VOLTAGES 	(S2MPA05_BUCK_VSEL_MASK + 1)

/* BUCK/LDO Enable control[7:6] */
#define S2MPA05_ENABLE_SHIFT		0x06
#define S2MPA05_ENABLE_MASK		(0x03 << S2MPA05_ENABLE_SHIFT)
#define SEL_VGPIO_ON			(0x01 << S2MPA05_ENABLE_SHIFT)

#define S2MPA05_REGULATOR_MAX		(S2MPA05_REG_MAX)

/* Set LDO/BUCK soft time */
#define S2MPA05_ENABLE_TIME_LDO		128
#define S2MPA05_ENABLE_TIME_BUCK	130

/* OI mask */
#define S2MPA05_PMIC_IRQ_OI_B1_MASK	(1 << 4)
#define S2MPA05_PMIC_IRQ_OI_B2_MASK	(1 << 5)
#define S2MPA05_PMIC_IRQ_OI_B3_MASK	(1 << 6)
#define S2MPA05_PMIC_IRQ_OI_B4_MASK	(1 << 7)

/* OCP mask */
#define S2MPA05_PMIC_IRQ_OCP_B1_MASK	(1 << 0)
#define S2MPA05_PMIC_IRQ_OCP_B2_MASK	(1 << 1)
#define S2MPA05_PMIC_IRQ_OCP_B3_MASK	(1 << 2)
#define S2MPA05_PMIC_IRQ_OCP_B4_MASK	(1 << 3)

/* Temp mask */
#define S2MPA05_IRQ_INT120C_MASK	(1 << 2)
#define S2MPA05_IRQ_INT140C_MASK	(1 << 3)

/*
 * sec_opmode_data - regulator operation mode data
 * @id: regulator id
 * @mode: regulator operation mode
 */

enum s2mpa05_temperature_source {
	S2MPA05_TEMP_120 = 0,	/* 120 degree */
	S2MPA05_TEMP_140,	/* 140 degree */

	S2MPA05_TEMP_NR,
};

enum s2mpa05_irq_source {
	S2MPA05_PMIC_INT1 = 0,
	S2MPA05_PMIC_INT2,
	S2MPA05_PMIC_INT3,
	S2MPA05_PMIC_INT4,
	S2MPA05_IRQ_GROUP_NR,
};

#define S2MPA05_NUM_IRQ_PMIC_REGS	4 /* INT1 ~ INT4 */
#define S2MPA05_BUCK_MAX		4
#define S2MPA05_TEMP_MAX		2

enum s2mpa05_irq {
	/* PMIC */
	S2MPA05_PMIC_IRQ_PWRONF_INT1,
	S2MPA05_PMIC_IRQ_PWRONR_INT1,
	S2MPA05_PMIC_IRQ_INT120C_INT1,
	S2MPA05_PMIC_IRQ_INT140C_INT1,
	S2MPA05_PMIC_IRQ_TSD_INT1,
	S2MPA05_PMIC_IRQ_WTSR_INT1,
	S2MPA05_PMIC_IRQ_WRSTB_INT1,
	S2MPA05_PMIC_IRQ_TX_TRAN_FAIL_INT1,

	S2MPA05_PMIC_IRQ_OCP_B1E_INT2,
	S2MPA05_PMIC_IRQ_OCP_B2E_INT2,
	S2MPA05_PMIC_IRQ_OCP_B3E_INT2,
	S2MPA05_PMIC_IRQ_OCP_B4E_INT2,
	S2MPA05_PMIC_IRQ_OI_B1E_INT2,
	S2MPA05_PMIC_IRQ_OI_B2E_INT2,
	S2MPA05_PMIC_IRQ_OI_B3E_INT2,
	S2MPA05_PMIC_IRQ_OI_B4E_INT2,

	S2MPA05_PMIC_IRQ_OVP_INT3,
	S2MPA05_PMIC_IRQ_BUCK_AUTO_EXIT_INT3,
	S2MPA05_PMIC_IRQ_RSVD1_INT3,
	S2MPA05_PMIC_IRQ_RSVD2_INT3,
	S2MPA05_PMIC_IRQ_LDO1_SCP_INT3,
	S2MPA05_PMIC_IRQ_LDO2_SCP_INT3,
	S2MPA05_PMIC_IRQ_LDO3_SCP_INT3,
	S2MPA05_PMIC_IRQ_LDO4_SCP_INT3,

	S2MPA05_PMIC_IRQ_RSVD1_INT4,
	S2MPA05_PMIC_IRQ_RSVD2_INT4,
	S2MPA05_PMIC_IRQ_SPMI_LDO_OK_F_INT4,
	S2MPA05_PMIC_IRQ_CHECKSUM_INT4,
	S2MPA05_PMIC_IRQ_PARITY_ERR_DATA_INT4,
	S2MPA05_PMIC_IRQ_PARITY_ERR_ADDR_L_INT4,
	S2MPA05_PMIC_IRQ_PARITY_ERR_ADDR_H_INT4,
	S2MPA05_PMIC_IRQ_PARITY_ERR_CMD_INT4,

	S2MPA05_IRQ_NR,
};

enum s2mpa05_irq_type {
	/* OI */
	S2MPA05_IRQ_OI_B1S = 1,
	S2MPA05_IRQ_OI_B2S,
	S2MPA05_IRQ_OI_B3S,
	S2MPA05_IRQ_OI_B4S,
	/* OCP */
	S2MPA05_IRQ_OCP_B1S,
	S2MPA05_IRQ_OCP_B2S,
	S2MPA05_IRQ_OCP_B3S,
	S2MPA05_IRQ_OCP_B4S,
	/* Temp */
	S2MPA05_IRQ_INT120C,
	S2MPA05_IRQ_INT140C,
};

enum sec_device_type {
	s2mpa05X,
};

struct s2mpa05_dev {
	struct device *dev;
	struct s2mpa05_platform_data *pdata;
	struct i2c_client *i2c;
	struct i2c_client *vgpio;
	struct i2c_client *pmic;
	struct i2c_client *close;
	struct i2c_client *gpio;
	struct mutex i2c_lock;
	struct mutex irqlock;
	struct apm_ops *ops;

	bool wakeup;
	int type;
	int device_type;

	/* IRQ */
	int irq;
	int irq_base;
	int irq_masks_cur[S2MPA05_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPA05_IRQ_GROUP_NR];

	/* pmic VER/REV register */
	uint8_t pmic_rev;	/* pmic Rev */

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *irq_dev;
#endif
	/* VGPIO_RX_MONITOR */
	//void __iomem *mem_base;

	/* Work queue */
	struct workqueue_struct *irq_wqueue;
	struct delayed_work irq_work;
};

enum s2mpa05_types {
	TYPE_s2mpa05,
};

extern int s2mpa05_notifier_init(struct s2mpa05_dev *s2mpa05);
extern void s2mpa05_notifier_deinit(struct s2mpa05_dev *s2mpa05);
/* s2mpa05 shared i2c API function */
extern int s2mpa05_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *dest);
extern int s2mpa05_bulk_read(struct i2c_client *i2c, uint8_t reg, int count,
			     uint8_t *buf);
extern int s2mpa05_write_reg(struct i2c_client *i2c, uint8_t reg, uint8_t value);
extern int s2mpa05_bulk_write(struct i2c_client *i2c, uint8_t reg, int count,
			      uint8_t *buf);
extern int s2mpa05_write_word(struct i2c_client *i2c, uint8_t reg, uint16_t value);
extern int s2mpa05_read_word(struct i2c_client *i2c, uint8_t reg);
extern int s2mpa05_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val, uint8_t mask);
#if IS_ENABLED(CONFIG_EXYNOS_AFM)
extern int extra_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int extra_pmic_get_i2c(struct i2c_client **i2c);
#endif

#endif /* __LINUX_MFD_s2mpa05_REGULATOR_H */
