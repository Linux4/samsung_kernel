// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *   USB Audio offloading Driver for Exynos
 *
 *   Copyright (c) 2017 by Kyounghye Yun <k-hye.yun@samsung.com>
 *
 */


#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/iommu.h>
#include <linux/phy/phy.h>

#include <soc/samsung/exynos-smc.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include "exynos_usb_audio.h"
#include "usbaudio.h"
#include "helper.h"
#include "card.h"
#include "quirks.h"

#define DEBUG 1
#define PARAM1 0x4
#define PARAM2 0x8
#define PARAM3 0xc
#define PARAM4 0x10
#define RESPONSE 0x20

#define USB_IPC_IRQ	4

static const unsigned long USB_LOCK_ID_OUT = 0x4F425355; // ASCII Value : USBO
static const unsigned long USB_LOCK_ID_IN = 0x49425355; // ASCII Value : USBI

static struct exynos_usb_audio usb_audio;
static int exynos_usb_audio_unmap_all(void);

struct hcd_hw_info *g_hwinfo;
int usb_audio_connection;

static void __iomem *base_addr;
struct usb_audio_msg usb_msg;
struct iommu_domain *iommu_domain;
struct usb_audio_gic_data *gic_data;

static int gicd_xwritel(struct usb_audio_gic_data *data, u32 value,
							unsigned int offset)
{
	unsigned long arg1;
	int ret = 0;

	offset += data->gicd_base_phys;
	arg1 = SMC_REG_ID_SFR_W(offset);
	ret = exynos_smc(SMC_CMD_REG, arg1, value, 0);
	if (ret < 0)
		dev_err(data->dev, "write fail %#x: %d\n", offset, ret);

	return ret;
}

int usb_audio_gic_generate_interrupt(struct device *dev, unsigned int irq)
{

	dev_dbg(dev, "%s(%d)\n", __func__, irq);

	return gicd_xwritel(gic_data, (0x1 << 16) | (irq & 0xf), 0xf00);
}

static int usb_msg_to_uram(struct device *dev)
{
	unsigned int resp = 0;
	int ret = 0, timeout = 1000;

	mutex_lock(&usb_audio.msg_lock);
	iowrite32(usb_msg.type, base_addr);
	iowrite32(usb_msg.param1, base_addr + PARAM1);
	iowrite32(usb_msg.param2, base_addr + PARAM2);
	iowrite32(usb_msg.param3, base_addr + PARAM3);
	iowrite32(usb_msg.param4, base_addr + PARAM4);

	phy_set_mode_ext(g_hwinfo->phy, PHY_MODE_ABOX_POWER, 1);

	ret = usb_audio_gic_generate_interrupt(dev, USB_IPC_IRQ);

	if (ret < 0)
		dev_err(dev, "failed to generate interrupt\n");

	while (timeout) {
		resp = ioread32(base_addr + RESPONSE);
		if (resp != 0)
			break;
		udelay(300);
		timeout--;
	}

	if (timeout == 0)
		dev_err(dev, "No response to message. err = %d\n", resp);

	phy_set_mode_ext(g_hwinfo->phy, PHY_MODE_ABOX_POWER, 0);

	iowrite32(0, base_addr + RESPONSE);

	mutex_unlock(&usb_audio.msg_lock);
	return 0;
}

static int usb_audio_iommu_map(unsigned long iova, phys_addr_t addr,
		size_t bytes)
{
	int ret = 0;

	ret = iommu_map(iommu_domain, iova, addr, bytes, 0);
	if (ret < 0)
		pr_info("iommu_map(%#lx) fail: %d\n", iova, ret);

	return ret;
}

static int usb_audio_iommu_unmap(unsigned long iova, size_t bytes)
{
	int ret = 0;

	ret = iommu_unmap(iommu_domain, iova, bytes);
	if (ret < 0)
		pr_info("iommu_unmap(%#lx) fail: %d\n", iova, ret);

	return ret;
}

static void exynos_usb_audio_set_device(struct usb_device *udev)
{
	usb_audio.udev = udev;
	usb_audio.is_audio = 1;
}

static int exynos_usb_audio_map_buf(struct usb_device *udev)
{
	struct hcd_hw_info *hwinfo = g_hwinfo;
	struct usb_device *usb_dev = udev;
	u32 id;
	int ret;

	if (DEBUG)
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s\n", __func__);

	id = (((usb_dev->descriptor.idVendor) << 16) |
			(usb_dev->descriptor.idProduct));

	/* iommu map for in data buffer */
	usb_audio.in_buf_addr = hwinfo->in_dma;

	if (DEBUG) {
		pr_info("pcm in data buffer pa addr : %#08x %08x\n",
				upper_32_bits(le64_to_cpu(hwinfo->in_dma)),
				lower_32_bits(le64_to_cpu(hwinfo->in_dma)));
	}

	ret = usb_audio_iommu_map(USB_AUDIO_PCM_INBUF, hwinfo->in_dma, PAGE_SIZE * 256);
	if (ret) {
		pr_err("abox iommu mapping for pcm in buf is failed\n");
		return ret;
	}

	/* iommu map for out data buffer */
	usb_audio.out_buf_addr = hwinfo->out_dma;

	if (DEBUG) {
		pr_info("pcm out data buffer pa addr : %#08x %08x\n",
				upper_32_bits(le64_to_cpu(hwinfo->out_dma)),
				lower_32_bits(le64_to_cpu(hwinfo->out_dma)));
	}

	ret = usb_audio_iommu_map(USB_AUDIO_PCM_OUTBUF, hwinfo->out_dma, PAGE_SIZE * 256);
	if (ret) {
		pr_err("abox iommu mapping for pcm out buf is failed\n");
		return ret;
	}

	return 0;
}

