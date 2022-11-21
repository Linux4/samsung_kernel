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
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif
#include "soc/sprd/mailbox.h"
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sizes.h>
#include <soc/sprd/sci_glb_regs.h>
#include <linux/sipc.h>
#include <linux/spipe.h>
#include <linux/sipc_priv.h>
#include "sctrl.h"

#define SMSG_TXBUF_ADDR		(0)
#define SMSG_TXBUF_SIZE		(SZ_1K)
#define SMSG_RXBUF_ADDR		(SMSG_TXBUF_SIZE)
#define SMSG_RXBUF_SIZE		(SZ_1K)
/*
#define SMSG_RINGHDR		(SMSG_TXBUF_SIZE + SMSG_RXBUF_SIZE)
#define SMSG_TXBUF_RDPTR	(SMSG_RINGHDR + 0)
#define SMSG_TXBUF_WRPTR	(SMSG_RINGHDR + 4)
#define SMSG_RXBUF_RDPTR	(SMSG_RINGHDR + 8)
#define SMSG_RXBUF_WRPTR	(SMSG_RINGHDR + 12)
*/

static struct class	*sctrl_class;
static struct sctrl_mgr strcl_mgr_val;
//#define SCTRL_LOOPBACK_DEBUG
#ifdef SCTRL_LOOPBACK_DEBUG
struct timer_list sctrl_ti;
static void sctrl_tx(void)
{
    struct smsg mrevt;
    smsg_set(&mrevt, 1, 0, 0, 0);
    smsg_send(SIPC_ID_PMIC, &mrevt, -1);
}
void sctrl_timer_func(unsigned long arg)
{
    struct timer_list *timer=(struct timer_list *)arg;
    unsigned long  j = jiffies;
    mod_timer(timer,j+8*HZ);
    pr_err(" sctrl_timer_func expired at jif=%lx\n", j);
    sctrl_tx();
}
#endif
static int sctrl_thread(void *data)
{
    struct sctrl_mgr *strcl_mgr_ptr= data;
    struct smsg mrecv;
    int rval;
    struct sched_param param = {.sched_priority = 90};

    /*set the thread as a real time thread, and its priority is 90*/
    sched_setscheduler(current, SCHED_RR, &param);

    /* since the channel open may hang, we call it in the sctrl thread */
    rval = smsg_ch_open(strcl_mgr_ptr->dst, strcl_mgr_ptr->channel, -1);
    if (rval != 0) {
        pr_err("Failed to open channel %d, rval=%d\n", strcl_mgr_ptr->channel,rval);
        /* assign NULL to thread poniter as failed to open channel */
        strcl_mgr_ptr->thread = NULL;
        return rval;
    }

#ifdef SCTRL_LOOPBACK_DEBUG
    /*initiated a timer to send smsg to arm7*/
    init_timer(&sctrl_ti);
    sctrl_ti.data=(unsigned long)&sctrl_ti;
    sctrl_ti.function=sctrl_timer_func;
    sctrl_ti.expires = jiffies + HZ;
    add_timer(&sctrl_ti);
#endif

    while (!kthread_should_stop()) {
        /* monitor sctrl rdptr/wrptr update smsg */
        smsg_set(&mrecv, strcl_mgr_ptr->channel, 0, 0, 0);
        rval = smsg_recv(strcl_mgr_ptr->dst, &mrecv, -1);

        if (rval == -EIO) {
            /* channel state is free */
            msleep(5);
            continue;
        }
        printk(KERN_ERR "sctrl thread recv msg: dst=%d, channel=%d, "
                        "type=%d, flag=0x%04x, value=0x%08x\n",
                            strcl_mgr_ptr->dst, strcl_mgr_ptr->channel,
                            mrecv.type, mrecv.flag, mrecv.value);
        switch (mrecv.type) {
        case  SMSG_TYPE_DFS_RSP:
            if(mrecv.value == 0)
            {
                strcl_mgr_ptr->state = SCTL_STATE_REV_CMPT;
            }
            else
            {
                strcl_mgr_ptr->state = SCTL_STATE_READY;
            }
            wake_up_interruptible_all(&strcl_mgr_ptr->rx_pending);
            break;
        default :
            break;
        }
#if 0
        case SMSG_TYPE_CLOSE:
        /* handle channel recovery */
        smsg_close_ack(strcl_mgr_ptr->dst, strcl_mgr_ptr->channel);
        strcl_mgr_ptr->state = SCTL_STATE_IDLE;
            break;

        case SMSG_TYPE_CMD:
        /* respond cmd done for sctrl init */
        smsg_set(&mcmd, strcl_mgr_ptr->channel, SMSG_TYPE_DONE,SMSG_DONE_SBUF_INIT, 0);
        smsg_send(strcl_mgr_ptr->dst, &mcmd, -1);
        strcl_mgr_ptr->state = SCTL_STATE_READY;
            break;
#endif
    }
    return 0;
}
void sctrl_send_async(uint32_t type, uint32_t target_id, uint32_t value)
{
        struct smsg mevt;
        smsg_set(&mevt,  1, type, target_id ,value);
        smsg_send(SIPC_ID_PMIC, &mevt, -1);
}
int sctrl_send_sync(uint32_t type, uint32_t target_id, uint32_t value)
{
        struct smsg mrecv;
        int rval = 0;
        int timeout =msecs_to_jiffies(50000);
        smsg_set(&mrecv,  1, type, target_id ,value);
        smsg_send(SIPC_ID_PMIC, &mrecv, -1);
        if(type ==SMSG_TYPE_DFS)
        {
            rval = wait_event_interruptible_timeout(strcl_mgr_val.rx_pending,\
                strcl_mgr_val.state != SCTL_STATE_IDLE, timeout);
            if (rval < 0) {
                pr_warning(" sctrl_send_sync wait interrupted!\n");
            } else if (rval == 0) {
                pr_err(" sctrl_send_sync wait timeout!\n");
                rval = -ETIME;
            }
            if (strcl_mgr_val.state == SCTL_STATE_REV_CMPT)
            {
                strcl_mgr_val.state =  SCTL_STATE_IDLE;
                pr_info(" sctrl_send_sync DFS Response!\n");
                return 0;
            }
            else
            {
                goto FAIL;
            }
        }
        else {
                goto FAIL;
        }
FAIL:
        strcl_mgr_val.state =  SCTL_STATE_IDLE;
        return -EINVAL;
}
int sctrl_create(uint8_t dst, uint8_t channel, uint32_t bufnum)
{
    int result;
    strcl_mgr_val.state = SCTL_STATE_IDLE;
    strcl_mgr_val.dst = dst;
    strcl_mgr_val.channel = channel;
    init_waitqueue_head(&strcl_mgr_val.tx_pending);
    init_waitqueue_head(&strcl_mgr_val.rx_pending);
    strcl_mgr_val.thread = kthread_create(sctrl_thread, &strcl_mgr_val, "sctrl-%d-%d", dst, channel);
    if (IS_ERR(strcl_mgr_val.thread)) {
        printk(KERN_ERR "Failed to create kthread: sctrl-%d-%d\n", dst, channel);
            result = PTR_ERR(strcl_mgr_val.thread);
            return result;
    }
    pr_err(" sctrl create strcl_mgr_val.thread %p\n", strcl_mgr_val.thread);
    wake_up_process(strcl_mgr_val.thread);
    return 0;
}
static int sctrl_open(struct inode *inode, struct file *filp)
{
	int minor = iminor(filp->f_path.dentry->d_inode);
	struct sctrl_device *sctrl;
	struct sctrl_buf *sbuf;

	sctrl = container_of(inode->i_cdev, struct sctrl_device, cdev);
	sbuf = kmalloc(sizeof(struct sctrl_buf), GFP_KERNEL);
	if (!sbuf) {
            pr_err(" sctrl open malloc sbuf fail\n");
            return -ENOMEM;
	}
	filp->private_data = sbuf;

	sbuf->dst = sctrl->init->dst;
	sbuf->channel = sctrl->init->channel;
	sbuf->bufid = minor - sctrl->minor;
        pr_info("sctrl open sucess\n");
	return 0;
}

