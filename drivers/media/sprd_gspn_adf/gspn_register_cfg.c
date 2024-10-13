/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#include "gspn_sync.h"

/*
func:GSPN_MiscInfoCfg
reg_base : gspn module ctl reg base
*/
static void GSPN_MiscInfoCfg(GSPN_CTL_REG_T *reg_base, GSPN_MISC_INFO_T *misc_info)
{
    GSPN_RUN_MODE_SET(reg_base,misc_info->run_mod);
    GSPN_R_GAP_SET(reg_base,misc_info->gap_rb);
    GSPN_W_GAP_SET(reg_base,misc_info->gap_wb);
    GSPN_HTAP4_SET(reg_base,misc_info->htap4_en);
    if(misc_info->scale_en) {
        GSPN_SCL_RST(reg_base);
    }
    GSPN_CMD_EN_SET(reg_base,0);
    GSPN_CMD_ADDR_SET(reg_base,0);
    GSPN_MOD1_BG_EN_SET(reg_base,misc_info->bg_en);
    GSPN_MOD1_PM_EN_SET(reg_base,misc_info->pmargb_en);
    GSPN_MOD1_SCL_EN_SET(reg_base,misc_info->scale_en);
    GSPN_MOD1_DITHER_EN_SET(reg_base,misc_info->dither1_en);
    GSPN_MOD1_SCL_SEQ_SET(reg_base,misc_info->scale_seq);
    GSPN_MOD2_DITHER_EN_SET(reg_base,misc_info->dither2_en);
}


static void GSPN_DES1InfoCfg(GSPN_CTL_REG_T *reg_base, GSPN_LAYER_DES1_INFO_T *des1_info, uint32_t run_mod)
{
    GSPN_DST1_R8COMPRESS_SET(reg_base,des1_info->compress_r8);
    GSPN_DST1_FMT_SET(reg_base,des1_info->fmt);
    GSPN_DST1_R2Y_MODE_SET(reg_base,des1_info->r2y_mod);
    GSPN_DST1_ROT_MODE_SET(reg_base,des1_info->rot_mod);
    GSPN_DST1_R8_AR_SWAP_SET(reg_base,des1_info->endian.a_swap_mod);
    GSPN_DST1_R5_RGB_SWAP_SET(reg_base,des1_info->endian.rgb_swap_mod);
    GSPN_DST1_UV_WORD_ENDIAN_SET(reg_base,des1_info->endian.uv_word_endn);
    GSPN_DST1_UV_DWORD_ENDIAN_SET(reg_base,des1_info->endian.uv_dword_endn);
    GSPN_DST1_Y_WORD_ENDIAN_SET(reg_base,des1_info->endian.y_word_endn);
    GSPN_DST1_Y_DWORD_ENDIAN_SET(reg_base,des1_info->endian.y_dword_endn);
    GSPN_DST1_Y_ADDR_SET(reg_base,des1_info->addr.plane_y);
    GSPN_DST1_U_ADDR_SET(reg_base,des1_info->addr.plane_u);
    GSPN_DST1_V_ADDR_SET(reg_base,des1_info->addr.plane_v);
    GSPN_DST1_PITCH_SET(reg_base,des1_info->pitch.w,des1_info->pitch.h);
    GSPN_DST1_BG_COLOR_SET(reg_base,des1_info->bg_color.a,des1_info->bg_color.r,des1_info->bg_color.g,des1_info->bg_color.b);
    if(run_mod == 0) {
        GSPN_SCL_OUT_SIZE_SET(reg_base,des1_info->scale_out_size.w,des1_info->scale_out_size.h);
    }
    GSPN_WORK_DST_START_SET(reg_base,des1_info->work_dst_start.x,des1_info->work_dst_start.y);
    GSPN_WORK_SRC_SIZE_SET(reg_base,des1_info->work_src_size.w,des1_info->work_src_size.h);
    GSPN_WORK_SRC_START_SET(reg_base,des1_info->work_src_start.x,des1_info->work_src_start.y);
}



/*
scale_out_size will both config in des1 des2,
*/
static void GSPN_DES2InfoCfg(GSPN_CTL_REG_T *reg_base, GSPN_LAYER_DES2_INFO_T *des2_info, uint32_t run_mod)
{
    GSPN_DST2_R8COMPRESS_SET(reg_base,des2_info->compress_r8);
    GSPN_DST2_FMT_SET(reg_base,des2_info->fmt);
    GSPN_DST2_R2Y_MODE_SET(reg_base,des2_info->r2y_mod);
    GSPN_DST2_ROT_MODE_SET(reg_base,des2_info->rot_mod);
    GSPN_DST2_R8_AR_SWAP_SET(reg_base,des2_info->endian.a_swap_mod);
    GSPN_DST2_R5_RGB_SWAP_SET(reg_base,des2_info->endian.rgb_swap_mod);
    GSPN_DST2_UV_WORD_ENDIAN_SET(reg_base,des2_info->endian.uv_word_endn);
    GSPN_DST2_UV_DWORD_ENDIAN_SET(reg_base,des2_info->endian.uv_dword_endn);
    GSPN_DST2_Y_WORD_ENDIAN_SET(reg_base,des2_info->endian.y_word_endn);
    GSPN_DST2_Y_DWORD_ENDIAN_SET(reg_base,des2_info->endian.y_dword_endn);
    GSPN_DST2_Y_ADDR_SET(reg_base,des2_info->addr.plane_y);
    GSPN_DST2_U_ADDR_SET(reg_base,des2_info->addr.plane_u);
    GSPN_DST2_V_ADDR_SET(reg_base,des2_info->addr.plane_v);
    GSPN_DST2_PITCH_SET(reg_base,des2_info->pitch.w,des2_info->pitch.h);
    if(run_mod == 1) {
        GSPN_SCL_OUT_SIZE_SET(reg_base,des2_info->scale_out_size.w,des2_info->scale_out_size.h);
    }
}