static int exynos_usb_audio_hcd(struct usb_device *udev)
{
	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	struct hcd_hw_info *hwinfo = g_hwinfo;
	int ret = 0;

	if (DEBUG) {
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s udev->devnum %d\n", __func__, udev->devnum);
		dev_info(&udev->dev, "=======[Check HW INFO] ========\n");
		dev_info(&udev->dev, "slot_id : %d\n", hwinfo->slot_id);
		dev_info(&udev->dev, "dcbaa : %#08llx\n", hwinfo->dcbaa_dma);
		dev_info(&udev->dev, "save : %#08llx\n", hwinfo->save_dma);
		dev_info(&udev->dev, "in_ctx : %#08llx\n", hwinfo->in_ctx);
		dev_info(&udev->dev, "out_ctx : %#08llx\n", hwinfo->out_ctx);
		dev_info(&udev->dev, "erst : %#08x %08x\n",
					upper_32_bits(le64_to_cpu(hwinfo->erst_addr)),
					lower_32_bits(le64_to_cpu(hwinfo->erst_addr)));
		dev_info(&udev->dev, "use_uram : %d\n", hwinfo->use_uram);
		dev_info(&udev->dev, "===============================\n");
	}

	/* back up each address for unmap */
	usb_audio.dcbaa_dma = hwinfo->dcbaa_dma;
	usb_audio.save_dma = hwinfo->save_dma;
	usb_audio.in_ctx = hwinfo->in_ctx;
	usb_audio.out_ctx = hwinfo->out_ctx;
	usb_audio.erst_addr = hwinfo->erst_addr;
	usb_audio.speed = hwinfo->speed;
	usb_audio.use_uram = hwinfo->use_uram;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : SFR MAPPING!\n");

	mutex_lock(&usb_audio.lock);
	ret = usb_audio_iommu_map(USB_AUDIO_XHCI_BASE, USB_AUDIO_XHCI_BASE, PAGE_SIZE * 16);
	/*
	 * Check whether usb buffer was unmapped already.
	 * If not, unmap all buffers and try map again.
	 */
	if (ret == -EADDRINUSE) {
		//cancel_work_sync(&usb_audio.usb_work);
		pr_err("iommu unmapping not done. unmap here\n", ret);
		exynos_usb_audio_unmap_all();
		ret = usb_audio_iommu_map(USB_AUDIO_XHCI_BASE,
				USB_AUDIO_XHCI_BASE, PAGE_SIZE * 16);
	}

	if (ret) {
		pr_err("iommu mapping for in buf failed %d\n", ret);
		goto err;
	}

	/*DCBAA mapping*/
	ret = usb_audio_iommu_map(USB_AUDIO_SAVE_RESTORE, hwinfo->save_dma, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for save_restore buffer is failed\n");
		goto err;
	}

	/*Device Context mapping*/
	ret = usb_audio_iommu_map(USB_AUDIO_DEV_CTX, hwinfo->out_ctx, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for device ctx is failed\n");
		goto err;
	}

	/*Input Context mapping*/
	ret = usb_audio_iommu_map(USB_AUDIO_INPUT_CTX, hwinfo->in_ctx, PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for input ctx is failed\n");
		goto err;
	}

	if (hwinfo->use_uram) {
		/*URAM Mapping*/
		ret = usb_audio_iommu_map(USB_URAM_BASE, USB_URAM_BASE, USB_URAM_SIZE);
		if (ret) {
			pr_err(" abox iommu mapping for URAM buffer is failed\n");
			goto err;
		}

		/* Mapping both URAM and original ERST address */
		ret = usb_audio_iommu_map(USB_AUDIO_ERST, hwinfo->erst_addr,
								PAGE_SIZE);
		if (ret) {
			pr_err(" abox iommu mapping for erst is failed\n");
			goto err;
		}
	} else {
		/*ERST mapping*/
		ret = usb_audio_iommu_map(USB_AUDIO_ERST, hwinfo->erst_addr,
								PAGE_SIZE);
		if (ret) {
			pr_err(" abox iommu mapping for erst is failed\n");
			goto err;
		}
	}
	usb_msg.type = GIC_USB_XHCI;
	usb_msg.param1 = usb_audio.is_first_probe;
	usb_msg.param2 = hwinfo->slot_id;
	usb_msg.param3 = lower_32_bits(le64_to_cpu(hwinfo->erst_addr));
	usb_msg.param4 = upper_32_bits(le64_to_cpu(hwinfo->erst_addr));

	ret = usb_msg_to_uram(dev);
	if (ret) {
		dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
		goto err;
	}

	usb_audio.is_first_probe = 0;
	mutex_unlock(&usb_audio.lock);

	return 0;
err:
	dev_err(&udev->dev, "%s error = %d\n", __func__, ret);
	usb_audio.is_first_probe = 0;
	mutex_unlock(&usb_audio.lock);
	return ret;
}

