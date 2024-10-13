#if defined(__UFS_CAL_LK__)		/* LK */
#include <platform/types.h>
#include <lk/reg.h>
#include <platform/sfr/ufs/ufs-vs-mmio.h>
#include <platform/sfr/ufs/ufs-cal-if.h>
#include <stdio.h>
#elif defined(__UFS_CAL_FW__)		/* FW */
#include <sys/types.h>
#include <bits.h>
#include "ufs-vs-mmio.h"
#include "ufs-cal-snr.h"
#include "ufs-cal-if.h"
#include <stdio.h>
#else					/* Kernel */
#include <linux/io.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "ufs-vs-mmio.h"
//#include "ufs-cal-if.h"
#define printf(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)
#endif

/*
 * CAL table, project specifics
 *
 * This is supposed to be in here, i.e.
 * right before definitions for version check.
 *
 * DO NOT MOVE THIS TO ANYWHERE RANDOMLY !!!
 */
#if defined(__UFS_CAL_LK__)             /* LK */
#include <platform/sfr/ufs/ufs-cal-snr.h>
#elif defined(__UFS_CAL_FW__)           /* FW */
#include "ufs-cal-snr.h"
#else                                   /* Kernel */
#include "ufs-cal-snr.h"
#endif

#include "ufs-cal-snr-if.h"

static ufs_cal_errno __write_sfr(struct ufs_vs_handle *handle,
				       struct ufs_cal_save_and_restore_cfg *cfg,
				       struct ufs_cal_param *p,
				       int lane)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	u32 *point_value;

	switch (cfg->ip_type) {
	/* hci */
	case HCI_STANDARD:
	case HCI_VENDOR_SPEC:
		hci_writel(handle, cfg->value1, cfg->save_addr);
		//printf("[Write HCI ]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->save_addr, cfg->value1, hci_readl(handle, cfg->save_addr));
		break;
	case UNIPRO:
		unipro_writel(handle, cfg->value1, cfg->save_addr);
		//printf("[Write UNIPRO]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->save_addr, cfg->value1, unipro_readl(handle, cfg->save_addr));
		break;
	case M_PHY_PCS_CMN:
		pcs_writel(handle, cfg->value1, cfg->save_addr);
		//printf("[Write M_PHY_PCS_CMN]0x%08x Value =  0x%x, READ SFR = 0x%x\n", PHY_PCS_COMN_ADDR(cfg->save_addr), cfg->value1, pcs_readl(handle, cfg->save_addr));
		break;
	case M_PHY_PCS_TRSV:
		point_value = (u32 *)&(cfg->value1) + lane;
		pcs_writel(handle, *point_value, PHY_PCS_TRSV_ADDR(cfg->save_addr, lane));
		//printf("[Write M_PHY_PCS_TRSV]0x%08x Value1 =  0x%x, Value2 = 0x%x, READ SFR = 0x%x\n", PHY_PCS_TRSV_ADDR(cfg->save_addr, lane), cfg->value1, cfg->value2, pcs_readl(handle, PHY_PCS_TRSV_ADDR(cfg->save_addr, lane)));
		break;
	case M_PHY_PMA_CMN:
		pma_writel(handle, cfg->value1, cfg->restore_addr);
		//printf("[Write M_PHY_PMA_CMN]0x%08x Value1 =  0x%x, READ SFR = 0x%x\n", PHY_PMA_COMN_ADDR(cfg->restore_addr), cfg->value1, pma_readl(handle, cfg->restore_addr));
		break;
	case M_PHY_PMA_TRSV:
		point_value = (u32 *)&(cfg->value1) + lane;
		pma_writel(handle, *point_value, PHY_PMA_TRSV_ADDR(cfg->restore_addr, lane));
		//printf("[Write M_PHY_PMA_TRSV]0x%08x Value1 =  0x%x, Value2 = 0x%x, READ SFR = 0x%x\n", PHY_PMA_TRSV_ADDR(cfg->restore_addr, lane), cfg->value1, cfg->value2, pma_readl(handle, PHY_PMA_TRSV_ADDR(cfg->restore_addr, lane)));
		break;
	default:
		break;
	}
	return ret;
}

