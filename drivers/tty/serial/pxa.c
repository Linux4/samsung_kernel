/*
 *  Based on drivers/serial/8250.c by Russell King.
 *
 *  Author:	Nicolas Pitre
 *  Created:	Feb 20, 2003
 *  Copyright:	(C) 2003 Monta Vista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Note 1: This driver is made separate from the already too overloaded
 * 8250.c because it needs some kirks of its own and that'll make it
 * easier to add DMA support.
 *
 * Note 2: I'm too sick of device allocation policies for serial ports.
 * If someone else wants to request an "official" allocation of major/minor
 * for this driver please be my guest.  And don't forget that new hardware
 * to come from Intel might have more than 3 or 4 of those UARTs.  Let's
 * hope for a better port registration and dynamic device allocation scheme
 * with the serial core maintainer satisfaction to appear soon.
 */


#if defined(CONFIG_SERIAL_PXA_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/serial_reg.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/wakelock.h>
#include <plat/pm.h>
#include <mach/dma.h>

#define	DMA_BLOCK	UART_XMIT_SIZE

#define PXA_UART_TX	0
#define PXA_UART_RX	1

#define PXA_NAME_LEN		8

struct uart_pxa_port {
	struct uart_port        port;
	unsigned int            ier;
	unsigned char           lcr;
	unsigned int            mcr;
	unsigned int            lsr_break_flag;
	struct clk		*clk;
	char			name[PXA_NAME_LEN];
	struct timer_list	pxa_timer;

#define TX_DMA_RUNNING		(1 << 0)
#define RX_DMA_RUNNING		(1 << 1)
	unsigned int		dma_status;
	struct work_struct	uart_tx_lpm_work;
	struct pm_qos_request	qos_idle[2];
	int			txdma;
	int			rxdma;
	void			*txdma_addr;
	void			*rxdma_addr;
	dma_addr_t		txdma_addr_phys;
	dma_addr_t		rxdma_addr_phys;
	int			dma_enable;
	int			tx_stop;
	int			rx_stop;
	int			data_len;
	volatile unsigned int	*txdrcmr;
	volatile unsigned int	*rxdrcmr;
	struct	tasklet_struct	tklet;
#ifdef	CONFIG_PM
	/* We needn't save rx dma register because we
	 * just restart the dma totallly after resume
	 */
	void			*buf_save;
	unsigned long		dcsr_tx;
	unsigned long		dsadr_tx;
	unsigned long		dtadr_tx;
	unsigned long		dcmd_tx;
#endif
};

static int uart_dma;

static int __init uart_dma_setup(char *__unused)
{
	uart_dma = 1;
	return 1;
}
__setup("uart_dma", uart_dma_setup);

