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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/math64.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/io.h>/*for ioremap*/
#include <linux/pid.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/device.h>
#endif

#include <soc/sprd/arch_misc.h>/*get chip id*/
#include <video/ion_sprd.h>
#include "gsp_config_if.h"
#include "scaler_coef_cal.h"
#include "gsp_work_queue.h"
#include "gsp_debug.h"



#ifdef CONFIG_OF
static struct device    *s_gsp_of_dev = NULL;
#endif
struct gsp_context *g_gsp_ctx = NULL;// only used to debug when kernel crash, not used to pass parameters

static int32_t gsp_drv_open(struct inode *node, struct file *file)
{
    int32_t ret = 0;
    struct miscdevice *miscdev = NULL;
    struct gsp_context *ctx = NULL;
    GSP_DEBUG("gsp_drv_open:pid:0x%08x enter.\n",current->pid);

    miscdev = file->private_data;
    if (NULL == miscdev) {
        return -EINVAL;
    }
    ctx = container_of(miscdev, struct gsp_context , dev);
	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    if (ret) {
		GSP_ERR("get an error gsp context\n");
        return -EINVAL;
    }
    file->private_data = (void*)ctx;

    return ret;
}


static int32_t gsp_drv_release(struct inode *node, struct file *file)
{
    GSP_DEBUG("gsp_drv_release:pid:0x%08x.\n\n",current->pid);
    file->private_data = NULL;
    return 0;
}

static int atoi(const char *str)
{
    int value = 0;
    while(*str>='0' && *str<='9') {
        value *= 10;
        value += *str - '0';
        str++;
    }
    return value;
}

ssize_t gsp_drv_write(struct file *file, const char __user * u_data, size_t cnt, loff_t *cnt_ret)
{
   return 1;
}

ssize_t gsp_drv_read(struct file *file, char __user *u_data, size_t cnt, loff_t *cnt_ret)
{
    return 1; 
}

uint32_t  __attribute__((weak)) sci_get_chip_id(void)
{
    GSP_DEBUG("GSP local read chip id, *(%p) == %x \n",(void*)(SPRD_AONAPB_BASE+0xFC),GSP_REG_READ(SPRD_AONAPB_BASE+0xFC));
    return GSP_REG_READ(SPRD_AONAPB_BASE+0xFC);
}

uint32_t  gsp_get_chip_id(void)
{
    uint32_t adie_chip_id = sci_get_chip_id();
    if((adie_chip_id & 0xffff0000) < 0x50000000) {
        GSP_ERR("%s[%d]:warning, chip id 0x%08x is invalidate, try to get it by reading reg directly!\n",
				__func__, __LINE__ , adie_chip_id);
        adie_chip_id = GSP_REG_READ(SPRD_AONAPB_BASE+0xFC);
        if((adie_chip_id & 0xffff0000) < 0x50000000) {
            GSP_ERR("%s[%d]:warning, chip id 0x%08x from reg is invalidate too!\n", __func__, __LINE__ , adie_chip_id);
        }
    }
    GSP_DEBUG("%s[%d] return chip id 0x%08x \n", __func__, __LINE__, adie_chip_id);
    return adie_chip_id;
}

