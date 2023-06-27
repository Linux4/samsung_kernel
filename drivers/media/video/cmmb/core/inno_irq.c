
/*
 * innofidei if2xx demod irq driver
 * 
 * Copyright (C) 2010 Innofidei Corporation
 * Author:      sean <zhaoguangyu@innofidei.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/bitops.h>
#include "inno_core.h"
#include <linux/gpio.h>
#include "inno_irq.h"
#include "inno_comm.h"
#include "inno_demod.h"
//#define IRQ_DEBUG

#ifdef  IRQ_DEBUG
#include <linux/timer.h>
#endif

#ifdef INNO_DEBUG_LOGTOFILE
extern unsigned char inno_log_buf[];
extern void inno_logfile_write(unsigned char*in_buf);
#undef pr_debug
#undef pr_err
#define pr_debug(fmt, ...) \
		{sprintf(inno_log_buf,"INFO:[INNO/core/inno_irq.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#define pr_err(fmt, ...) \
		{sprintf(inno_log_buf,"ERR:[INNO/core/inno_irq.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#else

#undef pr_debug
#define pr_debug(fmt, ...) \
printk(KERN_DEBUG "[INNO/core/inno_irq.c]" pr_fmt(fmt),##__VA_ARGS__)

#undef pr_err
#define pr_err(fmt, ...) \
printk(KERN_ERR "[INNO/core/inno_irq.c]" pr_fmt(fmt), ##__VA_ARGS__)

#endif

#define IRQ_REQ_DELAY_LIMITED 400 /* ms */

extern struct bus_type inno_lgx_bus_type;

#define MAX_REQ_NUM             10
static struct inno_req          irq_req[MAX_REQ_NUM];
static struct list_head         irq_req_free_list;
static spinlock_t               irq_req_lock;
/**
 * inno_lgx_match -find which lgx_device this interrupt belong to
 * @dev:device
 * @data:channel id 
 */
static int inno_lgx_match(struct device *dev, void *data)
{
        char name[10];
        int id = *(int *)data;
       
        if (id < UAM_BIT_SHIFT) 
                sprintf(name, LG_PREFIX"%d", id);
        else
                sprintf(name, UAM_PREFIX"%d", id - UAM_BIT_SHIFT);
        
        return (strcmp(name, dev_name(dev)) == 0);
}

extern void inno_ctl_notify_avail (void);
static void global_ca_err_notify(void)
{
        pr_debug("%s\n", __func__);
	inno_ctl_notify_avail ();
}

/**
 * inno_irq_req_handler -called by demod req workqueue
 * @data:unused
 *
 * identify which channel this interrupt belong to and notify correct lgx_device
 *
 * This fuction was edited by liujinpeng in 2012.04.05
 * i make this function handle every require each time, as you see in the while loop.
 * (while in the past, it just handle the first require it found, and leave the rest in global_lgx_ready)
 *
 */
//static unsigned long   global_lgx_ready = 0;
static int inno_irq_req_handler(void *data)
{
        unsigned long   lgx_ready = 0;  
        int             bit;
        struct device * dev;
        struct lgx_device *lgx_dev;
        struct lgx_driver *lgx_drv;
        struct inno_req *req = (struct inno_req *)data;

        //pr_debug("%s, global_lgx_ready = 0x%lx\n", __func__, global_lgx_ready);

	/* 
	if(global_lgx_ready)
	 {
	 	lgx_ready = global_lgx_ready;
	 }
	 else
	 {
        	inno_get_intr_ch(&lgx_ready);
		global_lgx_ready = lgx_ready;
	 }
	 */
	 
        inno_get_intr_ch(&lgx_ready);
        pr_debug("%s:lgx_ready = 0x%lx\n", __func__, lgx_ready);
		
#ifdef IRQ_DEBUG
        lgx_ready |= 0x1;
#endif
        //if (!lgx_ready)
          //      goto out;
      
        //bit = find_first_bit(&lgx_ready, LGX_BIT_CNT);
		//global_lgx_ready &= ~(unsigned long)(1<<bit);
        //pr_debug("%s, after global_lgx_ready = 0x%lx\n", __func__, global_lgx_ready);
	 
        //while (bit < LGX_BIT_CNT) {
	 //if (bit < LGX_BIT_CNT) {
        while (lgx_ready != 0) {
			bit = find_first_bit(&lgx_ready, LGX_BIT_CNT);
////////////inno change begin	20120816  from liuge //////////////		
			if (bit >= LGX_BIT_CNT) {
				pr_debug("%s: irq bit=%d, break!!!\n\n", __func__, bit);
				break;
			}
//////////////inno change end   20120816   from liuge ///////////////
			lgx_ready &= ~(unsigned long)(1<<bit);
			
			if (bit < UAM_BIT_SHIFT) {/* it's a logcal channel interrupt */
				pr_debug("%s: irq bit=%d, processed @ %d, irq occured @ %d\n\n", __func__, bit, jiffies_to_msecs(jiffies), jiffies_to_msecs(req->irq_jif));
				if (time_after(jiffies, req->irq_jif + msecs_to_jiffies(IRQ_REQ_DELAY_LIMITED))) {
					pr_err("%s:irq req is too late to be processed, so we drop it\n", __func__);
					//goto next;
					//goto out;
					continue;
				}
			}
	
			if(bit < CA_BIT_SHIFT) {
				dev = bus_find_device(&inno_lgx_bus_type, NULL, &bit, inno_lgx_match);
	             	if (dev && dev->driver) {
	                  	lgx_dev = container_of(dev, struct lgx_device, dev);
	                    lgx_drv = container_of(dev->driver, struct lgx_driver, driver);
						lgx_dev->irq_jif = req->irq_jif;
		                if (lgx_drv->data_notify) {lgx_drv->data_notify(lgx_dev); }
				}				
			} else {
				global_ca_err_notify (); // Check CA ERR Status, ONLY FOR IF206
			}
			
//next:                
	             //bit = find_next_bit(&lgx_ready, LGX_BIT_CNT, bit + 1);
        }

//out:
	pr_debug("free req = %p\n", req);
	spin_lock_irq(&irq_req_lock);
    list_add_tail(&req->queue, &irq_req_free_list);
	spin_unlock_irq(&irq_req_lock);
    return 0; 
}
#ifndef INNODEBUG_LOGDETAIL
#undef pr_debug
#define pr_debug(fmt,...)  
#endif

