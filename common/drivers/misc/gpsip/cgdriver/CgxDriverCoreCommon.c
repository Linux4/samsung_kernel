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
\brief CellGuide common driver core logic (OS, CPU and CG_Hardware independent)
\attention This file should not be modified.
If you think something is wrong here, please contact CellGuide

*/



#include "platform.h"
#include "CgBuildNumber.h"
#include "CgVersion.h"
#include "CgCpu.h"
#include "CgxDriverCore.h"
#include "CgxDriverCoreCommon.h"
#include "CgxDriverOs.h"

extern TCgCpuDmaTask gChunksList[MAX_DMA_TRANSFER_TASKS]; /**< DMA tasks list */

/** CGcore base address */
const U32 CGCORE_BASE_ADDR = CG_DRIVER_CGCORE_BASE_PA; /* Physical */
#ifndef CG_DRIVER_CGCORE_BASE_VA
	extern U32 CG_DRIVER_CGCORE_BASE_VA;
#endif

// Check validity of DMA channel and controller number, for read and write channels
///////////////////////////////////////////////////////////////////////////////////

//#if (CG_DMA_CONTROLLER(CG_DRIVER_DMA_CHANNEL_READ) > 0)
#if ( ((CG_DRIVER_DMA_CHANNEL_READ & 0xFFFF0000) >> 16) > 0 )
#endif

#ifdef CG_DRIVER_DMA_CHANNEL_READ
	#ifdef CG_DRIVER_DMA_MAX_CONTROLLER
		#if (CG_DMA_CONTROLLER(CG_DRIVER_DMA_CHANNEL_READ) > CG_DRIVER_DMA_MAX_CONTROLLER)
			#error DMA controller # is greater than controllers available
		#endif
	#else
		#if ( (CG_DMA_CONTROLLER(CG_DRIVER_DMA_CHANNEL_READ)) > 0)
			#error DMA controller # is greater than controllers available
		#endif
	#endif
	#ifdef CG_DRIVER_DMA_MAX_CHANNEL
		#if (CG_DMA_CHANNEL(CG_DRIVER_DMA_CHANNEL_READ) > CG_DRIVER_DMA_MAX_CHANNEL)
			#error DMA channel # is greater than channels available
		#endif
	#else
		#if (CG_DMA_CHANNEL(CG_DRIVER_DMA_CHANNEL_READ) > 32)
			#error DMA channel # is greater than channels available
		#endif
	#endif
#endif
#ifdef CG_DRIVER_DMA_CHANNEL_WRITE
	#ifdef CG_DRIVER_DMA_MAX_CONTROLLER
		#if CG_DMA_CONTROLLER(CG_DRIVER_DMA_CHANNEL_WRITE) > CG_DRIVER_DMA_MAX_CONTROLLER
			#error DMA controller # is greater than controllers available
		#endif
	#else
		#if CG_DMA_CONTROLLER(CG_DRIVER_DMA_CHANNEL_WRITE) > 0
			#error DMA controller # is greater than controllers available
		#endif
	#endif
	#ifdef CG_DRIVER_DMA_MAX_CHANNEL
		#if CG_DMA_CHANNEL(CG_DRIVER_DMA_CHANNEL_WRITE) > CG_DRIVER_DMA_MAX_CHANNEL
			#error DMA channel # is greater than channels available
		#endif
	#else
		#if CG_DMA_CHANNEL(CG_DRIVER_DMA_CHANNEL_WRITE) > 32
			#error DMA channel # is greater than channels available
		#endif
	#endif
#endif

// Block number starts at 1
BOOL isBlockCanceled(TCgxDriverState * apState, U32 aBlockNumber)
{
	if (	apState->transfer.cancel.request &&
			aBlockNumber * apState->transfer.blockSize >= apState->transfer.cancel.onByte )
		return TRUE;
	return FALSE;
}

// Block number starts at 1
BOOL wasBlockReceived(TCgxDriverState * apState, U32 aBlockNumber)
{

	if ( aBlockNumber <= apState->transfer.blocks.received )
		return TRUE;

	return FALSE;
}



#ifdef CGCORE_ACCESS_VIA_SPI
extern void trigger_gps_timeout(int timeout);
#endif



