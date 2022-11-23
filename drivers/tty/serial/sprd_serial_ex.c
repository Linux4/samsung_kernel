/*
 * Copyright (C) 2012-2020 Spreadtrum Communications Inc.
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

#if defined(CONFIG_SERIAL_SPRD_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/clk.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/sipc.h>

/* device name */
#define UART_NR_MAX		8
#define SPRD_TTY_NAME		"ttySE"
#define SPRD_FIFO_SIZE		128
#define SPRD_DEF_RATE		26000000
#define SPRD_BAUD_IO_LIMIT	3000000
#define SPRD_TIMEOUT		256000

/* the offset of serial registers and BITs for them */
/* data registers */
#define SPRD_TXD		0x0000
#define SPRD_RXD		0x0004

/* line status register and its BITs  */
#define SPRD_LSR		0x0008
#define SPRD_LSR_OE		BIT(4)
#define SPRD_LSR_FE		BIT(3)
#define SPRD_LSR_PE		BIT(2)
#define SPRD_LSR_BI		BIT(7)
#define SPRD_LSR_TX_OVER	BIT(15)

/* data number in TX and RX fifo */
#define SPRD_STS1		0x000C
#define SPRD_RX_FIFO_CNT_MASK	GENMASK(7, 0)
#define SPRD_TX_FIFO_CNT_MASK	GENMASK(15, 8)

/* interrupt enable register and its BITs */
#define SPRD_IEN		0x0010
#define SPRD_IEN_RX_FULL	BIT(0)
#define SPRD_IEN_TX_EMPTY	BIT(1)
#define SPRD_IEN_BREAK_DETECT	BIT(7)
#define SPRD_IEN_TIMEOUT	BIT(13)

/* interrupt clear register */
#define SPRD_ICLR		0x0014
#define SPRD_ICLR_TIMEOUT	BIT(13)

/* line control register */
#define SPRD_LCR		0x0018
#define SPRD_LCR_STOP_1BIT	0x10
#define SPRD_LCR_STOP_2BIT	0x30
#define SPRD_LCR_DATA_LEN	(BIT(2) | BIT(3))
#define SPRD_LCR_DATA_LEN5	0x0
#define SPRD_LCR_DATA_LEN6	0x4
#define SPRD_LCR_DATA_LEN7	0x8
#define SPRD_LCR_DATA_LEN8	0xc
#define SPRD_LCR_PARITY		(BIT(0) | BIT(1))
#define SPRD_LCR_PARITY_EN	0x2
#define SPRD_LCR_EVEN_PAR	0x0
#define SPRD_LCR_ODD_PAR	0x1

/* control register 1 */
#define SPRD_CTL1		0x001C
#define RX_HW_FLOW_CTL_THLD	BIT(6)
#define RX_HW_FLOW_CTL_EN	BIT(7)
#define TX_HW_FLOW_CTL_EN	BIT(8)
#define RX_TOUT_THLD_DEF	0x3E00
#define RX_HFC_THLD_DEF		0x40

/* fifo threshold register */
#define SPRD_CTL2		0x0020
#define THLD_TX_EMPTY		0x40
#define THLD_TX_EMPTY_SHIFT	8
#define THLD_RX_FULL		0x40

/* config baud rate register */
#define SPRD_CLKD0		0x0024
#define SPRD_CLKD0_MASK		GENMASK(15, 0)
#define SPRD_CLKD1		0x0028
#define SPRD_CLKD1_MASK		GENMASK(20, 16)
#define SPRD_CLKD1_SHIFT	16

/* interrupt mask status register */
#define SPRD_IMSR		0x002C
#define SPRD_IMSR_RX_FIFO_FULL	BIT(0)
#define SPRD_IMSR_TX_FIFO_EMPTY	BIT(1)
#define SPRD_IMSR_BREAK_DETECT	BIT(7)
#define SPRD_IMSR_TIMEOUT	BIT(13)
#define SPRD_DEFAULT_SOURCE_CLK	26000000