static int sctrl_release(struct inode *inode, struct file *filp)
{
	struct sctrl_buf *sbuf = filp->private_data;

	if (sbuf) {
		kfree(sbuf);
	}

	return 0;
}

static ssize_t sctrl_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct sctrl_buf *sbuf = filp->private_data;
        struct smsg mevt = {0};
	int timeout = -1;

	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
	}
        if(smsg_recv(sbuf->dst,&mevt,timeout) == 0)
        {
            if(copy_to_user((void __user *)buf, (void *)&mevt, sizeof(struct smsg)))
		    return -1;
	    else
		    return 0;
        }
        else{
            return -1;
        }
}

static ssize_t sctrl_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct sctrl_buf *sbuf = filp->private_data;
        struct smsg mevt = {0};
	int timeout = -1;

	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
	}
        pr_info("sctrl open sucess\n");

        sctrl_send_sync(SMSG_TYPE_DFS, SIPC_ID_PMIC,0xfe);
	return count;
}

static unsigned int sctrl_poll(struct file *filp, poll_table *wait)
{
    struct sctrl_buf *sbuf = filp->private_data;
    unsigned int mask = 0;
    if (!sbuf) {
        return -ENODEV;
    }
    return mask;
}

static long sctrl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}
uint32_t pmic_rxirq_status(void)
{
    return 1;
}
void pmic_rxirq_clear(void)
{
}
void pmic_txirq_trigger(void)
{
    mbox_raw_sent(ARM7, 0);
}