static void GSPN_L0InfoCfg(GSPN_CTL_REG_T *reg_base, GSPN_LAYER0_INFO_T *l0_info)
{
    GSPN_L0_Y2R_MODE_SET(reg_base,l0_info->y2r_mod);
    GSPN_L0_ZORDER_SET(reg_base,l0_info->z_order);
    GSPN_Lx_PALLET_EN_SET(reg_base,l0_info->pallet_en,0);
    GSPN_Lx_CK_EN_SET(reg_base,l0_info->ck_en,0);
    GSPN_Lx_FMT_SET(reg_base,l0_info->fmt,0);
    GSPN_Lx_R8_AR_SWAP_SET(reg_base,l0_info->endian.a_swap_mod,0);
    GSPN_Lx_R5_RGB_SWAP_SET(reg_base,l0_info->endian.rgb_swap_mod,0);
    GSPN_L0_UV_WORD_ENDIAN_SET(reg_base,l0_info->endian.uv_word_endn);
    GSPN_L0_UV_DWORD_ENDIAN_SET(reg_base,l0_info->endian.uv_dword_endn);
    GSPN_L0_Y_WORD_ENDIAN_SET(reg_base,l0_info->endian.y_word_endn);
    GSPN_L0_Y_DWORD_ENDIAN_SET(reg_base,l0_info->endian.y_dword_endn);
    GSPN_L0_Y_ADDR_SET(reg_base,l0_info->addr.plane_y);
    GSPN_L0_U_ADDR_SET(reg_base,l0_info->addr.plane_u);
    GSPN_L0_V_ADDR_SET(reg_base,l0_info->addr.plane_v);
    GSPN_Lx_PITCH_SET(reg_base,l0_info->pitch.w,0);
    GSPN_Lx_CLIP_START_SET(reg_base,l0_info->clip_start.x,l0_info->clip_start.y,0);
    GSPN_Lx_CLIP_SIZE_SET(reg_base,l0_info->clip_size.w,l0_info->clip_size.h,0);
    GSPN_Lx_DST_START_SET(reg_base,l0_info->dst_start.x,l0_info->dst_start.y,0);
    GSPN_Lx_PALLET_COLOR_SET(reg_base,l0_info->pallet_color.a,l0_info->pallet_color.r,l0_info->pallet_color.g,l0_info->pallet_color.b,0);
    GSPN_Lx_CK_COLOR_SET(reg_base,l0_info->alpha,l0_info->ck_color.r,l0_info->ck_color.g,l0_info->ck_color.b,0);

    if(l0_info->layer_en == 1 && l0_info->y2r_mod<2) {
        GSPN_L0_CONTRAST_SET(reg_base,l0_info->y2r_param.y_contrast);
        GSPN_L0_BRIGHT_SET(reg_base,l0_info->y2r_param.y_brightness);
        GSPN_L0_USATURATION_SET(reg_base,l0_info->y2r_param.u_saturation);
        GSPN_L0_UOFFSET_SET(reg_base,l0_info->y2r_param.u_offset);
        GSPN_L0_VSATURATION_SET(reg_base,l0_info->y2r_param.v_saturation);
        GSPN_L0_VOFFSET_SET(reg_base,l0_info->y2r_param.v_offset);
    } else {
        GSPN_L0_CONTRAST_SET(reg_base,0x100);
        GSPN_L0_BRIGHT_SET(reg_base,0x00);
        GSPN_L0_USATURATION_SET(reg_base,0x100);
        GSPN_L0_UOFFSET_SET(reg_base,0x80);
        GSPN_L0_VSATURATION_SET(reg_base,0x100);
        GSPN_L0_VOFFSET_SET(reg_base,0x80);
    }
    GSPN_MOD1_LxPM_EN_SET(reg_base,l0_info->pmargb_mod,0);
    GSPN_MOD1_Lx_EN_SET(reg_base,l0_info->layer_en,0);
}

static void GSPN_OSDInfoCfg(GSPN_CTL_REG_T *reg_base, GSPN_CMD_INFO_T *cmd)
{
    GSPN_Lx_INFO_CFG(reg_base,&cmd->l1_info,1);
    GSPN_Lx_INFO_CFG(reg_base,&cmd->l2_info,2);
    GSPN_Lx_INFO_CFG(reg_base,&cmd->l3_info,3);
}

/*
GSPN_InfoCfg
description: support dual-core, sub_idx == -1 means not be split
*/
int __must_check GSPN_InfoCfg(GSPN_KCMD_INFO_T *kcmd, int sub_idx)
{
    GSPN_CTL_REG_T *ctl_reg_base = NULL;
    GSPN_CMD_INFO_T *cmd = NULL;
    GSPN_CORE_T *core = NULL;

    if(kcmd == NULL || kcmd->occupied_core->ctl_reg_base == NULL) {
        return -1;
    }

    if(sub_idx < 0) {
        ctl_reg_base = kcmd->occupied_core->ctl_reg_base;
        cmd = &kcmd->src_cmd;
    } else {
        ctl_reg_base = kcmd->sub_cmd_occupied_core[sub_idx]->ctl_reg_base;
        cmd = &kcmd->sub_cmd[sub_idx];
    }

    core = kcmd->occupied_core;
    REG_SET(core->clk_select_reg_base, 3);

    GSPN_L0InfoCfg(ctl_reg_base, &cmd->l0_info);
    GSPN_OSDInfoCfg(ctl_reg_base, cmd);
    GSPN_DES1InfoCfg(ctl_reg_base, &cmd->des1_info, cmd->misc_info.run_mod);
    GSPN_DES2InfoCfg(ctl_reg_base, &cmd->des2_info, cmd->misc_info.run_mod);
    GSPN_MiscInfoCfg(ctl_reg_base, &cmd->misc_info);

    return GSPN_MOD1_ERRFLAG_GET(ctl_reg_base);//|GSPN_MOD1_ERRCODE_GET(ctl_reg_base);
    //return GSPN_MOD1_ERRCODE_GET(ctl_reg_base);//|GSPN_MOD2_ERRCODE_GET(ctl_reg_base);
}

