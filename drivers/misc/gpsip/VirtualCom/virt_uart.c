/******************************************************************************
 *                                LEGAL NOTICE                                *
 *                                                                            *
 *  USE OF THIS SOFTWARE (including any copy or compiled version thereof) AND *
 *  DOCUMENTATION IS SUBJECT TO THE SOFTWARE LICENSE AND RESTRICTIONS AND THE *
 *  WARRANTY DISLCAIMER SET FORTH IN LEGAL_NOTICE.TXT FILE. IF YOU DO NOT     *
 *  FULLY ACCEPT THE TERMS, YOU MAY NOT INSTALL OR OTHERWISE USE THE SOFTWARE *
 *  OR DOCUMENTATION.                                                         *
 *  NOTWITHSTANDING ANYTHING TO THE CONTRARY IN THIS NOTICE, INSTALLING OR    *
 *  OTHERISE USING THE SOFTWARE OR DOCUMENTATION INDICATES YOUR ACCEPTANCE OF *
 *  THE LICENSE TERMS AS STATED.                                              *
 *                                                                            *
 ******************************************************************************/
/* Version: 1.8.9\3686 */
/* Build  : 13 */
/* Date   : 12/08/2012 */

#define __VIRT_UART_C__


#include <linux/module.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/hardirq.h>
#include <linux/serial.h>
#include <linux/spinlock.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/sched.h>
#include <linux/wait.h>

#define MODULE_NAME		"virt_uart"
#define DRIVER_NAME		"virt_uart"
#define DEVICE_NAME		"ttyV"

/* 0 = dynamic major */
#define XVU_MAJOR		0

/* Number of Virtual UART pairs */
#define XVU_PAIRS		1

#define XVU_ERR( fmt, arg...) do { printk(KERN_ERR   MODULE_NAME " %s:%d [%s]... " fmt, __FILE__, __LINE__, __func__, ## arg ); } while (0)

#if defined(CGTEST) || defined(DEBUG)
#  define XVU_DBG( fmt, arg...) do { printk(KERN_ERR   MODULE_NAME " %s:%d [%s]... " fmt, __FILE__, __LINE__, __func__, ## arg ); } while (0)
#else
#  define XVU_DBG( fmt, arg...) do { } while (0)
#endif

typedef struct xvu_endpoint_s {
	int				open_count;
	struct tty_struct		*tty;
	int				mcr; /* Modem Control Register */
	int				msr; /* Modem Status Register  */
	wait_queue_head_t		msr_wait;
	spinlock_t			sercnt_lock;
	struct serial_icounter_struct	ser_cnt;
} xvu_endpoint_t;

typedef struct xvu_dev_s {
	struct semaphore	sem;
	xvu_endpoint_t		endpoint[XVU_PAIRS * 2];
} xvu_dev_t;


static struct tty_driver	*xvu_ttydriver = NULL;
static xvu_dev_t		xvu_dev;
static struct tty_port xvu_port[2];


#define __XVU_GetPeerIdx(idx) ( (idx) ^ 0x01 )

//bxd
#define init_MUTEX(sem)    sema_init(sem, 1)

/*
 * __XVU_SetMSR
 *
 * This sets the MSR (Modem Status Register), upon its peer's MCR (Modem Control
 * Register).
 * It also updates counters accordingly, and awakes potential TIOCMIWAITers.
 * **** It should be called with the xvu_dev.sem held ! ****
 */
