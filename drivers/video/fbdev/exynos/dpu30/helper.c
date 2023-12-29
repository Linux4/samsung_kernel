/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Helper file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <asm/cacheflush.h>
#include <asm/page.h>
#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
#include <soc/samsung/exynos-smc.h>
#endif

#include "decon.h"
#include "dsim.h"
#include "dpp.h"
#if defined(CONFIG_EXYNOS_DISPLAYPORT)
#include "displayport.h"
#endif
#include <video/mipi_display.h>

bool decon_intersect(struct decon_rect *r1, struct decon_rect *r2)
{
	return !(r1->left > r2->right || r1->right < r2->left ||
		r1->top > r2->bottom || r1->bottom < r2->top);
}

int decon_intersection(struct decon_rect *r1,
			struct decon_rect *r2, struct decon_rect *r3)
{
	r3->top = max(r1->top, r2->top);
	r3->bottom = min(r1->bottom, r2->bottom);
	r3->left = max(r1->left, r2->left);
	r3->right = min(r1->right, r2->right);
	return 0;
}

bool is_decon_rect_differ(struct decon_rect *r1, struct decon_rect *r2)
{
	return ((r1->left != r2->left) || (r1->top != r2->top) ||
		(r1->right != r2->right) || (r1->bottom != r2->bottom));
}

bool is_scaling(struct decon_win_config *config)
{
	return (config->dst.w != config->src.w) || (config->dst.h != config->src.h);
}

bool is_full(struct decon_rect *r, struct exynos_panel_info *lcd)
{
	return (r->left == 0) && (r->top == 0) &&
		(r->right == lcd->xres - 1) && (r->bottom == lcd->yres - 1);
}

void dpu_unify_rect(struct decon_rect *r1, struct decon_rect *r2,
		struct decon_rect *dst)
{
	dst->top = min(r1->top, r2->top);
	dst->bottom = max(r1->bottom, r2->bottom);
	dst->left = min(r1->right, r2->right);
	dst->right = max(r1->right, r2->right);
}

void decon_to_psr_info(struct decon_device *decon, struct decon_mode_info *psr)
{
	psr->psr_mode = decon->dt.psr_mode;
	psr->trig_mode = decon->dt.trig_mode;
	psr->dsi_mode = decon->dt.dsi_mode;
	psr->out_type = decon->dt.out_type;
}

void decon_to_init_param(struct decon_device *decon, struct decon_param *p)
{
	struct exynos_panel_info *lcd_info = decon->lcd_info;
	struct v4l2_mbus_framefmt mbus_fmt;

	mbus_fmt.width = 0;
	mbus_fmt.height = 0;
	mbus_fmt.code = 0;
	mbus_fmt.field = 0;
	mbus_fmt.colorspace = 0;

	p->lcd_info = lcd_info;
	p->psr.psr_mode = decon->dt.psr_mode;
	p->psr.trig_mode = decon->dt.trig_mode;
	p->psr.dsi_mode = decon->dt.dsi_mode;
	p->psr.out_type = decon->dt.out_type;
	p->nr_windows = decon->dt.max_win;
	p->disp_ss_regs = decon->res.ss_regs;
	decon_dbg("%s: psr(%d) trig(%d) dsi(%d) out(%d) wins(%d) LCD[%d %d]\n",
			__func__, p->psr.psr_mode, p->psr.trig_mode,
			p->psr.dsi_mode, p->psr.out_type, p->nr_windows,
			decon->lcd_info->xres, decon->lcd_info->yres);
}

void dpu_debug_printk(const char *function_name, const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk(KERN_INFO "[%s] %pV", function_name, &vaf);

	va_end(args);
}

void __iomem *dpu_get_sysreg_addr(void)
{
	void __iomem *regs;

	if (of_have_populated_dt()) {
		struct device_node *nd;
		nd = of_find_compatible_node(NULL, NULL,
				"samsung,exynos9-disp_ss");
		if (!nd) {
			decon_err("failed find compatible node(sysreg-disp)");
			return NULL;
		}

		regs = of_iomap(nd, 0);
		if (!regs) {
			decon_err("Failed to get sysreg-disp address.");
			return NULL;
		}
	} else {
		decon_err("failed have populated device tree");
		return NULL;
	}

	return regs;
}

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
static int decon_get_protect_id(int dma_id)
{
	int prot_id = 0;

	switch (dma_id) {
	case 0:	/* VG0 */
		prot_id = PROT_L0;
		break;
	case 1: /* G1 */
		prot_id = PROT_L1;
		break;
	case 2: /* G2 */
		prot_id = PROT_L2;
		break;
	case 3: /* G0 */
		prot_id = PROT_L3;
		break;
	default:
		decon_err("Unknown DMA_ID (%d)\n", dma_id);
		break;
	}

	return prot_id;
}