static int exynos_usb_audio_desc(struct usb_device *udev)
{
	int configuration, cfgno, i;
	unsigned char *buffer;
	unsigned int len = g_hwinfo->rawdesc_length;
	u64 desc_addr;
	u64 offset;
	int ret = 0;

	if (DEBUG)
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s\n", __func__);

	configuration = usb_choose_configuration(udev);

	cfgno = -1;
	for (i = 0; i < udev->descriptor.bNumConfigurations; i++) {
		if (udev->config[i].desc.bConfigurationValue ==
				configuration) {
			cfgno = i;
			pr_info("%s - chosen = %d, c = %d\n", __func__,
				i, configuration);
			break;
		}
	}

	if (cfgno == -1) {
		pr_info("%s - config select error, i=%d, c=%d\n",
			__func__, i, configuration);
		cfgno = 0;
	}

	/* need to memory mapping for usb descriptor */
	buffer = udev->rawdescriptors[cfgno];
	desc_addr = virt_to_phys(buffer);
	offset = desc_addr % PAGE_SIZE;

	/* store address information */
	usb_audio.desc_addr = desc_addr;
	usb_audio.offset = offset;

	desc_addr -= offset;

	mutex_lock(&usb_audio.lock);
	ret = usb_audio_iommu_map(USB_AUDIO_DESC, desc_addr, (PAGE_SIZE * 2));
	if (ret) {
		dev_err(&udev->dev, "USB AUDIO: abox iommu mapping for usb descriptor is failed\n");
		goto err;
	}

	usb_msg.type = GIC_USB_DESC;
	usb_msg.param1 = 1;
	usb_msg.param2 = len;
	usb_msg.param3 = offset;
	usb_msg.param4 = usb_audio.speed;

	if (DEBUG)
		dev_info(&udev->dev, "paddr : %#08llx / offset : %#08llx / len : %d / speed : %d\n",
							desc_addr + offset, offset, len, usb_audio.speed);

	ret = usb_msg_to_uram(&udev->dev);
	if (ret) {
		dev_err(&usb_audio.udev->dev, "erap usb desc control failed\n");
		goto err;
	}
	mutex_unlock(&usb_audio.lock);

	dev_info(&udev->dev, "USB AUDIO: Mapping descriptor for using on Abox USB F/W & Nofity mapping is done!");

	return 0;
err:
	dev_err(&udev->dev, "%s error = %d\n", __func__, ret);
	mutex_unlock(&usb_audio.lock);
	return ret;
}

static int exynos_usb_audio_conn(struct usb_device *udev, int is_conn)
{
	int ret = 0;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s\n", __func__);

	pr_info("USB DEVICE IS %s\n", is_conn ? "CONNECTION" : "DISCONNECTION");

	mutex_lock(&usb_audio.lock);
	usb_msg.type = GIC_USB_CONN;
	usb_msg.param1 = is_conn;

	if (is_conn == 0) {
		if (usb_audio.is_audio) {
			usb_audio.is_audio = 0;
			usb_audio.usb_audio_state = USB_AUDIO_REMOVING;
			ret = usb_msg_to_uram(&udev->dev);
			if (ret) {
				pr_err("erap usb dis_conn control failed\n");
				goto err;
			}
			exynos_usb_audio_unmap_all();
		} else {
			pr_err("Is not USB Audio device\n");
		}
	} else {
		//cancel_work_sync(&usb_audio.usb_work);
		usb_audio.indeq_map_done = 0;
		usb_audio.outdeq_map_done = 0;
		usb_audio.fb_indeq_map_done = 0;
		usb_audio.fb_outdeq_map_done = 0;
		usb_audio.pcm_open_done = 0;
		//reinit_completion(&usb_audio.discon_done);
		ret = usb_msg_to_uram(&udev->dev);
		if (ret) {
			pr_err("erap usb conn control failed\n");
			goto err;
		}
		usb_audio.usb_audio_state = USB_AUDIO_CONNECT;
		usb_audio_connection = 1;
	}
err:
	mutex_unlock(&usb_audio.lock);
	return ret;
}

static int exynos_usb_audio_pcm(bool is_open, bool direction)
{
	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	int ret = 0;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : %s\n", __func__);

	if (usb_audio.is_audio == 0) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio is 0. return!\n");
		return -1;
	}

	if (is_open)
		usb_audio.pcm_open_done = 1;
	dev_info(dev, "PCM  %s\n", is_open ? "OPEN" : "CLOSE");

	mutex_lock(&usb_audio.lock);
	usb_msg.type = GIC_USB_PCM_OPEN;
	usb_msg.param1 = is_open;
	usb_msg.param2 = direction;

	ret = usb_msg_to_uram(dev);
	mutex_unlock(&usb_audio.lock);
	if (ret) {
		dev_err(&usb_audio.udev->dev, "ERAP USB PCM control failed\n");
		return -1;
	}

	if (is_open == 0) {
		if (direction == SNDRV_PCM_STREAM_PLAYBACK) {
			/* wait for completion */
			dev_info(dev, "%s: PCM OUT close\n", __func__);
		} else if (direction == SNDRV_PCM_STREAM_CAPTURE) {
			/* wait for completion */
			dev_info(dev, "%s: PCM IN close\n", __func__);
		}
	}

	return 0;
}

