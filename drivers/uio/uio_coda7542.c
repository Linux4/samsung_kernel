/*
 * drivers/uio/uio_coda7542.c
 *
 * chip&media coda7542 UIO driver
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/uio_coda7542.h>

#include <mach/hardware.h>
#ifdef CONFIG_CPU_PXA988
#include <plat/pm.h>
#endif

#define VDEC_WORKING_BUFFER_SIZE	SZ_1M
#define UIO_CODA7542_VERSION		"build-001"

/* HW capability is 16, only use 10 instance to reduce memory requirements. */
#define MAX_NUM_VPUSLOT	10
#define MAX_NUM_FILEHANDLE 10

#define BIT_INT_CLEAR		(0xC)
#define BIT_BUSY_FLAG		(0x160)

struct uio_coda7542_dev {
	struct uio_info uio_info;
	void *reg_base;
	struct clk *clk;
	struct mutex mutex;	/* used to protect open/release */
	int power_status;       /* 0-off 1-on */
	int clk_status;
	int filehandle_ins_num;
	int firmware_download;
	struct semaphore sema;
	unsigned int coda_features;
};

/*
 * use following data structure to assicate fd and codec instance,
 * one fd corresponding to one codec instance
*/
struct coda7542_instance {
	int occupied;
	int gotsemaphore;
	int vpuslotidx; /* the item idex in fd_codecinst_arr[]*/
	int slotusing;
	unsigned int codectypeid;
};
static struct coda7542_instance fd_codecinst_arr[MAX_NUM_VPUSLOT];
#ifdef CONFIG_CPU_PXA988
/* used for vpu lpm constraints */
static struct pm_qos_request vpu_qos_idle;
#endif

struct vpu_info {
	unsigned int off;
	void *value;
};

static int coda7542_power_on(struct uio_coda7542_dev *cdev)
{
	if (cdev->power_status == 1)
		return 0;

	coda7542_power_switch(1);
	cdev->power_status = 1;

	return 0;
}

