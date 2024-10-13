/*
 * FM Radio driver for RDS with SPREADTRUM SC2331FM Radio chip
 *
 * Copyright (c) 2015 Spreadtrum
 * Author: Songhe Wei <songhe.wei@spreadtrum.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include "fmdrv_rds_parser.h"
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include "fmdrv.h"
#include "fmdrv_main.h"

static struct fm_rds_data *g_rds_data_p;
void rds_parser_init()
{
g_rds_data_p = get_rds_data();
}



/*
* rds_event_set
* To set rds event, and user space can use this flag to juge
* which event happened
* If success return 0, else return error code
*/
static signed int rds_event_set(unsigned short *events, signed int event_mask)
{
	FMR_ASSERT(events);
	*events |= event_mask;
	wake_up_interruptible(&fmdev->rds_han.rx_queue);
	fmdev->rds_han.new_data_flag = 1;
	return 0;
}
/*
* rds_flag_set
* To set rds event flag, and user space can use this flag to juge which event
* happened
* If success return 0, else return error code
*/
static signed int rds_flag_set(unsigned int *flags, signed int flag_mask)
{
	FMR_ASSERT(flags);
	*flags |= flag_mask;
	return 0;
}


/*
*Group types which contain this information:
*TA(Traffic Program) code 0A 0B 14B 15B
*
*/

void rds_get_eon_ta(unsigned char *buf)
{
	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	unsigned char data = *(buf + rds_data_unit_size + 2);
	unsigned char ta_tp;
	unsigned pi_on;
	if (*blk_4  == 0)
		return;
	/*bit3: TA ON    bit4: TP ON */
	ta_tp = (unsigned char)(((data & (1 << 4)) >> 4) | ((data & (1 << 3))
			<< 1));
	bytes_to_short(pi_on, blk_4 + 1);
	/* need add some code to adapter google upper layer  here */

}

/*
* EON = Enhanced Other Networks information
*Group types which contain this information: EON : 14A
* variant code is in blockB low 4 bits
*/

void rds_get_eon(unsigned char *buf)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	unsigned char variant = ((*(blk_2  + 2)) & 0x0F);
	unsigned short pi_on;
	if ((*blk_3 == 0) || (*blk_4 == 0))
		return;
	/* if the upper Layer true */
	bytes_to_short(pi_on, blk_4 + 1);
	/* fmdev->rds_data.pi_on = pi_on; */

}

/*
*PTYN = Programme TYpe Name
* From Group 10A, it's a 8 character description transmitted in two 10A group
* block 2 bit0 is PTYN segment address.
* block3 and block4 is PTYN text character
*/

void rds_get_ptyn(unsigned char *buf)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char *blk_head[2];
	unsigned char blk_num[2] = {RDS_BLCKC, RDS_BLCKD};
	unsigned char seg_addr = ((*(blk_2 + 2)) & 0x01);
	unsigned char ptyn[4], i, step;
	unsigned char *blkc = buf + 2 * rds_data_unit_size;
	unsigned char *blkd = buf + 2 * rds_data_unit_size;
	blk_head[0] = buf + 2 * rds_data_unit_size;
	blk_head[1] = buf + 3 * rds_data_unit_size;
	memcpy((void *)&ptyn[0], (void *)(blk_head[0] + 1), 2);
	memcpy((void *)&ptyn[2], (void *)(blk_head[1] + 1), 2);
	for (i = 0; i < 2; i++) {
		step = i >> 1;
		/* update seg_addr[0,1] if blockC/D is reliable data */
		if ((*blkc == 1) && (*blkd == 1)) {
			/* it's a new PTYN*/
			if (memcmp((void *)&ptyn[seg_addr * 4 + step], (void *)
				(ptyn + step), 2) != 0)
				memcpy((void *)&ptyn[seg_addr * 4 + step],
				(void *)(ptyn + step), 2);

			}
		}
}

/*
* EWS = Coding of Emergency Warning Systems
*    EWS inclued belows:
*	unsigned char data_5b;
*	unsigned short data_16b_1;
*	unsigned short data_16b_2;
*/

void rds_get_ews(unsigned char *buf)
{
	unsigned char data_5b;
	unsigned short data_16b_1;
	unsigned short data_16b_2;
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	data_5b = (unsigned char)((*(blk_2 + 2)) & 0x1F);
	bytes_to_short(data_16b_1, (blk_3 + 1));
	bytes_to_short(data_16b_2, (blk_4 + 1));
	/* judge if EWS event Flag was set, and send to read interface */
	return;

}