static struct gsp_capability* gsp_config_capability(struct gsp_context *ctx)
{
    uint32_t adie_chip_id = 0;
	struct gsp_capability *cap = &ctx->cap;	
    if (cap->magic != CAPABILITY_MAGIC_NUMBER) {  // not initialized
        memset((void*)cap, 0, sizeof(cap));
        cap->max_layer_cnt = 1;
        cap->blend_video_with_OSD = 0;
        cap->max_videoLayer_cnt = 1;
        cap->max_layer_cnt_with_video = 1;
        cap->scale_range_up = 64;
        cap->scale_range_down = 1;
        cap->scale_updown_sametime = 0;
        cap->OSD_scaling = 0;

        adie_chip_id = gsp_get_chip_id();
        switch(adie_chip_id&0xFFFF0000) {
            case 0x83000000:/*shark-0x8300a001 & 9620*/
                cap->version = 0x00;
                cap->scale_range_up=256;
                break;
            case 0x77150000:
            case 0x88150000:
                if(adie_chip_id == 0x7715a000
                   ||adie_chip_id == 0x7715a001
                   ||adie_chip_id == 0x8815a000) { /*dolphin iommu ctl reg access err*/
                    cap->version = 0x01;
                    cap->video_need_copy = 1;
                    cap->max_video_size = 1;
                } else if(adie_chip_id == 0x7715a002
                          ||adie_chip_id == 0x7715a003
                          ||adie_chip_id == 0x8815a001
                          ||adie_chip_id == 0x8815a002) { /*dolphin iommu ctl reg access ok, but with black line bug*/
                    cap->version = 0x02;
                    cap->video_need_copy = 1;
                    cap->max_video_size = 1;
                } else { /*adie_chip_id > 0x7715a003 || adie_chip_id > 0x8815a002, dolphin black line bug fixed*/
                    cap->version = 0x03;
                    cap->max_video_size = 1;
                    cap->scale_range_up = 256;
                    GSP_DEBUG("%s[%d]: info:a new chip id, treated as newest dolphin that"
							"without any bugs!\n",__func__,__LINE__);
                }
                break;
            case 0x87300000:
                if(adie_chip_id == 0x8730b000) { /*tshark, with black line bug*/
                    cap->version = 0x04;
                    cap->video_need_copy = 1;
                } else { /*tshark-0x8730b001 & tshark2-? & pike-?, black line bug fixed*/
			cap->version = 0x05;
			cap->max_layer_cnt = 2;
			cap->scale_range_up = 256;
			cap->blend_video_with_OSD = 1;
			cap->max_layer_cnt_with_video = 2;
		}
		break;
	#ifdef CONFIG_ARCH_SCX20
	    case 0x88600000:/*pike, same with tshark*/
                 cap->version = 0x05;
                 cap->max_layer_cnt = 2;
                 cap->scale_range_up = 256;
		break;
	#endif
            default:
                if(adie_chip_id != 0x96300000/*SharkL, with YCbCr->RGB888*/
                   && adie_chip_id != 0x96310000/*SharkL64*/) {
                    /*
                    pikeL/sharkLT8 ...
                    after sharkL, gsp will not update any more,so these late-comers are same with sharkL.
                    */
                    GSP_DEBUG("%s[%d]: info:a new chip id, be treated as sharkL!\n",
							__func__,__LINE__);
                }
                cap->version = 0x06;
                cap->blend_video_with_OSD = 1;
                cap->max_layer_cnt_with_video = 2;
                cap->max_layer_cnt = 2;
                cap->scale_range_up = 256;
                break;
        }

		cap->buf_type_support = GSP_ADDR_TYPE_IOVIRTUAL;
        cap->yuv_xywh_even = 1;
        cap->crop_min.w = cap->crop_min.h = 4;
        cap->out_min.w = cap->out_min.h = 4;
        cap->crop_max.w = cap->crop_max.h = 4095;
        cap->out_max.w = cap->out_max.h = 4095;
        cap->magic = CAPABILITY_MAGIC_NUMBER;
    }
    return cap;
}

int gsp_status_get(struct gsp_context *ctx)
{
	return ctx->status;
}

void gsp_status_set(struct gsp_context *ctx, int stat)
{
	ctx->status = stat;
}

