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
 * GNU General gLicense for more details.
 */

/**---------------------------------------------------------------------------*
**                         Dependencies                                      *
**---------------------------------------------------------------------------*/


#include "gsp_config_if.h"
#define REG_GET(reg)                (*(volatile uint32_t*)(reg))
#define REG_SET(reg,value)          (*(volatile uint32_t*)(reg) = (value))
#define REG_OR(reg,value)           (*(volatile uint32_t*)(reg) |= (value))
#define REG_CLR(reg,value)			(*(volatile uint32_t*)(reg) &= (~(value)))



/**---------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
 **                         Macro Definition                                  *
 **---------------------------------------------------------------------------*/


/**---------------------------------------------------------------------------*
 **                         Function Define                                   *
 **---------------------------------------------------------------------------*/

static void gsp_set_layer0_param(struct gsp_kcfg_info *kcfg,
		unsigned long reg_addr)
{
	struct gsp_cfg_info *cfg = &kcfg->cfg;
    if(!cfg->layer0_info.layer_en) {
        GSP_L0_ENABLE_SET(0, reg_addr);
        return ;
    }

    GSP_L0_ADDR_SET(cfg->layer0_info.src_addr, reg_addr);
    GSP_L0_PITCH_SET(cfg->layer0_info.pitch, reg_addr);
    GSP_L0_CLIPRECT_SET(cfg->layer0_info.clip_rect, reg_addr);
    GSP_L0_DESRECT_SET(cfg->layer0_info.des_rect, reg_addr);
    GSP_L0_GREY_SET(cfg->layer0_info.grey, reg_addr);
    GSP_L0_ENDIAN_SET(cfg->layer0_info.endian_mode, reg_addr);
    GSP_L0_ALPHA_SET(cfg->layer0_info.alpha, reg_addr);
    GSP_L0_COLORKEY_SET(cfg->layer0_info.colorkey, reg_addr);
    GSP_L0_IMGFORMAT_SET(cfg->layer0_info.img_format, reg_addr);
    GSP_L0_ROTMODE_SET(cfg->layer0_info.rot_angle, reg_addr);
    GSP_L0_COLORKEYENABLE_SET(cfg->layer0_info.colorkey_en, reg_addr);
    GSP_L0_PALLETENABLE_SET(cfg->layer0_info.pallet_en, reg_addr);
}


static void gsp_set_layer1_param(struct gsp_kcfg_info *kcfg,
		unsigned long reg_addr)
{
	struct gsp_cfg_info *cfg = &kcfg->cfg;
    if(!cfg->layer1_info.layer_en) {
        GSP_L1_ENABLE_SET(0, reg_addr);
        return ;
    }

    GSP_L1_ADDR_SET(cfg->layer1_info.src_addr, reg_addr);
    GSP_L1_PITCH_SET(cfg->layer1_info.pitch, reg_addr);
    GSP_L1_CLIPRECT_SET(cfg->layer1_info.clip_rect, reg_addr);
    GSP_L1_DESPOS_SET(cfg->layer1_info.des_pos, reg_addr);
    GSP_L1_GREY_SET(cfg->layer1_info.grey, reg_addr);
    GSP_L1_ENDIAN_SET(cfg->layer1_info.endian_mode, reg_addr);
    GSP_L1_ALPHA_SET(cfg->layer1_info.alpha, reg_addr);
    GSP_L1_COLORKEY_SET(cfg->layer1_info.colorkey, reg_addr);
    GSP_L1_IMGFORMAT_SET(cfg->layer1_info.img_format, reg_addr);
    GSP_L1_ROTMODE_SET(cfg->layer1_info.rot_angle, reg_addr);
    GSP_L1_COLORKEYENABLE_SET(cfg->layer1_info.colorkey_en, reg_addr);
    GSP_L1_PALLETENABLE_SET(cfg->layer1_info.pallet_en, reg_addr);

}


static void gsp_set_layerd_param(struct gsp_kcfg_info *kcfg,
		unsigned long reg_addr)
{
	struct gsp_cfg_info *cfg = &kcfg->cfg;
    if (!cfg->layer0_info.layer_en && !cfg->layer1_info.layer_en) {
        return ;
    }

    GSP_Ld_ADDR_SET(cfg->layer_des_info.src_addr, reg_addr);
    GSP_Ld_PITCH_SET(cfg->layer_des_info.pitch, reg_addr);
    GSP_Ld_ENDIAN_SET(cfg->layer_des_info.endian_mode, reg_addr);
    GSP_Ld_IMGFORMAT_SET(cfg->layer_des_info.img_format, reg_addr);
    GSP_Ld_COMPRESSRGB888_SET(cfg->layer_des_info.compress_r8_en, reg_addr);
}