static int coda7542_clk_on(struct uio_coda7542_dev *cdev)
{
	if (cdev->clk_status != 0)
		return 0;

#ifdef CONFIG_CPU_PXA988
	pm_qos_update_request(&vpu_qos_idle, PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
#endif
	clk_enable(cdev->clk);
	cdev->clk_status = 1;

	return 0;
}

static int coda7542_power_off(struct uio_coda7542_dev *cdev)
{
	if (cdev->power_status == 0)
		return 0;

	coda7542_power_switch(0);
	cdev->power_status = 0;
	return 0;
}

static int coda7542_clk_off(struct uio_coda7542_dev *cdev)
{
	if (cdev->clk_status == 0)
		return 0;

	clk_disable(cdev->clk);
#ifdef CONFIG_CPU_PXA988
	pm_qos_update_request(&vpu_qos_idle,
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
#endif
	cdev->clk_status = 0;

	return 0;
}

static int active_coda7542(struct uio_coda7542_dev *cdev, int on)
{
	int i, ret;

	if (on) {
		ret = coda7542_power_on(cdev);
		if (ret)
			return -1;

		ret = coda7542_clk_on(cdev);
		if (ret)
			return -1;

		sema_init(&cdev->sema, 1);
	} else {
		ret = coda7542_clk_off(cdev);
		if (ret)
			return -1;

		ret = coda7542_power_off(cdev);
		if (ret)
			return -1;
	}

	cdev->firmware_download = 0;

	/*clear fd_codecinst_arr */
	for (i = 0; i < MAX_NUM_VPUSLOT; i++) {
		fd_codecinst_arr[i].occupied = 0;
		fd_codecinst_arr[i].slotusing = 0;
		fd_codecinst_arr[i].codectypeid = 0;
	}

	return 0;
}


static int coda7542_abnormal_stop(struct uio_coda7542_dev *cdev)
{
	int i;
	printk(KERN_INFO "process was killed abnormal\n");
	/*
	 *currently, we couldn't do other thing except waiting
	 *for CNM VPU stop by itself
	 */
	for (i = 0; i < 6; i++) {
		schedule_timeout(msecs_to_jiffies(5));
		if (0 == __raw_readl(cdev->reg_base + BIT_BUSY_FLAG))
			return 0;
	}
	return -1;
}

static int coda7542_open(struct uio_info *info, struct inode *inode,
			void *file_priv)
{
	struct uio_coda7542_dev *cdev;
	struct uio_listener *listener = file_priv;
	int ret = 0, i;

	pr_debug("Enter coda7542_open()\n");
	cdev = (struct uio_coda7542_dev *)info->priv;
	mutex_lock(&cdev->mutex);
	if (cdev->filehandle_ins_num < MAX_NUM_FILEHANDLE)
		cdev->filehandle_ins_num++;
	else {
		printk(KERN_ERR "current coda7542 cannot accept " \
				"more file handle instance!!\n");
		ret = -EACCES;
		goto out;
	}

	if (cdev->filehandle_ins_num == 1) {
		if (0 != active_coda7542(cdev, 1)) {
			cdev->filehandle_ins_num = 0;
			ret = -EACCES;
			goto out;
		}
	}

	for (i = 0; i < MAX_NUM_VPUSLOT; i++) {
		if (fd_codecinst_arr[i].occupied == 0) {
			listener->extend = &fd_codecinst_arr[i];
			fd_codecinst_arr[i].occupied = 1;
			fd_codecinst_arr[i].gotsemaphore = 0;
			break;
		}
	}
	if (i == MAX_NUM_VPUSLOT) {
		cdev->filehandle_ins_num--;
		printk(KERN_ERR "couldn't found any idle seat for new fd!!\n");
		ret = -EACCES;
	}

out:
	mutex_unlock(&cdev->mutex);
	pr_debug("Leave coda7542_open(), ret = %d\n", ret);
	return ret;
}

static int coda7542_release(struct uio_info *info, struct inode *inode,
			void *file_priv)
{
	struct uio_coda7542_dev *cdev;
	struct uio_listener *listener = file_priv;
	struct coda7542_instance *pinst;
	int ret = 0;

	pr_debug("Enter coda7542_release()\n");
	cdev = (struct uio_coda7542_dev *)info->priv;
	pinst = (struct coda7542_instance *)listener->extend;
	mutex_lock(&cdev->mutex);

	/*
	 * if this codec instance got semaphore, it means this codec instance
	 * want VPU to do something, and other codec instance will wait.
	 * If process is killed before VPU finished working for this codec
	 * instance and instance release semaphore, we need do something.
	 */
	if (pinst->gotsemaphore) {
		if (cdev->clk_status == 1) {
			if (__raw_readl(cdev->reg_base + BIT_BUSY_FLAG) != 0)
				coda7542_abnormal_stop(cdev);
		}
		up(&cdev->sema);
		pinst->gotsemaphore = 0;
	}

	if (pinst->slotusing)
		pinst->slotusing = 0;
	pinst->occupied = 0;
	pinst->codectypeid = 0;
	listener->extend = NULL;

	if (cdev->filehandle_ins_num > 0) {
		cdev->filehandle_ins_num--;
		if (cdev->filehandle_ins_num == 0) {
			if (0 != active_coda7542(cdev, 0))
				ret = -EFAULT;
		}
	}
	mutex_unlock(&cdev->mutex);
	pr_debug("Leave coda7542_release(), ret = %d\n", ret);
	return ret;
}

static irqreturn_t coda7542_irq_handler(int irq, struct uio_info *dev_info)
{
	struct uio_coda7542_dev *cdev = dev_info->priv;
	/* clear the interrupt */
	__raw_writel(0x1, cdev->reg_base + BIT_INT_CLEAR);

	return IRQ_HANDLED;
}

static int coda7542_ioctl(struct uio_info *info, unsigned int cmd,
			unsigned long arg, void *file_priv)
{
	unsigned int value;
	int ret, idx;
	struct vpu_info vpu_info;
	void __user *argp = (void __user *)arg;
	struct uio_coda7542_dev *cdev = info->priv;
	struct coda7542_instance *pinst;
	struct uio_listener *listener = file_priv;
	pinst = (struct coda7542_instance *)listener->extend;

	switch (cmd) {
	case CODA7542_POWER_ON:
		mutex_lock(&cdev->mutex);
		ret = coda7542_power_on(cdev);
		if (0 != ret) {
			mutex_unlock(&cdev->mutex);
			return -EFAULT;
		}
		mutex_unlock(&cdev->mutex);
		break;
	case CODA7542_POWER_OFF:
		mutex_lock(&cdev->mutex);
		ret = coda7542_power_off(cdev);
		if (0 != ret) {
			mutex_unlock(&cdev->mutex);
			return -EFAULT;
		}
		mutex_unlock(&cdev->mutex);
		break;
	case CODA7542_CLK_ON:
		mutex_lock(&cdev->mutex);
		ret = coda7542_clk_on(cdev);
		if (0 != ret) {
			mutex_unlock(&cdev->mutex);
			return -EFAULT;
		}
		mutex_unlock(&cdev->mutex);
		break;
	case CODA7542_CLK_OFF:
		mutex_lock(&cdev->mutex);
		ret = coda7542_clk_off(cdev);
		if (0 != ret) {
			mutex_unlock(&cdev->mutex);
			return -EFAULT;
		}
		mutex_unlock(&cdev->mutex);
		break;
	case CODA7542_LOCK:
		if (copy_from_user(&vpu_info, argp, sizeof(struct vpu_info)))
			return -EFAULT;
		if (pinst->gotsemaphore == 0) {
			value = down_timeout(&cdev->sema,
					msecs_to_jiffies(vpu_info.off));
			if (value == 0)
				pinst->gotsemaphore = 1;
		} else
			value = 0;
		if (copy_to_user(vpu_info.value, &value, sizeof(unsigned int)))
			return -EFAULT;
		break;
	case CODA7542_UNLOCK:
		if (pinst->gotsemaphore) {
			up(&cdev->sema);
			pinst->gotsemaphore = 0;
		}
		break;
	case CODA7542_GETSET_INFO:
		if (copy_from_user(&vpu_info, argp, sizeof(struct vpu_info)))
			return -EFAULT;
		switch (vpu_info.off) {
		case 0:
			/* 0 get firmware_download */
			if (copy_to_user(vpu_info.value,
				&cdev->firmware_download, sizeof(int)))
				return -EFAULT;
			break;
		case 1:
			/* 1 set firmware_download */
			cdev->firmware_download = 1;
			break;
		case 2:
			/* 2 get vpu slot */
			if (pinst->slotusing == 0 && pinst->vpuslotidx
					< MAX_NUM_VPUSLOT)
				idx = pinst->vpuslotidx;
			else
				idx = -1;
			if (copy_to_user(vpu_info.value, &idx, sizeof(idx)))
				return -EFAULT;
			pinst->slotusing = 1;
			break;
		case 3:
			/* 3 release vpu slot */
			if (pinst->vpuslotidx < MAX_NUM_VPUSLOT)
				pinst->slotusing = 0;
			break;
		case 4:
			if (copy_to_user(vpu_info.value, &cdev->coda_features,
				sizeof(cdev->coda_features)))
				return -EFAULT;
			break;
		case 6:
			pinst->codectypeid = (unsigned int)(vpu_info.value);
			break;
			
		default:
			return -EFAULT;
		}
		break;
	default:
		return -EFAULT;
	}

	return 0;
}

static bool is_suspend;

static int coda7542_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct uio_coda7542_dev *cdev = platform_get_drvdata(pdev);
	int i;
	bool cannot_off = 0;

	if (cdev->filehandle_ins_num <= 0)
		return 0;
	if (cdev->clk_status != 0)
		return 0;

	for (i = 0; i < MAX_NUM_VPUSLOT; i++) {
		if ((fd_codecinst_arr[i].occupied == 1) &&
			((fd_codecinst_arr[i].codectypeid & VPU_CODEC_TYPEID_ENC_MASK)
			 || fd_codecinst_arr[i].codectypeid == 0)) {
			cannot_off = 1;
			break;
		}
	}
	if (!cannot_off) {
		printk(KERN_INFO "coda7542 poweroff in suspend\n");
		coda7542_power_off(cdev);
		is_suspend = 1;
	}
	return 0;
}

static int coda7542_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct uio_coda7542_dev *cdev = platform_get_drvdata(pdev);

	if (is_suspend) {
		printk(KERN_INFO "coda7542 poweron in resume\n");
		coda7542_power_on(cdev);
		is_suspend = 0;
	}
	return 0;
}


