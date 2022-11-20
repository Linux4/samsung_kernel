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
#include <linux/wakelock.h>

#include <video/sprd_vsp.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/globalregs.h>


#define VSP_MINOR MISC_DYNAMIC_MINOR
#define VSP_TIMEOUT_MS 1000

#define USE_INTERRUPT
/*#define RT_VSP_THREAD*/

#define DEFAULT_FREQ_DIV 0x0

#define SPRD_VSP_BASE SPRD_MEA_BASE
#define SPRD_VSP_PHYS SPRD_MEA_PHYS

#define DCAM_CFG_OFF			0x0
#define DCAM_SRC_SIZE_OFF		0xC
#define DCAM_ISP_DIS_SIZE_OFF		0x10
#define DCAM_VSP_TIME_OUT_OFF		0x14
#define DCAM_INT_STS_OFF		0x20
#define DCAM_INT_MASK_OFF		0x24
#define DCAM_INT_CLR_OFF		0x28
#define DCAM_INT_RAW_OFF		0x2C


#if 0//defined(CONFIG_ARCH_SC8825)
#ifdef USE_INTERRUPT
#undef USE_INTERRUPT
#endif

#define DCAM_CLOCK_EN		SPRD_AHB_BASE+0x0200//0x20900000
#define AHB_CTRL2                       SPRD_AHB_BASE+0x0208
//#define DCAM_CLK_FREQUENCE	0x8b00000c
#define PLL_SRC                            SPRD_GREG_BASE+0x0070  //0x4B000070

#endif


struct vsp_fh{
	int is_vsp_aquired;
	int is_clock_enabled;
};

struct vsp_dev{
	unsigned int freq_div;

	struct semaphore vsp_mutex;

	wait_queue_head_t wait_queue_work;
	int condition_work;
	int vsp_int_status;

	struct clk *vsp_clk;
	struct clk *vsp_parent_clk;

	struct vsp_fh *vsp_fp;
};

static struct vsp_dev vsp_hw_dev;
static struct wake_lock vsp_wakelock;

struct clock_name_map_t{
	unsigned long freq;
	char *name;
};

#if defined(CONFIG_ARCH_SC8825)
static struct clock_name_map_t clock_name_map[] = {
						{192000000,"clk_192m"},
						{153600000,"clk_153p6m"},
						{64000000,"clk_64m"},
						{48000000,"clk_48m"}
						};

#else
static struct clock_name_map_t clock_name_map[] = {
						{153600000,"l3_153m600k"},
						{128000000,"clk_128m"},
						{64000000,"clk_64m"},
						{48000000,"clk_48m"}
						};
#endif
static int max_freq_level = ARRAY_SIZE(clock_name_map);

static char *vsp_get_clk_src_name(unsigned int freq_level)
{
	if (freq_level >= max_freq_level - 1) {
		printk(KERN_INFO "set freq_level to 0");
		freq_level = 0;
	}

	return clock_name_map[freq_level].name;
}

static int find_vsp_freq_level(unsigned long freq)
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

static void disable_vsp (struct vsp_fh *vsp_fp)
{
	clk_disable(vsp_hw_dev.vsp_clk);
	vsp_fp->is_clock_enabled= 0;
        wake_unlock(&vsp_wakelock);
	pr_debug("vsp ioctl VSP_DISABLE\n");

	return;
}

