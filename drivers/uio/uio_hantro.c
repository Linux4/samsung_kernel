/*
 * drivers/uio/uio_hantro.c
 *
 * Hantro video decoder engine UIO driver.
 *
 * Baoyin Shan <byshan@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * Based on an earlier version by Rashid Zaman <rzaman@marvell.com>.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/uio_hantro.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_qos.h>

#include <linux/pm_runtime.h>

#define UIO_HANTRO_VERSION	"build-001"

#ifdef HANTRO_DEBUG
#undef PDEBUG
#define PDEBUG(fmt, args...) pr_info("uio_hantro: " fmt, ## args)
#else
#define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif

#define ENC_IO_SIZE (240 * 4)	/* bytes */

static const int dec_hw_id[] = { 0x8190, 0x8170, 0x9170, 0x9190, 0x6731 };
static const int enc_hw_id[] = { 0x6280, 0x7280, 0x8270, 0x8290, 0x4831 };
#define HW_DET_RETRY 3

static struct vpu_instance dec_ins_arr[MAX_NUM_DECINS];
static struct vpu_instance pp_ins_arr[MAX_NUM_PPINS];
static struct vpu_instance enc_ins_arr[MAX_NUM_ENCINS];

/* protect dec/pp/enc shared hw resource such as power*/
static DEFINE_MUTEX(hw_mutex);

/* semaphore to protect dfc and HW access */
static DEFINE_SEMAPHORE(dec_sema);
static DEFINE_SEMAPHORE(pp_sema);
static DEFINE_SEMAPHORE(enc_sema);

int vpu_dfc_lock(struct clk *clk, unsigned int lock)
{
	int ret = 0;
	if (!strcmp("vpu_decoder", clk->name)) {
		if (lock) {
			ret = down_interruptible(&dec_sema);
			ret = down_interruptible(&pp_sema);
		} else {
			up(&pp_sema);
			up(&dec_sema);
		}
	} else if (!strcmp("vpu_encoder", clk->name)) {
		if (lock)
			ret = down_interruptible(&enc_sema);
		else
			up(&enc_sema);
	}

	return ret;
}

static struct vpu_instance *__get_ins_arr(struct vpu_dev *vdev)
{
	if (vdev->codec_type == HANTRO_DEC)
		return dec_ins_arr;
	else if (vdev->codec_type == HANTRO_PP)
		return pp_ins_arr;
	else if (vdev->codec_type == HANTRO_ENC)
		return enc_ins_arr;
	else
		pr_err("%s, vpu get ins arr error!!!\n", __func__);

	return NULL;
}

static int __max_ins_num(struct vpu_dev *vdev)
{
	if (vdev->codec_type == HANTRO_DEC)
		return MAX_NUM_DECINS;
	else if (vdev->codec_type == HANTRO_PP)
		return MAX_NUM_PPINS;
	else if (vdev->codec_type == HANTRO_ENC)
		return MAX_NUM_ENCINS;
	else
		pr_err("%s, vpu get max ins num error!!!\n", __func__);

	return -1;
}

static int hantro_check_hw(struct vpu_dev *vdev)
{
	long int hw_id;
	int ret = 0;
	size_t num_hw;
	int retry_cnt = 0;

	while (retry_cnt++ < HW_DET_RETRY) {
		pr_info("%s, %d time...\n", __func__, retry_cnt);
		hw_id = readl(vdev->reg_base);
		pr_info("%s, HW ID=0x%lx\n", __func__, hw_id);

		hw_id = (hw_id >> 16) & 0xFFFF;   /* product version only */

		if ((vdev->codec_type == HANTRO_DEC) ||
				(vdev->codec_type == HANTRO_PP)) {
			num_hw = sizeof(dec_hw_id) / sizeof(*dec_hw_id);
			while (num_hw--) {
				if (hw_id == dec_hw_id[num_hw]) {
					pr_info("hx170dec: HW found\n");
					goto out;
				}
			}
		} else if (vdev->codec_type == HANTRO_ENC) {
			num_hw = sizeof(enc_hw_id) / sizeof(*enc_hw_id);
			while (num_hw--) {
				if (hw_id == enc_hw_id[num_hw]) {
					pr_info("hx280enc: HW found\n");
					goto out;
				}
			}
		}
	}
	pr_err("%s: No Compatible HW found at 0x%ld\n", __func__,
			(unsigned long)vdev->uio_info.mem[0].addr);
	ret = -ENODEV;

out:
	return ret;
}

