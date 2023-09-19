/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * lxs_ts_prd.c
 *
 * LXS touch raw-data debugging
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "lxs_ts_dev.h"
#include "lxs_ts_prd.h"

static u32 t_prd_dbg_flag;

/* usage
 * (1) echo <value> > /sys/module/{Siw Touch Module Name}/parameters/s_prd_dbg_flag
 * (2) insmod {Siw Touch Module Name}.ko s_prd_dbg_flag=<value>
 */
module_param_named(s_prd_dbg_flag, t_prd_dbg_flag, int, S_IRUGO|S_IWUSR|S_IWGRP);

static u32 t_prd_dbg_mask;

/* usage
 * (1) echo <value> > /sys/module/{Siw Touch Module Name}/parameters/s_prd_dbg_mask
 * (2) insmod {Siw Touch Module Name}.ko s_prd_dbg_mask=<value>
 */
module_param_named(s_prd_dbg_mask, t_prd_dbg_mask, int, S_IRUGO|S_IWUSR|S_IWGRP);

static struct lxs_prd_data *__ts_get_prd(struct lxs_ts_data *ts)
{
	return ts->prd;
}

static void __ts_set_prd(struct lxs_ts_data *ts, struct lxs_prd_data *prd)
{
	ts->prd = prd;
}

static int __get_digit_range_u16(uint16_t *buf, int size)
{
	int min = RAWDATA_MAX_DIGIT6;
	int max = RAWDATA_MIN_DIGIT6;
	int num;

	while (size--) {
		num = *buf++;

		if (num < min)
			min = num;

		if (num > max)
			max = num;
	}

	if ((max > RAWDATA_MAX_DIGIT5) || (min < RAWDATA_MIN_DIGIT5))
		return 6;

	if ((max > RAWDATA_MAX_DIGIT4) || (min < RAWDATA_MIN_DIGIT4))
		return 5;

	return DIGIT_RANGE_BASE;
}

static int __get_digit_range_s16(int16_t *buf, int size)
{
	int min = RAWDATA_MAX_DIGIT6;
	int max = RAWDATA_MIN_DIGIT6;
	int num;

	while (size--) {
		num = *buf++;

		if (num < min)
			min = num;

		if (num > max)
			max = num;
	}

	if ((max > RAWDATA_MAX_DIGIT5) || (min < RAWDATA_MIN_DIGIT5))
		return 6;

	if ((max > RAWDATA_MAX_DIGIT4) || (min < RAWDATA_MIN_DIGIT4))
		return 5;

	return DIGIT_RANGE_BASE;
}

static int __get_digit_range(void *buf, int size, int type)
{
	if (buf == NULL)
		return DIGIT_RANGE_BASE;

	if (type)
		return __get_digit_range_u16(buf, size);

	return __get_digit_range_s16(buf, size);
}

static int prd_check_exception(struct lxs_prd_data *prd)
{
	struct lxs_ts_data *ts = prd->ts;

	if (atomic_read(&ts->boot) == TC_IC_BOOT_FAIL) {
		t_prd_warn(prd, "%s\n", "boot failed");
		return 1;
	}

	if (atomic_read(&ts->init) != TC_IC_INIT_DONE) {
		t_prd_warn(prd, "%s\n", "not ready, need IC init");
		return 2;
	}

	if (is_ts_power_state_off(ts)) {
		t_prd_warn(prd, "%s\n", "Touch is stopped");
		return 3;
	}

	return 0;
}

static void __used prd_chip_reset(struct lxs_prd_data *prd)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;

	lxs_ts_ic_reset_sync(ts);
}

static int __used prd_chip_info(struct lxs_prd_data *prd)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;

	return lxs_ts_ic_info(ts);
}

static int __used prd_chip_driving(struct lxs_prd_data *prd, int mode)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;

	return lxs_ts_driving(ts, mode);
}

static int prd_stop_firmware(struct lxs_prd_data *prd, struct lxs_prd_raw_ctrl *ctrl)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;
	int is_fw_skip = !!(ctrl->flag & PRD_RAW_FLAG_FW_SKIP);
	int is_type_sys = !!(ctrl->flag & PRD_RAW_FLAG_TYPE_SYS);
	u32 addr = (is_type_sys) ? prd->sys_cmd : PRD_IC_START_REG;
	u32 cmd = ctrl->cmd;
	u32 read_val;
	u32 check_data = 0;
	int try_cnt = 0;
	int ret = 0;

	if (is_fw_skip)
		return 0;

	if (lxs_addr_is_invalid(addr)) {
		if (is_type_sys) {
			t_prd_err(prd, "sys_cmd is invalid, %X\n", addr);
			return -EINVAL;
		}
		return 0;
	}

	/*
	 * STOP F/W to check
	 */
	ret = lxs_ts_write_value(ts, addr, cmd);
	if (ret < 0)
		goto out;

	ret = lxs_ts_read_value(ts, addr, &check_data);
	if (ret < 0)
		goto out;

	if (!prd->app_read)
		t_prd_info(prd, "check_data : %Xh\n", check_data);

	if (ctrl->flag & PRD_RAW_FLAG_FW_NO_STS) {
		lxs_ts_delay(10);
		return 0;
	}

	addr = PRD_IC_READYSTATUS;
	if (lxs_addr_is_invalid(addr))
		return 0;

	try_cnt = 1000;
	do {
		--try_cnt;
		if (try_cnt == 0) {
			t_prd_err(prd, "[ERR] stop FW: timeout, %08Xh\n", read_val);
			ret = -ETIMEDOUT;
			goto out;
		}
		lxs_ts_read_value(ts, addr, &read_val);

		if (!prd->app_read)
			t_prd_dbg_base(prd, "Read RS_IMAGE = [%x] , OK RS_IMAGE = [%x]\n",
				read_val, (u32)RS_IMAGE);

		lxs_ts_delay(2);
	} while (read_val != (u32)RS_IMAGE);

out:
	return ret;
}

static int prd_start_firmware(struct lxs_prd_data *prd, struct lxs_prd_raw_ctrl *ctrl)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;
	u32 const cmd = PRD_CMD_NONE;
	int is_fw_skip = !!(ctrl->flag & PRD_RAW_FLAG_FW_SKIP);
	int is_type_sys = !!(ctrl->flag & PRD_RAW_FLAG_TYPE_SYS);
	u32 addr = (is_type_sys) ? prd->sys_cmd : PRD_IC_START_REG;
	u32 check_data = 0;
	u32 read_val = 0;
	int ret = 0;

	if (is_fw_skip)
		return 0;

	if (lxs_addr_is_invalid(addr)) {
		if (is_type_sys) {
			t_prd_err(prd, "sys_cmd is invalid, %X\n", addr);
			return -EINVAL;
		}
		return 0;
	}

	/* Release F/W to operate */
	ret = lxs_ts_write_value(ts, addr, cmd);
	if (ret < 0)
		goto out;

	ret = lxs_ts_read_value(ts, addr, &check_data);
	if (ret < 0)
		goto out;

	if (!prd->app_read)
		t_prd_dbg_base(prd, "check_data : %Xh\n", check_data);

	if (ctrl->flag & PRD_RAW_FLAG_FW_NO_STS) {
		lxs_ts_delay(10);
		return 0;
	}

	addr = PRD_IC_READYSTATUS;
	if (lxs_addr_is_invalid(addr))
		return 0;

	//for test
	ret = lxs_ts_read_value(ts, addr, &read_val);
	if (ret < 0)
		goto out;

	t_prd_dbg_base(prd, "Read RS_IMAGE = [%x]\n", read_val);

out:
	return ret;
}

static void __used __buff_16to8(struct lxs_prd_data *prd, void *dst, void *src, int size)
{
	u16 *tbuf16 = NULL;
	u16 *sbuf16 = src;
	s8 *dbuf8 = dst;
	int i;

	if ((dst == NULL) || (src == NULL)) {
		t_prd_err(prd, "%s: invalid buffer: dst %p, src %p\n", __func__, dst, src);
		return;
	}

	if (dst == src) {
		tbuf16 = kzalloc(size<<1, GFP_KERNEL);
		if (tbuf16 == NULL) {
			t_prd_err(prd, "%s: failed to allocate tbuf\n", __func__);
			return;
		}

		memcpy(tbuf16, sbuf16, size<<1);
		sbuf16 = tbuf16;
	}

	for (i = 0 ; i < size; i++)
		*dbuf8++ = *sbuf16++;

	if (tbuf16 != NULL)
		kfree(tbuf16);
}

static void __used __buff_8to16(struct lxs_prd_data *prd, void *dst, void *src, int size)
{
	s8 *tbuf8 = NULL;
	s8 *sbuf8 = src;
	u16 *dbuf16 = dst;
	int i;

	if ((dst == NULL) || (src == NULL)) {
		t_prd_err(prd, "%s: invalid buffer: dst %p, src %p\n", __func__, dst, src);
		return;
	}

	if (dst == src) {
		tbuf8 = kzalloc(size, GFP_KERNEL);
		if (tbuf8 == NULL) {
			t_prd_err(prd, "%s: failed to allocate tbuf\n", __func__);
			return;
		}

		memcpy(tbuf8, sbuf8, size);
		sbuf8 = tbuf8;
	}

	for (i = 0 ; i < size; i++)
		*dbuf16++ = *sbuf8++;

	if (tbuf8 != NULL)
		kfree(tbuf8);
}

/*
 * @reverse
 * 0 : for 'Left-Top to Left-Bottom'
 * 1 : for 'Right-Bottom to Right-Top'
 * 2 : for 'Left-Bottom to Left-Top'
 * 3 : for 'Right-Top to Right-Bottom'
 */
static void __used __buff_swap(struct lxs_prd_data *prd, void *dst, void *src,
						int row, int col, int data_fmt, int reverse)
{
	u8 *tbuf8 = NULL;
	u16 *tbuf16 = NULL;
	u8 *dst8, *src8;
	u16 *dst16, *src16;
	int is_16bit = !!(data_fmt & PRD_DATA_FMT_MASK);
	int size;
	int r, c;
	int i;

	if ((dst == NULL) || (src == NULL)) {
		t_prd_err(prd, "%s: invalid buffer: dst %p, src %p\n", __func__, dst, src);
		return;
	}

	size = (row * col);

	if (is_16bit) {
		dst16 = (reverse & 0x1) ? &((u16 *)dst)[size - 1] : (u16 *)dst;
		src16 = (u16 *)src;

		if (dst == src) {
			tbuf16 = kzalloc(size<<1, GFP_KERNEL);
			if (tbuf16 == NULL) {
				t_prd_err(prd, "%s: failed to allocate tbuf6\n", __func__);
				return;
			}

			memcpy(tbuf16, src16, size<<1);
			src16 = tbuf16;
		}

		switch (reverse) {
		case 3:
			for (i = 0; i < size; i++) {
				r = i / col;
				c = i % col;
				*dst16-- = src16[(c * row) + (row - r - 1)];
			}
			break;
		case 2:
			for (i = 0; i < size; i++) {
				r = i / col;
				c = i % col;
				*dst16++ = src16[(c * row) + (row - r - 1)];
			}
			break;
		case 1:
			for (i = 0; i < size; i++) {
				r = i / col;
				c = i % col;
				*dst16-- = src16[r + (row * c)];
			}
			break;
		default:
			for (i = 0; i < size; i++) {
				r = i / col;
				c = i % col;
				*dst16++ = src16[r + (row * c)];
			}
			break;
		}

		if (tbuf16 != NULL) {
			kfree(tbuf16);
		}
	} else {
		dst8 = (reverse & 0x1) ? &((u8 *)dst)[size - 1] : (u8 *)dst;
		src8 = (u8 *)src;

		if (dst == src) {
			tbuf8 = kzalloc(size, GFP_KERNEL);
			if (tbuf8 == NULL) {
				t_prd_err(prd, "%s: failed to allocate tbuf8\n", __func__);
				return;
			}

			memcpy(tbuf8, src8, size);
			src8 = tbuf8;
		}

		switch (reverse) {
		case 3:
			for (i = 0; i < size; i++) {
				r = i / col;
				c = i % col;
				*dst8-- = src8[(c * row) + (row - r - 1)];
			}
			break;
		case 2:
			for (i = 0; i < size; i++) {
				r = i / col;
				c = i % col;
				*dst8++ = src8[(c * row) + (row - r - 1)];
			}
			break;
		case 1:
			for (i = 0; i < size; i++) {
				r = i / col;
				c = i % col;
				*dst8-- = src8[r + (row * c)];
			}
			break;
		default:
			for (i = 0; i < size; i++) {
				r = i / col;
				c = i % col;
				*dst8++ = src8[r + (row * c)];
			}
			break;
		}

		if (tbuf8 != NULL) {
			kfree(tbuf8);
		}
	}
}

static int __cal_data_size(u32 row_size, u32 col_size, u32 data_fmt)
{
	int data_size = 0;

	if (data_fmt & PRD_DATA_FMT_RXTX)
		data_size = row_size + col_size;
	else
		data_size = row_size * col_size;

	if (data_fmt & PRD_DATA_FMT_SELF)
		data_size += (row_size + col_size);

	if (data_fmt & PRD_DATA_FMT_MASK)	//if 16bit
		data_size <<= 1;

	return data_size;
}

static int prd_read_memory(struct lxs_prd_data *prd, u32 offset, char *buf, int size)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;
	int ret = 0;

	if (!offset) {
		t_prd_err(prd, "%s\n", "raw memory failed: zero offset");
		ret = -EFAULT;
		goto out;
	}

	if (!size) {
		t_prd_err(prd, "%s\n", "raw memory failed: zero size");
		ret = -EFAULT;
		goto out;
	}

	if (buf == NULL) {
		t_prd_err(prd, "%s\n", "raw memory failed: NULL buf");
		ret = -EFAULT;
		goto out;
	}

	//offset write
	ret = lxs_ts_write_value(ts, SERIAL_DATA_OFFSET, offset);
	if (ret < 0)
		goto out;

	//read raw data
	ret = lxs_ts_reg_read(ts, DATA_BASE_ADDR, buf, size);
	if (ret < 0)
		goto out;

out:
	return ret;
}

