// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * Authors:
 * 	Storage Driver <storage.sec@samsung.com>
 */

#include "ufs-sec-feature.h"
#include "ufs-sec-sysfs.h"

#include <linux/time.h>
#include <linux/of.h>
#include <asm/unaligned.h>
#include <trace/hooks/ufshcd.h>
#include <linux/panic_notifier.h>
#include <linux/sec_debug.h>
#include <linux/reboot.h>

#define NOTI_WORK_DELAY_MS 500

void (*ufs_sec_wb_reset_notify)(void);

struct ufs_sec_feature_info ufs_sec_features;

static inline int ufs_sec_read_unit_desc_param(struct ufs_hba *hba,
					      int lun,
					      enum unit_desc_param param_offset,
					      u8 *param_read_buf,
					      u32 param_size)
{
	/*
	 * Unit descriptors are only available for general purpose LUs (LUN id
	 * from 0 to 7) and RPMB Well known LU.
	 */
	if (!ufs_is_valid_unit_desc_lun(&hba->dev_info, lun, param_offset))
		return -EOPNOTSUPP;

	return ufshcd_read_desc_param(hba, QUERY_DESC_IDN_UNIT, lun,
				      param_offset, param_read_buf, param_size);
}

static void ufs_sec_set_unique_number(struct ufs_hba *hba, u8 *desc_buf)
{
	struct ufs_vendor_dev_info *vdi = ufs_sec_features.vdi;
	u8 manid = desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];
	u8 serial_num_index = desc_buf[DEVICE_DESC_PARAM_SN];
	u8 snum_buf[SERIAL_NUM_SIZE];
	int buff_len = QUERY_DESC_MAX_SIZE;
	u8 *str_desc_buf = NULL;
	int err;

	/* read string desc */
	str_desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!str_desc_buf)
		return;


	/* spec is unicode but sec uses hex data */
	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_STRING, serial_num_index, 0,
			str_desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading string descriptor. err %d",
				__func__, err);
		goto out;
	}

	/* setup unique_number */
	memset(snum_buf, 0, sizeof(snum_buf));
	memcpy(snum_buf, str_desc_buf + QUERY_DESC_HDR_SIZE, SERIAL_NUM_SIZE);
	memset(vdi->unique_number, 0, sizeof(vdi->unique_number));

	sprintf(vdi->unique_number, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			manid,
			desc_buf[DEVICE_DESC_PARAM_MANF_DATE],
			desc_buf[DEVICE_DESC_PARAM_MANF_DATE + 1],
			snum_buf[0], snum_buf[1], snum_buf[2], snum_buf[3],
			snum_buf[4], snum_buf[5], snum_buf[6]);

	/* Null terminate the unique number string */
	vdi->unique_number[UFS_UN_20_DIGITS] = '\0';

	dev_dbg(hba->dev, "%s: ufs un : %s\n", __func__, vdi->unique_number);
out:
	if (str_desc_buf)
		kfree(str_desc_buf);
}

void ufs_sec_get_health_desc(struct ufs_hba *hba)
{
	struct ufs_vendor_dev_info *vdi = ufs_sec_features.vdi;
	int buff_len = hba->desc_size[QUERY_DESC_IDN_HEALTH];
	u8 *desc_buf = NULL;
	int err;

	desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!desc_buf)
		return;

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_HEALTH, 0, 0,
			desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading health descriptor. err %d",
				__func__, err);
		goto out;
	}

	/* getting Life Time at Device Health DESC*/
	vdi->lt = desc_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A];
	dev_info(hba->dev, "LT: 0x%02x\n", (desc_buf[3] << 4) | desc_buf[4]);

	vdi->flt = desc_buf[HEALTH_DESC_PARAM_VENDOR_LIFE_TIME_EST];
	dev_info(hba->dev, "FLT: 0x%02x\n", vdi->flt);
out:
	if (desc_buf)
		kfree(desc_buf);
}

/*
 * get dExtendedUFSFeaturesSupport from device descriptor
 *
 * checking device spec version for UFS v2.2 , v3.1 or later
 */
static void ufs_sec_get_ext_feature(struct ufs_hba *hba, u8 *desc_buf)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;

	if (!(dev_info->wspecversion >= 0x310 ||
	      dev_info->wspecversion == 0x220 ||
	     (hba->dev_quirks & UFS_DEVICE_QUIRK_SUPPORT_EXTENDED_FEATURES))) {
		ufs_sec_features.ext_ufs_feature_sup = 0x0;
		return;
	}

	ufs_sec_features.ext_ufs_feature_sup = get_unaligned_be32(desc_buf +
				DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP);
}

/* SEC next WB : begin */
#define UFS_WB_DISABLE_THRESHOLD_LT 9

static void ufs_sec_wb_update_err(void)
{
	struct ufs_sec_wb_info *wb_info = ufs_sec_features.ufs_wb;

	wb_info->err_cnt++;
}

static void ufs_sec_wb_update_info(struct ufs_hba *hba, int write_transfer_len)
{
	struct ufs_sec_wb_info *wb_info = ufs_sec_features.ufs_wb;
	enum ufs_sec_wb_state wb_state = hba->dev_info.wb_enabled;

	if (write_transfer_len) {
		/*
		 * write_transfer_len : Byte
		 * wb_info->amount_kb : KB
		 */
		wb_info->amount_kb += (unsigned long)(write_transfer_len >> 10);
		return;
	}

	switch (wb_state) {
	case WB_OFF:
		wb_info->enable_ms += jiffies_to_msecs(jiffies - wb_info->state_ts);
		wb_info->state_ts = jiffies;
		wb_info->disable_cnt++;
		break;
	case WB_ON:
		wb_info->disable_ms += jiffies_to_msecs(jiffies - wb_info->state_ts);
		wb_info->state_ts = jiffies;
		wb_info->enable_cnt++;
		break;
	default:
		break;
	}
}

bool ufs_sec_is_wb_supported(void)
{
	struct ufs_sec_wb_info *ufs_wb = ufs_sec_features.ufs_wb;
	struct ufs_vendor_dev_info *vdi = ufs_sec_features.vdi;

	if (!ufs_wb)
		return false;

	if (vdi->lt >= UFS_WB_DISABLE_THRESHOLD_LT)
		ufs_wb->support = false;

	return ufs_wb->support;
}
EXPORT_SYMBOL(ufs_sec_is_wb_supported);

static bool ufs_sec_check_ext_feature(u32 mask)
{
	if (ufs_sec_features.ext_ufs_feature_sup & mask)
		return true;

	return false;
}

static u32 ufs_sec_wb_buf_alloc(struct ufs_hba *hba,
		struct ufs_dev_info *dev_info, u8 *desc_buf)
{
	u32 wb_buf_alloc = 0;
	u8 lun;

	dev_info->wb_buffer_type = desc_buf[DEVICE_DESC_PARAM_WB_TYPE];

	if (dev_info->wb_buffer_type == WB_BUF_MODE_SHARED &&
			(hba->dev_info.wspecversion >= 0x310 ||
			 hba->dev_info.wspecversion == 0x220)) {
		wb_buf_alloc = get_unaligned_be32(desc_buf +
				DEVICE_DESC_PARAM_WB_SHARED_ALLOC_UNITS);
	} else {
		for (lun = 0; lun < UFS_UPIU_MAX_WB_LUN_ID; lun++) {
			wb_buf_alloc = 0;
			ufs_sec_read_unit_desc_param(hba,
					lun,
					UNIT_DESC_PARAM_WB_BUF_ALLOC_UNITS,
					(u8 *)&wb_buf_alloc,
					sizeof(wb_buf_alloc));
			if (wb_buf_alloc) {
				dev_info->wb_dedicated_lu = lun;
				break;
			}
		}
	}

