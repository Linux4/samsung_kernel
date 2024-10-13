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
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>

#include <asm/irq.h>
#include <asm/io.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/io.h>
#include <mach/globalregs.h>


#define SPRD_I2C_CTL_ID	(10)

/*offset*/
#define I2C_CTL                 		0x0000
#define I2C_CMD                 		0x0004
#define I2C_CLKD0               		0x0008
#define I2C_CLKD1               		0x000C
#define I2C_RST                 		0x0010
#define I2C_CMD_BUF             		0x0014
#define I2C_CMD_BUF_CTL		0x0018

/*The corresponding bit of I2C_CTL register*/
#define I2C_CTL_INT	(1 << 0)	/* I2c interrupt */
#define I2C_CTL_ACK	(1 << 1)	/* I2c received ack value */
#define I2C_CTL_BUSY	 (1 << 2)	/* I2c data line value */
#define I2C_CTL_IE	(1 << 3)	/* I2c interrupt enable */
#define I2C_CTL_EN	(1 << 4)	/* I2c module enable */
#define I2C_CTL_SCL_LINE	(1 << 5)	/*scl line signal */
#define I2C_CTL_SDA_LINE	(1 << 6)	/* sda line signal */
#define I2C_CTL_NOACK_INT_EN	(1 << 7)	/* no ack int enable */
#define I2C_CTL_NOACK_INT_STS		(1 << 8)	/* no ack int status */
#define I2C_CTL_NOACK_INT_CLR	(1 << 9)	/* no ack int clear */

/*The corresponding bit of I2C_CMD register*/
#define I2C_CMD_INT_ACK	(1 << 0)	/* I2c interrupt clear bit */
#define I2C_CMD_TX_ACK	(1 << 1)	/* I2c transmit ack that need to be send */
#define I2C_CMD_WRITE	(1 << 2)	/* I2c write command */
#define I2C_CMD_READ	(1 << 3)	/* I2c read command */
#define I2C_CMD_STOP	(1 << 4)	/* I2c stop command */
#define I2C_CMD_START	(1 << 5)	/* I2c start command */
#define I2C_CMD_ACK	(1 << 6)	/* I2c received ack  value */
#define I2C_CMD_BUSY	(1 << 7)	/* I2c busy in exec commands */
#define I2C_CMD_DATA	0xFF00	/* I2c data received or data need to be transmitted */

/*The corresponding bit of I2C_RST register*/
#define I2C_RST_RST	(1 << 0)	/* I2c reset bit */

/*The corresponding bit of I2C_CMD_BUF_CTL register*/
#define I2C_CTL_CMDBUF_EN	(1 << 0)	/* Enable the cmd buffer mode */
#define I2C_CTL_CMDBUF_EXEC	(1 << 1)	/* Start to exec the cmd in the cmd buffer */

#ifdef CONFIG_I2C_RESUME_EARLY
#include <linux/syscore_ops.h>
static struct platform_device *pdev_chip_i2c[SPRD_I2C_CTL_ID];
#endif


/* i2c controller state */
enum sprd_i2c_state {
	STATE_IDLE,
	STATE_START,
	STATE_READ,
	STATE_WRITE,
	STATE_STOP,
	STATE_ADDR_BYTE2
};
struct sprd_i2c {
	spinlock_t		lock;
	wait_queue_head_t	wait;

	struct i2c_msg		*msg;
	unsigned int		msg_num;
	unsigned int		msg_idx;
	unsigned int		msg_ptr;

	struct i2c_adapter	adap;

	enum sprd_i2c_state	state;
	struct clk *clk;
	int 			irq;
	bool is_suspended;
	void __iomem		*membase;
	unsigned int 		*memphys;
	unsigned int  		*memsize;
};
struct sprd_platform_i2c {
	unsigned int	normal_freq;	/* normal bus frequency */
	unsigned int	fast_freq;	/* fast frequency for the bus */
	unsigned int	min_freq;	/* min frequency for the bus */
};
static struct sprd_platform_i2c sprd_i2c_default_platform = {
	.normal_freq	= 100*1000,
	.fast_freq	= 400*1000,
	.min_freq	= 10*1000,
};
static struct sprd_i2c *sprd_i2c_ctl[SPRD_I2C_CTL_ID];//set to 10 


