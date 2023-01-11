/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.

 *(C) Copyright 2007 Marvell International Ltd.
 * All Rights Reserved
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include <linux/tty_ldisc.h>
#include <linux/sched.h>

#include <linux/poll.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define DRIVER_VERSION "v1.0"
#define DRIVER_AUTHOR "Vincent Yeung"
#define DRIVER_DESC "Pseudo CI TTY driver"

/* Module information */
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

#define CITTY_TTY_MAJOR		0	/* experimental range */
#define CITTY_TTY_MINORS	29	/* citty0 through citty28 */

/*
 *  The following Macro is used for debugging purpose
 */
/* #define CITTY_DEBUG */
/* #define DEBUG_BUF_CONTENT */ /* define this to see the buffer content */

#undef PDEBUG			/* undef it, just in case */
#ifdef CITTY_DEBUG
#define PDEBUG(fmt, args ...)	pr_debug("CITTY: " fmt, ## args)
#define F_ENTER()		pr_debug("CITTY: ENTER %s\n", __func__)
#define F_LEAVE()		pr_debug("CITTY: LEAVE %s\n", __func__)
#else
#define PDEBUG(fmt, args ...)	do {} while (0)
#define F_ENTER()		do {} while (0)
#define F_LEAVE()		do {} while (0)
#endif

struct citty_port {
	struct tty_port *port;	/* point to tty_port struct */

	/* for tiocmget and tiocmset functions */
	int msr;		/* MSR shadow */
	int mcr;		/* MCR shadow */

	/* for ioctl fun */
	struct serial_struct serial;
	wait_queue_head_t wait;
	struct async_icount icount;
};

static struct citty_port *citty_table[CITTY_TTY_MINORS];
static struct mutex mutex_lock_tty[CITTY_TTY_MINORS];
static struct tty_port citty_port_table[CITTY_TTY_MINORS];

/* CCTDEV stuff */
#define COPY_FROM_CITTY 0       /*Tx*/
#define COPY_FROM_USER 1        /*Rx*/
#define COPY_TO_CITTY 2         /*Rx*/
#define COPY_TO_USER 3          /*Tx*/

#define CCTDEV_NR_DEVS CITTY_TTY_MINORS   /*ccdev0 through ccdev28*/
#define CCTDEV_QUANTUM 4000
#define CCTDEV_QSET    1000

#define NUM_CITTY_BUF 100	/* 10 */
/* this is the max ethernet packet size */
#define CITTY_BUF_SIZE 1024	/* 1518 */ /*ETH_DATA_LEN*/ /*1024*/

int cctdev_major;
int cctdev_minor;
int cctdev_nr_devs = CCTDEV_NR_DEVS;	/* number of bare ccdev devices */
int cctdev_quantum = CCTDEV_QUANTUM;
int cctdev_qset = CCTDEV_QSET;

/*
 * Representation of ccdev quantum sets.
 */

struct cctdev_dev {
	int nreaders, nwriters;                 /* number of openings for r/w */
	struct fasync_struct *async_queue;      /* asynchronous readers */
	struct mutex lock;                   /* mutual exclusion lock */
	struct cdev cdev;                       /* Char device structure */
};

struct buf_struct {
	unsigned char *pBuf[NUM_CITTY_BUF];
	int iDatalen[NUM_CITTY_BUF];
	int iBufIn;
	int iBufOut;
	wait_queue_head_t gInq;
	wait_queue_head_t gOutq;
	struct mutex gMutex;
};

struct cctdev_dev *cctdev_devices;	/* allocated in cctdev_init_module */

/* only the transmiter buffer is needed */
struct buf_struct txCittyBuf[CITTY_TTY_MINORS];

static int cctdev_ready;

/*
 *   The part that handle I/O to user space
 *
 *   Note: cctdev is for functions that used for the char device for pTTY driver
 *
 */

void cci_init_buffer(struct buf_struct *buffer)
{
	int i;

	F_ENTER();

	for (i = 0; i < NUM_CITTY_BUF; i++) {
		buffer->pBuf[i] = kmalloc(CITTY_BUF_SIZE, GFP_KERNEL);
		if (!buffer->pBuf[i])
			pr_err("Failed to allocate memory.\n");

		buffer->iBufIn = 0;
		buffer->iBufOut = 0;

	}

	mutex_init(&buffer->gMutex);

	/* init the queue */
	init_waitqueue_head(&(buffer->gInq));
	init_waitqueue_head(&(buffer->gOutq));

	F_LEAVE();

}

void cci_free_buffer(struct buf_struct *buffer)
{
	int i;

	/* free the buffer */
	for (i = 0; i < NUM_CITTY_BUF; i++) {
		kfree(buffer->pBuf[i]);
		buffer->pBuf[i] = NULL;
	}
}