static void	__XVU_SetMSR( unsigned int idx )
{
	unsigned int	peermcr = 0;
	unsigned int	oldmsr = 0;
	unsigned int	msr = 0;
	unsigned long	flags = 0;

	peermcr = xvu_dev.endpoint[__XVU_GetPeerIdx(idx)].mcr;
	msr = oldmsr = xvu_dev.endpoint[idx].msr;

	XVU_DBG( "msr[%d] = 0x%08x\n", idx, msr );

	/* Peer is closed, set msr to 0 */
	if ( !xvu_dev.endpoint[__XVU_GetPeerIdx(idx)].open_count) {
		XVU_DBG( "Peer(%d) is closed... Setting msr as 0\n", __XVU_GetPeerIdx(idx) );
		msr = 0;
		goto out;
	}

	/*
	 * Peer is open.
	 * Update MSR upon peer's MCR
	 */
	if ( peermcr & TIOCM_DTR )
		msr |= (TIOCM_DSR | TIOCM_CAR);
	else
		msr &= ~(TIOCM_DSR | TIOCM_CAR);

	if ( peermcr & TIOCM_RTS )
		msr |= (TIOCM_CTS);
	else
		msr &= ~(TIOCM_CTS);

out:
	xvu_dev.endpoint[idx].msr = msr;

	/* Mask transitions (unchanged bits will be 0) */
	msr ^= oldmsr;
	if (msr) {
		XVU_DBG( "Transitioning...\n" );
		/* Update counters for TIOCGICOUNT */
		spin_lock_irqsave(&xvu_dev.endpoint[idx].sercnt_lock, flags);
		if ( msr & TIOCM_DSR ) xvu_dev.endpoint[idx].ser_cnt.dsr++;
		if ( msr & TIOCM_CAR ) xvu_dev.endpoint[idx].ser_cnt.dcd++;
		if ( msr & TIOCM_CTS ) xvu_dev.endpoint[idx].ser_cnt.cts++;
		spin_unlock_irqrestore(&xvu_dev.endpoint[idx].sercnt_lock, flags);

		/* MSR changed. Awake TIOCMIWAITers */
		wake_up_interruptible(&xvu_dev.endpoint[idx].msr_wait);
	}
}


/*
 * __XVU_SetMCR
 *
 * This sets the MCR (Modem Control Register), and updates its peer's MSR (Modem
 * Status Register).
 * **** It should be called with the xvu_dev.sem held ! ****
 */
static void	__XVU_SetMCR( unsigned int idx, unsigned int val )
{
	/* Set MSR */
	xvu_dev.endpoint[idx].mcr = val;

	XVU_DBG( "Setting MCR[%d]=0x%08x\n", idx, val );

	/* Update peer's MSR */
	__XVU_SetMSR( __XVU_GetPeerIdx(idx) );
}

static int	XVU_write(struct tty_struct *tty, const unsigned char *buffer,
					  int count)
{
	int			idx = 0;
	struct tty_struct	*peer = NULL;
	int			rv = -EINVAL;
	unsigned long		flags = 0;

	if (down_interruptible(&xvu_dev.sem))
		return -ERESTARTSYS;

	if (tty == NULL) {
		XVU_ERR( "Invalid (NULL) tty_struct\n");
		goto out;
	}

	if (buffer == NULL) {
		XVU_ERR( "Invalid (NULL) buffer\n");
		goto out;
	}

	if (count <= 0 ) {
		XVU_ERR( ": Invalid count = 0\n");
		goto out;
	}

	idx = tty->index;

	if ( (idx<0) || (idx >= XVU_PAIRS*2) || (idx != (int)tty->driver_data) ) {
		XVU_ERR( "Index %d out of range.\n", idx);
		goto out;
	}


	/* Is endpoint open ? */
	if (! xvu_dev.endpoint[idx].open_count ) {
		XVU_ERR( "Endpoint %d doesn't seem to be open.\n", idx);
		goto out;
	}

	/*
	* Send string to peer
	* If peer is closed, discard the string...
	* ...but return count as if we sent it.
	*/

	/* Is peer open ? */
	if (! xvu_dev.endpoint[__XVU_GetPeerIdx(idx)].open_count ) {
		/* Discard sent bytes ! */
		rv = count;
		goto out;
	}

	/* Safety ! Shouldn't happen !!! */
	peer = xvu_dev.endpoint[__XVU_GetPeerIdx(idx)].tty;
	if (! peer ) {
		/* Discard sent bytes ! */
		XVU_ERR( "endpoint[%d].tty is NULL\n", __XVU_GetPeerIdx(idx));
		rv = count;
		goto out;
	}

	rv = tty_buffer_request_room(tty->port, count);
	if (unlikely(rv == 0)) {
		XVU_ERR("tty_buffer_request_room rv:%d\n",rv);
	}
	rv = tty_insert_flip_string(peer->port, buffer, count);
	tty_schedule_flip(peer->port);

out:
	/* Update counters */
	if (rv > 0) {
		spin_lock_irqsave(&xvu_dev.endpoint[idx].sercnt_lock, flags);
		xvu_dev.endpoint[ idx ].ser_cnt.tx += rv;
		spin_unlock_irqrestore(&xvu_dev.endpoint[idx].sercnt_lock, flags);

		spin_lock_irqsave(&xvu_dev.endpoint[__XVU_GetPeerIdx(idx)].sercnt_lock, flags);
		xvu_dev.endpoint[__XVU_GetPeerIdx(idx)].ser_cnt.rx += rv;
		spin_unlock_irqrestore(&xvu_dev.endpoint[__XVU_GetPeerIdx(idx)].sercnt_lock, flags);
	}

	up(&xvu_dev.sem);
	return rv;
}