static inline void stop_dma(struct uart_pxa_port *up, int read)
{
	unsigned long flags;
	unsigned int channel, dcsr;
	int timeout = 10000;

	channel = read ? up->rxdma : up->txdma;
	dcsr = DCSR(channel);
	if (!(dcsr & DCSR_RUN))
		return;

	DCSR(channel) = dcsr & ~DCSR_RUN;

	/* if have DMA reqeust, wait. */
	while (!(DCSR(channel) & DCSR_STOPSTATE) && (timeout-- > 0))
		rmb();

	BUG_ON(timeout <= 0);
	spin_lock_irqsave(&up->port.lock, flags);
	if (read)
		up->dma_status &= ~RX_DMA_RUNNING;
	else
		up->dma_status &= ~TX_DMA_RUNNING;
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void pxa_uart_transmit_dma(int channel, void *data);
static void pxa_uart_receive_dma(int channel, void *data);
static void pxa_uart_transmit_dma_start(struct uart_pxa_port *up, int count);
static void pxa_uart_receive_dma_start(struct uart_pxa_port *up);
static inline void wait_for_xmitr(struct uart_pxa_port *up);
static inline void serial_out(struct uart_pxa_port *up, int offset, int value);

static unsigned int serial_pxa_tx_empty(struct uart_port *port);

#define PXA_TIMER_TIMEOUT (3*HZ)

static inline unsigned int serial_in(struct uart_pxa_port *up, int offset)
{
	offset <<= 2;
	return readl(up->port.membase + offset);
}

static inline void serial_out(struct uart_pxa_port *up, int offset, int value)
{
	offset <<= 2;
	writel(value, up->port.membase + offset);
}

static void serial_pxa_enable_ms(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;

	if (up->dma_enable)
		return;

	up->ier |= UART_IER_MSI;
	serial_out(up, UART_IER, up->ier);
}

static void serial_pxa_stop_tx(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	unsigned int timeout = 0x100000;

	if (up->dma_enable) {
		up->tx_stop = 1;

		if (up->ier & UART_IER_DMAE) {
			/*
			 * Here we still need DCSR as the judgment of
			 * whether dma has been transfer completed
			 * As there when this function is being caled,
			 * it would hold a spinlock and possible with irqsave,
			 * If this function is being called over core0,
			 * we may cannot get status change from the flag,
			 * As irq handler is being blocked to be called yet.
			 */
			while (!(DCSR(up->txdma) & DCSR_STOPSTATE)
					&& (timeout-- > 0))
				rmb();

			BUG_ON(timeout == 0);
		}
	} else {
		if (up->ier & UART_IER_THRI) {
			up->ier &= ~UART_IER_THRI;
			serial_out(up, UART_IER, up->ier);
		}
	}
}

static void serial_pxa_stop_rx(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;

	if (up->dma_enable) {
		if (up->ier & UART_IER_DMAE) {
			spin_unlock(&up->port.lock);
			stop_dma(up, 1);
			spin_lock(&up->port.lock);
		}
		up->rx_stop = 1;
	} else {
		up->ier &= ~UART_IER_RLSI;
		up->port.read_status_mask &= ~UART_LSR_DR;
		serial_out(up, UART_IER, up->ier);
	}
}

static inline void receive_chars(struct uart_pxa_port *up, int *status)
{
	struct tty_struct *tty = up->port.state->port.tty;
	unsigned int ch, flag;
	int max_count = 256;

	do {
		/* work around Errata #20 according to
		 * Intel(R) PXA27x Processor Family
		 * Specification Update (May 2005)
		 *
		 * Step 2
		 * Disable the Reciever Time Out Interrupt via IER[RTOEI]
		 */
		spin_lock(&up->port.lock);
		up->ier &= ~UART_IER_RTOIE;
		serial_out(up, UART_IER, up->ier);
		spin_unlock(&up->port.lock);

		ch = serial_in(up, UART_RX);
		flag = TTY_NORMAL;
		up->port.icount.rx++;

		if (unlikely(*status & (UART_LSR_BI | UART_LSR_PE |
				       UART_LSR_FE | UART_LSR_OE))) {
			/*
			 * For statistics only
			 */
			if (*status & UART_LSR_BI) {
				*status &= ~(UART_LSR_FE | UART_LSR_PE);
				up->port.icount.brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(&up->port))
					goto ignore_char;
			} else if (*status & UART_LSR_PE)
				up->port.icount.parity++;
			else if (*status & UART_LSR_FE)
				up->port.icount.frame++;
			if (*status & UART_LSR_OE)
				up->port.icount.overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			*status &= up->port.read_status_mask;

#ifdef CONFIG_SERIAL_PXA_CONSOLE
			if (up->port.line == up->port.cons->index) {
				/* Recover the break flag from console xmit */
				*status |= up->lsr_break_flag;
				up->lsr_break_flag = 0;
			}
#endif
			if (*status & UART_LSR_BI) {
				flag = TTY_BREAK;
			} else if (*status & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (*status & UART_LSR_FE)
				flag = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(&up->port, ch))
			goto ignore_char;

		uart_insert_char(&up->port, *status, UART_LSR_OE, ch, flag);

	ignore_char:
		*status = serial_in(up, UART_LSR);
	} while ((*status & UART_LSR_DR) && (max_count-- > 0));
	tty_flip_buffer_push(tty);

	/* work around Errata #20 according to
	 * Intel(R) PXA27x Processor Family
	 * Specification Update (May 2005)
	 *
	 * Step 6:
	 * No more data in FIFO: Re-enable RTO interrupt via IER[RTOIE]
	 */
	spin_lock(&up->port.lock);
	up->ier |= UART_IER_RTOIE;
	serial_out(up, UART_IER, up->ier);
	spin_unlock(&up->port.lock);
}

static void transmit_chars(struct uart_pxa_port *up)
{
	struct circ_buf *xmit = &up->port.state->xmit;
	int count;

	if (up->port.x_char) {
		serial_out(up, UART_TX, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(&up->port)) {
		spin_lock(&up->port.lock);
		serial_pxa_stop_tx(&up->port);
		spin_unlock(&up->port.lock);
		return;
	}

	count = up->port.fifosize / 2;
	do {
		serial_out(up, UART_TX, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		up->port.icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);


	if (uart_circ_empty(xmit)) {
		spin_lock(&up->port.lock);
		serial_pxa_stop_tx(&up->port);
		spin_unlock(&up->port.lock);
	}
}

static inline void
dma_receive_chars(struct uart_pxa_port *up, int *status)
{
	struct tty_struct *tty = up->port.state->port.tty;
	unsigned char ch;
	int max_count = 256;
	int count = 0;
	unsigned char *tmp;
	unsigned int flag = TTY_NORMAL;

	stop_dma(up, 1);
	/*
	 * deal with the bytes received by DMA
	 */
	count = DTADR(up->rxdma) - up->rxdma_addr_phys;
	tmp = up->rxdma_addr;
	if (up->port.sysrq) {
		while (count > 0) {
			if (!uart_handle_sysrq_char(&up->port, *tmp)) {
				uart_insert_char(&up->port, *status,
						 0, *tmp, flag);
				up->port.icount.rx++;
			}
			tmp++;
			count--;
		}
	} else {
		tty_insert_flip_string(tty, tmp, count);
		up->port.icount.rx += count;
	}

	/* deal with the bytes in rx FIFO */
	do {
		ch = serial_in(up, UART_RX);
		flag = TTY_NORMAL;
		up->port.icount.rx++;

		if (unlikely(*status & (UART_LSR_BI | UART_LSR_PE |
					UART_LSR_FE | UART_LSR_OE))) {
			/*
			 * For statistics only
			 */
			if (*status & UART_LSR_BI) {
				*status &= ~(UART_LSR_FE | UART_LSR_PE);
				up->port.icount.brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(&up->port))
					goto ignore_char2;
			} else if (*status & UART_LSR_PE)
				up->port.icount.parity++;
			else if (*status & UART_LSR_FE)
				up->port.icount.frame++;
			if (*status & UART_LSR_OE)
				up->port.icount.overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			*status &= up->port.read_status_mask;

#ifdef CONFIG_SERIAL_PXA_CONSOLE
			if (up->port.line == up->port.cons->index) {
				/* Recover the break flag from console xmit */
				*status |= up->lsr_break_flag;
				up->lsr_break_flag = 0;
			}
#endif
			if (*status & UART_LSR_BI)
				flag = TTY_BREAK;
			else if (*status & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (*status & UART_LSR_FE)
				flag = TTY_FRAME;
		}
		if (!uart_handle_sysrq_char(&up->port, ch))
			uart_insert_char(&up->port, *status, UART_LSR_OE,
					 ch, flag);
ignore_char2:
		*status = serial_in(up, UART_LSR);
	} while ((*status & UART_LSR_DR) && (max_count-- > 0));

	tty_schedule_flip(tty);

	if (up->rx_stop)
		return;
	pxa_uart_receive_dma_start(up);
}

static void serial_pxa_start_tx(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;

	if (up->dma_enable) {
		up->tx_stop = 0;
		tasklet_schedule(&up->tklet);
	} else {
		if (!(up->ier & UART_IER_THRI)) {
			up->ier |= UART_IER_THRI;
			serial_out(up, UART_IER, up->ier);
		}
	}
}

static inline void check_modem_status(struct uart_pxa_port *up)
{
	int status;

	status = serial_in(up, UART_MSR);

	if ((status & UART_MSR_ANY_DELTA) == 0)
		return;

	if (status & UART_MSR_TERI)
		up->port.icount.rng++;
	if (status & UART_MSR_DDSR)
		up->port.icount.dsr++;
	if (status & UART_MSR_DDCD)
		uart_handle_dcd_change(&up->port, status & UART_MSR_DCD);
	if (status & UART_MSR_DCTS)
		uart_handle_cts_change(&up->port, status & UART_MSR_CTS);

	wake_up_interruptible(&up->port.state->port.delta_msr_wait);
}

/*
 * This handles the interrupt from one port.
 */
static inline irqreturn_t serial_pxa_irq(int irq, void *dev_id)
{
	struct uart_pxa_port *up = dev_id;
	unsigned int iir, lsr;

	/*
	 * If FCR[4] is set, we may receive EOC interrupt when:
	 * 1) current descritor of DMA finishes successfully;
	 * 2) there are still trailing bytes in UART FIFO.
	 * This interrupt alway comes along with UART_IIR_NO_INT.
	 * So this interrupt is just ignored by us.
	 */
	iir = serial_in(up, UART_IIR);
	if (iir & UART_IIR_NO_INT)
		return IRQ_NONE;

	/* timer is not active */
	if (!mod_timer(&up->pxa_timer, jiffies + PXA_TIMER_TIMEOUT))
		pm_qos_update_request(&up->qos_idle[PXA_UART_RX],
				PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);

	lsr = serial_in(up, UART_LSR);
	if (up->dma_enable) {
		/* we only need to deal with FIFOE here */
		if (lsr & UART_LSR_FIFOE)
			dma_receive_chars(up, &lsr);
	} else {
		if (lsr & UART_LSR_DR)
			receive_chars(up, &lsr);

		check_modem_status(up);
		if (lsr & UART_LSR_THRE) {
			transmit_chars(up);
			/* wait Tx empty */
			while (!serial_pxa_tx_empty(	\
						(struct uart_port *)dev_id))
				;
		}
	}

	return IRQ_HANDLED;
}

static unsigned int serial_pxa_tx_empty(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&up->port.lock, flags);

	if (up->dma_enable) {
		if (up->ier & UART_IER_DMAE) {
			if (up->dma_status & TX_DMA_RUNNING) {
				spin_unlock_irqrestore(&up->port.lock, flags);
				return 0;
			}
		}
	}

	ret = serial_in(up, UART_LSR) & UART_LSR_TEMT ? TIOCSER_TEMT : 0;
	spin_unlock_irqrestore(&up->port.lock, flags);

	return ret;
}

static unsigned int serial_pxa_get_mctrl(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	unsigned char status;
	unsigned int ret;

	status = serial_in(up, UART_MSR);

	ret = 0;
	if (status & UART_MSR_DCD)
		ret |= TIOCM_CAR;
	if (status & UART_MSR_RI)
		ret |= TIOCM_RNG;
	if (status & UART_MSR_DSR)
		ret |= TIOCM_DSR;
	if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;
	return ret;
}

static void serial_pxa_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	unsigned int mcr = 0;

	if (mctrl & TIOCM_RTS)
		mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		mcr |= UART_MCR_LOOP;

	mcr |= up->mcr;

	serial_out(up, UART_MCR, mcr);
}

static void serial_pxa_break_ctl(struct uart_port *port, int break_state)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	unsigned long flags;

	spin_lock_irqsave(&up->port.lock, flags);
	if (break_state == -1)
		up->lcr |= UART_LCR_SBC;
	else
		up->lcr &= ~UART_LCR_SBC;
	serial_out(up, UART_LCR, up->lcr);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void pxa_uart_transmit_dma_start(struct uart_pxa_port *up, int count)
{
	DCSR(up->txdma)  = DCSR_NODESC;
	DSADR(up->txdma) = up->txdma_addr_phys;
	DTADR(up->txdma) = up->port.mapbase;
	DCMD(up->txdma) = DCMD_INCSRCADDR | DCMD_FLOWTRG | DCMD_ENDIRQEN |
		DCMD_WIDTH1 | DCMD_BURST16 | count;

	pm_qos_update_request(&up->qos_idle[PXA_UART_TX],
			PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);

	DCSR(up->txdma) |= DCSR_RUN;
}

static void pxa_uart_receive_dma_start(struct uart_pxa_port *up)
{
	unsigned long flags;

	spin_lock_irqsave(&up->port.lock, flags);
	if (up->dma_status & RX_DMA_RUNNING) {
		spin_unlock_irqrestore(&up->port.lock, flags);
		return;
	}
	up->dma_status |= RX_DMA_RUNNING;
	spin_unlock_irqrestore(&up->port.lock, flags);

	DCSR(up->rxdma)  = DCSR_NODESC | DCSR_EORIRQEN | DCSR_EORSTOPEN;
	DSADR(up->rxdma) = up->port.mapbase;
	DTADR(up->rxdma) = up->rxdma_addr_phys;
	DCMD(up->rxdma) = DCMD_INCTRGADDR | DCMD_FLOWSRC | DCMD_ENDIRQEN |
		DCMD_WIDTH1 | DCMD_BURST16 | DMA_BLOCK;
	DCSR(up->rxdma) |= DCSR_RUN;
}

static void pxa_uart_receive_dma(int channel, void *data)
{
	volatile unsigned long dcsr;
	unsigned long flags;
	struct uart_pxa_port *up = (struct uart_pxa_port *)data;
	struct tty_struct *tty = up->port.state->port.tty;
	unsigned int count;
	unsigned char *tmp = up->rxdma_addr;

	if (!mod_timer(&up->pxa_timer, jiffies + PXA_TIMER_TIMEOUT))
		pm_qos_update_request(&up->qos_idle[PXA_UART_RX],
				PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
	dcsr = DCSR(channel);

	if (dcsr & DCSR_ENDINTR)
		DCSR(channel) |= DCSR_ENDINTR;
	if (dcsr & DCSR_EORINTR)
		DCSR(channel) |= DCSR_EORINTR;
	if (dcsr & DCSR_BUSERR) {
		DCSR(channel) |= DCSR_BUSERR;
		printk(KERN_ALERT "%s(): DMA channel bus error\n", __func__);
	}

	count = DTADR(channel) - up->rxdma_addr_phys;
	if (up->port.sysrq) {
		while (count > 0) {
			if (!uart_handle_sysrq_char(&up->port, *tmp)) {
				tty_insert_flip_char(tty, *tmp, TTY_NORMAL);
				up->port.icount.rx++;
			}
			tmp++;
			count--;
		}
	} else {
		tty_insert_flip_string(tty, tmp, count);
		up->port.icount.rx += count;
	}
	tty_flip_buffer_push(tty);

	spin_lock_irqsave(&up->port.lock, flags);
	/*
	 * DMA_RUNNING flag should be clear only after
	 * all dma interface operation completed
	 */
	up->dma_status &= ~RX_DMA_RUNNING;
	spin_unlock_irqrestore(&up->port.lock, flags);

	if (up->rx_stop)
		return;
	pxa_uart_receive_dma_start(up);
	return;
}

static void pxa_uart_transmit_dma(int channel, void *data)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)data;
	struct circ_buf *xmit = &up->port.state->xmit;
	volatile unsigned long dcsr;

	dcsr = DCSR(channel);

	if (dcsr & DCSR_BUSERR) {
		DCSR(channel) |= DCSR_BUSERR;
		printk(KERN_ALERT "%s(): DMA channel bus error\n", __func__);
	}

	if (dcsr & DCSR_STOPSTATE)
		schedule_work(&up->uart_tx_lpm_work);

	if (dcsr & DCSR_ENDINTR)
		DCSR(channel) |= DCSR_ENDINTR;

	spin_lock(&up->port.lock);
	/*
	 * DMA_RUNNING flag should be clear only after
	 * all dma interface operation completed
	 */
	up->dma_status &= ~TX_DMA_RUNNING;
	spin_unlock(&up->port.lock);

	/* if tx stop, stop transmit DMA and return */
	if (up->tx_stop)
		return;

	if (up->port.x_char) {
		serial_out(up, UART_TX, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);

	if (!uart_circ_empty(xmit))
		tasklet_schedule(&up->tklet);
	return;
}

static void uart_pxa_dma_init(struct uart_pxa_port *up)
{

	if (0 == up->rxdma) {
		up->rxdma = pxa_request_dma(up->name, DMA_PRIO_LOW,
				pxa_uart_receive_dma, up);
		if (up->rxdma < 0)
			goto out;
	}

	if (0 == up->txdma) {
		up->txdma = pxa_request_dma(up->name, DMA_PRIO_LOW,
				pxa_uart_transmit_dma, up);
		if (up->txdma < 0)
			goto err_txdma;
	}

	if (NULL == up->txdma_addr) {
		up->txdma_addr = dma_alloc_coherent(NULL, DMA_BLOCK,
				&up->txdma_addr_phys, GFP_KERNEL);
		if (!up->txdma_addr)
			goto txdma_err_alloc;
	}

	if (NULL == up->rxdma_addr) {
		up->rxdma_addr = dma_alloc_coherent(NULL, DMA_BLOCK,
				&up->rxdma_addr_phys, GFP_KERNEL);
		if (!up->rxdma_addr)
			goto rxdma_err_alloc;
	}

#ifdef CONFIG_PM
	up->buf_save = kmalloc(DMA_BLOCK, GFP_KERNEL);
	if (!up->buf_save)
		goto buf_err_alloc;
#endif

	up->dma_status = 0;
	writel(up->rxdma | DRCMR_MAPVLD, up->rxdrcmr);
	writel(up->txdma | DRCMR_MAPVLD, up->txdrcmr);
	DALGN |= (0x1 << up->rxdma);

	return;

#ifdef CONFIG_PM
buf_err_alloc:
	dma_free_coherent(NULL, DMA_BLOCK, up->rxdma_addr,
			up->rxdma_addr_phys);
	up->rxdma_addr = NULL;
#endif
rxdma_err_alloc:
	dma_free_coherent(NULL, DMA_BLOCK, up->txdma_addr,
			up->txdma_addr_phys);
	up->txdma_addr = NULL;
txdma_err_alloc:
	pxa_free_dma(up->txdma);
	up->txdma = 0;
err_txdma:
	pxa_free_dma(up->rxdma);
	up->rxdma = 0;
out:
	return;
}

static void uart_pxa_dma_uninit(struct uart_pxa_port *up)
{
#ifdef CONFIG_PM
	kfree(up->buf_save);
#endif

	stop_dma(up, 0);
	stop_dma(up, 1);
	if (up->txdma_addr != NULL) {
		dma_free_coherent(NULL, DMA_BLOCK, up->txdma_addr,
				up->txdma_addr_phys);
		up->txdma_addr = NULL;
	}
	if (up->txdma != 0) {
		pxa_free_dma(up->txdma);
		writel(0, up->txdrcmr);
		up->txdma = 0;
	}

	if (up->rxdma_addr != NULL) {
		dma_free_coherent(NULL, DMA_BLOCK, up->rxdma_addr,
				up->rxdma_addr_phys);
		up->rxdma_addr = NULL;
	}

	if (up->rxdma != 0) {
		pxa_free_dma(up->rxdma);
		DALGN &= ~(0x1 << up->rxdma);
		writel(0, up->rxdrcmr);
		up->rxdma = 0;
	}

	return;
}

static void uart_task_action(unsigned long data)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)data;
	struct circ_buf *xmit = &up->port.state->xmit;
	unsigned char *tmp = up->txdma_addr;
	unsigned long flags;
	int count = 0, c;

	/* if the tx is stop, just return.*/
	if (up->tx_stop)
		return;

	spin_lock_irqsave(&up->port.lock, flags);
	if (up->dma_status & TX_DMA_RUNNING) {
		spin_unlock_irqrestore(&up->port.lock, flags);
		return;
	}

	up->dma_status |= TX_DMA_RUNNING;
	while (1) {
		c = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		if (c <= 0)
			break;

		memcpy(tmp, xmit->buf + xmit->tail, c);
		xmit->tail = (xmit->tail + c) & (UART_XMIT_SIZE - 1);
		tmp += c;
		count += c;
		up->port.icount.tx += c;
	}
	spin_unlock_irqrestore(&up->port.lock, flags);

	tmp = up->txdma_addr;
	up->tx_stop = 0;

	pr_debug("count =%d", count);
	pxa_uart_transmit_dma_start(up, count);
}

