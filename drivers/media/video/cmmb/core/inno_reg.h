
/*
 * 
 * Copyright (C) 2010 Innofidei Corporation
 * Author:      sean <zhaoguangyu@innofidei.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */


#ifndef __INNO_REG_H__
#define __INNO_REG_H__

#define READ_AHBM2             0x71     
#define WRITE_AHBM2             0x73   
#define READ_AHBM1              0x70     
#define WRITE_AHBM1             0x72     

/* CMD */
#define FETCH_PER_COMM0             0x50002140     /* CMD code */
#define FETCH_PER_COMM1             0x50002141     /* AP communication register 1 */
#define FETCH_PER_COMM2             0x50002142     /* AP communication register 2 */
#define FETCH_PER_COMM3             0x50002143     /* AP communication register 3 */
#define FETCH_PER_COMM4             0x50002144     /* AP communication register 4 */
#define FETCH_PER_COMM5             0x50002145     /* AP communication register 5 */
#define FETCH_PER_COMM6             0x50002146     /* AP communication register 6 */
#define FETCH_PER_COMM7             0x50002147     /* AP communication register 7 */

 

/*ACK*/
#define FETCH_PER_COMM8             0x50002148     /* AP communication register 8 */
#define FETCH_PER_COMM9             0x50002149     /* AP communication register 9 */
#define FETCH_PER_COMM10            0x5000214a     /* AP communication register 10 */
#define FETCH_PER_COMM11            0x5000214b     /* AP communication register 11 */
#define FETCH_PER_COMM12            0x5000214c     /* AP communication register 12 */
#define FETCH_PER_COMM13            0x5000214d     /* AP communication register 13 */
#define FETCH_PER_COMM14            0x5000214e     /* AP communication register 14 */
#define FETCH_PER_COMM15            0x5000214f     /* AP communication register 15 */

/*Status:*/

#define FETCH_PER_COMM31            0x5000215f    /* bit7:1 busy 0 over bit6-0:0 sucess 1 error */

/*Signal power*/
#define FETCH_PER_COMM16            0x50002150     /* AP communication register 16 */

/*current freq*/
#define FETCH_PER_COMM17            0x50002151    /* AP communication register 17 */

/*CN(SNR)*/
#define FETCH_PER_COMM18            0x50002152    /* AP communication register 18 */
#define FETCH_PER_COMM19            0x50002153    /* AP communication register 19 */
/*LDPC error rate*/
#define FETCH_PER_COMM20            0x50002154
/*RS error rate*/
#define FETCH_PER_COMM21              0x50002155
/*BER*/
#define FETCH_PER_COMM22            0x50002156     /* AP communication register 22 */
#define FETCH_PER_COMM23            0x50002157     /* AP communication register 23 */

// CW freq offset
#define FETCH_PER_COMM24	    0x50002158

#define FETCH_PER_COMM30	    0x5000215e

//error status
//#define FETCH_PER_COMM29            0x5000215d    //AP communication register 29

#define LG_CW_STATE_BASE		0x0000BBF0
#define LG0_CW_STATE_REG		LG_CW_STATE_BASE
#define LG1_CW_STATE_REG		LG_CW_STATE_BASE+1
#define LG2_CW_STATE_REG		LG_CW_STATE_BASE+2
#define LG3_CW_STATE_REG		LG_CW_STATE_BASE+3
#define LG4_CW_STATE_REG		LG_CW_STATE_BASE+4
#define LG5_CW_STATE_REG		LG_CW_STATE_BASE+5
#define LG6_CW_STATE_REG		LG_CW_STATE_BASE+6
#define LG7_CW_STATE_REG		LG_CW_STATE_BASE+7
#define LG8_CW_STATE_REG		LG_CW_STATE_BASE+8
#define LG9_CW_STATE_REG		LG_CW_STATE_BASE+9
#define LG10_CW_STATE_REG		LG_CW_STATE_BASE+10
#define LG11_CW_STATE_REG		LG_CW_STATE_BASE+11
	#define LG8_CW_DEF_VALUE	(0x5A)
	#define LG9_CW_DEF_VALUE	(0xA5)
	#define LG10_CW_DEF_VALUE	(0xAA)
	#define LG11_CW_DEF_VALUE	(0x55)