static ufs_cal_errno __read_sfr(struct ufs_vs_handle *handle,
				       struct ufs_cal_save_and_restore_cfg *cfg,
				       struct ufs_cal_param *p,
				       int lane)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	u32 *point_value;

	switch (cfg->ip_type) {
	/* hci */
	case HCI_STANDARD:
	case HCI_VENDOR_SPEC:
		cfg->value1 = hci_readl(handle, cfg->save_addr);
		//printf("[Read HCI ]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->save_addr, cfg->value1, hci_readl(handle, cfg->save_addr));
		break;
	case UNIPRO:
		cfg->value1 = unipro_readl(handle, cfg->save_addr);
		//printf("[Read UNIPRO]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->save_addr, cfg->value1, unipro_readl(handle, cfg->save_addr));
		break;
	case M_PHY_PCS_CMN:
		cfg->value1 = pcs_readl(handle, cfg->save_addr);
		//printf("[Read M_PHY_PCS]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->save_addr, cfg->value1, pcs_readl(handle, cfg->save_addr));
		break;
	case M_PHY_PCS_TRSV:
		point_value = (u32 *)&(cfg->value1) + lane;
		*point_value = pcs_readl(handle, PHY_PCS_TRSV_ADDR(cfg->save_addr, lane));
		//printf("[Read M_PHY_PCS_TRSV]0x%08x Value =  0x%x,  Value2 =  0x%x, READ SFR = 0x%x\n", PHY_PCS_TRSV_ADDR(cfg->save_addr, lane), cfg->value1, cfg->value2, pcs_readl(handle, PHY_PCS_TRSV_ADDR(cfg->save_addr, lane)));
		break;
	case M_PHY_PMA_CMN:
		cfg->value1 = pma_readl(handle, cfg->save_addr);
		//printf("[Read M_PHY_PMA_CMN]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->save_addr, cfg->value1, pma_readl(handle, cfg->save_addr));
		break;
	case M_PHY_PMA_TRSV:
		point_value = (u32 *)&(cfg->value1) + lane;
		*point_value = pma_readl(handle, PHY_PMA_TRSV_ADDR(cfg->save_addr, lane));
		//printf("[Read M_PHY_PMA_TRSV]0x%08x Value =  0x%x, Value =  0x%x, READ SFR = 0x%x\n", PHY_PMA_TRSV_ADDR(cfg->save_addr, lane), cfg->value1, cfg->value2, pma_readl(handle, PHY_PMA_TRSV_ADDR(cfg->save_addr, lane)));
		break;
	default:
		break;
	}

	return ret;
}

static ufs_cal_errno __set_or_clear_sfr(struct ufs_vs_handle *handle,
				       struct ufs_cal_save_and_restore_cfg *cfg,
				        struct ufs_cal_param *p,
				       int lane)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	ret = __read_sfr(handle, cfg, p, lane);
	if (ret)
		goto out;

	if (lane == 0) {
		if (cfg->type == OVERRIDE_SFR_SET)
			cfg->value1 |= cfg->mask;
		else
			cfg->value1 &= ~(cfg->mask);
	} else {
		if (cfg->type == OVERRIDE_SFR_SET)
			cfg->value2 |= cfg->mask;
		else
			cfg->value2 &= ~(cfg->mask);
	}
	ret = __write_sfr(handle, cfg, p, lane);
	if (ret)
		goto out;

out:
	return ret;
}

