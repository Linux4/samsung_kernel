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
#include <linux/dmaengine.h>
#include <linux/pm_qos.h>
#include <linux/edge_wakeup_mmp.h>
#include <linux/wakelock.h>

#define	DMA_BLOCK	UART_XMIT_SIZE

#define PXA_UART_TX	0
#define PXA_UART_RX	1

#define PXA_NAME_LEN		8

/*
 * DMA related data is stored in this struct,
 * making it separated from non-DMA mode.
 */
struct uart_pxa_dma {
#define TX_DMA_RUNNING		(1 << 0)
#define RX_DMA_RUNNING		(1 << 1)
	unsigned int		dma_status;

	struct dma_chan *txdma_chan;
	struct dma_chan *rxdma_chan;
	struct dma_async_tx_descriptor *rx_desc;
	struct dma_async_tx_descriptor *tx_desc;
	void			*txdma_addr;
	void			*rxdma_addr;
	dma_addr_t		txdma_addr_phys;
	dma_addr_t		rxdma_addr_phys;
	int			tx_stop;
	int			rx_stop;
	dma_cookie_t rx_cookie;
	dma_cookie_t tx_cookie;
	unsigned int		drcmr_tx;
	unsigned int		drcmr_rx;
	int			tx_size; /* size of last transmit bytes */
	struct	tasklet_struct	tklet;

#ifdef	CONFIG_PM
	/* We needn't save rx dma register because we
	 * just restart the dma totallly after resume
	 */
	void			*tx_buf_save;
	int			tx_saved_len;
#endif
};

struct uart_pxa_port {
	struct uart_port        port;
	unsigned int            ier;
	unsigned char           lcr;
	unsigned int            mcr;
	unsigned int            lsr_break_flag;
	struct timer_list	pxa_timer;
	struct pm_qos_request	qos_idle[2];
	s32			lpm_qos;
	int			edge_wakeup_gpio;
	struct clk		*clk;
	char			name[PXA_NAME_LEN];
	struct work_struct	uart_tx_lpm_work;
	int			dma_enable;
	struct uart_pxa_dma	uart_dma;
	unsigned long		flags;
};

static int uart_dma;

static int __init uart_dma_setup(char *__unused)
{
/* if CONFIG_MMP_PDMA not enabled, uart will work in non-DMA mode */
#ifdef CONFIG_MMP_PDMA
	uart_dma = 1;
#endif
	return 1;
}

__setup("uart_dma", uart_dma_setup);

static inline void stop_dma(struct uart_pxa_port *up, int read)
{
	unsigned long flags;
	struct uart_pxa_dma *pxa_dma = &up->uart_dma;
	struct dma_chan *channel;

	channel = read ? pxa_dma->rxdma_chan : pxa_dma->txdma_chan;

	dmaengine_terminate_all(channel);
	spin_lock_irqsave(&up->port.lock, flags);
	if (read)
		pxa_dma->dma_status &= ~RX_DMA_RUNNING;
	else
		pxa_dma->dma_status &= ~TX_DMA_RUNNING;
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void pxa_uart_transmit_dma_cb(void *data);
static void pxa_uart_receive_dma_cb(void *data);
static void pxa_uart_transmit_dma_start(struct uart_pxa_port *up, int count);
static void pxa_uart_receive_dma_start(struct uart_pxa_port *up);
static inline void wait_for_xmitr(struct uart_pxa_port *up);
static inline void serial_out(struct uart_pxa_port *up, int offset, int value);
static void pxa_timer_handler(unsigned long data);
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
		up->uart_dma.tx_stop = 1;

		if (up->ier & UART_IER_DMAE) {
			/*
			 * Here we cannot use dma_status to determine
			 * whether dma has been transfer completed
			 * As there when this function is being caled,
			 * it would hold a spinlock and possible with irqsave,
			 * If this function is being called over core0,
			 * we may cannot get status change from the flag,
			 * As irq handler is being blocked to be called yet.
			 */
			while (dma_async_is_tx_complete(up->uart_dma.txdma_chan,
			       up->uart_dma.tx_cookie, NULL, NULL)
				!= DMA_COMPLETE && (timeout-- > 0))
				udelay(1);

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
			spin_unlock_irqrestore(&up->port.lock, up->flags);
			stop_dma(up, PXA_UART_RX);
			spin_lock_irqsave(&up->port.lock, up->flags);
		}
		up->uart_dma.rx_stop = 1;
	} else {
		up->ier &= ~UART_IER_RLSI;
		up->port.read_status_mask &= ~UART_LSR_DR;
		serial_out(up, UART_IER, up->ier);
	}
}