#define SPRD_SBUF_SIZE			0x200
#define SPRD_SBUF_NUM			1
#define SPRD_SBUF_ID			0
#define SPRD_SBUF_RX_SIZE		0x80

struct sprd_uart_sbuf {
	u8 stype;
	u8 dst;
	u8 channel;
	u8 bufid;
	u32 len;
	u32 bufnum;
	u32 txbufsize;
	u32 rxbufsize;
};

struct sprd_uart_port {
	struct uart_port port;
	char name[16];
	struct clk *clk;

	struct regmap *aon_apb;
	bool tx_start;
	struct kthread_worker kworker;
	struct task_struct *kworker_task;
	struct kthread_work tx_work;
	struct sprd_uart_sbuf sbuf;
	char *rxbuf;
};

static struct sprd_uart_port *sprd_port[UART_NR_MAX];
static int sprd_ports_num;

static inline unsigned int serial_in(struct uart_port *port,
				     unsigned int offset)
{
	return readl_relaxed(port->membase + offset);
}

static inline void serial_out(struct uart_port *port, unsigned int offset,
			      int value)
{
	writel_relaxed(value, port->membase + offset);
}

static unsigned int sprd_tx_empty(struct uart_port *port)
{
	if (serial_in(port, SPRD_STS1) & SPRD_TX_FIFO_CNT_MASK)
		return 0;
	else
		return TIOCSER_TEMT;
}

static unsigned int sprd_get_mctrl(struct uart_port *port)
{
	return TIOCM_DSR | TIOCM_CTS;
}

static void sprd_cm4_uart_start_tx(struct uart_port *port)
{
	struct sprd_uart_port *sp = container_of(port,
				struct sprd_uart_port, port);

	sp->tx_start = true;
	kthread_queue_work(&sp->kworker, &sp->tx_work);
}

static void sprd_stop_tx(struct uart_port *port)
{
	struct sprd_uart_port *sp = container_of(port,
				struct sprd_uart_port, port);

	sp->tx_start = false;
}

static void sprd_start_tx(struct uart_port *port)
{
	sprd_cm4_uart_start_tx(port);
}

static void sprd_cm4_uart_rx(int event, void *data)
{
	struct sprd_uart_port *sup = (struct sprd_uart_port *)data;
	struct sprd_uart_sbuf *sbuf = &sup->sbuf;
	struct uart_port *port = &sup->port;
	struct tty_port *tty = &port->state->port;
	int cnt;

	switch (event) {
	case SBUF_NOTIFY_READ:
		cnt = sbuf_read(sbuf->dst, sbuf->channel, sbuf->bufid,
				(void *)sup->rxbuf, sbuf->len, -1);
		if (cnt < 0)
			return;
		spin_lock(&port->lock);
		port->icount.rx += cnt;
		tty_insert_flip_string(tty, sup->rxbuf, cnt);
		tty_flip_buffer_push(tty);
		spin_unlock(&port->lock);
		break;
	default:
		break;
	}
}

static void sprd_cm4_uart_tx(struct kthread_work *work)
{
	struct sprd_uart_port *sp =
		container_of(work, struct sprd_uart_port, tx_work);
	struct sprd_uart_sbuf *sbuf = &sp->sbuf;
	struct uart_port *port = &sp->port;
	struct circ_buf *xmit = &port->state->xmit;
	int cnt = 0, ret;

	if (port->x_char) {
		sbuf_write(sbuf->dst, sbuf->channel, sbuf->bufid, &port->x_char,
			   cnt, -1);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}

	spin_lock(&port->lock);
	cnt = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);

	if (uart_circ_empty(xmit) || uart_tx_stopped(port) || !sp->tx_start) {
		spin_unlock(&port->lock);
		return;
	}
	spin_unlock(&port->lock);

	while (cnt) {
		console_lock();
		ret = sbuf_write(sbuf->dst, sbuf->channel, sbuf->bufid,
				 &xmit->buf[xmit->tail], cnt, -1);
		console_unlock();
		if (ret < 0)
			return;
		spin_lock(&port->lock);
		xmit->tail = (xmit->tail + ret) & (UART_XMIT_SIZE - 1);
		port->icount.tx += ret;
		if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
			uart_write_wakeup(port);
		spin_unlock(&port->lock);
		cnt -= ret;
	}

	sprd_cm4_uart_tx(work);
}

