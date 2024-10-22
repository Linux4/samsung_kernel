// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "sapu_driver.h"
#include "mtk-smmu-v3.h"

#define ENABLE_DRAM_FB 1

#define SAPU_DATAMEM_PAGE_BASED_HEAP "mtk_sapu_page-uncached"
#define SAPU_DATAMEM_REGION_BASED_HEAP "mtk_sapu_data_shm_region-aligned"

enum SAPU_DATAMEM_TYPE {
	SAPU_DATAMEM_TYPE_DEFAULT,
	SAPU_DATAMEM_TYPE_REGION_BASED,
	SAPU_DATAMEM_TYPE_PAGE_BASED,
	SAPU_DATAMEM_TYPE_UNKNOWN
};

struct sapu_private *sapu;
struct sapu_private *get_sapu_private(void)
{
	return sapu;
}

static void sapu_rpm_lock_exit(void);

static struct mutex sapu_lock_rpm_mtx;
struct mutex *get_rpm_mtx(void)
{
	return &sapu_lock_rpm_mtx;
}

static struct sapu_lock_rpmsg_device sapu_lock_rpm_dev;
struct sapu_lock_rpmsg_device *get_rpm_dev(void)
{
	return &sapu_lock_rpm_dev;
}

static long
apusys_sapu_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	long ret;
	void __user *user_req = (void __user *)arg;

	ret = apusys_sapu_internal_ioctl(filep, cmd, user_req, 0);
	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long apusys_sapu_compat_ioctl(struct file *filep, unsigned int cmd,
	unsigned long arg)
{
	long ret;
	void __user *user_req = (void __user *)compat_ptr(arg);

	ret = apusys_sapu_internal_ioctl(filep, cmd, user_req, 1);
	return ret;
}
#endif

#if ENABLE_DRAM_FB