/**
 * queue req to demod req queue only 
 */
irqreturn_t inno_demod_irq_handler(int irqnr, void *devid)
{
        struct inno_req *req = NULL;
	int ret= 0;
	unsigned long flags;	
	spin_lock_irqsave(&irq_req_lock, flags);
        if (!list_empty(&irq_req_free_list)) {
                req = container_of(irq_req_free_list.next, struct inno_req, queue);
                list_del_init(&req->queue);
                req->context = req;
		req->irq_jif = jiffies;
        } else {
                pr_err("%s:no free req node\n", __func__);
        } 
	spin_unlock_irqrestore(&irq_req_lock, flags);
	if (req) {
		ret = inno_req_async(req);
		if (ret < 0) {  
			/* if demod is powered off, this req should be given back to free list */

			spin_lock_irqsave(&irq_req_lock, flags);
			list_add_tail(&req->queue, &irq_req_free_list);
			spin_unlock_irqrestore(&irq_req_lock, flags);
		}

	}

        return IRQ_HANDLED;
}
#ifndef INNODEBUG_LOGDETAIL
#undef pr_debug
#define pr_debug(fmt, ...) \
	printk(KERN_DEBUG "[INNO/core/inno_irq.c]" pr_fmt(fmt),##__VA_ARGS__)
#endif


#ifdef  IRQ_DEBUG
struct timer_list irq_timer;
struct timer_list irq_timer_1;
int    timer_stop = 0;
static int timer1_cnt = 0, timer_cnt = 0;
static void irq_timer_handler(unsigned long data)
{
        pr_debug("\n\n%s++++++++++++++++++++++++++++++++++++++++++++++++\n", __func__);
        inno_demod_irq_handler(0, 0);
        if (!timer_stop && timer_cnt++ < 10)
                mod_timer(&irq_timer, jiffies + 1*HZ);
}
static void irq_timer_1_handler(unsigned long data)
{
        inno_demod_irq_handler(0, 0);
        pr_debug("\n\n%s++++++++++++++++++++++++++++++++++++++++++++++++\n", __func__);
        if (!timer_stop && timer1_cnt++ < 10)
                mod_timer(&irq_timer_1, jiffies + 1*HZ);
}
#endif

int inno_irq_init(struct inno_demod *inno_demod)
{
        int ret = 0;
        int i;

        pr_debug("%s\n", __func__);

	 spin_lock_init(&irq_req_lock);
        INIT_LIST_HEAD(&irq_req_free_list);
        for (i = 0; i < MAX_REQ_NUM; i++)
        {
                irq_req[i].handler = inno_irq_req_handler;
                irq_req[i].context = NULL;
                irq_req[i].complete = NULL; 
                INIT_LIST_HEAD(&irq_req[i].queue); 
                list_add_tail(&irq_req[i].queue, &irq_req_free_list);
        }        

#ifdef  IRQ_DEBUG
        init_timer(&irq_timer);
        irq_timer.function = irq_timer_handler;
        mod_timer(&irq_timer, jiffies + 3*HZ);

        init_timer(&irq_timer_1);
        irq_timer_1.function = irq_timer_1_handler;
        mod_timer(&irq_timer_1, jiffies + 3*HZ);
#endif 
        return ret;
}

void inno_irq_exit(void)
{
        pr_debug("%s\n", __func__);
#ifdef  IRQ_DEBUG
        timer_stop = 1;
        del_timer_sync(&irq_timer);
        del_timer_sync(&irq_timer_1);
#endif
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sean.zhao <zhaoguangyu@innofidei.com>");
MODULE_DESCRIPTION("innofidei cmmb irq data flow");

