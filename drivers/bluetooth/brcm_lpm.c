/*

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.


   Copyright (C) 2006-2007 - Motorola
   Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.

   Date         Author           Comment
   -----------  --------------   --------------------------------
   2006-Apr-28  Motorola         The kernel module for running the Bluetooth(R)
                                 Sleep-Mode Protocol from the Host side
   2006-Sep-08  Motorola         Added workqueue for handling sleep work.
   2007-Jan-24  Motorola         Added mbm_handle_ioi() call to ISR.
   2009-Aug-10  Motorola         Changed "add_timer" to "mod_timer" to solve
                                 race when flurry of queued work comes in.
*/

#include <linux/module.h>       /* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/wakelock.h>
//#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
//#include <mach/serial.h>
#include <linux/serial_core.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h> /* event notifications */
#include "hci_uart.h"
#include <linux/seq_file.h>


//#define BT_SLEEP_DBG	pr_debug
#define BT_SLEEP_DBG BT_ERR
/*
 * Defines
 */

#define VERSION         "1.1"
#define PROC_DIR        "bluetooth/sleep"

#define POLARITY_LOW 0
#define POLARITY_HIGH 1

#define BT_PORT_ID	0

/* enable/disable wake-on-bluetooth */
#define BT_ENABLE_IRQ_WAKE 1

#define BT_BLUEDROID_SUPPORT 1

enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_SUSPEND = 1U << 1,
	DEBUG_BTWAKE = 1U << 2,
	DEBUG_VERBOSE = 1U << 3,
};
struct workqueue_struct *hostwake_work_queue;

static int debug_mask = DEBUG_USER_STATE;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

struct bluesleep_info {
        unsigned host_wake;
        unsigned ext_wake;
        unsigned host_wake_irq;
        struct uart_port *uport;
//        struct wake_lock wake_lock;
		struct wake_lock BT_wakelock;
		struct wake_lock host_wakelock;
	    int irq_polarity;
	    int has_ext_wake;
};

/* work function */
static void bluesleep_sleep_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);

/* Macros for handling sleep work */
#define bluesleep_rx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_rx_idle()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_idle()     schedule_delayed_work(&sleep_workqueue, 0)

/* 1 second timeout */
#define TX_TIMER_INTERVAL      8   //after 10S  DEV pin change,the BT can sleep

/* state variable names and bit positions */
#define BT_PROTO        0x01
#define BT_TXDATA       0x02
#define BT_ASLEEP       0x04
#define BT_EXT_WAKE	0x08
#define BT_SUSPEND	0x10

#if BT_BLUEDROID_SUPPORT
static bool has_lpm_enabled = false;
#else
/* global pointer to a single hci device. */
static struct hci_dev *bluesleep_hdev;
#endif

static struct platform_device *bluesleep_uart_dev;
static struct bluesleep_info *bsi;
static bool is_bt_stoped=0;

/* module usage */
static atomic_t open_count = ATOMIC_INIT(1);

/*
 * Local function prototypes
 */
#if !BT_BLUEDROID_SUPPORT
static int bluesleep_hci_event(struct notifier_block *this,
			unsigned long event, void *data);
#endif
static int bluesleep_start(void);
static void bluesleep_stop(void);
extern struct uart_port * serial_get_uart_port(int uart_index);
/*
 * Global variables
 */

/** Global state flags */
static unsigned long flags;

/** Tasklet to respond to change in hostwake line */
static struct tasklet_struct hostwake_task;

/** Transmission timer */
static void bluesleep_tx_timer_expire(unsigned long data);
static DEFINE_TIMER(tx_timer, bluesleep_tx_timer_expire, 0, 0);

/** Lock for state transitions */
static spinlock_t rw_lock;

#if !BT_BLUEDROID_SUPPORT
/** Notifier block for HCI events */
struct notifier_block hci_event_nblock = {
        .notifier_call = bluesleep_hci_event,
};
#endif

struct proc_dir_entry *bluetooth_dir, *sleep_dir;