static long gsp_drv_ioctl(struct file *file,
                          uint32_t cmd,
                          unsigned long arg)
{
    int32_t ret = -1;
    uint32_t ctl_code = _IOC_NR(cmd);
    uint32_t async_flag = (_IOC_TYPE(cmd)&GSP_IO_ASYNC_MASK)?1:0;
    size_t param_size = _IOC_SIZE(cmd);
    struct gsp_context *ctx = file->private_data;
	struct gsp_kcfg_info *kcfg = NULL;
    
	if (NULL == ctx) {
		GSP_ERR("ioctl with null context\n");
		return ret;
	}
	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    if (ret) {
		GSP_ERR("get an error gsp context\n");
		return ret;	
	}

    if (ctx->suspend_flag == 1) {
        GSP_ERR("%s[%d]: in suspend, ioctl just return!\n",__func__,__LINE__);
        return ret;
    }

    switch (ctl_code) {
        case GSP_GET_CAPABILITY:
            if (param_size >= sizeof(struct gsp_capability)) {
                struct gsp_capability *cap = gsp_config_capability(ctx);
                if(arg & MEM_OPS_ADDR_ALIGN_MASK || (unsigned long)cap & MEM_OPS_ADDR_ALIGN_MASK) {
                    GSP_ERR("%s[%d] copy_to_user use none 8B alignment address!",__func__,__LINE__);
                }
                ret = copy_to_user((void __user *)arg,(const void*)cap,sizeof(struct gsp_capability));
                if(ret) {
                    GSP_ERR("%s[%d] err:get gsp capability failed in copy_to_user !\n",__func__,__LINE__);
                    ret = GSP_KERNEL_COPY_ERR;
                    goto exit;
                }
                GSP_DEBUG("%s[%d]: get gsp capability success in copy_to_user \n",__func__,__LINE__);
            } else {
                GSP_ERR("%s[%d] err:get gsp capability, buffer is too small,come:%zu,need:%zu!",
						__func__, __LINE__, param_size, sizeof(struct gsp_capability));
                ret = GSP_KERNEL_COPY_ERR;
                goto exit;
            }
            break;

		case GSP_SET_PARAM: 
			GSP_DEBUG("set param, pid: %d\n", current->pid);
			gsp_work_thread_status_print(ctx);

			if (param_size < sizeof(struct gsp_cfg_info)) {
				GSP_ERR("ioctl param size error\n");
				ret = -EINVAL;
				goto exit;
			}

			if (ctx->frame_id < UINT_MAX)
				ctx->frame_id++;
			else 
				ctx->frame_id = 0;

			if (ctx->suspend_flag) {
				GSP_ERR("no need to get kcfg at suspend\n");
				goto exit;
			}
			/*get an empty kcfg from work queue*/
			kcfg = gsp_work_queue_get_empty_kcfg(ctx->wq, ctx);
			if (kcfg == NULL) {
				GSP_ERR("get empty kcfg failed at frame: %u\n",
						ctx->frame_id);
				print_work_queue_status(ctx->wq);
				if (ctx->suspend_flag)
					GSP_ERR("suspend flag: %d\n", ctx->suspend_flag);
				else
					gsp_timeout_print(ctx);
				goto exit;
			}

			ret = gsp_kcfg_fill((struct gsp_cfg_info __user *)arg,
					kcfg, ctx->timeline, ctx->frame_id);
			if (ret) {
				GSP_ERR("kcfg fill failed at frame: %u\n",
						ctx->frame_id);
				gsp_work_queue_put_back_kcfg(kcfg, ctx->wq);
				goto exit;
			}

			/*push filled kcfg into work queue*/
			ret = gsp_work_queue_push_kcfg(ctx->wq, kcfg);
			if (ret) {
				GSP_ERR("work queue push failed\n");
				goto destroy;
			}

			/*wake gsp work thread to process layers*/
			up(&ctx->wake_sema);
			if (async_flag) {
				ret = 0;
				break;
			}

			/*wait at sync case */
			ret = wait_event_interruptible_timeout(ctx->sync_wait,
					(kcfg->done > 0 || ctx->suspend_flag),
					msecs_to_jiffies(3000));
			if (ret <= 0) { /*ret<0:interrupted by singal ; ret==0: timeout;*/
				GSP_ERR("pid:0x%08x wait frame done failed! ret:%d,suspend:%d\n",
						current->pid, ret, ctx->suspend_flag);
				gsp_timeout_print(ctx);
				goto destroy;
			} else if (kcfg->done !=1 ) {
				GSP_ERR("process failed\n");
				ret = -1;
			} else {
				GSP_DEBUG("wait done success\n");
				ret = 0;
			}

			break;
		default:
			ret = GSP_KERNEL_CTL_CMD_ERR;
			break;
	}
	return ret;

destroy:
	gsp_kcfg_destroy(kcfg);
exit:
	return ret;
}

static struct file_operations gsp_drv_fops = {
    .owner          = THIS_MODULE,
    .open           = gsp_drv_open,
    .write          = gsp_drv_write,
    .read           = gsp_drv_read,
    .unlocked_ioctl = gsp_drv_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = gsp_drv_ioctl,
#endif
    .release        = gsp_drv_release
};