static inline void receive_chars(struct uart_pxa_port *up, int *status)
{
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
		spin_lock_irqsave(&up->port.lock, up->flags);
		up->ier &= ~UART_IER_RTOIE;
		serial_out(up, UART_IER, up->ier);
		spin_unlock_irqrestore(&up->port.lock, up->flags);

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
	tty_flip_buffer_push(&up->port.state->port);

	/* work around Errata #20 according to
	 * Intel(R) PXA27x Processor Family
	 * Specification Update (May 2005)
	 *
	 * Step 6:
	 * No more data in FIFO: Re-enable RTO interrupt via IER[RTOIE]
	 */
	spin_lock_irqsave(&up->port.lock, up->flags);
	up->ier |= UART_IER_RTOIE;
	serial_out(up, UART_IER, up->ier);
	spin_unlock_irqrestore(&up->port.lock, up->flags);
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
		spin_lock_irqsave(&up->port.lock, up->flags);
		serial_pxa_stop_tx(&up->port);
		spin_unlock_irqrestore(&up->port.lock, up->flags);
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
		spin_lock_irqsave(&up->port.lock, up->flags);
		serial_pxa_stop_tx(&up->port);
		spin_unlock_irqrestore(&up->port.lock, up->flags);
	}
}

static inline void
dma_receive_chars(struct uart_pxa_port *up, int *status)
{
	struct tty_port *port = &up->port.state->port;
	unsigned char ch;
	int max_count = 256;
	int count = 0;
	unsigned char *tmp;
	unsigned int flag = TTY_NORMAL;
	struct uart_pxa_dma *pxa_dma = &up->uart_dma;
	struct dma_tx_state dma_state;

	/*
	 * Pause DMA channel and deal with the bytes received by DMA
	 */
	dmaengine_pause(pxa_dma->rxdma_chan);
	dmaengine_tx_status(pxa_dma->rxdma_chan, pxa_dma->rx_cookie,
			    &dma_state);
	count = DMA_BLOCK - dma_state.residue;
	tmp = pxa_dma->rxdma_addr;
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
		tty_insert_flip_string(port, tmp, count);
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
			} else if (*status & UART_LSR_PE) {
				up->port.icount.parity++;
			} else if (*status & UART_LSR_FE) {
				up->port.icount.frame++;
			}
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

	tty_schedule_flip(port);
	stop_dma(up, 1);
	if (pxa_dma->rx_stop)
		return;
	pxa_uart_receive_dma_start(up);
}