static int	XVU_open(struct tty_struct *tty, struct file *filp)
{
	int	idx = 0;
	int	rv  = -EINVAL;
	int	idx2  = 0;

	if (down_interruptible(&xvu_dev.sem))
		return -ERESTARTSYS;

	if (tty == NULL) {
		XVU_ERR( ": Invalid (NULL) tty_struct\n");
		goto out;
	}

	idx = tty->index;
	idx2 = (int)(idx/2);

	if ( (idx<0) || (idx >= XVU_PAIRS*2) ) {
		XVU_ERR( "Index %d out of range.\n", idx);
		goto out;
	}

	tty->driver_data = (void *)idx;

	/* Safety ! Shouldn't happen !!! */
	if ( (xvu_dev.endpoint[idx].tty) && (xvu_dev.endpoint[idx].tty != tty) ) {
		XVU_ERR( "Endpoint[%d].tty=%p != tty=%p\n",
				idx, xvu_dev.endpoint[idx].tty, tty);
	}

	xvu_dev.endpoint[idx].tty = tty;
	xvu_dev.endpoint[idx].open_count++;

	XVU_DBG( "Opening %d(count=%d)... TTY=%p\n", idx, xvu_dev.endpoint[idx].open_count, tty );

	__XVU_SetMCR(idx, TIOCM_DTR | TIOCM_RTS);
	rv = 0;

out:
	up(&xvu_dev.sem);


	return rv;
}


static void	XVU_close(struct tty_struct *tty, struct file *filp)
{
	int	idx = 0;
	int	idx2  = 0;
	down(&xvu_dev.sem);

	if (tty == NULL) {
		XVU_ERR( "Invalid (NULL) tty_struct\n");
		goto out;
	}

	idx = tty->index;

	if ( (idx<0) || (idx >= XVU_PAIRS*2) || (idx != (int)tty->driver_data) ) {
		XVU_ERR( "Index %d out of range.\n", idx);
		goto out;
	}

	/* Is endpoint open ? */
	if (! xvu_dev.endpoint[idx].open_count ) {
		XVU_ERR( "Endpoint %d doesn't seem to be open.\n", idx);
		goto out;
	}

	XVU_DBG( "Closing %d\n", idx );
	xvu_dev.endpoint[idx].open_count--;

	idx2 = (int)(idx/2);
	if ((xvu_dev.endpoint[idx2].open_count <= 0) && (xvu_dev.endpoint[idx2+1].open_count <= 0)) {
		/* No more users holding this device */
		XVU_ERR( "No more users holding this device\n");
		xvu_dev.endpoint[idx].tty = NULL;
		wake_up_interruptible(&xvu_dev.endpoint[idx].msr_wait);
		__XVU_SetMCR(idx, 0);
	}

out:
	up(&xvu_dev.sem);
}


