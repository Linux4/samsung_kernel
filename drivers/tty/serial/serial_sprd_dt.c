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
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/termios.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <mach/hardware.h>
#include <linux/clk.h>
#include <mach/pinmap.h>
#include <mach/serial_sprd.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <mach/board.h>
#include <linux/gpio.h>

#define IRQ_WAKEUP 	0

#define UART_NR_MAX			CONFIG_SERIAL_SPRD_UART_NR
#define SP_TTY_NAME			"ttyS"
#define SP_TTY_MINOR_START	64
#define SP_TTY_MAJOR		TTY_MAJOR

#define UART_CLK		48000000

/*offset*/
#define ARM_UART_TXD	0x0000
#define ARM_UART_RXD	0x0004
#define ARM_UART_STS0	0x0008
#define ARM_UART_STS1	0x000C
#define ARM_UART_IEN	0x0010
#define ARM_UART_ICLR	0x0014
#define ARM_UART_CTL0	0x0018
#define ARM_UART_CTL1	0x001C
#define ARM_UART_CTL2	0x0020
#define ARM_UART_CLKD0	0x0024
#define ARM_UART_CLKD1	0x0028
#define ARM_UART_STS2	0x002C
/*UART IRQ num*/

/*UART FIFO watermark*/
#define SP_TX_FIFO		0x40
#define SP_RX_FIFO		0x40
/*UART IEN*/
#define UART_IEN_RX_FIFO_FULL	(0x1<<0)
#define UART_IEN_TX_FIFO_EMPTY	(0x1<<1)
#define UART_IEN_BREAK_DETECT	(0x1<<7)
#define UART_IEN_TIMEOUT     	(0x1<<13)

/*data length*/
#define UART_DATA_BIT	(0x3<<2)
#define UART_DATA_5BIT	(0x0<<2)
#define UART_DATA_6BIT	(0x1<<2)
#define UART_DATA_7BIT	(0x2<<2)
#define UART_DATA_8BIT	(0x3<<2)
/*stop bit*/
#define UART_STOP_1BIT	(0x1<<4)
#define UART_STOP_2BIT	(0x3<<4)
/*parity*/
#define UART_PARITY		0x3
#define UART_PARITY_EN	0x2
#define UART_EVEN_PAR	0x0
#define UART_ODD_PAR	0x1
/*line status */
#define UART_LSR_OE	(0x1<<4)
#define UART_LSR_FE	(0x1<<3)
#define UART_LSR_PE	(0x1<<2)
#define UART_LSR_BI	(0x1<<7)
#define UART_LSR_DR	(0x1<<8)
/*flow control */
#define RX_HW_FLOW_CTL_THRESHOLD	0x40
#define RX_HW_FLOW_CTL_EN		(0x1<<7)
#define TX_HW_FLOW_CTL_EN		(0x1<<8)
/*status indicator*/
#define UART_STS_RX_FIFO_FULL	(0x1<<0)
#define UART_STS_TX_FIFO_EMPTY	(0x1<<1)
#define UART_STS_BREAK_DETECT	(0x1<<7)
#define UART_STS_TIMEOUT     	(0x1<<13)
/*baud rate*/
#define BAUD_1200_48M	0x9C40
#define BAUD_2400_48M	0x4E20
#define BAUD_4800_48M	0x2710
#define BAUD_9600_48M	0x1388
#define BAUD_19200_48M	0x09C4
#define BAUD_38400_48M	0x04E2
#define BAUD_57600_48M	0x0314
#define BAUD_115200_48M	0x01A0
#define BAUD_230400_48M	0x00D0
#define BAUD_460800_48M	0x0068
#define BAUD_921600_48M	0x0034
#define BAUD_1000000_48M 0x0030
#define BAUD_1152000_48M 0x0029
#define BAUD_1500000_48M 0x0020
#define BAUD_2000000_48M 0x0018
#define BAUD_2500000_48M 0x0013
#define BAUD_3000000_48M 0x0010