static inline void dump_i2c_reg(struct sprd_i2c *pi2c)
{
	printk(KERN_ERR ": ======dump i2c-%d reg=======\n", pi2c->adap.nr);
	printk(KERN_ERR ": I2C_CTRL:0x%x\n",__raw_readl(pi2c->membase + I2C_CTL));
	printk(KERN_ERR ": I2C_CMD:0x%x\n",__raw_readl(pi2c->membase + I2C_CMD));
	printk(KERN_ERR ": I2C_DVD0:0x%x\n",__raw_readl(pi2c->membase + I2C_CLKD0));
	printk(KERN_ERR ": I2C_DVD1:0x%x\n",__raw_readl(pi2c->membase + I2C_CLKD1));
	printk(KERN_ERR ": I2C_RST:0x%x\n",__raw_readl(pi2c->membase + I2C_RST));
	printk(KERN_ERR ": I2C_CMD_BUF:0x%x\n",__raw_readl(pi2c->membase + I2C_CMD_BUF));
	printk(KERN_ERR ": I2C_CMD_BUF_CTL:0x%x\n",__raw_readl(pi2c->membase + I2C_CMD_BUF_CTL));
}

/* sprd_i2c_wait_exec
 *
 * wait i2c cmd executable
*/
static inline int sprd_i2c_wait_exec(struct sprd_i2c *i2c)
{
	unsigned int cmd;
	int timeout = 2000;

	while (timeout-- > 0) {
		cmd = __raw_readl(i2c->membase + I2C_CMD);
		if (!(cmd & I2C_CMD_BUSY))
			return 0;
	}
	printk( "I2C:timeout,busy in exec commands !\n");
	dump_i2c_reg(i2c);
	return -ETIMEDOUT;
}

/*irq enable function*/
static inline void sprd_i2c_enable_irq(struct sprd_i2c *i2c)
{
	unsigned int ctl;

	ctl=__raw_readl(i2c->membase+I2C_CTL);
	__raw_writel(ctl | I2C_CTL_IE,i2c->membase+I2C_CTL);

}

/*irq disable function*/
static inline void sprd_i2c_disable_irq(struct sprd_i2c *i2c)
{
	unsigned int ctl;

	ctl=__raw_readl(i2c->membase+I2C_CTL);
	__raw_writel(ctl &~I2C_CTL_IE,i2c->membase+I2C_CTL);
}

static inline void sprd_clr_irq(struct sprd_i2c *i2c)
{
	unsigned int cmd;

	cmd=__raw_readl(i2c->membase+I2C_CMD) & 0xff00;
	__raw_writel(cmd | I2C_CMD_INT_ACK, i2c->membase+I2C_CMD);
}

static inline struct sprd_platform_i2c *sprd_i2c_get_platformdata(struct device *dev)
{
	if (dev!=NULL && dev->platform_data != NULL)
		return (struct sprd_platform_i2c *)dev->platform_data;

	return &sprd_i2c_default_platform;
}

static void set_i2c_clk(struct sprd_i2c *i2c,unsigned int freq)
{
	unsigned int apb_clk;
	unsigned int i2c_div;

	apb_clk=26000000;
	i2c_div=apb_clk/(4*freq)-3;

	__raw_writel(i2c_div&0xffff,i2c->membase+I2C_CLKD0);
	__raw_writel(i2c_div>>16,i2c->membase+I2C_CLKD1);

}


/**
 * sprd_i2c_message_start - send START to the bus.
 * @i2c: The struct sprd_i2c.
 * @msg: Pointer to the messages to be processed.
 *
 * Returns 0 on success, otherwise a negative errno.
 */
