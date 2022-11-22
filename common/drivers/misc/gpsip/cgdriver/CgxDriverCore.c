/******************************************************************************
 *                                LEGAL NOTICE                                *
 *                                                                            *
 *  USE OF THIS SOFTWARE (including any copy or compiled version thereof) AND *
 *  DOCUMENTATION IS SUBJECT TO THE SOFTWARE LICENSE AND RESTRICTIONS AND THE *
 *  WARRANTY DISLCAIMER SET FORTH IN LEGAL_NOTICE.TXT FILE. IF YOU DO NOT     *
 *  FULLY ACCEPT THE TERMS, YOU MAY NOT INSTALL OR OTHERWISE USE THE SOFTWARE *
 *  OR DOCUMENTATION.                                                         *
 *  NOTWITHSTANDING ANYTHING TO THE CONTRARY IN THIS NOTICE, INSTALLING OR    *
 *  OTHERISE USING THE SOFTWARE OR DOCUMENTATION INDICATES YOUR ACCEPTANCE OF *
 *  THE LICENSE TERMS AS STATED.                                              *
 *                                                                            *
 ******************************************************************************/
/* Version: 1.8.9\3686 */
/* Build  : 13 */
/* Date   : 12/08/2012 */

/**
	\file
	\brief CellGuide CGsnap driver core logic (OS & CPU independed)
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide

*/


#include <linux/delay.h>

#include "CgxDriverCore.h"
#include "CgCpu.h"
#include "CgCpuOs.h"
#include "platform.h"
#include "CgxDriverPlatform.h"
#include "CgxDriverOs.h"
#include "CgBuildNumber.h"
#include "CgVersion.h"
#include "CgxDriverCoreCommon.h"


/** \cond */ // Not documented - these are CRT prototypes
void *memcpy(void *dest, const void *src, unsigned int n);
/** \endcond */



/**
	Driver version structure
	\note this structure is filled automatically on build time, by CellGuide build machine
*/
const TCgVersion gCgDriverVersion = {
	CG_BUILD_VERSION,			/**< Software Version number (Text)*/
	CG_BUILD_ORIGINATOR,		/**< Build Originator name (Text)*/
	CGX_PLATFORM_NAME,			/**< Platform Name (Text), as specified in platform.h file */
	CG_BUILD_NUMBER,			/**< Unique Build Number (Text) - automatically assigned by build machine*/
	CG_BUILD_DATE,				/**< Build Date (Text)*/
	CG_BUILD_TIME,				/**< Build Time (Text)*/
	CG_BUILD_MODE				/**< Build Mode (Text) - Release/Test/Debug */
};

extern void gps_chip_power_on(void);
extern void gps_chip_power_off(void);
extern void gps_gpio_request(void);

#define CG_MATH_ALIGNMENT(__var__,__base__)    __var__ = (__base__) * ((U32)(((__var__) + ((__base__)/2)) / (__base__)))


TCgCpuDmaTask gChunksList[MAX_DMA_TRANSFER_TASKS]; /**< DMA tasks list */
S32 CgxDriverSnapLengthExtraBytes = 0;

extern U32 CGCORE_BASE_ADDR;

/** CGsnap GPO register offset */
const U32 CGCORE_REG_OFFSET_GPO		= 0x00000040;

/** CGsnap version register offset */
const U32 CGCORE_REG_OFFSET_VERSION		= 0x00000044;

/** CGsnap interrupt register offset (KAGPSRawInt)*/
const U32 CGCORE_REG_RAW_INT			= 0x00000080;

/** CGsnap Soft_CMD register offset */
const U32 CGCORE_REG_OFFSET_SOFT_CMD	= 0x000000CC;
const U32 CGCORE_SOFT_CMD_POWER_DOWN	= 0xFDB96420;

/** CGsnap reset register offset */

const U32 CGCORE_REG_OFFSET_CORE_RESETS = 0x000000FC;

#ifndef RF_POWER_UP_VAL
	#define RF_POWER_UP_VAL	(0)	// ACLYS requires '0' for PU
#endif

bool flag_power_up = 0;
TCgReturnCode CgxDriverTcxoControl(u32 aEnable);

#define CGCORE_ENABLE_CORE		(0x00000000)
#define CGCORE_ENABLE_TCXO		(0x00000001)
#define CGCORE_ENABLE_RTC		(0x00000002)
#define CGCORE_ENABLE_BS		(0x00000003)
#define CGCORE_RF_POWER_UP_POL	(0x00000004)			// RF Power up polarity. "Power UP" GPO[0] value is determined by this bit's value.
														// (if bit is '1', during power up, the IP will output '1', and during power down,
														// it will output '0'. When this bit is '0', the output is reversed)

