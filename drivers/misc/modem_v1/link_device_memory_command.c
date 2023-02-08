/**
@file		link_device_memory_command.c
@brief		common functions for all types of memory interface media
@date		2014/02/18
@author		Hankook Jang (hankook.jang@samsung.com)
*/

/*
 * Copyright (C) 2011 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

#ifdef GROUP_MEM_LINK_COMMAND
/**
@weakgroup group_mem_link_command
@{
*/

static bool rild_ready(struct link_device *ld)
{
	struct io_device *fmt_iod;
	struct io_device *rfs_iod;
	int fmt_opened;
	int rfs_opened;

	fmt_iod = link_get_iod_with_format(ld, IPC_FMT);
	if (!fmt_iod) {
		mif_err("%s: No FMT io_device\n", ld->name);
		return false;
	}

	rfs_iod = link_get_iod_with_channel(ld, SIPC5_CH_ID_RFS_0);
	if (!rfs_iod) {
		mif_err("%s: No RFS io_device\n", ld->name);
		return false;
	}

	fmt_opened = atomic_read(&fmt_iod->opened);
	rfs_opened = atomic_read(&rfs_iod->opened);
	mif_err("%s: %s.opened=%d, %s.opened=%d\n", ld->name,
		fmt_iod->name, fmt_opened, rfs_iod->name, rfs_opened);
	if (fmt_opened > 0 && rfs_opened > 0)
		return true;

	return false;
}

static void cmd_phone_start_handler(struct mem_link_device *mld)
{
	int err;
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	struct io_device *iod;
	unsigned long flags;

	if (mc->phone_state != STATE_BOOTING) {
		mif_err("%s: Recv 0xC8 (CP_START) while %s.state = %s "\
			"(cp_boot_done = %d)\n", ld->name, mc->name,
			mc_state(mc), atomic_read(&mld->cp_boot_done));
		return;
	}

	spin_lock_irqsave(&mld->pif_init_lock, flags);

	mif_err("%s: Recv 0xC8 (CP_START)\n", ld->name);

	if (cp_online(mc))
		goto exit;

	iod = link_get_iod_with_format(ld, IPC_FMT);
	if (!iod) {
		mif_err("%s: ERR! no IPC_FMT iod\n", ld->name);
		goto exit;
	}

	err = mem_reset_ipc_link(mld);
	if (err)
		goto exit;

	if (wake_lock_active(&mld->dump_wlock))
		wake_unlock(&mld->dump_wlock);

	if (mld->finalize_cp_start)
		mld->finalize_cp_start(mld);

	if (rild_ready(ld)) {
		mif_err("%s: Send 0xC2 (INIT_END)\n", ld->name);
		send_ipc_irq(mld, cmd2int(CMD_INIT_END));
		atomic_set(&mld->cp_boot_done, 1);
	}

	iod->modem_state_changed(iod, STATE_ONLINE);

	complete_all(&ld->init_cmpl);

exit:
	spin_unlock_irqrestore(&mld->pif_init_lock, flags);
}

static void cmd_crash_reset_handler(struct mem_link_device *mld)
{
#ifdef DEBUG_MODEM_IF
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	struct utc_time t;
#endif

	if (!wake_lock_active(&mld->dump_wlock))
		wake_lock(&mld->dump_wlock);

#ifdef DEBUG_MODEM_IF
	get_utc_time(&t);
	evt_log(HMSU_FMT " CRASH_RESET: %s<-%s: ERR! CP_CRASH_RESET\n",
		t.hour, t.min, t.sec, t.us, ld->name, mc->name);
#endif

	mem_handle_cp_crash(mld, STATE_CRASH_RESET);
}

static void cmd_crash_exit_handler(struct mem_link_device *mld)
{
#ifdef DEBUG_MODEM_IF
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	struct utc_time t;
#endif

	del_timer(&mld->crash_ack_timer);

	if (!wake_lock_active(&mld->dump_wlock))
		wake_lock(&mld->dump_wlock);

#ifdef DEBUG_MODEM_IF
	get_utc_time(&t);
	if (mld->forced_cp_crash)
		evt_log(HMSU_FMT " CRASH_EXIT: %s<-%s: CP_CRASH_ACK\n",
			t.hour, t.min, t.sec, t.us, ld->name, mc->name);
	else
		evt_log(HMSU_FMT " CRASH_EXIT: %s<-%s: ERR! CP_CRASH_EXIT\n",
			t.hour, t.min, t.sec, t.us, ld->name, mc->name);

	if (!mld->forced_cp_crash)
		queue_work(system_nrt_wq, &mld->dump_work);
#endif

	mem_handle_cp_crash(mld, STATE_CRASH_EXIT);
}

/**
@brief		process a MEMORY command from CP

@param mld	the pointer to a mem_link_device instance
@param cmd	the MEMORY command from CP
*/
void mem_cmd_handler(struct mem_link_device *mld, u16 cmd)
{
	struct link_device *ld = &mld->link_dev;

	switch (cmd) {
	case CMD_PHONE_START:
		cmd_phone_start_handler(mld);
		break;

	case CMD_CRASH_RESET:
		cmd_crash_reset_handler(mld);
		break;

	case CMD_CRASH_EXIT:
		cmd_crash_exit_handler(mld);
		break;

	default:
		mif_err("%s: Unknown command 0x%04X\n", ld->name, cmd);
		break;
	}
}

/**
@}
*/
#endif

