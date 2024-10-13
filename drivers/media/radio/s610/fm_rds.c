
#include "fm_low_struc.h"
#include "radio-s610.h"
#include "fm_rds.h"
extern struct s610_radio *gradio;

#ifdef	USE_RINGBUFF_API
void ringbuf_reset(struct ringbuf_t *rb)
{
	rb->head = rb->tail = rb->buf;
}

int ringbuf_buffer_size(const struct ringbuf_t *rb)
{
	return rb->size;
}

int ringbuf_capacity(const struct ringbuf_t *rb)
{
	return ringbuf_buffer_size(rb) - 1;
}

static const u8 *ringbuf_end(const struct ringbuf_t *rb)
{
	return rb->buf + ringbuf_buffer_size(rb);
}

int ringbuf_bytes_free(const struct ringbuf_t *rb)
{
	if (rb->head >= rb->tail)
		return ringbuf_capacity(rb) - (rb->head - rb->tail);
	else
		return rb->tail - rb->head - 1;
}

int ringbuf_bytes_used(const struct ringbuf_t *rb)
{
	return ringbuf_capacity(rb) - ringbuf_bytes_free(rb);
}

int ringbuf_is_full(const struct ringbuf_t *rb)
{
	return ringbuf_bytes_free(rb) == 0;
}

int ringbuf_is_empty(const struct ringbuf_t *rb)
{
	return ringbuf_bytes_free(rb) == ringbuf_capacity(rb);
}

const void *ringbuf_tail(const struct ringbuf_t *rb)
{
	return rb->tail;
}

const void *ringbuf_head(const struct ringbuf_t *rb)
{
	return rb->head;
}

static u8 *ringbuf_nextp(struct ringbuf_t *rb, const u8 *p)
{
	/*
	 * The assert guarantees the expression (++p - rb->buf) is
	 * non-negative; therefore, the modulus operation is safe and
	 * portable.
	 */
	return rb->buf + ((++p - rb->buf) % ringbuf_buffer_size(rb));
}

void *ringbuf_memcpy_into(struct ringbuf_t *dst, const void *src, int count)
{
	const u8 *u8src = src;
	const u8 *bufend = ringbuf_end(dst);
	int overflow = count > ringbuf_bytes_free(dst);
	int nread = 0;
	int n = 0;

	while (nread != count) {
		n = MIN(bufend - dst->head, count - nread);
		memcpy(dst->head, u8src + nread, n);
		dst->head += n;
		nread += n;

		/* wrap? */
		if (dst->head == bufend)
			dst->head = dst->buf;
	}

	if (overflow)
		dst->tail = ringbuf_nextp(dst, dst->head);

	return dst->head;
}

void *ringbuf_memcpy_from(void *dst, struct ringbuf_t *src, int count)
{
	int n = 0;
	int bytes_used = ringbuf_bytes_used(src);
	u8 *u8dst = dst;
	const u8 *bufend = ringbuf_end(src);
	int nwritten = 0;

	if (count > bytes_used)
		return 0;

	while (nwritten != count) {
		n = MIN(bufend - src->tail, count - nwritten);
		memcpy(u8dst + nwritten, src->tail, n);
		src->tail += n;
		nwritten += n;

		/* wrap ? */
		if (src->tail == bufend)
			src->tail = src->buf;
	}

	return src->tail;
}

void * ringbuf_memcpy_remove(struct ringbuf_t *dst, int count)
{
	int n = 0;
	int bytes_used = ringbuf_bytes_used(dst);
	const u8 *bufend = ringbuf_end(dst);
	int nwritten = 0;
	unsigned char *cls_start;

	if (count > bytes_used)
		return 0;

	while (nwritten != count) {
		n = MIN(bufend - dst->head, count - nwritten);
		cls_start = dst->head - n;
		memset(cls_start, 0, n);
		dst->head -= n;
		nwritten += n;

		/* wrap ? */
		if (dst->head == bufend)
			dst->head = dst->buf;
	}

	return dst->head;
}
#endif /* USE_RINGBUFF_API */