const U32 CGCORE_CORE_RESETS_ENABLE		= ((1<<CGCORE_ENABLE_CORE) | (1<<CGCORE_ENABLE_TCXO) | ( RF_POWER_UP_VAL * (1<<CGCORE_RF_POWER_UP_POL)));
														// This is the default 'enable' value, to the core_Resets register.
														// It needs to be written prior to accessing CGsnap.
														// ALL CGCORE_ENABLE_XXX are reset to 0, when the MasterReset pin is toggled

#define CGCORE_ENABLE_DMA	(5)
#define CGCORE_ENABLE_GPS	(12)


#define CGCORE_ENABLE_SCLK  ((1<<CGCORE_ENABLE_DMA) | (1<<CGCORE_ENABLE_GPS))
#define CGCORE_RESET_DELAY			(1000)		// Used as a delay between '0' and '1', when reseting the CGsnap

TCgReturnCode CGCoreReset(U32 aUnitToReset);


//bxd add for slck enable
TCgReturnCode CGCoreSclkEnable(int enable)
{
	TCgReturnCode rc = ECgOk;

	if(1 == enable)
	{
		sci_glb_set(CG_DRIVER_SCLK_VA,BIT_12);
	}
	else if(0 == enable)
	{
		sci_glb_clr(CG_DRIVER_SCLK_VA,BIT_12);
	}
 	return rc;
}



TCgReturnCode CgxDriverDestroy(void *pDriver)
{
	DBG_FUNC_NAME("CgxDriverDestroy")
	TCgReturnCode rc = ECgOk;

	rc = CgCpuDmaDestroy(CG_DRIVER_DMA_CHANNEL_READ, CGCORE_BASE_ADDR + 0x00000070);
	if (!OK(rc)) DBGMSG("Failed to destroy DMA");
	rc = CgCpuGpioDestroy();
	if (!OK(rc)) DBGMSG("Failed to destroy GPIO");
	rc = CgCpuIntrDestroy();
	if (!OK(rc)) DBGMSG("Failed to destroy INTR");
	rc = CgCpuClockDestroy();
	if (!OK(rc)) DBGMSG("Failed to destroy CLK");
	rc = CgxDriverTransferEndDestroy(pDriver);
	return rc;
}



TCgReturnCode CgxDriverConstruct(void *pDriver, TCgxDriverState *pState)
{
	//DBG_FUNC_NAME("CgxDriverConstruct")
	TCgReturnCode rc = ECgOk;
	rc = CgCpuAllocateVa();
	memset(gChunksList,0,sizeof(TCgCpuDmaTask)*MAX_DMA_TRANSFER_TASKS);

#if 0
	if (OK(rc)) rc = CgCpuDmaCreate(CG_DRIVER_DMA_CHANNEL_READ, CGCORE_BASE_ADDR + 0x00000070);

	// set up DMA interrupt handler
	if (OK(rc)) rc = CgxDriverDmaInterruptPrepare();
#endif

	if (OK(rc)) rc = CgxDriverDataReadyInterruptHandlerStart(pDriver);

	// set up internal synchronization mechanism
	if (OK(rc)) rc = CgxDriverTransferEndCreate(pDriver);


	// set up GPS interrupt handler
	if (OK(rc)) rc = CgxDriverGpsInterruptPrepare();
	if (OK(rc)) rc = CgxDriverGpsInterruptHandlerStart(pDriver);
#if 0
	// enable IP (not-reset)
	if (OK(rc)) rc = CgCpuGpioModeSet(CG_DRIVER_GPIO_GPS_MRSTN, ECG_CPU_GPIO_OUTPUT);
       if (OK(rc)) rc = CgCpuIPMasterResetOn();
	if (OK(rc)) rc = CgCpuDelay(CGCORE_RESET_DELAY);
	if (OK(rc)) rc = CgCpuIPMasterResetClear();

    // set up Sclk register
    if (OK(rc)) rc = CGCoreSclkEnable(1);

   DBGMSG1("CGCORE_CORE_SCLK_ENABLE = 0x%08X", CGCORE_ENABLE_SCLK);

    // set up core-resets register
    if (OK(rc)) rc = CGCORE_WRITE_REG( CGCORE_REG_OFFSET_CORE_RESETS, CGCORE_CORE_RESETS_ENABLE);

    DBGMSG1("CGCORE_CORE_RESETS_ENABLE = 0x%08X", CGCORE_CORE_RESETS_ENABLE);

	// Setup GPIO for RF-Power, and power up RF chip
	//if (OK(rc)) rc = CgCpuGpioModeSet(CG_DRIVER_GPIO_RF_PD, ECG_CPU_GPIO_OUTPUT);
	//if (OK(rc)) rc = CgxDriverRFPowerUp();

    if (OK(rc)) rc = CgCpuGpioModeSet(CG_DRIVER_GPIO_TCXO_EN, ECG_CPU_GPIO_OUTPUT);
	if (OK(rc)) rc = CgxDriverTcxoControl(TRUE);
	gps_chip_power_off();
#endif

	gps_gpio_request();

	pState->constructionRc = rc;	// For later reference (if needed by application)
	pState->flags.resume = FALSE;

	return rc;
}



