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

#include "sprd_iommu_api.h"

u32 sprd_iommudrv_init(struct sprd_iommu_init_param *init_param,
		  sprd_iommu_hdl *iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;
	struct sprd_iommudrv_ops *drv_ops = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = kzalloc(sizeof(*iommu_data), GFP_KERNEL);
	if (!iommu_data)
		return SPRD_ERR_INITIALIZED;

	switch (init_param->iommu_id) {
	case IOMMU_EX_VSP:
	case IOMMU_EX_DCAM:
	case IOMMU_EX_CPP:
	case IOMMU_EX_GSP:
	case IOMMU_EX_JPG:
	case IOMMU_EX_DISP:
	case IOMMU_EX_ISP:
	case IOMMU_EX_NEWISP:
	case IOMMU_EX_FD:
	case IOMMU_EX_NPU:
	case IOMMU_EX_EPP:
	case IOMMU_EX_EDP:
	case IOMMU_EX_IDMA:
	case IOMMU_EX_VDMA:
		drv_ops = (&iommuex_drv_ops);
		break;
	case IOMMU_VAU_VSP:
	case IOMMU_VAU_VSP1:
	case IOMMU_VAU_VSP2:
	case IOMMU_VAU_DCAM:
	case IOMMU_VAU_DCAM1:
	case IOMMU_VAU_CPP:
	case IOMMU_VAU_JPG:
	case IOMMU_VAU_GSP:
	case IOMMU_VAU_GSP1:
	case IOMMU_VAU_DISP:
	case IOMMU_VAU_DISP1:
	case IOMMU_VAU_ISP:
	case IOMMU_VAU_FD:
	case IOMMU_VAU_NPU:
	case IOMMU_VAU_EPP:
	case IOMMU_VAU_EDP:
	case IOMMU_VAU_IDMA:
	case IOMMU_VAU_VDMA:
		drv_ops = (&iommuvau_drv_ops);
		break;
	default:
		kfree(iommu_data);
		return SPRD_ERR_INVALID_PARAM;
	}

	iommu_data->iommudrv_ops = drv_ops;
	if (iommu_data->iommudrv_ops)
		iommu_data->iommudrv_ops->init(init_param,
					(sprd_iommu_hdl)iommu_data);

	*iommu_hdl = (sprd_iommu_hdl)iommu_data;
	return SPRD_NO_ERR;
}

u32 sprd_iommudrv_uninit(sprd_iommu_hdl iommu_hdl)
{
	struct sprd_iommu_data *iommu_data;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	iommu_data->priv = NULL;

	kfree(iommu_data);

	return SPRD_NO_ERR;
}

u32 sprd_iommudrv_map(sprd_iommu_hdl iommu_hdl,
		  struct sprd_iommu_map_param *map_param)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if ((!iommu_hdl) || (!map_param))
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;
	else
		return iommu_data->iommudrv_ops->map(iommu_hdl, map_param);
}

u32 sprd_iommudrv_unmap(sprd_iommu_hdl iommu_hdl,
		  struct sprd_iommu_unmap_param *unmap_param)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;
	else
		return iommu_data->iommudrv_ops->unmap(iommu_hdl,
							unmap_param);
}

u32 sprd_iommudrv_unmap_orphaned(sprd_iommu_hdl iommu_hdl,
			struct sprd_iommu_unmap_param *unmap_param)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;
	else
		return iommu_data->iommudrv_ops->unmap_orphaned(iommu_hdl,
							unmap_param);
}

u32 sprd_iommudrv_enable(sprd_iommu_hdl iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;
	else
		return iommu_data->iommudrv_ops->enable(iommu_hdl);
}


u32 sprd_iommudrv_disable(sprd_iommu_hdl iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;
	else
		return iommu_data->iommudrv_ops->disable(iommu_hdl);
}


u32 sprd_iommudrv_suspend(sprd_iommu_hdl iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;
	else
		return iommu_data->iommudrv_ops->suspend(iommu_hdl);
}


u32 sprd_iommudrv_resume(sprd_iommu_hdl iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;
	else
		return iommu_data->iommudrv_ops->resume(iommu_hdl);
}

u32 sprd_iommudrv_reset(sprd_iommu_hdl  iommu_hdl, u32 channel_num)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;
	else
		return iommu_data->iommudrv_ops->reset(iommu_hdl,
							channel_num);
}

u32 sprd_iommudrv_set_bypass(sprd_iommu_hdl  iommu_hdl, bool vaor_bp_en)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;
	else
		return iommu_data->iommudrv_ops->set_bypass(iommu_hdl,
							  vaor_bp_en);
}

u32 sprd_iommudrv_release(sprd_iommu_hdl iommu_hdl)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;

	if (iommu_data->iommudrv_ops->release)
		iommu_data->iommudrv_ops->release(iommu_hdl);

	return SPRD_NO_ERR;
}

u32 sprd_iommudrv_virt_to_phy(sprd_iommu_hdl iommu_hdl,
			  u64 virt_addr, u64 *dest_addr)
{
	struct sprd_iommu_data *iommu_data = NULL;

	if (!iommu_hdl)
		return SPRD_ERR_INVALID_PARAM;

	iommu_data = (struct sprd_iommu_data *)iommu_hdl;

	if (!(iommu_data->iommudrv_ops))
		return SPRD_ERR_INVALID_PARAM;
	else
		return iommu_data->iommudrv_ops->virttophy(iommu_hdl,
						virt_addr,
						dest_addr);
}