static void sprd_i2c_message_start(struct sprd_i2c *i2c, struct i2c_msg *msg)
{
	unsigned int cmd;
	if (msg->flags & I2C_M_TEN) {
		cmd = 0xf0 | (((msg->addr >> 8) & 0x03)<<1);
		if (msg->flags & I2C_M_RD){
			i2c->state = STATE_START;
		}
		else{
			i2c->state = STATE_ADDR_BYTE2;
		}
	}
	else
	{
		cmd = (msg->addr & 0x7f) << 1;
	}

	if (msg->flags & I2C_M_RD)
		cmd |= 0x1;

	cmd=(cmd<<8) | I2C_CMD_START | I2C_CMD_WRITE;
	__raw_writel(cmd, i2c->membase + I2C_CMD);
}



void sprd_i2c_ctl_chg_clk(unsigned int id_nr, unsigned int freq)
{
	unsigned int tmp;
	clk_prepare_enable(sprd_i2c_ctl[id_nr]->clk);

	tmp = __raw_readl(sprd_i2c_ctl[id_nr]->membase + I2C_CTL);
	__raw_writel(tmp & (~I2C_CTL_EN),
		     sprd_i2c_ctl[id_nr]->membase + I2C_CTL);
	tmp = __raw_readl(sprd_i2c_ctl[id_nr]->membase + I2C_CTL);

	set_i2c_clk(sprd_i2c_ctl[id_nr], freq);

	tmp = __raw_readl(sprd_i2c_ctl[id_nr]->membase + I2C_CTL);
	__raw_writel(tmp | I2C_CTL_EN,
		     sprd_i2c_ctl[id_nr]->membase + I2C_CTL);
	tmp = __raw_readl(sprd_i2c_ctl[id_nr]->membase + I2C_CTL);
	clk_disable_unprepare(sprd_i2c_ctl[id_nr]->clk);
	
}

EXPORT_SYMBOL_GPL(sprd_i2c_ctl_chg_clk);

static int sprd_i2c_clk_init(struct sprd_i2c *pi2c)
{
	char buf[256] = { 0 };
	if (unlikely(pi2c->adap.nr >= 5)) /* dolphin less than 5 and shark's i2c device that large than 5 is named clk_i2c' */
		strcpy(buf, "clk_i2c");
	else
		sprintf(buf, "clk_i2c%d", pi2c->adap.nr);
	dev_info(&pi2c->adap.dev, "%s buf=%s", __func__, buf);

	pi2c->clk = clk_get(&pi2c->adap.dev, buf);
	if (IS_ERR(pi2c->clk)) {
		return -ENODEV;
	}
	return 0;
}

static void sprd_i2c_enable(struct sprd_i2c *i2c)
{
	unsigned int tmp;
	struct sprd_platform_i2c *pdata;

	__raw_writel(0x1,i2c->membase+I2C_RST);
	__raw_writel(0,i2c->membase+I2C_RST);

	tmp = __raw_readl(i2c->membase + I2C_CTL);
	__raw_writel(tmp & ~I2C_CTL_EN, i2c->membase + I2C_CTL);
	tmp = __raw_readl(i2c->membase + I2C_CTL);
	__raw_writel(tmp & ~I2C_CTL_IE, i2c->membase + I2C_CTL);
	tmp = __raw_readl(i2c->membase + I2C_CMD_BUF_CTL);
	__raw_writel(tmp & ~I2C_CTL_CMDBUF_EN, i2c->membase + I2C_CMD_BUF_CTL);

	pdata = sprd_i2c_get_platformdata(i2c->adap.dev.parent);
	dev_dbg(&i2c->adap.dev, "%s() freq=%d\n", __func__,
		pdata->normal_freq);
	set_i2c_clk(i2c,pdata->normal_freq);

	tmp = __raw_readl(i2c->membase + I2C_CTL);
	__raw_writel(tmp | I2C_CTL_EN | I2C_CTL_IE, i2c->membase + I2C_CTL);

	//__raw_writel(I2C_CMD_INT_ACK, i2c->membase + I2C_CMD);
	sprd_clr_irq(i2c);

}


/**
 * sprd_i2c_doxfer - The driver's doxfer function.
 * @i2c: Pointer to the sprd_i2c structure.
 * @msgs: Pointer to the messages to be processed.
 * @num: Length of the MSGS array.
 *
 * Returns the number of messages processed, or a negative errno on
 * failure.
 */