/*
 * Local functions
 */

static void hsuart_power(int on)
{BT_ERR("hsuart_power");
	if (test_bit(BT_SUSPEND, &flags))
		return;
	if (on) {
	  //msm_hs_request_clock_on(bsi->uport);
	  //	msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);
	} else {
	  //	msm_hs_set_mctrl(bsi->uport, 0);
	  //	msm_hs_request_clock_off(bsi->uport);
	}
	BT_SLEEP_DBG("uart power %d\n", on);
}


/**
 * @return 1 if the Host can go to sleep, 0 otherwise.
 */
int bluesleep_can_sleep(void)
{
        /* check if WAKE_BT_GPIO and BT_WAKE_GPIO are both deasserted */
	BT_SLEEP_DBG("bt_wake %d, host_wake %d, uport %p"
			      , gpio_get_value(bsi->ext_wake)
			      , gpio_get_value(bsi->host_wake)
			      , bsi->uport);
	return ((gpio_get_value(bsi->host_wake) != bsi->irq_polarity) &&
		(test_bit(BT_EXT_WAKE, &flags)) &&
		(bsi->uport != NULL));
}

void bluesleep_sleep_wakeup(void)
{  BT_ERR("bluesleep_sleep_wakeup");
        if (test_bit(BT_ASLEEP, &flags)) {
                BT_SLEEP_DBG("waking up...");
               // wake_lock(&bsi->wake_lock); //wake
                /* Start the timer */
                mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		if (bsi->has_ext_wake == 1)
                gpio_set_value(bsi->ext_wake, 0);
		clear_bit(BT_EXT_WAKE, &flags);
                clear_bit(BT_ASLEEP, &flags);
                /*Activating UART */
                hsuart_power(1);
        }
}

/**
 * @brief@  main sleep work handling function which update the flags
 * and activate and deactivate UART ,check FIFO.
 */
static void bluesleep_sleep_work(struct work_struct *work)
{
        BT_ERR("bluesleep_sleep_work");
        if (bluesleep_can_sleep()) {
		BT_SLEEP_DBG("can sleep...");
                /* already asleep, this is an error case */
                if (test_bit(BT_ASLEEP, &flags)) {
                        BT_SLEEP_DBG("already asleep");
                        return;
                }

                if (bsi->uport->ops->tx_empty(bsi->uport)) {
                        BT_SLEEP_DBG("going to sleep...");
                        set_bit(BT_ASLEEP, &flags);
                        /*Deactivating UART */
                        hsuart_power(0);
                        /* UART clk is not turned off immediately. Release
                         * wakelock after 500 ms.
                         */
                       // wake_lock_timeout(&bsi->wake_lock, HZ / 2);
                } else {
			BT_SLEEP_DBG("tx buffer is not empty, modify timer...");
			/*lgh add*/
			/*lgh add end*/
			mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
			return;
                }
	} else if (test_bit(BT_EXT_WAKE, &flags)
			&& !test_bit(BT_ASLEEP, &flags)) {
		BT_SLEEP_DBG("can not sleep, bt_wake %d", gpio_get_value(bsi->ext_wake) );
                mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
                gpio_set_value(bsi->ext_wake, 0);
		clear_bit(BT_EXT_WAKE, &flags);
        }else {
                bluesleep_sleep_wakeup();
        }
}

/**
 * A workqueue that runs in workqueue context and reads the value
 * of the HOST_WAKE GPIO pin and further defer the work.
 * @param work Not used.
 */
static void bluesleep_hostwake_task(unsigned long data)
{
        BT_ERR("hostwake line change");
		BT_ERR("irq_polarity=%d\n",bsi->irq_polarity);
		BT_ERR("bsi->host_wake=%d\n",gpio_get_value(bsi->host_wake));

	spin_lock(&rw_lock);
	if ((gpio_get_value(bsi->host_wake) == bsi->irq_polarity))
		bluesleep_rx_busy();
	else
		bluesleep_rx_idle();

	spin_unlock(&rw_lock);
}

