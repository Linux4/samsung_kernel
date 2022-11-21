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
#include <linux/io.h>//for ioremap
#include <linux/pid.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/device.h>
#endif

#include <soc/sprd/arch_misc.h>// get chip id
#include <video/ion_sprd.h>
//#include "gsp_drv.h"
#include "gsp_config_if.h"
#include "scaler_coef_cal.h"

#include "gsp_debug.h"



#ifdef CONFIG_OF
static struct device    *s_gsp_of_dev = NULL;
ulong   g_gsp_base_addr = 0;
ulong   g_gsp_mmu_ctrl_addr = 0;
#endif
gsp_context_t *g_gspCtx = NULL;// only used to debug when kernel crash, not used to pass parameters

static gsp_user* gsp_get_user(pid_t user_pid, gsp_context_t *gspCtx)
{
    gsp_user* ret_user = NULL;
    int i;

    for (i = 0; i < GSP_MAX_USER; i ++) {
        if ((gspCtx->gsp_user_array + i)->pid == user_pid) {
            ret_user = gspCtx->gsp_user_array + i;
            break;
        }
    }

    if (ret_user == NULL) {
        for (i = 0; i < GSP_MAX_USER; i ++) {
            if ((gspCtx->gsp_user_array + i)->pid == INVALID_USER_ID) {
                ret_user = gspCtx->gsp_user_array + i;
                ret_user->pid = user_pid;
                break;
            }
        }
    }

    return ret_user;
}


static int32_t gsp_drv_open(struct inode *node, struct file *file)
{
    int32_t ret = GSP_NO_ERR;
    gsp_user *pUserdata = NULL;
    struct miscdevice *miscdev = NULL;
    gsp_context_t *gspCtx = NULL;
    GSP_TRACE("gsp_drv_open:pid:0x%08x enter.\n",current->pid);

    miscdev = file->private_data;
    if (NULL == miscdev) {
        return -EINVAL;
    }
    gspCtx = container_of(miscdev, gsp_context_t , dev);
    if (NULL == gspCtx) {
        return -EINVAL;
    }

    pUserdata = (gsp_user *)gsp_get_user(current->pid, gspCtx);

    if (NULL == pUserdata) {
        printk("gsp_drv_open:pid:0x%08x user cnt full.\n",current->pid);
        ret = GSP_KERNEL_FULL;
        goto exit;
    }
    GSP_TRACE("gsp_drv_open:pid:0x%08x bf wait open sema.\n",current->pid);
    ret = down_interruptible(&pUserdata->sem_open);
    if(!ret) { //ret==0, wait success
        GSP_TRACE("gsp_drv_open:pid:0x%08x  wait open sema success.\n",current->pid);
        pUserdata->priv = (void*)gspCtx;
        file->private_data = pUserdata;
    } else { //ret == -EINTR
        ret = GSP_KERNEL_OPEN_INTR;
        printk("gsp_drv_open:pid:0x%08x  wait open sema failed.\n",current->pid);
    }

exit:
    return ret;
}


static int32_t gsp_drv_release(struct inode *node, struct file *file)
{
    gsp_user* pUserdata = file->private_data;

    GSP_TRACE("gsp_drv_release:pid:0x%08x.\n\n",current->pid);
    if(pUserdata == NULL) {
        printk("gsp_drv_release:error--pUserdata is null!, pid-0x%08x \n\n",current->pid);
        return -ENODEV;
    }
    pUserdata->pid = INVALID_USER_ID;
    sema_init(&pUserdata->sem_open, 1);
    /*
        //if caller thread hold gsp_hw_resource_sem,but was terminated,we release hw semaphore here
    if(gsp_cur_client_pid == current->pid)
    {
        GSP_Wait_Finish();//wait busy-bit down
        GSP_Deinit();
        //pUserdata->own_gsp_flag = 0;
        gsp_cur_client_pid = INVALID_USER_ID;
        sema_init(&gsp_wait_interrupt_sem,0);
        GSP_TRACE("%s:pid:0x%08x, release gsp-hw sema, L%d \n",__func__,pUserdata->pid,__LINE__);
        up(&gsp_hw_resource_sem);
    }
    */
    file->private_data = NULL;

    return GSP_NO_ERR;
}


ssize_t gsp_drv_write(struct file *file, const char __user * u_data, size_t cnt, loff_t *cnt_ret)
{
    gsp_user* pUserdata = file->private_data;

    if(pUserdata == NULL) {
        printk("%s:error--pUserdata is null!, pid-0x%08x \n\n",__func__,current->pid);
        return -ENODEV;
    }

    GSP_TRACE("gsp_drv_write:pid:0x%08x.\n",current->pid);

    pUserdata->is_exit_force = 1;
    /*
    nomater the target thread "pUserdata->pid" wait on done-sema or hw resource sema,
    send a signal to resume it and make it return from ioctl(),we does not up a sema to make target
    thread can distinguish GSP done and signal interrupt.
    */
    //send_sig(SIGABRT, (struct task_struct *)pUserdata->pid, 0);

    return 1;
}

ssize_t gsp_drv_read(struct file *file, char __user *u_data, size_t cnt, loff_t *cnt_ret)
{
    char rt_word[32]= {0};
    gsp_user* pUserdata = file->private_data;

    if(pUserdata == NULL) {
        printk("%s:error--pUserdata is null!, pid-0x%08x \n\n",__func__,current->pid);
        return -ENODEV;
    }

    *cnt_ret = 0;
    *cnt_ret += sprintf(rt_word + *cnt_ret, "gsp read %zd\n",cnt);
    return copy_to_user(u_data, (void*)rt_word, (ulong)*cnt_ret);
}


static void GSP_Coef_Tap_Convert(uint8_t h_tap,uint8_t v_tap, gsp_context_t *gspCtx)
{
    switch(h_tap) {
        case 8:
            gspCtx->gsp_cfg.layer0_info.row_tap_mode = 0;
            break;

        case 6:
            gspCtx->gsp_cfg.layer0_info.row_tap_mode = 1;
            break;

        case 4:
            gspCtx->gsp_cfg.layer0_info.row_tap_mode = 2;
            break;

        case 2:
            gspCtx->gsp_cfg.layer0_info.row_tap_mode = 3;
            break;

        default:
            gspCtx->gsp_cfg.layer0_info.row_tap_mode = 0;
            break;
    }

    switch(v_tap) {
        case 8:
            gspCtx->gsp_cfg.layer0_info.col_tap_mode = 0;
            break;

        case 6:
            gspCtx->gsp_cfg.layer0_info.col_tap_mode = 1;
            break;

        case 4:
            gspCtx->gsp_cfg.layer0_info.col_tap_mode = 2;
            break;

        case 2:
            gspCtx->gsp_cfg.layer0_info.col_tap_mode = 3;
            break;

        default:
            gspCtx->gsp_cfg.layer0_info.col_tap_mode = 0;
            break;
    }
    gspCtx->gsp_cfg.layer0_info.row_tap_mode &= 0x3;
    gspCtx->gsp_cfg.layer0_info.col_tap_mode &= 0x3;
}