/*******************************************************************
*  FUNCTION: read_citty_buffer
*
*  DESCRIPTION: To read the data from the CINET buffer (either Rx or Tx)
*
*	Note: don't need to pass in count (size), since the buffer should know
*			the length of the data
*
*  RETURNS: utlFAILED or utlSUCCESS
*
*******************************************************************/
size_t read_citty_buffer(char __user *buf,
			struct buf_struct *cittyBuf,
			short action)
{
	size_t count;
	unsigned char *pbuf;
	struct mutex *ttylock;
	size_t retval = 0;
	int curBufIndex;

	ttylock = &(cittyBuf->gMutex);
	F_ENTER();

	/* Enter critial section */
	if (mutex_lock_interruptible(ttylock))
		return -ERESTARTSYS;

	while (cittyBuf->iBufIn == cittyBuf->iBufOut) {
		mutex_unlock(ttylock);	/* release the lock */

		/* if (filp->f_flags & O_NONBLOCK) */
		/*      return -EAGAIN; */

		PDEBUG("%s reading: going to sleep with action %d",
		       current->comm, action);

		if (wait_event_interruptible
		    (cittyBuf->gInq, (cittyBuf->iBufIn != cittyBuf->iBufOut))) {
			/* waken up by signal, get the lock and process it */
			return -ERESTARTSYS;
		}
		/* otherwise loop, but first reacquire the lock */
		if (mutex_lock_interruptible(ttylock))
			return -ERESTARTSYS;
	}

	PDEBUG("There is something to read!");

	curBufIndex = cittyBuf->iBufOut++;
	pbuf = cittyBuf->pBuf[curBufIndex];

	/*
	 *  Check if it is flipped
	 */
	if (cittyBuf->iBufOut >= NUM_CITTY_BUF)
		cittyBuf->iBufOut = cittyBuf->iBufOut % NUM_CITTY_BUF;

	/* just to make sure */
	if (pbuf == NULL) {
		pr_debug("Nothing to read.\n");
		retval = 0;
		mutex_unlock(ttylock);
		/* return -EIO; */
		return 0;
	}

	/* read only up to the size of data or the buffer */
	count = cittyBuf->iDatalen[curBufIndex];
	if (count > CITTY_BUF_SIZE)
		count = CITTY_BUF_SIZE;

	/* #if 0 */
	if (action == COPY_TO_USER) {
		PDEBUG("%s: copy to user with count: %d and buf index %d",
		     __func__, count, curBufIndex);

		if (copy_to_user(buf, pbuf, count)) {
			mutex_unlock(ttylock);
			pr_err("%s: Copy to User failed.\n", __func__);
			return -EFAULT;
		}
	} else if (action == COPY_TO_CITTY) {

		PDEBUG("%s: This shouldn't be called in CI TTY", __func__);
	}

	retval = count;

	/* exit critical section */
	mutex_unlock(ttylock);

	/* finally, awake any writers and return */
	wake_up_interruptible(&cittyBuf->gOutq);
	PDEBUG("%s did read %li bytes", current->comm, (long)count);

	/* #endif */

	F_LEAVE();

	return retval;

}

/* How much space is free? */
static int spacefree(struct buf_struct *buffer)
{
	if (buffer->iBufOut == buffer->iBufIn)
		return (NUM_CITTY_BUF - 1) * CITTY_BUF_SIZE;

	return (((buffer->iBufOut + NUM_CITTY_BUF -
		  buffer->iBufIn) % NUM_CITTY_BUF) - 1) * CITTY_BUF_SIZE;
}

size_t write_citty_buffer(struct buf_struct *cittyBuf,
			  const char *buf, size_t count, short action)
{
	unsigned char *pbuf;
	struct mutex *ttylock;
	int curBufIndex;

	F_ENTER();

	/* make it a non-blocking write */
	if (spacefree(cittyBuf) == 0) {
		pr_err("%s warning: Write Buffer overflow.\n",
		       current->comm);
		return -EIO;
	}

	ttylock = &(cittyBuf->gMutex);
	if (mutex_lock_interruptible(ttylock))
		return -ERESTARTSYS;

	/* Make sure there's space to write */
	while (spacefree(cittyBuf) == 0) {	/* full */
		PDEBUG("%s Going to define wait:", current->comm);

		mutex_unlock(ttylock);	/* release the lock */

		/* if (filp->f_flags & O_NONBLOCK) */
		/*      return -EAGAIN; */

		PDEBUG("%s writing: going to sleep", current->comm);
		if (wait_event_interruptible
		    (cittyBuf->gOutq, spacefree(cittyBuf))) {
			/* waken up by signal, get the lock and process it */
			return -ERESTARTSYS;
		}

		if (mutex_lock_interruptible(ttylock))
			return -ERESTARTSYS;
	}

	curBufIndex = cittyBuf->iBufIn++;
	pbuf = cittyBuf->pBuf[curBufIndex];

	PDEBUG("%s Going to check flip", current->comm);
	/*
	 *  Check if it is flipped
	 */
	if (cittyBuf->iBufIn >= NUM_CITTY_BUF)
		cittyBuf->iBufIn = cittyBuf->iBufIn % NUM_CITTY_BUF;

	/* Check space */
	if (pbuf == NULL) {
		pr_warn("warning: Buffer overflowed.\n");
		mutex_unlock(ttylock);
		return -EIO;
	}

	/* ok, space is there, accept something */
	/* write only up to the size of the buffer */
	if (count > CITTY_BUF_SIZE) {
		count = CITTY_BUF_SIZE;
		pr_warn("warning: Buffer too size to write.\n");
	}

	if (action == COPY_FROM_USER) {
		PDEBUG("%s: going to copy_from_user at buf "
		     "index %d and count %d",
		     __func__, curBufIndex, count);
		if (copy_from_user((pbuf), buf, count)) {
			mutex_unlock(ttylock);
			return -EFAULT;
		}
	} else if (action == COPY_FROM_CITTY) {
		/* it is from the cinet_hard_start_xmit */
		PDEBUG("%s: going to COPY_FROM_CITTY at buf "
		     "index %d and count %d",
		     __func__, curBufIndex, count);
		memcpy(pbuf, buf, count);

	} else {
		pr_warn("undefined action.\n");
	}

	/* saving datalen */
	cittyBuf->iDatalen[curBufIndex] = count;

	mutex_unlock(ttylock);

	/* finally, awake any reader */
	/* blocked in read() and select() */
	wake_up_interruptible(&cittyBuf->gInq);

	F_LEAVE();

	return count;
}