#ifdef	USE_RINGBUFF_API
void fm_rds_write_data(struct s610_radio *radio,
		u16 rds_data, fm_rds_block_type_enum blk_type,
		fm_host_rds_errors_enum errors)
{
	u8 buf_ptr[HOST_RDS_BLOCK_SIZE];
	u16 usage;

	if (ringbuf_is_full(&radio->rds_rb)) {
		dev_info(radio->dev, "%s():>>>RB full! H[%ld]T[%ld]",
			__func__,
			(unsigned long) (radio->rds_rb.head - radio->rds_rb.buf),
			(unsigned long) (radio->rds_rb.tail - radio->rds_rb.buf));

		goto skip_into_buf;
	}

	buf_ptr[HOST_RDS_BLOCK_FMT_LSB] = (u8)(rds_data & 0xff);
	buf_ptr[HOST_RDS_BLOCK_FMT_MSB] = (u8)(rds_data >> 8);
	buf_ptr[HOST_RDS_BLOCK_FMT_STATUS] =
			(blk_type << HOST_RDS_DATA_BLKTYPE_POSI)
			| (errors << HOST_RDS_DATA_ERRORS_POSI)
			| HOST_RDS_DATA_AVAIL_MASK;

	if (!ringbuf_memcpy_into(&radio->rds_rb, buf_ptr, HOST_RDS_BLOCK_SIZE)) {
		usage = ringbuf_bytes_used(&radio->rds_rb);
		dev_info(radio->dev,
			"%s():>>>RB memcpy into fail! usage:%04d",
			__func__, ringbuf_bytes_used(&radio->rds_rb));
		if (!usage)
			return;
	}

skip_into_buf:
	usage = ringbuf_bytes_used(&radio->rds_rb);

	if (usage >= HOST_RDS_BLOCK_SIZE)
		radio->low->fm_state.status |= STATUS_MASK_RDS_AVA;

	if (radio->low->fm_state.rds_mem_thresh != 0) {
		if (usage >= (radio->low->fm_state.rds_mem_thresh+(HOST_RDS_BLOCK_SIZE*4))) {
			if (atomic_read(&radio->is_rds_new))
				return;

			fm_set_flag_bits(radio, FLAG_BUF_FUL);
			atomic_set(&radio->is_rds_new, 1);
			wake_up_interruptible(&radio->core->rds_read_queue);
			radio->rds_n_count++;

			if (!(radio->rds_n_count%200)) {
				fm_update_rssi_work(radio);
				dev_info(radio->dev,
					">>>[FM] RSSI[%03d] NCOUNT[%08d] FIFO_ERR[%08d] USAGE[%04d] SYNCLOSS[%08d]",
					radio->low->fm_state.rssi, radio->rds_n_count, radio->rds_fifo_err_cnt,
					usage, radio->rds_sync_loss_cnt);
			}
		}
	}
}
#else	/* USE_RINGBUFF_API */
void fm_rds_write_data(struct s610_radio *radio, u16 rds_data,
	fm_rds_block_type_enum blk_type, fm_host_rds_errors_enum errors)
{
	u8 *buf_ptr;
	u16 usage;
	u16 capa;

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
		if (usage >= (radio->low->fm_state.rds_mem_thresh+(HOST_RDS_BLOCK_SIZE*4))) {
			if (atomic_read(&radio->is_rds_new))
				return;

			fm_set_flag_bits(radio, FLAG_BUF_FUL);
			atomic_set(&radio->is_rds_new, 1);
			wake_up_interruptible(&radio->core->rds_read_queue);
			radio->rds_n_count++;

			if (!(radio->rds_n_count%200)) {
				fm_update_rssi_work(radio);
				dev_info(radio->dev,
					">>>[FM] RSSI[%03d] NCOUNT[%08d] FIFO_ERR[%08d] USAGE[%04d] SYNCLOSS[%08d]",
					radio->low->fm_state.rssi, radio->rds_n_count, radio->rds_fifo_err_cnt,
					usage, radio->rds_sync_loss_cnt);
			}
		}
	}
}
#endif /* USE_RINGBUFF_API */

#ifdef	USE_RDS_BLOCK_SEQ_CORRECT
#ifdef	USE_RINGBUFF_API
void fm_rds_write_data_remove(struct s610_radio *radio,
	fm_rds_rm_align_enum removeblock)
{
	unsigned long pre_head, cur_head;
	pre_head = (unsigned long) radio->rds_rb.head;
	ringbuf_memcpy_remove(&radio->rds_rb,
		(int)removeblock*HOST_RDS_BLOCK_SIZE);
	cur_head = (unsigned long) radio->rds_rb.head;
	dev_info(radio->dev, ">>> pre-head :%08lX, cur-head :%08lX\n",
		pre_head, cur_head);
}
#else
void fm_rds_write_data_remove(struct s610_radio *radio,
	fm_rds_rm_align_enum removeblock)
{
	int i;
	u8 *buf_ptr;

	for (i = 0; i < removeblock*HOST_RDS_BLOCK_SIZE; i++) {
		buf_ptr = radio->low->rds_buffer->base
			+ radio->low->rds_buffer->index;
		buf_ptr[0] = 0;
		if (radio->low->rds_buffer->index == 0)
			radio->low->rds_buffer->index = radio->low->rds_buffer->size;
		radio->low->rds_buffer->index--;
	}
	RDSEBUG(radio, "%s():<<<WR-RM:%d index[%04d]",
		 __func__, removeblock,
		radio->low->rds_buffer->index);

}
#endif	/*USE_RINGBUFF_API*/
#endif	/* USE_RDS_BLOCK_SEQ_CORRECT */

#ifdef	USE_RINGBUFF_API
int fm_read_rds_data(struct s610_radio *radio, u8 *buffer, int size,
		u16 *blocks)
{
	u16 rsize = size;

	if (ringbuf_is_empty(&radio->rds_rb)) {
		dev_info(radio->dev,
			"%s():>>>RB empty!! H[%04ld]T[%04ld]",
			__func__,
			(unsigned long) (radio->rds_rb.head - radio->rds_rb.buf),
			(unsigned long) (radio->rds_rb.tail - radio->rds_rb.buf));

		return 0;
	}
	radio->rb_used = ringbuf_bytes_used(&radio->rds_rb);
	if (!ringbuf_memcpy_from(buffer, &radio->rds_rb, rsize)) {
		dev_info(radio->dev,
			"%s():>>>RB memcpy from fail! H[%04ld]T[%04ld]",
			__func__,
			(unsigned long) (radio->rds_rb.head - radio->rds_rb.buf),
			(unsigned long) (radio->rds_rb.tail - radio->rds_rb.buf));

		/* ringbuff reset */
		ringbuf_reset(&radio->rds_rb);
		if (blocks)
			blocks = 0;
		return 0;
	}

	if (blocks)
		*blocks = rsize / HOST_RDS_BLOCK_SIZE;

	/* Update RDS flags */
	if ((rsize / HOST_RDS_BLOCK_SIZE) < radio->low->fm_state.rds_mem_thresh)
		fm_clear_flag_bits(radio, FLAG_BUF_FUL);

	dev_info(radio->dev,
		"%s():>>>RB1 H[%04ld]T[%04ld]",
		__func__,
		(unsigned long) (radio->rds_rb.head - radio->rds_rb.buf),
		(unsigned long) (radio->rds_rb.tail - radio->rds_rb.buf));

	return rsize;
}
#else	/* USE_RINGBUFF_API */
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


