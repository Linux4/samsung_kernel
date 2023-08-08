/*
 * Copyright (c) 2021 Maxim Integrated Products, Inc.
 * Author: Maxim Integrated <opensource@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/firmware.h>
#include <linux/string.h>

/* for Regmap */
#include <linux/regmap.h>

/* for Device Tree */
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max17333.h>

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#define DRIVER_NAME    MAX17333_NAME

#define I2C_ADDR_PMIC		(0x6C >> 1) /* Model Gauge */
#define I2C_ADDR_SHDW		(0x16 >> 1) /* SHDW */

#define RETRY_COUNT_MAX     (2)

static const struct regmap_config max17333_regmap_config = {
	.reg_bits   = 8,
	.val_bits   = 16,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.cache_type = REGCACHE_NONE,
};

static const struct regmap_config max17333_regmap_config_shdw = {
	.reg_bits   = 8,
	.val_bits   = 16,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.cache_type = REGCACHE_NONE,
};

static char *max17333_type[] = {
	"SUB",
	"MAIN",
};

/*******************************************************************************
 * Chip IO
 ******************************************************************************/
int max17333_read(struct regmap *regmap, u8 addr, u16 *val)
{
	unsigned int buf = 0;
	int rc = 0, cnt = 0;

	for (cnt = 0; cnt < 3; ++cnt) {
		rc = regmap_read(regmap, (unsigned int)addr, &buf);

		if (rc < 0) {
			pr_err("%s : regmap_read returns error no : %d\n", __func__, rc);
			msleep(30);
		} else
			break;
	}

	if (rc < 0) {
#if IS_ENABLED(CONFIG_SEC_ABC) && !defined(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=battery@WARN=lim_i2c_fail");
#endif
		return rc;
	}

	*val = (u16)buf;
	return 0;
}
EXPORT_SYMBOL(max17333_read);

int max17333_write(struct regmap *regmap, u8 addr, u16 val)
{
	int rc = 0, cnt = 0;

	for (cnt = 0; cnt < 3; ++cnt) {
		rc = regmap_write(regmap, (unsigned int)addr, (unsigned int)val);
		if (rc < 0) {
			pr_err("%s : regmap_write returns error no : %d\n", __func__, rc);
			msleep(30);
		} else
			break;
	}

#if IS_ENABLED(CONFIG_SEC_ABC) && !defined(CONFIG_SEC_FACTORY)
	if (rc < 0)
		sec_abc_send_event("MODULE=battery@WARN=lim_i2c_fail");
#endif
	return rc;
}
EXPORT_SYMBOL(max17333_write);

int max17333_bulk_write(struct regmap *regmap, u8 addr, unsigned int *val, int size)
{
	int rc = 0;
	int i = 0;

	for (i = 0; i < size; i++) {
		rc = max17333_write(regmap, (unsigned int)(addr + i), (u16)(val[i]));
		if (rc < 0) {
			pr_err("%s : regmap_write returns error no : %d\n", __func__, rc);
			return rc;
		}
	}

	return 0;
}
EXPORT_SYMBOL(max17333_bulk_write);

