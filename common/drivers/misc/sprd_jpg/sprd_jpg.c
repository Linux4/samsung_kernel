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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <video/sprd_jpg.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/globalregs.h>
#include <mach/sci.h>

#include <linux/sprd_iommu.h>

#define JPG_MINOR MISC_DYNAMIC_MINOR
#define JPG_TIMEOUT_MS 1000

#define USE_INTERRUPT
/*#define RT_VSP_THREAD*/

#define DEFAULT_FREQ_DIV 0x0

#define JPG_RESET_REG       0x60d00004      //bit[6]
#define JPG_CLK_EN_REG  0x60d00000  //bit[5]
#define AXI_CLK_EN_REG  0x60d00008      //bit[6]

#ifndef CONFIG_OF
#define SPRD_MMAHB_BASE_DT SPRD_MMAHB_BASE
#define SPRD_JPG_BASE_DT SPRD_JPG_BASE
#else
#define         clk_enable      clk_prepare_enable
#define         clk_disable     clk_disable_unprepare
static unsigned int SPRD_JPG_BASE_DT;

//will be removed later
#define SPRD_MMAHB_BASE_DT		SCI_IOMAP(0x340000)
#endif

#define GLB_CTRL_OFFSET		0x00
#define MB_CFG_OFFSET		0x04

#define GLB_PITCH_OFFSET	0x0C
#define GLB_STS_OFFSET		0x10

#define GLB_INT_STS_OFFSET	0x20
#define GLB_INT_EN_OFFSET	0x24
#define GLB_INT_CLR_OFFSET	0x28
#define GLB_INT_RAW_OFFSET	0x2C

struct jpg_fh {
    int is_jpg_aquired;
    int is_clock_enabled;
};

struct jpg_dev {
    unsigned int freq_div;

    struct semaphore jpg_mutex;

    wait_queue_head_t wait_queue_work_MBIO;
    int condition_work_MBIO;
    wait_queue_head_t wait_queue_work_VLC;
    int condition_work_VLC;
    wait_queue_head_t wait_queue_work_BSM;
    int condition_work_BSM;
    int jpg_int_status;

    struct clk *jpg_clk;
    struct clk *jpg_parent_clk;
    struct clk *mm_clk;

    unsigned int irq;

    struct jpg_fh *jpg_fp;
    struct device_node *dev_np;
};

static struct jpg_dev jpg_hw_dev;

struct clock_name_map_t {
    unsigned long freq;
    char *name;
};

static struct clock_name_map_t clock_name_map[] = {
#if defined(CONFIG_ARCH_SCX15)
    {192000000,"clk_192m"},
    {153000000,"clk_153p6m"},
    {128000000,"clk_128m"},
    {76800000,"clk_76m8"}
#else
    {256000000,"clk_256m"},
    {192000000,"clk_192p6m"},
    {128000000,"clk_128m"},
    {76800000,"clk_76m8"}
#endif
};

static int max_freq_level = ARRAY_SIZE(clock_name_map);

static char *jpg_get_clk_src_name(unsigned int freq_level)
{
    if (freq_level >= max_freq_level ) {
        printk(KERN_INFO "set freq_level to 0");
        freq_level = 0;
    }

    return clock_name_map[freq_level].name;
}

static int find_jpg_freq_level(unsigned long freq)
{
    int level = 0;
    int i;
    for (i = 0; i < max_freq_level; i++) {
        if (clock_name_map[i].freq == freq) {
            level = i;
            break;
        }
    }
    return level;
}

static void disable_jpg (struct jpg_fh *jpg_fp)
{
    clk_disable(jpg_hw_dev.jpg_clk);
    jpg_fp->is_clock_enabled= 0;
    pr_debug("jpg ioctl JPG_DISABLE\n");

    return;
}