static struct smsg_ipc smsg_ipc_pmic = {
	.name = "sipc-pmic",
	.dst = SIPC_ID_PMIC,
	.core_id = ARM7,
	.rxirq_status = pmic_rxirq_status,
	.rxirq_clear = pmic_rxirq_clear,
	.txirq_trigger = pmic_txirq_trigger,
};

static int __init sipc_pmic_init(ssize_t base)
{

	smsg_ipc_pmic.txbuf_size = SMSG_TXBUF_SIZE / sizeof(struct smsg);
	smsg_ipc_pmic.txbuf_addr = base + SMSG_TXBUF_ADDR;
	smsg_ipc_pmic.txbuf_rdptr =base+0x1c00+ 0;
	smsg_ipc_pmic.txbuf_wrptr = base+0x1c00+ 8;

	smsg_ipc_pmic.rxbuf_size = SMSG_RXBUF_SIZE / sizeof(struct smsg);
	smsg_ipc_pmic.rxbuf_addr = base + SMSG_RXBUF_ADDR;
	smsg_ipc_pmic.rxbuf_rdptr =  base+0x1c00+ 16;
	smsg_ipc_pmic.rxbuf_wrptr =  base+0x1c00+ 24;

	return smsg_ipc_create(SIPC_ID_PMIC, &smsg_ipc_pmic);
}
static const struct file_operations sctrl_fops = {
	.open		= sctrl_open,
	.release	= sctrl_release,
	.read		= sctrl_read,
	.write		= sctrl_write,
	.poll		= sctrl_poll,
	.unlocked_ioctl	= sctrl_ioctl,
	.owner		= THIS_MODULE,
	.llseek		= default_llseek,
};

static int sctrl_parse_dt(struct spipe_init_data **init, struct device *dev)
{
#ifdef CONFIG_OF
	struct device_node *np = dev->of_node;
	struct spipe_init_data *pdata = NULL;
        ssize_t ring_base = 0;
        ssize_t vm_base= 0;
        uint32_t rxbuf_size = 0;
        uint32_t txbuf_size = 0;
	int ret;
	uint32_t data;

	pdata = kzalloc(sizeof(struct spipe_init_data), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "Failed to allocate pdata memory\n");
		return -ENOMEM;
	}

	ret = of_property_read_string(np, "sprd,name", (const char**)&pdata->name);
	if (ret) {
		printk(KERN_ERR "Failed to read sprd name\n");
		goto error;
	}

	ret = of_property_read_u32(np, "sprd,dst", (uint32_t *)&data);
	if (ret) {
		printk(KERN_ERR "Failed to read sprd dst\n");
		goto error;
	}
	pdata->dst = (uint8_t)data;

	ret = of_property_read_u32(np, "sprd,channel", (uint32_t *)&data);
	if (ret) {
		printk(KERN_ERR "Failed to read sprd channel\n");
		goto error;
	}
	pdata->channel = (uint8_t)data;
        ret = of_property_read_u32(np, "sprd,ringnr", (uint32_t *)&data);
        if (ret) {
            printk(KERN_ERR "Failed to read sprd ringnr, ret=%d\n", ret);
            goto error;
        }
        pdata->ringnr=data;
	ret = of_property_read_u32(np, "sprd,ringbase", (uint32_t *)&vm_base);
	if(ret) {
		goto error;
	}
	ring_base = (unsigned long)ioremap_nocache(vm_base,SZ_8K);
	if(!ring_base){
		printk(KERN_ERR "Unable to map ring base\n");
		goto error;
	}

	ret = of_property_read_u32(np, "sprd,size-rxbuf", &rxbuf_size);
	if (ret) {
		goto error;
	}

	ret = of_property_read_u32(np, "sprd,size-txbuf", &txbuf_size);
	if (ret) {
		goto error;
	}
        pr_info("sipc_pmic_init with base=0x%lx\n", ring_base);
        sipc_pmic_init(ring_base);
	*init = pdata;
	return ret;