static int citty_open(struct tty_struct *tty, struct file *file)
{
	struct citty_port *citty = NULL;
	int index = tty->index;

	F_ENTER();
	schedule();
	/* initialize the pointer in case something fails */
	tty->driver_data = NULL;

	/* get the serial object associated with this tty pointer */
	if (!cctdev_ready
	    || (cctdev_devices[index].nreaders == 0
		&& cctdev_devices[index].nwriters == 0))
		return -EAGAIN;

	mutex_lock(&mutex_lock_tty[index]);
	citty = citty_table[index];
	if (!citty) {
		citty = kmalloc(sizeof(*citty), GFP_KERNEL);
		if (!citty) {
			mutex_unlock(&mutex_lock_tty[index]);
			return -ENOMEM;
		}
		/* save tty_port structure within the our structure */
		citty_table[index] = citty;
		citty->port = &citty_port_table[index];
		citty->port->count = 0;
	}

	/* do the real open operation */
	/* save our structure within the tty structure */
	tty->driver_data = citty;

	/* vcy */
	tty->flags = TTY_NO_WRITE_SPLIT | tty->flags;

	/* save tty_struct within the tty_port structure */
	citty->port->tty = tty;

	++citty->port->count;
	if (citty->port->count == 1) {
		/* this is the first time this port is opened */
		/* do any hardware initialization needed here */
	}

	mutex_unlock(&mutex_lock_tty[index]);

	F_LEAVE();
	return 0;
}

static void citty_close(struct tty_struct *tty, struct file *file)
{
	struct citty_port *citty = NULL;
	int index = tty->index;

	F_ENTER();

	mutex_lock(&mutex_lock_tty[index]);
	citty = citty_table[index];
	/* Modified by Rovin Yu: release citty and related resource */
	if (citty) {
		if (!citty->port->count) {
			/* port was never opened */
			goto exit;
		}
		--citty->port->count;
		PDEBUG("citty_close: index is %d, count is %d\n",
				index, citty->port->count);
		if (citty->port->count <= 0) {
			citty_table[index] = NULL;
			kfree(citty);
		}
	}
exit:
	mutex_unlock(&mutex_lock_tty[index]);
	F_LEAVE();
}

static int citty_write(struct tty_struct *tty, const unsigned char *buffer,
		       int count)
{
	struct citty_port *citty = NULL;
	int index = tty->index;
	int retval = -EINVAL;

	F_ENTER();
	/* for some reason, this function is called with count == 0 */
	if (count <= 0) {
		pr_err("Error: count is %d.\n", count);
		return 0;
	}

	citty = citty_table[index];
	if (!citty) {
		PDEBUG("Warning: citty_write: citty is NULL\n");
		return -ENODEV;
	}

	if (!citty->port->count) {
		pr_err("Error: citty_write: port was not open\n");
		/* port was not opened */
		goto exit;
	}
#ifdef DEBUG_BUF_CONTENT
	/* int i; */
	pr_debug("CITTY Tx Buffer datalen is %d\n data:", count);
	for (i = 0; i < count; i++)
		pr_debug("%02x(%c)", buffer[i]&0xff, buffer[i]&0xff);
	/*    pr_debug(" %02x", buffer[i]&0xff ); */
	pr_debug("\n");
#endif

	/*
	 * packet is ready for transmission:
	 * write the packet to TxCitty buffer
	 */
	retval =
	    write_citty_buffer(&txCittyBuf[tty->index], buffer, count,
			       COPY_FROM_CITTY);

exit:
	F_LEAVE();

	return retval;
}

