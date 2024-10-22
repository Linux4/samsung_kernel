// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "ccci_fsm_internal.h"
#include "modem_sys.h"
#include "md_sys1_platform.h"
#include <soc/mediatek/emi.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
#include <linux/soc/mediatek/devapc_public.h>
#endif
#if IS_ENABLED(CONFIG_MTK_EMI)
#include <soc/mediatek/smpu.h>
#endif

static int s_is_normal_mdee;

void mdee_set_ex_start_str(struct ccci_fsm_ee *ee_ctl,
	const unsigned int type, const char *str)
{
	u64 ts_nsec;
	unsigned long rem_nsec;
	int ret = 0;

	if (type == MD_FORCE_ASSERT_BY_AP_MPU) {
		ret = scnprintf(ee_ctl->ex_mpu_string, MD_EX_MPU_STR_LEN,
			"EMI MPU VIOLATION: %s", str);
		if (ret <= 0 || ret >= MD_EX_MPU_STR_LEN) {
			CCCI_ERROR_LOG(0, FSM,
				"%s:snprintf ee_ctl->ex_mpu_string fail\n",
				__func__);
			ee_ctl->ex_mpu_string[0] = 0;
		}
	}
	ts_nsec = local_clock();
	rem_nsec = do_div(ts_nsec, 1000000000);
	scnprintf(ee_ctl->ex_start_time, MD_EX_START_TIME_LEN,
		"AP detect MDEE time:%5lu.%06lu\n",
		(unsigned long)ts_nsec, rem_nsec / 1000);
	CCCI_MEM_LOG_TAG(0, FSM, "%s\n",
		ee_ctl->ex_start_time);
}

void mdee_set_ex_time_str(unsigned int type, char *str)
{
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;

	if (ctl == NULL) {
		CCCI_ERROR_LOG(0, FSM,
			"%s:ccci_fsm_entries is null\n", __func__);
		return;
	}
	mdee_set_ex_start_str(&ctl->ee_ctl, type, str);
}

void fsm_md_bootup_timeout_handler(struct ccci_fsm_ee *ee_ctl)
{
	struct ccci_mem_layout *mem_layout
		= ccci_md_get_mem();

	CCCI_NORMAL_LOG(0, FSM,
		"Dump MD layout struct\n");
	ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, mem_layout,
		sizeof(struct ccci_mem_layout));
	CCCI_NORMAL_LOG(0, FSM,
		"Dump queue 0 & 1\n");
	ccci_md_dump_info(
		(DUMP_FLAG_QUEUE_0_1 | DUMP_MD_BOOTUP_STATUS
		| DUMP_FLAG_REG | DUMP_FLAG_CCIF_REG), NULL, 0);
	CCCI_NORMAL_LOG(0, FSM,
		"Dump MD ee boot failed info\n");

	ee_ctl->ops->dump_ee_info(ee_ctl, MDEE_DUMP_LEVEL_BOOT_FAIL, 0);
}