TCgReturnCode CgxDriverPowerUp(void)
{
	TCgReturnCode rc = ECgOk;
	U32 version = 0;

	// In order to power up the CGsnap, do a dummy access
	rc = CGCORE_READ_REG(CGCORE_REG_OFFSET_VERSION, &version );
	return rc;
}

TCgReturnCode CgxDriverPowerDown(void)
{
	TCgReturnCode rc = ECgOk;

	// Write software power-down command to CGsnap
	rc = CGCORE_WRITE_REG( CGCORE_REG_OFFSET_SOFT_CMD, CGCORE_SOFT_CMD_POWER_DOWN );

	return rc;
}

#if 0
//gaole add
void CgCoreReleaseRes(void)
{
	printk("\n gaole:  run to CgCoreReleaseRes \n");
	memset(gChunksList,0,sizeof(TCgCpuDmaTask)*MAX_DMA_TRANSFER_TASKS);
}

void CgGpsReset(void)
{
	CGCORE_WRITE_REG( 0x00000038, 0x0);

	CGCORE_WRITE_REG( 0x0000003c, 0x0);

	CGCORE_WRITE_REG( CGCORE_REG_OFFSET_CORE_RESETS, CGCORE_CORE_RESETS_ENABLE);
}
#endif
/*
split blocks to chunks
last chunk might be small
last chunk generate app event
start of block aligned to chunk size
*/