static int32_t GSP_Scaling_Coef_Gen_And_Config(ulong* force_calc, gsp_context_t *gspCtx)
{
    uint8_t     h_tap = 8;
    uint8_t     v_tap = 8;
    uint32_t    *tmp_buf = NULL;
    uint32_t    *h_coeff = NULL;
    uint32_t    *v_coeff = NULL;
    uint32_t    coef_factor_w = 0;
    uint32_t    coef_factor_h = 0;
    uint32_t    after_rotate_w = 0;
    uint32_t    after_rotate_h = 0;
    uint32_t    coef_in_w = 0;
    uint32_t    coef_in_h = 0;
    uint32_t    coef_out_w = 0;
    uint32_t    coef_out_h = 0;
    static volatile uint32_t coef_in_w_last = 0;//if the new in w h out w h equal last params, don't need calc again
    static volatile uint32_t coef_in_h_last = 0;
    static volatile uint32_t coef_out_w_last = 0;
    static volatile uint32_t coef_out_h_last = 0;
    static volatile uint32_t coef_h_tap_last = 0;
    static volatile uint32_t coef_v_tap_last = 0;

    if(gspCtx->gsp_cfg.layer0_info.scaling_en == 1) {
        if(gspCtx->gsp_cfg.layer0_info.des_rect.rect_w < 4
           ||gspCtx->gsp_cfg.layer0_info.des_rect.rect_h < 4) {
            return GSP_KERNEL_GEN_OUT_RANG;
        }

        if(gspCtx->gsp_cfg.layer0_info.rot_angle == GSP_ROT_ANGLE_0
           ||gspCtx->gsp_cfg.layer0_info.rot_angle == GSP_ROT_ANGLE_180
           ||gspCtx->gsp_cfg.layer0_info.rot_angle == GSP_ROT_ANGLE_0_M
           ||gspCtx->gsp_cfg.layer0_info.rot_angle == GSP_ROT_ANGLE_180_M) {
            after_rotate_w = gspCtx->gsp_cfg.layer0_info.clip_rect.rect_w;
            after_rotate_h = gspCtx->gsp_cfg.layer0_info.clip_rect.rect_h;
        } else if(gspCtx->gsp_cfg.layer0_info.rot_angle == GSP_ROT_ANGLE_90
                  ||gspCtx->gsp_cfg.layer0_info.rot_angle == GSP_ROT_ANGLE_270
                  ||gspCtx->gsp_cfg.layer0_info.rot_angle == GSP_ROT_ANGLE_90_M
                  ||gspCtx->gsp_cfg.layer0_info.rot_angle == GSP_ROT_ANGLE_270_M) {
            after_rotate_w = gspCtx->gsp_cfg.layer0_info.clip_rect.rect_h;
            after_rotate_h = gspCtx->gsp_cfg.layer0_info.clip_rect.rect_w;
        }

        coef_factor_w = CEIL(after_rotate_w,gspCtx->gsp_cfg.layer0_info.des_rect.rect_w);
        coef_factor_h = CEIL(after_rotate_h,gspCtx->gsp_cfg.layer0_info.des_rect.rect_h);

        if(coef_factor_w > 16 || coef_factor_h > 16) {
            return GSP_KERNEL_GEN_OUT_RANG;
        }

        if(coef_factor_w > 8) {
            coef_factor_w = 4;
        } else if(coef_factor_w > 4) {
            coef_factor_w = 2;
        } else {
            coef_factor_w = 1;
        }

        if(coef_factor_h > 8) {
            coef_factor_h = 4;
        } else if(coef_factor_h > 4) {
            coef_factor_h = 2;
        } else {
            coef_factor_h= 1;
        }

        coef_in_w = CEIL(after_rotate_w,coef_factor_w);
        coef_in_h = CEIL(after_rotate_h,coef_factor_h);
        coef_out_w = gspCtx->gsp_cfg.layer0_info.des_rect.rect_w;
        coef_out_h = gspCtx->gsp_cfg.layer0_info.des_rect.rect_h;

        if(GSP_SRC_FMT_RGB565 < gspCtx->gsp_cfg.layer0_info.img_format
           && gspCtx->gsp_cfg.layer0_info.img_format < GSP_SRC_FMT_8BPP
           && (coef_in_w>coef_out_w||coef_in_h>coef_out_h)) { //video scaling down
            h_tap = 2;
            v_tap = 2;
            if(coef_in_w*3 <= coef_in_h*2) { // height is larger than 1.5*width
                v_tap = 4;
            }
            if(coef_in_h*3 <= coef_in_w*2) { // width is larger than 1.5*height
                h_tap = 4;
            }
            //GSP_TRACE("GSP, for video scaling down, we change tap to 2.\n");
        }

        //give hal a chance to set tap number
        if((gspCtx->gsp_cfg.layer0_info.row_tap_mode>0) || (gspCtx->gsp_cfg.layer0_info.col_tap_mode>0)) {
            //GSP_TRACE("GSP, hwc set tap: %dx%d-> ",h_tap,v_tap);
            h_tap = (gspCtx->gsp_cfg.layer0_info.row_tap_mode>0)?gspCtx->gsp_cfg.layer0_info.row_tap_mode:h_tap;
            v_tap = (gspCtx->gsp_cfg.layer0_info.col_tap_mode>0)?gspCtx->gsp_cfg.layer0_info.col_tap_mode:v_tap;
            //GSP_TRACE("%dx%d\n",h_tap,v_tap);
        }

        if(*force_calc == 1
           ||coef_in_w_last != coef_in_w
           || coef_in_h_last != coef_in_h
           || coef_out_w_last != coef_out_w
           || coef_out_h_last != coef_out_h
           || coef_h_tap_last != h_tap
           || coef_v_tap_last != v_tap) {
            tmp_buf = (uint32_t *)kmalloc(GSP_COEFF_BUF_SIZE, GFP_KERNEL);
            if (NULL == tmp_buf) {
                printk("SCALE DRV: No mem to alloc coeff buffer! \n");
                return GSP_KERNEL_GEN_ALLOC_ERR;
            }
            h_coeff = tmp_buf;
            v_coeff = tmp_buf + (GSP_COEFF_COEF_SIZE/4);

            if (!(GSP_Gen_Block_Ccaler_Coef(coef_in_w,
                                            coef_in_h,
                                            coef_out_w,
                                            coef_out_h,
                                            h_tap,
                                            v_tap,
                                            h_coeff,
                                            v_coeff,
                                            tmp_buf + (GSP_COEFF_COEF_SIZE/2),
                                            GSP_COEFF_POOL_SIZE, gspCtx))) {
                kfree(tmp_buf);
                printk("GSP DRV: GSP_Gen_Block_Ccaler_Coef error! \n");
                return GSP_KERNEL_GEN_COMMON_ERR;
            }
            GSP_Scale_Coef_Tab_Config(h_coeff,v_coeff);//write coef-metrix to register
            coef_in_w_last = coef_in_w;
            coef_in_h_last = coef_in_h;
            coef_out_w_last = coef_out_w;
            coef_out_h_last = coef_out_h;
            coef_h_tap_last = h_tap;
            coef_v_tap_last = v_tap;
            *force_calc = 0;
        }

        GSP_Coef_Tap_Convert(coef_h_tap_last,coef_v_tap_last, gspCtx);
        GSP_L0_SCALETAPMODE_SET(gspCtx->gsp_cfg.layer0_info.row_tap_mode,gspCtx->gsp_cfg.layer0_info.col_tap_mode);
        GSP_TRACE("GSP DRV: GSP_Gen_Block_Ccaler_Coef, register: r_tap%d,c_tap %d \n",
                  ((volatile GSP_REG_T*)GSP_REG_BASE)->gsp_layer0_cfg_u.mBits.row_tap_mod,
                  ((volatile GSP_REG_T*)GSP_REG_BASE)->gsp_layer0_cfg_u.mBits.col_tap_mod);//
        kfree(tmp_buf);
    }
    return GSP_NO_ERR;
}

