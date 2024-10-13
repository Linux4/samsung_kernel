/******************************************************************************
 **  File Name:
 **  Author:
 **  Date:
 **  Copyright:
 **  Description:
 **
 **
 *****************************************************************************
 *****************************************************************************
 **  Edit History
 **--------------------------------------------------------------------------*
 **  DATE			NAME			DESCRIPTION
 **  22/08/2001		xxx				Create.
 *****************************************************************************/


/**---------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **---------------------------------------------------------------------------*/
#ifndef __SPRD_ZIP_ENC_H__
#define __SPRD_ZIP_ENC_H__

#include <linux/bitops.h>

#define ZIPENC_CTRL            			(0x0000)
#define ZIPENC_IN_ADDR         			(0x0004)
#define ZIPENC_IN_LEN        			(0x0008)
#define ZIPENC_OUT_ADDR       			(0x000c)

#define ZIPENC_OUT_LEN        			(0x0010)
#define ZIPENC_CUR_IN_ADDR       		(0x0014)
#define ZIPENC_CUR_IN_LEN         		(0x0018)
#define ZIPENC_CUR_OUT_ADDR   			(0x001c)

#define ZIPENC_CUR_OUT_LEN	 		(0x0020)
#define ZIPENC_IN_QUEUE      			(0x0024)
#define ZIPENC_OUT_QUEUE   			(0x0028)
#define ZIPENC_COMP_LEN      			(0x002c)

#define ZIPENC_COMP_STS   			(0x0030)
#define ZIPENC_STS0  				(0x0034)
#define ZIPENC_STS1  				(0x0038)
#define ZIPENC_STS2  				(0x003C)

#define ZIPENC_INT_EN 				(0x0040)
#define ZIPENC_INT_CLR   			(0x0044)
#define ZIPENC_INT_STS  			(0x0048)
#define ZIPENC_INT_RAW  			(0x004c)

#define ZIPENC_IN_ADDRX 			(0x0050)
#define ZIPENC_IN_LENX   			(0x0054)
#define ZIPENC_OUT_ADDRX  			(0x0058)
#define ZIPENC_OUT_LENX  			(0x005c)
#define ZIPENC_TIME_OUT  			(0x0060)


#define	ZIPENC_EN				BIT(0)
#define	ZIPENC_MODE				BIT(1)
#define	ZIPENC_RUN				BIT(4)

#define	ZIPENC_IN_SWT_SHIFT			(8)
#define	ZIPENC_IN_SWT_MASK			(0x03<<ZIPENC_IN_SWT_SHIFT)

#define	ZIPENC_OUT_SWT_SHIFT			(10)
#define	ZIPENC_OUT_SWT_MASK			(0x03<<ZIPENC_OUT_SWT_SHIFT)

#define	ZIPENC_COMP_LEN_SHIFT			(0)
#define	ZIPENC_COMP_LEN_MASK			(0x1ffff<<ZIPENC_COMP_LEN_SHIFT)

#define	ZIPENC_ERR_STS_SHIFT			(0)
#define	ZIPENC_ERR_STS_MASK			(0xff<<ZIPENC_ERR_STS_SHIFT)

#define	ZIPENC_OL_STS_SHIFT			(8)
#define	ZIPENC_OL_STS_MASK			(0xff<<ZIPENC_OL_STS_SHIFT)


#define ZIPENC_TIME_OUT_NUM_SHIFT 		(0)
#define ZIPENC_TIME_OUT_NUM_MASK 		(0x00ffffff<<ZIPENC_TIME_OUT_NUM_SHIFT)

#define ZIPENC_TIME_OUT_FLAG_SHIFT 		(24)
#define ZIPENC_TIME_OUT_FLAG_MASK 		(0x00ffffff<<ZIPENC_TIME_OUT_FLAG_SHIFT)


#define	ZIPENC_IN_QUEUE_CLR			BIT(15)
#define	ZIPENC_OUT_QUEUE_CLR			BIT(15)

#define ZIPENC_TIMEOUT_EN       		BIT(2)
#define	ZIPENC_SEG_DONE_EN      		BIT(1)
#define	ZIPENC_DONE_EN           		BIT(0)

#define ZIPENC_TIMEOUT_CLR       		BIT(2)
#define	ZIPENC_SEG_DONE_CLR      		BIT(1)
#define	ZIPENC_DONE_CLR          		BIT(0)

#define ZIPENC_NORMAL_LEN                               0
#define ZIPENC_LIMIT_LEN                                1

#define ZIPENC_SWITCH_MODE0                             0
#define ZIPENC_SWITCH_MODE1                             1
#define ZIPENC_SWITCH_MODE2                             2
#define ZIPENC_SWITCH_MODE3                             3

#define ZIPENC_WAIT_POOL                                0
#define ZIPENC_WAIT_INT                                 1

#define ZIPENC_NORMAL                                   0
#define ZIPENC_QUEUE                                    1

#define ZIP_WORK_LENGTH                 (0x1000)

typedef enum ZIPENC_INT_TYPE
{
	ZIPENC_DONE_INT = 0,
	ZIPENC_SEG_DONE_INT,
	ZIPENC_TIMEOUT_INT,
	ZIPENC_ALL_INT
}ZIPENC_INT_TYPE;

//void sprd_zipenc_enable(u32 enable);
void sped_zipenc_reset(void);
void ZipEnc_Restart(void);
int Zip_Enc_Wait_Pool(ZIPENC_INT_TYPE num);
void ZipEnc_Run(void);
void ZipEnc_LengthLimitMode(u32 enable);
void ZipEnc_SetSrcCfg(unsigned long src_start_addr, u32 src_len);
void ZipEnc_SetDestCfg(unsigned long dest_start_addr,u32 dst_len);
void ZipEnc_In_Out_SwtMode(u32 inmode,u32 outmode);
void ZipEnc_InQueue_Clr(void);
void ZipEnc_OutQueue_Clr(void);
void ZipEnc_Set_Timeout(u32 time);
u32 ZipEnc_Get_Comp_Len(void);
u32 ZipEnc_Get_Ol_Sts(void);
u32 ZipEnc_Get_Err_Sts(void);
u32 ZipEnc_Get_Timeout_Flag(void);
#endif