static int sprd_cm4_uart_init(struct sprd_uart_port *sp)
{
	struct sprd_uart_sbuf *sbuf = &sp->sbuf;
	int ret;

	sbuf->dst = SIPC_ID_PM_SYS;
	sbuf->channel = SMSG_CH_COMM;
	sbuf->txbufsize = SPRD_SBUF_SIZE;
	sbuf->rxbufsize = SPRD_SBUF_SIZE;
	sbuf->bufnum = SPRD_SBUF_NUM;
	sbuf->bufid = SPRD_SBUF_ID;
	sbuf->len = SPRD_SBUF_RX_SIZE;

	sp->rxbuf = devm_kzalloc(sp->port.dev, SPRD_SBUF_RX_SIZE,
		GFP_KERNEL);
	if (!sp->rxbuf)
		return -ENOMEM;

	ret = sbuf_create(sbuf->dst, sbuf->channel, sbuf->bufnum,
			  sbuf->txbufsize, sbuf->rxbufsize);
	if (ret) {
		dev_err(sp->port.dev, "init sbuf fail\n");
		return ret;
	}

	ret = sbuf_register_notifier(sbuf->dst, sbuf->channel,
				     sbuf->bufid, sprd_cm4_uart_rx, sp);
	if (ret) {
		dev_err(sp->port.dev, "register sbuf notifier fail\n");
		return ret;
	}

	kthread_init_worker(&sp->kworker);
	sp->kworker_task = kthread_run(kthread_worker_fn,
					   &sp->kworker, "%s",
					   sp->name);
	if (IS_ERR(sp->kworker_task)) {
		dev_emerg(sp->port.dev, "failed to create cm4 uart task\n");
		return PTR_ERR(sp->kworker_task);
	}
	kthread_init_work(&sp->tx_work, sprd_cm4_uart_tx);

	return 0;
}

static void sprd_stop_rx(struct uart_port *port)
{
	unsigned int ien, iclr;

	iclr = serial_in(port, SPRD_ICLR);
	ien = serial_in(port, SPRD_IEN);

	ien &= ~(SPRD_IEN_RX_FULL | SPRD_IEN_BREAK_DETECT);
	iclr |= SPRD_IEN_RX_FULL | SPRD_IEN_BREAK_DETECT;

	serial_out(port, SPRD_IEN, ien);
	serial_out(port, SPRD_ICLR, iclr);
}

static int sprd_startup(struct uart_port *port)
{
	unsigned int ien, fc;
	unsigned int timeout;
	unsigned long flags;

	serial_out(port, SPRD_CTL2,
		   THLD_TX_EMPTY << THLD_TX_EMPTY_SHIFT | THLD_RX_FULL);

	/* clear rx fifo */
	timeout = SPRD_TIMEOUT;
	while (timeout-- && serial_in(port, SPRD_STS1) & SPRD_RX_FIFO_CNT_MASK)
		serial_in(port, SPRD_RXD);

	/* clear tx fifo */
	timeout = SPRD_TIMEOUT;
	while (timeout-- && serial_in(port, SPRD_STS1) & SPRD_TX_FIFO_CNT_MASK)
		cpu_relax();

	/* clear interrupt */
	serial_out(port, SPRD_IEN, 0);
	serial_out(port, SPRD_ICLR, ~0);

	fc = serial_in(port, SPRD_CTL1);
	fc |= RX_TOUT_THLD_DEF | RX_HFC_THLD_DEF;
	serial_out(port, SPRD_CTL1, fc);

	/* enable interrupt */
	spin_lock_irqsave(&port->lock, flags);
	ien = serial_in(port, SPRD_IEN);
	ien |= SPRD_IEN_RX_FULL | SPRD_IEN_BREAK_DETECT | SPRD_IEN_TIMEOUT;
	serial_out(port, SPRD_IEN, ien);
	spin_unlock_irqrestore(&port->lock, flags);

	return 0;
}