static void serial_pxa_start_tx(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;

	if (up->dma_enable) {
		up->uart_dma.tx_stop = 0;
		tasklet_schedule(&up->uart_dma.tklet);
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
				      up->lpm_qos);

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
			while (!serial_pxa_tx_empty((struct uart_port *)dev_id))
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
			if (up->uart_dma.dma_status & TX_DMA_RUNNING) {
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
	struct uart_pxa_dma *pxa_dma = &up->uart_dma;
	struct dma_slave_config slave_config;
	int ret;

	slave_config.direction	    = DMA_MEM_TO_DEV;
	slave_config.dst_addr	    = up->port.mapbase;
	slave_config.dst_maxburst   = 16;
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	/*
	 * FIXME: keep slave_id for compatibility.
	 * If mmp_pdma doesn't use it any more, we should remove all code
	 * related with slave_id/drcmr_rx/drdmr_tx.
	 */
	if (pxa_dma->drcmr_tx)
		slave_config.slave_id       = pxa_dma->drcmr_tx;
	else
		slave_config.slave_id	    = 0;

	ret = dmaengine_slave_config(pxa_dma->txdma_chan, &slave_config);
	if (ret) {
		dev_err(up->port.dev,
			"%s: dmaengine slave config err.\n", __func__);
		return;
	}

	pxa_dma->tx_size = count;
	pxa_dma->tx_desc = dmaengine_prep_slave_single(pxa_dma->txdma_chan,
		pxa_dma->txdma_addr_phys, count, DMA_MEM_TO_DEV, 0);
	pxa_dma->tx_desc->callback = pxa_uart_transmit_dma_cb;
	pxa_dma->tx_desc->callback_param = up;

	pxa_dma->tx_cookie = dmaengine_submit(pxa_dma->tx_desc);
	pm_qos_update_request(&up->qos_idle[PXA_UART_TX],
			      up->lpm_qos);

	dma_async_issue_pending(pxa_dma->txdma_chan);
}

static void pxa_uart_receive_dma_start(struct uart_pxa_port *up)
{
	unsigned long flags;
	struct uart_pxa_dma *uart_dma = &up->uart_dma;
	struct dma_slave_config slave_config;
	int ret;

	spin_lock_irqsave(&up->port.lock, flags);
	if (uart_dma->dma_status & RX_DMA_RUNNING) {
		spin_unlock_irqrestore(&up->port.lock, flags);
		return;
	}
	uart_dma->dma_status |= RX_DMA_RUNNING;
	spin_unlock_irqrestore(&up->port.lock, flags);

	slave_config.direction	    = DMA_DEV_TO_MEM;
	slave_config.src_addr	    = up->port.mapbase;
	slave_config.src_maxburst   = 16;
	slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	/*
	 * FIXME: keep slave_id for compatibility.
	 * If mmp_pdma doesn't use it any more, we should remove all code
	 * related with slave_id/drcmr_rx/drdmr_tx.
	 */
	if (uart_dma->drcmr_rx)
		slave_config.slave_id       = uart_dma->drcmr_rx;
	else
		slave_config.slave_id	    = 0;

	ret = dmaengine_slave_config(uart_dma->rxdma_chan, &slave_config);
	if (ret) {
		dev_err(up->port.dev,
			"%s: dmaengine slave config err.\n", __func__);
		return;
	}

	uart_dma->rx_desc = dmaengine_prep_slave_single(uart_dma->rxdma_chan,
		uart_dma->rxdma_addr_phys, DMA_BLOCK, DMA_DEV_TO_MEM, 0);
	uart_dma->rx_desc->callback = pxa_uart_receive_dma_cb;
	uart_dma->rx_desc->callback_param = up;

	uart_dma->rx_cookie = dmaengine_submit(uart_dma->rx_desc);
	dma_async_issue_pending(uart_dma->rxdma_chan);
}

static void pxa_uart_receive_dma_cb(void *data)
{
	unsigned long flags;
	struct uart_pxa_port *up = (struct uart_pxa_port *)data;
	struct uart_pxa_dma *pxa_dma = &up->uart_dma;
	struct tty_port *port = &up->port.state->port;
	unsigned int count;
	unsigned char *tmp = pxa_dma->rxdma_addr;
	struct dma_tx_state dma_state;

	if (!mod_timer(&up->pxa_timer, jiffies + PXA_TIMER_TIMEOUT))
		pm_qos_update_request(&up->qos_idle[PXA_UART_RX],
				      up->lpm_qos);

	dmaengine_tx_status(pxa_dma->rxdma_chan, pxa_dma->rx_cookie,
			    &dma_state);
	count = DMA_BLOCK - dma_state.residue;
	if (up->port.sysrq) {
		while (count > 0) {
			if (!uart_handle_sysrq_char(&up->port, *tmp)) {
				tty_insert_flip_char(port, *tmp, TTY_NORMAL);
				up->port.icount.rx++;
			}
			tmp++;
			count--;
		}
	} else {
		tty_insert_flip_string(port, tmp, count);
		up->port.icount.rx += count;
	}
	tty_flip_buffer_push(port);

	spin_lock_irqsave(&up->port.lock, flags);
	/*
	 * DMA_RUNNING flag should be clear only after
	 * all dma interface operation completed
	 */
	pxa_dma->dma_status &= ~RX_DMA_RUNNING;
	spin_unlock_irqrestore(&up->port.lock, flags);

	if (pxa_dma->rx_stop)
		return;
	pxa_uart_receive_dma_start(up);
	return;
}

static void pxa_uart_transmit_dma_cb(void *data)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)data;
	struct uart_pxa_dma *pxa_dma = &up->uart_dma;
	struct circ_buf *xmit = &up->port.state->xmit;

	if (dma_async_is_tx_complete(pxa_dma->txdma_chan, pxa_dma->tx_cookie,
				     NULL, NULL) == DMA_COMPLETE)
		schedule_work(&up->uart_tx_lpm_work);

	spin_lock_irqsave(&up->port.lock, up->flags);
	/*
	 * DMA_RUNNING flag should be clear only after
	 * all dma interface operation completed
	 */
	pxa_dma->dma_status &= ~TX_DMA_RUNNING;
	spin_unlock_irqrestore(&up->port.lock, up->flags);

	/* if tx stop, stop transmit DMA and return */
	if (pxa_dma->tx_stop)
		return;

	if (up->port.x_char) {
		serial_out(up, UART_TX, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);

	if (!uart_circ_empty(xmit))
		tasklet_schedule(&pxa_dma->tklet);
	return;
}