static struct wake_lock uart_rx_lock;	// UART0  RX  IRQ
static bool is_uart_rx_wakeup;
static struct serial_data plat_data;

static inline unsigned int serial_in(struct uart_port *port, int offset)
{
	return __raw_readl(port->membase + offset);
}

static inline void serial_out(struct uart_port *port, int offset, int value)
{
	__raw_writel(value, port->membase + offset);
}

static unsigned int serial_sprd_tx_empty(struct uart_port *port)
{
	if (serial_in(port, ARM_UART_STS1) & 0xff00)
		return 0;
	else
		return 1;
}

static unsigned int serial_sprd_get_mctrl(struct uart_port *port)
{
	return TIOCM_DSR | TIOCM_CTS;
}

static void serial_sprd_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static void serial_sprd_stop_tx(struct uart_port *port)
{
	unsigned int ien, iclr;

	iclr = serial_in(port, ARM_UART_ICLR);
	ien = serial_in(port, ARM_UART_IEN);

	iclr |= UART_IEN_TX_FIFO_EMPTY;
	ien &= ~UART_IEN_TX_FIFO_EMPTY;

	serial_out(port, ARM_UART_ICLR, iclr);
	serial_out(port, ARM_UART_IEN, ien);
}

static void serial_sprd_start_tx(struct uart_port *port)
{
	unsigned int ien;

	ien = serial_in(port, ARM_UART_IEN);
	if (!(ien & UART_IEN_TX_FIFO_EMPTY)) {
		ien |= UART_IEN_TX_FIFO_EMPTY;
		serial_out(port, ARM_UART_IEN, ien);
	}
}

static void serial_sprd_stop_rx(struct uart_port *port)
{
	unsigned int ien, iclr;

	iclr = serial_in(port, ARM_UART_ICLR);
	ien = serial_in(port, ARM_UART_IEN);

	ien &= ~(UART_IEN_RX_FIFO_FULL | UART_IEN_BREAK_DETECT);
	iclr |= UART_IEN_RX_FIFO_FULL | UART_IEN_BREAK_DETECT;

	serial_out(port, ARM_UART_IEN, ien);
	serial_out(port, ARM_UART_ICLR, iclr);
}

static void serial_sprd_enable_ms(struct uart_port *port)
{
}

static void serial_sprd_break_ctl(struct uart_port *port, int break_state)
{
}