static int sprd_i2c_doxfer(struct sprd_i2c *i2c, struct i2c_msg *msgs, int num)
{
	unsigned int timeout;
	unsigned long  flags;
	int ret;
	clk_prepare_enable(i2c->clk);

	ret = sprd_i2c_wait_exec(i2c);
	if (ret != 0) {
		ret = -EAGAIN;
		goto out;
	}

	spin_lock_irqsave(&i2c->lock,flags);

	i2c->msg = msgs;
	i2c->msg_num = num;
	i2c->msg_ptr = 0;
	i2c->msg_idx = 0;
	i2c->state = STATE_START;

	sprd_clr_irq(i2c);
	sprd_i2c_enable_irq(i2c);
	sprd_i2c_message_start(i2c, msgs);

	spin_unlock_irqrestore(&i2c->lock,flags);

	timeout=wait_event_timeout(i2c->wait, i2c->msg_num == 0, HZ);

	ret = i2c->msg_idx;
	/* having these next two as dev_err() makes life very
	 * noisy when doing an i2cdetect */
	if (timeout == 0){
		printk("i2c timeout i2c_trl =0x%x, i2c_cmd=0x%x\n",
				__raw_readl(i2c->membase+I2C_CTL), __raw_readl(i2c->membase+I2C_CMD));

		sprd_i2c_disable_irq(i2c);
		sprd_clr_irq(i2c);
		sprd_i2c_enable(i2c);
		ret = -ENXIO;
	}else if (ret != num){
		printk("incomplete xfer (%d)\n", ret);
		ret = -EAGAIN;
	}

 out:
	 clk_disable_unprepare(i2c->clk);
	return ret;
}

/**
 * sprd_i2c_xfer - The driver's master_xfer function.
 * @adap: Pointer to the i2c_adapter structure.
 * @msgs: Pointer to the messages to be processed.
 * @num: Length of the MSGS array.
 *
 * Returns the number of messages processed, or a negative errno on
 * failure.
*/
static int sprd_i2c_xfer(struct i2c_adapter *adap,struct i2c_msg *msgs, int num)
{
	struct sprd_i2c *i2c = (struct sprd_i2c *)adap->algo_data;
	int retry;
	int ret;
	if (i2c->is_suspended)
		return -EBUSY; 
	for (retry = 0; retry < adap->retries; retry++) {

		ret = sprd_i2c_doxfer(i2c, msgs, num);

		if (ret != -EAGAIN)
			return ret;

		printk("I2C:Retrying transmission (%d)\n", retry);

		udelay(100);
	}
	printk("I2C:transmission failed!\n");
	return -EREMOTEIO;
}

/* declare our i2c functionality */
static unsigned int sprd_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL ;
}

/* i2c bus registration info */
static const struct i2c_algorithm sprd_i2c_algorithm = {
	.master_xfer		= sprd_i2c_xfer,
	.functionality		= sprd_i2c_func,
};

/* sprd_i2c_complete
 *
 * complete the message and wake up the caller, using the given return code,
 * or zero to mean ok.
*/
static inline void sprd_i2c_complete(struct sprd_i2c *i2c, int ret)
{
	i2c->msg_ptr = 0;
	i2c->msg = NULL;
	i2c->msg_idx ++;
	i2c->msg_num = 0;
	if (ret)
		i2c->msg_idx = ret;

	wake_up(&i2c->wait);
}

static inline void sprd_i2c_stop_status(struct sprd_i2c *i2c, int ret)
{
	i2c->state = STATE_STOP;

	sprd_i2c_complete(i2c, ret);
	sprd_i2c_disable_irq(i2c);
}

static inline void sprd_i2c_stop(struct sprd_i2c *i2c, int ret)
{
	unsigned int cmd;

	cmd= I2C_CMD_STOP | I2C_CMD_WRITE;
	__raw_writel(cmd,i2c->membase+I2C_CMD);

	i2c->state = STATE_STOP;

	sprd_i2c_complete(i2c, ret);
	sprd_i2c_disable_irq(i2c);
}

