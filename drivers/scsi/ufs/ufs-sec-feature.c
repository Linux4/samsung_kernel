// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#include <linux/time.h>
#include <linux/of.h>
#include <asm/unaligned.h>
#include <scsi/scsi_proto.h>
#include <linux/sec_debug.h>

#include "ufs-sec-feature.h"
#include "ufs-sec-sysfs.h"

struct ufs_sec_err_info ufs_err_info;
struct ufs_sec_err_info ufs_err_info_backup;

static void ufs_sec_set_unique_number(struct ufs_hba *hba,
			u8 *str_desc_buf, u8 *desc_buf)
{
	u8 manid;
	u8 snum_buf[SERIAL_NUM_SIZE + 1];
	struct ufs_dev_info *dev_info = &hba->dev_info;

	manid = dev_info->wmanufacturerid & 0xFF;
	memset(hba->unique_number, 0, sizeof(hba->unique_number));
	memset(snum_buf, 0, sizeof(snum_buf));

	memcpy(snum_buf, str_desc_buf + QUERY_DESC_HDR_SIZE, SERIAL_NUM_SIZE);

	sprintf(hba->unique_number, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			manid,
			desc_buf[DEVICE_DESC_PARAM_MANF_DATE],
			desc_buf[DEVICE_DESC_PARAM_MANF_DATE + 1],
			snum_buf[0], snum_buf[1], snum_buf[2], snum_buf[3],
			snum_buf[4], snum_buf[5], snum_buf[6]);

	/* Null terminate the unique number string */
	hba->unique_number[UFS_UN_20_DIGITS] = '\0';
}

void ufs_sec_get_health_info(struct ufs_hba *hba)
{
	u8 desc_buf[hba->desc_size.hlth_desc];
	int err;

	err = ufshcd_read_health_desc(hba, desc_buf, hba->desc_size.hlth_desc);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading health descriptor. err %d",
			__func__, err);
		return;
	}

	/* getting Life Time at Device Health DESC*/
	hba->lifetime = desc_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A];

	dev_info(hba->dev, "LT: 0x%02x\n", desc_buf[3] << 4 | desc_buf[4]);
}

void ufs_set_sec_features(struct ufs_hba *hba, u8 *str_desc_buf, u8 *desc_buf)
{
	ufs_sec_set_unique_number(hba, str_desc_buf, desc_buf);
	ufs_sec_get_health_info(hba);

	ufs_sec_add_sysfs_nodes(hba);
}

void ufs_remove_sec_features(struct ufs_hba *hba)
{
	ufs_sec_remove_sysfs_nodes(hba);
}

void ufs_sec_hwrst_cnt_check(void)
{
	struct SEC_UFS_op_count *op_cnt = &ufs_err_info.op_count;

	SEC_UFS_ERR_COUNT_INC(op_cnt->HW_RESET_count, UINT_MAX);
	SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
}

static void ufs_sec_check_link_startup_error_cnt(void)
{
	struct SEC_UFS_op_count *op_cnt = &ufs_err_info.op_count;

	SEC_UFS_ERR_COUNT_INC(op_cnt->link_startup_count, UINT_MAX);
	SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
}

static void ufs_sec_uic_error_check(enum ufs_event_type evt, u32 reg)
{
	struct SEC_UFS_UIC_err_count *uic_err_cnt = &ufs_err_info.UIC_err_count;
	u32 lane_idx = 0;

	switch (evt) {
	case UFS_EVT_PA_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->PA_ERR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);

		if (reg & UIC_PHY_ADAPTER_LAYER_GENERIC_ERROR)
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->PA_ERR_linereset, UINT_MAX);

		lane_idx = reg & UIC_PHY_ADAPTER_LAYER_LANE_ERR_MASK;
		if (lane_idx)
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->PA_ERR_lane[lane_idx - 1], UINT_MAX);
		break;
	case UFS_EVT_DL_ERR:
		if (reg & UIC_DATA_LINK_LAYER_ERROR_PA_INIT) {
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_PA_INIT_ERROR_cnt, U8_MAX);
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		}
		if (reg & UIC_DATA_LINK_LAYER_ERROR_NAC_RECEIVED) {
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_NAC_RECEIVED_ERROR_cnt, U8_MAX);
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		}
		if (reg & UIC_DATA_LINK_LAYER_ERROR_TCx_REPLAY_TIMEOUT) {
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_TC_REPLAY_ERROR_cnt, U8_MAX);
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		}
		break;
	case UFS_EVT_NL_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->NL_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		break;
	case UFS_EVT_TL_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->TL_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		break;
	case UFS_EVT_DME_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DME_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		break;
	default:
		break;
	}
}