static inline void serial_sprd_rx_chars(int irq, void *dev_id)
{
	struct uart_port *port = (struct uart_port *)dev_id;
	struct tty_port *tty = &port->state->port;
	unsigned int status, ch, flag, lsr, max_count = 2048;

	status = serial_in(port, ARM_UART_STS1);
	lsr = serial_in(port, ARM_UART_STS0);
	while ((status & 0x00ff) && max_count--) {
		ch = serial_in(port, ARM_UART_RXD);
		flag = TTY_NORMAL;
		port->icount.rx++;

		if (unlikely(lsr &
			     (UART_LSR_BI | UART_LSR_PE | UART_LSR_FE |
			      UART_LSR_OE))) {
			/*
			 *for statistics only
			 */
			if (lsr & UART_LSR_BI) {
				lsr &= ~(UART_LSR_FE | UART_LSR_PE);
				port->icount.brk++;
				/*
				 *we do the SysRQ and SAK checking here because otherwise the
				 *break may get masked by ignore_status_mask or read_status_mask
				 */
				if (uart_handle_break(port))
					goto ignore_char;
			} else if (lsr & UART_LSR_PE)
				port->icount.parity++;
			else if (lsr & UART_LSR_FE)
				port->icount.frame++;
			if (lsr & UART_LSR_OE)
				port->icount.overrun++;
			/*
			 *mask off conditions which should be ignored
			 */
			lsr &= port->read_status_mask;
			if (lsr & UART_LSR_BI)
				flag = TTY_BREAK;
			else if (lsr & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (lsr & UART_LSR_FE)
				flag = TTY_FRAME;
		}
		if (uart_handle_sysrq_char(port, ch))
			goto ignore_char;

		uart_insert_char(port, lsr, UART_LSR_OE, ch, flag);
ignore_char:
		status = serial_in(port, ARM_UART_STS1);
		lsr = serial_in(port, ARM_UART_STS0);
	}
	//tty->low_latency = 1;
	tty_flip_buffer_push(tty);
}

static inline void serial_sprd_tx_chars(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;
	struct circ_buf *xmit = &port->state->xmit;
	int count;
	unsigned long flags;

	if (port->x_char) {
		serial_out(port, ARM_UART_TXD, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
	spin_lock_irqsave(&port->lock, flags);
	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		serial_sprd_stop_tx(port);
		spin_unlock_irqrestore(&port->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&port->lock, flags);
	count = SP_TX_FIFO;
	do {
		serial_out(port, ARM_UART_TXD, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
		uart_write_wakeup(port);
	}
	spin_lock_irqsave(&port->lock, flags);
	if (uart_circ_empty(xmit)) {
		serial_sprd_stop_tx(port);
	}
	spin_unlock_irqrestore(&port->lock, flags);

}

/*
 *this handles the interrupt from one port
 */
static irqreturn_t serial_sprd_interrupt_chars(int irq, void *dev_id)
{
	struct uart_port *port = (struct uart_port *)dev_id;
	u32 int_status;

	int_status = serial_in(port, ARM_UART_STS2);

	serial_out(port, ARM_UART_ICLR, 0xffffffff);

	if (int_status &
	    (UART_STS_RX_FIFO_FULL | UART_STS_BREAK_DETECT |
	     UART_STS_TIMEOUT)) {
		serial_sprd_rx_chars(irq, port);
	}
	if (int_status & UART_STS_TX_FIFO_EMPTY) {
		serial_sprd_tx_chars(irq, port);
	}

	return IRQ_HANDLED;
}

#define SPRD_EICINT_BASE	(SPRD_EIC_BASE+0x80)
/*
 *this handles the interrupt from rx0 wakeup
 */
static irqreturn_t wakeup_rx_interrupt(int irq, void *dev_id)
{
	u32 val;

	//SIC polarity 1
	val = __raw_readl(SPRD_EICINT_BASE + 0x10);
	if ((val & BIT(0)) == BIT(0))
		val &= ~BIT(0);
	else
		val |= BIT(0);
	__raw_writel(val, SPRD_EICINT_BASE + 0x10);

	//clear interrupt
	val = __raw_readl(SPRD_EICINT_BASE + 0x0C);
	val |= BIT(0);
	__raw_writel(val, SPRD_EICINT_BASE + 0x0C);

	// set wakeup symbol
	is_uart_rx_wakeup = true;

	return IRQ_HANDLED;
}

/* FIXME: this pin config should be just defined int general pin mux table */
static void serial_sprd_pin_config(void)
{
#ifndef CONFIG_ARCH_SCX35
	value = __raw_readl(SPRD_GREG_BASE + 0x08);
	value |= 0x07 << 20;
	__raw_writel(value, SPRD_GREG_BASE + 0x08);
#endif
}

static int serial_sprd_startup(struct uart_port *port)
{
	int ret = 0;
	unsigned int ien, ctrl1;
	unsigned long flags;

	/* FIXME: don't know who change u0cts pin in 88 */
	serial_sprd_pin_config();

	/* set fifo water mark,tx_int_mark=8,rx_int_mark=1 */
#if 0				/* ? */
	serial_out(port, ARM_UART_CTL2, 0x801);
#endif

	serial_out(port, ARM_UART_CTL2, ((SP_TX_FIFO << 8) | SP_RX_FIFO));
	/* clear rx fifo */
	while (serial_in(port, ARM_UART_STS1) & 0x00ff) {
		serial_in(port, ARM_UART_RXD);
	}
	/* clear tx fifo */
	while (serial_in(port, ARM_UART_STS1) & 0xff00) ;
	/* clear interrupt */
	serial_out(port, ARM_UART_IEN, 0x00);
	serial_out(port, ARM_UART_ICLR, 0xffffffff);
	/* allocate irq */
	ret =
	    request_irq(port->irq, serial_sprd_interrupt_chars, IRQF_DISABLED,
			"serial", port);
	if (ret) {
		printk(KERN_ERR "fail to request serial irq %d\n", port->irq);
		free_irq(port->irq, port);
	}

	if (BT_RX_WAKE_UP == plat_data.wakeup_type) {
		int ret2 = 0;

		if (!port->line) {
			ret2 =
			    request_irq(IRQ_WAKEUP, wakeup_rx_interrupt,
					IRQF_SHARED, "wakeup_rx", port);
			if (ret2) {
				printk("fail to request wakeup irq\n");
				free_irq(IRQ_WAKEUP, NULL);
			}
		}
	}

	ctrl1 = serial_in(port, ARM_UART_CTL1);
	ctrl1 |= 0x3e00 | SP_RX_FIFO;
	serial_out(port, ARM_UART_CTL1, ctrl1);

	spin_lock_irqsave(&port->lock, flags);
	/* enable interrupt */
	ien = serial_in(port, ARM_UART_IEN);
	ien |=
	    UART_IEN_RX_FIFO_FULL | UART_IEN_TX_FIFO_EMPTY |
	    UART_IEN_BREAK_DETECT | UART_IEN_TIMEOUT;
	serial_out(port, ARM_UART_IEN, ien);

	spin_unlock_irqrestore(&port->lock, flags);

	return 0;
}

static void serial_sprd_shutdown(struct uart_port *port)
{
	serial_out(port, ARM_UART_IEN, 0x0);
	serial_out(port, ARM_UART_ICLR, 0xffffffff);
	free_irq(port->irq, port);
}

static void serial_sprd_set_termios(struct uart_port *port,
				    struct ktermios *termios,
				    struct ktermios *old)
{
	unsigned int baud, quot;
	unsigned int lcr, fc;
	/* ask the core to calculate the divisor for us */
	baud = uart_get_baud_rate(port, termios, old, 1200, 3000000);

	quot = (unsigned int)((port->uartclk + baud / 2) / baud);

	/* set data length */
	lcr = serial_in(port, ARM_UART_CTL0);
	lcr &= ~UART_DATA_BIT;
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		lcr |= UART_DATA_5BIT;
		break;
	case CS6:
		lcr |= UART_DATA_6BIT;
		break;
	case CS7:
		lcr |= UART_DATA_7BIT;
		break;
	default:
	case CS8:
		lcr |= UART_DATA_8BIT;
		break;
	}
	/* calculate stop bits */
	lcr &= ~(UART_STOP_1BIT | UART_STOP_2BIT);
	if (termios->c_cflag & CSTOPB)
		lcr |= UART_STOP_2BIT;
	else
		lcr |= UART_STOP_1BIT;
	/* calculate parity */
	lcr &= ~UART_PARITY;
	if (termios->c_cflag & PARENB) {
		lcr |= UART_PARITY_EN;
		if (termios->c_cflag & PARODD)
			lcr |= UART_ODD_PAR;
		else
			lcr |= UART_EVEN_PAR;
	}
	/* change the port state. */
	/* update the per-port timeout */
	uart_update_timeout(port, termios->c_cflag, baud);

	port->read_status_mask = UART_LSR_OE;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= UART_LSR_BI;
	/* characters to ignore */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		port->ignore_status_mask |= UART_LSR_BI;
		/* if we ignore parity and break indicators,ignore overruns too */
		if (termios->c_iflag & IGNPAR)
			port->ignore_status_mask |= UART_LSR_OE;
	}
	/* ignore all characters if CREAD is not set */
#if 0				/* ? */
	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |= UART_LSR_DR;
#endif
	/* flow control */
	fc = serial_in(port, ARM_UART_CTL1);
	fc &=
	    ~(RX_HW_FLOW_CTL_THRESHOLD | RX_HW_FLOW_CTL_EN | TX_HW_FLOW_CTL_EN);
	if (termios->c_cflag & CRTSCTS) {
		fc |= RX_HW_FLOW_CTL_THRESHOLD;
		fc |= RX_HW_FLOW_CTL_EN;
		fc |= TX_HW_FLOW_CTL_EN;
	}
	/* clock divider bit0~bit15 */
	serial_out(port, ARM_UART_CLKD0, quot & 0xffff);
	/* clock divider bit16~bit20 */
	serial_out(port, ARM_UART_CLKD1, (quot & 0x1f0000) >> 16);
	serial_out(port, ARM_UART_CTL0, lcr);
	fc |= 0x3e00 | SP_RX_FIFO;
	serial_out(port, ARM_UART_CTL1, fc);
}

static const char *serial_sprd_type(struct uart_port *port)
{
	return "SPX";
}

static void serial_sprd_release_port(struct uart_port *port)
{
}

static int serial_sprd_request_port(struct uart_port *port)
{
	return 0;
}

static void serial_sprd_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE && serial_sprd_request_port(port) == 0)
		port->type = PORT_SPRD;
}