static ufs_cal_errno __check_mask_bit(struct ufs_vs_handle *handle,
				       struct ufs_cal_save_and_restore_cfg *cfg,
				       struct ufs_cal_param *p,
				       int lane)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	u32 read_value = 0;
	u32 i = 0;

	do {
		switch (cfg->ip_type) {
		/* hci */
		case HCI_STANDARD:
		case HCI_VENDOR_SPEC:
			read_value = hci_readl(handle, cfg->save_addr);
			//printf("[__check_mask_bit ]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->save_addr, cfg->value1, hci_readl(handle, cfg->save_addr));
			break;
		case UNIPRO:
			read_value = unipro_readl(handle, cfg->save_addr);
			//printf("[__check_mask_bit]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->save_addr, cfg->value1, unipro_readl(handle, cfg->save_addr));
			break;
		case M_PHY_PCS_CMN:
			read_value = pcs_readl(handle, cfg->save_addr);
			//printf("[__check_mask_bit]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->save_addr, cfg->value1, pcs_readl(handle, cfg->save_addr));
			break;
		case M_PHY_PCS_TRSV:
			read_value = pcs_readl(handle, PHY_PCS_TRSV_ADDR(cfg->save_addr, lane));
			//printf("[__check_mask_bit]0x%08x Value =  0x%x, READ SFR = 0x%x\n", PHY_PCS_TRSV_ADDR(cfg->save_addr, lane), cfg->value1, pcs_readl(handle, PHY_PCS_TRSV_ADDR(cfg->save_addr, lane)));
			break;
		case M_PHY_PMA_CMN:
			read_value = pma_readl(handle, cfg->save_addr);
			//printf("[__check_mask_bit]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->save_addr, cfg->value1, pma_readl(handle, cfg->save_addr));
			break;
		case M_PHY_PMA_TRSV:
			read_value = pma_readl(handle, PHY_PCS_TRSV_ADDR(cfg->save_addr, lane));
			//printf("[__check_mask_bit]0x%08x Value =  0x%x, READ SFR = 0x%x\n", PHY_PMA_TRSV_ADDR(cfg->save_addr, lane), cfg->value1, pma_readl(handle, PHY_PCS_TRSV_ADDR(cfg->save_addr, lane)));
			break;
		default:
			break;
		}

		if (handle->udelay)
			handle->udelay(DELAY_PERIOD_IN_US);

		if(i++ > 200)
			return UFS_CAL_TIMEOUT;
	} while ((read_value & cfg->mask) != cfg->value1) ;

	return ret;
}

static ufs_cal_errno __write_mphy_override_bit(struct ufs_vs_handle *handle,
				       struct ufs_cal_save_and_restore_cfg *cfg,
				       struct ufs_cal_param *p,
				       int lane)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	if (p->save_and_restore_mode == SAVE_MODE) {

		switch (cfg->type) {
		/* hci */
		case PHY_OVERRIDE_PMA_BIAS_CAL:
			cfg->value1 = p->m_phy_bias;
			break;
		case PHY_OVERRIDE_PCS_PMA_HS_SERIES:
			cfg->value1 = (pcs_readl(handle, cfg->save_addr) & 0x3) >> 1;
			break;
		case PHY_OVERRIDE_PCS_PMA_HS_RX_GEAR:
		case PHY_OVERRIDE_PCS_PMA_HS_TX_GEAR:
				cfg->value1 = pcs_readl(handle, cfg->save_addr);
			break;
		default:
			break;
		}
		pma_writel(handle, cfg->value1, cfg->restore_addr);
	//printf("[__write_mphy_override_bit ]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->restore_addr, cfg->value1, pma_readl(handle, cfg->restore_addr));
	}

	if (p->save_and_restore_mode == SAVE_MODE)
		cfg->value1 |= cfg->mask;
	else if (p->save_and_restore_mode == RESTORE_MODE)
		cfg->value1 = pma_readl(handle, cfg->restore_addr) & (~(cfg->mask));
	else
		cfg->value1 = 0;

	pma_writel(handle, cfg->value1, cfg->restore_addr);
	//printf("[__write_mphy_override_bit ]0x%08x Value =  0x%x, READ SFR = 0x%x\n", cfg->restore_addr, cfg->value1, pma_readl(handle, cfg->restore_addr));

	return ret;
}