static int hantro_reset(struct vpu_dev *vdev)
{
	int i;

	if (!atomic_read(&vdev->clk_on))
		return 0;

	if ((vdev->codec_type == HANTRO_DEC) ||
			(vdev->codec_type == HANTRO_PP)) {
		writel(0, vdev->reg_base + 0x04);
		for (i = 4; i < vdev->uio_info.mem[0].size; i += 4)
			writel(0, vdev->reg_base + i);
	} else if (vdev->codec_type == HANTRO_ENC) {
		writel(0, vdev->reg_base + 0x38);
		for (i = 4; i < ENC_IO_SIZE; i += 4)
			writel(0, vdev->reg_base + i);
	}

	return 0;
}

static int hantro_cleanup(struct vpu_dev *vdev)
{
	if (!atomic_read(&vdev->clk_on))
		return 0;

	switch (vdev->codec_type) {
	case HANTRO_DEC:
		writel(0, vdev->reg_base + 0x04);
		break;
	case HANTRO_PP:
		writel(0, vdev->reg_base + 0xF0);
		break;
	case HANTRO_ENC:
		writel(0, vdev->reg_base + 0x38);
		writel(0, vdev->reg_base + INT_REG_ENC);
		break;
	default:
		return -ENODEV;
	}

	return 0;
}

static void vpu_lock_init(struct vpu_dev *vdev)
{
	switch (vdev->codec_type) {
	case HANTRO_DEC:
		vdev->sema = &dec_sema;
		break;
	case HANTRO_PP:
		vdev->sema = &pp_sema;
		break;
	case HANTRO_ENC:
		vdev->sema = &enc_sema;
		break;
	default:
		return;
	}
}

static int vpu_lock(unsigned long ms, struct vpu_dev *vdev)
{
	int ret;

	ret = down_timeout(vdev->sema, msecs_to_jiffies(ms));
	PDEBUG("%s, dev_type = %d, ret = %d\n",
			__func__, vdev->codec_type, ret);

	return ret;
}

static int vpu_unlock(struct vpu_dev *vdev)
{
	PDEBUG("%s, dev_type = %d, sema_count = %d\n",
			__func__, vdev->codec_type, vdev->sema->count);
	if (vdev->sema->count == 0) {
		up(vdev->sema);
		return 0;
	} else if (vdev->sema->count == 1) {
		return 0;
	} else
		return -1;
}

static int vpu_clk_on(struct vpu_dev *vdev)
{
	if ((NULL != vdev->fclk) && (NULL != vdev->bclk) &&
		!atomic_read(&vdev->clk_on)) {
		clk_prepare_enable(vdev->fclk);
		clk_prepare_enable(vdev->bclk);
		atomic_set(&vdev->clk_on, 1);
	}

	PDEBUG("%s, dev_type = %d\n", __func__, vdev->codec_type);

	return 0;
}

static int vpu_clk_off(struct vpu_dev *vdev)
{
	if ((NULL != vdev->fclk) && (NULL != vdev->bclk) &&
		atomic_cmpxchg(&vdev->clk_on, 1, 0)) {
		clk_disable_unprepare(vdev->fclk);
		clk_disable_unprepare(vdev->bclk);
	}

	PDEBUG("%s, dev_type = %d\n", __func__, vdev->codec_type);

	return 0;
}

static int vpu_power_on(struct vpu_dev *vdev)
{
	mutex_lock(&hw_mutex);
	if (!atomic_cmpxchg(&vdev->power_on, 0, 1))
		pm_runtime_get_sync(vdev->dev);
	mutex_unlock(&hw_mutex);
	PDEBUG("%s, dev_type = %d\n", __func__, vdev->codec_type);

	return 0;
}

static int vpu_power_off(struct vpu_dev *vdev)
{
	mutex_lock(&hw_mutex);
	if (atomic_cmpxchg(&vdev->power_on, 1, 0))
		pm_runtime_put_sync(vdev->dev);
	mutex_unlock(&hw_mutex);
	PDEBUG("%s, dev_type = %d\n", __func__, vdev->codec_type);

	return 0;
}

static int vpu_turn_on(struct vpu_dev *vdev)
{
	int ret;

	ret = vpu_power_on(vdev);
	if (ret)
		return -1;

	ret = vpu_clk_on(vdev);
	if (ret)
		return -1;

	return 0;
}