void rfd_get_rtplus(unsigned char *buf)
{
	/*tBTA_RDS_RTP        *p_rtp_cb = &p_cb->rtp_cb;*/
	unsigned char	*blk_b = buf + rds_data_unit_size;
	unsigned char	*blk_c = buf + 2 * rds_data_unit_size;
	unsigned char	*blk_d = buf + 3 * rds_data_unit_size;
	unsigned char	toggle_bit = (*(blk_b + 2) & 0x10) >> 4;
	unsigned char	content_type, s_marker, l_marker;
	bool running;

	running = ((*(blk_b + 2) & 0x08) != 0) ? 1 : 0;
	if ((*blk_c == 1) && (*blk_b == 1)) {
		content_type = ((*(blk_b + 2) & 0x07) << 3) + (*(blk_c + 1)
			>> 5);
		s_marker = (((*(blk_c + 1) & 0x1F) << 1) + (*(blk_c + 2)
			>> 7));
		l_marker = (((*(blk_c + 2)) & 0x7F) >> 1);
		}
	if ((*blk_c == 1) && (*blk_d == 1)) {
		content_type = ((*(blk_c + 2) & 0x01) << 5) +
			(*(blk_d + 1) >> 3);
		s_marker = (*(blk_d + 2) >> 5) + ((*(blk_d + 1) & 0x07) << 3);
		l_marker = (*(blk_d + 2) & 0x1f);

		}

}
/*
* ODA = Open Data Applications
*
*/

void rds_get_oda(unsigned char *buf)
{
	rfd_get_rtplus(buf);

}
/*
* TDC = Transparent Data Channel
*
*/

void rds_get_tdc(unsigned char *buf, unsigned char version)
{
/* 2nd  block  */
	unsigned char	*blk_b	= buf + rds_data_unit_size;
/* 3rd block  */
	unsigned char	*blk_c	= buf + 2*rds_data_unit_size;
/* 4rd block  */
	unsigned char	*blk_d	= buf + 3*rds_data_unit_size;
	unsigned char chnl_num, len, tdc_seg[4];
    /* block C can be BlockC_C */
    /* unrecoverable block 3,or ERROR in block 4, discard this group */
	if ((*blk_b == 0) || (*blk_c == 0) || (*blk_d == 0))
		return;
    /* if block data is reliable enough, decode it */
  /*memset(&tdc_data, 0 , sizeof(tBTA_RDS_TDC_DATA)); */

    /* read TDChannel number */
	chnl_num    = *(blk_b + 2) & 0x1f;
	if (version == grp_ver_a) {
		memcpy(tdc_seg, blk_c + 1, 2);
		len = 2;
		}

	memcpy(tdc_seg +  len, blk_d + 1, 2);
	len += 2;

}

/*
*CT = Programe Clock time
*
*
*/

void rds_get_ct(unsigned char *buf)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	unsigned char b3_1 = *(blk_3 + 1), b3_2 = *(blk_3 + 2);
	unsigned char b4_1 = *(blk_4 + 1), b4_2 = *(blk_4 + 2);
	unsigned int temp1, temp2;

	unsigned int day = 0;
	unsigned char hour, minute, sense, offset;

	if ((*(blk_3) == 0) || (*(blk_4) == 0))
		return;
	temp1 = (unsigned int) ((b3_1 << 8) | b3_2);
	temp2 = (unsigned int) (*(blk_2 + 2) & 0x03);
	day = (temp2 << 15) | (temp1 >> 1);

	temp1 = (unsigned int)(b3_2 & 0x01);
	temp2 = (unsigned int)(b4_1 & 0xF0);
	hour = (unsigned char)((temp1 << 4) | (temp2 >> 4));
	minute = ((b4_1 & 0x0F) << 2) | ((b4_2 & 0xC0) >> 6);
	sense = (b4_2 & 0x20) >> 5;
	offset = b4_2 & 0x1F;
	/* set RDS EVENT FLAG  in here */
	fmdev->rds_data.CT.day = day;
	fmdev->rds_data.CT.hour = hour;
	fmdev->rds_data.CT.minute = minute;
	fmdev->rds_data.CT.local_time_offset_half_hour = offset;
	fmdev->rds_data.CT.local_time_offset_signbit = sense;

}