	return wb_buf_alloc;
}

static int __ufs_sec_wb_ctrl(bool enable)
{
	struct ufs_hba *hba = ufs_sec_features.vdi->hba;
	enum query_opcode opcode;
	int ret = 0;
	u8 index;

	if (!ufs_sec_is_wb_supported())
		return -EOPNOTSUPP;

	if (enable)
		opcode = UPIU_QUERY_OPCODE_SET_FLAG;
	else
		opcode = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	index = ufshcd_wb_get_query_index(hba);
	ret = ufshcd_query_flag_retry(hba, opcode,
			QUERY_FLAG_IDN_WB_EN, index, NULL);
	if (!ret) {
		hba->dev_info.wb_enabled = enable;
		ufs_sec_wb_update_info(hba, 0);
	} else
		ufs_sec_wb_update_err();

	pr_info("%s(%s) is %s, ret=%d.\n", __func__,
			enable ? "enable" : "disable",
			ret ? "failed" : "done", ret);

	return ret;
}

int ufs_sec_wb_ctrl(bool enable)
{
	struct ufs_hba *hba = ufs_sec_features.vdi->hba;
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->pm_op_in_progress || hba->is_sys_suspended) {
		pr_err("%s: ufs is suspended.\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL) {
		pr_err("%s: UFS Host state=%d.\n", __func__, hba->ufshcd_state);
		ret = -EBUSY;
		goto out;
	}

	if (!(enable ^ hba->dev_info.wb_enabled)) {
		pr_info("%s: write booster is already %s\n",
				__func__, enable ? "enabled" : "disabled");
		ret = 0;
		goto out;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	ufshcd_rpm_get_sync(hba);
	ret = __ufs_sec_wb_ctrl(enable);
	ufshcd_rpm_put(hba);

	return ret;
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return ret;
}
EXPORT_SYMBOL(ufs_sec_wb_ctrl);

static void ufs_sec_wb_toggle_flush_during_h8(struct ufs_hba *hba, bool set)
{
	enum query_opcode opcode;
	u8 index;
	int ret = 0;

	if (!ufs_sec_is_wb_supported())
		return;

	if (set)
		opcode = UPIU_QUERY_OPCODE_SET_FLAG;
	else
		opcode = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	index = ufshcd_wb_get_query_index(hba);
	ret = ufshcd_query_flag_retry(hba, opcode,
			QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8,
			index, NULL);

	dev_info(hba->dev, "%s: %s WB flush during H8 is %s.\n", __func__,
			set ? "set" : "clear",
			ret ? "failed" : "done");
}

static void ufs_sec_wb_probe(struct ufs_hba *hba, u8 *desc_buf)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	struct ufs_sec_wb_info *ufs_wb = NULL;
	struct ufs_sec_wb_info *ufs_wb_backup = NULL;
	u32 wb_buf_alloc = 0;

	if (!ufs_sec_check_ext_feature(UFS_DEV_WRITE_BOOSTER_SUP)) {
		dev_err(hba->dev, "%s: Failed check_ext_feature", __func__);
		goto wb_disabled;
	}

	dev_info->b_presrv_uspc_en =
		desc_buf[DEVICE_DESC_PARAM_WB_PRESRV_USRSPC_EN];
	if (!dev_info->b_presrv_uspc_en) {
		dev_err(hba->dev, "%s: Failed preserved user space en value",
				__func__);
		goto wb_disabled;
	}

	wb_buf_alloc = ufs_sec_wb_buf_alloc(hba, dev_info, desc_buf);
	if (!wb_buf_alloc) {
		dev_err(hba->dev, "%s: Failed wb_buf_alloc", __func__);
		goto wb_disabled;
	}

	ufs_wb = devm_kzalloc(hba->dev, sizeof(struct ufs_sec_wb_info),
			GFP_KERNEL);
	if (!ufs_wb) {
		dev_err(hba->dev, "%s: Failed allocating ufs_wb(%lu)",
				__func__, sizeof(struct ufs_sec_wb_info));
		goto wb_disabled;
	}

	ufs_wb_backup = devm_kzalloc(hba->dev, sizeof(struct ufs_sec_wb_info),
			GFP_KERNEL);
	if (!ufs_wb_backup) {
		dev_err(hba->dev, "%s: Failed allocating ufs_wb_backup(%lu)",
				__func__, sizeof(struct ufs_sec_wb_info));
		goto wb_disabled;
	}

	ufs_wb->support = true;
	ufs_wb->state_ts = jiffies;
	ufs_wb_backup->state_ts = jiffies;

	ufs_sec_features.ufs_wb = ufs_wb;
	ufs_sec_features.ufs_wb_backup = ufs_wb_backup;

	hba->dev_info.wb_enabled = WB_OFF;

	dev_info(hba->dev, "%s: SEC WB is supported. type=%s%d, size=%u.\n",
			__func__,
			(dev_info->wb_buffer_type == WB_BUF_MODE_LU_DEDICATED) ?
			" dedicated LU" : " shared",
			(dev_info->wb_buffer_type == WB_BUF_MODE_LU_DEDICATED) ?
			dev_info->wb_dedicated_lu : 0, wb_buf_alloc);
wb_disabled:
	return;
}

/* Called by blk-sec-wb */
void ufs_sec_wb_register_reset_notify(void *func)
{
	ufs_sec_wb_reset_notify = func;
}
EXPORT_SYMBOL(ufs_sec_wb_register_reset_notify);

void ufs_sec_wb_config(struct ufs_hba *hba)
{
	/*
	 * 1. Default WB state is WB_OFF when UFS is initialized.
	 * 2. If UFS error handling occurs in WB_ON state,
	 *    It needs to be reset to WB_OFF state,
	 *    due to UFS reset.
	 */
	if (hba->dev_info.wb_enabled == WB_ON) {
		hba->dev_info.wb_enabled = WB_OFF;

		/* Call ssg's reset_noti func. */
		if (ufs_sec_wb_reset_notify != NULL)
			(*ufs_sec_wb_reset_notify)();
	}

	ufs_sec_wb_toggle_flush_during_h8(hba, true);
}
/* SEC next WB : end */

/* SEC error info : begin */
inline bool ufs_sec_is_err_cnt_allowed(void)
{
	return ufs_sec_features.ufs_err && ufs_sec_features.ufs_err_backup;
}

void ufs_sec_inc_hwrst_cnt(void)
{
	struct SEC_UFS_op_cnt *op_cnt = NULL;

	if (!ufs_sec_is_err_cnt_allowed())
		return;

	op_cnt = &get_err_member(op_cnt);

	SEC_UFS_ERR_CNT_INC(op_cnt->HW_RESET_cnt, UINT_MAX);
	SEC_UFS_ERR_CNT_INC(op_cnt->op_err, UINT_MAX);

#if IS_ENABLED(CONFIG_SEC_ABC)
	if ((op_cnt->HW_RESET_cnt % 10) == 0)
		sec_abc_send_event("MODULE=storage@WARN=ufs_hwreset_err");
#endif
}

static void ufs_sec_inc_link_startup_error_cnt(void)
{
	struct SEC_UFS_op_cnt *op_cnt = &get_err_member(op_cnt);

	SEC_UFS_ERR_CNT_INC(op_cnt->link_startup_cnt, UINT_MAX);
	SEC_UFS_ERR_CNT_INC(op_cnt->op_err, UINT_MAX);
}