static void ufs_sec_uic_fatal_check(u32 errors)
{
	struct SEC_UFS_Fatal_err_count *fatal_err_cnt = &ufs_err_info.Fatal_err_count;

	if (errors & DEVICE_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->DFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (errors & CONTROLLER_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->CFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (errors & SYSTEM_BUS_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->SBFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (errors & CRYPTO_ENGINE_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->CEFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	/* UIC_LINK_LOST : can not be checked in ufshcd.c */
	if (errors & UIC_LINK_LOST) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->LLE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
}

static void ufs_sec_utp_error_check(struct ufs_hba *hba, int tag)
{
	struct SEC_UFS_UTP_count *utp_err = &ufs_err_info.UTP_count;
	struct ufshcd_lrb *lrbp = NULL;
	u8 opcode = 0;

	/* check tag value */
	if (tag >= hba->nutrs)
		return;

	lrbp = &hba->lrb[tag];
	/* check lrbp */
	if (!lrbp || !lrbp->cmd || (lrbp->task_tag != tag))
		return;

	opcode = lrbp->cmd->cmnd[0];

	switch (opcode) {
	case WRITE_10:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_write_err, U8_MAX);
		break;
	case READ_10:
	case READ_16:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_read_err, U8_MAX);
		break;
	case SYNCHRONIZE_CACHE:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_sync_cache_err, U8_MAX);
		break;
	case UNMAP:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_unmap_err, U8_MAX);
		break;
	default:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_etc_err, U8_MAX);
		break;
	}

	SEC_UFS_ERR_COUNT_INC(utp_err->UTP_err, UINT_MAX);
}

void ufs_sec_op_err_check(struct ufs_hba *hba,
			enum ufs_event_type evt, void *data)
{
	u32 error_val = *(u32 *)data;

	switch (evt) {
	case UFS_EVT_LINK_STARTUP_FAIL:
		ufs_sec_check_link_startup_error_cnt();
		break;
	case UFS_EVT_DEV_RESET:
		break;
	case UFS_EVT_PA_ERR:
	case UFS_EVT_DL_ERR:
	case UFS_EVT_NL_ERR:
	case UFS_EVT_TL_ERR:
	case UFS_EVT_DME_ERR:
		if (error_val) /* error register */
			ufs_sec_uic_error_check(evt, error_val);
		break;
	case UFS_EVT_FATAL_ERR:
		if (error_val) /* hba->errors */
			ufs_sec_uic_fatal_check(error_val);
		break;
	case UFS_EVT_ABORT: /* tag */
		ufs_sec_utp_error_check(hba, (int)error_val);
		break;
	case UFS_EVT_HOST_RESET:
		break;
	case UFS_EVT_SUSPEND_ERR:
	case UFS_EVT_RESUME_ERR:
		break;
	case UFS_EVT_AUTO_HIBERN8_ERR:
		break;
	default:
		break;
	}
}