/*
*
*
*/

void rds_get_oda_aid(unsigned char *buf)
{
	;

}

/*
*rt == Radio Text
* Group types which contain this information: 2A 2B
* 2A: address in block2 last 4bits, Text in block3 and block4
* 2B: address in block2 last 4bits, Text in block4(16bits)
*/

void rds_get_rt(unsigned char *buf, unsigned char grp_type)
{
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char *blk_4 = buf + 3 *  rds_data_unit_size;
	unsigned char addr = ((*(buf + rds_data_unit_size + 2)) & 0x0F);
	unsigned char text_flag = ((*(buf + rds_data_unit_size + 2)) & 0x10);
	unsigned char test_old_flag = 0;
	fm_pr("RT Text A/B Flag is %d", text_flag);
	if (text_flag != test_old_flag) {
		memset(fmdev->rds_data.rt_data.textdata[3] , ' ', 64);
		/* clean up previous Radio Text */
		fmdev->rds_data.rt_data.textdata[3][63] = '\0';
		test_old_flag = text_flag;
		}

	if (grp_type == 0x2A) {
		if (*(blk_3 + 1) == 0x0d)
			*(blk_3 + 1) = '\0';
		if (*(blk_3 + 2) == 0x0d)
			*(blk_3 + 2) = '\0';
		if (*(blk_4 + 1) == 0x0d)
			*(blk_4 + 1) = '\0';
		if (*(blk_4 + 2) == 0x0d)
			*(blk_4 + 2) = '\0';
		fmdev->rds_data.rt_data.textdata[3][addr * 4] = *(blk_3 + 1);
		fmdev->rds_data.rt_data.textdata[3][addr * 4 + 1] =
			*(blk_3 + 2);
		fmdev->rds_data.rt_data.textdata[3][addr * 4 + 2] =
			*(blk_4 + 1);
		fmdev->rds_data.rt_data.textdata[3][addr * 4 + 3] =
			*(blk_4 + 2);
		}
	else {/* group type = 2B */
		if (*(blk_3 + 1) == 0x0d)
			*(blk_3 + 1) = '\0';
		if (*(blk_3 + 2) == 0x0d)
			*(blk_3 + 2) = '\0';
		fmdev->rds_data.rt_data.textdata[3][addr * 2] = *(blk_3 + 1);
		fmdev->rds_data.rt_data.textdata[3][addr * 2 + 1] =
			*(blk_3 + 2);
		}
	rds_event_set(&(fmdev->rds_data.event_status),
		RDS_EVENT_LAST_RADIOTEXT);
	fm_pr("RT is %s", fmdev->rds_data.rt_data.textdata[3]);
}
/*
* PIN = Programme Item Number
*
*/

void rds_get_pin(unsigned char *buf)
{
	struct RDS_PIN {
		unsigned char day;
		unsigned char hour;
		unsigned char minute;
		} rds_pin;

	unsigned char *blk_4 = buf + 3 * rds_data_unit_size;
	unsigned char byte1 = *(blk_4 + 1), byte2 = *(blk_4 + 2);
	if (*blk_4 == 0)
		return;
	rds_pin.day = ((byte1 & 0xF8) >> 3);
	rds_pin.hour = (byte1 & 0x07) << 2 | ((byte2 & 0xC0) >> 6);
	rds_pin.minute = (byte2 & 0x3F);
}

/*
* SLC = Slow Labelling codes from group 1A, block3
* LA 0 0 0 OPC ECC
*/

void rds_get_slc(unsigned char *buf)
{
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	unsigned char variant_code, slc_type, ecc_code, paging;
	unsigned short data;
	if ((*blk_3) == 0)
		return;
	bytes_to_short(data, blk_3);
	data &= 0x0FFF;
/* take bit12 ~ bit14 of block3 as variant code */
	variant_code = ((*(blk_3 + 1) & 0x70) >> 4);
	if ((variant_code == 0x04) || (variant_code == 0x05))
		slc_type = 0x04;
	else
		slc_type = variant_code;
	if (slc_type == 0) {
		ecc_code = *(blk_3 + 2);
		paging = (*(blk_3 + 1) & 0x0f);
		}
	fmdev->rds_data.extend_country_code = ecc_code;

}