TCgReturnCode CgxDriverPrepareRecieve(
	TCgxDriverState *pState,
	unsigned char *apBuf,
	U32 aBufPhys,
	U32 aBufSize,
	unsigned long aLength,
	unsigned long aBlockLength,
	TCgByteOrder aByteOrder)
{
	DBG_FUNC_NAME("CgxDriverPrepareRecieve")
	TCgReturnCode rc = ECgOk;
	U32 blockIndex = 0;
	U32 chunksInBlock = 0;
	U32 chunkSize = 0;
	U32 length = aLength + CgxDriverSnapLengthExtraBytes; // Potential patch
	U32 numOfBlocksInBuffer = 0;

	memset(gChunksList,0,sizeof(TCgCpuDmaTask)*MAX_DMA_TRANSFER_TASKS);

	DBGMSG1("buffer v-address = 0x%x", apBuf);
	DBGMSG1("start address value = 0x%x", *((U32*)apBuf) );
	DBGMSG1("dest address value = 0x%x", *((U32*)apBuf + 128) );

	pState->transfer.blockSize = ((aBlockLength == 0) || (length < aBlockLength)) ? length : aBlockLength;
	/*bxd modify for not limted to 512k Bytes each block*/
	chunkSize =  pState->transfer.blockSize;
	//chunkSize = MIN(MAX_DMA_CHUNK_SIZE, pState->transfer.blockSize);
	chunksInBlock = pState->transfer.blockSize / chunkSize;


        DBGMSG3("Requested %d bytes, Block=(%d bytes,%d chunks)", length, pState->transfer.blockSize, chunksInBlock);
	DBGMSG1("Maximum DMA chunk size is set to %d bytes", chunkSize);

	pState->flags.overrun = FALSE;
	pState->transfer.cancel.request = FALSE;
	pState->transfer.cancel.onByte = 0;
	pState->transfer.cancel.onChunk = 0;
	pState->transfer.done = 0;
	pState->transfer.originalBuffer = apBuf;
	pState->transfer.bufferPhys = pState->buffer.physAddr;
	pState->transfer.byteOrder = aByteOrder;

	pState->transfer.bytes.required = length;
	pState->transfer.bytes.received = 0;

	pState->transfer.blocks.required = (length + pState->transfer.blockSize - 1) / pState->transfer.blockSize;
	DBGMSG1("%d blocks required", pState->transfer.blocks.required);
	pState->transfer.lastBlockSize = length - (pState->transfer.blocks.required - 1) * pState->transfer.blockSize;
	DBGMSG1("last block size: %d bytes", pState->transfer.lastBlockSize);
	pState->transfer.blocks.received = 0;

	pState->transfer.chunks.required = 0;
	pState->transfer.chunks.received = 0;
	pState->transfer.chunks.active = 0;

	// split snap to blocks
	//////////////////////////////////////////////////////////////////////////

	if (pState->transfer.blocks.required * chunksInBlock > MAX_DMA_TRANSFER_TASKS) {
		DBGMSG2("ERROR! too many chunks! %d > %d", pState->transfer.blocks.required * chunksInBlock, MAX_DMA_TRANSFER_TASKS);
		return ECgErrMemory;
		}

	numOfBlocksInBuffer = aBufSize / pState->transfer.blockSize;
	DBGMSG2("aBufSize = %d, blockSize = %d", aBufSize, pState->transfer.blockSize );
	for(blockIndex = 0; blockIndex < pState->transfer.blocks.required; blockIndex++)
	{
		// Init the snap's blocks' offsets
		pState->transfer.bufferOffset[blockIndex] = pState->transfer.blockSize * (blockIndex % numOfBlocksInBuffer);	// roll-over to start of buffer,
																														// if we reached end of buffer.
		//DBGMSG2("buffer[%d] = 0x%08X", blockIndex, pState->transfer.bufferPhys + pState->transfer.bufferOffset[blockIndex])
	}

	// split blocks to chunks
	//////////////////////////////////////////////////////////////////////////

	for(blockIndex = 0; blockIndex < pState->transfer.blocks.required; blockIndex++) {
		U32 chunkIndex = 0;
		U32 more, dest;

		more = (blockIndex == (pState->transfer.blocks.required-1)) ? pState->transfer.lastBlockSize : pState->transfer.blockSize;
		dest = pState->transfer.bufferPhys + pState->transfer.bufferOffset[blockIndex];

		//		DBGMSG4("Block %02d/%02d: % 6d bytes @ 0x%08X", blockIndex + 1, pState->transfer.blocks.required,
		for(chunkIndex = 0; chunkIndex < chunksInBlock-1; chunkIndex++) {
			U32 size = MIN(more, chunkSize);
			if ((more - size) < chunkSize)
				break;
			gChunksList[pState->transfer.chunks.required].address = dest;
			gChunksList[pState->transfer.chunks.required].length = size;
			more -= size;
			//			DBGMSG5("chunk %02d/%02d in block: length %d bytes @ 0x%08X (%d more)",	chunkIndex + 1, chunksInBlock,
			dest += size;
			pState->transfer.chunks.required++;
			}
		// last chunk
		gChunksList[pState->transfer.chunks.required].address = dest;
		gChunksList[pState->transfer.chunks.required].length = more;
				DBGMSG3("last chunk out of %02d in block: % 6d bytes @ 0x%08X", chunksInBlock, more, dest);
		pState->transfer.chunks.required++;
	}

	DBGMSG1("%d chunks required", pState->transfer.chunks.required);

	if (length > pState->transfer.blockSize * pState->transfer.blocks.required) {
		// there are leftovers
		U32 more = length - pState->transfer.blockSize * pState->transfer.blocks.required;
		U32 dest = pState->transfer.bufferPhys + pState->transfer.bufferOffset[pState->transfer.blocks.required];
				DBGMSG4("Extra Block % 2d/% 2d: % 6d bytes @ 0x%08X", pState->transfer.blocks.required, pState->transfer.blocks.required, more, dest);
		for (; (more > 0) && (pState->transfer.chunks.required < MAX_DMA_TRANSFER_TASKS); pState->transfer.chunks.required++) {
			U32 size = MIN(more, chunkSize);
			gChunksList[pState->transfer.chunks.required].address = dest;
			gChunksList[pState->transfer.chunks.required].length = size;
			more -= size;
			dest += size;
			DBGMSG4("chunk % 4d: % 4d bytes @ 0x%08X (%d bytes left)", pState->transfer.chunks.required + 1, size, dest, more);
			}
		DBGMSG1("%d chunks required", pState->transfer.chunks.required);
		}


	// setup 1st DMA chunk
         #ifndef CGCORE_ACCESS_VIA_SPI
	rc = CgCpuDmaSetupFromGps(gChunksList, pState->transfer.chunks.required, CG_DRIVER_DMA_CHANNEL_READ, CGCORE_BASE_ADDR + 0x00000070);
	rc = CgCpuDmaStart(CG_DRIVER_DMA_CHANNEL_READ);
        #else
      //  rc =CgxCpuSpiReadData();
        #endif

	return rc;
}



