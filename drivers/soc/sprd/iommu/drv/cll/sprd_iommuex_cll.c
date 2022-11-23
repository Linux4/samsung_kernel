/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#include "sprd_iommuex_cll.h"

static u32 sprd_iommuex_cll_init(struct sprd_iommu_init_param *init_param,
						sprd_iommu_hdl iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;
	u32 iommu_id;
	unsigned int pagt_size = 0;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	iommu_priv = kzalloc(sizeof(*iommu_priv), GFP_KERNEL);
	if (!iommu_priv)
		return SPRD_ERR_INITIALIZED;

	iommu_priv->frc_reg_addr = init_param->frc_reg_addr;
	iommu_priv->mmu_reg_addr = init_param->ctrl_reg_addr;
	iommu_id = init_param->iommu_id;
	iommu_priv->iommu_id = init_param->iommu_id;
	iommu_priv->vpn_base_addr = init_param->vpn_base_addr;
	iommu_priv->vpn_range = init_param->vpn_range;

	/*
	*in acual use:jpg/gsp 256M,cpp 128M,DISP 128M(sharkl2) 256M(isharkl2),
	*vsp 256M, dcam 64M
	*/
	pagt_size = (iommu_priv->vpn_range / MMU_MAPING_PAGESIZE) * 4;
	if (init_param->pagt_base_ddr > 0) {
		iommu_priv->pagt_base_phy_ddr = init_param->pagt_base_ddr;
		iommu_priv->pagt_ddr_size = init_param->pagt_ddr_size;
		iommu_priv->pagt_base_ddr =
			(ulong)ioremap_nocache(iommu_priv->pagt_base_phy_ddr,
					iommu_priv->pagt_ddr_size);
	} else {
		iommu_priv->pagt_base_phy_ddr = 0;
		iommu_priv->pagt_ddr_size = 0;
		iommu_priv->pagt_base_ddr = 0;
	}

	iommu_priv->pgt_size = pagt_size;
	if (iommu_priv->pagt_base_phy_ddr > 0) {
		/*page table in ddr use reserved memory*/
		iommu_priv->ppn_base_addr =
			iommu_priv->pagt_base_ddr;
	} else {
		iommu_priv->ppn_base_addr =
			(ulong)kmalloc(pagt_size, GFP_KERNEL);
		if (!(iommu_priv->ppn_base_addr))
			return SPRD_ERR_INITIALIZED;
	}

	if (iommu_id == IOMMU_EX_ISP)
		memset((void *)iommu_priv->ppn_base_addr, 0x0, pagt_size);
	else
		memset((void *)iommu_priv->ppn_base_addr, 0xff, pagt_size);

	iommu_priv->default_addr = init_param->faultpage_addr;
	iommu_priv->map_cnt = 0;
	iommu_priv->mini_ppn1 = init_param->mini_ppn1;
	iommu_priv->ppn1_range = init_param->ppn1_range;
	iommu_priv->mini_ppn2 = init_param->mini_ppn2;
	iommu_priv->ppn2_range = init_param->ppn2_range;

	iommu_data->priv = (void *)(iommu_priv);
	return SPRD_NO_ERR;
}

static u32 sprd_iommuex_cll_uninit(sprd_iommu_hdl  iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;
	u32 iommu_id;
	u8 pa_out_range_r_en = 0;
	u8 pa_out_range_w_en = 0;
	u8 va_out_range_r_en = 0;
	u8 va_out_range_w_en = 0;
	u8 invalid_r_en = 0;
	u8 invalid_w_en = 0;
	u8 unsecure_r_en = 0;
	u8 unsecure_w_en = 0;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;
	if (!(iommu_data->priv))
		return SPRD_ERR_INITIALIZED;

	iommu_priv = (struct sprd_iommuex_priv *)(iommu_data->priv);

	iommu_id = iommu_priv->iommu_id;

	if (iommu_id == IOMMU_EX_CPP) {
		pa_out_range_r_en = 1;
		pa_out_range_w_en = 1;
		va_out_range_r_en = 1;
		va_out_range_w_en = 1;
		invalid_r_en = 1;
		invalid_w_en = 1;
		unsecure_r_en = 1;
		unsecure_w_en = 1;
	}

	mmu_ex_enable(iommu_priv->mmu_reg_addr, iommu_id, 0);
	if (iommu_priv->pagt_base_phy_ddr > 0)
		iounmap((void __iomem *)iommu_priv->pagt_base_ddr);
	else
		kfree((void *)iommu_priv->ppn_base_addr);

	iommu_priv->ppn_base_addr = 0;

	memset(iommu_data->priv, 0, sizeof(*(iommu_data->priv)));
	kfree(iommu_data->priv);
	iommu_data->priv = NULL;

	return SPRD_NO_ERR;
}