/*
* Group types which contain this information: 0A 0B
* PS = Programme Service name
* block2 last 2bit stard for address, block4 16bits meaning ps.
*/

void rds_get_ps(unsigned char *buf)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char *blk_4 = buf + 3 *  rds_data_unit_size;
	unsigned char index = (unsigned char)((*(blk_2 + 2) & 0x03) * 2);
	fm_pr("PS start receive");
	fm_pr("blk2 =%d, blk4=%d", *blk_2, *blk_4);
/*	if ((*blk_2) == 1) { */
		fmdev->rds_data.ps_data.addr_cnt = index;
		fmdev->rds_data.ps_data.PS[3][index] = *(blk_4 + 1);
		fmdev->rds_data.ps_data.PS[3][index + 1] = *(blk_4 + 2);

		rds_event_set(&(fmdev->rds_data.event_status),
			RDS_EVENT_PROGRAMNAME);
		fm_pr("The event is %x", fmdev->rds_data.event_status);
/*		} */
	fm_pr("The PS is %s", fmdev->rds_data.ps_data.PS[3]);
	fm_pr("blk4+1=%x", *(blk_4 + 1));
	fm_pr("blk4+2=%x", *(blk_4 + 2));

}
unsigned short rds_get_freq(void)
{
	return 0;
}
void rds_get_af_method(unsigned char AFH, unsigned char AFL)
{
	static signed short pre_af_num;
	unsigned char  indx, indx2, num;
	fm_pr("af code is %d and %d", AFH, AFL);
	if (AFH >= RDS_AF_NUM_1 && AFH <= RDS_AF_NUM_25) {
		if (AFH == RDS_AF_NUM_1) {
			fmdev->rds_data.af_data.ismethod_a = RDS_AF_M_A;
			fmdev->rds_data.af_data.AF_NUM = 1;
			}
/* have got af number */
		fmdev->rds_data.af_data.isafnum_get = 0;
		pre_af_num = AFH - 224;
		if (pre_af_num != fmdev->rds_data.af_data.AF_NUM)
			fmdev->rds_data.af_data.AF_NUM = pre_af_num;
		else
			fmdev->rds_data.af_data.isafnum_get = 1;
		if ((AFL < 205) && (AFL > 0)) {
			fmdev->rds_data.af_data.AF[0][0] = AFL + 875;
			/* convert to 100KHz */
#ifdef SPRD_FM_50KHZ_SUPPORT
			fmdev->rds_data.af_data.AF[0][0] *= 10;
#endif
			if ((fmdev->rds_data.af_data.AF[0][0]) !=
				(fmdev->rds_data.af_data.AF[1][0])) {
				fmdev->rds_data.af_data.AF[1][0] =
					fmdev->rds_data.af_data.AF[0][0];
				}
			else {
				if (fmdev->rds_data.af_data.AF[1][0] !=
					rds_get_freq())
					fmdev->rds_data.af_data.ismethod_a = 1;
				else
					fmdev->rds_data.af_data.ismethod_a = 0;
				}

/*only one AF handle */
			if ((fmdev->rds_data.af_data.isafnum_get) &&
				(fmdev->rds_data.af_data.AF_NUM == 1)) {
				fmdev->rds_data.af_data.addr_cnt = 0xFF;
/*pstRDSData->event_status |= RDS_EVENT_AF_LIST; */
				}
			}
		}
	else if ((fmdev->rds_data.af_data.isafnum_get) &&
		(fmdev->rds_data.af_data.addr_cnt != 0xFF)) {
		/*AF Num correct */
		num = fmdev->rds_data.af_data.AF_NUM;
		num = num >> 1;
/* WCN_DBG(FM_DBG | RDSC, "RetrieveGroup0 +num:%d\n", num); */
/* Put AF freq fm_s32o buffer and check if AF freq is repeat again */
		for (indx = 1; indx < (num + 1); indx++) {
			if ((AFH == (fmdev->rds_data.af_data.AF[0][2*num-1]))
				&& (AFL ==
				(fmdev->rds_data.af_data.AF[0][2*indx]))) {
				pr_info("AF same as");
				break;
				}
			else if (!(fmdev->rds_data.af_data.AF[0][2 * indx-1])) {
				/*convert to 100KHz */
				fmdev->rds_data.af_data.AF[0][2*indx-1] =
					AFH + 875;
				fmdev->rds_data.af_data.AF[0][2*indx] =
					AFL + 875;
#ifdef MTK_FM_50KHZ_SUPPORT
				fmdev->rds_data.af_data.AF[0][2*indx-1] *= 10;
				fmdev->rds_data.af_data.AF[0][2*indx] *= 10;
#endif
				break;
				}
			}
		num = fmdev->rds_data.af_data.AF_NUM;
/*WCN_DBG(FM_DBG | RDSC, "RetrieveGroup0 ++num:%d\n", num); */
		if (num <= 0)
			return;
		if ((fmdev->rds_data.af_data.AF[0][num-1]) == 0)
			return;
		num = num >> 1;
		for (indx = 1; indx < num; indx++) {
			for (indx2 = indx + 1; indx2 < (num + 1); indx2++) {
				AFH = fmdev->rds_data.af_data.AF[0][2*indx-1];
				AFL = fmdev->rds_data.af_data.AF[0][2*indx];
				if (AFH > (fmdev->rds_data.af_data.AF[0][2*indx2
					-1])) {
					fmdev->rds_data.af_data.AF[0][2*indx-1]
					= fmdev->rds_data.af_data.AF[0][2
					*indx2-1];
					fmdev->rds_data.af_data.AF[0][2*indx] =
					fmdev->rds_data.af_data.AF[0][2*indx2];
					fmdev->rds_data.af_data.AF[0][2*indx2-1]
						= AFH;
					fmdev->rds_data.af_data.AF[0][2*indx2]
						= AFL;
					}
				else if (AFH == (fmdev->rds_data.af_data.AF[0][2
					*indx2-1])) {
					if (AFL > (fmdev->rds_data.af_data.AF[0]
						[2*indx2])) {
						fmdev->rds_data.af_data.AF[0][2
							*indx-1]
						= fmdev->rds_data.af_data
						.AF[0][2*indx2-1];
						fmdev->rds_data.af_data.AF[0][2
							*indx] = fmdev->rds_data
							.af_data.AF[0][2*indx2];
						fmdev->rds_data.af_data.AF[0][2*
							indx2-1] = AFH;
						fmdev->rds_data.af_data.AF[0][2
							*indx2] = AFL;
						}
					}
				}
			}

/* arrange frequency from low to high:end */
/* compare AF buff0 and buff1 data:start */
		num = fmdev->rds_data.af_data.AF_NUM;
		indx2 = 0;
		for (indx = 0; indx < num; indx++) {
			if ((fmdev->rds_data.af_data.AF[1][indx]) ==
				(fmdev->rds_data.af_data.AF[0][indx])) {
				if (fmdev->rds_data.af_data.AF[1][indx] != 0)
					indx2++;
				}
			else
				fmdev->rds_data.af_data.AF[1][indx] =
				fmdev->rds_data.af_data.AF[0][indx];
			}

		/* compare AF buff0 and buff1 data:end */
		if (indx2 == num) {
			fmdev->rds_data.af_data.addr_cnt = 0xFF;
			/*pstRDSData->event_status |= RDS_EVENT_AF_LIST;*/
			for (indx = 0; indx < num; indx++) {
				if ((fmdev->rds_data.af_data.AF[1][indx])
					== 0) {
					fmdev->rds_data.af_data.addr_cnt = 0x0F;
					/*pstRDSData->event_status &= (~RDS_
					EVENT_AF_LIST);*/
					}
				}
			}
		else
			fmdev->rds_data.af_data.addr_cnt = 0x0F;
		}
}
/*
*Group types which contain this information: 0A
*AF = Alternative Frequencies
* af infomation in block 3
*/