/**
 * Handles proper timer action when outgoing data is delivered to the
 * HCI line discipline. Sets BT_TXDATA.
 */
static void bluesleep_outgoing_data(void)
{      
        unsigned long irq_flags;
        BT_ERR("bluesleep_outgoing_data\n");
        spin_lock_irqsave(&rw_lock, irq_flags);
		wake_lock(&bsi->BT_wakelock);


		if (bsi->has_ext_wake == 1)               //wsh
        gpio_set_value(bsi->ext_wake, 0);         //wsh
		set_bit(BT_EXT_WAKE, &flags);             //wsh
		

        /* log data passing by */
        set_bit(BT_TXDATA, &flags);

	spin_unlock_irqrestore(&rw_lock, irq_flags);
	mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ)); //add
	#if 0
        /* if the tx side is sleeping... */
	if (test_bit(BT_EXT_WAKE, &flags)) {

                BT_SLEEP_DBG("tx was sleeping");
                bluesleep_sleep_wakeup();
        }
	#endif
}

static ssize_t bluesleep_write_proc_lpm(struct file *file, const char __user *buffer,
					size_t count, loff_t *pos)
{
	char b;
	BT_ERR("bluesleep_write_proc_lpm\n");

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;
   BT_ERR("bluesleep_write_proc_lpm_b=%d\n",b);
	if (b == '0') {
		/* HCI_DEV_UNREG */
		bluesleep_stop();
		has_lpm_enabled = false;
		bsi->uport = NULL;
	} else {
		/* HCI_DEV_REG */
		if (!has_lpm_enabled) {
			has_lpm_enabled = true;
			bsi->uport = serial_get_uart_port(BT_PORT_ID);
			/* if bluetooth started, start bluesleep*/
			bluesleep_start();
		}
	}

	return count;
}


static int lpm_proc_show(struct seq_file * m,void * v)
{
	//unsigned int lpm;
	BT_ERR("bluesleep_read_proc_lpm\n");
	seq_printf(m, "unsupported to read\n");
	return 0;
}

static int bluesleep_open_proc_lpm(struct inode *inode, struct file *file)
{
	return single_open(file, lpm_proc_show, PDE_DATA(inode));

}
static ssize_t bluesleep_write_proc_btwrite(struct file *file, const char __user *buffer,size_t count, loff_t *pos)
{                           
	char b;
	BT_ERR("bluesleep_write_proc_btwrite");

	if (count < 1)
		return -EINVAL;
	if (is_bt_stoped==1)
		return count;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;
	BT_ERR("bluesleep_write_proc_btwrite=%d",b);

	/* HCI_DEV_WRITE */
	if (b != '0')
		bluesleep_outgoing_data();

	return count;
}
static int btwrite_proc_show(struct seq_file * m,void * v)
{
	//unsigned int btwrite;
	BT_ERR("bluesleep_read_proc_lpm\n");
	seq_printf(m, "unsupported to read\n");
	return 0;
}

static int bluesleep_open_proc_btwrite(struct inode *inode, struct file *file)
{
	return single_open(file, btwrite_proc_show, PDE_DATA(inode));

}

/**
 * Handles transmission timer expiration.
 * @param data Not used.
 */
static void bluesleep_tx_timer_expire(unsigned long data)
{   
	if(bsi->uport==NULL)
		return;
	if (bsi->uport->ops->tx_empty(bsi->uport))
    	{  BT_ERR("empty");
           gpio_set_value(bsi->ext_wake, 1);
		   wake_unlock(&bsi->BT_wakelock);
    	}
	else
		{
		  BT_ERR("not  empty");
		  mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));
		}

     //gpio_set_value(bsi->ext_wake, 1);