#define WRITE_AND_VERIFY_RETRY 3
#define WRITE_AND_VERIFY_DELAY 1 // ms
int max17333_write_and_verify(struct regmap *regmap, u8 addr, u16 val)
{
	int rc = 0;
	unsigned int pval = 0;
	int retry = WRITE_AND_VERIFY_RETRY;

	while (retry > 0) {
		retry--;
		rc = regmap_write(regmap, (unsigned int)addr, (unsigned int)val);
		if (rc < 0) {
			pr_err("%s : (retry:%d/%d) regmap_write returns error no : %d\n", __func__, (WRITE_AND_VERIFY_RETRY-retry), WRITE_AND_VERIFY_RETRY, rc);
			continue;
		}
		usleep_range(1000, 1100); // WRITE_AND_VERIFY_DELAY

		rc = regmap_read(regmap, (unsigned int)addr, &pval);
		if (rc < 0) {
			pr_err("%s : (retry:%d/%d) regmap_read returns error no : %d\n", __func__, (WRITE_AND_VERIFY_RETRY-retry), WRITE_AND_VERIFY_RETRY, rc);
			continue;
		}

		if (val == (u16)pval) {
			retry = 0;
		} else {
			pr_err("%s : (retry:%d/%d) not matched 0x%x=0x%x\n", __func__, (WRITE_AND_VERIFY_RETRY-retry), WRITE_AND_VERIFY_RETRY, addr, val);
			continue;
		}
	}

	if (val != (u16)pval) {
		pr_err("%s : retried %d times but failed\n", __func__, WRITE_AND_VERIFY_RETRY);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(max17333_write_and_verify);

int max17333_bulk_write_and_verify(struct regmap *regmap, u8 addr, unsigned int *val, int size)
{
	int rc = 0;
	int i = 0;
	unsigned int pval = 0;

	for (i = 0; i < size; i++) {

		int retry = WRITE_AND_VERIFY_RETRY;

		while (retry > 0) {
			retry--;
			rc = max17333_write(regmap, (unsigned int)(addr + i), (u16)(val[i]));
			if (rc < 0) {
				pr_err("%s : (retry:%d/%d) regmap_write returns error no : %d\n", __func__, (WRITE_AND_VERIFY_RETRY-retry), WRITE_AND_VERIFY_RETRY, rc);
				continue;
			}
			usleep_range(3000, 3100); // WRITE_AND_VERIFY_DELAY

			rc = regmap_read(regmap, (unsigned int)(addr + i), &pval);
			if (rc < 0) {
				pr_err("%s : (retry:%d/%d) regmap_read returns error no : %d\n", __func__, (WRITE_AND_VERIFY_RETRY-retry), WRITE_AND_VERIFY_RETRY, rc);
				continue;
			}

			if ((u16)(val[i]) == (u16)pval) {
				retry = 0;
			} else {
				pr_err("%s : (retry:%d/%d) not matched 0x%x=0x%x\n", __func__, (WRITE_AND_VERIFY_RETRY-retry), WRITE_AND_VERIFY_RETRY, addr, val[i]);
				continue;
			}
		}

		if ((u16)(val[i]) != (u16)pval) {
			pr_err("%s : retried %d times but failed\n", __func__, WRITE_AND_VERIFY_RETRY);
			return -1;
		}
	}

	return 0;
}
EXPORT_SYMBOL(max17333_bulk_write_and_verify);

int max17333_update_bits(struct regmap *regmap, u8 addr, u16 mask, u16 val)
{
	int rc;

	rc = regmap_update_bits(regmap, (unsigned int)addr,
				(unsigned int)mask, (unsigned int)val);

	if (rc < 0)
		pr_err("%s : regmap_update_bits returns error no : %d\n", __func__, rc);

	return rc;
}
EXPORT_SYMBOL(max17333_update_bits);

/* Declare Interrupt */
static const struct regmap_irq max17333_intsrc_irqs[] = {
	{ .reg_offset = 0, .mask = BIT_STATUS_PA,},
	{ .reg_offset = 0, .mask = BIT_STATUS_SMX,},
	{ .reg_offset = 0, .mask = BIT_STATUS_TMX,},
	{ .reg_offset = 0, .mask = BIT_STATUS_VMX,},
	{ .reg_offset = 0, .mask = BIT_STATUS_CA,},
	{ .reg_offset = 0, .mask = BIT_STATUS_SMN,},
	{ .reg_offset = 0, .mask = BIT_STATUS_TMN,},
	{ .reg_offset = 0, .mask = BIT_STATUS_VMN,},
	{ .reg_offset = 0, .mask = BIT_STATUS_DSOCI,},
	{ .reg_offset = 0, .mask = BIT_STATUS_IMX,},
	{ .reg_offset = 0, .mask = BIT_STATUS_ALLOWCHGB,},
	{ .reg_offset = 0, .mask = BIT_STATUS_BST,},
	{ .reg_offset = 0, .mask = BIT_STATUS_IMN,},
	{ .reg_offset = 0, .mask = BIT_STATUS_POR,},
};

static const struct regmap_irq_chip max17333_intsrc_irq_chip = {
	.name = "max17333 intsrc",
	.status_base = REG_STATUS,
	.mask_base = REG_STATUS_MASK,
	.num_regs = 1,
	.irqs = max17333_intsrc_irqs,
	.num_irqs = ARRAY_SIZE(max17333_intsrc_irqs),
};

static int max17333_lock_write_protection(struct max17333_dev *dev, bool lock_en)
{
	int rc = 0;
	u16 val;

	pr_info("%s: lock = %d\n", __func__, lock_en);

	rc = max17333_read(dev->regmap_pmic, REG_COMMSTAT, &val);
	if (rc < 0)
		goto err;

	if (lock_en) {
		/* lock write protection */
		rc = max17333_write(dev->regmap_pmic, REG_COMMSTAT, val | 0x00F9);
		rc |= max17333_write(dev->regmap_pmic, REG_COMMSTAT, val | 0x00F9);
	} else {
		/* unlock write protection */
		rc = max17333_write(dev->regmap_pmic, REG_COMMSTAT, val & 0xFF06);
		rc |= max17333_write(dev->regmap_pmic, REG_COMMSTAT, val & 0xFF06);
	}
	if (rc < 0)
		goto err;

	rc = max17333_read(dev->regmap_pmic, REG_COMMSTAT, &val);
	if (rc < 0)
		goto err;

	pr_info("%s: CommStat : 0x%04X\n", __func__, val);
	return 0;

err:
	pr_err("<%s> failed\n", __func__);
	return rc;
}

int max17333_map_irq(struct max17333_dev *max17333, int irq)
{
	if (max17333->irqc_intsrc)
		return -1;
	return regmap_irq_get_virq(max17333->irqc_intsrc, irq);
}
EXPORT_SYMBOL_GPL(max17333_map_irq);

static int max17333_pmic_irq_int(struct max17333_dev *chip)
{
	struct device *dev = chip->dev;
	struct i2c_client *client = to_i2c_client(dev);
	int rc = 0;

	/* disable all interrupt source */
	max17333_write(chip->regmap_pmic, REG_PROTALRTS, 0xFFFF);

	/* interrupt source */
	rc = regmap_add_irq_chip(chip->regmap_pmic, chip->irq,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT |
			IRQF_SHARED, -1, &max17333_intsrc_irq_chip,
			&chip->irqc_intsrc);
	if (rc != 0) {
		dev_err(&client->dev, "failed to add insrc irq chip: %d\n",
			rc);
		goto out;
	}

	pr_info("<%s> IRQ initialize done\n", client->name);
	return 0;

out:
	return rc;
}

/*******************************************************************************
 *  device
 ******************************************************************************/
static int max17333_add_devices(struct max17333_dev *pmic,
		struct mfd_cell *cells, int n_devs)
{
	struct device *dev = pmic->dev;
	int rc;

	pr_info("%s: size %d\n", __func__, n_devs);
	rc = mfd_add_devices(dev, -1, cells, n_devs, NULL, 0, NULL);

	return rc;
}

static struct mfd_cell max17333_devices[] = {
#ifdef USE_MAX17333_FUELGAUGE
	{ .name = MAX17333_BATTERY_NAME, },
#endif
	{ .name = MAX17333_CHARGER_NAME, },
};

static struct mfd_cell max17333_devices_sub[] = {
#ifdef USE_MAX17333_FUELGAUGE
	{ .name = MAX17333_BATTERY_SUB_NAME, },
#endif
	{ .name = MAX17333_CHARGER_SUB_NAME, },
};

static int max17333_write_shdw_data(struct max17333_dev *pmic)
{
	int rc = 0;
	int retry = RETRY_COUNT_MAX;
	struct regmap *regmap_shdw = pmic->regmap_shdw;
	struct regmap *regmap_pmic = pmic->regmap_pmic;
	struct max17333_shdw_data *ndata = &(pmic->pdata->shdw_data);

	while (retry--) {
		pr_info("%s remain retry count : %d\n", __func__, retry);
		rc = max17333_write_and_verify(regmap_shdw, REG_NVALRTTH_SHDW, ndata->nVAlrtTh);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NTALRTTH_SHDW, ndata->nTAlrtTh);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NIALRTTH_SHDW, ndata->nIAlrtTh);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NSALRTTH_SHDW, ndata->nSAlrtTh);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NRSENSE_SHDW, ndata->nRSense);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NFILTERCFG_SHDW, ndata->nFilterCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NVEMPTY_SHDW, ndata->nVEmpty);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NLEARNCFG_SHDW, ndata->nLearnCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NQRTABLE00_SHDW, ndata->nQRTable00);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NQRTABLE10_SHDW, ndata->nQRTable10);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NQRTABLE20_SHDW, ndata->nQRTable20);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NQRTABLE30_SHDW, ndata->nQRTable30);
		//rc |= max17333_write_and_verify(regmap_shdw, REG_NCYCLES_SHDW, ndata->nCycles);
		rc |= max17333_write_and_verify(regmap_pmic, REG_CYCLES, ndata->nCycles);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NFULLCAPNOM_SHDW, ndata->nFullCapNom);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NRCOMP0_SHDW, ndata->nRComp0);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NTEMPCO_SHDW, ndata->nTempCo);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NBATTSTATUS_SHDW, ndata->nBattStatus);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NFULLCAPREP_SHDW, ndata->nFullCapRep);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NVOLTTEMP_SHDW, ndata->nVoltTemp);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NMAXMINCURR_SHDW, ndata->nMaxMinCurr);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NMAXMINVOLT_SHDW, ndata->nMaxMinVolt);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NMAXMINTEMP_SHDW, ndata->nMaxMinTemp);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NFAULTLOG_SHDW, ndata->nFaultLog);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NTIMERH_SHDW, ndata->nTimerH);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NCONFIG_SHDW, ndata->nConfig);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NRIPPLECFG_SHDW, ndata->nRippleCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NMISCCFG_SHDW, ndata->nMiscCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NDESIGNCAP_SHDW, ndata->nDesignCap);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NI2CCFG_SHDW, ndata->nI2CCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NFULLCFG_SHDW, ndata->nFullCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NRELAXCFG_SHDW, ndata->nRelaxCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NCONVGCFG_SHDW, ndata->nConvgCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NRGAIN_SHDW, ndata->nRGain);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NAGECHGCFG_SHDW, ndata->nAgeChgCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NTTFCFG_SHDW, ndata->nTTFCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NHIBCFG_SHDW, ndata->nHibCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NCHGCTRL1_SHDW, ndata->nChgCtrl1);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NICHGTERM_SHDW, ndata->nIChgTerm);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NCHGCFG0_SHDW, ndata->nChgCfg0);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NCHGCTRL0_SHDW, ndata->nChgCtrl0);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NSTEPCURR_SHDW, ndata->nStepCurr);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NSTEPVOLT_SHDW, ndata->nStepVolt);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NCHGCTRL2_SHDW, ndata->nChgCtrl2);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NPACKCFG_SHDW, ndata->nPackCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NCGAIN_SHDW, ndata->nCGain);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NADCCFG_SHDW, ndata->nADCCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NTHERMCFG_SHDW, ndata->nThermCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NCHGCFG1_SHDW, ndata->nChgCfg1);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NVCHGCFG1_SHDW, ndata->nVChgCfg1);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NVCHGCFG2_SHDW, ndata->nVChgCfg2);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NICHGCFG1_SHDW, ndata->nIChgCfg1);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NICHGCFG2_SHDW, ndata->nIChgCfg2);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NUVPRTTH_SHDW, ndata->nUVPrtTh);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NTPRTTH1_SHDW, ndata->nTPrtTh1);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NTPRTTH3_SHDW, ndata->nTPrtTh3);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NIPRTTH1_SHDW, ndata->nIPrtTh1);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NIPRTTH2_SHDW, ndata->nIPrtTh2);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NTPRTTH2_SHDW, ndata->nTPrtTh2);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NPROTMISCTH_SHDW, ndata->nProtMiscTh);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NPROTCFG_SHDW, ndata->nProtCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NNVCFG0_SHDW, ndata->nNVCfg0);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NNVCFG1_SHDW, ndata->nNVCfg1);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NOVPRTTH_SHDW, ndata->nOVPrtTh);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NNVCFG2_SHDW, ndata->nNVCfg2);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NDELAYCFG_SHDW, ndata->nDelayCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NODSCTH_SHDW, ndata->nODSCTh);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NODSCCFG_SHDW, ndata->nODSCCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NPROTCFG2_SHDW, ndata->nProtCfg2);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NDPLIMIT_SHDW, ndata->nDPLimit);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NSCOCVLIM_SHDW, ndata->nScOcvLim);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NAGEFCCFG_SHDW, ndata->nAgeFcCfg);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NDESIGNVOLTAGE_SHDW, ndata->nDesignVoltage);
		rc |= max17333_write_and_verify(regmap_shdw, REG_NCHGCFG2_SHDW, ndata->nChgCfg2);
		rc |= max17333_bulk_write_and_verify(pmic->regmap_shdw, REG_NSERIALNUMBER0_SHDW,
				ndata->nSerialNumber, ARRAY_SIZE(ndata->nSerialNumber));
		rc |= max17333_bulk_write_and_verify(pmic->regmap_shdw, REG_NDEVICENAME0_SHDW,
				ndata->nDeviceName, ARRAY_SIZE(ndata->nDeviceName));
		rc |= max17333_bulk_write_and_verify(pmic->regmap_shdw, REG_NMANFCTRNAME0_SHDW,
				ndata->nManfctrName, ARRAY_SIZE(ndata->nManfctrName));
		rc |= max17333_bulk_write_and_verify(pmic->regmap_shdw, REG_NXTABLE0_SHDW,
				ndata->nXTable, ARRAY_SIZE(ndata->nXTable));
		rc |= max17333_bulk_write_and_verify(pmic->regmap_shdw, REG_NOCVTABLE0_SHDW,
				ndata->nOCVTable, ARRAY_SIZE(ndata->nOCVTable));

		if (rc < 0)
			continue;

		break;
	}

	return rc;

}