void ufs_sec_uic_cmd_error_check(struct ufs_hba *hba, u32 cmd)
{
	struct SEC_UFS_UIC_cmd_count *uic_cmd_cnt = &ufs_err_info.UIC_cmd_count;
	struct SEC_UFS_op_count *op_cnt = &ufs_err_info.op_count;

	/* check UIC CMD result */
	if ((hba->active_uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT) == UIC_CMD_RESULT_SUCCESS)
		return;

	switch (cmd & COMMAND_OPCODE_MASK) {
	case UIC_CMD_DME_GET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_GET_err, U8_MAX);
		break;
	case UIC_CMD_DME_SET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_SET_err, U8_MAX);
		break;
	case UIC_CMD_DME_PEER_GET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_PEER_GET_err, U8_MAX);
		break;
	case UIC_CMD_DME_PEER_SET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_PEER_SET_err, U8_MAX);
		break;
	case UIC_CMD_DME_POWERON:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_POWERON_err, U8_MAX);
		break;
	case UIC_CMD_DME_POWEROFF:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_POWEROFF_err, U8_MAX);
		break;
	case UIC_CMD_DME_ENABLE:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_ENABLE_err, U8_MAX);
		break;
	case UIC_CMD_DME_RESET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_RESET_err, U8_MAX);
		break;
	case UIC_CMD_DME_END_PT_RST:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_END_PT_RST_err, U8_MAX);
		break;
	case UIC_CMD_DME_LINK_STARTUP:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_LINK_STARTUP_err, U8_MAX);
		break;
	case UIC_CMD_DME_HIBER_ENTER:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_HIBER_ENTER_err, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->Hibern8_enter_count, UINT_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
		break;
	case UIC_CMD_DME_HIBER_EXIT:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_HIBER_EXIT_err, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->Hibern8_exit_count, UINT_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
		break;
	case UIC_CMD_DME_TEST_MODE:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_TEST_MODE_err, U8_MAX);
		break;
	default:
		break;
	}

	SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->UIC_cmd_err, UINT_MAX);
}

void ufs_sec_tm_error_check(u8 tm_cmd)
{
	struct SEC_UFS_UTP_count *utp_err = &ufs_err_info.UTP_count;

	switch (tm_cmd) {
	case UFS_QUERY_TASK:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_query_task_count, U8_MAX);
		break;
	case UFS_ABORT_TASK:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_abort_task_count, U8_MAX);
		break;
	case UFS_LOGICAL_RESET:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_logical_reset_count, U8_MAX);
		break;
	default:
		break;
	}

	SEC_UFS_ERR_COUNT_INC(utp_err->UTP_err, UINT_MAX);
}

static void ufs_sec_query_error_check(struct ufs_hba *hba,
			struct ufshcd_lrb *lrbp)
{
	struct SEC_UFS_QUERY_count *query_cnt = &ufs_err_info.query_count;
	struct ufs_query_req *request = &hba->dev_cmd.query.request;
	enum query_opcode opcode = request->upiu_req.opcode;
	enum dev_cmd_type cmd_type = hba->dev_cmd.type;

	/* check Overall Command Status */
	if ((le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS) == OCS_SUCCESS)
		return;

