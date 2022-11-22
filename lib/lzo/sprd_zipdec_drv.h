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
 **                         Dependencies                                      							   *
 **---------------------------------------------------------------------------*/
#ifndef __SPRD_ZIP_DEC_H__
#define __SPRD_ZIP_DEC_H__

#include <linux/bitops.h>

#define ZIPDEC_CTRL				(0x0000)
#define ZIPDEC_INT_EN				(0x0004)
#define ZIPDEC_INT_CLR				(0x0008)
#define ZIPDEC_INT_STS				(0x000C)

#define ZIPDEC_INT_RAW				(0x0010)
#define ZIPDEC_STS0				(0x0014)
#define ZIPDEC_STS1				(0x0018)
#define ZIPDEC_STS2				(0x001C)

#define ZIPDEC_STS3				(0x0020)
#define ZIPDEC_STS4				(0x0024)

#define ZIPDEC_IN_ADDR				(0x0040)
#define ZIPDEC_IN_ADDR1				(0x0044)
#define ZIPDEC_IN_ADDR2				(0x0048)
#define ZIPDEC_IN_ADDR3				(0x004C)

#define ZIPDEC_IN_ADDR4				(0x0050)
#define ZIPDEC_IN_ADDR5				(0x0054)
#define ZIPDEC_IN_ADDR6				(0x0058)
#define ZIPDEC_IN_ADDR7				(0x005C)

#define ZIPDEC_IN_LEN				(0x0060)
#define ZIPDEC_IN_LEN1				(0x0064)
#define ZIPDEC_IN_LEN2				(0x0068)
#define ZIPDEC_IN_LEN3				(0x006C)

#define ZIPDEC_IN_LEN4				(0x0070)
#define ZIPDEC_IN_LEN5				(0x0074)
#define ZIPDEC_IN_LEN6				(0x0078)
#define ZIPDEC_IN_LEN7				(0x007C)

#define ZIPDEC_OUT_ADDR				(0x0080)
#define ZIPDEC_OUT_ADDR1			(0x0084)
#define ZIPDEC_OUT_ADDR2			(0x0088)
#define ZIPDEC_OUT_ADDR3			(0x008C)

#define ZIPDEC_OUT_ADDR4			(0x0090)
#define ZIPDEC_OUT_ADDR5			(0x0094)
#define ZIPDEC_OUT_ADDR6			(0x0098)
#define ZIPDEC_OUT_ADDR7			(0x009C)

#define ZIPDEC_OUT_LEN				(0x00A0)
#define ZIPDEC_OUT_LEN1				(0x00A4)
#define ZIPDEC_OUT_LEN2				(0x00A8)
#define ZIPDEC_OUT_LEN3				(0x00AC)

#define ZIPDEC_OUT_LEN4				(0x00B0)
#define ZIPDEC_OUT_LEN5				(0x00B4)
#define ZIPDEC_OUT_LEN6				(0x00B8)
#define ZIPDEC_OUT_LEN7				(0x00BC)
#define ZIPDEC_TIME_OUT 			(0x00C0)

#define	ZIPDEC_EN				BIT(0)
#define	ZIPDEC_QUEUE_EN				BIT(3)
#define	ZIPDEC_RUN				BIT(4)

#define	ZIPDEC_IN_SWT_SHIFT			(8)
#define	ZIPDEC_IN_SWT_MASK			(0x03<<ZIPDEC_IN_SWT_SHIFT)

#define	ZIPDEC_OUT_SWT_SHIFT			(10)
#define	ZIPDEC_OUT_SWT_MASK			(0x03<<ZIPDEC_OUT_SWT_SHIFT)

#define	ZIPDEC_QUEUE_LEN_SHIFT			(16)
#define	ZIPDEC_QUEUE_LEN_MASK			(0x07<<ZIPDEC_QUEUE_LEN_SHIFT)

#define ZIPDEC_DONE_EN                     	BIT(0)
#define ZIPDEC_QUEUE_DONE_EN              	BIT(1)
#define ZIPDEC_ERR_EN                      	BIT(2)
#define ZIPDEC_LEN_ERR_EN                 	BIT(5)
#define ZIPDEC_SAVE_LEN_ERR_EN           	BIT(4)
#define ZIPDEC_FETCH_LEN_ERR_EN          	BIT(3)
#define ZIPDEC_TIMEOUT_EN                	BIT(6)

#define ZIPDEC_DONE_CLR                     	BIT(0)
#define ZIPDEC_QUEUE_DONE_CLR              	BIT(1)
#define ZIPDEC_ERR_CLR                      	BIT(2)
#define ZIPDEC_LEN_ERR_CLR                 	BIT(5)
#define ZIPDEC_SAVE_LEN_ERR_CLR           	BIT(4)
#define ZIPDEC_FETCH_LEN_ERR_CLR          	BIT(3)
#define ZIPDEC_TIMEOUT_CLR                  	BIT(6)

#define ZIPDEC_SWITCH_MODE0			0
#define ZIPDEC_SWITCH_MODE1			1
#define ZIPDEC_SWITCH_MODE2			2
#define ZIPDEC_SWITCH_MODE3			3

#define ZIPDEC_WAIT_POOL			0
#define ZIPDEC_WAIT_INT				1

#define ZIPDEC_NORMAL				0
#define ZIPDEC_QUEUE				1

#define ZIP_WORK_LENGTH                 (0x1000)

typedef enum ZIPDEC_INT_TYPE
{
	ZIPDEC_DONE_INT = 0,
	ZIPDEC_QUEUE_DONE_INT,
	ZIPDEC_ERR_INT,
	ZIPDEC_LEN_ERR_INT,
	ZIPDEC_SAVE_LEN_ERR_INT,
	ZIPDEC_FETCH_LEN_ERR_INT,
	ZIPDEC_TIMEOUT_INT,
	ZIPDEC_ALL_INT
}ZIPDEC_INT_TYPE;

void sprd_zipmatrix_reset(void);
void sprd_zipdec_enable(u32 enable);
void sprd_zipdec_reset(void);
void ZipDec_Run(void);
void ZipDec_Queue_Enable(u32 enable);
void ZipDec_SetSrcCfg(u32 num,unsigned long src_start_addr, u32 src_len);
void ZipDec_SetDestCfg(u32 num,unsigned long dest_start_addr,u32 dst_len);
void ZipDec_In_Out_SwtMode(u32 inmode,u32 outmode);
void ZipDec_QueueEn(u32 enable);
void ZipDec_QueueLength(u32 length);
void ZipDec_Set_Timeout(u32 time);
int Zip_Dec_Wait_Pool(ZIPDEC_INT_TYPE num);
void ZipDec_Restart(void);
#endif