static void release_jpg(struct jpg_fh *jpg_fp)
{
    pr_debug("jpg ioctl JPG_RELEASE\n");
    jpg_fp->is_jpg_aquired = 0;
    up(&jpg_hw_dev.jpg_mutex);

    return;
}
#if defined(CONFIG_ARCH_SCX35)
#ifdef USE_INTERRUPT
static irqreturn_t jpg_isr(int irq, void *data);
#endif
#endif
static long jpg_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret, cmd0;
    struct clk *clk_parent;
    char *name_parent;
    unsigned long frequency;
    struct jpg_fh *jpg_fp = filp->private_data;

    switch (cmd) {
    case JPG_CONFIG_FREQ:
        get_user(jpg_hw_dev.freq_div, (int __user *)arg);
        name_parent = jpg_get_clk_src_name(jpg_hw_dev.freq_div);
        clk_parent = clk_get(NULL, name_parent);
        if ((!clk_parent )|| IS_ERR(clk_parent)) {
            printk(KERN_ERR "clock[%s]: failed to get parent [%s] \
by clk_get()!\n", "clk_vsp", name_parent);
            return -EINVAL;
        }
        ret = clk_set_parent(jpg_hw_dev.jpg_clk, clk_parent);
        if (ret) {
            printk(KERN_ERR "clock[%s]: clk_set_parent() failed!",
                   "clk_jpg");
            return -EINVAL;
        } else {
            clk_put(jpg_hw_dev.jpg_parent_clk);
            jpg_hw_dev.jpg_parent_clk = clk_parent;
        }
        pr_debug(KERN_INFO "JPG_CONFIG_FREQ %d\n", jpg_hw_dev.freq_div);
        break;
    case JPG_GET_FREQ:
        frequency = clk_get_rate(jpg_hw_dev.jpg_clk);
        ret = find_jpg_freq_level(frequency);
        put_user(ret, (int __user *)arg);
        printk(KERN_INFO "jpg ioctl JPG_GET_FREQ %d\n", ret);
        break;
    case JPG_ENABLE:
        pr_debug("jpg ioctl JPG_ENABLE\n");

        clk_enable(jpg_hw_dev.jpg_clk);
#ifdef CONFIG_OF
        sci_glb_set(SPRD_MMAHB_BASE_DT+0x08, BIT(6));
#endif
        jpg_fp->is_clock_enabled= 1;
        break;
    case JPG_DISABLE:
        disable_jpg(jpg_fp);
#ifdef CONFIG_OF
        sci_glb_clr(SPRD_MMAHB_BASE_DT+0x08, BIT(6));
#endif
        break;
    case JPG_ACQUAIRE:
        pr_debug("jpg ioctl JPG_ACQUAIRE begin\n");
        ret = down_timeout(&jpg_hw_dev.jpg_mutex,
                           msecs_to_jiffies(JPG_TIMEOUT_MS));
        if (ret) {
            printk(KERN_ERR "jpg error timeout\n");
            up(&jpg_hw_dev.jpg_mutex);
            return ret;
        }
#ifdef RT_VSP_THREAD
        if (!rt_task(current)) {
            struct sched_param schedpar;
            int ret;
            struct cred *new = prepare_creds();
            cap_raise(new->cap_effective, CAP_SYS_NICE);
            commit_creds(new);
            schedpar.sched_priority = 1;
            ret = sched_setscheduler(current, SCHED_RR, &schedpar);
            if (ret!=0)
                printk(KERN_ERR "jpg change pri fail a\n");
        }
#endif
        jpg_fp->is_jpg_aquired = 1;
        jpg_hw_dev.jpg_fp = jpg_fp;
        pr_debug("jpg ioctl JPG_ACQUAIRE end\n");
        break;
    case JPG_RELEASE:
        release_jpg(jpg_fp);
        break;
#ifdef USE_INTERRUPT
    case JPG_START:
        pr_debug("jpg ioctl JPG_START\n");
        ret = wait_event_interruptible_timeout(
                  jpg_hw_dev.wait_queue_work_VLC,
                  jpg_hw_dev.condition_work_VLC,
                  msecs_to_jiffies(JPG_TIMEOUT_MS));
        if (ret == -ERESTARTSYS) {
            printk(KERN_ERR "jpg error start -ERESTARTSYS\n");
            jpg_hw_dev.jpg_int_status |= 1<<30;
            ret = -EINVAL;
        } else if (ret == 0) {
            printk(KERN_ERR "jpg error start  timeout\n");
            jpg_hw_dev.jpg_int_status |= 1<<31;
            ret = -ETIMEDOUT;
        } else {
            ret = 0;
        }
        if (ret) {
            /*clear jpg int*/
            __raw_writel((1<<3)|(1<<2)|(1<<1)|(1<<0),
                         SPRD_JPG_BASE_DT+GLB_INT_CLR_OFFSET);
        }
        put_user(jpg_hw_dev.jpg_int_status, (int __user *)arg);
        jpg_hw_dev.condition_work_MBIO= 0;
        jpg_hw_dev.condition_work_VLC= 0;
        jpg_hw_dev.condition_work_BSM= 0;
        jpg_hw_dev.jpg_int_status = 0;
        pr_debug("jpg ioctl JPG_START end\n");
        return ret;
        break;
#endif
    case JPG_RESET:
        pr_debug("jpg ioctl JPG_RESET\n");
        sci_glb_set(SPRD_MMAHB_BASE_DT+0x04, BIT(6));
        sci_glb_clr(SPRD_MMAHB_BASE_DT+0x04, BIT(6));

        break;

    case JPG_ACQUAIRE_MBIO_VLC_DONE:

        cmd0 = (int)arg;
        if(3 == cmd0) //MBIO
        {
            //printk("jpg ioctl JPG_ACQUAIRE_MBIO_DONE E\n");
            ret = wait_event_interruptible_timeout(
                      jpg_hw_dev.wait_queue_work_MBIO,
                      jpg_hw_dev.condition_work_MBIO,
                      msecs_to_jiffies(JPG_TIMEOUT_MS));

            if (ret == -ERESTARTSYS) {
                printk(KERN_ERR "jpg error start -ERESTARTSYS\n");
                ret = -EINVAL;
            } else if (ret == 0) {
                printk(KERN_ERR "jpg error start  timeout\n");
                ret = __raw_readl(SPRD_JPG_BASE_DT+GLB_INT_STS_OFFSET);
                printk("jpg_int_status %x",ret);
                ret = -ETIMEDOUT;
            } else {
                ret = 0;
            }

            if (ret) { //timeout , clean all init bits.
                /*clear jpg int*/
                __raw_writel((1<<3)|(1<<2)|(1<<1)|(1<<0),
                             SPRD_JPG_BASE_DT+GLB_INT_CLR_OFFSET);
                ret  = 1;
            }
            else //catched an init
            {
                ret = 0;
                //printk("jpg_int_status %x",__raw_readl(SPRD_JPG_BASE+GLB_INT_STS_OFFSET));
            }

            //printk(KERN_ERR "JPG_ACQUAIRE_MBIO_DONE X %x\n",ret);
            jpg_hw_dev.jpg_int_status &= (~0x8);
            jpg_hw_dev.condition_work_MBIO= 0;
        }
        else if(1 == cmd0)
        {
            //printk("jpg ioctl JPG_ACQUAIRE_VLC_DONE E\n");
            ret = wait_event_interruptible_timeout(
                      jpg_hw_dev.wait_queue_work_VLC,
                      jpg_hw_dev.condition_work_VLC,
                      msecs_to_jiffies(JPG_TIMEOUT_MS));

            if (ret == -ERESTARTSYS) {
                printk(KERN_ERR "jpg error start -ERESTARTSYS\n");
                ret = -EINVAL;
            } else if (ret == 0) {
                printk(KERN_ERR "jpg error start  timeout\n");
                ret = __raw_readl(SPRD_JPG_BASE_DT+GLB_INT_STS_OFFSET);
                printk("jpg_int_status %x",ret);
                ret = -ETIMEDOUT;
            } else {
                ret = 0;
            }

            if (ret) { //timeout , clean all init bits.
                /*clear jpg int*/
                __raw_writel((1<<3)|(1<<2)|(1<<1)|(1<<0),
                             SPRD_JPG_BASE_DT+GLB_INT_CLR_OFFSET);
                ret  = 1;
            }
            else //catched an init
            {
                ret = 4;
                //	printk("jpg_int_status %x",__raw_readl(SPRD_JPG_BASE+GLB_INT_STS_OFFSET));
            }

            //printk(KERN_ERR "JPG_ACQUAIRE_VLC_DONE %x\n",ret);
            jpg_hw_dev.jpg_int_status &=(~0x2);
            jpg_hw_dev.condition_work_VLC= 0;
        }
        else {
            printk(KERN_ERR "JPG_ACQUAIRE_MBIO_DONE error arg");
            ret = -1;
        }
        return ret;

        break;

    default:
        return -EINVAL;
    }
    return 0;
}