static int coda7542_probe(struct platform_device *pdev)
{
	struct resource *res, *sram_res;
	struct uio_coda7542_dev *cdev;
	int i, irq_func, ret = 0;
	dma_addr_t mem_dma_addr;
	void *mem_vir_addr;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no memory resources given\n");
		return -ENODEV;
	}

	sram_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!sram_res)
		dev_info(&pdev->dev, "no sram resources given\n");

	irq_func = platform_get_irq(pdev, 0);
	if (irq_func < 0) {
		dev_err(&pdev->dev, "missing irq resource in interrupt mode\n");
		return -ENODEV;
	}

	cdev = devm_kzalloc(&pdev->dev, sizeof(*cdev), GFP_KERNEL);
	if (!cdev) {
		dev_err(&pdev->dev, "uio_coda7542_dev: out of memory\n");
		return -ENOMEM;
	}

	cdev->clk = clk_get(&pdev->dev, "VPUCLK");
	if (IS_ERR(cdev->clk)) {
		dev_err(&pdev->dev, "cannot get coda7542 clock\n");
		ret = PTR_ERR(cdev->clk);
		goto err_clk;
	}

	cdev->reg_base = (void *)ioremap(res->start, res->end - res->start + 1);
	if (!cdev->reg_base) {
		dev_err(&pdev->dev, "can't remap register area\n");
		ret = -ENOMEM;
		goto err_reg_base;
	}

	platform_set_drvdata(pdev, cdev);

	cdev->uio_info.name = UIO_CODA7542_NAME;
	cdev->uio_info.version = UIO_CODA7542_VERSION;
	cdev->uio_info.mem[0].internal_addr = (void __iomem *)cdev->reg_base;
	cdev->uio_info.mem[0].addr = res->start;
	cdev->uio_info.mem[0].memtype = UIO_MEM_PHYS;
	cdev->uio_info.mem[0].size = res->end - res->start + 1;

	/*
	 * this 2MB buffer contains firmware buffer, working buffer
	 * and parameter buffer, which shares in multi-process.
	 */
	mem_vir_addr = (void *)__get_free_pages(GFP_DMA | GFP_KERNEL,
					get_order(VDEC_WORKING_BUFFER_SIZE));
	if (!mem_vir_addr) {
		ret = -ENOMEM;
		goto err_uio_mem;
	}
	mem_dma_addr = (dma_addr_t)__virt_to_phys((unsigned int)mem_vir_addr);

	cdev->uio_info.mem[1].internal_addr = (void __iomem *)mem_vir_addr;
	cdev->uio_info.mem[1].addr = mem_dma_addr;
	cdev->uio_info.mem[1].memtype = UIO_MEM_PHYS;
	cdev->uio_info.mem[1].size = VDEC_WORKING_BUFFER_SIZE;
	dev_info(&pdev->dev, "[1] internal addr[0x%08x]," \
			"addr[0x%08x] size[%ld]\n",
			(unsigned int)cdev->uio_info.mem[1].internal_addr,
			(unsigned int)cdev->uio_info.mem[1].addr,
			cdev->uio_info.mem[1].size);

	if (sram_res) {
		cdev->uio_info.mem[2].addr = sram_res->start;
		cdev->uio_info.mem[2].memtype = UIO_MEM_PHYS;
		cdev->uio_info.mem[2].size = sram_res->end -
					     sram_res->start + 1;
		dev_info(&pdev->dev, "[2] addr[0x%08x] size[%ld]\n",
				(unsigned int)cdev->uio_info.mem[2].addr,
				cdev->uio_info.mem[2].size);
	}

	cdev->uio_info.irq_flags = IRQF_DISABLED;
	cdev->uio_info.irq = irq_func;
	cdev->uio_info.handler = coda7542_irq_handler;
	cdev->uio_info.priv = cdev;

	cdev->uio_info.open = coda7542_open;
	cdev->uio_info.release = coda7542_release;
	cdev->uio_info.ioctl = coda7542_ioctl;
	cdev->uio_info.mmap = NULL;
	if (pdev->dev.platform_data)
		cdev->coda_features =
			*((unsigned int *)(pdev->dev.platform_data));

	mutex_init(&(cdev->mutex));
	cdev->filehandle_ins_num = 0;
	for (i = 0; i < MAX_NUM_VPUSLOT; i++)
		fd_codecinst_arr[i].vpuslotidx = i;

	ret = uio_register_device(&pdev->dev, &cdev->uio_info);
	if (ret) {
		dev_err(&pdev->dev, "failed to register uio device\n");
		goto err_uio_register;
	}