TCgReturnCode CgxDriverExecute(
									   void *pDriver,
									   TCgxDriverState *pState,
									   U32 aRequest,
									   TCgDriverControl *pControl,
									   TCgDriverStatus *pResults)
{
	DBG_FUNC_NAME("CgxDriverExecute")
	TCgReturnCode rc = ECgOk;

	if (pControl == NULL)		return ECgNullPointer;
	if (pResults == NULL)		return ECgNullPointer;
	if ((pState->flags.resume == TRUE) && (aRequest != CGX_IOCTL_INIT) && (aRequest != CGX_IOCTL_RESUME_CLEAR))
	{
		pResults->rc = ECgResume;
		pResults->state = *pState;
		DBGMSG1("Resume Need to clear [%d]", aRequest);
		return ECgOk;
	}
	pResults->rc = ECgOk;
	switch (aRequest)
	{
	case CGX_IOCTL_WAIT_RCV:

		// Check if current block was overrun by the DMA
		//////////////////////////////////////////////////////////////////////////

		if ( ((S32)pState->transfer.blocks.received - (S32)pControl->wait.blockNumber )
			> ((S32)pState->buffer.length / (S32)pState->transfer.blockSize) )

		{
			DBGMSG1("pState->transfer.blocks.received	= %d", pState->transfer.blocks.received);
			DBGMSG1("pControl->wait.blockNumber			= %d", pControl->wait.blockNumber);
			DBGMSG1("pState->buffer.length				= %d", pState->buffer.length);
			DBGMSG1("pState->transfer.blockSize			= %d", pState->transfer.blockSize);

			pResults->rc = ECgDataOverrun;
		}

		DBGMSG3("WAIT_RCV : blocks wait/received/total = %d/%d/%d",
				pControl->wait.blockNumber + 1, pState->transfer.blocks.received, pState->transfer.blocks.required);



		// Wait for block, if not canceled or already received
		//////////////////////////////////////////////////////////////////////////

		if ( !isBlockCanceled(pState, pControl->wait.blockNumber + 1))
		{

			// Check if block was received - if so, return. If not, wait for it
			if ( !wasBlockReceived(pState, pControl->wait.blockNumber + 1))
			{
				pState->flags.wait = TRUE;
				pResults->rc = CgxDriverTransferEndWait(pDriver, pControl->wait.timeoutMS);
			}
		}
		else
		{

			pResults->rc = ECgCanceled;
			if ( !isBlockCanceled(pState, pControl->wait.blockNumber))
			{
				  printk("kyle last block\n");
				  if ( !wasBlockReceived(pState, pControl->wait.blockNumber + 1))
				  {
					 pState->flags.wait = TRUE;
					 pResults->rc = CgxDriverTransferEndWait(pDriver,pControl->wait.timeoutMS);
				  }
			}
		}
		pState->flags.wait = FALSE;
		if (!OK(pResults->rc) && (pResults->rc != ECgCanceled))
		{
			DBGMSG2("CGX_IOCTL_WAIT_RCV Failed! %d (t.o=%d ms)", pResults->rc, pControl->wait.timeoutMS);
		}
		DBGMSG2("App Buffer 0x%08X, phys:0x%08X", pState->transfer.originalBuffer, pState->transfer.bufferPhys);
		DBGMSG2("received %d blocks %d chunks", pState->transfer.blocks.received, pState->transfer.chunks.received);

		// Store the requested block's address, for use of the application
		//////////////////////////////////////////////////////////////////////////

		if( pState->transfer.blocks.received == 0 )
		{
			// This is an error - we returned from wait(), but no block was received.
			pResults->buffer.bufferAppVirtAddr = pState->transfer.originalBuffer;
			if ( OK(pResults->rc) )
			{
				pResults->rc = ECgOutOfBounds;
				DBGMSG1("ECgOutOfBounds [received=%d]", pState->transfer.blocks.received);

			}
		}
		else
		{
			// Release possible event received, so we won't receive a false event - timeout is 0
			CgxDriverTransferEndWait(pDriver, 0);

			pResults->buffer.bufferAppVirtAddr = pState->transfer.originalBuffer + pState->transfer.bufferOffset[pControl->wait.blockNumber] ;
		}

		CgCpuCacheSync();
		DBGMSG2("received %d blocks %d chunks", pState->transfer.blocks.received, pState->transfer.chunks.received);
		DBGMSG3("0x%08X,0x%08X,%d", pState->transfer.bufferOffset[0], pState->transfer.bufferOffset[1],(pState->transfer.blocks.received-1) % 2);

		switch( pResults->rc )
		{
		case ECgOk:
			pResults->buffer.size = (pControl->wait.blockNumber == pState->transfer.blocks.required)
									? pState->transfer.lastBlockSize
									: pState->transfer.blockSize;
			break;
		case ECgCanceled:
			pResults->buffer.size = pState->transfer.cancel.onByte;
			break;
		default:
			break;
		}

		// Reorder bytes
		//////////////////////////////////////////////////////////////////////////

		//Bug 2981 (Critical) - requires a frame 00/01/02 after downloading- during CS/WS if (OK(pResults->rc))
		{
			DBGMSG2("Byte order %d %d", CGX_DRIVER_NATIVE_BYTE_ORDER, pState->transfer.byteOrder);
			if ((CGX_DRIVER_NATIVE_BYTE_ORDER != pState->transfer.byteOrder) &&
				(pState->transfer.chunks.received > 0))
			{
				U32 length = gChunksList[pControl->wait.blockNumber].length;
				if(pState->transfer.blocks.required <= 1)
					length = pState->transfer.blockSize;

				if ( isBlockCanceled(pState, pControl->wait.blockNumber + 1) )
				{
					//U32 requestedDmaCount = 0;
					//CgCpuDmaRequestedCount(CG_DRIVER_DMA_CHANNEL_READ, &requestedDmaCount);
					//length = (U32) pState->transfer.cancel.onByte % requestedDmaCount;
				}
				DBGMSG4("size %d  s_vir 0x%x, 0x%x ,0x%x", length,(U32)pState->buffer.virtAddr,
                                gChunksList[pControl->wait.blockNumber].address,pState->transfer.bufferPhys);

//                                pState->transfer.byteOrder = 0;

                                CgSwapBytesInBlock(	(void*)((U32)pState->buffer.virtAddr	+
					(gChunksList[pControl->wait.blockNumber].address - pState->transfer.bufferPhys)),
					(void*)((U32)pState->buffer.virtAddr	+
					(gChunksList[pControl->wait.blockNumber].address - pState->transfer.bufferPhys)),
					length,
					pState->transfer.byteOrder,
					CGX_DRIVER_NATIVE_BYTE_ORDER );




			}
		CgCpuCacheSync();
		}

		DBGMSG2("WAIT_RCV : rc = %p, pResults->rc = %p", pResults->buffer.bufferAppVirtAddr, pResults->buffer.bufferPhysAddr);
		DBGMSG2("WAIT_RCV : rc = %d, pResults->rc = %d", rc, pResults->rc);

		break;


	default:
		rc = CgxDriverExecuteSpecific(pDriver, pState, aRequest, pControl, pResults);
	}
	if (OK(rc))
	{
		if ((pState->flags.resume == TRUE)  || (pResults->rc == ECgResume)){
			pResults->rc = ECgResume;
			DBGMSG("Resume!");
		}
		DBGMSG1("End %d", aRequest);
		pResults->state = *pState;
        	DBGMSG1("the lastest %d", aRequest);
		switch(pResults->rc)
		{
		case ECgCanceled:
		case ECgResume:
			break;
		default:DBGERR(pResults->rc);
		}
	}
	return rc;
}