static int vpu_turn_off(struct vpu_dev *vdev)
{
	int ret;

	ret = vpu_clk_off(vdev);
	if (ret)
		return -1;

	ret = vpu_power_off(vdev);
	if (ret)
		return -1;

	return 0;
}

static int vpu_open(struct uio_info *info, struct inode *inode,
			void *file_priv)
{
	struct vpu_dev *vdev;
	struct uio_listener *listener = file_priv;
	struct vpu_instance *vi_arr;
	int ret = 0, i, max_ins;

	vdev = (struct vpu_dev *)info->priv;
	vi_arr = vdev->fd_ins_arr;
	mutex_lock(&vdev->mutex);

	if (vdev->ins_cnt++ == 0)
		vpu_turn_on(vdev);

	max_ins = __max_ins_num(vdev);
	for (i = 0; i < max_ins; i++) {
		if (vi_arr[i].occupied == 0) {
			listener->extend = &vi_arr[i];
			vi_arr[i].occupied = 1;
			vi_arr[i].got_sema = 0;
			break;
		}
	}
	if (i == max_ins) {
		vdev->ins_cnt--;
		pr_err("%s, vpu instance excess max instance num!!\n",
				__func__);
		ret = -EBUSY;
	}

	mutex_unlock(&vdev->mutex);
	PDEBUG("dev opened by %d, ins_arr[%d]\n", vdev->codec_type, i);
	return ret;
}

static int vpu_release(struct uio_info *info, struct inode *inode,
				void *file_priv)
{
	struct vpu_dev *vdev;
	struct uio_listener *listener = file_priv;
	struct vpu_instance *vi;

	vdev = (struct vpu_dev *)info->priv;
	vi = (struct vpu_instance *)listener->extend;

	mutex_lock(&vdev->mutex);
	if (vi->got_sema) {
		pr_err("%s, instance still got semaphore!!!\n", __func__);
		hantro_cleanup(vdev);
		vpu_unlock(vdev);
		vi->got_sema = 0;
	}

	vi->occupied = 0;
	listener->extend = NULL;

	if ((vdev->ins_cnt > 0) && (vdev->ins_cnt-- == 1))
		vpu_turn_off(vdev);

	mutex_unlock(&vdev->mutex);

	PDEBUG("dev released by codec_type:%d\n", vdev->codec_type);
	return 0;
}

static irqreturn_t vpu_func_irq_handler(int irq, struct uio_info *dev_info)
{
	struct vpu_dev *vdev = dev_info->priv;
	unsigned long flags;
	u32 irq_status_dec, irq_status_pp, irq_status_enc;

	spin_lock_irqsave(&vdev->lock, flags);

	if (!atomic_read(&vdev->clk_on)) {
		spin_unlock_irqrestore(&vdev->lock, flags);
		return IRQ_NONE;
	}

	if (vdev->codec_type == HANTRO_DEC) {
		irq_status_dec = readl(vdev->reg_base + INT_REG_DEC);
		if (irq_status_dec & DEC_INT_BIT)
			writel(irq_status_dec & (~DEC_INT_BIT),
				vdev->reg_base + INT_REG_DEC);
		else {
			spin_unlock_irqrestore(&vdev->lock, flags);
			return IRQ_NONE;
		}
	} else if (vdev->codec_type == HANTRO_PP) {
		irq_status_pp = readl(vdev->reg_base + INT_REG_PP);
		if (irq_status_pp & PP_INT_BIT)
			writel(irq_status_pp & (~PP_INT_BIT),
				vdev->reg_base + INT_REG_PP);
		else {
			spin_unlock_irqrestore(&vdev->lock, flags);
			return IRQ_NONE;
		}
	} else if (vdev->codec_type == HANTRO_ENC) {
		irq_status_enc = readl(vdev->reg_base + INT_REG_ENC);
		if (irq_status_enc & 0x01)
			/* clear enc IRQ and slice ready interrupt bit */
			writel(irq_status_enc & (~0x101),
					vdev->reg_base + INT_REG_ENC);
		/* FIXME Does it need to handle frame ready here?? */
		else {
			spin_unlock_irqrestore(&vdev->lock, flags);
			return IRQ_NONE;
		}
	}

	spin_unlock_irqrestore(&vdev->lock, flags);

	return IRQ_HANDLED;
}