static void pxa_uart_dma_init(struct uart_pxa_port *up)
{
	struct uart_pxa_dma *pxa_dma = &up->uart_dma;
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	if (NULL == pxa_dma->rxdma_chan) {
		pxa_dma->rxdma_chan = dma_request_slave_channel(up->port.dev,
								"rx");
		if (IS_ERR_OR_NULL(pxa_dma->rxdma_chan))
			goto out;
	}

	if (NULL == pxa_dma->txdma_chan) {
		pxa_dma->txdma_chan = dma_request_slave_channel(up->port.dev,
								"tx");
		if (IS_ERR_OR_NULL(pxa_dma->txdma_chan))
			goto err_txdma;
	}

	if (NULL == pxa_dma->txdma_addr) {
		pxa_dma->txdma_addr = dma_alloc_coherent(up->port.dev,
			DMA_BLOCK, &pxa_dma->txdma_addr_phys, GFP_KERNEL);
		if (!pxa_dma->txdma_addr)
			goto txdma_err_alloc;
	}

	if (NULL == pxa_dma->rxdma_addr) {
		pxa_dma->rxdma_addr = dma_alloc_coherent(up->port.dev,
			DMA_BLOCK, &pxa_dma->rxdma_addr_phys, GFP_KERNEL);
		if (!pxa_dma->rxdma_addr)
			goto rxdma_err_alloc;
	}

#ifdef CONFIG_PM
	pxa_dma->tx_buf_save = kmalloc(DMA_BLOCK, GFP_KERNEL);
	if (!pxa_dma->tx_buf_save)
		goto buf_err_alloc;
#endif

	pxa_dma->dma_status = 0;
	return;

#ifdef CONFIG_PM
buf_err_alloc:
	dma_free_coherent(up->port.dev, DMA_BLOCK, pxa_dma->rxdma_addr,
			  pxa_dma->rxdma_addr_phys);
	pxa_dma->rxdma_addr = NULL;
#endif
rxdma_err_alloc:
	dma_free_coherent(up->port.dev, DMA_BLOCK, pxa_dma->txdma_addr,
			  pxa_dma->txdma_addr_phys);
	pxa_dma->txdma_addr = NULL;
txdma_err_alloc:
	dma_release_channel(pxa_dma->txdma_chan);
	pxa_dma->txdma_chan = NULL;
err_txdma:
	dma_release_channel(pxa_dma->rxdma_chan);
	pxa_dma->rxdma_chan = NULL;
out:
	return;
}