static void gsp_set_misc_param(struct gsp_kcfg_info *kcfg,
		unsigned long reg_addr)
{
	struct gsp_cfg_info *cfg = &kcfg->cfg;
    if(!cfg->layer0_info.layer_en && !cfg->layer1_info.layer_en) {
        return ;
    }

    GSP_L0_ENABLE_SET(cfg->layer0_info.layer_en, reg_addr);
    GSP_L1_ENABLE_SET(cfg->layer1_info.layer_en, reg_addr);

    if(cfg->layer0_info.scaling_en == 1) {
        GSP_SCALESTATUS_RESET(reg_addr);
    }
    GSP_SCALE_ENABLE_SET(cfg->layer0_info.scaling_en, reg_addr);


    GSP_PMARGB_ENABLE_SET(cfg->layer0_info.pmargb_en
						||cfg->layer1_info.pmargb_en,
							reg_addr);
    GSP_L0_PMARGBMODE_SET(cfg->layer0_info.pmargb_mod, reg_addr);
    GSP_L1_PMARGBMODE_SET(cfg->layer1_info.pmargb_mod, reg_addr);
    GSP_PAGES_BOARDER_SPLIT_SET(cfg->misc_info.split_pages, reg_addr);
    GSP_Y2R_OPT_SET(cfg->misc_info.y2r_opt, reg_addr);
    GSP_DITHER_ENABLE_SET(cfg->misc_info.dithering_en, reg_addr);
    GSP_CLOCK_SET(cfg->misc_info.gsp_clock);
    GSP_EMC_GAP_SET(cfg->misc_info.gsp_gap, reg_addr);
}

void gsp_module_enable(struct gsp_context *gsp_ctx)
{
    int ret = 0;
    if(gsp_ctx->gsp_clk != NULL) {
        ret = clk_prepare_enable(gsp_ctx->gsp_clk);
        if(ret) {
            GSP_ERR("%s: enable clock failed!\n",__func__);
            return;
        } else {
            GSP_DEBUG("%s: enable clock ok!\n",__func__);
        }
    } else {
        GSP_ERR("%s: gsp_clk not init yet!\n",__func__);
    }
}

void gsp_module_disable(struct gsp_context *gsp_ctx)
{
    if(gsp_ctx->gsp_clk != NULL) {
        //GSP_HWMODULE_DISABLE();//disable may not use the enable regiter
        clk_disable_unprepare(gsp_ctx->gsp_clk);
    } else {
        GSP_ERR("%s: gsp_clk not init yet!\n",__func__);
    }
}

/*
func:gsp_clock_check_phase0
desc: check all clock except iommu
*/
int gsp_clock_check_phase0(unsigned long reg_addr)
{
    int ret = 0;

    /*check GSP enable*/
    if(0 == (GSP_REG_READ(GSP_MOD_EN) & GSP_MOD_EN_BIT)) {
        GSP_ERR("%s: err: gsp enable is not set!%lx:%08x\n",__func__,
               (unsigned long)GSP_MOD_EN,GSP_REG_READ(GSP_MOD_EN));
        ret++;
    } else {
        if(GSP_WORKSTATUS_GET(reg_addr) != 0) {
            GSP_ERR("%s: err:busy is still on!!!!\n",__func__);
            ret++;
        }
    }

    /*check GSP clock select*/
    if(GSP_CLK_SEL_BIT_MASK !=
			(GSP_REG_READ(GSP_CLOCK_BASE) & GSP_CLK_SEL_BIT_MASK)) {
        GSP_ERR("%s: info: gsp clock select is not set to hightest freq!%lx:%08x\n",__func__,
               (unsigned long)GSP_CLOCK_BASE, GSP_REG_READ(GSP_CLOCK_BASE));
    }

    /*check GSP AUTO_GATE clock*/
    if(0 == (GSP_REG_READ(GSP_AUTO_GATE_ENABLE_BASE)
				&GSP_AUTO_GATE_ENABLE_BIT)) {
        GSP_ERR("%s: err: gsp auto gate clock is not enable!%lx:%08x\n",__func__,
               (unsigned long)GSP_AUTO_GATE_ENABLE_BASE, GSP_REG_READ(GSP_AUTO_GATE_ENABLE_BASE));
        ret++;
    }

    /*check GSP EMC clock*/
    if(0 == (GSP_REG_READ(GSP_EMC_MATRIX_BASE) & GSP_EMC_MATRIX_BIT)) {
        GSP_ERR("%s: err: gsp emc clock is not enable!%lx:%08x\n",__func__,
               (unsigned long)GSP_EMC_MATRIX_BASE,GSP_REG_READ(GSP_EMC_MATRIX_BASE));
        ret++;
    }
    return (ret>0)?GSP_KERNEL_CLOCK_ERR:GSP_NO_ERR;
}

