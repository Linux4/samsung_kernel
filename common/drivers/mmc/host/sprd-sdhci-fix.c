/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/notifier.h>
#include <linux/wakelock.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mmc/sprd-sdhci.h>
#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/pinmap.h>
#include "sdhci.h"
#include "sprd-sdhci-regulator.h"

extern void mmc_power_off(struct mmc_host *host);
extern void mmc_power_cycle(struct mmc_host *host);
extern void mmc_set_timing(struct mmc_host *host, unsigned int timing);

#define DRIVER_NAME "sprd-sdhci"
#define SPRD_SDHCI_HOST_DEFAULT_CLOCK 26000000
#define SDHCI_HOST_TO_SPRD_HOST(host) ((struct sprd_sdhci_host *)sdhci_priv(host))
#define  SDHCI_FIX_PRE_COUNT	15

struct sprd_sdhci_host_fix {
	unsigned int quirks;
	unsigned int quirks2;
	void (*probe)(struct sdhci_host *host);
};

struct sprd_mmc_core_fix {
	unsigned int start_version, end_version;
	void (*probe)(struct sdhci_host *host);
	void (*remove)(struct sdhci_host *host);
};

struct sprd_sdhci_host {
	unsigned int clk_enabled:1;
	unsigned int ro:1;
	unsigned char chip_select_last;
	unsigned char	timing;
	atomic_t wake_lock_count;
	spinlock_t lock;
	const struct of_device_id *of_id;
	struct mmc_host_ops mmc_host_ops, standard_mmc_host_ops;
	struct sdhci_ops sdhci_host_ops;
	struct tasklet_struct finish_tasklet;
	struct tasklet_struct card_tasklet;
	struct wake_lock wake_lock;
	struct sprd_sdhci_host_platdata *platdata;
	struct sdhci_host *host;
	struct clk  *clk, *clk_parent;
	struct delayed_work detect, saved_detect;
	struct notifier_block vmmc_nb;
	struct regulator_dev *vmmc_rdev;
};

static void sdhci_clear_set_irqs(struct sdhci_host *host, u32 clear, u32 set)
{
	u32 ier;

	ier = sdhci_readl(host, SDHCI_INT_ENABLE);
	ier &= ~clear;
	ier |= set;
	sdhci_writel(host, ier, SDHCI_INT_ENABLE);
	sdhci_writel(host, ier, SDHCI_SIGNAL_ENABLE);
}

static void sdhci_reset(struct sdhci_host *host, u8 mask)
{
	unsigned long timeout;
	u32 uninitialized_var(ier);

	if (host->quirks & SDHCI_QUIRK_NO_CARD_NO_RESET) {
		if (!(sdhci_readl(host, SDHCI_PRESENT_STATE) &
			SDHCI_CARD_PRESENT))
			return;
	}

	if (host->quirks & SDHCI_QUIRK_RESTORE_IRQS_AFTER_RESET)
		ier = sdhci_readl(host, SDHCI_INT_ENABLE);

	if (host->ops->platform_reset_enter)
		host->ops->platform_reset_enter(host, mask);

	sdhci_writeb(host, mask | 0x08, SDHCI_SOFTWARE_RESET);

	if (mask & SDHCI_RESET_ALL)
		host->clock = 0;

	/* Wait max 100 ms */
	timeout = 100;

	/* hw clears the bit when it's done */
	while (sdhci_readb(host, SDHCI_SOFTWARE_RESET) & mask) {
		if (timeout == 0) {
			printk("%s: Reset 0x%x never completed.\n",
				mmc_hostname(host->mmc), (int)mask);
			return;
		}
		timeout--;
		mdelay(1);
	}

	if (host->ops->platform_reset_exit)
		host->ops->platform_reset_exit(host, mask);

	if (host->quirks & SDHCI_QUIRK_RESTORE_IRQS_AFTER_RESET)
		sdhci_clear_set_irqs(host, SDHCI_INT_ALL_MASK, ier);

	if (host->flags & (SDHCI_USE_SDMA | SDHCI_USE_ADMA)) {
		if ((host->ops->enable_dma) && (mask & SDHCI_RESET_ALL))
			host->ops->enable_dma(host);
	}
}

static void inline sprd_sdhci_host_wake_lock(struct wake_lock *lock) {
	struct sprd_sdhci_host *sprd_host = container_of(lock, struct sprd_sdhci_host, wake_lock);
	if(atomic_inc_return(&sprd_host->wake_lock_count) == 1) {
		wake_lock(lock);
	}
}

static void inline sprd_sdhci_host_wake_unlock(struct wake_lock *lock) {
	struct sprd_sdhci_host *sprd_host = container_of(lock, struct sprd_sdhci_host, wake_lock);
	if(atomic_dec_return(&sprd_host->wake_lock_count) == 0) {
		wake_unlock(lock);
	}
}

static void sprd_sdhci_host_close_clock(struct sdhci_host *host) {
	u16 clk;
	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
	udelay(200);
	clk &= ~SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

static ssize_t sprd_sdhci_host_detect_irq_restore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sprd_sdhci_host *sprd_host;
	struct sprd_sdhci_host_platdata *host_pdata;
	if(buf == NULL)
		return -1;
	if(!host)
		return -1;
	sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	if(!sprd_host)
		return -1;
	host_pdata = sprd_host->platdata;
	if(!host_pdata)
		return -1;
	if(!host->mmc)
		return -1;
	mmc_detect_change(host->mmc, 0);
	return count;
}

static ssize_t sprd_sdhci_host_keep_power_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sprd_sdhci_host *sprd_host;
	struct sprd_sdhci_host_platdata *host_pdata;
	if(buf == NULL)
		return -1;
	if(!host)
		return -1;
	sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	if(!sprd_host)
		return -1;
	host_pdata = sprd_host->platdata;
	if(!host_pdata)
		return -1;
	return sprintf(buf, "%u\n", host_pdata->keep_power);
}

static ssize_t sprd_sdhci_host_keep_power_restore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	unsigned long flags;
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sprd_sdhci_host *sprd_host;
	struct sprd_sdhci_host_platdata *host_pdata;
	if(buf == NULL)
		return -1;
	if(!host)
		return -1;
	sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	if(!sprd_host)
		return -1;
	host_pdata = sprd_host->platdata;
	if(!host_pdata)
		return -1;
	device_lock(dev);
	spin_lock_irqsave(&sprd_host->lock, flags);
	host_pdata->keep_power = ((buf == NULL || buf[0] == 0 || buf[0] == '0') ? 0 : 1);
	spin_unlock_irqrestore(&sprd_host->lock, flags);
	device_unlock(dev);
	return count;
}

static ssize_t sprd_sdhci_timing_show(struct device *dev, struct device_attribute *attr, char *buf) {
	unsigned long flags;
	unsigned char timing = MMC_TIMING_LEGACY;
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	if(buf == NULL)
		return -1;
	if(!host)
		return -1;
	if(!host->mmc)
		return -1;
	spin_lock_irqsave(&host->lock, flags);
	timing = host->mmc->ios.timing;
	spin_unlock_irqrestore(&host->lock, flags);
	return sprintf(buf, "%u\n", timing);
}

