/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "met_log_emi.h"
#include "gl_met_log.h"

#if CFG_SUPPORT_MET_LOG

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define MET_LOG_STATS_UPDATE_PERIOD (1000) /* ms */
#define MET_LOG_EMI_SIZE 0x19000
#define MET_LOG_EMI_RESERVED 0x10 /* for rp, wp*/
#define ADDR_SYNC_FOR_IRP 0x00401800

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
struct MET_LOG_EMI_CTRL g_met_log_emi_ctx;

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Check if EMI is empty.
 *
 */
/*----------------------------------------------------------------------------*/
static u_int8_t met_log_emi_is_empty(struct MET_LOG_EMI_CTRL *ctrl)
{
	if (ctrl->irp == ctrl->wp)
		return TRUE;
	else
		return FALSE;
}

static void met_log_emi_refresh_header(struct ADAPTER *ad,
	struct MET_LOG_EMI_CTRL *ctrl)
{
	struct mt66xx_chip_info *chip_info = ad->chip_info;
	struct MET_LOG_HEADER common = {0};

	emi_mem_read(chip_info,
		     ctrl->offset,
		     &common,
		     sizeof(common));

	ctrl->rp = common.rp;
	ctrl->wp = common.wp;
	ctrl->checksum = common.checksum;

	DBGLOG(MET, TRACE,
			"irp: 0x%x, rp: 0x%x, wp: 0x%x, checksum: 0x%x\n",
			ctrl->irp,
			ctrl->rp,
			ctrl->wp,
			ctrl->checksum);
}

static void met_log_emi_handler(void)
{
	struct MET_LOG_EMI_CTRL *ctrl = &g_met_log_emi_ctx;
	uint32_t rp = 0, recv = 0, handled = 0;
	struct ADAPTER *ad = NULL;

	DBGLOG(MET, TRACE, "\n");

	ad = (struct ADAPTER *) ctrl->priv;
	if (!ad)
		return;

	met_log_emi_refresh_header(ad, ctrl);
	ctrl->irp = ctrl->rp;
	ctrl->start_addr = ctrl->rp;
	ctrl->end_addr = ctrl->rp + ctrl->emi_size;
	DBGLOG(MET, INFO,
		"irp: 0x%x, start_addr: 0x%x, end_addr: 0x%x\n",
		ctrl->irp,
		ctrl->start_addr,
		ctrl->end_addr);

	while (1) {
		if (!ctrl->initialized)
			break;
		if (test_bit(GLUE_FLAG_HALT_BIT, &ad->prGlueInfo->ulFlag) ||
		    kalIsResetting()) {
			DBGLOG(MET, INFO,
				"wifi off, stop Met log.\n");
			break;
		}

		met_log_emi_refresh_header(ad, ctrl);
		if (met_log_emi_is_empty(ctrl)) {
			DBGLOG(MET, TRACE, "EMI is empty, no more MET log!\n");
			msleep(MET_LOG_STATS_UPDATE_PERIOD);
			continue;
		}

		rp = 0; recv = 0; handled = 0;
		if (ctrl->wp > ctrl->irp)
			recv = ctrl->wp - ctrl->irp;
		else
			recv = ctrl->emi_size - ctrl->irp + ctrl->wp;
		if (recv > ctrl->emi_size) {
			DBGLOG(MET, ERROR,
				"Invalid recv emi_size (%u %u)\n",
				recv, ctrl->emi_size);
			msleep(MET_LOG_STATS_UPDATE_PERIOD);
			continue;
		}
		if (recv % 8 != 0) {
			DBGLOG(MET, INFO,
				"align 8, change recv size from %d to %d\n",
				recv, recv - (recv % 8));
			recv -= (recv % 8);
		}

		rp = ctrl->irp;

		while (recv) {
			uint32_t size = 0;

			if (rp + recv > ctrl->end_addr)
				size = ctrl->end_addr - rp;
			else
				size = recv;

			DBGLOG(MET, INFO,
				"Read data from: 0x%x, size: 0x%x\n",
				rp,
				size);

			if (emi_mem_read(ad->chip_info,
				     rp,
				     ctrl->buffer + handled,
				     size)) {
				DBGLOG(MET, ERROR,
					"Read EMI fail!\n");
				continue;
			}

			handled += size;
			rp += size;
			if (rp >= ctrl->end_addr)
				rp = (rp % ctrl->end_addr) + ctrl->start_addr;

			recv -= size;
		}

		ctrl->irp += handled;
		if (ctrl->irp >= ctrl->end_addr)
			ctrl->irp = (ctrl->irp % ctrl->end_addr)
					+ ctrl->start_addr;

		met_log_print_data(ctrl->buffer,
				handled,
				ctrl->project_id,
				ad->chip_info->chip_id);
		msleep(MET_LOG_STATS_UPDATE_PERIOD);
	}

	DBGLOG(MET, TRACE, "\n");
}