#ifdef USE_INTERRUPT
static irqreturn_t jpg_isr(int irq, void *data)
{
    int int_status;

    int_status   =__raw_readl(SPRD_JPG_BASE_DT+GLB_INT_STS_OFFSET);
    //printk(KERN_INFO "jpg_isr JPG_INT_STS %x\n",int_status);
    if((int_status) & 0xb) // JPEG ENC
    {
        int ret = 7; // 7 : invalid
        if((int_status >> 3) & 0x1) //JPEG ENC  MBIO DONE
        {
            __raw_writel((1<<3), SPRD_JPG_BASE_DT+GLB_INT_CLR_OFFSET);
            ret = 0;
            jpg_hw_dev.jpg_int_status |=0x8;

            jpg_hw_dev.condition_work_MBIO= 1;
            wake_up_interruptible(&jpg_hw_dev.wait_queue_work_MBIO);
            //		printk(KERN_ERR "jpg_isr MBIO");
        }
        if((int_status >> 0) & 0x1)  // JPEG ENC BSM INIT
        {
            __raw_writel((1<<0), SPRD_JPG_BASE_DT+GLB_INT_CLR_OFFSET);
            jpg_hw_dev.jpg_int_status |=0x1;

            jpg_hw_dev.condition_work_BSM= 1;
            wake_up_interruptible(&jpg_hw_dev.wait_queue_work_BSM);
            printk(KERN_ERR "jpg_isr BSM");
        }
        if((int_status >> 1) & 0x1)  // JPEG ENC VLC DONE INIT
        {
            __raw_writel((1<<1), SPRD_JPG_BASE_DT+GLB_INT_CLR_OFFSET);
            jpg_hw_dev.jpg_int_status |=0x2;

            jpg_hw_dev.condition_work_VLC= 1;
            wake_up_interruptible(&jpg_hw_dev.wait_queue_work_VLC);
            //		printk(KERN_ERR "jpg_isr VLC");
        }

        // jpg_hw_dev.jpg_int_status = ret;
    }
//	jpg_hw_dev.condition_work = 1;
//	wake_up_interruptible(&jpg_hw_dev.wait_queue_work);

    return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id  of_match_table_jpg[] = {
    { .compatible = "sprd,sprd_jpg", },
    { },
};

static void jpg_parse_dt(struct device *dev)
{
    struct device_node *np = dev->of_node;
    struct resource res;
    int ret;

    ret = of_address_to_resource(np, 0, &res);
    if(ret < 0) {
        dev_err(dev, "no reg of property specified\n");
        return;
    }
    SPRD_JPG_BASE_DT = SPRD_JPG_BASE;//res.start;

    jpg_hw_dev.irq = irq_of_parse_and_map(np, 0);
    jpg_hw_dev.dev_np = np;

    return;
}
#else
static void jpg_parse_dt(
    struct device *dev)
{
    jpg_hw_dev.irq = IRQ_JPG_INT;
    return;
}
#endif

static int jpg_nocache_mmap(struct file *filp, struct vm_area_struct *vma)
{
    printk(KERN_INFO "@jpg[%s]\n", __FUNCTION__);
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    vma->vm_pgoff     = (SPRD_JPG_PHYS>>PAGE_SHIFT);
    if (remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff,
                        vma->vm_end - vma->vm_start, vma->vm_page_prot))
        return -EAGAIN;
    printk(KERN_INFO "@jpg mmap %x,%x,%x\n", (unsigned int)PAGE_SHIFT,
           (unsigned int)vma->vm_start,
           (unsigned int)(vma->vm_end - vma->vm_start));
    return 0;
}

