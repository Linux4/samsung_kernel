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