static void ufs_sec_inc_uic_cmd_error(struct ufs_hba *hba, u32 cmd, bool timeout)
{
	struct SEC_UFS_UIC_cmd_cnt *uiccmd_cnt = NULL;
	struct SEC_UFS_op_cnt *op_cnt = NULL;

	if (!ufs_sec_is_err_cnt_allowed())
		return;

	if (((hba->active_uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT)
			== UIC_CMD_RESULT_SUCCESS)
			&& !timeout)
		return;

	uiccmd_cnt = &get_err_member(UIC_cmd_cnt);
	op_cnt = &get_err_member(op_cnt);

	switch (cmd & COMMAND_OPCODE_MASK) {
	case UIC_CMD_DME_GET:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_GET_err, U8_MAX);
		break;
	case UIC_CMD_DME_SET:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_SET_err, U8_MAX);
		break;
	case UIC_CMD_DME_PEER_GET:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_PEER_GET_err, U8_MAX);
		break;
	case UIC_CMD_DME_PEER_SET:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_PEER_SET_err, U8_MAX);
		break;
	case UIC_CMD_DME_POWERON:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_POWERON_err, U8_MAX);
		break;
	case UIC_CMD_DME_POWEROFF:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_POWEROFF_err, U8_MAX);
		break;
	case UIC_CMD_DME_ENABLE:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_ENABLE_err, U8_MAX);
		break;
	case UIC_CMD_DME_RESET:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_RESET_err, U8_MAX);
		break;
	case UIC_CMD_DME_END_PT_RST:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_END_PT_RST_err, U8_MAX);
		break;
	case UIC_CMD_DME_LINK_STARTUP:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_LINK_STARTUP_err, U8_MAX);
		break;
	case UIC_CMD_DME_HIBER_ENTER:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_HIBER_ENTER_err, U8_MAX);
		SEC_UFS_ERR_CNT_INC(op_cnt->Hibern8_enter_cnt, UINT_MAX);
		SEC_UFS_ERR_CNT_INC(op_cnt->op_err, UINT_MAX);
		break;
	case UIC_CMD_DME_HIBER_EXIT:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_HIBER_EXIT_err, U8_MAX);
		SEC_UFS_ERR_CNT_INC(op_cnt->Hibern8_exit_cnt, UINT_MAX);
		SEC_UFS_ERR_CNT_INC(op_cnt->op_err, UINT_MAX);
		break;
	case UIC_CMD_DME_TEST_MODE:
		SEC_UFS_ERR_CNT_INC(uiccmd_cnt->DME_TEST_MODE_err, U8_MAX);
		break;
	default:
		break;
	}

	SEC_UFS_ERR_CNT_INC(uiccmd_cnt->UIC_cmd_err, UINT_MAX);
}

static void ufs_sec_inc_uic_fatal(u32 errors)
{
	struct SEC_UFS_Fatal_err_cnt *f_ec = &get_err_member(Fatal_err_cnt);

	if (errors & DEVICE_FATAL_ERROR) {
		SEC_UFS_ERR_CNT_INC(f_ec->DFE, U8_MAX);
		SEC_UFS_ERR_CNT_INC(f_ec->Fatal_err, UINT_MAX);
	}
	if (errors & CONTROLLER_FATAL_ERROR) {
		SEC_UFS_ERR_CNT_INC(f_ec->CFE, U8_MAX);
		SEC_UFS_ERR_CNT_INC(f_ec->Fatal_err, UINT_MAX);
	}
	if (errors & SYSTEM_BUS_FATAL_ERROR) {
		SEC_UFS_ERR_CNT_INC(f_ec->SBFE, U8_MAX);
		SEC_UFS_ERR_CNT_INC(f_ec->Fatal_err, UINT_MAX);
	}
	if (errors & CRYPTO_ENGINE_FATAL_ERROR) {
		SEC_UFS_ERR_CNT_INC(f_ec->CEFE, U8_MAX);
		SEC_UFS_ERR_CNT_INC(f_ec->Fatal_err, UINT_MAX);
	}
	/* UIC_LINK_LOST : can not be checked in ufshcd.c */
	if (errors & UIC_LINK_LOST) {
		SEC_UFS_ERR_CNT_INC(f_ec->LLE, U8_MAX);
		SEC_UFS_ERR_CNT_INC(f_ec->Fatal_err, UINT_MAX);
	}
}

static void ufs_sec_inc_uic_error(enum ufs_event_type evt, u32 reg)
{
	struct SEC_UFS_UIC_err_cnt *uicerr_cnt = &get_err_member(UIC_err_cnt);
	int val = 0;

	switch (evt) {
	case UFS_EVT_PA_ERR:
		SEC_UFS_ERR_CNT_INC(uicerr_cnt->PAERR_cnt, U8_MAX);
		SEC_UFS_ERR_CNT_INC(uicerr_cnt->UIC_err, UINT_MAX);

		if (reg & UIC_PHY_ADAPTER_LAYER_GENERIC_ERROR)
			SEC_UFS_ERR_CNT_INC(uicerr_cnt->PAERR_linereset,
					UINT_MAX);

		val = reg & UIC_PHY_ADAPTER_LAYER_LANE_ERR_MASK;
		if (val)
			SEC_UFS_ERR_CNT_INC(uicerr_cnt->PAERR_lane[val - 1],
					UINT_MAX);
		break;
	case UFS_EVT_DL_ERR:
		if (reg & UIC_DATA_LINK_LAYER_ERROR_PA_INIT) {
			SEC_UFS_ERR_CNT_INC(uicerr_cnt->DL_PA_INIT_ERR_cnt,
					U8_MAX);
			SEC_UFS_ERR_CNT_INC(uicerr_cnt->UIC_err, UINT_MAX);
		}
		if (reg & UIC_DATA_LINK_LAYER_ERROR_NAC_RECEIVED) {
			SEC_UFS_ERR_CNT_INC(uicerr_cnt->DL_NAC_RCVD_ERR_cnt,
					U8_MAX);
			SEC_UFS_ERR_CNT_INC(uicerr_cnt->UIC_err, UINT_MAX);
		}
		if (reg & UIC_DATA_LINK_LAYER_ERROR_TCx_REPLAY_TIMEOUT) {
			SEC_UFS_ERR_CNT_INC(uicerr_cnt->DL_TC_REPLAY_ERR_cnt,
					U8_MAX);
			SEC_UFS_ERR_CNT_INC(uicerr_cnt->UIC_err, UINT_MAX);
		}
		break;
	case UFS_EVT_NL_ERR:
		SEC_UFS_ERR_CNT_INC(uicerr_cnt->NLERR_cnt, U8_MAX);
		SEC_UFS_ERR_CNT_INC(uicerr_cnt->UIC_err, UINT_MAX);
		break;
	case UFS_EVT_TL_ERR:
		SEC_UFS_ERR_CNT_INC(uicerr_cnt->TLERR_cnt, U8_MAX);
		SEC_UFS_ERR_CNT_INC(uicerr_cnt->UIC_err, UINT_MAX);
		break;
	case UFS_EVT_DME_ERR:
		SEC_UFS_ERR_CNT_INC(uicerr_cnt->DMEERR_cnt, U8_MAX);
		SEC_UFS_ERR_CNT_INC(uicerr_cnt->UIC_err, UINT_MAX);
		break;
	default:
		break;
	}
}