static int citty_write_room(struct tty_struct *tty)
{
	struct citty_port *citty = NULL;
	int index = tty->index;
	int room = -EINVAL;

	F_ENTER();

	mutex_lock(&mutex_lock_tty[index]);
	citty = citty_table[index];
	if (!citty) {
		mutex_unlock(&mutex_lock_tty[index]);
		return -ENODEV;
	}

	if (!citty->port->count) {
		PDEBUG("citty_write_room: no port is open.");
		/* port was not opened */
		goto exit;
	}

	/* calculate how much room is left in the device */
	/* CHECKPOINT */
	/* room = CITTY_BUF_SIZE * spacefree(  &txCittyBuf ); */
	room = CITTY_BUF_SIZE * spacefree(&txCittyBuf[tty->index]);

exit:
	mutex_unlock(&mutex_lock_tty[index]);

	F_LEAVE();

	return room;
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK | BRKINT | \
			IGNPAR | PARMRK | INPCK))

static void citty_set_termios(struct tty_struct *tty,
			      struct ktermios *old_termios)
{
	unsigned int cflag;

	F_ENTER();
	cflag = tty->termios.c_cflag;

	/* check that they really want us to change something */
	if (old_termios) {
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(tty->termios.c_iflag) ==
		     RELEVANT_IFLAG(old_termios->c_iflag))) {
			PDEBUG(" - nothing to change...\n");
			return;
		}
	}

	/* get the byte size */
	switch (cflag & CSIZE) {
	case CS5:
		PDEBUG(" - data bits = 5\n");
		break;
	case CS6:
		PDEBUG(" - data bits = 6\n");
		break;
	case CS7:
		PDEBUG(" - data bits = 7\n");
		break;
	default:
	case CS8:
		PDEBUG(" - data bits = 8\n");
		break;
	}

	/* determine the parity */
	if (cflag & PARENB)
		if (cflag & PARODD)
			PDEBUG(" - parity = odd\n");
		else
			PDEBUG(" - parity = even\n");
	else
		PDEBUG(" - parity = none\n");

	/* figure out the stop bits requested */
	if (cflag & CSTOPB)
		PDEBUG(" - stop bits = 2\n");
	else
		PDEBUG(" - stop bits = 1\n");

	/* figure out the hardware flow control settings */
	if (cflag & CRTSCTS)
		PDEBUG(" - RTS/CTS is enabled\n");
	else
		PDEBUG(" - RTS/CTS is disabled\n");

	/* determine software flow control */
	/* if we are implementing XON/XOFF, set the start and
	 * stop character in the device */
	if (I_IXOFF(tty) || I_IXON(tty)) {
		/* CHECKPOINT */
		/* Invalid code here;
		 * seems software flow control is not supported
		 */
#if 0
		unsigned char stop_char  = STOP_CHAR(tty);
		unsigned char start_char = START_CHAR(tty);

		/* if we are implementing INBOUND XON/XOFF */
		if (I_IXOFF(tty))
			PDEBUG(" - INBOUND XON/XOFF is enabled, "
			       "XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			PDEBUG(" - INBOUND XON/XOFF is disabled");

		/* if we are implementing OUTBOUND XON/XOFF */
		if (I_IXON(tty))
			PDEBUG(" - OUTBOUND XON/XOFF is enabled, "
			       "XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			PDEBUG(" - OUTBOUND XON/XOFF is disabled");
#endif
	}

	/* get the baud rate wanted */
	PDEBUG(" - baud rate = %d", tty_get_baud_rate(tty));

	F_LEAVE();
}

/* Our fake UART values */
#define MCR_DTR         0x01
#define MCR_RTS         0x02
#define MCR_LOOP        0x04
#define MSR_CTS         0x08
#define MSR_CD          0x10
#define MSR_RI          0x20
#define MSR_DSR         0x40
static int citty_tiocmget(struct tty_struct *tty)
{
	struct citty_port *citty = tty->driver_data;
	unsigned int result = 0;
	unsigned int msr = citty->msr;
	unsigned int mcr = citty->mcr;

	F_ENTER();

	result = ((mcr & MCR_DTR)  ? TIOCM_DTR  : 0) |  /* DTR is set */
		 ((mcr & MCR_RTS)  ? TIOCM_RTS  : 0) |  /* RTS is set */
		 ((mcr & MCR_LOOP) ? TIOCM_LOOP : 0) |  /* LOOP is set */
		 ((msr & MSR_CTS)  ? TIOCM_CTS  : 0) |  /* CTS is set */
		 ((msr & MSR_CD)   ? TIOCM_CAR  : 0) |  /* CD is set */
		 ((msr & MSR_RI)   ? TIOCM_RI   : 0) |  /* RI is set */
		 ((msr & MSR_DSR)  ? TIOCM_DSR  : 0);   /* DSR is set */

	F_LEAVE();
	return result;
}

static int citty_tiocmset(struct tty_struct *tty,
			  unsigned int set, unsigned int clear)
{
	struct citty_port *citty = tty->driver_data;
	unsigned int mcr = citty->mcr;

	F_ENTER();
	if (set & TIOCM_RTS)
		mcr |= MCR_RTS;
	if (set & TIOCM_DTR)
		mcr |= MCR_RTS;
	if (set & TIOCM_LOOP)
		mcr |= MCR_RTS;
	if (set & TIOCM_CTS)
		mcr |= MCR_RTS;
	if (set & TIOCM_RI)
		mcr |= MCR_RTS;
	if (set & TIOCM_DSR)
		mcr |= MCR_RTS;
	if (set & TIOCM_CAR)
		mcr |= MCR_RTS;

	if (clear & TIOCM_RTS)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_DTR)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_LOOP)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_CTS)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_CAR)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_RI)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_DSR)
		mcr &= ~MCR_RTS;

	/* set the new MCR value in the device */
	citty->mcr = mcr;

	F_LEAVE();
	return 0;
}

