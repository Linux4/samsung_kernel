/****************************************************************

N-Trig ncp character driver

Zoro Software.
D-Trig Digitizer modules files
Copyright (C) 2010, Dmitry Kuzminov

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

 This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

****************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <asm/current.h>
#include <asm/segment.h>
#include <linux/uaccess.h>

#include "ntrig-dispatcher.h"
#include "ntrig-common.h"
/******************************************************************************/

#define DEVICE_NAME_NCP			"ntrig_ncp"
#define NTRIG_NCP_DRIVER_DRIVER_AUTHOR	"Dmitry Kuminov <dima@zoro-sw.com>"
#define NTRIG_NCP_DRIVER_DRIVER_DESC	\
	"ntrig ncp driver,based on character device, for NCP communication"
#define NTRIG_NCP_DRIVER_PLATFORM_NAME	"ntrig_ncp"
#define NTRIG_DEV_NAME_NCP		"ntrig"
	/* file name to be used by user application: /dev/ntrig */

#define NTRIG_NCP_MAJOR		223
	/* TODO: what is the major for NCP? - temporary put 223 (quirk+1) */

#define SEMAPHORE_TIME_OUT	(12) /* Jiffy=4 ms on 2.6 48 milisecond */

#define BLOCKING_MODE		0x01
#define POLLING_MODE		0x02

/******************************************************************************/

/* NOTE: Static variables and global variables are automatically initialized to
 * 0 by the compiler. The kernel style checker tool (checkpatch.pl) complains
 * if they are explicitly initialized to 0 (or NULL) in their definition.
 */

/* we need to ensure that only one process/thread is reading from char driver */
static int g_singleton;

/* flag that indicates file mode operations */
static int g_file_mode = BLOCKING_MODE;

/* major number for driver recognition */
static int g_major_ncp = NTRIG_NCP_MAJOR;

/* my class to creare /dev/ntrig pipe */
static struct class *driver_class;

/******************************************************************************/

/*
 * Callback function from dispatcher when driver is ready
 */
/*static int on_message(void * buf);*/

/* Exported API */
static int ncp_open(struct inode *inode, struct file *filep);
static int ncp_flush(struct file *filep, fl_owner_t id);
static ssize_t ncp_read(struct file *filep, char *buff, size_t count,
	loff_t *offp);
static ssize_t ncp_write(struct file *filep, const char *buff, size_t count,
	loff_t *offp);

static bool memory_lock(struct semaphore *sem);
static void memory_unlock(struct semaphore *sem);

static void init_synch_data(void);

/******************************************************************************/

static const struct file_operations ncp_fops = {
	.owner =        THIS_MODULE,
	.read =         ncp_read,
	.write =        ncp_write,
	.open =         ncp_open,
	.flush =        ncp_flush,
};

/******************************************************************************/

/* Threads protection on data */
/* There used to be a common semaphore for reads and writes - so a Write had to
 * wait for a Read in execution, which can take up to 10 seconds if there is no
 * data to read. It seems like there was no real reason to stall Writes until a
 * currently-executing Read is completed, so the common semaphore is replaced
 * by two separate semaphores, one for Writes and one for Reads.
 * This change was done for the Debug Agent.
 */
struct semaphore g_sem_read, g_sem_write;

static bool memory_lock(struct semaphore *sem)
{
	if (down_interruptible(sem))	/* semaphore was not aquired */
		return false;
	return true;
}

static void memory_unlock(struct semaphore *sem)
{
	up(sem);
}

static void init_synch_data(void)
{
	/* resources protection */
	sema_init(&g_sem_read, 1);
	sema_init(&g_sem_write, 1);
}


/******************************************************************************/
static int ncp_open(struct inode *inode, struct file *filep)
{
	if (g_singleton && !(filep->f_flags & O_WRONLY)) {
		printk(KERN_ALERT "%s file already opened by other thread\n",
			__func__);
		return -EBUSY;
	}

	if (filep->f_flags & O_NONBLOCK)
		g_file_mode = POLLING_MODE;
	else
		g_file_mode = BLOCKING_MODE;

	if (!(filep->f_flags & O_WRONLY))
		g_singleton = 1;
	return DTRG_NO_ERROR;
}

static int ncp_flush(struct file *filep, fl_owner_t id)
{
	if (!(filep->f_flags & O_WRONLY))
		g_singleton = 0;
	return DTRG_NO_ERROR;
}

/**
 * Applicaton reads from NCP file /dev/ntrig:
 * get data from dispatcher (dispatcher gets data from device) and
 * return it to application (in *buff).
 * Operation is synchronous and bloking
 */
