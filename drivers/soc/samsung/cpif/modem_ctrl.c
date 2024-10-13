/*
 * Copyright (C) 2019 Samsung Electronics.
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

#include "modem_prj.h"
#include "modem_utils.h"

void modem_ctrl_set_kerneltime(struct modem_ctl *mc)
{
	struct modem_data *modem = mc->mdm_data;
	struct mem_link_device *mld = modem->mld;
	struct utc_time t;

	get_utc_time(&t);
	mif_info("time = %d.%d\n", t.sec + (t.min * 60), t.us);

	if (mld->ap2cp_kerneltime_sec.type == DRAM_V2) {
		set_ctrl_msg(&mld->ap2cp_kerneltime_sec, t.sec + (t.min * 60));
		set_ctrl_msg(&mld->ap2cp_kerneltime_usec, t.us);
	} else {
		update_ctrl_msg(&mld->ap2cp_kerneltime, t.sec + (t.min * 60),
				modem->sbi_ap2cp_kerneltime_sec_mask,
				modem->sbi_ap2cp_kerneltime_sec_pos);
		update_ctrl_msg(&mld->ap2cp_kerneltime, t.us,
				modem->sbi_ap2cp_kerneltime_usec_mask,
				modem->sbi_ap2cp_kerneltime_usec_pos);
	}
}

/* change the modem state & notify io devices about this change */
void change_modem_state(struct modem_ctl *mc, enum modem_state state)
{
	enum modem_state old_state;
	struct io_device *iod;
	unsigned long flags;

	spin_lock_irqsave(&mc->lock, flags);
	old_state = mc->phone_state;
	if (state == old_state) {
		spin_unlock_irqrestore(&mc->lock, flags);
		return; /* no need to wakeup */
	}
	mc->phone_state = state;
	spin_unlock_irqrestore(&mc->lock, flags);

	mif_err("%s->state changed (%s -> %s)\n", mc->name,
		cp_state_str(old_state), cp_state_str(state));

	list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
		if (atomic_read(&iod->opened) > 0)
			wake_up(&iod->wq);
	}
}
