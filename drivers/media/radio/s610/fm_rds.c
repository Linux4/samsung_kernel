
#include "fm_low_struc.h"
#include "radio-s610.h"
#include "fm_rds.h"

void fm_rds_write_data(struct s610_radio *radio, u16 rds_data,
	fm_rds_block_type_enum blk_type, fm_host_rds_errors_enum errors)
{
	u8 *buf_ptr;
	u16 usage;
	u16 capa;

/*	API_ENTRY(radio);*/

	capa = radio->low->rds_buffer->size;
	if (radio->low->rds_buffer->outdex
			> radio->low->rds_buffer->index)
		usage = radio->low->rds_buffer->size
			- radio->low->rds_buffer->outdex
			+ radio->low->rds_buffer->index;
	else
		usage = radio->low->rds_buffer->index
			- radio->low->rds_buffer->outdex;

	if ((capa - usage) >= (HOST_RDS_BLOCK_SIZE * 4)) {
		buf_ptr = radio->low->rds_buffer->base
				+ radio->low->rds_buffer->index;

		buf_ptr[HOST_RDS_BLOCK_FMT_LSB] = (u8)(rds_data & 0xff);
		buf_ptr[HOST_RDS_BLOCK_FMT_MSB] = (u8) (rds_data >> 8);

		buf_ptr[HOST_RDS_BLOCK_FMT_STATUS] = (blk_type
				<< HOST_RDS_DATA_BLKTYPE_POSI)
				| (errors << HOST_RDS_DATA_ERRORS_POSI)
				| HOST_RDS_DATA_AVAIL_MASK;

		/* Advances the buffer's index */
		radio->low->rds_buffer->index
			+= HOST_RDS_BLOCK_SIZE;

		/* Check if the buffer's index wraps */
		if (radio->low->rds_buffer->index >=
				radio->low->rds_buffer->size) {
			radio->low->rds_buffer->index -=
				radio->low->rds_buffer->size;
		}

		if (usage >= HOST_RDS_BLOCK_SIZE)
			radio->low->fm_state.status	|= STATUS_MASK_RDS_AVA;

	}

	if (radio->low->fm_state.rds_mem_thresh != 0) {
		if (usage
				>= (radio->low->fm_state.rds_mem_thresh
					* HOST_RDS_BLOCK_SIZE)) {
			fm_set_flag_bits(radio, FLAG_BUF_FUL);
			radio->rds_cnt_mod++;
			if (!(radio->rds_cnt_mod % 100))
			APIEBUG(radio, ">N ");

			radio->rds_n_count++;
			radio->rds_new = TRUE;
			atomic_set(&radio->is_rds_new, 1);
			wake_up_interruptible(&radio->core->rds_read_queue);
		}
	}

/*	API_EXIT(radio);*/

}


u16 fm_rds_get_avail_bytes(struct s610_radio *radio)
{
	u16 avail_bytes;

	if (radio->low->rds_buffer->outdex >
			radio->low->rds_buffer->index)
		avail_bytes = (radio->low->rds_buffer->size
				- radio->low->rds_buffer->outdex
				+ radio->low->rds_buffer->index);
	else
		avail_bytes = (radio->low->rds_buffer->index
				- radio->low->rds_buffer->outdex);

	return avail_bytes;
}


bool fm_read_rds_data(struct s610_radio *radio, u16 *buffer,
		u16 *size, u16 *blocks)
{
	u16 avail_bytes;
	s16 avail_blocks;
	s16 orig_avail;
	u8 *buf_ptr;

	if (radio->low->rds_buffer == NULL) {
		*size = 0;
		if (blocks)
			*blocks = 0;
		return FALSE;
	}

	orig_avail = avail_bytes = fm_rds_get_avail_bytes(radio);

	if (avail_bytes > *size)
		avail_bytes = *size;

	avail_blocks = avail_bytes / HOST_RDS_BLOCK_SIZE;
	avail_bytes = avail_blocks * HOST_RDS_BLOCK_SIZE;

	if (avail_bytes == 0) {
		*size = 0;
		if (blocks)
			*blocks = 0;
		return FALSE;
	}

	buf_ptr = radio->low->rds_buffer->base
		+ radio->low->rds_buffer->outdex;
	(void) memcpy(buffer, buf_ptr, avail_bytes);

	/* advances the buffer's outdex */
	radio->low->rds_buffer->outdex += avail_bytes;

	/* Check if the buffer's outdex wraps */
	if (radio->low->rds_buffer->outdex >= radio->low->rds_buffer->size)
		radio->low->rds_buffer->outdex -= radio->low->rds_buffer->size;

	if (orig_avail == avail_bytes) {
		buffer[(avail_blocks - 1) * HOST_RDS_BLOCK_SIZE
			   + HOST_RDS_BLOCK_FMT_STATUS] &=
					   ~HOST_RDS_DATA_AVAIL_MASK;
		radio->low->fm_state.status &= ~STATUS_MASK_RDS_AVA;
	}

	*size = avail_bytes; /* number of bytes read */

	if (blocks)
		*blocks = avail_bytes / HOST_RDS_BLOCK_SIZE;

	/* Update RDS flags */
	if ((avail_bytes / HOST_RDS_BLOCK_SIZE)
			< radio->low->fm_state.rds_mem_thresh)
		fm_clear_flag_bits(radio, FLAG_BUF_FUL);

	return TRUE;
}