static void __used prd_cal_rawdata_gap_x(struct lxs_prd_data *prd, char *dst, char *src,
			int row_size, int col_size, int data_fmt, u32 flag)
{
	uint16_t *dst_u16 = (uint16_t *)dst;
	int16_t *dst_s16 = (int16_t *)dst;
	s8 *dst_s8 = (s8 *)dst;
	uint16_t *src_u16 = (uint16_t *)src;
	int16_t *src_s16 = (int16_t *)src;
	s8 *src_s8 = (s8 *)src;
//	int is_self = !!(data_fmt & PRD_DATA_FMT_SELF);
	int is_16bit = !!(data_fmt & PRD_DATA_FMT_MASK);
	int is_u16bit = !!(data_fmt & PRD_DATA_FMT_U16);
	int is_ign_edge_t = !!(flag & PRD_RAW_FLAG_IGN_EDGE_T);
	int is_ign_edge_b = !!(flag & PRD_RAW_FLAG_IGN_EDGE_B);
	int offset, curr_raw;
	int i, j;

	if (dst == NULL) {
		t_prd_err(prd, "%s: dst is null\n", __func__);
		return;
	}

	if (src == NULL) {
		t_prd_err(prd, "%s: src is null\n", __func__);
		return;
	}

	memset(dst, 0, (row_size * col_size)<<1);

	for (j = 0; j < row_size; j++) {
		for (i = 1; i < (col_size>>1); i++) {
			offset = (j * col_size) + i;
			if (is_16bit) {
				if (is_u16bit) {
					curr_raw = src_u16[offset] - src_u16[offset-1];
					dst_u16[offset] = (int)((curr_raw * 100)/src_u16[offset]);
				} else {
					curr_raw = src_s16[offset] - src_s16[offset-1];
					dst_s16[offset] = (int)((curr_raw * 100)/src_s16[offset]);
				}
			} else {
				curr_raw = src_s8[offset] - src_s8[offset-1];
				dst_s8[offset] = (int)((curr_raw * 100)/src_s8[offset]);
			}
		}

		for (; i < (col_size - 1); i++) {
			offset = (j * col_size) + i;
			if (is_16bit) {
				if (is_u16bit) {
					curr_raw = src_u16[offset+1] - src_u16[offset];
					dst_u16[offset] = (int)((curr_raw * 100)/src_u16[offset]);
				} else {
					curr_raw = src_s16[offset+1] - src_s16[offset];
					dst_s16[offset] = (int)((curr_raw * 100)/src_s16[offset]);
				}
			} else {
				curr_raw = src_s8[offset+1] - src_s8[offset];
				dst_s8[offset] = (int)((curr_raw * 100)/src_s8[offset]);
			}
		}
	}

	if (is_ign_edge_t) {
		if (is_16bit) {
			if (is_u16bit) {
				dst_u16[1] = 0;
				dst_u16[col_size-2] = 0;
			} else {
				dst_s16[1] = 0;
				dst_s16[col_size-2] = 0;
			}
		} else {
			dst_s8[1] = 0;
			dst_s8[col_size-2] = 0;
		}
	}

	if (is_ign_edge_b) {
		offset = (row_size - 1) * col_size;
		if (is_16bit) {
			if (is_u16bit) {
				dst_u16[offset+1] = 0;
				dst_u16[offset+col_size-2] = 0;
			} else {
				dst_s16[offset+1] = 0;
				dst_s16[offset+col_size-2] = 0;
			}
		} else {
			dst_s8[offset+1] = 0;
			dst_s8[offset+col_size-2] = 0;
		}
	}
}

static void __used prd_cal_rawdata_gap_y(struct lxs_prd_data *prd, char *dst, char *src,
			int row_size, int col_size, int data_fmt, u32 flag)
{
	uint16_t *dst_u16 = (uint16_t *)dst;
	int16_t *dst_s16 = (int16_t *)dst;
	s8 *dst_s8 = (s8 *)dst;
	uint16_t *src_u16 = (uint16_t *)src;
	int16_t *src_s16 = (int16_t *)src;
	s8 *src_s8 = (s8 *)src;
//	int is_self = !!(data_fmt & PRD_DATA_FMT_SELF);
	int is_16bit = !!(data_fmt & PRD_DATA_FMT_MASK);
	int is_u16bit = !!(data_fmt & PRD_DATA_FMT_U16);
	int is_ign_edge_t = !!(flag & PRD_RAW_FLAG_IGN_EDGE_T);
	int is_ign_edge_b = !!(flag & PRD_RAW_FLAG_IGN_EDGE_B);
	int offset, offset2, curr_raw;
	int i, j;

	if (dst == NULL) {
		t_prd_err(prd, "%s: dst is null\n", __func__);
		return;
	}

	if (src == NULL) {
		t_prd_err(prd, "%s: src is null\n", __func__);
		return;
	}

	memset(dst, 0, (row_size * col_size)<<1);

	for (j = 0; j < (row_size-1); j++) {
		for (i = 0; i < col_size; i++) {
			offset = (j * col_size) + i;
			offset2 = ((j+1) * col_size) + i;
			if (is_16bit) {
				if (is_u16bit) {
					curr_raw = src_u16[offset] - src_u16[offset2];
					dst_u16[offset] = (int)((curr_raw * 100)/src_u16[offset2]);
				} else {
					curr_raw = src_s16[offset] - src_s16[offset2];
					dst_s16[offset] = (int)((curr_raw * 100)/src_s16[offset2]);
				}
			} else {
				curr_raw = src_s8[offset] - src_s8[offset2];
				dst_s8[offset] = (int)((curr_raw * 100)/src_s8[offset2]);
			}
		}
	}

	if (is_ign_edge_t) {
		if (is_16bit) {
			if (is_u16bit) {
				dst_u16[0] = 0;
				dst_u16[col_size-1] = 0;
			} else {
				dst_s16[0] = 0;
				dst_s16[col_size-1] = 0;
			}
		} else {
			dst_s8[0] = 0;
			dst_s8[col_size-1] = 0;
		}
	}

	if (is_ign_edge_b) {
		offset = (row_size - 2) * col_size;
		if (is_16bit) {
			if (is_u16bit) {
				dst_u16[offset] = 0;
				dst_u16[offset+col_size-1] = 0;
			} else {
				dst_s16[offset] = 0;
				dst_s16[offset+col_size-1] = 0;
			}
		} else {
			dst_s8[offset] = 0;
			dst_s8[offset+col_size-1] = 0;
		}
	}
}

static int prd_print_pre(struct lxs_prd_data *prd, char *prt_buf,
				int prt_size, int row_size, int col_size,
				char *name, int digit_range, int is_self)
{
	char *log_line = prd->log_line;
	const char *raw_fmt_col_pre = NULL;
	const char *raw_fmt_col_no = NULL;
	int log_size = 0;
	int i;

	if (is_self) {
		switch (digit_range) {
		case 6:
			raw_fmt_col_pre = "   :   [SF]   ";
			raw_fmt_col_no = "[%2d]   ";
			break;
		case 5:
			raw_fmt_col_pre = "   :  [SF]  ";
			raw_fmt_col_no = "[%2d]  ";
			break;
		default:
			raw_fmt_col_pre = "   : [SF] ";
			raw_fmt_col_no = "[%2d] ";
			break;
		}
	} else {
		switch (digit_range) {
		case 6:
			raw_fmt_col_pre = "   :   ";
			raw_fmt_col_no = "[%2d]   ";
			break;
		case 5:
			raw_fmt_col_pre = "   :  ";
			raw_fmt_col_no = "[%2d]  ";
			break;
		default:
			raw_fmt_col_pre = "   : ";
			raw_fmt_col_no = "[%2d] ";
			break;
		}
	}

	log_size += lxs_prd_log_buf_snprintf(log_line, log_size,
					"-------- %s(%d %d) --------",
					name, row_size, col_size);

	prt_size += lxs_prd_log_flush(prd, log_line, prt_buf, log_size, prt_size);

	log_size = 0;
	log_size += lxs_prd_log_buf_snprintf(log_line, log_size, "%s", raw_fmt_col_pre);

	for (i = 0; i < col_size; i++)
		log_size += lxs_prd_log_buf_snprintf(log_line, log_size, raw_fmt_col_no, i);

	prt_size += lxs_prd_log_flush(prd, log_line, prt_buf, log_size, prt_size);

	return prt_size;
}

static int prd_print_min_max(struct lxs_prd_data *prd, char *prt_buf,
				int prt_size, int min, int max, char *name)
{
	prt_size += lxs_prd_buf_snprintf(prt_buf, prt_size, "\n");

	prt_size += lxs_prd_buf_snprintf(prt_buf, prt_size,
				"%s min : %d , max : %d\n",
				name, min, max);

	t_prd_info(prd, "%s min : %d , max : %d\n", name, min, max);

	return prt_size;
}

/*
 * Default - Row : Tx, Col : Rx
 *
 * [Non-Self format]
 *    : [ 0] [ 1] [ 2] ... [cc]
 * [ 0] xxxx xxxx xxxx ... xxxx
 * [ 1] xxxx xxxx xxxx ... xxxx
 * ...
 * [rr] xxxx xxxx xxxx ... xxxx
 *
 *
 *
 * [Self format]
 *	  : [SF] [ 0] [ 1] [ 2] ... [cc]
 * [SF]      xxxx xxxx xxxx ... xxxx
 * [ 0] xxxx xxxx xxxx xxxx ... xxxx
 * [ 1] xxxx xxxx xxxx xxxx ... xxxx
 * ...
 * [rr] xxxx xxxx xxxx xxxx ... xxxx
 *
 * cc : col_size - 1
 * rr : row_size - 1
 * SF : SELF line
 *
 */
static int __used prd_print_rawdata(struct lxs_prd_data *prd, char *prt_buf,
			int prt_size, char *raw_buf, int row_size, int col_size,
			int data_fmt, u32 flag, char *name, int post)
{
	const char *raw_fmt_row_no = NULL;
	const char *raw_fmt_empty = NULL;
	const char *raw_fmt = NULL;
	char *log_line = prd->log_line;
	void *rawdata_buf = (raw_buf) ? raw_buf : prd->raw_buf;
	uint16_t *rawdata_u16 = (uint16_t *)rawdata_buf;
	int16_t *rawdata_s16 = (int16_t *)rawdata_buf;
	s8 *rawdata_s8 = (s8 *)rawdata_buf;
	uint16_t *buf_self_u16 = NULL;
	int16_t *buf_self_s16 = NULL;
	s8 *buf_self_s8 = NULL;
	int is_self = !!(data_fmt & PRD_DATA_FMT_SELF);
	int is_16bit = !!(data_fmt & PRD_DATA_FMT_MASK);
	int is_u16bit = !!(data_fmt & PRD_DATA_FMT_U16);
	int is_self_misalign = !!(data_fmt & PRD_DATA_FMT_SELF_MISALIGN);
	int is_self_rev_c = !!(data_fmt & PRD_DATA_FMT_SELF_REV_C);
	int is_self_rev_r = !!(data_fmt & PRD_DATA_FMT_SELF_REV_R);
	int is_ign_edge_t = !!(flag & PRD_RAW_FLAG_IGN_EDGE_T);
	int is_ign_edge_b = !!(flag & PRD_RAW_FLAG_IGN_EDGE_B);
	int digit_range = DIGIT_RANGE_BASE;
	int digit_width_self = DIGIT_RANGE_BASE;
	int self_offset_col = 0;
	int self_offset_row = col_size;
	int self_step_col = 1;
	int self_step_row = 1;
	int min_raw = PRD_RAWDATA_MAX;
	int max_raw = PRD_RAWDATA_MIN;
	int log_size = 0;
	int curr_raw;
	int i, j;

	if (is_self) {
		buf_self_u16 = &rawdata_u16[row_size * col_size];
		buf_self_s16 = &rawdata_s16[row_size * col_size];
		buf_self_s8 = &rawdata_s8[row_size * col_size];

		if (is_self_misalign) {
			self_offset_col = row_size;
			self_offset_row = 0;
		}

		if (is_self_rev_c) {
			self_offset_col += (col_size - 1);
			self_step_col = -1;
		}

		if (is_self_rev_r) {
			self_offset_row += (row_size - 1);
			self_step_row = -1;
		}
	}

	if (is_16bit) {
		digit_range = __get_digit_range(rawdata_buf, row_size * col_size, is_u16bit);

		if (is_self) {
			digit_width_self = __get_digit_range((is_u16bit) ? (void *)buf_self_u16 : (void *)buf_self_s16, row_size + col_size, is_u16bit);
			if (digit_width_self > digit_range)
				digit_range = digit_width_self;
		}
	}

	raw_fmt_row_no = "[%2d] ";

	switch (digit_range) {
	case 6:
		raw_fmt_empty = "       ";
		raw_fmt = "%6d ";
		break;
	case 5:
		raw_fmt_empty = "      ";
		raw_fmt = "%5d ";
		break;
	default:
		raw_fmt_empty = "     ";
		raw_fmt = "%4d ";
		break;
	}

	prt_size = prd_print_pre(prd, prt_buf, prt_size, row_size, col_size, name, digit_range, is_self);

	if (is_self) {
		/* self data - horizental line */
		log_size = 0;
		memset(log_line, 0, sizeof(prd->log_line));

		log_size += lxs_prd_log_buf_snprintf(log_line, log_size, "[SF] %s", raw_fmt_empty);

		for (i = 0; i < col_size; i++) {
			if (is_16bit)
				curr_raw = (is_u16bit) ? buf_self_u16[self_offset_col] : buf_self_s16[self_offset_col];
			else
				curr_raw = buf_self_s8[self_offset_col];

			self_offset_col += self_step_col;

			log_size += lxs_prd_log_buf_snprintf(log_line, log_size, raw_fmt, curr_raw);
		}

		prt_size += lxs_prd_log_flush(prd, log_line, prt_buf, log_size, prt_size);
	}

	for (i = 0; i < row_size; i++) {
		log_size = 0;
		memset(log_line, 0, sizeof(prd->log_line));
		log_size += lxs_prd_log_buf_snprintf(log_line, log_size, raw_fmt_row_no, i);

		if (is_self) {
			/* self data : vertical line */
			if (is_16bit)
				curr_raw = (is_u16bit) ? buf_self_u16[self_offset_row] : buf_self_s16[self_offset_row];
			else
				curr_raw = buf_self_s8[self_offset_row];

			self_offset_row += self_step_row;

			log_size += lxs_prd_log_buf_snprintf(log_line, log_size, raw_fmt, curr_raw);
		}

		for (j = 0; j < col_size; j++) {
			/* rawdata */
			if (is_16bit)
				curr_raw = (is_u16bit) ? *rawdata_u16++ : *rawdata_s16++;
			else
				curr_raw = *rawdata_s8++;

			log_size += lxs_prd_log_buf_snprintf(log_line, log_size, raw_fmt, curr_raw);

			if (is_ign_edge_t && !i)
				if (!j || (j == (col_size-1)))
					continue;

			if (is_ign_edge_b && (i == (row_size - 1)))
				if (!j || (j == (col_size-1)))
					continue;

			if (curr_raw < min_raw)
				min_raw = curr_raw;

			if (curr_raw > max_raw)
				max_raw = curr_raw;
		}

		prt_size += lxs_prd_log_flush(prd, log_line, prt_buf, log_size, prt_size);
	}

	if (post)
		prt_size = prd_print_min_max(prd, prt_buf, prt_size, min_raw, max_raw, name);

	return prt_size;
}