/* is_lastmsg()
 *
 * returns TRUE if the current message is the last in the set
*/
static inline int is_lastmsg(struct sprd_i2c *i2c)
{
	return i2c->msg_idx >= (i2c->msg_num - 1);
}

/* is_msglast
 *
 * returns TRUE if we this is the last byte in the current message
*/
static inline int is_msglast(struct sprd_i2c *i2c)
{
	return i2c->msg_ptr == i2c->msg->len-1;
}

/* is_msgend
 *
 * returns TRUE if we reached the end of the current message
*/
static inline int is_msgend(struct sprd_i2c *i2c)
{
	return i2c->msg_ptr >= i2c->msg->len;
}

static inline void sprd_i2c_read_tx_ack(struct sprd_i2c *i2c)
{
	unsigned int cmd;

	cmd =I2C_CMD_READ | I2C_CMD_STOP | I2C_CMD_TX_ACK;
	__raw_writel(cmd,i2c->membase+I2C_CMD);
}

static inline void sprd_i2c_read(struct sprd_i2c *i2c)
{
	unsigned int cmd;

	cmd =I2C_CMD_READ;
	__raw_writel(cmd,i2c->membase+I2C_CMD);
}

static void sprd_read_handle(struct sprd_i2c *i2c)
{
	if (is_msglast(i2c)) {
		if (is_lastmsg(i2c)){
			sprd_i2c_read_tx_ack(i2c);
		}
	} else if (is_msgend(i2c)) {
		if (is_lastmsg(i2c)) {
			i2c->state = STATE_STOP;
			sprd_i2c_complete(i2c,0);
			sprd_i2c_disable_irq(i2c);

		} else {
			i2c->msg_ptr = 0;
			i2c->msg_idx++;
			i2c->msg++;
			sprd_i2c_read(i2c);
		}
	}
	else{
		sprd_i2c_read(i2c);
	}
}

static int sprd_i2c_irq_nextbyte(struct sprd_i2c *i2c,unsigned int cmd_reg)
{
	unsigned char byte;
	unsigned int cmd;
	int ret = 0;

	switch (i2c->state) {

	case STATE_IDLE:
		printk("sprd_i2c_irq_nextbyte error: called in STATE_IDLE\n");
		goto out;
		break;

	case STATE_STOP:
		printk( "sprd_i2c_irq_nextbyte error: called in STATE_STOP\n");
		sprd_i2c_disable_irq(i2c);
		goto out;

	case STATE_ADDR_BYTE2:
		cmd =((i2c->msg->addr & 0xff)<<8) | I2C_CMD_WRITE;
		__raw_writel(cmd,i2c->membase+I2C_CMD);
		i2c->state = STATE_START;
		break;

	case STATE_START:
		if (cmd_reg  & I2C_CMD_ACK && !(i2c->msg->flags & I2C_M_IGNORE_NAK)) {
			printk("I2C error:ack was not received\n");
			dump_i2c_reg(i2c);
			sprd_i2c_stop(i2c,-EREMOTEIO);
			goto out;
		}

		if (i2c->msg->flags & I2C_M_RD)
			i2c->state = STATE_READ;
		else
			i2c->state = STATE_WRITE;

		if (is_lastmsg(i2c) && i2c->msg->len == 0) {
			printk("detect address finish\n");
			sprd_i2c_stop(i2c,0);
			goto out;
		}

		if (i2c->state == STATE_READ)
		{
			sprd_read_handle(i2c);
			break;
		}

	case STATE_WRITE:
		if (!is_msgend(i2c)) {
			if(is_msglast(i2c) && is_lastmsg(i2c)){
				byte = i2c->msg->buf[i2c->msg_ptr++];
				cmd =(byte<<8) | I2C_CMD_WRITE | I2C_CMD_STOP;
				__raw_writel(cmd,i2c->membase+I2C_CMD);

			}
			else
			{
				byte = i2c->msg->buf[i2c->msg_ptr++];
				cmd =(byte<<8) | I2C_CMD_WRITE;

				__raw_writel(cmd,i2c->membase+I2C_CMD);
			}
		} else if (!is_lastmsg(i2c)) {

			i2c->msg_ptr = 0;
			i2c->msg_idx ++;
			i2c->msg++;

			sprd_i2c_message_start(i2c, i2c->msg);
			i2c->state = STATE_START;

		} else {
			sprd_i2c_stop_status(i2c,0);
		}
		break;

	case STATE_READ:
		if (!(i2c->msg->flags & I2C_M_IGNORE_NAK) &&
		    !(is_msglast(i2c) && is_lastmsg(i2c))) {

			if (cmd_reg  & I2C_CMD_ACK) {
				printk("I2C READ error: No Ack\n");
				dump_i2c_reg(i2c);
				sprd_i2c_stop(i2c,-ECONNREFUSED);
				goto out;
			}
		}

		cmd = __raw_readl(i2c->membase+I2C_CMD);
		byte = (unsigned char)(cmd>>8);
		i2c->msg->buf[i2c->msg_ptr++] = byte;
		sprd_read_handle(i2c);

		break;
	}
 out:
	return ret;
}