	if (cmd_type == DEV_CMD_TYPE_NOP) {
		SEC_UFS_ERR_COUNT_INC(query_cnt->NOP_err, U8_MAX);
	} else {
		switch (opcode) {
		case UPIU_QUERY_OPCODE_READ_DESC:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Desc_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_WRITE_DESC:
			SEC_UFS_ERR_COUNT_INC(query_cnt->W_Desc_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_READ_ATTR:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Attr_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_WRITE_ATTR:
			SEC_UFS_ERR_COUNT_INC(query_cnt->W_Attr_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_READ_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_SET_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Set_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_CLEAR_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Clear_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Toggle_Flag_err, U8_MAX);
			break;
		default:
			break;
		}
	}

	SEC_UFS_ERR_COUNT_INC(query_cnt->Query_err, UINT_MAX);
}

static void ufs_sec_check_medium_error_region(struct ufs_sec_cmd_info *ufs_cmd)
{
	struct SEC_SCSI_SENSE_err_log *sense_err_log = &ufs_err_info.sense_err_log;
	unsigned int lba_count = sense_err_log->issue_LBA_count;
	unsigned long region_bit = 0;
	int i = 0;

	if (ufs_cmd->lun == 0) {
		if (lba_count < SEC_MAX_LBA_LOGGING) {
			for (i = 0; i < SEC_MAX_LBA_LOGGING; i++) {
				if (sense_err_log->issue_LBA_list[i] == ufs_cmd->lba)
					return;
			}
			sense_err_log->issue_LBA_list[lba_count] = ufs_cmd->lba;
			sense_err_log->issue_LBA_count++;
		}

		region_bit = ufs_cmd->lba / SEC_ISSUE_REGION_STEP;
		if (region_bit > 51)
			region_bit = 52;
	} else if (ufs_cmd->lun < UFS_UPIU_WLUN_ID) {
		region_bit = (unsigned long)(64 - ufs_cmd->lun);
	}

	sense_err_log->issue_region_map |= ((u64)1 << region_bit);
}

static void ufs_sec_sense_err_check(struct ufshcd_lrb *lrbp, struct ufs_sec_cmd_info *ufs_cmd)
{
	struct SEC_SCSI_SENSE_count *sense_err = &ufs_err_info.sense_count;
	u8 sense_key = 0;
	u8 asc = 0;
	u8 ascq = 0;

	sense_key = lrbp->ucd_rsp_ptr->sr.sense_data[2] & 0x0F;
	if (sense_key != MEDIUM_ERROR && sense_key != HARDWARE_ERROR)
		return;

	asc = lrbp->ucd_rsp_ptr->sr.sense_data[12];
	ascq = lrbp->ucd_rsp_ptr->sr.sense_data[13];

	pr_err("UFS: sense key 0x%x(asc 0x%x, ascq 0x%x), opcode 0x%x, lba 0x%x, len 0x%x.\n",
			sense_key, asc, ascq,
			ufs_cmd->opcode, ufs_cmd->lba, ufs_cmd->transfer_len);

	if (sense_key == MEDIUM_ERROR) {
		sense_err->scsi_medium_err++;
		ufs_sec_check_medium_error_region(ufs_cmd);
#ifdef CONFIG_SEC_DEBUG
		/* only work for debug level is mid */
		if (SEC_DEBUG_LEVEL(kernel))
			panic("ufs medium error\n");
#endif
	} else if (sense_key == HARDWARE_ERROR) {
		sense_err->scsi_hw_err++;
#ifdef CONFIG_SEC_DEBUG
		/* only work for debug level is mid */
		if (SEC_DEBUG_LEVEL(kernel))
			panic("ufs hardware error\n");
#endif
	}
}

static bool ufs_sec_get_scsi_cmd_info(struct ufshcd_lrb *lrbp,
			struct ufs_sec_cmd_info *ufs_cmd)
{
	struct scsi_cmnd *cmd;

	if (!lrbp || !lrbp->cmd || !ufs_cmd)
		return false;

	cmd = lrbp->cmd;

	ufs_cmd->opcode = (u8)(*cmd->cmnd);
	ufs_cmd->lba = (cmd->cmnd[2] << 24) | (cmd->cmnd[3] << 16) |
					(cmd->cmnd[4] << 8) | cmd->cmnd[5];
	ufs_cmd->transfer_len = (cmd->cmnd[7] << 8) | cmd->cmnd[8];
	ufs_cmd->lun = ufshcd_scsi_to_upiu_lun(cmd->device->lun);

	return true;
}

void ufs_sec_compl_cmd_check(struct ufs_hba *hba,
			struct ufshcd_lrb *lrbp)
{
	struct ufs_sec_cmd_info ufs_cmd = { 0, };
	bool is_scsi_cmd = false;
	unsigned long flags;

	is_scsi_cmd = ufs_sec_get_scsi_cmd_info(lrbp, &ufs_cmd);

	if (is_scsi_cmd) {
		ufs_sec_sense_err_check(lrbp, &ufs_cmd);
		/*
		 * check hba->req_abort_count, if the cmd is aborting
		 * it's the one way to check aborting
		 * hba->req_abort_count is cleared in queuecommand and after
		 * error handling
		 */
		if (hba->req_abort_count > 0)
			ufs_sec_utp_error_check(hba, lrbp->task_tag);
	} else {
		/* in timeout error case, can not be logging */
		ufs_sec_query_error_check(hba, lrbp);
	}
}