static int exynos_usb_scenario_ctl_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = AUDIO_MODE_NORMAL;
	uinfo->value.integer.max = AUDIO_MODE_CALL_SCREEN;
	return 0;
}

static int exynos_usb_scenario_ctl_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct exynos_usb_audio *usb = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = usb->user_scenario;
	return 0;
}

static int exynos_usb_scenario_ctl_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct exynos_usb_audio *usb = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	u32 new_mode;

	if (usb->user_scenario !=
	     ucontrol->value.integer.value[0]) {
		pr_info("%s, scenario = %d, set = %d\n", __func__,
			usb->user_scenario, ucontrol->value.integer.value[0]);
		new_mode = ucontrol->value.integer.value[0];
		/* New audio mode is call */
		if (new_mode == AUDIO_MODE_IN_CALL &&
				usb->user_scenario != AUDIO_MODE_IN_CALL) {
			/*atomic_set(&usb_audio.chip->active, 0);*/
			phy_set_mode_ext(usb_audio.phy,
					 PHY_MODE_CALL_ENTER, new_mode);
		} else if (usb->user_scenario == AUDIO_MODE_IN_CALL && /* previous mode */
						 new_mode != AUDIO_MODE_IN_CALL) {
			/*atomic_set(&usb_audio.chip->active, 1);*/
			phy_set_mode_ext(usb_audio.phy,
					 PHY_MODE_CALL_EXIT, new_mode);
		}
		usb->user_scenario = ucontrol->value.integer.value[0];
		changed = 1;
	}

	return changed;
}

static int exynos_usb_add_ctls(struct snd_usb_audio *chip,
				unsigned long private_value)
{
	struct snd_kcontrol_new knew = {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.name = "USB Audio Mode",
		.info = exynos_usb_scenario_ctl_info,
		.get = exynos_usb_scenario_ctl_get,
		.put = exynos_usb_scenario_ctl_put,
	};

	int err;

	if (!chip)
		return -ENODEV;

	knew.private_value = private_value;
	usb_audio.kctl = snd_ctl_new1(&knew, &usb_audio);
	if (!usb_audio.kctl) {
		dev_err(&usb_audio.udev->dev,
			"USB_AUDIO_IPC : %s-ctl new error\n", __func__);
		return -ENOMEM;
	}
	err = snd_ctl_add(chip->card, usb_audio.kctl);
	if (err < 0) {
		dev_err(&usb_audio.udev->dev,
			"USB_AUDIO_IPC : %s-ctl add error\n", __func__);
		return err;
	}
	usb_audio.chip = chip;

	return 0;
}

static int exynos_usb_audio_unmap_all(void)
{
	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	struct hcd_hw_info *hwinfo = g_hwinfo;
	u64 addr;
	u64 offset;
	int ret = 0;
	int err = 0;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : %s\n", __func__);

	/* unmapping in pcm buffer */
	addr = usb_audio.in_buf_addr;
	if (DEBUG)
		pr_info("PCM IN BUFFER FREE: paddr = %#08llx\n", addr);

	ret = usb_audio_iommu_unmap(USB_AUDIO_PCM_INBUF, PAGE_SIZE * 256);
	if (ret < 0) {
		pr_err("iommu un-mapping for in buf failed %d\n", ret);
		return ret;
	}

	addr = usb_audio.out_buf_addr;
	if (DEBUG)
		pr_info("PCM OUT BUFFER FREE: paddr = %#08llx\n", addr);

	ret = usb_audio_iommu_unmap(USB_AUDIO_PCM_OUTBUF, PAGE_SIZE * 256);
	if (ret < 0) {
		err = ret;
		pr_err("iommu unmapping for pcm out buf failed\n");
	}

	/* unmapping usb descriptor */
	addr = usb_audio.desc_addr;
	offset = usb_audio.offset;

	if (DEBUG)
		pr_info("DESC BUFFER: paddr : %#08llx / offset : %#08llx\n",
				addr, offset);

	ret = usb_audio_iommu_unmap(USB_AUDIO_DESC, PAGE_SIZE * 2);
	if (ret < 0) {
		err = ret;
		pr_err("iommu unmapping for descriptor failed\n");
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_SAVE_RESTORE, PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu unmapping for dcbaa failed\n");
	}

	/*Device Context unmapping*/
	ret = usb_audio_iommu_unmap(USB_AUDIO_DEV_CTX, PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu unmapping for device ctx failed\n");
	}

	/*Input Context unmapping*/
	ret = usb_audio_iommu_unmap(USB_AUDIO_INPUT_CTX, PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu unmapping for input ctx failed\n");
	}

	/*ERST unmapping*/
	ret = usb_audio_iommu_unmap(USB_AUDIO_ERST, PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu Un-mapping for erst failed\n");
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_IN_DEQ, PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err("abox iommu un-mapping for in buf failed %d\n", ret);
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_FBOUT_DEQ, PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err("abox iommu un-mapping for fb_out buf failed %d\n", ret);
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_OUT_DEQ, PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err("iommu un-mapping for out buf failed %d\n", ret);
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_FBIN_DEQ, PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err("iommu un-mapping for fb_in buf failed %d\n", ret);
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_XHCI_BASE, PAGE_SIZE * 16);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu Un-mapping for in buf failed\n");
	}

	if (hwinfo->use_uram) {
		ret = usb_audio_iommu_unmap(USB_URAM_BASE, USB_URAM_SIZE);
		if (ret < 0) {
			err = ret;
			pr_err(" abox iommu Un-mapping for in buf failed - URAM\n");
		}
	}

	return err;
}