/*
func:GSP_Info_Config
desc:config info to register
*/
static uint32_t GSP_Info_Config(gsp_context_t *gspCtx)
{
    GSP_ConfigLayer(GSP_MODULE_LAYER0,gspCtx);
    GSP_ConfigLayer(GSP_MODULE_LAYER1,gspCtx);
    GSP_ConfigLayer(GSP_MODULE_ID_MAX,gspCtx);
    GSP_ConfigLayer(GSP_MODULE_DST,gspCtx);
    return GSP_ERRCODE_GET();
}

/*
func:GSP_Cache_Flush
desc:
*/
static void GSP_Cache_Flush(void)
{

}

/*
func:GSP_Cache_Invalidate
desc:
*/
static void GSP_Cache_Invalidate(void)
{

}

/*
func:GSP_Release_HWSema
desc:
*/
static void GSP_Release_HWSema(gsp_context_t *gspCtx)
{
    gsp_user *pTempUserdata = NULL;

    GSP_TRACE("%s:pid:0x%08x, was killed without release GSP hw semaphore, L%d \n",__func__,gspCtx->gsp_cur_client_pid,__LINE__);

    pTempUserdata = (gsp_user *)gsp_get_user(gspCtx->gsp_cur_client_pid, gspCtx);
    pTempUserdata->pid = INVALID_USER_ID;
    sema_init(&pTempUserdata->sem_open, 1);

    GSP_Wait_Finish();//wait busy-bit down
    GSP_Deinit(gspCtx);
    gspCtx->gsp_cur_client_pid = INVALID_USER_ID;
    sema_init(&gspCtx->gsp_wait_interrupt_sem,0);
    up(&gspCtx->gsp_hw_resource_sem);
}

static int GSP_Map(gsp_context_t *gspCtx)
{
    struct ion_addr_data data;

    if(gspCtx->gsp_cfg.layer0_info.src_addr.addr_y == 0
       &&gspCtx->gsp_cfg.layer0_info.mem_info.share_fd) {
        data.fd_buffer = gspCtx->gsp_cfg.layer0_info.mem_info.share_fd;
        if(sprd_ion_get_gsp_addr(&data)) {
            printk("%s, L%d, error!\n",__func__,__LINE__);
            return -1;
        }
        if(data.iova_enabled)
            gspCtx->gsp_cfg.layer0_info.src_addr.addr_y = data.iova_addr;
        else
            gspCtx->gsp_cfg.layer0_info.src_addr.addr_y = data.phys_addr;
        gspCtx->gsp_cfg.layer0_info.src_addr.addr_uv = gspCtx->gsp_cfg.layer0_info.src_addr.addr_y + gspCtx->gsp_cfg.layer0_info.mem_info.uv_offset;
        gspCtx->gsp_cfg.layer0_info.src_addr.addr_v = gspCtx->gsp_cfg.layer0_info.src_addr.addr_y + gspCtx->gsp_cfg.layer0_info.mem_info.v_offset;
    }

    if(gspCtx->gsp_cfg.layer1_info.src_addr.addr_y == 0
       &&gspCtx->gsp_cfg.layer1_info.mem_info.share_fd) {
        data.fd_buffer = gspCtx->gsp_cfg.layer1_info.mem_info.share_fd;
        if(sprd_ion_get_gsp_addr(&data)) {
            printk("%s, L%d, error!\n",__func__,__LINE__);
            return -1;
        }
        if(data.iova_enabled)
            gspCtx->gsp_cfg.layer1_info.src_addr.addr_y = data.iova_addr;
        else
            gspCtx->gsp_cfg.layer1_info.src_addr.addr_y = data.phys_addr;
        gspCtx->gsp_cfg.layer1_info.src_addr.addr_uv = gspCtx->gsp_cfg.layer1_info.src_addr.addr_y + gspCtx->gsp_cfg.layer1_info.mem_info.uv_offset;
        gspCtx->gsp_cfg.layer1_info.src_addr.addr_v = gspCtx->gsp_cfg.layer1_info.src_addr.addr_y + gspCtx->gsp_cfg.layer1_info.mem_info.v_offset;

    }

    if(gspCtx->gsp_cfg.layer_des_info.src_addr.addr_y == 0
       &&gspCtx->gsp_cfg.layer_des_info.mem_info.share_fd) {
        data.fd_buffer = gspCtx->gsp_cfg.layer_des_info.mem_info.share_fd;
        if(sprd_ion_get_gsp_addr(&data)) {
            printk("%s, L%d, error!\n",__func__,__LINE__);
            return -1;
        }
        if(data.iova_enabled)
            gspCtx->gsp_cfg.layer_des_info.src_addr.addr_y = data.iova_addr;
        else
            gspCtx->gsp_cfg.layer_des_info.src_addr.addr_y = data.phys_addr;
        gspCtx->gsp_cfg.layer_des_info.src_addr.addr_uv = gspCtx->gsp_cfg.layer_des_info.src_addr.addr_y + gspCtx->gsp_cfg.layer_des_info.mem_info.uv_offset;
        gspCtx->gsp_cfg.layer_des_info.src_addr.addr_v = gspCtx->gsp_cfg.layer_des_info.src_addr.addr_y + gspCtx->gsp_cfg.layer_des_info.mem_info.v_offset;

    }

    return 0;
}