void fsm_md_exception_stage(struct ccci_fsm_ee *ee_ctl, int stage)
{
	unsigned long flags;

	if (stage == 0) { /* CCIF handshake just came in */
		mdee_set_ex_start_str(ee_ctl, 0, NULL);
		ee_ctl->mdlog_dump_done = 0;
		ee_ctl->ee_info_flag = 0;
		spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
		ee_ctl->ee_info_flag |= (MD_EE_FLOW_START | MD_EE_SWINT_GET);
		if (ccci_fsm_get_md_state()
				== BOOT_WAITING_FOR_HS1)
			ee_ctl->ee_info_flag |= MD_EE_DUMP_IN_GPD;
		spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
	} else if (stage == 1) { /* got MD_EX_REC_OK or first timeout */
		int ee_case;
		unsigned int ee_info_flag = 0;
		unsigned int md_dump_flag = 0;
		struct ccci_mem_layout *mem_layout = ccci_md_get_mem();
		struct ccci_modem *md = ccci_get_modem();

		CCCI_ERROR_LOG(0, FSM, "MD exception stage 1!\n");
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		tracing_off();
#endif
		CCCI_MEM_LOG_TAG(0, FSM,
			"MD exception stage 1! ee=%x\n",
			ee_ctl->ee_info_flag);
		spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
		ee_info_flag = ee_ctl->ee_info_flag;
		spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);

		if ((ee_info_flag & (MD_EE_SWINT_GET
			| MD_EE_MSG_GET | MD_EE_OK_MSG_GET)) ==
		    (MD_EE_SWINT_GET | MD_EE_MSG_GET
			| MD_EE_OK_MSG_GET)) {
			ee_case = MD_EE_CASE_NORMAL;
			CCCI_DEBUG_LOG(0, FSM,
				"Recv SWINT & MD_EX & MD_EX_REC_OK\n");
		} else if (ee_info_flag & MD_EE_MSG_GET) {
			ee_case = MD_EE_CASE_ONLY_EX;
			CCCI_NORMAL_LOG(0, FSM, "Only recv MD_EX.\n");
		} else if (ee_info_flag & MD_EE_SWINT_GET) {
			ee_case = MD_EE_CASE_ONLY_SWINT;
			CCCI_NORMAL_LOG(0, FSM, "Only recv SWINT.\n");
		} else if (ee_info_flag & MD_EE_PENDING_TOO_LONG) {
			ee_case = MD_EE_CASE_NO_RESPONSE;
		} else if (ee_info_flag & MD_EE_WDT_GET) {
			ee_case = MD_EE_CASE_WDT;
			md->per_md_data.md_dbg_dump_flag |=
			(1 << MD_DBG_DUMP_TOPSM) | (1 << MD_DBG_DUMP_MDRGU)
			| (1 << MD_DBG_DUMP_OST);
		} else {
			CCCI_ERROR_LOG(0, FSM,
				"Invalid MD_EX, ee_info=%x\n", ee_info_flag);
			goto _dump_done;
		}
		ee_ctl->ee_case = ee_case;

		/* Dump MD EE info */
		CCCI_MEM_LOG_TAG(0, FSM, "Dump MD EX log\n");
		/*parse & dump md ee info*/
		if (ee_ctl->ops->dump_ee_info)
			ee_ctl->ops->dump_ee_info(ee_ctl,
				MDEE_DUMP_LEVEL_STAGE1, ee_case);

		/* Dump MD register*/
		md_dump_flag = DUMP_FLAG_REG | DUMP_FLAG_MD_WDT;

		if (ee_case == MD_EE_CASE_ONLY_SWINT)
			md_dump_flag |= (DUMP_FLAG_QUEUE_0 | DUMP_FLAG_CCIF
					| DUMP_FLAG_CCIF_REG);
		ccci_md_dump_info(md_dump_flag, NULL, 0);

		/* check this first, as we overwrite share memory here */
		if (ee_case == MD_EE_CASE_NO_RESPONSE)
			ccci_md_dump_info(DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG,
					NULL, 0);

		/* Dump MD memory layout */
		CCCI_MEM_LOG_TAG(0, FSM, "Dump MD layout struct\n");
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, mem_layout,
			sizeof(struct ccci_mem_layout));
		/* Dump CCB memory */
		ccci_md_dump_info(
			DUMP_FLAG_SMEM_CCB_CTRL | DUMP_FLAG_SMEM_CCB_DATA,
			NULL, 0);

		CCCI_ERROR_LOG(0, FSM, "MD exception stage 1: end\n");
_dump_done:
		return;
	} else if (stage == 2) { /* got MD_EX_PASS or second timeout */
		unsigned long flags;
		unsigned int md_dump_flag = 0;

#ifdef CUST_FT_EE_TRIGGER_REBOOT
		struct ccci_modem *md;
#endif
		CCCI_ERROR_LOG(0, FSM, "MD exception stage 2!\n");
		CCCI_MEM_LOG_TAG(0, FSM, "MD exception stage 2! ee=%x\n",
			ee_ctl->ee_info_flag);

		md_dump_flag = DUMP_FLAG_REG | DUMP_FLAG_MD_WDT;
		if (ee_ctl->ee_case == MD_EE_CASE_ONLY_SWINT)
			md_dump_flag |= (DUMP_FLAG_QUEUE_0
			| DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG);
		ccci_md_dump_info(md_dump_flag, NULL, 0);
		/* check this first, as we overwrite share memory here */
		if (ee_ctl->ee_case == MD_EE_CASE_NO_RESPONSE)
			ccci_md_dump_info(
				DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG, NULL, 0);

		/*parse & dump md ee info*/
		if (ee_ctl->ops->dump_ee_info)
			ee_ctl->ops->dump_ee_info(ee_ctl,
				MDEE_DUMP_LEVEL_STAGE2, ee_ctl->ee_case);

		spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
		/* this flag should be the last action of
		 * a regular exception flow, clear flag
		 * for reset MD later
		 */
		ee_ctl->ee_info_flag = 0;
		spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);

		CCCI_ERROR_LOG(0, FSM,
			"MD exception stage 2:end\n");
