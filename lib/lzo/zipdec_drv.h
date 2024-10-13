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

#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>

#define 	ZIPDEC_BASE				SPRD_ZIPDEC_BASE
#define 	ZIPDEC_CTRL				(ZIPDEC_BASE+0x0000)
#define 	ZIPDEC_INT_EN				(ZIPDEC_BASE+0x0004)
#define 	ZIPDEC_INT_CLR				(ZIPDEC_BASE+0x0008)
#define 	ZIPDEC_INT_STS				(ZIPDEC_BASE+0x000C)

#define 	ZIPDEC_INT_RAW				(ZIPDEC_BASE+0x0010)
#define 	ZIPDEC_STS0				(ZIPDEC_BASE+0x0014)
#define 	ZIPDEC_STS1				(ZIPDEC_BASE+0x0018)
#define 	ZIPDEC_STS2				(ZIPDEC_BASE+0x001C)

#define 	ZIPDEC_STS3				(ZIPDEC_BASE+0x0020)
#define 	ZIPDEC_STS4				(ZIPDEC_BASE+0x0024)

#define 	ZIPDEC_IN_ADDR				(ZIPDEC_BASE+0x0040)
#define 	ZIPDEC_IN_ADDR1				(ZIPDEC_BASE+0x0044)
#define 	ZIPDEC_IN_ADDR2				(ZIPDEC_BASE+0x0048)
#define 	ZIPDEC_IN_ADDR3				(ZIPDEC_BASE+0x004C)

#define 	ZIPDEC_IN_ADDR4				(ZIPDEC_BASE+0x0050)
#define 	ZIPDEC_IN_ADDR5				(ZIPDEC_BASE+0x0054)
#define 	ZIPDEC_IN_ADDR6				(ZIPDEC_BASE+0x0058)
#define 	ZIPDEC_IN_ADDR7				(ZIPDEC_BASE+0x005C)

#define 	ZIPDEC_IN_LEN				(ZIPDEC_BASE+0x0060)
#define 	ZIPDEC_IN_LEN1				(ZIPDEC_BASE+0x0064)
#define 	ZIPDEC_IN_LEN2				(ZIPDEC_BASE+0x0068)
#define 	ZIPDEC_IN_LEN3				(ZIPDEC_BASE+0x006C)

#define 	ZIPDEC_IN_LEN4				(ZIPDEC_BASE+0x0070)
#define 	ZIPDEC_IN_LEN5				(ZIPDEC_BASE+0x0074)
#define 	ZIPDEC_IN_LEN6				(ZIPDEC_BASE+0x0078)
#define 	ZIPDEC_IN_LEN7				(ZIPDEC_BASE+0x007C)

#define 	ZIPDEC_OUT_ADDR				(ZIPDEC_BASE+0x0080)
#define 	ZIPDEC_OUT_ADDR1			(ZIPDEC_BASE+0x0084)
#define 	ZIPDEC_OUT_ADDR2			(ZIPDEC_BASE+0x0088)
#define 	ZIPDEC_OUT_ADDR3			(ZIPDEC_BASE+0x008C)

#define 	ZIPDEC_OUT_ADDR4			(ZIPDEC_BASE+0x0090)
#define 	ZIPDEC_OUT_ADDR5			(ZIPDEC_BASE+0x0094)
#define 	ZIPDEC_OUT_ADDR6			(ZIPDEC_BASE+0x0098)
#define 	ZIPDEC_OUT_ADDR7			(ZIPDEC_BASE+0x009C)

#define 	ZIPDEC_OUT_LEN				(ZIPDEC_BASE+0x00A0)
#define 	ZIPDEC_OUT_LEN1				(ZIPDEC_BASE+0x00A4)
#define 	ZIPDEC_OUT_LEN2				(ZIPDEC_BASE+0x00A8)
#define 	ZIPDEC_OUT_LEN3				(ZIPDEC_BASE+0x00AC)