static int GSP_Unmap(gsp_context_t *gspCtx)
{
    if(gspCtx->gsp_cfg.layer0_info.mem_info.share_fd)
        sprd_ion_free_gsp_addr(gspCtx->gsp_cfg.layer0_info.mem_info.share_fd);

    if(gspCtx->gsp_cfg.layer1_info.mem_info.share_fd)
        sprd_ion_free_gsp_addr(gspCtx->gsp_cfg.layer1_info.mem_info.share_fd);

    if(gspCtx->gsp_cfg.layer_des_info.mem_info.share_fd)
        sprd_ion_free_gsp_addr(gspCtx->gsp_cfg.layer_des_info.mem_info.share_fd);

    return 0;
}

uint32_t  __attribute__((weak)) sci_get_chip_id(void)
{
    printk("GSP local read chip id, *(%p) == %x \n",(void*)(SPRD_AONAPB_BASE+0xFC),GSP_REG_READ(SPRD_AONAPB_BASE+0xFC));
    return GSP_REG_READ(SPRD_AONAPB_BASE+0xFC);
}

uint32_t  gsp_get_chip_id(void)
{
    uint32_t adie_chip_id = sci_get_chip_id();
    if((adie_chip_id & 0xffff0000) < 0x50000000) {
        printk("%s[%d]:warning, chip id 0x%08x is invalidate, try to get it by reading reg directly!\n", __func__, __LINE__ , adie_chip_id);
        adie_chip_id = GSP_REG_READ(SPRD_AONAPB_BASE+0xFC);
        if((adie_chip_id & 0xffff0000) < 0x50000000) {
            printk("%s[%d]:warning, chip id 0x%08x from reg is invalidate too!\n", __func__, __LINE__ , adie_chip_id);
        }
    }
    printk("%s[%d] return chip id 0x%08x \n", __func__, __LINE__, adie_chip_id);
    return adie_chip_id;
}

static GSP_ADDR_TYPE_E GSP_Get_Addr_Type(void)
{
    static volatile GSP_ADDR_TYPE_E s_gsp_addr_type = GSP_ADDR_TYPE_INVALUE;
    static uint32_t s_iommuCtlBugChipList[]= {0x7715a000,0x7715a001,0x8815a000}; //bug chip list
    //0x8730b000 tshark
    //0x8300a001 shark


#ifndef CONFIG_SPRD_IOMMU // shark or (dolphin/tshark not define IOMMU)
    s_gsp_addr_type = GSP_ADDR_TYPE_PHYSICAL;
#else // (dolphin/tshark defined IOMMU)
    if(s_gsp_addr_type == GSP_ADDR_TYPE_INVALUE) {
        uint32_t adie_chip_id = 0;
        int i = 0;
        /*set s_gsp_addr_type according to the chip id*/
        //adie_chip_id = sci_get_ana_chip_id();
        //printk("GSPa : get chip id :0x%08x \n", adie_chip_id);
        adie_chip_id = gsp_get_chip_id();

        if((adie_chip_id & 0xffff0000) > 0x50000000) {
            printk("GSP : get chip id :%08x is validate, scan bugchip list.\n", adie_chip_id);
            for (i=0; i<ARRAY_SIZE(s_iommuCtlBugChipList); i++) {
                if(s_iommuCtlBugChipList[i] == adie_chip_id) {
                    printk("GSP : match bug chip id :%08x == [%d]\n", adie_chip_id,i);
#ifdef GSP_IOMMU_WORKAROUND1
                    s_gsp_addr_type = GSP_ADDR_TYPE_IOVIRTUAL;
#else
                    s_gsp_addr_type = GSP_ADDR_TYPE_PHYSICAL;
#endif
                    break;
                }
            }
            if(s_gsp_addr_type == GSP_ADDR_TYPE_INVALUE) {
                printk("GSP : mismatch bug chip id.\n");
                s_gsp_addr_type = GSP_ADDR_TYPE_IOVIRTUAL;
                printk("dolphin tshark GSP : gsp address type :%d \n", s_gsp_addr_type);
            }
        } else {
            printk("GSP : get chip id :%08x is invalidate,set address type as physical.\n", adie_chip_id);
            s_gsp_addr_type = GSP_ADDR_TYPE_PHYSICAL;
        }
    }
#endif
    printk("GSP [%d]: gsp address type :%d ,\n", __LINE__, s_gsp_addr_type);
    return s_gsp_addr_type;
}