static ufs_cal_errno __save_and_shift_restore_mphy(struct ufs_vs_handle *handle,
				       struct ufs_cal_save_and_restore_cfg *cfg,
				       struct ufs_cal_param *p,
				       int lane)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	u32 read_value = 0;

	if (p->save_and_restore_mode == SAVE_MODE) {
		ret = __read_sfr(handle, cfg, p, lane);
		if (ret) {
			ret = UFS_CAL_ERROR;
			goto out;
		}

		if (lane == 0)
			cfg->value1 &= cfg->mask;
		else
			cfg->value2 &= cfg->mask;

	} else {
		/*
		 * different to write SFR by RATE of A/B
		 * so check to READ and Wrtie SFR ADDRESS BIT
		 * should add discuss about how to write address about 0x220, 0x224
		 */

		switch (cfg->ip_type) {
		/* PMA */
		case M_PHY_PMA_CMN:
			read_value = pma_readl(handle, cfg->restore_addr);
			break;
		case M_PHY_PMA_TRSV:
			read_value = pma_readl(handle, PHY_PMA_TRSV_ADDR(cfg->restore_addr, lane));
			break;
		default:
			break;
		}
		read_value &= ~(cfg->mask);

		if (lane == 0)
			cfg->value1 |= read_value;
		else
			cfg->value2 |= read_value;

		ret = __write_sfr(handle, cfg, p, lane);
		if (ret) {
			ret = UFS_CAL_ERROR;
			goto out;
		}
	}

out:
	return ret;
}

static ufs_cal_errno __write_sfr_oc_code(struct ufs_vs_handle *handle,
				       struct ufs_cal_save_and_restore_cfg *cfg,
				       struct ufs_cal_param *p,
				       int lane)
{
	u32 oc_value;

	switch (cfg->ip_type) {
	case M_PHY_PMA_TRSV:
		oc_value = (u32)p->backup_oc_code[lane][cfg->value1];
		pma_writel(handle, oc_value, PHY_PMA_TRSV_ADDR(cfg->restore_addr, lane));

		break;
	default:
		break;
	}

	return UFS_CAL_NO_ERROR;
}

static inline ufs_cal_errno __match_board_by_cfg(u8 board, u8 cfg_board)
{
	ufs_cal_errno match = UFS_CAL_ERROR;

	if (board & cfg_board)
		match = UFS_CAL_NO_ERROR;

	return match;
}

static ufs_cal_errno ufs_cal_save_restore_config_uic(struct ufs_cal_param *p,
					     struct ufs_cal_save_and_restore_cfg *cfg)
{
	struct ufs_vs_handle *handle = p->handle;
	u8 i = 0;
	ufs_cal_errno ret = UFS_CAL_INV_ARG;

	if (!cfg)
		goto out;

	ret = UFS_CAL_NO_ERROR;
	for (; cfg->type != CFG_NONE; cfg++) {
		for (i = 0; i < p->available_lane; i++) {
			if (p->board && UFS_CAL_ERROR ==
				__match_board_by_cfg(p->board, cfg->board))
				continue;

			if (i > 0)
				if (cfg->ip_type == HCI_STANDARD ||
					cfg->ip_type == HCI_VENDOR_SPEC ||
					cfg->ip_type == UNIPRO ||
					cfg->ip_type == M_PHY_PCS_CMN ||
					cfg->ip_type == M_PHY_PMA_CMN)
					continue;

			if (p->save_and_restore_mode == SAVE_MODE
					&& cfg->resume_mode == RESTORE_ONLY)
					continue;

			if (p->save_and_restore_mode == RESTORE_MODE
					&& cfg->resume_mode == SAVE_ONLY)
					continue;

			if (p->save_and_restore_mode == RESTORE_MODE
					&& cfg->resume_mode == OVERRIDE_SFR_CLEAR)
					continue;

			switch (cfg->type) {
			case WRITE_ONLY:
				ret = __write_sfr(handle, cfg, p, i);
				if (ret)
					goto out;
				break;
			case OVERRIDE_SFR_SET:
			case OVERRIDE_SFR_CLEAR:
				ret = __set_or_clear_sfr(handle, cfg, p, i);
					if (ret)
					goto out;
				break;
			case READ_WRITE_SFR:
				if (p->save_and_restore_mode == SAVE_MODE) {
					ret = __read_sfr(handle, cfg, p, i);
					if (ret)
						goto out;
				} else {
					ret = __write_sfr(handle, cfg, p, i);
					if (ret)
						goto out;
				}
				break;
			case READ_MASK_BIT:
					ret = __check_mask_bit(handle, cfg, p, i);
					if (ret)
						goto out;
				break;
			case READ_AND_SHIFT_WRITE_PMA:
					ret = __save_and_shift_restore_mphy(handle, cfg, p, i);
					if (ret)
						goto out;
				break;
			case DELAY_TIME:
					handle->udelay(cfg->value1);
				break;
			case PHY_OVERRIDE_PMA_BIAS_CAL:
			case PHY_OVERRIDE_PMA_REF_CLK_MODE:
			case PHY_OVERRIDE_PMA_REF_CLK_SEL:
			case PHY_OVERRIDE_PMA_TX_PWM_CLK_SEL:
			case PHY_OVERRIDE_PCS_PMA_HS_SERIES:
			case PHY_OVERRIDE_PCS_PMA_HS_RX_GEAR:
			case PHY_OVERRIDE_PCS_PMA_HS_TX_GEAR:
					ret = __write_mphy_override_bit(handle, cfg, p, i);
					if (ret)
						goto out;
				break;
			case PMA_AFC_OTP_CODE:
				cfg->value1 = (u8)p->pma_otp[cfg->save_addr - PMA_OTP_BASE];
				ret = __write_sfr(handle, cfg, p, i);
				if (ret)
					goto out;
				break;
			case PMA_AFC_OTP_CHECK:
				if ((p->pma_otp[cfg->save_addr - PMA_OTP_BASE] & 0xF) == cfg->mask) {
					ret = __write_sfr(handle, cfg, p, i);
					if (ret)
						goto out;
				}
				break;
			case WRITE_SFR_OC_CODE:
				ret = __write_sfr_oc_code(handle, cfg, p, i);
				break;
			default:
				break;
			}
		}
	}

out:
	return ret;
}