void rds_get_af(unsigned char *buf)
{
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
	if (*blk_3 != 1)
		return;
	rds_get_af_method(*(blk_3 + 1), *(blk_3 + 2));
	fmdev->rds_data.af_data.AF[1][24] = 0;
	/*rds_event_set(&(fmdev->rds_data.event_status), RDS_EVENT_AF);*/

}
/*
*Group types which contain this information: 0A 0B 15B
*
*/

void rds_get_di_ms(unsigned char *buf)
{
	;

}

/*
*Group types which contain this information: TP_all(byte1 bit2);
*   TA: 0A 0B 14B 15B(byte2 bit4)
* TP = Traffic Program identification; TA = Traffic Announcement
*
*/

void rds_get_tp_ta(unsigned char *buf, unsigned char grp_type)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char byte1 = *(blk_2 + 1), byte2 = *(blk_2 + 2);
	unsigned char ta_tp;
	unsigned short *event = &(fmdev->rds_data.event_status);
	if ((*blk_2) == 0)
		return;
	ta_tp = (unsigned char)((byte1 & (1<<2))>>2);
	if (grp_type == 0x0a || grp_type == 0x0B || grp_type == 0xFB) {
		ta_tp |= (byte2 & (1 << 4));
		rds_event_set(event, RDS_EVENT_TAON_OFF);
		}

}