int exynos_usb_audio_pcmbuf(struct usb_device *udev, int iface)
{

	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	struct hcd_hw_info *hwinfo = g_hwinfo;
	u64 out_dma = 0;
	int ret = 0;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s\n", __func__);

	if (!usb_audio.is_audio) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio is 0. return!\n");
		return -1;
	}

	mutex_lock(&usb_audio.lock);
	out_dma = iommu_iova_to_phys(iommu_domain, USB_AUDIO_PCM_OUTBUF);
	usb_msg.type = GIC_USB_PCM_BUF;
	usb_msg.param1 = lower_32_bits(le64_to_cpu(out_dma));
	usb_msg.param2 = upper_32_bits(le64_to_cpu(out_dma));
	usb_msg.param3 = lower_32_bits(le64_to_cpu(hwinfo->in_dma));
	usb_msg.param4 = upper_32_bits(le64_to_cpu(hwinfo->in_dma));

	if (DEBUG) {
		pr_info("pcm out data buffer pa addr : %#08x %08x\n",
				upper_32_bits(le64_to_cpu(out_dma)),
				lower_32_bits(le64_to_cpu(out_dma)));
		pr_info("pcm in data buffer pa addr : %#08x %08x\n",
				upper_32_bits(le64_to_cpu(hwinfo->in_dma)),
				lower_32_bits(le64_to_cpu(hwinfo->in_dma)));
		pr_info("erap param2 : %#08x param1 : %08x\n",
				usb_msg.param2, usb_msg.param1);
		pr_info("erap param4 : %#08x param3 : %08x\n",
				usb_msg.param4, usb_msg.param3);
	}

	ret = usb_msg_to_uram(dev);
	mutex_unlock(&usb_audio.lock);

	if (ret) {
		dev_err(&usb_audio.udev->dev, "erap usb transfer pcm buffer is failed\n");
		return ret; /* need to fix err num */
	}

	return 0;
}