static GSP_CAPABILITY_T* GSP_Config_Capability(void)
{
    uint32_t adie_chip_id = 0;
    static GSP_CAPABILITY_T s_gsp_capability;

    if(s_gsp_capability.magic != CAPABILITY_MAGIC_NUMBER) {  // not initialized
        memset((void*)&s_gsp_capability,0,sizeof(s_gsp_capability));
        s_gsp_capability.max_layer_cnt = 1;
        s_gsp_capability.blend_video_with_OSD=0;
        s_gsp_capability.max_videoLayer_cnt = 1;
        s_gsp_capability.max_layer_cnt_with_video = 1;
        s_gsp_capability.scale_range_up=64;
        s_gsp_capability.scale_range_down=1;
        s_gsp_capability.scale_updown_sametime=0;
        s_gsp_capability.OSD_scaling=0;

        adie_chip_id = gsp_get_chip_id();
        switch(adie_chip_id&0xFFFF0000) {
            case 0x83000000:/*shark-0x8300a001 & 9620*/
                s_gsp_capability.version = 0x00;
                s_gsp_capability.scale_range_up=256;
                break;
            case 0x77150000:
            case 0x88150000:
                if(adie_chip_id == 0x7715a000
                   ||adie_chip_id == 0x7715a001
                   ||adie_chip_id == 0x8815a000) { /*dolphin iommu ctl reg access err*/
                    s_gsp_capability.version = 0x01;
                    s_gsp_capability.video_need_copy = 1;
                    s_gsp_capability.max_video_size = 1;
                } else if(adie_chip_id == 0x7715a002
                          ||adie_chip_id == 0x7715a003
                          ||adie_chip_id == 0x8815a001
                          ||adie_chip_id == 0x8815a002) { /*dolphin iommu ctl reg access ok, but with black line bug*/
                    s_gsp_capability.version = 0x02;
                    s_gsp_capability.video_need_copy = 1;
                    s_gsp_capability.max_video_size = 1;
                } else { /*adie_chip_id > 0x7715a003 || adie_chip_id > 0x8815a002, dolphin black line bug fixed*/
                    s_gsp_capability.version = 0x03;
                    s_gsp_capability.max_video_size = 1;
                    s_gsp_capability.scale_range_up=256;
                    printk("%s[%d]: info:a new chip id, treated as newest dolphin that without any bugs!\n",__func__,__LINE__);
                }
                break;
            case 0x87300000:
                if(adie_chip_id == 0x8730b000) { /*tshark, with black line bug*/
                    s_gsp_capability.version = 0x04;
                    s_gsp_capability.video_need_copy = 1;
                } else { /*tshark-0x8730b001 & tshark2-? & pike-?, black line bug fixed*/
                    s_gsp_capability.version = 0x05;
                    s_gsp_capability.max_layer_cnt = 2;
                    s_gsp_capability.scale_range_up=256;
                }
                break;
	#ifdef CONFIG_ARCH_SCX20
	    case 0x88600000:/*pike, same with tshark*/
                 s_gsp_capability.version = 0x05;
                 s_gsp_capability.max_layer_cnt = 2;
                 s_gsp_capability.scale_range_up=256;
		break;
	#endif
            default:
                if(adie_chip_id != 0x96300000/*SharkL, with YCbCr->RGB888*/
                   && adie_chip_id != 0x96310000/*SharkL64*/) {
                    /*
                    pikeL/sharkLT8 ...
                    after sharkL, gsp will not update any more,so these late-comers are same with sharkL.
                    */
                    printk("%s[%d]: info:a new chip id, be treated as sharkL!\n",__func__,__LINE__);
                }
                s_gsp_capability.version = 0x06;
                s_gsp_capability.blend_video_with_OSD=1;
                s_gsp_capability.max_layer_cnt_with_video = 3;
                s_gsp_capability.max_layer_cnt = 2;
                s_gsp_capability.scale_range_up=256;
                break;
        }
		#ifdef CONFIG_ARCH_SCX35LT8
			s_gsp_capability.buf_type_support=GSP_ADDR_TYPE_IOVIRTUAL;
		#else
			s_gsp_capability.buf_type_support=GSP_Get_Addr_Type();
		#endif
		#ifdef CONFIG_GSP_THREE_OVERLAY
			s_gsp_capability.max_layer_cnt = 3;
		#endif
        s_gsp_capability.yuv_xywh_even = 1;
        s_gsp_capability.crop_min.w=s_gsp_capability.crop_min.h=4;
        s_gsp_capability.out_min.w=s_gsp_capability.out_min.h=4;
        s_gsp_capability.crop_max.w=s_gsp_capability.crop_max.h=4095;
        s_gsp_capability.out_max.w=s_gsp_capability.out_max.h=4095;
        s_gsp_capability.magic = CAPABILITY_MAGIC_NUMBER;
    }
    return &s_gsp_capability;
}
static long gsp_drv_ioctl(struct file *file,
                          uint32_t cmd,
                          unsigned long arg)
{
    int32_t ret = -GSP_NO_ERR;
    size_t param_size = _IOC_SIZE(cmd);
    gsp_user* pUserdata = file->private_data;
    gsp_context_t *gspCtx = NULL;
    struct timespec start_time;
    struct timespec end_time;
    //long long cost=0;
    memset(&start_time,0,sizeof(start_time));
    memset(&end_time,0,sizeof(end_time));

    GSP_TRACE("%s:pid:0x%08x,cmd:0x%08x, io number 0x%x, param_size %zu \n",
              __func__,
              pUserdata->pid,
              cmd,
              _IOC_NR(cmd),
              param_size);

    if (NULL == pUserdata || NULL == pUserdata->priv) {
        return -EINVAL;
    }
    gspCtx = (gsp_context_t*)pUserdata->priv;
    if(gspCtx->suspend_resume_flag==1) {
        printk("%s[%d]: in suspend, ioctl just return!\n",__func__,__LINE__);
        return ret;
    }

    switch (cmd) {
        case GSP_IO_GET_CAPABILITY:
            if (param_size>=sizeof(GSP_CAPABILITY_T)) {
                GSP_CAPABILITY_T *cap=GSP_Config_Capability();
                if(arg & MEM_OPS_ADDR_ALIGN_MASK || (ulong)cap & MEM_OPS_ADDR_ALIGN_MASK) {
                    GSP_TRACE("%s[%d] copy_to_user use none 8B alignment address!",__func__,__LINE__);
                }
                ret=copy_to_user((void __user *)arg,(const void*)cap,sizeof(GSP_CAPABILITY_T));
                if(ret) {
                    printk("%s[%d] err:get gsp capability failed in copy_to_user !\n",__func__,__LINE__);
                    ret = GSP_KERNEL_COPY_ERR;
                    goto exit;
                }
                GSP_TRACE("%s[%d]: get gsp capability success in copy_to_user \n",__func__,__LINE__);
            } else {
                printk("%s[%d] err:get gsp capability, buffer is too small,come:%zu,need:%zu!",__func__, __LINE__,
                       param_size,sizeof(GSP_CAPABILITY_T));
                ret = GSP_KERNEL_COPY_ERR;
                goto exit;
            }
            break;

        case GSP_IO_SET_PARAM: {
            if (param_size) {
                GSP_TRACE("%s:pid:0x%08x, bf wait gsp-hw sema, L%d \n",__func__,pUserdata->pid,__LINE__);

                // the caller thread was killed without release GSP hw semaphore
                if( gspCtx->gsp_cur_client_pid != INVALID_USER_ID) {
                    struct pid * __pid = NULL;
                    struct task_struct *__task = NULL;
                    pid_t temp_pid = INVALID_USER_ID;

                    GSP_TRACE("%sL%d current:%08x store_pid:0x%08x, \n",__func__,__LINE__,current->pid, gspCtx->gsp_cur_client_pid);
                    //barrier();
                    temp_pid =  gspCtx->gsp_cur_client_pid;
                    __pid = find_get_pid(temp_pid);
                    if(__pid != NULL) {
                        __task = get_pid_task(__pid,PIDTYPE_PID);
                        //barrier();

                        if(__task != NULL) {
                            if(__task->pid !=  gspCtx->gsp_cur_client_pid) {
                                GSP_Release_HWSema(gspCtx);
                            }
                        } else {
                            GSP_Release_HWSema(gspCtx);
                        }
                    } else {
                        GSP_Release_HWSema(gspCtx);
                    }
                    //barrier();
                }


                ret = down_interruptible(&gspCtx->gsp_hw_resource_sem);
                if(ret) {
                    printk("%s:pid:0x%08x, wait gsp-hw sema interrupt by signal,return, L%d \n",__func__,pUserdata->pid,__LINE__);
                    //receive a signal
                    ret = GSP_KERNEL_CFG_INTR;
                    goto exit;
                }
                GSP_TRACE("%s:pid:0x%08x, wait gsp-hw sema success, L%d \n",__func__,pUserdata->pid,__LINE__);
                gspCtx->gsp_cur_client_pid = pUserdata->pid;
                if(arg & MEM_OPS_ADDR_ALIGN_MASK || (ulong)&gspCtx->gsp_cfg & MEM_OPS_ADDR_ALIGN_MASK) {
                    GSP_TRACE("%s[%d] copy_from_user use none 8B alignment address!",__func__,__LINE__);
                }
                ret=copy_from_user((void*)&gspCtx->gsp_cfg, (void*)arg, param_size);
                if(ret) {
                    printk("%s:pid:0x%08x, copy_params_from_user failed! \n",__func__,pUserdata->pid);
                    ret = GSP_KERNEL_COPY_ERR;
                    goto exit1;
                } else {
                    GSP_TRACE("%s:pid:0x%08x, copy_params_from_user success!, L%d \n",__func__,pUserdata->pid,__LINE__);
                    ret = GSP_Init(gspCtx);
                    if(ret) {
                        goto exit1;
                    }
                    // if the y u v address is virtual, should be converted to phy address here!!!
                    if(gspCtx->gsp_cfg.layer0_info.layer_en == 1) {
                        if(gspCtx->gsp_cfg.layer0_info.rot_angle & 0x1) { //90 270
                            if((gspCtx->gsp_cfg.layer0_info.clip_rect.rect_w != gspCtx->gsp_cfg.layer0_info.des_rect.rect_h) ||
                               (gspCtx->gsp_cfg.layer0_info.clip_rect.rect_h != gspCtx->gsp_cfg.layer0_info.des_rect.rect_w)) {
                                gspCtx->gsp_cfg.layer0_info.scaling_en = 1;
                            }
                        } else { // 0
                            if((gspCtx->gsp_cfg.layer0_info.clip_rect.rect_w != gspCtx->gsp_cfg.layer0_info.des_rect.rect_w) ||
                               (gspCtx->gsp_cfg.layer0_info.clip_rect.rect_h != gspCtx->gsp_cfg.layer0_info.des_rect.rect_h)) {
                                gspCtx->gsp_cfg.layer0_info.scaling_en = 1;
                            }
                        }
                    }

                    if(GSP_Map(gspCtx)) {
                        ret = GSP_KERNEL_ADDR_MAP_ERR;
                        goto exit3;
                    }

                    gspCtx->gsp_cfg.misc_info.gsp_clock = GSP_CLOCK;
                    if(GSP_GAP & 0x100) {
                        gspCtx->gsp_cfg.misc_info.gsp_gap = (GSP_GAP & 0xff);
                    }
                    ret = GSP_Info_Config(gspCtx);
                    GSP_TRACE("%s:pid:0x%08x, config hw %s!, L%d \n",__func__,pUserdata->pid,(ret>0)?"failed":"success",__LINE__);
                    if(ret) {
                        printk("%s%d:pid:0x%08x, gsp config err:%d, release hw sema.\n",__func__,__LINE__,pUserdata->pid,ret);
                        goto exit3;
                    }
                }
            }

            GSP_TRACE("%s:pid:0x%08x, in trigger to run , L%d \n",__func__,pUserdata->pid,__LINE__);
            if(gspCtx->gsp_cur_client_pid == pUserdata->pid) {
                GSP_TRACE("%s:pid:0x%08x, calc coef and trigger to run , L%d \n",__func__,pUserdata->pid,__LINE__);
                GSP_Cache_Flush();
                ret = GSP_Scaling_Coef_Gen_And_Config(&gspCtx->gsp_coef_force_calc, gspCtx);
                if(ret) {
                    goto exit3;
                }

                ret = GSP_Trigger();
                GSP_TRACE("%sL%d:pid:0x%08x, trigger %s!\n",__func__,__LINE__,pUserdata->pid,(ret)?"failed":"success");
                if(ret) {
                    printk("%s%d:pid:0x%08x, trigger failed!! err_code:%d \n",__func__,__LINE__,pUserdata->pid,ret);
                    printCfgInfo(gspCtx);
                    printGPSReg();
                    ERR_RECORD_ADD(*(GSP_REG_T *)GSP_REG_BASE);
                    ERR_RECORD_INDEX_ADD_WP();
                    if (ERR_RECORD_FULL()) {
                        ERR_RECORD_INDEX_ADD_RP();
                    }
                    GSP_TRACE("%s:pid:0x%08x, release hw sema, L%d \n",__func__,pUserdata->pid,__LINE__);
                    goto exit3;
                }
            } else {
                GSP_TRACE("%s:pid:0x%08x,exit L%d \n",__func__,pUserdata->pid,__LINE__);
                ret = GSP_KERNEL_CALLER_NOT_OWN_HW;
                goto exit3;
            }

            if(gspCtx->gsp_cur_client_pid == pUserdata->pid) {
                GSP_TRACE("%s:pid:0x%08x, bf wait done sema, L%d \n",__func__,pUserdata->pid,__LINE__);
                //ret = down_interruptible(&gsp_wait_interrupt_sem);//interrupt lose
                ret = down_timeout(&gspCtx->gsp_wait_interrupt_sem,msecs_to_jiffies(500));//for interrupt lose, timeout return -ETIME,
                if (ret == 0) { //gsp process over
                    GSP_TRACE("%s:pid:0x%08x, wait done sema success, L%d \n",__func__,pUserdata->pid,__LINE__);
                } else if (ret == -ETIME) {
                    printk("%s%d:pid:0x%08x, wait done sema 500-ms-timeout,it's abnormal!!!!!!!! \n",__func__,__LINE__,pUserdata->pid);
                    GPSTimeoutPrint(gspCtx);
                    ret = GSP_KERNEL_WAITDONE_TIMEOUT;
                } else if (ret) { // == -EINTR
                    printk("%s:pid:0x%08x, wait done sema interrupted by a signal, L%d \n",__func__,pUserdata->pid,__LINE__);
                    ret = GSP_KERNEL_WAITDONE_INTR;
                }

                if (pUserdata->is_exit_force) {
                    pUserdata->is_exit_force = 0;
                    ret = GSP_KERNEL_FORCE_EXIT;
                }

                GSP_Wait_Finish();//wait busy-bit down
                GSP_Unmap(gspCtx);
                GSP_Cache_Invalidate();
                GSP_Deinit(gspCtx);
                gspCtx->gsp_cur_client_pid = INVALID_USER_ID;
                sema_init(&gspCtx->gsp_wait_interrupt_sem,0);
                GSP_TRACE("%s:pid:0x%08x, release gsp-hw sema, L%d \n",__func__,pUserdata->pid,__LINE__);
                up(&gspCtx->gsp_hw_resource_sem);
            }
        }
        break;

        default:
            ret = GSP_KERNEL_CTL_CMD_ERR;
            break;
    }

    return ret;

exit3:
    GSP_Deinit(gspCtx);
//exit2:
    GSP_Unmap(gspCtx);
exit1:
    gspCtx->gsp_cur_client_pid = INVALID_USER_ID;
    up(&gspCtx->gsp_hw_resource_sem);
exit:
    if (ret) {
        printk("%s:pid:0x%08x, error code 0x%x \n", __func__,pUserdata->pid,ret);
    }
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
    gsp_context_t *gspCtx = (gsp_context_t *)dev_id;

    GSP_TRACE("%s enter!\n",__func__);

    if (NULL == gspCtx) {
        return IRQ_NONE;
    }
    GSP_IRQSTATUS_CLEAR();
    GSP_IRQENABLE_SET(GSP_IRQ_TYPE_DISABLE);
    up(&gspCtx->gsp_wait_interrupt_sem);

    return IRQ_HANDLED;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void gsp_early_suspend(struct early_suspend* es)
{
    gsp_context_t *gspCtx = NULL;
    int32_t ret = -GSP_NO_ERR;

    printk("%s%d\n",__func__,__LINE__);

    gspCtx = container_of(es, gsp_context_t, earlysuspend);

    gspCtx->suspend_resume_flag = 1;

    //in case of GSP is processing now, wait it finish and then disable
    ret = down_timeout(&gspCtx->gsp_hw_resource_sem,msecs_to_jiffies(500));
    if(ret) {
        printk("%s[%d]: wait gsp-hw sema failed, ret: %d\n",__func__,__LINE__,ret);
        GPSTimeoutPrint(gspCtx);
    } else {
        printk("%s[%d]: wait gsp-hw sema success. \n",__func__,__LINE__);
        up(&gspCtx->gsp_hw_resource_sem);
        //GSP_module_disable(gspCtx);
    }
}

static void gsp_late_resume(struct early_suspend* es)
{
    gsp_context_t *gspCtx = NULL;

    printk("%s%d\n",__func__,__LINE__);

    gspCtx = container_of(es, gsp_context_t, earlysuspend);

    gspCtx->gsp_coef_force_calc = 1;
    //GSP_module_enable(gspCtx);//

    GSP_AUTO_GATE_ENABLE();//bug 198152
    gspCtx->suspend_resume_flag = 0;
}

#else
static int gsp_suspend(struct platform_device *pdev,pm_message_t state)
{
    gsp_context_t *gspCtx = NULL;
    int32_t ret = -GSP_NO_ERR;
    printk("%s%d\n",__func__,__LINE__);

    gspCtx = platform_get_drvdata(pdev);
    if (NULL == gspCtx) {
        return -EINVAL;
    }

    gspCtx->suspend_resume_flag = 1;

    //in case of GSP is processing now, wait it finish and then disable
    ret = down_timeout(&gspCtx->gsp_hw_resource_sem,msecs_to_jiffies(500));
    if(ret) {
        printk("%s[%d]: wait gsp-hw sema failed, ret: %d\n",__func__,__LINE__,ret);
        GPSTimeoutPrint(gspCtx);
        return ret;
    } else {
        printk("%s[%d]: wait gsp-hw sema success. \n",__func__,__LINE__);
        up(&gspCtx->gsp_hw_resource_sem);
        //GSP_module_disable(gspCtx);
    }
    return 0;
}

static int gsp_resume(struct platform_device *pdev)
{
    gsp_context_t *gspCtx = NULL;

    gspCtx = platform_get_drvdata(pdev);
    if (NULL == gspCtx) {
        return -EINVAL;
    }

    printk("%s%d\n",__func__,__LINE__);
    gspCtx->gsp_coef_force_calc = 1;
    //GSP_module_enable(gspCtx);//

    GSP_AUTO_GATE_ENABLE();//bug 198152
    gspCtx->suspend_resume_flag = 0;
    return 0;
}

#endif


static int32_t gsp_clock_init(gsp_context_t *gspCtx)
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
        printk(KERN_ERR "gsp: get emc clk_parent failed!\n");
        return -1;
    } else {
        printk(KERN_INFO "gsp: get emc clk_parent ok!\n");//pr_debug
    }