#if 0
        unsigned long irq_flags;
		BT_ERR("bluesleep_tx_timer_expire");

        spin_lock_irqsave(&rw_lock, irq_flags);

        BT_SLEEP_DBG("Tx timer expired");

        /* were we silent during the last timeout? */
        if (!test_bit(BT_TXDATA, &flags)) {
                BT_SLEEP_DBG("Tx has been idle");
		if (bsi->has_ext_wake == 1)
                gpio_set_value(bsi->ext_wake, 1);

                bluesleep_tx_idle();
        } else {
                BT_SLEEP_DBG("Tx data during last period");
                mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));
        }

        /* clear the incoming data flag */
        clear_bit(BT_TXDATA, &flags);

        spin_unlock_irqrestore(&rw_lock, irq_flags);
#endif
}

/**
 * Schedules a workqueue to run when receiving an interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
	/* schedule a tasklet to handle the change in the host wake line */

	int ext_wake, host_wake;
	BT_ERR("bluesleep_hostwake_isr\n");
	ext_wake = gpio_get_value(bsi->ext_wake);
	host_wake = gpio_get_value(bsi->host_wake);
	BT_DBG("ext_wake : %d, host_wake : %d", ext_wake, host_wake);
    if(host_wake== 0)
    	{
    	  wake_lock(&bsi->host_wakelock);
		  irq_set_irq_type(irq, IRQF_TRIGGER_HIGH);  

    	}
	else
		{
		  //wake_unlock(&bsi->host_wakelock);
		  wake_lock_timeout(&bsi->host_wakelock, HZ*5);
		  irq_set_irq_type(irq,IRQF_TRIGGER_LOW);
		}
		
//	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
	if (host_wake == 0) {
		BT_DBG("bluesleep_hostwake_isr : Registration Tasklet");
	   // tasklet_schedule(&hostwake_task);
	}
	
	return IRQ_HANDLED;
}

/**
 * Starts the Sleep-Mode Protocol on the Host.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int bluesleep_start(void)
{
        int retval;
        unsigned long irq_flags;
       	BT_ERR("bluesleep_start\n");
		is_bt_stoped=0;
        spin_lock_irqsave(&rw_lock, irq_flags);

        if (test_bit(BT_PROTO, &flags)) {
                spin_unlock_irqrestore(&rw_lock, irq_flags);
                return 0;
        }

        spin_unlock_irqrestore(&rw_lock, irq_flags);

        if (!atomic_dec_and_test(&open_count)) {
                atomic_inc(&open_count);
                return -EBUSY;
        }

        /* start the timer */
        mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));

        /* assert BT_WAKE */
	if (bsi->has_ext_wake == 1)
        gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);
#if BT_ENABLE_IRQ_WAKE
	retval = enable_irq_wake(bsi->host_wake_irq);
	if (retval < 0) {
		BT_ERR("Couldn't enable BT_HOST_WAKE as wakeup interrupt");
		goto fail;
	}
#endif
	set_bit(BT_PROTO, &flags);
	//wake_lock(&bsi->wake_lock);
	return 0;
fail:
        del_timer(&tx_timer);
        atomic_inc(&open_count);

        return retval;
}

/**
 * Stops the Sleep-Mode Protocol on the Host.
 */
static void bluesleep_stop(void)
{
        unsigned long irq_flags;
		BT_ERR("bluesleep_stop\n");

        spin_lock_irqsave(&rw_lock, irq_flags);

        if (!test_bit(BT_PROTO, &flags)) {
                spin_unlock_irqrestore(&rw_lock, irq_flags);
                return;
        }

        /* assert BT_WAKE */
	if (bsi->has_ext_wake == 1)
        gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);
        del_timer(&tx_timer);
        clear_bit(BT_PROTO, &flags);

	if (test_bit(BT_ASLEEP, &flags)) {
		clear_bit(BT_ASLEEP, &flags);
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		hsuart_power(1);
	} else {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
	}

        atomic_inc(&open_count);

#if BT_ENABLE_IRQ_WAKE
        if (disable_irq_wake(bsi->host_wake_irq))
                BT_SLEEP_DBG("Couldn't disable hostwake IRQ wakeup mode\n");