/*
func:GSPN_ClocksCheckPhase0
desc: check all clock except iommu
*/
static int __must_check GSPN_ClocksCheckPhase0(GSPN_CONTEXT_T *ctx, uint32_t core_id)
{
    int ret = 0;
    GSPN_CORE_MNG_T *core_mng = &ctx->core_mng;
    GSPN_CTL_REG_T *reg_base = core_mng->cores[core_id].ctl_reg_base;
    if(!CORE_ID_VALID(core_id) || reg_base == 0) {
        return -1;
    }

    //check GSPN module enable
    if(core_mng->module_sel_reg_base && ctx->capability.version == 0x10) {
        if(!(REG_GET(core_mng->module_sel_reg_base)&0x1)) {
            GSPN_LOGE("T8: module select is old gsp! 0x%08x & 0x1.\n",
                      REG_GET(core_mng->module_sel_reg_base));
            ret++;
        }
    }
    if(!(REG_GET(core_mng->enable_reg_base) & core_mng->cores[core_id].gspn_en_rst_bit)) {
        GSPN_LOGE("core%d module enable is disabled! 0x%08x & 0x%08x\n",
                  core_id,REG_GET(core_mng->enable_reg_base), core_mng->cores[core_id].gspn_en_rst_bit);
        ret++;
    } else {
        if(REG_GET(core_mng->reset_reg_base) & core_mng->cores[core_id].gspn_en_rst_bit) {
            GSPN_LOGE("core%d soft reset is set! 0x%08x & 0x%08x\n",
                      core_id, REG_GET(core_mng->reset_reg_base), core_mng->cores[core_id].gspn_en_rst_bit);
            ret++;
        } else {
            if(GSPN_MOD1_BUSY_GET(reg_base) || GSPN_MOD2_BUSY_GET(reg_base)) {
                GSPN_LOGE("core%d busy is still on! mod1:0x%08x,mod2:0x%08x\n",
                          core_id, (( GSPN_CTL_REG_T*)reg_base)->mod1_cfg.dwValue, (( GSPN_CTL_REG_T*)reg_base)->mod2_cfg.dwValue);
                ret++;
            }
        }
    }

    //check GSPN MTX EB clock 
    if(!(REG_GET(core_mng->cores[core_id].emc_reg_base) & core_mng->cores[core_id].mtx_en_bit)) {
        GSPN_LOGE("core%d mtx en is disabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(core_mng->cores[core_id].emc_reg_base), core_mng->cores[core_id].mtx_en_bit);
        ret++;
    }
    //check GSPN NOC AUTO_GATE enable / NOC FORCE_GATE clock
    if((REG_GET(core_mng->cores[core_id].auto_gate_reg_base) & core_mng->cores[core_id].noc_auto_bit)) {
        GSPN_LOGI("core%d noc auto is enabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(core_mng->cores[core_id].auto_gate_reg_base), core_mng->cores[core_id].noc_auto_bit);
    } else if ((REG_GET(core_mng->cores[core_id].auto_gate_reg_base) & core_mng->cores[core_id].noc_force_bit)) {
        GSPN_LOGI("core%d noc force is enabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(core_mng->cores[core_id].auto_gate_reg_base), core_mng->cores[core_id].noc_force_bit);
    } else {
        GSPN_LOGE("core%d GSPN NOC AUTO_GATE clock and NOC FORCE_GATE clock are both not enabled! ");
        ret++;
    }


    //check GSPN MTX AUTO_GATE clock  / MTX FORCE_GATE clock
    if((REG_GET(core_mng->cores[core_id].auto_gate_reg_base) & core_mng->cores[core_id].mtx_auto_bit)) {
        GSPN_LOGI("core%d mtx auto is enabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(core_mng->cores[core_id].auto_gate_reg_base), core_mng->cores[core_id].mtx_auto_bit);
    } else if ((REG_GET(core_mng->cores[core_id].auto_gate_reg_base) & core_mng->cores[core_id].mtx_force_bit)) {
        GSPN_LOGI("core%d mtx force is enabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(core_mng->cores[core_id].auto_gate_reg_base), core_mng->cores[core_id].mtx_force_bit);
    } else {
        GSPN_LOGE("core%d GSPN MTX AUTO_GATE clock and MTX FORCE_GATE clock are both not enabled! ");
        ret++;
    }

    //check GSPN AUTO_GATE clock / GSPN FORCE_GATE clock
    if((REG_GET(core_mng->cores[core_id].auto_gate_reg_base) & core_mng->cores[core_id].auto_gate_bit)) {
        GSPN_LOGI("core%d auto gate is enabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(core_mng->cores[core_id].auto_gate_reg_base), core_mng->cores[core_id].auto_gate_bit);
    } else if (REG_GET(core_mng->cores[core_id].auto_gate_reg_base) & core_mng->cores[core_id].force_gate_bit) {
        GSPN_LOGI("core%d force gate is enabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(core_mng->cores[core_id].auto_gate_reg_base), core_mng->cores[core_id].force_gate_bit);
    } else {
        GSPN_LOGE("core%d GSPN AUTO_GATE clock and FORCE_GATE clock are both not enabled! ");
        ret++;
    }

    //check GSPN EMC clock
    if(!(REG_GET(core_mng->cores[core_id].emc_reg_base) & core_mng->cores[core_id].emc_en_bit)) {
        GSPN_LOGE("core%d emc clock is disabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(core_mng->cores[core_id].emc_reg_base), core_mng->cores[core_id].emc_en_bit);
        ret++;
    }

    return (ret>0)?GSPN_K_CLK_CHK_ERR:GSPN_NO_ERR;
}

/*
func:GSPN_ClocksCheckPhase1
desc: check iommu cfg
*/
static int __must_check GSPN_ClocksCheckPhase1(GSPN_CORE_MNG_T *core_mng, uint32_t core_id)
{
#define IOVA_CHECK(addr)    (0x10000000<= (addr) && (addr) < 0x80000000)
    int ret = 0;
    int iommu_enabled = 1;
    GSPN_CTL_REG_T *reg_base = core_mng->cores[core_id].ctl_reg_base;
    uint32_t iommu_ctl = REG_GET(core_mng->cores[core_id].iommu_ctl_reg_base);
    uint32_t addr_y0 = GSPN_L0_Y_ADDR_GET(reg_base);
    uint32_t addr_u0 = GSPN_L0_U_ADDR_GET(reg_base);
    uint32_t addr_v0 = GSPN_L0_V_ADDR_GET(reg_base);

    uint32_t addr_y1 = GSPN_Lx_ADDR_GET(reg_base,1);
    uint32_t addr_y2 = GSPN_Lx_ADDR_GET(reg_base,2);
    uint32_t addr_y3 = GSPN_Lx_ADDR_GET(reg_base,3);

    uint32_t addr_yd1 = GSPN_DST1_Y_ADDR_GET(reg_base);
    uint32_t addr_ud1 = GSPN_DST1_U_ADDR_GET(reg_base);
    uint32_t addr_vd1 = GSPN_DST1_V_ADDR_GET(reg_base);

    uint32_t addr_yd2 = GSPN_DST2_Y_ADDR_GET(reg_base);
    uint32_t addr_ud2 = GSPN_DST2_U_ADDR_GET(reg_base);
    uint32_t addr_vd2 = GSPN_DST2_V_ADDR_GET(reg_base);

    if(!(REG_GET(core_mng->enable_reg_base) & core_mng->cores[core_id].mmu_en_rst_bit)
       ||(REG_GET(core_mng->reset_reg_base) & core_mng->cores[core_id].mmu_en_rst_bit)
       ||!(REG_GET(core_mng->cores[core_id].iommu_ctl_reg_base) & 0x1)) {
        iommu_enabled = 0;
    }

    //check GSP IOMMU ENABLE
#if 0
    if((iommu_enabled == 0 || (iommu_ctl & 0xF0000000)==0)/*IOMMU be disabled*/
       &&(((GSPN_MOD1_Lx_EN_GET(reg_base,0) == 1) && (IOVA_CHECK(addr_y0) || IOVA_CHECK(addr_u0) || IOVA_CHECK(addr_v0)))/*L0 is enabled and use iova*/
          ||((GSPN_MOD1_Lx_EN_GET(reg_base,1) == 1) && IOVA_CHECK(addr_y1))/*L1 is enabled and use iova*/
          ||((GSPN_MOD1_Lx_EN_GET(reg_base,2) == 1) && IOVA_CHECK(addr_y2))/*L2 is enabled and use iova*/
          ||((GSPN_MOD1_Lx_EN_GET(reg_base,3) == 1) && IOVA_CHECK(addr_y3))/*L3 is enabled and use iova*/
          ||((GSPN_RUN_MODE_GET(reg_base) == 0 ||GSPN_MOD1_Lx_EN_GET(reg_base,1) == 1 ||GSPN_MOD1_Lx_EN_GET(reg_base,2) == 1 ||GSPN_MOD1_Lx_EN_GET(reg_base,3) == 1)
             && (IOVA_CHECK(addr_yd1) || IOVA_CHECK(addr_ud1) || IOVA_CHECK(addr_vd1)))
          ||((GSPN_RUN_MODE_GET(reg_base) == 1 ||GSPN_MOD1_Lx_EN_GET(reg_base,0) == 1)
             && (IOVA_CHECK(addr_yd2) || IOVA_CHECK(addr_ud2) || IOVA_CHECK(addr_vd2))))) {
        GSPN_LOGE("core%d use iova but iommu is disabled!\n",core_id);
        GSPN_LOGE("en_reg:0x%08x, rst_reg:0x%08x, mask:0x%08x; ctl_reg:0x%08x, mask:0x%08x;\n",
                  REG_GET(core_mng->enable_reg_base),
                  REG_GET(core_mng->reset_reg_base),
                  core_mng->cores[core_id].mmu_en_rst_bit,
                  REG_GET(core_mng->cores[core_id].iommu_ctl_reg_base),0x1);
        ret++;
    }
#endif
    return (ret>0)?GSPN_K_CLK_CHK_ERR:GSPN_NO_ERR;
}

/*
func:GSPN_TryToRecover
desc: check all clock except iommu
*/
int GSPN_TryToRecover(GSPN_CONTEXT_T *ctx, uint32_t core_id)
{
    GSPN_CTL_REG_T *reg_base = NULL;
    uint32_t* base = NULL;
    uint32_t mask = 0;

    if(core_id < 0 || core_id >= GSPN_CORE_MAX || ctx == NULL) {
        return -1;
    }
    reg_base = ctx->core_mng.cores[core_id].ctl_reg_base;

    //on sharkLT8, check GSP/GSPN module select
    base = ctx->core_mng.module_sel_reg_base;
    mask = 0x1;
    if(base && (ctx->capability.version == 0x10) && (mask != (REG_GET(base)&mask))) {
        GSPN_LOGW("core%d clock select is not set to hightest freq! 0x%08x\n",
                  core_id,REG_GET(base));
        REG_OR(base,mask);
    }

    //check GSPN clock select
    base = ctx->core_mng.cores[core_id].clk_select_reg_base;
    mask = 0x3;
    if(mask != (REG_GET(base)&mask)) {
        GSPN_LOGW("core%d clock select is not set to hightest freq! 0x%08x\n",
                  core_id,REG_GET(base));
    }

    //check GSPN module enable
    base = ctx->core_mng.enable_reg_base;
    mask = ctx->core_mng.cores[core_id].gspn_en_rst_bit;
    if(mask != (REG_GET(base)&mask)) {
        GSPN_LOGE("core%d enable is not set! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(base), mask);
        REG_OR(base,mask);
    }

    //check GSPN module reset
    base = ctx->core_mng.reset_reg_base;
    mask = ctx->core_mng.cores[core_id].gspn_en_rst_bit;
    if(mask == (REG_GET(base)&mask)) {
        GSPN_LOGE("core%d reset is set! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(base), mask);
        REG_CLR(base,mask);
    } else {
        GSPN_LOGW("reset core%d. 0x%08x & 0x%08x\n",
                  core_id, REG_GET(base), mask);
        GSPN_SOFT_RESET(reg_base,base, mask);
    }

    //check GSPN AUTO_GATE clock
    base = ctx->core_mng.cores[core_id].auto_gate_reg_base;
    mask = ctx->core_mng.cores[core_id].auto_gate_bit;
    if(mask != (REG_GET(base)&mask)) {
        GSPN_LOGE("core%d auto gate is disabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(base), mask);
        REG_OR(base,mask);
    }

    //check GSPN EMC clock
    base = ctx->core_mng.cores[core_id].emc_reg_base;
    mask = ctx->core_mng.cores[core_id].emc_en_bit;
    if(mask != (REG_GET(base)&mask)) {
        GSPN_LOGE("core%d emc is disabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(base), mask);
        REG_OR(base,mask);
    }

    //check iommu clock
    base = ctx->core_mng.enable_reg_base;
    mask = ctx->core_mng.cores[core_id].mmu_en_rst_bit;
    if(mask != (REG_GET(base)&mask)) {
        GSPN_LOGE("core%d mmu_en disabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(base), mask);
        REG_OR(base,mask);
    }
    base = ctx->core_mng.reset_reg_base;
    mask = ctx->core_mng.cores[core_id].mmu_en_rst_bit;
    if(mask == (REG_GET(base)&mask)) {
        GSPN_LOGE("core%d mmu_rst set! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(base), mask);
        REG_CLR(base,mask);
    }
    base = ctx->core_mng.cores[core_id].iommu_ctl_reg_base;
    mask = 0x1;
    if(mask != (REG_GET(base)&mask)) {
        GSPN_LOGE("core%d mmu_local_en disabled! 0x%08x & 0x%08x\n",
                  core_id, REG_GET(base), mask);
        REG_OR(base,mask);
    }

    return GSPN_NO_ERR;
}

static int GSPN_CoreClockSwitch(GSPN_CORE_T *core, uint32_t enable)
{
    int ret = 0;
	if(core == NULL) {
		GSPN_LOGE("mng or core is null pointer\n");	
		return -1;
	}

    if(core->gspn_clk == NULL
		|| core->mtx_clk == NULL) {
        GSPN_LOGW("clock not init yet.gspn_clk:%p,emc_clk:%p\n",core->gspn_clk,core->mtx_clk);
        return -1;
    }

    if(enable) {
        ret = clk_prepare_enable(core->gspn_clk);
        if(ret) {
            GSPN_LOGE("enable gspn clock failed!err:%d.\n",ret);
            return ret;
        } else {
            GSPN_LOGI("enable gspn clock ok!\n");
        }
        ret = clk_prepare_enable(core->mmu_clk);
        if(ret) {
            GSPN_LOGE("enable mmu clock failed!err:%d.\n",ret);
            return ret;
        } else {
            GSPN_LOGI("enable mmu clock ok!\n");
        }
        ret = clk_prepare_enable(core->mtx_clk);
        if(ret) {
            GSPN_LOGE("enable mtx clock failed!err:%d.\n",ret);
            return ret;
        } else {
            GSPN_LOGI("enable mtx clock ok!\n");
        }
        ret = clk_prepare_enable(core->niu_clk);
        if(ret) {
            GSPN_LOGE("enable niu clock failed!err:%d.\n",ret);
            return ret;
        } else {
            GSPN_LOGI("enable niu clock ok!\n");
        }
        ret = clk_prepare_enable(core->disp_clk);
        if(ret) {
            GSPN_LOGE("enable disp clock failed!err:%d.\n",ret);
            return ret;
        } else {
            GSPN_LOGI("enable disp clock ok!\n");
        }
        //GSPN_AUTO_GATE_ENABLE();
    } else {
        clk_disable_unprepare(core->disp_clk);
        clk_disable_unprepare(core->niu_clk);
        clk_disable_unprepare(core->mtx_clk);
        clk_disable_unprepare(core->mmu_clk);
        clk_disable_unprepare(core->gspn_clk);
        GSPN_LOGI("disable clock ok!\n");
    }

    return ret;
}


int GSPN_CoreEnable(GSPN_CONTEXT_T *ctx,int32_t core_id)
{
    int ret = 0;
    if(!CORE_ID_VALID(core_id) || ctx == NULL) {
        return -1;
    }

    ret = GSPN_CoreClockSwitch(&ctx->core_mng.cores[core_id],1);
    if(ret) {
        GSPN_LOGE("module enable failed!\n");
        return GSPN_K_CLK_EN_ERR;
    } else {
        GSPN_LOGI("module enable success!\n");
    }

    //GSPN_SOFT_RESET(core->ctl_reg_base,0);
    ret = GSPN_ClocksCheckPhase0(ctx,core_id);
    return ret;
}

void GSPN_CoreDisable(GSPN_CORE_T *core)
{
    GSPN_IRQSTATUS_CLEAR(core->ctl_reg_base);
    GSPN_IRQENABLE_SET(core->ctl_reg_base,GSPN_IRQ_TYPE_DISABLE);
    //GSPN_CoreClockSwitch(core, 0);  hl add for debug
}

int GSPN_Trigger(GSPN_CORE_MNG_T *core_mng,int32_t core_id)
{
    int32_t ret = 0;
    int32_t sub_idx = -1;
    uint32_t reg_val = 0;
    GSPN_CTL_REG_T *reg_base = 0;
    GSPN_CMD_INFO_T *cmd = NULL;
    GSPN_KCMD_INFO_T *kcmd = NULL;
    if((!CORE_ID_VALID(core_id)) || core_mng == NULL) {
        return -1;
    }

    kcmd = core_mng->cores[core_id].current_kcmd;
    sub_idx = core_mng->cores[core_id].current_kcmd_sub_cmd_index;
    reg_base = core_mng->cores[core_id].ctl_reg_base;
    if(sub_idx < 0) {
        cmd = &kcmd->src_cmd;
    } else {
        cmd = &kcmd->sub_cmd[sub_idx];
    }

    ret = GSPN_ClocksCheckPhase1(core_mng,core_id);
    if(ret) {
        return ret;
    }

    if(cmd->des1_info.layer_en == 1) {
        if(GSPN_MOD1_ERRFLAG_GET(reg_base) || GSPN_MOD1_ERRCODE_GET(reg_base)) {
            //parameters err, but it could not happen, we have checked this after info configuration.
			GSPN_LOGE("mod1 parameters err\n");
            return GSPN_MOD1_ERRCODE_GET(reg_base);
        } else {
            //params ok,prepare interrupt
            reg_val = GSPN_INT_REG_GET(reg_base);
            reg_val |= GSPN_INT_EN_BLD_BIT;//bld int en
            reg_val |= GSPN_INT_EN_BERR_BIT;//bld err int en
            reg_val |= GSPN_INT_CLR_BLD_BIT;//bld int clr
            reg_val |= GSPN_INT_CLR_BERR_BIT;//bld err int clr
            GSPN_INT_REG_SET(reg_base,reg_val);

            GSPN_BLEND_TRIGGER(reg_base);
        }
    }

    if(cmd->des2_info.layer_en == 1) {
        if(GSPN_MOD2_ERRFLAG_GET(reg_base) || GSPN_MOD2_ERRCODE_GET(reg_base)) {
            //parameters err, but it could not happen, we have checked this after info configuration.
			GSPN_LOGE("mod2 parameters err\n");
            return GSPN_MOD2_ERRCODE_GET(reg_base);
        } else {
            //params ok,prepare interrupt
            reg_val = GSPN_INT_REG_GET(reg_base);
            reg_val |= GSPN_INT_EN_SCL_BIT;//scl int en
            reg_val |= GSPN_INT_EN_SERR_BIT;//scl err int en
            reg_val |= GSPN_INT_CLR_SCL_BIT;//scl int clr
            reg_val |= GSPN_INT_CLR_SERR_BIT;//scl err int clr
            GSPN_INT_REG_SET(reg_base,reg_val);

            GSPN_SCALE_TRIGGER(reg_base);
        }
    }

    return 0;
}


/*
func:gspn_int_clear_and_disable
desc:
*/
static void gspn_int_clear_and_disable(GSPN_CORE_T *core)
{
    int32_t sub_idx = -1;
    GSPN_KCMD_INFO_T *kcmd = NULL;
    GSPN_CTL_REG_T *reg_base = NULL;
    GSPN_CMD_INFO_T *cmd = NULL;
    uint32_t reg_val = 0;

	if (core == NULL) {
		GSPN_LOGE("interrupt clear with null core\n");	
		return;
	}

    sub_idx = core->current_kcmd_sub_cmd_index;
    kcmd = core->current_kcmd;
    reg_base = core->ctl_reg_base;
	if (kcmd == NULL) {
		GSPN_LOGE("interrupt clear with null kcmd\n");	
		return;
	}

	if (kcmd->status != 1)
		GSPN_LOGE("ISR handle kcmd status error\n");
	kcmd->status = 2;

    if(sub_idx < 0) {
        cmd = &kcmd->src_cmd;
    } else {
		GSPN_LOGE("interrupt clear error path\n");
        cmd = &kcmd->sub_cmd[sub_idx];
    }

	if (cmd == NULL) {
		GSPN_LOGE("clear reg with null cmd\n");
		return;
	}

    if(cmd->des1_info.layer_en == 1) {
        reg_val = GSPN_INT_REG_GET(reg_base);
        reg_val &= (~GSPN_INT_EN_BLD_BIT);//bld-int disable
        reg_val &= (~GSPN_INT_EN_BERR_BIT);//bld-err-int disable
        reg_val |= GSPN_INT_CLR_BLD_BIT;//bld-int clear
        reg_val |= GSPN_INT_CLR_BERR_BIT;//bld-err-int clear
        GSPN_INT_REG_SET(reg_base,reg_val);
    }

    if(cmd->des2_info.layer_en == 1) {
        reg_val = GSPN_INT_REG_GET(reg_base);
        reg_val &= (~GSPN_INT_EN_SCL_BIT);//scl-int disable
        reg_val &= (~GSPN_INT_EN_SERR_BIT);//scl-err-int disable
        reg_val |= GSPN_INT_CLR_SCL_BIT;//scl-int clear
        reg_val |= GSPN_INT_CLR_SERR_BIT;//scl-err-int clear
        GSPN_INT_REG_SET(reg_base,reg_val);
    }
}


/*
func:gspn_irq_handler
desc:we treat a core as a whole part, scaler & blender not be operated separately
*/
irqreturn_t gspn_irq_handler_gsp0(int32_t irq, void *dev_id)
{
    GSPN_CONTEXT_T *ctx = (GSPN_CONTEXT_T *)dev_id;
    uint32_t int_value = 0;
    int32_t i = 0;

    if (NULL == ctx) {
        GSPN_LOGE("irq failed! ctx == NULL!\n");
        return IRQ_NONE;
    }

	i = 0;
	int_value = GSPN_INT_REG_GET(ctx->core_mng.cores[i].ctl_reg_base);
	if(int_value & (GSPN_INT_STS_BERR_BIT|GSPN_INT_STS_SERR_BIT)) {
		GSPN_LOGE("err detected! int_reg:0x%08x\n",int_value);
		//GSPN_DumpReg(&ctx->core_mng, i);
	}
	gspn_int_clear_and_disable(&ctx->core_mng.cores[i]);

	up(&ctx->wake_work_thread_sema);

    return IRQ_HANDLED;
}

/*
func:gspn_irq_handler
desc:we treat a core as a whole part, scaler & blender not be operated separately
*/
irqreturn_t gspn_irq_handler_gsp1(int32_t irq, void *dev_id)
{
    GSPN_CONTEXT_T *ctx = (GSPN_CONTEXT_T *)dev_id;
    uint32_t int_value = 0;
    int32_t i = 0;

    if (NULL == ctx) {
        GSPN_LOGE("irq failed! ctx == NULL!\n");
        return IRQ_NONE;
    }

	i = 1;
	int_value = GSPN_INT_REG_GET(ctx->core_mng.cores[i].ctl_reg_base);
	if(int_value & (GSPN_INT_STS_BERR_BIT|GSPN_INT_STS_SERR_BIT)) {
		GSPN_LOGE("err detected! int_reg:0x%08x\n",int_value);
		//GSPN_DumpReg(&ctx->core_mng, i);
	}
	gspn_int_clear_and_disable(&ctx->core_mng.cores[i]);

	up(&ctx->wake_work_thread_sema);

    return IRQ_HANDLED;
}


/*
func:GSPN_DumpReg
description: dump cores's clock reg, ctl register. only read, won't set register.
*/
void GSPN_DumpReg(GSPN_CORE_MNG_T *core_mng, uint32_t core_id)
{
    int32_t i = 0;
    int32_t core_en = 0;
    GSPN_CORE_T *core = &core_mng->cores[core_id];
    uint32_t *reg_base = (uint32_t *)core->ctl_reg_base;
    GSPN_CTL_REG_T *reg_struct = core->ctl_reg_base;
#define gspn_printk(fmt, ...)   printk(GSPN_TAG"[%d] " pr_fmt(fmt),core_id,##__VA_ARGS__)

    /*core's clock enable*/
    if(!(REG_GET(core_mng->enable_reg_base) & core->gspn_en_rst_bit)) {
        gspn_printk("warning, module is disabled!!");
    } else {
        core_en = 1;
        gspn_printk("info, module is enabled,");
    }
    printk(" reg:0x%08x, mask:0x%08x\n",
           REG_GET(core_mng->enable_reg_base),
           core->gspn_en_rst_bit);

    /*core's clock reset*/
    if(REG_GET(core_mng->reset_reg_base) & core->gspn_en_rst_bit) {
        gspn_printk("warning, module reset is set!!");
    } else {
        gspn_printk("info, module reset is clean,");
    }
    printk(" reg:0x%08x, mask:0x%08x\n",
           REG_GET(core_mng->reset_reg_base),
           core->gspn_en_rst_bit);

    /*core's clock source select*/
    if(REG_GET(core->clk_select_reg_base) != 0x3) {
        gspn_printk("warning, module clk select is not set to hightest freq!!");
    } else {
        gspn_printk("module clk select is at the hightest freq.");
    }
    printk(" reg:0x%08x, mask:0x%08x\n",
           REG_GET(core->clk_select_reg_base),
           0x3);

    /*core's auto gate enable*/
    if(!(REG_GET(core->auto_gate_reg_base) & core->auto_gate_bit)) {
        gspn_printk("warning, auto gate is not enabled!!");
    } else {
        gspn_printk("info, auto gate is enabled,");
    }
    printk(" reg:0x%08x, mask:0x%08x\n",
           REG_GET(core->auto_gate_reg_base),
           core_mng->cores[core_id].auto_gate_bit);

    /*core's force gate enable*/
    if(REG_GET(core->auto_gate_reg_base) & core->force_gate_bit) {
        gspn_printk("warning, force gate is enabled!!");
    } else {
        gspn_printk("info, force gate is disabled,");
    }
    printk(" reg:0x%08x, mask:0x%08x\n",
           REG_GET(core->auto_gate_reg_base),
           core->force_gate_bit);

    /*core's emc clock enable*/
    if(!(REG_GET(core_mng->cores[core_id].emc_reg_base) & core_mng->cores[core_id].emc_en_bit)) {
        gspn_printk("warning, emc is disabled!!");
    } else {
        gspn_printk("info, emc is enabled,");
    }
    printk(" reg:0x%08x, mask:0x%08x\n",
           REG_GET(core->emc_reg_base),
           core->emc_en_bit);

    /*core's iommu info*/
    if(!(REG_GET(core_mng->enable_reg_base) & core->mmu_en_rst_bit)
       ||(REG_GET(core_mng->reset_reg_base) & core->mmu_en_rst_bit)
       ||!(REG_GET(core->iommu_ctl_reg_base) & 0x1)) {
        gspn_printk("warning, iommu is disabled!!");
    } else {
        gspn_printk("info, iommu is enabled,");
    }
    printk(" en_reg:0x%08x, rst_reg:0x%08x, mask:0x%08x; ctl_reg:0x%08x, mask:0x%08x;\n",
           REG_GET(core_mng->enable_reg_base),
           REG_GET(core_mng->reset_reg_base),
           core->mmu_en_rst_bit,
           REG_GET(core->iommu_ctl_reg_base),0x1);

    if(core_en == 1) {
        /*core's ctl reg*/
        i = 0;
        while(i < 112) {
            printk("GSPN[%d] %p: %08x %08x %08x %08x\n",
                   core_id,reg_base,reg_base[0],reg_base[1],reg_base[2],reg_base[3]);
            i+=4;
            reg_base+=4;
        }

        /*core's ctl reg parsed*/
        gspn_printk("run_mode%d,scale_seq%d,cmd_en%d",
                    reg_struct->glb_cfg.mBits.run_mod,
                    reg_struct->mod1_cfg.mBits.scale_seq,
                    reg_struct->glb_cfg.mBits.cmd_en);
        if(reg_struct->glb_cfg.mBits.cmd_en) {
            printk(",up_m1%d,up_d%d,up_l3%d,up_l2%d,up_l1%d,up_l0%d,up_coef%d,stopc%d,cnt%d\n",
                   reg_struct->cmd_sts0.mBits.new_m1,
                   reg_struct->cmd_sts0.mBits.new_d,
                   reg_struct->cmd_sts0.mBits.new_l3,
                   reg_struct->cmd_sts0.mBits.new_l2,
                   reg_struct->cmd_sts0.mBits.new_l1,
                   reg_struct->cmd_sts0.mBits.new_l0,
                   reg_struct->cmd_sts0.mBits.new_c,
                   reg_struct->cmd_sts0.mBits.stopc,
                   reg_struct->cmd_sts0.mBits.stat_cmd_num);
        } else {
            printk("\n");
        }

        if(reg_struct->glb_cfg.mBits.run_mod == 0) {
            gspn_printk("mod1: bk_en%d,pm_en%d,scl_en%d,dither_en%d,scl_seq%d,l3_en%d,l2_en%d,l1_en%d,l0_en%d\n",
                        reg_struct->mod1_cfg.mBits.bg_en,
                        reg_struct->mod1_cfg.mBits.pmargb_en,
                        reg_struct->mod1_cfg.mBits.scale_en,
                        reg_struct->mod1_cfg.mBits.dither1_en,
                        reg_struct->mod1_cfg.mBits.scale_seq,
                        reg_struct->mod1_cfg.mBits.l3_en,
                        reg_struct->mod1_cfg.mBits.l2_en,
                        reg_struct->mod1_cfg.mBits.l1_en,
                        reg_struct->mod1_cfg.mBits.l0_en);
            gspn_printk("mod1: err_flg%d,err_code%d,w_busy%d,r_busy%d,bld_busy%d\n",
                        reg_struct->mod1_cfg.mBits.err1_flg,
                        reg_struct->mod1_cfg.mBits.err1_code,
                        reg_struct->mod1_cfg.mBits.wch_busy,
                        reg_struct->mod1_cfg.mBits.rch_busy,
                        reg_struct->mod1_cfg.mBits.bld_busy);
            gspn_printk("seq0 des1:fmt%d,compress%d,%s,rot%d {pitch%dx%d (%d,%d)[%dx%d]->(%d,%d)",
                        reg_struct->des1_data_cfg.mBits.format,
                        reg_struct->des1_data_cfg.mBits.compress_r8,
                        reg_struct->des1_data_cfg.mBits.r2y_mod?"reduce":"full",
                        reg_struct->des1_data_cfg.mBits.rot_mod,
                        reg_struct->des1_pitch.mBits.w,
                        reg_struct->des1_pitch.mBits.h,
                        reg_struct->work_src_start.mBits.x,
                        reg_struct->work_src_start.mBits.y,
                        reg_struct->work_src_size.mBits.w,
                        reg_struct->work_src_size.mBits.h,
                        reg_struct->work_des_start.mBits.x,
                        reg_struct->work_des_start.mBits.y);
            if(reg_struct->mod1_cfg.mBits.scale_seq == 0) {
                printk("}\n");
            } else {
                printk("[%dx%d]}\n",reg_struct->des_scl_size.mBits.w,reg_struct->des_scl_size.mBits.h);
            }

            /*layer0 info*/
            if(reg_struct->mod1_cfg.mBits.l0_en) {
                gspn_printk("L0:fmt%d,y2y%d,%s,zorder%d {pitch%04d (%d,%d)[%dx%d]->(%d,%d)}\n",
                            reg_struct->l0_cfg.mBits.format,
                            reg_struct->l0_cfg.mBits.y2r_mod>1?1:0,
                            (reg_struct->l0_cfg.mBits.y2r_mod&1)?"reduce":"full",
                            reg_struct->l0_cfg.mBits.layer_num,
                            reg_struct->l0_pitch.mBits.w,
                            reg_struct->l0_clip_start.mBits.x,
                            reg_struct->l0_clip_start.mBits.y,
                            reg_struct->l0_clip_size.mBits.w,
                            reg_struct->l0_clip_size.mBits.h,
                            reg_struct->l0_des_start.mBits.x,
                            reg_struct->l0_des_start.mBits.y);
                if(reg_struct->mod1_cfg.mBits.scale_seq == 0) {
                    printk("[%dx%d]}\n",reg_struct->des_scl_size.mBits.w,reg_struct->des_scl_size.mBits.h);
                } else {
                    printk("}\n");
                }
            }

            /*layer1 info*/
            if(reg_struct->mod1_cfg.mBits.l1_en) {
                gspn_printk("L1:fmt%d {pitch%04d (%d,%d)[%dx%d]->(%d,%d)}\n",
                            reg_struct->l1_cfg.mBits.format,
                            reg_struct->l1_pitch.mBits.w,
                            reg_struct->l1_clip_start.mBits.x,
                            reg_struct->l1_clip_start.mBits.y,
                            reg_struct->l1_clip_size.mBits.w,
                            reg_struct->l1_clip_size.mBits.h,
                            reg_struct->l1_des_start.mBits.x,
                            reg_struct->l1_des_start.mBits.y);
            }

            /*layer2 info*/
            if(reg_struct->mod1_cfg.mBits.l1_en) {
                gspn_printk("L2:fmt%d {pitch%04d (%d,%d)[%dx%d]->(%d,%d)}\n",
                            reg_struct->l2_cfg.mBits.format,
                            reg_struct->l2_pitch.mBits.w,
                            reg_struct->l2_clip_start.mBits.x,
                            reg_struct->l2_clip_start.mBits.y,
                            reg_struct->l2_clip_size.mBits.w,
                            reg_struct->l2_clip_size.mBits.h,
                            reg_struct->l2_des_start.mBits.x,
                            reg_struct->l2_des_start.mBits.y);
            }
            /*layer3 info*/
            if(reg_struct->mod1_cfg.mBits.l1_en) {
                gspn_printk("L3:fmt%d {pitch%04d (%d,%d)[%dx%d]->(%d,%d)}\n",
                            reg_struct->l3_cfg.mBits.format,
                            reg_struct->l3_pitch.mBits.w,
                            reg_struct->l3_clip_start.mBits.x,
                            reg_struct->l3_clip_start.mBits.y,
                            reg_struct->l3_clip_size.mBits.w,
                            reg_struct->l3_clip_size.mBits.h,
                            reg_struct->l3_des_start.mBits.x,
                            reg_struct->l3_des_start.mBits.y);
            }
        } else {
        }
    }
}