/*
func:gsp_clock_check_phase1
desc: check iommu cfg
*/
int gsp_clock_check_phase1(unsigned long gsp_reg_addr,
		unsigned long mmu_reg_addr)
{
    int ret = 0;
    uint32_t ctl_val = GSP_REG_READ(mmu_reg_addr);
    uint32_t addr_y0 = GSP_L0_ADDRY_GET(gsp_reg_addr);
    uint32_t addr_uv0 = GSP_L0_ADDRUV_GET(gsp_reg_addr);
    uint32_t addr_va0 = GSP_L0_ADDRVA_GET(gsp_reg_addr);
    uint32_t addr_y1 = GSP_L1_ADDRY_GET(gsp_reg_addr);
    uint32_t addr_uv1 = GSP_L1_ADDRUV_GET(gsp_reg_addr);
    uint32_t addr_va1 = GSP_L1_ADDRVA_GET(gsp_reg_addr);
    uint32_t addr_yd = GSP_Ld_ADDRY_GET(gsp_reg_addr);
    uint32_t addr_uvd = GSP_Ld_ADDRUV_GET(gsp_reg_addr);
    uint32_t addr_vad = GSP_Ld_ADDRVA_GET(gsp_reg_addr);

#define IOVA_CHECK(addr)    (0x10000000<= (addr) && (addr) < 0x80000000)

    /*check GSP IOMMU ENABLE*/
    if(((ctl_val & 0x1)==0 || (ctl_val & 0xF0000000)==0)/*IOMMU be disabled*/
    && (((GSP_L0_ENABLE_GET(gsp_reg_addr) == 1)
	&& (IOVA_CHECK(addr_y0)
	|| IOVA_CHECK(addr_uv0)
	|| IOVA_CHECK(addr_va0)))/*L0 is enabled and use iova*/
    || ((GSP_L1_ENABLE_GET(gsp_reg_addr) == 1)
	&& (IOVA_CHECK(addr_y1)
	|| IOVA_CHECK(addr_uv1)
	|| IOVA_CHECK(addr_va1)))/*L1 is enabled and use iova*/
    || ((GSP_L0_ENABLE_GET(gsp_reg_addr) == 1
	|| GSP_L1_ENABLE_GET(gsp_reg_addr) == 1)
	&& (IOVA_CHECK(addr_yd)
	|| IOVA_CHECK(addr_uvd)
	|| IOVA_CHECK(addr_vad))))) {
        GSP_ERR("%s: err: gsp iommu is not enable or iova base is null!%lx:%08x\n",__func__,
               (unsigned long)mmu_reg_addr, GSP_REG_READ(mmu_reg_addr));
        ret++;
    }
    return (ret > 0)?GSP_KERNEL_CLOCK_ERR:GSP_NO_ERR;
}

int gsp_enable(struct gsp_context *gsp_ctx)
{
    int ret = 0;
    gsp_module_enable(gsp_ctx);
    ret = clk_prepare_enable(gsp_ctx->gsp_emc_clk);
    //GSP_AUTO_GATE_ENABLE();//gsp driver take charge of auto_gate bit instead of pm
    if(ret) {
        GSP_ERR("%s: enable emc clock failed!\n",__func__);
        return GSP_KERNEL_CLOCK_ERR;
    } else {
        GSP_DEBUG("%s: enable emc clock ok!\n",__func__);
    }

    //GSP_HWMODULE_SOFTRESET();//sharkL bug 350028
    GSP_IRQMODE_SET(GSP_IRQ_MODE_LEVEL, gsp_ctx->gsp_reg_addr);
    ret = gsp_clock_check_phase0(gsp_ctx->gsp_reg_addr);
    return ret;
}