static void release_vsp(struct vsp_fh *vsp_fp)
{
	pr_debug("vsp ioctl VSP_RELEASE\n");
	vsp_fp->is_vsp_aquired = 0;
	up(&vsp_hw_dev.vsp_mutex);

	return;
}
#if defined(CONFIG_ARCH_SC8825)
#ifdef USE_INTERRUPT
static irqreturn_t vsp_isr(int irq, void *data);
#endif
#endif
static long vsp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret, cmd0;
	struct clk *clk_parent;
	char *name_parent;
	unsigned long frequency;
	struct vsp_fh *vsp_fp = filp->private_data;

	switch (cmd) {
	case VSP_CONFIG_FREQ:
		get_user(vsp_hw_dev.freq_div, (int __user *)arg);
		name_parent = vsp_get_clk_src_name(vsp_hw_dev.freq_div);
		clk_parent = clk_get(NULL, name_parent);
		if ((!clk_parent )|| IS_ERR(clk_parent)) {
			printk(KERN_ERR "clock[%s]: failed to get parent [%s] \
by clk_get()!\n", "clk_vsp", name_parent);
			return -EINVAL;
		}
		ret = clk_set_parent(vsp_hw_dev.vsp_clk, clk_parent);
		if (ret) {
			printk(KERN_ERR "clock[%s]: clk_set_parent() failed!",
				"clk_vsp");
			return -EINVAL;
		} else {
			clk_put(vsp_hw_dev.vsp_parent_clk);
			vsp_hw_dev.vsp_parent_clk = clk_parent;
		}
		printk(KERN_INFO "VSP_CONFIG_FREQ %d\n", vsp_hw_dev.freq_div);
		break;
	case VSP_GET_FREQ:
		frequency = clk_get_rate(vsp_hw_dev.vsp_clk);
		ret = find_vsp_freq_level(frequency);
		put_user(ret, (int __user *)arg);
		printk(KERN_INFO "vsp ioctl VSP_GET_FREQ %d\n", ret);
		break;
	case VSP_ENABLE:
		pr_debug("vsp ioctl VSP_ENABLE\n");
                wake_lock(&vsp_wakelock);
		ret = clk_enable(vsp_hw_dev.vsp_clk);
		vsp_fp->is_clock_enabled= 1;        
		break;
	case VSP_DISABLE:
		disable_vsp(vsp_fp);
		break;
	case VSP_ACQUAIRE:
		pr_debug("vsp ioctl VSP_ACQUAIRE begin\n");
		ret = down_timeout(&vsp_hw_dev.vsp_mutex,
				msecs_to_jiffies(VSP_TIMEOUT_MS));
		if (ret) {
			printk(KERN_ERR "vsp error timeout\n");
			up(&vsp_hw_dev.vsp_mutex);
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
				printk(KERN_ERR "vsp change pri fail a\n");
		}
#endif
		vsp_fp->is_vsp_aquired = 1;
		vsp_hw_dev.vsp_fp = vsp_fp;
		pr_debug("vsp ioctl VSP_ACQUAIRE end\n");
		break;
	case VSP_RELEASE:
		release_vsp(vsp_fp);
		break;
#ifdef USE_INTERRUPT
	case VSP_START:
		pr_debug("vsp ioctl VSP_START\n");
		ret = wait_event_interruptible_timeout(
			vsp_hw_dev.wait_queue_work,
			vsp_hw_dev.condition_work,
			msecs_to_jiffies(VSP_TIMEOUT_MS));
		if (ret == -ERESTARTSYS) {
			printk("KERN_ERR vsp error start -ERESTARTSYS\n");
			vsp_hw_dev.vsp_int_status |= 1<<30;
			ret = -EINVAL;
		} else if (ret == 0) {
			printk("KERN_ERR vsp error start  timeout\n");
			vsp_hw_dev.vsp_int_status |= 1<<31;
			ret = -ETIMEDOUT;
		} else {
			ret = 0;
		}
		if (ret) {
			/*clear vsp int*/
			__raw_writel((1<<10)|(1<<12)|(1<<15)|(1<<16),
				SPRD_VSP_BASE+DCAM_INT_CLR_OFF);
		}
		put_user(vsp_hw_dev.vsp_int_status, (int __user *)arg);
		vsp_hw_dev.vsp_int_status = 0;
		vsp_hw_dev.condition_work = 0;
		pr_debug("vsp ioctl VSP_START end\n");
		return ret;
		break;
#endif
	case VSP_RESET:
		pr_debug("vsp ioctl VSP_RESET\n");
		sprd_greg_set_bits(REG_TYPE_AHB_GLOBAL, BIT(15), AHB_SOFT_RST);
		sprd_greg_clear_bits(REG_TYPE_AHB_GLOBAL,BIT(15), AHB_SOFT_RST);
		break;
#if defined(CONFIG_ARCH_SC8825)		

	case VSP_ACQUAIRE_MEA_DONE:

		pr_debug("vsp ioctl VSP_ACQUAIRE_MEA_DONE\n");
		ret = wait_event_interruptible_timeout(
			vsp_hw_dev.wait_queue_work,
			vsp_hw_dev.condition_work,
			msecs_to_jiffies(VSP_TIMEOUT_MS));
		if (ret == -ERESTARTSYS) {
			printk("KERN_ERR vsp error start -ERESTARTSYS\n");
			ret = -EINVAL;
		} else if (ret == 0) {
			printk("KERN_ERR vsp error start  timeout\n");
			ret = -ETIMEDOUT;
		} else {
			ret = 0;
		}
		
		if (ret) { //timeout , clean all init bits.
			/*clear vsp int*/
			__raw_writel((1<<7)|(1<<8)|(1<<14),
				SPRD_VSP_BASE+DCAM_INT_CLR_OFF);			
			ret  = 1;
		} 
		else //catched an init
		{
			ret = vsp_hw_dev.vsp_int_status;
		}
				
		printk(KERN_ERR "VSP_ACQUAIRE_MEA_DONE %x\n",ret);
		vsp_hw_dev.vsp_int_status = 0;
		vsp_hw_dev.condition_work = 0;
		pr_debug("vsp ioctl VSP_ACQUAIRE_MEA_DONE end\n");
		return ret;

               break;
			   
	case VSP_ACQUAIRE_MP4ENC_DONE:
		cmd0 = 0;
		printk(KERN_ERR "VSP_ACQUAIRE_MP4ENC_DONE in !\n");
		ret= __raw_readl(SPRD_VSP_BASE+DCAM_INT_RAW_OFF);
		
		while(!((ret & 0x80)|| (ret & 0x10000)  ) && (cmd0 < (int)arg))
		{ 
			ret = __raw_readl(SPRD_VSP_BASE+DCAM_INT_RAW_OFF);
			//printk(KERN_INFO "DCAM_INT_RAW_OFF %x !\n",ret);
			cmd0++;
			msleep(1);
		}

		if(ret & 0x80)
		{
			printk(KERN_INFO "vsp stream buffer is full!\n");
			return 2;//
		}else if(ret & 0x10000)
		{
			printk(KERN_INFO "VSP MP4ENC_DONE DONE!\n");
			return 0;
		}else  if(cmd0 >=  (int)arg)
        	{
              		 printk(KERN_INFO "VSP_ACQUAIRE_MP4ENC_DONE time out\n");
                   	return 1;
		}else{
			printk(KERN_INFO "vsp: ERR\n");
			return -EINVAL;
		}
		break;
#endif
	default:
		return -EINVAL;
	}
	return 0;
}