static int prd_print_data_pre(struct lxs_prd_data *prd, char *prt_buf,
				int prt_size, int row_size, int col_size,
				char *name, int digit_range, int is_data)
{
	char *log_line = prd->log_line;
	const char *raw_fmt_col_pre = NULL;
	const char *raw_fmt = NULL;
	int log_size = 0;
	int c_size = (is_data) ? col_size : max(row_size, col_size);
	int i;

	switch (digit_range) {
	case 6:
		raw_fmt_col_pre = "     :   ";
		raw_fmt = "[%2d]   ";
		break;
	case 5:
		raw_fmt_col_pre = "     :  ";
		raw_fmt = "[%2d]  ";
		break;
	default:
		raw_fmt_col_pre = "     : ";
		raw_fmt = "[%2d] ";
		break;
	}

	log_size += lxs_prd_log_buf_snprintf(log_line, log_size,
					"-------- %s(%d %d) --------",
					name, row_size, col_size);

	prt_size += lxs_prd_log_flush(prd, log_line, prt_buf, log_size, prt_size);

	log_size = 0;
	if (is_data)
		log_size += lxs_prd_log_buf_snprintf(log_line, log_size, "  ");

	log_size += lxs_prd_log_buf_snprintf(log_line, log_size, "%s", raw_fmt_col_pre);

	for (i = 0; i < c_size; i++)
		log_size += lxs_prd_log_buf_snprintf(log_line, log_size, raw_fmt, i);

	prt_size += lxs_prd_log_flush(prd, log_line, prt_buf, log_size, prt_size);

	return prt_size;
}

/*

* Base - Row : Tx, Col : Rx, Rx first
 *
 *      : [ 0] [ 1] [ 2] ... [cc]
 * Rx[rr] xxxx xxxx xxxx ... xxxx //col_size
 * Tx[tt] xxxx xxxx xxxx ...      //row_size
 *
 * if is_self_tx_first,
 *      : [ 0] [ 1] [ 2] ... [cc]
 * Tx[tt] xxxx xxxx xxxx ...      //row_size
 * Rx[rr] xxxx xxxx xxxx ...      //col_size
 *
 */
static int __used prd_print_data_rxtx(struct lxs_prd_data *prd, char *prt_buf,
			int prt_size, char *raw_buf, int row_size, int col_size,
			int data_fmt, u32 flag, char *name)
{
	const char *raw_fmt = NULL;
	char *log_line = prd->log_line;
	void *rawdata_buf = (raw_buf) ? raw_buf : prd->raw_buf;
	uint16_t *rawdata_u16 = (uint16_t *)rawdata_buf;
	int16_t *rawdata_s16 = (int16_t *)rawdata_buf;
	s8 *rawdata_s8 = (s8 *)rawdata_buf;
	char *name1 = NULL;
	char *name2 = NULL;
	int is_16bit = !!(data_fmt & PRD_DATA_FMT_MASK);
	int is_u16bit = !!(data_fmt & PRD_DATA_FMT_U16);
	int is_self_tx_first = !!(data_fmt & PRD_DATA_FMT_SELF_TX_FIRST);
	int digit_range = DIGIT_RANGE_BASE;
	int line1_size = col_size;
	int line2_size = row_size;
	int log_size = 0;
	int curr_raw;
	int j;

	name1 = (is_self_tx_first) ? "Tx" : "Rx";
	name2 = (is_self_tx_first) ? "Rx" : "Tx";

	/*
	 * row/col swap
	 */
	if (is_self_tx_first) {
		line1_size = row_size;
		line2_size = col_size;
	}

	if (is_16bit)
		digit_range = __get_digit_range(rawdata_s16, row_size + col_size, is_u16bit);

	switch (digit_range) {
	case 6:
		raw_fmt = "%6d ";
		break;
	case 5:
		raw_fmt = "%5d ";
		break;
	default:
		raw_fmt = "%4d ";
		break;
	}

	prt_size = prd_print_data_pre(prd, prt_buf, prt_size, row_size, col_size, name, digit_range, 0);

	log_size = 0;
	memset(log_line, 0, sizeof(prd->log_line));

	log_size += lxs_prd_log_buf_snprintf(log_line, log_size, "%s[%2d] ", name1, line1_size);

	for (j = 0; j < line1_size; j++) {
		if (is_16bit)
			curr_raw = (is_u16bit) ? *rawdata_u16++ : *rawdata_s16++;
		else
			curr_raw = *rawdata_s8++;

		log_size += lxs_prd_log_buf_snprintf(log_line, log_size, raw_fmt, curr_raw);
	}

	prt_size += lxs_prd_log_flush(prd, log_line, prt_buf, log_size, prt_size);

	log_size = 0;
	memset(log_line, 0, sizeof(prd->log_line));

	log_size += lxs_prd_log_buf_snprintf(log_line, log_size, "%s[%2d] ", name2, line2_size);

	for (j = 0; j < line2_size; j++) {
		if (is_16bit)
			curr_raw = (is_u16bit) ? *rawdata_u16++ : *rawdata_s16++;
		else
			curr_raw = *rawdata_s8++;

		log_size += lxs_prd_log_buf_snprintf(log_line, log_size, raw_fmt, curr_raw);
	}

	prt_size += lxs_prd_log_flush(prd, log_line, prt_buf, log_size, prt_size);

	log_size = 0;
	memset(log_line, 0, sizeof(prd->log_line));

	return prt_size;
}

/*
 *        : [ 0] [ 1] [ 2] ... [cc]
 * Data[ 0] xxxx xxxx xxxx ... xxxx
 * Data[rr] xxxx xxxx xxxx ... xxxx
 *
 * cc : col_size - 1
 * rr : col_size
 *
 */
static int __used prd_print_data_raw(struct lxs_prd_data *prd, char *prt_buf,
			int prt_size, char *raw_buf, int row_size, int col_size,
			int data_fmt, u32 flag, char *name)
{
	const char *raw_fmt = NULL;
	char *log_line = prd->log_line;
	void *rawdata_buf = (raw_buf) ? raw_buf : prd->raw_buf;
	uint16_t *rawdata_u16 = (uint16_t *)rawdata_buf;
	int16_t *rawdata_s16 = (int16_t *)rawdata_buf;
	s8 *rawdata_s8 = (s8 *)rawdata_buf;
	int is_16bit = !!(data_fmt & PRD_DATA_FMT_MASK);
	int is_u16bit = !!(data_fmt & PRD_DATA_FMT_U16);
	int digit_range = DIGIT_RANGE_BASE;
	int log_size = 0;
	int curr_raw;
	int offset;
	int i, j;

	if (is_16bit)
		digit_range = __get_digit_range(rawdata_s16, row_size + col_size, is_u16bit);

	switch (digit_range) {
	case 6:
		raw_fmt = "%6d ";
		break;
	case 5:
		raw_fmt = "%5d ";
		break;
	default:
		raw_fmt = "%4d ";
		break;
	}

	prt_size = prd_print_data_pre(prd, prt_buf, prt_size, row_size, col_size, name, digit_range, 1);

	offset = 0;
	for (i = 0; i < row_size; i++) {
		log_size = 0;
		memset(log_line, 0, sizeof(prd->log_line));

		log_size += lxs_prd_log_buf_snprintf(log_line,
						log_size, "Data[%2d] ", offset);

		for (j = 0; j < col_size; j++) {
			if (is_16bit)
				curr_raw = (is_u16bit) ? *rawdata_u16++ : *rawdata_s16++;
			else
				curr_raw = *rawdata_s8++;

			log_size += lxs_prd_log_buf_snprintf(log_line, log_size, raw_fmt, curr_raw);
		}

		prt_size += lxs_prd_log_flush(prd, log_line, prt_buf, log_size, prt_size);

		offset += col_size;
	}

	return prt_size;
}

static int __used prd_prt_rawdata(void *prd_data, void *ctrl_data, int prt_size)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_raw_ctrl *ctrl = (struct lxs_prd_raw_ctrl *)ctrl_data;
	int ret;

	ret = prd_print_rawdata(prd, ctrl->prt_buf, prt_size,
			ctrl->raw_buf, ctrl->row_act, ctrl->col_act,
			ctrl->data_fmt, ctrl->flag, ctrl->name, 1);

	return ret;
}

static int prd_mod_raw_data_dim(struct lxs_prd_data *prd, struct lxs_prd_raw_ctrl *ctrl)
{
	if (!ctrl->is_dim)
		return 0;

	/* TBD */

	return 0;
}

/*
 * eliminate pad-area
 */
static int prd_mod_raw_data_pad(struct lxs_prd_data *prd, struct lxs_prd_raw_ctrl *ctrl)
{
	struct lxs_prd_raw_pad *pad = ctrl->pad;
	int16_t *rbuf_s16 = (int16_t *)ctrl->raw_buf;
	s8 *rbuf_s8 = (s8 *)ctrl->raw_buf;
	int16_t *tbuf_s16 = (int16_t *)ctrl->raw_tmp;
	s8 *tbuf_s8 = (s8 *)ctrl->raw_tmp;
	int is_16bit = !!(ctrl->data_fmt & PRD_DATA_FMT_MASK);
	int node_size = 0;
	int ctrl_col = 0;
	int off_row = 0;
	int off_col = 0;
	int pad_col = 0;
	int row, col;
	int i;

	if (!ctrl->is_pad)
		return 0;

	node_size = (ctrl->row_act * ctrl->col_act);
	ctrl_col = ctrl->col_act;

	pad = ctrl->pad;

	off_row = pad->row;
	off_col = pad->col;
	pad_col = (pad->col<<1);

	for (i = 0; i < node_size; i++) {
		row = i / ctrl_col;
		col = i % ctrl_col;
		if (is_16bit)
			rbuf_s16[i] = tbuf_s16[(row + off_row)*(ctrl_col + pad_col) + (col + off_col)];
		else
			rbuf_s8[i] = tbuf_s8[(row + off_row)*(ctrl_col + pad_col) + (col + off_col)];
	}

	return 0;
}

static int __used prd_mod_raw_data(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_raw_ctrl *ctrl = (struct lxs_prd_raw_ctrl *)ctrl_data;

	prd_mod_raw_data_pad(prd, ctrl);

	prd_mod_raw_data_dim(prd, ctrl);

	return 0;
}

static int __used prd_get_raw_data(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_raw_ctrl *ctrl = (struct lxs_prd_raw_ctrl *)ctrl_data;
	void *rbuf = ctrl->raw_buf;
	int data_size = ctrl->data_size;
	int ret = 0;

	if (ctrl->is_pad) {
		struct lxs_prd_raw_pad *pad = ctrl->pad;
		int row_size = ctrl->row_act + (pad->row<<1);
		int col_size = ctrl->col_act + (pad->col<<1);

		data_size = __cal_data_size(row_size, col_size, ctrl->data_fmt);

		rbuf = ctrl->raw_tmp;
	}

#if 0
	if (ctrl->is_dim)
		rbuf = ctrl->raw_tmp;
#endif

	ret = prd_read_memory(prd, ctrl->offset, rbuf, data_size);
	if (ret < 0)
		return ret;

	return 0;
}

#if defined(__PRD_SUPPORT_SD_TEST)
static int prd_write_sd_log(struct lxs_prd_data *prd, char *write_data, int write_time)
{
	return 0;
}

static void prd_fw_version_log(struct lxs_prd_data *prd)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;
	struct lxs_ts_fw_info *fw = &ts->fw;
	char *log_buf = prd->log_line;
	int ret = 0;

	memset(log_buf, 0, sizeof(prd->log_line));

	ret = snprintf(log_buf, PRD_LOG_LINE_SIZE,
				"======== Firmware Info ========\n");
	ret += snprintf(log_buf + ret, PRD_LOG_LINE_SIZE - ret,
				"LX%08X (%s, %d)\n",
				fw->ver_code_dev, fw->product_id, fw->revision);

	prd_write_sd_log(prd, log_buf, TIME_INFO_SKIP);
}

static int __prd_sd_pre(struct lxs_prd_data *prd, char *buf)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;
	int ret = 0;

	ret = lxs_ts_load_fw_from_mp_bin(ts);
	if (ret < 0)
		goto out;

out:
	return ret;
}

static void __prd_sd_end(struct lxs_prd_data *prd)
{
#if defined(USE_FW_MP)
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;

	atomic_set(&ts->fwsts, 0);
#endif
}

static int __prd_sd_post(struct lxs_prd_data *prd, char *prt_buf)
{
	t_prd_info(prd, "%s\n", "======== RESULT =======");
	t_prd_info(prd, "%s", prt_buf);

	prd_write_sd_log(prd, prt_buf, TIME_INFO_SKIP);

	__prd_sd_end(prd);

	return 0;
}

static int prd_mod_sd_data_dim(struct lxs_prd_data *prd, struct lxs_prd_sd_ctrl *sd_ctrl)
{
	if (!sd_ctrl->is_dim)
		return 0;

	/* TBD */

	return 0;
}

/*
 * eliminate pad-area
 */
static int prd_mod_sd_data_pad(struct lxs_prd_data *prd, struct lxs_prd_sd_ctrl *sd_ctrl)
{
	if (!sd_ctrl->is_pad)
		return 0;

	/* TBD */

	return 0;
}

static int __used prd_mod_sd_data(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)ctrl_data;

	prd_mod_sd_data_pad(prd, sd_ctrl);

	prd_mod_sd_data_dim(prd, sd_ctrl);

	return 0;
}

static int __used prd_get_sd_raw(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)ctrl_data;
	char *rbuf = sd_ctrl->raw_buf;
	int data_size = sd_ctrl->data_size;
	u32 offset = sd_ctrl->offset;
	int ret = 0;

	if (sd_ctrl->is_pad) {
		struct lxs_prd_raw_pad *pad = sd_ctrl->pad;
		int row_size = sd_ctrl->row_act + (pad->row<<1);
		int col_size = sd_ctrl->col_act + (pad->col<<1);

		data_size = __cal_data_size(row_size, col_size, sd_ctrl->data_fmt);

		rbuf = sd_ctrl->raw_tmp;
	}

#if 0
	if (sd_ctrl->is_dim)
		rbuf = sd_ctrl->raw_tmp;
#endif

	ret = prd_read_memory(prd, offset, rbuf, data_size);
	if (ret < 0)
		return ret;

	return 0;
}

static int __used prd_mod_sd_m1(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)ctrl_data;

	memcpy(sd_ctrl->raw_tmp, sd_ctrl->raw_buf, sd_ctrl->data_size);

	__buff_swap(prd, sd_ctrl->raw_buf, sd_ctrl->raw_tmp,
		sd_ctrl->row_act, sd_ctrl->col_act, sd_ctrl->data_fmt, 0);

	return 0;
}