#endif
	wake_unlock(&bsi->host_wakelock);
	wake_unlock(&bsi->BT_wakelock);
	is_bt_stoped=1;
     //  wake_lock_timeout(&bsi->wake_lock, HZ / 2);
}

/**
 * Write the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 * @param file Not used.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be written.
 * @param data Not used.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static ssize_t bluepower_write_proc_btwake(struct file *file, const char __user *buffer,
                                            size_t count, loff_t *pos)
{
        char *buf;
		BT_ERR("bluepower_write_proc_btwake\n");

        if (count < 1)
              return -EINVAL;

        buf = kmalloc(count, GFP_KERNEL);
        if (!buf)
              return -ENOMEM;

        if (copy_from_user(buf, buffer, count)) {
              kfree(buf);
              return -EFAULT;
        }

        if (buf[0] == '0') {
		if (bsi->has_ext_wake == 1)
              gpio_set_value(bsi->ext_wake, 0);
		clear_bit(BT_EXT_WAKE, &flags);
        } 
		else if (buf[0] == '1') {
		if (bsi->has_ext_wake == 1)
              gpio_set_value(bsi->ext_wake, 1);
		set_bit(BT_EXT_WAKE, &flags);
        } else {
              kfree(buf);
              return -EINVAL;
        }

        kfree(buf);

        return count;
}
static int btwake_proc_show(struct seq_file *m, void *v)
{
	unsigned int btwake;
	btwake = test_bit(BT_EXT_WAKE, &flags) ? 1 : 0;
//	return sprintf(buff, "asleep: %u\n", asleep); //cat: asleep:0
        seq_printf(m, "%u\n", btwake);
	return 0;
}

static int bluepower_open_proc_btwake(struct inode *inode, struct file *file)
{
	return single_open(file, btwake_proc_show, PDE_DATA(inode));

}



static int hostwake_proc_show(struct seq_file *m, void *v)
{
	unsigned int hostwake;
	hostwake = gpio_get_value(bsi->host_wake);
    seq_printf(m, "%u\n", hostwake);
	return 0;
}

static int bluepower_open_proc_hostwake(struct inode *inode, struct file *file)
{
	return single_open(file, hostwake_proc_show, PDE_DATA(inode));

}



static int bluesleep_proc_show(struct seq_file *m, void *v)
{
//	seq_printf(m, "%d\n", val);
	unsigned int asleep;
	asleep = test_bit(BT_ASLEEP, &flags) ? 1 : 0;
//	return sprintf(buff, "asleep: %u\n", asleep); //cat: asleep:0
    seq_printf(m, "%u\n", asleep);
//	seq_puts()

	return 0;
}

static int bluesleep_open_proc_asleep(struct inode *inode, struct file *file)
{
	return single_open(file, bluesleep_proc_show, PDE_DATA(inode));

}

static ssize_t bluesleep_write_proc_proto(struct file *filp,const char __user *buff, size_t count, loff_t *pos)
{
        char proto;
		BT_ERR("bluesleep_write_proc_proto\n");

       if(count>0){
        if (copy_from_user(&proto, buff, 1))    //or get_user(proto, buffer)
            return -EFAULT;
		
	     BT_SLEEP_DBG("write proto %c", proto);

        if (proto == '0')
                bluesleep_stop();
        else
                bluesleep_start();
       	}

        /* claim that we wrote everything */
        return count; 
}


static int proto_proc_show(struct seq_file *m, void *v)
{
	unsigned int proto;
	proto = test_bit(BT_PROTO, &flags) ? 1 : 0;
    seq_printf(m, "%u\n", proto);
	return 0;
}

static int bluesleep_open_proc_proto(struct inode *inode, struct file *file)
{
	return single_open(file, proto_proc_show, PDE_DATA(inode));

}



void bluesleep_setup_uart_port(struct platform_device *uart_dev)
{
	bluesleep_uart_dev = uart_dev;
}

