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


#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>

#define 	ZIPENC_BASE				SPRD_ZIPENC_BASE
#define 	ZIPENC_CTRL            			(ZIPENC_BASE+0x0000)
#define 	ZIPENC_IN_ADDR         			(ZIPENC_BASE+0x0004)
#define 	ZIPENC_IN_LEN        			(ZIPENC_BASE+0x0008)
#define 	ZIPENC_OUT_ADDR       			(ZIPENC_BASE+0x000c)

#define 	ZIPENC_OUT_LEN        			(ZIPENC_BASE+0x0010)
#define 	ZIPENC_CUR_IN_ADDR       		(ZIPENC_BASE+0x0014)
#define 	ZIPENC_CUR_IN_LEN         		(ZIPENC_BASE+0x0018)
#define 	ZIPENC_CUR_OUT_ADDR   			(ZIPENC_BASE+0x001c)

#define 	ZIPENC_CUR_OUT_LEN	 		(ZIPENC_BASE+0x0020)
#define 	ZIPENC_IN_QUEUE      			(ZIPENC_BASE+0x0024)
#define 	ZIPENC_OUT_QUEUE   			(ZIPENC_BASE+0x0028)
#define 	ZIPENC_COMP_LEN      			(ZIPENC_BASE+0x002c)

#define 	ZIPENC_COMP_STS   			(ZIPENC_BASE+0x0030)
#define 	ZIPENC_STS0  				(ZIPENC_BASE+0x0034)
#define 	ZIPENC_STS1  				(ZIPENC_BASE+0x0038)
#define 	ZIPENC_STS2  				(ZIPENC_BASE+0x003C)

#define 	ZIPENC_INT_EN 				(ZIPENC_BASE+0x0040)
#define 	ZIPENC_INT_CLR   			(ZIPENC_BASE+0x0044)
#define 	ZIPENC_INT_STS  			(ZIPENC_BASE+0x0048)
#define 	ZIPENC_INT_RAW  			(ZIPENC_BASE+0x004c)

#define 	ZIPENC_IN_ADDRX 			(ZIPENC_BASE+0x0050)
#define 	ZIPENC_IN_LENX   			(ZIPENC_BASE+0x0054)
#define 	ZIPENC_OUT_ADDRX  			(ZIPENC_BASE+0x0058)
#define 	ZIPENC_OUT_LENX  			(ZIPENC_BASE+0x005c)
#define 	ZIPENC_TIME_OUT  			(ZIPENC_BASE+0x0060)


#define	ZIPENC_EN					BIT_0
#define	ZIPENC_MODE					BIT_1
#define	ZIPENC_RUN					BIT_4

#define	ZIPENC_IN_SWT_SHIFT				(8)
#define	ZIPENC_IN_SWT_MASK				(0x03<<ZIPENC_IN_SWT_SHIFT)

#define	ZIPENC_OUT_SWT_SHIFT				(10)
#define	ZIPENC_OUT_SWT_MASK				(0x03<<ZIPENC_OUT_SWT_SHIFT)

#define	ZIPENC_COMP_LEN_SHIFT				(0)
#define	ZIPENC_COMP_LEN_MASK				(0x1ffff<<ZIPENC_COMP_LEN_SHIFT)

#define	ZIPENC_ERR_STS_SHIFT				(0)
#define	ZIPENC_ERR_STS_MASK				(0xff<<ZIPENC_ERR_STS_SHIFT)

#define	ZIPENC_OL_STS_SHIFT				(8)
#define	ZIPENC_OL_STS_MASK				(0xff<<ZIPENC_OL_STS_SHIFT)


#define 	ZIPENC_TIME_OUT_NUM_SHIFT 		(0)
#define 	ZIPENC_TIME_OUT_NUM_MASK 		(0x00ffffff<<ZIPENC_TIME_OUT_NUM_SHIFT)

#define 	ZIPENC_TIME_OUT_FLAG_SHIFT 		(24)
#define 	ZIPENC_TIME_OUT_FLAG_MASK 		(0x00ffffff<<ZIPENC_TIME_OUT_FLAG_SHIFT)


#define	ZIPENC_IN_QUEUE_CLR					BIT_15
#define	ZIPENC_OUT_QUEUE_CLR					BIT_15

 typedef enum ZIPENC_INT_TYPE
{
	ZIPENC_DONE_INT = 0,
	ZIPENC_SEG_DONE_INT,
	ZIPENC_TIMEOUT_INT,
	ZIPENC_ALL_INT
}ZIPENC_INT_TYPE;

#define ZIPENC_TIMEOUT_EN       			BIT_2
#define	ZIPENC_SEG_DONE_EN      			BIT_1
#define	ZIPENC_DONE_EN           			BIT_0

#define ZIPENC_TIMEOUT_CLR       			BIT_2
#define	ZIPENC_SEG_DONE_CLR      			BIT_1
#define	ZIPENC_DONE_CLR          			BIT_0

typedef void (*ZIP_INT_CALLBACK)(uint32_t);

void ZipEnc_Enable(void);
void ZipEnc_Disable(void);
void ZipEnc_Run(void);
void ZipEnc_SegInt_EN(uint32_t enable);
void ZipEnc_AllInt_EN(uint32_t enable);
void ZipEnc_SegInt_Clr(void);
void ZipEnc_AllInt_Clr(void);
void ZipEnc_Reset(void);
void ZipEnc_LengthLimitMode(void);
void ZipEnc_LengthLimitModeDisable(void);
void ZipEnc_SetSrcCfg(uint32_t src_start_addr, uint32_t src_len);
void ZipEnc_SetDestCfg(uint32_t dest_start_addr,uint32_t dst_len);
void ZipEnc_IntEn(uint32_t int_type);
void ZipEnc_IntDisable(ZIPENC_INT_TYPE int_type);
void ZipEnc_IntClr(ZIPENC_INT_TYPE int_type);
void ZipEnc_In_Swt(uint32_t mode);
void ZipEnc_Out_Swt(uint32_t mode);
void ZipEnc_InQueue_Clr(void);
void ZipEnc_OutQueue_Clr(void);
void ZipEnc_Set_Timeout(uint32_t time);
uint32_t ZipEnc_Get_Comp_Len(void);
uint32_t ZipEnc_Get_Ol_Sts(void);
uint32_t ZipEnc_Get_Err_Sts(void);
uint32_t ZipEnc_Get_Timeout_Flag(void);
void ZipEnc_RegCallback (uint32_t chan_id, ZIP_INT_CALLBACK callback);
void ZipEnc_UNRegCallback (uint32_t chan_id);
irqreturn_t ZipEnc_IsrHandler (int irq ,void *data);