TCgReturnCode CgxDriverIsReadCanceled(TCgxDriverState *pState)
{
	DBG_FUNC_NAME("CgxDriverIsReadCanceled")
	TCgReturnCode rc = ECgOk;


	// Check if snap was unconditionally canceled
	if (pState->transfer.cancel.request && (pState->transfer.bytes.received > pState->transfer.cancel.onByte))
	{
		CGCoreReset(CGCORE_ENABLE_CORE);
		CgCpuDmaStop(CG_DRIVER_DMA_CHANNEL_READ);
		return ECgCanceled;
	}

	// Check if we have reached the cancel point
	if (pState->transfer.cancel.request) {
		U32 curDmaCount = 0;
		U32 requestedDmaCount = 0;
		rc = CgCpuDmaRequestedCount(CG_DRIVER_DMA_CHANNEL_READ, &requestedDmaCount); // how many bursts requested to receive
		DBGMSG1("requestedDmaCount = %d", requestedDmaCount);

		rc = CgCpuDmaCurCount(CG_DRIVER_DMA_CHANNEL_READ, &curDmaCount); // how many bursts left to receive
		DBGMSG1("curDmaCount = %d", curDmaCount);

		DBGMSG2("%d >= %d ? ==> canceled", pState->transfer.bytes.received + (requestedDmaCount - curDmaCount) , pState->transfer.cancel.onByte);

		if (pState->transfer.bytes.received + (requestedDmaCount - curDmaCount) >= pState->transfer.cancel.onByte)
			{
			CGCoreReset(CGCORE_ENABLE_CORE);
			CgCpuDmaStop(CG_DRIVER_DMA_CHANNEL_READ);
			return ECgCanceled;
			}
		}

	return ECgOk;
}

TCgReturnCode CGCoreEnable(U32 aUnitToEnable)
{
	DBG_FUNC_NAME("CGCoreEnable")
	TCgReturnCode rc = ECgOk;

	U32 enableValue = 1;
	U32 coreResetsReg = 0;

	rc = CGCORE_READ_REG(CGCORE_REG_OFFSET_CORE_RESETS, &coreResetsReg);
//	DBGMSG1("Read coreResetsReg=0x%08X", coreResetsReg);
	REG_SET1(coreResetsReg, aUnitToEnable, enableValue);
	if (OK(rc)) rc = CGCORE_WRITE_REG( CGCORE_REG_OFFSET_CORE_RESETS, coreResetsReg);
	DBGMSG1("Write [0x%08X]", coreResetsReg);
	return rc;
}

TCgReturnCode CGCoreDisable(U32 aUnitToDisable)
{
	DBG_FUNC_NAME("CGCoreDisable")
	TCgReturnCode rc = ECgOk;

	U32 disableValue = 0;
	U32 coreResetsReg = 0;

	rc = CGCORE_READ_REG(CGCORE_REG_OFFSET_CORE_RESETS,&coreResetsReg);
//	DBGMSG1("Read coreResetsReg=0x%08X", coreResetsReg);
	REG_SET1(coreResetsReg, aUnitToDisable, disableValue);
	if (OK(rc)) rc = CGCORE_WRITE_REG(CGCORE_REG_OFFSET_CORE_RESETS,coreResetsReg);
	DBGMSG1("Write [0x%08X]", coreResetsReg);
	return ECgOk;
}

TCgReturnCode CGCoreReset(U32 aUnitToReset)
{
	DBG_FUNC_NAME("CGCoreReset")
	TCgReturnCode rc = ECgOk;
	U32 coreResetsReg = 0;
	U32 coreResetsRegOld = 0;

	////////////////// Core Disable //////////////
	rc = CGCORE_READ_REG(CGCORE_REG_OFFSET_CORE_RESETS,&coreResetsReg);
	coreResetsRegOld = coreResetsReg;
	REG_SET1(coreResetsReg, aUnitToReset, 0);
	if (OK(rc)) rc = CGCORE_WRITE_REG(CGCORE_REG_OFFSET_CORE_RESETS,coreResetsReg);
	DBGMSG1("Write [0x%08X]", coreResetsReg);

	if (OK(rc))	rc = CgCpuDelay(CGCORE_RESET_DELAY);

	////////////////// Core Enable //////////////
	if (OK(rc)) rc = CGCORE_WRITE_REG( CGCORE_REG_OFFSET_CORE_RESETS, coreResetsRegOld);
	DBGMSG1("Write [0x%08X]", coreResetsRegOld);

	return rc;
}