static ssize_t sprd_sdhci_host_timing_restore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	unsigned int timing;
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	if(buf == NULL)
		return -1;
	if(!host)
		return -1;
	if(!host->mmc)
		return -1;
	timing =  buf[0] - '0';
	if(timing > MMC_TIMING_MMC_HS200)
		return -1;
	device_lock(dev);
	mmc_claim_host(host->mmc);
	mmc_set_timing(host->mmc, timing);
	mmc_release_host(host->mmc);
	device_unlock(dev);
	return count;
}

#ifdef CONFIG_PM_RUNTIME
static ssize_t sprd_sdhci_host_runtime_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	if(buf == NULL)
		return -1;
	if(!host)
		return -1;
	if(!pm_runtime_enabled(dev))
		return sprintf(buf, "forbidden\n");
	else if(atomic_read(&dev->power.usage_count) > 0)
		return sprintf(buf, "disabled\n");
	else if(pm_runtime_suspended(dev))
		return sprintf(buf, "suspended\n");
	else
		return sprintf(buf, "resumed\n");
}

static ssize_t sprd_sdhci_host_runtime_restore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	int enable;
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	if(buf == NULL)
		return -1;
	if(!host)
		return -1;
	if(!pm_runtime_enabled(dev))
		return -1;
	enable = (buf[0] == 0 || buf[0] == '0') ? 0 : 1;
	if(enable)
		pm_runtime_put_autosuspend(dev);
	else
		pm_runtime_get_sync(dev);
	return count;
}
#endif

static void sprd_sdhci_host_enable_clock(struct sdhci_host *host, int enable) {
	unsigned long flags;
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	if(enable) {
		/*spin_lock_irqsave(&sprd_host->lock, flags);*/
		if(!sprd_host->clk_enabled) {
			sprd_host->clk_enabled = true;
			clk_prepare_enable(sprd_host->clk);
		}
		/*spin_unlock_irqrestore(&sprd_host->lock, flags);*/
	} else {
		/*spin_lock_irqsave(&sprd_host->lock, flags);*/
		if(sprd_host->clk_enabled) {
			sprd_host->clk_enabled = false;
			clk_disable_unprepare(sprd_host->clk);
		}
		/*spin_unlock_irqrestore(&sprd_host->lock, flags);*/
	}
}

static void sprd_sdhci_host_set_clock(struct sdhci_host *host, unsigned int clock) {
	sprd_sdhci_host_close_clock(host);
	if (clock > 400000) {
		if (host->quirks & SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK) {
			host->timeout_clk = clock / 1000;
			host->mmc->max_discard_to = (1 << 27) / host->timeout_clk;
		}
	}
}

static unsigned int sprd_sdhci_host_get_max_clock(struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	return (host_pdata->max_frequency) ? : SPRD_SDHCI_HOST_DEFAULT_CLOCK;
}

static void sprd_sdhci_host_hw_reset(struct sdhci_host *host) {
	printk("%s,sprd_sdhci_host_hw_reset\n",mmc_hostname(host->mmc));
	mmc_power_off(host->mmc);
	msleep(299);
	mmc_power_cycle(host->mmc);
}

static unsigned int sprd_sdhci_host_get_ro(struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	return sprd_host->ro;
}

static void sprd_sdhci_host_redirect_platform_send_init_74_clocks_to_chip_select(struct sdhci_host *host, u8 power_mode) {
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	spin_lock(&sprd_host->lock);
	if(sprd_host->chip_select_last ==  MMC_CS_DONTCARE && mmc->ios.chip_select == MMC_CS_HIGH) {
		sprd_host->chip_select_last = MMC_CS_HIGH;
		if(host_pdata->d3_gpio > 0 && gpio_is_valid(host_pdata->d3_gpio)) {
			unsigned int reg_val;
			reg_val = pinmap_get(host_pdata->pinmap_offset + host_pdata->d3_index);
			reg_val &= ~(3UL << 4);
			reg_val |= (host_pdata->gpio_func << 4);
			pinmap_set( host_pdata->pinmap_offset + host_pdata->d3_index, reg_val);
			if(!gpio_request(host_pdata->d3_gpio, "d3-gpio")) {
				gpio_direction_output(host_pdata->d3_gpio, 1);
				gpio_set_value(host_pdata->d3_gpio, 1);
				gpio_free(host_pdata->d3_gpio);
			} else {
				reg_val &= ~(3UL << 4);
				reg_val |= (host_pdata->sd_func << 4);
				pinmap_set(host_pdata->pinmap_offset + host_pdata->d3_index, reg_val);
			}
		}
	} else  if(sprd_host->chip_select_last ==  MMC_CS_HIGH && mmc->ios.chip_select == MMC_CS_DONTCARE) {
		sprd_host->chip_select_last = MMC_CS_DONTCARE;
		if(host_pdata->d3_gpio > 0 && gpio_is_valid(host_pdata->d3_gpio)) {
			unsigned int reg_val;
			reg_val = pinmap_get(host_pdata->pinmap_offset + host_pdata->d3_index);
			reg_val &=  ~(3UL << 4);
			reg_val |= (host_pdata->sd_func << 4);
			pinmap_set(host_pdata->pinmap_offset + host_pdata->d3_index, reg_val);
		}
	}
	spin_unlock(&sprd_host->lock);
}

static int sprd_sdhci_host_set_uhs_signaling(struct sdhci_host *host, unsigned int uhs) {
	u16 clk, div, ctrl_2;
	unsigned int pre_addr = 0;
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	if (uhs == MMC_TIMING_MMC_HS200)
		ctrl_2 |= SDHCI_CTRL_HS_SDR200;
	else if (uhs == MMC_TIMING_UHS_SDR12) {
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
		pre_addr = SDHCI_PRESET_FOR_SDR12;
	} else if (uhs == MMC_TIMING_UHS_SDR25) {
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
		pre_addr = SDHCI_PRESET_FOR_SDR25;
	} else if (uhs == MMC_TIMING_UHS_SDR50) {
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
		pre_addr = SDHCI_PRESET_FOR_SDR50;
	} else if (uhs == MMC_TIMING_UHS_SDR104) {
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
		pre_addr = SDHCI_PRESET_FOR_SDR104;
	} else if (uhs == MMC_TIMING_UHS_DDR50) {
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
		pre_addr = SDHCI_PRESET_FOR_DDR50;
		/* set write/read delay value */
		if(host_pdata->write_delay)
			sdhci_writel(host, host_pdata->write_delay , 0x0080);
		if(host_pdata->read_pos_delay)
			sdhci_writel(host, host_pdata->read_pos_delay , 0x0084);
		if(host_pdata->read_neg_delay)
			sdhci_writel(host, host_pdata->read_neg_delay , 0x0088);
	}
	
	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);
	#if 0
	if (pre_addr) {
		if(!(host->quirks2 & SDHCI_QUIRK2_PRESET_VALUE_BROKEN)) {
			clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
			div = ((clk  >> SDHCI_DIVIDER_SHIFT) & SDHCI_DIV_MASK) | (((clk >> SDHCI_DIVIDER_HI_SHIFT) & (SDHCI_DIV_HI_MASK >> SDHCI_DIV_MASK_LEN)) << SDHCI_DIVIDER_SHIFT);
			sdhci_writel(host, div, pre_addr);
		}
	}
	#endif
	return 0;
}