/*
*Group types which contain this information: all
*block2:Programme Type code = 5 bits($)
*         #### ##$$ $$$# ####
*
*/

void rds_get_pty(unsigned char *buf)
{
	unsigned char *blk_2 = buf + rds_data_unit_size;
	unsigned char byte1 = *(blk_2 + 1), byte2 = *(blk_2 + 2), pty;
	if ((*blk_2) == 1)
		pty = ((byte2 >> 5) | ((byte1 & 0x3) << 3));
	fmdev->rds_data.PTY = pty;

}

/*
* Group types which contain this information: all
* Read PI code from the group. grp_typeA: block 1 and block3,
* grp_type B: block3
*
*/

void rds_get_pi_code(unsigned char *buf, unsigned char version)
{
	unsigned char *blk_3 = buf + 2 * rds_data_unit_size;
/* pi_code for version A, pi_code_b for version B */
	unsigned short pi_code = 0, pi_code_b = 0;
	unsigned char crc_flag1 = *buf;
	unsigned char crc_flag3 = *(buf + 2 * rds_data_unit_size);

	if (version == invalid_grp_type)
		return;

	if (crc_flag1 == 1)
		bytes_to_short(pi_code, buf+1);
	else
		return;

	if (version == grp_ver_b) {
		if (crc_flag3 == 1)
			bytes_to_short(pi_code_b, blk_3 + 1);
		}

	if (pi_code == 0 && pi_code_b != 0)
		pi_code = pi_code_b;


/* send pi_code value to global and copy to user space in read rds interface */
	fmdev->rds_data.PI = pi_code;

}


/*
* Block 1: PIcode(16bit)+CRC
* Block 2 : Group type code(4bit)
*+ B0 version(1bit 0:version A; 1:version B)
*+ TP(1bit)+ PTY(5 bits)
* @ buffer point to the start of Block 1
* Block3: 16bits + 10bits
* Block4: 16bits + 10bits
* rds_get_group_type from Block2
*/
unsigned char rds_get_group_type(unsigned char *buffer)
{
	unsigned char *crc_blk_2 = buffer + rds_data_unit_size;
	unsigned char blk2_byte1 = *(crc_blk_2+1);
	unsigned char group_type;
	unsigned char crc_flag = *crc_blk_2;
	if (crc_flag == 1)
		group_type = (blk2_byte1 & grp_type_mask);
	else
		group_type = invalid_grp_type;
/* 0:version A, 1: version B */
	if (blk2_byte1 & grp_ver_bit)
		group_type |= grp_ver_b;
	else
		group_type |= grp_ver_a;

	return group_type;

}

void dump_rx_data(char *buffer, char len)
{
	char i;
	fm_pr("\n fm rx data(%d): 0x ", len);
	for (i = 0; i < len; i++)
		pr_err("%x", buffer[i]);
	pr_err("\n");

}

static int atoi(const char *str)
{
	int value = 0;
	while (*str >= '0' && *str <= '9') {
		value *= 10;
		value += *str - '0';
		str++;
		}
	return value;
}


