/*
 *  FM Drivr for Connectivity chip of Spreadtrum.
 *
 *  FM RDS Parser module header.
 *
 *  Copyright (C) 2015 Spreadtrum Company
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _FMDRV_RDS_PARSER
#define _FMDRV_RDS_PARSER

#define RDS_BLCKA		0x00	/* Block1 */
#define RDS_BLCKB		0x10	/* Block2 */
#define RDS_BLCKC		0x20	/* Block3 */
#define RDS_BLCKD		0x30	/* Block4 */
#define RDS_BLCKC_C		0x40	/* BlockC hyphen */
#define RDS_BLCKE_B		0x50	/* BlockE in RBDS */
#define RDS_BLCKE		0x60	/* Block E  */

/* 3bytes = 8bit(CRC flag) + 16bits (1 block ) */
#define rds_data_unit_size  3
#define rds_data_group_size  (3*4)
#define grp_type_mask     0xF0
#define grp_ver_mask       0x0F
#define grp_ver_bit          (0x01<<3) /* 0:version A, 1: version B */
#define grp_ver_a           0x0A
#define grp_ver_b            0x0B
#define invalid_grp_type    0x00

#define RDS_AF_FILL         205    /* AF fill in code */
#define RDS_AF_INVAL_L      205    /* AF invalid code low marker */
#define RDS_AF_INVAL_M      223    /* AF invalid code middle marker */
#define RDS_AF_NONE         224    /* 0 AF follow */
#define RDS_AF_NUM_1        225    /* 1 AF follow */
#define RDS_AF_NUM_25       249    /* 25 AFs follow */
#define RDS_LF_MF           250    /* LF/MF follow */
#define RDS_AF_INVAL_H      251    /* AF invalid code high marker */
#define RDS_AF_INVAL_T      255    /* AF invalid code top marker */
#define RDS_MF_LOW          0x10   /* lowest MF frequency */

#define RDS_FM_BASE         875     /* FM base frequency */
#define RDS_MF_BASE         531     /* MF base frequency */
#define RDS_LF_BASE         153     /* LF base frequency */

#define RDS_MIN_DAY         1      /* minimum day */
#define RDS_MAX_DAY         31     /* maximum day */
#define RDS_MIN_HUR         0      /* minimum hour */
#define RDS_MAX_HUR         23     /* maximum hour */
#define RDS_MIN_MUT         0      /* minimum minute */
#define RDS_MAX_MUT         59     /* maximum minute */
/* left over rds data length max in control block */
#define BTA_RDS_LEFT_LEN         24
/* Max radio text length */
#define BTA_RDS_RT_LEN           64
/* 8 character RDS feature length, i.e. PS, PTYN */
#define BTA_RDS_LEN_8            8

/* AF encoding method */
enum {
	RDS_AF_M_U, /* unknown */
	RDS_AF_M_A, /* method - A */
	RDS_AF_M_B  /* method - B */
};


/* change 8 bits to 16bits */
#define bytes_to_short(dest, src)  (dest = (unsigned short)(((unsigned short)\
	(*(src)) << 8) + (unsigned short)(*((src) + 1))))
int rds_parser(unsigned char *, unsigned char);
extern struct fmdrv_ops *fmdev;

#define FMR_ASSERT(a) { \
	if ((a) == NULL) { \
		pr_info("%s,invalid pointer\n", __func__);\
		return -1002; \
		} \
	}

#endif