// this interrupt occur on end of DMA transaction (entire snap, block or sub-block)
TCgReturnCode CgxDriverDataReadyInterruptHandler(void *pDriver, TCgxDriverState *pState)
{
	DBG_FUNC_NAME("CgxDriverDataReadyInterruptHandler")
	TCgReturnCode rc = ECgOk;

	pState->transfer.blocks.received++;

	// update chunk counters
	pState->transfer.chunks.received++;
	pState->transfer.chunks.active++;

	// update total received bytes count
	pState->transfer.bytes.received += gChunksList[pState->transfer.chunks.active-1].length;

	DBGMSG2("%d/%d", pState->transfer.chunks.active, pState->transfer.chunks.required);

	if (pState->flags.wait) // release a waiting thread, if any
		rc = CgxDriverTransferEndSignal(pDriver);

	// check if end of snap (last block)
	if (! isBlockCanceled(pState, pState->transfer.blocks.received + 1) )
	{
		//	DBGMSG2("chunks.active = %d. chunks.received = %d", pState->transfer.chunks.active, pState->transfer.chunks.received);
		if (pState->transfer.chunks.active >= pState->transfer.chunks.required)
		{
			//	DBGMSG("last chunk");
			pState->transfer.done = 0;
		}
		#ifdef CG_DRIVER_DMA_MULTI_BLOCK_MODE
			#if CG_DRIVER_DMA_MULTI_BLOCK_MODE == CG_DRIVER_DMA_MULTI_BLOCK_MODE_PENDING
			// not last block, so we need to order another chunk
			else
			{
				// sub-block is used when DMA max size is small -->  only need to increment the size
				rc = CgCpuDmaSetupFromGpsNextTask(	gChunksList, pState->transfer.chunks.required, pState->transfer.chunks.active,
					CG_DRIVER_DMA_CHANNEL_READ, CGCORE_BASE_ADDR + 0x00000070);
			}
			#endif
		#endif
	}


	DBGMSG("Done");
	return rc;
}