static int __used prd_prt_sd_raw(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)ctrl_data;
	int size = 0;

	size = prd_print_rawdata(prd, sd_ctrl->prt_buf, 0,
				sd_ctrl->raw_buf, sd_ctrl->row_act, sd_ctrl->col_act,
				sd_ctrl->data_fmt, sd_ctrl->flag, sd_ctrl->name, 0);

	prd_write_sd_log(prd, sd_ctrl->prt_buf, TIME_INFO_SKIP);

	memset(sd_ctrl->prt_buf, 0, strlen(sd_ctrl->prt_buf));

	return size;
}

static int __used prd_prt_sd_gap_x(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)ctrl_data;
	int size = 0;

	prd_cal_rawdata_gap_x(prd, sd_ctrl->raw_tmp, sd_ctrl->raw_buf,
		sd_ctrl->row_act, sd_ctrl->col_act, sd_ctrl->data_fmt, sd_ctrl->flag);

	size = prd_print_rawdata(prd, sd_ctrl->prt_buf, 0,
				sd_ctrl->raw_tmp, sd_ctrl->row_act, sd_ctrl->col_act,
				sd_ctrl->data_fmt, sd_ctrl->flag, sd_ctrl->name, 0);

	prd_write_sd_log(prd, sd_ctrl->prt_buf, TIME_INFO_SKIP);

	memset(sd_ctrl->prt_buf, 0, strlen(sd_ctrl->prt_buf));

	return size;
}

static int __used prd_prt_sd_gap_y(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)ctrl_data;
	int size = 0;

	prd_cal_rawdata_gap_y(prd, sd_ctrl->raw_tmp, sd_ctrl->raw_buf,
		sd_ctrl->row_act, sd_ctrl->col_act, sd_ctrl->data_fmt, sd_ctrl->flag);

	size = prd_print_rawdata(prd, sd_ctrl->prt_buf, 0,
				sd_ctrl->raw_tmp, sd_ctrl->row_act, sd_ctrl->col_act,
				sd_ctrl->data_fmt, sd_ctrl->flag, sd_ctrl->name, 0);

	prd_write_sd_log(prd, sd_ctrl->prt_buf, TIME_INFO_SKIP);

	memset(sd_ctrl->prt_buf, 0, strlen(sd_ctrl->prt_buf));

	return size;
}

/*
 *	result - Pass : 0, Fail : 1
 */
static int prd_chk_sd_common(struct lxs_prd_data *prd,
			struct lxs_prd_sd_ctrl *sd_ctrl, char *raw_tmp)
{
	char *prt_buf = sd_ctrl->prt_buf;
	char *raw_buf = (raw_tmp) ? raw_tmp : sd_ctrl->raw_buf;
	uint16_t *rawdata_u16 = (uint16_t *)raw_buf;
	int16_t *rawdata_s16 = (int16_t *)raw_buf;
	s8 *rawdata_s8 = (s8 *)raw_buf;
	int is_16bit = !!(sd_ctrl->data_fmt & PRD_DATA_FMT_MASK);
	int is_u16bit = !!(sd_ctrl->data_fmt & PRD_DATA_FMT_U16);
	int empty_node = !!(sd_ctrl->empty_node != NULL);
	int is_ign_edge_t = !!(sd_ctrl->flag & PRD_RAW_FLAG_IGN_EDGE_T);
	int is_ign_edge_b = !!(sd_ctrl->flag & PRD_RAW_FLAG_IGN_EDGE_B);
	int row_size = sd_ctrl->row_act;
	int col_size = sd_ctrl->col_act;
	int empty_col_s = 0;
	int empty_col_e = 0;
	int empty_row_s = 0;
	int empty_row_e = 0;
	int curr_raw;
	int prt_size = 0;
	int min_raw = PRD_RAWDATA_MAX;
	int max_raw = PRD_RAWDATA_MIN;
	int i, j;

	if (empty_node) {
		empty_col_s = sd_ctrl->empty_node->col_s;
		empty_col_e = sd_ctrl->empty_node->col_e;
		empty_row_s = sd_ctrl->empty_node->row_s;
		empty_row_e = sd_ctrl->empty_node->row_e;
	}

	t_prd_info(prd, "%s\n", "-------- Check --------");
	prt_size += lxs_prd_buf_snprintf(prt_buf, prt_size, "-------- Check --------\n");

	if (raw_buf == NULL) {
		t_prd_err(prd, "%s\n", "check failed: NULL buf");
		prt_size += lxs_prd_buf_snprintf(prt_buf, prt_size, "check failed: NULL buf\n");
		return 1;
	}

	for (i = 0; i < row_size; i++) {
		for (j = 0; j < col_size; j++) {
			if (is_16bit)
				curr_raw = (is_u16bit) ? *rawdata_u16++ : *rawdata_s16++;
			else
				curr_raw = *rawdata_s8++;

			if (empty_node &&
				((i >= empty_row_s) && (i <= empty_row_e)) &&
				((j >= empty_col_s) && (j <= empty_col_e))) {
				t_prd_dbg_trace(prd, "empty node: [%d, %d] <= [%d, %d] < [%d, %d]\n",
					empty_row_s, empty_col_s, i, j, empty_row_e, empty_col_e);
				continue;
			}

			if (is_ign_edge_t && !i)
				if (!j || (j == (col_size-1)))
					continue;

			if (is_ign_edge_b && (i == (row_size - 1)))
				if (!j || (j == (col_size-1)))
					continue;

			if (curr_raw < min_raw)
				min_raw = curr_raw;

			if (curr_raw > max_raw)
				max_raw = curr_raw;
		}
	}

	sd_ctrl->min = min_raw;
	sd_ctrl->max = max_raw;

	t_prd_info(prd, "%s min : %d , max : %d\n", sd_ctrl->name, min_raw, max_raw);
	prt_size += lxs_prd_buf_snprintf(prt_buf, prt_size,
		"%s min : %d , max : %d\n", sd_ctrl->name, min_raw, max_raw);

	return 0;
}

static int __used prd_chk_sd_raw(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)ctrl_data;

	return prd_chk_sd_common(prd, sd_ctrl, NULL);
}

static int __used prd_chk_sd_gap_x(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)ctrl_data;

	return prd_chk_sd_common(prd, sd_ctrl, sd_ctrl->raw_tmp);
}

static int __used prd_chk_sd_gap_y(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)ctrl_data;

	return prd_chk_sd_common(prd, sd_ctrl, sd_ctrl->raw_tmp);
}

static int __used prd_get_sd_os(void *prd_data, void *ctrl_data)
{
	return prd_get_sd_raw(prd_data, ctrl_data);
}

static int __used prd_prt_sd_rxtx(void *prd_data, void *ctrl_data)
{
	struct lxs_prd_data *prd = (struct lxs_prd_data *)prd_data;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)ctrl_data;
	int size = 0;

	size = prd_print_data_rxtx(prd, sd_ctrl->prt_buf, 0,
				sd_ctrl->raw_buf, sd_ctrl->row_act, sd_ctrl->col_act,
				sd_ctrl->data_fmt, sd_ctrl->flag, sd_ctrl->name);

	prd_write_sd_log(prd, sd_ctrl->prt_buf, TIME_INFO_SKIP);

	memset(sd_ctrl->prt_buf, 0, strlen(sd_ctrl->prt_buf));

	return size;
}

static int __used prd_chk_sd_rxtx(void *prd_data, void *ctrl_data)
{
	return prd_chk_sd_raw(prd_data, ctrl_data);
}

static void prd_test_mode_debug(struct lxs_prd_data *prd, struct lxs_prd_sd_ctrl *sd_ctrl)
{
	/* TBD */
}

static int prd_write_test_mode(struct lxs_prd_data *prd, struct lxs_prd_sd_ctrl *sd_ctrl)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;
	int cmd = sd_ctrl->cmd;
	int is_fw_skip = !!(sd_ctrl->flag & PRD_RAW_FLAG_FW_SKIP);
	int delay = (sd_ctrl->delay) ? sd_ctrl->delay : 200;
	u32 addr = TC_TSP_TEST_CTL;
	int retry = 80;
	u32 rdata = 0x1;
	u32 rdata_lsb = !!(sd_ctrl->flag & PRD_RAW_FLAG_RESP_LSB);
	int ret = 0;

	if (is_fw_skip)
		return 0;

	ret = lxs_ts_write_value(ts, addr, cmd);
	if (ret < 0)
		return ret;

	t_prd_info(prd, "write testmode[%04Xh] = %Xh\n", addr, cmd);

	lxs_ts_delay(delay);

	/* Check Test Result - wait until 0 is written */
	addr = TC_TSP_TEST_STS;
	do {
		lxs_ts_delay(100);
		ret = lxs_ts_read_value(ts, addr, &rdata);
		if (ret < 0)
			return ret;

		if (ret >= 0)
			t_prd_dbg_base(prd, "rdata[%04Xh] = 0x%x\n",
				addr, rdata);

		if (rdata_lsb)
			rdata &= 0xFFFF;
	} while ((rdata != 0xAA) && retry--);

	if (rdata != 0xAA) {
		t_prd_err(prd, "%s test '%s' Time out, %08Xh\n",
			(sd_ctrl->is_sd) ? "sd" : "lpwg_sd", sd_ctrl->name, rdata);
		prd_test_mode_debug(prd, sd_ctrl);

		t_prd_err(prd, "%s\n", "Write test mode failed");
		return -ETIMEDOUT;
	}

	return 0;
}

static int prd_sd_test_core(struct lxs_prd_data *prd, struct lxs_prd_sd_ctrl *sd_ctrl)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;
	char *test_str = sd_ctrl->name;
	char info_str[64] = { 0, };
	char info_str_wr[64] = { 0, };
	int cmd = sd_ctrl->cmd;
	int is_os = sd_ctrl->is_os;
	int is_os_data = !!(sd_ctrl->flag & PRD_DATA_FMT_OS_DATA);
	int test_result = 0;
	int cmp_result = 0;
	int ret = 0;

	t_prd_info(prd, "======== %s ========\n", test_str);

	if (is_os && !cmd) {
		snprintf(info_str, sizeof(info_str),
			"os test: test %s, but command is zero\n", sd_ctrl->name);
		prd_write_sd_log(prd, info_str, TIME_INFO_SKIP);
		t_prd_err(prd, "%s", info_str);
		return 1;
	}

	if (is_os)
		snprintf(info_str, sizeof(info_str), "[%s %s] Data Offset = 0x%X",
			sd_ctrl->name, (is_os_data) ? "Data" : "Raw", sd_ctrl->offset);
	else
		snprintf(info_str, sizeof(info_str), "[%s] Data Offset = 0x%X",
			sd_ctrl->name, sd_ctrl->offset);

	snprintf(info_str_wr, sizeof(info_str_wr), "\n%s\n", info_str);

	prd_write_sd_log(prd, info_str_wr, TIME_INFO_SKIP);

	t_prd_info(prd, "%s\n", info_str);

	/* Test Start & Finish Check */
	ret = prd_write_test_mode(prd, sd_ctrl);
	if (ret < 0)
		return ret;

	if (is_os) {
		ret = lxs_ts_read_value(ts, TC_TSP_TEST_RESULT, &test_result);
		if (ret < 0)
			return ret;

		t_prd_info(prd, "%s test result = %d\n", sd_ctrl->name, test_result);
	}

	memset(prd->raw_buf, 0, sd_ctrl->data_size);

	sd_ctrl->row_act = sd_ctrl->row_size;
	sd_ctrl->col_act = sd_ctrl->col_size;
	sd_ctrl->raw_buf = prd->raw_buf;
	sd_ctrl->raw_tmp = prd->raw_tmp;
	sd_ctrl->prt_buf = prd->log_pool;

	/* specific case */
	if (sd_ctrl->odd_sd_func) {
		return sd_ctrl->odd_sd_func(prd, sd_ctrl);
	}

	if (sd_ctrl->get_sd_func) {
		ret = sd_ctrl->get_sd_func(prd, sd_ctrl);
		if (ret < 0)
			return ret;
	}

	if (sd_ctrl->mod_sd_func) {
		ret = sd_ctrl->mod_sd_func(prd, sd_ctrl);
		if (ret < 0)
			return ret;
	}

	if (sd_ctrl->prt_sd_func)
		sd_ctrl->prt_sd_func(prd, sd_ctrl);

	if (sd_ctrl->cmp_sd_func) {
		cmp_result = sd_ctrl->cmp_sd_func(prd, sd_ctrl);
		if (cmp_result)
			return cmp_result;
	}

	if (is_os)
		return test_result;

	return 0;
}

static int prd_sd_test_common(struct lxs_prd_data *prd, int *sd_result_list, int is_sd)
{
	struct lxs_prd_sd_ctrl *sd_ctrl = (is_sd) ? prd->sd_ctrl : prd->lpwg_sd_ctrl;
	int num_ctrl = (is_sd) ? prd->num_sd_ctrl : prd->num_lpwg_sd_ctrl;
	int i;
	int ret = 0;

	for (i = 0; i < num_ctrl ; i++) {
		ret = prd_sd_test_core(prd, sd_ctrl);

		if (ret)
			sd_result_list[sd_ctrl->id] = (ret < 0) ? 1 : ret;

		if (ret < 0) {
			t_prd_err(prd, "%s test(%s) failed, %d\n",
				(sd_ctrl->is_os) ? "os" : "raw",
				sd_ctrl->name, ret);
		//	return ret;
		}

		sd_ctrl++;
	}

	return 0;
}

static int prd_show_do_sd(struct lxs_prd_data *prd, char *prt_buf)
{
	struct lxs_prd_sd_ctrl *sd_ctrl = prd->sd_ctrl;
	int sd_result_list[PRD_CTRL_IDX_SD_MAX] = { 0, };
	int size = 0;
	char *item = NULL;
	int i;
	int ret = 0;

	ret = __prd_sd_pre(prd, prt_buf);
	if (ret < 0)
		goto out;

	ret = prd_sd_test_common(prd, sd_result_list, 1);
	if (ret < 0)
		goto out_test;

	size += lxs_snprintf(prt_buf, size, "%d ", prd->num_sd_ctrl + 1);

	for (i = 0; i < prd->num_sd_ctrl; i++) {
		item = sd_ctrl->name;
	#if defined(__PRD_SUPPORT_SD_MIN_MAX)
		size += lxs_snprintf(prt_buf, size, "%s:%d,%d ",
				item, sd_ctrl->min, sd_ctrl->max);
	#else
		size += lxs_snprintf_sd_result(prt_buf, size,
				item, sd_result_list[sd_ctrl->id]);
	#endif
		sd_ctrl++;
	}

	ret = lxs_ts_sram_test((struct lxs_ts_data *)prd->ts, 1);
#if defined(__PRD_SUPPORT_SD_MIN_MAX)
	size += lxs_snprintf(prt_buf, size, "SRAM:%d", (ret < 0) ? 1 : 0);
#else
	size += lxs_snprintf_sd_result(prt_buf, size, "SRAM", (ret < 0) ? 1 : 0);
#endif

	size += lxs_snprintf(prt_buf, size, "\n");

out_test:
	__prd_sd_post(prd, prt_buf);

out:
	return size;
}