int fm_read_rds_data(struct s610_radio *radio, u8 *buffer, int size,
		u16 *blocks)
{
	u16 avail_bytes;
	s16 avail_blocks;
	s16 orig_avail;
	u8 *buf_ptr;

	if (radio->low->rds_buffer == NULL) {
		size = 0;
		if (blocks)
			*blocks = 0;
		return FALSE;
	}

	orig_avail = avail_bytes = fm_rds_get_avail_bytes(radio);

	if (avail_bytes > size)
		avail_bytes = size;

	avail_blocks = avail_bytes / HOST_RDS_BLOCK_SIZE;
	avail_bytes = avail_blocks * HOST_RDS_BLOCK_SIZE;

	if (avail_bytes == 0) {
		size = 0;
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

	size = avail_bytes; /* number of bytes read */

	if (blocks)
		*blocks = avail_bytes / HOST_RDS_BLOCK_SIZE;

	/* Update RDS flags */
	if ((avail_bytes / HOST_RDS_BLOCK_SIZE)
			< radio->low->fm_state.rds_mem_thresh)
		fm_clear_flag_bits(radio, FLAG_BUF_FUL);

	return size;
}
#endif	/* USE_RINGBUFF_API */

#ifdef USE_RDS_HW_DECODER
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
#else	/*USE_RDS_HW_DECODER*/
void fm_rds_change_state(struct s610_radio *radio,
		fm_rds_state_enum new_state)
{
	radio->low->fm_rds_state.current_state = new_state;
}

fm_rds_state_enum fm_rds_get_state(struct s610_radio *radio)
{
	return radio->low->fm_rds_state.current_state;
}
#endif	/*USE_RDS_HW_DECODER*/

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
			RDSEBUG(radio, "%s() >>>>> RDS sync-loss[%08d]!!!!!",
				__func__, radio->rds_sync_loss_cnt);
			radio->rds_sync_loss_cnt++;
#ifdef USE_RDS_HW_DECODER
			fm_rds_change_state(radio, RDS_STATE_HAVE_DATA);
#else
			/*remove data*/
			fm_rds_write_data_remove(radio, fm_rds_get_state(radio)+1);
			fm_rds_change_state(radio, RDS_STATE_INIT);
#endif	/*USE_RDS_HW_DECODER*/
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
	struct fm_rds_parser_info *pi;

	if (radio->rds_paser_enable)
		pi = &(radio->pi);

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
		if (radio->rds_paser_enable) {
			fm_rds_parser(pi, data, block_type, error_type);
			fm_rds_write_data_pi(radio, pi);
		} else {
		fm_rds_write_data(radio, data, block_type, error_type);
	}
	}

	return error_type;
}

#ifdef	USE_RDS_BLOCK_SEQ_CORRECT
fm_rds_rm_align_enum fm_check_block_seq(fm_rds_block_type_enum pre_block_type,
	fm_rds_block_type_enum curr_block_type)
{
	fm_rds_rm_align_enum ret = RDS_RM_ALIGN_NONE;

	if ((pre_block_type == RDS_BLKTYPE_A) && (curr_block_type != RDS_BLKTYPE_B)) {
		ret = RDS_RM_ALIGN_1;
	}
	else if ((pre_block_type == RDS_BLKTYPE_B) && (curr_block_type != RDS_BLKTYPE_C)) {
		ret = RDS_RM_ALIGN_2;
	}
	else if ((pre_block_type == RDS_BLKTYPE_C) && (curr_block_type != RDS_BLKTYPE_D)) {
		ret = RDS_RM_ALIGN_3;
	}
	else if ((pre_block_type == RDS_BLKTYPE_D) && (curr_block_type != RDS_BLKTYPE_A)) {
		ret = RDS_RM_ALIGN_0;
	}
	return ret;
}
#endif	/* USE_RDS_BLOCK_SEQ_CORRECT */