static int decon_control_protection(int ch, bool en)
{
	int ret = SUCCESS_EXYNOS_SMC;
	int prot_id;

	/* content protection uses dma type */
	prot_id = decon_get_protect_id(ch);
	ret = exynos_smc(SMC_PROTECTION_SET, 0, prot_id,
		(en ? SMC_PROTECTION_ENABLE : SMC_PROTECTION_DISABLE));

	if (ret)
		decon_err("DMA CH%d (en=%d): exynos_smc call fail (err=%d)\n",
			ch, en, ret);
	else
		decon_dbg("DMA CH%d protection %s\n",
			ch, en ? "enabled" : "disabled");

	return ret;
}

void decon_set_protected_content(struct decon_device *decon,
		struct decon_reg_data *regs)
{
	bool en;
	int ch, i, ret = 0;
	u32 change = 0;
	u32 cur_protect_bits = 0;

	/* IDMA protection configs */
	for (i = 0; i < decon->dt.max_win; i++) {
		if (!regs)
			break;

		cur_protect_bits |=
			(regs->protection[i] << regs->dpp_config[i].channel);
	}

	/* ODMA protection config (WB: writeback) */
	if (decon->dt.out_type == DECON_OUT_WB)
		if (regs)
			cur_protect_bits |= (regs->protection[decon->dt.max_win] << ODMA_WB);

	if (decon->prev_protection_bitmask != cur_protect_bits) {

		/* apply protection configs for each DPP channel */
		for (ch = 0; ch < decon->dt.dpp_cnt; ch++) {
			en = cur_protect_bits & (1 << ch);

			change = (cur_protect_bits & (1 << ch)) ^
				(decon->prev_protection_bitmask & (1 << ch));

			if (change)
				ret = decon_control_protection(ch, en);
		}
	}

	/* save current portection configs */
	decon->prev_protection_bitmask = cur_protect_bits;
}
#endif

#if defined(DPU_DUMP_BUFFER_IRQ)
/* id : VGF0=0, VGF1=1 */
static void dpu_dump_data_to_console(void *v_addr, int buf_size, int id)
{
	dpp_info("=== (CH#%d) Frame Buffer Data(128 Bytes) ===\n", id);

	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			v_addr, buf_size, false);
}

void dpu_dump_afbc_info(void)
{
	int i, j;
	struct decon_device *decon;
	struct dpu_afbc_info *afbc_info;
	void *v_addr[MAX_DECON_WIN];
	int size[MAX_DECON_WIN];
	int decon_cnt;

	decon_cnt = get_decon_drvdata(0)->dt.decon_cnt;

	for (i = 0; i < decon_cnt; i++) {
		decon = get_decon_drvdata(i);
		if (decon == NULL)
			continue;

		afbc_info = &decon->d.prev_afbc_info;
		decon_info("%s: previous AFBC channel information\n", __func__);
		for (j = 0; j < decon->dt.max_win; ++j) { /* all the dpp that has afbc */
			if (!afbc_info->is_afbc[j])
				continue;

			v_addr[j] = dma_buf_vmap(afbc_info->dma_buf[j]);
			if (IS_ERR_OR_NULL(v_addr[j])) {
				decon_err("%s: Failed to get v_addr (err %pK)\n", __func__, v_addr[j]);
				return;
			}
			size[j] = afbc_info->dma_buf[j]->size;
			decon_info("\t[DMA%d] Base(0x%p), KV(0x%p), size(%d)\n",
					j, (void *)afbc_info->dma_addr[j],
					v_addr[j], size[j]);
			dma_buf_vunmap(afbc_info->dma_buf[j], v_addr[j]);
		}

		afbc_info = &decon->d.cur_afbc_info;
		decon_info("%s: current AFBC channel information\n", __func__);
		for (j = 0; j < decon->dt.max_win; ++j) { /* all the dpp that has afbc */
			if (!afbc_info->is_afbc[j])
				continue;

			v_addr[j] = dma_buf_vmap(afbc_info->dma_buf[j]);
			if (IS_ERR_OR_NULL(v_addr[j])) {
				decon_err("%s: Failed to get v_addr (err %pK)\n", __func__, v_addr[j]);
				return;
			}
			size[j] = afbc_info->dma_buf[j]->size;
			decon_info("\t[DMA%d] Base(0x%p), KV(0x%p), size(%d)\n",
					j, (void *)afbc_info->dma_addr[j],
					v_addr[j], size[j]);
			dma_buf_vunmap(afbc_info->dma_buf[j], v_addr[j]);
		}
	}
}