TCgReturnCode CgxDriverExecuteSpecific(
	void *pDriver,
	TCgxDriverState *pState,
	U32 aRequest,
	TCgDriverControl *pControl,
	TCgDriverStatus *pResults)
{
	DBG_FUNC_NAME("CgxDriverExecuteSpecific")
        DBGMSG1("aRequest =%x",aRequest);
    switch (aRequest) {
		case CGX_IOCTL_INIT:
			DBGMSG("CGX_IOCTL_INIT");
			// This is 1st time initialization, meaning the software is restarted, not resumed, so set the state accordingly.
			DBGMSG1("pState = 0x%x", pState);

			pState->flags.resume = FALSE;
                        DBGMSG("after pState->flags.resume = FALSE;");
			pResults->rc =  CgxDriverInit(pDriver);
			break;

		case CGX_IOCTL_STOP:
			pResults->rc =  CgxDriverDestroy(pDriver);
			break;

		case CGX_IOCTL_READ_REG:
			pState->transfer.cancel.request = FALSE;
			// Verify register is of the CGX_sat_ram group
			pResults->readReg.value = 0;
			DBGMSG1("readReg.offset %x ",pControl->readReg.offset);
			if ( pControl->readReg.offset >= CGCORE_REG_OFFSET_SAT_RAM && pControl->readReg.offset <= CGCORE_REG_OFFSET_SAT_RAM_LAST )
			{
				// Write read command to the APB (9 bits of the register offset, with 0x80000000, as the read command)
				pResults->rc =  CGCORE_WRITE_REG( CGCORE_REG_OFFSET_APB_COMMAND, (0x000001FF & pControl->readReg.offset) | 0x80000000);

				// Wait 4 system clocks
				CgCpuDelay(100);	// TODO : what is the actual period of this wait. Is it 4 system clocks?

				// Read data from the APB
				if (OK(pResults->rc))
					pResults->rc = CGCORE_READ_REG( CGCORE_REG_OFFSET_APB_DATA, &pResults->readReg.value);
			}
			else
			{
				// It is not part of the CGX_sat_ram. Read register normaly, via the APB

				pResults->rc = CGCORE_READ_REG( pControl->readReg.offset, &pResults->readReg.value);
			}
			break;

		case CGX_IOCTL_WRITE_REG:
			pState->transfer.cancel.request = FALSE;
			// Write to CGsnap register
			pResults->rc =  CGCORE_WRITE_REG( pControl->writeReg.offset, pControl->writeReg.value);
			// DBGMSG2("CGX_IOCTL_WRITE_REG *0x%08X = 0x%08X", CGCORE_BASE_ADDR_VA + pControl->writeReg.offset, pControl->writeReg.value);
			break;

		case CGX_IOCTL_PREPARE_RECEIVE:
             DBGMSG("CGX_IOCTL_PREPARE_RECEIVE");
			// Prepare driver for massive data receive (snap)
			pState->transfer.cancel.request = FALSE;
			pResults->rc = CgxDriverPrepareRecieve(pState, pControl->read.buf, pControl->read.bufPhys,
				pState->buffer.length, 	pControl->read.alignedLength, pControl->read.blockLength,
				pControl->read.byteOrder);
			break;

		case CGX_IOCTL_STATUS:
			pResults->rc = pState->constructionRc;
			break;

		case CGX_IOCTL_RESUME_CLEAR:
			DBGMSG("CGX_IOCTL_RESUME_CLEAR!");
			pState->flags.resume = FALSE;
			pState->transfer.cancel.request = FALSE;
			break;
		case CGX_IOCTL_RESET:
            DBGMSG1("CGX_IOCTL_RESET level %d", pControl->reset.resetLevel);
			pState->transfer.cancel.request = FALSE;
			switch( pControl->reset.resetLevel )
			{
			case 0:	// Full reset
				if (OK(pResults->rc)) pResults->rc = CgCpuIPMasterResetOn();
				if (OK(pResults->rc)) pResults->rc = CgCpuDelay(CGCORE_RESET_DELAY);
				if (OK(pResults->rc)) pResults->rc = CgCpuIPMasterResetClear();
				if (OK(pResults->rc)) pResults->rc = CGCORE_WRITE_REG( CGCORE_REG_OFFSET_CORE_RESETS, CGCORE_CORE_RESETS_ENABLE);
				break;
			case 1:	// Semi reset
				if (OK(pResults->rc)) pResults->rc = CGCoreReset(CGCORE_ENABLE_CORE);
				break;
			case 2:	// CPU reset
				break;
			default : // Semi reset, also.
				if (OK(pResults->rc)) pResults->rc = CGCoreReset(CGCORE_ENABLE_CORE);
				break;
			}
			pResults->rc = CgCpuDmaStop(CG_DRIVER_DMA_CHANNEL_READ);
			break;

		case CGX_IOCTL_COUNTER_RESET:
            DBGMSG("CGX_IOCTL_COUNTER_RESET!");
			pResults->rc = CgCpuDmaStop(CG_DRIVER_DMA_CHANNEL_READ);
			if (OK(pResults->rc)) pResults->rc = CGCoreReset(CGCORE_ENABLE_TCXO);
			break;

		case CGX_IOCTL_POWER_DOWN:
            DBGMSG("CGX_IOCTL_POWER_DOWN!");
			flag_power_up = 0;
			pResults->rc = CgxDriverPowerDown();
			if (OK(pResults->rc)) pResults->rc = CgxDriverRFPowerDown();
			break;

		case CGX_IOCTL_POWER_UP:
			flag_power_up = 1;
			pState->transfer.cancel.request = FALSE;
			pResults->rc = CgxDriverPowerUp();
			if (OK(pResults->rc)) pResults->rc = CgxDriverRFPowerUp();
			break;

		case CGX_IOCTL_POWER_OFF :
			pResults->rc = CgxDriverPowerOff();
			break;

		case CGX_IOCTL_POWER_ON :
			pResults->rc = CgxDriverPowerOn();
			break;

		case CGX_IOCTL_GPIO_SET:
			//DBGMSG2("CGX_IOCTL_GPIO_SET 0x%08X = %d", pControl->GPIO.code, pControl->GPIO.out);
			if (pControl->GPIO.out)
				pResults->rc = CgCpuGpioSet(pControl->GPIO.code);
			else
				pResults->rc = CgCpuGpioReset(pControl->GPIO.code);
			//rc = CgxDriverDebugGpio((int)pControl->GPIO.code, (int)pControl->GPIO.out);
			break;

		case CGX_IOCTL_GPIO_GET:
			//DBGMSG1("CGX_IOCTL_GPIO_GET 0x%08X", pControl->GPIO.code);
			//pResults->GPIO.in = 0x5a5a;
			//DBGMSG2("CGX_IOCTL_GPIO_GET before    0x%08X=0x%08X", pControl->GPIO.code, pResults->GPIO.in)
			pResults->rc = CgCpuGpioGet(pControl->GPIO.code, (int *)&(pResults->GPIO.val));
			pResults->GPIO.code = pControl->GPIO.code;
			//DBGMSG2("CGX_IOCTL_GPIO_GET after     0x%08X=0x%08X", pControl->GPIO.code, pResults->GPIO.in)
			//rc = CgxDriverDebugGpioRead((int)pResults->GPIO.code, (int*)&pResults->GPIO.in);
			break;

		case CGX_IOCTL_GET_VERSION:
			memcpy(pResults->version.buildVersion, gCgDriverVersion.buildVersion, sizeof(pResults->version.buildVersion));
			memcpy(pResults->version.buildMode, gCgDriverVersion.buildMode, sizeof(pResults->version.buildNumber));
			memcpy(pResults->version.buildNumber, gCgDriverVersion.buildNumber, sizeof(pResults->version.buildNumber));
			memcpy(pResults->version.buildTime, gCgDriverVersion.buildTime, sizeof(pResults->version.buildTime));
			memcpy(pResults->version.buildDate, gCgDriverVersion.buildDate, sizeof(pResults->version.buildDate));
			break;

		case CGX_IOCTL_CANCEL_RECEIVE:
			DBGMSG1("Request cancel on byte %d", pControl->cancel.onByteCount);

			// set up receive abort flag.
			// the flag is cleared be prepare receive command

			if (pControl->cancel.onByteCount > (U32)CgxDriverSnapLengthExtraBytes) {
				pState->transfer.cancel.onByte = pControl->cancel.onByteCount - CgxDriverSnapLengthExtraBytes;
				CG_MATH_ALIGNMENT((pState->transfer.cancel.onByte), NUMBER_1K);
				DBGMSG1("cancel on Byte: 0x%08X", pState->transfer.cancel.onByte);
				}
			else {
				pState->transfer.cancel.onByte = 0; // Cancel snap req immediately
				}
 			pState->transfer.cancel.request = TRUE;
			DBGMSG1("cancel on Byte: 0x%08X", ((pState->transfer.cancel.onByte / 16) & 0x00FFFFF) | 0x0F000000);
			CGCORE_WRITE_REG( 0x30, ((pState->transfer.cancel.onByte / 16) & 0x00FFFFF) | 0x0F000000);
			CGCORE_WRITE_REG( 0x34, 0);

			// check if cancel point passed
			pResults->rc = CgxDriverIsReadCanceled(pState);
			if ((pControl->cancel.onByteCount == 0) ||	// Cancel snap req immediately
				(!OK(pResults->rc) && pState->flags.wait)) {// release a waiting thread, if any
				CGCoreReset(CGCORE_ENABLE_CORE);
				CgCpuDmaStop(CG_DRIVER_DMA_CHANNEL_READ);
				CgxDriverDataReadyInterruptHandler(pDriver, pState);
				}

			break;

		case CGX_IOCTL_ALLOC_BUFF:
            DBGMSG1("CGX_IOCTL_ALLOC_BUFF, pControl->alloc.length=%d", pControl->alloc.length);

 //                        pControl->alloc.length = 1 *1024 * 1024; // 1M

			pResults->rc = CgxDriverAllocInternalBuff(pDriver, pControl->alloc.length,
													(void**)&(pResults->buffer.bufferPhysAddr), pControl->alloc.processId);
			DBGMSG1("alloc p =0x%08X", pState->buffer.virtAddr);
			if (OK(pResults->rc)) {
				pResults->buffer.size = pControl->alloc.length;
				pResults->buffer.bufferAppVirtAddr = pState->buffer.virtAddr;
				}
			break;

		case CGX_IOCTL_FREE_BUFF:
			DBGMSG("CGX_IOCTL_FREE_BUFF");
			pResults->rc = CgxDriverFreeInternalBuff(pDriver, pControl->alloc.processId);
			break;
		case CGX_IOCTL_READ_MEM:
            DBGMSG("CGX_IOCTL_READ_MEM");
			pState->transfer.cancel.request = FALSE;
			DBGMSG1("read from 0x%08X", pControl->readReg.offset);
			pResults->rc = CgxCpuReadMemory(pControl->readReg.offset, 0, &pResults->readReg.value);
			break;

		case CGX_IOCTL_CPU_REVISION:
			pResults->rc = CgCpuRevision(pResults->cpuRevision.revisionCode);	// TODO - fix CgCpuRevision so it reads from the right address
			break;

		case CGX_IOCTL_TCXO_ENABLE:
            DBGMSG("CGX_IOCTL_TCXO_ENABLE");
			pResults->rc = CgxDriverTcxoControl(pControl->tcxoControl.enable);
			break;

		case CGX_IOCTL_RF_WRITE_CONTROL:
            DBGMSG("CGX_IOCTL_RF_WRITE_CONTROL");
			pResults->rc = CgCpuRFControlWriteByte(pControl->writeReg.value);
			break;

		default:
			return ECgGeneralFailure;
		}

	return ECgOk;
}