static ssize_t citty_read_proc(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct citty_port *citty = NULL;
	int length = 0;
	int i;
	char temp[256] = { 0 };
	int rc = -EFAULT;

	F_ENTER();

	for (i = 0; i < CITTY_TTY_MINORS; ++i) {
		citty = citty_table[i];
		if (citty == NULL)
			continue;

		length +=
		    snprintf(temp + length, sizeof(temp) - length, "%d,", i);
	}
	if (length > 0)
		temp[length - 1] = '\0';
	if (length + 1 > count) {
		pr_err("%s: error: no enough space.\n", __func__);
		goto err_exit;
	}
	if (copy_to_user(buf, (void *)&temp, length + 1)) {
		pr_err("%s: copy_to_user failed.\n", __func__);
		goto err_exit;
	}
	pr_debug("citty read proc: %s\n", temp);
	rc = 0;

err_exit:
	F_LEAVE();
	return rc;
}

static int citty_ioctl_tiocgserial(struct tty_struct *tty, struct file *file,
				   unsigned int cmd, unsigned long arg)
{
	struct citty_port *citty = tty->driver_data;

	F_ENTER();

	if (cmd == TIOCGSERIAL) {
		struct serial_struct tmp;

		if (!arg)
			return -EFAULT;

		memset(&tmp, 0, sizeof(tmp));

		tmp.type                = citty->serial.type;
		tmp.line                = citty->serial.line;
		tmp.port                = citty->serial.port;
		tmp.irq                 = citty->serial.irq;
		tmp.flags               = ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ;
		tmp.xmit_fifo_size      = citty->serial.xmit_fifo_size;
		tmp.baud_base           = citty->serial.baud_base;
		tmp.close_delay         = 5 * HZ;
		tmp.closing_wait        = 30 * HZ;
		tmp.custom_divisor      = citty->serial.custom_divisor;
		tmp.hub6                = citty->serial.hub6;
		tmp.io_type             = citty->serial.io_type;

		if (copy_to_user
		    ((void __user *)arg, &tmp, sizeof(struct serial_struct)))
			return -EFAULT;
		return 0;
	}

	F_LEAVE();
	return -ENOIOCTLCMD;
}

static int citty_ioctl_tiocmiwait(struct tty_struct *tty, struct file *file,
				  unsigned int cmd, unsigned long arg)
{
	struct citty_port *citty = tty->driver_data;

	F_ENTER();

	if (cmd == TIOCMIWAIT) {
		DECLARE_WAITQUEUE(wait, current);
		struct async_icount cnow;
		struct async_icount cprev;

		cprev = citty->icount;
		while (1) {
			add_wait_queue(&citty->wait, &wait);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			remove_wait_queue(&citty->wait, &wait);

			/* see if a signal woke us up */
			if (signal_pending(current))
				return -ERESTARTSYS;

			cnow = citty->icount;
			if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
			    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
				return -EIO;	/* no change => error */
			if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
			    ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
			    ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
			    ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts))) {
				return 0;
			}
			cprev = cnow;
		}

	}

	F_LEAVE();
	return -ENOIOCTLCMD;
}

static int citty_ioctl_tiocgicount(struct tty_struct *tty, struct file *file,
				   unsigned int cmd, unsigned long arg)
{
	struct citty_port *citty = tty->driver_data;

	F_ENTER();
	if (cmd == TIOCGICOUNT) {
		struct async_icount cnow = citty->icount;
		struct serial_icounter_struct icount;

		icount.cts      = cnow.cts;
		icount.dsr      = cnow.dsr;
		icount.rng      = cnow.rng;
		icount.dcd      = cnow.dcd;
		icount.rx       = cnow.rx;
		icount.tx       = cnow.tx;
		icount.frame    = cnow.frame;
		icount.overrun  = cnow.overrun;
		icount.parity   = cnow.parity;
		icount.brk      = cnow.brk;
		icount.buf_overrun = cnow.buf_overrun;

		if (copy_to_user((void __user *)arg, &icount, sizeof(icount)))
			return -EFAULT;
		return 0;
	}

	F_LEAVE();
	return -ENOIOCTLCMD;
}

/* the real citty_ioctl function.
 * The above is done to get the small functions*/
