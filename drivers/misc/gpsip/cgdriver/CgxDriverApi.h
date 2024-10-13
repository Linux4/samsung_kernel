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
	\brief CGsnap driver API
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide
	This file defines the application interface to interact with the CGsnap device driver.

*/

#ifndef _CGX_DRIVER_API_H__
#define _CGX_DRIVER_API_H__

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "CgTypes.h"
#include "CgReturnCodes.h"
#include "CgCpu.h"



/** IOCTL codes:*/
#define CGX_IOCTL_INIT				0x0001	/**< Initialize driver */
#define CGX_IOCTL_STOP				0x0002	/**< Driver stop and de-initialize */
#define CGX_IOCTL_WRITE				0x0003  /**< ACLYS only: Write data */
#define CGX_IOCTL_PREPARE_RECEIVE	0x0004  /**< Prepare for massive data (snap) transfer */
#define CGX_IOCTL_RESET				0x0005  /**< Reset device */
#define CGX_IOCTL_POWER_UP			0x0006  /**< Power up device */
#define CGX_IOCTL_STATUS			0x0007  /**< Get driver status */
#define CGX_IOCTL_GPIO_SET			0x0008  /**< Set Host GPIO */
#define CGX_IOCTL_GPIO_GET			0x0009  /**< Get Host GPIO */
#define CGX_IOCTL_GPIO_MODE_SET		0x000A  /**< Set Host GPIO mode */

#define CGX_IOCTL_GET_VERSION		0x001A  /**< Get driver version */
#define CGX_IOCTL_CLEAR_OVERRUN		0x001B  /**< ACLYS only */
#define CGX_IOCTL_WAIT_RCV			0x001C  /**< Wait for massive transfer to finish */
#define CGX_IOCTL_CANCEL_RECEIVE	0x001D  /**< Cancel massive transfer */
#define CGX_IOCTL_ALLOC_BUFF		0x001E  /**< Allocate kernel work memory buffer */
#define CGX_IOCTL_FREE_BUFF 		0x001F  /**< Free kernel work memory buffer */
#define CGX_IOCTL_READ_REG			0x0020	/**< CGsnap only: Read device register */
#define CGX_IOCTL_WRITE_REG			0x0021	/**< CGsnap only: Write device register */
#define CGX_IOCTL_POWER_DOWN		0x0022  /**< Power down device */
#define CGX_IOCTL_COUNTER_RESET		0x0023  /**< Reset counters */
#define CGX_IOCTL_READ_MEM			0x0024  /**< Read memory address */
#define CGX_IOCTL_RECORD_START		0x0015  /**< Setup and start raw data recording */
#define CGX_IOCTL_RECORD_WAIT		0x0016  /**< Wait for recording block */
#define CGX_IOCTL_RECORD_STOP		0x0017  /**< Stop raw data recording */
#define CGX_IOCTL_READ_RTC			0x0028
#define CGX_IOCTL_CPU_REVISION		0x0029	/**< Detect CPU revision */
#define CGX_IOCTL_RESUME_CLEAR		0x002A	/**< clear resume mode  */

#define CGX_IOCTL_TCXO_ENABLE		0x0030	/**< TCXO Enable/Disable control */

#define CGX_IOCTL_POWER_OFF			0x0040	/**< Cut power completely from the device */
#define CGX_IOCTL_POWER_ON			0x0041	/**< Restore power fully to the device */

#define CGX_IOCTL_RF_WRITE_CONTROL	0x0050	/**< Write to RF control channel */

#define CGX_IOCTL_DMA_TEST			0x0100	/**< DMA test */





/** Driver transfer control structure */

typedef struct {
    U32 physAddr;								/**< Physical address */
    U8 *virtAddr;								/**< Virtual address, in the application process' address space  */
	U32 length;									/**< Length, in bytes */
	U8 * driverContextVirtBufferAddr;			/**< virtual address, of buffer used between the DMA and the driver */
												/**< in the driver process' context */
} TCgxDriverPhysicalMemory;

#ifndef MAX_NUM_OF_BLOCKS_IN_SINGLE_DMA_XFER
	/**
		Define the maximum number of blocks in a single DMA transfer
	**/
	#define MAX_NUM_OF_BLOCKS_IN_SINGLE_DMA_XFER 64	// For now, same as MAX_DMA_TRANSFER_TASKS
#endif