error:
	kfree(pdata);
	*init = NULL;
	return ret;
#else
	return -ENODEV;
#endif
}

static inline void sctrl_destroy_pdata(struct spipe_init_data **init)
{
#ifdef CONFIG_OF
	struct spipe_init_data *pdata = *init;

	if (pdata) {
		kfree(pdata);
	}

	*init = NULL;
#else
	return;
#endif
}

static int sctrl_probe(struct platform_device *pdev)
{
	struct spipe_init_data *init = pdev->dev.platform_data;
	struct sctrl_device *sctrl;
	dev_t devid;
	int i, rval;

	if (pdev->dev.of_node && !init) {
		rval = sctrl_parse_dt(&init, &pdev->dev);
		if (rval) {
			printk(KERN_ERR "Failed to parse sctrl device tree, ret=%d\n", rval);
			return rval;
		}
	}
	pr_info("sctrl: after parse device tree, name=%s, dst=%u, channel=%u, ringnr=%u, rxbuf_size=%u, txbuf_size=%u\n",
		init->name, init->dst, init->channel, init->ringnr, init->rxbuf_size, init->txbuf_size);


	rval =  sctrl_create(init->dst, init->channel, init->ringnr);
	if (rval != 0) {
		printk(KERN_ERR "Failed to create sctrl device: %d\n", rval);
		sctrl_destroy_pdata(&init);
		return rval;
	}

	sctrl = kzalloc(sizeof(struct sctrl_device), GFP_KERNEL);
	if (sctrl == NULL) {
		sctrl_destroy_pdata(&init);
		printk(KERN_ERR "Failed to allocate sctrl_device\n");
		return -ENOMEM;
	}
	rval = alloc_chrdev_region(&devid, 0, init->ringnr, init->name);
	if (rval != 0) {
		kfree(sctrl);
		sctrl_destroy_pdata(&init);
		printk(KERN_ERR "Failed to alloc sctrl chrdev\n");
		return rval;
	}

	cdev_init(&(sctrl->cdev), &sctrl_fops);
	rval = cdev_add(&(sctrl->cdev), devid, init->ringnr);
	if (rval != 0) {
		kfree(sctrl);
		unregister_chrdev_region(devid, init->ringnr);
		sctrl_destroy_pdata(&init);
		printk(KERN_ERR "Failed to add sctrl cdev\n");
		return rval;
	}

	sctrl->major = MAJOR(devid);
	sctrl->minor = MINOR(devid);
	if (init->ringnr > 1) {
		for (i = 0; i < init->ringnr; i++) {
			device_create(sctrl_class, NULL,
				MKDEV(sctrl->major, sctrl->minor + i),
				NULL, "%s%d", init->name, i);
		}
	} else {
		device_create(sctrl_class, NULL,
			MKDEV(sctrl->major, sctrl->minor),
			NULL, "%s", init->name);
	}

	sctrl->init = init;
	platform_set_drvdata(pdev, sctrl);
	return 0;
}

static int  sctrl_remove(struct platform_device *pdev)
{
	struct sctrl_device *sctrl = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < sctrl->init->ringnr; i++) {
		device_destroy(sctrl_class,
				MKDEV(sctrl->major, sctrl->minor + i));
	}
	cdev_del(&(sctrl->cdev));
	unregister_chrdev_region(
		MKDEV(sctrl->major, sctrl->minor), sctrl->init->ringnr);

	sctrl_destroy_pdata(&sctrl->init);

	kfree(sctrl);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id sctrl_match_table[] = {
	{.compatible = "sprd,sctrl", },
	{ },
};

static struct platform_driver sctrl_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sctrl",
		.of_match_table = sctrl_match_table,
	},
	.probe = sctrl_probe,
	.remove = sctrl_remove,
};

static int __init sctrl_init(void)
{
	sctrl_class = class_create(THIS_MODULE, "sctrl");
	if (IS_ERR(sctrl_class))
		return PTR_ERR(sctrl_class);

	return platform_driver_register(&sctrl_driver);
}

static void __exit sctrl_exit(void)
{
	class_destroy(sctrl_class);
	platform_driver_unregister(&sctrl_driver);
}

module_init(sctrl_init);
module_exit(sctrl_exit);

MODULE_AUTHOR("Andrew Yang");
MODULE_DESCRIPTION("SIPC/SCTRL driver");
MODULE_LICENSE("GPL");
