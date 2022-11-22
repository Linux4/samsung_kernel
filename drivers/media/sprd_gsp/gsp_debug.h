
#ifndef __GSP_DEBUG_IF_H_
#define __GSP_DEBUG_IF_H_

#include "gsp_drv_common.h"


#define ERR_RECORD_INDEX_ADD(v) (v = ((v+1)&(GSP_ERR_RECORD_CNT-1)))
#define ERR_RECORD_INDEX_ADD_RP()   ERR_RECORD_INDEX_ADD(gspCtx->gsp_reg_err_record_rp)
#define ERR_RECORD_INDEX_ADD_WP()   ERR_RECORD_INDEX_ADD(gspCtx->gsp_reg_err_record_wp)
#define ERR_RECORD_INDEX_GET_RP()   (gspCtx->gsp_reg_err_record_rp)
#define ERR_RECORD_INDEX_GET_WP()   (gspCtx->gsp_reg_err_record_wp)
#define ERR_RECORD_ADD(v)   (gspCtx->gsp_reg_err_record[gspCtx->gsp_reg_err_record_wp] = (v))
#define ERR_RECORD_GET(v)   (&gspCtx->gsp_reg_err_record[gspCtx->gsp_reg_err_record_rp])
#define ERR_RECORD_EMPTY()  (gspCtx->gsp_reg_err_record_rp == gspCtx->gsp_reg_err_record_wp)
#define ERR_RECORD_FULL()   (((gspCtx->gsp_reg_err_record_wp+1)&(GSP_ERR_RECORD_CNT-1)) == gspCtx->gsp_reg_err_record_rp)


#ifndef GSP_DEBUG
#define GSP_GAP                                 0
#define GSP_CLOCK                           3
#else
static ulong gsp_gap = 0;
module_param(gsp_gap, ulong, 0644);//S_IRUGO|S_IWUGO
MODULE_PARM_DESC(gsp_gap, "gsp ddr gap(0~255)");

static uint gsp_clock = 3;
module_param(gsp_clock, uint, 0644);
MODULE_PARM_DESC(gsp_clock, "gsp clock(0:96M 1:153.6M 2:192M 3:256M)");

#define GSP_GAP                                 gsp_gap
#define GSP_CLOCK                           gsp_clock
#endif

#ifdef GSP_DEBUG
#define GSP_TRACE   printk
#else
#define GSP_TRACE   pr_debug
#endif