static int serial_sprd_verify_port(struct uart_port *port,
				   struct serial_struct *ser)
{
	if (unlikely(ser->type != PORT_SPRD))
		return -EINVAL;
	if (unlikely(port->irq != ser->irq))
		return -EINVAL;
	return 0;
}

static struct uart_ops serial_sprd_ops = {
	.tx_empty = serial_sprd_tx_empty,
	.get_mctrl = serial_sprd_get_mctrl,
	.set_mctrl = serial_sprd_set_mctrl,
	.stop_tx = serial_sprd_stop_tx,
	.start_tx = serial_sprd_start_tx,
	.stop_rx = serial_sprd_stop_rx,
	.enable_ms = serial_sprd_enable_ms,
	.break_ctl = serial_sprd_break_ctl,
	.startup = serial_sprd_startup,
	.shutdown = serial_sprd_shutdown,
	.set_termios = serial_sprd_set_termios,
	.type = serial_sprd_type,
	.release_port = serial_sprd_release_port,
	.request_port = serial_sprd_request_port,
	.config_port = serial_sprd_config_port,
	.verify_port = serial_sprd_verify_port,
};
static struct uart_port *serial_sprd_ports[UART_NR_MAX] = { 0 };

static struct {
	uint32_t ien;
	uint32_t ctrl0;
	uint32_t ctrl1;
	uint32_t ctrl2;
	uint32_t clkd0;
	uint32_t clkd1;
	uint32_t dspwait;
} uart_bak[UART_NR_MAX];