int exynos_usb_audio_setintf(struct usb_device *udev, int iface, int alt, int direction)
{
	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	struct hcd_hw_info *hwinfo = g_hwinfo;
	int ret;
	u64 in_offset, out_offset;

	if (usb_audio.pcm_open_done == 0) {
		dev_info(dev, "USB_AUDIO_IPC : pcm node was not opened!\n");
		return -EPERM;
	}

	if (DEBUG)
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s, alt = %d\n",
				__func__, alt);

	if (usb_audio.is_audio == 0) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio is 0. return!\n");
		return -ENODEV;
	}

	if (alt == 0 && direction == SNDRV_PCM_STREAM_PLAYBACK) {
		if (usb_audio.pcm_open_done &&
				(!usb_audio.outdeq_map_done ||
				(hwinfo->out_deq != hwinfo->old_out_deq))) {
			dev_info(dev, "out_deq wait required\n");
			/* TODO: Wait for completion */
		}
	} else if (alt == 0 && direction == SNDRV_PCM_STREAM_CAPTURE) {
		if (usb_audio.pcm_open_done &&
				(!usb_audio.indeq_map_done ||
				(hwinfo->in_deq != hwinfo->old_in_deq))) {
			dev_info(dev, "in_deq wait required\n");
			/* TODO: Wait for completion */
		}
	}

	/* one more check whether usb audio is 0 or not */
	if (usb_audio.is_audio == 0) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio = 0. return!\n");
		return -ENODEV;
	}

	mutex_lock(&usb_audio.lock);
	usb_msg.type = GIC_USB_SET_INTF;
	usb_msg.param1 = alt;
	usb_msg.param2 = iface;

	if (direction) {
		/* IN EP */
		dev_dbg(dev, "in deq : %#08llx, %#08llx, %#08llx, %#08llx\n",
				hwinfo->in_deq, hwinfo->old_in_deq,
				hwinfo->fb_out_deq, hwinfo->fb_old_out_deq);
		if (!usb_audio.indeq_map_done ||
			(hwinfo->in_deq != hwinfo->old_in_deq)) {
			dev_info(dev, "in_deq map required\n");
			if (usb_audio.indeq_map_done) {
				/* Blocking SYSMMU Fault by ABOX */
				usb_msg.param1 = 0;
				ret = usb_msg_to_uram(dev);
				if (ret) {
					dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
					goto err;
				}
				usb_msg.param1 = alt;
				ret = usb_audio_iommu_unmap(USB_AUDIO_IN_DEQ, PAGE_SIZE);
				if (ret < 0) {
					pr_err("un-map of in buf failed %d\n", ret);
					goto err;
				}
			}
			usb_audio.indeq_map_done = 1;

			in_offset = hwinfo->in_deq % PAGE_SIZE;
			ret = usb_audio_iommu_map(USB_AUDIO_IN_DEQ,
					(hwinfo->in_deq - in_offset),
					PAGE_SIZE);
			if (ret < 0) {
				pr_err("map for in buf failed %d\n", ret);
				goto err;
			}
		}

		if (hwinfo->fb_out_deq) {
			dev_dbg(dev, "fb_in deq : %#08llx\n", hwinfo->fb_out_deq);
			if (!usb_audio.fb_outdeq_map_done ||
					(hwinfo->fb_out_deq != hwinfo->fb_old_out_deq)) {
				if (usb_audio.fb_outdeq_map_done) {
					/* Blocking SYSMMU Fault by ABOX */
					usb_msg.param1 = 0;
					ret = usb_msg_to_uram(dev);
					if (ret) {
						dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
						goto err;
					}
					usb_msg.param1 = alt;
					ret = usb_audio_iommu_unmap(USB_AUDIO_FBOUT_DEQ, PAGE_SIZE);
					if (ret < 0) {
						pr_err("un-map for fb_out buf failed %d\n", ret);
						goto err;
					}
				}
				usb_audio.fb_outdeq_map_done = 1;

				out_offset = hwinfo->fb_out_deq % PAGE_SIZE;
				ret = usb_audio_iommu_map(USB_AUDIO_FBOUT_DEQ,
						(hwinfo->fb_out_deq - out_offset),
						PAGE_SIZE);
				if (ret < 0) {
					pr_err("map for fb_out buf failed %d\n", ret);
					goto err;
				}
			}
		}
		usb_msg.param3 = lower_32_bits(le64_to_cpu(hwinfo->in_deq));
		usb_msg.param4 = upper_32_bits(le64_to_cpu(hwinfo->in_deq));
	} else {
		/* OUT EP */
		dev_dbg(dev, "out deq : %#08llx, %#08llx, %#08llx, %#08llx\n",
				hwinfo->out_deq, hwinfo->old_out_deq,
				hwinfo->fb_in_deq, hwinfo->fb_old_in_deq);
		if (!usb_audio.outdeq_map_done ||
			(hwinfo->out_deq != hwinfo->old_out_deq)) {
			dev_info(dev, "out_deq map required\n");
			if (usb_audio.outdeq_map_done) {
				/* Blocking SYSMMU Fault by ABOX */
				usb_msg.param1 = 0;
				ret = usb_msg_to_uram(dev);
				if (ret) {
					dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
					goto err;
				}
				usb_msg.param1 = alt;
				ret = usb_audio_iommu_unmap(USB_AUDIO_OUT_DEQ, PAGE_SIZE);
				if (ret < 0) {
					pr_err("un-map for out buf failed %d\n", ret);
					goto err;
				}
			}
			usb_audio.outdeq_map_done = 1;

			out_offset = hwinfo->out_deq % PAGE_SIZE;
			ret = usb_audio_iommu_map(USB_AUDIO_OUT_DEQ,
					(hwinfo->out_deq - out_offset),
					PAGE_SIZE);
			if (ret < 0) {
				pr_err("map for out buf failed %d\n", ret);
				goto err;
			}
		}

		if (hwinfo->fb_in_deq) {
			dev_info(dev, "fb_in deq : %#08llx\n", hwinfo->fb_in_deq);
			if (!usb_audio.fb_indeq_map_done ||
					(hwinfo->fb_in_deq != hwinfo->fb_old_in_deq)) {
				if (usb_audio.fb_indeq_map_done) {
					/* Blocking SYSMMU Fault by ABOX */
					usb_msg.param1 = 0;
					ret = usb_msg_to_uram(dev);
					if (ret) {
						dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
						goto err;
					}
					usb_msg.param1 = alt;
					ret = usb_audio_iommu_unmap(USB_AUDIO_FBIN_DEQ, PAGE_SIZE);
					if (ret < 0) {
						pr_err("un-map for fb_in buf failed %d\n", ret);
						goto err;
					}
				}
				usb_audio.fb_indeq_map_done = 1;

				in_offset = hwinfo->fb_in_deq % PAGE_SIZE;
				ret = usb_audio_iommu_map(USB_AUDIO_FBIN_DEQ,
						(hwinfo->fb_in_deq - in_offset),
						PAGE_SIZE);
				if (ret < 0) {
					pr_err("map for fb_in buf failed %d\n", ret);
					goto err;
				}
			}
		}

		usb_msg.param3 = lower_32_bits(le64_to_cpu(hwinfo->out_deq));
		usb_msg.param4 = upper_32_bits(le64_to_cpu(hwinfo->out_deq));
	}

	/* one more check connection to prevent kernel panic */
	if (!usb_audio.is_audio) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio is 0. return!\n");
		ret = -1;
		goto err;
	}

	ret = usb_msg_to_uram(dev);
	if (ret) {
		dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
		goto err;
	}
	mutex_unlock(&usb_audio.lock);

	if (DEBUG) {
		dev_info(&udev->dev, "Alt#%d / Intf#%d / Direction %s / EP DEQ : %#08x %08x\n",
				usb_msg.param1, usb_msg.param2,
				direction ? "IN" : "OUT",
				usb_msg.param4, usb_msg.param3);
	}

	return 0;