static void sprd_sdhci_host_detect_work(struct work_struct *work) {
	unsigned long flags;
	struct mmc_host *mmc = container_of(work, struct mmc_host, detect.work);
	struct sdhci_host *host = mmc_priv(mmc);
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	spin_lock_irqsave(&host->lock, flags);
	spin_lock(&sprd_host->lock);
	mmc->detect = sprd_host->saved_detect;
	smp_wmb();
	spin_unlock(&sprd_host->lock);
	spin_unlock_irqrestore(&host->lock, flags);
	mmc->detect_change = 0;
	wake_unlock(&mmc->detect_wake_lock);
}

int sprd_sdhci_host_regulator_vmmc_notify(struct notifier_block *nb, unsigned long action, void *data) {
	int retval = 0;
	struct sprd_sdhci_host *sprd_host = container_of(nb, struct sprd_sdhci_host, vmmc_nb);
	struct sdhci_host *host = sprd_host->host;
	if(host && host->vqmmc) {
		switch(action) {
			case REGULATOR_EVENT_ENABLE:
				if(!regulator_is_enabled(host->vqmmc))
					retval = regulator_enable(host->vqmmc);
				break;
			case REGULATOR_EVENT_DISABLE:
				if(regulator_is_enabled(host->vqmmc))
					retval = regulator_disable(host->vqmmc);
				break;
			case REGULATOR_EVENT_VOLTAGE_CHANGE:
			default:
				break;
		}
	}
	return retval;
}

static void sprd_sdhci_host_sdhci_request(struct mmc_host *mmc, struct mmc_request *mrq) {
	struct sdhci_host *host = mmc_priv(mmc);
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_sdhci_host_wake_lock(&sprd_host->wake_lock);
	sprd_host->standard_mmc_host_ops.request(mmc, mrq);
}

static void sprd_sdhci_host_sdhci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios) {
	struct sdhci_host *host = mmc_priv(mmc);
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_sdhci_host_wake_lock(&sprd_host->wake_lock);
	sprd_host->standard_mmc_host_ops.set_ios(mmc, ios);
	sprd_sdhci_host_wake_unlock(&sprd_host->wake_lock);
}

static int sprd_sdhci_host_sdhci_start_signal_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios) {
	int err;
	struct sdhci_host *host = mmc_priv(mmc);
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_sdhci_host_wake_lock(&sprd_host->wake_lock);
	err = sprd_host->standard_mmc_host_ops.start_signal_voltage_switch(mmc, ios);
	sprd_sdhci_host_wake_unlock(&sprd_host->wake_lock);
	return err;
}

static int sprd_sdhci_host_sdhci_card_busy(struct mmc_host *mmc) {
	int err;
	struct sdhci_host *host = mmc_priv(mmc);
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_sdhci_host_wake_lock(&sprd_host->wake_lock);
	err = sprd_host->standard_mmc_host_ops.card_busy(mmc);
	sprd_sdhci_host_wake_unlock(&sprd_host->wake_lock);
	return err;
}

static void sprd_sdhci_host_tasklet_finish(unsigned long param) {
	struct sdhci_host *host;
	struct sprd_sdhci_host *sprd_host;
	host = (struct sdhci_host*)param;
	sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_host->finish_tasklet.func(param);
	sprd_sdhci_host_wake_unlock(&sprd_host->wake_lock);
}

static int sprd_sdhci_host_get_cd(struct mmc_host *mmc) {
	int gpio_cd;
	struct sdhci_host *host = mmc_priv(mmc);

	if (host->flags & SDHCI_DEVICE_DEAD)
		return 0;

	/* If polling/nonremovable, assume that the card is always present. */
	if (host->mmc->caps & MMC_CAP_NONREMOVABLE)
		return 1;

	gpio_cd = mmc_gpio_get_cd(mmc);
	/* Try slot gpio detect */
	if(!IS_ERR_VALUE(gpio_cd))
		return !!gpio_cd;

	return 0;
}

static void sprd_sdhci_host_card_event(struct mmc_host *mmc) {
	int irq;
	int gpio;
	int value;
	unsigned long flags;
	struct sdhci_host *host = mmc_priv(mmc);
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);

	irq = mmc->slot.cd_irq;
	gpio = irq_to_gpio(irq);
	value = gpio_get_value(gpio);
	
	sprd_sdhci_host_wake_lock(&sprd_host->wake_lock);

	spin_lock_irqsave(&host->lock, flags);

	/* Check host->mrq first in case we are runtime suspended */
	if (host->mrq && value) {
		printk("%s: Card removed during transfer!\n",
			mmc_hostname(host->mmc));
		printk("%s: Resetting controller.\n",
			mmc_hostname(host->mmc));

		sdhci_reset(host, SDHCI_RESET_CMD);
		sdhci_reset(host, SDHCI_RESET_DATA);

		host->mrq->cmd->error = -ENOMEDIUM;
		tasklet_schedule(&host->finish_tasklet);
	}

	spin_unlock_irqrestore(&host->lock, flags);
	sprd_sdhci_host_wake_unlock(&sprd_host->wake_lock);
}

static void sprd_sdhci_host_platform_reset_enter(struct sdhci_host *host, u8 mask) {
#if 0
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
#endif
	if (mask & SDHCI_RESET_ALL) {
#if 0
		unsigned long flags;
		unsigned int tmp_flag = 0;
#endif
		sprd_sdhci_host_close_clock(host);
		#if 0
		spin_lock_irqsave(&sprd_host->lock, flags);
		sprd_host->chip_select_last = MMC_CS_DONTCARE;
		/* if sdio0 set data[3] to gpio out mode, and data is 1 , for shark*/
		if((tmp_flag & 1) != 0) {
			sci_glb_clr(REG_AON_APB_APB_EB0, BIT_GPIO_EB);
		}
		if((tmp_flag & (1 << 1)) != 0) {
			sci_glb_clr(REG_AON_APB_APB_EB0, BIT_PIN_EB);
		}
		/* set sdio0 data[3] to sdio data pin*/
		__raw_writel(reg_val, (volatile void __iomem *)(SPRD_PIN_BASE + 0x1E0));
		spin_unlock_irqrestore(&sprd_host->lock, flags);
		#endif
	}
}

static void sprd_sdhci_host_platform_reset_exit(struct sdhci_host *host, u8 mask) {
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	if (mask & SDHCI_RESET_ALL) {
		if (pdev->id == SDC_SLAVE_WIFI || pdev->id == SDC_SLAVE_CP) {
			unsigned int tmp_val;
			tmp_val = sdhci_readw(host, SDHCI_TRANSFER_MODE);
			tmp_val &= 0xFFFFF8FF;		// clear bit[10:8];
			sdhci_writew(host, tmp_val, SDHCI_TRANSFER_MODE);
		}
	}
}