static void pxa_uart_dma_uninit(struct uart_pxa_port *up)
{
	struct uart_pxa_dma *pxa_dma;
	pxa_dma = &up->uart_dma;

#ifdef CONFIG_PM
	kfree(pxa_dma->tx_buf_save);
#endif

	stop_dma(up, PXA_UART_TX);
	stop_dma(up, PXA_UART_RX);
	if (pxa_dma->txdma_addr != NULL) {
		dma_free_coherent(up->port.dev, DMA_BLOCK, pxa_dma->txdma_addr,
				  pxa_dma->txdma_addr_phys);
		pxa_dma->txdma_addr = NULL;
	}
	if (pxa_dma->txdma_chan != NULL) {
		dma_release_channel(pxa_dma->txdma_chan);
		pxa_dma->txdma_chan = NULL;
	}

	if (pxa_dma->rxdma_addr != NULL) {
		dma_free_coherent(up->port.dev, DMA_BLOCK, pxa_dma->rxdma_addr,
				  pxa_dma->rxdma_addr_phys);
		pxa_dma->rxdma_addr = NULL;
	}

	if (pxa_dma->rxdma_chan != NULL) {
		dma_release_channel(pxa_dma->rxdma_chan);
		pxa_dma->rxdma_chan = NULL;
	}

	return;
}

static void uart_task_action(unsigned long data)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)data;
	struct circ_buf *xmit = &up->port.state->xmit;
	unsigned char *tmp = up->uart_dma.txdma_addr;
	unsigned long flags;
	int count = 0, c;

	/* if the tx is stop, just return.*/
	if (up->uart_dma.tx_stop)
		return;

	spin_lock_irqsave(&up->port.lock, flags);
	if (up->uart_dma.dma_status & TX_DMA_RUNNING) {
		spin_unlock_irqrestore(&up->port.lock, flags);
		return;
	}

	up->uart_dma.dma_status |= TX_DMA_RUNNING;
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
		pxa_uart_dma_init(up);
		up->uart_dma.rx_stop = 0;
		pxa_uart_receive_dma_start(up);
		tasklet_init(&up->uart_dma.tklet, uart_task_action,
			     (unsigned long)up);
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
		tasklet_kill(&up->uart_dma.tklet);
		pxa_uart_dma_uninit(up);
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
	if (termios->c_iflag & (IGNBRK | BRKINT | PARMRK))
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
	if (!up->dma_enable) {
		/* Don't enable modem status interrupt if DMA is enabled.
		 * Inherited from the old code.
		 * Please also refer to serial_pxa_enable_ms().
		 */
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

	clk_enable(up->clk);
	local_irq_save(flags);
	if (up->port.sysrq)
		locked = 0;
	else if (oops_in_progress)
		locked = spin_trylock_irqsave(&up->port.lock, up->flags);
	else
		spin_lock_irqsave(&up->port.lock, up->flags);

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

	if (locked)
		spin_unlock_irqrestore(&up->port.lock, up->flags);
	local_irq_restore(flags);
	clk_disable(up->clk);

}

#ifdef CONFIG_CONSOLE_POLL
/*
 * Console polling routines for writing and reading from the uart while
 * in an interrupt or debug context.
 */