static ssize_t prd_show_sd(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	char log_tmp[64] = {0, };
	int size = 0;
	int ret = 0;

	mutex_lock(&prd->lock);

	if (!prd->num_sd_ctrl) {
		size += lxs_snprintf(buf, size, "sd disabled\n");
		t_prd_err(prd, "%s", buf);
		goto out;
	}

	ret = prd_check_exception(prd);
	if (ret) {
		size += lxs_snprintf(buf, size,
					"drv exception(%d) detected, test canceled\n", ret);
		t_prd_err(prd, "%s", buf);
		goto out;
	}

	if (!is_ts_power_state_on(ts)) {
		size += lxs_snprintf(buf, size,
					"Not power on state, %d\n", ts_get_power_state(ts));
		goto out;
	}

	lxs_ts_irq_enable(ts, false);

	/* file create , time log */
	snprintf(log_tmp, sizeof(log_tmp), "\nShow_sd Test Start");
	prd_write_sd_log(prd, log_tmp, TIME_INFO_SKIP);
	prd_write_sd_log(prd, "\n", TIME_INFO_WRITE);

	t_prd_info(prd, "%s\n", "show_sd test begins");

	prd_fw_version_log(prd);

	mutex_lock(&ts->device_mutex);

	size = prd_show_do_sd(prd, buf);

	mutex_unlock(&ts->device_mutex);

	prd_write_sd_log(prd, "Show_sd Test End\n", TIME_INFO_WRITE);

	t_prd_info(prd, "%s\n", "show_sd test terminated");

	prd_chip_reset(prd);

out:
	mutex_unlock(&prd->lock);

	return (ssize_t)size;
}
static DEVICE_ATTR(sd, 0664, prd_show_sd, NULL);

static int prd_show_do_lpwg_sd(struct lxs_prd_data *prd, char *prt_buf)
{
	struct lxs_prd_sd_ctrl *lpwg_sd_ctrl = prd->lpwg_sd_ctrl;
	int lpwg_sd_result_list[PRD_CTRL_IDX_LPWG_SD_MAX] = { 0, };
	int size = 0;
	char *item = NULL;
	int i;
	int ret = 0;

	ret = __prd_sd_pre(prd, prt_buf);
	if (ret < 0)
		goto out;

	ret = prd_sd_test_common(prd, lpwg_sd_result_list, 0);
	if (ret < 0)
		goto out_test;

	size += lxs_snprintf(prt_buf, size, "%d ", prd->num_lpwg_sd_ctrl);

	for (i = 0; i < prd->num_lpwg_sd_ctrl; i++) {
		item = lpwg_sd_ctrl->name;
	#if defined(__PRD_SUPPORT_SD_MIN_MAX)
		size += lxs_snprintf(prt_buf, size, "%s:%d,%d ",
				item, lpwg_sd_ctrl->min, lpwg_sd_ctrl->max);
	#else
		size += lxs_snprintf_sd_result(prt_buf, size,
				item, lpwg_sd_result_list[lpwg_sd_ctrl->id]);
	#endif
		lpwg_sd_ctrl++;
	}

	size += lxs_snprintf(prt_buf, size, "\n");

out_test:
	__prd_sd_post(prd, prt_buf);

out:
	return size;
}

static ssize_t prd_show_lpwg_sd(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	char log_tmp[64] = {0, };
	int size = 0;
	int ret = 0;

	mutex_lock(&prd->lock);

	if (!prd->num_lpwg_sd_ctrl) {
		size += lxs_snprintf(buf, size, "lpwg_sd disabled\n");
		t_prd_err(prd, "%s", buf);
		goto out;
	}

	ret = prd_check_exception(prd);
	if (ret) {
		size += lxs_snprintf(buf, size,
					"drv exception(%d) detected, test canceled\n", ret);
		t_prd_err(prd, "%s", buf);
		goto out;
	}

	if (!is_ts_power_state_lpm(ts)) {
		size += lxs_snprintf(buf, size,
					"Not power lpm state, %d\n", ts_get_power_state(ts));
		goto out;
	}

	lxs_ts_irq_enable(ts, false);

	/* file create , time log */
	snprintf(log_tmp, sizeof(log_tmp), "\nShow_lpwg_sd Test Start");
	prd_write_sd_log(prd, log_tmp, TIME_INFO_SKIP);
	prd_write_sd_log(prd, "\n", TIME_INFO_WRITE);

	t_prd_info(prd, "%s\n", "show_lpwg_sd test begins");

	prd_fw_version_log(prd);

	mutex_lock(&ts->device_mutex);
	size = prd_show_do_lpwg_sd(prd, buf);
	mutex_unlock(&ts->device_mutex);

	prd_write_sd_log(prd, "Show_lpwg_sd Test End\n", TIME_INFO_WRITE);

	t_prd_info(prd, "%s\n", "show_lpwg_sd test terminated");

	prd_chip_reset(prd);

out:
	mutex_unlock(&prd->lock);

	return (ssize_t)size;
}
static DEVICE_ATTR(lpwg_sd, 0664, prd_show_lpwg_sd, NULL);

static void *prd_lookup_sd_ctrl(struct lxs_prd_data *prd, int type)
{
	if (type < PRD_CTRL_IDX_SD_MAX) {
		int idx = prd->sd_lookup_table[type];
		if (idx != -1)
			return &prd->sd_ctrl[idx];
	}

	return NULL;
}

static void *prd_lookup_lpwg_sd_ctrl(struct lxs_prd_data *prd, int type)
{
	if (type < PRD_CTRL_IDX_LPWG_SD_MAX) {
		int idx = prd->lpwg_sd_lookup_table[type];
		if (idx != -1)
			return &prd->lpwg_sd_ctrl[idx];
	}

	return NULL;
}

static void __run_print_rawdata(char *prt_buf, char *raw_buf,
			int row_size, int col_size, int data_fmt, u32 flag)
{
	uint16_t *rawdata_u16 = (uint16_t *)raw_buf;
	int16_t *rawdata_s16 = (int16_t *)raw_buf;
	s8 *rawdata_s8 = (s8 *)raw_buf;
	int is_16bit = !!(data_fmt & PRD_DATA_FMT_MASK);
	int is_u16bit = !!(data_fmt & PRD_DATA_FMT_U16);
	int curr_raw;
	char tmp[RUN_DATA_WORD_LEN];
	int i, j;

	if (prt_buf == NULL)
		return;

	for (i = 0; i < row_size; i++) {
		for (j = 0; j < col_size; j++) {
			if (is_16bit)
				curr_raw = (is_u16bit) ? *rawdata_u16++ : *rawdata_s16++;
			else
				curr_raw = *rawdata_s8++;

			memset(tmp, 0x00, RUN_DATA_WORD_LEN);
			snprintf(tmp, RUN_DATA_WORD_LEN, "%d,", curr_raw);
			strncat(prt_buf, tmp, RUN_DATA_WORD_LEN);
		}
	}
}

static int __run_sd_func(void *ts_data, int type, int *min, int *max, char *prt_buf, int opt, int is_sd)
{
	struct lxs_ts_data *ts = ts_data;
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	struct lxs_prd_sd_ctrl *sd_ctrl;
	struct lxs_prd_sd_ctrl __ctrl;
	struct lxs_prd_sd_ctrl *ctrl = &__ctrl;
	char *raw_buf;
	int ret;

	if (prd == NULL)
		return -EINVAL;

	mutex_lock(&prd->lock);

	if (is_sd)
		sd_ctrl = prd_lookup_sd_ctrl(prd, type);
	else
		sd_ctrl = prd_lookup_lpwg_sd_ctrl(prd, type);
	if (sd_ctrl == NULL) {
		t_prd_err(prd, "%s: %sSD not support, %d\n",
			__func__, (is_sd) ? "" : "LPWG_", type);
		ret = -EINVAL;
		goto out;
	}

	memcpy(ctrl, sd_ctrl, sizeof(struct lxs_prd_sd_ctrl));

	if (opt & BIT(0))
		ctrl->flag |= PRD_RAW_FLAG_FW_SKIP;
	else
		ctrl->flag &= ~PRD_RAW_FLAG_FW_SKIP;

	ret = prd_sd_test_core(prd, ctrl);
	if (ret < 0)
		goto out;

	raw_buf = (ctrl->flag & PRD_RAW_FLAG_DATA_TMP) ? ctrl->raw_tmp : ctrl->raw_buf;

	__run_print_rawdata(prt_buf, raw_buf,
		ctrl->row_act, ctrl->col_act,
		ctrl->data_fmt, ctrl->flag);

	if (min)
		*min = ctrl->min;

	if (max)
		*max = ctrl->max;

out:
	mutex_unlock(&prd->lock);

	return ret;
}

static int __used run_open(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_SD_OPEN_NODE, min, max, prt_buf, opt, 1);
}

static int __used run_short_mux(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_SD_SHORT_MUX_NODE, min, max, prt_buf, opt, 1);
}

static int __used run_short_ch(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_SD_SHORT_CH_NODE, min, max, prt_buf, opt, 1);
}

static int __used run_np_m1_raw(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_SD_U3_M1_RAW, min, max, prt_buf, opt, 1);
}

static int __used run_np_m1_jitter(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_SD_U3_M1_JITTER, min, max, prt_buf, opt, 1);
}

static int __used run_np_m2_raw(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_SD_U3_M2_RAW, min, max, prt_buf, opt, 1);
}

static int __used run_np_m2_gap_x(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_SD_U3_M2_GAP_X, min, max, prt_buf, opt, 1);
}

static int __used run_np_m2_gap_y(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_SD_U3_M2_GAP_Y, min, max, prt_buf, opt, 1);
}

static int __used run_np_m2_jitter(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_SD_U3_JITTER, min, max, prt_buf, opt, 1);
}

static int __used run_lp_m1_raw(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_LPWG_SD_U0_M1_RAW, min, max, prt_buf, opt, 0);
}

static int __used run_lp_m1_jitter(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_LPWG_SD_U0_M1_JITTER, min, max, prt_buf, opt, 0);
}

static int __used run_lp_m2_raw(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_LPWG_SD_U0_M2_RAW, min, max, prt_buf, opt, 0);
}

static int __used run_lp_m2_jitter(void *ts_data, int *min, int *max, char *prt_buf, int opt)
{
	return __run_sd_func(ts_data, PRD_CTRL_IDX_LPWG_SD_U0_JITTER, min, max, prt_buf, opt, 0);
}

#endif	/* __PRD_SUPPORT_SD_TEST */

static void *prd_lookup_raw_ctrl(struct lxs_prd_data *prd, int type)
{
	if (type < PRD_CTRL_IDX_MAX) {
		int idx = prd->raw_lookup_table[type];
		if (idx != -1)
			return &prd->raw_ctrl[idx];
	}

	return NULL;
}

static ssize_t prd_get_data_core(struct lxs_prd_data *prd, struct lxs_prd_raw_ctrl *ctrl, char *raw_buf)
{
	int data_size = ctrl->data_size;
	int ret = 0;

	if (raw_buf == NULL)
		raw_buf = prd->raw_buf;

	memset(raw_buf, 0, data_size);

	ret = prd_stop_firmware(prd, ctrl);
	if (ret < 0)
		goto out;

	ctrl->row_act = ctrl->row_size;
	ctrl->col_act = ctrl->col_size;
	ctrl->raw_buf = raw_buf;
	ctrl->raw_tmp = prd->raw_tmp;

	if (ctrl->get_raw_func) {
		ret = ctrl->get_raw_func(prd, ctrl);
		if (ret < 0)
			goto out;
	}

	ret = prd_start_firmware(prd, ctrl);
	if (ret < 0)
		goto out;

	if (ctrl->mod_raw_func) {
		ret = ctrl->mod_raw_func(prd, ctrl);
		if (ret < 0)
			goto out;
	}

out:
	return ret;
}

static ssize_t prd_show_check_power(struct device *dev, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int size = 0;

	if (ts->shutdown_called) {
		size += sprintf(buf, "shutdown called\n");
		goto out;
	}

	if (is_ts_power_state_off(ts)) {
		size += sprintf(buf, "touch stopped\n");
		goto out;
	}

out:
	return (ssize_t)size;
}

static ssize_t prd_show_common(struct device *dev, char *buf, int type)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	struct lxs_prd_raw_ctrl *ctrl = NULL;
	int prt_size = 0;
	int size = 0;
	int ret = 0;

	size = prd_show_check_power(dev, buf);
	if (size)
		return (ssize_t)size;

	mutex_lock(&prd->lock);

	ret = prd_check_exception(prd);
	if (ret) {
		size = sprintf(buf, "drv exception(%d) detected, test canceled\n", ret);
		t_prd_err(prd, "%s", buf);
		goto out;
	}

	ctrl = prd_lookup_raw_ctrl(prd, type);
	if (ctrl == NULL) {
		size = sprintf(buf, "type(%d) disabled, test canceled\n", ret);
		t_prd_err(prd, "%s", buf);
		goto out;
	}

	ctrl->prt_buf = prd->log_pool;

	t_prd_info(prd, "======== %s(%d %d %d 0x%X) ========\n",
		ctrl->name, ctrl->cmd, ctrl->row_size, ctrl->col_size, ctrl->offset);
	prt_size += lxs_prd_buf_snprintf(ctrl->prt_buf, prt_size,
					"======== %s(%d %d %d 0x%X) ========\n",
					ctrl->name, ctrl->cmd,
					ctrl->row_size, ctrl->col_size, ctrl->offset);

	ret = prd_get_data_core(prd, ctrl, prd->raw_buf);
	if (ret < 0) {
		t_prd_err(prd, "%s(%d) failed, %d\n", __func__, type, ret);
		prt_size += lxs_prd_buf_snprintf(ctrl->prt_buf, prt_size,
						"%s(%d) failed, %d\n", __func__, type, ret);
		goto done;
	}

	if (ctrl->prt_raw_func)
		ctrl->prt_raw_func(prd, ctrl, prt_size);

done:
	size += lxs_prd_buf_snprintf(buf, size, "%s\n", ctrl->prt_buf);

#if 0
	size += lxs_prd_buf_snprintf(buf, size, "Get Data[%s] result: %s\n",
				ctrl->name, (ret < 0) ? "Fail" : "Pass");
#endif