ufs_cal_errno ufs_cal_pre_pm(struct ufs_cal_param *p, IP_NAME ip_name)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	struct ufs_cal_save_and_restore_cfg *cfg;

	switch(ip_name) {
		case IP_HCI:
			cfg = save_hci_sfr;
			break;
		case IP_UNIPRO:
			cfg = save_unipro_sfr;
			break;
		case IP_PCS:
			cfg = save_pcs_sfr;
			break;
		case IP_PMA:
			if (p->evt_ver == 0) {
				cfg = save_pma_sfr_evt0;
			} else {
				cfg = save_pma_sfr_evt1;
			}
			break;
		default:
			break;
	}

	ret = ufs_cal_save_restore_config_uic(p, cfg);

	return ret;
}


ufs_cal_errno ufs_cal_hce_enable(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	struct ufs_cal_save_and_restore_cfg *cfg;

	cfg = hcs_dp_enable;

	ret = ufs_cal_save_restore_config_uic(p, cfg);

	return ret;
}

ufs_cal_errno ufs_cal_post_pm(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	struct ufs_cal_save_and_restore_cfg *cfg;

	cfg = override_mphy_sfr;

	ret = ufs_cal_save_restore_config_uic(p, cfg);

	return ret;
}

ufs_cal_errno ufs_cal_resume_hibern8(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	struct ufs_cal_save_and_restore_cfg *cfg;

	cfg = unipro_resume_hibern;

	ret = ufs_cal_save_restore_config_uic(p, cfg);

	return ret;
}

ufs_cal_errno ufs_cal_resume_unipro_dme_reset(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	struct ufs_cal_save_and_restore_cfg *cfg;

	cfg = unipro_wa_enable;

	ret = ufs_cal_save_restore_config_uic(p, cfg);

	return ret;
}

ufs_cal_errno ufs_save_register(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	p->handle->udelay(p->ah8_brefclkgatingwaittime  + 5);
	p->vops->gate_clk(p);
	p->vops->ctrl_gpio(p);

	ret = ufs_cal_pre_pm(p, IP_UNIPRO);
	if (ret != UFS_CAL_NO_ERROR) {
		ret = UFS_CAL_ERROR;
		goto out;
	}

	ret = ufs_cal_pre_pm(p, IP_PCS);
	if (ret != UFS_CAL_NO_ERROR) {
		ret = UFS_CAL_ERROR;
		goto out;
	}

	ret = ufs_cal_pre_pm(p, IP_HCI);
	if (ret != UFS_CAL_NO_ERROR) {
		ret = UFS_CAL_ERROR;
		goto out;
	}

	if (p->snr_phy_power_on == false) {
		ret = ufs_cal_pre_pm(p, IP_PMA);
		if (ret != UFS_CAL_NO_ERROR) {
			ret = UFS_CAL_ERROR;
			goto out;
		}
	} else {	//(p->snr_phy_power_on == true)
		ret =  ufs_cal_post_pm(p);
		if (ret != UFS_CAL_NO_ERROR) {
			ret = UFS_CAL_ERROR;
			goto out;
		}
	}

out:
	return ret;
}