/**
 * sprd_i2c_irq - the interrupt service routine.
 * @int: The irq, unused.
 * @dev_id: Our struct sprd_i2c.
 */
static irqreturn_t sprd_i2c_irq(int irq, void *dev_id)
{
	struct sprd_i2c *i2c;
	unsigned int cmd;
	unsigned int ctl;
	int ret;

	i2c = (struct sprd_i2c *)dev_id;

	ctl = __raw_readl(i2c->membase+I2C_CTL);

	if (!(ctl & I2C_CTL_INT)) {
#if  0//only for debug
		unsigned int debug_irq_status = 0;
		debug_irq_status = __raw_readl(SPRD_I2C0_BASE+I2C_CTL);
		debug_irq_status |= __raw_readl(SPRD_I2C1_BASE+I2C_CTL);
		debug_irq_status |= __raw_readl(SPRD_I2C2_BASE+I2C_CTL);
		debug_irq_status |= __raw_readl(SPRD_I2C3_BASE+I2C_CTL);
		if (!(debug_irq_status & I2C_CTL_INT)) {
				printk("I2C:sprd_i2c_irq, NOT FOUND Or had handled!!!\n");
		}
#endif
		return IRQ_NONE;
	}
	sprd_clr_irq(i2c);

	if (i2c->state == STATE_IDLE) {
		printk( "I2c irq: error i2c->state == IDLE\n");
		goto out;
	}

   	ret = sprd_i2c_wait_exec(i2c);
	if (ret != 0) {
		printk("I2C busy on exec command!\n");
		goto out;
	}


	cmd =__raw_readl(i2c->membase+I2C_CMD);
	ret=sprd_i2c_irq_nextbyte(i2c,cmd);


	if(ret!=0)
		printk("I2C irq:error\n");
 out:
	return IRQ_HANDLED;
}


static ssize_t i2c_reset(struct device* cd, struct device_attribute *attr,
		       const char* buf, size_t len)
{
	printk("%s\n",__func__);
	sprd_i2c_enable(sprd_i2c_ctl[1]);
	return len;
}

static DEVICE_ATTR(reset, S_IRUGO | S_IWUSR, NULL, i2c_reset);

static int i2c_create_sysfs(struct platform_device *pdev)
{
	int err;
	struct device *dev = &(pdev->dev);

	err = device_create_file(dev, &dev_attr_reset);

	return err;
}


