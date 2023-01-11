/*
 *  i2c_adap_pxa.c
 *
 *  I2C adapter for the PXA I2C bus access.
 *
 *  Copyright (C) 2002 Intrinsyc Software Inc.
 *  Copyright (C) 2004-2005 Deep Blue Solutions Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  History:
 *    Apr 2002: Initial version [CS]
 *    Jun 2002: Properly separated algo/adap [FB]
 *    Jan 2003: Fixed several bugs concerning interrupt handling [Kai-Uwe Bloem]
 *    Jan 2003: added limited signal handling [Kai-Uwe Bloem]
 *    Sep 2004: Major rework to ensure efficient bus handling [RMK]
 *    Dec 2004: Added support for PXA27x and slave device probing [Liam Girdwood]
 *    Feb 2005: Rework slave mode handling [RMK]
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/i2c-pxa.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/i2c/pxa-i2c.h>
#include <linux/pm_qos.h>
#include <linux/platform_data/mmp-hwlock.h>
#include <linux/of_gpio.h>

#include <asm/irq.h>

struct pxa_reg_layout {
	u32 ibmr;
	u32 idbr;
	u32 icr;
	u32 isr;
	u32 isar;
	u32 ilcr;
	u32 iwcr;
	u32 icyc;
};

enum pxa_i2c_types {
	REGS_PXA2XX,
	REGS_PXA3XX,
	REGS_CE4100,
	REGS_PXA910,
};

/*
 * I2C registers definitions
 */
static struct pxa_reg_layout pxa_reg_layout[] = {
	[REGS_PXA2XX] = {
		.ibmr =	0x00,
		.idbr =	0x08,
		.icr =	0x10,
		.isr =	0x18,
		.isar =	0x20,
	},
	[REGS_PXA3XX] = {
		.ibmr =	0x00,
		.idbr =	0x04,
		.icr =	0x08,
		.isr =	0x0c,
		.isar =	0x10,
	},
	[REGS_CE4100] = {
		.ibmr =	0x14,
		.idbr =	0x0c,
		.icr =	0x00,
		.isr =	0x04,
		/* no isar register */
	},
	[REGS_PXA910] = {
		.ibmr = 0x00,
		.idbr = 0x08,
		.icr =	0x10,
		.isr =	0x18,
		.isar = 0x20,
		/* ilcr and iwcr are used to adjust clock rate */
		.ilcr = 0x28,
		.iwcr = 0x30,
		.icyc = 0x60,
	},
};

static const struct platform_device_id i2c_pxa_id_table[] = {
	{ "pxa2xx-i2c",		REGS_PXA2XX },
	{ "pxa3xx-pwri2c",	REGS_PXA3XX },
	{ "ce4100-i2c",		REGS_CE4100 },
	{ "pxa910-i2c",		REGS_PXA910 },
	{ },
};
MODULE_DEVICE_TABLE(platform, i2c_pxa_id_table);

/*
 * I2C bit definitions
 */

#define ICR_START	(1 << 0)	   /* start bit */
#define ICR_STOP	(1 << 1)	   /* stop bit */
#define ICR_ACKNAK	(1 << 2)	   /* send ACK(0) or NAK(1) */
#define ICR_TB		(1 << 3)	   /* transfer byte bit */
#define ICR_MA		(1 << 4)	   /* master abort */
#define ICR_SCLE	(1 << 5)	   /* master clock enable */
#define ICR_IUE		(1 << 6)	   /* unit enable */
#define ICR_GCD		(1 << 7)	   /* general call disable */
#define ICR_ITEIE	(1 << 8)	   /* enable tx interrupts */
#define ICR_IRFIE	(1 << 9)	   /* enable rx interrupts */
#define ICR_BEIE	(1 << 10)	   /* enable bus error ints */
#define ICR_SSDIE	(1 << 11)	   /* slave STOP detected int enable */
#define ICR_ALDIE	(1 << 12)	   /* enable arbitration interrupt */
#define ICR_SADIE	(1 << 13)	   /* slave address detected int enable */
#define ICR_UR		(1 << 14)	   /* unit reset */
#define ICR_FM		(1 << 15)	   /* fast mode */
#define ICR_HS		(1 << 16)	   /* High Speed mode */
#define ICR_GPIOEN	(1 << 19)	   /* enable GPIO mode for SCL in HS */
#define ICR_BUSRSTEN	(1 << 28)	   /* enable bus reset */

#define ISR_RWM		(1 << 0)	   /* read/write mode */
#define ISR_ACKNAK	(1 << 1)	   /* ack/nak status */
#define ISR_UB		(1 << 2)	   /* unit busy */
#define ISR_IBB		(1 << 3)	   /* bus busy */
#define ISR_SSD		(1 << 4)	   /* slave stop detected */
#define ISR_ALD		(1 << 5)	   /* arbitration loss detected */
#define ISR_ITE		(1 << 6)	   /* tx buffer empty */
#define ISR_IRF		(1 << 7)	   /* rx buffer full */
#define ISR_GCAD	(1 << 8)	   /* general call address detected */
#define ISR_SAD		(1 << 9)	   /* slave address detected */
#define ISR_BED		(1 << 10)	   /* bus error no ACK/NAK */

struct pxa_i2c {
	spinlock_t		lock;
	wait_queue_head_t	wait;
	struct i2c_msg		*msg;
	unsigned int		msg_num;
	unsigned int		msg_idx;
	unsigned int		msg_ptr;
	unsigned int		slave_addr;
	unsigned int		req_slave_addr;

	struct i2c_adapter	adap;
	struct clk		*clk;
#ifdef CONFIG_I2C_PXA_SLAVE
	struct i2c_slave_client *slave;
#endif

	unsigned int		irqlogidx;
	u32			isrlog[32];
	u32			icrlog[32];

	void __iomem		*reg_base;
	void __iomem		*reg_ibmr;
	void __iomem		*reg_idbr;
	void __iomem		*reg_icr;
	void __iomem		*reg_isr;
	void __iomem		*reg_isar;
	void __iomem		*reg_ilcr;
	void __iomem		*reg_iwcr;
	void __iomem		*reg_icyc;

	resource_size_t		iobase;
	resource_size_t		iosize;

	int			irq;
	unsigned int		use_pio :1;
	unsigned int		fast_mode :1;
	unsigned int		high_mode:1;
	unsigned int		always_on:1;
	unsigned int		enable_bus_rst:1;
	unsigned int		gpio_bus_rst:1;
	/* pins configration may be different between AP and CP */
	unsigned int		apdcp:1;

	unsigned char		master_code;
	unsigned long		rate;
	bool			highmode_enter;
	unsigned int		ilcr;
	unsigned int		iwcr;
	void __iomem		*hwlock_addr;
	struct pm_qos_request	qos_idle;
	s32			pm_qos;
	u32			icr_save;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pin_i2c;
	struct pinctrl_state	*pin_gpio;
	struct pinctrl_state	*pin_i2c_cp;
};

#define _IBMR(i2c)	((i2c)->reg_ibmr)
#define _IDBR(i2c)	((i2c)->reg_idbr)
#define _ICR(i2c)	((i2c)->reg_icr)
#define _ISR(i2c)	((i2c)->reg_isr)
#define _ISAR(i2c)	((i2c)->reg_isar)
#define _ILCR(i2c)	((i2c)->reg_ilcr)
#define _IWCR(i2c)	((i2c)->reg_iwcr)
#define _ICYC(i2c)	((i2c)->reg_icyc)

/*
 * I2C Slave mode address
 */
#define I2C_PXA_SLAVE_ADDR      0x1

#define DEBUG 0
static void i2c_pxa_reset(struct pxa_i2c *i2c);

#ifdef DEBUG

struct bits {
	u32	mask;
	const char *set;
	const char *unset;
};
#define PXA_BIT(m, s, u)	{ .mask = m, .set = s, .unset = u }