static int serial_pxa_startup(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	unsigned long flags;
	int retval;

	if (port->line == 3) /* HWUART */
		up->mcr |= UART_MCR_AFE;
	else
		up->mcr = 0;

	up->port.uartclk = clk_get_rate(up->clk);

	/*
	 * Allocate the IRQ
	 */
	retval = request_irq(up->port.irq, serial_pxa_irq, 0, up->name, up);
	if (retval)
		return retval;

	/*
	 * Clear the FIFO buffers and disable them.
	 * (they will be reenabled in set_termios())
	 */
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
			UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);

	/*
	 * Clear the interrupt registers.
	 */
	(void) serial_in(up, UART_LSR);
	(void) serial_in(up, UART_RX);
	(void) serial_in(up, UART_IIR);
	(void) serial_in(up, UART_MSR);

	/*
	 * Now, initialize the UART
	 */
	serial_out(up, UART_LCR, UART_LCR_WLEN8);

	spin_lock_irqsave(&up->port.lock, flags);
	up->port.mctrl |= TIOCM_OUT2;
	serial_pxa_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Finally, enable interrupts.  Note: Modem status interrupts
	 * are set via set_termios(), which will be occurring imminently
	 * anyway, so we don't enable them here.
	 */
	if (up->dma_enable) {
		uart_pxa_dma_init(up);
		up->rx_stop = 0;
		pxa_uart_receive_dma_start(up);
		tasklet_init(&up->tklet, uart_task_action, (unsigned long)up);
	}

	spin_lock_irqsave(&up->port.lock, flags);
	if (up->dma_enable) {
		up->ier = UART_IER_DMAE | UART_IER_UUE;
	} else {
		up->ier = UART_IER_RLSI | UART_IER_RDI |
			UART_IER_RTOIE | UART_IER_UUE;
	}
	serial_out(up, UART_IER, up->ier);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * And clear the interrupt registers again for luck.
	 */
	(void) serial_in(up, UART_LSR);
	(void) serial_in(up, UART_RX);
	(void) serial_in(up, UART_IIR);
	(void) serial_in(up, UART_MSR);

	return 0;
}