static int max17333_write_batt_profile_data(struct max17333_dev *pmic)
{
	int rc = 0;
	int retry = RETRY_COUNT_MAX;
	struct regmap *regmap_pmic = pmic->regmap_pmic;
	struct max17333_shdw_data *ndata = &(pmic->pdata->shdw_data);

	while (retry--) {
		pr_info("%s remain retry count : %d\n", __func__, retry);
		rc = max17333_bulk_write_and_verify(regmap_pmic, REG_BATTPROFILE,
				ndata->battProfile, ARRAY_SIZE(ndata->battProfile));

		if (rc < 0)
			continue;

		break;
	}

	return rc;
}

static void max17333_print_shdw_data(struct max17333_pmic_platform_data *pdata)
{
	struct max17333_shdw_data *ndata = &(pdata->shdw_data);

	pr_info("nVAlrtTh      : [%d] = 0x%04x\n", ndata->nVAlrtTh, ndata->nVAlrtTh);
	pr_info("nTAlrtTh      : [%d] = 0x%04x\n", ndata->nTAlrtTh, ndata->nTAlrtTh);
	pr_info("nIAlrtTh      : [%d] = 0x%04x\n", ndata->nIAlrtTh, ndata->nIAlrtTh);
	pr_info("nSAlrtTh      : [%d] = 0x%04x\n", ndata->nSAlrtTh, ndata->nSAlrtTh);
	pr_info("nRSense       : [%d] = 0x%04x\n", ndata->nRSense, ndata->nRSense);
	pr_info("nFilterCfg    : [%d] = 0x%04x\n", ndata->nFilterCfg, ndata->nFilterCfg);
	pr_info("nVEmpty       : [%d] = 0x%04x\n", ndata->nVEmpty, ndata->nVEmpty);
	pr_info("nLearnCfg     : [%d] = 0x%04x\n", ndata->nLearnCfg, ndata->nLearnCfg);
	pr_info("nQRTable00    : [%d] = 0x%04x\n", ndata->nQRTable00, ndata->nQRTable00);
	pr_info("nQRTable10    : [%d] = 0x%04x\n", ndata->nQRTable10, ndata->nQRTable10);
	pr_info("nQRTable20    : [%d] = 0x%04x\n", ndata->nQRTable20, ndata->nQRTable20);
	pr_info("nQRTable30    : [%d] = 0x%04x\n", ndata->nQRTable30, ndata->nQRTable30);
	pr_info("nCycles       : [%d] = 0x%04x\n", ndata->nCycles, ndata->nCycles);
	pr_info("nFullCapNom   : [%d] = 0x%04x\n", ndata->nFullCapNom, ndata->nFullCapNom);
	pr_info("nRComp0       : [%d] = 0x%04x\n", ndata->nRComp0, ndata->nRComp0);
	pr_info("nTempCo       : [%d] = 0x%04x\n", ndata->nTempCo, ndata->nTempCo);
	pr_info("nBattStatus   : [%d] = 0x%04x\n", ndata->nBattStatus, ndata->nBattStatus);
	pr_info("nFullCapRep   : [%d] = 0x%04x\n", ndata->nFullCapRep, ndata->nFullCapRep);
	pr_info("nVoltTemp     : [%d] = 0x%04x\n", ndata->nVoltTemp, ndata->nVoltTemp);
	pr_info("nMaxMinCurr   : [%d] = 0x%04x\n", ndata->nMaxMinCurr, ndata->nMaxMinCurr);
	pr_info("nMaxMinVolt   : [%d] = 0x%04x\n", ndata->nMaxMinVolt, ndata->nMaxMinVolt);
	pr_info("nMaxMinTemp   : [%d] = 0x%04x\n", ndata->nMaxMinTemp, ndata->nMaxMinTemp);
	pr_info("nFaultLog     : [%d] = 0x%04x\n", ndata->nFaultLog, ndata->nFaultLog);
	pr_info("nTimerH       : [%d] = 0x%04x\n", ndata->nTimerH, ndata->nTimerH);
	pr_info("nConfig       : [%d] = 0x%04x\n", ndata->nConfig, ndata->nConfig);
	pr_info("nRippleCfg    : [%d] = 0x%04x\n", ndata->nRippleCfg, ndata->nRippleCfg);
	pr_info("nMiscCfg      : [%d] = 0x%04x\n", ndata->nMiscCfg, ndata->nMiscCfg);
	pr_info("nDesignCap    : [%d] = 0x%04x\n", ndata->nDesignCap, ndata->nDesignCap);
	pr_info("nI2CCfg       : [%d] = 0x%04x\n", ndata->nI2CCfg, ndata->nI2CCfg);
	pr_info("nFullCfg      : [%d] = 0x%04x\n", ndata->nFullCfg, ndata->nFullCfg);
	pr_info("nRelaxCfg     : [%d] = 0x%04x\n", ndata->nRelaxCfg, ndata->nRelaxCfg);
	pr_info("nConvgCfg     : [%d] = 0x%04x\n", ndata->nConvgCfg, ndata->nConvgCfg);
	pr_info("nRGain        : [%d] = 0x%04x\n", ndata->nRGain, ndata->nRGain);
	pr_info("nAgeChgCfg    : [%d] = 0x%04x\n", ndata->nAgeChgCfg, ndata->nAgeChgCfg);
	pr_info("nTTFCfg       : [%d] = 0x%04x\n", ndata->nTTFCfg, ndata->nTTFCfg);
	pr_info("nHibCfg       : [%d] = 0x%04x\n", ndata->nHibCfg, ndata->nHibCfg);
	pr_info("nChgCtrl1     : [%d] = 0x%04x\n", ndata->nChgCtrl1, ndata->nChgCtrl1);
	pr_info("nIChgTerm     : [%d] = 0x%04x\n", ndata->nIChgTerm, ndata->nIChgTerm);
	pr_info("nChgCfg0      : [%d] = 0x%04x\n", ndata->nChgCfg0, ndata->nChgCfg0);
	pr_info("nChgCtrl0     : [%d] = 0x%04x\n", ndata->nChgCtrl0, ndata->nChgCtrl0);
	pr_info("nStepCurr     : [%d] = 0x%04x\n", ndata->nStepCurr, ndata->nStepCurr);
	pr_info("nStepVolt     : [%d] = 0x%04x\n", ndata->nStepVolt, ndata->nStepVolt);
	pr_info("nChgCtrl2     : [%d] = 0x%04x\n", ndata->nChgCtrl2, ndata->nChgCtrl2);
	pr_info("nPackCfg      : [%d] = 0x%04x\n", ndata->nPackCfg, ndata->nPackCfg);
	pr_info("nCGain        : [%d] = 0x%04x\n", ndata->nCGain, ndata->nCGain);
	pr_info("nADCCfg       : [%d] = 0x%04x\n", ndata->nADCCfg, ndata->nADCCfg);
	pr_info("nThermCfg     : [%d] = 0x%04x\n", ndata->nThermCfg, ndata->nThermCfg);
	pr_info("nChgCfg1      : [%d] = 0x%04x\n", ndata->nChgCfg1, ndata->nChgCfg1);
	pr_info("nVChgCfg1     : [%d] = 0x%04x\n", ndata->nVChgCfg1, ndata->nVChgCfg1);
	pr_info("nVChgCfg2     : [%d] = 0x%04x\n", ndata->nVChgCfg2, ndata->nVChgCfg2);
	pr_info("nIChgCfg1     : [%d] = 0x%04x\n", ndata->nIChgCfg1, ndata->nIChgCfg1);
	pr_info("nIChgCfg2     : [%d] = 0x%04x\n", ndata->nIChgCfg2, ndata->nIChgCfg2);
	pr_info("nUVPrtTh      : [%d] = 0x%04x\n", ndata->nUVPrtTh, ndata->nUVPrtTh);
	pr_info("nTPrtTh1      : [%d] = 0x%04x\n", ndata->nTPrtTh1, ndata->nTPrtTh1);
	pr_info("nTPrtTh3      : [%d] = 0x%04x\n", ndata->nTPrtTh3, ndata->nTPrtTh3);
	pr_info("nIPrtTh1      : [%d] = 0x%04x\n", ndata->nIPrtTh1, ndata->nIPrtTh1);
	pr_info("nIPrtTh2      : [%d] = 0x%04x\n", ndata->nIPrtTh2, ndata->nIPrtTh2);
	pr_info("nTPrtTh2      : [%d] = 0x%04x\n", ndata->nTPrtTh2, ndata->nTPrtTh2);
	pr_info("nProtMiscTh   : [%d] = 0x%04x\n", ndata->nProtMiscTh, ndata->nProtMiscTh);
	pr_info("nProtCfg      : [%d] = 0x%04x\n", ndata->nProtCfg, ndata->nProtCfg);
	pr_info("nNVCfg0       : [%d] = 0x%04x\n", ndata->nNVCfg0, ndata->nNVCfg0);
	pr_info("nNVCfg1       : [%d] = 0x%04x\n", ndata->nNVCfg1, ndata->nNVCfg1);
	pr_info("nOVPrtTh      : [%d] = 0x%04x\n", ndata->nOVPrtTh, ndata->nOVPrtTh);
	pr_info("nNVCfg2       : [%d] = 0x%04x\n", ndata->nNVCfg2, ndata->nNVCfg2);
	pr_info("nDelayCfg     : [%d] = 0x%04x\n", ndata->nDelayCfg, ndata->nDelayCfg);
	pr_info("nODSCTh       : [%d] = 0x%04x\n", ndata->nODSCTh, ndata->nODSCTh);
	pr_info("nODSCCfg      : [%d] = 0x%04x\n", ndata->nODSCCfg, ndata->nODSCCfg);
	pr_info("nProtCfg2     : [%d] = 0x%04x\n", ndata->nProtCfg2, ndata->nProtCfg2);
	pr_info("nDPLimit      : [%d] = 0x%04x\n", ndata->nDPLimit, ndata->nDPLimit);
	pr_info("nScOcvLim     : [%d] = 0x%04x\n", ndata->nScOcvLim, ndata->nScOcvLim);
	pr_info("nAgeFcCfg     : [%d] = 0x%04x\n", ndata->nAgeFcCfg, ndata->nAgeFcCfg);
	pr_info("nDesignVoltage: [%d] = 0x%04x\n", ndata->nDesignVoltage, ndata->nDesignVoltage);
	pr_info("nChgCfg2      : [%d] = 0x%04x\n", ndata->nChgCfg2, ndata->nChgCfg2);
	pr_info("nRFastVShdn   : [%d] = 0x%04x\n", ndata->nRFastVShdn, ndata->nRFastVShdn);
	pr_info("nManfctrDate  : [%d] = 0x%04x\n", ndata->nManfctrDate, ndata->nManfctrDate);
	pr_info("nFirstUsed    : [%d] = 0x%04x\n", ndata->nFirstUsed, ndata->nFirstUsed);
	pr_info("nSerialNumber : [%d] = 0x%04x\n", ndata->nSerialNumber[0], ndata->nSerialNumber[0]);
	pr_info("nManfctrName  : [%d] = 0x%04x\n", ndata->nManfctrName[0], ndata->nManfctrName[0]);
	pr_info("nDeviceName   : [%d] = 0x%04x\n", ndata->nDeviceName[0], ndata->nDeviceName[0]);
	pr_info("nXTable       : [%d] = 0x%04x\n", ndata->nXTable[0], ndata->nXTable[0]);
	pr_info("nOCVTable     : [%d] = 0x%04x\n", ndata->nOCVTable[0], ndata->nOCVTable[0]);
	pr_info("battProfile   : [%d] = 0x%04x\n", ndata->battProfile[0], ndata->battProfile[0]);
}