static int dram_fb_register(void)
{
	int ret;
	struct platform_device *pdev;
	struct dma_heap *dma_heap;
	struct device *dmem_device;
	struct sg_table *dmem_sgt;
	struct device_node *sapu_node;
	struct device_node *smmu_node = NULL;
	struct platform_device *smmu_dev = NULL;

	u32 higher_32_iova;
	u32 lower_32_iova;
	u32 datamem_type = SAPU_DATAMEM_TYPE_DEFAULT;

	if (!sapu) {
		pr_info("%s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	pdev = sapu->pdev;
	if (pdev == NULL) {
		pr_info("%s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	dmem_device = &pdev->dev;
	if (dmem_device == NULL) {
		pr_info("%s %d\n", __func__, __LINE__);
		ret = -EINVAL;
		return -EINVAL;
	}

	ret = sapu->platdata->ops.power_ctrl(sapu, 1);
	if (ret != 0) {
		pr_info("%s %d: power on fail\n", __func__, __LINE__);
		return -EAGAIN;
	}

	sapu_node = dmem_device->of_node;
	if (!sapu_node) {
		pr_info("%s sapu_node is NULL\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(sapu_node, "datamem-type", &datamem_type);

	if (ret == -EINVAL)
		pr_info("[%s] datamem-type prop not found. Determined by smmu support", __func__);
	else
		pr_info("[%s] read prop datamem-type=%d (ret=%d)\n", __func__, datamem_type, ret);

	switch (datamem_type) {
	case SAPU_DATAMEM_TYPE_DEFAULT: {
		/* Legacy for mt6897 & mt6989 */
		pr_info("[%s] smmu support = %d\n", __func__, smmu_v3_enabled());
		if (smmu_v3_enabled()) {
			pr_info("[%s] find dmaheap %s\n", __func__, SAPU_DATAMEM_PAGE_BASED_HEAP);
			dma_heap = dma_heap_find(SAPU_DATAMEM_PAGE_BASED_HEAP);
		} else {
			pr_info("[%s] find dmaheap %s\n", __func__, SAPU_DATAMEM_REGION_BASED_HEAP);
			dma_heap = dma_heap_find(SAPU_DATAMEM_REGION_BASED_HEAP);
		}
		break;
	}
	case SAPU_DATAMEM_TYPE_REGION_BASED: {
		pr_info("[%s] find dmaheap %s\n", __func__, SAPU_DATAMEM_REGION_BASED_HEAP);
		dma_heap = dma_heap_find(SAPU_DATAMEM_REGION_BASED_HEAP);
		break;
	}
	case SAPU_DATAMEM_TYPE_PAGE_BASED: {
		pr_info("[%s] find dmaheap %s\n", __func__, SAPU_DATAMEM_PAGE_BASED_HEAP);
		dma_heap = dma_heap_find(SAPU_DATAMEM_PAGE_BASED_HEAP);
		break;
	}
	default: {
		pr_err("[%s] unknown datamem type = %u\n", __func__, datamem_type);
		dma_heap = NULL;
		break;
	}
	}
	if (!dma_heap) {
		pr_info("[%s]dma_heap_find fail\n", __func__);
		ret = -ENODEV;
		goto err_return;
	}

	sapu->dram_fb_info.dram_fb_dmabuf = dma_heap_buffer_alloc(
						dma_heap, 0x200000,
						DMA_HEAP_VALID_FD_FLAGS,
						DMA_HEAP_VALID_HEAP_FLAGS);

	dma_heap_put(dma_heap);
	if (IS_ERR(sapu->dram_fb_info.dram_fb_dmabuf)) {
		pr_info("[%s]dma_heap_buffer_alloc fail\n", __func__);
		ret = PTR_ERR(sapu->dram_fb_info.dram_fb_dmabuf);
		goto err_return;
	}

	if (smmu_v3_enabled()) {
		smmu_node = of_parse_phandle(sapu_node, "smmu-device", 0);
		if (!smmu_node) {
			pr_info("%s get smmu_node failed\n", __func__);
			return -ENODEV;
		}

		smmu_dev = of_find_device_by_node(smmu_node);
		if (!smmu_node) {
			pr_info("%s get smmu_dev failed\n", __func__);
			return -ENODEV;
		}

		sapu->dram_fb_info.dram_fb_attach = dma_buf_attach(
					sapu->dram_fb_info.dram_fb_dmabuf,
					&smmu_dev->dev);
	} else {
		sapu->dram_fb_info.dram_fb_attach = dma_buf_attach(
					sapu->dram_fb_info.dram_fb_dmabuf,
					dmem_device);
	}
	if (IS_ERR(sapu->dram_fb_info.dram_fb_attach)) {
		pr_info("[%s]dma_buf_attach fail\n", __func__);
		ret = PTR_ERR(sapu->dram_fb_info.dram_fb_attach);
		goto dmabuf_free;
	}

	dmem_sgt = dma_buf_map_attachment(sapu->dram_fb_info.dram_fb_attach,
					  DMA_BIDIRECTIONAL);
	if (IS_ERR(dmem_sgt)) {
		pr_info("[%s]dma_buf_map_attachment fail\n", __func__);
		ret = PTR_ERR(dmem_sgt);
		goto dmabuf_detach;
	}

	sapu->dram_fb_info.dram_dma_addr = sg_dma_address(dmem_sgt->sgl);

	higher_32_iova = (sapu->dram_fb_info.dram_dma_addr >> 32) & 0xFFFFFFFF;
	lower_32_iova = sapu->dram_fb_info.dram_dma_addr & 0xFFFFFFFF;
	ret = trusty_std_call32(pdev->dev.parent,
				MTEE_SMCNR(MT_SMCF_SC_SAPU_DRAM_FB,
					   pdev->dev.parent),
				1, higher_32_iova, lower_32_iova);

	if (ret) {
		pr_info("[%s]dram fallback register fail(0x%x)\n",
			__func__, ret);
		goto dmabuf_detach;
	} else {
		pr_info("[%s]dram fallback register success(0x%x)\n",
			__func__, ret);
	}

	ret = sapu->platdata->ops.power_ctrl(sapu, 0);
	if (ret != 0) {
		pr_info("%s %d: power off fail\n", __func__, __LINE__);
		return -EBUSY;
	}

	return 0;

dmabuf_detach:
	dma_buf_detach(sapu->dram_fb_info.dram_fb_dmabuf,
			sapu->dram_fb_info.dram_fb_attach);
dmabuf_free:
	dma_heap_buffer_free(sapu->dram_fb_info.dram_fb_dmabuf);
err_return:
	sapu->platdata->ops.power_ctrl(sapu, 0);

	return ret;
}

static int dram_fb_unregister(void)
{
	int ret;
	struct platform_device *pdev;

	u32 higher_32_iova;
	u32 lower_32_iova;

	if (!sapu) {
		pr_info("%s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	pdev = sapu->pdev;
	if (pdev == NULL) {
		pr_info("%s %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	// ret = apusys_pwr_switch(true, data);
	// if (ret != 0) {
	//	pr_info("%s %d: power on fail\n", __func__, __LINE__);
	//	return -EAGAIN;
	// }

	higher_32_iova = (sapu->dram_fb_info.dram_dma_addr >> 32) & 0xFFFFFFFF;
	lower_32_iova = sapu->dram_fb_info.dram_dma_addr & 0xFFFFFFFF;
	ret = trusty_std_call32(pdev->dev.parent,
			MTEE_SMCNR(MT_SMCF_SC_SAPU_DRAM_FB,
			pdev->dev.parent),
			0, higher_32_iova, lower_32_iova);

	if (ret) {
		pr_info("[%s]dram callback unregister fail(0x%x)\n",
			__func__, ret);
		return -EPERM;
	}
	pr_info("[%s]dram callback unregister success(0x%x)\n",
			__func__, ret);

	dma_buf_detach(sapu->dram_fb_info.dram_fb_dmabuf,
		sapu->dram_fb_info.dram_fb_attach);
	dma_heap_buffer_free(sapu->dram_fb_info.dram_fb_dmabuf);

	sapu->dram_fb_info.dram_fb_attach = NULL;
	sapu->dram_fb_info.dram_fb_dmabuf = NULL;
	sapu->dram_fb_info.dram_dma_addr = 0;

	// ret = apusys_pwr_switch(false, data);
	// if (ret != 0) {
	//	pr_info("%s %d: power off fail\n", __func__, __LINE__);
	//	return -EAGAIN;
	// }

	return 0;
}

#endif // ENABLE_DRAM_FB

static int apusys_sapu_open(struct inode *inode, struct file *filep)
{
	int ret = 0;

	if (!sapu) {
		pr_info("%s %d\n", __func__, __LINE__);
		return -EINVAL;
	}
	mutex_lock(&sapu->dmabuf_lock);
#if ENABLE_DRAM_FB
	if (sapu->ref_count == 0 || !sapu->dram_register) {
		ret = dram_fb_register();
		if (!ret)
			sapu->dram_register = true;
	}
#endif // ENABLE_DRAM_FB
	sapu->ref_count++;
	mutex_unlock(&sapu->dmabuf_lock);
	return ret;
}

static int apusys_sapu_release(struct inode *inode, struct file *filep)
{
	int ret = 0;

	if (!sapu) {
		pr_info("error: %s - NULL sapu\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&sapu->dmabuf_lock);
#if ENABLE_DRAM_FB
	if (sapu->ref_count == 1 && sapu->dram_register) {
		ret = dram_fb_unregister();
		if (!ret)
			sapu->dram_register = false;
	}
#endif // ENABLE_DRAM_FB
	sapu->ref_count--;
	mutex_unlock(&sapu->dmabuf_lock);

	return ret;
}

static ssize_t
apusys_sapu_read(struct file *filep, char __user *buf, size_t count,
					   loff_t *pos)
{
	return 0;
}

/*
 *static struct miscdevice apusys_sapu_device = {
 *	.minor = MISC_DYNAMIC_MINOR,
 *	.name = "apusys_sapu",
 *	.fops = &apusys_sapu_fops,
 *	.mod = 0x0660,
 *};
 *
 *struct mtk_sapu {
 *	struct platform_device *pdev;
 *	struct miscdevice mdev;
 *};
 */

static const struct file_operations apusys_sapu_fops = {
	.owner = THIS_MODULE,
	.open = apusys_sapu_open,
	.release = apusys_sapu_release,
	.unlocked_ioctl = apusys_sapu_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = apusys_sapu_compat_ioctl,
#endif
	.read = apusys_sapu_read,
};

static int apusys_sapu_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;

	sapu = devm_kzalloc(dev, sizeof(struct sapu_private), GFP_KERNEL);
	if (!sapu)
		return -ENOMEM;

	platform_set_drvdata(pdev, sapu);
	sapu->pdev = pdev;

	sapu->mdev.minor  = MISC_DYNAMIC_MINOR;
	sapu->mdev.name   = "apusys_sapu";
	sapu->mdev.fops   = &apusys_sapu_fops;
	sapu->mdev.parent = NULL;
	sapu->mdev.mode   = 0x0660;

	kref_init(&sapu->lock_ref_cnt); // ref_count == 1

	//apusys_sapu_device.this_device = apusys_sapu_device->dev;
	ret = misc_register(&sapu->mdev);

	if (ret != 0) {
		pr_info("error: misc register failed.");
		devm_kfree(dev, sapu);
	}

	mutex_init(&sapu->dmabuf_lock);
	memset(&sapu->dram_fb_info, 0, sizeof(sapu->dram_fb_info));
	sapu->ref_count = 0;
	sapu->dram_register = false;

	/* Get platdata from dts match */
	sapu->platdata = (struct sapu_platdata *)
			 of_device_get_match_data(&pdev->dev);
	if (!sapu->platdata) {
		pr_info("error: %s - NULL sapu platdata\n", __func__);
		return -EINVAL;
	}

	/* runtime set platdata by dts*/
	set_sapu_platdata_by_dts();

	sapu->platdata->ops.detect();

	return ret;
}
static int apusys_sapu_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	if (!sapu) {
		mutex_destroy(&sapu->dmabuf_lock);
		misc_deregister(&sapu->mdev);
		devm_kfree(dev, sapu);
	}

	return 0;
}

static int sapu_lock_rpmsg_probe(struct rpmsg_device *rpdev)
{
	pr_info("%s: name=%s, src=%d\n",
			__func__, rpdev->id.name, rpdev->src);

	sapu_lock_rpm_dev.ept = rpdev->ept;
	sapu_lock_rpm_dev.rpdev = rpdev;

	pr_info("%s: rpdev->ept = %p\n", __func__, rpdev->ept);

	return 0;
}

static int sapu_lock_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct sapu_power_ctrl *d = data;

	pr_info("%s: lock = %d\n", __func__, d->lock);
	complete(&sapu_lock_rpm_dev.ack);

	return 0;
}

static void sapu_lock_rpmsg_remove(struct rpmsg_device *rpdev)
{
	sapu_rpm_lock_exit();
}

#define MODULE_NAME "apusys_sapu"
static const struct of_device_id apusys_sapu_of_match[] = {
	{ .compatible = "mediatek,trusty-sapu", .data = &sapu_platdata},
	{ /* end of list */},
};
MODULE_DEVICE_TABLE(of, apusys_sapu_of_match);

struct platform_driver apusys_sapu_driver = {
	.probe = apusys_sapu_probe,
	.remove = apusys_sapu_remove,
	.driver	= {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = apusys_sapu_of_match,
	},
};

static const struct of_device_id sapu_lock_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-lock-rv-rpmsg", },
	{},
};

struct rpmsg_driver sapu_lock_rpmsg_driver = {
	.drv = {
		.name = "apu-lock-rv-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = sapu_lock_rpmsg_of_match,
	},
	.probe = sapu_lock_rpmsg_probe,
	.callback = sapu_lock_rpmsg_cb,
	.remove = sapu_lock_rpmsg_remove,
};

static int sapu_rpm_lock_init(void)
{
	int ret = 0;

	init_completion(&sapu_lock_rpm_dev.ack);
	mutex_init(&sapu_lock_rpm_mtx);

	pr_info("%s: register rpmsg...\n", __func__);
	ret = register_rpmsg_driver(&sapu_lock_rpmsg_driver);
	if (ret) {
		pr_info("(%d)failed to register sapu lock rpmsg driver\n",
			ret);
		mutex_destroy(&sapu_lock_rpm_mtx);
		goto error;
	}
error:
	return ret;
}

static void sapu_rpm_lock_exit(void)
{
	unregister_rpmsg_driver(&sapu_lock_rpmsg_driver);
	mutex_destroy(&sapu_lock_rpm_mtx);
}

static int __init sapu_init(void)
{
	int ret;

	ret = platform_driver_register(&apusys_sapu_driver);

	if (ret) {
		pr_info("[%s] %s register fail\n",
			__func__, "apusys_sapu_driver");
		goto sapu_driver_quit;
	}

	ret = sapu_rpm_lock_init();

	if (ret) {
		pr_info("[%s] %s register fail\n",
			__func__,
			"sapu_lock_rpmsg_driver");
		goto sapu_rpmsg_quit;
	}

	return 0;

sapu_rpmsg_quit:
	unregister_rpmsg_driver(&sapu_lock_rpmsg_driver);
sapu_driver_quit:
	platform_driver_unregister(&apusys_sapu_driver);
	return ret;
}

void sapu_exit(void)
{
	unregister_rpmsg_driver(&sapu_lock_rpmsg_driver);
	platform_driver_unregister(&apusys_sapu_driver);
}

module_init(sapu_init);
module_exit(sapu_exit);
MODULE_LICENSE("GPL v2");