TCgReturnCode CgxDriverGpsInterruptHandler(void *pDriver, TCgxDriverState *pState)
{
	DBG_FUNC_NAME("CgxDriverGpsInterruptHandler")
	TCgReturnCode rc = ECgOk;
	U32 intcode = 0;
	pState->counters.interrupt.all++;

	// read interrupt source register from CGsnap
	rc = CGCORE_READ_REG( CGCORE_REG_INT_SRC, &intcode);
	rc = CGCORE_WRITE_REG( CGCORE_REG_INT_CLR, intcode);
	printk("%s%s%s%s%s%s%s\n",
		intcode & CGCORE_INT_RTC		? "RTC " : "",
		intcode & CGCORE_INT_SNAP_START ? "BEG " : "",
		intcode & CGCORE_INT_SNAP_END	? "END " : "",
		intcode & CGCORE_INT_TCXO_EXPIRED ? "TCXO " : "",
		intcode & CGCORE_INT_OVERRUN	? "OVR " : "",
		intcode & CGCORE_INT_EOT		? "EOT " : "",
		intcode & CGCORE_INT_EOM		? "EOM " : ""
		);

	if (intcode & CGCORE_INT_SNAP_START) { // Indicates snap was started
		pState->counters.interrupt.snapStart++;
		DBGMSG1("%d so far\n", pState->counters.interrupt.snapStart);
		if (!isBlockCanceled(pState,1) && !pState->transfer.done && pState->transfer.bytes.required > 1)
		{
			// 1st DMA transfer task already set-up by PrepareReceive.
			// some DMAC don't have LLI support, but other mechanism that requires setting next block
			// each time
			#ifdef CG_DRIVER_DMA_MULTI_BLOCK_MODE
				#if CG_DRIVER_DMA_MULTI_BLOCK_MODE == CG_DRIVER_DMA_MULTI_BLOCK_MODE_PENDING
					rc = CgCpuDmaSetupFromGpsNextTask(gChunksList, pState->transfer.chunks.required, pState->transfer.chunks.active, CG_DRIVER_DMA_CHANNEL_READ, CGCORE_BASE_ADDR + 0x00000070);
				#endif
			#endif
		}
		else
		{
			DBGMSG("problem!\n");
			//			if (pState->transfer.cancel.request)
			//				DBGMSG1("transfer.cancel.request=%d!", pState->transfer.cancel.request);
			//			if (pState->transfer.done)
			//				DBGMSG1("transfer.done=%d!", pState->transfer.done );
			//			if (pState->transfer.bytes.required == 0)
			// 				DBGMSG1("transfer.bytes.required=%d!", pState->transfer.bytes.required);
		}
	}

	if (intcode & CGCORE_INT_SNAP_END)
	{
		if (pState->transfer.cancel.request)
		{
			DBGMSG("Snap end due to : cancel\n");
			rc = CgCpuDmaStop(CG_DRIVER_DMA_CHANNEL_READ);
			rc = CgxDriverDataReadyInterruptHandler(pDriver, pState);
		}
		else
		{
			DBGMSG("Snap end due to : normal\n");
		}
	}

	if (intcode & CGCORE_INT_OVERRUN) {
		DBGMSG("Snap overrun\n");
		pState->flags.overrun = TRUE;
		pState->counters.interrupt.overrun++;
		if (pState->flags.wait) // release a waiting thread, if any
			CgxDriverTransferEndSignal(pDriver);
		// mark interrupt as handled
		intcode &= ~CGCORE_INT_OVERRUN;
	}

	DBGMSG("Done");

	return rc;
}