#define 	ZIPDEC_OUT_LEN4				(ZIPDEC_BASE+0x00B0)
#define 	ZIPDEC_OUT_LEN5				(ZIPDEC_BASE+0x00B4)
#define 	ZIPDEC_OUT_LEN6				(ZIPDEC_BASE+0x00B8)
#define 	ZIPDEC_OUT_LEN7				(ZIPDEC_BASE+0x00BC)
#define 	ZIPDEC_TIME_OUT 			(ZIPDEC_BASE+0x00C0)

#define	ZIPDEC_EN					BIT_0
#define	ZIPDEC_QUEUE_EN					BIT_3
#define	ZIPDEC_RUN					BIT_4

#define	ZIPDEC_IN_SWT_SHIFT				(8)
#define	ZIPDEC_IN_SWT_MASK				(0x03<<ZIPDEC_IN_SWT_SHIFT)

#define	ZIPDEC_OUT_SWT_SHIFT				(10)
#define	ZIPDEC_OUT_SWT_MASK				(0x03<<ZIPDEC_OUT_SWT_SHIFT)

#define	ZIPDEC_QUEUE_LEN_SHIFT				(16)
#define	ZIPDEC_QUEUE_LEN_MASK				(0x07<<ZIPDEC_QUEUE_LEN_SHIFT)


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


#define ZIPDEC_DONE_EN                     		BIT_0
#define ZIPDEC_QUEUE_DONE_EN              		BIT_1
#define ZIPDEC_ERR_EN                      		BIT_2
#define ZIPDEC_LEN_ERR_EN                 		BIT_5
#define ZIPDEC_SAVE_LEN_ERR_EN           		BIT_4
#define ZIPDEC_FETCH_LEN_ERR_EN          		BIT_3
#define ZIPDEC_TIMEOUT_EN                		BIT_6

#define ZIPDEC_DONE_CLR                     		BIT_0
#define ZIPDEC_QUEUE_DONE_CLR              		BIT_1
#define ZIPDEC_ERR_CLR                      		BIT_2
#define ZIPDEC_LEN_ERR_CLR                 		BIT_5
#define ZIPDEC_SAVE_LEN_ERR_CLR           		BIT_4
#define ZIPDEC_FETCH_LEN_ERR_CLR          		BIT_3
#define ZIPDEC_TIMEOUT_CLR                  		BIT_6

typedef void (*ZIP_INT_CALLBACK)(uint32_t);

void ZipMatrix_Reset(void);
void ZipDec_Enable(void);
void ZipDec_Disable(void);
void ZipDec_Run(void);
void ZipDec_Queue_Enable(uint32_t enable);
void ZipDec_SetSrcCfg(uint32_t num,uint32_t src_start_addr, uint32_t src_len);
void ZipDec_SetDestCfg(uint32_t num,uint32_t dest_start_addr,uint32_t dst_len);
void ZipDec_Reset(void);
void ZipDec_In_Swt(uint32_t mode);
void ZipDec_Out_Swt(uint32_t mode);
void ZipDec_QueueEn(void);
void ZipDec_QueueDisable(void);
void ZipDec_QueueLength(uint32_t length);
void ZipDec_IntEn(ZIPDEC_INT_TYPE int_type);
void ZipDec_IntDisable(ZIPDEC_INT_TYPE int_type);
void ZipDec_IntClr(ZIPDEC_INT_TYPE int_type);
void ZipDec_Set_Timeout(uint32_t time);
void ZipDec_SetSrcCfg(uint32_t num,uint32_t src_start_addr, uint32_t src_len);
void ZipDec_SetDestCfg(uint32_t num,uint32_t dest_start_addr,uint32_t dst_len);
void ZipDec_RegCallback (uint32_t chan_id, ZIP_INT_CALLBACK callback);
void ZipDec_UNRegCallback (uint32_t chan_id);
irqreturn_t ZipDec_IsrHandler (int irq ,void *data);