static inline void
decode_bits(const char *prefix, const struct bits *bits, int num, u32 val)
{
	printk("%s %08x: ", prefix, val);
	while (num--) {
		const char *str = val & bits->mask ? bits->set : bits->unset;
		if (str)
			printk("%s ", str);
		bits++;
	}
}

static const struct bits isr_bits[] = {
	PXA_BIT(ISR_RWM,	"RX",		"TX"),
	PXA_BIT(ISR_ACKNAK,	"NAK",		"ACK"),
	PXA_BIT(ISR_UB,		"Bsy",		"Rdy"),
	PXA_BIT(ISR_IBB,	"BusBsy",	"BusRdy"),
	PXA_BIT(ISR_SSD,	"SlaveStop",	NULL),
	PXA_BIT(ISR_ALD,	"ALD",		NULL),
	PXA_BIT(ISR_ITE,	"TxEmpty",	NULL),
	PXA_BIT(ISR_IRF,	"RxFull",	NULL),
	PXA_BIT(ISR_GCAD,	"GenCall",	NULL),
	PXA_BIT(ISR_SAD,	"SlaveAddr",	NULL),
	PXA_BIT(ISR_BED,	"BusErr",	NULL),
};

static void decode_ISR(unsigned int val)
{
	decode_bits(KERN_DEBUG "ISR", isr_bits, ARRAY_SIZE(isr_bits), val);
	printk("\n");
}

static const struct bits icr_bits[] = {
	PXA_BIT(ICR_START,  "START",	NULL),
	PXA_BIT(ICR_STOP,   "STOP",	NULL),
	PXA_BIT(ICR_ACKNAK, "ACKNAK",	NULL),
	PXA_BIT(ICR_TB,     "TB",	NULL),
	PXA_BIT(ICR_MA,     "MA",	NULL),
	PXA_BIT(ICR_SCLE,   "SCLE",	"scle"),
	PXA_BIT(ICR_IUE,    "IUE",	"iue"),
	PXA_BIT(ICR_GCD,    "GCD",	NULL),
	PXA_BIT(ICR_ITEIE,  "ITEIE",	NULL),
	PXA_BIT(ICR_IRFIE,  "IRFIE",	NULL),
	PXA_BIT(ICR_BEIE,   "BEIE",	NULL),
	PXA_BIT(ICR_SSDIE,  "SSDIE",	NULL),
	PXA_BIT(ICR_ALDIE,  "ALDIE",	NULL),
	PXA_BIT(ICR_SADIE,  "SADIE",	NULL),
	PXA_BIT(ICR_UR,     "UR",		"ur"),
};

#ifdef CONFIG_I2C_PXA_SLAVE
static void decode_ICR(unsigned int val)
{
	decode_bits(KERN_DEBUG "ICR", icr_bits, ARRAY_SIZE(icr_bits), val);
	printk("\n");
}
#endif

static unsigned int i2c_debug = DEBUG;

static void i2c_pxa_show_state(struct pxa_i2c *i2c, int lno, const char *fname)
{
	dev_dbg(&i2c->adap.dev, "state:%s:%d: ISR=%08x, ICR=%08x, IBMR=%02x\n", fname, lno,
		readl(_ISR(i2c)), readl(_ICR(i2c)), readl(_IBMR(i2c)));
}

#define show_state(i2c) i2c_pxa_show_state(i2c, __LINE__, __func__)

static void i2c_bus_reset(struct pxa_i2c *i2c)
{
	int ret, ccnt, pins_scl, pins_sda;
	struct device *dev = i2c->adap.dev.parent;
	struct device_node *np = dev->of_node;

	/* cannot do bus reset */
	if (!i2c->enable_bus_rst)
		return;
	/* hardware bus reset, no need to toggle gpio */
	if (!i2c->gpio_bus_rst) {
		/* 9 cycles */
		writel(readl(_ICYC(i2c)) | 0x9, _ICYC(i2c));
		writel(readl(_ICR(i2c)) | ICR_BUSRSTEN, _ICR(i2c));
		/*
		 * wait until the control bit is self-cleard
		 * suppose we use 100kbps, 9 clocks need about 100us
		 */
		udelay(100);
		WARN_ON(readl(_ICR(i2c)) & ICR_BUSRSTEN);
		return;
	}

	if (!i2c->pinctrl) {
		dev_info(dev, "no need to do i2c bus reset\n");
		return;
	}

	ret = pinctrl_select_state(i2c->pinctrl, i2c->pin_gpio);
	if (ret) {
		dev_err(dev, "cannot set gpio pins\n");
		return;
	}

	pins_scl = of_get_named_gpio(np, "i2c-gpio", 0);
	if (!gpio_is_valid(pins_scl)) {
		dev_err(dev, "get scl gpio fail\n");
		goto err_out;
	}
	pins_sda = of_get_named_gpio(np, "i2c-gpio", 1);
	if (!gpio_is_valid(pins_sda) < 0) {
		dev_err(dev, "get sda gpio fail\n");
		goto err_out;
	}

	gpio_request(pins_scl, NULL);
	gpio_request(pins_sda, NULL);

	gpio_direction_input(pins_sda);
	for (ccnt = 20; ccnt; ccnt--) {
		gpio_direction_output(pins_scl, ccnt & 0x01);
		udelay(5);
	}
	gpio_direction_output(pins_scl, 0);
	udelay(5);
	gpio_direction_output(pins_sda, 0);
	udelay(5);
	/* stop signal */
	gpio_direction_output(pins_scl, 1);
	udelay(5);
	gpio_direction_output(pins_sda, 1);
	udelay(5);

	gpio_free(pins_scl);
	gpio_free(pins_sda);

err_out:
	ret = pinctrl_select_state(i2c->pinctrl, i2c->pin_i2c);
	if (ret)
		dev_err(dev, "cannot set default(i2c) pins\n");
	return;
}

static void i2c_pxa_scream_blue_murder(struct pxa_i2c *i2c, const char *why)
{
	unsigned int i;
	printk(KERN_ERR"i2c: <%s> slave_0x%x error: %s\n", i2c->adap.name,
		i2c->req_slave_addr >> 1, why);
	printk(KERN_ERR "i2c: msg_num: %d msg_idx: %d msg_ptr: %d\n",
		i2c->msg_num, i2c->msg_idx, i2c->msg_ptr);
	printk(KERN_ERR "i2c: IBMR: %08x IDBR: %08x ICR: %08x ISR: %08x\n",
		readl(_IBMR(i2c)), readl(_IDBR(i2c)), readl(_ICR(i2c)),
		readl(_ISR(i2c)));
	printk(KERN_DEBUG "i2c: log: ");
	for (i = 0; i < i2c->irqlogidx; i++)
		printk("[%08x:%08x] ", i2c->isrlog[i], i2c->icrlog[i]);
	printk("\n");
	if (strcmp(why, "exhausted retries")) {
		i2c_bus_reset(i2c);
		/* reset i2c contorler */
		i2c_pxa_reset(i2c);
	}
}

#else /* ifdef DEBUG */

#define i2c_debug	0

#define show_state(i2c) do { } while (0)
#define decode_ISR(val) do { } while (0)
#define decode_ICR(val) do { } while (0)
#define i2c_pxa_scream_blue_murder(i2c, why) do { } while (0)

#endif /* ifdef DEBUG / else */

static void i2c_pxa_master_complete(struct pxa_i2c *i2c, int ret);
static irqreturn_t i2c_pxa_handler(int this_irq, void *dev_id);

static int ripc_status;