static void sprd_shutdown(struct uart_port *port)
{
	serial_out(port, SPRD_IEN, 0);
	serial_out(port, SPRD_ICLR, ~0);
	devm_free_irq(port->dev, port->irq, port);
}

static void sprd_set_termios(struct uart_port *port,
			     struct ktermios *termios,
			     struct ktermios *old)
{
	unsigned int baud, quot;
	unsigned int lcr = 0, fc;
	unsigned long flags;

	/* ask the core to calculate the divisor for us */
	baud = uart_get_baud_rate(port, termios, old, 0, SPRD_BAUD_IO_LIMIT);

	quot = port->uartclk / baud;

	/* set data length */
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		lcr |= SPRD_LCR_DATA_LEN5;
		break;
	case CS6:
		lcr |= SPRD_LCR_DATA_LEN6;
		break;
	case CS7:
		lcr |= SPRD_LCR_DATA_LEN7;
		break;
	case CS8:
	default:
		lcr |= SPRD_LCR_DATA_LEN8;
		break;
	}

	/* calculate stop bits */
	lcr &= ~(SPRD_LCR_STOP_1BIT | SPRD_LCR_STOP_2BIT);
	if (termios->c_cflag & CSTOPB)
		lcr |= SPRD_LCR_STOP_2BIT;
	else
		lcr |= SPRD_LCR_STOP_1BIT;

	/* calculate parity */
	lcr &= ~SPRD_LCR_PARITY;
	termios->c_cflag &= ~CMSPAR;	/* no support mark/space */
	if (termios->c_cflag & PARENB) {
		lcr |= SPRD_LCR_PARITY_EN;
		if (termios->c_cflag & PARODD)
			lcr |= SPRD_LCR_ODD_PAR;
		else
			lcr |= SPRD_LCR_EVEN_PAR;
	}

	spin_lock_irqsave(&port->lock, flags);

	/* update the per-port timeout */
	uart_update_timeout(port, termios->c_cflag, baud);

	port->read_status_mask = SPRD_LSR_OE;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= SPRD_LSR_FE | SPRD_LSR_PE;
	if (termios->c_iflag & (IGNBRK | BRKINT | PARMRK))
		port->read_status_mask |= SPRD_LSR_BI;

	/* characters to ignore */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= SPRD_LSR_PE | SPRD_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		port->ignore_status_mask |= SPRD_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			port->ignore_status_mask |= SPRD_LSR_OE;
	}

	/* flow control */
	fc = serial_in(port, SPRD_CTL1);
	fc &= ~(RX_HW_FLOW_CTL_THLD | RX_HW_FLOW_CTL_EN | TX_HW_FLOW_CTL_EN);
	if (termios->c_cflag & CRTSCTS) {
		fc |= RX_HW_FLOW_CTL_THLD;
		fc |= RX_HW_FLOW_CTL_EN;
		fc |= TX_HW_FLOW_CTL_EN;
	}

	/* clock divider bit0~bit15 */
	serial_out(port, SPRD_CLKD0, quot & SPRD_CLKD0_MASK);

	/* clock divider bit16~bit20 */
	serial_out(port, SPRD_CLKD1,
		   (quot & SPRD_CLKD1_MASK) >> SPRD_CLKD1_SHIFT);
	serial_out(port, SPRD_LCR, lcr);
	fc |= RX_TOUT_THLD_DEF | RX_HFC_THLD_DEF;
	serial_out(port, SPRD_CTL1, fc);

	spin_unlock_irqrestore(&port->lock, flags);

	/* Don't rewrite B0 */
	if (tty_termios_baud_rate(termios))
		tty_termios_encode_baud_rate(termios, baud, baud);
}