static int jpg_open(struct inode *inode, struct file *filp)
{
    struct clk *clk_mm_i;
    struct clk *clk_jpg;
    struct clk *clk_parent;
    char *name_parent;
    int ret_val,ret;
    struct jpg_fh *jpg_fp = kmalloc(sizeof(struct jpg_fh), GFP_KERNEL);
    if (jpg_fp == NULL) {
        printk(KERN_ERR "jpg open error occured\n");
        return  -EINVAL;
    }
    filp->private_data = jpg_fp;
    jpg_fp->is_clock_enabled = 0;
    jpg_fp->is_jpg_aquired = 0;

//	init_waitqueue_head(&jpg_hw_dev.wait_queue_work_MBIO);
    jpg_hw_dev.condition_work_MBIO= 0;
//	init_waitqueue_head(&jpg_hw_dev.wait_queue_work_VLC);
    jpg_hw_dev.condition_work_VLC= 0;
//	init_waitqueue_head(&jpg_hw_dev.wait_queue_work_BSM);
    jpg_hw_dev.condition_work_BSM= 0;
    jpg_hw_dev.jpg_int_status = 0;

#if defined(CONFIG_ARCH_SCX35)
#ifdef CONFIG_OF
    clk_mm_i = of_clk_get_by_name(jpg_hw_dev.dev_np, "clk_mm_i");
#else
    clk_mm_i = clk_get(NULL, "clk_mm_i");
#endif
    if (IS_ERR(clk_mm_i) || (!clk_mm_i)) {
        printk(KERN_ERR "###: Failed : Can't get clock [%s}!\n",
               "clk_mm_i");
        printk(KERN_ERR "###: clk_mm_i =  %p\n", clk_mm_i);
        ret = -EINVAL;
        goto errout;
    } else {
        jpg_hw_dev.mm_clk= clk_mm_i;
    }
#endif


    printk("JPEG mmi_clk open");
    clk_enable(jpg_hw_dev.mm_clk);


#ifdef CONFIG_OF
    clk_jpg= of_clk_get_by_name(jpg_hw_dev.dev_np, "clk_jpg");
#else
    clk_jpg = clk_get(NULL, "clk_jpg");
#endif
    if (IS_ERR(clk_jpg) || (!clk_jpg)) {
        printk(KERN_ERR "###: Failed : Can't get clock [%s}!\n",
               "clk_vsp");
        printk(KERN_ERR "###: jpg_clk =  %p\n", clk_jpg);
        ret = -EINVAL;
        goto errout;
    } else {
        jpg_hw_dev.jpg_clk = clk_jpg;
    }

    name_parent = jpg_get_clk_src_name(jpg_hw_dev.freq_div);
    clk_parent = clk_get(NULL, name_parent);
    if ((!clk_parent )|| IS_ERR(clk_parent) ) {
        printk(KERN_ERR "clock[%s]: failed to get parent in probe[%s] \
by clk_get()!\n", "clk_jpg", name_parent);
        ret = -EINVAL;
        goto errout;
    } else {
        jpg_hw_dev.jpg_parent_clk = clk_parent;
    }

    ret_val = clk_set_parent(jpg_hw_dev.jpg_clk, jpg_hw_dev.jpg_parent_clk);
    if (ret_val) {
        printk(KERN_ERR "clock[%s]: clk_set_parent() failed in probe!",
               "clk_jpg");
        ret = -EINVAL;
        goto errout;
    }

    printk("jpg parent clock name %s\n", name_parent);
    printk("jpg_freq %d Hz",
           (int)clk_get_rate(jpg_hw_dev.jpg_clk));

#if defined(CONFIG_SPRD_IOMMU)
    {
        sprd_iommu_module_enable(IOMMU_MM);
    }
#endif

    printk(KERN_INFO "jpg_open %p\n", jpg_fp);
    return 0;

errout:
#if defined(CONFIG_ARCH_SCX35)
    if (jpg_hw_dev.mm_clk) {
        clk_put(jpg_hw_dev.mm_clk);
    }
#endif
    if (jpg_hw_dev.jpg_clk) {
        clk_put(jpg_hw_dev.jpg_clk);
    }

    if (jpg_hw_dev.jpg_parent_clk) {
        clk_put(jpg_hw_dev.jpg_parent_clk);
    }
    return ret;
}