static int dpu_dump_buffer_data(struct dpp_device *dpp)
{
	int i;
	int id_idx = 0;
	int dump_size = 128;
	int decon_cnt;
	struct decon_device *decon;
	struct dpu_afbc_info *afbc_info;

	decon_cnt = get_decon_drvdata(0)->dt.decon_cnt;

	if (dpp->state == DPP_STATE_ON) {

		for (i = 0; i < decon_cnt; i++) {
			decon = get_decon_drvdata(i);
			if (decon == NULL)
				continue;

			id_idx = dpp->id;

			afbc_info = &decon->d.cur_afbc_info;
			if (!afbc_info->is_afbc[id_idx])
				continue;

			if (afbc_info->dma_buf[id_idx]->size > 2048)
				dump_size = 128;
			else
				dump_size = afbc_info->dma_buf[id_idx]->size / 16;

			decon_info("Base(0x%p), KV(0x%p), size(%d)\n",
				(void *)afbc_info->dma_addr[id_idx],
				afbc_info->dma_v_addr[id_idx], dump_size);

			dpu_dump_data_to_console(afbc_info->dma_v_addr[id_idx],
					dump_size, dpp->id);
		}
	}

	return 0;
}
#endif

#define MAX_IOMMU_FAULT_RETRY_CNT	3

int dpu_sysmmu_fault_handler_dsim(struct iommu_fault *fault, void *data)
{
	struct decon_device *decon = NULL;
	struct dpp_device *dpp = NULL;
	int i;
	struct dsim_device *dsim = (struct dsim_device *)data;
	struct device *dev = dsim->dev;
	struct iommu_domain *domain = iommu_get_domain_for_dev(dev);

	decon = get_decon_drvdata(0);

	for (i = 0; i < decon->dt.dpp_cnt; i++) {
		if (test_bit(i, &decon->prev_used_dpp)) {
			dpp = get_dpp_drvdata(i);
#if defined(DPU_DUMP_BUFFER_IRQ)
			dpu_dump_buffer_data(dpp);
#endif
		}
	}

	decon_dump(decon, false);

	if(dsim->iommu_fault_retry_cnt < MAX_IOMMU_FAULT_RETRY_CNT) {
		if (fault->type == IOMMU_FAULT_DMA_UNRECOV) {
			if (((void *)iommu_iova_to_phys(domain, fault->event.addr)) != NULL) {
				decon_info("%s: Retry since IOVA is valid \n", __func__);
				dsim->iommu_fault_retry_cnt++;
				return -EAGAIN;
			}
		} else if (fault->type == IOMMU_FAULT_PAGE_REQ) {
			if (((void *)iommu_iova_to_phys(domain, fault->prm.addr)) != NULL) {
				decon_info("%s: Retry since IOVA is valid \n", __func__);
				dsim->iommu_fault_retry_cnt++;
				return -EAGAIN;
			}
		}
	}

	/* iommu_fault_retry_cnt will be reset, either on valid dsim interrupt
	 * or when iommu_fault_retry_cnt reaches  MAX_IOMMU_FAULT_RETRY_CNT
	 */
	dsim->iommu_fault_retry_cnt = 0;

	return 0;
}

int register_lcd_status_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&lcd_status_notifier_list, nb);
}
EXPORT_SYMBOL(register_lcd_status_notifier);

int unregister_lcd_status_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&lcd_status_notifier_list, nb);
}
EXPORT_SYMBOL(unregister_lcd_status_notifier);

/* If lcd status is
 *	0 : LCD ON
 *	1 : LCD OFF
 */
void lcd_status_notifier(u32 lcd_status)
{
	atomic_notifier_call_chain(&lcd_status_notifier_list, lcd_status, NULL);
}

#if IS_ENABLED(CONFIG_EXYNOS_PD)
int dpu_pm_domain_check_status(struct exynos_pm_domain *pm_domain)
{
	if (!pm_domain || !pm_domain->check_status)
		return 0;

	if (pm_domain->check_status(pm_domain))
		return 1;
	else
		return 0;
}
#endif

void dsim_to_regs_param(struct dsim_device *dsim, struct dsim_regs *regs)
{
	regs->regs = dsim->res.regs;
	regs->ss_regs = dsim->res.ss_regs;
	regs->phy_regs = dsim->res.phy_regs;
	regs->phy_regs_ex = dsim->res.phy_regs_ex;
}

void dpu_save_fence_info(int fd, struct dma_fence *fence,
		struct dpu_fence_info *fence_info)
{
	fence_info->fd = fd;
	fence_info->context = fence->context;
	fence_info->seqno = fence->seqno;
	fence_info->flags = fence->flags;
	strlcpy(fence_info->name, fence->ops->get_driver_name(fence),
			MAX_DPU_FENCE_NAME);
}