static void serial_pxa_shutdown(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	unsigned long flags;

	flush_work(&up->uart_tx_lpm_work);

	free_irq(up->port.irq, up);

	if (up->dma_enable) {
		tasklet_kill(&up->tklet);
		uart_pxa_dma_uninit(up);
	}

	/*
	 * Disable interrupts from this port
	 */
	spin_lock_irqsave(&up->port.lock, flags);
	up->ier = 0;
	serial_out(up, UART_IER, 0);

	up->port.mctrl &= ~TIOCM_OUT2;
	serial_pxa_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Disable break condition and FIFOs
	 */
	serial_out(up, UART_LCR, serial_in(up, UART_LCR) & ~UART_LCR_SBC);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
				  UART_FCR_CLEAR_RCVR |
				  UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);
}

static void
serial_pxa_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	unsigned char cval, fcr = 0;
	unsigned long flags;
	unsigned int baud, quot = 0;
	unsigned int dll;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		cval = UART_LCR_WLEN5;
		break;
	case CS6:
		cval = UART_LCR_WLEN6;
		break;
	case CS7:
		cval = UART_LCR_WLEN7;
		break;
	default:
	case CS8:
		cval = UART_LCR_WLEN8;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		cval |= UART_LCR_STOP;
	if (termios->c_cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;

	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old, 0, 921600*16*4/16);
	if (baud > 921600) {
		port->uartclk = 921600*16*4; /* 58.9823MHz as the clk src */
		if (B1500000 == (termios->c_cflag & B1500000))
			quot = 2;
		if (B3500000 == (termios->c_cflag & B3500000))
			quot = 1;
		if (quot == 0)
			quot = uart_get_divisor(port, baud);
	} else {
		quot = uart_get_divisor(port, baud);
	}

	if (up->dma_enable) {
		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_PXAR32 |
					     UART_FCR_PXA_TRAIL;
		fcr &= ~UART_FCR_PXA_BUS32;
	} else {
		if ((up->port.uartclk / quot) < (2400 * 16))
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_PXAR1;
		else if ((up->port.uartclk / quot) < (230400 * 16))
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_PXAR8;
		else
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_PXAR32;
	}

	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&up->port.lock, flags);
	if (baud > 921600)
		up->ier |= UART_IER_HSE;
	else
		up->ier &= ~UART_IER_HSE;

	/*
	 * Ensure the port will be enabled.
	 * This is required especially for serial console.
	 */
	up->ier |= UART_IER_UUE;

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	up->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		up->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		up->port.read_status_mask |= UART_LSR_BI;

	/*
	 * Characters to ignore
	 */
	up->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		up->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		up->port.ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			up->port.ignore_status_mask |= UART_LSR_OE;
	}

	/*
	 * ignore all characters if CREAD is not set
	 */
	if ((termios->c_cflag & CREAD) == 0)
		up->port.ignore_status_mask |= UART_LSR_DR;

	/*
	 * CTS flow control flag and modem status interrupts
	 */

	if (up->dma_enable) {
		if (termios->c_cflag & CRTSCTS)
			up->mcr |= UART_MCR_AFE;
		else
			up->mcr &= UART_MCR_AFE;
	} else {
		up->ier &= ~UART_IER_MSI;
		if (UART_ENABLE_MS(&up->port, termios->c_cflag))
			up->ier |= UART_IER_MSI;
	}

	serial_out(up, UART_IER, up->ier);

	if (termios->c_cflag & CRTSCTS)
		up->mcr |= UART_MCR_AFE;
	else
		up->mcr &= ~UART_MCR_AFE;

	serial_out(up, UART_LCR, cval | UART_LCR_DLAB);	/* set DLAB */
	serial_out(up, UART_DLL, quot & 0xff);		/* LS of divisor */

	/*
	 * work around Errata #75 according to Intel(R) PXA27x Processor Family
	 * Specification Update (Nov 2005)
	 */
	dll = serial_in(up, UART_DLL);
	WARN_ON(dll != (quot & 0xff));

	serial_out(up, UART_DLM, quot >> 8);		/* MS of divisor */
	serial_out(up, UART_LCR, cval);			/* reset DLAB */
	up->lcr = cval;					/* Save LCR */
	serial_pxa_set_mctrl(&up->port, up->port.mctrl);
	serial_out(up, UART_FCR, fcr);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void