/*                 p here
* string: 1,     1,16551,1,11,1,57725,1,26400
* hex: 1,       1,0x40A7,1,0xB,1,0xE17D,1,0x6720
*/
string_to_hex(unsigned char *buf_src, unsigned char *buf_des)
{
	unsigned int c1, c2, c3, c4;/*crc data*/
	unsigned int b1, b2, b3, b4;/* block data */
	sscanf(buf_src, "%d,%d,%d,%d,%d,%d,%d,%d",
		&c1, &b1, &c2, &b2, &c3, &b3, &c4, &b4);
	fm_pr("new decimal data is :%d,%d,%d,%d,%d,%d,%d,%d",
		c1, b1, c2, b2, c3, b3, c4, b4);
	buf_des[0] = c1;
	buf_des[1] = b1 >> 8;
	buf_des[2] = b1 & 0xFF;

	buf_des[3] = c2;
	buf_des[4] = b2 >> 8;
	buf_des[5] = b2 & 0xFF;

	buf_des[6] = c3;
	buf_des[7] = b3 >> 8;
	buf_des[8] = b3 & 0xFF;

	buf_des[9] = c4;
	buf_des[10] = b4 >> 8;
	buf_des[11] = b4 & 0xFF;

}
/*
* rds_parser
* Block0: PI code(16bits)
* Block1: Group type(4bits), B0=version code(1bit),
* TP=traffic program code(1bit),
* PTY=program type code(5bits), other(5bits)
* @getfreq - function pointer, AF need get current freq
* Theoretically From FIFO :
* One Group = Block1(16 bits) + CRC(10 bits)
*+Block2 +CRC(10 bits)
*+ Block3(16 bits) + CRC(10 bits)
*+ Block4(16 bits) + CRC(10 bits)
* From marlin2 chip, the data stream is like below:
* One Group = CRC_Flag(8bit)+Block1(16bits)
*                 + CRC_Flag(8bit)+Block2(16bits)
*                  + CRC_Flag(8bit)+Block3(16bits)
*                  + CRC_Flag(8bit)+Block4(16bits)
*/
int rds_parser(unsigned char *buffer1, unsigned char len)
{
	unsigned char grp_type;
	unsigned char *buffer;
	unsigned char a[12];
	buffer = a;
	dump_rx_data(buffer1, len);

	string_to_hex(buffer1, buffer);
	dump_rx_data(buffer, len);

	grp_type = rds_get_group_type(buffer);
	fm_pr("group type is : 0x%x", grp_type);

	rds_get_pi_code(buffer, grp_type & grp_ver_mask);
	rds_get_pty(buffer);
	rds_get_tp_ta(buffer, grp_type);

	switch (grp_type) {

	case invalid_grp_type:
		fm_pr("invalid group type\n");
		break;
/* Processing group 0A */
	case 0x0A:
		rds_get_di_ms(buffer);
		rds_get_af(buffer);
		rds_get_ps(buffer);
		break;
/* Processing group 0B */
	case 0x0B:
		rds_get_di_ms(buffer);
		rds_get_ps(buffer);
		break;
	case 0x1A:
		rds_get_slc(buffer);
		rds_get_pin(buffer);
		break;
	case 0x1B:
		rds_get_pin(buffer);
		break;
	case 0x2A:
	case 0x2B:
		rds_get_rt(buffer, grp_type);
		break;
	case 0x3A:
		rds_get_oda_aid(buffer);
		break;

	case 0x4A:
		rds_get_ct(buffer);
		break;
	case 0x5A:
	case 0x5B:
		if (1)
			rds_get_tdc(buffer, grp_type & grp_ver_mask);
		else
			rds_get_oda(buffer);
		break;
	case 0x9a:
		if (1)
			rds_get_ews(buffer);
		else
			rds_get_oda(buffer);
		break;

	case 0xAA:/* 10A group*/
		rds_get_ptyn(buffer);
		break;

	case 0xEA:
		rds_get_eon(buffer);
		break;

	case 0xEB:
		rds_get_eon_ta(buffer);
		break;

	case 0xFB:
		rds_get_di_ms(buffer);
		break;

/*ODA (Open Data Applications) group availability signaled in type 3A groups*/
	case 0x3B:
	case 0x4B:
	case 0x6A:
	case 0x6B:
	case 0x7A:
	case 0x7B:
	case 0x8A:
	case 0x8B:
	case 0x9B:
	case 0xAB:
	case 0xBA:
	case 0xBB:
	case 0xCA:
	case 0xCB:
	case 0xDB:
	case 0xDA:
	case 0xFA:
		if (1)
			rds_get_oda(buffer);
		break;

	default:
		fm_pr("rds group type[0x%x] not to be processed", grp_type);
		break;
	}


	/*
	tasklet_schedule(&fmdev->rx_task);
	*/



}