static void printCfgInfo(gsp_context_t *gspCtx)// gspDrvdata
{
    GSP_CONFIG_INFO_T               gsp_cfg;

    if (NULL == gspCtx) {
        return;
    }

    gsp_cfg = gspCtx->gsp_cfg;

    printk("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& \n");
    printk( "misc: ahb_clock %d|gsp_clock %d|dithering_en %d|gsp_gap %d\n",
            gsp_cfg.misc_info.ahb_clock,
            gsp_cfg.misc_info.gsp_clock,
            gsp_cfg.misc_info.dithering_en,
            gsp_cfg.misc_info.gsp_gap);

    printk( "L0: format:%d,pitch:%d,clip(x:%d,y:%d,w:%d,h:%d) =rot:%d=> des(x:%d,y:%d,w:%d,h:%d)\n",
            gsp_cfg.layer0_info.img_format,
            gsp_cfg.layer0_info.pitch,
            gsp_cfg.layer0_info.clip_rect.st_x,
            gsp_cfg.layer0_info.clip_rect.st_y,
            gsp_cfg.layer0_info.clip_rect.rect_w,
            gsp_cfg.layer0_info.clip_rect.rect_h,
            gsp_cfg.layer0_info.rot_angle,
            gsp_cfg.layer0_info.des_rect.st_x,
            gsp_cfg.layer0_info.des_rect.st_y,
            gsp_cfg.layer0_info.des_rect.rect_w,
            gsp_cfg.layer0_info.des_rect.rect_h);
    printk( "L0: alpha:%d,colorkey_en:%d,pallet_en:%d,scaling_en:%d,layer_en:%d,pmargb_en:%d,pmargb_mod:%d\n",
            gsp_cfg.layer0_info.alpha,
            gsp_cfg.layer0_info.colorkey_en,
            gsp_cfg.layer0_info.pallet_en,
            gsp_cfg.layer0_info.scaling_en,
            gsp_cfg.layer0_info.layer_en,
            gsp_cfg.layer0_info.pmargb_en,
            gsp_cfg.layer0_info.pmargb_mod);
    printk( "L0: col_tap_mode:%d,row_tap_mode:%d,a_swap_mode:%d,rgb_swap_mode:%d,va_word_endn:%d,va_lng_wrd_endn:%d,uv_word_endn:%d,uv_lng_wrd_endn:%d,y_word_endn:%d,y_lng_wrd_endn:%d\n",
            gsp_cfg.layer0_info.col_tap_mode,
            gsp_cfg.layer0_info.row_tap_mode,
            gsp_cfg.layer0_info.endian_mode.a_swap_mode,
            gsp_cfg.layer0_info.endian_mode.rgb_swap_mode,
            gsp_cfg.layer0_info.endian_mode.va_word_endn,
            gsp_cfg.layer0_info.endian_mode.va_lng_wrd_endn,
            gsp_cfg.layer0_info.endian_mode.uv_word_endn,
            gsp_cfg.layer0_info.endian_mode.uv_lng_wrd_endn,
            gsp_cfg.layer0_info.endian_mode.y_word_endn,
            gsp_cfg.layer0_info.endian_mode.y_lng_wrd_endn);
    printk( "L0: addr_y:0x%08x,addr_uv:0x%08x,addr_v:0x%08x,(grey r:%d,g:%d,b:%d,a:%d),(colorkey r:%d,g:%d,b:%d,a:%d)\n",
            gsp_cfg.layer0_info.src_addr.addr_y,
            gsp_cfg.layer0_info.src_addr.addr_uv,
            gsp_cfg.layer0_info.src_addr.addr_v,
            gsp_cfg.layer0_info.grey.r_val,
            gsp_cfg.layer0_info.grey.g_val,
            gsp_cfg.layer0_info.grey.b_val,
            gsp_cfg.layer0_info.grey.a_val,
            gsp_cfg.layer0_info.colorkey.r_val,
            gsp_cfg.layer0_info.colorkey.g_val,
            gsp_cfg.layer0_info.colorkey.b_val,
            gsp_cfg.layer0_info.colorkey.a_val);


    printk( "L1: format:%d,pitch:%d,clip(x:%d,y:%d,w:%d,h:%d) =rot:%d=> des(x:%d,y:%d)\n",
            gsp_cfg.layer1_info.img_format,
            gsp_cfg.layer1_info.pitch,
            gsp_cfg.layer1_info.clip_rect.st_x,
            gsp_cfg.layer1_info.clip_rect.st_y,
            gsp_cfg.layer1_info.clip_rect.rect_w,
            gsp_cfg.layer1_info.clip_rect.rect_h,
            gsp_cfg.layer1_info.rot_angle,
            gsp_cfg.layer1_info.des_pos.pos_pt_x,
            gsp_cfg.layer1_info.des_pos.pos_pt_y);
    printk( "L1: alpha:%d,colorkey_en:%d,pallet_en:%d,layer_en:%d,pmargb_en:%d,pmargb_mod:%d\n",
            gsp_cfg.layer1_info.alpha,
            gsp_cfg.layer1_info.colorkey_en,
            gsp_cfg.layer1_info.pallet_en,
            gsp_cfg.layer1_info.layer_en,
            gsp_cfg.layer1_info.pmargb_en,
            gsp_cfg.layer1_info.pmargb_mod);

    printk( "L1: col_tap_mode:%d,row_tap_mode:%d,a_swap_mode:%d,rgb_swap_mode:%d,va_word_endn:%d,va_lng_wrd_endn:%d,uv_word_endn:%d,uv_lng_wrd_endn:%d,y_word_endn:%d,y_lng_wrd_endn:%d\n",
            gsp_cfg.layer1_info.col_tap_mode,
            gsp_cfg.layer1_info.row_tap_mode,
            gsp_cfg.layer1_info.endian_mode.a_swap_mode,
            gsp_cfg.layer1_info.endian_mode.rgb_swap_mode,
            gsp_cfg.layer1_info.endian_mode.va_word_endn,
            gsp_cfg.layer1_info.endian_mode.va_lng_wrd_endn,
            gsp_cfg.layer1_info.endian_mode.uv_word_endn,
            gsp_cfg.layer1_info.endian_mode.uv_lng_wrd_endn,
            gsp_cfg.layer1_info.endian_mode.y_word_endn,
            gsp_cfg.layer1_info.endian_mode.y_lng_wrd_endn);
    printk( "L1: addr_y:0x%08x,addr_uv:0x%08x,addr_v:0x%08x,(grey r:%d,g:%d,b:%d,a:%d),(colorkey r:%d,g:%d,b:%d,a:%d)\n",
            gsp_cfg.layer1_info.src_addr.addr_y,
            gsp_cfg.layer1_info.src_addr.addr_uv,
            gsp_cfg.layer1_info.src_addr.addr_v,
            gsp_cfg.layer1_info.grey.r_val,
            gsp_cfg.layer1_info.grey.g_val,
            gsp_cfg.layer1_info.grey.b_val,
            gsp_cfg.layer1_info.grey.a_val,
            gsp_cfg.layer1_info.colorkey.r_val,
            gsp_cfg.layer1_info.colorkey.g_val,
            gsp_cfg.layer1_info.colorkey.b_val,
            gsp_cfg.layer1_info.colorkey.a_val);


    printk( "Ld cfg:fmt:%d|pitch %04d|cmpr8 %d\n",
            gsp_cfg.layer_des_info.img_format,
            gsp_cfg.layer_des_info.pitch,
            gsp_cfg.layer_des_info.compress_r8_en);
    printk( "Ld Yaddr 0x%08x|Uaddr 0x%08x|Vaddr 0x%08x\n",
            gsp_cfg.layer_des_info.src_addr.addr_y,
            gsp_cfg.layer_des_info.src_addr.addr_uv,
            gsp_cfg.layer_des_info.src_addr.addr_v);
    printk( "Ld:a_swap_mode:%d,rgb_swap_mode:%d,va_word_endn:%d,va_lng_wrd_endn:%d,uv_word_endn:%d,uv_lng_wrd_endn:%d,y_word_endn:%d,y_lng_wrd_endn:%d\n",
            gsp_cfg.layer_des_info.endian_mode.a_swap_mode,
            gsp_cfg.layer_des_info.endian_mode.rgb_swap_mode,
            gsp_cfg.layer_des_info.endian_mode.va_word_endn,
            gsp_cfg.layer_des_info.endian_mode.va_lng_wrd_endn,
            gsp_cfg.layer_des_info.endian_mode.uv_word_endn,
            gsp_cfg.layer_des_info.endian_mode.uv_lng_wrd_endn,
            gsp_cfg.layer_des_info.endian_mode.y_word_endn,
            gsp_cfg.layer_des_info.endian_mode.y_lng_wrd_endn);
    printk("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& \n");
}


