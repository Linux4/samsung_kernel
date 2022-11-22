/*
 * innofidei if2xx demod core driver
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
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/slab.h>
#include "inno_core.h"
#include "inno_irq.h"
#include "inno_ctl.h"
#include "inno_power.h"
#include "inno_sys.h"
#include "inno_demod.h"
#include "inno_comm.h"


#ifdef INNO_DEBUG_LOGTOFILE
extern unsigned char inno_log_buf[];
extern void inno_logfile_write(unsigned char*in_buf);
#undef pr_debug
#undef pr_err
#define pr_debug(fmt, ...) \
		{sprintf(inno_log_buf,"INFO:[INNO/core/_inno_core.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#define pr_err(fmt, ...) \
		{sprintf(inno_log_buf,"ERR:[INNO/core/_inno_core.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#else

#undef pr_debug
#define pr_debug(fmt, ...) \
printk(KERN_DEBUG "[INNO/core/_inno_core.c]" pr_fmt(fmt),##__VA_ARGS__)

#undef pr_err
#define pr_err(fmt, ...) \
printk(KERN_ERR "[INNO/core/_inno_core.c]" pr_fmt(fmt), ##__VA_ARGS__)

#endif
				

struct inno_lgxinfo {
        unsigned     n_lgx_info;
        struct inno_lgx_info    *lgx_info;
};


static struct inno_lgxinfo      lgxinfo;
static void inno_register_lgxinfo(struct inno_lgx_info const *info, unsigned n)
{
        lgxinfo.lgx_info = (struct inno_lgx_info *)kmalloc(n * sizeof *info, GFP_KERNEL);//checked
        if (!lgxinfo.lgx_info)
                return;
        lgxinfo.n_lgx_info = n;
        memcpy(lgxinfo.lgx_info, info, n * sizeof *info);
        
}

static void inno_free_lgxinfo(void)
{
        if (lgxinfo.lgx_info)
                kfree(lgxinfo.lgx_info);
        lgxinfo.n_lgx_info = 0;
}

static void scan_lgxinfo(struct lgx_driver *lgx_drv)
{
        unsigned                n;
        struct inno_lgx_info    *info = lgxinfo.lgx_info;

        for (n = lgxinfo.n_lgx_info; n > 0; n--, info++) {
                pr_debug("%s:info<name = %s, id = %d>, driver<name = %s>\n", __func__, info->name, info->id, lgx_drv->driver.name);
                if (strcmp(info->name, lgx_drv->driver.name) == 0)
                        info->priv = (void *)inno_lgx_new_device(info->name, lgx_drv->major, info->id, NULL);
        }
}

/*static*/ struct inno_demod  *inno_demod = NULL; 

static int req_default_complete(struct inno_req *req)
{
        complete(req->done);
        return 0;
}
#ifndef INNODEBUG_LOGDETAIL
#undef pr_debug
#define pr_debug(fmt,...)  
#endif

/**
 * inno_req_async -simple add req to demod req queue and wakeup req workqueue
 * @inno_req: initialized by caller
 */
int inno_req_async(struct inno_req *req)
{

	unsigned long flags;	
        if (inno_demod->power_status == POWER_OFF)
                return -EIO;

	spin_lock_irqsave(&inno_demod->req_lock, flags);
        INIT_LIST_HEAD(&req->queue);
        list_add_tail(&req->queue, &inno_demod->req_queue);
        queue_work(inno_demod->req_wq, &inno_demod->req_work);
	spin_unlock_irqrestore(&inno_demod->req_lock, flags);
        return 0;
}
EXPORT_SYMBOL_GPL(inno_req_async);

/**
 * inno_req_sync -simple add req to demod req queue and wait for done
 * @inno_req: initialized by caller
 */
int inno_req_sync(struct inno_req *req)
{
        DECLARE_COMPLETION_ONSTACK(done);
        
        if (inno_demod->power_status == POWER_OFF)
                return -EAGAIN;
    
        req->done = &done;
        req->complete = req_default_complete;
        req->result = 0;
        inno_req_async(req);
        wait_for_completion(&done);
        pr_debug("%s:result = %d\n", __func__, req->result);
        return req->result;
}
EXPORT_SYMBOL_GPL(inno_req_sync);