err:
	dev_err(&udev->dev, "%s error = %d\n", __func__, ret);
	mutex_unlock(&usb_audio.lock);
	return ret;
}

int exynos_usb_audio_init(struct device *dev, struct platform_device *pdev)
{
	struct device_node *np;
	struct device_node *np_abox;
	struct device_node *np_tmp;
	struct platform_device *pdev_abox;
	struct platform_device *pdev_tmp;
	struct device	*dev_gic;
	int	err = 0;

	if (!dev) {
		dev_err(dev, "%s - dev node is null\n", __func__);
		goto probe_failed;
	}

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : %s\n", __func__);

	np = dev->of_node;
	np_abox = of_parse_phandle(np, "abox", 0);
	if (!np_abox) {
		dev_err(dev, "Failed to get abox device node\n");
		goto probe_failed;
	}

	pdev_abox = of_find_device_by_node(np_abox);
	if (!pdev_abox) {
		dev_err(&usb_audio.udev->dev, "Failed to get abox platform device\n");
		goto probe_failed;
	}

	iommu_domain = iommu_get_domain_for_dev(&pdev_abox->dev);
	if (!iommu_domain) {
		dev_err(dev, "Failed to get iommu domain\n");
		goto probe_failed;
	}

	np_tmp = of_parse_phandle(np, "samsung,abox-gic", 0);
	if (!np_tmp) {
		dev_err(dev, "Failed to get abox-gic device node\n");
		goto probe_failed;
	}

	pdev_tmp = of_find_device_by_node(np_tmp);
	if (!pdev_tmp) {
		dev_err(dev, "Failed to get abox-gic platform device\n");
		goto probe_failed;
	}

	base_addr = ioremap(EXYNOS_URAM_ABOX_EMPTY_ADDR, EMPTY_SIZE);
	if (!base_addr) {
		dev_err(dev, "URAM ioremap failed\n");
		goto probe_failed;
	}

	dev_gic = &pdev_tmp->dev;
	gic_data = dev_get_drvdata(dev_gic);

	/* Get USB2.0 PHY for main hcd */
	usb_audio.phy = devm_phy_get(dev, "usb2-phy");
	if (IS_ERR_OR_NULL(usb_audio.phy)) {
		usb_audio.phy = NULL;
		dev_err(dev, "%s: failed to get phy\n", __func__);
	}

	mutex_init(&usb_audio.lock);
	mutex_init(&usb_audio.msg_lock);
	init_completion(&usb_audio.in_conn_stop);
	init_completion(&usb_audio.out_conn_stop);
	init_completion(&usb_audio.discon_done);
	usb_audio.abox = pdev_abox;
	usb_audio.hcd_pdev = pdev;
	usb_audio.udev = NULL;
	usb_audio.is_audio = 0;
	usb_audio.is_first_probe = 1;
	usb_audio.user_scenario = AUDIO_MODE_NORMAL;
	usb_audio.usb_audio_state = USB_AUDIO_DISCONNECT;
	usb_audio_connection = 0;

	//INIT_WORK(&usb_audio.usb_work, exynos_usb_audio_work);

	return 0;

probe_failed:
	err = -EPROBE_DEFER;

	return err;
}

/* card */
int exynos_usb_audio_connect(struct usb_interface *intf)
{
	struct usb_interface_descriptor *altsd;
	struct usb_host_interface *alts;
	struct usb_device *udev = interface_to_usbdev(intf);

	alts = &intf->altsetting[0];
	altsd = get_iface_desc(alts);

	if ((altsd->bInterfaceClass == USB_CLASS_AUDIO ||
		altsd->bInterfaceClass == USB_CLASS_VENDOR_SPEC) &&
		altsd->bInterfaceSubClass == USB_SUBCLASS_MIDISTREAMING) {
		pr_info("USB_AUDIO_IPC : %s - MIDI device detected!\n", __func__);
	} else {
		pr_info("USB_AUDIO_IPC : %s - No MIDI device detected!\n", __func__);

		if (usb_audio.usb_audio_state == USB_AUDIO_REMOVING) {
			/* wait for completion */
			if (usb_audio.usb_audio_state == USB_AUDIO_REMOVING) {
				usb_audio.usb_audio_state =
					USB_AUDIO_TIMEOUT_PROBE;
				pr_err("%s: timeout for disconnect\n", __func__);
			}
		}

		if ((usb_audio.usb_audio_state == USB_AUDIO_DISCONNECT)
			|| (usb_audio.usb_audio_state == USB_AUDIO_TIMEOUT_PROBE)) {
			pr_info("USB_AUDIO_IPC : %s - USB Audio set!\n", __func__);
			exynos_usb_audio_set_device(udev);
			exynos_usb_audio_hcd(udev);
			exynos_usb_audio_conn(udev, 1);
			exynos_usb_audio_desc(udev);
			exynos_usb_audio_map_buf(udev);
			if (udev->do_remote_wakeup)
				usb_enable_autosuspend(udev);
		} else {
			pr_err("USB audio is can not support second device!!");
			return -EPERM;
		}
	}

	return 0;
}