static int vpu_ioctl(struct uio_info *info, unsigned int cmd,
			unsigned long arg, void *file_priv)
{
	int ret = 0;
	int ins_id = -1;
	void *argp = (void *)arg;
	struct vpu_dev *vdev = info->priv;
	struct uio_listener *listener = file_priv;
	struct vpu_instance *vi;

	vi = (struct vpu_instance *)listener->extend;

	switch (cmd) {
	case HANTRO_CMD_POWER_ON:
		mutex_lock(&vdev->mutex);
		ret = vpu_power_on(vdev);
		mutex_unlock(&vdev->mutex);
		break;
	case HANTRO_CMD_POWER_OFF:
		mutex_lock(&vdev->mutex);
		ret = vpu_power_off(vdev);
		mutex_unlock(&vdev->mutex);
		break;
	case HANTRO_CMD_CLK_ON:
		mutex_lock(&vdev->mutex);
		ret = vpu_clk_on(vdev);
		mutex_unlock(&vdev->mutex);
		break;
	case HANTRO_CMD_CLK_OFF:
		mutex_lock(&vdev->mutex);
		ret = vpu_clk_off(vdev);
		mutex_unlock(&vdev->mutex);
		break;
	case HANTRO_CMD_LOCK:
		if (vi->got_sema == 0) {
			ret = vpu_lock(arg, vdev);
			uio_event_sync(listener);
			if (ret == 0)
				vi->got_sema = 1;
		}
		break;
	case HANTRO_CMD_UNLOCK:
		if (vi->got_sema) {
			ret = vpu_unlock(vdev);
			vi->got_sema = 0;
		}
		break;
	case HANTRO_CMD_GET_INS_ID:
		ins_id = vi->id;
		if (copy_to_user(argp, &ins_id, sizeof(ins_id))) {
			pr_err("%s, copy to user error!\n", __func__);
			ret = -EFAULT;
		}
		break;
	case HANTRO_CMD_QUERY_CAP:
		if (copy_to_user(argp, &vdev->hw_cap, sizeof(vdev->hw_cap))) {
			pr_err("%s, copy to user error!\n", __func__);
			ret = -EFAULT;
		}
		break;
	default:
		ret = -EFAULT;
		break;
	}

	return ret;
}

static int vpu_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct vpu_dev *vdev;
	int ret = 0, i, max_ins;
	int irq_num;
	int codec_type;
	int capacity;
	struct device_node *np = pdev->dev.of_node;

	if (!np)
		return -EINVAL;
	if (of_property_read_u32(np, "marvell,codec-type", &codec_type))
		return -EINVAL;

	if (of_property_read_u32(np, "marvell,hw-capacity", &capacity))
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no memory resources given!!!\n");
		return -ENODEV;
	}

	irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0) {
		dev_err(&pdev->dev, "missing irq resource!!!\n");
		return -ENODEV;
	}

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev) {
		dev_err(&pdev->dev, "vpu_dev: out of memory!!!\n");
		return -ENOMEM;
	}

	vdev->fclk = devm_clk_get(&pdev->dev, "vpu-fclk");
	if (IS_ERR(vdev->fclk)) {
		dev_err(&pdev->dev, "Cannot get fclk ptr.\n");
		ret = PTR_ERR(vdev->fclk);
		goto err_clk2;
	}

	vdev->bclk = devm_clk_get(&pdev->dev, "vpu-bclk");
	if (IS_ERR(vdev->bclk)) {
		dev_err(&pdev->dev, "cannot get bclk ptr.\n");
		ret = PTR_ERR(vdev->bclk);
		goto err_clk1;
	}

	vdev->reg_base = (void *)ioremap_nocache(res->start,
		resource_size(res));
	if (!vdev->reg_base) {
		dev_err(&pdev->dev, "can't remap VPU registers!!!\n");
		ret = -ENOMEM;
		goto err_reg_base;
	}

	platform_set_drvdata(pdev, vdev);

	spin_lock_init(&vdev->lock);

	/* DEC: 0, PP: 1, ENC: 2 */
	vdev->codec_type = codec_type;
	pr_info("%s, vdev->codec_type = %d\n", __func__, vdev->codec_type);
	vdev->flags = 0;
	atomic_set(&vdev->power_on, 0);
	atomic_set(&vdev->clk_on, 0);
	vdev->ins_cnt = 0;
	vdev->hw_cap = capacity;

	vdev->dev = &pdev->dev;
	vdev->uio_info.name = UIO_HANTRO_NAME;
	vdev->uio_info.version = UIO_HANTRO_VERSION;
	vdev->uio_info.mem[0].internal_addr = (void __iomem *)vdev->reg_base;
	vdev->uio_info.mem[0].addr = res->start;
	vdev->uio_info.mem[0].memtype = UIO_MEM_PHYS;
	vdev->uio_info.mem[0].size = resource_size(res);

	vdev->uio_info.irq_flags = IRQF_SHARED;
	vdev->uio_info.irq = irq_num;
	vdev->uio_info.handler = vpu_func_irq_handler;
	vdev->uio_info.priv = vdev;

	vdev->uio_info.open = vpu_open;
	vdev->uio_info.release = vpu_release;
	vdev->uio_info.ioctl = vpu_ioctl;
	vdev->uio_info.mmap = NULL;

	pm_runtime_enable(vdev->dev);

	mutex_init(&(vdev->mutex));
	vpu_lock_init(vdev);
	/* for multi-instance */
	max_ins = __max_ins_num(vdev);
	vdev->fd_ins_arr = __get_ins_arr(vdev);
	for (i = 0; i < max_ins; i++) {
		vdev->fd_ins_arr[i].id = i;
		vdev->fd_ins_arr[i].occupied = 0;
		vdev->fd_ins_arr[i].got_sema = 0;
	}

	ret = uio_register_device(&pdev->dev, &vdev->uio_info);
	if (ret) {
		dev_err(&pdev->dev, "failed to register uio device\n");
		goto err_uio_register;
	}

	/* turn on power/clk before check HW */
	vpu_turn_on(vdev);
	if (hantro_check_hw(vdev)) {
		ret = -EBUSY;
		vpu_turn_off(vdev);
		goto err_uio_register;
	}
	hantro_reset(vdev);
	vpu_turn_off(vdev);

	return 0;