static void sprd_sdhci_host_fix_sprd_host_chip_select(struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_host->sdhci_host_ops.platform_send_init_74_clocks = sprd_sdhci_host_redirect_platform_send_init_74_clocks_to_chip_select;
}

static void sprd_sdhci_host_fix_sprd_host_execute_tuning(struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_host->mmc_host_ops.execute_tuning = NULL;
}

static void sprd_sdhci_host_fix_sprd_host_sd_uhsi_1p8v(struct sdhci_host *host) {
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	if(pdev->id == SDC_SLAVE_SD)
		host->quirks2 |= SDHCI_QUIRK2_NO_1_8_V;
}

static void sprd_sdhci_host_fix_mmc_core_detect_work(struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_host->saved_detect = host->mmc->detect;
	INIT_DELAYED_WORK(&host->mmc->detect, sprd_sdhci_host_detect_work);
}

static void sprd_sdhci_host_fix_mmc_core_get_regulator(struct sdhci_host *host) {
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata =  sprd_host->platdata;
	sprd_host->vmmc_nb.notifier_call = sprd_sdhci_host_regulator_vmmc_notify;
	sprd_host->vmmc_rdev = sprd_sdhci_regulator_init(pdev, &sprd_host->vmmc_nb, host_pdata->vdd_extmmc);
	if(!IS_ERR_OR_NULL(sprd_host->vmmc_rdev)) {
		struct regulator *vmmc, *vqmmc;
		vmmc = regulator_get(mmc_dev(host->mmc), "vmmc");
		if(!IS_ERR_OR_NULL(vmmc)) {
			u32 caps;
			unsigned int ocr_avail;
			ocr_avail = mmc_regulator_get_ocrmask(vmmc);
			regulator_put(vmmc);
			caps = sdhci_readl(host, SDHCI_CAPABILITIES);
			if(!((caps & SDHCI_CAN_VDD_330) && (ocr_avail & (MMC_VDD_32_33 | MMC_VDD_33_34)))) {
				ocr_avail &= ~(MMC_VDD_32_33 | MMC_VDD_33_34);
				caps &= ~ SDHCI_CAN_VDD_330;
				host->quirks |= SDHCI_QUIRK_MISSING_CAPS;
			}
			if(!((caps & SDHCI_CAN_VDD_300) && (ocr_avail & (MMC_VDD_29_30 | MMC_VDD_30_31)))) {
				ocr_avail &= ~(MMC_VDD_29_30 | MMC_VDD_30_31);
				caps &= ~ SDHCI_CAN_VDD_300;
				host->quirks |= SDHCI_QUIRK_MISSING_CAPS;
			}
			if(!((caps & SDHCI_CAN_VDD_180) && (ocr_avail & (MMC_VDD_165_195)))) {
				ocr_avail &= ~(MMC_VDD_165_195);
				caps &= ~ SDHCI_CAN_VDD_180;
				host->quirks |= SDHCI_QUIRK_MISSING_CAPS;
			}
			if(host->quirks & SDHCI_QUIRK_MISSING_CAPS) {
				host->caps = caps;
				host->caps1 = sdhci_readl(host, SDHCI_CAPABILITIES_1);
			}
			host->ocr_avail_mmc = host->ocr_avail_sd = host->ocr_avail_sdio = ocr_avail;
		}
		if(host_pdata->init_voltage_level > 0) {
			vqmmc = regulator_get(mmc_dev(host->mmc), "vqmmc");
			if(!IS_ERR_OR_NULL(vqmmc)) {
				int uV = regulator_list_voltage(vqmmc, host_pdata->init_voltage_level - 1);
				if(uV > 0)
					regulator_set_voltage(vqmmc, uV, uV);
				regulator_put(vqmmc);
			}
		}
	}
}

static void sprd_sdhci_host_fix_mmc_core_mmc_ops(struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_host->mmc_host_ops = sprd_host->standard_mmc_host_ops = host->mmc->ops[0];
	host->mmc->ops = (const struct mmc_host_ops *)&sprd_host->mmc_host_ops;
}

static void sprd_sdhci_host_fix_mmc_core_wakelock(struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_host->mmc_host_ops.request = sprd_sdhci_host_sdhci_request;
	sprd_host->mmc_host_ops.set_ios = sprd_sdhci_host_sdhci_set_ios;
	sprd_host->mmc_host_ops.start_signal_voltage_switch = sprd_sdhci_host_sdhci_start_signal_voltage_switch;
	sprd_host->mmc_host_ops.card_busy = sprd_sdhci_host_sdhci_card_busy;
	sprd_host->finish_tasklet = host->finish_tasklet;
	tasklet_init(&host->finish_tasklet, sprd_sdhci_host_tasklet_finish, (unsigned long)host);
}

static void sprd_sdhci_host_fix_mmc_core_get_cd(struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	if(host_pdata->detect_gpio > 0 && pdev->id != SDC_SLAVE_CP) {
		host->mmc->caps &= ~MMC_CAP_NONREMOVABLE;
		sprd_host->mmc_host_ops.get_cd = sprd_sdhci_host_get_cd;
	}
}

static void sprd_sdhci_host_fix_mmc_core_need_poll(struct sdhci_host *host) {
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	if (host_pdata->detect_gpio > 0 || pdev->id == SDC_SLAVE_WIFI || pdev->id == SDC_SLAVE_SD)
		host->mmc->caps &= ~MMC_CAP_NEEDS_POLL;
}

static void sprd_sdhci_host_fix_mmc_core_card_event(struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	if(host_pdata->detect_gpio > 0) {
#ifndef CONFIG_OF
		int retval;
		retval = mmc_gpio_request_cd(host->mmc, host_pdata->detect_gpio);
		if(retval <  0)
			return;
#endif
		sprd_host->mmc_host_ops.card_event = sprd_sdhci_host_card_event;
	}
}

static int sprd_sdhci_host_get_clock(struct platform_device *pdev, struct sdhci_host *host) {
#ifndef CONFIG_OF
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	BUG_ON( host_pdata->clk_name == NULL);
	BUG_ON( host_pdata->clk_parent_name == NULL);
	sprd_host->clk = clk_get(NULL, host_pdata->clk_name);
	if(IS_ERR_OR_NULL(sprd_host->clk))
		return PTR_ERR(sprd_host->clk);
	sprd_host->clk_parent = clk_get(NULL, host_pdata->clk_parent_name);
	if(IS_ERR_OR_NULL(sprd_host->clk_parent))
		return PTR_ERR(sprd_host->clk_parent);
	clk_set_parent(sprd_host->clk, sprd_host->clk_parent);
	return 0;
#else
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct device_node *np = pdev->dev.of_node;
	sprd_host->clk = of_clk_get(np, 0);
	if(IS_ERR_OR_NULL(sprd_host->clk))
		return PTR_ERR(sprd_host->clk);
	sprd_host->clk_parent = of_clk_get(np, 1);
	if(IS_ERR_OR_NULL(sprd_host->clk_parent))
		return PTR_ERR(sprd_host->clk_parent);
	clk_set_parent(sprd_host->clk, sprd_host->clk_parent);
	return 0;
#endif
}