#ifdef CUST_FT_EE_TRIGGER_REBOOT
		md = ccci_get_modem();
		if (md == NULL) {
			CCCI_ERROR_LOG(0, FSM,
				"fail to get md struct-no any userful MD!!\n");
			return;
		}
		if (md->ccci_drv_trigger_upload) {
			CCCI_ERROR_LOG(0, FSM, "[md1] drv trigger panic\n");
			drv_tri_panic_by_lvl();
			return;
		}
		if (!ee_ctl->is_normal_ee_case) {
			CCCI_ERROR_LOG(0, FSM,
				"[md1] EE time out case, call panic\n");
			drv_tri_panic_by_lvl();
		}
		ee_ctl->is_normal_ee_case = 0;
#endif
	}
}

void fsm_md_wdt_handler(struct ccci_fsm_ee *ee_ctl)
{
	unsigned long flags;

	spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
	ee_ctl->ee_info_flag |= (MD_EE_FLOW_START | MD_EE_WDT_GET);
	spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
	fsm_md_exception_stage(ee_ctl, 1);
	msleep(MD_EX_PASS_TIMEOUT);
	fsm_md_exception_stage(ee_ctl, 2);
}

void fsm_md_no_response_handler(struct ccci_fsm_ee *ee_ctl)
{
	unsigned long flags;

	spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
	ee_ctl->ee_info_flag |= (MD_EE_FLOW_START | MD_EE_PENDING_TOO_LONG);
	spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
	fsm_md_exception_stage(ee_ctl, 1);
	msleep(MD_EX_PASS_TIMEOUT);
	fsm_md_exception_stage(ee_ctl, 2);
}

void fsm_ee_message_handler(struct ccci_fsm_ee *ee_ctl, struct sk_buff *skb)
{
	struct ccci_fsm_ctl *ctl
		= container_of(ee_ctl, struct ccci_fsm_ctl, ee_ctl);
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	unsigned long flags;
	enum MD_STATE md_state = ccci_fsm_get_md_state();

	if (md_state != EXCEPTION) {
		CCCI_ERROR_LOG(0, FSM,
			"receive invalid MD_EX %x when MD state is %d\n",
			ccci_h->reserved, md_state);
		return;
	}
	if (ccci_h->data[1] == MD_EX) {
		if (unlikely(ccci_h->reserved != MD_EX_CHK_ID)) {
			CCCI_ERROR_LOG(0, FSM,
				"receive invalid MD_EX %x\n",
				ccci_h->reserved);
		} else {
			spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
			ee_ctl->ee_info_flag
				|= (MD_EE_FLOW_START | MD_EE_MSG_GET);
			spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
			ccci_port_send_msg_to_md(
			CCCI_CONTROL_TX, MD_EX, MD_EX_CHK_ID, 1);
			fsm_append_event(ctl, CCCI_EVENT_MD_EX, NULL, 0);
		}
	} else if (ccci_h->data[1] == MD_EX_REC_OK) {
		if (unlikely(ccci_h->reserved != MD_EX_REC_OK_CHK_ID)) {
			CCCI_ERROR_LOG(0, FSM,
				"receive invalid MD_EX_REC_OK %x\n",
				ccci_h->reserved);
		} else {
			spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
			ee_ctl->ee_info_flag
				|= (MD_EE_FLOW_START | MD_EE_OK_MSG_GET);
			spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
			/* Keep exception info package from MD*/
			if (ee_ctl->ops->set_ee_pkg)
				ee_ctl->ops->set_ee_pkg(ee_ctl,
				skb_pull(skb, sizeof(struct ccci_header)),
				skb->len - sizeof(struct ccci_header));

			fsm_append_event(ctl,
				CCCI_EVENT_MD_EX_REC_OK, NULL, 0);
		}
	} else if (ccci_h->data[1] == MD_EX_PASS) {
		spin_lock_irqsave(&ee_ctl->ctrl_lock, flags);
		ee_ctl->ee_info_flag |= MD_EE_PASS_MSG_GET;
#ifdef CUST_FT_EE_TRIGGER_REBOOT
		/* dump share memory again to let MD check exception flow */
		ee_ctl->is_normal_ee_case = 1;
#endif
		spin_unlock_irqrestore(&ee_ctl->ctrl_lock, flags);
		fsm_append_event(ctl, CCCI_EVENT_MD_EX_PASS, NULL, 0);
	} else if (ccci_h->data[1] == CCCI_DRV_VER_ERROR) {
		CCCI_ERROR_LOG(0, FSM,
			"AP/MD driver version mis-match\n");
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aed_md_exception_api(NULL, 0, NULL,
			0, "AP/MD driver version mis-match\n",
			DB_OPT_DEFAULT);
#endif
	}
}