TCgReturnCode CgxDriverPowerOn(void)
{
	TCgReturnCode rc = ECgOk;

	gps_chip_power_on();

	// Enable clock to the GPS device
	rc = CGCoreEnable(CGCORE_ENABLE_TCXO);

	return rc;
}

TCgReturnCode CgxDriverPowerOff(void)
{
	TCgReturnCode rc = ECgOk;

	// Disable clock to the GPS device
	rc = CGCoreDisable(CGCORE_ENABLE_TCXO);
	gps_chip_power_off();

	return rc;
}

TCgReturnCode CgxDriverTcxoControl(u32 aEnable)
{
	TCgReturnCode rc = ECgOk;

	if(aEnable)
		CGCoreEnable(CGCORE_ENABLE_TCXO);
	else
		CGCoreDisable(CGCORE_ENABLE_TCXO);;

	return rc;
}

TCgReturnCode CgCGCoreGpioSet(U32 aGpioPin, U32 aVal)
{
	TCgReturnCode rc = ECgOk;
	U32 valToSet = 0;

	if (aGpioPin <= 0 || aGpioPin > 4)		rc = ECgBadArgument;

	if (OK(rc))	rc = CGCORE_READ_REG( CGCORE_REG_OFFSET_GPO, &valToSet);
	REG_SET1(valToSet,aGpioPin,aVal ? 1 : 0);
	if (OK(rc))	rc = CGCORE_WRITE_REG( CGCORE_REG_OFFSET_GPO, valToSet );

	return rc;
}