static int max17333_pmic_get_shdw_platdata(
	struct max17333_pmic_platform_data *pdata, struct device_node *node)
{
	int rc;
	u32 pval;
	struct device_node *np;
	struct max17333_shdw_data *ndata = &(pdata->shdw_data);

	if (of_property_read_bool(node, "use-default")) {
		pr_info("%s: use default nv data\n", __func__);
		return -1;
	}

	np = of_get_child_by_name(node, "shdw-data");
	if (!np) {
		pr_warn("%s: invalid nv data\n", __func__);
		return -1;
	}

	rc = of_property_read_u32(np, "nVAlrtTh", &pval);
	if (rc < 0)
		return rc;
	ndata->nVAlrtTh = (u16)pval;

	rc = of_property_read_u32(np, "nTAlrtTh", &pval);
	if (rc < 0)
		return rc;
	ndata->nTAlrtTh = (u16)pval;

	rc = of_property_read_u32(np, "nIAlrtTh", &pval);
	if (rc < 0)
		return rc;
	ndata->nIAlrtTh = (u16)pval;

	rc = of_property_read_u32(np, "nSAlrtTh", &pval);
	if (rc < 0)
		return rc;
	ndata->nSAlrtTh = (u16)pval;

	rc = of_property_read_u32(np, "nRSense", &pval);
	if (rc < 0)
		return rc;
	ndata->nRSense = (u16)pval;