static int bluesleep_populate_dt_pinfo(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int tmp;

	tmp = of_get_named_gpio(np, "host-wake-gpio", 0);
	if (tmp < 0) {
		BT_ERR("couldn't find host_wake gpio");
		return -ENODEV;
	}
	bsi->host_wake = tmp;

	tmp = of_get_named_gpio(np, "bt-wake-gpio", 0);
	if (tmp < 0)
		bsi->has_ext_wake = 0;
	else
		bsi->has_ext_wake = 1;

	if (bsi->has_ext_wake)
		bsi->ext_wake = tmp;

	BT_INFO("bt_host_wake %d, bt-wake-gpio %d",
			bsi->host_wake,
			bsi->ext_wake);
	return 0;
}

static int bluesleep_populate_pinfo(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_host_wake");
	if (!res) {
		BT_ERR("couldn't find host_wake gpio");
		return -ENODEV;
	}
	bsi->host_wake = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_ext_wake");
	if (!res)
		bsi->has_ext_wake = 0;
	else
		bsi->has_ext_wake = 1;

	if (bsi->has_ext_wake)
		bsi->ext_wake = res->start;

	return 0;
}

static int bluesleep_probe(struct platform_device *pdev)
{
	//struct resource *res;
	int ret;

	bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		ret = bluesleep_populate_dt_pinfo(pdev);
		if (ret < 0) {
			BT_ERR("couldn't populate info from dt");
			return ret;
		}
	} else {
		ret = bluesleep_populate_pinfo(pdev);
		if (ret < 0) {
			BT_ERR("couldn't populate info");
			return ret;
		}
	}

	/* configure host_wake as input */
	ret = gpio_request_one(bsi->host_wake, GPIOF_IN, "bt_host_wake");
	if (ret < 0) {
		BT_ERR("failed to configure input"
				" direction for GPIO %d, error %d",
				bsi->host_wake, ret);
		goto free_bsi;
	}

	if (debug_mask & DEBUG_BTWAKE)
		pr_info("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake) {
		/* configure ext_wake as output mode*/
		ret = gpio_request_one(bsi->ext_wake,
				GPIOF_OUT_INIT_LOW, "bt_ext_wake");
		if (ret < 0) {
			BT_ERR("failed to configure output"
				" direction for GPIO %d, error %d",
				  bsi->ext_wake, ret);
			goto free_bt_host_wake;
		}
	}
	clear_bit(BT_EXT_WAKE, &flags);

	BT_SLEEP_DBG("allocat irq hostwake = %d\n", bsi->host_wake);
				//bsi->host_wake_irq = sprd_alloc_gpio_irq(bsi->host_wake);  
	bsi->host_wake_irq = gpio_to_irq(bsi->host_wake);  
	BT_SLEEP_DBG("irq = bsi->host_wake_irq %d", bsi->host_wake_irq);
	if (bsi->host_wake_irq < 0) {
	   BT_SLEEP_DBG("couldn't find host_wake irq\n");
	   ret = -ENODEV;
	   goto free_bt_ext_wake;
				}


	bsi->irq_polarity = POLARITY_LOW;/*low edge (falling edge)*/

	//wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND, "bluesleep");
	
	wake_lock_init(&bsi->BT_wakelock, WAKE_LOCK_SUSPEND, "bluesleep1");
	wake_lock_init(&bsi->host_wakelock, WAKE_LOCK_SUSPEND, "bluesleep2");
	clear_bit(BT_SUSPEND, &flags);

	BT_INFO("host_wake_irq %d, polarity %d",
			bsi->host_wake_irq,
			bsi->irq_polarity);

	ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
			//IRQF_TRIGGER_FALLING |IRQF_NO_SUSPEND, remove yan
			(IRQF_TRIGGER_LOW|IRQF_NO_SUSPEND),  //add low level
			"bt_host_wake", bsi);

	if (ret  < 0) {
		BT_ERR("Couldn't acquire BT_HOST_WAKE IRQ");
		goto free_bt_ext_wake;
	}

	return 0;

free_bt_ext_wake:
        gpio_free(bsi->ext_wake);