/*

TCgReturnCode CgxDriverRFPowerDown(void)
{
	DBG_FUNC_NAME("CgxDriverRFPowerDown")
	TCgReturnCode rc = ECgOk;

	#if CG_DRIVER_CG_CORE_CONTROLS_RF_PD
		// Do nothing - Core controls RF power down pin
	#else
		#ifdef RF_POWER_UP_VAL
			rc = CgCpuGpioSet(CG_DRIVER_GPIO_RF_PD, RF_POWER_UP_VAL);
		#else
			rc = CgCpuGpioSet(CG_DRIVER_GPIO_RF_PD, 1);
		#endif
	#endif

	return rc;
}

TCgReturnCode CgxDriverRFPowerUp(void)
{
	DBG_FUNC_NAME("CgxDriverRFPowerUp");
	TCgReturnCode rc = ECgOk;

	#if CG_DRIVER_CG_CORE_CONTROLS_RF_PD
		// Do nothing - Core controls RF power down pin
	#else
		#ifdef RF_POWER_UP_VAL
			rc = CgCpuGpioSet(CG_DRIVER_GPIO_RF_PD, !RF_POWER_UP_VAL);
		#else
			rc = CgCpuGpioSet(CG_DRIVER_GPIO_RF_PD, 0);
		#endif
	#endif

	return rc;
}

*/