static int serial_pxa_get_poll_char(struct uart_port *port)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;
	unsigned char lsr = serial_in(up, UART_LSR);

	while (!(lsr & UART_LSR_DR))
		lsr = serial_in(up, UART_LSR);

	return serial_in(up, UART_RX);
}


static void serial_pxa_put_poll_char(struct uart_port *port,
			 unsigned char c)
{
	unsigned int ier;
	struct uart_pxa_port *up = (struct uart_pxa_port *)port;

	/*
	 *	First save the IER then disable the interrupts
	 */
	ier = serial_in(up, UART_IER);
	serial_out(up, UART_IER, UART_IER_UUE);

	wait_for_xmitr(up);
	/*
	 *	Send the character out.
	 *	If a LF, also do CR...
	 */
	serial_out(up, UART_TX, c);
	if (c == 10) {
		wait_for_xmitr(up);
		serial_out(up, UART_TX, 13);
	}

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore the IER
	 */
	wait_for_xmitr(up);
	serial_out(up, UART_IER, ier);
}

#endif /* CONFIG_CONSOLE_POLL */

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

static struct uart_ops serial_pxa_pops = {
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
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char = serial_pxa_get_poll_char,
	.poll_put_char = serial_pxa_put_poll_char,
#endif
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
	struct uart_pxa_dma *pxa_dma = &sport->uart_dma;
	struct dma_tx_state dma_state;

	if (!console_suspend_enabled)
		return 0;

	if (sport && (sport->ier & UART_IER_DMAE)) {
		int sent = 0;
		unsigned long flags;

		local_irq_save(flags);
		/*
		 * tx stop and suspend and when resume,
		 * tx startup would be called and set it to 0
		*/
		pxa_dma->tx_stop = 1;
		pxa_dma->rx_stop = 1;
		pxa_dma->tx_saved_len = 0;
		if (dma_async_is_tx_complete(pxa_dma->txdma_chan,
					     pxa_dma->tx_cookie, NULL, NULL)
			!= DMA_COMPLETE) {
			dmaengine_pause(pxa_dma->txdma_chan);
			dmaengine_tx_status(pxa_dma->txdma_chan,
					    pxa_dma->tx_cookie, &dma_state);
			sent = pxa_dma->tx_size - dma_state.residue;
			pxa_dma->tx_saved_len = dma_state.residue;
			memcpy(pxa_dma->tx_buf_save, pxa_dma->txdma_addr + sent,
			       dma_state.residue);
			stop_dma(sport, PXA_UART_TX);
		}
		if (dma_async_is_tx_complete(pxa_dma->rxdma_chan,
					     pxa_dma->rx_cookie, NULL, NULL)
			!= DMA_COMPLETE) {
			dmaengine_pause(pxa_dma->rxdma_chan);
			pxa_uart_receive_dma_cb(sport);
			stop_dma(sport, PXA_UART_RX);
		}
		local_irq_restore(flags);
	}

	if (sport)
		uart_suspend_port(&serial_pxa_reg, &sport->port);

	/* Remove uart rx constraint which will block system entering D1p. */
	if (del_timer_sync(&sport->pxa_timer))
		pxa_timer_handler((unsigned long)sport);

        return 0;
}