out:
	mutex_unlock(&prd->lock);

	return (ssize_t)size;
}

DEVICE_ATTR_PRD_BIN(debug_buf, DEBUG, 0);
#if defined(__PRD_USE_BIG_ATTR)
DEVICE_ATTR_PRD_BIN(rawdata, RAW, 1);
DEVICE_ATTR_PRD_BIN(base, BASE, 1);
DEVICE_ATTR_PRD_BIN(delta, DELTA, 1);
DEVICE_ATTR_PRD_BIN(label, LABEL, 1);
DEVICE_ATTR_PRD_BIN(sysr, SYSR, 0);
DEVICE_ATTR_PRD_BIN(sysb, SYSB, 0);
#else
DEVICE_ATTR_PRD_STD(rawdata, RAW, 1);
DEVICE_ATTR_PRD_STD(base, BASE, 1);
DEVICE_ATTR_PRD_STD(delta, DELTA, 1);
DEVICE_ATTR_PRD_STD(label, LABEL, 1);
DEVICE_ATTR_PRD_STD(sysr, SYSR, 0);
DEVICE_ATTR_PRD_STD(sysb, SYSB, 0);
#endif

static int prd_show_app_blocked(struct lxs_ts_data *ts)
{
	if (atomic_read(&ts->init) == TC_IC_INIT_NEED)
		return 1;

	if (atomic_read(&ts->fwup))
		return 2;

	return 0;
}

static ssize_t prd_show_app_op_end(struct lxs_prd_data *prd, char *buf, int prev_mode)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)prd->ts;
	struct lxs_prd_raw_ctrl *raw_ctrl = prd->app_raw_ctrl;
	int ret = 0;

	if (prd_show_app_blocked(ts))
		return 1;

	buf[0] = REPORT_END_RS_OK;
	if (prev_mode != REPORT_OFF) {
		prd->app_mode = REPORT_OFF;
		ret = prd_start_firmware(prd, raw_ctrl);
		if (ret < 0) {
			t_prd_err(prd, "prd_start_firmware failed, %d\n", ret);
			buf[0] = REPORT_END_RS_NG;
		}
	}

	return 1;
}

static ssize_t prd_show_app_common(struct device *dev, char *buf, int mode)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	struct lxs_prd_raw_ctrl *ctrl = NULL;
	int prev_mode = prd->app_mode;
	int data_size = 0;
	int ctrl_id = -1;
	int is_16bit = 0;
	int do_conv_16to8 = 0;

	mutex_lock(&prd->lock);

	prd->app_read = 1;

	if (prd_show_app_blocked(ts)) {
		memset(buf, 0, PAGE_SIZE);
		data_size = 1;
		goto out;
	}

	if (mode < REPORT_MAX)
		t_prd_dbg_base(prd, "show app mode : %s(%d)\n",
			prd_app_mode_str[mode], mode);

	if (mode == REPORT_OFF) {
		data_size = prd_show_app_op_end(prd, buf, prev_mode);
		goto out;
	}

	prd->app_mode = mode;

	ctrl_id = prd_app_mode_ctrl_id[mode];

	if (ctrl_id < 0) {
		t_prd_err(prd, "unknown mode, %d\n", mode);
		if (prev_mode != REPORT_OFF)
			prd_show_app_op_end(prd, buf, prev_mode);

		data_size = PAGE_SIZE;
		memset(buf, 0, PAGE_SIZE);
		goto out;
	}

	ctrl = prd_lookup_raw_ctrl(prd, ctrl_id);
	if (ctrl == NULL)
		goto out;

	prd->app_raw_ctrl = ctrl;

	data_size = ctrl->data_size;

	is_16bit = !!(ctrl->data_fmt & PRD_DATA_FMT_MASK);

	switch (mode) {
	case REPORT_LABEL:
		do_conv_16to8 = is_16bit;
		break;
	}

	/*
	 * if do_conv_16to8
	 *   memory > prd->raw_app > prd->raw_buf > buf
	 * else
	 *   mempry > prd->raw_app > buf
	 */
	prd_get_data_core(prd, ctrl, prd->raw_app);

	if (do_conv_16to8) {
		data_size >>= 1;	//node size

		__buff_16to8(prd, prd->raw_buf, prd->raw_app, data_size);
		ctrl->raw_buf = prd->raw_buf;
	}

	memcpy(buf, ctrl->raw_buf, data_size);

out:
	prd->app_read = 0;

	mutex_unlock(&prd->lock);

#if 0
	if (ts->prd_quirks & PRD_QUIRK_RAW_RETURN_MODE_VAL) {
		return (ssize_t)mode;
	}
#endif

	return (ssize_t)data_size;
}

DEVICE_ATTR_PRD_APP(raw, RAW);
DEVICE_ATTR_PRD_APP(base, BASE);
DEVICE_ATTR_PRD_APP(delta, DELTA);
DEVICE_ATTR_PRD_APP(label, LABEL);
DEVICE_ATTR_PRD_APP(debug_buf, DEBUG_BUG);
DEVICE_ATTR_PRD_APP(sysr, SYSR);
DEVICE_ATTR_PRD_APP(sysb, SYSB);
DEVICE_ATTR_PRD_APP(end, OFF);

static ssize_t prd_show_app_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	u32 temp = prd->sysfs_flag;

	memset(buf, 0, PRD_APP_INFO_SIZE);

	buf[0] = (temp & 0xff);
	buf[1] = ((temp >> 8) & 0xff);
	buf[2] = ((temp >> 16) & 0xff);
	buf[3] = ((temp >> 24) & 0xff);

	buf[8] = prd->row;
	buf[9] = prd->col;
	buf[10] = 0;	/* col_add */
	buf[11] = prd->ch;
	buf[12] = 2;	/* m1_col size */
	buf[13] = 0;	/* command type */
	buf[14] = 0;	/* second_scr.bound_i */
	buf[15] = 0;	/* second_scr.bound_j */
	if (prd->self) {
		buf[16] = 0;	/* self_rx_first */
		buf[16] |= 0;	/* self_reverse_c */
		buf[16] |= 0;	/* self_reverse_r */
	}

	if (prd_show_app_blocked(ts))
		t_prd_info(prd, "%s\n", "[Warning] prd app access blocked");

	t_prd_info(prd,
		"prd info: f %08Xh, r %d, c %d, ca %d, ch %d, m1 %d, cmd %d, bi %d, bj %d, self %Xh\n",
		temp, prd->row, prd->col, 0, prd->ch, 2, 0, 0, 0, buf[16]);

	return PRD_APP_INFO_SIZE + !!(prd->self);
}
static DEVICE_ATTR(prd_app_info, 0664, prd_show_app_info, NULL);

static ssize_t prd_show_app_reg(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	u32 addr = prd->app_reg_addr;
	u32 size = prd->app_reg_size;
	int ret = 0;

	if (prd_show_app_blocked(ts)) {
		t_prd_err(prd, "%s blocked\n", __func__);
		return (ssize_t)size;
	}

	if (size) {
		ret = lxs_ts_reg_read(ts, addr, buf, size);
		if (ret < 0)
			t_prd_err(prd, "prd_app_reg read(addr 0x%X, size 0x%X) failed, %d\n",
				addr, size, ret);
	}

	return (ssize_t)size;
}

/*
 * [Read control]
 * echo 0x1234 0x100 0x00 > prd_app_reg : Set read access w/o kernel log
 * echo 0x1234 0x100 0x01 > prd_app_reg : Set read access w/h kernel log
 * cat prd_app_reg                      : Actual read (binary)
 *
 * [Write control]
 * echo 0x1234 0x100 0x10 > prd_app_reg : Direct reg. write w/o kernel log
 * echo 0x1234 0x100 0x11 > prd_app_reg : Direct reg. write w/h kernel log
 * : 0x100 is data itself, not size value
 */
static ssize_t prd_store_app_reg(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	u32 addr = 0;
	u32 size = 4;
	u32 flag = 0;
	int ret = 0;

	if (prd_show_app_blocked(ts)) {
		t_prd_err(prd, "%s blocked\n", __func__);
		return count;
	}

	if (sscanf(buf, "%X %X %X", &addr, &size, &flag) <= 0) {
		t_prd_err(prd, "%s\n", "Invalid param");
		return count;
	}

	if (addr >= 0x0FFF) {
		t_prd_err(prd, "Invalid addr, %X\n", addr);
		return count;
	}

	if (flag & APP_REG_FLAG_WR) {
		u32 data = size;

		ret = lxs_ts_write_value(ts, addr, data);
		if (ret < 0) {
			t_prd_err(prd, "prd_app_reg write(addr 0x%X, data 0x%X) failed, %d\n",
				addr, data, ret);
		} else {
			if (flag & APP_REG_FLAG_LOG) {
				t_prd_info(prd, "prd_app_reg write(addr 0x%X, data 0x%X)\n",
					addr, data);
			}
		}
		return count;
	}

	if (!size || (size > PAGE_SIZE)) {
		t_prd_err(prd, "Invalid size, %X\n", size);
		return count;
	}

	prd->app_reg_addr = addr;
	prd->app_reg_size = size;

	if (flag & APP_REG_FLAG_LOG)
		t_prd_info(prd, "prd_app_reg set(addr 0x%X, size 0x%X)\n", addr, size);

	return count;
}
static DEVICE_ATTR(prd_app_reg, 0664, prd_show_app_reg, prd_store_app_reg);

static ssize_t prd_show_dbg_mask(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	int size = 0;

	size += lxs_snprintf(buf, size,
				"prd->dbg_mask  %08Xh\n",
				prd->dbg_mask);
	size += lxs_snprintf(buf, size,
				"t_prd_dbg_flag %08Xh\n",
				t_prd_dbg_flag);
	size += lxs_snprintf(buf, size,
				"t_prd_dbg_mask %08Xh\n",
				t_prd_dbg_mask);

	size += lxs_snprintf(buf, size,
				"\nUsage:\n");
	size += lxs_snprintf(buf, size,
				" prd->dbg_mask  : echo 0 {mask_value} > prd_dbg_mask\n");
	size += lxs_snprintf(buf, size,
				" t_prd_dbg_flag : echo 8 {mask_value} > prd_dbg_mask\n");
	size += lxs_snprintf(buf, size,
				" t_prd_dbg_mask : echo 9 {mask_value} > prd_dbg_mask\n");

	return (ssize_t)size;
}

static void prd_store_dbg_mask_usage(struct lxs_prd_data *prd)
{
	t_prd_info(prd, "%s\n", "Usage:");
	t_prd_info(prd, "%s\n", " prd->dbg_mask  : echo 0 {mask_value(hex)} > prd_dbg_mask");
	t_prd_info(prd, "%s\n", " t_prd_dbg_flag : echo 8 {mask_value(hex)} > prd_dbg_mask");
	t_prd_info(prd, "%s\n", " t_prd_dbg_mask : echo 9 {mask_value(hex)} > prd_dbg_mask");
}

static ssize_t prd_store_dbg_mask(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	int type = 0;
	u32 old_value, new_value = 0;

	if (sscanf(buf, "%d %X", &type, &new_value) <= 0) {
		t_prd_err(prd, "%s\n", "Invalid param");
		prd_store_dbg_mask_usage(prd);
		return count;
	}

	switch (type) {
	case 0:
		old_value = prd->dbg_mask;
		prd->dbg_mask = new_value;
		t_prd_info(prd, "prd->dbg_mask changed : %08Xh -> %08xh\n",
			old_value, new_value);
		break;
	case 8:
		old_value = t_prd_dbg_flag;
		t_prd_dbg_flag = new_value;
		input_info(true, dev, "t_prd_dbg_flag changed : %08Xh -> %08xh\n",
			old_value, new_value);
		break;
	case 9:
		old_value = t_prd_dbg_mask;
		t_prd_dbg_mask = new_value;
		input_info(true, dev, "t_prd_dbg_mask changed : %08Xh -> %08xh\n",
			old_value, new_value);
		break;
	default:
		prd_store_dbg_mask_usage(prd);
		break;
	}

	return count;
}
static DEVICE_ATTR(prd_dbg_mask, 0664, prd_show_dbg_mask, prd_store_dbg_mask);

static ssize_t prd_show_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	struct lxs_prd_raw_ctrl *raw_ctrl = prd->raw_ctrl;
	struct lxs_prd_sd_ctrl *sd_ctrl = prd->sd_ctrl;
	struct lxs_prd_sd_ctrl *lpwg_sd_ctrl = prd->lpwg_sd_ctrl;
	char *cal;
	int pad_row, pad_col;
	int i;
	int size = 0;

	size += lxs_snprintf(buf, size, "row %d, col %d, ch %d\n",
				prd->row, prd->col, prd->ch);
#if defined(USE_RINFO)
	size += lxs_snprintf(buf, size, "raw_base 0x%X, sys_base 0x%X, sys_cmd 0x%X, dbg_addr 0x%X\n",
				prd->raw_base, prd->sys_base, prd->sys_cmd, prd->sys_dbg_addr);