int i2c_pxa_set_pinstate(struct i2c_adapter *adap, char *name)
{
	struct pinctrl_state *s;
	struct pxa_i2c *i2c;
	int ret;

	if (!adap) {
		dev_err(&adap->dev, "adapter is NULL!\n");
		return -1;
	} else
		i2c = adap->algo_data;

	if (!i2c || !i2c->pinctrl) {
		dev_err(&adap->dev, "i2c pinctrl is NULL\n");
		return -1;

		s = pinctrl_lookup_state(i2c->pinctrl, name);
		if (IS_ERR(s)) {
			dev_err(&adap->dev, "cannot get %s pinstate\n", name);
			ret = IS_ERR(s);
			return ret;
		}
		ret =  pinctrl_select_state(i2c->pinctrl, s);
		if (ret < 0) {
			dev_err(&adap->dev, "cannot set %s pinstate\n", name);
			return ret;
		}
	}
	return 0;
}

void mmp_hwlock_lock(struct i2c_adapter *adap)
{
	int cnt = 0;
	unsigned long flags;

	struct pxa_i2c *i2c = adap->algo_data;

	spin_lock_irqsave(&lock_for_ripc, flags);
	while(__raw_readl(i2c->hwlock_addr)) {
		ripc_status = false;
		spin_unlock_irqrestore(&lock_for_ripc, flags);
		cpu_relax();
		udelay(50);
		cnt++;
		if (cnt >= 10000) {
			pr_warn("AP: fail to lock ripc!\n");
			cnt = 0;
		}
		spin_lock_irqsave(&lock_for_ripc, flags);
	}
	/* sure to hold ripc */
	ripc_status = true;

	spin_unlock_irqrestore(&lock_for_ripc, flags);

	/*
	 * devm_pinctrl_get() cannot be used with irq disabled,
	 * for it may sleep when allocing memory
	 */
	if (i2c->apdcp && pinctrl_select_state(i2c->pinctrl, i2c->pin_i2c) < 0)
		dev_err(&i2c->adap.dev, "cannot set i2c ap pins\n");
}

void mmp_hwlock_unlock(struct i2c_adapter *adap)
{
	unsigned long flags;

	struct pxa_i2c *i2c = adap->algo_data;

	if (i2c->apdcp && pinctrl_select_state(i2c->pinctrl, i2c->pin_i2c_cp) < 0)
		dev_err(&i2c->adap.dev, "cannot set i2c cp pins\n");

	spin_lock_irqsave(&lock_for_ripc, flags);

	__raw_writel(1, i2c->hwlock_addr);
	ripc_status = false;

	spin_unlock_irqrestore(&lock_for_ripc, flags);
}

int mmp_hwlock_trylock(struct i2c_adapter *adap)
{
	unsigned long flags;

	struct pxa_i2c *i2c = adap->algo_data;

	spin_lock_irqsave(&lock_for_ripc, flags);
	ripc_status = !__raw_readl(i2c->hwlock_addr);
	spin_unlock_irqrestore(&lock_for_ripc, flags);

	if (ripc_status && i2c->apdcp &&
	    (pinctrl_select_state(i2c->pinctrl, i2c->pin_i2c) < 0))
		dev_err(&i2c->adap.dev, "could not set i2c ap pins\n");

	return ripc_status;
}

/* for telephony: must held by spinlock */
bool mmp_hwlock_get_status(void)
{
	return ripc_status;
}
EXPORT_SYMBOL(mmp_hwlock_get_status);

/* enable/disable i2c unit */
static inline void i2c_pxa_enable(struct pxa_i2c *i2c, bool enable)
{
	if (enable)
		writel(readl(_ICR(i2c)) | ICR_IUE, _ICR(i2c));
	else
		writel(readl(_ICR(i2c)) & ~ICR_IUE, _ICR(i2c));
	udelay(100);
	i2c->icr_save = readl(_ICR(i2c));
}

static inline int i2c_pxa_is_slavemode(struct pxa_i2c *i2c)
{
	return !(readl(_ICR(i2c)) & ICR_SCLE);
}

static void i2c_pxa_abort(struct pxa_i2c *i2c)
{
	int i = 250;

	if (i2c_pxa_is_slavemode(i2c)) {
		dev_dbg(&i2c->adap.dev, "%s: called in slave mode\n", __func__);
		return;
	}

	while ((i > 0) && (readl(_IBMR(i2c)) & 0x1) == 0) {
		unsigned int icr = readl(_ICR(i2c));

		icr &= ~ICR_START;
		icr |= ICR_ACKNAK | ICR_STOP | ICR_TB;

		writel(icr, _ICR(i2c));

		show_state(i2c);

		mdelay(1);
		i --;
	}

	writel(readl(_ICR(i2c)) & ~(ICR_MA | ICR_START | ICR_STOP),
	       _ICR(i2c));
}

static int i2c_pxa_wait_bus_not_busy(struct pxa_i2c *i2c)
{
	int timeout = DEF_TIMEOUT;

	if (readl(_ISR(i2c)) & (ISR_IBB | ISR_UB)) {
		i2c_pxa_reset(i2c);
		timeout /= 2;
	}

	while (timeout-- && readl(_ISR(i2c)) & (ISR_IBB | ISR_UB)) {
		if ((readl(_ISR(i2c)) & ISR_SAD) != 0)
			timeout += 4;

		msleep(2);
		show_state(i2c);
	}

	if (timeout < 0)
		show_state(i2c);

	return timeout < 0 ? I2C_RETRY : 0;
}

static int i2c_pxa_wait_master(struct pxa_i2c *i2c)
{
	unsigned long timeout = jiffies + HZ*4;

	while (time_before(jiffies, timeout)) {
		if (i2c_debug > 1)
			dev_dbg(&i2c->adap.dev, "%s: %ld: ISR=%08x, ICR=%08x, IBMR=%02x\n",
				__func__, (long)jiffies, readl(_ISR(i2c)), readl(_ICR(i2c)), readl(_IBMR(i2c)));

		if (readl(_ISR(i2c)) & ISR_SAD) {
			if (i2c_debug > 0)
				dev_dbg(&i2c->adap.dev, "%s: Slave detected\n", __func__);
			goto out;
		}

		/* wait for unit and bus being not busy, and we also do a
		 * quick check of the i2c lines themselves to ensure they've
		 * gone high...
		 */
		if ((readl(_ISR(i2c)) & (ISR_UB | ISR_IBB)) == 0 && readl(_IBMR(i2c)) == 3) {
			if (i2c_debug > 0)
				dev_dbg(&i2c->adap.dev, "%s: done\n", __func__);
			return 1;
		}

		msleep(1);
	}

	if (i2c_debug > 0)
		dev_dbg(&i2c->adap.dev, "%s: did not free\n", __func__);
 out:
	return 0;
}

static int i2c_pxa_set_master(struct pxa_i2c *i2c)
{
	if (i2c_debug)
		dev_dbg(&i2c->adap.dev, "setting to bus master\n");

	if ((readl(_ISR(i2c)) & (ISR_UB | ISR_IBB)) != 0) {
		dev_dbg(&i2c->adap.dev, "%s: unit is busy\n", __func__);
		if (!i2c_pxa_wait_master(i2c)) {
			dev_dbg(&i2c->adap.dev, "%s: error: unit busy\n", __func__);
			return I2C_RETRY;
		}
	}

	writel(readl(_ICR(i2c)) | ICR_SCLE, _ICR(i2c));
	return 0;
}