static ssize_t ncp_read(struct file *filep, char __user *buff, size_t count,
	loff_t *offp)
{
	int retval = 0;
	char *out_buf;
	int res = 0;

	ntrig_dbg("ntrig-ncp-driver inside %s\n", __func__);

	/* See comment above the definition of g_sem_read */
	if (!memory_lock(&g_sem_read)) {	/* failed to lock */
		ntrig_err(
			"ntrig-ncp-driver inside %s -> "
			"failed to aquire semaphore\n", __func__);
		return -EINTR;
	}

	out_buf = kmalloc(count, GFP_KERNEL);
	if (!out_buf) {
		retval = -ENOMEM;
		goto ERR_EXIT;
	}
	memset(out_buf, 0, count);

	/* get data from dispatcher */
	retval = read_ncp_from_device(out_buf, count);
	if (retval < 0) {
		ntrig_err(
			"ntrig-ncp-driver: Leaving %s, "
			"failed to read ncp, retval = %d\n", __func__, retval);
		goto ERR;
	}
	res = copy_to_user(buff, out_buf, retval);
	ntrig_dbg("ntrig-ncp-driver: %s, copy_to_user returned %d\n",
		__func__, res);
	if (res) {
		retval = DTRG_FAILED;
		ntrig_err(
			"ntrig-ncp-driver: Leaving %s, failed to "
			"copy_to_user, retval = %d\n", __func__, retval);
	} else
		ntrig_dbg("ntrig-ncp-driver: Leaving %s, count = %d\n",
			__func__, retval);

ERR:
	kfree(out_buf);
ERR_EXIT:
	memory_unlock(&g_sem_read);
	return retval;
}

/**
 * Applicaton writes to NCP file /dev/ntrig:
 * pass data (buff) to dispatcher (dispatcher passes the data to the device).
 * Operation is synchronous and bloking
*/
static ssize_t ncp_write(struct file *filep, const char __user *buff,
	size_t count, loff_t *offp)
{
	char *in_buf = NULL;
	ntrig_dbg(
		"ntrig-ncp-driver inside %s filep=%p,buff=%p,count=%d,"
		"offp=%p\n", __func__, filep, buff, (int)count, offp);

	/* See comment above the definition of g_sem_write */
	if (!memory_lock(&g_sem_write)) {	/* failed to lock */
		ntrig_err(
			"ntrig-ncp-driver inside %s -> "
			"failed to aquire semaphore\n", __func__);
		return -EINTR;
	}

	in_buf = kmalloc(count, GFP_KERNEL);
	if (!in_buf)
		goto ERR;

	/* We must copy_from_user here, because later functions dereference
	 * data in user buffer */
	memset(in_buf, 0, count);

	if (copy_from_user(in_buf, buff, count) != 0) {
		ntrig_err(
			"ntrig-ncp-driver Userspace -> kernel copy failed!\n");
		goto FREE_ERR;
	}

	write_ncp_to_device(in_buf);


FREE_ERR:
	kfree(in_buf);

ERR:
	memory_unlock(&g_sem_write);
	return count;
}

int bus_init_ncp(void)
{
	ntrig_dbg_lvl(NTRIG_DEBUG_LEVEL_ONCE,
		"ntrig-ncp-driver inside %s\n", __func__);

	if (register_chrdev(g_major_ncp, DEVICE_NAME_NCP, &ncp_fops)) {
		ntrig_err(
			"ntrig-ncp-driver inside %s failed to register "
			"driver\n", __func__);
		goto ERROR;
	}

	/**
	 * Prepare synchronisation variables
	 */
	init_synch_data();

	/**
	 * create device
	 */
	driver_class = class_create(THIS_MODULE, DEVICE_NAME_NCP);
	device_create(driver_class, NULL, MKDEV(NTRIG_NCP_MAJOR, 0), "%s",
		NTRIG_DEV_NAME_NCP);

	return DTRG_NO_ERROR;

ERROR:
	return DTRG_FAILED;
}
EXPORT_SYMBOL_GPL(bus_init_ncp);

void bus_exit_ncp(void)
{
	ntrig_dbg_lvl(NTRIG_DEBUG_LEVEL_ONCE,
		"ntrig-ncp-driver inside %s\n", __func__);
	/**
	 * remove dev/ntrig
	 */
	device_destroy(driver_class, MKDEV(NTRIG_NCP_MAJOR, 0));
	class_destroy(driver_class);

	unregister_chrdev(g_major_ncp, DEVICE_NAME_NCP);
}
EXPORT_SYMBOL_GPL(bus_exit_ncp);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(NTRIG_NCP_DRIVER_DRIVER_AUTHOR);
MODULE_DESCRIPTION(NTRIG_NCP_DRIVER_DRIVER_DESC);
MODULE_SUPPORTED_DEVICE(NTRIG_NCP_DRIVER_PLATFORM_NAME);