#define RF108_STATE_REG0		LG_CW_STATE_BASE+12
#define RF108_STATE_REG1		LG_CW_STATE_BASE+13
#define RF108_STATE_REG2		LG_CW_STATE_BASE+14
#define RF108_STATE_REG3		LG_CW_STATE_BASE+15

#ifdef NJG1142 
	#define RF108_STATE_REG0_VALUE		(1)
	#define RF108_STATE_REG1_VALUE		(-19)
	#define RF108_STATE_REG2_VALUE		(12)
	#define RF108_STATE_REG3_VALUE		(-2)
#endif
#ifdef RF2884
	#define RF108_STATE_REG0_VALUE		(0)
	#define RF108_STATE_REG1_VALUE		(-33)
	#define RF108_STATE_REG2_VALUE		(15)
	#define RF108_STATE_REG3_VALUE		(-4)
#endif
#ifdef BGA728L7 
	#define RF108_STATE_REG0_VALUE		(0)
	#define RF108_STATE_REG1_VALUE		(-34)
	#define RF108_STATE_REG2_VALUE		(15)
	#define RF108_STATE_REG3_VALUE		(-6)
#endif
#ifdef MBC13720
	#define RF108_STATE_REG0_VALUE		(1)
	#define RF108_STATE_REG1_VALUE		(-39)
	#define RF108_STATE_REG2_VALUE		(20)
	#define RF108_STATE_REG3_VALUE		(-6)
#endif
#ifdef MBC13850
	#define RF108_STATE_REG0_VALUE		(1)
	#define RF108_STATE_REG1_VALUE		(-39)
	#define RF108_STATE_REG2_VALUE		(20)
	#define RF108_STATE_REG3_VALUE		(-6)
#endif
#ifdef AN26072A
	#define RF108_STATE_REG0_VALUE		(1)
	#define RF108_STATE_REG1_VALUE		(-29)
	#define RF108_STATE_REG2_VALUE		(15)
	#define RF108_STATE_REG3_VALUE		(-2)
#endif
#ifdef MGA68563
	#define RF108_STATE_REG0_VALUE		(1)
	#define RF108_STATE_REG1_VALUE		(-13)
	#define RF108_STATE_REG2_VALUE		(10)
	#define RF108_STATE_REG3_VALUE		(10)
#endif


/*Singal quality */
#define REG_SIGNAL_QUALITY          0x5000215E

/*M0_REG address*/
#define M0_REG_CLK_CTR           0x4000401c
#define M0_REG_CPU_CTR          0x40004010
#define M0_REG_PLL1_CTR                 0x4000402c     /* M0_REG_PLL1_CTR */
#define M0_REG_PLL_STATUS     0x40004048
#define FW_BASE_ADDR            0x20000000

#define OFDM_SYNC_STATE        0x50000028

#define FETCH_LDPC_TOTAL       0x50002044  
#define FETCH_LDPC_ERR          0x5000204C
#define FETCH_RS_TOTAL          0x50002050
#define FETCH_RS_ERR                    0x50002054
#define FETCH_LG8_11_MODE       0x50002068

#define FETCH_INT_STATUS0       0x50002104   

#define FETCH_INTEN0_ADDR       0x50002100 

//About Chip ID, add by liujinpeng
#define VERSION_ID_YEAR             0x50000000     //version_id[31:24]
#define VERSION_ID_MONTH             0x50000000     //version_id[31:24]
#define VERSION_ID_MAIN             0x50000008     //version_id[15:8] 
#define VERSION_ID_SUB              0x5000000c     //version_id[7:0]

#endif