static void printGPSReg(void)
{
    GSP_REG_T *g_gsp_reg = (GSP_REG_T *)GSP_REG_BASE;
    uint32_t i = 0;

    printk("********************************************* \n");
    printk("GSP_EN			0x%08x: 0x%08x; 0x%08x: 0x%08x; 0x%08x: 0x%08x;\n",
           (0X20D00000),GSP_REG_READ(GSP_MOD_EN),
           (0X20D01000),GSP_REG_READ(GSP_MOD_EN+0x1000),
           (0X20D02000),GSP_REG_READ(GSP_MOD_EN+0x1000));//
    printk("GSP_RESET		0x%08x: 0x%08x; 0x%08x: 0x%08x; 0x%08x: 0x%08x;\n",
           (0X20D00004),GSP_REG_READ(GSP_SOFT_RESET),
           (0X20D01004),GSP_REG_READ(GSP_SOFT_RESET+0x1000),
           (0X20D02004),GSP_REG_READ(GSP_SOFT_RESET+0x1000));//
    printk("AUTO_GATE		0x%08x: 0x%08x; 0x%08x: 0x%08x; 0x%08x: 0x%08x;\n",
           (0X20D00040),GSP_REG_READ(GSP_AUTO_GATE_ENABLE_BASE),
           (0X20D01040),GSP_REG_READ(GSP_AUTO_GATE_ENABLE_BASE+0x1000),
           (0X20D02040),GSP_REG_READ(GSP_AUTO_GATE_ENABLE_BASE+0x1000));//
    printk("GSP_CLOCK		0x%08x: 0x%08x \n",(0X71200028),GSP_REG_READ(GSP_CLOCK_BASE));
    printk("AHB_CLOCK		0x%08x: 0x%08x \n",(0X71200020),GSP_REG_READ(GSP_AHB_CLOCK_BASE));
    printk("EMC_EN			0x%08x: 0x%08x \n",(0X402E0004),GSP_REG_READ(GSP_EMC_MATRIX_BASE));

    while(i < 0x198) {
        printk("0x%08x: 0x%08x \n",(0x20a00000+i),GSP_REG_READ(GSP_REG_BASE+i));
        i += 4;
    }
    printk( "misc: run %d|busy %d|errflag %d|errcode %02d|dither %d|pmmod0 %d|pmmod1 %d|pmen %d|scale %d|reserv2 %d|scl_stat_clr %d|l0en %d|l1en %d|rb %d\n",
            g_gsp_reg->gsp_cfg_u.mBits.gsp_run,
            g_gsp_reg->gsp_cfg_u.mBits.gsp_busy,
            g_gsp_reg->gsp_cfg_u.mBits.error_flag,
            g_gsp_reg->gsp_cfg_u.mBits.error_code,
            g_gsp_reg->gsp_cfg_u.mBits.dither_en,
            g_gsp_reg->gsp_cfg_u.mBits.pmargb_mod0,
            g_gsp_reg->gsp_cfg_u.mBits.pmargb_mod1,
            g_gsp_reg->gsp_cfg_u.mBits.pmargb_en,
            g_gsp_reg->gsp_cfg_u.mBits.scale_en,
            g_gsp_reg->gsp_cfg_u.mBits.y2r_opt,
            g_gsp_reg->gsp_cfg_u.mBits.scale_status_clr,
            g_gsp_reg->gsp_cfg_u.mBits.l0_en,
            g_gsp_reg->gsp_cfg_u.mBits.l1_en,
            g_gsp_reg->gsp_cfg_u.mBits.dist_rb);
    printk( "misc: inten %d|intmod %d|intclr %d\n",
            g_gsp_reg->gsp_int_cfg_u.mBits.int_en,
            g_gsp_reg->gsp_int_cfg_u.mBits.int_mod,
            g_gsp_reg->gsp_int_cfg_u.mBits.int_clr);


    printk( "L0 cfg:fmt %d|rot %d|ck %d|pallet %d|rowtap %d|coltap %d\n",
            g_gsp_reg->gsp_layer0_cfg_u.mBits.img_format_l0,
            g_gsp_reg->gsp_layer0_cfg_u.mBits.rot_mod_l0,
            g_gsp_reg->gsp_layer0_cfg_u.mBits.ck_en_l0,
            g_gsp_reg->gsp_layer0_cfg_u.mBits.pallet_en_l0,
            g_gsp_reg->gsp_layer0_cfg_u.mBits.row_tap_mod,
            g_gsp_reg->gsp_layer0_cfg_u.mBits.col_tap_mod);
    printk( "L0 blockalpha %03d, pitch %04d,(%04d,%04d)%04dx%04d => (%04d,%04d)%04dx%04d\n",
            g_gsp_reg->gsp_layer0_alpha_u.mBits.alpha_l0,
            g_gsp_reg->gsp_layer0_pitch_u.mBits.pitch0,
            g_gsp_reg->gsp_layer0_clip_start_u.mBits.clip_start_x_l0,
            g_gsp_reg->gsp_layer0_clip_start_u.mBits.clip_start_y_l0,
            g_gsp_reg->gsp_layer0_clip_size_u.mBits.clip_size_x_l0,
            g_gsp_reg->gsp_layer0_clip_size_u.mBits.clip_size_y_l0,
            g_gsp_reg->gsp_layer0_des_start_u.mBits.des_start_x_l0,
            g_gsp_reg->gsp_layer0_des_start_u.mBits.des_start_y_l0,
            g_gsp_reg->gsp_layer0_des_size_u.mBits.des_size_x_l0,
            g_gsp_reg->gsp_layer0_des_size_u.mBits.des_size_y_l0);
    printk( "L0 Yaddr 0x%08x|Uaddr 0x%08x|Vaddr 0x%08x\n",
            g_gsp_reg->gsp_layer0_y_addr_u.dwValue,
            g_gsp_reg->gsp_layer0_uv_addr_u.dwValue,
            g_gsp_reg->gsp_layer0_va_addr_u.dwValue);
    printk( "L0 grey(%03d,%03d,%03d) colorkey(%03d,%03d,%03d)\n",
            g_gsp_reg->gsp_layer0_grey_rgb_u.mBits.layer0_grey_r,
            g_gsp_reg->gsp_layer0_grey_rgb_u.mBits.layer0_grey_g,
            g_gsp_reg->gsp_layer0_grey_rgb_u.mBits.layer0_grey_b,
            g_gsp_reg->gsp_layer0_ck_u.mBits.ck_r_l0,
            g_gsp_reg->gsp_layer0_ck_u.mBits.ck_g_l0,
            g_gsp_reg->gsp_layer0_ck_u.mBits.ck_b_l0);
    printk( "L0 endian: y %d|u %d|v %d|rgb %d|alpha %d\n",
            g_gsp_reg->gsp_layer0_endian_u.mBits.y_endian_mod_l0,
            g_gsp_reg->gsp_layer0_endian_u.mBits.uv_endian_mod_l0,
            g_gsp_reg->gsp_layer0_endian_u.mBits.va_endian_mod_l0,
            g_gsp_reg->gsp_layer0_endian_u.mBits.rgb_swap_mod_l0,
            g_gsp_reg->gsp_layer0_endian_u.mBits.a_swap_mod_l0);


    printk( "L1 cfg:fmt %d|rot %d|ck %d|pallet %d\n",
            g_gsp_reg->gsp_layer1_cfg_u.mBits.img_format_l1,
            g_gsp_reg->gsp_layer1_cfg_u.mBits.rot_mod_l1,
            g_gsp_reg->gsp_layer1_cfg_u.mBits.ck_en_l1,
            g_gsp_reg->gsp_layer1_cfg_u.mBits.pallet_en_l1);
    printk( "L1 blockalpha %03d, pitch %04d,(%04d,%04d)%04dx%04d => (%04d,%04d)\n",
            g_gsp_reg->gsp_layer1_alpha_u.mBits.alpha_l1,
            g_gsp_reg->gsp_layer1_pitch_u.mBits.pitch1,
            g_gsp_reg->gsp_layer1_clip_start_u.mBits.clip_start_x_l1,
            g_gsp_reg->gsp_layer1_clip_start_u.mBits.clip_start_y_l1,
            g_gsp_reg->gsp_layer1_clip_size_u.mBits.clip_size_x_l1,
            g_gsp_reg->gsp_layer1_clip_size_u.mBits.clip_size_y_l1,
            g_gsp_reg->gsp_layer1_des_start_u.mBits.des_start_x_l1,
            g_gsp_reg->gsp_layer1_des_start_u.mBits.des_start_y_l1);
    printk( "L1 Yaddr 0x%08x|Uaddr 0x%08x|Vaddr 0x%08x\n",
            g_gsp_reg->gsp_layer1_y_addr_u.dwValue,
            g_gsp_reg->gsp_layer1_uv_addr_u.dwValue,
            g_gsp_reg->gsp_layer1_va_addr_u.dwValue);
    printk( "L1 grey(%03d,%03d,%03d) colorkey(%03d,%03d,%03d)\n",
            g_gsp_reg->gsp_layer1_grey_rgb_u.mBits.grey_r_l1,
            g_gsp_reg->gsp_layer1_grey_rgb_u.mBits.grey_g_l1,
            g_gsp_reg->gsp_layer1_grey_rgb_u.mBits.grey_b_l1,
            g_gsp_reg->gsp_layer1_ck_u.mBits.ck_r_l1,
            g_gsp_reg->gsp_layer1_ck_u.mBits.ck_g_l1,
            g_gsp_reg->gsp_layer1_ck_u.mBits.ck_b_l1);
    printk( "L1 endian: y %d|u %d|v %d|rgb %d|alpha %d\n",
            g_gsp_reg->gsp_layer1_endian_u.mBits.y_endian_mod_l1,
            g_gsp_reg->gsp_layer1_endian_u.mBits.uv_endian_mod_l1,
            g_gsp_reg->gsp_layer1_endian_u.mBits.va_endian_mod_l1,
            g_gsp_reg->gsp_layer1_endian_u.mBits.rgb_swap_mod_l1,
            g_gsp_reg->gsp_layer1_endian_u.mBits.a_swap_mod_l1);


    printk( "Ld cfg:fmt %d|cmpr8 %d|pitch %04d\n",
            g_gsp_reg->gsp_des_data_cfg_u.mBits.des_img_format,
            g_gsp_reg->gsp_des_data_cfg_u.mBits.compress_r8,
            g_gsp_reg->gsp_des_pitch_u.mBits.des_pitch);
    printk( "Ld Yaddr 0x%08x|Uaddr 0x%08x|Vaddr 0x%08x\n",
            g_gsp_reg->gsp_des_y_addr_u.dwValue,
            g_gsp_reg->gsp_des_uv_addr_u.dwValue,
            g_gsp_reg->gsp_des_v_addr_u.dwValue);
    printk( "Ld endian: y %d|u %d|v %d|rgb %d|alpha %d\n",
            g_gsp_reg->gsp_des_data_endian_u.mBits.y_endian_mod,
            g_gsp_reg->gsp_des_data_endian_u.mBits.uv_endian_mod,
            g_gsp_reg->gsp_des_data_endian_u.mBits.v_endian_mod,
            g_gsp_reg->gsp_des_data_endian_u.mBits.rgb_swap_mod,
            g_gsp_reg->gsp_des_data_endian_u.mBits.a_swap_mod);
    printk( "********************************************* \n");
}