static const char *sprd_type(struct uart_port *port)
{
	return "SPX";
}

static void sprd_release_port(struct uart_port *port)
{
	/* nothing to do */
}

static int sprd_request_port(struct uart_port *port)
{
	return 0;
}

static void sprd_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_SPRD;
}

static int sprd_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	if (ser->type != PORT_SPRD)
		return -EINVAL;
	if (port->irq != ser->irq)
		return -EINVAL;
	if (port->iotype != ser->io_type)
		return -EINVAL;
	return 0;
}

static void sprd_pm(struct uart_port *port, unsigned int state,
		unsigned int oldstate)
{
	struct sprd_uart_port *sup =
		container_of(port, struct sprd_uart_port, port);

	switch (state) {
	case UART_PM_STATE_ON:
		clk_prepare_enable(sup->clk);
		break;
	case UART_PM_STATE_OFF:
		clk_disable_unprepare(sup->clk);
		break;
	}
}

#ifdef CONFIG_CONSOLE_POLL
static int sprd_poll_init(struct uart_port *port)
{
	if (port->state->pm_state != UART_PM_STATE_ON) {
		sprd_pm(port, UART_PM_STATE_ON, 0);
		port->state->pm_state = UART_PM_STATE_ON;
	}

	return 0;
}

static int sprd_poll_get_char(struct uart_port *port)
{
	while (!(serial_in(port, SPRD_STS1) & SPRD_RX_FIFO_CNT_MASK))
		cpu_relax();

	return serial_in(port, SPRD_RXD);
}

static void sprd_poll_put_char(struct uart_port *port, unsigned char ch)
{
	while (serial_in(port, SPRD_STS1) & SPRD_TX_FIFO_CNT_MASK)
		cpu_relax();

	serial_out(port, SPRD_TXD, ch);
}
#endif

static void sprd_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	/* nothing to do */
}

static const struct uart_ops serial_sprd_ops = {
	.tx_empty = sprd_tx_empty,
	.get_mctrl = sprd_get_mctrl,
	.set_mctrl = sprd_set_mctrl,
	.stop_tx = sprd_stop_tx,
	.start_tx = sprd_start_tx,
	.stop_rx = sprd_stop_rx,
	.startup = sprd_startup,
	.shutdown = sprd_shutdown,
	.set_termios = sprd_set_termios,
	.type = sprd_type,
	.release_port = sprd_release_port,
	.request_port = sprd_request_port,
	.config_port = sprd_config_port,
	.verify_port = sprd_verify_port,
	.pm = sprd_pm,
#ifdef CONFIG_CONSOLE_POLL
	.poll_init	= sprd_poll_init,
	.poll_get_char	= sprd_poll_get_char,
	.poll_put_char	= sprd_poll_put_char,
#endif
};

#ifdef CONFIG_SERIAL_SPRD_CONSOLE
static void wait_for_xmitr(struct uart_port *port)
{
	unsigned int status, tmout = 10000;

	/* wait up to 10ms for the character(s) to be sent */
	do {
		status = serial_in(port, SPRD_STS1);
		if (--tmout == 0)
			break;
		udelay(1);
	} while (status & SPRD_TX_FIFO_CNT_MASK);
}

static void sprd_console_putchar(struct uart_port *port, int ch)
{
	wait_for_xmitr(port);
	serial_out(port, SPRD_TXD, ch);
}

static void sprd_console_write(struct console *co, const char *s,
			       unsigned int count)
{
	struct uart_port *port = &sprd_port[co->index]->port;
	int locked = 1;
	unsigned long flags;

	if (port->sysrq)
		locked = 0;
	else if (oops_in_progress)
		locked = spin_trylock_irqsave(&port->lock, flags);
	else
		spin_lock_irqsave(&port->lock, flags);

	uart_console_write(port, s, count, sprd_console_putchar);

	/* wait for transmitter to become empty */
	wait_for_xmitr(port);

	if (locked)
		spin_unlock_irqrestore(&port->lock, flags);
}