#ifdef CONFIG_OF
    gspCtx->gsp_emc_clk = of_clk_get_by_name(s_gsp_of_dev->of_node, GSP_EMC_CLOCK_NAME);
#else
    gspCtx->gsp_emc_clk = clk_get(NULL, GSP_EMC_CLOCK_NAME);
#endif
    if (IS_ERR(gspCtx->gsp_emc_clk)) {
        printk(KERN_ERR "gsp: get emc clk failed!\n");
        return -1;
    } else {
        printk(KERN_INFO "gsp: get emc clk ok!\n");//pr_debug
    }

    ret = clk_set_parent(gspCtx->gsp_emc_clk, emc_clk_parent);
    if(ret) {
        printk(KERN_ERR "gsp: gsp set emc clk parent failed!\n");
        return -1;
    } else {
        printk(KERN_INFO "gsp: gsp set emc clk parent ok!\n");//pr_debug
    }

#ifdef CONFIG_OF
    gsp_clk_parent = of_clk_get_by_name(s_gsp_of_dev->of_node, GSP_CLOCK_PARENT3);
#else
    gsp_clk_parent = clk_get(NULL, GSP_CLOCK_PARENT3);
#endif
    if (IS_ERR(gsp_clk_parent)) {
        printk(KERN_ERR "gsp: get clk_parent failed!\n");
        return -1;
    } else {
        printk(KERN_INFO "gsp: get clk_parent ok!\n");
    }