static int citty_ioctl(struct tty_struct *tty,
		       unsigned int cmd, unsigned long arg)
{
	struct file *file = NULL;
	F_ENTER();

	switch (cmd) {
	case TIOCGSERIAL:
		return citty_ioctl_tiocgserial(tty, file, cmd, arg);
	case TIOCMIWAIT:
		return citty_ioctl_tiocmiwait(tty, file, cmd, arg);
	case TIOCGICOUNT:
		return citty_ioctl_tiocgicount(tty, file, cmd, arg);
#ifndef TCGETS2
	case TCSETS:
		if (user_termios_to_kernel_termios(&tty->termios,
				(struct termios __user *) arg))
			return -EFAULT;
		else
			return 0;
	case TCGETS:
		if (kernel_termios_to_user_termios(
				(struct termios __user *)arg, &tty->termios))
			return -EFAULT;
		else
			return 0;
#else
	case TCSETS:
		if (user_termios_to_kernel_termios_1(&tty->termios,
				(struct termios __user *) arg))
			return -EFAULT;
		else
			return 0;
	case TCSETS2:
		if (user_termios_to_kernel_termios(&tty->termios,
				(struct termios2 __user *) arg))
			return -EFAULT;
		else
			return 0;
	case TCGETS:
		if (kernel_termios_to_user_termios_1(
				(struct termios __user *)arg, &tty->termios))
			return -EFAULT;
		else
			return 0;
	case TCGETS2:
		if (kernel_termios_to_user_termios(
				(struct termios2 __user *)arg, &tty->termios))
			return -EFAULT;
		else
			return 0;
#endif
	case TCSETSF:             /* 0x5404 */
	case TCSETAF:             /* 0x5408 */

		return 0;         /* has to return zero for qtopia to work */
	default:
		PDEBUG("citty_ioctl cmd: %d.\n", cmd);
		return -ENOIOCTLCMD;           /* for PPPD to work? */

		break;
	}

	F_LEAVE();

}

static int citty_chars_in_buffer(struct tty_struct *tty)
{
	/* (NUM_CITTY_BUF-spacefree(txCittyBuf[tty->index]))*CITTY_BUF_SIZE; */
	return 0;
}

static void citty_flush_chars(struct tty_struct *tty)
{
	F_ENTER();
	return;
}

static void citty_wait_until_sent(struct tty_struct *tty, int timeout)
{
	F_ENTER();
	return;
}

static void citty_throttle(struct tty_struct *tty)
{
	F_ENTER();
	return;
}

static void citty_unthrottle(struct tty_struct *tty)
{
	F_ENTER();
	return;
}

static void citty_stop(struct tty_struct *tty)
{
	F_ENTER();
	return;
}

static void citty_start(struct tty_struct *tty)
{
	F_ENTER();
	return;
}

static void citty_hangup(struct tty_struct *tty)
{
	F_ENTER();
	return;
}

static void citty_flush_buffer(struct tty_struct *tty)
{
	F_ENTER();
	return;
}

static void citty_set_ldisc(struct tty_struct *tty)
{
	F_ENTER();
	return;
}

static const struct file_operations citty_proc_ops = {
	.read = citty_read_proc,
};

static const struct tty_operations serial_ops = {
	.open			= citty_open,
	.close			= citty_close,
	.write			= citty_write,
	.write_room		= citty_write_room,
	.set_termios		= citty_set_termios,
	.proc_fops              = &citty_proc_ops,
	.tiocmget		= citty_tiocmget,
	.tiocmset		= citty_tiocmset,
	.ioctl			= citty_ioctl,
	/* for PPPD to work chars_in_buffer needs to return zero. */
	/* uncomment this, minicom works */
	.chars_in_buffer	= citty_chars_in_buffer,
	.flush_chars		= citty_flush_chars,
	.wait_until_sent	= citty_wait_until_sent,
	.throttle		= citty_throttle,
	.unthrottle		= citty_unthrottle,
	.stop			= citty_stop,
	.start			= citty_start,
	.hangup			= citty_hangup,
	.flush_buffer		= citty_flush_buffer,
	.set_ldisc		= citty_set_ldisc,

};

static struct tty_driver *citty_tty_driver;

/*******************************************************************
*  FUNCTION: cctdev_read
*
*  DESCRIPTION: To pass the data from TxBuffer to CI
*
*  RETURNS: utlFAILED or utlSUCCESS
*
*******************************************************************/
ssize_t cctdev_read(struct file *filp, char __user *buf, size_t count,
		    loff_t *f_pos)
{
	struct cctdev_dev *dev = filp->private_data;
	ssize_t retval = 0;
	int minor_num;

	F_ENTER();

	/* Extract Minor Number */
	minor_num = MINOR(dev->cdev.dev);

	PDEBUG("txCittyBuf[%d].iBufOut: %d, iBufIn: %d\n", minor_num,
	       txCittyBuf[minor_num].iBufOut, txCittyBuf[minor_num].iBufIn);

	retval = read_citty_buffer(buf, &txCittyBuf[minor_num], COPY_TO_USER);

	F_LEAVE();

	return retval;
}