ufs_cal_errno ufs_restore_register(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	ret = ufs_cal_hce_enable(p);
	if (ret != UFS_CAL_NO_ERROR) {
		ret = UFS_CAL_ERROR;
		goto out;
	}

	p->vops->ctrl_gpio(p);
	p->vops->pad_retention(p);

	if (p->snr_phy_power_on == false) {
		ret = ufs_cal_pre_pm(p, IP_PMA);
		if (ret != UFS_CAL_NO_ERROR) {
			ret = UFS_CAL_ERROR;
			goto out;
		}

		ret = ufs_cal_resume_unipro_dme_reset(p);
		if (ret != UFS_CAL_NO_ERROR) {
			ret = UFS_CAL_ERROR;
			goto out;
		}
	}

	ret = ufs_cal_pre_pm(p, IP_UNIPRO);
	if (ret != UFS_CAL_NO_ERROR) {
		ret = UFS_CAL_ERROR;
		goto out;
	}

	ret = ufs_cal_pre_pm(p, IP_PCS);
	if (ret != UFS_CAL_NO_ERROR) {
		ret = UFS_CAL_ERROR;
		goto out;
	}

	ret = ufs_cal_pre_pm(p, IP_HCI);
	if (ret != UFS_CAL_NO_ERROR) {
		ret = UFS_CAL_ERROR;
		goto out;
	}

	p->vops->gate_clk(p);
	p->handle->udelay(10);

	if (p->snr_phy_power_on == true) {
		p->vops->pmu_input_iso(p);

		ret = ufs_cal_post_pm(p);
		if (ret != UFS_CAL_NO_ERROR) {
			ret = UFS_CAL_ERROR;
			goto out;
		}
	}

	ret = ufs_cal_resume_hibern8(p);
	if (ret != UFS_CAL_NO_ERROR) {
		ret = UFS_CAL_ERROR;
		goto out;
	}

out:
	return ret;
}

ufs_cal_errno ufs_cal_snr_store(struct ufs_cal_param *p)
{
	struct ufs_cal_save_and_restore_cfg *cfg;

	cfg = p->evt_ver ? save_pma_sfr_evt1 : save_pma_sfr_evt0;

	while (1) {
		if (!cfg->save_addr)
			break;

		if (cfg->save_addr == p->addr) {
			cfg->value1 = p->value;
			cfg->value2 = p->value;

			return UFS_CAL_NO_ERROR;
		}

		cfg++;
	}

	return UFS_CAL_INV_ARG;
}

ufs_cal_errno ufs_cal_snr_show(struct ufs_cal_param *p)
{
	struct ufs_cal_save_and_restore_cfg *cfg;

	cfg = p->evt_ver ? save_pma_sfr_evt1 : save_pma_sfr_evt0;

	while (1) {
		if (!cfg->save_addr)
			break;

		if (cfg->save_addr == p->addr) {
			p->value = cfg->value1;

			return UFS_CAL_NO_ERROR;
		}

		cfg++;
	}

	return UFS_CAL_INV_ARG;
}

ufs_cal_errno ufs_cal_snr_init(struct ufs_cal_param *p)
{
	struct ufs_cal_save_and_restore_cfg *cfg, *cfg_over;

	if (p->overwrite) {
		cfg_over = p->evt_ver ? save_pma_evt1_overwrite[p->overwrite - 1]
			: save_pma_evt0_overwrite[p->overwrite - 1];

		while (cfg_over->save_addr) {
			cfg = p->evt_ver ? save_pma_sfr_evt1 : save_pma_sfr_evt0;

			while ((cfg->save_addr != cfg_over->save_addr)
					&& cfg->save_addr)
				cfg++;

			cfg->value1 = cfg_over->value1;
			cfg->value2 = cfg_over->value2;
			cfg_over++;
		}
	}

	return UFS_CAL_NO_ERROR;
}