static int	XVU_write_room(struct tty_struct *tty)
{
	int			idx = 0;
	struct tty_struct	*peer = NULL;
	struct tty_buffer	*b = NULL;
	unsigned long		flags = 0;
	int			rv = -EINVAL;

	down(&xvu_dev.sem);

	if (tty == NULL) {
		XVU_ERR( "Invalid (NULL) tty_struct\n");
		goto out;
	}

	idx = tty->index;

	if ( (idx<0) || (idx >= XVU_PAIRS*2) || (idx != (int)tty->driver_data) ) {
		XVU_ERR( "Index %d out of range.\n", idx);
		goto out;
	}

	/* is endpoint open ? */
	if (! xvu_dev.endpoint[idx].open_count ) {
		XVU_ERR( "Endpoint %d doesn't seem to be open.\n", idx);
		goto out;
	}

	/* Is peer open ? */
	if (! xvu_dev.endpoint[__XVU_GetPeerIdx(idx)].open_count ) {
		/* We'll discard sent bytes anyway, so... lie ! */
		rv = 255;
		goto out;
	}

	/* Safety ! Shouldn't happen !!! */
	peer = xvu_dev.endpoint[__XVU_GetPeerIdx(idx)].tty;
	if (! peer ) {
		/* We'll discard sent bytes anyway, so... lie ! */
		XVU_ERR( "Endpoint[%d].tty is NULL\n", __XVU_GetPeerIdx(idx));
		rv = 255;
		goto out;
	}

	/*
	 * Calculate how much room is left in the peer buffer
	 */
	spin_lock_irqsave(&peer->port->buf.lock, flags);

	b = peer->port->buf.tail;
	if (b != NULL)
		rv = b->size - b->used;
	else
		rv = 0;

	spin_unlock_irqrestore(&peer->port->buf.lock, flags);

out:
	up(&xvu_dev.sem);

	return rv;
}


/*
 * Needed to avoid Oops on poll !
 */
static int	XVU_chars_in_buffer(struct tty_struct *tty)
{
	return 0;
}


//static int	XVU_tiocmget(struct tty_struct *tty, struct file *filp)
static int	XVU_tiocmget(struct tty_struct *tty)
{
	int		idx = 0;
	int		rv = -EINVAL;

	if (down_interruptible(&xvu_dev.sem))
		return -ERESTARTSYS;

	if (tty == NULL) {
		XVU_ERR( "Invalid (NULL) tty_struct\n");
		goto out;
	}

	idx = tty->index;

	if ( (idx<0) || (idx >= XVU_PAIRS*2) || (idx != (int)tty->driver_data) ) {
		XVU_ERR( ": Index %d out of range.\n", idx);
		goto out;
	}

	rv = (xvu_dev.endpoint[idx].mcr | xvu_dev.endpoint[idx].msr);

	XVU_DBG( "In tiocmget(%d) returning: 0x%08x\n", idx, rv );

out:
	up(&xvu_dev.sem);

	return rv;
}


//static int	XVU_tiocmset(struct tty_struct *tty, struct file *filp,
//		unsigned int set, unsigned int clear)
static int	XVU_tiocmset(struct tty_struct *tty,
		unsigned int set, unsigned int clear)

{
	int		idx = 0;

	if (tty == NULL) {
		XVU_ERR( "Invalid (NULL) tty_struct\n");
		return -EINVAL;
	}

	idx = tty->index;

	if ( (idx<0) || (idx >= XVU_PAIRS*2) || (idx != (int)tty->driver_data) ) {
		XVU_ERR( "Index %d out of range.\n", idx);
		return -EINVAL;
	}

	/* Only accept RTS and DTR flag changes */
	set   &= (TIOCM_RTS | TIOCM_DTR);
	clear &= (TIOCM_RTS | TIOCM_DTR);

	XVU_DBG( "In tiocmset(%d) SET=0x%08x CLR=0x%08x\n", idx, set, clear );

	if (down_interruptible(&xvu_dev.sem))
		return -ERESTARTSYS;
	__XVU_SetMCR(  idx, ( (xvu_dev.endpoint[idx].mcr | set) & (~clear) )  );
	up(&xvu_dev.sem);

	return 0;
}