void exynos_usb_audio_disconn(struct usb_interface *intf)
{
	struct usb_device *udev = interface_to_usbdev(intf);

	exynos_usb_audio_conn(udev, 0);
}

/* clock */
int exynos_usb_audio_set_interface(struct usb_device *udev,
		struct usb_host_interface *alts, int iface, int alt)
{
	unsigned char ep;
	unsigned char numEndpoints;
	int direction;
	int i;

	if (alts != NULL) {
		numEndpoints = get_iface_desc(alts)->bNumEndpoints;
		if (numEndpoints < 1)
			return -22;
		if (numEndpoints == 1)
			ep = get_endpoint(alts, 0)->bEndpointAddress;
		else {
			for (i = 0; i < numEndpoints; i++) {
				ep = get_endpoint(alts, i)->bmAttributes;
				if (!(ep & 0x30)) {
					ep = get_endpoint(alts, i)->bEndpointAddress;
					break;
				}
			}
		}
		if (ep & USB_DIR_IN)
			direction = SNDRV_PCM_STREAM_CAPTURE;
		else
			direction = SNDRV_PCM_STREAM_PLAYBACK;

		exynos_usb_audio_setintf(udev, iface, alt, direction);

		pr_info("%s %d direction: %d\n", __func__, __LINE__, direction);
	}

	return 0;
}

int exynos_usb_audio_set_pcmintf(struct usb_interface *intf,
				int iface, int alt, int direction)
{
	struct usb_device *udev = interface_to_usbdev(intf);

	exynos_usb_audio_setintf(udev, iface, alt, direction);

	return 0;
}

/* pcm */
int exynos_usb_audio_set_rate(int iface, int rate, int alt)
{

	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	int ret;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s\n", __func__);

	if (!usb_audio.is_audio) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio is 0. return!\n");
		return -ENODEV;
	}

	mutex_lock(&usb_audio.lock);
	usb_msg.type = GIC_USB_SAMPLE_RATE;
	usb_msg.param1 = iface;
	usb_msg.param2 = rate;
	usb_msg.param3 = alt;

	ret = usb_msg_to_uram(dev);
	mutex_unlock(&usb_audio.lock);
	if (ret) {
		dev_err(&usb_audio.udev->dev, "erap usb transfer sample rate is failed\n");
		return ret;
	}

	return 0;
}

int exynos_usb_audio_pcm_control(struct usb_device *udev,
			enum snd_vendor_pcm_open_close onoff, int direction)
{
	if (onoff == 1) {
		exynos_usb_audio_pcm(1, direction);
	} else if (onoff == 0) {
		if (direction == SNDRV_PCM_STREAM_PLAYBACK)
			reinit_completion(&usb_audio.out_conn_stop);
		else if (direction == SNDRV_PCM_STREAM_CAPTURE)
			reinit_completion(&usb_audio.in_conn_stop);

		if (!usb_audio.pcm_open_done) {
			pr_info("%s : pcm node was not opened!\n", __func__);
			return 0;
		}
		exynos_usb_audio_pcm(0, direction);
	}

	return 0;
}

int exynos_usb_audio_add_control(struct snd_usb_audio *chip)
{
	int ret;

	if (chip != NULL) {
		exynos_usb_add_ctls(chip, 0);
		return 0;
	}

	ret = usb_audio.user_scenario;

	return ret;
}

int exynos_usb_audio_set_pcm_binterval(const struct audioformat *fp,
				 const struct audioformat *found,
				 int *cur_attr, int *attr)
{
	if (usb_audio.user_scenario >= AUDIO_MODE_IN_CALL) {
		if (fp->datainterval < found->datainterval) {
			pr_info("Chose smaller interval = %d, pre = %d\n",
					fp->datainterval, found->datainterval);
			found = fp;
			cur_attr = attr;
		}
	} else {
		if (fp->datainterval > found->datainterval) {
			pr_info("Chose bigger interval = %d, pre = %d\n",
					fp->datainterval, found->datainterval);
			found = fp;
			cur_attr = attr;
		}
	}

	pr_info("found interval = %d, mode = %d\n",
			found->datainterval, usb_audio.user_scenario);

	return 0;
}

int exynos_usb_audio_exit(void)
{
	/* future use */
	return 0;
}

MODULE_AUTHOR("Jaehun Jung <jh0801.jung@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Exynos USB Audio offloading driver");