#ifdef CONFIG_I2C_PXA_SLAVE
static int i2c_pxa_wait_slave(struct pxa_i2c *i2c)
{
	unsigned long timeout = jiffies + HZ*1;

	/* wait for stop */

	show_state(i2c);

	while (time_before(jiffies, timeout)) {
		if (i2c_debug > 1)
			dev_dbg(&i2c->adap.dev, "%s: %ld: ISR=%08x, ICR=%08x, IBMR=%02x\n",
				__func__, (long)jiffies, readl(_ISR(i2c)), readl(_ICR(i2c)), readl(_IBMR(i2c)));

		if ((readl(_ISR(i2c)) & (ISR_UB|ISR_IBB)) == 0 ||
		    (readl(_ISR(i2c)) & ISR_SAD) != 0 ||
		    (readl(_ICR(i2c)) & ICR_SCLE) == 0) {
			if (i2c_debug > 1)
				dev_dbg(&i2c->adap.dev, "%s: done\n", __func__);
			return 1;
		}

		msleep(1);
	}

	if (i2c_debug > 0)
		dev_dbg(&i2c->adap.dev, "%s: did not free\n", __func__);
	return 0;
}

/*
 * clear the hold on the bus, and take of anything else
 * that has been configured
 */
static void i2c_pxa_set_slave(struct pxa_i2c *i2c, int errcode)
{
	show_state(i2c);

	if (errcode < 0) {
		udelay(100);   /* simple delay */
	} else {
		/* we need to wait for the stop condition to end */

		/* if we where in stop, then clear... */
		if (readl(_ICR(i2c)) & ICR_STOP) {
			udelay(100);
			writel(readl(_ICR(i2c)) & ~ICR_STOP, _ICR(i2c));
		}

		if (!i2c_pxa_wait_slave(i2c)) {
			dev_err(&i2c->adap.dev, "%s: wait timedout\n",
				__func__);
			return;
		}
	}

	writel(readl(_ICR(i2c)) & ~(ICR_STOP|ICR_ACKNAK|ICR_MA), _ICR(i2c));
	writel(readl(_ICR(i2c)) & ~ICR_SCLE, _ICR(i2c));

	if (i2c_debug) {
		dev_dbg(&i2c->adap.dev, "ICR now %08x, ISR %08x\n", readl(_ICR(i2c)), readl(_ISR(i2c)));
		decode_ICR(readl(_ICR(i2c)));
	}
}
#else
#define i2c_pxa_set_slave(i2c, err)	do { } while (0)
#endif

static void i2c_pxa_reset(struct pxa_i2c *i2c)
{
	pr_debug("Resetting I2C Controller Unit\n");

	/* abort any transfer currently under way */
	i2c_pxa_abort(i2c);

	/* reset according to 9.8 */
	writel(ICR_UR, _ICR(i2c));
	writel(I2C_ISR_INIT, _ISR(i2c));
	writel(readl(_ICR(i2c)) & ~ICR_UR, _ICR(i2c));

#ifdef CONFIG_I2C_PXA_SLAVE
	if (i2c->reg_isar)
		writel(i2c->slave_addr, _ISAR(i2c));
#endif

	/* set control register values */
	writel(I2C_ICR_INIT | (i2c->fast_mode ? ICR_FM : 0), _ICR(i2c));
	writel(readl(_ICR(i2c)) | (i2c->high_mode ? ICR_HS : 0), _ICR(i2c));

	if (i2c->ilcr)
		writel(i2c->ilcr, _ILCR(i2c));
	if (i2c->iwcr)
		writel(i2c->iwcr, _IWCR(i2c));
	udelay(2);

#ifdef CONFIG_I2C_PXA_SLAVE
	dev_info(&i2c->adap.dev, "Enabling slave mode\n");
	writel(readl(_ICR(i2c)) | ICR_SADIE | ICR_ALDIE | ICR_SSDIE, _ICR(i2c));
#endif

	i2c_pxa_set_slave(i2c, 0);

	/* enable unit */
	i2c_pxa_enable(i2c, true);
}


#ifdef CONFIG_I2C_PXA_SLAVE
/*
 * PXA I2C Slave mode
 */

static void i2c_pxa_slave_txempty(struct pxa_i2c *i2c, u32 isr)
{
	if (isr & ISR_BED) {
		/* what should we do here? */
	} else {
		int ret = 0;

		if (i2c->slave != NULL)
			ret = i2c->slave->read(i2c->slave->data);

		writel(ret, _IDBR(i2c));
		writel(readl(_ICR(i2c)) | ICR_TB, _ICR(i2c));   /* allow next byte */
	}
}

static void i2c_pxa_slave_rxfull(struct pxa_i2c *i2c, u32 isr)
{
	unsigned int byte = readl(_IDBR(i2c));

	if (i2c->slave != NULL)
		i2c->slave->write(i2c->slave->data, byte);

	writel(readl(_ICR(i2c)) | ICR_TB, _ICR(i2c));
}

static void i2c_pxa_slave_start(struct pxa_i2c *i2c, u32 isr)
{
	int timeout;

	if (i2c_debug > 0)
		dev_dbg(&i2c->adap.dev, "SAD, mode is slave-%cx\n",
		       (isr & ISR_RWM) ? 'r' : 't');

	if (i2c->slave != NULL)
		i2c->slave->event(i2c->slave->data,
				 (isr & ISR_RWM) ? I2C_SLAVE_EVENT_START_READ : I2C_SLAVE_EVENT_START_WRITE);

	/*
	 * slave could interrupt in the middle of us generating a
	 * start condition... if this happens, we'd better back off
	 * and stop holding the poor thing up
	 */
	writel(readl(_ICR(i2c)) & ~(ICR_START|ICR_STOP), _ICR(i2c));
	writel(readl(_ICR(i2c)) | ICR_TB, _ICR(i2c));

	timeout = 0x10000;

	while (1) {
		if ((readl(_IBMR(i2c)) & 2) == 2)
			break;

		timeout--;

		if (timeout <= 0) {
			dev_err(&i2c->adap.dev, "timeout waiting for SCL high\n");
			break;
		}
	}

	writel(readl(_ICR(i2c)) & ~ICR_SCLE, _ICR(i2c));
}

static void i2c_pxa_slave_stop(struct pxa_i2c *i2c)
{
	if (i2c_debug > 2)
		dev_dbg(&i2c->adap.dev, "ISR: SSD (Slave Stop)\n");

	if (i2c->slave != NULL)
		i2c->slave->event(i2c->slave->data, I2C_SLAVE_EVENT_STOP);

	if (i2c_debug > 2)
		dev_dbg(&i2c->adap.dev, "ISR: SSD (Slave Stop) acked\n");

	/*
	 * If we have a master-mode message waiting,
	 * kick it off now that the slave has completed.
	 */
	if (i2c->msg)
		i2c_pxa_master_complete(i2c, I2C_RETRY);
}
#else
static void i2c_pxa_slave_txempty(struct pxa_i2c *i2c, u32 isr)
{
	if (isr & ISR_BED) {
		/* what should we do here? */
	} else {
		writel(0, _IDBR(i2c));
		writel(readl(_ICR(i2c)) | ICR_TB, _ICR(i2c));
	}
}

static void i2c_pxa_slave_rxfull(struct pxa_i2c *i2c, u32 isr)
{
	writel(readl(_ICR(i2c)) | ICR_TB | ICR_ACKNAK, _ICR(i2c));
}

static void i2c_pxa_slave_start(struct pxa_i2c *i2c, u32 isr)
{
	int timeout;

	/*
	 * slave could interrupt in the middle of us generating a
	 * start condition... if this happens, we'd better back off
	 * and stop holding the poor thing up
	 */
	writel(readl(_ICR(i2c)) & ~(ICR_START|ICR_STOP), _ICR(i2c));
	writel(readl(_ICR(i2c)) | ICR_TB | ICR_ACKNAK, _ICR(i2c));

	timeout = 0x10000;

	while (1) {
		if ((readl(_IBMR(i2c)) & 2) == 2)
			break;

		timeout--;

		if (timeout <= 0) {
			dev_err(&i2c->adap.dev, "timeout waiting for SCL high\n");
			break;
		}
	}

	writel(readl(_ICR(i2c)) & ~ICR_SCLE, _ICR(i2c));
}