static void md_cd_exception(struct ccci_modem *md, enum HIF_EX_STAGE stage)
{
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDCCCI_DBG);

	CCCI_ERROR_LOG(0, FSM, "MD exception HIF %d\n", stage);
	ccci_event_log("md:MD exception HIF %d\n", stage);
	/* in exception mode, MD won't sleep, so we do not
	 * need to request MD resource first
	 */
	switch (stage) {
	case HIF_EX_INIT:
		if ((mdccci_dbg != NULL) && (*((int *)(mdccci_dbg->base_ap_view_vir +
			CCCI_SMEM_OFFSET_SEQERR)) != 0)) {
			CCCI_ERROR_LOG(0, FSM, "MD found wrong sequence number\n");
		}
		ccci_hif_md_exception(md->hif_flag, stage);
		/* Rx dispatch does NOT depend on queue index
		 * in port structure, so it still can find right port.
		 */
		ccci_hif_send_data(CCIF_HIF_ID, H2D_EXCEPTION_ACK);
		break;
	case HIF_EX_INIT_DONE:
		break;
	case HIF_EX_CLEARQ_DONE:
		ccci_hif_md_exception(md->hif_flag, stage);
		/* tell MD to reset CLDMA */
		ccci_hif_send_data(CCIF_HIF_ID, H2D_EXCEPTION_CLEARQ_ACK);
		CCCI_NORMAL_LOG(0, FSM, "send clearq_ack to MD\n");
		break;
	case HIF_EX_ALLQ_RESET:
		md->per_md_data.is_in_ee_dump = 1;
		ccci_hif_md_exception(md->hif_flag, stage);
		break;
	default:
		break;
	};
}

static void polling_ready(struct ccci_modem *md, int step)
{
	int cnt = 500; /*MD timeout is 10s*/
	int time_once = 10;
	struct md_sys1_info *md_info =
		(struct md_sys1_info *)md->private_data;

#ifdef CCCI_EE_HS_POLLING_TIME
	cnt = CCCI_EE_HS_POLLING_TIME / time_once;
#endif
	while (cnt > 0) {
		if (md_info->channel_id & (1 << step)) {
			CCCI_DEBUG_LOG(0, FSM,
				"poll RCHNUM %d\n", md_info->channel_id);
			return;
		}
		msleep(time_once);
		cnt--;
	}
	CCCI_ERROR_LOG(0, FSM,
		"poll EE HS timeout, RCHNUM %d\n", md_info->channel_id);
}

static int md_cd_ee_handshake(struct ccci_modem *md, int timeout)
{
	/* seems sometime MD send D2H_EXCEPTION_INIT_DONE and
	 * D2H_EXCEPTION_CLEARQ_DONE together
	 */
	/*polling_ready(md_ctrl, D2H_EXCEPTION_INIT);*/
	md_cd_exception(md, HIF_EX_INIT);
	polling_ready(md, D2H_EXCEPTION_INIT_DONE);
	md_cd_exception(md, HIF_EX_INIT_DONE);

	polling_ready(md, D2H_EXCEPTION_CLEARQ_DONE);
	md_cd_exception(md, HIF_EX_CLEARQ_DONE);

	polling_ready(md, D2H_EXCEPTION_ALLQ_RESET);
	md_cd_exception(md, HIF_EX_ALLQ_RESET);

	return 0;
}