static void sprd_iommuex_flush_pgt(ulong ppn_base,
				   u32 start_entry, u32 end_entry)
{
#ifdef CONFIG_ARM64
	__dma_flush_area((void *)(ppn_base +
		start_entry * 4),
		end_entry * 4);
#else
	dmac_flush_range((void *)(ppn_base +
		start_entry * 4),
		(void *)(ppn_base +
		end_entry * 4));
#endif
}

static u32 sprd_iommuex_cll_enable(sprd_iommu_hdl iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;
	u32 iommu_id;
	ulong addr_range = 0;
	ulong pgt_addr_phy = 0;
	ulong fault_page = 0;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;
	if (!(iommu_data->priv))
		return SPRD_ERR_INITIALIZED;

	iommu_priv = (struct sprd_iommuex_priv *)(iommu_data->priv);
	iommu_id = iommu_priv->iommu_id;

	if (mmu_ex_check_en(iommu_priv->mmu_reg_addr, iommu_id))
		goto exit;

	/*config first vpn*/
	if (iommu_priv->pagt_base_phy_ddr > 0)
		pgt_addr_phy = iommu_priv->pagt_base_phy_ddr;
	else
		pgt_addr_phy = virt_to_phys(
					(void *)iommu_priv->ppn_base_addr);

	mmu_ex_first_vpn(iommu_priv->mmu_reg_addr, iommu_id,
		iommu_priv->vpn_base_addr);
	mmu_ex_first_ppn(iommu_priv->mmu_reg_addr, iommu_id, pgt_addr_phy);

	fault_page = iommu_priv->default_addr;
	mmu_ex_default_ppn(iommu_priv->mmu_reg_addr, iommu_id, fault_page);

	if (iommu_id != IOMMU_EX_ISP) {
		if (iommu_id == IOMMU_EX_DISP)
			mmu_ex_vpn_range(iommu_priv->mmu_reg_addr,
			    iommu_id, (iommu_priv->vpn_range >> 12) - 1);

		/*vpn_range temporary use default value*/
		if (iommu_priv->mini_ppn1 > 0)
			mmu_ex_mini_ppn1(iommu_priv->mmu_reg_addr, iommu_id,
				iommu_priv->mini_ppn1);

		if (iommu_priv->ppn1_range > 0) {
			addr_range = 0;
			addr_range = (iommu_priv->ppn1_range +
					(1 << SHIFT_1M) - 1)
					& (~((1 << SHIFT_1M) - 1));
			mmu_ex_mini_ppn1(iommu_priv->mmu_reg_addr, iommu_id,
				iommu_priv->ppn1_range);
		}

		if (iommu_priv->mini_ppn2 > 0)
			mmu_ex_mini_ppn2(iommu_priv->mmu_reg_addr, iommu_id,
				iommu_priv->mini_ppn2);

		if (iommu_priv->ppn2_range > 0) {
			addr_range = (iommu_priv->ppn2_range +
				(1 << SHIFT_1M) - 1) & (~((1 << SHIFT_1M) - 1));
			mmu_ex_mini_ppn2(iommu_priv->mmu_reg_addr, iommu_id,
				iommu_priv->ppn2_range);
		}
		/*config update arqos,access ddr priority,default 7*/
		mmu_ex_pt_update_arqos(iommu_priv->mmu_reg_addr, 7);
	} else {
		/*isp iommu need config page table size*/
		mmuex_pagetable_size(iommu_priv->mmu_reg_addr,
			iommu_priv->pgt_size - 1);
	}

	mmu_ex_vaorbypass_clkgate_enable_combined(iommu_priv->mmu_reg_addr,
		iommu_id);
exit:
	/*for sharkle dcam_if r4p0*/
	mmu_ex_frc_copy(iommu_priv->frc_reg_addr, iommu_id);

	return SPRD_NO_ERR;
}