static void i2c_pxa_slave_stop(struct pxa_i2c *i2c)
{
	if (i2c->msg)
		i2c_pxa_master_complete(i2c, I2C_RETRY);
}
#endif

/*
 * PXA I2C Master mode
 */

static inline unsigned int i2c_pxa_addr_byte(struct i2c_msg *msg)
{
	unsigned int addr = (msg->addr & 0x7f) << 1;

	if (msg->flags & I2C_M_RD)
		addr |= 1;

	return addr;
}

static inline void i2c_pxa_start_message(struct pxa_i2c *i2c)
{
	u32 icr;

	/*
	 * Step 1: target slave address into IDBR
	 */
	writel(i2c_pxa_addr_byte(i2c->msg), _IDBR(i2c));
	i2c->req_slave_addr = i2c_pxa_addr_byte(i2c->msg);

	/*
	 * Step 2: initiate the write.
	 */
	icr = readl(_ICR(i2c)) & ~(ICR_STOP | ICR_ALDIE);
	writel(icr | ICR_START | ICR_TB, _ICR(i2c));
}

static inline void i2c_pxa_stop_message(struct pxa_i2c *i2c)
{
	u32 icr;

	/*
	 * Clear the STOP and ACK flags
	 */
	icr = readl(_ICR(i2c));
	icr &= ~(ICR_STOP | ICR_ACKNAK);
	writel(icr, _ICR(i2c));
}

static int i2c_pxa_pio_set_master(struct pxa_i2c *i2c)
{
	/* make timeout the same as for interrupt based functions */
	long timeout = 2 * DEF_TIMEOUT;

	/*
	 * Wait for the bus to become free.
	 */
	while (timeout-- && readl(_ISR(i2c)) & (ISR_IBB | ISR_UB)) {
		udelay(1000);
		show_state(i2c);
	}

	if (timeout < 0) {
		show_state(i2c);
		dev_err(&i2c->adap.dev,
			"i2c_pxa: timeout waiting for bus free\n");
		return I2C_RETRY;
	}

	/*
	 * Set master mode.
	 */
	writel(readl(_ICR(i2c)) | ICR_SCLE, _ICR(i2c));

	return 0;
}

/*
 * PXA I2C send master code
 * 1. Load master code to IDBR and send it.
 *    Note for HS mode, set ICR [GPIOEN].
 * 2. Wait until win arbitration.
 */
static int i2c_pxa_send_mastercode(struct pxa_i2c *i2c)
{
	u32 icr;
	long timeout;

	spin_lock_irq(&i2c->lock);
	i2c->highmode_enter = true;
	writel(i2c->master_code, _IDBR(i2c));

	icr = readl(_ICR(i2c)) & ~(ICR_STOP | ICR_ALDIE);
	icr |= ICR_GPIOEN | ICR_START | ICR_TB | ICR_ITEIE;
	writel(icr, _ICR(i2c));

	spin_unlock_irq(&i2c->lock);
	timeout = wait_event_timeout(i2c->wait, !i2c->highmode_enter, HZ * 1);

	i2c->highmode_enter = false;

	return (timeout == 0) ? I2C_RETRY : 0;
}

static int i2c_pxa_do_pio_xfer(struct pxa_i2c *i2c,
			       struct i2c_msg *msg, int num)
{
	unsigned long timeout = 100000; /* 1 seconds */
	int ret = 0;

	ret = i2c_pxa_pio_set_master(i2c);
	if (ret)
		goto out;

	i2c->msg = msg;
	i2c->msg_num = num;
	i2c->msg_idx = 0;
	i2c->msg_ptr = 0;
	i2c->irqlogidx = 0;

	pm_qos_update_request(&i2c->qos_idle, i2c->pm_qos);

	i2c_pxa_start_message(i2c);

	while (i2c->msg_num > 0 && --timeout) {
		i2c_pxa_handler(0, i2c);
		udelay(10);
	}