serial_pxa_pm(struct uart_port *port, unsigned int state,
	      unsigned int oldstate)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;

	if (!state)
		clk_prepare_enable(up->clk);
	else
		clk_disable_unprepare(up->clk);
}

static void serial_pxa_release_port(struct uart_port *port)
{
}

static int serial_pxa_request_port(struct uart_port *port)
{
	return 0;
}

static void serial_pxa_config_port(struct uart_port *port, int flags)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	up->port.type = PORT_PXA;
}

static int
serial_pxa_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* we don't want the core code to modify any port params */
	return -EINVAL;
}

static const char *
serial_pxa_type(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	return up->name;
}

static struct uart_pxa_port *serial_pxa_ports[4];
static struct uart_driver serial_pxa_reg;

#ifdef CONFIG_SERIAL_PXA_CONSOLE

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

/*
 *	Wait for transmitter & holding register to empty
 */
static inline void wait_for_xmitr(struct uart_pxa_port *up)
{
	unsigned int status, tmout = 10000;

	/* Wait up to 10ms for the character(s) to be sent. */
	do {
		status = serial_in(up, UART_LSR);

		if (status & UART_LSR_BI)
			up->lsr_break_flag = UART_LSR_BI;

		if (--tmout == 0)
			break;
		udelay(1);
	} while ((status & BOTH_EMPTY) != BOTH_EMPTY);

	/* Wait up to 1s for flow control if necessary */
	if (up->port.flags & UPF_CONS_FLOW) {
		tmout = 1000000;
		while (--tmout &&
		       ((serial_in(up, UART_MSR) & UART_MSR_CTS) == 0))
			udelay(1);
	}
}