static u32 sprd_iommuex_cll_disable(sprd_iommu_hdl  iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;
	u32 iommu_id;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;
	if (!(iommu_data->priv))
		return SPRD_ERR_INITIALIZED;

	iommu_priv = (struct sprd_iommuex_priv *)(iommu_data->priv);
	iommu_id = iommu_priv->iommu_id;

	mmu_ex_first_vpn(iommu_priv->mmu_reg_addr, iommu_id, 0);
	mmu_ex_first_ppn(iommu_priv->mmu_reg_addr, iommu_id, 0);
	mmu_ex_default_ppn(iommu_priv->mmu_reg_addr, iommu_id, 0);
	if (iommu_id != IOMMU_EX_ISP) {

		if (iommu_priv->mini_ppn1 > 0)
			mmu_ex_mini_ppn1(iommu_priv->mmu_reg_addr,
				iommu_id, 0);

		if (iommu_priv->ppn1_range > 0)
			mmu_ex_mini_ppn1(iommu_priv->mmu_reg_addr,
				iommu_id, 0x1fff);

		if (iommu_priv->mini_ppn2 > 0)
			mmu_ex_mini_ppn2(iommu_priv->mmu_reg_addr,
				iommu_id, 0);

		if (iommu_priv->ppn2_range > 0)
			mmu_ex_mini_ppn2(iommu_priv->mmu_reg_addr,
				iommu_id, 0x1fff);
	} else {
		/*isp iommu need config page table size*/
		mmuex_pagetable_size(iommu_priv->mmu_reg_addr, 0);
	}

	mmu_ex_enable(iommu_priv->mmu_reg_addr, iommu_id, 0);

	mmu_ex_frc_copy(iommu_priv->frc_reg_addr, iommu_id);

	return SPRD_NO_ERR;
}

static u32 sprd_iommuex_cll_map(sprd_iommu_hdl  iommu_hdl,
				struct sprd_iommu_map_param *map_param)
{
	u32 entry_index = 0;
	u32 valid_page_entries = 0;
	ulong ppn_addr;
	ulong phy_addr = 0;
	u32 pte;
	u32 vir_base_entry = 0;
	u32 total_page_entries = 0;
	u32 align_map_size = 0;
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;
	u32 fault_page;
	struct scatterlist *sg;
	u32 sg_index = 0;
	u32 iommu_id;

	if ((!iommu_hdl) || (!map_param))
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;
	if (!(iommu_data->priv))
		return SPRD_ERR_INITIALIZED;

	iommu_priv = (struct sprd_iommuex_priv *)(iommu_data->priv);

	iommu_id = iommu_priv->iommu_id;

	vir_base_entry = (u32)VIR_TO_ENTRY_IDX(map_param->start_virt_addr,
					      iommu_priv->vpn_base_addr);
	total_page_entries = vir_base_entry;

	if (map_param->sg_table == NULL) {
		align_map_size = MAP_SIZE_PAGE_ALIGN_UP(map_param->total_map_size);
		valid_page_entries  = (u32)SIZE_TO_ENTRIES(align_map_size);
		fault_page = iommu_priv->default_addr >> MMU_MAPING_PAGESIZE_SHIFFT;
		if (iommu_id == IOMMU_EX_ISP)
			fault_page = fault_page | 0x80000000;
		memset32((void *)(iommu_priv->ppn_base_addr + vir_base_entry * 4),
				  fault_page, valid_page_entries);
		total_page_entries += valid_page_entries;
	} else {
		for_each_sg(map_param->sg_table->sgl, sg,
			     map_param->sg_table->nents, sg_index) {

			align_map_size = MAP_SIZE_PAGE_ALIGN_UP(sg->length);
			valid_page_entries = (u32)SIZE_TO_ENTRIES(align_map_size);

			for (entry_index = 0; entry_index < valid_page_entries;
			      entry_index++) {
				phy_addr = sg_to_phys(sg) +
					(entry_index << MMU_MAPING_PAGESIZE_SHIFFT);
				pte = phy_addr >> MMU_MAPING_PAGESIZE_SHIFFT;
				/*isp_iommu the hightest bit 1 indicates valid addr*/
				if (iommu_id == IOMMU_EX_ISP)
					pte |= 0x80000000;
				ppn_addr = iommu_priv->ppn_base_addr
					   + (total_page_entries + entry_index) * 4;
				*(u32 *)ppn_addr =  pte;
			}
			total_page_entries += entry_index;
		}
	}