#ifdef USE_RDS_HW_DECODER
void fm_process_rds_data(struct s610_radio *radio)
{
	u32 fifo_data;
	u16 i;
	u16 avail_blocks;
	u16 data;
	u8 status;
	u16 err_count;
	fm_rds_block_type_enum block_type;
#ifdef	USE_RDS_BLOCK_SEQ_CORRECT
	fm_rds_rm_align_enum rm_blk = RDS_RM_ALIGN_NONE;
#endif	/* USE_RDS_BLOCK_SEQ_CORRECT */

	if (!radio->low->fm_state.rds_rx_enabled)
		return;

	avail_blocks = (fmspeedy_get_reg_int(0xFFF2BF) + 1) / 4;

	for (i = 0; i < avail_blocks; i++) {
		/* Fetch the RDS word data. */
		atomic_set(&radio->is_rds_doing, 1);
		fifo_data = fmspeedy_get_reg_int(0xFFF3C0);
		radio->rds_fifo_rd_cnt++;
		data = (u16)((fifo_data >> 16) & 0xFFFF);
		status = (u8)((fifo_data >> 8) & 0xFF);
		block_type =
			(fm_rds_block_type_enum) ((status & RDS_BLK_TYPE_MASK)
				>> RDS_BLK_TYPE_SHIFT);
		err_count = (status & RDS_ERR_CNT_MASK);
		atomic_set(&radio->is_rds_doing, 0);

		switch (radio->low->fm_rds_state.current_state) {
		case RDS_STATE_INIT:
			fm_rds_change_state(radio, RDS_STATE_HAVE_DATA);
		case RDS_STATE_HAVE_DATA:
			if ((block_type == RDS_BLKTYPE_A)
					&& (err_count == 0)) {
#ifdef	USE_RDS_BLOCK_SEQ_CORRECT
				radio->block_seq = RDS_BLKTYPE_A;
#endif	/*USE_RDS_BLOCK_SEQ_CORRECT */
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
#ifdef	USE_RDS_BLOCK_SEQ_CORRECT
			rm_blk = fm_check_block_seq(radio->block_seq, block_type);
			if (rm_blk < RDS_RM_ALIGN_NONE) {
				RDSEBUG(radio,
					"pre block[%02d],curr block[%02d],err count[%08d]",
					radio->block_seq, block_type, radio->rds_fifo_err_cnt);
				if (rm_blk != RDS_RM_ALIGN_0) {
					fm_rds_write_data_remove(radio, rm_blk);
					radio->block_seq = RDS_BLKTYPE_D;
				}
				fm_rds_change_state(radio, RDS_STATE_HAVE_DATA);
				radio->rds_fifo_err_cnt++;
				break;
			}
			radio->block_seq = block_type;
#endif /*	USE_RDS_BLOCK_SEQ_CORRECT */
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
#endif /* USE_RDS_HW_DECODER */
void find_pi_data(struct fm_rds_parser_info *pi, u16 info)
{

	pi->pi_buf[pi->pi_idx % 2] = info;

	if (!(++pi->pi_idx % 2)) {
		if (pi->pi_buf[0] == pi->pi_buf[1]) {
			pi->rds_event |= RDS_EVENT_PI_MASK;
			RDSEBUG(gradio, "[RDS] PI : 0x%x\n", pi->pi_buf[0]);
		}
	}
}

void find_ecc_data(struct fm_rds_parser_info *pi, u16 info)
{

	pi->ecc_buf[pi->ecc_idx % 2] = info & 0xFF;

	if (!(++pi->ecc_idx % 2)) {
		if (pi->ecc_buf[0] == pi->ecc_buf[1]) {
			pi->rds_event |= RDS_EVENT_ECC_MASK;
			RDSEBUG(gradio, "[RDS] ECC : %d\n", pi->ecc_buf[0]);
		}
	}
}

void store_ps_data(struct fm_rds_parser_info *pi,
	u16 info, u8 err_cnt)
{
	char a, b;
	u32 i = pi->ps_idx % 3;
	u8 seg = pi->ps_segment;

	if (pi->drop_blk)
		return;

	a = (info >> 8) & 0xff;
	b = info & 0xff;

	pi->ps_buf[i][seg * 2] = a;
	pi->ps_buf[i][seg * 2 + 1] = b;
	pi->ps_err[i][seg] = err_cnt;
	/*RDSEBUG(gradio,
		"[RDS] PS: [%c][%c] [%d][%d], modulo idx=%d, seg=%d\n",
		a, b, (int)a, (int)b, i, seg);*/
}

void validate_ps_data(struct fm_rds_parser_info *pi)
{
	u32 i;
	bool match = true;

	for (i = 0; i < pi->ps_len / 2; i++) {
		if (pi->ps_err[pi->ps_idx % 3][i] > 0)
			break;
	}

	if (i == pi->ps_len / 2) {
		memcpy(pi->ps_candidate,
			pi->ps_buf[pi->ps_idx % 3],
			pi->ps_len);
		memset(pi->ps_err[pi->ps_idx % 3],
			0xFF, MAX_PS / 2);
		pi->ps_candidate[pi->ps_len] = 0;
		pi->rds_event |= RDS_EVENT_PS_MASK;
		pi->ps_idx++;
		RDSEBUG(gradio,
			"[RDS] ### PS candidate[i]: %s\n",
			pi->ps_candidate);
	} else {
		if (++pi->ps_idx >= 3) {
			for (i = 0; i < pi->ps_len; i++) {
				if (pi->ps_buf[0][i] == pi->ps_buf[1][i])
					pi->ps_candidate[i] = pi->ps_buf[0][i];
				else if (pi->ps_buf[0][i] == pi->ps_buf[2][i])
					pi->ps_candidate[i] = pi->ps_buf[0][i];
				else if (pi->ps_buf[1][i] == pi->ps_buf[2][i])
					pi->ps_candidate[i] = pi->ps_buf[1][i];
				else
					match = false;
			}

			if (match) {
				pi->ps_candidate[pi->ps_len] = 0;
				pi->rds_event |= RDS_EVENT_PS_MASK;
				RDSEBUG(gradio,
					"[RDS] ### PS candidate[m]: %s\n",
					pi->ps_candidate);
			}
		}
	}

	i = pi->ps_idx - 1;
	pi->ps_buf[i % 3][pi->ps_len] = 0;

	for (i = 0; i < 3; i++)
		RDSEBUG(gradio,
		"[RDS] ### PS received[%d]: %s\n",
			i, pi->ps_buf[i]);
}

void store_rt_data(struct fm_rds_parser_info *pi, u16 info, u8 blk_type, u8 err_cnt)
{
	char a, b;
	u32 i = pi->rt_idx % 3;
	u8 seg = pi->rt_segment;

	if (pi->drop_blk)
		return;

	a = (info >> 8) & 0xff;
	b = info & 0xff;

	switch (blk_type) {
	case RDS_BLKTYPE_C:
		pi->rt_buf[i][seg * 4] = a;
		pi->rt_buf[i][seg * 4 + 1] = b;
		pi->rt_err[i][seg * 2] = err_cnt;
		/*RDSEBUG(gradio,
			"[RDS] RT_A(C): [%c][%c] [%d][%d], modulo idx=%d, seg=%d\n",
			a, b, (int)a, (int)b, i, seg);*/
		break;

	case RDS_BLKTYPE_D:
		if (pi->grp == RDS_GRPTYPE_2A) {
			pi->rt_buf[i][seg * 4 + 2] = a;
			pi->rt_buf[i][seg * 4 + 3] = b;
			pi->rt_err[i][seg * 2 + 1] = err_cnt;
			/*RDSEBUG(gradio,
				"[RDS] RT_A(D): [%c][%c] [%d][%d], modulo idx=%d, seg=%d\n",
				a, b, (int)a, (int)b, i, seg);*/
		} else if (pi->grp == RDS_GRPTYPE_2B) {
			pi->rt_buf[i][seg * 2] = a;
			pi->rt_buf[i][seg * 2 + 1] = b;
			pi->rt_err[i][seg] = err_cnt;
			/*RDSEBUG(gradio,
				"[RDS] RT_B(C): [%c][%c] [%d][%d], modulo idx=%d, seg=%d\n",
				a, b, (int)a, (int)b, i, seg);*/
		}
	default:
		break;
	}
}

void validate_rt_data(struct fm_rds_parser_info *pi)
{
	u32 i;
	bool match = true;

	for (i = 0; i < pi->rt_len / 2; i++) {
		if (pi->rt_err[pi->rt_idx % 3][i] > 0)
			break;
	}

	if (i == pi->rt_len / 2) {
		memcpy(pi->rt_candidate, pi->rt_buf[pi->rt_idx % 3], pi->rt_len);
		memset(pi->rt_err[pi->rt_idx % 3], 0xFF, MAX_RT / 2);
		pi->rt_candidate[pi->rt_len] = 0;

		if (strlen(pi->rt_candidate) == pi->rt_len) {
			pi->rds_event |= RDS_EVENT_RT_MASK;
			pi->rt_idx++;
			RDSEBUG(gradio,
				"[RDS] ### RT candidate[i]: %s,  %d, %d\n",
				pi->rt_candidate, (int)strlen(pi->rt_candidate), pi->rt_len);
		}
	} else {
		if (++pi->rt_idx >= 3) {
			for (i = 0; i < pi->rt_len; i++) {
				if (pi->rt_buf[0][i] == pi->rt_buf[1][i])
					pi->rt_candidate[i] = pi->rt_buf[0][i];
				else if (pi->rt_buf[0][i] == pi->rt_buf[2][i])
					pi->rt_candidate[i] = pi->rt_buf[0][i];
				else if (pi->rt_buf[1][i] == pi->rt_buf[2][i])
					pi->rt_candidate[i] = pi->rt_buf[1][i];
				else
					match = false;
			}

			if (match) {
				pi->rt_candidate[pi->rt_len] = 0;

				if (strlen(pi->rt_candidate) == pi->rt_len) {
					pi->rds_event |= RDS_EVENT_RT_MASK;
					RDSEBUG(gradio,
						"[RDS] ### RT candidate[m]: %s, %d, %d\n",
						pi->rt_candidate, (int)strlen(pi->rt_candidate), pi->rt_len);
				}
			}
		}
	}

	i = pi->rt_idx - 1;
	pi->rt_buf[i % 3][pi->rt_len] = 0;
	for (i = 0; i < 3; i++)
		RDSEBUG(gradio,
		"[RDS] ### RT received[%d]: %s\n",
		i, pi->rt_buf[i]);
}

void find_group_data(struct fm_rds_parser_info *pi, u16 info)
{
	u8 segment, rt_change;

	pi->grp = (info >> 11) & 0x1f;

	switch (pi->grp) {
	case RDS_GRPTYPE_0A:
	case RDS_GRPTYPE_0B:
		segment = info & 0x3;

		if (!segment && pi->ps_segment != 0xFF)
			validate_ps_data(pi);

		pi->ps_segment = segment;

		if (pi->ps_len < (segment + 1) * 2)
			pi->ps_len = (segment + 1) * 2;

		/*RDSEBUG(gradio,
			"[RDS] PS: seg=%d, len=%d\n",
			segment, pi->ps_len);*/
		break;

	case RDS_GRPTYPE_2A:
	case RDS_GRPTYPE_2B:
		segment = info & 0xF;
		rt_change = (info & 0x10) >> 4;

		RDSEBUG(gradio,
			"[RDS] segment=%d, pi->rt_segment=%d, pi->rt_change=%d, rt_change=%d\n",
				segment, pi->rt_segment, pi->rt_change, rt_change);

		if ((!segment && pi->rt_segment != 0xFF)
				|| (pi->rt_change != 0xFF && pi->rt_change != rt_change)) {
			validate_rt_data(pi);

			if (pi->rt_change != 0xFF && pi->rt_change != rt_change) {
				pi->rt_len = 0;
				pi->rt_idx = 0;
				memset(pi->rt_buf, 0, 3 * (MAX_RT + 1));
				memset(pi->rt_err, 0xFF, 3 * MAX_RT / 2);
			}
		}

		pi->rt_segment = segment;
		pi->rt_change = rt_change;

		if (pi->grp == RDS_GRPTYPE_2A) {
			if (pi->rt_len < (segment + 1) * 4)
				pi->rt_len = (segment + 1) * 4;
		} else {
			if (pi->rt_len < (segment + 1) * 2)
				pi->rt_len = (segment + 1) * 2;
		}

		RDSEBUG(gradio,
			"[RDS] RT: seg=%d, tc=%d, len=%d\n",
			segment, rt_change, pi->rt_len);
		break;

	default:
		break;
	}
}

void find_af_data(struct fm_rds_parser_info *pi, u16 info)
{

	pi->af_buf[pi->af_idx % 2] = info;

	if (!(++pi->af_idx % 2)) {
		if (pi->af_buf[0] == pi->af_buf[1])
			pi->rds_event |= RDS_EVENT_AF_MASK;
	}
}

void fm_rds_parser(struct fm_rds_parser_info *pi, u16 info, u8 blk_type, u8 err_cnt)
{

	switch (blk_type) {
	case RDS_BLKTYPE_A:
		find_pi_data(pi, info);
		break;

	case RDS_BLKTYPE_B:
		if (err_cnt > 2) {
			pi->grp = RDS_GRPTYPE_NONE;
			pi->drop_blk = true;
			break;
		} else {
			pi->drop_blk = false;
		}

		find_group_data(pi, info);
		break;

	case RDS_BLKTYPE_C:
		if (err_cnt > 5)
			return;

		switch (pi->grp) {
		case RDS_GRPTYPE_0A:
			find_af_data(pi, info);
			break;

		case RDS_GRPTYPE_0B:
			find_pi_data(pi, info);
			break;

		case RDS_GRPTYPE_1A:
			find_ecc_data(pi, info);
			break;

		case RDS_GRPTYPE_2A:
			store_rt_data(pi, info, blk_type, err_cnt);
			break;

		case RDS_GRPTYPE_2B:
			find_pi_data(pi, info);
			break;

		default:
			break;
		}
		break;

	case RDS_BLKTYPE_D:
		if (err_cnt > 5)
			return;

			switch (pi->grp) {
			case RDS_GRPTYPE_0A:
			case RDS_GRPTYPE_0B:
				store_ps_data(pi, info, err_cnt);
				break;

			case RDS_GRPTYPE_2A:
			case RDS_GRPTYPE_2B:
				store_rt_data(pi, info, blk_type, err_cnt);
				break;

			default:
				break;
			}
			break;
	}
}

void fm_rds_write_data_pi(struct s610_radio *radio,
	struct fm_rds_parser_info *pi)
{
	u8 buf_ptr[RDS_MEM_MAX_THRESH_PARSER];
	u8 i, str_len;
	int total_size = 0;
	u16 usage;

#ifdef USE_RINGBUFF_API
	if (ringbuf_bytes_free(&radio->rds_rb) < RDS_MEM_MAX_THRESH_PARSER)	{
		dev_info(radio->dev, ">>> ringbuff not free, wait..");
		/*ringbuf_reset(&radio->rds_rb);*/
		return;
	}
#else
	dev_info(radio->dev, ">>> ringbuff API not used, fail..");
	return;
#endif /* USE_RINGBUFF_API */

	memset(buf_ptr, 0, RDS_MEM_MAX_THRESH_PARSER);

	while (pi->rds_event) {
		RDSEBUG(radio,
			"%s() rds_event[%04X]\n", __func__, pi->rds_event);

		if (pi->rds_event & RDS_EVENT_PI_MASK) {
			pi->rds_event &= ~RDS_EVENT_PI_MASK;
			buf_ptr[total_size] = RDS_EVENT_PI;
			buf_ptr[total_size+1] = 2;
			buf_ptr[total_size+2] = (u8)(pi->pi_buf[0] & 0xFF);
			buf_ptr[total_size+3] = (u8)(pi->pi_buf[0] >> 8);
			RDSEBUG(radio,
				"[RDS] Enqueue PI data[%02X:%02X:%02X:%02X]. total=%d\n",
				buf_ptr[total_size],buf_ptr[total_size+1],buf_ptr[total_size+2],buf_ptr[total_size+3],
				total_size+4);

			total_size += 4;

		} else if (pi->rds_event & RDS_EVENT_AF_MASK) {
			pi->rds_event &= ~RDS_EVENT_AF_MASK;
			buf_ptr[total_size] = RDS_EVENT_AF;
			buf_ptr[total_size+1] = 2;
			buf_ptr[total_size+2] = (u8)(pi->af_buf[0] & 0xFF);
			buf_ptr[total_size+3] = (u8)(pi->af_buf[0] >> 8);
			RDSEBUG(radio,
				"[RDS] Enqueue AF data[%02X:%02X:%02X:%02X]. total=%d\n",
				buf_ptr[total_size],buf_ptr[total_size+1],buf_ptr[total_size+2],buf_ptr[total_size+3],
				total_size+4);

			total_size += 4;

		} else if (pi->rds_event & RDS_EVENT_PS_MASK) {
			pi->rds_event &= ~RDS_EVENT_PS_MASK;
			str_len = strlen(pi->ps_candidate);
			buf_ptr[total_size] = RDS_EVENT_PS;
			buf_ptr[total_size+1] = str_len;

			for (i = 0; i < str_len; i++) {
				buf_ptr[total_size + i + 2] = pi->ps_candidate[i];
			}

			total_size += str_len + 2;
			RDSEBUG(radio,
				"[RDS] Enqueue PS data. total=%d,%d,%d\n", total_size, pi->ps_len + 2, str_len);
		} else if (pi->rds_event & RDS_EVENT_RT_MASK) {
			pi->rds_event &= ~RDS_EVENT_RT_MASK;

			str_len = strlen(pi->rt_candidate);
			buf_ptr[total_size] = RDS_EVENT_RT;
			buf_ptr[total_size+1] = str_len;

			for (i = 0; i < str_len; i++) {
				buf_ptr[total_size + i + 2] = pi->rt_candidate[i];
			}

			total_size += str_len + 2;
			RDSEBUG(radio,
				"[RDS] Enqueue RT data. total=%d,%d,%d\n", total_size, pi->rt_len + 2, str_len + 2);
		} else if (pi->rds_event & RDS_EVENT_ECC_MASK) {
			pi->rds_event &= ~RDS_EVENT_ECC_MASK;

			buf_ptr[total_size] = RDS_EVENT_ECC;
			buf_ptr[total_size+1] = 1;
			buf_ptr[total_size+2] = (u8)pi->ecc_buf[0];
			RDSEBUG(radio,
				"[RDS] Enqueue ECC data[%02d:%02d:%02d]. total=%d\n",
				buf_ptr[total_size],buf_ptr[total_size+1],buf_ptr[total_size+2],
				total_size+3);

			total_size += 3;
		}

		if (!pi->rds_event && total_size) {
			if (!ringbuf_memcpy_into(&radio->rds_rb, buf_ptr, total_size)) {
				usage = ringbuf_bytes_used(&radio->rds_rb);
				dev_info(radio->dev,
					"%s():>>>RB memcpy into fail! usage:%04d",
					__func__, ringbuf_bytes_used(&radio->rds_rb));
				if (!usage)
					return;
			}

			usage = ringbuf_bytes_used(&radio->rds_rb);

			if (atomic_read(&radio->is_rds_new))
				return;

			fm_set_flag_bits(radio, FLAG_BUF_FUL);
			atomic_set(&radio->is_rds_new, 1);
			wake_up_interruptible(&radio->core->rds_read_queue);
			radio->rds_n_count++;

			if (!(radio->rds_n_count%200)) {
				fm_update_rssi_work(radio);
				dev_info(radio->dev,
					">>>[FM] RSSI[%03d] NCOUNT[%08d] FIFO_ERR[%08d] USAGE[%04d] SYNCLOSS[%08d]",
					radio->low->fm_state.rssi, radio->rds_n_count, radio->rds_fifo_err_cnt,
					usage, radio->rds_sync_loss_cnt);
			}
		}
	}
}

#ifndef USE_RDS_HW_DECODER
struct fm_decoded_data {
	u16 info;
	fm_rds_block_type_enum blk_type;
	u8 err_cnt;
};

void put_bit_to_byte(u8 *fifo, u32 data)
{
	s8 i, j;
	u32 mask = 0x80000000;

	for (i = BUF_LEN - 1, j = 0; i >= 0; i--, j++) {
		*(fifo + j) = (mask & data) ? 1 : 0;
		mask = mask >> 1;
	}
}

u32 get_block_data(u8 *fifo, u32 sp)
{
	u8 i, j;
	u32 data = 0;

	data |= *(fifo + (sp++ % 160));

	for (i = BLK_LEN-1, j = 0; i > 0; i--, j++) {
		data <<= 1;
		data |= *(fifo + (sp++ % 160));
	}

	return data;
}

u8 find_code_word(u32 *data, fm_rds_block_type_enum b_type, bool seq_lock)
{
	u16 first13;
	u16 second13;
	u16 syndrome;

	first13 = *data >> 13;
	second13 = (*data & 0x1FFF) ^ OFFSET_WORD[b_type];

	syndrome = CRC_LUT[(CRC_LUT[first13] << 3) ^ second13];

	if (syndrome) {
		u32 corrected;
		u8 i, j;
		u8 maxerr = (b_type == RDS_BLKTYPE_A) ? 2 : sizeof(burstErrorPattern) / sizeof(u8);

		for (i = 0; i < maxerr; i++) {
			for (j = 0; j <= BLK_LEN - burstErrorLen[i]; j++) {
				corrected = *data ^ (burstErrorPattern[i] << j);

				first13 = corrected >> 13;
				second13 = (corrected & 0x1FFF) ^ OFFSET_WORD[b_type];

				syndrome = CRC_LUT[(CRC_LUT[first13] << 3) ^ second13];

				if (!syndrome) {
					*data = corrected;
					return burstErrorLen[i];
				}
			}
		}
	} else {
		return 0;
	}
	return 6;
}

void fm_process_rds_data(struct s610_radio *radio)
{
	u16 i, j, k, l;
	u16 avail_blocks;
	u32 fifo_data;
	u32 data;
	fm_host_rds_errors_enum err_cnt;
	u8 min_pos = 0;
	u8 min_blk = 0;
	u8 min_blk_tmp = 0;
	u8 min_pos_sum, min_blk_sum;

	static u32 idx;
	static u8 fifo[160];
	static u32 start_pos;
	static u32 end_pos;
	static bool fetch_data;
	static bool seq_lock;
	static bool remains;
	static struct fm_decoded_data decoded[BLK_LEN][RDS_NUM_BLOCK_TYPES - 1][RDS_NUM_BLOCK_TYPES - 1];

	avail_blocks = (fmspeedy_get_reg_int(0xFFF2BF) + 1) / 4;

	while (avail_blocks) {
		/* Fetch the RDS raw data. */
		if (fetch_data) {
			fifo_data = fmspeedy_get_reg_int(0xFFF3C0);
			put_bit_to_byte(fifo + 32 * ((idx++) % 5), fifo_data);
			avail_blocks--;
		}

		switch (fm_rds_get_state(radio)) {
		case RDS_STATE_INIT:
			fm_rds_change_state(radio, RDS_STATE_HAVE_DATA);
			fm_update_rds_sync_status(radio, false);
			radio->low->fm_rds_state.error_bits = 0;
			radio->low->fm_rds_state.error_blks = 0;

			idx = 0;
			start_pos = 0;
			end_pos = BLK_LEN - 1;
			fetch_data = false;
			seq_lock = false;
			memset(fifo, 0, sizeof(fifo) / sizeof(u8));

		case RDS_STATE_HAVE_DATA:
			if (idx < 5) {
				fifo_data = fmspeedy_get_reg_int(0xFFF3C0);
				put_bit_to_byte(fifo + 32 * ((idx++) % 5), fifo_data);
				avail_blocks--;

				if (idx == 5) {
					for (i = 0; i < BLK_LEN; i++) {
						for (j = 0; j < RDS_NUM_BLOCK_TYPES - 1; j++) {
							start_pos = i;

							for (k = 0, l = j; k < RDS_NUM_BLOCK_TYPES - 1; k++) {
								data = get_block_data(fifo, start_pos);
								err_cnt = find_code_word(&data, l, seq_lock);

								decoded[i][j][k].info = data >> 10;
								decoded[i][j][k].blk_type = l;
								decoded[i][j][k].err_cnt = err_cnt;
								start_pos += BLK_LEN;
								l = (l + 1) % (RDS_NUM_BLOCK_TYPES - 1);
							}
						}
					}

					for (i = 0, min_pos_sum = 0xFF; i < BLK_LEN; i++) {
						for (j = 0, min_blk_sum = 0xFF; j < RDS_NUM_BLOCK_TYPES - 1; j++) {
							for (k = 0, err_cnt = 0; k < RDS_NUM_BLOCK_TYPES - 1; k++) {
								err_cnt += decoded[i][j][k].err_cnt;
							}

							if (min_blk_sum > err_cnt) {
								min_blk_sum = err_cnt;
								min_blk_tmp = j;
							}
						}

						if (min_pos_sum > min_blk_sum) {
							min_pos_sum = min_blk_sum;
							min_pos = i;
							min_blk = min_blk_tmp;
						}

					}

					fm_rds_change_state(radio, RDS_STATE_PRE_SYNC);
				} else
					break;
			}

		case RDS_STATE_PRE_SYNC:
			for (i = 0; i < RDS_NUM_BLOCK_TYPES - 1; i++) {
				if (decoded[min_pos][min_blk][i].blk_type == RDS_BLKTYPE_A) {
					fm_update_rds_sync_status(radio, TRUE);
				}

				if (fm_get_rds_sync_status(radio)) {

					if (fm_rds_process_block(radio, decoded[min_pos][min_blk][i].info,
							decoded[min_pos][min_blk][i].blk_type, decoded[min_pos][min_blk][i].err_cnt)
							== HOST_RDS_ERRS_UNCORR) {
						fm_rds_update_error_status(radio,
								radio->low->fm_state.rds_unsync_uncorr_weight);
					} else {
						fm_rds_update_error_status(radio, decoded[min_pos][min_blk][i].err_cnt);
					}
				}
			}

			start_pos = min_pos + BLK_LEN * 3;
			end_pos = start_pos + BLK_LEN - 1;

			fm_rds_change_state(radio, (min_blk + 3) % (RDS_NUM_BLOCK_TYPES - 1));
			seq_lock = false;
			remains = true;


		case RDS_STATE_FOUND_BL_A:
		case RDS_STATE_FOUND_BL_B:
		case RDS_STATE_FOUND_BL_C:
		case RDS_STATE_FOUND_BL_D:
			if ((end_pos / BUF_LEN != (end_pos + BLK_LEN) / BUF_LEN) && !fetch_data && !remains) {
				fetch_data = true;
			} else {
				if (end_pos + BLK_LEN >= 160 && remains) {
					remains = false;
					fetch_data = true;
					break;
				}

				start_pos += BLK_LEN;
				end_pos += BLK_LEN;

				data = get_block_data(fifo, start_pos);
				fetch_data = false;

				i = (fm_rds_get_state(radio) + 1) % (RDS_NUM_BLOCK_TYPES - 1);
				fm_rds_change_state(radio, i);
				err_cnt = find_code_word(&data, i, seq_lock);

				if (fm_rds_process_block(radio, data >> 10, i, err_cnt) == HOST_RDS_ERRS_UNCORR) {
					fm_rds_update_error_status(radio, radio->low->fm_state.rds_unsync_uncorr_weight);
				} else {
					fm_rds_update_error_status(radio, err_cnt);
				}
			}
			break;

		default:
			break;
		}
	}
}
#endif	/*USE_RDS_HW_DECODER*/