static int sprd_i2c_probe(struct platform_device *pdev)
{
	struct sprd_i2c *i2c;
	struct resource *res;
	int irq;
	int ret;

	i2c = kzalloc(sizeof(struct sprd_i2c), GFP_KERNEL);
	if (!i2c){
		printk("I2C:kzalloc failed!\n");
		return  -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	printk("I2C: res->start=%x\n", res->start);
	irq = platform_get_irq(pdev, 0);
	printk("I2C: irq=%d\n", irq);
	if (res == NULL || irq < 0){
		printk("I2C:platform_get_resource&irq failed!\n");
		ret=-ENODEV;
		goto err_irq;
	}
#if 0
	if (!request_mem_region(res->start, res_len(res), res->name)){
		printk("I2C:request_mem_region failed!\n");
		ret=-ENOMEM;
		goto err_irq;
	}
#endif

	i2c->membase= (void*)(res->start);
	if (!i2c->membase) {
		printk("I2C:ioremap failed!\n");
		ret=-EIO;
		goto err_irq;
	}

	i2c->irq = irq;

	spin_lock_init(&i2c->lock);
	init_waitqueue_head(&i2c->wait);

	snprintf(i2c->adap.name,sizeof(i2c->adap.name),"%s","sprd-i2c");
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.retries = 4;
	i2c->adap.algo = &sprd_i2c_algorithm;
	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;

        i2c->adap.nr = pdev->id;
      /* initialize the i2c controller */

	  ret = sprd_i2c_clk_init(i2c);
	  if (ret) {
		  dev_err(&pdev->dev, "get src clk failed\n");
		  goto err_irq;
	  }
	  
	  clk_prepare_enable(i2c->clk);
	  sprd_i2c_enable(i2c);

	  clk_disable_unprepare(i2c->clk);


	//sprd_i2c_special_init(pdev->id);

	ret = request_irq(i2c->irq, sprd_i2c_irq, IRQF_SHARED,pdev->name,i2c);
	if (ret) {
		printk("I2C:request_irq failed!\n");
		goto err_irq;
	}

	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret < 0){
		printk("I2C:add_adapter failed!\n");
		goto err_adap;
	}

	sprd_i2c_ctl[pdev->id] = i2c;
	printk("I2C:sprd_i2c_ctl[%d]=%x",pdev->id, (unsigned int)(sprd_i2c_ctl[pdev->id]));
	platform_set_drvdata(pdev, i2c);
#ifdef CONFIG_I2C_RESUME_EARLY
		pdev_chip_i2c[pdev->id] = pdev;
#endif

	i2c_create_sysfs(pdev);

	return 0;

err_adap:
	free_irq(i2c->irq,i2c);
err_irq:
	kfree(i2c);

	return ret;
}

static int sprd_i2c_remove(struct platform_device *pdev)
{
	struct sprd_i2c *i2c = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
#ifdef CONFIG_I2C_RESUME_EARLY
		pdev_chip_i2c[pdev->id] = NULL;
#endif

	i2c_del_adapter(&i2c->adap);

	free_irq(i2c->irq, i2c);

	kfree(i2c);
	printk("[I2C:sprd_i2c_remove]");

	return 0;
}

#if defined (CONFIG_PM) && defined(CONFIG_ARCH_SCX35)
struct i2c_regs {
	unsigned long ctl;/*0x0*/
	unsigned long cmd;/*0x4*/
	unsigned long div0;/*0x8*/
	unsigned long div1;/*0xc*/
	unsigned long rst;/*0x10*/
	unsigned long cmd_buf;/*0x14*/
	unsigned long cmd_buf_ctl;/*0x18*/

	/*global i2c_regs*/
};

static struct i2c_regs l2c_saved_regs[SPRD_I2C_CTL_ID];
static int i2c_controller_suspend(struct platform_device *pdev,
				      pm_message_t state)
{
	struct sprd_i2c *pi2c = platform_get_drvdata(pdev);

	if (pi2c && (pi2c->adap.nr < ARRAY_SIZE(l2c_saved_regs))) {
		clk_prepare_enable(pi2c->clk);		
		l2c_saved_regs[pi2c->adap.nr].ctl = __raw_readl(pi2c->membase + I2C_CTL);
		l2c_saved_regs[pi2c->adap.nr].cmd = __raw_readl(pi2c->membase + I2C_CMD);
		l2c_saved_regs[pi2c->adap.nr].div0 = __raw_readl(pi2c->membase + I2C_CLKD0);
		l2c_saved_regs[pi2c->adap.nr].div1 = __raw_readl(pi2c->membase + I2C_CLKD1);
		l2c_saved_regs[pi2c->adap.nr].rst = __raw_readl(pi2c->membase + I2C_RST);
		l2c_saved_regs[pi2c->adap.nr].cmd_buf = __raw_readl(pi2c->membase + I2C_CMD_BUF);
		l2c_saved_regs[pi2c->adap.nr].cmd_buf_ctl = __raw_readl(pi2c->membase + I2C_CMD_BUF_CTL);
		pi2c->is_suspended = true; 
	}
	if (pi2c && !IS_ERR(pi2c->clk))
		clk_disable_unprepare(pi2c->clk);
	return 0;
}

