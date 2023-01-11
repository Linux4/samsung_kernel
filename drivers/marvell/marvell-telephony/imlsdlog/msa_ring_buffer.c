/*
* This software program is licensed subject to the GNU General Public License
* (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

* (C) Copyright 2013 Marvell International Ltd.
* All Rights Reserved
*/

#include <linux/kernel.h>
#include <linux/ratelimit.h>

#include "msa_ring_buffer.h"

struct msa_slog_tag {
	unsigned char dirty;
};

struct msa_ring_buffer_state {
	struct msa_slog_tag slots[SLOT_NUMBER];
	unsigned char read_index;
	unsigned char write_index;
	int overwrite_number;
	int overrun_number;
};

static struct msa_ring_buffer_state msa_ring;

void init_ring_state(void)
{
	int slot_index;
	for (slot_index = 0; slot_index < SLOT_NUMBER; slot_index++)
		msa_ring.slots[slot_index].dirty = 0;
	msa_ring.read_index = 0;
	msa_ring.write_index = 0;
	msa_ring.overwrite_number = 0;
	msa_ring.overrun_number = 0;
}

void slot_data_in_ring(int received_slot)
{
	int current_wslot;
	do {
		current_wslot = msa_ring.write_index;
		if (unlikely(msa_ring.slots[current_wslot].dirty)) {
			msa_ring.overwrite_number++;
			pr_err_ratelimited(
				"[iml]msa ring override a slot, total:%d\n",
				msa_ring.overwrite_number);
		}
		msa_ring.slots[current_wslot].dirty = 1;
		if (current_wslot == received_slot)
			break;
		msa_ring.write_index = current_wslot + 1 == SLOT_NUMBER ?
		    0 : current_wslot + 1;

	} while (1);
	msa_ring.write_index = current_wslot + 1 == SLOT_NUMBER ?
	    0 : current_wslot + 1;
}

int get_next_read_slot(void)
{

/*
	if( msa_ring.read_index == msa_ring.write_index){
		if(msa_ring.slots[msa_ring.read_index].dirty){
			return msa_ring.read_index;
		}
	}

	*/
	while (msa_ring.read_index != msa_ring.write_index) {
		if (msa_ring.slots[msa_ring.read_index].dirty) {
			int ret = msa_ring.read_index;
			msa_ring.read_index =
			    msa_ring.read_index + 1 ==
			    SLOT_NUMBER ? 0 : msa_ring.read_index + 1;
			return ret;
		}
		msa_ring.read_index = msa_ring.read_index + 1 == SLOT_NUMBER ?
		    0 : msa_ring.read_index + 1;
	}
/*
	if( msa_ring.read_index == msa_ring.write_index){
		if(msa_ring.slots[msa_ring.read_index].dirty){
			return msa_ring.read_index;
		}
	}
*/
	return -1;
}

void slot_data_out_ring(int slot)
{
	if (unlikely(!msa_ring.slots[slot].dirty))
		pr_err_ratelimited("miss slot %d\n", slot);
	msa_ring.slots[slot].dirty = 0;
}