#ifdef USE_INTERRUPT
static irqreturn_t vsp_isr(int irq, void *data)
{
	int int_status;
	
	int_status = vsp_hw_dev.vsp_int_status = __raw_readl(SPRD_VSP_BASE+DCAM_INT_STS_OFF);
	printk(KERN_INFO "VSP_INT_STS %x\n",int_status);
	if((int_status >> 15) & 0x1) // CMD DONE
	{
		__raw_writel((1<<10)|(1<<12)|(1<<15), SPRD_VSP_BASE+DCAM_INT_CLR_OFF);

		disable_vsp(vsp_hw_dev.vsp_fp);
		release_vsp(vsp_hw_dev.vsp_fp);
	}
	else if((int_status >> 16) & 0x1) // MPEG4 ENC DONE
	{
		__raw_writel((1<<16), SPRD_VSP_BASE+DCAM_INT_CLR_OFF);
	}
	else if((int_status) & 0x4180) // JPEG ENC 
	{
		int ret = 7; // 7 : invalid
		 if((int_status >> 14) & 0x1) //JPEG ENC  MEA DONE
		{
			__raw_writel((1<<14), SPRD_VSP_BASE+DCAM_INT_CLR_OFF);
			ret = 0;
		}
		if((int_status >> 7) & 0x1)  // JPEG ENC BSM INIT
		{
			__raw_writel((1<<7),
				SPRD_VSP_BASE+DCAM_INT_CLR_OFF);
			ret = 2;
		}
		 if((int_status >> 8) & 0x1)  // JPEG ENC VLC DONE INIT
		{
			__raw_writel((1<<8), SPRD_VSP_BASE+DCAM_INT_CLR_OFF);
			ret = 4;			
		}

		 vsp_hw_dev.vsp_int_status = ret;
	}
	vsp_hw_dev.condition_work = 1;
	wake_up_interruptible(&vsp_hw_dev.wait_queue_work);



	return IRQ_HANDLED;
}
#endif