static int jpg_release (struct inode *inode, struct file *filp)
{
    struct jpg_fh *jpg_fp = filp->private_data;

#if defined(CONFIG_SPRD_IOMMU)
    {
        sprd_iommu_module_disable(IOMMU_MM);
    }
#endif

    if (jpg_fp->is_clock_enabled) {
        printk(KERN_ERR "error occured and close clock \n");
        clk_disable(jpg_hw_dev.jpg_clk);
    }

    if (jpg_fp->is_jpg_aquired) {
        printk(KERN_ERR "error occured and up jpg_mutex \n");
        up(&jpg_hw_dev.jpg_mutex);
    }

    kfree(filp->private_data);

    clk_disable(jpg_hw_dev.mm_clk);

    printk("JPEG mmi_clk close !!");
    printk(KERN_INFO "jpg_release %p\n", jpg_fp);

    return 0;
}

static const struct file_operations jpg_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = jpg_ioctl,
    .mmap  = jpg_nocache_mmap,
    .open = jpg_open,
    .release = jpg_release,
};

static struct miscdevice jpg_dev = {
    .minor   = JPG_MINOR,
    .name   = "sprd_jpg",
    .fops   = &jpg_fops,
};

static int jpg_probe(struct platform_device *pdev)
{

    int ret_val;
    int ret;
    int cmd0;

#ifdef CONFIG_OF
    if (pdev->dev.of_node) {
        jpg_parse_dt(&pdev->dev);
    }
#else
    jpg_parse_dt(&pdev->dev);
#endif

    sema_init(&jpg_hw_dev.jpg_mutex, 1);

    init_waitqueue_head(&jpg_hw_dev.wait_queue_work_MBIO);
    jpg_hw_dev.condition_work_MBIO= 0;
    init_waitqueue_head(&jpg_hw_dev.wait_queue_work_VLC);
    jpg_hw_dev.condition_work_VLC= 0;
    init_waitqueue_head(&jpg_hw_dev.wait_queue_work_BSM);
    jpg_hw_dev.condition_work_BSM= 0;
    jpg_hw_dev.jpg_int_status = 0;


    jpg_hw_dev.freq_div = DEFAULT_FREQ_DIV;

    jpg_hw_dev.jpg_clk = NULL;
    jpg_hw_dev.jpg_parent_clk = NULL;
    jpg_hw_dev.mm_clk= NULL;


    ret = misc_register(&jpg_dev);
    if (ret) {
        printk(KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
               JPG_MINOR, ret);
        goto errout;
    }

#ifdef USE_INTERRUPT
    /* register isr */
    ret = request_irq(jpg_hw_dev.irq, jpg_isr, 0, "JPG", &jpg_hw_dev);
    if (ret) {
        printk(KERN_ERR "jpg: failed to request irq!\n");
        ret = -EINVAL;
        goto errout2;
    }
#endif


    return 0;

#ifdef USE_INTERRUPT
errout2:
    misc_deregister(&jpg_dev);
#endif

errout:

    return ret;
}