static int clk_startup(struct platform_device *pdev)
{
	struct clk *clk;
	struct clk *clk_parent;
	char clk_name[10];
	int ret;
	int clksrc;
	struct serial_data plat_local_data;

	sprintf(clk_name, "clk_uart%d", pdev->id);
	clk = clk_get(NULL, clk_name);
	if (IS_ERR(clk)) {
		printk("clock[%s]: failed to get clock by clk_get()!\n",
		       clk_name);
		return -1;
	}

	plat_local_data = *(struct serial_data *)(pdev->dev.platform_data);
	clksrc = plat_local_data.clk;

	if (clksrc == 48000000) {
		clk_parent = clk_get(NULL, "clk_48m");
	} else {
		clk_parent = clk_get(NULL, "ext_26m");
	}
	if (IS_ERR(clk_parent)) {
		printk("clock[%s]: failed to get parent [%s] by clk_get()!\n",
		       clk_name, "clk_48m");
		return -1;
	}

	ret = clk_set_parent(clk, clk_parent);
	if (ret) {
		printk("clock[%s]: clk_set_parent() failed!\n", clk_name);
	}
	ret = clk_prepare_enable(clk);
	if (ret) {
		printk("clock[%s]: clk_enable() failed!\n", clk_name);
	}
	return 0;
}