static void ccci_md_exception_handshake(int timeout)
{
	struct ccci_modem *md = ccci_get_modem();

	md_cd_ee_handshake(md, timeout);
}

void fsm_md_normal_ee_handler(struct ccci_fsm_ctl *ctl)
{
	struct ccci_fsm_event *event = NULL;
	int count = 0, ex_got = 0;
	int rec_ok_got = 0, pass_got = 0;
	unsigned long flags;

	s_is_normal_mdee = 1;
	/* no need to implement another
	 * event polling in EE_CTRL,
	 * so we do it here
	 */
	ccci_md_exception_handshake(MD_EX_CCIF_TIMEOUT);
#if IS_ENABLED(CONFIG_MTK_EMI)
	CCCI_NORMAL_LOG(0, FSM, "normal_ee_handler mtk_clear_md_violation\n");
	mtk_clear_md_violation();
	smpu_clear_md_violation();
#endif
	count = 0;
	while (count < MD_EX_REC_OK_TIMEOUT/EVENT_POLL_INTEVAL) {
		spin_lock_irqsave(&ctl->event_lock, flags);
		if (!list_empty(&ctl->event_queue)) {
			event = list_first_entry(&ctl->event_queue,
				struct ccci_fsm_event, entry);
			if (event->event_id == CCCI_EVENT_MD_EX) {
				ex_got = 1;
				fsm_finish_event(ctl, event);
			} else if (event->event_id ==
					CCCI_EVENT_MD_EX_REC_OK) {
				rec_ok_got = 1;
				fsm_finish_event(ctl, event);
			}
		}
		spin_unlock_irqrestore(&ctl->event_lock, flags);
		if (rec_ok_got)
			break;
		count++;
		msleep(EVENT_POLL_INTEVAL);
	}
	fsm_md_exception_stage(&ctl->ee_ctl, 1);
	count = 0;
	while (count < MD_EX_PASS_TIMEOUT/EVENT_POLL_INTEVAL) {
		spin_lock_irqsave(&ctl->event_lock, flags);
		if (!list_empty(&ctl->event_queue)) {
			event = list_first_entry(&ctl->event_queue,
				struct ccci_fsm_event, entry);
			if (event->event_id == CCCI_EVENT_MD_EX_PASS) {
				pass_got = 1;
				fsm_finish_event(ctl, event);
			}
		}
		spin_unlock_irqrestore(&ctl->event_lock, flags);
		if (pass_got)
			break;
		count++;
		msleep(EVENT_POLL_INTEVAL);
	}
	fsm_md_exception_stage(&ctl->ee_ctl, 2);

}

int fsm_check_ee_done(struct ccci_fsm_ee *ee_ctl, int timeout)
{
	int count = 0;
	bool is_ee_done = 0;
	int time_step = 200; /*ms*/
	int loop_max = timeout * 1000 / time_step;

	CCCI_BOOTUP_LOG(0, FSM, "checking EE status\n");
	while (ccci_fsm_get_md_state() == EXCEPTION) {
		if (ccci_port_get_critical_user(CRIT_USR_MDLOG)) {
			CCCI_DEBUG_LOG(0, FSM,
				"MD logger is running, waiting for EE dump done\n");
			is_ee_done = !(ee_ctl->ee_info_flag & MD_EE_FLOW_START)
				&& ee_ctl->mdlog_dump_done;
		} else
			is_ee_done = !(ee_ctl->ee_info_flag & MD_EE_FLOW_START);
		if (!is_ee_done) {
			msleep(time_step);
			count++;
		} else
			break;

		if (loop_max && (count > loop_max)) {
			CCCI_ERROR_LOG(0, FSM, "wait EE done timeout\n");
#ifdef DEBUG_FOR_CCB
			/* Dump CCB memory */
			ccci_port_dump_status();
			ccci_md_dump_info(DUMP_FLAG_CCIF |
				DUMP_FLAG_CCIF_REG | DUMP_FLAG_IRQ_STATUS |
				DUMP_FLAG_SMEM_CCB_CTRL |
				DUMP_FLAG_SMEM_CCB_DATA,
				NULL, 0);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
			/*
			 * aee_kernel_warning("ccci_EE_timeout",
			 *	"MD EE debug: wait dump done timeout");
			 */
#endif
#endif
			return -1;
		}
	}
	CCCI_BOOTUP_LOG(0, FSM, "check EE done\n");
	return 0;
}