/** Driver state structure */
typedef struct {
	struct {
		BOOL terminate;						/**< Termination flag (for IST) */
		BOOL wait;							/**< Wait for transfer complete flag */
		BOOL overrun;						/**< Overrun indication */
		BOOL resume;						/**< Resume indication */
		} flags;							/**< Flags */
	struct {
		struct {
			U32 all;						/**< Total interrupts counter */
			U32 snapStart;					/**< Snap start interrupts counter */
			U32 overrun;					/**< Overrun interrupts counter */
			} interrupt;					/**< Interrupts counters*/
		} counters;							/**< Counters */
	struct {
		U32 bufferPhys;						/**< physical memory address of receive buffer */
		U8 *originalBuffer;					/**< pointer to original received data buffer (as passed by caller) */
		TCgByteOrder byteOrder;				/**< required byte order for active transfer*/
		BOOL done;							/**< active transfer done flag*/
		struct {
			U32 required;					/**< total number of bytes required for active transfer*/
			U32 received;					/**< number of bytes received in active transfer*/
			} bytes;
		struct {
			U32 required;					/**< total number of blocks to receive */
			U32 received;					/**< number of blocks already received */
			} blocks;
		struct {
			U32 required;					/**< total number of DMA tasks */
			U32 received;					/**< number of DMA tasks processed */
			U32 active;						/**< active task index */
			} chunks;
		struct {
			BOOL request;					/**< Abort transfer flag */
			U32 onByte;						/**< if cancel is requested, indicate the cancel position*/
			U32 onChunk;
			} cancel;
		U32 bufferOffset[MAX_NUM_OF_BLOCKS_IN_SINGLE_DMA_XFER];	/**< Internal buffer addresses (for DMA) */
		U32 blockSize;						/**< Current transfer block size */
		U32 lastBlockSize;					/**< Current transfer last block size (might be smaller or larger)*/
		} transfer;
    TCgxDriverPhysicalMemory	buffer;		/**< Snap buffer information*/
	TCgReturnCode				constructionRc;/**< What return code was returned from the driver construction*/
} TCgxDriverState;




/** Driver control structure, passed from application on IOCTL call*/
typedef union
{
	struct {
		U32 alignedLength;					/**< Number of bytes to write (aligned to hardware requirements) */
		U32 blockLength;					/**< Block length (zero for single block snap) */
		TCgByteOrder byteOrder;				/**< Required byte order */
		unsigned char *buf;					/**< pointer to memory allocated for the data */
		U32 bufPhys;						/**< CGsnap only: */
		U32 timeoutMS;						/**< Timeout in mili-seconds */
		} read;								/**< Read data */

	struct {
		U32 timeoutMS;						/**< Timeout in milli-seconds */
		U32 blockNumber;					/**< Block number to wait for */
		} wait;

	struct {
		unsigned char *pSource;				/**< Source address to write from */
		U32 length;							/**< Number of bytes to write */
		} write;							/**< ACLYS only: Write data to ACLYS */

	struct {
		U32 code;							/**< GPIO code (CellGuide conventions)*/
		U32 out;							/**< GPIO value for output (write) operation*/
		TCgCpuGpioMode	mode;				/**< GPIO mode to set */
		} GPIO;								/**< Manual Read/Write GPIO */

	struct {
		U32 offset;							/**< register offset from CGsnap base address */
		} readReg;							/**< CGsnap only: Read CGsnap register */

	struct {
		U32 offset;							/**< Register offset from CGsnap base address */
		U32 value;							/**< Value to write */
		} writeReg;							/**< CGsnap only: Write CGsnap register */

	struct {
		U32 onByteCount;					/**< Position to cancel the transfer
												0 = Transfer will be canceled as soon as possible
												n = Transfer will be canceled as soon as possible After n bytes have been received
											  */
		} cancel;							/**< Cancel transfer request */
	struct {
		U32 resetLevel;						/**< Reset level:
												0 = Normal-Reset
												1 = Semi-Reset
											  */
		} reset;							/**< Reset transfer request */

	struct {
		U32 length;							/**< Number of bytes to allocate */
		U32 processId;						/**< ID of process that requested the buffer allocation */
		} alloc;							/**< Allocate In-Kernel Transfer Buffer */

	struct {
		BOOL enable;						/**< enable power to TCXO */
	} tcxoControl;							/**< Control TCXO power */

} TCgDriverControl;




/** Driver results structure, returned from driver on IOCTL call finish*/
typedef struct
{
	TCgReturnCode rc;						/**< Last operation detailed return code*/
	struct {
		void *bufferPhysAddr;				/**< Pointer to buffer start */
		void *bufferAppVirtAddr;			/**< Pointer to buffer start virtual address, in application's process' context*/
		U32 size;							/**< Number of bytes*/
		} buffer;							/**< Retrieve incoming data buffer details */

	struct {
		U32 code;							/**< GPIO code (CellGuide conventions)*/
		U32 val;							/**< GPIO value for output (write) operation*/
		} GPIO;								/**< Result for GPIO Read request*/

	struct {
		char buildVersion[32];				/**< Build version ("1.8") */
		char buildNumber[32];				/**< Build number ("1747") */
		char buildDate[32];					/**< Build date ("10/04/2007")*/
		char buildTime[32];					/**< Build time ("03:49")*/
		char buildMode[32];					/**< Build mode ("RELEASE", "DEBUG", "TEST")*/
		char buildName[32];					/**< Driver platform name*/
		} version;							/**< Retrieve driver version info*/

	struct {
		U32 offset;							/**< Register offset from CGsnap base address */
		U32 value;							/**< REgister value */
		} readReg;							/**< CGsnap only: Result for CGsnap register read request*/

	struct {
		char revisionCode[32];				/**< CPU revision code  */
		} cpuRevision;						/**< Detect CPU revision */

	struct {
		U32 year;
		U32 month;
		U32 day;
		U32 hour;
		U32 minute;
		U32 second;
		U32 dayInWeek;
		U32 tick;
	} readRtc;

	TCgxDriverState state;					/**< Driver internal state information (for debug) */
} TCgDriverStatus;





#ifdef __cplusplus
} // extern "C"
#endif //__cplusplus


#endif // _CGXDRIVERAPI_H__