static int i2c_controller_resume(struct platform_device *pdev)
{
	unsigned int tmp;
	struct sprd_i2c *pi2c = platform_get_drvdata(pdev);

	if (pi2c && !IS_ERR(pi2c->clk))
		clk_prepare_enable(pi2c->clk);
	if (pi2c) {
	        tmp = __raw_readl( pi2c->membase + I2C_CTL);
		__raw_writel(tmp & (~I2C_CTL_EN), pi2c->membase + I2C_CTL);
		__raw_writel(l2c_saved_regs[pi2c->adap.nr].div0, pi2c->membase + I2C_CLKD0);
		__raw_writel(l2c_saved_regs[pi2c->adap.nr].div1, pi2c->membase + I2C_CLKD1);
		__raw_writel(l2c_saved_regs[pi2c->adap.nr].ctl, pi2c->membase + I2C_CTL);
		__raw_writel(l2c_saved_regs[pi2c->adap.nr].cmd, pi2c->membase + I2C_CMD);
		__raw_writel(l2c_saved_regs[pi2c->adap.nr].rst, pi2c->membase + I2C_RST);
		__raw_writel(l2c_saved_regs[pi2c->adap.nr].cmd_buf, pi2c->membase + I2C_CMD_BUF);
		__raw_writel(l2c_saved_regs[pi2c->adap.nr].cmd_buf_ctl, pi2c->membase + I2C_CMD_BUF_CTL);
		pi2c->is_suspended = false; 
		clk_disable_unprepare(pi2c->clk);
	}
	return 0;
}

#ifdef CONFIG_I2C_RESUME_EARLY
static int i2c_controller_suspend_late(void)
{
	int i;
	pm_message_t state = {
		.event = 0
	};

	for (i = 0; i < ARRAY_SIZE(sprd_i2c_ctl); i++) {
		if (pdev_chip_i2c[i] == NULL)
			continue;
		i2c_controller_suspend(pdev_chip_i2c[i], state);
	}

	return 0;
}

static void i2c_controller_resume_early(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sprd_i2c_ctl); i++) {
		if (pdev_chip_i2c[i] == NULL)
			continue;
		i2c_controller_resume(pdev_chip_i2c[i]);
	}
}

static struct syscore_ops sprd_i2c_syscore_ops = {
	.suspend = i2c_controller_suspend_late,
	.resume = i2c_controller_resume_early,
};

static int __init sprd_i2c_syscore_init(void)
{
	register_syscore_ops(&sprd_i2c_syscore_ops);
	return 0;
}
subsys_initcall(sprd_i2c_syscore_init);
#endif /* CONFIG_I2C_RESUME_EARLY */
#else
#define i2c_controller_suspend	NULL
#define i2c_controller_resume	NULL
#endif


static struct platform_driver sprd_i2c_driver = {
	.probe		= sprd_i2c_probe,
	.remove		= sprd_i2c_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "sprd-i2c",
	},
#ifndef CONFIG_I2C_RESUME_EARLY
	.suspend = i2c_controller_suspend,
	.resume = i2c_controller_resume,
#endif
	
};

static int __init i2c_adap_sprd_init(void)
{
	printk(KERN_INFO"I2c:sprd driver $Revision:1.0 $\n");

	return platform_driver_register(&sprd_i2c_driver);
}
subsys_initcall(i2c_adap_sprd_init);

static void __exit i2c_adap_sprd_exit(void)
{
	platform_driver_unregister(&sprd_i2c_driver);
}
module_exit(i2c_adap_sprd_exit);

MODULE_DESCRIPTION("sprd I2C Bus driver");
MODULE_AUTHOR("hao liu, <hao.liu@spreadtrum.com>");
MODULE_LICENSE("GPL");