#ifdef CONFIG_OF
    gspCtx->gsp_clk = of_clk_get_by_name(s_gsp_of_dev->of_node, GSP_CLOCK_NAME);
#else
    gspCtx->gsp_clk = clk_get(NULL, GSP_CLOCK_NAME);
#endif
    if (IS_ERR(gspCtx->gsp_clk)) {
        printk(KERN_ERR "gsp: get clk failed!\n");
        return -1;
    } else {
        printk(KERN_INFO "gsp: get clk ok!\n");
    }

    ret = clk_set_parent(gspCtx->gsp_clk, gsp_clk_parent);
    if(ret) {
        printk(KERN_ERR "gsp: gsp set clk parent failed!\n");
        return -1;
    } else {
        printk(KERN_INFO "gsp: gsp set clk parent ok!\n");
    }
    return ret;
}


int32_t gsp_drv_probe(struct platform_device *pdev)
{
    int32_t ret = 0;
    int32_t i = 0;
    gsp_context_t *gspCtx;

#ifdef CONFIG_OF
    struct resource r;
#endif

    gspCtx =kzalloc(sizeof(gsp_context_t), GFP_KERNEL);

    if (NULL == gspCtx) {
        dev_err(&pdev->dev, "Can't alloc memory for module data.\n");
        ret = -ENOMEM;
        goto ERROR_EXIT2;
    }

    GSP_TRACE("gsp_probe enter .\n");
    printk("%s,AHB clock :%d\n", __func__,GSP_AHB_CLOCK_GET());

#ifdef CONFIG_OF
    s_gsp_of_dev = &(pdev->dev);

    gspCtx->gsp_irq_num = irq_of_parse_and_map(s_gsp_of_dev->of_node, 0);

    if(0 != of_address_to_resource(s_gsp_of_dev->of_node, 0, &r)) {
        printk(KERN_ERR "gsp probe fail. (can't get register base address)\n");
        goto ERROR_EXIT2;
    }
    g_gsp_base_addr = (unsigned long)ioremap_nocache(r.start, resource_size(&r));
    if(!g_gsp_base_addr)
        BUG();

#ifndef GSP_IOMMU_WORKAROUND1
#if defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ARCH_SCX30G) || defined(CONFIG_ARCH_SCX35L)
    ret = of_property_read_u32(s_gsp_of_dev->of_node, "gsp_mmu_ctrl_base", (u32*)&g_gsp_mmu_ctrl_addr);

    if(0 != ret) {
        printk("%s: read gsp_mmu_ctrl_addr fail (%d)\n", __func__, ret);
        goto ERROR_EXIT2;
    }
    g_gsp_mmu_ctrl_addr = (ulong)ioremap_nocache(g_gsp_mmu_ctrl_addr,sizeof(uint32_t));
    if(!g_gsp_mmu_ctrl_addr)
        BUG();
#endif
#endif

    printk("gsp: irq = %d, g_gsp_base_addr = 0x%lx,\n", gspCtx->gsp_irq_num, g_gsp_base_addr);
#else
    gspCtx->gsp_irq_num = TB_GSP_INT;
#endif

    GSP_AUTO_GATE_ENABLE();
    ret = gsp_clock_init(gspCtx);
    if (ret) {
        printk(KERN_ERR "gsp emc clock init failed. \n");
        goto ERROR_EXIT2;
    }
    //GSP_module_enable(gspCtx);

    gspCtx->dev.minor = MISC_DYNAMIC_MINOR;
    gspCtx->dev.name = "sprd_gsp";
    gspCtx->dev.fops = &gsp_drv_fops;
    ret = misc_register(&gspCtx->dev);
    if (ret) {
        printk(KERN_ERR "gsp cannot register miscdev (%d)\n", ret);
        goto ERROR_EXIT2;
    }

    ret = request_irq(gspCtx->gsp_irq_num,//
                      gsp_irq_handler,
                      0,//IRQF_SHARED
                      "GSP",
                      gspCtx);

    if (ret) {
        printk("could not request irq %d\n", gspCtx->gsp_irq_num);
        goto ERROR_EXIT1;
    }

    gspCtx->gsp_cur_client_pid = INVALID_USER_ID;
    for (i=0; i<GSP_MAX_USER; i++) {
        gspCtx->gsp_user_array[i].pid = INVALID_USER_ID;
        gspCtx->gsp_user_array[i].is_exit_force = 0;
        sema_init(&(gspCtx->gsp_user_array[i].sem_open), 1);
    }

    /* initialize locks*/
    memset(&gspCtx->gsp_cfg,0,sizeof(gspCtx->gsp_cfg));
    sema_init(&gspCtx->gsp_hw_resource_sem, 1);
    sema_init(&gspCtx->gsp_wait_interrupt_sem, 0);
    /*initialize gsp_perf*/

    gspCtx->cache_coef_init_flag = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
    memset(& gspCtx->earlysuspend,0,sizeof( gspCtx->earlysuspend));
    gspCtx->earlysuspend.suspend = gsp_early_suspend;
    gspCtx->earlysuspend.resume  = gsp_late_resume;
    gspCtx->earlysuspend.level   = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
    register_early_suspend(&gspCtx->earlysuspend);
#endif

    platform_set_drvdata(pdev, gspCtx);
    g_gspCtx = gspCtx;
    return ret;

ERROR_EXIT1:
    misc_deregister(&gspCtx->dev);
ERROR_EXIT2:
    if (gspCtx) {
        kfree(gspCtx);
    }
    return ret;
}


static int32_t gsp_drv_remove(struct platform_device *dev)
{
    gsp_context_t *gspCtx;
    GSP_TRACE( "gsp_remove called !\n");

    gspCtx = platform_get_drvdata(dev);
    if (NULL == gspCtx) {
        return -EINVAL;
    }
    free_irq(gspCtx->gsp_irq_num, gsp_irq_handler);
    misc_deregister(&gspCtx->dev);
    kfree(gspCtx);
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
    printk("gsp_drv_init enter! \n");

    if (platform_driver_register(&gsp_driver) != 0) {
        printk("gsp platform driver register Failed! \n");
        return -1;
    } else {
        GSP_TRACE("gsp platform driver registered successful! \n");
    }
    return 0;
}


void gsp_drv_exit(void)
{
    platform_driver_unregister(&gsp_driver);
    GSP_TRACE("gsp platform driver unregistered! \n");
}

module_init(gsp_drv_init);
module_exit(gsp_drv_exit);

MODULE_DESCRIPTION("GSP Driver");
MODULE_LICENSE("GPL");