	i2c_pxa_stop_message(i2c);
	pm_qos_update_request(&i2c->qos_idle,
			      PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

	/*
	 * We place the return code in i2c->msg_idx.
	 */
	ret = i2c->msg_idx;

out:
	if (timeout == 0) {
		i2c_pxa_scream_blue_murder(i2c, "timeout");
		ret = I2C_RETRY;
	}

	if (ret < 0)
		i2c_pxa_reset(i2c);

	return ret;
}

/*
 * We are protected by the adapter bus mutex.
 */
static int i2c_pxa_do_xfer(struct pxa_i2c *i2c, struct i2c_msg *msg, int num)
{
	long timeout;
	int ret;

	/*
	 * Wait for the bus to become free.
	 */
	ret = i2c_pxa_wait_bus_not_busy(i2c);
	if (ret) {
		dev_err(&i2c->adap.dev, "i2c_pxa: timeout waiting for bus free\n");
		goto out;
	}

	/*
	 * Set master mode.
	 */
	ret = i2c_pxa_set_master(i2c);
	if (ret) {
		dev_err(&i2c->adap.dev, "i2c_pxa_set_master: error %d\n", ret);
		goto out;
	}

	if (i2c->high_mode) {
		ret = i2c_pxa_send_mastercode(i2c);
		if (ret) {
			dev_err(&i2c->adap.dev, "i2c_pxa_send_mastercode timeout\n");
			goto out;
			}
	}

	pm_qos_update_request(&i2c->qos_idle, i2c->pm_qos);

	spin_lock_irq(&i2c->lock);

	i2c->msg = msg;
	i2c->msg_num = num;
	i2c->msg_idx = 0;
	i2c->msg_ptr = 0;
	i2c->irqlogidx = 0;

	i2c_pxa_start_message(i2c);

	spin_unlock_irq(&i2c->lock);

	/*
	 * The rest of the processing occurs in the interrupt handler.
	 */
	timeout = wait_event_timeout(i2c->wait, i2c->msg_num == 0, HZ * 1);
	i2c_pxa_stop_message(i2c);

	pm_qos_update_request(&i2c->qos_idle,
			      PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

	/*
	 * We place the return code in i2c->msg_idx.
	 */
	ret = i2c->msg_idx;

	if (!timeout && i2c->msg_num) {
		i2c_pxa_scream_blue_murder(i2c, "timeout");
		ret = I2C_RETRY;
	}

 out:
	if (ret < 0)
		i2c_pxa_reset(i2c);

	return ret;
}

static int i2c_pxa_pio_xfer(struct i2c_adapter *adap,
			    struct i2c_msg msgs[], int num)
{
	struct pxa_i2c *i2c = adap->algo_data;
	int ret, i;

	/* enable i2c unit before transmission */
	i2c_pxa_enable(i2c, true);

	/* If the I2C controller is disabled we need to reset it
	  (probably due to a suspend/resume destroying state). We do
	  this here as we can then avoid worrying about resuming the
	  controller before its users. */
	if (!(readl(_ICR(i2c)) & ICR_IUE))
		i2c_pxa_reset(i2c);

	for (i = adap->retries; i >= 0; i--) {
		ret = i2c_pxa_do_pio_xfer(i2c, msgs, num);
		if (ret != I2C_RETRY)
			goto out;

		if (i2c_debug)
			dev_dbg(&adap->dev, "Retrying transmission\n");
		udelay(100);
	}
	i2c_pxa_scream_blue_murder(i2c, "exhausted retries");
	ret = -EREMOTEIO;
 out:
	i2c_pxa_set_slave(i2c, ret);

	/* disable i2c unit after transmission */
	i2c_pxa_enable(i2c, false);

	return ret;
}

/*
 * i2c_pxa_master_complete - complete the message and wake up.
 */
static void i2c_pxa_master_complete(struct pxa_i2c *i2c, int ret)
{
	i2c->msg_ptr = 0;
	i2c->msg = NULL;
	i2c->msg_idx ++;
	i2c->msg_num = 0;
	if (ret)
		i2c->msg_idx = ret;
	if (!i2c->use_pio)
		wake_up(&i2c->wait);
}

static void i2c_pxa_irq_txempty(struct pxa_i2c *i2c, u32 isr)
{
	u32 icr = readl(_ICR(i2c)) & ~(ICR_START|ICR_STOP|ICR_ACKNAK|ICR_TB);

 again:
	/*
	 * If ISR_ALD is set, we lost arbitration.
	 */
	if (isr & ISR_ALD) {
		/*
		 * Do we need to do anything here?  The PXA docs
		 * are vague about what happens.
		 */
		i2c_pxa_scream_blue_murder(i2c, "ALD set");

		/*
		 * We ignore this error.  We seem to see spurious ALDs
		 * for seemingly no reason.  If we handle them as I think
		 * they should, we end up causing an I2C error, which
		 * is painful for some systems.
		 */
		return; /* ignore */
	}

	if (isr & ISR_BED) {
		int ret = BUS_ERROR;

		/*
		 * I2C bus error - either the device NAK'd us, or
		 * something more serious happened.  If we were NAK'd
		 * on the initial address phase, we can retry.
		 */
		if (isr & ISR_ACKNAK) {
			if (i2c->msg_ptr == 0 && i2c->msg_idx == 0)
				ret = I2C_RETRY;
			else
				ret = XFER_NAKED;
		}
		i2c_pxa_master_complete(i2c, ret);
	} else if (isr & ISR_RWM) {
		/*
		 * Read mode.  We have just sent the address byte, and
		 * now we must initiate the transfer.
		 */
		if (i2c->msg_ptr == i2c->msg->len - 1 &&
		    i2c->msg_idx == i2c->msg_num - 1)
			icr |= ICR_STOP | ICR_ACKNAK;

		icr |= ICR_ALDIE | ICR_TB;
	} else if (i2c->msg_ptr < i2c->msg->len) {
		/*
		 * Write mode.  Write the next data byte.
		 */
		writel(i2c->msg->buf[i2c->msg_ptr++], _IDBR(i2c));

		icr |= ICR_ALDIE | ICR_TB;

		/*
		 * If this is the last byte of the last message, send
		 * a STOP.
		 */
		if (i2c->msg_ptr == i2c->msg->len &&
		    i2c->msg_idx == i2c->msg_num - 1)
			icr |= ICR_STOP;
	} else if (i2c->msg_idx < i2c->msg_num - 1) {
		/*
		 * Next segment of the message.
		 */
		i2c->msg_ptr = 0;
		i2c->msg_idx ++;
		i2c->msg++;

		/*
		 * If we aren't doing a repeated start and address,
		 * go back and try to send the next byte.  Note that
		 * we do not support switching the R/W direction here.
		 */
		if (i2c->msg->flags & I2C_M_NOSTART)
			goto again;

		/*
		 * Write the next address.
		 */
		writel(i2c_pxa_addr_byte(i2c->msg), _IDBR(i2c));
		i2c->req_slave_addr = i2c_pxa_addr_byte(i2c->msg);

		/*
		 * And trigger a repeated start, and send the byte.
		 */
		icr &= ~ICR_ALDIE;
		icr |= ICR_START | ICR_TB;
	} else {
		if (i2c->msg->len == 0) {
			/*
			 * Device probes have a message length of zero
			 * and need the bus to be reset before it can
			 * be used again.
			 */
			i2c_pxa_reset(i2c);
		}
		i2c_pxa_master_complete(i2c, 0);
	}

	i2c->icrlog[i2c->irqlogidx-1] = icr;

	writel(icr, _ICR(i2c));
	show_state(i2c);
}

static void i2c_pxa_irq_rxfull(struct pxa_i2c *i2c, u32 isr)
{
	u32 icr = readl(_ICR(i2c)) & ~(ICR_START|ICR_STOP|ICR_ACKNAK|ICR_TB);

	/*
	 * Read the byte.
	 */
	i2c->msg->buf[i2c->msg_ptr++] = readl(_IDBR(i2c));

	if (i2c->msg_ptr < i2c->msg->len) {
		/*
		 * If this is the last byte of the last
		 * message, send a STOP.
		 */
		if (i2c->msg_ptr == i2c->msg->len - 1)
			icr |= ICR_STOP | ICR_ACKNAK;

		icr |= ICR_ALDIE | ICR_TB;
	} else {
		i2c_pxa_master_complete(i2c, 0);
	}

	i2c->icrlog[i2c->irqlogidx-1] = icr;

	writel(icr, _ICR(i2c));
}

#define VALID_INT_SOURCE	(ISR_SSD | ISR_ALD | ISR_ITE | ISR_IRF | \
				ISR_SAD | ISR_BED)
static irqreturn_t i2c_pxa_handler(int this_irq, void *dev_id)
{
	struct pxa_i2c *i2c = dev_id;
	u32 isr = readl(_ISR(i2c));

	if (!(isr & VALID_INT_SOURCE))
		return IRQ_NONE;

	if (i2c_debug > 2 && 0) {
		dev_dbg(&i2c->adap.dev, "%s: ISR=%08x, ICR=%08x, IBMR=%02x\n",
			__func__, isr, readl(_ICR(i2c)), readl(_IBMR(i2c)));
		decode_ISR(isr);
	}

	if (i2c->irqlogidx < ARRAY_SIZE(i2c->isrlog))
		i2c->isrlog[i2c->irqlogidx++] = isr;

	show_state(i2c);

	/*
	 * Always clear all pending IRQs.
	 */
	writel(isr & VALID_INT_SOURCE, _ISR(i2c));

	if (isr & ISR_SAD)
		i2c_pxa_slave_start(i2c, isr);
	if (isr & ISR_SSD)
		i2c_pxa_slave_stop(i2c);

	if (i2c_pxa_is_slavemode(i2c)) {
		if (isr & ISR_ITE)
			i2c_pxa_slave_txempty(i2c, isr);
		if (isr & ISR_IRF)
			i2c_pxa_slave_rxfull(i2c, isr);
	} else if (i2c->msg && (!i2c->highmode_enter)) {
		if (isr & ISR_ITE)
			i2c_pxa_irq_txempty(i2c, isr);
		if (isr & ISR_IRF)
			i2c_pxa_irq_rxfull(i2c, isr);
	} else if ((isr & ISR_ITE) && i2c->highmode_enter) {
		i2c->highmode_enter = false;
		wake_up(&i2c->wait);
	} else {
		i2c_pxa_scream_blue_murder(i2c, "spurious irq");
	}

	return IRQ_HANDLED;
}


static int i2c_pxa_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	struct pxa_i2c *i2c = adap->algo_data;
	int ret, i;
	unsigned int icr;

	icr = readl(_ICR(i2c));
	if (icr != i2c->icr_save) {
		pr_warn("i2c: <%s> ICR is modified!\n", i2c->adap.name);
		pr_warn("i2c: IBMR: %08x IDBR: %08x ICR: %08x ISR: %08x\n",
			readl(_IBMR(i2c)), readl(_IDBR(i2c)),
			readl(_ICR(i2c)), readl(_ISR(i2c)));
		pr_warn("i2c: reset controller!\n");
		i2c_pxa_reset(i2c);
	}

	/* enable i2c unit before transmission */
	if (!(icr & ICR_IUE))
		i2c_pxa_enable(i2c, true);

	enable_irq(i2c->irq);
	for (i = adap->retries; i >= 0; i--) {
		ret = i2c_pxa_do_xfer(i2c, msgs, num);
		if (ret != I2C_RETRY)
			goto out;

		if (i2c_debug)
			dev_dbg(&adap->dev, "Retrying transmission\n");
		udelay(100);
	}
	i2c_pxa_scream_blue_murder(i2c, "exhausted retries");
	ret = -EREMOTEIO;
 out:
	i2c_pxa_set_slave(i2c, ret);
	disable_irq(i2c->irq);