err_uio_register:
	iounmap(vdev->uio_info.mem[0].internal_addr);
err_reg_base:
	devm_clk_put(&pdev->dev, vdev->bclk);
err_clk1:
	devm_clk_put(&pdev->dev, vdev->fclk);
err_clk2:
	kfree(vdev);

	return ret;
}

static int vpu_remove(struct platform_device *pdev)
{
	struct vpu_dev *vdev = platform_get_drvdata(pdev);

	uio_unregister_device(&vdev->uio_info);
	iounmap(vdev->uio_info.mem[0].internal_addr);

	devm_clk_put(&pdev->dev, vdev->fclk);
	devm_clk_put(&pdev->dev, vdev->bclk);

	pm_runtime_disable(vdev->dev);

	kfree(vdev);

	return 0;
}

#ifdef CONFIG_PM
static int vpu_suspend(struct platform_device *dev, pm_message_t state)
{
	struct vpu_dev *vdev = platform_get_drvdata(dev);

	/* turn off power if vpu isn't running and power is on,
	 * otherwise, do nothing.
	 */
	if (!atomic_read(&vdev->clk_on) && atomic_read(&vdev->power_on)) {
		vpu_power_off(vdev);
		atomic_set(&vdev->suspend, 1);
	}

	return 0;
}

static int vpu_resume(struct platform_device *dev)
{
	struct vpu_dev *vdev = platform_get_drvdata(dev);

	/* Turn on the power if it was turned off in suspend. */
	if (atomic_cmpxchg(&vdev->suspend, 1, 0))
		vpu_power_on(vdev);
	return 0;
}
#endif

static struct of_device_id hantro_dt_ids[] = {
	{ .compatible = "mrvl,mmp-hantro",},
	{}
};
MODULE_DEVICE_TABLE(of, hantro_dt_ids);

static struct platform_driver vpu_driver = {
	.probe = vpu_probe,
	.remove = vpu_remove,
#ifdef CONFIG_PM
	.suspend = vpu_suspend,
	.resume = vpu_resume,
#endif
	.driver = {
		.name = UIO_HANTRO_NAME,
		.owner = THIS_MODULE,
		.of_match_table = hantro_dt_ids,
	},
};

static int __init uio_vpu_init(void)
{
	return platform_driver_register(&vpu_driver);
}

static void __exit uio_vpu_exit(void)
{
	platform_driver_unregister(&vpu_driver);
}

module_init(uio_vpu_init);
module_exit(uio_vpu_exit);

MODULE_AUTHOR("Baoyin Shan (byshan@marvell.com)");
MODULE_DESCRIPTION("UIO driver for Hantro codec");
MODULE_LICENSE("GPL v2");
