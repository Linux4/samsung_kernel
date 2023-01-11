/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */

#include "tzdd_internal.h"
#include "tzdd_pt_core.h"

static uint32_t _check_message_flag(tee_msg_head_t tee_msg_head)
{
	/*
	 * TEE_MSG_FAKE: (the fake message for SSTd)
	 * 1. tzdd_send_num --
	 * 2. read the msg from RB, then discard it
	 * TEE_MSG_IGNORE_COUNTER: (the real message for SSTd)
	 * 1. handle the msg as normal
	 * 2. did NOT modify the tzdd_send_num
	 */
	if ((tee_msg_head.msg_prop.type == TEE_MSG_TYPE_CONTROL) &&
		(tee_msg_head.msg_prop.ctl.cmd == TEE_MSG_CTL_TRAPPED)) {
		return TEE_MSG_FAKE;
	}

	if (tee_msg_head.msg_prop.bp.has_trapped == true)
		return TEE_MSG_IGNORE_COUNTER;

	return TEE_MSG_NORMAL;
}

void tzdd_pt_send(tee_msg_head_t *tee_msg_send_head)
{
	tee_msgm_t tee_msgm_handle;
	tee_cmd_t tee_msg_cmd;

	/* deal with cancellation */
	if (tee_msg_send_head->msg_prop.bp.stat == TEE_MSG_STAT_CAN) {
		void *arg;

		tee_msgm_handle = tee_msgm_create_inst();
		tee_msgm_set_msg_buf(tee_msgm_handle, NULL,
					(uint8_t *) tee_msg_send_head +
					sizeof(tee_msg_head_t), NULL);
		tee_msgm_get_cmd(tee_msgm_handle, &tee_msg_cmd, NULL);

		if (tee_msg_cmd == TEE_CMD_OPEN_SS) {
			tee_set_ret_open_ss_arg_t tee_set_ret_open_ss_arg;
			tee_set_ret_open_ss_arg.ss_tw = NULL;
			tee_set_ret_open_ss_arg.ret_orig = TEEC_ORIGIN_API;
			tee_set_ret_open_ss_arg.ret = TEEC_ERROR_CANCEL;

			arg = &tee_set_ret_open_ss_arg;
		} else if (tee_msg_cmd == TEE_CMD_INV_OP) {
			tee_set_ret_inv_op_arg_t tee_set_ret_inv_op_arg;
			tee_set_ret_inv_op_arg.ret_orig = TEEC_ORIGIN_API;
			tee_set_ret_inv_op_arg.ret = TEEC_ERROR_CANCEL;

			arg = &tee_set_ret_inv_op_arg;
		} else {
			OSA_ASSERT(0);
		}

		tee_msgm_set_ret(tee_msgm_handle, arg);

		tee_msgm_destroy_inst(tee_msgm_handle);

		osa_release_sem(tee_msg_send_head->comp);
	}
	/* Call ComM to copy the buffer into RC */
	tee_cm_send_data((uint8_t *) tee_msg_send_head);

	return;
}

uint32_t tzdd_pt_recv(void)
{
	tee_msg_head_t *tee_msg_recv_head;
	tee_msgm_t tee_msgm_handle;
	tee_cmd_t tee_msg_cmd;
	tee_msg_head_t tee_msg_head;
	uint32_t msg_flag;

	tee_cm_get_msgm_head(&tee_msg_head);
	msg_flag = _check_message_flag(tee_msg_head);

	if (msg_flag != TEE_MSG_FAKE) {
		#if 0
		TZDD_DBG
			("%s: tee_msg_head =\
			{magic (%c%c%c%c) ,\
			size (0x%08x)},\
			cmd (0x%x),\
			msg_flag (0x%x)\n",\
			__func__, tee_msg_head.magic[0], tee_msg_head.magic[1],
			tee_msg_head.magic[2], tee_msg_head.magic[3],
			tee_msg_head.msg_sz,
			*((uint32_t *) (tee_msg_head.tee_msg) + 8), msg_flag);
		#endif /* 0 */

		tee_msg_recv_head = tee_msg_head.tee_msg;
		tee_cm_recv_data((uint8_t *)tee_msg_recv_head);

		tee_msgm_handle = tee_msgm_create_inst();
		tee_msgm_set_msg_buf(tee_msgm_handle, NULL,
					(uint8_t *)tee_msg_recv_head +
					sizeof(tee_msg_head_t), NULL);
		tee_msgm_get_cmd(tee_msgm_handle, &tee_msg_cmd, NULL);
		if (tee_msg_cmd == TEE_CMD_CAN_OP) {
			osa_destroy_sem(tee_msg_recv_head->comp);
			osa_kmem_free(tee_msg_recv_head);
		} else {

			osa_release_sem(tee_msg_recv_head->comp);
		}
		tee_msgm_destroy_inst(tee_msgm_handle);
	} else if (msg_flag == TEE_MSG_FAKE) {
		/* delete fake msg from comm channel */
		tee_msg_head_t tee_msg_head_for_fake_msg;
		tee_cm_recv_data((uint8_t *)&tee_msg_head_for_fake_msg);
	}
	return msg_flag;
}