	/* disable i2c unit after transmission */
	i2c_pxa_enable(i2c, false);

	return ret;
}

static u32 i2c_pxa_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm i2c_pxa_algorithm = {
	.master_xfer	= i2c_pxa_xfer,
	.functionality	= i2c_pxa_functionality,
};

static const struct i2c_algorithm i2c_pxa_pio_algorithm = {
	.master_xfer	= i2c_pxa_pio_xfer,
	.functionality	= i2c_pxa_functionality,
};

void i2c_set_pio_mode(struct i2c_adapter *adap, unsigned int use_pio)
{
	static int bus_idle;
	static int i = 25;
	struct pxa_i2c *i2c;
	if (!adap) {
		pr_err("%s: adapter is NULL!\n", __func__);
		return;
	}

	i2c = adap->algo_data;

	i2c_pxa_reset(i2c);
	bus_idle = (!(readl(_ISR(i2c)) & (ISR_UB | ISR_IBB)) &&
		    readl(_IBMR(i2c)) == 0x3);
	while ((i > 0) && (!bus_idle)) {
		dev_info(&adap->dev, "wait until i2c bus is idle.\n");
		i2c_pxa_reset(i2c);
		cpu_relax();
		bus_idle = (!(readl(_ISR(i2c)) & (ISR_UB | ISR_IBB)) &&
			    readl(_IBMR(i2c)) == 0x3);
		i--;

	}
	if ((i == 0) && (!bus_idle))
		dev_err(&adap->dev, "wait i2c bus idle fails!!\n");

	if (use_pio) {
		adap->algo = &i2c_pxa_pio_algorithm;
		dev_info(&adap->dev, "use pio mode\n");
	} else {
		adap->algo = &i2c_pxa_algorithm;
		dev_info(&adap->dev, "do not use pio mode\n");
	}
}

static struct of_device_id i2c_pxa_dt_ids[] = {
	{ .compatible = "mrvl,pxa-i2c", .data = (void *)REGS_PXA2XX },
	{ .compatible = "mrvl,pwri2c", .data = (void *)REGS_PXA3XX },
	{ .compatible = "mrvl,mmp-twsi", .data = (void *)REGS_PXA910 },
	{}
};
MODULE_DEVICE_TABLE(of, i2c_pxa_dt_ids);

static int i2c_pxa_probe_dt(struct platform_device *pdev, struct pxa_i2c *i2c,
			    enum pxa_i2c_types *i2c_types)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *of_id =
			of_match_device(i2c_pxa_dt_ids, &pdev->dev);

	if (!of_id)
		return 1;

	/* For device tree we always use the dynamic or alias-assigned ID */
	i2c->adap.nr = -1;

	if (of_get_property(np, "mrvl,i2c-polling", NULL))
		i2c->use_pio = 1;
	if (of_get_property(np, "mrvl,i2c-fast-mode", NULL))
		i2c->fast_mode = 1;
	if (of_get_property(np, "mrvl,i2c-high-mode", NULL)) {
		i2c->high_mode = 1;
		ret = of_property_read_u8(np, "marvell,master-code",
					  &i2c->master_code);
		if (ret) {
			i2c->master_code = 0xe;
			ret = 0;
			dev_warn(&pdev->dev,
				 "set master code to default value 0xe\n");
		}
		ret = of_property_read_u32(np, "marvell,clk-rate",
					   (u32 *)&i2c->rate);
		if (ret) {
			dev_err(&pdev->dev, "failed to get clk rate\n");
			return ret;
		}
	}
	if (of_get_property(np, "mrvl,i2c-apdcp", NULL))
		i2c->apdcp = 1;

	*i2c_types = (long)(of_id->data);

	ret = of_property_read_u32(np, "marvell,i2c-ilcr", &i2c->ilcr);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "marvell,i2c-iwcr", &i2c->iwcr);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "lpm-qos", &i2c->pm_qos);
	if (ret)
		return ret;

	if (of_get_property(np, "marvell,i2c-always-on", NULL))
		i2c->always_on = 1;

	if (of_get_property(np, "marvell,i2c-enable-bus-rst", NULL))
		i2c->enable_bus_rst = 1;

	/* cannot do bus reset */
	if (!i2c->enable_bus_rst)
		return 0;

	if (of_get_property(np, "marvell,i2c-gpio-bus-rst", NULL))
		i2c->gpio_bus_rst = 1;

	/* use hardware bus reset, no need to toggle scl/sda */
	if (!i2c->gpio_bus_rst)
		return 0;

	i2c->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(i2c->pinctrl)) {
		i2c->pinctrl = NULL;
		dev_warn(&pdev->dev, "could not get pinctrl\n");
	} else {
		i2c->pin_i2c = pinctrl_lookup_state(i2c->pinctrl, "default");
		if (IS_ERR(i2c->pin_i2c)) {
			dev_err(&pdev->dev, "could not get default(i2c) pinstate\n");
			ret = IS_ERR(i2c->pin_i2c);
		}

		i2c->pin_gpio = pinctrl_lookup_state(i2c->pinctrl, "gpio");
		if (IS_ERR(i2c->pin_gpio)) {
			dev_err(&pdev->dev, "could not get gpio pinstate\n");
			ret = IS_ERR(i2c->pin_gpio);
		}

		if (i2c->apdcp) {
			i2c->pin_i2c_cp = pinctrl_lookup_state(i2c->pinctrl,
									"i2c_cp");
			if (IS_ERR(i2c->pin_i2c_cp)) {
				dev_err(&pdev->dev, "could not get i2c_cp pinstate\n");
				ret = IS_ERR(i2c->pin_i2c_cp);
			}
		}

		if (ret) {
			if (i2c->apdcp)
				i2c->pin_i2c_cp = NULL;
			i2c->pin_i2c = NULL;
			i2c->pin_gpio = NULL;
			i2c->pinctrl = NULL;
			ret = 0;
		}
	}

	return 0;
}

static int i2c_pxa_probe_pdata(struct platform_device *pdev,
			       struct pxa_i2c *i2c,
			       enum pxa_i2c_types *i2c_types)
{
	struct i2c_pxa_platform_data *plat = dev_get_platdata(&pdev->dev);
	const struct platform_device_id *id = platform_get_device_id(pdev);

	*i2c_types = id->driver_data;
	if (plat) {
		i2c->use_pio = plat->use_pio;
		i2c->fast_mode = plat->fast_mode;
		i2c->high_mode = plat->high_mode;
		i2c->apdcp = plat->apdcp;
		i2c->master_code = plat->master_code;
		if (!i2c->master_code)
			i2c->master_code = 0xe;
		i2c->rate = plat->rate;
		i2c->always_on = plat->always_on;
	}
	return 0;
}