#endif

	if (prd->num_raw_ctrl)
		size += lxs_snprintf(buf, size, "----\n");

	for (i = 0; i < prd->num_raw_ctrl; i++) {
		cal = (raw_ctrl->data_fmt & PRD_DATA_FMT_RXTX) ? "+" : "x";

		if (raw_ctrl->is_pad) {
			pad_row = raw_ctrl->pad->row;
			pad_col = raw_ctrl->pad->col;
		} else {
			pad_row = 0;
			pad_col = 0;
		}

		size += lxs_snprintf(buf, size,
				" raw[%2d] (%2d) %12s (%5d, 0x%08X, 0x%08X), %2d %s %2d (%4d)(%d,%d), off 0x%04X\n",
				i, raw_ctrl->id, raw_ctrl->name, raw_ctrl->cmd,
				raw_ctrl->data_fmt, raw_ctrl->flag,
				raw_ctrl->row_size, cal, raw_ctrl->col_size, raw_ctrl->data_size,
				pad_row, pad_col,
				raw_ctrl->offset);

		raw_ctrl++;
	}

	if (prd->num_sd_ctrl)
		size += lxs_snprintf(buf, size, "----\n");

	for (i = 0; i < prd->num_sd_ctrl; i++) {
		cal = (sd_ctrl->data_fmt & PRD_DATA_FMT_RXTX) ? "+" : "x";

		if (sd_ctrl->is_pad) {
			pad_row = raw_ctrl->pad->row;
			pad_col = raw_ctrl->pad->col;
		} else {
			pad_row = 0;
			pad_col = 0;
		}

		size += lxs_snprintf(buf, size,
				"  sd[%2d] (%2d) %12s (0x%03X, 0x%08X, 0x%08X), %2d %s %2d (%4d)(%d,%d), off 0x%04X, sd/os %d/%d, dly %d\n",
				i, sd_ctrl->id, sd_ctrl->name, sd_ctrl->cmd,
				sd_ctrl->data_fmt, sd_ctrl->flag,
				sd_ctrl->row_size, cal, sd_ctrl->col_size, sd_ctrl->data_size,
				pad_row, pad_col,
				sd_ctrl->offset, sd_ctrl->is_sd, sd_ctrl->is_os,
				sd_ctrl->delay);

		sd_ctrl++;
	}

	if (prd->num_lpwg_sd_ctrl)
		size += lxs_snprintf(buf, size, "----\n");

	for (i = 0; i < prd->num_lpwg_sd_ctrl; i++) {
		cal = (lpwg_sd_ctrl->data_fmt & PRD_DATA_FMT_RXTX) ? "+" : "x";

		if (lpwg_sd_ctrl->is_pad) {
			pad_row = raw_ctrl->pad->row;
			pad_col = raw_ctrl->pad->col;
		} else {
			pad_row = 0;
			pad_col = 0;
		}

		size += lxs_snprintf(buf, size,
				"lpwg[%2d] (%2d) %12s (0x%03X, 0x%08X, 0x%08X), %2d %s %2d (%4d)(%d,%d), off 0x%04X, sd/os %d/%d, dly %d\n",
				i, lpwg_sd_ctrl->id, lpwg_sd_ctrl->name, lpwg_sd_ctrl->cmd,
				lpwg_sd_ctrl->data_fmt, lpwg_sd_ctrl->flag,
				lpwg_sd_ctrl->row_size, cal, lpwg_sd_ctrl->col_size, lpwg_sd_ctrl->data_size,
				pad_row, pad_col,
				lpwg_sd_ctrl->offset, lpwg_sd_ctrl->is_os, lpwg_sd_ctrl->is_os,
				lpwg_sd_ctrl->delay);

		lpwg_sd_ctrl++;
	}

	size += lxs_snprintf(buf, size, "\n");

	return (ssize_t)size;
}
static DEVICE_ATTR(prd_info, 0664, prd_show_info, NULL);

static struct attribute *lxs_prd_attribute_list[] = {
#if defined(__PRD_SUPPORT_SD_TEST)
	&dev_attr_sd.attr,
	&dev_attr_lpwg_sd.attr,
#endif
	/* */
#if !defined(__PRD_USE_BIG_ATTR)
	&dev_attr_rawdata.attr,
	&dev_attr_delta.attr,
	&dev_attr_label.attr,
	&dev_attr_base.attr,
	&dev_attr_sysr.attr,
	&dev_attr_sysb.attr,
#endif
	/* */
	&dev_attr_prd_app_raw.attr,
	&dev_attr_prd_app_base.attr,
	&dev_attr_prd_app_delta.attr,
	&dev_attr_prd_app_label.attr,
	&dev_attr_prd_app_debug_buf.attr,
	&dev_attr_prd_app_sysr.attr,
	&dev_attr_prd_app_sysb.attr,
	&dev_attr_prd_app_end.attr,
	&dev_attr_prd_app_info.attr,
	&dev_attr_prd_app_reg.attr,
	/* */
	&dev_attr_prd_dbg_mask.attr,
	&dev_attr_prd_info.attr,
	/* */
	NULL,
};

static struct bin_attribute *lxs_prd_attribute_bin_list[] = {
#if defined(__PRD_USE_BIG_ATTR)
	&touch_bin_attr_rawdata,
	&touch_bin_attr_delta,
	&touch_bin_attr_label,
	&touch_bin_attr_base,
	&touch_bin_attr_sysr,
	&touch_bin_attr_sysb,
#endif
	&touch_bin_attr_debug_buf,
	/* */
	NULL,	/* end mask */
};

static struct attribute_group lxs_prd_attribute_group = {
	.attrs = lxs_prd_attribute_list,
	.bin_attrs = lxs_prd_attribute_bin_list,
};

static int lxs_prd_create_group(struct lxs_ts_data *ts)
{
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	int ret = 0;

	ret = sysfs_create_group(&ts->sec.fac_dev->kobj, &lxs_prd_attribute_group);
	if (ret < 0)
		t_prd_err(prd, "sysfs_create_group failed, %d\n", ret);

	return ret;
}

static void lxs_prd_remove_group(struct lxs_ts_data *ts)
{
//	struct lxs_prd_data *prd = __ts_get_prd(ts);

	sysfs_remove_group(&ts->sec.fac_dev->kobj, &lxs_prd_attribute_group);
}

#define __ENABLE_RAW_CTRL

#if defined(__PRD_SUPPORT_SD_TEST)
#define __ENABLE_SD_CTRL
#define __ENABLE_LPWG_SD_CTRL
#endif

/*
 * Chip Setup : SW89112
 */
#define __ROW				36
#define __COL				18

#define __FMT_RAW			(PRD_DATA_FMT_S16)
#define __FLAG_RAW			(PRD_RAW_FLAG_IGN_EDGE_T)

#define __FMT_RAW_RXTX		(PRD_DATA_FMT_S16 | PRD_DATA_FMT_RXTX)

#define __FLAG_SYS			(PRD_RAW_FLAG_FW_NO_STS | PRD_RAW_FLAG_TYPE_SYS | PRD_RAW_FLAG_IGN_EDGE_T)

#define __FLAG_SD_DEF		(PRD_RAW_FLAG_TBL_OFFSET)
#define __FLAG_SD_OS_1ST	(__FLAG_SD_DEF | PRD_RAW_FLAG_IGN_EDGE_T)
#define __FLAG_SD_OS_2ND	(__FLAG_SD_DEF | PRD_RAW_FLAG_IGN_EDGE_T | PRD_RAW_FLAG_FW_SKIP)
#define __FLAG_SD_M2		(__FLAG_SD_DEF | PRD_RAW_FLAG_IGN_EDGE_T)
#define __FLAG_SD_M2_2ND	(__FLAG_SD_M2 | PRD_RAW_FLAG_FW_SKIP)
#define __FLAG_SD_M1		(__FLAG_SD_DEF | PRD_RAW_FLAG_TBL_RXTX)

#define __FLAG_LPWG_SD_DEF	(PRD_RAW_FLAG_TBL_OFFSET)
#define __FLAG_LPWG_SD_M2	(__FLAG_LPWG_SD_DEF | PRD_RAW_FLAG_IGN_EDGE_T)
#define __FLAG_LPWG_SD_M1	(__FLAG_LPWG_SD_DEF | PRD_RAW_FLAG_TBL_RXTX)

#if defined(__ENABLE_RAW_CTRL)
static const struct lxs_prd_raw_ctrl __used raw_ctrl_sw89112[] = {
	/* 33 x 41 */
	RAW_CTRL_INIT(PRD_CTRL_IDX_RAW, "raw", PRD_CMD_RAW,
		__FMT_RAW, __FLAG_RAW,
		__ROW, __COL, 0x21C2, prd_get_raw_data, NULL, prd_prt_rawdata),
	RAW_CTRL_INIT(PRD_CTRL_IDX_BASE, "base", PRD_CMD_BASE,
		__FMT_RAW, __FLAG_RAW,
		__ROW, __COL, 0x21C2, prd_get_raw_data, NULL, prd_prt_rawdata),
	RAW_CTRL_INIT(PRD_CTRL_IDX_DELTA, "delta", PRD_CMD_DELTA,
		__FMT_RAW, 0,
		__ROW, __COL, 0x21C2, prd_get_raw_data, NULL, prd_prt_rawdata),
	RAW_CTRL_INIT(PRD_CTRL_IDX_LABEL, "label", PRD_CMD_LABEL,
		__FMT_RAW, 0,
		__ROW, __COL, 0x21C2, prd_get_raw_data, NULL, prd_prt_rawdata),
	RAW_CTRL_INIT(PRD_CTRL_IDX_DEBUG, "debug", PRD_CMD_DEBUG,
		__FMT_RAW, 0,
		__ROW, __COL, 0x21C2, prd_get_raw_data, NULL, prd_prt_rawdata),
	/* Sys Raw */
	RAW_CTRL_INIT(PRD_CTRL_IDX_SYSR, "sysr", PRD_CMD_SYSR,
		__FMT_RAW, __FLAG_SYS,
		__ROW, __COL, 0x0D64, prd_get_raw_data, NULL, prd_prt_rawdata),
	RAW_CTRL_INIT(PRD_CTRL_IDX_SYSB, "sysb", PRD_CMD_SYSB,
		__FMT_RAW, __FLAG_SYS,
		__ROW, __COL, 0x0D64+0x1B0, prd_get_raw_data, NULL, prd_prt_rawdata),
};
#define TBL_RAW_CTRL	raw_ctrl_sw89112
#define NUM_RAW_CTRL	ARRAY_SIZE(raw_ctrl_sw89112)
#else
#define TBL_RAW_CTRL	NULL
#define NUM_RAW_CTRL	0
#endif

/*
 * open      : 1
 * short     : 2
 * m2        : 5
 * m1        : 3
 * jitter    : 13
 * m1_jitter : 4
 * p_jitter  : 14
 */
#if defined(__ENABLE_SD_CTRL)
static const struct lxs_prd_sd_ctrl __used sd_ctrl_sw89112[] = {
	/* OS */
	SD_CTRL_OS_INIT(PRD_CTRL_IDX_SD_OPEN_NODE, "OPEN", 0x301,
		__FMT_RAW, __FLAG_SD_OS_1ST,
		__ROW, __COL, 0xD64, prd_get_sd_os, NULL, prd_prt_sd_raw, prd_chk_sd_raw, 1000),
	/* */
	SD_CTRL_OS_INIT(PRD_CTRL_IDX_SD_SHORT_MUX_NODE, "SHORT_MUX", 0x30C,
		__FMT_RAW, __FLAG_SD_OS_1ST,
		__ROW, __COL, 0xD64, prd_get_sd_os, NULL, prd_prt_sd_raw, prd_chk_sd_raw, 1000),
	SD_CTRL_OS_INIT(PRD_CTRL_IDX_SD_SHORT_CH_NODE, "SHORT_CH", 0x30C,
		__FMT_RAW, __FLAG_SD_OS_2ND,
		__ROW, __COL, 0xEA8, prd_get_sd_os, NULL, prd_prt_sd_raw, prd_chk_sd_raw, 1000),
	/* */
	SD_CTRL_INIT(PRD_CTRL_IDX_SD_U3_M1_RAW, "NP_M1_RAW", 0x303,
		__FMT_RAW, __FLAG_SD_M1,
		__ROW, 2, 0xD64, prd_get_sd_raw, prd_mod_sd_m1, prd_prt_sd_raw, prd_chk_sd_raw, 0),
	SD_CTRL_INIT(PRD_CTRL_IDX_SD_U3_M1_JITTER, "NP_M1_JITTER", 0x304,
		__FMT_RAW, __FLAG_SD_M1,
		__ROW, 2, 0xD64, prd_get_sd_raw, prd_mod_sd_m1, prd_prt_sd_raw, prd_chk_sd_raw, 800),
	/* */
	SD_CTRL_INIT(PRD_CTRL_IDX_SD_U3_M2_RAW, "NP_M2_RAW", 0x305,
		__FMT_RAW, __FLAG_SD_M2,
		__ROW, __COL, 0xD64, prd_get_sd_raw, NULL, prd_prt_sd_raw, prd_chk_sd_raw, 0),
	SD_CTRL_INIT(PRD_CTRL_IDX_SD_U3_M2_GAP_X, "NP_M2_GAP_X", 0x305,
		__FMT_RAW, __FLAG_SD_M2_2ND | PRD_RAW_FLAG_DATA_TMP,
		__ROW, __COL, 0xD64, prd_get_sd_raw, NULL, prd_prt_sd_gap_x, prd_chk_sd_gap_x, 0),
	SD_CTRL_INIT(PRD_CTRL_IDX_SD_U3_M2_GAP_Y, "NP_M2_GAP_Y", 0x305,
		__FMT_RAW, __FLAG_SD_M2_2ND | PRD_RAW_FLAG_DATA_TMP,
		__ROW, __COL, 0xD64, prd_get_sd_raw, NULL, prd_prt_sd_gap_y, prd_chk_sd_gap_y, 0),
	SD_CTRL_INIT(PRD_CTRL_IDX_SD_U3_JITTER, "NP_M2_JITTER", 0x30D,
		__FMT_RAW, __FLAG_SD_M2,
		__ROW, __COL, 0xD64, prd_get_sd_raw, NULL, prd_prt_sd_raw, prd_chk_sd_raw, 800),
};
#define TBL_SD_CTRL	sd_ctrl_sw89112
#define NUM_SD_CTRL	ARRAY_SIZE(sd_ctrl_sw89112)
#else
#define TBL_SD_CTRL	NULL
#define NUM_SD_CTRL	0
#endif

#if defined(__ENABLE_LPWG_SD_CTRL)
static const struct lxs_prd_sd_ctrl __used lpwg_sd_ctrl_sw89112[] = {
	LPWG_SD_CTRL_INIT(PRD_CTRL_IDX_LPWG_SD_U0_M1_RAW, "LP_M1_RAW", 0x003,
		__FMT_RAW, __FLAG_LPWG_SD_M1,
		__ROW, 2, 0xD64, prd_get_sd_raw, prd_mod_sd_m1, prd_prt_sd_raw, prd_chk_sd_raw, 0),
	LPWG_SD_CTRL_INIT(PRD_CTRL_IDX_LPWG_SD_U0_M1_JITTER, "LP_M1_JITTER", 0x004,
		__FMT_RAW, __FLAG_LPWG_SD_M1,
		__ROW, 2, 0xD64, prd_get_sd_raw, prd_mod_sd_m1, prd_prt_sd_raw, prd_chk_sd_raw, 800),
	/* */
	LPWG_SD_CTRL_INIT(PRD_CTRL_IDX_LPWG_SD_U0_M2_RAW, "LP_M2_RAW", 0x005,
		__FMT_RAW, __FLAG_LPWG_SD_M2,
		__ROW, __COL, 0xD64, prd_get_sd_raw, NULL, prd_prt_sd_raw, prd_chk_sd_raw, 0),
	LPWG_SD_CTRL_INIT(PRD_CTRL_IDX_LPWG_SD_U0_JITTER, "LP_M2_JITTER", 0x00D,
		__FMT_RAW, __FLAG_LPWG_SD_M2,
		__ROW, __COL, 0xD64, prd_get_sd_raw, NULL, prd_prt_sd_raw, prd_chk_sd_raw, 800),
};
#define TBL_LPWG_SD_CTRL	lpwg_sd_ctrl_sw89112
#define NUM_LPWG_SD_CTRL	ARRAY_SIZE(lpwg_sd_ctrl_sw89112)
#else
#define TBL_LPWG_SD_CTRL	NULL
#define NUM_LPWG_SD_CTRL	0
#endif