	if (iommu_priv->pagt_base_phy_ddr == 0)
		sprd_iommuex_flush_pgt(iommu_priv->ppn_base_addr,
				       vir_base_entry,
				       total_page_entries);

	sprd_iommuex_cll_enable(iommu_hdl);

	/*we must update the tlb rame,because prefetch may be have taken addr
	*information before we map this time
	*/
	if (iommu_id == IOMMU_EX_ISP) {
		mmuex_tlb_enable(iommu_priv->mmu_reg_addr, 0, 0);
		mmuex_tlb_enable(iommu_priv->mmu_reg_addr, 1, 1);
	} else
		mmu_ex_update(iommu_priv->mmu_reg_addr, iommu_id);
	/*for sharkle dcam_if r4p0*/
	mmu_ex_frc_copy(iommu_priv->frc_reg_addr, iommu_id);

	iommu_priv->map_cnt++;

	return SPRD_NO_ERR;
}

static u32 sprd_iommuex_cll_unmap(sprd_iommu_hdl iommu_hdl,
			struct sprd_iommu_unmap_param *unmap_param)
{
	u32 valid_page_entries = 0;
	ulong vir_base_entry = 0;
	u64 align_map_size = 0;
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;
	u32 iommu_id;

	if ((!iommu_hdl) || (!unmap_param))
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;
	if (!(iommu_data->priv))
		return SPRD_ERR_INITIALIZED;

	iommu_priv = (struct sprd_iommuex_priv *)(iommu_data->priv);
	iommu_id = iommu_priv->iommu_id;

	vir_base_entry = (ulong)VIR_TO_ENTRY_IDX(
					unmap_param->start_virt_addr,
					iommu_priv->vpn_base_addr);


	align_map_size = MAP_SIZE_PAGE_ALIGN_UP(unmap_param->total_map_size);
	valid_page_entries  = (u32)SIZE_TO_ENTRIES(align_map_size);

	if (iommu_id == IOMMU_EX_ISP) {
		memset((void *)(iommu_priv->ppn_base_addr +
				vir_base_entry * 4),
				0x0, valid_page_entries * 4);
		if (iommu_priv->pagt_base_phy_ddr == 0)
			sprd_iommuex_flush_pgt(iommu_priv->ppn_base_addr,
					       vir_base_entry,
					       vir_base_entry +
					       valid_page_entries);

		if (unmap_param->ch_type == EX_CH_READ ||
		   unmap_param->ch_type == EX_CH_WRITE) {
			mmu_ex_tlb_update(iommu_priv->mmu_reg_addr,
				unmap_param->ch_type, unmap_param->ch_id);
		} else {
			mmuex_tlb_enable(iommu_priv->mmu_reg_addr, 0, 0);
			mmuex_tlb_enable(iommu_priv->mmu_reg_addr, 1, 1);
		}
	} else {
		memset((void *)(iommu_priv->ppn_base_addr +
				vir_base_entry * 4),
				0xFF, valid_page_entries * 4);
		if (iommu_priv->pagt_base_phy_ddr == 0)
			sprd_iommuex_flush_pgt(iommu_priv->ppn_base_addr,
					       vir_base_entry,
					       vir_base_entry +
					       valid_page_entries);
		mmu_ex_update(iommu_priv->mmu_reg_addr, iommu_id);
	}

	iommu_priv->map_cnt--;

	return SPRD_NO_ERR;
}

static u32 sprd_iommuex_cll_unmap_orphaned(sprd_iommu_hdl iommu_hdl,
			struct sprd_iommu_unmap_param *unmap_param)
{
	u32 valid_page_entries = 0;
	ulong vir_base_entry = 0;
	u64 align_map_size = 0;
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;
	u32 iommu_id;

	if ((!iommu_hdl) || (!unmap_param))
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;
	if (!(iommu_data->priv))
		return SPRD_ERR_INITIALIZED;

	iommu_priv = (struct sprd_iommuex_priv *)(iommu_data->priv);
	iommu_id = iommu_priv->iommu_id;

	vir_base_entry = (ulong)VIR_TO_ENTRY_IDX(
					unmap_param->start_virt_addr,
					iommu_priv->vpn_base_addr);


	align_map_size = MAP_SIZE_PAGE_ALIGN_UP(unmap_param->total_map_size);
	valid_page_entries  = (u32)SIZE_TO_ENTRIES(align_map_size);