static void serial_pxa_console_putchar(struct uart_port *port, int ch)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;

	wait_for_xmitr(up);
	serial_out(up, UART_TX, ch);
}

/*
 * Print a string to the serial port trying not to disturb
 * any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void
serial_pxa_console_write(struct console *co, const char *s, unsigned int count)
{
	struct uart_pxa_port *up = serial_pxa_ports[co->index];
	unsigned int ier;
	unsigned long flags;
	int locked = 1;

	local_irq_save(flags);
	if (up->port.sysrq)
		locked = 0;
	else if (oops_in_progress)
		locked = spin_trylock(&up->port.lock);
	else
		spin_lock(&up->port.lock);

	clk_enable(up->clk);

	/*
	 *	First save the IER then disable the interrupts
	 */
	ier = serial_in(up, UART_IER);
	serial_out(up, UART_IER, UART_IER_UUE);

	uart_console_write(&up->port, s, count, serial_pxa_console_putchar);

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore the IER
	 */
	wait_for_xmitr(up);
	serial_out(up, UART_IER, ier);

	clk_disable(up->clk);

	if (locked)
		spin_unlock(&up->port.lock);
	local_irq_restore(flags);
}

static int __init
serial_pxa_console_setup(struct console *co, char *options)
{
	struct uart_pxa_port *up;
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index == -1 || co->index >= serial_pxa_reg.nr)
		co->index = 0;
	up = serial_pxa_ports[co->index];
	if (!up)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(&up->port, co, baud, parity, bits, flow);
}