static void met_log_emi_work(struct work_struct *work)
{
	met_log_emi_handler();
}

uint32_t met_log_emi_init(struct ADAPTER *ad)
{
	struct MET_LOG_EMI_CTRL *ctrl = &g_met_log_emi_ctx;
	uint32_t u4EmiMetOffset = 0;
	uint32_t u4ProjectId = 0;
	uint32_t status = WLAN_STATUS_SUCCESS;

	if (ctrl->initialized) {
		DBGLOG(MET, WARN,
			"Met log already init!\n");
		return status;
	}

	DBGLOG(INIT, TRACE, "\n");

	u4EmiMetOffset = emi_mem_offset_convert(kalGetEmiMetOffset());
	u4ProjectId = kalGetProjectId();
	kalMemZero(ctrl, sizeof(*ctrl));
	ctrl->priv = ad;
	ctrl->project_id = u4ProjectId;
	ctrl->offset = u4EmiMetOffset;
	ctrl->emi_size = MET_LOG_EMI_SIZE - MET_LOG_EMI_RESERVED;

	DBGLOG(MET, INFO,
		"offset: 0x%x, emi_size: 0x%x\n",
		ctrl->offset,
		ctrl->emi_size);

	ctrl->buffer = kalMemAlloc(ctrl->emi_size, VIR_MEM_TYPE);
	if (!ctrl->buffer) {
		DBGLOG(MET, ERROR,
			"Alloc buffer failed.\n");
		status = WLAN_STATUS_RESOURCES;
		goto exit;
	} else {
		kalMemZero(ctrl->buffer, ctrl->emi_size);
	}

	ctrl->wq = create_singlethread_workqueue("met_log_emi");
	if (!ctrl->wq) {
		DBGLOG(MET, ERROR,
			"create_singlethread_workqueue failed.\n");
		status = WLAN_STATUS_RESOURCES;
		goto exit;
	}
	ctrl->initialized = TRUE;
	INIT_WORK(&ctrl->work, met_log_emi_work);
	queue_work(ctrl->wq, &ctrl->work);

exit:
	if (status != WLAN_STATUS_SUCCESS) {
		DBGLOG(MET, ERROR,
			"status: 0x%x\n",
			status);
		met_log_emi_deinit(ad);
	}

	return status;
}

uint32_t met_log_emi_deinit(struct ADAPTER *ad)
{
	struct MET_LOG_EMI_CTRL *ctrl = &g_met_log_emi_ctx;
	uint32_t status = WLAN_STATUS_SUCCESS;

	DBGLOG(INIT, TRACE, "\n");

	if (!ctrl->initialized) {
		DBGLOG(MET, WARN,
			"Met log already deinit!\n");
		return status;
	}

	ctrl->initialized = FALSE;
	cancel_work_sync(&ctrl->work);
	if (ctrl->wq)
		destroy_workqueue(ctrl->wq);
	if (ctrl->buffer)
		kalMemFree(ctrl->buffer, VIR_MEM_TYPE, ctrl->emi_size);
	ctrl->priv = NULL;

	return status;
}
#endif /* CFG_SUPPORT_MET_LOG */