	rc = of_property_read_u32(np, "nFilterCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nFilterCfg = (u16)pval;

	rc = of_property_read_u32(np, "nVEmpty", &pval);
	if (rc < 0)
		return rc;
	ndata->nVEmpty = (u16)pval;

	rc = of_property_read_u32(np, "nLearnCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nLearnCfg = (u16)pval;

	rc = of_property_read_u32(np, "nQRTable00", &pval);
	if (rc < 0)
		return rc;
	ndata->nQRTable00 = (u16)pval;

	rc = of_property_read_u32(np, "nQRTable10", &pval);
	if (rc < 0)
		return rc;
	ndata->nQRTable10 = (u16)pval;

	rc = of_property_read_u32(np, "nQRTable20", &pval);
	if (rc < 0)
		return rc;
	ndata->nQRTable20 = (u16)pval;

	rc = of_property_read_u32(np, "nQRTable30", &pval);
	if (rc < 0)
		return rc;
	ndata->nQRTable30 = (u16)pval;

	rc = of_property_read_u32(np, "nCycles", &pval);
	if (rc < 0)
		return rc;
	ndata->nCycles = (u16)pval;

	rc = of_property_read_u32(np, "nFullCapNom", &pval);
	if (rc < 0)
		return rc;
	ndata->nFullCapNom = (u16)pval;

	rc = of_property_read_u32(np, "nRComp0", &pval);
	if (rc < 0)
		return rc;
	ndata->nRComp0 = (u16)pval;

	rc = of_property_read_u32(np, "nTempCo", &pval);
	if (rc < 0)
		return rc;
	ndata->nTempCo = (u16)pval;

	rc = of_property_read_u32(np, "nBattStatus", &pval);
	if (rc < 0)
		return rc;
	ndata->nBattStatus = (u16)pval;

	rc = of_property_read_u32(np, "nFullCapRep", &pval);
	if (rc < 0)
		return rc;
	ndata->nFullCapRep = (u16)pval;

	rc = of_property_read_u32(np, "nVoltTemp", &pval);
	if (rc < 0)
		return rc;
	ndata->nVoltTemp = (u16)pval;

	rc = of_property_read_u32(np, "nMaxMinCurr", &pval);
	if (rc < 0)
		return rc;
	ndata->nMaxMinCurr = (u16)pval;

	rc = of_property_read_u32(np, "nMaxMinVolt", &pval);
	if (rc < 0)
		return rc;
	ndata->nMaxMinVolt = (u16)pval;

	rc = of_property_read_u32(np, "nMaxMinTemp", &pval);
	if (rc < 0)
		return rc;
	ndata->nMaxMinTemp = (u16)pval;

	rc = of_property_read_u32(np, "nFaultLog", &pval);
	if (rc < 0)
		return rc;
	ndata->nFaultLog = (u16)pval;

	rc = of_property_read_u32(np, "nTimerH", &pval);
	if (rc < 0)
		return rc;
	ndata->nTimerH = (u16)pval;

	rc = of_property_read_u32(np, "nConfig", &pval);
	if (rc < 0)
		return rc;
	ndata->nConfig = (u16)pval;

	rc = of_property_read_u32(np, "nRippleCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nRippleCfg = (u16)pval;

	rc = of_property_read_u32(np, "nMiscCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nMiscCfg = (u16)pval;

	rc = of_property_read_u32(np, "nDesignCap", &pval);
	if (rc < 0)
		return rc;
	ndata->nDesignCap = (u16)pval;

	rc = of_property_read_u32(np, "nI2CCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nI2CCfg = (u16)pval;

	rc = of_property_read_u32(np, "nFullCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nFullCfg = (u16)pval;

	rc = of_property_read_u32(np, "nRelaxCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nRelaxCfg = (u16)pval;

	rc = of_property_read_u32(np, "nConvgCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nConvgCfg = (u16)pval;

	rc = of_property_read_u32(np, "nRGain", &pval);
	if (rc < 0)
		return rc;
	ndata->nRGain = (u16)pval;

	rc = of_property_read_u32(np, "nAgeChgCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nAgeChgCfg = (u16)pval;

	rc = of_property_read_u32(np, "nTTFCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nTTFCfg = (u16)pval;

	rc = of_property_read_u32(np, "nHibCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nHibCfg = (u16)pval;

	rc = of_property_read_u32(np, "nChgCtrl1", &pval);
	if (rc < 0)
		return rc;
	ndata->nChgCtrl1 = (u16)pval;

	rc = of_property_read_u32(np, "nIChgTerm", &pval);
	if (rc < 0)
		return rc;
	ndata->nIChgTerm = (u16)pval;


	rc = of_property_read_u32(np, "nChgCfg0", &pval);
	if (rc < 0)
		return rc;
	ndata->nChgCfg0 = (u16)pval;

	rc = of_property_read_u32(np, "nChgCtrl0", &pval);
	if (rc < 0)
		return rc;
	ndata->nChgCtrl0 = (u16)pval;

	rc = of_property_read_u32(np, "nStepCurr", &pval);
	if (rc < 0)
		return rc;
	ndata->nStepCurr = (u16)pval;

	rc = of_property_read_u32(np, "nStepVolt", &pval);
	if (rc < 0)
		return rc;
	ndata->nStepVolt = (u16)pval;

	rc = of_property_read_u32(np, "nChgCtrl2", &pval);
	if (rc < 0)
		return rc;
	ndata->nChgCtrl2 = (u16)pval;

	rc = of_property_read_u32(np, "nPackCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nPackCfg = (u16)pval;

	rc = of_property_read_u32(np, "nCGain", &pval);
	if (rc < 0)
		return rc;
	ndata->nCGain = (u16)pval;

	rc = of_property_read_u32(np, "nADCCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nADCCfg = (u16)pval;

	rc = of_property_read_u32(np, "nThermCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nThermCfg = (u16)pval;

	rc = of_property_read_u32(np, "nChgCfg1", &pval);
	if (rc < 0)
		return rc;
	ndata->nChgCfg1 = (u16)pval;

	rc = of_property_read_u32(np, "nVChgCfg1", &pval);
	if (rc < 0)
		return rc;
	ndata->nVChgCfg1 = (u16)pval;

	rc = of_property_read_u32(np, "nVChgCfg2", &pval);
	if (rc < 0)
		return rc;
	ndata->nVChgCfg2 = (u16)pval;

	rc = of_property_read_u32(np, "nIChgCfg1", &pval);
	if (rc < 0)
		return rc;
	ndata->nIChgCfg1 = (u16)pval;

	rc = of_property_read_u32(np, "nIChgCfg2", &pval);
	if (rc < 0)
		return rc;
	ndata->nIChgCfg2 = (u16)pval;

	rc = of_property_read_u32(np, "nUVPrtTh", &pval);
	if (rc < 0)
		return rc;
	ndata->nUVPrtTh = (u16)pval;

	rc = of_property_read_u32(np, "nTPrtTh1", &pval);
	if (rc < 0)
		return rc;
	ndata->nTPrtTh1 = (u16)pval;

	rc = of_property_read_u32(np, "nTPrtTh3", &pval);
	if (rc < 0)
		return rc;
	ndata->nTPrtTh3 = (u16)pval;

	rc = of_property_read_u32(np, "nIPrtTh1", &pval);
	if (rc < 0)
		return rc;
	ndata->nIPrtTh1 = (u16)pval;

	rc = of_property_read_u32(np, "nIPrtTh2", &pval);
	if (rc < 0)
		return rc;
	ndata->nIPrtTh2 = (u16)pval;

	rc = of_property_read_u32(np, "nTPrtTh2", &pval);
	if (rc < 0)
		return rc;
	ndata->nTPrtTh2 = (u16)pval;

	rc = of_property_read_u32(np, "nProtMiscTh", &pval);
	if (rc < 0)
		return rc;
	ndata->nProtMiscTh = (u16)pval;

	rc = of_property_read_u32(np, "nProtCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nProtCfg = (u16)pval;

	rc = of_property_read_u32(np, "nNVCfg0", &pval);
	if (rc < 0)
		return rc;
	ndata->nNVCfg0 = (u16)pval;

	rc = of_property_read_u32(np, "nNVCfg1", &pval);
	if (rc < 0)
		return rc;
	ndata->nNVCfg1 = (u16)pval;

	rc = of_property_read_u32(np, "nOVPrtTh", &pval);
	if (rc < 0)
		return rc;
	ndata->nOVPrtTh = (u16)pval;

	rc = of_property_read_u32(np, "nNVCfg2", &pval);
	if (rc < 0)
		return rc;
	ndata->nNVCfg2 = (u16)pval;

	rc = of_property_read_u32(np, "nDelayCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nDelayCfg = (u16)pval;

	rc = of_property_read_u32(np, "nODSCTh", &pval);
	if (rc < 0)
		return rc;
	ndata->nODSCTh = (u16)pval;

	rc = of_property_read_u32(np, "nODSCCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nODSCCfg = (u16)pval;

	rc = of_property_read_u32(np, "nProtCfg2", &pval);
	if (rc < 0)
		return rc;
	ndata->nProtCfg2 = (u16)pval;

	rc = of_property_read_u32(np, "nDPLimit", &pval);
	if (rc < 0)
		return rc;
	ndata->nDPLimit = (u16)pval;

	rc = of_property_read_u32(np, "nScOcvLim", &pval);
	if (rc < 0)
		return rc;
	ndata->nScOcvLim = (u16)pval;

	rc = of_property_read_u32(np, "nAgeFcCfg", &pval);
	if (rc < 0)
		return rc;
	ndata->nAgeFcCfg = (u16)pval;

	rc = of_property_read_u32(np, "nDesignVoltage", &pval);
	if (rc < 0)
		return rc;
	ndata->nDesignVoltage = (u16)pval;

	rc = of_property_read_u32(np, "nChgCfg2", &pval);
	if (rc < 0)
		return rc;
	ndata->nChgCfg2 = (u16)pval;

	rc = of_property_read_u32(np, "nRFastVShdn", &pval);
	if (rc < 0)
		return rc;
	ndata->nRFastVShdn = (u16)pval;

	rc = of_property_read_u32(np, "nManfctrDate", &pval);
	if (rc < 0)
		return rc;
	ndata->nManfctrDate = (u16)pval;

	rc = of_property_read_u32(np, "nFirstUsed", &pval);
	if (rc < 0)
		return rc;
	ndata->nFirstUsed = (u16)pval;

	rc = of_property_read_u32_array(np, "nSerialNumber", ndata->nSerialNumber, 3);
	if (rc < 0)
		return rc;

	rc = of_property_read_u32_array(np, "nDeviceName", ndata->nDeviceName, 2);
	if (rc < 0)
		return rc;

	rc = of_property_read_u32_array(np, "nManfctrName", ndata->nManfctrName, 3);
	if (rc < 0)
		return rc;

	rc = of_property_read_u32_array(np, "nXTable", ndata->nXTable, 12);
	if (rc < 0)
		return rc;

	rc = of_property_read_u32_array(np, "nOCVTable", ndata->nOCVTable, 12);
	if (rc < 0)
		return rc;

	rc = of_property_read_u32_array(np, "nROMID", ndata->nROMID, 4);
	if (rc < 0)
		return rc;

	rc = of_property_read_u32_array(np, "battProfile", ndata->battProfile, 32);
	if (rc < 0)
		return rc;

	return rc;
}

#if 0
static int max17333_check_shdw_data(struct max17333_dev *pmic)
{
	int i;
	u16 pval;
	struct max17333_shdw_data *ndata = &(pmic->pdata->shdw_data);

	pr_info("%s check nv data : nXTable, nOCVTable\n", __func__);

	for (i = 0; i < ARRAY_SIZE(ndata->nXTable); i++) {
		max17333_read(pmic->regmap_shdw, (REG_NXTABLE0_SHDW + i), &pval);
		if (ndata->nXTable[i] != pval) {
			pr_info("%s nXTable[%d] not matched\n", __func__, i);
			return -1;
		}
	}

	for (i = 0; i < ARRAY_SIZE(ndata->nOCVTable); i++) {
		max17333_read(pmic->regmap_shdw, (REG_NOCVTABLE0_SHDW + i), &pval);
		if (ndata->nOCVTable[i] != pval) {
			pr_info("%s nOCVTable[%d] not matched\n", __func__, i);
			return -1;
		}
	}

	return 0;
}
#endif

static bool max17333_pmic_version_check(struct max17333_dev *pmic)
{
	struct max17333_shdw_data *ndata = &(pmic->pdata->shdw_data);
	unsigned int new_ver = ndata->nManfctrName[2];
	int rc;
	u16 pval;

	rc = max17333_read(pmic->regmap_shdw, REG_VERSION_SS_SHDW, &pval);
	pr_info("%s ver_from_reg=0x%04X | ver_from_dts=0x%04X\n", __func__, pval, new_ver);
	if (rc < 0)
		return false;

	if (new_ver == pval)
		return false;

	return true;
}

static void max17333_set_designcap(struct max17333_dev *pmic)
{
	u16 rval;
	int ret;

	ret = max17333_read(pmic->regmap_pmic, REG_DESIGNCAP, &rval);
	if (ret < 0)
		return;

	rval &= 0xFFF0;
	rval |= 0x0001;

	ret = max17333_write(pmic->regmap_shdw, REG_NDESIGNCAP_SHDW, rval);
	if (ret < 0)
		return;
}

#define READ_CONFIG2_RETRY 4
#define READ_CONFIG2_DELAY 500 // ms
static int max17333_pmic_shdw_update(struct max17333_dev *pmic)
{
	int rc;
	u16 pval;
	bool is_changed;
	int retry = READ_CONFIG2_RETRY;
	u16 config2_val = MAX17333_CONFIG2_POR_CMD;

	rc = max17333_read(pmic->regmap_pmic, REG_STATUS, &pval);
	if (rc < 0)
		return rc;

	if (pval & 0x0002) {
		pr_info("%s power on reset detected\n", __func__);
		rc = max17333_write_batt_profile_data(pmic);
		rc |= max17333_write_shdw_data(pmic);
		max17333_update_bits(pmic->regmap_pmic, REG_STATUS,	BIT_STATUS_POR, BIT_CLEAR);
		max17333_write(pmic->regmap_pmic, REG_CONFIG2, MAX17333_CONFIG2_POR_CMD);
		/* Wait 2s for POR_CMD to clear */
		retry = READ_CONFIG2_RETRY;
		while (retry > 0) {
			config2_val = MAX17333_CONFIG2_POR_CMD;
			retry--;
			msleep(READ_CONFIG2_DELAY);
			rc = max17333_read(pmic->regmap_pmic, REG_CONFIG2, &config2_val);
			if (rc < 0) {
				pr_err("%s : (retry:%d/%d) max17333_read returns error no : %d\n", __func__, (READ_CONFIG2_RETRY-retry), READ_CONFIG2_RETRY, rc);
				continue;
			}
			if ((config2_val & MAX17333_CONFIG2_POR_CMD) == 0) {
				retry = 0;
			} else {
				pr_err("%s : (retry:%d/%d) config2(0x%04X) is not cleared\n", __func__, (READ_CONFIG2_RETRY-retry), READ_CONFIG2_RETRY, config2_val);
				continue;
			}
		}

		max17333_set_designcap(pmic);

		if ((config2_val & MAX17333_CONFIG2_POR_CMD) != 0) {
			pr_err("%s : retried %d times (%d ms) but failed\n", __func__, READ_CONFIG2_RETRY, READ_CONFIG2_RETRY * READ_CONFIG2_DELAY);
			return -1;
		}
	} else {
		/* check the version */
		is_changed = max17333_pmic_version_check(pmic);
		if (is_changed) {
			pr_info("%s version changed\n", __func__);
			rc = max17333_write_batt_profile_data(pmic);
			rc |= max17333_write_shdw_data(pmic);
			max17333_write(pmic->regmap_pmic, REG_CONFIG2, MAX17333_CONFIG2_POR_CMD);
			/* Wait 2s for POR_CMD to clear */
			retry = READ_CONFIG2_RETRY;
			while (retry > 0) {
				config2_val = MAX17333_CONFIG2_POR_CMD;
				retry--;
				msleep(READ_CONFIG2_DELAY);
				rc = max17333_read(pmic->regmap_pmic, REG_CONFIG2, &config2_val);
				if (rc < 0) {
					pr_err("%s : (retry:%d/%d) max17333_read returns error no : %d\n", __func__, (READ_CONFIG2_RETRY-retry), READ_CONFIG2_RETRY, rc);
					continue;
				}
				if ((config2_val & MAX17333_CONFIG2_POR_CMD) == 0) {
					retry = 0;
				} else {
					pr_err("%s : (retry:%d/%d) config2(0x%04X) is not cleared\n", __func__, (READ_CONFIG2_RETRY-retry), READ_CONFIG2_RETRY, config2_val);
					continue;
				}
			}

			max17333_set_designcap(pmic);

			if ((config2_val & MAX17333_CONFIG2_POR_CMD) != 0) {
				pr_err("%s : retried %d times (%d ms) but failed\n", __func__, READ_CONFIG2_RETRY, READ_CONFIG2_RETRY * READ_CONFIG2_DELAY);
				return -1;
			}
			return rc;
		}
		pr_info("%s version not changed\n", __func__);
	}

	return rc;
}

static void *max17333_pmic_get_platdata(struct max17333_dev *pmic)
{
#ifdef CONFIG_OF
	struct device *dev = pmic->dev;
	struct device_node *shdw_np, *np = dev->of_node;
	struct i2c_client *client = to_i2c_client(dev);
	struct max17333_pmic_platform_data *pdata;
	int rc;
	unsigned int pval;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(!pdata)) {
		pr_err("<%s> out of memory (%uB requested)\n", client->name,
				(unsigned int) sizeof(*pdata));
		pdata = ERR_PTR(-ENOMEM);
		return pdata;
	}

	rc = of_property_read_u32(np, "rsense", &pval);
	pdata->rsense = (!rc) ? pval : 10;
	pr_info("%s rsense = %d\n", __func__, pdata->rsense);

	rc = of_property_read_u32(np, "battery-type", &pval);
	pdata->battery_type = (!rc) ? pval : 0;
	pr_info("%s battery type = [%s]%d\n", __func__, max17333_type[pdata->battery_type], pdata->battery_type);

	if (of_property_read_bool(np, "max17333,check-shdw-update")) {
		shdw_np = of_get_child_by_name(np, "max17333,shdw-init");
		if (shdw_np) {
			rc = max17333_pmic_get_shdw_platdata(pdata, shdw_np);
			if (rc < 0) {
				pr_warn("%s read shdw-init(%d) failed\n", __func__, pdata->battery_type);
				pdata->check_shdw_update = false;
			} else {
				pr_info("%s read shdw-init(%d) succeeded\n", __func__, pdata->battery_type);
				pdata->check_shdw_update = true;
				/* Print Debug Messge */
				 max17333_print_shdw_data(pdata);
			}
		}
	} else {
		pr_warn("%s max17333,check-nv-update is not enable\n", __func__);
		pdata->check_shdw_update = false;
	}

	return pdata;

#else /* CONFIG_OF */
	return dev_get_platdata(pmic->dev) ?
		dev_get_platdata(pmic->dev) : ERR_PTR(-EINVAL);
#endif /* CONFIG_OF */
}

#if 0
static void max17333_manual_chg_enable(struct max17333_dev *pmic)
{
	u16 rval;
	int ret;

	pr_info("%s: manual chg read back : start\n", __func__);

	ret = max17333_read(pmic->regmap_pmic, REG_CONFIG, &rval);
	if (ret < 0)
		return;

	ret = max17333_write(pmic->regmap_pmic, REG_CONFIG, (rval | MAX17333_CONFIG_MANCHG));

	pr_info("%s: manual chg read back : end\n", __func__);
}
#endif

static int max17333_pre_init_data(struct max17333_dev *pmic)
{
#ifdef CONFIG_OF
	int size = 0, cnt = 0, rc = 0;
	struct device *dev = pmic->dev;
	struct device_node *np = dev->of_node;
	u16 *init_data;

	pr_info("%s: max17333_pmic_get_platdata\n", __func__);
	pmic->pdata = max17333_pmic_get_platdata(pmic);
	if (IS_ERR(pmic->pdata)) {
		rc = PTR_ERR(pmic->pdata);
		pmic->pdata = NULL;
		pr_err("platform data is missing [%d]\n", rc);
		return rc;
	}

	if (pmic->pdata->check_shdw_update) {
		pr_info("%s: shadow ram update : start\n", __func__);
		rc = max17333_pmic_shdw_update(pmic);
		if (rc < 0) {
			pr_info("%s: update failed -> Full Reset\n", __func__);
			max17333_write(pmic->regmap_pmic, REG_COMMAND, MAX17333_COMM_FULL_RESET);
			msleep(100);
			return -1;
		}
#if 0
		/* need to check : ManChg bit may not be activated due to Power-On-Reset */
		max17333_manual_chg_enable(pmic);
		/* Wait 500 ms for Manual chg enable */
		msleep(500);
#endif
	}

	pr_info("%s: Pmic initialize\n", __func__);

	size = device_property_read_u16_array(dev, "max17333,pmic-init", NULL, 0);
	if (size > 0) {
		init_data = kmalloc(size, GFP_KERNEL);
		of_property_read_u16_array(np, "max17333,pmic-init", init_data, size);
		for (cnt = 0; cnt < size; cnt += 2) {
			max17333_write(pmic->regmap_pmic,
				(u8)(init_data[cnt]), init_data[cnt+1]);
		}
		kfree(init_data);
	}

#endif

	/* nODSCTh.OCMargin = 2 */
	max17333_update_bits(pmic->regmap_shdw, REG_NODSCTH_SHDW, MAX17333_NODSCTH_OCMARGIN, (2<<MAX17333_NODSCTH_OCMARGIN_SHIFT));

	return 0;
}

static int max17333_pmic_setup(struct max17333_dev *pmic)
{
	struct device *dev = pmic->dev;
	struct i2c_client *client = to_i2c_client(dev);
	int rc = 0;

	pr_info("%s: max17333_pmic_irq_int\n", __func__);
	/* IRQ init */
	rc = max17333_pmic_irq_int(pmic);
	if (rc != 0) {
		dev_err(&client->dev, "failed to initialize irq: %d\n", rc);
	}

	if (pmic->pdata->battery_type) {
		pr_info("%s: max17333_add_devices\n", __func__);
		rc = max17333_add_devices(pmic, max17333_devices,
				ARRAY_SIZE(max17333_devices));
	} else {
		pr_info("%s: max17333_add_devices_sub\n", __func__);
		rc = max17333_add_devices(pmic, max17333_devices_sub,
				ARRAY_SIZE(max17333_devices_sub));
	}
	//if (IS_ERR_VALUE(rc)) {
	if (rc != 0) {
		pr_err("<%s> failed to add sub-devices [%d]\n",
			client->name, rc);
		goto err_add_devices;
	}

	/* set device able to wake up system */
	device_init_wakeup(dev, true);
	if (pmic->irq)
		enable_irq_wake((unsigned int)pmic->irq);

	pr_info("%s: Done\n", __func__);
	return 0;

err_add_devices:
	if (pmic->irq)
		regmap_del_irq_chip(pmic->irq, pmic->irqc_intsrc);

	return rc;
}

/*******************************************************************************
 *** MAX17333 MFD Core
 ******************************************************************************/

static __always_inline void max17333_destroy(struct max17333_dev *pmic)
{
	struct device *dev = pmic->dev;

	mfd_remove_devices(pmic->dev);

	if (likely(pmic->irq > 0))
		regmap_del_irq_chip(pmic->irq, pmic->irqc_intsrc);

	if (likely(pmic->irq_gpio >= 0))
		gpio_free((unsigned int)pmic->irq_gpio);

	if (likely(pmic->regmap_pmic))
		regmap_exit(pmic->regmap_pmic);

	if (likely(pmic->regmap_shdw))
		regmap_exit(pmic->regmap_shdw);

#ifdef CONFIG_OF
	if (likely(pmic->pdata))
		devm_kfree(dev, pmic->pdata);
#endif /* CONFIG_OF */

	mutex_destroy(&pmic->lock);
	devm_kfree(dev, pmic);
}

static int max17333_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct max17333_dev *pmic;
	int rc;

	pr_info("%s: max17333 I2C Driver Loading\n", __func__);

	pmic = devm_kzalloc(&client->dev, sizeof(*pmic), GFP_KERNEL);
	if (unlikely(!pmic)) {
		pr_err("<%s> out of memory (%uB requested)\n", client->name,
				(unsigned int) sizeof(*pmic));
		return -ENOMEM;
	}

	i2c_set_clientdata(client, pmic);

	mutex_init(&pmic->lock);
	pmic->dev      = &client->dev;

	if (!client->irq) {
		pr_err("<%s>Invalid irq num\n", client->name);
		pmic->irq = -1;
	} else {
		pr_info("<%s> irq [%d]\n", client->name, pmic->irq);
		pmic->irq = client->irq;
	}

	pmic->irq_gpio = -1;

	pmic->pmic = client;

	pmic->regmap_pmic = devm_regmap_init_i2c(client, &max17333_regmap_config);
	if (IS_ERR(pmic->regmap_pmic)) {
		rc = PTR_ERR(pmic->regmap_pmic);
		pmic->regmap_pmic = NULL;
		pr_err("<%s> failed to initialize i2c\n",
			client->name);
		pr_err("<%s> regmap pmic [%d]\n",
			client->name,	rc);
		goto abort;
	}

	pmic->shdw = i2c_new_dummy_device(client->adapter, I2C_ADDR_SHDW);

	if (!pmic->shdw) {
		rc = -ENOMEM;
		goto abort;
	}

	i2c_set_clientdata(pmic->shdw, pmic);
	pmic->regmap_shdw = devm_regmap_init_i2c(pmic->shdw, &max17333_regmap_config_shdw);
	if (IS_ERR(pmic->regmap_shdw)) {
		rc = PTR_ERR(pmic->regmap_shdw);
		pmic->regmap_shdw = NULL;
		pr_err("<%s> failed to initialize i2c\n",
			client->name);
		pr_err("<%s> regmap shdw [%d]\n",
			client->name,	rc);
		goto abort;
	}

	/* unlock Write Protection */
	rc = max17333_lock_write_protection(pmic, false);

	max17333_pre_init_data(pmic);

	rc = max17333_pmic_setup(pmic);
	if (rc != 0) {
		pr_err("<%s> failed to set up interrupt\n",
			client->name);
		pr_err("<%s> and add sub-device [%d]\n",
			client->name,	rc);
		goto abort;
	}

	pr_info("%s [%s]: Done to probe\n", __func__, max17333_type[pmic->pdata->battery_type]);
	return 0;

abort:
	pr_err("%s: Failed to probe\n", __func__);
	i2c_set_clientdata(client, NULL);
	max17333_destroy(pmic);
	return rc;
}

static int max17333_i2c_remove(struct i2c_client *client)
{
	struct max17333_dev *pmic = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	max17333_destroy(pmic);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max17333_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	pr_info("<%s> suspending\n", client->name);

	return 0;
}

static int max17333_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	pr_info("<%s> resuming\n", client->name);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(max17333_pm, max17333_suspend, max17333_resume);

#ifdef CONFIG_OF
static const struct of_device_id max17333_of_id[] = {
	{.compatible = "maxim,max17333"},
	{ },
};
MODULE_DEVICE_TABLE(of, max17333_of_id);
#endif /* CONFIG_OF */

static const struct i2c_device_id max17333_i2c_id[] = {
	{ MAX17333_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max17333_i2c_id);

static struct i2c_driver max17333_i2c_driver = {
	.driver.name            = DRIVER_NAME,
	.driver.owner           = THIS_MODULE,
	.driver.pm              = &max17333_pm,
#ifdef CONFIG_OF
	.driver.of_match_table  = max17333_of_id,
#endif /* CONFIG_OF */
	.id_table               = max17333_i2c_id,
	.probe                  = max17333_i2c_probe,
	.remove                 = max17333_i2c_remove,
};

static __init int max17333_init(void)
{
	int rc = -ENODEV;

	rc = i2c_add_driver(&max17333_i2c_driver);
	if (rc != 0)
		pr_err("Failed to register I2C driver: %d\n", rc);

	pr_info("%s: Added I2C Driver\n", __func__);

	return rc;
}
module_init(max17333_init);

static __exit void max17333_exit(void)
{
	i2c_del_driver(&max17333_i2c_driver);
}
module_exit(max17333_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAX17333 Charger");
MODULE_VERSION(MAX17333_RELEASE_VERSION);