free_bt_host_wake:
        gpio_free(bsi->host_wake);
free_bsi:
        kfree(bsi);
        return ret;
}

static int bluesleep_remove(struct platform_device *pdev)
{  BT_ERR("bluesleep_remove");
	free_irq(bsi->host_wake_irq, NULL);
        gpio_free(bsi->host_wake);
        gpio_free(bsi->ext_wake);
      //  wake_lock_destroy(&bsi->wake_lock);
      wake_lock_destroy(&bsi->BT_wakelock);
      wake_lock_destroy(&bsi->host_wakelock);
      
        kfree(bsi);
        return 0;
}


static int bluesleep_resume(struct platform_device *pdev)
{   BT_ERR("bluesleep_resume");
	if (test_bit(BT_SUSPEND, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			BT_ERR("bluesleep resuming...\n");
		if ((bsi->uport != NULL) &&
			(gpio_get_value(bsi->host_wake) == bsi->irq_polarity)) {
			if (debug_mask & DEBUG_SUSPEND)
				BT_ERR("bluesleep resume from BT event...\n");
//			msm_hs_request_clock_on(bsi->uport);   ???????????????????
//			msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);????????????????
		}
		clear_bit(BT_SUSPEND, &flags);
	}
	return 0;
}

static int bluesleep_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("bluesleep suspending...\n");
	set_bit(BT_SUSPEND, &flags);
	return 0;
}

static struct of_device_id bluesleep_match_table[] = {
	{ .compatible = "broadcom,bluesleep" },
	{}
};

static struct platform_driver bluesleep_driver = {
        .probe = bluesleep_probe,
        .remove = bluesleep_remove,
	.suspend = bluesleep_suspend,
	.resume = bluesleep_resume,
        .driver = {
                .name = "bluesleep",
                .owner = THIS_MODULE,
		.of_match_table = bluesleep_match_table,
        },
};


static const struct file_operations lpm_proc_btwake_fops = {
	.owner = THIS_MODULE,
	.open = bluepower_open_proc_btwake,
	.read = seq_read,
	.write = bluepower_write_proc_btwake,
	.release = single_release,
};


static const struct file_operations lpm_proc_hostwake_fops = {
		.owner = THIS_MODULE,
		.open = bluepower_open_proc_hostwake,
		.read = seq_read,
		.release = single_release,
	};

static const struct file_operations lpm_proc_proto_fops = {
	.owner = THIS_MODULE,
	.open= bluesleep_open_proc_proto,
	.read=seq_read,
	.write = bluesleep_write_proc_proto,
	.release = single_release,
};

static const struct file_operations lpm_proc_asleep_fops = {
	.owner = THIS_MODULE,
	.open= bluesleep_open_proc_asleep,
	.read= seq_read,
//	.read = bluesleep_read_proc_asleep,
	.release = single_release,
};

static const struct file_operations lpm_proc_lpm_fops = {
	.owner = THIS_MODULE,
	.open= bluesleep_open_proc_lpm,
	.read = seq_read,
	.write = bluesleep_write_proc_lpm,
	.release = single_release,
};

static const struct file_operations lpm_proc_btwrite_fops = {
	.owner = THIS_MODULE,
	.open = bluesleep_open_proc_btwrite,
	.read = seq_read,
	.write = bluesleep_write_proc_btwrite,
	.release = single_release,
};