static int serial_pxa_resume(struct device *dev)
{
        struct uart_pxa_port *sport = dev_get_drvdata(dev);
	struct uart_pxa_dma *pxa_dma = &sport->uart_dma;

	if (!console_suspend_enabled)
		return 0;

        if (sport)
                uart_resume_port(&serial_pxa_reg, &sport->port);

	if (sport && (sport->ier & UART_IER_DMAE)) {
		if (pxa_dma->tx_saved_len > 0) {
			memcpy(pxa_dma->txdma_addr, pxa_dma->tx_buf_save,
			       pxa_dma->tx_saved_len);
			pxa_uart_transmit_dma_start(sport,
						    pxa_dma->tx_saved_len);
		} else {
			tasklet_schedule(&pxa_dma->tklet);
		}

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

static void uart_edge_wakeup_handler(int gpio, void *data)
{
	struct uart_pxa_port *up = (struct uart_pxa_port *)data;
	if (!mod_timer(&up->pxa_timer, jiffies + PXA_TIMER_TIMEOUT))
		pm_qos_update_request(&up->qos_idle[PXA_UART_RX],
				      up->lpm_qos);
}

static void uart_tx_lpm_handler(struct work_struct *work)
{
	struct uart_pxa_port *up =
		container_of(work, struct uart_pxa_port, uart_tx_lpm_work);

	/* Polling until TX FIFO is empty */
	while (!(serial_in(up, UART_LSR) & UART_LSR_TEMT))
		usleep_range(1000, 2000);
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

	/* device tree is used */
	if (uart_dma)
		sport->dma_enable = 1;

	ret = of_alias_get_id(np, "serial");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get alias id, errno %d\n", ret);
		return ret;
	}
	sport->port.line = ret;

	if (of_property_read_u32(np, "lpm-qos", &sport->lpm_qos)) {
		dev_err(&pdev->dev, "cannot find lpm-qos in device tree\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np,
			"edge-wakeup-gpio", &sport->edge_wakeup_gpio))
		dev_info(&pdev->dev, "no edge-wakeup-gpio defined\n");

	return 0;
}

static int serial_pxa_probe(struct platform_device *dev)
{
	struct uart_pxa_port *sport;
	struct resource *mmres, *irqres, *dmares;
	int ret;
	struct uart_pxa_dma *pxa_dma;

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
	sport->edge_wakeup_gpio = -1;

	pxa_dma = &sport->uart_dma;
	pxa_dma->drcmr_rx = 0;
	pxa_dma->drcmr_tx = 0;
	pxa_dma->txdma_chan = NULL;
	pxa_dma->rxdma_chan = NULL;
	pxa_dma->txdma_addr = NULL;
	pxa_dma->rxdma_addr = NULL;
	sport->dma_enable = 0;

	ret = serial_pxa_probe_dt(dev, sport);
	if (ret > 0)
		sport->port.line = dev->id;
	else if (ret < 0)
		goto err_clk;
	snprintf(sport->name, PXA_NAME_LEN - 1, "UART%d", sport->port.line + 1);

	if (ret > 0 && uart_dma) {
		/* Get Rx DMA mapping value */
		dmares = platform_get_resource(dev, IORESOURCE_DMA, 0);
		if (dmares) {
			pxa_dma->drcmr_rx = dmares->start;

			/* Get Tx DMA mapping value */
			dmares = platform_get_resource(dev, IORESOURCE_DMA, 1);
			if (dmares) {
				pxa_dma->drcmr_tx = dmares->start;
				sport->dma_enable = 1;
			}
		}
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

	if (sport->edge_wakeup_gpio >= 0) {
		ret = request_mfp_edge_wakeup(sport->edge_wakeup_gpio,
					      uart_edge_wakeup_handler,
					      sport, &dev->dev);
		if (ret) {
			dev_err(&dev->dev, "failed to request edge wakeup.\n");
			goto err_qos;
		}
	}

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
	if (sport->edge_wakeup_gpio >= 0)
		remove_mfp_edge_wakeup(sport->edge_wakeup_gpio);
 err_qos:
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

	uart_remove_one_port(&serial_pxa_reg, &sport->port);

	clk_unprepare(sport->clk);
	clk_put(sport->clk);

	if (sport->edge_wakeup_gpio >= 0)
		remove_mfp_edge_wakeup(sport->edge_wakeup_gpio);

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

static int __init serial_pxa_init(void)
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

static void __exit serial_pxa_exit(void)
{
	platform_driver_unregister(&serial_pxa_driver);
	uart_unregister_driver(&serial_pxa_reg);
}

module_init(serial_pxa_init);
module_exit(serial_pxa_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pxa2xx-uart");