static irqreturn_t gsp_irq_handler(int32_t irq, void *dev_id)
{
    struct gsp_context *ctx = (struct gsp_context *)dev_id;
	struct gsp_kcfg_info *kcfg = NULL;
	unsigned long reg_addr = 0;
	int ret = -1;
    GSP_DEBUG("%s enter!\n",__func__);

	if (NULL == ctx) {
		GSP_ERR("ISR with null context\n");
		return IRQ_NONE;
	}

	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    if (ret) {
		GSP_ERR("get an error gsp contexti at ISR\n");
        return IRQ_NONE;
    }
	reg_addr = ctx->gsp_reg_addr;
    GSP_IRQSTATUS_CLEAR(reg_addr);
    GSP_IRQENABLE_SET(GSP_IRQ_TYPE_DISABLE, reg_addr);
	/*set kcfg status to indicate that ISR hase handled kcfg*/
	kcfg = ctx->current_kcfg;	
	if (kcfg != NULL) {
		if (kcfg->tag > GSP_EMPTY_MAX
				|| kcfg->tag < 0)
			GSP_ERR("get empty kcfg tag error\n");
	}

	gsp_status_set(ctx, GSP_STATUS_ISR_DONE);
	up(&ctx->wake_sema);

    return IRQ_HANDLED;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void gsp_early_suspend(struct early_suspend* es)
{
    struct gsp_context *ctx = NULL;
    int32_t ret = -1;

    GSP_DEBUG("%s%d\n", __func__, __LINE__);

    ctx = container_of(es, struct gsp_context, earlysuspend);
	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    if (ret) {
		GSP_ERR("get an error gsp context\n");
        return;
    }

    ctx->suspend_flag = 1;
    up(&ctx->wake_sema);
    wake_up_interruptible_all(&ctx->wq->empty_wait);
    wake_up_interruptible_all(&ctx->sync_wait);

    /*in case of GSP is processing now, wait it finish and then disable*/
	ret = down_timeout(&ctx->suspend_clean_done_sema, msecs_to_jiffies(3000));
    if(ret)
        GSP_ERR("%s[%d]: wait suspend clean done sema failed, ret: %d\n",
				__func__, __LINE__, ret);
    else
        GSP_DEBUG("%s[%d]: wait suspend clean done sema success. \n",
				__func__, __LINE__);
}

static void gsp_late_resume(struct early_suspend* es)
{
    struct gsp_context *ctx = NULL;
	int ret = -1;

    GSP_DEBUG("%s%d\n",__func__,__LINE__);

    ctx = container_of(es, struct gsp_context, earlysuspend);
	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    if (ret) {
		GSP_ERR("get an error gsp context\n");
        return;
    }

    ctx->gsp_coef_force_calc = 1;

    GSP_AUTO_GATE_ENABLE();//bug 198152
    ctx->suspend_flag = 0;
}

#else
static int gsp_suspend(struct platform_device *pdev,
		pm_message_t state)
{
    struct gsp_context *ctx = NULL;
    int32_t ret = -GSP_NO_ERR;
    GSP_DEBUG("%s%d\n",__func__,__LINE__);

    ctx = platform_get_drvdata(pdev);
	if (NULL == ctx) {
		GSP_ERR("suspend with null context\n");
		return -EINVAL;
	}
	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    if (ret) {
		GSP_ERR("get an error gsp context at suspend\n");
        return -EINVAL;
    }

    ctx->suspend_flag = 1;

    /*in case of GSP is processing now, wait it finish and then disable*/
    ret = down_timeout(&ctx->suspend_clean_done_sema, msecs_to_jiffies(3000));
    if(ret)
        GSP_ERR("%s[%d]: wait suspend clean done sema failed, ret: %d\n",
				__func__, __LINE__, ret);
    else
        GSP_DEBUG("%s[%d]: wait suspend clean done sema success. \n",
				__func__, __LINE__);
	return ret;
}

static int gsp_resume(struct platform_device *pdev)
{
    struct gsp_context *ctx = NULL;
	int ret = -1;

    ctx = platform_get_drvdata(pdev);
	if (NULL == ctx) {
		GSP_ERR("suspend with null context\n");
		return -EINVAL;
	}
	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    if (ret) {
		GSP_ERR("get an error gsp context\n");
        return -EINVAL;
    }

    GSP_DEBUG("%s%d\n",__func__,__LINE__);
    ctx->gsp_coef_force_calc = 1;
    //GSP_module_enable(ctx);//

    GSP_AUTO_GATE_ENABLE();//bug 198152
    ctx->suspend_flag = 0;
    return 0;
}

#endif


static int32_t gsp_clock_init(struct gsp_context *ctx)
{
    struct clk *emc_clk_parent = NULL;
    struct clk *gsp_clk_parent = NULL;
    int ret = 0;

#ifdef CONFIG_OF
    emc_clk_parent = of_clk_get_by_name(s_gsp_of_dev->of_node, GSP_EMC_CLOCK_PARENT_NAME);
#else
    emc_clk_parent = clk_get(NULL, GSP_EMC_CLOCK_PARENT_NAME);
#endif
    if (IS_ERR(emc_clk_parent)) {
        GSP_ERR("gsp: get emc clk_parent failed!\n");
        return -1;
    } else {
        GSP_DEBUG("gsp: get emc clk_parent ok!\n");
    }

#ifdef CONFIG_OF
    ctx->gsp_emc_clk = of_clk_get_by_name(s_gsp_of_dev->of_node, GSP_EMC_CLOCK_NAME);
#else
    ctx->gsp_emc_clk = clk_get(NULL, GSP_EMC_CLOCK_NAME);
#endif
    if (IS_ERR(ctx->gsp_emc_clk)) {
        GSP_ERR("gsp: get emc clk failed!\n");
        return -1;
    } else {
        GSP_DEBUG("gsp: get emc clk ok!\n");
    }

    ret = clk_set_parent(ctx->gsp_emc_clk, emc_clk_parent);
    if(ret) {
        GSP_ERR("gsp: gsp set emc clk parent failed!\n");
        return -1;
    } else {
        GSP_DEBUG("gsp: gsp set emc clk parent ok!\n");
    }

#ifdef CONFIG_OF
    gsp_clk_parent = of_clk_get_by_name(s_gsp_of_dev->of_node, GSP_CLOCK_PARENT3);
#else
    gsp_clk_parent = clk_get(NULL, GSP_CLOCK_PARENT3);
#endif
    if (IS_ERR(gsp_clk_parent)) {
        GSP_ERR("gsp: get clk_parent failed!\n");
        return -1;
    } else {
        GSP_DEBUG("gsp: get clk_parent ok!\n");
    }

#ifdef CONFIG_OF
    ctx->gsp_clk = of_clk_get_by_name(s_gsp_of_dev->of_node, GSP_CLOCK_NAME);
#else
    ctx->gsp_clk = clk_get(NULL, GSP_CLOCK_NAME);
#endif
    if (IS_ERR(ctx->gsp_clk)) {
        GSP_ERR("gsp: get clk failed!\n");
        return -1;
    } else {
        GSP_DEBUG("gsp: get clk ok!\n");
    }

    ret = clk_set_parent(ctx->gsp_clk, gsp_clk_parent);
    if (ret) {
        GSP_ERR("gsp: gsp set clk parent failed!\n");
        return -1;
    } else {
        GSP_DEBUG("gsp: gsp set clk parent ok!\n");
    }
    return ret;
}

int32_t gsp_drv_probe(struct platform_device *pdev)
{
    int32_t ret = 0;
    struct gsp_context *ctx;
	struct gsp_sync_timeline * tl = NULL;
	struct task_struct *thr = NULL;
    struct resource r;

    ctx =kzalloc(sizeof(struct gsp_context), GFP_KERNEL);

    if (NULL == ctx) {
        dev_err(&pdev->dev, "Can't alloc memory for module data.\n");
        ret = -ENOMEM;
    }

    GSP_DEBUG("gsp_probe enter .\n");
    GSP_DEBUG("%s,AHB clock :%d\n", __func__,GSP_AHB_CLOCK_GET());

	strlcpy(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    s_gsp_of_dev = &(pdev->dev);

    ctx->gsp_irq_num = irq_of_parse_and_map(s_gsp_of_dev->of_node, 0);

    if (0 != of_address_to_resource(s_gsp_of_dev->of_node, 0, &r)) {
        GSP_ERR("gsp probe fail. (can't get register base address)\n");
        goto exit;
    }
    ctx->gsp_reg_addr = (unsigned long)ioremap_nocache(r.start, resource_size(&r));
    if(ctx->gsp_reg_addr == 0)
        BUG();

    ret = of_property_read_u32(s_gsp_of_dev->of_node, "gsp_mmu_ctrl_base", (u32*)&ctx->mmu_reg_addr);

    if(0 != ret) {
        GSP_ERR("%s: read gsp_mmu_ctrl_addr fail (%d)\n", __func__, ret);
        return ret;
    }
    ctx->mmu_reg_addr = (unsigned long)ioremap_nocache(ctx->mmu_reg_addr, sizeof(uint32_t));
    if(ctx->mmu_reg_addr == 0)
        BUG();

    GSP_DEBUG("gsp: irq = %d, gsp reg addr = 0x%lx,\n",
			ctx->gsp_irq_num, ctx->gsp_reg_addr);

    GSP_AUTO_GATE_ENABLE();
    ret = gsp_clock_init(ctx);
    if (ret) {
        GSP_ERR("gsp emc clock init failed. \n");
        goto exit;
    }

    ctx->dev.minor = MISC_DYNAMIC_MINOR;
    ctx->dev.name = "sprd_gsp";
    ctx->dev.fops = &gsp_drv_fops;
    ret = misc_register(&ctx->dev);
    if (ret) {
        GSP_ERR("gsp cannot register miscdev (%d)\n", ret);
        goto exit;
    }

    ret = request_irq(ctx->gsp_irq_num,
                      gsp_irq_handler,
                      0,/*IRQF_SHARED*/
                      "GSP",
                      ctx);

    if (ret) {
        GSP_ERR("could not request irq %d\n", ctx->gsp_irq_num);
        goto deregister;
    }

    ctx->cache_coef_init_flag = 0;
	ctx->frame_id = 0;
	/*create the timeline*/
	tl = gsp_sync_timeline_create("gsp_timeline");
	if (tl == NULL) {
		GSP_ERR("gsp timeline create failed\n");
		goto deregister;
	} else {
		ctx->timeline = tl;
		GSP_DEBUG("gsp timeline create success\n");
	}

	init_waitqueue_head(&ctx->sync_wait);

	ctx->wq = kzalloc(sizeof(struct gsp_work_queue), GFP_KERNEL);
	if (ctx->wq == NULL) {
		GSP_ERR("gsp allocate wq memory failed\n");
		goto destroy;
	}
	memset(ctx->wq, 0, sizeof(struct gsp_work_queue));
	gsp_work_queue_init(ctx->wq);
#ifdef CONFIG_HAS_EARLYSUSPEND
    memset(& ctx->earlysuspend, 0, sizeof( ctx->earlysuspend));
	INIT_LIST_HEAD(&ctx->earlysuspend.link);
    ctx->earlysuspend.suspend = gsp_early_suspend;
    ctx->earlysuspend.resume  = gsp_late_resume;
    ctx->earlysuspend.level   = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
    register_early_suspend(&ctx->earlysuspend);
#endif

    platform_set_drvdata(pdev, ctx);
    g_gsp_ctx = ctx;
	sema_init(&ctx->wake_sema, 0);
	sema_init(&ctx->suspend_clean_done_sema, 0);
	thr = kthread_run(gsp_work_thread, ctx, "gsp_work_thread");
	if (thr == NULL) {
		GSP_ERR("gsp work thread create failed\n");
		goto destroy;
	} else {
		GSP_DEBUG("gsp work thread create success\n");	
		ctx->work_thread = thr;
	}
    return ret;

destroy:
	gsp_sync_timeline_destroy(ctx->timeline);
deregister:
    misc_deregister(&ctx->dev);
exit:
    return ret;
}

static int32_t gsp_drv_remove(struct platform_device *dev)
{
    struct gsp_context *ctx;
	int ret = -1;
    GSP_DEBUG( "gsp_remove called !\n");

    ctx = platform_get_drvdata(dev);
	ret = strncmp(ctx->name, GSP_CONTEXT_NAME, sizeof(ctx->name));
    if (ret) {
		GSP_ERR("get an error gsp context\n");
        return -EINVAL;
    }
    free_irq(ctx->gsp_irq_num, gsp_irq_handler);
    misc_deregister(&ctx->dev);
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sprdgsp_dt_ids[] = {
    { .compatible = "sprd,gsp", },
    {}
};
#endif

static struct platform_driver gsp_driver = {
    .probe = gsp_drv_probe,
    .remove = gsp_drv_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend = gsp_suspend,
    .resume = gsp_resume,
#endif
    .driver =
    {
        .owner = THIS_MODULE,
        .name = "sprd_gsp",
#ifdef CONFIG_OF
        .of_match_table = of_match_ptr(sprdgsp_dt_ids),
#endif
    }
};

int32_t __init gsp_drv_init(void)
{
    GSP_DEBUG("gsp_drv_init enter! \n");

    if (platform_driver_register(&gsp_driver) != 0) {
        GSP_ERR("gsp platform driver register Failed! \n");
        return -1;
    } else {
        GSP_DEBUG("gsp platform driver registered successful! \n");
    }
    return 0;
}


void gsp_drv_exit(void)
{
    platform_driver_unregister(&gsp_driver);
    GSP_DEBUG("gsp platform driver unregistered! \n");
}

module_init(gsp_drv_init);
module_exit(gsp_drv_exit);

MODULE_DESCRIPTION("GSP Driver");
MODULE_LICENSE("GPL");