static int vsp_nocache_mmap(struct file *filp, struct vm_area_struct *vma)
{
	printk(KERN_INFO "@vsp[%s]\n", __FUNCTION__);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_pgoff     = (SPRD_VSP_PHYS>>PAGE_SHIFT);
	if (remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff,
		vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;
	printk(KERN_INFO "@vsp mmap %x,%x,%x\n", (unsigned int)PAGE_SHIFT,
		(unsigned int)vma->vm_start,
		(unsigned int)(vma->vm_end - vma->vm_start));
	return 0;
}

static int vsp_open(struct inode *inode, struct file *filp)
{
	struct vsp_fh *vsp_fp = kmalloc(sizeof(struct vsp_fh), GFP_KERNEL);
	if (vsp_fp == NULL) {
		printk(KERN_ERR "vsp open error occured\n");
		return  -EINVAL;
	}
        
	filp->private_data = vsp_fp;
	vsp_fp->is_clock_enabled = 0;
	vsp_fp->is_vsp_aquired = 0;

	printk(KERN_INFO "vsp_open %p\n", vsp_fp);
	return 0;
}

static int vsp_release (struct inode *inode, struct file *filp)
{
	struct vsp_fh *vsp_fp = filp->private_data;

	if (vsp_fp->is_clock_enabled) {
		printk(KERN_ERR "error occured and close clock \n");
		clk_disable(vsp_hw_dev.vsp_clk);
	}

	if (vsp_fp->is_vsp_aquired) {
		printk(KERN_ERR "error occured and up vsp_mutex \n");
		up(&vsp_hw_dev.vsp_mutex);
	}

	kfree(filp->private_data);

	return 0;
}

static const struct file_operations vsp_fops =
{
	.owner = THIS_MODULE,
	.unlocked_ioctl = vsp_ioctl,
	.mmap  = vsp_nocache_mmap,
	.open = vsp_open,
	.release = vsp_release,
};

static struct miscdevice vsp_dev = {
	.minor   = VSP_MINOR,
	.name   = "sprd_vsp",
	.fops   = &vsp_fops,
};

static int vsp_probe(struct platform_device *pdev)
{
	struct clk *clk_vsp;
	struct clk *clk_parent;
	char *name_parent;
	int ret_val;
	int ret;
	int cmd0;

    wake_lock_init(&vsp_wakelock, WAKE_LOCK_SUSPEND,
		"pm_message_wakelock_vsp");
     
	sema_init(&vsp_hw_dev.vsp_mutex, 1);

	init_waitqueue_head(&vsp_hw_dev.wait_queue_work);
	vsp_hw_dev.vsp_int_status = 0;
	vsp_hw_dev.condition_work = 0;

	vsp_hw_dev.freq_div = DEFAULT_FREQ_DIV;

	vsp_hw_dev.vsp_clk = NULL;
	vsp_hw_dev.vsp_parent_clk = NULL;

#if defined(CONFIG_ARCH_SC8825)
		//cmd0 = __raw_readl(AHB_CTRL2);//,"AHB_CTRL2:Read the AHB_CTRL2 CLOCK");
		//cmd0 |= 0x440;
		//__raw_writel(cmd0,AHB_CTRL2);//,"AHB_CTRL2:enable MMMTX_CLK_EN");

		//cmd0 = __raw_readl(PLL_SRC);//"PLL_SRC:Read the PLL_SRC CLOCK");
		//cmd0 &=~(0xc);//192M
		//__raw_writel(cmd0,PLL_SRC);//"PLL_SRC:set vsp clock");

		//cmd0 = __raw_readl(DCAM_CLOCK_EN);//,"DCAM_CLOCK_EN:Read the DCAM_CLOCK_EN ");
		//cmd0 = 0xFFFFFFFF;
		//__raw_writel(cmd0,DCAM_CLOCK_EN);//"DCAM_CLOCK_EN:enable DCAM_CLOCK_EN");

#endif

	clk_vsp = clk_get(NULL, "clk_vsp");
	if (IS_ERR(clk_vsp) || (!clk_vsp)) {
		printk(KERN_ERR "###: Failed : Can't get clock [%s}!\n",
			"clk_vsp");
		printk(KERN_ERR "###: vsp_clk =  %p\n", clk_vsp);
		ret = -EINVAL;
		goto errout;
	} else {
		vsp_hw_dev.vsp_clk = clk_vsp;
	}

	name_parent = vsp_get_clk_src_name(vsp_hw_dev.freq_div);
	clk_parent = clk_get(NULL, name_parent);
	if ((!clk_parent )|| IS_ERR(clk_parent) ) {
		printk(KERN_ERR "clock[%s]: failed to get parent in probe[%s] \
by clk_get()!\n", "clk_vsp", name_parent);
		ret = -EINVAL;
		goto errout;
	} else {
		vsp_hw_dev.vsp_parent_clk = clk_parent;
	}

	ret_val = clk_set_parent(vsp_hw_dev.vsp_clk, vsp_hw_dev.vsp_parent_clk);
	if (ret_val) {
		printk(KERN_ERR "clock[%s]: clk_set_parent() failed in probe!",
			"clk_vsp");
		ret = -EINVAL;
		goto errout;
	}

	printk("vsp parent clock name %s\n", name_parent);
	printk("vsp_freq %d Hz",
		(int)clk_get_rate(vsp_hw_dev.vsp_clk));

	ret = misc_register(&vsp_dev);
	if (ret) {
		printk(KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
			VSP_MINOR, ret);
		goto errout;
	}

#ifdef USE_INTERRUPT
	/* register isr */
	ret = request_irq(IRQ_VSP_INT, vsp_isr, 0, "VSP", &vsp_hw_dev);
	if (ret) {
		printk(KERN_ERR "vsp: failed to request irq!\n");
		ret = -EINVAL;
		goto errout2;
	}
#endif


	return 0;

#ifdef USE_INTERRUPT
errout2:
	misc_deregister(&vsp_dev);
#endif

errout:
	if (vsp_hw_dev.vsp_clk) {
		clk_put(vsp_hw_dev.vsp_clk);
	}

	if (vsp_hw_dev.vsp_parent_clk) {
		clk_put(vsp_hw_dev.vsp_parent_clk);
	}
	return ret;
}

static int vsp_remove(struct platform_device *pdev)
{
	printk(KERN_INFO "vsp_remove called !\n");

	misc_deregister(&vsp_dev);

#ifdef USE_INTERRUPT
	free_irq(IRQ_VSP_INT, &vsp_hw_dev);
#endif


	if (vsp_hw_dev.vsp_clk) {
		clk_put(vsp_hw_dev.vsp_clk);
	}

	if (vsp_hw_dev.vsp_parent_clk) {
		clk_put(vsp_hw_dev.vsp_parent_clk);
	}

	printk(KERN_INFO "vsp_remove Success !\n");
	return 0;
}

static struct platform_driver vsp_driver = {
	.probe    = vsp_probe,
	.remove   = vsp_remove,
	.driver   = {
		.owner = THIS_MODULE,
		.name = "sprd_vsp",
	},
};

static int __init vsp_init(void)
{
	printk(KERN_INFO "vsp_init called !\n");
	if (platform_driver_register(&vsp_driver) != 0) {
		printk(KERN_ERR "platform device vsp drv register Failed \n");
		return -1;
	}
	return 0;
}

static void __exit vsp_exit(void)
{
	printk(KERN_INFO "vsp_exit called !\n");
	platform_driver_unregister(&vsp_driver);
}

module_init(vsp_init);
module_exit(vsp_exit);

MODULE_DESCRIPTION("SPRD VSP Driver");
MODULE_LICENSE("GPL");