static struct console serial_pxa_console = {
	.name		= "ttyS",
	.write		= serial_pxa_console_write,
	.device		= uart_console_device,
	.setup		= serial_pxa_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &serial_pxa_reg,
};

#define PXA_CONSOLE	&serial_pxa_console
#else
#define PXA_CONSOLE	NULL
#endif

struct uart_ops serial_pxa_pops = {
	.tx_empty	= serial_pxa_tx_empty,
	.set_mctrl	= serial_pxa_set_mctrl,
	.get_mctrl	= serial_pxa_get_mctrl,
	.stop_tx	= serial_pxa_stop_tx,
	.start_tx	= serial_pxa_start_tx,
	.stop_rx	= serial_pxa_stop_rx,
	.enable_ms	= serial_pxa_enable_ms,
	.break_ctl	= serial_pxa_break_ctl,
	.startup	= serial_pxa_startup,
	.shutdown	= serial_pxa_shutdown,
	.set_termios	= serial_pxa_set_termios,
	.pm		= serial_pxa_pm,
	.type		= serial_pxa_type,
	.release_port	= serial_pxa_release_port,
	.request_port	= serial_pxa_request_port,
	.config_port	= serial_pxa_config_port,
	.verify_port	= serial_pxa_verify_port,
};

static struct uart_driver serial_pxa_reg = {
	.owner		= THIS_MODULE,
	.driver_name	= "PXA serial",
	.dev_name	= "ttyS",
	.major		= TTY_MAJOR,
	.minor		= 64,
	.nr		= 4,
	.cons		= PXA_CONSOLE,
};

#ifdef CONFIG_PM
static int serial_pxa_suspend(struct device *dev)
{
        struct uart_pxa_port *sport = dev_get_drvdata(dev);
	if (sport && (sport->ier & UART_IER_DMAE)) {
		int length = 0, sent = 0;
		unsigned long flags;

		local_irq_save(flags);
		sport->tx_stop = 1;
		sport->rx_stop = 1;
		sport->data_len = 0;
		if (DCSR(sport->txdma) & DCSR_RUN) {
			stop_dma(sport, 0);
			length = DCMD(sport->txdma) & 0x1FFF;
			sent = DSADR(sport->txdma) -
				sport->txdma_addr_phys;
			memcpy(sport->buf_save, sport->txdma_addr
				 + sent, length);
			sport->data_len = length;

		}

		if (DCSR(sport->rxdma) & DCSR_RUN)
			stop_dma(sport, 1);
		pxa_uart_receive_dma(sport->rxdma, sport);

		local_irq_restore(flags);
	}

	if (sport)
		uart_suspend_port(&serial_pxa_reg, &sport->port);

        return 0;
}

static int serial_pxa_resume(struct device *dev)
{
        struct uart_pxa_port *sport = dev_get_drvdata(dev);

        if (sport)
                uart_resume_port(&serial_pxa_reg, &sport->port);

	if (sport && (sport->ier & UART_IER_DMAE)) {
		if (sport->data_len > 0) {
			memcpy(sport->txdma_addr, sport->buf_save,
					sport->data_len);
			pxa_uart_transmit_dma_start(sport,
					sport->data_len);
		} else
			tasklet_schedule(&sport->tklet);

		pxa_uart_receive_dma_start(sport);
	}

        return 0;
}

static const struct dev_pm_ops serial_pxa_pm_ops = {
	.suspend	= serial_pxa_suspend,
	.resume		= serial_pxa_resume,
};
#endif

static void pxa_timer_handler(unsigned long data)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)data;
	pm_qos_update_request(&up->qos_idle[PXA_UART_RX],
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
}

static void uart_tx_lpm_handler(struct work_struct *work)
{
	struct uart_pxa_port *up =
		container_of(work, struct uart_pxa_port, uart_tx_lpm_work);

	/* Polling until TX FIFO is empty */
	while(!(serial_in(up, UART_LSR) & UART_LSR_TEMT))
		msleep(1);
	pm_qos_update_request(&up->qos_idle[PXA_UART_TX],
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
}

static struct of_device_id serial_pxa_dt_ids[] = {
	{ .compatible = "mrvl,pxa-uart", },
	{ .compatible = "mrvl,mmp-uart", },
	{}
};
MODULE_DEVICE_TABLE(of, serial_pxa_dt_ids);

static int serial_pxa_probe_dt(struct platform_device *pdev,
			       struct uart_pxa_port *sport)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;

	if (!np)
		return 1;

	ret = of_alias_get_id(np, "serial");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get alias id, errno %d\n", ret);
		return ret;
	}
	sport->port.line = ret;
	return 0;
}