int md_fsm_exp_info(unsigned int channel_id)
{
	struct ccci_modem *md;
	struct md_sys1_info *md_info;

	md = ccci_get_modem();
	if (!md)
		return 0;

	md_info = (struct md_sys1_info *)md->private_data;
	if (channel_id & (1 << D2H_EXCEPTION_INIT)) {
		ccci_fsm_recv_md_interrupt(MD_IRQ_CCIF_EX);
		md_info->channel_id = channel_id & ~(1 << D2H_EXCEPTION_INIT);
		return 0;
	}

	if (channel_id & (1<<AP_MD_PEER_WAKEUP)) {
		__pm_wakeup_event(md_info->peer_wake_lock,
			jiffies_to_msecs(HZ));
		channel_id &= ~(1 << AP_MD_PEER_WAKEUP);
	}

	if (channel_id & (1<<AP_MD_SEQ_ERROR)) {
		CCCI_ERROR_LOG(0, FSM, "MD check seq fail\n");
		md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
		channel_id &= ~(1 << AP_MD_SEQ_ERROR);
	}

	md_info->channel_id |= channel_id;
	return 0;
}
EXPORT_SYMBOL(md_fsm_exp_info);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
atomic_t pw_off_disable_dapc_ke;
atomic_t md_dapc_ke_occurred;
atomic_t en_flight_timeout; /* enter flight mode timeout && devapc occurred */

static int s_devapc_dump_counter;

int ccci_fsm_increase_devapc_dump_counter(void)
{
	return (++ s_devapc_dump_counter);
}

void dump_md_info_in_devapc(struct ccci_modem *md)
{
	unsigned char ccif_sram[CCCI_EE_SIZE_CCIF_SRAM] = { 0 };
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDCCCI_DBG);
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);

	// DUMP_FLAG_CCIF_REG
	CCCI_MEM_LOG_TAG(0, FSM, "Dump CCIF REG\n");
	ccci_hif_dump_status(CCIF_HIF_ID, DUMP_FLAG_CCIF_REG, NULL, -1);

	// DUMP_FLAG_CCIF
	ccci_hif_dump_status(1 << CCIF_HIF_ID, DUMP_FLAG_CCIF, ccif_sram,
			sizeof(ccif_sram));

	// DUMP_FLAG_QUEUE_0_1
	ccci_hif_dump_status(md->hif_flag, DUMP_FLAG_QUEUE_0_1, NULL, 0);

	// DUMP_FLAG_REG
	if (md->hw_info->plat_ptr->debug_reg)
		md->hw_info->plat_ptr->debug_reg(md, false);

	// DUMP_MD_BOOTUP_STATUS
	if (md->hw_info->plat_ptr->get_md_bootup_status)
		md->hw_info->plat_ptr->get_md_bootup_status(NULL, 0);

	// MD_DBG_DUMP_SMEM
	CCCI_MEM_LOG_TAG(0, FSM, "Dump MD EX log\n");
	if (mdccci_dbg != NULL)
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, mdccci_dbg->base_ap_view_vir,
				mdccci_dbg->size);
	else
		CCCI_MEM_LOG_TAG(0, FSM, "Error: %s mdccci_dbg is NULL\n", __func__);
	CCCI_MEM_LOG_TAG(0, FSM, "Dump mdss_dbg log\n");
	if (mdss_dbg != NULL)
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, mdss_dbg->base_ap_view_vir,
				mdss_dbg->size);
	else
		CCCI_MEM_LOG_TAG(0, FSM, "Error: %s mdss_dbg is NULL\n", __func__);
	CCCI_MEM_LOG_TAG(0, FSM, "Dump mdl2sram log\n");
	if (md->hw_info->md_l2sram_base) {
		md_cd_lock_modem_clock_src(1);
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, md->hw_info->md_l2sram_base,
			md->hw_info->md_l2sram_size);
		md_cd_lock_modem_clock_src(0);
	}
}