static void ufs_sec_inc_tm_error(u8 tm_cmd)
{
	struct SEC_UFS_UTP_cnt *utp_err = &get_err_member(UTP_cnt);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	struct ufs_vendor_dev_info *ufs_vdi = ufs_sec_features.vdi;

	if ((tm_cmd == UFS_LOGICAL_RESET) && ufs_vdi)
		ufs_vdi->device_stuck = true;
#endif
	switch (tm_cmd) {
	case UFS_QUERY_TASK:
		SEC_UFS_ERR_CNT_INC(utp_err->UTMR_query_task_cnt, U8_MAX);
		break;
	case UFS_ABORT_TASK:
		SEC_UFS_ERR_CNT_INC(utp_err->UTMR_abort_task_cnt, U8_MAX);
		break;
	case UFS_LOGICAL_RESET:
		SEC_UFS_ERR_CNT_INC(utp_err->UTMR_logical_reset_cnt, U8_MAX);
		break;
	default:
		break;
	}

	SEC_UFS_ERR_CNT_INC(utp_err->UTP_err, UINT_MAX);
}

#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
#define UFS_SEC_TESTMODE_CMD_LOG_MAX 10
static void ufs_sec_print_cmdlog(struct ufs_hba *hba)
{
	struct ufs_sec_cmd_log_info *ufs_cmd_log =
		ufs_sec_features.ufs_cmd_log;
	struct ufs_sec_cmd_log_entry *entry = NULL;
	int i = (ufs_cmd_log->pos + UFS_SEC_CMD_LOGGING_MAX
			- UFS_SEC_TESTMODE_CMD_LOG_MAX);
	int idx = 0;

	dev_err(hba->dev, "UFS CMD hist\n");
	dev_err(hba->dev, "%02s: %10s: %2s %3s %4s %9s %6s %16s\n",
			"No", "log string", "lu", "tag",
			"c_id", "lba", "length", "time");

	for (idx = 0; idx < UFS_SEC_TESTMODE_CMD_LOG_MAX; idx++, i++) {
		i %= UFS_SEC_CMD_LOGGING_MAX;
		entry = &ufs_cmd_log->entries[i];
		dev_err(hba->dev, "%2d: %10s: %2d %3d 0x%02x %9u %6d %16llu\n",
				idx,
				entry->str, entry->lun, entry->tag,
				entry->cmd_id, entry->lba,
				entry->transfer_len, entry->tstamp);
	}
}
#endif	// CONFIG_SEC_UFS_CMD_LOGGING

static void ufs_sec_print_evt_hist(struct ufs_hba *hba);

static void ufs_sec_trigger_bug(struct ufs_hba *hba)
{
	// print all event due to the last event would never be printed
	ufs_sec_print_evt_hist(hba);
#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
	// print cmd log
	ufs_sec_print_cmdlog(hba);
#endif
	BUG();
}
#endif	// CONFIG_SCSI_UFS_TEST_MODE

void ufs_sec_check_device_stuck(void)
{
	struct ufs_vendor_dev_info *ufs_vdi = ufs_sec_features.vdi;

	if (!ufs_vdi)
		return;

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	if (ufs_vdi && ufs_vdi->device_stuck) {
		/* waiting for cache flush and make a panic */
		ssleep(2);
		panic("UFS TM ERROR\n");
	}
#endif

#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
	/*
	 * do not recover system if test mode is enabled
	 * reset recovery is in progress from ufshcd_err_handler
	 */
	if (ufs_vdi && ufs_vdi->hba && ufs_vdi->hba->eh_flags)
		ufs_sec_trigger_bug(ufs_vdi->hba);
#endif
}

static void ufs_sec_inc_utp_error(struct ufs_hba *hba, int tag)
{
	struct SEC_UFS_UTP_cnt *utp_err = &get_err_member(UTP_cnt);
	struct ufshcd_lrb *lrbp = NULL;
	int opcode = 0;

	if (tag >= hba->nutrs)
		return;

	lrbp = &hba->lrb[tag];
	if (!lrbp || !lrbp->cmd || (lrbp->task_tag != tag))
		return;

	opcode = lrbp->cmd->cmnd[0];

	switch (opcode) {
	case WRITE_10:
		SEC_UFS_ERR_CNT_INC(utp_err->UTR_write_err, U8_MAX);
		break;
	case READ_10:
	case READ_16:
		SEC_UFS_ERR_CNT_INC(utp_err->UTR_read_err, U8_MAX);
		break;
	case SYNCHRONIZE_CACHE:
		SEC_UFS_ERR_CNT_INC(utp_err->UTR_sync_cache_err, U8_MAX);
		break;
	case UNMAP:
		SEC_UFS_ERR_CNT_INC(utp_err->UTR_unmap_err, U8_MAX);
		break;
	default:
		SEC_UFS_ERR_CNT_INC(utp_err->UTR_etc_err, U8_MAX);
		break;
	}

	SEC_UFS_ERR_CNT_INC(utp_err->UTP_err, UINT_MAX);
}

static void ufs_sec_inc_query_error(struct ufs_hba *hba,
		struct ufshcd_lrb *lrbp, bool timeout)
{
	struct SEC_UFS_QUERY_cnt *query_cnt = NULL;
	struct ufs_query_req *request = &hba->dev_cmd.query.request;
	enum query_opcode opcode = request->upiu_req.opcode;
	enum dev_cmd_type cmd_type = hba->dev_cmd.type;

	if (!ufs_sec_is_err_cnt_allowed())
		return;

	if (((le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS)
			== OCS_SUCCESS)
			&& !timeout)
		return;

	/* get last query cmd information when timeout occurs */
	if (timeout) {
		opcode = ufs_sec_features.last_qcmd;
		cmd_type = ufs_sec_features.qcmd_type;
	}

	query_cnt = &get_err_member(Query_cnt);