static int serial_sprd_setup_port(struct platform_device *pdev,
				  struct resource *mem, struct resource *irq)
{
	struct serial_data plat_local_data;
	struct uart_port *up;
	struct device_node *np = pdev->dev.of_node;

	up = kzalloc(sizeof(*up), GFP_KERNEL);
	if (up == NULL) {
		return -ENOMEM;
	}

	up->line = pdev->id;
	up->type = PORT_SPRD;
	up->iotype = SERIAL_IO_PORT;
	up->membase = (void *)mem->start;
	up->mapbase = up->membase;

	plat_local_data = *(struct serial_data *)(pdev->dev.platform_data);
	up->uartclk = plat_local_data.clk;

	up->irq = irq->start;
	up->fifosize = 128;
	up->ops = &serial_sprd_ops;
	up->flags = ASYNC_BOOT_AUTOCONF;

	serial_sprd_ports[pdev->id] = up;

	clk_startup(pdev);
	return 0;
}

#ifdef CONFIG_SERIAL_SPRD_UART_CONSOLE
static inline void wait_for_xmitr(struct uart_port *port)
{
	unsigned int status, tmout = 10000;
	/* wait up to 10ms for the character(s) to be sent */
	do {
		status = serial_in(port, ARM_UART_STS1);
		if (--tmout == 0)
			break;
		udelay(1);
	} while (status & 0xff00);
}

static void serial_sprd_console_putchar(struct uart_port *port, int ch)
{
	wait_for_xmitr(port);
	serial_out(port, ARM_UART_TXD, ch);
}

static void serial_sprd_console_write(struct console *co, const char *s,
				      unsigned int count)
{
	struct uart_port *port = serial_sprd_ports[co->index];
	int ien;
	int locked = 1;
	unsigned long flags;

	if (oops_in_progress)
		locked = spin_trylock_irqsave(&port->lock, flags);
	else
		spin_lock_irqsave(&port->lock, flags);
	/*firstly,save the IEN register and disable the interrupts */
	ien = serial_in(port, ARM_UART_IEN);
	serial_out(port, ARM_UART_IEN, 0x0);

	uart_console_write(port, s, count, serial_sprd_console_putchar);
	/*finally,wait for  TXD FIFO to become empty and restore the IEN register */
	wait_for_xmitr(port);
	serial_out(port, ARM_UART_IEN, ien);
	if (locked)
		spin_unlock_irqrestore(&port->lock, flags);
}

static int __init serial_sprd_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	if (unlikely(co->index >= UART_NR_MAX || co->index < 0))
		co->index = 0;

	port = serial_sprd_ports[co->index];
	if (port == NULL) {
		printk(KERN_INFO "srial port %d not yet initialized\n",
		       co->index);
		return -ENODEV;
	}
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct uart_driver serial_sprd_reg;
static struct console serial_sprd_console = {
	.name = "ttyS",
	.write = serial_sprd_console_write,
	.device = uart_console_device,
	.setup = serial_sprd_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &serial_sprd_reg,
};

#define SPRD_CONSOLE		&serial_sprd_console

#else /* !CONFIG_SERIAL_SPRD_UART_CONSOLE */
#define SPRD_CONSOLE		NULL
#endif

static struct uart_driver serial_sprd_reg = {
	.owner = THIS_MODULE,
	.driver_name = "serial_sprd",
	.dev_name = SP_TTY_NAME,
	.major = SP_TTY_MAJOR,
	.minor = SP_TTY_MINOR_START,
	.nr = UART_NR_MAX,
	.cons = SPRD_CONSOLE,
};