void gsp_disable(struct gsp_context *ctx)
{
	unsigned long reg_addr = ctx->gsp_reg_addr;
    clk_disable_unprepare(ctx->gsp_emc_clk);
    GSP_IRQSTATUS_CLEAR(reg_addr);
    GSP_IRQENABLE_SET(GSP_IRQ_TYPE_DISABLE, reg_addr);
    gsp_module_disable(ctx);
}

void gsp_config_layer(GSP_MODULE_ID_E layer_id, struct gsp_kcfg_info *kcfg,
		unsigned long gsp_reg_addr)
{
    switch(layer_id) {
        case GSP_MODULE_LAYER0:
            gsp_set_layer0_param(kcfg, gsp_reg_addr);
            break;

        case GSP_MODULE_LAYER1:
            gsp_set_layer1_param(kcfg, gsp_reg_addr);
            break;

        case GSP_MODULE_DST:
            gsp_set_layerd_param(kcfg, gsp_reg_addr);
            break;

        default:
            gsp_set_misc_param(kcfg, gsp_reg_addr);
            break;
    }
}

uint32_t gsp_trigger(unsigned long gsp_reg_addr,
		unsigned long mmu_reg_addr)
{
    int ret = gsp_clock_check_phase1(gsp_reg_addr, mmu_reg_addr);
    if(ret) {
        return ret;
    }
    if(GSP_ERRFLAG_GET(gsp_reg_addr))
        return GSP_ERRCODE_GET(gsp_reg_addr);

    GSP_IRQENABLE_SET(GSP_IRQ_TYPE_ENABLE, gsp_reg_addr);
    GSP_ENGINE_TRIGGER(gsp_reg_addr);
    return 0;
}

/*
func:gsp_TryToRecover
desc: check all clock except iommu
*/
int gsp_try_to_recover(struct gsp_context *ctx)
{
    GSP_REG_T *reg_base = NULL;
    uint32_t* base = NULL;
    uint32_t mask = 0;

	if (NULL == ctx) {
		GSP_ERR("try to recover with null context\n");
		return -1;
	}

    if (strncmp(ctx->name, "gsp-ctx", sizeof(ctx->name))) {
		GSP_ERR("get an error gsp context\n");
        return -1;
    }
    reg_base = (GSP_REG_T *)ctx->gsp_reg_addr;

    /*check gsp clock select*/
    base = (uint32_t *)GSP_CLOCK_BASE;
    mask = 0x3;
    if(mask != (REG_GET(base)&mask)) {
        GSP_DEBUG("gsp clock select is not set to hightest freq! 0x%08x\n",
                  REG_GET(base));
    }

    /*check gsp module enable*/
    base = (uint32_t)GSP_MOD_EN;
    mask = GSP_MOD_EN_BIT;
    if(mask != (REG_GET(base)&mask)) {
        GSP_ERR("gsp enable is not set! 0x%08x & 0x%08x\n",
                  REG_GET(base), mask);
        REG_OR(base,mask);
    }

    /*check gsp module reset*/
    base = (uint32_t *)GSP_SOFT_RESET;
    mask = GSP_SOFT_RST_BIT;
    if(mask == (REG_GET(base) & mask)) {
        GSP_ERR("gsp reset is set! 0x%08x & 0x%08x\n",
                  REG_GET(base), mask);
        REG_CLR(base,mask);
    } else {
        GSP_DEBUG("reset gsp. 0x%08x & 0x%08x\n",
                  REG_GET(base), mask);
		GSP_HWMODULE_SOFTRESET(reg_base);
    }

    /*check gsp AUTO_GATE clock*/
    base = (uint32_t *)GSP_AUTO_GATE_ENABLE_BASE;
    mask = GSP_AUTO_GATE_ENABLE_BIT;
    if(mask != (REG_GET(base) & mask)) {
        GSP_ERR("gsp auto gate is disabled! 0x%08x & 0x%08x\n",
                  REG_GET(base), mask);
        REG_OR(base,mask);
    }

    /*check gsp EMC clock*/
    base = (uint32_t *)GSP_EMC_MATRIX_BASE;
    mask = GSP_EMC_MATRIX_BIT;
    if(mask != (REG_GET(base) & mask)) {
        GSP_ERR("gsp emc is disabled! 0x%08x & 0x%08x\n",
                  REG_GET(base), mask);
        REG_OR(base,mask);
    }

    return GSP_NO_ERR;
}