	if (iommu_id == IOMMU_EX_ISP) {
		memset((void *)(iommu_priv->ppn_base_addr +
				vir_base_entry * 4),
				0x0, valid_page_entries * 4);
	} else {
		memset((void *)(iommu_priv->ppn_base_addr +
				vir_base_entry * 4),
				0xFF, valid_page_entries * 4);
	}

	if (iommu_priv->pagt_base_phy_ddr == 0)
		sprd_iommuex_flush_pgt(iommu_priv->ppn_base_addr,
				       vir_base_entry,
				       vir_base_entry +
				       valid_page_entries);

	iommu_priv->map_cnt--;

	return SPRD_NO_ERR;
}

static u32 sprd_iommuex_cll_suspend(sprd_iommu_hdl iommu_hdl)
{
	return 0;
}

static u32 sprd_iommuex_cll_resume(sprd_iommu_hdl  iommu_hdl)
{
	return 0;
}

static u32 sprd_iommuex_cll_release(sprd_iommu_hdl  iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;
	if (!(iommu_data->priv))
		return SPRD_ERR_INITIALIZED;

	iommu_priv = (struct sprd_iommuex_priv *)(iommu_data->priv);
	iommu_priv->map_cnt = 0;
	return 0;
}

static u32 sprd_iommuex_cll_reset(sprd_iommu_hdl  iommu_hdl, u32 channel_num)
{
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;
	if (!(iommu_data->priv))
		return SPRD_ERR_INITIALIZED;

	iommu_priv = (struct sprd_iommuex_priv *)(iommu_data->priv);

	sprd_iommuex_cll_enable(iommu_hdl);

	if (iommu_priv->iommu_id == IOMMU_EX_ISP) {
		mmuex_tlb_enable(iommu_priv->mmu_reg_addr, 0, 0);
		mmuex_tlb_enable(iommu_priv->mmu_reg_addr, 1, 1);
	} else
		mmu_ex_update(iommu_priv->mmu_reg_addr,
			iommu_priv->iommu_id);

	return 0;
}

static u32 sprd_iommuex_cll_set_bypass(sprd_iommu_hdl  iommu_hdl, bool vaor_bp_en)
{
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;
	if (!(iommu_data->priv))
		return SPRD_ERR_INITIALIZED;

	iommu_priv = (struct sprd_iommuex_priv *)(iommu_data->priv);

	mmu_ex_vaout_bypass_enable(iommu_priv->mmu_reg_addr,
				   iommu_priv->iommu_id,
				   vaor_bp_en);
	return 0;
}

static u32 sprd_iommuex_cll_virt_to_phy(sprd_iommu_hdl iommu_hdl,
			u64 virt_addr, u64 *dest_addr)
{
	u64 entry_index = 0;
	u64 phy_page_addr = 0;
	u64 page_in_offset = 0;
	u64 real_phy_addr = 0;

	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommuex_priv *iommu_priv = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;
	if (!(iommu_data->priv))
		return SPRD_ERR_INITIALIZED;

	iommu_priv = (struct sprd_iommuex_priv *)(iommu_data->priv);

	entry_index = VIR_TO_ENTRY_IDX(virt_addr,
				iommu_priv->vpn_base_addr);
	phy_page_addr = mmu_ex_read_page_entry(iommu_priv->ppn_base_addr,
					    entry_index);
	page_in_offset = virt_addr & MMU_MAPING_PAGE_MASK;
	real_phy_addr = (phy_page_addr << MMU_MAPING_PAGESIZE_SHIFFT)
				+ page_in_offset;

	*dest_addr = real_phy_addr;
	return SPRD_NO_ERR;
}

struct sprd_iommudrv_ops iommuex_drv_ops = {
	sprd_iommuex_cll_init,
	sprd_iommuex_cll_uninit,

	sprd_iommuex_cll_map,
	sprd_iommuex_cll_unmap,

	sprd_iommuex_cll_enable,
	sprd_iommuex_cll_disable,

	sprd_iommuex_cll_suspend,
	sprd_iommuex_cll_resume,
	sprd_iommuex_cll_release,
	sprd_iommuex_cll_reset,
	sprd_iommuex_cll_set_bypass,
	sprd_iommuex_cll_virt_to_phy,
	sprd_iommuex_cll_unmap_orphaned,
};