static int jpg_remove(struct platform_device *pdev)
{
    printk(KERN_INFO "jpg_remove called !\n");

    misc_deregister(&jpg_dev);

#ifdef USE_INTERRUPT
    free_irq(jpg_hw_dev.irq, &jpg_hw_dev);
#endif


    if (jpg_hw_dev.jpg_clk) {
        clk_put(jpg_hw_dev.jpg_clk);
    }

    if (jpg_hw_dev.jpg_parent_clk) {
        clk_put(jpg_hw_dev.jpg_parent_clk);
    }

    printk(KERN_INFO "jpg_remove Success !\n");
    return 0;
}

static struct platform_driver jpg_driver = {
    .probe    = jpg_probe,
    .remove   = jpg_remove,
    .driver   = {
        .owner = THIS_MODULE,
        .name = "sprd_jpg",
#ifdef CONFIG_OF
        .of_match_table = of_match_ptr(of_match_table_jpg) ,
#endif
    },
};

static int __init jpg_init(void)
{
    printk(KERN_INFO "jpg_init called !\n");
    if (platform_driver_register(&jpg_driver) != 0) {
        printk(KERN_ERR "platform device jpg drv register Failed \n");
        return -1;
    }
    return 0;
}

static void __exit jpg_exit(void)
{
    printk(KERN_INFO "jpg_exit called !\n");
    platform_driver_unregister(&jpg_driver);
}

module_init(jpg_init);
module_exit(jpg_exit);

MODULE_DESCRIPTION("SPRD JPG Driver");
MODULE_LICENSE("GPL");