/* reqs are really done by this work queue */
static void reqwork_handler(struct work_struct *_work)
{
        pr_debug("%s\n", __func__);
        spin_lock_irq(&inno_demod->req_lock);
        while (!list_empty(&inno_demod->req_queue)) {
                struct inno_req                 *req;

                req = container_of(inno_demod->req_queue.next, struct inno_req, queue);
                list_del_init(&req->queue);
                spin_unlock_irq(&inno_demod->req_lock);
               
                pr_debug("%s:process req +++++++++++++++++++++++++\n", __func__);
                if (req->handler)
                        req->result = req->handler(req->context); 
                pr_debug("%s:process req -------------------------\n", __func__);
                
                if (req->complete)
                        req->complete(req); 
                
                spin_lock_irq(&inno_demod->req_lock);
        }
        spin_unlock_irq(&inno_demod->req_lock);
        pr_debug("%s:exit\n", __func__);
}

#ifndef INNODEBUG_LOGDETAIL
#undef pr_debug
#define pr_debug(fmt, ...) \
	printk(KERN_DEBUG "[INNO/core/_inno_core.c]" pr_fmt(fmt),##__VA_ARGS__)
#endif

static int inno_lgx_uevent(struct device *dev, struct kobj_uevent_env *env)
{
        struct lgx_device  *pdev;
        pdev = container_of(dev, struct lgx_device, dev);

        add_uevent_var(env, "MODALIAS=:innolgxbus:%s", pdev->name);
        return 0;
}

/**
 * inno_lgx_match -bind lgx_device to lgx_driver
 * @dev:device
 * @drv:driver
 */
static int inno_lgx_match(struct device *dev, struct device_driver *drv)
{
        struct lgx_device *lgx;

        lgx = container_of(dev, struct lgx_device, dev);
        return (strcmp(lgx->name, drv->name) == 0);
}

struct bus_type inno_lgx_bus_type = {
        .name           = "innolgx",
        .match          = inno_lgx_match,
        .uevent         = inno_lgx_uevent,
};

static int inno_lgx_drv_probe(struct device *_dev)
{
        struct lgx_driver *drv;
        struct lgx_device *dev;
        pr_debug("%s\n", __func__);

        drv = container_of(_dev->driver, struct lgx_driver, driver);
        dev = container_of(_dev, struct lgx_device, dev);
        return drv->probe(dev);
}

static int inno_lgx_drv_remove(struct device *_dev)
{
        struct lgx_driver *drv;
        struct lgx_device *dev;
        pr_debug("%s\n", __func__);

        drv = container_of(_dev->driver, struct lgx_driver, driver);
        dev = container_of(_dev, struct lgx_device, dev);
        return drv->remove(dev);
}

/**
 * inno_lgx_driver_register - register a lgx_driver to inno_lgx_bus_type
 * @drv:initialized lgx_driver by inno_lgx.c
 */
int inno_lgx_driver_register(struct lgx_driver *drv)
{
        int ret = 0;
        pr_debug("%s\n", __func__);
        if (inno_demod == NULL) {
                pr_err("platform_device is not present\n");
                return -EIO;
        }
        drv->driver.bus = &inno_lgx_bus_type;
        if (drv->probe)
                drv->driver.probe = inno_lgx_drv_probe;
        if (drv->remove)
                drv->driver.remove = inno_lgx_drv_remove;
        ret = driver_register(&drv->driver);
        if (ret < 0)
                return ret;

        scan_lgxinfo(drv);
        return ret;
}
EXPORT_SYMBOL_GPL(inno_lgx_driver_register);

void inno_lgx_driver_unregister(struct lgx_driver *drv)
{               
        unsigned                n;
        struct inno_lgx_info    *info = lgxinfo.lgx_info;

        pr_debug("%s\n", __func__);

        for (n = lgxinfo.n_lgx_info; n > 0; n--, info++) {
                pr_debug("%s:info<name = %s, id = %d>, driver<name = %s>\n", __func__, info->name, info->id, drv->driver.name);
                if (strcmp(info->name, drv->driver.name) == 0) {
                        if (info->priv)
                                inno_lgx_device_unregister((struct lgx_device *)info->priv);
                }
        }

        driver_unregister(&drv->driver);
}
EXPORT_SYMBOL_GPL(inno_lgx_driver_unregister);

static void inno_lgx_dev_release(struct device *dev)
{
        struct lgx_device *lgx = container_of(dev, struct lgx_device,
                                                  dev);
        pr_debug("%s:%s\n", __func__, dev_name(&lgx->dev));
	kfree(lgx->name);
        kfree(lgx);
}

/**
 * inno_lgx_alloc_device - create a lgx_device
 * @name: only "innolg" and "innouam" is useful device name, this name is used by inno_irq.c also
 * @major: lgx char device major number
 * @id   : lgx channel id, equ to char device minor number
 * @data : priv
 */
struct lgx_device *inno_lgx_alloc_device(char *name, int major, int id, void *data)
{
        struct lgx_device       *lgx;

        lgx = kzalloc(sizeof(*lgx), GFP_KERNEL);//checked
        if (!lgx) {
                dev_err(inno_demod->dev, "cannot alloc lgx_device\n");
                return NULL;
        }

        lgx->name = kzalloc(strlen(name)+1, GFP_KERNEL); //checked 
        if (!lgx->name) {
                dev_err(inno_demod->dev, "cannot alloc lgx_device name\n");
                return NULL;
        }

        strcpy(lgx->name, name);
        lgx->id         = id;
        lgx->dev.devt   = MKDEV(major, id);
        lgx->dev.parent = inno_demod->dev;
        lgx->dev.bus = &inno_lgx_bus_type;
        lgx->dev.release = inno_lgx_dev_release;
        dev_set_name(&lgx->dev, "%s%u", name, id);
        dev_set_drvdata(&lgx->dev, data);
        device_initialize(&lgx->dev);
        return lgx;
}

int inno_lgx_add_device(struct lgx_device *lgx)
{
        int status;
        
        if (bus_find_device_by_name(&inno_lgx_bus_type, NULL, dev_name(&lgx->dev))
                        != NULL) {
                dev_err(inno_demod->dev, "lg%d already in use\n",
                                lgx->id);
                status = -EBUSY;
                goto done;
        }
        
        /* Device may be bound to an active driver when this returns */
        status = device_add(&lgx->dev);
        if (status < 0)
                dev_err(inno_demod->dev, "can't %s %s, status %d\n",
                                "add", dev_name(&lgx->dev), status);
done:
        return status;
}

/**
 * inno_lgx_new_device - create a lgx_device and attach it to inno_lgx_bus_type
 * @name: only "innolg" and "innouam" is useful device name, this name is used by inno_irq.c also
 * @major: lgx char device major number
 * @id   : lgx channel id, equ to char device minor number
 * @data : priv
 */
struct lgx_device *inno_lgx_new_device(char *name, int major, int id, void *data)
{
        int ret;
        struct lgx_device *idev;
        pr_debug("%s\n", __func__);
        if (inno_demod == NULL) {
                pr_err("platform_device is not present\n");
                return NULL;
        }
        idev = inno_lgx_alloc_device(name, major, id, data);
		if (idev == NULL) {return NULL;}
        ret = inno_lgx_add_device(idev);
        if (ret < 0) {
                kfree(idev);
                return NULL;
        }
        return idev;
}
EXPORT_SYMBOL_GPL(inno_lgx_new_device);

void inno_lgx_device_unregister(struct lgx_device *lgx)
{
        pr_debug("%s:%s\n", __func__, dev_name(&lgx->dev));
        device_del(&lgx->dev);

        /* ref of device is 1 after device_initialize
         * so need to call put_device to dec device ref to 0,
         * then release func will be called */
        put_device(&lgx->dev);
}
EXPORT_SYMBOL_GPL(inno_lgx_device_unregister);

/* if demod->kerf equ to 0, this func will be called auto, we power down demod here */
static void inno_demod_release(struct kref *kref)
{
        struct list_head *node, *next;

        pr_debug("%s\n", __FUNCTION__);
        pr_debug("%s:power off device here\n", __func__);
        inno_demod->power_status = POWER_OFF;

        spin_lock_irq(&inno_demod->req_lock);
        list_for_each_safe(node, next, &inno_demod->req_queue) {
                list_del(node);
        }
        spin_unlock_irq(&inno_demod->req_lock);

        flush_workqueue(inno_demod->req_wq);
        inno_demod_power(inno_demod, 0);
        
}

int inno_demod_power_status(void)
{
        return inno_demod->power_status;
}

void inno_demod_dec_ref(void)
{
        pr_debug("%s\n", __func__);
        if (inno_demod->power_status == POWER_OFF) 
                return ;
        kref_put(&inno_demod->kref, inno_demod_release);
}
EXPORT_SYMBOL_GPL(inno_demod_dec_ref);

/* if it's first time , we power on demod and download firmware here */
int inno_demod_inc_ref(void)
{
        int ret = 0;
       
        if (inno_demod->power_status == POWER_OFF) {
                pr_debug("%s:power on device here\n", __func__);

                /* download firmware is done by inno_demod_power */
                ret = inno_demod_power(inno_demod, 1);
                if (ret < 0)
                        return ret;

                inno_demod->power_status = POWER_ON;
                kref_init(&inno_demod->kref);
        }
        else {               
                pr_debug("%s:inc demod ref\n", __func__);
                kref_get(&inno_demod->kref);
        }

        return ret;
}
EXPORT_SYMBOL_GPL(inno_demod_inc_ref);

static int inno_demod_probe(struct platform_device *pdev)
{
        int ret = 0;
        struct inno_lgx_platform_info *plat_info = pdev->dev.platform_data;
        ret = bus_register(&inno_lgx_bus_type);
        if (ret < 0)
                goto exit;
        
        inno_demod = kzalloc(sizeof(*inno_demod), GFP_KERNEL); //checked
        if (inno_demod == NULL) {
                ret = -ENOMEM;
                goto err1;
        }

        if (inno_comm_init()) {
                ret = -EIO;
                goto err1;
        }

        memset(&lgxinfo, 0 ,sizeof(lgxinfo));

        inno_demod->power_status = POWER_OFF;        
        inno_demod->cls.owner = THIS_MODULE;
        inno_demod->cls.name = "innodemod";
        inno_demod_sysfs_init(&inno_demod->cls);
        ret = class_register(&inno_demod->cls);
        if (ret) {
                ret = -EIO;
                goto err1;
        }

        inno_demod->pdev = pdev;
        inno_demod->dev = &pdev->dev;

        inno_demod->req_wq = create_singlethread_workqueue("demodreq_workqueue");
        INIT_WORK(&inno_demod->req_work, reqwork_handler);
        INIT_LIST_HEAD(&inno_demod->req_queue);
        spin_lock_init(&inno_demod->req_lock);
        
        inno_irq_init(inno_demod);
 
        inno_ctl_init(inno_demod);
       
        if (plat_info)
                inno_register_lgxinfo(plat_info->info, plat_info->num_info); 

	goto exit;
err1:
        if (inno_demod)
                kfree(inno_demod);
        bus_unregister(&inno_lgx_bus_type);
        
exit:
        return ret;
}

static int __devexit inno_demod_remove(struct platform_device *pdev)
{
        struct list_head *node, *next;
        pr_debug("%s\n", __func__);

        inno_demod->power_status = POWER_OFF;

        spin_lock_irq(&inno_demod->req_lock);
        list_for_each_safe(node, next, &inno_demod->req_queue) {
                list_del(node);
        }
        spin_unlock_irq(&inno_demod->req_lock);

        flush_workqueue(inno_demod->req_wq);
        inno_demod_power(inno_demod, 0);

        inno_free_lgxinfo();

        inno_ctl_exit();
        inno_irq_exit();

        destroy_workqueue(inno_demod->req_wq);

        class_unregister(&inno_demod->cls);

        kfree(inno_demod);
        bus_unregister(&inno_lgx_bus_type);
        return 0;
}

static struct platform_driver inno_demod_driver = {
        .probe          = inno_demod_probe,
        .remove         = __devexit_p(inno_demod_remove),
        .driver         = {
                .name   = "inno-demod",
                        .owner  = THIS_MODULE,
        },
};

static int __init inno_demod_init(void) 
{ 
        int ret;
        pr_debug("%s\n", __func__);

        ret = platform_driver_register(&inno_demod_driver);
        if (ret < 0) {
                pr_err("register inno demod driver failed! (%d)\n", ret);
                return ret;
        }
       
        return 0;
}

static void __exit inno_demod_exit(void) 
{
        platform_driver_unregister(&inno_demod_driver);
        pr_debug("%s\n", __func__);
}


module_init(inno_demod_init);
module_exit(inno_demod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sean.zhao <zhaoguangyu@innofidei.com>");
MODULE_DESCRIPTION("innofidei cmmb core control");