void fm_rds_change_state(struct s610_radio *radio,
		fm_rds_state_enum new_state)
{
	fm_rds_state_enum old_state =
		(fm_rds_state_enum) radio->low->fm_rds_state.current_state;

	radio->low->fm_rds_state.current_state = new_state;

	if ((old_state == RDS_STATE_FULL_SYNC)
			&& (new_state == RDS_STATE_HAVE_DATA)) {
		fm_update_rds_sync_status(radio, FALSE); /* unsynced */
	} else if ((old_state != RDS_STATE_FULL_SYNC)
			&& (new_state == RDS_STATE_FULL_SYNC)) {
		fm_update_rds_sync_status(radio, TRUE); /* synced */
	}
}

void fm_rds_update_error_status(struct s610_radio *radio, u16 errors)
{
	if (errors == 0) {
		radio->low->fm_rds_state.error_bits = 0;
		radio->low->fm_rds_state.error_blks = 0;
	} else {
		radio->low->fm_rds_state.error_bits += errors;
		radio->low->fm_rds_state.error_blks++;
	}

	if (radio->low->fm_rds_state.error_blks
			>= radio->low->fm_state.rds_unsync_blk_cnt) {
		if (radio->low->fm_rds_state.error_bits
			>= radio->low->fm_state.rds_unsync_bit_cnt) {
			/* sync-loss */
			fm_rds_change_state(radio, RDS_STATE_HAVE_DATA);
		}
		radio->low->fm_rds_state.error_bits = 0;
		radio->low->fm_rds_state.error_blks = 0;
	}
}

static fm_host_rds_errors_enum fm_rds_process_block(
		struct s610_radio *radio,
		u16 data, fm_rds_block_type_enum block_type,
		u16 err_count)
{
	fm_host_rds_errors_enum error_type;

	if (err_count == 0) {
		error_type = HOST_RDS_ERRS_NONE;
	} else if ((err_count <= 2)
			&& (err_count
			<= radio->low->fm_config.rds_error_limit)) {
		error_type = HOST_RDS_ERRS_2CORR;
	} else if ((err_count <= 5)
			&& (err_count
			<= radio->low->fm_config.rds_error_limit)) {
		error_type = HOST_RDS_ERRS_5CORR;
	} else {
		error_type = HOST_RDS_ERRS_UNCORR;
	}

	/* Write the data into the buffer */
	if ((block_type != RDS_BLKTYPE_E)
		|| (radio->low->fm_state.save_eblks)) {
		fm_rds_write_data(radio, data, block_type, error_type);
	}

	return error_type;
}

void fm_process_rds_data(struct s610_radio *radio)
{
	u32 fifo_data;
	u16 i;
	u16 avail_blocks;
	u16 data;
	u8 status;
	u16 err_count;
	fm_rds_block_type_enum block_type;

	if (!radio->low->fm_state.rds_rx_enabled)
		return;

	avail_blocks = (fmspeedy_get_reg_int(0xFFF2BF) + 1) / 4;

	for (i = 0; i < avail_blocks; i++) {
		/* Fetch the RDS word data. */
		fifo_data = fmspeedy_get_reg_int(0xFFF3C0);

		data = (u16)((fifo_data >> 16) & 0xFFFF);
		status = (u8)((fifo_data >> 8) & 0xFF);

		block_type =
			(fm_rds_block_type_enum) ((status & RDS_BLK_TYPE_MASK)
				>> RDS_BLK_TYPE_SHIFT);
		err_count = (status & RDS_ERR_CNT_MASK);

		switch (radio->low->fm_rds_state.current_state) {
		case RDS_STATE_INIT:
			fm_rds_change_state(radio, RDS_STATE_HAVE_DATA);
			break;
		case RDS_STATE_HAVE_DATA:
			if ((block_type == RDS_BLKTYPE_A)
					&& (err_count == 0)) {
				/* Move to full sync */
				fm_rds_change_state(radio,
						RDS_STATE_FULL_SYNC);
				fm_rds_process_block(radio,
						data, block_type, err_count);
			}
			break;
		case RDS_STATE_PRE_SYNC:
			break;
		case RDS_STATE_FULL_SYNC:
			if (fm_rds_process_block(radio,
				data, block_type, err_count)
				== HOST_RDS_ERRS_UNCORR) {
				fm_rds_update_error_status(radio,
				radio->low->fm_state.rds_unsync_uncorr_weight);
			} else {
				fm_rds_update_error_status(radio, err_count);
			}
			break;
		}
	}
}