#ifdef CONFIG_CPU_PXA988
	vpu_qos_idle.name = pdev->name;
	pm_qos_add_request(&vpu_qos_idle, PM_QOS_CPUIDLE_BLOCK,
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
#endif

	return 0;

err_uio_register:
	free_pages((unsigned long)cdev->uio_info.mem[1].internal_addr,
			get_order(VDEC_WORKING_BUFFER_SIZE));
err_uio_mem:
	iounmap(cdev->uio_info.mem[0].internal_addr);
err_reg_base:
	clk_put(cdev->clk);
err_clk:
	devm_kfree(&pdev->dev, cdev);

	return ret;
}

static int coda7542_remove(struct platform_device *pdev)
{
	struct uio_coda7542_dev *cdev = platform_get_drvdata(pdev);

#ifdef CONFIG_CPU_PXA988
	pm_qos_remove_request(&vpu_qos_idle);
#endif
	uio_unregister_device(&cdev->uio_info);
	free_pages((unsigned long)cdev->uio_info.mem[1].internal_addr,
			get_order(VDEC_WORKING_BUFFER_SIZE));
	iounmap(cdev->uio_info.mem[0].internal_addr);

	clk_put(cdev->clk);

	kfree(cdev);

	return 0;
}

static void coda7542_shutdown(struct platform_device *dev)
{
}
static const struct dev_pm_ops coda7542_pm_ops = {
	.suspend = coda7542_suspend,
	.resume	= coda7542_resume,
};

static struct platform_driver coda7542_driver = {
	.probe = coda7542_probe,
	.remove = coda7542_remove,
	.shutdown = coda7542_shutdown,
	.driver = {
		.name = UIO_CODA7542_NAME,
		.owner = THIS_MODULE,
		.pm	= &coda7542_pm_ops,
	},
};


module_platform_driver(coda7542_driver);

MODULE_AUTHOR("Yuhua Guo (guoyh@marvell.com)");
MODULE_DESCRIPTION("UIO driver for Chip and Media Coda7542 codec");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" UIO_CODA7542_NAME);