static int __init sprd_console_setup(struct console *co, char *options)
{
	struct sprd_uart_port *sprd_uart_port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index >= UART_NR_MAX || co->index < 0)
		co->index = 0;

	sprd_uart_port = sprd_port[co->index];
	if (!sprd_uart_port || !sprd_uart_port->port.membase) {
		pr_info("serial port %d not yet initialized\n", co->index);
		return -ENODEV;
	}

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(&sprd_uart_port->port, co, baud,
				parity, bits, flow);
}

static struct uart_driver sprd_uart_driver;
static struct console sprd_console = {
	.name = SPRD_TTY_NAME,
	.write = sprd_console_write,
	.device = uart_console_device,
	.setup = sprd_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &sprd_uart_driver,
};

static int __init sprd_serial_console_init(void)
{
	register_console(&sprd_console);
	return 0;
}
console_initcall(sprd_serial_console_init);

#define SPRD_CONSOLE	(&sprd_console)

/* Support for earlycon */
static void sprd_putc(struct uart_port *port, int c)
{
	unsigned int timeout = SPRD_TIMEOUT;

	while (timeout-- &&
	       !(readl(port->membase + SPRD_LSR) & SPRD_LSR_TX_OVER))
		cpu_relax();

	writeb(c, port->membase + SPRD_TXD);
}

static void sprd_early_write(struct console *con, const char *s, unsigned int n)
{
	struct earlycon_device *dev = con->data;

	uart_console_write(&dev->port, s, n, sprd_putc);
}

static int __init sprd_early_console_setup(struct earlycon_device *device,
					   const char *opt)
{
	if (!device->port.membase)
		return -ENODEV;

	device->con->write = sprd_early_write;
	return 0;
}
OF_EARLYCON_DECLARE(sprd_serial, "sprd,sc9836-uart-ex",
		    sprd_early_console_setup);

#else /* !CONFIG_SERIAL_SPRD_CONSOLE */
#define SPRD_CONSOLE		NULL
#endif

static struct uart_driver sprd_uart_driver = {
	.owner = THIS_MODULE,
	.driver_name = "sprd-serial-ex",
	.dev_name = SPRD_TTY_NAME,
	.major = 0,
	.minor = 0,
	.nr = UART_NR_MAX,
	.cons = SPRD_CONSOLE,
};

static int sprd_probe_dt_alias(int index, struct device *dev)
{
	struct device_node *np;
	int ret = index;

	if (!IS_ENABLED(CONFIG_OF))
		return ret;

	np = dev->of_node;
	if (!np)
		return ret;

	ret = of_alias_get_id(np, "serial");
	if (ret < 0)
		ret = index;
	else if (ret >= ARRAY_SIZE(sprd_port) || sprd_port[ret] != NULL) {
		dev_warn(dev, "requested serial port %d not available.\n", ret);
		ret = index;
	}

	return ret;
}

static int sprd_remove(struct platform_device *dev)
{
	struct sprd_uart_port *sup = platform_get_drvdata(dev);

	if (sup) {
		uart_remove_one_port(&sprd_uart_driver, &sup->port);
		sprd_port[sup->port.line] = NULL;
		sprd_ports_num--;
		if (!IS_ERR(sup->kworker_task))
			kthread_stop(sup->kworker_task);
	}

	sbuf_destroy(SIPC_ID_PM_SYS, SMSG_CH_COMM);

	if (!sprd_ports_num)
		uart_unregister_driver(&sprd_uart_driver);

	return 0;
}

static bool sprd_uart_is_console(struct uart_port *uport)
{
	struct console *cons = sprd_uart_driver.cons;

	if (cons && cons->index >= 0 && cons->index == uport->line)
		return true;

	return false;
}