static void GPSTimeoutPrint(gsp_context_t *gspCtx)
{
    printk("%s%d:GSP_EN:0x%08x,EMC_MATRIX:0x%08x,IOMMU:0x%08x,GSP_GAP:0x%08x,GSP_CLOCK:0x%08x,GSP_AUTO_GATE:0x%08x\n",__func__,__LINE__,
           (uint32_t)(GSP_REG_READ(GSP_MOD_EN)&GSP_MOD_EN_BIT),
           (uint32_t)(GSP_REG_READ(GSP_EMC_MATRIX_BASE)&GSP_EMC_MATRIX_BIT),
           GSP_REG_READ(GSP_MMU_CTRL_BASE),
           ((volatile GSP_REG_T*)GSP_REG_BASE)->gsp_cfg_u.mBits.dist_rb,
           GSP_REG_READ(GSP_CLOCK_BASE)&0x3,
           (uint32_t)(GSP_REG_READ(GSP_AUTO_GATE_ENABLE_BASE)&GSP_AUTO_GATE_ENABLE_BIT));
    printCfgInfo(gspCtx);
    printGPSReg();
}

void printCMDQ(GSP_LAYER1_REG_T *pDescriptors, uint32_t c)
{
    uint32_t i = 0;
    while(i < c) {
        printk("CMDQ[%d]cfg:%08x\n",i,pDescriptors->gsp_layer1_cfg_u.dwValue);
        printk("CMDQ[%d]YADDR:%08x\n",i,pDescriptors->gsp_layer1_y_addr_u.dwValue);
        printk("CMDQ[%d]UADDR:%08x\n",i,pDescriptors->gsp_layer1_uv_addr_u.dwValue);
        printk("CMDQ[%d]VADDR:%08x\n",i,pDescriptors->gsp_layer1_va_addr_u.dwValue);
        printk("CMDQ[%d]Pitch:%08x\n",i,pDescriptors->gsp_layer1_pitch_u.dwValue);
        printk("CMDQ[%d]clip x:%04d y:%04d\n",i,pDescriptors->gsp_layer1_clip_start_u.mBits.clip_start_x_l1,pDescriptors->gsp_layer1_clip_start_u.mBits.clip_start_y_l1);
        printk("CMDQ[%d]clip w:%04d h:%04d\n",i,pDescriptors->gsp_layer1_clip_size_u.mBits.clip_size_x_l1,pDescriptors->gsp_layer1_clip_size_u.mBits.clip_size_y_l1);
        printk("CMDQ[%d]des x:%04d y:%04d\n",i,pDescriptors->gsp_layer1_des_start_u.mBits.des_start_x_l1,pDescriptors->gsp_layer1_des_start_u.mBits.des_start_y_l1);
        printk("CMDQ[%d]GREY:%08x\n",i,pDescriptors->gsp_layer1_grey_rgb_u.dwValue);
        printk("CMDQ[%d]Endian:%08x\n",i,pDescriptors->gsp_layer1_endian_u.dwValue);
        printk("CMDQ[%d]Alpha:%08x\n",i,pDescriptors->gsp_layer1_alpha_u.dwValue);
        printk("CMDQ[%d]CK:%08x\n",i,pDescriptors->gsp_layer1_ck_u.dwValue);
        i++;
        pDescriptors++;
    }
}

#endif