static int serial_sprd_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *mem, *irq;
	struct device_node *np = pdev->dev.of_node;

	if (np)
		pdev->id = of_alias_get_id(np, "serial");

	if (unlikely(pdev->id < 0 || pdev->id >= UART_NR_MAX)) {
		dev_err(&pdev->dev, "does not support id %d\n", pdev->id);
		return -ENXIO;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!mem)) {
		dev_err(&pdev->dev, "not provide mem resource\n");
		return -ENODEV;
	}

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (unlikely(!irq)) {
		dev_err(&pdev->dev, "not provide irq resource\n");
		return -ENODEV;
	}
	if (np) {
		struct serial_data *plat_local_data;
		char *local_string;
		plat_local_data =
		    kmalloc(sizeof(struct serial_data), GFP_KERNEL);
		if (!plat_local_data) {
			dev_err(&pdev->dev,
				"fail to malloc memory for platform_data\n");
			return -ENOMEM;
		}
		ret =
		    of_property_read_u32(np, "sprdclk", &plat_local_data->clk);
		if (ret) {
			dev_err(&pdev->dev, "fail to get sprdclk\n");
			plat_local_data->clk = 26000000;
		}
		ret =
		    of_property_read_string(np, "sprdwaketype", &local_string);
		if (ret) {
			dev_err(&pdev->dev, "fail to get sprdwaketype\n");
			plat_local_data->wakeup_type = BT_RTS_HIGH_WHEN_SLEEP;
		} else {
			if (!strcmp(local_string, "BT_RTS_HIGH_WHEN_SLEEP"))
				plat_local_data->wakeup_type =
				    BT_RTS_HIGH_WHEN_SLEEP;
			else if (!strcmp(local_string, "BT_RX_WAKE_UP"))
				plat_local_data->wakeup_type = BT_RX_WAKE_UP;
			else if (!strcmp(local_string, "BT_NO_WAKE_UP"))
				plat_local_data->wakeup_type = BT_RX_WAKE_UP;
		}
		pdev->dev.platform_data = (void *)plat_local_data;
	}

	ret = serial_sprd_setup_port(pdev, mem, irq);
	if (unlikely(ret != 0)) {
		dev_err(&pdev->dev, "setup port failed\n");
		return ret;
	}
	ret = uart_add_one_port(&serial_sprd_reg, serial_sprd_ports[pdev->id]);
	if (likely(ret == 0)) {
		platform_set_drvdata(pdev, serial_sprd_ports[pdev->id]);
	}
	if (!((void *)(pdev->dev.platform_data))) {
		dev_err(&pdev->dev, "serial driver get platform data failed\n");
		return -ENODEV;
	}
	plat_data = *(struct serial_data *)(pdev->dev.platform_data);
	printk("bt host wake up type is %d, clk is %d \n",
	       plat_data.wakeup_type, plat_data.clk);
	if (BT_RX_WAKE_UP == plat_data.wakeup_type) {
		wake_lock_init(&uart_rx_lock, WAKE_LOCK_SUSPEND,
			       "uart_rx_lock");
	}

	return ret;
}

static int serial_sprd_remove(struct platform_device *dev)
{
	struct uart_port *up = platform_get_drvdata(dev);
	struct device_node *np = dev->dev.of_node;

	if (np) {
		kfree(dev->dev.platform_data);
	}
	platform_set_drvdata(dev, NULL);
	if (up) {
		uart_remove_one_port(&serial_sprd_reg, up);
		kfree(up);
		serial_sprd_ports[dev->id] = NULL;
	}
	return 0;
}