static int i2c_pxa_probe(struct platform_device *dev)
{
	struct i2c_pxa_platform_data *plat = dev_get_platdata(&dev->dev);
	enum pxa_i2c_types i2c_type;
	struct pxa_i2c *i2c;
	struct resource *res = NULL;
	int ret, irq;

	i2c = kzalloc(sizeof(struct pxa_i2c), GFP_KERNEL);
	if (!i2c) {
		ret = -ENOMEM;
		goto emalloc;
	}

	/* Default adapter num to device id; i2c_pxa_probe_dt can override. */
	i2c->adap.nr = dev->id;

	ret = i2c_pxa_probe_dt(dev, i2c, &i2c_type);
	if (ret > 0)
		ret = i2c_pxa_probe_pdata(dev, i2c, &i2c_type);
	if (ret < 0)
		goto eres;

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(dev, 0);
	if (res == NULL || irq < 0) {
		ret = -ENODEV;
		goto eres;
	}

	if (!request_mem_region(res->start, resource_size(res), res->name)) {
		ret = -ENOMEM;
		goto eres;
	}

	i2c->adap.owner   = THIS_MODULE;
	i2c->adap.retries = 3;

	i2c->qos_idle.name = i2c->adap.name;
	pm_qos_add_request(&i2c->qos_idle, PM_QOS_CPUIDLE_BLOCK,
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

	spin_lock_init(&i2c->lock);
	init_waitqueue_head(&i2c->wait);

	strlcpy(i2c->adap.name, "pxa_i2c-i2c", sizeof(i2c->adap.name));

	i2c->clk = clk_get(&dev->dev, NULL);
	if (IS_ERR(i2c->clk)) {
		ret = PTR_ERR(i2c->clk);
		goto eclk;
	}

	i2c->reg_base = ioremap(res->start, resource_size(res));
	if (!i2c->reg_base) {
		ret = -EIO;
		goto eremap;
	}

	i2c->reg_ibmr = i2c->reg_base + pxa_reg_layout[i2c_type].ibmr;
	i2c->reg_idbr = i2c->reg_base + pxa_reg_layout[i2c_type].idbr;
	i2c->reg_icr = i2c->reg_base + pxa_reg_layout[i2c_type].icr;
	i2c->reg_isr = i2c->reg_base + pxa_reg_layout[i2c_type].isr;
	i2c->reg_icyc = i2c->reg_base + pxa_reg_layout[i2c_type].icyc;
	if (i2c_type != REGS_CE4100)
		i2c->reg_isar = i2c->reg_base + pxa_reg_layout[i2c_type].isar;

	i2c->reg_ilcr = i2c->reg_base + pxa_reg_layout[i2c_type].ilcr;
	i2c->reg_iwcr = i2c->reg_base + pxa_reg_layout[i2c_type].iwcr;

	i2c->iobase = res->start;
	i2c->iosize = resource_size(res);

	res = platform_get_resource(dev, IORESOURCE_MEM, 1);
	if (res) {
		i2c->hwlock_addr = ioremap(res->start, resource_size(res));
		dev_info(&dev->dev, "hardware lock address: 0x%p\n",
			 i2c->hwlock_addr);
	} else
		dev_dbg(&dev->dev, "no hardware lock used\n");

	i2c->irq = irq;

	i2c->slave_addr = I2C_PXA_SLAVE_ADDR;
	i2c->highmode_enter = false;

	if (plat) {
#ifdef CONFIG_I2C_PXA_SLAVE
		i2c->slave_addr = plat->slave_addr;
		i2c->slave = plat->slave;
#endif
		i2c->ilcr = plat->ilcr;
		i2c->iwcr = plat->iwcr;
		i2c->adap.class = plat->class;
		i2c->adap.hardware_lock = plat->hardware_lock;
		i2c->adap.hardware_unlock = plat->hardware_unlock;
		i2c->adap.hardware_trylock = plat->hardware_trylock;
	} else {
		if (i2c->hwlock_addr) {
			spin_lock_init(&lock_for_ripc);

			i2c->adap.hardware_lock = mmp_hwlock_lock;
			i2c->adap.hardware_unlock = mmp_hwlock_unlock;
			i2c->adap.hardware_trylock = mmp_hwlock_trylock;
		}
	}

	if (i2c->high_mode) {
		if (i2c->rate) {
			clk_set_rate(i2c->clk, i2c->rate);
			pr_info("i2c: <%s> set rate to %ld\n",
				i2c->adap.name, clk_get_rate(i2c->clk));
		} else
			pr_warn("i2c: <%s> clock rate not set\n",
				i2c->adap.name);
	}

	clk_prepare_enable(i2c->clk);

	if (i2c->use_pio) {
		i2c->adap.algo = &i2c_pxa_pio_algorithm;
	} else {
		i2c->adap.algo = &i2c_pxa_algorithm;
		ret = request_irq(irq, i2c_pxa_handler,
				IRQF_SHARED | IRQF_NO_SUSPEND,
				dev_name(&dev->dev), i2c);
		if (ret)
			goto ereqirq;
	}

	i2c_pxa_reset(i2c);

	disable_irq(i2c->irq);

	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &dev->dev;
#ifdef CONFIG_OF
	i2c->adap.dev.of_node = dev->dev.of_node;
#endif

	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret < 0) {
		printk(KERN_INFO "I2C: Failed to add bus\n");
		goto eadapt;
	}

	platform_set_drvdata(dev, i2c);

#ifdef CONFIG_I2C_PXA_SLAVE
	printk(KERN_INFO "I2C: %s: PXA I2C adapter, slave address %d\n",
	       dev_name(&i2c->adap.dev), i2c->slave_addr);
#else
	printk(KERN_INFO "I2C: %s: PXA I2C adapter\n",
	       dev_name(&i2c->adap.dev));
#endif
	/* i2c unit is enabled before transmission */
	i2c_pxa_enable(i2c, false);

	return 0;

eadapt:
	if (!i2c->use_pio)
		free_irq(irq, i2c);
ereqirq:
	clk_disable_unprepare(i2c->clk);
	iounmap(i2c->reg_base);
eremap:
	clk_put(i2c->clk);
eclk:
	release_mem_region(res->start, resource_size(res));
	pm_qos_remove_request(&i2c->qos_idle);
eres:
	kfree(i2c);
emalloc:
	return ret;
}

static int i2c_pxa_remove(struct platform_device *dev)
{
	struct pxa_i2c *i2c = platform_get_drvdata(dev);
	pm_qos_remove_request(&i2c->qos_idle);

	i2c_del_adapter(&i2c->adap);
	if (!i2c->use_pio)
		free_irq(i2c->irq, i2c);

	clk_disable_unprepare(i2c->clk);
	clk_put(i2c->clk);

	iounmap(i2c->reg_base);
	release_mem_region(i2c->iobase, i2c->iosize);
	kfree(i2c);

	return 0;
}

#ifdef CONFIG_PM
static int i2c_pxa_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pxa_i2c *i2c = platform_get_drvdata(pdev);

	/* i2c bus which needs clk on in suspend, for example, for pmic */
	if (i2c->always_on)
		return 0;

	clk_disable(i2c->clk);

	return 0;
}

static int i2c_pxa_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pxa_i2c *i2c = platform_get_drvdata(pdev);

	/* PMIC i2c bus */
	if (i2c->always_on)
		return 0;

	clk_enable(i2c->clk);
	i2c_pxa_reset(i2c);

	return 0;
}

static const struct dev_pm_ops i2c_pxa_dev_pm_ops = {
	.suspend_noirq = i2c_pxa_suspend_noirq,
	.resume_noirq = i2c_pxa_resume_noirq,
};

#define I2C_PXA_DEV_PM_OPS (&i2c_pxa_dev_pm_ops)
#else
#define I2C_PXA_DEV_PM_OPS NULL
#endif

static struct platform_driver i2c_pxa_driver = {
	.probe		= i2c_pxa_probe,
	.remove		= i2c_pxa_remove,
	.driver		= {
		.name	= "pxa2xx-i2c",
		.owner	= THIS_MODULE,
		.pm	= I2C_PXA_DEV_PM_OPS,
		.of_match_table = i2c_pxa_dt_ids,
	},
	.id_table	= i2c_pxa_id_table,
};

static int __init i2c_adap_pxa_init(void)
{
	return platform_driver_register(&i2c_pxa_driver);
}

static void __exit i2c_adap_pxa_exit(void)
{
	platform_driver_unregister(&i2c_pxa_driver);
}

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pxa2xx-i2c");

subsys_initcall(i2c_adap_pxa_init);
module_exit(i2c_adap_pxa_exit);