static int	__XVU_tiocmiwait(struct tty_struct *tty, unsigned long arg)
{
	int				idx = 0;
	int				rv= -EINVAL;
	struct serial_icounter_struct	cnow = {0};
	struct serial_icounter_struct	cprev = {0};
	unsigned long			flags = 0;

	if (tty == NULL) {
		XVU_ERR( "Invalid (NULL) tty_struct\n");
		return -EINVAL;
	}

	idx = tty->index;

	if ( (idx<0) || (idx >= XVU_PAIRS*2) || (idx != (int)tty->driver_data) ) {
		XVU_ERR( "Index %d out of range.\n", idx);
		return -EINVAL;
	}

	spin_lock_irqsave(&xvu_dev.endpoint[idx].sercnt_lock, flags);
	cnow = xvu_dev.endpoint[idx].ser_cnt;

	XVU_DBG( "In tiocmiwait(%d) waiting: 0x%08lx now={CTS=%d, DCD=%d, RNG=%d, DSR=%d}\n", idx, arg, cnow.cts, cnow.dcd, cnow.rng, cnow.dsr );

	spin_unlock_irqrestore(&xvu_dev.endpoint[idx].sercnt_lock, flags);

	rv = wait_event_interruptible( xvu_dev.endpoint[idx].msr_wait, ({
		cprev = cnow;
		spin_lock_irqsave(&xvu_dev.endpoint[idx].sercnt_lock, flags);
		cnow = xvu_dev.endpoint[idx].ser_cnt;
		spin_unlock_irqrestore(&xvu_dev.endpoint[idx].sercnt_lock, flags);

		/* Device closed or serial counters changed */
		(xvu_dev.endpoint[idx].open_count == 0)        ||
		((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ||
		((arg & TIOCM_CAR) && (cnow.dcd != cprev.dcd)) ||
		((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
		((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr));
	}));

	XVU_DBG( "In tiocmiwait(%d) done wait: 0x%08lx now={CTS=%d, DCD=%d, RNG=%d, DSR=%d}\n", idx, arg, cnow.cts, cnow.dcd, cnow.rng, cnow.dsr );

	if (rv < 0) {
		XVU_DBG( "In tiocmiwait(%d) rv=%d, returning: ERESTARTSYS\n", idx, rv );
		return -ERESTARTSYS;
	}

	/*
	 * return -EIO if we were awoken by device being closed
	 */
	if (xvu_dev.endpoint[idx].open_count == 0) {
		XVU_DBG( "In tiocmiwait(%d) returning: EIO\n", idx );
		return -EIO;
	}

	return 0;
}


//static int	XVU_ioctl(struct tty_struct *tty, struct file *filp,
//		unsigned int cmd, unsigned long arg)
static int	XVU_ioctl(struct tty_struct *tty,
		unsigned int cmd, unsigned long arg)

{
	int				idx = 0;
	struct serial_icounter_struct	icnt = {0};
	unsigned long			flags = 0;

	if (tty == NULL) {
		XVU_ERR( "Invalid (NULL) tty_struct\n");
		return -EINVAL;
	}

	idx = tty->index;

	if ( (idx<0) || (idx >= XVU_PAIRS*2) || (idx != (int)tty->driver_data) ) {
		XVU_ERR( "Index %d out of range.\n", idx);
		return -EINVAL;
	}

	if (cmd == TIOCMIWAIT)
		return __XVU_tiocmiwait( tty, arg );

	if (cmd == TIOCGICOUNT) {
		spin_lock_irqsave(&xvu_dev.endpoint[idx].sercnt_lock, flags);
		icnt = xvu_dev.endpoint[idx].ser_cnt;
		spin_unlock_irqrestore(&xvu_dev.endpoint[idx].sercnt_lock, flags);

		if ( copy_to_user( (void __user *)arg, &icnt, sizeof(icnt)) )
			return -EFAULT;
		else
			return 0;
	}

	return -ENOIOCTLCMD;
}


static struct tty_operations serial_ops = {
	open:			XVU_open,
	close:			XVU_close,
	write:			XVU_write,
	write_room:		XVU_write_room,
	chars_in_buffer:	XVU_chars_in_buffer,
	tiocmget:		XVU_tiocmget,
	tiocmset:		XVU_tiocmset,
	ioctl:			XVU_ioctl,
};


/*
 * Module Initialization and Cleanup
 */
static int __init	XVU_Init(void)
{
	int rv = -EINVAL;
	int i = 0;

	memset(&xvu_dev, 0, sizeof(xvu_dev));
	init_MUTEX(&xvu_dev.sem);
	for (i=0;i<XVU_PAIRS * 2; i++)
		init_waitqueue_head(&xvu_dev.endpoint[i].msr_wait);

	/* allocate the tty driver with 2 minor devices per pair */
	xvu_ttydriver = alloc_tty_driver( XVU_PAIRS * 2);
	if (!xvu_ttydriver)
		return -ENOMEM;
	
	tty_port_init(&xvu_port[0]);
	tty_port_init(&xvu_port[1]);

	/* initialize the tty driver */
	xvu_ttydriver->owner                = THIS_MODULE;
	xvu_ttydriver->driver_name          = DRIVER_NAME;
	xvu_ttydriver->name                 = DEVICE_NAME;
	xvu_ttydriver->major                = XVU_MAJOR;
	xvu_ttydriver->minor_start          = 0;
	xvu_ttydriver->type                 = TTY_DRIVER_TYPE_SERIAL;
	xvu_ttydriver->subtype              = SERIAL_TYPE_NORMAL;
	xvu_ttydriver->flags                = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW;
	xvu_ttydriver->init_termios         = tty_std_termios;
	xvu_ttydriver->init_termios.c_cflag = B115200 | CS8 | CREAD | HUPCL | CLOCAL;
	tty_set_operations(xvu_ttydriver, &serial_ops);
	tty_port_link_device(&xvu_port[0], xvu_ttydriver, 0);
	tty_port_link_device(&xvu_port[1], xvu_ttydriver, 1);

	/* register the tty driver */
	rv = tty_register_driver(xvu_ttydriver);
	if (rv) {
		printk(KERN_ERR MODULE_NAME ": Failed to register Virtual UART tty driver");
		put_tty_driver(xvu_ttydriver);
		return rv;
	}

	down(&xvu_dev.sem);
	for (i=0;i<XVU_PAIRS * 2; i++)
	{
		spin_lock_init(&xvu_dev.endpoint[i].sercnt_lock);
		__XVU_SetMSR(i);
	}
	up(&xvu_dev.sem);

	printk(KERN_INFO MODULE_NAME ": Virtual UART Device Ready... Major DevId: %d\n", xvu_ttydriver->major );
	printk(KERN_INFO MODULE_NAME ": Created %d Virtual UARTs. /dev/ttyV0 -> /dev/ttyV%d\n", XVU_PAIRS*2, XVU_PAIRS*2 -1 );
	return 0;
}


static void __exit	XVU_Deinit(void)
{
	int	idx = 0;

	down(&xvu_dev.sem);

	/* Report unclosed pending endpoints (in case of forced unload) */
	for (idx = 0; idx < XVU_PAIRS * 2; idx++)
		if ( xvu_dev.endpoint[idx].open_count )
			XVU_ERR( "Endpoint[%d].open_count=%d != 0\n",
					idx, xvu_dev.endpoint[idx].open_count );

	if (xvu_ttydriver)
		tty_unregister_driver(xvu_ttydriver);

	put_tty_driver(xvu_ttydriver);

	up(&xvu_dev.sem);

	printk(KERN_INFO MODULE_NAME ": Virtual UART Device Unloaded...\n" );
}

module_init(XVU_Init);
module_exit(XVU_Deinit);

MODULE_AUTHOR("Pablo 'merKur' Kohan <pablo@ximpo.com>");
MODULE_DESCRIPTION("Virtual UART-pair");
MODULE_LICENSE("GPL v2");