/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int __init bluesleep_init(void)
{
        int retval;
        struct proc_dir_entry *ent;
//	    char name[64];

        BT_SLEEP_DBG("BlueSleep Mode Driver Ver %s", VERSION);
/***************************************************************/
		tasklet_init(&hostwake_task, bluesleep_hostwake_task, (unsigned long)bsi);
        flags = 0; /* clear all status bits */

        /* Initialize spinlock. */
        spin_lock_init(&rw_lock);

        /* Initialize timer */
        init_timer(&tx_timer);
        tx_timer.function = bluesleep_tx_timer_expire;
        tx_timer.data = 0;
/*********************************************************************/
        retval = platform_driver_register(&bluesleep_driver);
        if (retval)
            return retval;

	if (bsi == NULL)
		return 0;

#if !BT_BLUEDROID_SUPPORT
	bluesleep_hdev = NULL;
#endif

        bluetooth_dir = proc_mkdir("bluetooth", NULL);
        if (bluetooth_dir == NULL) {
                BT_SLEEP_DBG("Unable to create /proc/bluetooth directory");
                return -ENOMEM;
        }

        sleep_dir = proc_mkdir("sleep", bluetooth_dir);
        if (sleep_dir == NULL) {
                BT_SLEEP_DBG("Unable to create /proc/%s directory", PROC_DIR);
                return -ENOMEM;
        }

        /* Creating read/write  entry */
		ent=proc_create("btwake", S_IRUGO | S_IWUSR | S_IWGRP, sleep_dir,&lpm_proc_btwake_fops);  /*read/write */
		ent=proc_create("hostwake", S_IRUGO, sleep_dir,&lpm_proc_hostwake_fops);                  /* only read */
		ent=proc_create("proto", S_IRUGO | S_IWUSR | S_IWGRP, sleep_dir,&lpm_proc_proto_fops);    /* read/write */
		ent=proc_create("asleep", S_IRUGO, sleep_dir,&lpm_proc_asleep_fops);                      /* only read */
#if BT_BLUEDROID_SUPPORT
		ent=proc_create("lpm", S_IRUGO | S_IWUSR | S_IWGRP, sleep_dir,&lpm_proc_lpm_fops);         /* read/write*/
		ent=proc_create("btwrite", S_IRUGO | S_IWUSR | S_IWGRP, sleep_dir,&lpm_proc_btwrite_fops); /*read/write */
		if (ent == NULL) {
		    BT_SLEEP_DBG("Unable to create /proc/%s/btwake entry", PROC_DIR);
			retval = -ENOMEM;
		    goto fail;
			  }

#endif


 /* assert bt wake */
	if (bsi->has_ext_wake == 1)
    gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);
	BT_ERR("a=%d,b=%d\n",bsi->has_ext_wake,bsi->ext_wake);
	
#if !BT_BLUEDROID_SUPPORT
	hci_register_notifier(&hci_event_nblock);
#endif

        return 0;

fail:
#if BT_BLUEDROID_SUPPORT
	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
#endif
	remove_proc_entry("asleep", sleep_dir);
	remove_proc_entry("proto", sleep_dir);
	remove_proc_entry("hostwake", sleep_dir);
	remove_proc_entry("btwake", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
	return retval;
}

/**
 * Cleans up the module.
 */
static void __exit bluesleep_exit(void)
{
	if (bsi == NULL)
		return;
        /* assert bt wake */
	if (bsi->has_ext_wake == 1)
        gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);
        if (test_bit(BT_PROTO, &flags)) {
                if (disable_irq_wake(bsi->host_wake_irq))
                        BT_SLEEP_DBG("Couldn't disable hostwake IRQ wakeup mode \n");
                free_irq(bsi->host_wake_irq, NULL);
                del_timer(&tx_timer);
                if (test_bit(BT_ASLEEP, &flags))
                        hsuart_power(1);
        }

#if !BT_BLUEDROID_SUPPORT
        hci_unregister_notifier(&hci_event_nblock);
#endif
        platform_driver_unregister(&bluesleep_driver);
#if BT_BLUEDROID_SUPPORT
        remove_proc_entry("btwrite", sleep_dir);
	    remove_proc_entry("lpm", sleep_dir);
#endif
        remove_proc_entry("asleep", sleep_dir);
        remove_proc_entry("proto", sleep_dir);
        remove_proc_entry("hostwake", sleep_dir);
        remove_proc_entry("btwake", sleep_dir);
        remove_proc_entry("sleep", bluetooth_dir);
        remove_proc_entry("bluetooth", 0);
//	destroy_workqueue(hostwake_work_queue);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