static int serial_pxa_probe(struct platform_device *dev)
{
	struct uart_pxa_port *sport;
	struct resource *mmres, *irqres, *dmares;
	int ret;

	mmres = platform_get_resource(dev, IORESOURCE_MEM, 0);
	irqres = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	if (!mmres || !irqres)
		return -ENODEV;

	sport = kzalloc(sizeof(struct uart_pxa_port), GFP_KERNEL);
	if (!sport)
		return -ENOMEM;

	sport->clk = clk_get(&dev->dev, NULL);
	if (IS_ERR(sport->clk)) {
		ret = PTR_ERR(sport->clk);
		goto err_free;
	}
	ret = clk_prepare(sport->clk);
	if (ret) {
		clk_put(sport->clk);
		goto err_free;
	}

	sport->port.type = PORT_PXA;
	sport->port.iotype = UPIO_MEM;
	sport->port.mapbase = mmres->start;
	sport->port.irq = irqres->start;
	sport->port.fifosize = 64;
	sport->port.ops = &serial_pxa_pops;
	sport->port.dev = &dev->dev;
	sport->port.flags = UPF_IOREMAP | UPF_BOOT_AUTOCONF;
	sport->port.uartclk = clk_get_rate(sport->clk);

	ret = serial_pxa_probe_dt(dev, sport);
	if (ret > 0)
		sport->port.line = dev->id;
	else if (ret < 0)
		goto err_clk;
	snprintf(sport->name, PXA_NAME_LEN - 1, "UART%d", sport->port.line + 1);

	sport->rxdrcmr = NULL;
	sport->txdrcmr = NULL;
	sport->txdma = 0;
	sport->rxdma = 0;
	sport->txdma_addr = NULL;
	sport->rxdma_addr = NULL;
	sport->dma_enable = 0;

	if (uart_dma) {
		/* Get Rx DMA mapping register */
		dmares = platform_get_resource(dev, IORESOURCE_DMA, 0);
		if (dmares)
			sport->rxdrcmr = &DRCMR(dmares->start);

		/* Get Tx DMA mapping register */
		dmares = platform_get_resource(dev, IORESOURCE_DMA, 1);
		if (dmares)
			sport->txdrcmr = &DRCMR(dmares->start);

		if (sport->rxdrcmr && sport->txdrcmr)
			sport->dma_enable = 1;
	}

	sport->qos_idle[PXA_UART_TX].name = kasprintf(GFP_KERNEL,
					    "%s%s", "tx", sport->name);
	if (!sport->qos_idle[PXA_UART_TX].name) {
		ret = -ENOMEM;
		goto err_clk;
	}

	sport->qos_idle[PXA_UART_RX].name = kasprintf(GFP_KERNEL,
					    "%s%s", "rx", sport->name);
	if (!sport->qos_idle[PXA_UART_RX].name) {
		ret = -ENOMEM;
		kfree(sport->qos_idle[PXA_UART_TX].name);
		goto err_clk;
	}

	pm_qos_add_request(&sport->qos_idle[PXA_UART_TX], PM_QOS_CPUIDLE_BLOCK,
			   PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	pm_qos_add_request(&sport->qos_idle[PXA_UART_RX], PM_QOS_CPUIDLE_BLOCK,
			   PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

	sport->port.membase = ioremap(mmres->start, resource_size(mmres));
	if (!sport->port.membase) {
		ret = -ENOMEM;
		goto err_map;
	}

	INIT_WORK(&sport->uart_tx_lpm_work, uart_tx_lpm_handler);

	init_timer(&sport->pxa_timer);
	sport->pxa_timer.function = pxa_timer_handler;
	sport->pxa_timer.data = (long)sport;
	serial_pxa_ports[sport->port.line] = sport;

	uart_add_one_port(&serial_pxa_reg, &sport->port);
	platform_set_drvdata(dev, sport);

	return 0;

 err_map:
	kfree(sport->qos_idle[PXA_UART_TX].name);
	kfree(sport->qos_idle[PXA_UART_RX].name);
	pm_qos_remove_request(&sport->qos_idle[PXA_UART_RX]);
	pm_qos_remove_request(&sport->qos_idle[PXA_UART_TX]);
 err_clk:
	clk_unprepare(sport->clk);
	clk_put(sport->clk);
 err_free:
	kfree(sport);
	return ret;
}

static int serial_pxa_remove(struct platform_device *dev)
{
	struct uart_pxa_port *sport = platform_get_drvdata(dev);

	kfree(sport->qos_idle[PXA_UART_TX].name);
	kfree(sport->qos_idle[PXA_UART_RX].name);
	pm_qos_remove_request(&sport->qos_idle[PXA_UART_RX]);
	pm_qos_remove_request(&sport->qos_idle[PXA_UART_TX]);

	platform_set_drvdata(dev, NULL);

	uart_remove_one_port(&serial_pxa_reg, &sport->port);

	clk_unprepare(sport->clk);
	clk_put(sport->clk);
	kfree(sport);
	serial_pxa_ports[dev->id] = NULL;

	return 0;
}

static struct platform_driver serial_pxa_driver = {
        .probe          = serial_pxa_probe,
        .remove         = serial_pxa_remove,

	.driver		= {
	        .name	= "pxa2xx-uart",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &serial_pxa_pm_ops,
#endif
		.of_match_table = serial_pxa_dt_ids,
	},
};

int __init serial_pxa_init(void)
{
	int ret;

	ret = uart_register_driver(&serial_pxa_reg);
	if (ret != 0)
		return ret;

	ret = platform_driver_register(&serial_pxa_driver);
	if (ret != 0)
		uart_unregister_driver(&serial_pxa_reg);

	return ret;
}

void __exit serial_pxa_exit(void)
{
	platform_driver_unregister(&serial_pxa_driver);
	uart_unregister_driver(&serial_pxa_reg);
}

module_init(serial_pxa_init);
module_exit(serial_pxa_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pxa2xx-uart");