/*******************************************************************
*  FUNCTION: cctdev_write
*
*  DESCRIPTION: Once data come, push it to the tty port
*
*
*  RETURNS: utlFAILED or utlSUCCESS
*
*******************************************************************/
ssize_t cctdev_write(struct file *filp, const char __user *buf, size_t count,
		     loff_t *f_pos)
{
	struct cctdev_dev *dev = filp->private_data;
	int minor_num;
	int c;
	char *tbuf = NULL;

	/* send the data to the tty layer for users to read.
	 * This doesn't
	 * actually push the data through unless tty->low_latency is set */
	struct citty_port *citty = NULL;
	struct tty_struct *tty = NULL;

	F_ENTER();
	/* Extract Minor Number */
	minor_num = MINOR(dev->cdev.dev);
	PDEBUG("cctdev_write: minor_num:%d\n", minor_num);

	mutex_lock(&mutex_lock_tty[minor_num]);

	/*
	 *   get the serial object associated with this tty pointer
	 *   always give the data to the first open Dev.
	 */
	citty = citty_table[minor_num];
	PDEBUG("cctdev_write: citty: 0x%x, minor_num:%d\n",
		(unsigned long)citty, minor_num);

	if (!citty) {
		PDEBUG("Warning: citty is NULL.\n");
		mutex_unlock(&mutex_lock_tty[minor_num]);
		return -ENODEV;
	}

	tty = citty->port->tty;

	if (!tty) {
		mutex_unlock(&mutex_lock_tty[minor_num]);
		return -ENODEV;
	}

	tbuf = memdup_user(buf, count);
	if (IS_ERR(tbuf)) {
		pr_warn("%s: memdup_user returned error[%ld]\n",
			__func__, PTR_ERR(tbuf));
		mutex_unlock(&mutex_lock_tty[minor_num]);
		return PTR_ERR(tbuf);
	}

	c = tty->ldisc->ops->receive_buf2(tty, tbuf, NULL, count);
	kfree(tbuf);
	mutex_unlock(&mutex_lock_tty[minor_num]);
	return c;
}

static int cctdev_fasync(int fd, struct file *filp, int mode)
{
	struct cctdev_dev *dev = filp->private_data;

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

/*
 * The ioctl() implementation
 */
long cctdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int retval = 0;

	return retval;

}

unsigned int cctdev_poll(struct file *filp, poll_table *wait)
{
	struct cctdev_dev *dev = filp->private_data;
	int minor_num;
	unsigned int mask = 0;

	F_ENTER();

	minor_num = MINOR(dev->cdev.dev);

	mutex_lock(&txCittyBuf[minor_num].gMutex);

	poll_wait(filp, &txCittyBuf[minor_num].gInq, wait);

	if (txCittyBuf[minor_num].iBufOut != txCittyBuf[minor_num].iBufIn)
		mask |= POLLIN | POLLRDNORM;

	mask |= POLLOUT | POLLWRNORM;

	mutex_unlock(&txCittyBuf[minor_num].gMutex);

	F_LEAVE();

	return mask;

}

/*
 * Open and close
 */

int cctdev_open(struct inode *inode, struct file *filp)
{
	struct cctdev_dev *dev;	/* device information */

	F_ENTER();

	dev = container_of(inode->i_cdev, struct cctdev_dev, cdev);

	filp->private_data = dev;	/* for other methods */

	/* used to keep track of how many readers */
	mutex_lock(&dev->lock);
	if (filp->f_mode & FMODE_READ)
		dev->nreaders++;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters++;
	mutex_unlock(&dev->lock);

	F_LEAVE();

	return nonseekable_open(inode, filp);	/* success */
}

int cctdev_release(struct inode *inode, struct file *filp)
{

	struct cctdev_dev *dev = filp->private_data;

	/* remove this filp from the asynchronously notified filp's */
	cctdev_fasync(-1, filp, 0);
	mutex_lock(&dev->lock);
	if (filp->f_mode & FMODE_READ)
		dev->nreaders--;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters--;
	if (dev->nreaders == 0 && dev->nwriters == 0) {
		int minor_num = MINOR(dev->cdev.dev);
		struct citty_port *citty;
		mutex_lock(&mutex_lock_tty[minor_num]);
		citty = citty_table[minor_num];
		if (citty && citty->port->tty)
			tty_hangup(citty->port->tty);
		mutex_unlock(&mutex_lock_tty[minor_num]);
	}
	mutex_unlock(&dev->lock);

	return 0;
}

const struct file_operations cctdev_fops = {
	.owner		=    THIS_MODULE,
	.read		=     cctdev_read,
	.write		=    cctdev_write,
	.unlocked_ioctl		=    cctdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cctdev_ioctl,
#endif
	.open		=     cctdev_open,
	.release	=  cctdev_release,
	.fasync		=       cctdev_fasync,
	.llseek		=       no_llseek,
	.poll		=     cctdev_poll,
};

/*
 * Finally, the module stuff
 */

static struct class *cctdev_class;

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void cctdev_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(cctdev_major, cctdev_minor);

	/* Get rid of our char dev entries */
	if (cctdev_devices) {
		for (i = 0; i < cctdev_nr_devs; i++) {
			cdev_del(&cctdev_devices[i].cdev);
			device_destroy(cctdev_class,
				       MKDEV(cctdev_major, cctdev_minor + i));
		}
		kfree(cctdev_devices);
	}
	class_destroy(cctdev_class);