void ccci_dump_md_in_devapc(char *user_info)
{
	struct ccci_modem *md = NULL;

	CCCI_NORMAL_LOG(0, FSM, "%s called by %s\n", __func__, user_info);
	md = ccci_get_modem();
	if (md != NULL) {
		CCCI_NORMAL_LOG(0, FSM, "%s dump start\n", __func__);
		dump_md_info_in_devapc(md);
	} else
		CCCI_NORMAL_LOG(0, FSM, "%s error, md is NULL!\n", __func__);
	CCCI_NORMAL_LOG(0, FSM, "%s dump finished\n", __func__);
}

static enum devapc_cb_status devapc_dump_adv_cb(uint32_t vio_addr)
{
	int count;

	CCCI_NORMAL_LOG(0, FSM,
		"[%s] vio_addr: 0x%x; is normal mdee: %d\n",
		__func__, vio_addr, s_is_normal_mdee);

	if (ccci_fsm_get_md_state() == EXCEPTION && s_is_normal_mdee) {
		count = ccci_fsm_increase_devapc_dump_counter();

		CCCI_NORMAL_LOG(0, FSM,
			"[%s] count: %d\n", __func__, count);

		if (count == 1)
			ccci_dump_md_in_devapc((char *)__func__);

		return DEVAPC_NOT_KE;

	} else {
		atomic_set(&md_dapc_ke_occurred, 1);
		ccci_dump_md_in_devapc((char *)__func__);

		/*
		 * debug patch
		 * during stop modem, if devapc occurred, don't trigger KE
		 */
		if (atomic_read(&pw_off_disable_dapc_ke))
			return DEVAPC_NOT_KE;
		else
			return DEVAPC_OK;
	}
}

static struct devapc_vio_callbacks devapc_md_vio_handle = {
	.id = INFRA_SUBSYS_MD,
	.debug_dump_adv = devapc_dump_adv_cb,
};
#endif

void fsm_ee_cmd_init(enum CCCI_FSM_COMMAND cmd_id)
{
	s_is_normal_mdee = 0;
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	s_devapc_dump_counter = 0;
#endif

	switch (cmd_id) {
	case CCCI_COMMAND_START:
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
		atomic_set(&pw_off_disable_dapc_ke, 0);
		atomic_set(&md_dapc_ke_occurred, 0);
#endif
		atomic_set(&en_flight_timeout, 0);
		break;
	case CCCI_COMMAND_STOP:
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
		atomic_set(&pw_off_disable_dapc_ke, 1);
#endif
		break;
	default:
		break;
	}

}

void fsm_ee_cmd_deinit(enum CCCI_FSM_COMMAND cmd_id)
{
	switch (cmd_id) {
	case CCCI_COMMAND_STOP:
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
		if (atomic_read(&md_dapc_ke_occurred) && atomic_read(&en_flight_timeout)) {
			CCCI_ERROR_LOG(0, FSM,
				"md_dapc_ke_occurred and en_flight_timeout, bug_on\n");
			BUG_ON(1);
		}
#endif
		break;
	default:
		break;
	}
}

int fsm_ee_init(struct ccci_fsm_ee *ee_ctl)
{
	int ret = 0;
	struct ccci_modem *md = ccci_get_modem();

	spin_lock_init(&ee_ctl->ctrl_lock);

	if (md->hw_info->plat_val->md_gen  >= 6299)
		ret = mdee_dumper_v6_alloc(ee_ctl);
	else if (md->hw_info->plat_val->md_gen  >= 6297)
		ret = mdee_dumper_v5_alloc(ee_ctl);
	else if (md->hw_info->plat_val->md_gen  >= 6292)
		ret = mdee_dumper_v3_alloc(ee_ctl);
	else if (md->hw_info->plat_val->md_gen  == 6291)
		ret = mdee_dumper_v2_alloc(ee_ctl);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_md_vio_handle);
#endif

	return ret;
}