static int sprd_clk_init(struct uart_port *uport)
{
	struct clk *clk_uart, *clk_parent;
	struct sprd_uart_port *u = sprd_port[uport->line];

	clk_uart = devm_clk_get(uport->dev, "uart");
	if (IS_ERR(clk_uart)) {
		dev_warn(uport->dev, "uart%d can't get uart clock\n",
			 uport->line);
		clk_uart = NULL;
	}

	clk_parent = devm_clk_get(uport->dev, "source");
	if (IS_ERR(clk_parent)) {
		dev_warn(uport->dev, "uart%d can't get source clock\n",
			 uport->line);
		clk_parent = NULL;
	}

	if (!clk_uart || clk_set_parent(clk_uart, clk_parent))
		uport->uartclk = SPRD_DEFAULT_SOURCE_CLK;
	else
		uport->uartclk = clk_get_rate(clk_uart);

	u->clk = devm_clk_get(uport->dev, "enable");
	if (IS_ERR(u->clk)) {
		if (PTR_ERR(u->clk) == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		dev_warn(uport->dev, "uart%d can't get enable clock\n",
			uport->line);

		/* To keep console alive even if the error occurred */
		if (!sprd_uart_is_console(uport))
			return PTR_ERR(u->clk);

		u->clk = NULL;
	}

	return 0;
}

static int sprd_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct uart_port *up;
	int index;
	int ret;

	for (index = 0; index < ARRAY_SIZE(sprd_port); index++)
		if (sprd_port[index] == NULL)
			break;

	if (index == ARRAY_SIZE(sprd_port))
		return -EBUSY;

	index = sprd_probe_dt_alias(index, &pdev->dev);

	sprd_port[index] = devm_kzalloc(&pdev->dev, sizeof(*sprd_port[index]),
					GFP_KERNEL);
	if (!sprd_port[index])
		return -ENOMEM;

	up = &sprd_port[index]->port;
	up->dev = &pdev->dev;
	up->line = index;
	up->type = PORT_SPRD;
	up->iotype = UPIO_MEM;
	up->uartclk = SPRD_DEF_RATE;
	up->fifosize = SPRD_FIFO_SIZE;
	up->ops = &serial_sprd_ops;
	up->flags = UPF_BOOT_AUTOCONF;

	ret = sprd_clk_init(up);
	if (ret)
		return ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	up->membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(up->membase))
		return PTR_ERR(up->membase);

	up->mapbase = res->start;

	if (!sprd_ports_num) {
		ret = uart_register_driver(&sprd_uart_driver);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to register SPRD-UART driver\n");
			return ret;
		}
	}
	sprd_ports_num++;

	ret = sprd_cm4_uart_init(sprd_port[index]);
	if (ret) {
		pr_err("cm4 uart init failed\r\n");
		sprd_remove(pdev);
		return ret;
	}

	ret = uart_add_one_port(&sprd_uart_driver, up);
	if (ret) {
		sprd_port[index] = NULL;
		sprd_remove(pdev);
	}

	platform_set_drvdata(pdev, up);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int sprd_suspend(struct device *dev)
{
	struct sprd_uart_port *sup = dev_get_drvdata(dev);

	uart_suspend_port(&sprd_uart_driver, &sup->port);

	return 0;
}

static int sprd_resume(struct device *dev)
{
	struct sprd_uart_port *sup = dev_get_drvdata(dev);

	uart_resume_port(&sprd_uart_driver, &sup->port);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(sprd_pm_ops, sprd_suspend, sprd_resume);

static const struct of_device_id serial_ids[] = {
	{.compatible = "sprd,sc9836-uart-ex",},
	{}
};
MODULE_DEVICE_TABLE(of, serial_ids);

static struct platform_driver sprd_platform_driver = {
	.probe		= sprd_probe,
	.remove		= sprd_remove,
	.driver		= {
		.name	= "sprd-serial-ex",
		.of_match_table = of_match_ptr(serial_ids),
		.pm	= &sprd_pm_ops,
	},
};

module_platform_driver(sprd_platform_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum SoC serial driver series");