#ifdef cctdev_DEBUG		/* use proc only if debugging */
	cctdev_remove_proc();
#endif

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, cctdev_nr_devs);

}

/*
 * Set up the char_dev structure for this device.
 */
static void cctdev_setup_cdev(struct cctdev_dev *dev, int index)
{
	int err, devno = MKDEV(cctdev_major, cctdev_minor + index);

	F_ENTER();

	cdev_init(&dev->cdev, &cctdev_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &cctdev_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		pr_notice("Error %d adding cctdev%d", err, index);

	F_LEAVE();

}

int cctdev_init_module(void)
{
	int result, i;
	dev_t dev = 0;
	char name[256];

	F_ENTER();

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (cctdev_major) {
		dev = MKDEV(cctdev_major, cctdev_minor);
		result = register_chrdev_region(dev, cctdev_nr_devs, "cctdev");
	} else {
		result = alloc_chrdev_region(&dev, cctdev_minor, cctdev_nr_devs,
					     "cctdev");
		cctdev_major = MAJOR(dev);
	}

	if (result < 0) {
		pr_warn("cctdev: can't get major %d\n",
		       cctdev_major);
		return result;
	}

	/*
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	cctdev_devices =
	    kzalloc(cctdev_nr_devs * sizeof(struct cctdev_dev), GFP_KERNEL);
	if (!cctdev_devices) {
		result = -ENOMEM;
		goto fail;	/* Make this more graceful */
	}

	cctdev_class = class_create(THIS_MODULE, "cctdev");
	/* Initialize each device. */
	for (i = 0; i < cctdev_nr_devs; i++) {
		sprintf(name, "%s%d", "cctdev", i);
		mutex_init(&cctdev_devices[i].lock);
		cctdev_setup_cdev(&cctdev_devices[i], i);
		device_create(cctdev_class, NULL,
			      MKDEV(cctdev_major, cctdev_minor + i), NULL,
			      name);
	}

	/* At this point call the init function for any friend device */
	dev = MKDEV(cctdev_major, cctdev_minor + cctdev_nr_devs);

#ifdef cctdev_DEBUG		/* only when debugging */
	cctdev_create_proc();
#endif

	F_LEAVE();
	cctdev_ready = 1;
	return 0;		/* succeed */

fail:
	cctdev_cleanup_module();

	return result;
}

static int __init citty_init(void)
{
	int retval;
	int i;

	F_ENTER();

	/* allocate the tty driver */
	citty_tty_driver = alloc_tty_driver(CITTY_TTY_MINORS);
	if (!citty_tty_driver)
		return -ENOMEM;

	/* initialize the tty driver */
	citty_tty_driver->owner = THIS_MODULE;
	citty_tty_driver->driver_name = "citty_tty";
	citty_tty_driver->name = "citty";
	citty_tty_driver->major = CITTY_TTY_MAJOR;
	citty_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	citty_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	citty_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	citty_tty_driver->init_termios = tty_std_termios;
	/* B115200 | CS8 | CREAD | HUPCL | CLOCAL; */
	citty_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | CLOCAL;
	citty_tty_driver->init_termios.c_iflag = IGNBRK | IGNCR | IGNPAR;
	citty_tty_driver->init_termios.c_oflag = 0;
	citty_tty_driver->init_termios.c_lflag = 0;

	tty_set_operations(citty_tty_driver, &serial_ops);

	/* register the tty driver */
	retval = tty_register_driver(citty_tty_driver);
	if (retval) {
		pr_err("failed to register citty tty driver");
		put_tty_driver(citty_tty_driver);
		citty_tty_driver = NULL;
		return retval;
	}

	/* register tty devices */
	for (i = 0; i < CITTY_TTY_MINORS; ++i) {
		/* Init buffer */
		cci_init_buffer(&txCittyBuf[i]);
		mutex_init(&mutex_lock_tty[i]);

		tty_port_init(&citty_port_table[i]);
		tty_port_register_device(&citty_port_table[i],
				citty_tty_driver, i, NULL);
	}

	pr_info(DRIVER_DESC " " DRIVER_VERSION "\n");

	cctdev_init_module();

	F_LEAVE();
	return retval;
}

static void __exit citty_exit(void)
{
	struct citty_port *citty = NULL;
	int i;

	F_ENTER();

	/* unregister devices  */
	for (i = 0; i < CITTY_TTY_MINORS; ++i) {
		mutex_lock(&mutex_lock_tty[i]);
		citty = citty_table[i];
		if (citty) {
			citty_table[i] = NULL;
			kfree(citty);
		}
		mutex_unlock(&mutex_lock_tty[i]);
		tty_port_destroy(&citty_port_table[i]);
		tty_unregister_device(citty_tty_driver, i);
		cci_free_buffer(&txCittyBuf[i]);
	}

	/* unregister driver */
	tty_unregister_driver(citty_tty_driver);
	put_tty_driver(citty_tty_driver);

	/* clean up for the cctdev stuff */
	cctdev_cleanup_module();

	F_LEAVE();
}

/* End handling I/O */

module_init(citty_init);
module_exit(citty_exit);