	if (cmd_type == DEV_CMD_TYPE_NOP) {
		SEC_UFS_ERR_CNT_INC(query_cnt->NOP_err, U8_MAX);
	} else {
		switch (opcode) {
		case UPIU_QUERY_OPCODE_READ_DESC:
			SEC_UFS_ERR_CNT_INC(query_cnt->R_Desc_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_WRITE_DESC:
			SEC_UFS_ERR_CNT_INC(query_cnt->W_Desc_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_READ_ATTR:
			SEC_UFS_ERR_CNT_INC(query_cnt->R_Attr_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_WRITE_ATTR:
			SEC_UFS_ERR_CNT_INC(query_cnt->W_Attr_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_READ_FLAG:
			SEC_UFS_ERR_CNT_INC(query_cnt->R_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_SET_FLAG:
			SEC_UFS_ERR_CNT_INC(query_cnt->Set_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_CLEAR_FLAG:
			SEC_UFS_ERR_CNT_INC(query_cnt->Clear_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
			SEC_UFS_ERR_CNT_INC(query_cnt->Toggle_Flag_err,
					U8_MAX);
			break;
		default:
			break;
		}
	}

	SEC_UFS_ERR_CNT_INC(query_cnt->Query_err, UINT_MAX);
}

void ufs_sec_inc_op_err(struct ufs_hba *hba, enum ufs_event_type evt,
		void *data)
{
	u32 error_val = *(u32 *)data;

	if (!ufs_sec_is_err_cnt_allowed())
		return;

	switch (evt) {
	case UFS_EVT_LINK_STARTUP_FAIL:
		ufs_sec_inc_link_startup_error_cnt();
		break;
	case UFS_EVT_DEV_RESET:
		break;
	case UFS_EVT_PA_ERR:
	case UFS_EVT_DL_ERR:
	case UFS_EVT_NL_ERR:
	case UFS_EVT_TL_ERR:
	case UFS_EVT_DME_ERR:
		if (error_val)
			ufs_sec_inc_uic_error(evt, error_val);
		break;
	case UFS_EVT_FATAL_ERR:
		if (error_val)
			ufs_sec_inc_uic_fatal(error_val);
		break;
	case UFS_EVT_ABORT:
		ufs_sec_inc_utp_error(hba, (int)error_val);
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
#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
#define UFS_TEST_COUNT 3
	if (evt == UFS_EVT_PA_ERR || evt == UFS_EVT_DL_ERR ||
			evt == UFS_EVT_LINK_STARTUP_FAIL) {
		struct ufs_event_hist *e = &hba->ufs_stats.event[evt];

		if (e->cnt > UFS_TEST_COUNT)
			ufs_sec_trigger_bug(hba);
	}
#endif
}

static void ufs_sec_update_medium_err_region(struct ufs_sec_cmd_info *ufs_cmd)
{
	struct SEC_SCSI_SENSE_err_log *se_log = &get_err_member(sense_err_log);
	unsigned int lba_cnt = se_log->issue_LBA_cnt;
	unsigned long region_bit = 0;
	int i = 0;

	/*
	 * When MEDIUM_ERROR occurs,
	 * 1. issue_LBA_list[] : record LBAs
	 * 2. issue_region_map : set bit the region
	 *    ______________________________________________
	 *    |63|62|61|60| ....    |52|51|50| ....    |1|0|
	 *    ----------------------------------------------
	 *   1) 0 ~ 51 : per 300MB : total 15600MB region (lba 0~3993600)
	 *   2) 52  : region of 15600MB~ (USERDATA)
	 *   3) ~63 : other LU
	 *     -> In case of SEC_UFS_USER_LUN == 0
	 *       >> If MEDIUM_ERROR occurs on LU1, set bit "62"
	 *       >> If MEDIUM_ERROR occurs on LU2, set bit "61"
	 *       ....
	 *       >> If MEDIUM_ERROR occurs on LU7, set bit "56"
	 *     -> In case of SEC_UFS_USER_LUN != 0
	 *       >> If MEDIUM_ERROR occurs on LU0, set bit "63"
	 */
	if (ufs_cmd->lun == SEC_UFS_USER_LUN) {
		if (lba_cnt < SEC_MAX_LBA_LOGGING) {
			for (i = 0; i < SEC_MAX_LBA_LOGGING; i++) {
				if (se_log->issue_LBA_list[i] == ufs_cmd->lba)
					return;
			}
			se_log->issue_LBA_list[lba_cnt] = ufs_cmd->lba;
			se_log->issue_LBA_cnt++;
		}

		region_bit = ufs_cmd->lba / SEC_ISSUE_REGION_STEP;
		if (region_bit > 51)
			region_bit = 52;
	} else if (ufs_cmd->lun < UFS_UPIU_MAX_GENERAL_LUN) {
		region_bit = (unsigned long)(63 - ufs_cmd->lun);
	}

	se_log->issue_region_map |= ((u64)1 << region_bit);
}

static void ufs_sec_inc_sense_err(struct ufshcd_lrb *lrbp,
		struct ufs_sec_cmd_info *ufs_cmd)
{
	struct SEC_SCSI_SENSE_cnt *sense_err = NULL;
	u8 sense_key = 0;
	u8 asc = 0;
	u8 ascq = 0;
	bool secdbgMode = false;

	sense_key = lrbp->ucd_rsp_ptr->sr.sense_data[2] & 0x0F;
	if (sense_key != MEDIUM_ERROR && sense_key != HARDWARE_ERROR)
		return;

#if IS_ENABLED(CONFIG_SEC_DEBUG)
	secdbgMode = sec_debug_is_enabled();
#endif
	asc = lrbp->ucd_rsp_ptr->sr.sense_data[12];
	ascq = lrbp->ucd_rsp_ptr->sr.sense_data[13];

	pr_err("UFS: LU%u: sense key 0x%x(asc 0x%x, ascq 0x%x),"
			"opcode 0x%x, lba 0x%x, len 0x%x.\n",
			ufs_cmd->lun, sense_key, asc, ascq,
			ufs_cmd->opcode, ufs_cmd->lba, ufs_cmd->transfer_len);

	if (!ufs_sec_is_err_cnt_allowed())
		goto out;

	sense_err = &get_err_member(sense_cnt);

	if (sense_key == MEDIUM_ERROR) {
		sense_err->scsi_medium_err++;
		ufs_sec_update_medium_err_region(ufs_cmd);
#if IS_ENABLED(CONFIG_SEC_ABC)
		sec_abc_send_event("MODULE=storage@WARN=ufs_medium_err");
#endif
	} else
		sense_err->scsi_hw_err++;

out:
	if (secdbgMode)
		panic("ufs %s error\n", (sense_key == MEDIUM_ERROR) ?
					"medium" : "hardware");
}

void ufs_sec_print_err_info(struct ufs_hba *hba)
{
	dev_err(hba->dev, "Count: %u UIC: %u UTP: %u QUERY: %u\n",
		SEC_UFS_ERR_INFO_GET_VALUE(op_cnt, HW_RESET_cnt),
		SEC_UFS_ERR_INFO_GET_VALUE(UIC_err_cnt, UIC_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UTP_cnt, UTP_err),
		SEC_UFS_ERR_INFO_GET_VALUE(Query_cnt, Query_err));

	dev_err(hba->dev, "Sense Key: medium: %u, hw: %u\n",
		SEC_UFS_ERR_INFO_GET_VALUE(sense_cnt, scsi_medium_err),
		SEC_UFS_ERR_INFO_GET_VALUE(sense_cnt, scsi_hw_err));
}

static void ufs_sec_init_error_logging(struct device *dev)
{
	struct ufs_sec_err_info *ufs_err = NULL;
	struct ufs_sec_err_info *ufs_err_backup = NULL;
	struct ufs_sec_err_info *ufs_err_hist = NULL;

	ufs_err = devm_kzalloc(dev, sizeof(struct ufs_sec_err_info),
			GFP_KERNEL);
	ufs_err_backup = devm_kzalloc(dev, sizeof(struct ufs_sec_err_info),
			GFP_KERNEL);
	ufs_err_hist = devm_kzalloc(dev, sizeof(struct ufs_sec_err_info),
			GFP_KERNEL);
	if (!ufs_err || !ufs_err_backup || !ufs_err_hist) {
		dev_err(dev, "%s: Failed allocating ufs_err(backup)(%lu)",
				__func__, sizeof(struct ufs_sec_err_info));
		devm_kfree(dev, ufs_err);
		devm_kfree(dev, ufs_err_backup);
		devm_kfree(dev, ufs_err_hist);
		return;
	}

	ufs_sec_features.ufs_err = ufs_err;
	ufs_sec_features.ufs_err_backup = ufs_err_backup;
	ufs_sec_features.ufs_err_hist = ufs_err_hist;

	ufs_sec_features.ucmd_complete = true;
	ufs_sec_features.qcmd_complete = true;
}
/* SEC error info : end */

#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
/* SEC cmd log : begin */
inline bool ufs_sec_is_cmd_log_allowed(void)
{
	return ufs_sec_features.ufs_cmd_log;
}

static void __ufs_sec_log_cmd(struct ufs_hba *hba, int str_idx,
		unsigned int tag, u8 cmd_id, u8 idn, u8 lun, u32 lba,
		int transfer_len)
{
	struct ufs_sec_cmd_log_info *ufs_cmd_log =
		ufs_sec_features.ufs_cmd_log;
	struct ufs_sec_cmd_log_entry *entry =
		&ufs_cmd_log->entries[ufs_cmd_log->pos];

	entry->lun = lun;
	entry->str = ufs_sec_log_str[str_idx];
	entry->cmd_id = cmd_id;
	entry->lba = lba;
	entry->transfer_len = transfer_len;
	entry->idn = idn;
	entry->tag = tag;
	entry->tstamp = ktime_get();
	entry->outstanding_reqs = hba->outstanding_reqs;
	ufs_cmd_log->pos =
		(ufs_cmd_log->pos + 1) % UFS_SEC_CMD_LOGGING_MAX;
}

static void ufs_sec_log_cmd(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
		int str_t, struct ufs_sec_cmd_info *ufs_cmd, u8 cmd_id)
{
	u8 opcode = 0;
	u8 idn = 0;

	if (!ufs_sec_is_cmd_log_allowed())
		return;

	switch (str_t) {
	case UFS_SEC_CMD_SEND:
	case UFS_SEC_CMD_COMP:
		__ufs_sec_log_cmd(hba, str_t, lrbp->task_tag, ufs_cmd->opcode,
				0, lrbp->lun, ufs_cmd->lba,
				ufs_cmd->transfer_len);
		break;
	case UFS_SEC_QUERY_SEND:
	case UFS_SEC_QUERY_COMP:
		opcode = hba->dev_cmd.query.request.upiu_req.opcode;
		idn = hba->dev_cmd.query.request.upiu_req.idn;
		__ufs_sec_log_cmd(hba, str_t, lrbp->task_tag, opcode,
				idn, lrbp->lun, 0, 0);
		break;
	case UFS_SEC_NOP_SEND:
	case UFS_SEC_NOP_COMP:
		__ufs_sec_log_cmd(hba, str_t, lrbp->task_tag, 0,
				0, lrbp->lun, 0, 0);
		break;
	case UFS_SEC_TM_SEND:
	case UFS_SEC_TM_COMP:
	case UFS_SEC_TM_ERR:
	case UFS_SEC_UIC_SEND:
	case UFS_SEC_UIC_COMP:
		__ufs_sec_log_cmd(hba, str_t, 0, cmd_id, 0, 0, 0, 0);
		break;
	default:
		break;
	}
}

static void ufs_sec_init_cmd_logging(struct device *dev)
{
	struct ufs_sec_cmd_log_info *ufs_cmd_log = NULL;

	ufs_cmd_log = devm_kzalloc(dev, sizeof(struct ufs_sec_cmd_log_info),
			GFP_KERNEL);
	if (!ufs_cmd_log) {
		dev_err(dev, "%s: Failed allocating ufs_cmd_log(%lu)",
				__func__,
				sizeof(struct ufs_sec_cmd_log_info));
		return;
	}

	ufs_cmd_log->entries = devm_kcalloc(dev, UFS_SEC_CMD_LOGGING_MAX,
			sizeof(struct ufs_sec_cmd_log_entry), GFP_KERNEL);
	if (!ufs_cmd_log->entries) {
		dev_err(dev, "%s: Failed allocating cmd log entry(%lu)",
				__func__,
				sizeof(struct ufs_sec_cmd_log_entry)
				* UFS_SEC_CMD_LOGGING_MAX);
		devm_kfree(dev, ufs_cmd_log);
		return;
	}

	pr_info("SEC UFS cmd logging is initialized.\n");

	ufs_sec_features.ufs_cmd_log = ufs_cmd_log;
}
/* SEC cmd log : end */
#endif

/* panic notifier : begin */
static void ufs_sec_print_evt(struct ufs_hba *hba, u32 id,
		char *err_name)
{
	int i;
	bool found = false;
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];

	for (i = 0; i < UFS_EVENT_HIST_LENGTH; i++) {
		int p = (i + e->pos) % UFS_EVENT_HIST_LENGTH;

		// do not print if dev_reset[0]'s tstamp is in 2 sec.
		// because that happened on booting seq.
		if ((id == UFS_EVT_DEV_RESET) && (p == 0) &&
				(ktime_to_us(e->tstamp[p]) < 2000000))
			continue;

		if (e->tstamp[p] == 0)
			continue;
		dev_err(hba->dev, "%s[%d] = 0x%x at %lld us\n", err_name, p,
			e->val[p], ktime_to_us(e->tstamp[p]));
		found = true;
	}

	if (!found)
		dev_err(hba->dev, "No record of %s\n", err_name);
	else
		dev_err(hba->dev, "%s: total cnt=%llu\n", err_name, e->cnt);
}

static void ufs_sec_print_evt_hist(struct ufs_hba *hba)
{
	ufshcd_dump_regs(hba, 0, UFSHCI_REG_SPACE_SIZE, "host_regs: ");

	ufs_sec_print_evt(hba, UFS_EVT_PA_ERR, "pa_err");
	ufs_sec_print_evt(hba, UFS_EVT_DL_ERR, "dl_err");
	ufs_sec_print_evt(hba, UFS_EVT_NL_ERR, "nl_err");
	ufs_sec_print_evt(hba, UFS_EVT_TL_ERR, "tl_err");
	ufs_sec_print_evt(hba, UFS_EVT_DME_ERR, "dme_err");
	ufs_sec_print_evt(hba, UFS_EVT_AUTO_HIBERN8_ERR,
			"auto_hibern8_err");
	ufs_sec_print_evt(hba, UFS_EVT_FATAL_ERR, "fatal_err");
	ufs_sec_print_evt(hba, UFS_EVT_LINK_STARTUP_FAIL,
			"link_startup_fail");
	ufs_sec_print_evt(hba, UFS_EVT_RESUME_ERR, "resume_fail");
	ufs_sec_print_evt(hba, UFS_EVT_SUSPEND_ERR,
			"suspend_fail");
	ufs_sec_print_evt(hba, UFS_EVT_DEV_RESET, "dev_reset");
	ufs_sec_print_evt(hba, UFS_EVT_HOST_RESET, "host_reset");
	ufs_sec_print_evt(hba, UFS_EVT_ABORT, "task_abort");
}

/*
 * ufs_sec_check_and_is_err - Check and compare UFS Error counts
 * @buf: has to be filled by using SEC_UFS_ERR_SUM() or SEC_UFS_ERR_HIST_SUM()
 *
 * Returns
 *  0     - If the buf has no error counts
 *  other - If the buf is invalid or has error count value
 */
static int ufs_sec_check_and_is_err(char *buf)
{
	const char *no_err = "U0I0H0L0X0Q0R0W0F0SM0SH0";

	if (!buf || strlen(buf) < strlen(no_err))
		return -EINVAL;

	return strncmp(buf, no_err, strlen(no_err));
}

static int ufs_sec_panic_callback(struct notifier_block *nfb,
		unsigned long event, void *panic_msg)
{
	struct ufs_vendor_dev_info *ufs_vdi = ufs_sec_features.vdi;
	char err_buf[ERR_SUM_SIZE];
	char hist_buf[ERR_HIST_SUM_SIZE];
	char *str = (char *)panic_msg;
	bool is_FSpanic = !strncmp(str, "F2FS", 4) || !strncmp(str, "EXT4", 4);

	SEC_UFS_ERR_SUM(err_buf);
	SEC_UFS_ERR_HIST_SUM(hist_buf);
	pr_info("ufs: %s hist: %s", err_buf, hist_buf);

	if (ufs_vdi && is_FSpanic && ufs_sec_check_and_is_err(err_buf))
		ufs_sec_print_evt_hist(ufs_vdi->hba);

	return NOTIFY_OK;
}

static struct notifier_block ufs_sec_panic_notifier = {
	.notifier_call = ufs_sec_panic_callback,
	.priority = 1,
};
/* panic notifier : end */

/* reboot notifier : begin */
static int ufs_sec_reboot_notify(struct notifier_block *notify_block,
		unsigned long event, void *unused)
{
	char buf[ERR_HIST_SUM_SIZE];

	SEC_UFS_ERR_HIST_SUM(buf);
	pr_info("%s: UFS Info: %s\n", __func__, buf);

	return NOTIFY_OK;
}
/* reboot notifier : end */

/* I/O error uevent : begin */
void ufs_sec_print_err(void)
{
	char err_buf[ERR_SUM_SIZE];
	char hist_buf[ERR_HIST_SUM_SIZE];

	SEC_UFS_ERR_SUM(err_buf);
	SEC_UFS_ERR_HIST_SUM(hist_buf);

	if (ufs_sec_check_and_is_err(hist_buf))
		pr_info("ufs: %s, hist : %s", err_buf, hist_buf);
}

static void ufs_sec_err_noti_work(struct work_struct *work)
{
	int ret;
	char buf[ERR_SUM_SIZE];

	SEC_UFS_ERR_SUM(buf);
	pr_info("%s: UFS Info: %s\n", __func__, buf);

	ret = kobject_uevent(&sec_ufs_cmd_dev->kobj, KOBJ_CHANGE);
	if (ret)
		pr_err("%s: Failed to send uevent with err %d\n", __func__, ret);
}

static int ufs_sec_err_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	char buf[ERR_SUM_SIZE];

	add_uevent_var(env, "DEVNAME=%s", dev->kobj.name);
	add_uevent_var(env, "NAME=UFSINFO");

	SEC_UFS_ERR_SUM(buf);
	return add_uevent_var(env, "DATA=%s", buf);
}

static struct device_type ufs_type = {
	.uevent = ufs_sec_err_uevent,
};

static inline bool ufs_sec_is_uevent_condition(u32 hw_rst_cnt)
{
	return ((hw_rst_cnt == 1) || !(hw_rst_cnt % 10));
}

static void ufs_sec_trigger_err_noti_uevent(struct ufs_hba *hba)
{
	u32 hw_rst_cnt = SEC_UFS_ERR_INFO_GET_VALUE(op_cnt, HW_RESET_cnt);
	char buf[ERR_SUM_SIZE];

	/* eh_flags is only set during error handling */
	if (!hba->eh_flags)
		return;

	SEC_UFS_ERR_SUM(buf);

	if (!ufs_sec_check_and_is_err(buf))
		return;

	if (!ufs_sec_is_uevent_condition(hw_rst_cnt))
		return;

	if (sec_ufs_cmd_dev)
		schedule_delayed_work(&ufs_sec_features.noti_work,
				msecs_to_jiffies(NOTI_WORK_DELAY_MS));
}
/* I/O error uevent : end */

void ufs_sec_config_features(struct ufs_hba *hba)
{
	ufs_sec_wb_config(hba);
	ufs_sec_trigger_err_noti_uevent(hba);
}

void ufs_sec_adjust_caps_quirks(struct ufs_hba *hba)
{
	hba->caps &= ~UFSHCD_CAP_WB_EN;
}

void ufs_sec_init_logging(struct device *dev)
{
	ufs_sec_init_error_logging(dev);
#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
	ufs_sec_init_cmd_logging(dev);
#endif
}

void ufs_sec_set_features(struct ufs_hba *hba)
{
	struct ufs_vendor_dev_info *vdi = NULL;
	int buff_len;
	u8 *desc_buf = NULL;
	int err;

	if (ufs_sec_features.vdi)
		return;

	vdi = devm_kzalloc(hba->dev, sizeof(struct ufs_vendor_dev_info),
			GFP_KERNEL);
	if (!vdi) {
		dev_err(hba->dev, "%s: Failed allocating ufs_vdi(%lu)",
				__func__, sizeof(struct ufs_vendor_dev_info));
		return;
	}

	vdi->hba = hba;

	ufs_sec_features.vdi = vdi;

	/* read device desc */
	buff_len = hba->desc_size[QUERY_DESC_IDN_DEVICE];
	desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!desc_buf)
		return;

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_DEVICE, 0, 0,
			desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading device desc. err %d",
				__func__, err);
		goto out;
	}

	ufs_sec_set_unique_number(hba, desc_buf);
	ufs_sec_get_health_desc(hba);
	ufs_sec_get_ext_feature(hba, desc_buf);

	ufs_sec_wb_probe(hba, desc_buf);

	ufs_sec_add_sysfs_nodes(hba);

	atomic_notifier_chain_register(&panic_notifier_list,
			&ufs_sec_panic_notifier);

	ufs_sec_features.reboot_notify.notifier_call = ufs_sec_reboot_notify;
	register_reboot_notifier(&ufs_sec_features.reboot_notify);
	sec_ufs_cmd_dev->type = &ufs_type;
	INIT_DELAYED_WORK(&ufs_sec_features.noti_work, ufs_sec_err_noti_work);
out:
	if (desc_buf)
		kfree(desc_buf);
}

void ufs_sec_remove_features(struct ufs_hba *hba)
{
	ufs_sec_remove_sysfs_nodes(hba);
	unregister_reboot_notifier(&ufs_sec_features.reboot_notify);
}

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
static void ufs_sec_check_invalid_opcode(u8 opcode)
{
	/* add opcode cases to check */
	/* UFS device doesn't support WRITE SAME CMDs */
	switch (opcode) {
	case WRITE_SAME:
	case WRITE_SAME_16:
		panic("Check UFS opcode 0x%x", opcode);
		break;
	default:
		break;
	}
}
#endif

static bool ufs_sec_get_scsi_cmd_info(struct ufshcd_lrb *lrbp,
		struct ufs_sec_cmd_info *ufs_cmd)
{
	struct scsi_cmnd *cmd;

	if (!lrbp || !lrbp->cmd || !ufs_cmd)
		return false;

	cmd = lrbp->cmd;

	ufs_cmd->opcode = (u8)(*cmd->cmnd);
	ufs_cmd->lba = ((cmd->cmnd[2] << 24) | (cmd->cmnd[3] << 16) |
			(cmd->cmnd[4] << 8) | cmd->cmnd[5]);
	ufs_cmd->transfer_len = (cmd->cmnd[7] << 8) | cmd->cmnd[8];
	ufs_cmd->lun = ufshcd_scsi_to_upiu_lun(cmd->device->lun);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	ufs_sec_check_invalid_opcode(ufs_cmd->opcode);
#endif
	return true;
}

static void ufs_sec_customize_upiu_flags(struct ufshcd_lrb *lrbp)
{
	u8 upiu_flags = 0x0;
	struct request *rq;

	if (!lrbp->cmd || !lrbp->ucd_req_ptr)
		return;

	rq = scsi_cmd_to_rq(lrbp->cmd);
	switch (req_op(rq)) {
	case REQ_OP_READ:
		upiu_flags |= UPIU_CMD_PRIO_HIGH;
		break;
	case REQ_OP_WRITE:
		if (rq->cmd_flags & REQ_SYNC)
			upiu_flags |= UPIU_CMD_PRIO_HIGH;
		break;
	case REQ_OP_FLUSH:
		upiu_flags |= UPIU_TASK_ATTR_HEADQ;
		break;
	case REQ_OP_DISCARD:
		upiu_flags |= UPIU_TASK_ATTR_ORDERED;
		break;
	}

	lrbp->ucd_req_ptr->header.dword_0 |=
		UPIU_HEADER_DWORD(0, upiu_flags, 0, 0);
}

/*
 * vendor hooks
 * check include/trace/hooks/ufshcd.h
 */
static void sec_android_vh_ufs_send_command(void *data,
		struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_sec_cmd_info ufs_cmd = { 0, };
	struct ufs_query_req *request = NULL;
	enum dev_cmd_type cmd_type;
	enum query_opcode opcode;
	bool is_scsi_cmd = false;

	is_scsi_cmd = ufs_sec_get_scsi_cmd_info(lrbp, &ufs_cmd);

	if (is_scsi_cmd) {
		ufs_sec_customize_upiu_flags(lrbp);

#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
		ufs_sec_log_cmd(hba, lrbp, UFS_SEC_CMD_SEND, &ufs_cmd, 0);
#endif
	} else {
#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
		if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP)
			ufs_sec_log_cmd(hba, lrbp, UFS_SEC_NOP_SEND, NULL, 0);
		else if (hba->dev_cmd.type == DEV_CMD_TYPE_QUERY)
			ufs_sec_log_cmd(hba, lrbp, UFS_SEC_QUERY_SEND,
					NULL, 0);
#endif

		/* in timeout error case, last cmd is not completed */
		if (!ufs_sec_features.qcmd_complete)
			ufs_sec_inc_query_error(hba, lrbp, true);

		request = &hba->dev_cmd.query.request;
		opcode = request->upiu_req.opcode;
		cmd_type = hba->dev_cmd.type;

		ufs_sec_features.last_qcmd = opcode;
		ufs_sec_features.qcmd_type = cmd_type;
		ufs_sec_features.qcmd_complete = false;
	}
}

static void sec_android_vh_ufs_compl_command(void *data,
		struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_sec_cmd_info ufs_cmd = { 0, };
	bool is_scsi_cmd = false;
	int transfer_len = 0;

	is_scsi_cmd = ufs_sec_get_scsi_cmd_info(lrbp, &ufs_cmd);

	if (is_scsi_cmd) {
#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
		ufs_sec_log_cmd(hba, lrbp, UFS_SEC_CMD_COMP, &ufs_cmd, 0);
#endif

		ufs_sec_inc_sense_err(lrbp, &ufs_cmd);

		/*
		 * check hba->req_abort_count, if the cmd is aborting
		 * it's the one way to check aborting
		 * hba->req_abort_count is cleared in queuecommand and after
		 * error handling
		 */
		if (hba->req_abort_count > 0 && ufs_sec_is_err_cnt_allowed())
			ufs_sec_inc_utp_error(hba, lrbp->task_tag);

		if (hba->dev_info.wb_enabled == WB_ON
				&& ufs_cmd.opcode == WRITE_10) {
			transfer_len = be32_to_cpu(lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
			ufs_sec_wb_update_info(hba, transfer_len);
		}
	} else {
#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
		if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP)
			ufs_sec_log_cmd(hba, lrbp, UFS_SEC_NOP_COMP, NULL, 0);
		else if (hba->dev_cmd.type == DEV_CMD_TYPE_QUERY)
			ufs_sec_log_cmd(hba, lrbp, UFS_SEC_QUERY_COMP,
					NULL, 0);
#endif

		ufs_sec_features.qcmd_complete = true;

		/* check and count error, except timeout */
		ufs_sec_inc_query_error(hba, lrbp, false);
	}
}

static void sec_android_vh_ufs_send_uic_command(void *data,
		struct ufs_hba *hba, struct uic_command *ucmd, int str_t)
{
	u32 cmd;
	u8 cmd_id;

	if (str_t == UFS_CMD_SEND) {
		/* in timeout error case, last cmd is not completed */
		if (!ufs_sec_features.ucmd_complete) {
			ufs_sec_inc_uic_cmd_error(hba,
					ufs_sec_features.last_ucmd, true);
		}

		cmd = ucmd->command;
		ufs_sec_features.last_ucmd = cmd;
		ufs_sec_features.ucmd_complete = false;

		cmd_id = (u8)(cmd & COMMAND_OPCODE_MASK);
#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
		ufs_sec_log_cmd(hba, NULL, UFS_SEC_UIC_SEND, NULL, cmd_id);
#endif
	} else {
		cmd = ufshcd_readl(hba, REG_UIC_COMMAND);

		ufs_sec_features.ucmd_complete = true;

		/* check and count error, except timeout */
		ufs_sec_inc_uic_cmd_error(hba, cmd, false);

		cmd_id = (u8)(cmd & COMMAND_OPCODE_MASK);
#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
		ufs_sec_log_cmd(hba, NULL, UFS_SEC_UIC_COMP, NULL, cmd_id);
#endif
	}
}

static void sec_android_vh_ufs_send_tm_command(void *data,
		struct ufs_hba *hba, int tag, int str_t)
{
	struct utp_task_req_desc treq = { { 0 }, };
	u8 tm_func = 0;
	int sec_log_str_t = 0;

	memcpy(&treq, hba->utmrdl_base_addr + tag, sizeof(treq));

	tm_func = (be32_to_cpu(treq.upiu_req.req_header.dword_1) >> 16) & 0xFF;

	if (str_t == UFS_TM_SEND)
		sec_log_str_t = UFS_SEC_TM_SEND;
	else if (str_t == UFS_TM_COMP)
		sec_log_str_t = UFS_SEC_TM_COMP;
	else
		sec_log_str_t = UFS_SEC_TM_ERR;

#if IS_ENABLED(CONFIG_SEC_UFS_CMD_LOGGING)
	ufs_sec_log_cmd(hba, NULL, sec_log_str_t, NULL, tm_func);
#endif

	if ((str_t == UFS_TM_ERR) && ufs_sec_is_err_cnt_allowed())
		ufs_sec_inc_tm_error(tm_func);

#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
	if (str_t == UFS_TM_COMP && !hba->eh_flags) {
		dev_err(hba->dev,
			"%s: ufs tm cmd is succeeded and forced BUG called\n", __func__);
		ssleep(2);
		ufs_sec_trigger_bug(hba);
	}
#endif
}

static void sec_android_vh_ufs_update_sdev(void *data, struct scsi_device *sdev)
{
	blk_queue_rq_timeout(sdev->request_queue, SCSI_UFS_TIMEOUT);
}

void ufs_sec_register_vendor_hooks(void)
{
	register_trace_android_vh_ufs_send_command(sec_android_vh_ufs_send_command, NULL);
	register_trace_android_vh_ufs_compl_command(sec_android_vh_ufs_compl_command, NULL);
	register_trace_android_vh_ufs_send_uic_command(sec_android_vh_ufs_send_uic_command, NULL);
	register_trace_android_vh_ufs_send_tm_command(sec_android_vh_ufs_send_tm_command, NULL);
	register_trace_android_vh_ufs_update_sdev(sec_android_vh_ufs_update_sdev, NULL);
}