static int serial_sprd_suspend(struct platform_device *dev, pm_message_t state)
{
	/* TODO */
	int id = dev->id;
	struct uart_port *port;

	port = serial_sprd_ports[id];
	uart_bak[id].ien = serial_in(port, ARM_UART_IEN);
	uart_bak[id].ctrl0 = serial_in(port, ARM_UART_CTL0);
	uart_bak[id].ctrl1 = serial_in(port, ARM_UART_CTL1);
	uart_bak[id].ctrl2 = serial_in(port, ARM_UART_CTL2);
	uart_bak[id].clkd0 = serial_in(port, ARM_UART_CLKD0);
	uart_bak[id].clkd1 = serial_in(port, ARM_UART_CLKD1);

	if (BT_RX_WAKE_UP == plat_data.wakeup_type) {
		is_uart_rx_wakeup = false;
	} else if (BT_RTS_HIGH_WHEN_SLEEP == plat_data.wakeup_type) {
		/*when the uart0 going to sleep,config the RTS pin of hardware flow
		   control as the AF3 to make the pin can be set to high */
		unsigned long fc = 0;
		struct uart_port *port = serial_sprd_ports[0];
		pinmap_set(REG_PIN_U0RTS,
			(BIT_PIN_SLP_AP | BITS_PIN_DS(3) | BITS_PIN_AF(3) |
			BIT_PIN_WPU | BIT_PIN_SLP_WPU | BIT_PIN_SLP_OE));
#if defined(U0RTS_BT_CS_GPIO)
		gpio_direction_output(U0RTS_BT_CS_GPIO, 1);
#endif
		fc = serial_in(port, ARM_UART_CTL1);
		fc &= ~(RX_HW_FLOW_CTL_EN | TX_HW_FLOW_CTL_EN);
		serial_out(port, ARM_UART_CTL1, fc);
	} else {
		pr_debug("BT host wake up feature has not been supported\n");
	}

	return 0;
}

static int serial_sprd_resume(struct platform_device *dev)
{
	/* TODO */
	int id = dev->id;
	struct uart_port *port = serial_sprd_ports[id];

	port = serial_sprd_ports[id];
	serial_out(port, ARM_UART_CTL0, uart_bak[id].ctrl0);
	serial_out(port, ARM_UART_CTL1, uart_bak[id].ctrl1);
	serial_out(port, ARM_UART_CTL2, uart_bak[id].ctrl2);
	serial_out(port, ARM_UART_CLKD0, uart_bak[id].clkd0);
	serial_out(port, ARM_UART_CLKD1, uart_bak[id].clkd1);
	serial_out(port, ARM_UART_IEN, uart_bak[id].ien);

	if (BT_RX_WAKE_UP == plat_data.wakeup_type) {
		if (is_uart_rx_wakeup) {
			is_uart_rx_wakeup = false;
			wake_lock_timeout(&uart_rx_lock, HZ / 5);	// 0.2s
		}
	} else if (BT_RTS_HIGH_WHEN_SLEEP == plat_data.wakeup_type) {
		/*when the uart0 waking up,reconfig the RTS pin of hardware flow control work
		   in the hardware flow control mode to make the pin can be controlled by
		   hardware */
		unsigned long fc = 0;
		struct uart_port *port = serial_sprd_ports[0];
		fc = serial_in(port, ARM_UART_CTL1);
		fc |= (RX_HW_FLOW_CTL_EN | TX_HW_FLOW_CTL_EN);
		serial_out(port, ARM_UART_CTL1, fc);
		pinmap_set(REG_PIN_U0RTS,
		(BIT_PIN_SLP_AP | BITS_PIN_DS(1) | BITS_PIN_AF(0) |
		BIT_PIN_NUL | BIT_PIN_SLP_NUL | BIT_PIN_SLP_Z));
	} else {
		pr_debug("BT host wake up feature has not been supported\n");
	}

	return 0;
}

static const struct of_device_id serial_ids[] = {
	{.compatible = "sprd,serial",},
	{}
};

static struct platform_driver serial_sprd_driver = {
	.probe = serial_sprd_probe,
	.remove = serial_sprd_remove,
	.suspend = serial_sprd_suspend,
	.resume = serial_sprd_resume,
	.driver = {
		   .name = "serial_sprd",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(serial_ids),
		   },
};

static int __init serial_sprd_init(void)
{
	int ret = 0;

	ret = uart_register_driver(&serial_sprd_reg);
	if (unlikely(ret != 0))
		return ret;

	ret = platform_driver_register(&serial_sprd_driver);
	if (unlikely(ret != 0))
		uart_unregister_driver(&serial_sprd_reg);

	return ret;
}

static void __exit serial_sprd_exit(void)
{
	platform_driver_unregister(&serial_sprd_driver);
	uart_unregister_driver(&serial_sprd_reg);
}

module_init(serial_sprd_init);
module_exit(serial_sprd_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sprd serial driver $Revision:1.0$");
