/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/string.h>

#include "fm_typedef.h"
#include "fm_rds.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_stdlib.h"

/* static rds_ps_state_machine_t ps_state_machine = RDS_PS_START; */
/* static rds_rt_state_machine_t rt_state_machine = RDS_RT_START; */
struct fm_state_machine {
	fm_s32 state;
	fm_s32 (*state_get)(struct fm_state_machine *thiz);
	fm_s32 (*state_set)(struct fm_state_machine *thiz, fm_s32 new_state);
};

static fm_s32 fm_state_get(struct fm_state_machine *thiz)
{
	return thiz->state;
}

static fm_s32 fm_state_set(struct fm_state_machine *thiz, fm_s32 new_state)
{
	return thiz->state = new_state;
}

#define STATE_SET(a, s)        \
{                           \
	if ((a)->state_set) {          \
		(a)->state_set((a), (s));    \
	}                       \
}

#define STATE_GET(a)         \
({                             \
	fm_s32 __ret = 0;              \
	if ((a)->state_get) {          \
		__ret = (a)->state_get((a));    \
	}                       \
	__ret;                  \
})

static fm_u16 (*rds_get_freq)(void);
static fm_u32 g_rt_stable_counter;
static fm_u32 g_rt_stable_flag;
static fm_u32 g_rtp_updated;
static fm_u32 g_rtp_not_update_counter;
/* RDS spec related handle flow */
/*
 * rds_cnt_get
 * To get rds group count form raw data
 * If success return 0, else return error code
*/
static fm_s32 rds_cnt_get(struct rds_rx_t *rds_raw, fm_s32 raw_size, fm_s32 *cnt)
{
	fm_s32 gap = sizeof(rds_raw->cos) + sizeof(rds_raw->sin);

	if (rds_raw == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (cnt == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	*cnt = (raw_size - gap) / sizeof(rds_packet_t);
	WCN_DBG(FM_INF | RDSC, "group cnt=%d\n", *cnt);

	return 0;
}

/*
 * rds_grp_get
 * To get rds group[n] data form raw data with index
 * If success return 0, else return error code
*/
static fm_s32 rds_grp_get(fm_u16 *dst, struct rds_rx_t *raw, fm_s32 idx)
{
	if (dst == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (raw == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (idx > (MAX_RDS_RX_GROUP_CNT - 1))
		return -FM_EPARA;

	dst[0] = raw->data[idx].blkA;
	dst[1] = raw->data[idx].blkB;
	dst[2] = raw->data[idx].blkC;
	dst[3] = raw->data[idx].blkD;
	dst[4] = raw->data[idx].crc;
	dst[5] = raw->data[idx].cbc;

	WCN_DBG(FM_NTC | RDSC, "BLOCK:%04x %04x %04x %04x, CRC:%04x CBC:%04x\n", dst[0], dst[1],
		dst[2], dst[3], dst[4], dst[5]);

	return 0;
}

/*
 * rds_checksum_check
 * To check CRC rerult, if OK, *valid=fm_true, else *valid=fm_false
 * If success return 0, else return error code
*/
static fm_s32 rds_checksum_check(fm_u16 crc, fm_s32 mask, fm_bool *valid)
{
	if (valid == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if ((crc & mask) == mask)
		*valid = fm_true;
	else
		*valid = fm_false;

	return 0;
}

/*
 * rds_cbc_get - To get block_n's correct bit count form cbc
 * @cbc, the group's correct bit count
 * @blk, target the block
 *
 * If success, return block_n's cbc, else error code
*/
/*
static fm_s32 rds_cbc_get(fm_u16 cbc, enum rds_blk_t blk)
{
    int ret = 0;

    switch (blk) {
    case RDS_BLK_A:
	ret = (cbc & 0xF000) >> 12;
	break;
    case RDS_BLK_B:
	ret = (cbc & 0x0F00) >> 8;
	break;
    case RDS_BLK_C:
	ret = (cbc & 0x00F0) >> 4;
	break;
    case RDS_BLK_D:
	ret = (cbc & 0x000F) >> 0;
	break;
    default:
	break;
    }

    WCN_DBG(FM_INF | RDSC, "group cbc=0x%04x\n", cbc);
    return ret;
}
*/
/*
 * rds_event_set
 * To set rds event, and user space can use this flag to juge which event happened
 * If success return 0, else return error code
*/
static fm_s32 rds_event_set(fm_u16 *events, fm_s32 event_mask)
{
	if (events == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	WCN_DBG(FM_NTC | RDSC, "rds set event[%x->%x]\n", event_mask, *events);
	*events |= event_mask;

	return 0;
}

/*
 * rds_flag_set
 * To set rds event flag, and user space can use this flag to juge which event happened
 * If success return 0, else return error code
*/
static fm_s32 rds_flag_set(fm_u32 *flags, fm_s32 flag_mask)
{
	if (flags == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	*flags |= flag_mask;
	WCN_DBG(FM_NTC | RDSC, "rds set flag[%x->%x]\n", flag_mask, *flags);

	return 0;
}

/*
 * rds_grp_type_get
 * To get rds group type form blockB
 * If success return 0, else return error code
*/
static fm_s32 rds_grp_type_get(fm_u16 crc, fm_u16 blk, fm_u8 *type, fm_u8 *subtype)
{
	fm_bool valid = fm_false;

	if (type == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (subtype == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	/* to get the group type from block B */
	rds_checksum_check(crc, FM_RDS_GDBK_IND_B, &valid);

	if (valid == fm_true) {
		*type = (blk & 0xF000) >> 12;	/* Group type(4bits) */
		*subtype = (blk & 0x0800) >> 11;	/* version code(1bit), 0=vesionA, 1=versionB */
	} else {
		WCN_DBG(FM_WAR | RDSC, "Block1 CRC err\n");
		return -FM_ECRC;
	}

	WCN_DBG(FM_DBG | RDSC, "Type=%d, subtype:%s\n", (fm_s32) *type, *subtype ? "version B" : "version A");
	return 0;
}

/*
 * rds_grp_counter_add
 * @type -- group type, rang: 0~15
 * @subtype -- sub group type, rang:0~1
 *
 * add group counter, g0a~g15b
 * we use type value as the index
 * If success return 0, else return error code
*/
static fm_s32 rds_grp_counter_add(fm_u8 type, fm_u8 subtype, struct rds_group_cnt_t *gc)
{
	if (gc == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (type > 15)
		return -FM_EPARA;

	switch (subtype) {
	case RDS_GRP_VER_A:
		gc->groupA[type]++;
		break;
	case RDS_GRP_VER_B:
		gc->groupB[type]++;
		break;
	default:
		return -FM_EPARA;
	}

	gc->total++;
	WCN_DBG(FM_INF | RDSC, "group counter:%d\n", (fm_s32) gc->total);
	return 0;
}

/*
 * rds_grp_counter_get
 *
 * read group counter , g0a~g15b
 * If success return 0, else return error code
*/
extern fm_s32 rds_grp_counter_get(struct rds_group_cnt_t *dst, struct rds_group_cnt_t *src)
{
	if (dst == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (src == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	fm_memcpy(dst, src, sizeof(struct rds_group_cnt_t));
	WCN_DBG(FM_DBG | RDSC, "rds gc get[total=%d]\n", (fm_s32) dst->total);
	return 0;
}

/*
 * rds_grp_counter_reset
 *
 * clear group counter to 0, g0a~g15b
 * If success return 0, else return error code
*/
extern fm_s32 rds_grp_counter_reset(struct rds_group_cnt_t *gc)
{
	if (gc == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	fm_memset(gc, 0, sizeof(struct rds_group_cnt_t));
	return 0;
}

extern fm_s32 rds_log_in(struct rds_log_t *thiz, struct rds_rx_t *new_log, fm_s32 new_len)
{
	if (new_log == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}


		new_len = (new_len < sizeof(struct rds_rx_t)) ? new_len : sizeof(struct rds_rx_t);
		fm_memcpy(&(thiz->rds_log[thiz->in]), new_log, new_len);
		thiz->log_len[thiz->in] = new_len;
		thiz->in = (thiz->in + 1) % thiz->size;
		thiz->len++;
		thiz->len = (thiz->len >= thiz->size) ? thiz->size : thiz->len;
		WCN_DBG(FM_DBG | RDSC, "add a new log[len=%d]\n", thiz->len);


	return 0;
}

extern fm_s32 rds_log_out(struct rds_log_t *thiz, struct rds_rx_t *dst, fm_s32 *dst_len)
{
	if (dst == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dst_len == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (thiz->len > 0) {
		*dst_len = thiz->log_len[thiz->out];
		*dst_len = (*dst_len < sizeof(struct rds_rx_t)) ? *dst_len : sizeof(struct rds_rx_t);
		fm_memcpy(dst, &(thiz->rds_log[thiz->out]), *dst_len);
		thiz->out = (thiz->out + 1) % thiz->size;
		thiz->len--;
		WCN_DBG(FM_DBG | RDSC, "del a new log[len=%d]\n", thiz->len);
	} else {
		*dst_len = 0;
		WCN_DBG(FM_WAR | RDSC, "rds log buf is empty\n");
	}

	return 0;
}

/*
 * rds_grp_pi_get
 * To get rds group pi code form blockA
 * If success return 0, else return error code
*/
static fm_s32 rds_grp_pi_get(fm_u16 crc, fm_u16 blk, fm_u16 *pi, fm_bool *dirty)
{
	fm_s32 ret = 0;
	fm_bool valid = fm_false;

	if (pi == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* to get the group pi code from block A */
	ret = rds_checksum_check(crc, FM_RDS_GDBK_IND_A, &valid);

	if (valid == fm_true) {
		if (*pi != blk) {
			/* PI=program Identication */
			*pi = blk;
			*dirty = fm_true;	/* yes, we got new PI code */
		} else {
			*dirty = fm_false;	/* PI is the same as last one */
		}
	} else {
		WCN_DBG(FM_WAR | RDSC, "Block0 CRC err\n");
		return -FM_ECRC;
	}

	WCN_DBG(FM_NTC | RDSC, "PI=0x%04x, %s\n", *pi, *dirty ? "new" : "old");
	return ret;
}

/*
 * rds_grp_pty_get
 * To get rds group pty code form blockB
 * If success return 0, else return error code
*/
static fm_s32 rds_grp_pty_get(fm_u16 crc, fm_u16 blk, fm_u8 *pty, fm_bool *dirty)
{
	fm_s32 ret = 0;
/* fm_bool valid = fm_false; */

	if (pty == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* to get PTY code from block B */
/* ret = rds_checksum_check(crc, FM_RDS_GDBK_IND_B, &valid); */

/* if (valid == fm_false) { */
/* WCN_DBG(FM_WAR | RDSC, "Block1 CRC err\n"); */
/* return -FM_ECRC; */
/* } */

	if (*pty != ((blk & 0x03E0) >> 5)) {
		/* PTY=Program Type Code */
		*pty = (blk & 0x03E0) >> 5;
		*dirty = fm_true;	/* yes, we got new PTY code */
	} else {
		*dirty = fm_false;	/* PTY is the same as last one */
	}

	WCN_DBG(FM_INF | RDSC, "PTY=%d, %s\n", (fm_s32) *pty, *dirty ? "new" : "old");
	return ret;
}

/*
 * rds_grp_tp_get
 * To get rds group tp code form blockB
 * If success return 0, else return error code
*/
static fm_s32 rds_grp_tp_get(fm_u16 crc, fm_u16 blk, fm_u8 *tp, fm_bool *dirty)
{
	fm_s32 ret = 0;
/* fm_bool valid = fm_false; */

	if (tp == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* to get TP code from block B */
/* ret = rds_checksum_check(crc, FM_RDS_GDBK_IND_B, &valid); */

/* if (valid == fm_false) { */
/* WCN_DBG(FM_WAR | RDSC, "Block1 CRC err\n"); */
/* return -FM_ECRC; */
/* } */

	if (*tp != ((blk & 0x0400) >> 10)) {
		/* Tranfic Program Identification */
		*tp = (blk & 0x0400) >> 10;
		*dirty = fm_true;	/* yes, we got new TP code */
	} else {
		*dirty = fm_false;	/* TP is the same as last one */
	}

	WCN_DBG(FM_NTC | RDSC, "TP=%d, %s\n", (fm_s32)*tp, *dirty ? "new" : "old");
	return ret;
}

/*
 * rds_g0_ta_get
 * To get rds group ta code form blockB
 * If success return 0, else return error code
*/
static fm_s32 rds_g0_ta_get(fm_u16 blk, fm_u8 *ta, fm_bool *dirty)
{
	fm_s32 ret = 0;

	if (ta == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* TA=Traffic Announcement code */
	if (*ta != ((blk & 0x0010) >> 4)) {
		*ta = (blk & 0x0010) >> 4;
		*dirty = fm_true;	/* yes, we got new TA code */
	} else {
		*dirty = fm_false;	/* TA is the same as last one */
	}

	WCN_DBG(FM_INF | RDSC, "TA=%d, %s\n", (fm_s32) *ta, *dirty ? "new" : "old");
	return ret;
}

/*
 * rds_g0_music_get
 * To get music-speech switch code form blockB
 * If success return 0, else return error code
*/
static fm_s32 rds_g0_music_get(fm_u16 blk, fm_u8 *music, fm_bool *dirty)
{
	fm_s32 ret = 0;

	if (music == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* M/S=music speech switch code */
	if (*music != ((blk & 0x0008) >> 3)) {
		*music = (blk & 0x0008) >> 3;
		*dirty = fm_true;	/* yes, we got new music code */
	} else {
		*dirty = fm_false;	/* music  is the same as last one */
	}

	WCN_DBG(FM_INF | RDSC, "Music=%d, %s\n", (fm_s32) *music, *dirty ? "new" : "old");
	return ret;
}

/*
 * rds_g0_ps_addr_get
 * To get ps addr form blockB, blkB b0~b1
 * If success return 0, else return error code
*/
static fm_s32 rds_g0_ps_addr_get(fm_u16 blkB, fm_u8 *addr)
{
	if (addr == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	*addr = (fm_u8) blkB & 0x03;

	WCN_DBG(FM_INF | RDSC, "addr=0x%02x\n", *addr);
	return 0;
}

/*
 * rds_g0_di_flag_get
 * To get DI segment flag form blockB, blkB b2
 * If success return 0, else return error code
*/
static fm_s32 rds_g0_di_flag_get(fm_u16 blkB, fm_u8 *flag)
{
	if (flag == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	*flag = (fm_u8) ((blkB & 0x0004) >> 2);

	WCN_DBG(FM_INF | RDSC, "flag=0x%02x\n", *flag);
	return 0;
}

static fm_s32 rds_g0_ps_get(fm_u16 crc, fm_u16 blkD, fm_u8 addr, fm_u8 *buf)
{
/* fm_bool valid = fm_false; */
	fm_s32 idx = 0;

	if (buf == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* ps segment addr rang 0~3 */
	if (addr > 0x03) {
		WCN_DBG(FM_ERR | RDSC, "addr invalid(0x%02x)\n", addr);
		return -FM_EPARA;
	}

	idx = 2 * addr;
	buf[idx] = blkD >> 8;
	buf[idx + 1] = blkD & 0xFF;
#if 0
	rds_checksum_check(crc, FM_RDS_GDBK_IND_D, &valid);

	if (valid == fm_true) {
		buf[idx] = blkD >> 8;
		buf[idx + 1] = blkD & 0xFF;
	} else {
		WCN_DBG(FM_ERR | RDSC, "ps crc check err\n");
		return -FM_ECRC;
	}
#endif

	WCN_DBG(FM_NTC | RDSC, "PS:addr[%02x]:0x%02x 0x%02x\n", addr, buf[idx], buf[idx + 1]);
	return 0;
}

/*
 * rds_g0_ps_cmp
 * this function is the most importent flow for PS parsing
 * 1.Compare fresh buf with once buf per byte, if eque copy this byte to twice buf, else copy it to once buf
 * 2.Check wether we got a full segment
 * If success return 0, else return error code
*/
static fm_s32 rds_g0_ps_cmp(fm_u8 addr, fm_u16 cbc, fm_u8 *fresh,
				fm_u8 *once, fm_u8 *twice, fm_u8 *bm, fm_u8 *once_bm, fm_u8 *firstCollectionDone)
{
	fm_s32 ret = 0, indx;

	fm_u8 PS_Num;
	/* fm_u8 corrBitCnt_BlkB, corrBitCnt_BlkD; */
	static fm_s8 Pre_PS_Num = -1;

	if (fresh == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (once == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (twice == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (addr > 3) {		/* ps limited in 8 chars */
		WCN_DBG(FM_NTC | RDSC, "PS Address error, addr=%x\n", addr);
		return -1;
	}

	PS_Num = addr;
	for (indx = 0; indx < 2; indx++) {

		if (once[2 * PS_Num + indx] == fresh[2 * PS_Num + indx]) {
			/* set once bimmap 1 */
			*once_bm |= 1 << (2 * PS_Num + indx);

			if (twice[2 * PS_Num + indx] == once[2 * PS_Num + indx]) {
				/* set twice bitmap 1 */
				*bm  |= 1 << (2 * PS_Num + indx);
			} else {
				/* set twice bitmap */
				*bm &= ~(1 << (2 * PS_Num + indx));
				/* memcpy from once to twice */
				twice[2 * PS_Num + indx] = once[2 * PS_Num + indx];
		}

		} else {
			/* set once bitmap */
			*once_bm &= ~(1 << (2 * PS_Num + indx));
			/* memcpy from fresh to once */
			once[2 * PS_Num + indx] = fresh[2 * PS_Num + indx];
		}
	}


	WCN_DBG(FM_NTC | RDSC, "current firstCollection = %x, addr=%d. Prev_PS_NUM = %d\n",
		*firstCollectionDone, addr, Pre_PS_Num);
	*firstCollectionDone |= 1 << PS_Num;

	Pre_PS_Num = PS_Num;

	WCN_DBG(FM_NTC | RDSC, "firstCollection=%x, addr = %x\n", *firstCollectionDone, addr);
	WCN_DBG(FM_NTC | RDSC, "PS[0]=%x %x %x %x %x %x %x %x\n", fresh[0], fresh[1], fresh[2],
		fresh[3], fresh[4], fresh[5], fresh[6], fresh[7]);
	WCN_DBG(FM_NTC | RDSC, "PS[1]=%x %x %x %x %x %x %x %x\n", once[0], once[1], once[2],
		once[3], once[4], once[5], once[6], once[7]);
	WCN_DBG(FM_NTC | RDSC, "PS[2]=%x %x %x %x %x %x %x %x\n", twice[0], twice[1], twice[2],
		twice[3], twice[4], twice[5], twice[6], twice[7]);
	return ret;
}

/*
 * rds_g2_rt_addr_get
 * To get rt addr form blockB
 * If success return 0, else return error code
*/
static fm_s32 rds_g2_rt_addr_get(fm_u16 blkB, fm_u8 *addr)
{
	fm_s32 ret = 0;

	if (addr == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	*addr = (fm_u8) blkB & 0x0F;

	WCN_DBG(FM_INF | RDSC, "addr=0x%02x\n", *addr);
	return ret;
}

static fm_s32 rds_g2_txtAB_get(fm_u16 blk, fm_u8 *txtAB, fm_bool *dirty)
{
	fm_s32 ret = 0;
	static fm_bool once_dirty = fm_false;

	if (txtAB == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	*dirty = fm_false;
	if (*txtAB != ((blk & 0x0010) >> 4)) {
		if (once_dirty) {
			*txtAB = (blk & 0x0010) >> 4;
			*dirty = fm_true;	/* yes, we got new txtAB code */
			once_dirty = fm_false;
			WCN_DBG(FM_NTC | RDSC, "changed! txtAB=%d\n", *txtAB);
			return ret;
		}
		once_dirty = fm_true;
	} else {
		once_dirty = fm_false;	/* txtAB is the same as last one */
	}

	WCN_DBG(FM_INF | RDSC, "txtAB=%d, %s\n", *txtAB, *dirty ? "new" : "old");
	return ret;
}

static fm_s32 rds_g2_rt_get(fm_u16 crc, fm_u8 subtype, fm_u16 blkC, fm_u16 blkD, fm_u8 addr, fm_u8 *buf)
{
	fm_s32 ret = 0;
	fm_bool valid = fm_false;
	fm_s32 idx = 0;

	if (buf == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	/* text segment addr rang 0~15 */
	if (addr > 0x0F) {
		WCN_DBG(FM_ERR | RDSC, "addr invalid(0x%02x)\n", addr);
		ret = -FM_EPARA;
		return ret;
	}

	switch (subtype) {
	case RDS_GRP_VER_A:
		idx = 4 * addr;
		ret = rds_checksum_check(crc, FM_RDS_GDBK_IND_C | FM_RDS_GDBK_IND_D, &valid);

		if (valid == fm_true) {
			buf[idx] = blkC >> 8;
			buf[idx + 1] = blkC & 0xFF;
			buf[idx + 2] = blkD >> 8;
			buf[idx + 3] = blkD & 0xFF;
		} else {
			WCN_DBG(FM_ERR | RDSC, "rt crc check err\n");
			ret = -FM_ECRC;
		}

		break;
	case RDS_GRP_VER_B:
		idx = 2 * addr;
		ret = rds_checksum_check(crc, FM_RDS_GDBK_IND_D, &valid);

		if (valid == fm_true) {
			buf[idx] = blkD >> 8;
			buf[idx + 1] = blkD & 0xFF;
		} else {
			WCN_DBG(FM_ERR | RDSC, "rt crc check err\n");
			ret = -FM_ECRC;
		}

		break;
	default:
		break;
	}

	WCN_DBG(FM_NTC | RDSC, "fresh addr[%02x]:0x%02x%02x 0x%02x%02x\n", addr, buf[idx],
		buf[idx + 1], buf[idx + 2], buf[idx + 3]);
	return ret;
}
static fm_s32 rds_g2_rt_cmp_new(fm_u8 addr, fm_u16 cbc, fm_u8 subtype,
				fm_u8 *fresh, fm_u8 *once, fm_u8 *twice,
				fm_u8 *bitmap_once, fm_u8 *bitmap_twice,
				fm_u16 *firstCollectionDone)
{
	fm_s32 ret = 0;
	fm_s32 seg_width = 0;
	fm_s32 i = 0;
	fm_s32 x = 0, y = 0;
	if (fresh == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (once == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (twice == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (bitmap_once == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (bitmap_twice == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (firstCollectionDone == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	seg_width = (subtype == RDS_GRP_VER_A) ? 4 : 2; /* RT segment width */
#if 0
	if (addr == 0) {
		*firstCollectionDone = 0;
		WCN_DBG(FM_NTC  | RDSC, "start from addr zero");
	}
	*firstCollectionDone |= 1 << addr;
#endif
	for (i = 0; i < seg_width ; i++) {
		x = (seg_width * addr + i) / 8;
		y = (seg_width * addr + i) % 8;
		WCN_DBG(FM_INF | RDSC, "addr * seg_width + i = %d, x = %d, y = %d\n", seg_width * addr + i, x, y);

		if (fresh[seg_width * addr + i] == once[seg_width * addr + i]) {
			/* set bitmap_once 1 */
			bitmap_once[x] |= 1 << y;
			if (twice[seg_width * addr + i] == once[seg_width * addr + i]) {
				/* set bitmap_twice 1 */
				bitmap_twice[x] |= 1 << y;
			} else {
				/* set bitmap_twice 0 */
				bitmap_twice[x] &= ~(1 << y);
				/* memcpy from once to twice */
				twice[seg_width * addr + i] = once[seg_width * addr + i];
				WCN_DBG(FM_NTC | RDSC, "twice[%d]= 0x%02x\n",
						seg_width * addr + i, twice[seg_width * addr + i]);
			}

		} else {
			bitmap_once[x] &= ~(1 << y);
			bitmap_twice[x] &= ~(1 << y);
			once[seg_width * addr + i] = fresh[seg_width * addr + i];	/* use new val */
			WCN_DBG(FM_INF | RDSC, "once[%d]= 0x%02x\n", seg_width * addr + i, once[seg_width * addr + i]);
		}
	}

	return ret;

}

/*
 * rds_g2_rt_check_end
 * check 0x0D end flag
 * If we got the end, then caculate the RT length
 * If success return 0, else return error code
*/
static fm_s32 rds_g2_rt_check_end(fm_u8 addr, fm_u8 subtype, fm_u8 *twice, fm_bool *end, fm_u32 *rt_len)
{
	fm_s32 index = 0;
	fm_s32 seg_width = 0;

	if (twice == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (end == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	seg_width = (subtype == RDS_GRP_VER_A) ? 4 : 2;	/* RT segment width */
	*end = fm_false;
	for (index = 0; index < seg_width; index++) {
			/* if we got 0x0D twice, it means a RT end */
		if (twice[seg_width * addr + index] == 0x0D) {
			*rt_len = seg_width * addr + index + 1;
			*end = fm_true;
			WCN_DBG(FM_NTC | RDSC, "get 0x0D, addr = %d, index = %d, rt_len = %d\n", addr, index, *rt_len);
			break;
		}
	}


	return 0;
}

static fm_s32 rds_retrieve_g0_af(fm_u16 *block_data, fm_u8 SubType, rds_t *pstRDSData)
{
	static fm_s16 preAF_Num;
	fm_u8 indx, indx2, AF_H, AF_L, num;
	fm_s16 temp_H, temp_L;
	fm_s32 ret = 0;
	fm_bool valid = fm_false;
	fm_bool dirty = fm_false;
	fm_u16 *event = &pstRDSData->event_status;
	fm_u32 *flag = &pstRDSData->RDSFlag.flag_status;

/* ret = rds_checksum_check(block_data[4], FM_RDS_GDBK_IND_D, &valid); */

/* if (valid == fm_false) { */
/* WCN_DBG(FM_WAR | RDSC, "Group0 BlockD crc err\n"); */
/* return -FM_ECRC; */
/* } */

	ret = rds_g0_ta_get(block_data[1], &pstRDSData->RDSFlag.TA, &dirty);

	if (ret) {
		WCN_DBG(FM_WAR | RDSC, "get ta failed[ret=%d]\n", ret);
	} else if (dirty == fm_true) {
		ret = rds_event_set(event, RDS_EVENT_FLAGS);	/* yes, we got new TA code */
		ret = rds_flag_set(flag, RDS_FLAG_IS_TA);
	}

	ret = rds_g0_music_get(block_data[1], &pstRDSData->RDSFlag.Music, &dirty);

	if (ret) {
		WCN_DBG(FM_WAR | RDSC, "get music failed[ret=%d]\n", ret);
	} else if (dirty == fm_true) {
		ret = rds_event_set(event, RDS_EVENT_FLAGS);	/* yes, we got new MUSIC code */
		ret = rds_flag_set(flag, RDS_FLAG_IS_MUSIC);
	}

	if ((pstRDSData->Switch_TP) && (pstRDSData->RDSFlag.TP) && !(pstRDSData->RDSFlag.TA))
		ret = rds_event_set(event, RDS_EVENT_TAON_OFF);

	if (SubType)	/* Type B no AF information */
		goto out;

	/* Type A */

	ret = rds_checksum_check(block_data[4], FM_RDS_GDBK_IND_C, &valid);

	if (valid == fm_false) {
		WCN_DBG(FM_WAR | RDSC, "Group0 BlockC crc err\n");
		return -FM_ECRC;
	}

	AF_H = (block_data[2] & 0xFF00) >> 8;
	AF_L = block_data[2] & 0x00FF;

	if ((AF_H > 224) && (AF_H < 250)) {
		/* Followed AF Number, see RDS spec Table 11, valid(224-249) */
		WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 AF_H:%d, AF_L:%d\n", AF_H, AF_L);
		preAF_Num = AF_H - 224;	/* AF Number */

		if (preAF_Num != pstRDSData->AF_Data.AF_Num) {
			pstRDSData->AF_Data.AF_Num = preAF_Num;
			pstRDSData->AF_Data.isAFNum_Get = 0;
		} else {
			/* Get the same AFNum two times */
			pstRDSData->AF_Data.isAFNum_Get = 1;
		}

		if ((AF_L < 205) && (AF_L > 0)) {
			/* See RDS Spec table 10, valid VHF */
			pstRDSData->AF_Data.AF[0][0] = AF_L + 875;	/* convert to 100KHz */

			pstRDSData->AF_Data.AF[0][0] *= 10;

			WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 AF[0][0]:%d\n",
				pstRDSData->AF_Data.AF[0][0]);

			if ((pstRDSData->AF_Data.AF[0][0]) != (pstRDSData->AF_Data.AF[1][0])) {
				pstRDSData->AF_Data.AF[1][0] = pstRDSData->AF_Data.AF[0][0];
			} else {
				if (pstRDSData->AF_Data.AF[1][0] != rds_get_freq())
					pstRDSData->AF_Data.isMethod_A = 1;
				else
					pstRDSData->AF_Data.isMethod_A = 0;
			}

			WCN_DBG(FM_NTC | RDSC,
				"RetrieveGroup0 isAFNum_Get:%d, isMethod_A:%d\n",
				pstRDSData->AF_Data.isAFNum_Get, pstRDSData->AF_Data.isMethod_A);

			/* only one AF handle */
			if ((pstRDSData->AF_Data.isAFNum_Get)
			    && (pstRDSData->AF_Data.AF_Num == 1)) {
				pstRDSData->AF_Data.Addr_Cnt = 0xFF;
				pstRDSData->event_status |= RDS_EVENT_AF_LIST;
				WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 RDS_EVENT_AF_LIST update\n");
			}
		}
	} else if ((pstRDSData->AF_Data.isAFNum_Get)
		   && (pstRDSData->AF_Data.Addr_Cnt != 0xFF)) {
		/* AF Num correct */
		num = pstRDSData->AF_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		num = num >> 1;
		WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 +num:%d\n", num);

		/* Put AF freq fm_s32o buffer and check if AF freq is repeat again */
		for (indx = 1; indx < (num + 1); indx++) {
			if ((AF_H == (pstRDSData->AF_Data.AF[0][2 * indx - 1] / 10 - 875))
			    && (AF_L == (pstRDSData->AF_Data.AF[0][2 * indx] / 10 - 875))) {
				WCN_DBG(FM_ERR | RDSC, "RetrieveGroup0 AF same as indx:%d\n", indx);
				break;
			} else if (!(pstRDSData->AF_Data.AF[0][2 * indx - 1])) {
				/* null buffer */
				/* convert to 100KHz */
				pstRDSData->AF_Data.AF[0][2 * indx - 1] = AF_H + 875;
				pstRDSData->AF_Data.AF[0][2 * indx] = AF_L + 875;

				pstRDSData->AF_Data.AF[0][2 * indx - 1] *= 10;
				pstRDSData->AF_Data.AF[0][2 * indx] *= 10;

				WCN_DBG(FM_NTC | RDSC,
					"RetrieveGroup0 AF[0][%d]:%d, AF[0][%d]:%d\n",
					2 * indx - 1,
					pstRDSData->AF_Data.AF[0][2 * indx - 1],
					2 * indx, pstRDSData->AF_Data.AF[0][2 * indx]);
				break;
			}
		}

		num = pstRDSData->AF_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 ++num:%d\n", num);

		if (num <= 0)
			goto out;

		if ((pstRDSData->AF_Data.AF[0][num - 1]) == 0)
			goto out;

		num = num >> 1;
		WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 +++num:%d\n", num);

		/* arrange frequency from low to high:start */
		for (indx = 1; indx < num; indx++) {
			for (indx2 = indx + 1; indx2 < (num + 1); indx2++) {
				temp_H = pstRDSData->AF_Data.AF[0][2 * indx - 1];
				temp_L = pstRDSData->AF_Data.AF[0][2 * indx];

				if (temp_H > (pstRDSData->AF_Data.AF[0][2 * indx2 - 1])) {
					pstRDSData->AF_Data.AF[0][2 * indx - 1] =
					    pstRDSData->AF_Data.AF[0][2 * indx2 - 1];
					pstRDSData->AF_Data.AF[0][2 * indx] =
					    pstRDSData->AF_Data.AF[0][2 * indx2];
					pstRDSData->AF_Data.AF[0][2 * indx2 - 1] = temp_H;
					pstRDSData->AF_Data.AF[0][2 * indx2] = temp_L;
				} else if (temp_H == (pstRDSData->AF_Data.AF[0][2 * indx2 - 1])) {
					if (temp_L > (pstRDSData->AF_Data.AF[0][2 * indx2])) {
						pstRDSData->AF_Data.AF[0][2 * indx - 1] =
						    pstRDSData->AF_Data.AF[0][2 * indx2 - 1];
						pstRDSData->AF_Data.AF[0][2 * indx] =
						    pstRDSData->AF_Data.AF[0][2 * indx2];
						pstRDSData->AF_Data.AF[0][2 * indx2 - 1] = temp_H;
						pstRDSData->AF_Data.AF[0][2 * indx2] = temp_L;
					}
				}
			}
		}

		/* arrange frequency from low to high:end */
		/* compare AF buff0 and buff1 data:start */
		num = pstRDSData->AF_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		indx2 = 0;

		for (indx = 0; indx < num; indx++) {
			if ((pstRDSData->AF_Data.AF[1][indx]) == (pstRDSData->AF_Data.AF[0][indx])) {
				if (pstRDSData->AF_Data.AF[1][indx] != 0)
					indx2++;
			} else
				pstRDSData->AF_Data.AF[1][indx] = pstRDSData->AF_Data.AF[0][indx];
		}

		WCN_DBG(FM_NTC | RDSC, "RetrieveGroup0 indx2:%d, num:%d\n", indx2, num);

		/* compare AF buff0 and buff1 data:end */
		if (indx2 == num) {
			pstRDSData->AF_Data.Addr_Cnt = 0xFF;
			pstRDSData->event_status |= RDS_EVENT_AF_LIST;
			WCN_DBG(FM_NTC | RDSC,
				"RetrieveGroup0 AF_Num:%d\n",
				pstRDSData->AF_Data.AF_Num);

			for (indx = 0; indx < num; indx++) {
				if ((pstRDSData->AF_Data.AF[1][indx]) == 0) {
					pstRDSData->AF_Data.Addr_Cnt = 0x0F;
					pstRDSData->event_status &= (~RDS_EVENT_AF_LIST);
				}
			}
		} else
			pstRDSData->AF_Data.Addr_Cnt = 0x0F;
	}

out:	return ret;
}

static fm_s32 rds_retrieve_g0_di(fm_u16 *block_data, fm_u8 SubType, rds_t *pstRDSData)
{
	fm_u8 DI_Code, DI_Flag;
	fm_s32 ret = 0;
/* fm_bool valid = fm_false; */

	fm_u16 *event = &pstRDSData->event_status;
	fm_u32 *flag = &pstRDSData->RDSFlag.flag_status;

	/* parsing Program service name segment (in BlockD) */
/* ret = rds_checksum_check(block_data[4], FM_RDS_GDBK_IND_D, &valid); */

/* if (valid == fm_false) { */
/* WCN_DBG(FM_WAR | RDSC, "Group0 BlockD crc err\n"); */
/* return -FM_ECRC; */
/* } */

	rds_g0_ps_addr_get(block_data[1], &DI_Code);
	rds_g0_di_flag_get(block_data[1], &DI_Flag);

	switch (DI_Code) {
	case 3:

		if (pstRDSData->RDSFlag.Stereo != DI_Flag) {
			pstRDSData->RDSFlag.Stereo = DI_Flag;
			ret = rds_event_set(event, RDS_EVENT_FLAGS);
			ret = rds_flag_set(flag, RDS_FLAG_IS_STEREO);
		}

		break;
	case 2:

		if (pstRDSData->RDSFlag.Artificial_Head != DI_Flag) {
			pstRDSData->RDSFlag.Artificial_Head = DI_Flag;
			ret = rds_event_set(event, RDS_EVENT_FLAGS);
			ret = rds_flag_set(flag, RDS_FLAG_IS_ARTIFICIAL_HEAD);
		}

		break;
	case 1:

		if (pstRDSData->RDSFlag.Compressed != DI_Flag) {
			pstRDSData->RDSFlag.Compressed = DI_Flag;
			ret = rds_event_set(event, RDS_EVENT_FLAGS);
			ret = rds_flag_set(flag, RDS_FLAG_IS_COMPRESSED);
		}

		break;
	case 0:

		if (pstRDSData->RDSFlag.Dynamic_PTY != DI_Flag) {
			pstRDSData->RDSFlag.Dynamic_PTY = DI_Flag;
			ret = rds_event_set(event, RDS_EVENT_FLAGS);
			ret = rds_flag_set(flag, RDS_FLAG_IS_DYNAMIC_PTY);
		}

		break;
	default:
		break;
	}

	return ret;
}

static fm_s32 rds_retrieve_g0_ps(fm_u16 *block_data, fm_u8 SubType, rds_t *pstRDSData)
{
	fm_u8 ps_addr;
	fm_s32 ret = 0, i = 0;
	fm_bool valid = fm_false;

	static fm_u8 bm_once;
	static struct fm_state_machine ps_sm = {
		.state = RDS_PS_START,
		.state_get = fm_state_get,
		.state_set = fm_state_set,
	};
	static fm_u8 collectFirstStringDone;

	fm_u16 *event = &pstRDSData->event_status;

	/* parsing Program service name segment (in BlockD) */
	ret = rds_checksum_check(block_data[4], FM_RDS_GDBK_IND_D, &valid);

	if (valid == fm_false) {
		WCN_DBG(FM_WAR | RDSC, "Group0 BlockD crc err\n");
		return -FM_ECRC;
	}

	rds_g0_ps_addr_get(block_data[1], &ps_addr);

	/* PS parsing state machine run */
	while (1) {
		switch (STATE_GET(&ps_sm)) {
		case RDS_PS_START:
			if (rds_g0_ps_get(block_data[4], block_data[3], ps_addr, pstRDSData->PS_Data.PS[0])) {
				STATE_SET(&ps_sm, RDS_PS_FINISH);	/* if CRC error, we should not do parsing */
				break;
			}

			rds_g0_ps_cmp(ps_addr, block_data[5], pstRDSData->PS_Data.PS[0],
					pstRDSData->PS_Data.PS[1], pstRDSData->PS_Data.PS[2],
					&pstRDSData->PS_Data.Addr_Cnt, &bm_once, &collectFirstStringDone);
			WCN_DBG(FM_NTC | RDSC, "PS[3]=%x %x %x %x %x %x %x %x\n",
					pstRDSData->PS_Data.PS[3][0],
					pstRDSData->PS_Data.PS[3][1],
					pstRDSData->PS_Data.PS[3][2],
					pstRDSData->PS_Data.PS[3][3],
					pstRDSData->PS_Data.PS[3][4],
					pstRDSData->PS_Data.PS[3][5],
					pstRDSData->PS_Data.PS[3][6],
					pstRDSData->PS_Data.PS[3][7]);

			STATE_SET(&ps_sm, RDS_PS_DECISION);
			break;
		case RDS_PS_DECISION:

			if (collectFirstStringDone == 0x0F || pstRDSData->PS_Data.Addr_Cnt == 0xFF || bm_once == 0xFF) {
				STATE_SET(&ps_sm, RDS_PS_GETLEN);
				WCN_DBG(FM_DBG | RDSC, "collecction = 0x%2x, addr = 0x%2x, bm_once = 0x%2x\n",
						collectFirstStringDone, pstRDSData->PS_Data.Addr_Cnt, bm_once);
			} else {
				STATE_SET(&ps_sm, RDS_PS_FINISH);
			}

			break;
		case RDS_PS_GETLEN:

			for (i = 0; i < 8; i++) {
				if (pstRDSData->PS_Data.Addr_Cnt & (1 << i))
					pstRDSData->PS_Data.PS[3][i] =  pstRDSData->PS_Data.PS[2][i];
				else if (bm_once & (1 << i))
					pstRDSData->PS_Data.PS[3][i] =  pstRDSData->PS_Data.PS[1][i];
				else if (collectFirstStringDone & (1 << (i / 2)))
					pstRDSData->PS_Data.PS[3][i] =  pstRDSData->PS_Data.PS[0][i];
			}

			if ((collectFirstStringDone == 0x0F)
					&& !(bm_once == 0xFF)
					&& !(pstRDSData->PS_Data.Addr_Cnt == 0xFF)) {

				collectFirstStringDone = 0;
			} else if ((bm_once == 0xFF) && !(pstRDSData->PS_Data.Addr_Cnt == 0xFF)) {
				collectFirstStringDone = 0;
				bm_once = 0;
			} else if (pstRDSData->PS_Data.Addr_Cnt == 0xFF) {
				bm_once = 0;
				collectFirstStringDone = 0;
			}

			rds_event_set(event, RDS_EVENT_PROGRAMNAME);
			WCN_DBG(FM_NTC | RDSC, "get an PS!\n");

			WCN_DBG(FM_NTC | RDSC, "PS[3]=%x %x %x %x %x %x %x %x\n",
				pstRDSData->PS_Data.PS[3][0],
				pstRDSData->PS_Data.PS[3][1],
				pstRDSData->PS_Data.PS[3][2],
				pstRDSData->PS_Data.PS[3][3],
				pstRDSData->PS_Data.PS[3][4],
				pstRDSData->PS_Data.PS[3][5],
				pstRDSData->PS_Data.PS[3][6],
				pstRDSData->PS_Data.PS[3][7]);
			STATE_SET(&ps_sm, RDS_PS_FINISH);
			break;
		case RDS_PS_FINISH:
			STATE_SET(&ps_sm, RDS_PS_START);
			goto out;
		default:
			break;
		}
	}

out:
	return ret;
}

static fm_s32 rds_retrieve_g0(fm_u16 *block_data, fm_u8 SubType, rds_t *pstRDSData)
{
	fm_s32 ret = 0;

	ret = rds_retrieve_g0_af(block_data, SubType, pstRDSData);

	if (ret)
		return ret;

	ret = rds_retrieve_g0_di(block_data, SubType, pstRDSData);

	if (ret)
		return ret;

	ret = rds_retrieve_g0_ps(block_data, SubType, pstRDSData);

	if (ret)
		return ret;

	return ret;
}

static fm_s32 rds_ecc_get(fm_u16 blk, fm_u8 *ecc, fm_bool *dirty)
{
	fm_s32 ret = 0;
/* fm_bool valid = fm_false; */

	if (ecc == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (dirty == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}

	if (*ecc != (blk & 0xFF)) {
		*ecc = (fm_u8)blk & 0xFF;
		*dirty = fm_true;	/* yes, we got new ecc code */
	} else {
		*dirty = fm_false;	/* ecc is the same as last one */
	}

	WCN_DBG(FM_NTC | RDSC, "ecc=%02x, %s\n", *ecc, *dirty ? "new" : "old");
	return ret;
}


static fm_s32 rds_retrieve_g1(fm_u16 *block_data, fm_u8 SubType, rds_t *pstRDSData)
{
	fm_u8 variant_code = (block_data[2] & 0x7000) >> 12;
	fm_s32 ret = 0;
	fm_bool dirty = fm_false;

	WCN_DBG(FM_NTC | RDSC, "variant code:%d\n", variant_code);

	if (variant_code == 0) {
		ret = rds_ecc_get(block_data[2], &pstRDSData->Extend_Country_Code, &dirty);
		if (!ret) {
			if (dirty == fm_true)
				rds_event_set(&pstRDSData->event_status, RDS_EVENT_ECC_CODE);
		} else
			WCN_DBG(FM_ERR | RDSC, "get ecc fail(%d)\n", ret);
		WCN_DBG(FM_DBG | RDSC, "Extend_Country_Code:%d\n", pstRDSData->Extend_Country_Code);
	} else if (variant_code == 3) {
		pstRDSData->Language_Code = block_data[2] & 0xFFF;
		WCN_DBG(FM_DBG | RDSC, "Language_Code:%d\n", pstRDSData->Language_Code);
	}

	pstRDSData->Radio_Page_Code = block_data[1] & 0x001F;
	pstRDSData->Program_Item_Number_Code = block_data[3];

	return ret;
}

static fm_s32 rds_g2_rt_one_group_change(fm_u8 *src, fm_u8 *dst, fm_u8 addr)
{
	fm_u32 idx;
	fm_u8 temp[4];

	idx = addr * 4;
	memset(temp, 0x20, 4);
#if 0
	if (!memcmp(temp, &src[idx], 4) ||
		!memcmp(temp, &dst[idx], 4)) {
		WCN_DBG(FM_NTC | RDSC, "src or dst is space\n");
		return 0;
	}
#endif
	if (!memcmp(temp, &src[idx], 4) &&
		!memcmp(temp, &dst[idx], 4)) {
		WCN_DBG(FM_NTC | RDSC, "src and dst are space\n");
		return 1;
	}
	if (memcmp(&src[idx], &dst[idx], 4))
		return 1;

	return 0;
}

static fm_s32 rds_retrieve_g2(fm_u16 *source, fm_u8 subtype, rds_t *target)
{
	fm_s32 ret = 0;
	fm_u16 crc, cbc;
	fm_u16 blkA, blkB, blkC, blkD;
	fm_u8 *fresh, *once, *twice, *display;
	fm_u8 temp[64];
	fm_u16 *event;
	fm_u32 *flag;
	fm_u16 i = 0, x = 0, y = 0;
	static struct fm_state_machine rt_sm = {
		.state = RDS_RT_START,
		.state_get = fm_state_get,
		.state_set = fm_state_set,
	};


	fm_u8 rt_addr = 0;
	/* text AB flag 0 --> 1 or 1-->0 meas new RT incoming */
	fm_bool txtAB_change = fm_false;
	fm_bool txt_fresh_end = fm_false;
	fm_bool txt_once_end = fm_false;
	fm_bool txt_twice_end = fm_false;

	fm_s32 indx = 0;
	fm_s32 seg_width = 0;
	fm_s32 rt_fresh_len = 0;
	fm_s32 rt_once_len = 0;
	fm_s32 rt_twice_len = 0;
	fm_s32 bufsize = 0;
	static fm_u8 bitmap_once[8];
	static fm_u8 bitmap_twice[8];
	fm_u8 bitmap_complete[8];
	static fm_u16 rt_firstCollectDone;
	static fm_u8 group_counter;

	if (source == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	if (target == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	/* source */
	blkA = source[0];
	blkB = source[1];
	blkC = source[2];
	blkD = source[3];
	crc = source[4];
	cbc = source[5];
	if (cbc) {
		WCN_DBG(FM_NTC | RDSC, "%s, cbc=0x%4x, return\n", __func__, cbc);
		return 0;
	}
	WCN_DBG(FM_NTC | RDSC, "%s,blkA=0x%4x,blkB=0x%4x,blkC=0x%4x,blkD=0x%4x,crc=0x%4x,cbc=0x%4x\n",
		__func__, blkA, blkB, blkC, blkD, crc, cbc);
	/* target */
	fresh = target->RT_Data.TextData[0];
	once = target->RT_Data.TextData[1];
	twice = target->RT_Data.TextData[2];
	display = target->RT_Data.TextData[3];
	event = &target->event_status;
	flag = &target->RDSFlag.flag_status;
	bufsize = sizeof(target->RT_Data.TextData[0]);

	group_counter++;

	for (i = 0; i < 8; i++)
		bitmap_complete[i] = 0xFF;

	/* get basic info: addr, txtAB */
	if (rds_g2_rt_addr_get(blkB, &rt_addr))
		return ret;

	if (rt_addr == 0)
		group_counter = 0;

	if (rds_g2_rt_get(crc, subtype, blkC, blkD, rt_addr, fresh) != 0) {
		WCN_DBG(FM_NTC | RDSC, "%s, crc error, drop this\n", __func__);
		return ret;
	}
	if (rds_g2_txtAB_get(blkB, &target->RDSFlag.Text_AB, &txtAB_change))
		return ret;
	if (txtAB_change == fm_true) {
		/* clear buf */
		fm_memset(fresh, 0x20, bufsize);
		fm_memset(once, 0x20, bufsize);
		fm_memset(twice, 0x20, bufsize);
		target->RT_Data.isRTDisplay = 0;

		fm_memset(bitmap_once, 0, sizeof(bitmap_once) / sizeof(fm_u8));
		fm_memset(bitmap_twice, 0, sizeof(bitmap_twice) / sizeof(fm_u8));

		rt_firstCollectDone = 0;
		WCN_DBG(FM_NTC | RDSC, "%s, txtAB_change changed! clear bitmap, target->RT_Data.isRTDisplay = %d\n",
				__func__, target->RT_Data.isRTDisplay);
		g_rt_stable_counter = 0;
		g_rt_stable_flag = 0;
	}

	if (rds_g2_rt_one_group_change(fresh, display, rt_addr)) {
		/* some issue need to fix or this control may be not needed */
		rt_firstCollectDone |= 1 << rt_addr;
		WCN_DBG(FM_NTC | RDSC, "one group changed(%x)\n", rt_firstCollectDone);
	}
	fm_memset(temp, 0x20, 64);
	/* RT parsing state machine run */
	while (1) {
		switch (STATE_GET(&rt_sm)) {
		case RDS_RT_START:
		{
			if (rds_g2_rt_get(crc, subtype, blkC, blkD, rt_addr, fresh) == 0)
				rds_g2_rt_cmp_new(rt_addr, cbc, subtype, fresh, once, twice,
					bitmap_once, bitmap_twice, &rt_firstCollectDone);

			rds_g2_rt_check_end(rt_addr, subtype, fresh, &txt_fresh_end, &rt_fresh_len);
			rds_g2_rt_check_end(rt_addr, subtype, once, &txt_once_end, &rt_once_len);
			rds_g2_rt_check_end(rt_addr, subtype, twice, &txt_twice_end, &rt_twice_len);
			WCN_DBG(FM_NTC | RDSC, "fresh_len = %d, once_len =%d, twice_len = %d\n",
				rt_fresh_len, rt_once_len, rt_twice_len);
			if (!txt_fresh_end && !txt_once_end) {
				txt_twice_end = fm_false;
				rt_twice_len = 0;
			}
			STATE_SET(&rt_sm, RDS_RT_DECISION);
		break;
		}
		case RDS_RT_DECISION:
		{
			seg_width = (subtype == RDS_GRP_VER_A) ? 4 : 2;
			WCN_DBG(FM_NTC | RDSC, "fresh_end = %d, once_end = %d, twice_end = %d\n",
				txt_fresh_end, txt_once_end, txt_twice_end);

			WCN_DBG(FM_NTC | RDSC, "width(%d), firstcd(0x%4x), bp_once(%d), bp_twice(%d)\n",
				seg_width,
				rt_firstCollectDone,
				memcmp(&bitmap_once, &bitmap_complete, sizeof(fm_u8) * 8),
				memcmp(&bitmap_twice, &bitmap_complete, sizeof(fm_u8) * 8));
			WCN_DBG(FM_NTC | RDSC, "rt_addr:%d, group_counter:%d\n",
				rt_addr, group_counter);

			STATE_SET(&rt_sm, RDS_RT_GETLEN);

		break;
		}
		case RDS_RT_GETLEN:
		{
			for (indx = 0; indx < 0x0F; indx++) {
				for (i = 0; i < seg_width; i++) {
					x = (indx * seg_width + i) / 8;
					y = (indx * seg_width + i) % 8;
					if (bitmap_twice[x] & (1 << y))
						temp[indx * seg_width + i] = twice[indx * seg_width + i];
					else if (bitmap_once[x] & (1 << y))
						temp[indx * seg_width + i] = once[indx * seg_width + i];
					else if (rt_firstCollectDone & (1 << indx))
						temp[indx * seg_width + i] = fresh[indx * seg_width + i];
					else
						temp[indx * seg_width + i] = fresh[indx * seg_width + i];
				}
			}

			if ((txt_twice_end && txt_once_end && txt_fresh_end)
				|| memcmp(&bitmap_twice, &bitmap_complete, sizeof(fm_u8) * 2 * seg_width) == 0) {
				target->RT_Data.isRTDisplay = 4;
				g_rt_stable_flag = 1;
				WCN_DBG(FM_NTC | RDSC, "target->RT_Data.isRTDisplay = 4\n");
			} else if ((txt_once_end && txt_fresh_end)
				|| memcmp(&bitmap_once, &bitmap_complete, sizeof(fm_u8) * 2 * seg_width) == 0) {
				target->RT_Data.isRTDisplay = 3;
				g_rt_stable_flag = 1;
				WCN_DBG(FM_NTC | RDSC, "target->RT_Data.isRTDisplay = 3\n");
			} else if (txt_fresh_end || rt_firstCollectDone == 0xFFFF) {
				target->RT_Data.isRTDisplay = 2;
				WCN_DBG(FM_NTC | RDSC, "target->RT_Data.isRTDisplay = 2\n");
				rt_firstCollectDone = 0;
				g_rt_stable_flag = 0;
			} else if ((rt_addr == 0xf) && (group_counter >= 8)) {
				target->RT_Data.isRTDisplay = 1;
				g_rt_stable_flag = 0;
				WCN_DBG(FM_NTC | RDSC, "target->RT_Data.isRTDisplay = 1\n");
			} else {
				target->RT_Data.isRTDisplay = 0;
				rt_firstCollectDone = 0;
				g_rt_stable_flag = 0;
				WCN_DBG(FM_NTC | RDSC, "target->RT_Data.isRTDisplay = 0\n");
			}



			if (memcmp(display, temp, bufsize)) {
				memcpy(display, temp, bufsize);

				if (rt_twice_len)
					target->RT_Data.TextLength = rt_twice_len;
				else if (rt_once_len)
					target->RT_Data.TextLength = rt_once_len;
				else if (rt_fresh_len)
					target->RT_Data.TextLength = rt_fresh_len;
				else
					target->RT_Data.TextLength = 16 * seg_width;

				rds_event_set(event, RDS_EVENT_LAST_RADIOTEXT);

				/* yes we got a new RT */
				WCN_DBG(FM_NTC | RDSC, "Yes, get an RT[len=%d], isRTDisplay = %d, string:%s\n",
					target->RT_Data.TextLength,
					target->RT_Data.isRTDisplay,
					display);
				g_rt_stable_counter = 0;
			} else {
				g_rt_stable_counter++;
				WCN_DBG(FM_NTC | RDSC, "RT NOT changed(%d)\n", g_rt_stable_counter);
			}

			STATE_SET(&rt_sm, RDS_RT_FINISH);
			break;
		}
		case RDS_RT_FINISH:
		{
			STATE_SET(&rt_sm, RDS_RT_START);
			goto out;
		}
		default:
			break;
		}
	}

out:

	return ret;
}

static fm_s32 rds_retrieve_g3(fm_u16 *block_data, fm_u8 SubType, fm_s16 *aid, fm_u8 *ODAgroup, rds_t *pstRDSData)
{
	fm_s32 ret = 0;
	fm_s16 AID = 0;
	fm_u8 ODA_group = 0;
	fm_u8 RT_flag = 0;
	fm_u8 CB_flag = 0;
	fm_u8 SCB = 0;
	fm_u8 Template_number = 0;
	fm_bool valid = fm_false;

	ret = rds_checksum_check(block_data[4], FM_RDS_GDBK_IND_C | FM_RDS_GDBK_IND_D, &valid);
	if (valid == fm_false) {
		WCN_DBG(FM_WAR | RDSC, "Group3 BlockC/D crc err\n");
		return -FM_ECRC;
	}

	if (!SubType) {
		AID = block_data[3];
		WCN_DBG(FM_NTC | RDSC, "[sss]get AID = %04x =[int] %d\n", AID, AID);
		*aid = AID;

		if (AID == 0x4BD7) {

			RT_flag = (block_data[2] & 0x2000) >> 13;
			WCN_DBG(FM_NTC | RDSC, "get RT flag = %d\n", RT_flag);
			if (RT_flag == 0)
				WCN_DBG(FM_NTC | RDSC, "RT+ use RT group\n");
			else if (RT_flag == 1)
				WCN_DBG(FM_NTC | RDSC, "RT+ use eRT group\n");
			else
				WCN_DBG(FM_WAR | RDSC, "invalid RT flag\n");

			CB_flag = (block_data[2] & 0x1000) >> 12;
			WCN_DBG(FM_NTC | RDSC, "get CB flag = %d\n", CB_flag);
			if (CB_flag == 1) {
				WCN_DBG(FM_NTC | RDSC, "use SCB and template\n");
				SCB = (block_data[2] & 0x0F00) >> 8;
				Template_number = (block_data[2] & 0x00FF);
				WCN_DBG(FM_NTC | RDSC, "SCB=%d,template_number=%d\n", SCB, Template_number);
			} else if (CB_flag == 0)
				WCN_DBG(FM_NTC | RDSC, "not use SCB and template\n");
			else
				WCN_DBG(FM_WAR | RDSC, "invalid CB flag\n");

			ODA_group = (block_data[1] & 0x001E) >> 1;
			if (ODA_group == 0x0) {
				WCN_DBG(FM_NTC | RDSC, "NO inforamtion ODA group\n");
				*ODAgroup = 0;
			} else {
				*ODAgroup = ODA_group;
				WCN_DBG(FM_NTC | RDSC, "RT+ tag group : %dA\n", ODA_group);
			}
		}
	} else
		WCN_DBG(FM_NTC | RDSC, "No AID infor in group B\n");

	return ret;
}

static fm_s32 rds_retrieve_goda(fm_u16 *block_data, fm_u8 SubType, fm_s16 AID, rds_t *pstRDSData)
{
	fm_s32 ret = 0;
	fm_bool valid = fm_false;
	fm_u8 toggle, running, content1, startpos1, len1, content2, startpos2, len2;
	struct rds_rtp_t temp;

	WCN_DBG(FM_NTC | RDSC, "%s enter\n", __func__);

	ret = rds_checksum_check(block_data[4], FM_RDS_GDBK_IND_C | FM_RDS_GDBK_IND_D, &valid);
	if (valid == fm_false) {
		WCN_DBG(FM_WAR | RDSC, "BlockC/D crc err\n");
		return -FM_ECRC;
	}
	WCN_DBG(FM_NTC | RDSC, "%s after rds_checksum_check\n", __func__);

	switch (AID) {
	case 0x4BD7:
		WCN_DBG(FM_NTC | RDSC, "do RT+ infor parser\n");

		if (!SubType) {
			toggle = (block_data[1] & 0x0010) >> 4;
			running = (block_data[1] & 0x0008) >> 3;
			WCN_DBG(FM_NTC | RDSC, "RT+ toggle(%d), running(%d)\n", toggle, running);

			content1 = (block_data[1] & 0x0007) << 3;
			content1 |= (block_data[2] & 0xE000) >> 13;
			startpos1 = (block_data[2] & 0x1F80) >> 7;
			len1 = (block_data[2] & 0x007E) >> 1;
			WCN_DBG(FM_NTC | RDSC, "content type1(%d),start_pos1(%d),len1(%d)\n",
							content1, startpos1, len1);

			content2 = (block_data[2] & 0x0001) << 5;
			content2 |= (block_data[3] & 0xF800) >> 11;
			startpos2 = (block_data[3] & 0x07E0) >> 5;
			len2 = (block_data[3] & 0x001F);
			WCN_DBG(FM_NTC | RDSC, "content type2(%d),start_pos2(%d),len2(%d)\n",
							content2, startpos2, len2);

			temp.running = running;
			temp.toggle = toggle;
			temp.rtp_tag[0].content_type = content1;
			temp.rtp_tag[0].start_pos = startpos1;
			temp.rtp_tag[0].additional_len = len1;
			temp.rtp_tag[1].content_type = content2;
			temp.rtp_tag[1].start_pos = startpos2;
			temp.rtp_tag[1].additional_len = len2;

			if (memcmp(&temp, &pstRDSData->RTP_Data, sizeof(struct rds_rtp_t))) {
				fm_memcpy(&pstRDSData->RTP_Data, &temp, sizeof(struct rds_rtp_t));
				g_rtp_updated = 1;
				WCN_DBG(FM_NTC | RDSC, "RT+ updated!\n");
			} else {
				g_rtp_not_update_counter++;
				WCN_DBG(FM_NTC | RDSC, "RT+ tag NO update(%d)\n", g_rtp_not_update_counter);
			}
			if (((g_rt_stable_counter >= 3) || g_rt_stable_flag)) {
				if (g_rtp_updated) { /* handling the rtp repeat slow case */
					rds_event_set(&pstRDSData->event_status, RDS_EVENT_RTP_TAG);
					WCN_DBG(FM_NTC | RDSC, "get an new RTP,rtp updated(%d/%d)\n",
					g_rt_stable_counter, g_rt_stable_flag);
					g_rtp_updated = 0;
				}
				if (g_rtp_not_update_counter >= 2) { /* handling rtp block by HAL check fail */
					rds_event_set(&pstRDSData->event_status, RDS_EVENT_RTP_TAG);
					WCN_DBG(FM_NTC | RDSC, "rtp not updated for long(%d/%d/%d)\n",
					g_rt_stable_counter, g_rt_stable_flag, g_rtp_not_update_counter);
					g_rtp_not_update_counter = 0;
				}
			} else {
				WCN_DBG(FM_NTC | RDSC, "rt not stable(%d/%d)\n",
				g_rt_stable_counter, g_rt_stable_flag);
			}
		} else
			WCN_DBG(FM_NTC | RDSC, "No RT+ infor in group B\n");
		break;
	default:
		WCN_DBG(FM_WAR | RDSC, "not support AID(0x%04x)\n", AID);
		break;
	}

	return 0;
}

static fm_s32 rds_retrieve_g4(fm_u16 *block_data, fm_u8 SubType, rds_t *pstRDSData)
{
	fm_u16 year, month, k = 0, D2, minute;
	fm_u32 MJD, D1;
	fm_s32 ret = 0;

	WCN_DBG(FM_DBG | RDSC, "RetrieveGroup4 %d\n", SubType);

	if (!SubType) {
		/* Type A */
		if ((block_data[4] & FM_RDS_GDBK_IND_C) && (block_data[4] & FM_RDS_GDBK_IND_D)) {
			MJD = (fm_u32) (((block_data[1] & 0x0003) << 15) + ((block_data[2] & 0xFFFE) >> 1));
			year = (MJD * 100 - 1507820) / 36525;
			month = (MJD * 10000 - 149561000 - 3652500 * year) / 306001;

			if ((month == 14) || (month == 15))
				k = 1;

			D1 = (fm_u32) ((36525 * year) / 100);
			D2 = (fm_u16) ((306001 * month) / 10000);
			pstRDSData->CT.Year = 1900 + year + k;
			pstRDSData->CT.Month = month - 1 - k * 12;
			pstRDSData->CT.Day = (fm_u16) (MJD - 14956 - D1 - D2);
			pstRDSData->CT.Hour = ((block_data[2] & 0x0001) << 4) + ((block_data[3] & 0xF000) >> 12);
			minute = (block_data[3] & 0x0FC0) >> 6;

			if (block_data[3] & 0x0020)
				pstRDSData->CT.Local_Time_offset_signbit = 1;	/* 0=+, 1=- */

			pstRDSData->CT.Local_Time_offset_half_hour = block_data[3] & 0x001F;

			if (pstRDSData->CT.Minute != minute) {
				pstRDSData->CT.Minute = (block_data[3] & 0x0FC0) >> 6;
				pstRDSData->event_status |= RDS_EVENT_UTCDATETIME;
			}
		}
	}

	return ret;
}

static fm_s32 rds_retrieve_g14(fm_u16 *block_data, fm_u8 SubType, rds_t *pstRDSData)
{
	static fm_s16 preAFON_Num;
	fm_u8 TP_ON, TA_ON, PI_ON, PS_Num, AF_H, AF_L, indx, indx2, num;
	fm_s32 ret = 0;
	fm_s16 temp_H, temp_L;

	WCN_DBG(FM_DBG | RDSC, "RetrieveGroup14 %d\n", SubType);
	/* SubType = (*(block_data+1)&0x0800)>>11; */
	PI_ON = block_data[3];
	TP_ON = block_data[1] & 0x0010;

	if ((!SubType) && (block_data[4] & FM_RDS_GDBK_IND_C)) {
		/* Type A */
		PS_Num = block_data[1] & 0x000F; /* variant code */

		if (PS_Num < 4) { /* variant code = 0~3 represent PS */
			for (indx = 0; indx < 2; indx++) {
				pstRDSData->PS_ON[2 * PS_Num] = block_data[2] >> 8;
				pstRDSData->PS_ON[2 * PS_Num + 1] = block_data[2] & 0xFF;
			}

			goto out;
		} else if (PS_Num > 4)	/* variant code > 4 */
			goto out;

		/* variant code = 4 represent AF(ON) */

		AF_H = (block_data[2] & 0xFF00) >> 8;
		AF_L = block_data[2] & 0x00FF;

		if ((AF_H > 224) && (AF_H < 250)) {
			/* Followed AF Number */
			pstRDSData->AFON_Data.isAFNum_Get = 0;
			preAFON_Num = AF_H - 224;

			if (pstRDSData->AFON_Data.AF_Num != preAFON_Num)
				pstRDSData->AFON_Data.AF_Num = preAFON_Num;
			else
				pstRDSData->AFON_Data.isAFNum_Get = 1;

			if (AF_L < 205) {
				pstRDSData->AFON_Data.AF[0][0] = AF_L + 875;
				pstRDSData->AFON_Data.AF[0][0] *= 10;

				if ((pstRDSData->AFON_Data.AF[0][0]) != (pstRDSData->AFON_Data.AF[1][0]))
					pstRDSData->AFON_Data.AF[1][0] = pstRDSData->AFON_Data.AF[0][0];
				else
					pstRDSData->AFON_Data.isMethod_A = 1;
			}

			goto out;
		}

		if (!(pstRDSData->AFON_Data.isAFNum_Get) || ((pstRDSData->AFON_Data.Addr_Cnt) == 0xFF))
			goto out;

		/* AF Num correct */
		num = pstRDSData->AFON_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		num = num >> 1;

		/* Put AF freq fm_s32o buffer and check if AF freq is repeat again */
		for (indx = 1; indx < (num + 1); indx++) {
			if ((AF_H == (pstRDSData->AFON_Data.AF[0][2 * indx - 1] / 10 - 875))
			    && (AF_L == (pstRDSData->AFON_Data.AF[0][2 * indx] / 10 - 875))) {
				WCN_DBG(FM_NTC | RDSC, "RetrieveGroup14 AFON same as indx:%d\n", indx);
				break;
			} else if (!(pstRDSData->AFON_Data.AF[0][2 * indx - 1])) {
				/* null buffer */
				pstRDSData->AFON_Data.AF[0][2 * indx - 1] = AF_H + 875;
				pstRDSData->AFON_Data.AF[0][2 * indx] = AF_L + 875;
				pstRDSData->AFON_Data.AF[0][2 * indx - 1] *= 10;
				pstRDSData->AFON_Data.AF[0][2 * indx] *= 10;
				WCN_DBG(FM_NTC | RDSC,
					"RetrieveGroup14 AF[0][%d]:%d, AF[0][%d]:%d\n",
					2 * indx - 1,
					pstRDSData->AFON_Data.AF[0][2 * indx - 1],
					2 * indx, pstRDSData->AFON_Data.AF[0][2 * indx]);
				break;
			}
		}

		num = pstRDSData->AFON_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		if (num <= 0)
			goto out;

		if ((pstRDSData->AFON_Data.AF[0][num - 1]) == 0)
			goto out;

		num = num >> 1;
		/* arrange frequency from low to high:start */
		for (indx = 1; indx < num; indx++) {
			for (indx2 = indx + 1; indx2 < (num + 1); indx2++) {
				temp_H = pstRDSData->AFON_Data.AF[0][2 * indx - 1];
				temp_L = pstRDSData->AFON_Data.AF[0][2 * indx];

				if (temp_H > (pstRDSData->AFON_Data.AF[0][2 * indx2 - 1])) {
					pstRDSData->AFON_Data.AF[0][2 * indx - 1] =
					    pstRDSData->AFON_Data.AF[0][2 * indx2 - 1];
					pstRDSData->AFON_Data.AF[0][2 * indx] =
					    pstRDSData->AFON_Data.AF[0][2 * indx2];
					pstRDSData->AFON_Data.AF[0][2 * indx2 - 1] =
					    temp_H;
					pstRDSData->AFON_Data.AF[0][2 * indx2] = temp_L;
				} else if (temp_H == (pstRDSData->AFON_Data.AF[0][2 * indx2 - 1])) {
					if (temp_L > (pstRDSData->AFON_Data.AF[0][2 * indx2])) {
						pstRDSData->AFON_Data.AF[0][2 * indx - 1] =
						    pstRDSData->AFON_Data.AF[0][2 * indx2 - 1];
						pstRDSData->AFON_Data.AF[0][2 * indx] =
						    pstRDSData->AFON_Data.AF[0][2 * indx2];
						pstRDSData->AFON_Data.AF[0][2 * indx2 - 1] = temp_H;
						pstRDSData->AFON_Data.AF[0][2 * indx2] = temp_L;
					}
				}
			}
		}

		/* arrange frequency from low to high:end */
		/* compare AF buff0 and buff1 data:start */
		num = pstRDSData->AFON_Data.AF_Num;
		num = (num > 25) ? 25 : num;
		indx2 = 0;

		for (indx = 0; indx < num; indx++) {
			if ((pstRDSData->AFON_Data.AF[1][indx]) == (pstRDSData->AFON_Data.AF[0][indx])) {
				if (pstRDSData->AFON_Data.AF[1][indx] != 0)
					indx2++;
			} else
				pstRDSData->AFON_Data.AF[1][indx] = pstRDSData->AFON_Data.AF[0][indx];
		}

		/* compare AF buff0 and buff1 data:end */
		if (indx2 == num) {
			pstRDSData->AFON_Data.Addr_Cnt = 0xFF;
			pstRDSData->event_status |= RDS_EVENT_AFON_LIST;

			for (indx = 0; indx < num; indx++) {
				if ((pstRDSData->AFON_Data.AF[1][indx]) == 0) {
					pstRDSData->AFON_Data.Addr_Cnt = 0x0F;
					pstRDSData->event_status &= (~RDS_EVENT_AFON_LIST);
				}
			}
		} else
			pstRDSData->AFON_Data.Addr_Cnt = 0x0F;
	} else {
		/* Type B */
		TA_ON = block_data[1] & 0x0008;
		WCN_DBG(FM_DBG | RDSC,
			"TA g14 typeB pstRDSData->RDSFlag.TP=%d pstRDSData->RDSFlag.TA=%d TP_ON=%d TA_ON=%d\n",
			pstRDSData->RDSFlag.TP, pstRDSData->RDSFlag.TA, TP_ON, TA_ON);

		if ((!pstRDSData->RDSFlag.TP) && (pstRDSData->RDSFlag.TA) && TP_ON && TA_ON) {
			fm_s32 TA_num = 0;

			for (num = 0; num < 25; num++) {
				if (pstRDSData->AFON_Data.AF[1][num] != 0)
					TA_num++;
				else
					break;
			}

			WCN_DBG(FM_NTC | RDSC, "TA set RDS_EVENT_TAON");

			if (TA_num == pstRDSData->AFON_Data.AF_Num)
				pstRDSData->event_status |= RDS_EVENT_TAON;
		}
	}

out:	return ret;
}

/*
 *  rds_parser
 *  Block0:	PI code(16bits)
 *  Block1:	Group type(4bits), B0=version code(1bit), TP=traffic program code(1bit),
 *  PTY=program type code(5bits), other(5bits)
 *  Block2:	16bits
 *  Block3:	16bits
 *  @rds_dst - target buffer that record RDS parsing result
 *  @rds_raw - rds raw data
 *  @rds_size - size of rds raw data
 *  @getfreq - function pointer, AF need get current freq
 */
fm_s32 rds_parser(rds_t *rds_dst, struct rds_rx_t *rds_raw, fm_s32 rds_size, fm_u16(*getfreq) (void))
{
	fm_s32 ret = 0;
	/* block_data[0] = blockA,   block_data[1] = blockB, block_data[2] = blockC,   block_data[3] = blockD, */
	/* block_data[4] = CRC,      block_data[5] = CBC */
	fm_u16 block_data[6];
	fm_u8 GroupType, SubType = 0;

	static fm_u8 ODAgroup;
	static fm_s16 AID;
	fm_s32 rds_cnt = 0;
	fm_s32 i = 0;
	fm_bool dirty = fm_false;
	/* target buf to fill the result in */
	fm_u16 *event = &rds_dst->event_status;
	fm_u32 *flag = &rds_dst->RDSFlag.flag_status;

	if (getfreq == NULL) {
		pr_err("%s,invalid pointer\n", __func__);
		return -FM_EPARA;
	}
	rds_get_freq = getfreq;

	ret = rds_cnt_get(rds_raw, rds_size, &rds_cnt);

	if (ret) {
		WCN_DBG(FM_WAR | RDSC, "get cnt err[ret=%d]\n", ret);
		return ret;
	}

	while (rds_cnt > 0) {
		ret = rds_grp_get(&block_data[0], rds_raw, i);

		if (ret) {
			WCN_DBG(FM_WAR | RDSC, "get group err[ret=%d]\n", ret);
			goto do_next;
		}

		ret = rds_grp_type_get(block_data[4], block_data[1], &GroupType, &SubType);

		if (ret) {
			WCN_DBG(FM_WAR | RDSC, "get group type err[ret=%d]\n", ret);
			goto do_next;
		}

		ret = rds_grp_counter_add(GroupType, SubType, &rds_dst->gc);

		ret = rds_grp_pi_get(block_data[4], block_data[0], &rds_dst->PI, &dirty);

		if (ret) {
			WCN_DBG(FM_WAR | RDSC, "get group pi err[ret=%d]\n", ret);
			goto do_next;
		} else if (dirty == fm_true) {
			ret = rds_event_set(event, RDS_EVENT_PI_CODE);	/* yes, we got new PI code */
		}

		ret = rds_grp_pty_get(block_data[4], block_data[1], &rds_dst->PTY, &dirty);

		if (ret) {
			WCN_DBG(FM_WAR | RDSC, "get group pty err[ret=%d]\n", ret);
			goto do_next;
		} else if (dirty == fm_true) {
			ret = rds_event_set(event, RDS_EVENT_PTY_CODE);	/* yes, we got new PTY code */
		}

		ret = rds_grp_tp_get(block_data[4], block_data[1], &rds_dst->RDSFlag.TP, &dirty);

		if (ret) {
			WCN_DBG(FM_WAR | RDSC, "get group tp err[ret=%d]\n", ret);
			goto do_next;
		} else if (dirty == fm_true) {
			ret = rds_event_set(event, RDS_EVENT_FLAGS);	/* yes, we got new TP code */
			ret = rds_flag_set(flag, RDS_FLAG_IS_TP);
		}

		switch (GroupType) {
		case 0:
			ret = rds_retrieve_g0(&block_data[0], SubType, rds_dst);
			if (ret)
				goto do_next;

			break;
		case 1:
			ret = rds_retrieve_g1(&block_data[0], SubType, rds_dst);
			if (ret)
				goto do_next;

			break;
		case 2:
			ret = rds_retrieve_g2(&block_data[0], SubType, rds_dst);
			if (ret)
				goto do_next;

			break;
		case 3:
			ret = rds_retrieve_g3(&block_data[0], SubType, &AID, &ODAgroup, rds_dst);
			if (ret)
				goto do_next;

			break;
		case 4:
			ret = rds_retrieve_g4(&block_data[0], SubType, rds_dst);
			if (ret)
				goto do_next;

			break;
		case 14:
			ret = rds_retrieve_g14(&block_data[0], SubType, rds_dst);
			if (ret)
				goto do_next;

			break;
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 13:
			if (AID == 0x0) {
				WCN_DBG(FM_NTC | RDSC, "this group(%d) shold be used normally\n", GroupType);
				/* DO normal parser */
			} else if (GroupType != ODAgroup) {
				WCN_DBG(FM_WAR | RDSC, "current group(%d) not match ODA group(%d) in 3A\n",
					GroupType, ODAgroup);
				/* ODA group check fail */
			} else {
				WCN_DBG(FM_NTC | RDSC, "do ODA parser, current group(%d)\n", GroupType);
				ret = rds_retrieve_goda(&block_data[0], SubType, AID, rds_dst);
				if (ret)
					goto do_next;
			}
			break;
		case 11:
		case 12:
		case 15:
			if (GroupType != ODAgroup) {
				WCN_DBG(FM_WAR | RDSC, "current group(%d) not match ODA group(%d) in 3A\n",
					GroupType, ODAgroup);
				/* ODA group check fail */
			} else {
				WCN_DBG(FM_NTC | RDSC, "do ODA parser, current group(%d)\n", GroupType);
				ret = rds_retrieve_goda(&block_data[0], SubType, AID, rds_dst);
				if (ret)
					goto do_next;
			}
			break;
		default:
			break;
		}

do_next:

		if (ret && (ret != -FM_ECRC)) {
			WCN_DBG(FM_ERR | RDSC, "parsing err[ret=%d]\n", ret);
			return ret;
		}

		rds_cnt--;
		i++;
	}

	return ret;
}