static void __used prd_check_raw_ctrl(struct lxs_prd_data *prd, struct lxs_prd_raw_ctrl *ctrl, int num)
{
	struct lxs_prd_raw_pad *pad = ctrl->pad;
	u32 data_fmt;
	u32 flag;
	int row_size;
	int col_size;
	int i;

	for (i = 0; i < num; i++) {
		data_fmt = ctrl->data_fmt;
		flag = ctrl->flag;

		prd->raw_lookup_table[ctrl->id] = i;

		if (flag & PRD_RAW_FLAG_TBL_RXTX) {
			if (data_fmt & PRD_DATA_FMT_SYS_PDATA) {
				if (prd->sys_pdata_rx && prd->sys_pdata_tx) {
					ctrl->row_size = prd->sys_pdata_tx;
					ctrl->col_size = prd->sys_pdata_rx;
				}
			}

			if (data_fmt & PRD_DATA_FMT_SYS_PDBG) {
				if (prd->sys_pdebug_rx && prd->sys_pdebug_tx) {
					ctrl->row_size = prd->sys_pdebug_tx;
					ctrl->col_size = prd->sys_pdebug_rx;
				}
			}
		} else {
			if (prd->row_adj && prd->col_adj) {
				ctrl->row_size = prd->row_adj;
				ctrl->col_size = prd->col_adj;

				if (data_fmt & PRD_DATA_FMT_SYS_ROW)
					ctrl->row_size++;

				if (data_fmt & PRD_DATA_FMT_SYS_COL)
					ctrl->col_size++;
			}
		}

		row_size = ctrl->row_size;
		col_size = ctrl->col_size;

		if (data_fmt & PRT_DATA_FMT_DIM) {
			ctrl->row_size = col_size;
			ctrl->col_size = row_size;

			row_size = ctrl->row_size;
			col_size = ctrl->col_size;
		}

		ctrl->data_size = __cal_data_size(row_size, col_size, data_fmt);

		if (!(flag & PRD_RAW_FLAG_TBL_OFFSET)) {
			if (flag & PRD_RAW_FLAG_TYPE_SYS) {
				if (prd->sys_base)
					ctrl->offset = prd->sys_base;
			} else {
				if (prd->raw_base)
					ctrl->offset = prd->raw_base;
			}
		}

		if (pad)
			ctrl->is_pad = !!(data_fmt & PRD_DATA_FMT_PAD) & !!(pad->row || pad->col);

		ctrl->is_dim = !!(data_fmt & PRT_DATA_FMT_DIM);

		ctrl++;
	}
}

static void __used prd_check_sd_ctrl(struct lxs_prd_data *prd, struct lxs_prd_sd_ctrl *sd_ctrl, int num, int is_sd)
{
	struct lxs_prd_raw_pad *pad = sd_ctrl->pad;
	u32 data_fmt;
	u32 flag;
	int row_size;
	int col_size;
	int i;

	for (i = 0; i < num; i++) {
		data_fmt = sd_ctrl->data_fmt;
		flag = sd_ctrl->flag;

		if (is_sd) {
			prd->sd_lookup_table[sd_ctrl->id] = i;
		} else {
			prd->lpwg_sd_lookup_table[sd_ctrl->id] = i;
		}

		if (flag & PRD_RAW_FLAG_TBL_RXTX) {
			/* TBD */
		} else {
			if (prd->row_adj && prd->col_adj) {
				sd_ctrl->row_size = prd->row_adj;
				sd_ctrl->col_size = prd->col_adj;

				if (data_fmt & PRD_DATA_FMT_SYS_ROW)
					sd_ctrl->row_size++;

				if (data_fmt & PRD_DATA_FMT_SYS_COL)
					sd_ctrl->col_size++;
			}
		}

		row_size = sd_ctrl->row_size;
		col_size = sd_ctrl->col_size;

		if (data_fmt & PRT_DATA_FMT_DIM) {
			sd_ctrl->row_size = col_size;
			sd_ctrl->col_size = row_size;

			row_size = sd_ctrl->row_size;
			col_size = sd_ctrl->col_size;
		}

		sd_ctrl->data_size = __cal_data_size(row_size, col_size, data_fmt);;

		if (!(flag & PRD_RAW_FLAG_TBL_OFFSET)) {
			/* TBD */
		}

		if (pad)
			sd_ctrl->is_pad = !!(data_fmt & PRD_DATA_FMT_PAD) & !!(pad->row || pad->col);

		sd_ctrl->is_dim = !!(data_fmt & PRT_DATA_FMT_DIM);

		sd_ctrl++;
	}
}

static void prd_cmd_set(struct lxs_ts_data *ts)
{
	struct lxs_prd_data *prd = __ts_get_prd(ts);

	ts->run_prt_size = (prd->row * prd->col) * RUN_DATA_WORD_LEN;
#if defined(__PRD_SUPPORT_SD_TEST)
	ts->run_open = run_open;
	ts->run_short_mux = run_short_mux;
	ts->run_short_ch = run_short_ch;
	ts->run_np_m1_raw = run_np_m1_raw;
	ts->run_np_m1_jitter = run_np_m1_jitter;
	ts->run_np_m2_raw = run_np_m2_raw;
	ts->run_np_m2_gap_x = run_np_m2_gap_x;
	ts->run_np_m2_gap_y = run_np_m2_gap_y;
	ts->run_np_m2_jitter = run_np_m2_jitter;

	ts->run_lp_m1_raw = run_lp_m1_raw;
	ts->run_lp_m1_jitter = run_lp_m1_jitter;
	ts->run_lp_m2_raw = run_lp_m2_raw;
	ts->run_lp_m2_jitter = run_lp_m2_jitter;
#endif
}

static void prd_cmd_clr(struct lxs_ts_data *ts)
{
	ts->run_open = NULL;
	ts->run_short_mux = NULL;
	ts->run_short_ch = NULL;
	ts->run_np_m1_raw = NULL;
	ts->run_np_m1_jitter = NULL;
	ts->run_np_m2_raw = NULL;
	ts->run_np_m2_gap_x = NULL;
	ts->run_np_m2_gap_y = NULL;
	ts->run_np_m2_jitter = NULL;

	ts->run_lp_m1_raw = NULL;
	ts->run_lp_m1_jitter = NULL;
	ts->run_lp_m2_raw = NULL;
	ts->run_lp_m2_jitter = NULL;
}

static void prd_free_param(struct lxs_ts_data *ts)
{
	struct lxs_prd_data *prd = __ts_get_prd(ts);

	prd_cmd_clr(ts);

	if (prd->raw_ctrl) {
		kfree(prd->raw_ctrl);
		prd->raw_ctrl = NULL;
	}

	if (prd->sd_ctrl) {
		kfree(prd->sd_ctrl);
		prd->sd_ctrl = NULL;
	}

	if (prd->lpwg_sd_ctrl) {
		kfree(prd->lpwg_sd_ctrl);
		prd->lpwg_sd_ctrl = NULL;
	}
}

#if defined(USE_RINFO)
static void prd_init_rinfo(struct lxs_ts_data *ts)
{
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	u32 temp = 0;

	temp = ts->rinfo_data[3];
	prd->raw_base = temp >> 16; //0x5970

	temp = ts->rinfo_data[4];
	prd->sys_base = temp >> 16; //0x1AE8

	temp = ts->rinfo_data[6];
	prd->sys_cmd = temp >> 16;	//0x066B

	temp = ts->rinfo_data[9];
	prd->sys_dbg_addr = temp & 0xFFFF;	//0x06A4

	t_prd_info(prd, "param: rinfo: raw_base 0x%X, sys_base 0x%X, sys_cmd 0x%X\n",
		prd->raw_base, prd->sys_base, prd->sys_cmd);
	t_prd_info(prd, "param: rinfo: dbg_addr 0x%X\n", prd->sys_dbg_addr);
}
#endif

static int prd_init_param(struct lxs_ts_data *ts)
{
	struct lxs_ts_fw_info *fw = &ts->fw;
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	struct lxs_prd_raw_ctrl *raw_ctrl = (struct lxs_prd_raw_ctrl *)TBL_RAW_CTRL;
	struct lxs_prd_sd_ctrl *sd_ctrl = (struct lxs_prd_sd_ctrl *)TBL_SD_CTRL;
	struct lxs_prd_sd_ctrl *lpwg_sd_ctrl = (struct lxs_prd_sd_ctrl *)TBL_LPWG_SD_CTRL;
	void *buf = NULL;
	int size;
	int i;

	if (t_prd_dbg_flag & PRD_DBG_FLAG_DISABLE)
		goto out;

	prd->panel_type = 0;

	prd->self = 1;
	if (ts->rx_count && ts->tx_count) {
		prd->row = ts->tx_count;
		prd->col = ts->rx_count;

		prd->row_adj = prd->row;
		prd->col_adj = prd->col;
	} else {
		prd->row = __ROW;
		prd->col = __COL;

		ts->tx_count = prd->row;
		ts->rx_count = prd->col;
	}
	prd->ch = prd->col;

	if (!prd->row || !prd->col)
		return -EINVAL;

	t_prd_info(prd, "param: row %d, col %d\n", prd->row, prd->col);

	prd->sys_cmd = 0x274;

#if defined(USE_RINFO)
	prd_init_rinfo(ts);
#else
	t_prd_info(prd, "param: rinfo: sys_cmd 0x%X\n", prd->sys_cmd);
#endif

	for (i = 0 ; i < PRD_CTRL_IDX_MAX; i++)
		prd->raw_lookup_table[i] = -1;

	for (i = 0 ; i < PRD_CTRL_IDX_SD_MAX; i++)
		prd->sd_lookup_table[i] = -1;

	for (i = 0 ; i < PRD_CTRL_IDX_LPWG_SD_MAX; i++)
		prd->lpwg_sd_lookup_table[i] = -1;

	if (raw_ctrl) {
		prd->num_raw_ctrl = NUM_RAW_CTRL;
		size = sizeof(struct lxs_prd_raw_ctrl) * prd->num_raw_ctrl;
		buf = kzalloc(size, GFP_KERNEL);
		if (!buf) {
			t_prd_err(prd, "%s\n", "failed to allocate prd->raw_ctrl");
			goto out;
		}
		prd->raw_ctrl = buf;
		memcpy(prd->raw_ctrl, raw_ctrl, size);
		prd_check_raw_ctrl(prd, prd->raw_ctrl, prd->num_raw_ctrl);
	}

	if (sd_ctrl) {
		prd->num_sd_ctrl = NUM_SD_CTRL;
		size = sizeof(struct lxs_prd_sd_ctrl) * prd->num_sd_ctrl;
		buf = kzalloc(size, GFP_KERNEL);
		if (!buf) {
			t_prd_err(prd, "%s\n", "failed to allocate prd->sd_ctrl");
			goto out;
		}
		prd->sd_ctrl = buf;
		memcpy(prd->sd_ctrl, sd_ctrl, size);
		prd_check_sd_ctrl(prd, prd->sd_ctrl, prd->num_sd_ctrl, 1);
	}

	if (lpwg_sd_ctrl) {
		prd->num_lpwg_sd_ctrl = NUM_LPWG_SD_CTRL;
		size = sizeof(struct lxs_prd_sd_ctrl) * prd->num_lpwg_sd_ctrl;
		buf = kzalloc(size, GFP_KERNEL);
		if (!buf) {
			t_prd_err(prd, "%s\n", "failed to allocate prd->lpwg_sd_ctrl");
			goto out;
		}
		prd->lpwg_sd_ctrl = buf;
		memcpy(prd->lpwg_sd_ctrl, lpwg_sd_ctrl, size);
		prd_check_sd_ctrl(prd, prd->lpwg_sd_ctrl, prd->num_lpwg_sd_ctrl, 0);
	}

	prd->sysfs_flag = ~0;

	if (!prd->num_raw_ctrl && !prd->num_sd_ctrl && !prd->num_lpwg_sd_ctrl)
		return -ENODEV;

#if defined(__PRD_TEST_LOG_OFF)
	t_prd_info(prd, "%s\n", "test: raw log off");
	t_prd_dbg_flag |= PRD_DBG_FLAG_RAW_LOG_OFF;
#endif

	prd_cmd_set(ts);

	t_prd_info(prd, "[%s] param init done\n", fw->product_id);

	return 0;

out:
	prd_free_param(ts);

	t_prd_info(prd, "[%s] param init failed\n", fw->product_id);

	return -EFAULT;
}

static struct lxs_prd_data *prd_alloc(struct lxs_ts_data *ts)
{
	struct lxs_prd_data *prd;

	prd = kzalloc(sizeof(*prd), GFP_KERNEL);
	if (!prd) {
		input_err(true, &ts->client->dev, "%s: failed to allocated memory\n", __func__);
		goto out;
	}

	prd->ts = ts;

	prd->dev = &ts->client->dev;

	__ts_set_prd(ts, prd);

	mutex_init(&prd->lock);

	return prd;

out:
	return NULL;
}

static void prd_free(struct lxs_ts_data *ts)
{
	struct lxs_prd_data *prd = __ts_get_prd(ts);

	if (prd) {
		__ts_set_prd(ts, NULL);

		mutex_destroy(&prd->lock);

		kfree(prd);
	}
}

static int prd_create(struct lxs_ts_data *ts)
{
	struct lxs_prd_data *prd = __ts_get_prd(ts);
	int ret = 0;

	if (prd != NULL)
		return 0;

	prd = prd_alloc(ts);
	if (!prd)
		return -ENOMEM;

	ret = prd_init_param(ts);
	/* Just skip sysfs generation */
	if (ret < 0)
		return 0;

	ret = lxs_prd_create_group(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: prd sysfs register failed, %d\n",
			__func__, ret);
		goto out_sysfs;
	}

	input_info(true, &ts->client->dev, "%s: prd syfs regiser done\n", __func__);

	prd->sysfs_done = 1;

	return 0;

out_sysfs:
	prd_free_param(ts);

	prd_free(ts);

	return ret;
}

static void prd_remove(struct lxs_ts_data *ts)
{
	struct lxs_prd_data *prd = __ts_get_prd(ts);

	if (prd == NULL)
		return;

	if (prd->sysfs_done) {
		lxs_prd_remove_group(ts);
		prd_free_param(ts);
	}

	prd_free(ts);

	input_info(true, &ts->client->dev, "%s: prd sysfs unregister done\n", __func__);
}

void lxs_ts_prd_init(struct lxs_ts_data *ts)
{
	if (atomic_read(&ts->boot) == TC_IC_BOOT_FAIL)
		return;

	prd_create(ts);
}

void lxs_ts_prd_remove(struct lxs_ts_data *ts)
{
	prd_remove(ts);
}

MODULE_LICENSE("GPL");