static void sprd_sdhci_host_put_clock(struct platform_device *pdev, struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	if(!IS_ERR_OR_NULL(sprd_host->clk)) {
		clk_disable_unprepare(sprd_host->clk);
		clk_put(sprd_host->clk);
		sprd_host->clk = 0;
	}
	if(!IS_ERR_OR_NULL(sprd_host->clk_parent)) {
		clk_disable_unprepare(sprd_host->clk_parent);
		clk_put(sprd_host->clk_parent);
		sprd_host->clk_parent = 0;
	}
}

static void sprd_sdhci_host_put_detect(struct sdhci_host *host) {
	if(host->mmc->slot.cd_irq > 0)
		mmc_gpio_free_cd(host->mmc);
}

#ifdef CONFIG_PM_RUNTIME
static int sprd_sdhci_host_get_runtime(struct platform_device *pdev, struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	if(!host_pdata->runtime)
		return 0;
	pm_runtime_set_active(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 100);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_suspend_ignore_children(mmc_classdev(host->mmc), true);
	return 0;
}

static void sprd_sdhci_host_put_runtime(struct platform_device *pdev, struct sdhci_host *host) {
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	if(!host_pdata->runtime)
		return;
	if(pm_runtime_enabled(&pdev->dev)) {
		pm_runtime_disable(&pdev->dev);
		pm_runtime_set_suspended(&pdev->dev);
	}
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id sprd_sdhci_host_of_match[];
#endif

static DEVICE_ATTR(detect_irq, S_IWUSR, NULL, sprd_sdhci_host_detect_irq_restore);
static DEVICE_ATTR(keep_power, S_IRUGO | S_IWUSR, sprd_sdhci_host_keep_power_show, sprd_sdhci_host_keep_power_restore);
static DEVICE_ATTR(timing, S_IRUGO | S_IWUSR, sprd_sdhci_timing_show, sprd_sdhci_host_timing_restore);
#ifdef CONFIG_PM_RUNTIME
static DEVICE_ATTR(runtime, S_IRUGO | S_IWUSR, sprd_sdhci_host_runtime_show, sprd_sdhci_host_runtime_restore);
#endif

static const struct attribute *sprd_sdhci_host_attrs[] = {
	&dev_attr_detect_irq.attr,
	&dev_attr_keep_power.attr,
	&dev_attr_timing.attr,
#ifdef CONFIG_PM_RUNTIME
	&dev_attr_runtime.attr,
#endif
	NULL
};

static const struct attribute_group sprd_sdhci_host_attr_group = {
	.attrs = sprd_sdhci_host_attrs,
};

static const struct attribute_group *sprd_sdhci_host_attr_groups[] = {
	&sprd_sdhci_host_attr_group,
	NULL
};

static const struct device_type sprd_sdhci_host_device_type = {
	.name		= "sprd-sdhci-host",
	.groups		= sprd_sdhci_host_attr_groups,
};

static const struct sprd_sdhci_host_fix sprd_sdhci_host_fix_base = {
	.quirks = SDHCI_QUIRK_NO_HISPD_BIT | SDHCI_QUIRK_BROKEN_TIMEOUT_VAL | SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK | SDHCI_QUIRK_BROKEN_CARD_DETECTION | SDHCI_QUIRK_CLOCK_BEFORE_RESET,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

static const struct sprd_sdhci_host_fix sprd_sdhci_host_fix_chip_select = {
	.probe = sprd_sdhci_host_fix_sprd_host_chip_select,
};

static const struct sprd_sdhci_host_fix sprd_sdhci_host_fix_execute_tuning = {
	.probe = sprd_sdhci_host_fix_sprd_host_execute_tuning,
};

static const struct sprd_sdhci_host_fix sprd_sdhci_host_fix_sd_uhsi_1p8v = {
	.probe = sprd_sdhci_host_fix_sprd_host_sd_uhsi_1p8v,
};

static const struct sprd_mmc_core_fix sprd_sdhci_host_fix_detect_work = {
	.start_version = KERNEL_VERSION(3, 10, 0),
	.probe = sprd_sdhci_host_fix_mmc_core_detect_work,
};

static const struct sprd_mmc_core_fix sprd_sdhci_host_fix_regulator = {
	.start_version = KERNEL_VERSION(3, 10, 0),
	.probe = sprd_sdhci_host_fix_mmc_core_get_regulator,
};

static const struct sprd_mmc_core_fix sprd_sdhci_host_fix_mmc_ops = {
	.probe = sprd_sdhci_host_fix_mmc_core_mmc_ops,
};

static const struct sprd_mmc_core_fix sprd_sdhci_host_fix_wakelock = {
	.probe = sprd_sdhci_host_fix_mmc_core_wakelock,
};

static const struct sprd_mmc_core_fix sprd_sdhci_host_fix_get_cd = {
	.start_version = KERNEL_VERSION(3, 10, 0),
	.probe = sprd_sdhci_host_fix_mmc_core_get_cd,
};

static const struct sprd_mmc_core_fix sprd_sdhci_host_fix_need_poll = {
	.start_version = KERNEL_VERSION(3, 10, 0),
	.probe = sprd_sdhci_host_fix_mmc_core_need_poll,
};

static const struct sprd_mmc_core_fix sprd_sdhci_host_fix_card_event = {
	.start_version = KERNEL_VERSION(3, 10, 0),
	.probe = sprd_sdhci_host_fix_mmc_core_card_event,
	.remove = sprd_sdhci_host_put_detect,
};

static const struct sprd_sdhci_host_fix *sprd_sdhci_host_sprd_host_fixes_shark[] = {
	[0] = &sprd_sdhci_host_fix_base,
	[1] = &sprd_sdhci_host_fix_chip_select,
	[2] = &sprd_sdhci_host_fix_sd_uhsi_1p8v,
	[SDHCI_FIX_PRE_COUNT + 0] = &sprd_sdhci_host_fix_execute_tuning,
	[SDHCI_FIX_PRE_COUNT + 1] = NULL,
};

static const struct sprd_sdhci_host_fix *sprd_sdhci_host_sprd_host_fixes_dolphin[] = {
	[0] = &sprd_sdhci_host_fix_base,
	[1] = &sprd_sdhci_host_fix_sd_uhsi_1p8v,
	[SDHCI_FIX_PRE_COUNT + 0] = &sprd_sdhci_host_fix_execute_tuning,
	[SDHCI_FIX_PRE_COUNT + 1] = NULL,
};

static const struct sprd_mmc_core_fix *sprd_sdhci_host_mmc_core_fixes[] = {
	[0] = &sprd_sdhci_host_fix_detect_work,
	[1] = &sprd_sdhci_host_fix_regulator,
	[SDHCI_FIX_PRE_COUNT + 0] = &sprd_sdhci_host_fix_mmc_ops,
	[SDHCI_FIX_PRE_COUNT + 1] = &sprd_sdhci_host_fix_wakelock,
	[SDHCI_FIX_PRE_COUNT + 2] = &sprd_sdhci_host_fix_get_cd,
	[SDHCI_FIX_PRE_COUNT + 3] = &sprd_sdhci_host_fix_need_poll,
	[SDHCI_FIX_PRE_COUNT + 4] = &sprd_sdhci_host_fix_card_event,
};

static const struct sdhci_ops sprd_sdhci_host_default_ops = {
	.set_clock = sprd_sdhci_host_set_clock,
	.get_max_clock = sprd_sdhci_host_get_max_clock,
	.hw_reset = sprd_sdhci_host_hw_reset,
	.get_ro = sprd_sdhci_host_get_ro,
	.set_uhs_signaling = sprd_sdhci_host_set_uhs_signaling,
	.platform_reset_enter = sprd_sdhci_host_platform_reset_enter,
	.platform_reset_exit = sprd_sdhci_host_platform_reset_exit,
};

static void sprd_sdhci_host_fix_sprd_host_pre(struct sdhci_host *host) {
	int i;
	const struct sprd_sdhci_host_fix **sprd_sdhci_host_fixes;
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	const struct platform_device_id *id_entry = pdev->id_entry;
#ifdef CONFIG_OF
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	const struct of_device_id *of_id = sprd_host->of_id;
#endif
	if(id_entry)
		sprd_sdhci_host_fixes = (const struct sprd_sdhci_host_fix **)id_entry->driver_data;
#ifdef CONFIG_OF
	else if(of_id)
		sprd_sdhci_host_fixes = (const struct sprd_sdhci_host_fix **)of_id->data;
#endif
	else
		return;
	for(i = 0; i < SDHCI_FIX_PRE_COUNT && sprd_sdhci_host_fixes[i]; i++) {
		const struct sprd_sdhci_host_fix *sprd_sdhci_host_fix = sprd_sdhci_host_fixes[i];
		if(sprd_sdhci_host_fix->quirks)
			host->quirks |=  sprd_sdhci_host_fix->quirks;
		if(sprd_sdhci_host_fix->quirks2)
			host->quirks2 |=  sprd_sdhci_host_fix->quirks2;
		if(sprd_sdhci_host_fix->probe)
			sprd_sdhci_host_fix->probe(host);
	}
}

static void sprd_sdhci_host_fix_sprd_host_post(struct sdhci_host *host) {
	int i;
	const struct sprd_sdhci_host_fix **sprd_sdhci_host_fixes;
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	const struct platform_device_id *id_entry = pdev->id_entry;
#ifdef CONFIG_OF
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	const struct of_device_id *of_id = sprd_host->of_id;
#endif
	if(id_entry)
		sprd_sdhci_host_fixes = (const struct sprd_sdhci_host_fix **)id_entry->driver_data;
#ifdef CONFIG_OF
	else if(of_id)
		sprd_sdhci_host_fixes = (const struct sprd_sdhci_host_fix **)of_id->data;
#endif
	else
		return;
	for(i = SDHCI_FIX_PRE_COUNT; sprd_sdhci_host_fixes[i]; i++) {
		const struct sprd_sdhci_host_fix *sprd_sdhci_host_fix = sprd_sdhci_host_fixes[i];
		if(sprd_sdhci_host_fix->quirks)
			host->quirks |=  sprd_sdhci_host_fix->quirks;
		if(sprd_sdhci_host_fix->quirks2)
			host->quirks2 |=  sprd_sdhci_host_fix->quirks2;
		if(sprd_sdhci_host_fix->probe)
			sprd_sdhci_host_fix->probe(host);
	}
}

static void sprd_sdhci_host_fix_mmc_core_pre(struct sdhci_host *host) {
	int i;
	for(i = 0; i < SDHCI_FIX_PRE_COUNT && sprd_sdhci_host_mmc_core_fixes[i]; i++) {
		const struct sprd_mmc_core_fix *mmc_core_fix = sprd_sdhci_host_mmc_core_fixes[i];
		if((mmc_core_fix->start_version == 0 || LINUX_VERSION_CODE >=  mmc_core_fix->start_version)
	            && (mmc_core_fix->end_version == 0 || LINUX_VERSION_CODE <  mmc_core_fix->end_version)) {
			if(mmc_core_fix->probe)
				mmc_core_fix->probe(host);
		}
	}
}

static void sprd_sdhci_host_fix_mmc_core_post(struct sdhci_host *host) {
	int i;
	for(i = SDHCI_FIX_PRE_COUNT; i < ARRAY_SIZE(sprd_sdhci_host_mmc_core_fixes); i++) {
		const struct sprd_mmc_core_fix *mmc_core_fix = sprd_sdhci_host_mmc_core_fixes[i];
		if(mmc_core_fix) {
			if((mmc_core_fix->start_version == 0 || LINUX_VERSION_CODE >=  mmc_core_fix->start_version)
		            && (mmc_core_fix->end_version == 0 || LINUX_VERSION_CODE <  mmc_core_fix->end_version)) {
				if(mmc_core_fix->probe)
					mmc_core_fix->probe(host);
			}
		}
	}
}

static void sprd_sdhci_host_fix_mmc_core_remove(struct sdhci_host *host) {
	int i;
	for(i = 0; i < ARRAY_SIZE(sprd_sdhci_host_mmc_core_fixes); i++) {
		const struct sprd_mmc_core_fix *mmc_core_fix = sprd_sdhci_host_mmc_core_fixes[i];
		if(mmc_core_fix) {
			if((mmc_core_fix->start_version == 0 || LINUX_VERSION_CODE >=  mmc_core_fix->start_version)
		            && (mmc_core_fix->end_version == 0 || LINUX_VERSION_CODE <  mmc_core_fix->end_version)) {
				if(mmc_core_fix->remove)
					mmc_core_fix->remove(host);
			}
		}
	}
}

static void sprd_sdhci_host_open(struct sdhci_host *host, struct sprd_sdhci_host *sprd_host) {
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	// ahb enbale sdio controller
	sci_glb_set(REG_AP_AHB_AHB_EB, host_pdata->enb_bit);
	udelay(500);
	sci_glb_set(REG_AP_AHB_AHB_RST, host_pdata->rst_bit);
	udelay(200);
	sci_glb_clr(REG_AP_AHB_AHB_RST, host_pdata->rst_bit);
}

static void sprd_sdhci_host_close(struct sdhci_host *host, struct sprd_sdhci_host *sprd_host) {
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	// close controller clock
	sprd_sdhci_host_close_clock(host);
	// ahb disable sdio controller
	if(host_pdata){
		sci_glb_set(REG_AP_AHB_AHB_EB, host_pdata->enb_bit);
	}
}

#ifdef CONFIG_OF
static void sprd_sdhci_host_of_parse(struct platform_device *pdev, struct sdhci_host *host) {
	unsigned int d3_gpio = 0, d3_index, sd_func, gpio_func;
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	struct device_node *np = pdev->dev.of_node;
	of_property_read_u32(np, "id", &pdev->id);
	of_property_read_u32(np, "caps", &host_pdata->caps);
	of_property_read_u32(np, "caps2", &host_pdata->caps2);
	of_property_read_u32(np, "max-frequency", &host_pdata->max_frequency);
	of_property_read_u32(np, "enb-bit", &host_pdata->enb_bit);
	of_property_read_u32(np, "rst-bit", &host_pdata->rst_bit);
	of_property_read_u32(np, "enb-reg", &host_pdata->enb_reg);
	of_property_read_u32(np, "rst-reg", &host_pdata->rst_reg);
	of_property_read_u32(np, "write-delay", &host_pdata->write_delay);
	of_property_read_u32(np, "read-pos-delay", &host_pdata->read_pos_delay);
	of_property_read_u32(np, "read-neg-delay", &host_pdata->read_neg_delay);
	of_property_read_u32(np, "cd-gpios", &host_pdata->detect_gpio);
	of_property_read_u32(np, "init-voltage-level", &host_pdata->init_voltage_level);
	of_property_read_u32(np, "pinmap-offset", &host_pdata->pinmap_offset);
	of_property_read_u32(np, "d3-gpio", &host_pdata->d3_gpio);
	of_property_read_u32(np, "d3-index", &host_pdata->d3_index);
	of_property_read_u32(np, "sd-func", &host_pdata->sd_func);
	of_property_read_u32(np, "gpio-func", &host_pdata->gpio_func);
	of_property_read_string(np, "vdd-extmmc", &host_pdata->vdd_extmmc);
	of_property_read_u32(np, "keep-power", &host_pdata->keep_power);
	of_property_read_u32(np, "runtime", &host_pdata->runtime);
	mmc_of_parse(host->mmc);
}
#endif

#ifdef CONFIG_PM
static int sprd_sdhci_host_pm_suspend(struct device *dev) {
	int retval = 0;
	unsigned long flags;
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	struct sprd_sdhci_host_platdata *host_pdata = sprd_host->platdata;
	if(pm_runtime_enabled(dev))
		retval = pm_runtime_get_sync(dev);
	if(retval >= 0) {
		spin_lock_irqsave(&host->lock, flags);
		spin_lock(&sprd_host->lock);
		if(host_pdata->keep_power && host->mmc->pm_caps & MMC_PM_KEEP_POWER)
			host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
		spin_unlock(&sprd_host->lock);
		spin_unlock_irqrestore(&host->lock, flags);
		retval = sdhci_suspend_host(host);
		if(!retval) {
			sprd_sdhci_host_wake_lock(&sprd_host->wake_lock);
			spin_lock_irqsave(&host->lock, flags);
			sprd_sdhci_host_close_clock(host);
			sprd_sdhci_host_enable_clock(host, 0);
			spin_unlock_irqrestore(&host->lock, flags);
			sprd_sdhci_host_wake_unlock(&sprd_host->wake_lock);
		} else {
			if(pm_runtime_enabled(dev))
				pm_runtime_put_autosuspend(&pdev->dev);
		}
	}
	return retval;
}

static int sprd_sdhci_host_pm_resume(struct device *dev) {
	int retval = 0;
	unsigned long flags;
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	spin_lock_irqsave(&host->lock, flags);
	sprd_sdhci_host_enable_clock(host, 1);
	spin_unlock_irqrestore(&host->lock, flags);
	udelay(100);
	retval = sdhci_resume_host(host);
	if(pm_runtime_enabled(dev))
		pm_runtime_put_autosuspend(&pdev->dev);
	return retval;
}

#ifdef CONFIG_PM_RUNTIME
static int sprd_sdhci_host_runtime_suspend(struct device *dev) {
	int rc = -EBUSY;
	unsigned long flags;
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	if(dev->driver != NULL) {
		struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
		sprd_sdhci_host_wake_lock(&sprd_host->wake_lock);
		sdhci_runtime_suspend_host(host);
		/*spin_lock_irqsave(&host->lock, flags);*/
		sprd_sdhci_host_close_clock(host);
		sprd_sdhci_host_enable_clock(host, 0);
		/*spin_unlock_irqrestore(&host->lock, flags);*/
		sprd_sdhci_host_wake_unlock(&sprd_host->wake_lock);
		rc = 0;
	}
	return rc;
}

static int sprd_sdhci_host_runtime_resume(struct device *dev) {
	unsigned long flags;
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	if(dev->driver != NULL) {
		struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
		/*spin_lock_irqsave(&host->lock, flags);*/
		sprd_sdhci_host_enable_clock(host, 1);
		/*spin_unlock_irqrestore(&host->lock, flags);*/
		udelay(100);
		if(pdev->id == SDC_SLAVE_EMMC && host->mmc->ios.signal_voltage == MMC_SIGNAL_VOLTAGE_330)
			host->mmc->ios.signal_voltage = MMC_SIGNAL_VOLTAGE_180;
		sprd_sdhci_host_wake_lock(&sprd_host->wake_lock);
		sdhci_runtime_resume_host(host);
		sprd_sdhci_host_wake_unlock(&sprd_host->wake_lock);
	}
	return 0;
}

static int sprd_sdhci_host_runtime_idle(struct device *dev) {
	return 0;
}
#endif

static int sprd_sdhci_host_probe(struct platform_device *pdev)
{
	int retval;
	int irq;
	struct resource *res;
	struct sdhci_host *host;
	struct sprd_sdhci_host *sprd_host;
	struct sprd_sdhci_host_platdata *host_pdata;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(!res)
		return -ENOENT;
	host = sdhci_alloc_host(&pdev->dev, sizeof(struct sprd_sdhci_host));
	if (IS_ERR(host))
		return PTR_ERR(host);
#ifndef CONFIG_OF
	host_pdata = dev_get_platdata(&pdev->dev);
	if(!host_pdata) {
		retval = -EINVAL;
		goto ERROR_PLATDATA;
	}
#else
	host_pdata = kzalloc(sizeof(struct sprd_sdhci_host_platdata), GFP_KERNEL);
	if(!host_pdata) {
		retval = -ENOMEM;
		goto ERROR_ALLOC;
	}
#endif
	sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	sprd_host->platdata = host_pdata;
	sprd_host->host = host;
#ifdef CONFIG_OF
	sprd_host->of_id = of_match_device(sprd_sdhci_host_of_match, &pdev->dev);
	sprd_sdhci_host_of_parse(pdev, host);
#endif
	platform_set_drvdata(pdev, host);
	host->ops = (const struct sdhci_ops *)&sprd_host->sdhci_host_ops;
	host->mmc->class_dev.type = &sprd_sdhci_host_device_type;
	pdev->dev.dma_mask = &host->dma_mask;
	host->dma_mask = DMA_BIT_MASK(64);
	host->hw_name = "";
	host->irq = irq;
	host->ioaddr = (void __iomem *)res->start;
#ifdef CONFIG_OF
	host->mmc->pm_caps |= MMC_PM_IGNORE_PM_NOTIFY;
#else
	host->mmc->pm_caps |= MMC_PM_IGNORE_PM_NOTIFY | MMC_PM_KEEP_POWER;
#endif
	host->mmc->pm_flags |=  MMC_PM_IGNORE_PM_NOTIFY;
	host->mmc->caps |= host_pdata->caps;
	host->mmc->caps2 |= host_pdata->caps2;
#ifdef CONFIG_PM_RUNTIME
	if(!host_pdata->runtime)
		host->mmc->caps &= ~MMC_CAP_POWER_OFF_CARD;
#endif
	sprd_host->clk_enabled = false;
	sprd_host->sdhci_host_ops = sprd_sdhci_host_default_ops;
	spin_lock_init(&sprd_host->lock);
	wake_lock_init(&sprd_host->wake_lock, WAKE_LOCK_SUSPEND, kasprintf(GFP_KERNEL, "%s-wakelock", dev_name(&pdev->dev)));
	device_init_wakeup(&pdev->dev, 0);
	device_set_wakeup_enable(&pdev->dev, 0);
	retval = sprd_sdhci_host_get_clock(pdev, host);
	if(retval < 0)
		goto ERROR_CLOCK;
	pm_runtime_get_noresume(&pdev->dev);
#ifdef CONFIG_PM_RUNTIME
	retval = sprd_sdhci_host_get_runtime(pdev, host);
	if (retval < 0)
		goto ERROR_RUNTIME;
#endif
	sprd_sdhci_host_open(host, sprd_host);
	sprd_sdhci_host_fix_mmc_core_pre(host);
	sprd_sdhci_host_fix_sprd_host_pre(host);
	retval = sdhci_add_host(host);
	if (retval)
		goto ERROR_ADD_HOST;
	smp_rmb();
	flush_delayed_work(&host->mmc->detect);
	sprd_sdhci_host_fix_mmc_core_post(host);
	sprd_sdhci_host_fix_sprd_host_post(host);
	pm_runtime_put_noidle(&pdev->dev);
	mmc_detect_change(host->mmc, 0);
	return 0;
ERROR_ADD_HOST:
	sprd_sdhci_host_fix_mmc_core_remove(host);
	sprd_sdhci_host_close(host, sprd_host);
#ifdef CONFIG_PM_RUNTIME
ERROR_RUNTIME:
	sprd_sdhci_host_put_runtime(pdev, host);
	pm_runtime_put_noidle(&pdev->dev);
#endif
ERROR_CLOCK:
	sprd_sdhci_host_put_clock(pdev, host);
#ifdef CONFIG_OF
	kfree(host_pdata);
ERROR_ALLOC:
#else
ERROR_PLATDATA:
#endif
	sdhci_free_host(host);
	return retval;
}

static int sprd_sdhci_host_remove(struct platform_device *pdev) {
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	if(pm_runtime_suspended(&pdev->dev))
		pm_runtime_get_sync(&pdev->dev);
	if(host->mmc->slot.cd_irq > 0)
		mmc_gpio_free_cd(mmc);
	mmc_claim_host(mmc);
	sprd_sdhci_host_put_runtime(pdev, host);
	sprd_sdhci_host_put_clock(pdev, host);
	sprd_sdhci_host_fix_mmc_core_remove(host);
	mmc_release_host(mmc);
	sdhci_remove_host(host, 1);
	sprd_sdhci_host_close(host, sprd_host);
	wake_lock_destroy(&sprd_host->wake_lock);
#ifdef CONFIG_OF
	if(sprd_host->platdata)
		kfree(sprd_host->platdata);
#endif
	sdhci_free_host(host);
	return 0;
}

static void sprd_sdhci_host_shutdown(struct platform_device *pdev)
{
#if 0
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct mmc_host *mmc = host->mmc;
	struct sprd_sdhci_host *sprd_host = SDHCI_HOST_TO_SPRD_HOST(host);
	if(pm_runtime_suspended(&pdev->dev))
		pm_runtime_get_sync(&pdev->dev);
	if(host->mmc->slot.cd_irq > 0)
		mmc_gpio_free_cd(mmc);
	//tasklet_kill_immediate();
	if (cancel_delayed_work_sync(&mmc->detect))
		sprd_sdhci_host_wake_unlock(&mmc->detect_wake_lock);
	mmc_claim_host(mmc);
	sprd_sdhci_host_put_runtime(pdev, host);
	sprd_sdhci_host_put_clock(pdev, host);
	sprd_sdhci_host_fix_mmc_core_remove(host);
	mmc_release_host(mmc);
	sdhci_remove_host(host, 1);
	sprd_sdhci_host_close(host, sprd_host);
	wake_lock_destroy(&sprd_host->wake_lock);
#ifdef CONFIG_OF
	if(sprd_host->platdata)
		kfree(sprd_host->platdata);
#endif
	sdhci_free_host(host);
#endif
}

static const struct dev_pm_ops sprd_sdhci_host_dev_pm_ops = {
	.suspend	 = sprd_sdhci_host_pm_suspend,
	.resume   = sprd_sdhci_host_pm_resume,
	SET_RUNTIME_PM_OPS(sprd_sdhci_host_runtime_suspend, sprd_sdhci_host_runtime_resume, sprd_sdhci_host_runtime_idle)
};

#define SPRD_SDHCI_HOST_DEV_PM_OPS       (&sprd_sdhci_host_dev_pm_ops)
#else
#define SPRD_SDHCI_HOST_DEV_PM_OPS       NULL
#endif

static const struct platform_device_id sprd_sdhci_host_driver_ids[] = {
	{"sprd-sdhci-shark", (kernel_ulong_t)sprd_sdhci_host_sprd_host_fixes_shark},
	{"sprd-sdhci-dolphin", (kernel_ulong_t)sprd_sdhci_host_sprd_host_fixes_dolphin},
	{},
};
MODULE_DEVICE_TABLE(platform, sprd_sdhci_host_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id sprd_sdhci_host_of_match[] = {
	{ .compatible = "sprd,sdhci-shark", .data = (const void *)sprd_sdhci_host_sprd_host_fixes_shark },
	{ .compatible = "sprd,sdhci-dolphin", .data = (const void *)sprd_sdhci_host_sprd_host_fixes_dolphin },
	{}
};
MODULE_DEVICE_TABLE(of, sprd_sdhci_host_of_match);
#endif

static struct platform_driver sprd_sdhci_host_driver = {
	.probe		= sprd_sdhci_host_probe,
	.remove		= sprd_sdhci_host_remove,
	.shutdown	= sprd_sdhci_host_shutdown,
	.id_table          = sprd_sdhci_host_driver_ids,
	.driver		= {
		.owner	= THIS_MODULE,
		.pm 	=  SPRD_SDHCI_HOST_DEV_PM_OPS,
		.name	= DRIVER_NAME,
		.of_match_table = of_match_ptr(sprd_sdhci_host_of_match),
	},
};

static int __init sprd_sdhci_host_init(void)
{
	return platform_driver_register(&sprd_sdhci_host_driver);
}

static void __exit sprd_sdhci_host_exit(void)
{
	platform_driver_unregister(&sprd_sdhci_host_driver);
}

module_init(sprd_sdhci_host_init);
module_exit(sprd_sdhci_host_exit);

MODULE_DESCRIPTION("Spredtrum SDHCI glue");
MODULE_AUTHOR("spreadtrum.com");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sprd-sdhci-host");
