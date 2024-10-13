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
	\brief CPU API
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide

	CellGuide's API to access low-level CPU resources

	The API is used by CellGuide drivers and applications that require access to low-level CPU resources
	like DMA, SPI.

	CPU depended code is divided to the following API groups:
	\li \ref cg_cpu_clk
	\li \ref cg_cpu_dma
	\li \ref cg_cpu_gpio
	\li \ref cg_cpu_spi
	\li \ref cg_cpu_interrupt
	\li \ref cg_cpu_rtc
	\li \ref cg_cpu_miscellanies
*/

#ifndef CG_CPU_H
#define CG_CPU_H

#ifdef	__cplusplus
	extern "C" {
#endif /*__cplusplus*/

// ===========================================================================

#include "CgTypes.h"
#include "CgReturnCodes.h"
#include "CgCpuOs.h"

// ===========================================================================

/** helper macro to calculate minimum of 2 numbers */
#ifndef MIN
	#define MIN(__a__, __b__) 					((__a__) < (__b__) ? (__a__) : (__b__))
#endif

/** helper macro to align memory */
#ifndef ALLIGN
	#define ALLIGN(__var__,__base__)    ((__var__)%(__base__)) ? __var__ = (__base__) * ((U32)((__var__) / (__base__)) + 1) : (__var__)
#endif

/** helper macro to find the ceiling of a number */
#ifndef DIV_CEIL
	#define DIV_CEIL(__counter__,__devider__)			( ((__counter__) + (__devider__) -1) / (__devider__) )
#endif

/** Undefined addresss means the address needs to be filled during porting **/
#define CG_UNDEFINED_ADDRESS		(0xFFFFFFFF)

/** helper macro to get the 'controller' part of a channel code */
#define CG_CONTROLLER(ch)			(ch & 0xFFFF0000)

/** helper macro to get the 'channel' part of a channel code */
#define CG_CHANNEL(ch)				(ch & 0x0000FFFF)

/** helper macro to check NULL pointer in argument */
#define ASSERT_NOT_NULL(p)			if (p == NULL) return ECgNullPointer;

/** helper macro to calculate register index from it's offset */
#define REGIDX(offset)			(offset/4)


/** Bit field definition
  This structure is used to define a sub-register bit field, with arbitrary size.
  */
typedef struct {
	U32 start;								/**< start bit (0 based)  0 == LSB */
	U32 length;								/**< length of bit field (1-32) */
	U32 defaultValue;						/**< default value */
} TCgBitField;


#define CG_CPU_DEVICE_SPI			(1)

#define CG_CPU_SPI_MODE_DMA			(0)		/**< Use DMA to handle SPI communication */
#define CG_CPU_SPI_MODE_MANUAL		(1)		/**< Use polling to handle SPI communication */

/** \name  Controllers logical codes
\{
*/


/**
DMA Controller logical unit code
\note Do not change; this value is used internally in the driver
\ingroup cg_cpu_dma
*/
#define CG_CPU_CONTROLLER_DMA						(0x00020000)

/**
SPI Controller logical unit code
\note Do not change; this value is used internally in the driver
\ingroup cg_cpu_spi
*/
#define CG_CPU_CONTROLLER_SPI						(0x00030000)

/**
GPIO Controller logical unit code
\note Do not change; this value is used internally in the driver
\ingroup cg_cpu_gpio
*/
#define CG_CPU_CONTROLLER_GPIO						(0x00040000)



/** \} Controllers logical codes*/

/** Virtual GPIO groups definitions
	These values are used to define CellGuide 'virtual' GPIO codes, later converted to specific
	CPU GPIO register and pin.
 */


/** Maximum number of GPIOs in a group.
	the number is artificial, and should be the largest GPIO group there is.
*/
#define CG_CPU_GPIO_GROUP_SIZE		(128)

#define CG_CPU_GPIO_GROUP_INDEX_A  (1) /**< GPIO group A index */
#define CG_CPU_GPIO_GROUP_INDEX_B  (CG_CPU_GPIO_GROUP_INDEX_A+1) /**< GPIO group B index */
#define CG_CPU_GPIO_GROUP_INDEX_C  (CG_CPU_GPIO_GROUP_INDEX_B+1) /**< GPIO group C index */
#define CG_CPU_GPIO_GROUP_INDEX_D  (CG_CPU_GPIO_GROUP_INDEX_C+1) /**< GPIO group A index */
#define CG_CPU_GPIO_GROUP_INDEX_E  (CG_CPU_GPIO_GROUP_INDEX_D+1) /**< GPIO group A index */
#define CG_CPU_GPIO_GROUP_INDEX_F  (CG_CPU_GPIO_GROUP_INDEX_E+1) /**< GPIO group A index */
#define CG_CPU_GPIO_GROUP_INDEX_G  (CG_CPU_GPIO_GROUP_INDEX_F+1) /**< GPIO group G index */
#define CG_CPU_GPIO_GROUP_INDEX_H  (CG_CPU_GPIO_GROUP_INDEX_G+1) /**< GPIO group H index */
#define CG_CPU_GPIO_GROUP_INDEX_I  (CG_CPU_GPIO_GROUP_INDEX_H+1) /**< GPIO group I index */
#define CG_CPU_GPIO_GROUP_INDEX_J  (CG_CPU_GPIO_GROUP_INDEX_I+1) /**< GPIO group J index */
#define CG_CPU_GPIO_GROUP_INDEX_K  (CG_CPU_GPIO_GROUP_INDEX_J+1) /**< GPIO group K index */
#define CG_CPU_GPIO_GROUP_INDEX_L  (CG_CPU_GPIO_GROUP_INDEX_K+1) /**< GPIO group L index */
#define CG_CPU_GPIO_GROUP_INDEX_M  (CG_CPU_GPIO_GROUP_INDEX_L+1) /**< GPIO group M index */
#define CG_CPU_GPIO_GROUP_INDEX_N  (CG_CPU_GPIO_GROUP_INDEX_M+1) /**< GPIO group N index */
#define CG_CPU_GPIO_GROUP_INDEX_O  (CG_CPU_GPIO_GROUP_INDEX_N+1) /**< GPIO group O index */
#define CG_CPU_GPIO_GROUP_INDEX_P  (CG_CPU_GPIO_GROUP_INDEX_O+1) /**< GPIO group P index */
#define CG_CPU_GPIO_GROUP_INDEX_Q  (CG_CPU_GPIO_GROUP_INDEX_P+1) /**< GPIO group Q index */
#define CG_CPU_GPIO_GROUP_INDEX_R  (CG_CPU_GPIO_GROUP_INDEX_Q+1) /**< GPIO group R index */
#define CG_CPU_GPIO_GROUP_INDEX_S  (CG_CPU_GPIO_GROUP_INDEX_R+1) /**< GPIO group S index */
#define CG_CPU_GPIO_GROUP_INDEX_T  (CG_CPU_GPIO_GROUP_INDEX_S+1) /**< GPIO group T index */
#define CG_CPU_GPIO_GROUP_INDEX_U  (CG_CPU_GPIO_GROUP_INDEX_T+1) /**< GPIO group U index */
#define CG_CPU_GPIO_GROUP_INDEX_V  (CG_CPU_GPIO_GROUP_INDEX_U+1) /**< GPIO group V index */
#define CG_CPU_GPIO_GROUP_INDEX_W  (CG_CPU_GPIO_GROUP_INDEX_V+1) /**< GPIO group W index */
#define CG_CPU_GPIO_GROUP_INDEX_X  (CG_CPU_GPIO_GROUP_INDEX_W+1) /**< GPIO group X index */
#define CG_CPU_GPIO_GROUP_INDEX_Y  (CG_CPU_GPIO_GROUP_INDEX_X+1) /**< GPIO group Y index */
#define CG_CPU_GPIO_GROUP_INDEX_Z  (CG_CPU_GPIO_GROUP_INDEX_Y+1) /**< GPIO group Z index */
#define CG_CPU_GPIO_GROUP_INDEX___  (CG_CPU_GPIO_GROUP_INDEX_Z+1)


#define CG_CPU_GPIO_GROUP_A	 (CG_CPU_GPIO_GROUP_INDEX_A * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group A */
#define CG_CPU_GPIO_GROUP_B	 (CG_CPU_GPIO_GROUP_INDEX_B * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group B */
#define CG_CPU_GPIO_GROUP_C	 (CG_CPU_GPIO_GROUP_INDEX_C * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group C */
#define CG_CPU_GPIO_GROUP_D	 (CG_CPU_GPIO_GROUP_INDEX_D * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group D */
#define CG_CPU_GPIO_GROUP_E	 (CG_CPU_GPIO_GROUP_INDEX_E * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group E */
#define CG_CPU_GPIO_GROUP_F	 (CG_CPU_GPIO_GROUP_INDEX_F * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group F */
#define CG_CPU_GPIO_GROUP_G	 (CG_CPU_GPIO_GROUP_INDEX_G * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group G */
#define CG_CPU_GPIO_GROUP_H	 (CG_CPU_GPIO_GROUP_INDEX_H * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group H */
#define CG_CPU_GPIO_GROUP_I	 (CG_CPU_GPIO_GROUP_INDEX_I * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group I */
#define CG_CPU_GPIO_GROUP_J	 (CG_CPU_GPIO_GROUP_INDEX_J * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group J */
#define CG_CPU_GPIO_GROUP_K	 (CG_CPU_GPIO_GROUP_INDEX_K * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group K */
#define CG_CPU_GPIO_GROUP_L	 (CG_CPU_GPIO_GROUP_INDEX_L * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group L */
#define CG_CPU_GPIO_GROUP_M	 (CG_CPU_GPIO_GROUP_INDEX_M * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group M */
#define CG_CPU_GPIO_GROUP_N	 (CG_CPU_GPIO_GROUP_INDEX_N * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group N */
#define CG_CPU_GPIO_GROUP_O	 (CG_CPU_GPIO_GROUP_INDEX_O * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group O */
#define CG_CPU_GPIO_GROUP_P	 (CG_CPU_GPIO_GROUP_INDEX_P * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group P */
#define CG_CPU_GPIO_GROUP_Q	 (CG_CPU_GPIO_GROUP_INDEX_Q * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group Q */
#define CG_CPU_GPIO_GROUP_R	 (CG_CPU_GPIO_GROUP_INDEX_R * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group R */
#define CG_CPU_GPIO_GROUP_S	 (CG_CPU_GPIO_GROUP_INDEX_S * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group S */
#define CG_CPU_GPIO_GROUP_T	 (CG_CPU_GPIO_GROUP_INDEX_T * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group T */
#define CG_CPU_GPIO_GROUP_U	 (CG_CPU_GPIO_GROUP_INDEX_U * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group U */
#define CG_CPU_GPIO_GROUP_V	 (CG_CPU_GPIO_GROUP_INDEX_V * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group V */
#define CG_CPU_GPIO_GROUP_W	 (CG_CPU_GPIO_GROUP_INDEX_W * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group W */
#define CG_CPU_GPIO_GROUP_X	 (CG_CPU_GPIO_GROUP_INDEX_X * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group X */
#define CG_CPU_GPIO_GROUP_Y	 (CG_CPU_GPIO_GROUP_INDEX_Y * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group Y */
#define CG_CPU_GPIO_GROUP_Z	 (CG_CPU_GPIO_GROUP_INDEX_Z * CG_CPU_GPIO_GROUP_SIZE)	/**< 1st GPIO code in group Z */



#define REG32(r) *((volatile U32*)(r))

#define REG_GET1(aReg, aPin) \
	((aReg >> aPin) & 0x0001)

#define REG_GET2(aReg, aPin) \
	((aReg >> (aPin/2)) & 0x0003)

#define REG_GET4(aReg, aPin) \
	((aReg >> (aPin/4)) & 0x000F)

#define REG_GET8(aReg, aPin) \
	((aReg >> (aPin/8)) & 0x00FF)


#define REG_SET(aReg, aOffset, aMask, aVal)  {\
	U32 regval = ((aVal) << ((aOffset)));	\
	U32 mask = ~((U32)aMask << ((aOffset)));	\
	if ((aReg & ~mask) != regval) {\
 		aReg &= (mask); \
		aReg |= regval; \
		}\
	}

#define REG_SET1(aReg, aPin, aVal)  {\
	U32 regval = ((aVal) << ((aPin)));	\
	U32 mask = ~((U32)1 << ((aPin)));	\
 	if(aVal == 0) aReg &= (mask); \
	else aReg |= regval; \
	}


#define REG_SET2(aReg, aPin, aVal)  {\
	U32 regval = ((aVal) << ((aPin)*2));	\
	U32 mask = ~((U32)3 << ((aPin)*2));	\
	if ((aReg & ~mask) != regval) {\
 		aReg &= (mask); \
		aReg |= regval; \
		}\
	}

#define REG_SET4(aReg, aPin, aVal)  {\
	U32 regval = ((aVal) << ((aPin)*4));	\
	U32 mask = ~((U32)0xF << ((aPin)*4));	\
	if ((aReg & ~mask) != regval) {\
 		aReg &= (mask); \
		aReg |= regval; \
		}\
	}


#define REG_SET8(aReg, aPin, aVal)  {\
	U32 regval = ((aVal) << ((aPin)*8));	\
	U32 mask = ~((U32)0xFF << ((aPin)*8));	\
	if ((aReg & ~mask) != regval) {\
		aReg &= (mask); \
		aReg |= regval; \
		}\
}


/*********************************************************** CLOCK *************************/
/** \defgroup cg_cpu_clk Clock control
	Define the API to access the CPU clock configuration. This API is used to enable/disable generated clocks
	to required hardware units like SPI controller, UART controller etc.
*/




/**
    \ingroup cg_cpu_clk
    Create access to 'clock' registers.

	\if OS_WCE
		in windows CE, usually use VirtualAlloc/VirtualCopy to generate an access to the DMA registers.
	\endif

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuClockCreate(void);


/**
    \ingroup cg_cpu_clk
    Destroy access to 'clock' registers

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuClockDestroy(void);


/**
    \ingroup cg_cpu_clk
    Enable the clock to specific CPU unit
	Logical unit codes are defined per CPU. each unit may have sub-units.

	\attention Requires porting

	\param[in]	aUnit		Logical unit code to enable

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuClockEnable(U32 aUnit);


/**
    \ingroup cg_cpu_clk
    Disable the clock to specific CPU unit.
	Logical unit codes are defined per CPU. Each unit may have sub-units.

	\attention Requires porting

	\param[in]	aUnit		Logical unit code to disable

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuClockDisable(U32 aUnit);


/*********************************************************** DMA ***************************/

/**
	\section DMA_CH_CODE DMA channel code
	code = DMA Controller code + channel number

	example: CG_CPU_DMA_CONTROLLER_0 + 4 ==> DMA controller 1, channel 4
*/
#define CG_CPU_DMA_CONTROLLER_0			(0x00000000)
#define CG_CPU_DMA_CONTROLLER_1			(0x00010000)
#define CG_CPU_DMA_CONTROLLER_2			(0x00020000)
#define CG_CPU_DMA_CONTROLLER_3			(0x00030000)
#define CG_CPU_DMA_CONTROLLER_4			(0x00040000)

#define CG_DMA_CONTROLLER(ch)			((ch & 0xFFFF0000) >> 16)
#define CG_DMA_CHANNEL(ch)				(ch & 0x0000FFFF)
#define CG_DMA_IS_VALID(ch)
/** DMA multi block mode
	Defines the method of executing consecutive DMA transfers.
	There are 3 different possible methods :
	LLI - means that the DMA needs to be configured only once, and no extra tx commands should be issued
	PENDING - means that after receiving the first GPS data interrupt, the DMA should issue the next task, so it is prepared for it.
	NONE - means that the DMA should prepared for the next task, only after receiving the DMA interrupt for tx-done.
*/

#define CG_DRIVER_DMA_MULTI_BLOCK_MODE_NONE		0
#define CG_DRIVER_DMA_MULTI_BLOCK_MODE_PENDING	1
#define CG_DRIVER_DMA_MULTI_BLOCK_MODE_LLI		2

typedef struct {
    U32 address;
	U32 length;
} TCgCpuDmaTask;


/** \defgroup cg_cpu_dma DMA control
	Define the API to access Direct Memory Access unit for specific CPU.
*/

/**
    \ingroup cg_cpu_dma
    Create access to DMA controller.
	Depending on OS, CPU and platform implementation, this might require remapping physical
	address space into virtual address space. However, on some systems the virtual address is
	statically allocated, so this function should be left empty (for example, for Linux driver).

	In the CellGuide provided implementation, if \ref CG_DRIVER_DMA_BASE_VA is defined in platform.h, then
	a static virtual address should be used, otherwise, the physical address (CG_DRIVER_DMA_BASE_PA)
	is mapped to a virtual address.

	The function assigns gpDMA with the DMA controller virtual address.

	\if OS_WCE
		\note in windows CE, usually use VirtualAlloc/VirtualCopy to generate an access to the DMA registers.
		if a consist storage is needed, it should be global.
	\endif

	\attention DMA channel number is a 'logical' number, a combination of the controller and channel, and
		should be parsed before used directly with hardware registers
	\note On some implementations, the CGsnap read port address is required for proper DMA setup,
		and therefore it is passed as an argument. If it is not required, then it should be ignored,
		but not removed from the function prototype.

	\param[in]  dmaChannel : The DMA channel to create
	\param[in] ipDataSourceAddress : The address of the IP, from which the DMA should read data

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuDmaCreate(U32 dmaChannel, U32 ipDataSourceAddress);


/**
    \ingroup cg_cpu_dma
    Destroy DMA access

    \param[in]	aDmaChannel			Logical channel number
    \param[in]	aIpDataSourceAddress			Address

    \return System wide return code (see CgReturnCodes.h)
	\retval ECgOk			On success
*/
TCgReturnCode CgCpuDmaDestroy(U32 aDmaChannel, U32 aIpDataSourceAddress);


/**
    \ingroup cg_cpu_dma
    Check if the DMA channel is ready for next operation.

    \param[in]	aDmaChannel			Logical channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
	\retval ECgBadArgument	On invalid channel
	\retval ECgBusy			if not ready
*/
TCgReturnCode CgCpuDmaIsReady(U32 aDmaChannel);



/**
    \ingroup cg_cpu_dma
    Get the DMA channel current bytes count

    \param[in]	aDmaChannel			Channel number
    \param[out]	apCount				Bytes count

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
	\retval ECgBadArgument	On invalid channel
	\retval ECgBusy			When channel is not ready
*/
TCgReturnCode CgCpuDmaCurCount(U32 aDmaChannel, U32 *apCount);

/**
    \ingroup cg_cpu_dma
    Get the DMA channel requested bytes count

    \param[in]	aDmaChannel			Logical channel number
    \param[out]	apCount				Bytes count

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
	\retval ECgBadArgument	On invalid channel
	\retval ECgBusy			When channel is not ready
*/
TCgReturnCode CgCpuDmaRequestedCount(U32 aDmaChannel, U32 *apCount);

/**
    \ingroup cg_cpu_dma
    Wait for the DMA channel to finish its current transfer

    \param[in]	aDmaChannel			Logical channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
	\retval ECgBadArgument	On invalid channel
	\retval ECgBusy			When channel is not ready
*/TCgReturnCode CgCpuDmaWaitFinish(U32 aDmaChannel);


/**
    \ingroup cg_cpu_dma
    Stop DMA channel

    \param[in]	aDmaChannel			Logical channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
	\retval ECgBadArgument	On invalid channel
*/
TCgReturnCode CgCpuDmaStop(U32 aDmaChannel);


/**
    \ingroup cg_cpu_dma
    Start the specified DMA channel.
	The channel initialization (setting source, destination, etc) is done in another function.

    \param[in]	aDmaChannelCode		DMA channel code.
									\ref DMA_CH_CODE

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
	\retval ECgBadArgument	On invalid channel
*/
TCgReturnCode CgCpuDmaStart(U32 aDmaChannelCode);


/**
    \ingroup cg_cpu_dma
    Configure DMA to write data to SPI

    \param pSource			Source memory address to write from
    \param aLength			Number of bytes to write
    \param aWriteDmaChannel DMA channel to use
    \param aDeviceChannel	SPI channel to use

	\note The function checks that the DMA channel selected is the right one for the SPI channel.

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuDmaSetupToDevice(U32 pSource, U32 aLength, U32 aWriteDmaChannel, int aDeviceChannel);




/**
    \ingroup cg_cpu_dma
    Configure DMA to read data from SPI

    \param pTarget			Source memory address to write to
    \param aLength			Number of bytes to transfer
    \param aBlockLength		Number of bytes in a block
    \param aReadDmaChannel	DMA channel to use for read
    \param aWriteDmaChannel DMA channel to use for write
    \param aDeviceChannel	SPI channel to use

	\note if SPI has 'auto output' generator, there is no need to use the 'write' channel.

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuDmaSetupFromDevice(U32 pTarget, U32 aLength, U32 aBlockLength, U32 aReadDmaChannel, U32 aWriteDmaChannel, int aDeviceChannel);



/**
    \ingroup cg_cpu_dma
    Configure DMA to read data from CGsnap

    \param apDmaTask		DMA tasks list
    \param aDmaTaskCount	Total number of DMA tasks
    \param aDmaChannelCode	DMA channel to use for read (\ref DMA_CH_CODE)
    \param aDeviceCode		Device code

	\note if SPI has 'auto output' generator, there is no need to use the 'write' channel.

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuDmaSetupFromDeviceFirstTask(TCgCpuDmaTask *apDmaTask, U32 aDmaTaskCount, U32 aDmaChannelCode, U32 aDeviceCode);


/**
    \ingroup cg_cpu_dma
    Configure DMA to read data from SPI

    \param apDmaTask		DMA tasks list
    \param aDmaTaskCount	Total number of DMA tasks
    \param aActiveTaskIndex	Active DMA task number
    \param aDmaChannel		DMA channel to use for read
    \param aDeviceCode		Device code

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuDmaSetupFromDeviceNextTask(TCgCpuDmaTask *apDmaTask, U32 aDmaTaskCount, U32 aActiveTaskIndex, U32 aDmaChannel, U32 aDeviceCode);

/**
    \ingroup cg_cpu_dma
    Configure DMA to read data from GPS device.
	This handles the initial transfer setup, and is called by the application prior to CgCpuDmaStart.
    Configure DMA to read data from CGsnap.
	- DMA source
		- Device is CGsnap IP
		- Address is 'aDeviceAddress'
		- Non increment
		- Use handshake
	- Burst transfer (burst = 16 bytes)
	- Word (32bit) transfer mode
	- Enable interrupt at end of transfer

	If the FMA supports LLI (Linked List Interface), then a proper DMA tasks list should be generated from the generic
	tasks list.

    \param apDmaTask		DMA tasks list
	Since the destination buffer maybe smaller than the transfer size, it is required to split the DMA
	transfer into several DMA tasks. The destination of each task will take into consideration the buffer size.
	The application sets the buffer size on init.
	The application decides on transfer size, transfer task size and task destination address, for every transfer.

	For example, if the transfer size is 1500KB, and the buffer size is 500KB, and the task size is 100KB,
	then there will be 15 transfer tasks, each of 100KB, and the destination addresses in each task will be 0, 100KB, 200KB,
	300KB, 400KB, 0KB, 100KB, etc.

	In case the DMAC supports LLI, all required tasks shall be set in the DMAC.
	Otherwise, the tasks are stored temporarily, and will be used by CgCpuDmaSetupFromGpsNextTask.

    \param apDmaTask		DMA tasks list (pairs of addresses and lengths)
    \param aDmaTaskCount	Total number of DMA tasks
    \param aDmaChannel		DMA channel to use for read (\ref DMA_CH_CODE)
    \param aDeviceAddress	Device address

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuDmaSetupFromGps(TCgCpuDmaTask *apDmaTask, U32 aDmaTaskCount, U32 aDmaChannel, U32 aDeviceAddress);




/**
    \ingroup cg_cpu_dma
    Configure DMA to read next DMA task from GPS device.
	This function is only required when the DMA controller does not support LLI
	This function is only required when the DMA controller does not support LLI
	This sets up the DMA to perform the 2nd and above transfer tasks, of the same transfer.

	In case the DMAC doesn't support LLI, this function is called by the the DMA interrupt handler, to set-up the
	next transfer task.
	Otherwise, this function is never called.


    \param apDmaTask		DMA tasks list (the same
    \param aDmaTaskCount	Total number of DMA tasks
    \param aActiveTaskIndex	Active DMA task number
    \param aDmaChannel		DMA channel to use for read
    \param aDeviceAddress	Device address

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuDmaSetupFromGpsNextTask(TCgCpuDmaTask *apDmaTask, U32 aDmaTaskCount, U32 aActiveTaskIndex, U32 aDmaChannel, U32 aDeviceAddress);

/*********************************************************** GPIO **************************/
/** \defgroup cg_cpu_gpio GPIO control
	Configure, set and read GPIO


	CellGuide GPIO code is an abstract number, combination of group constant and pin number.
	The number is translated to specific CPU values by specific implementation.
*/


/** GPIO pin configuration modes */
typedef enum {
	ECG_CPU_GPIO_INPUT,				/**< GPIO to act as input */
	ECG_CPU_GPIO_OUTPUT,			/**< GPIO to act as output */
	ECG_CPU_GPIO_SPECIAL,			/**< GPIO to act as special function */
	ECG_CPU_GPIO_FALLING_INT,		/**< GPIO to act as falling edge interrupt*/
	ECG_CPU_GPIO_SPECIAL2,			/**< GPIO to act as special function */
	ECG_CPU_GPIO_SPECIAL3,			/**< GPIO to act as special function */
	ECG_CPU_GPIO_MODE___
} TCgCpuGpioMode;

/** GPIO pin configuration modes */
typedef enum {
	ECG_CPU_GPIO_PULL_NONE,			/**< GPIO with no pull */
	ECG_CPU_GPIO_PULL_DOWN,			/**< GPIO with pull down */
	ECG_CPU_GPIO_PULL_UP,			/**< GPIO with pull up */
	ECG_CPU_GPIO_PULL___
} TCgCpuGpioPull;

/**
    \ingroup cg_cpu_gpio
    Create access to GPIO controller.
	Depending on OS, CPU and platform implementation, this might require remapping physical
	address space into virtual address space. However, on some systems the virtual address is
	statically allocated, so this function should be left empty (for example, for Linux driver).

	In the 'standard' implementation, if CG_DRIVER_GPIO_BASE_VA is defined in platform.h, then
	a static virtual address should be used, otherwise, the physical address (CG_DRIVER_GPIO_BASE_PA)
	is mapped to a virtual address.

	The function assigns gpGpio with the GPIO controller virtual address.

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuGpioCreate(void);


/**
    \ingroup cg_cpu_gpio
    Destroy access to GPIO controller.
	The implementation depends on the method used for allocation (see CgCpuGpioCreate)

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuGpioDestroy(void);


/**
    \ingroup cg_cpu_gpio
    Set a specific GPIO to 'output' mode and set it to '1' value

    \param[in]	aPinCode			GPIO pin code

	\note Pin code is CellGuide internal code for each GPIO, which is a combination of the
	GPIO group offset and the number inside the group.

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuGpioSet(U32 aPinCode);


/**
    \ingroup cg_cpu_gpio
    Set a specific GPIO to 'output' mode and set it to '0' value

	\note Pin code is CellGuide internal code for each GPIO, which is a combination of the
	GPIO group offset and the number inside the group.

	\param[in]	aPinCode			GPIO pin code

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuGpioReset(U32 aPinCode);


/**
    \ingroup cg_cpu_gpio
    Set a specific GPIO to 'input' mode, read the value and return it

    \note Pin code is CellGuide internal code for each GPIO, which is a combination of the
	GPIO group offset and the number inside the group.

    \param[in]	aPinCode			GPIO pin code
    \param[out] aPinValue			current value

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuGpioGet(U32 aPinCode, int *aPinValue);


/**
    \ingroup cg_cpu_gpio
    Return IRQ code for the specific GPIO pin

    \note Pin code is CellGuide internal code for each GPIO, which is a combination of the
	GPIO group offset and the number inside the group.

    \param[in]	aPin				GPIO pin code
    \param[out]	aCode				IRQ code

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuGpioIntCode(U32 aPin, U32 *aCode);


/**
    \ingroup cg_cpu_gpio
    Set a specific GPIO mode

    \note Pin code is CellGuide internal code for each GPIO, which is a combination of the
	GPIO group offset and the number inside the group.

    \param[in]	aPinCode			GPIO pin code
    \param[in]	aPinMode			Requested mode

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuGpioModeSet(U32 aPinCode, TCgCpuGpioMode aPinMode);

/**
    \ingroup cg_cpu_gpio

    Set a specific GPIO strength

    \note Pin code is CellGuide internal code for each GPIO, which is a combination of the
	GPIO group offset and the number inside the group.

	\attention Not all CPUs support internal drive level. If the CPU does not support this feature, this function should be left empty.

    \param[in]	aPinCode			GPIO pin code
    \param[in]	aStrength			Requested strength

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuGpioStrength(U32 aPinCode, U32 aStrength);

/**
    \ingroup cg_cpu_gpio
    Set a specific GPIO pull up/down

    \note Pin code is CellGuide internal code for each GPIO, which is a combination of the
	GPIO group offset and the number inside the group.

	\attention Not all CPUs support internal pull, if the CPU does not support this feature, this function should be left empty.

    \param[in]	aPinCode			GPIO pin code
    \param[in]	aPull				Requested

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuGpioPull(U32 aPinCode, TCgCpuGpioPull aPull);

/**
    \ingroup cg_cpu_gpio
    Set a specific GPIO direction

    \note Pin code is CellGuide internal code for each GPIO, which is a combination of the
	GPIO group offset and the number inside the group.

    \param[in]	aPinCode			GPIO pin code
    \param[in]	aPinMode			Requested mode

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuGpioDirection(U32 aPinCode, TCgCpuGpioMode aPinMode);


/**
    \ingroup cg_cpu_gpio
    Clear interrupt bit

    \note Pin code is CellGuide internal code for each GPIO, which is a combination of the
	GPIO group offset and the number inside the group.

    \param[in]	aPinCode			GPIO pin code

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuGpioIntClear(U32 aPinCode);

/*********************************************************** SPI ***************************/
/** \defgroup cg_cpu_spi SPI control
	SPI programming
*/




/**
    \ingroup cg_cpu_spi
    Delay for specific time

    \param aCount

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuDelay(U32 aCount);


typedef enum {
	ECG_CPU_SPI_INPUT_DMA,
	ECG_CPU_SPI_INPUT_POLLING,
	ECG_CPU_SPI_OUTPUT_DMA,
	ECG_CPU_SPI_OUTPUT_POLLING,
	ECG_CPU_SPI_INIT,
	ECG_CPU_SPI_SLAVE_INPUT_DMA,
	ECG_CPU_SPI_SLEEP,
	ECG_CPU_SPI_MODE___
} TCgCpuSpiMode;



/**
    \ingroup cg_cpu_spi
    Create access to SPI

	\param[in]	aChannelCode		SPI Channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiCreate(U32 aChannelCode);


/**
    \ingroup cg_cpu_spi
    Destroy access to SPI

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiDestroy(void);


/**
    \ingroup cg_cpu_spi
    Enable SPI controller for specific channel

	\param[in]	aDeviceChannel		SPI Channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiEnable(int aDeviceChannel);

/**
    \ingroup cg_cpu_spi
    Disable SPI controller for specific channel

	\param[in]	aDeviceChannel		SPI Channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiDisable(int aDeviceChannel);

/**
    \ingroup cg_cpu_spi
    Check if SPI channel number is valid for the specific system

    \param[in]	aDeviceChannel		SPI Channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiIsValid(int aDeviceChannel);

/**
    \ingroup cg_cpu_spi
    Setup SPI channel

	\param[in]	aDeviceChannel		SPI channel number
	\param[in]	aPrescale			SPI clock divider
	\param[in]	aMode				SPI mode
	\param[in]	aLengthBytes		Number of bytes

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiConfig(U32 aDeviceChannel, U32 aPrescale, TCgCpuSpiMode aMode, U32 aLengthBytes);

/**
    \ingroup cg_cpu_spi
    Setup SPI channel

	\param[in]	aDeviceChannel		SPI channel number
	\param[in]	aLengthBytes		Number of bytes

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiSlaveConfig(U32 aDeviceChannel, U32 aLengthBytes);

/**
    \ingroup cg_cpu_spi
    Setup SPI channel GPIO

	\param[in]	aDeviceChannel			SPI channel number
	\param[in]	aPrescale			SPI clock divider
	\param[in]	aMode				SPI mode

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiSetup(U32 aDeviceChannel, U32 aPrescale, TCgCpuSpiMode aMode);

/**
    \ingroup cg_cpu_spi
    Return number of words waiting in receive FIFO

	\param[in]	aDeviceChannel		SPI channel number
	\param[out]	pCount				number of words (word width is according to config)

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiReadCount(U32 aDeviceChannel, U32 *pCount);


/**
    \ingroup cg_cpu_spi
    Loop until SPI write is finished.

	\param[in]	aDeviceChannel			SPI channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiTxFinishWait(U32 aDeviceChannel);

/**
    \ingroup cg_cpu_spi
    Rx FIFO is empty

	\param[in]	aDeviceChannel			SPI channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
BOOL CgCpuSpiRxFifoEmpty(U32 aDeviceChannel);


/**
    \ingroup cg_cpu_spi
    Clear SPI receive FIFO

	\param[in]	aDeviceChannel			SPI channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiClearFifo(U32 aDeviceChannel);

/**
    \ingroup cg_cpu_spi
    Loop until SPI read is finished.

	\param[in]	aDeviceChannel		SPI channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiRxFinishWait(U32 aDeviceChannel);

/**
    \ingroup cg_cpu_spi
    Read one byte from SPI channel

	\param[in]	aDeviceChannel		SPI channel number
	\param[out]	aByte				Data read from SPI

    \return Value read from SPI
*/
TCgReturnCode CgCpuSpiReadByte(U32 aDeviceChannel, volatile unsigned char *aByte);

/**
    \ingroup cg_cpu_spi
    Read one Word from SPI channel

	\param[in]	aDeviceChannel		SPI channel number
	\param[out]	aValue				Data read from SPI

    \return Value read from SPI
*/
TCgReturnCode CgCpuSpiReadU16(U32 aDeviceChannel, volatile U16 *aValue);

/**
    \ingroup cg_cpu_spi
    Read one DWord from SPI channel

	\param[in]	aDeviceChannel		SPI channel number
	\param[out]	aValue				Data read from SPI

    \return Value read from SPI
*/
TCgReturnCode CgCpuSpiReadU32(U32 aDeviceChannel, volatile U32 *aValue);

/**
    \ingroup cg_cpu_spi
    Read one byte from SPI channel (read 2nd FIFO byte)

	\param[in]	aDeviceChannel		SPI channel number
	\param[out]	aByte				Data read from SPI

    \return Value read from SPI
*/
TCgReturnCode CgCpuSpiReadByte2(U32 aDeviceChannel, volatile unsigned char *aByte);


/**
    \ingroup cg_cpu_spi
    Write one byte to SPI channel

	\param[in]	aDeviceChannel		SPI channel number
	\param[in]	aValue				Value to write

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiWriteByte(U32 aDeviceChannel, unsigned char aValue);

/**
    \ingroup cg_cpu_spi
    Write one Word to SPI channel

	\param[in]	aDeviceChannel		SPI channel number
	\param[in]	aValue				Value to write

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiWriteU16(U32 aDeviceChannel, U16 aValue);

/**
    \ingroup cg_cpu_spi
    Write one DWord to SPI channel

	\param[in]	aDeviceChannel		SPI channel number
	\param[in]	aValue				Value to write

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiWriteU32(U32 aDeviceChannel, U32 aValue);


/**
    \ingroup cg_cpu_spi
    Return the GPIO code for selected SPI channel MOSI

    \param[in]	aDeviceChannel		channel number
    \param[out] aGpioCode			gpio code

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiMosi(U32 aDeviceChannel, U32 *aGpioCode);


/**
    \ingroup cg_cpu_spi
    Return the GPIO number for selected SPI channel MISO

    \param[in]	aDeviceChannel		channel number
    \param[out] aGpio				gpio code

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiMiso(U32 aDeviceChannel, U32 *aGpio);


/**
    \ingroup cg_cpu_spi
    Return the GPIO number for selected SPI channel CLK

    \param[in]	aDeviceChannel		channel number
    \param[out] aGpio				gpio code

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiClk(U32 aDeviceChannel, U32 *aGpio);


/**
    \ingroup cg_cpu_spi
    Return the GPIO number for selected SPI channel CS (Chip Select)

    \param[in]	aDeviceChannel			channel number
    \param[out] aGpio				gpio code

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiCs(U32 aDeviceChannel, U32 *aGpio);


/**
    \ingroup cg_cpu_spi
    Start DMA transaction

	\param[in]	aDeviceChannel		SPI Channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiDmaStart(int aDeviceChannel);

/**
    \ingroup cg_cpu_spi
    Stop DMA transaction

	\param[in]	aDeviceChannel		SPI Channel number

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuSpiDmaStop(int aDeviceChannel);


/*********************************************************** INTR **************************/
/** \defgroup cg_cpu_interrupt Interrupt control
	Access to interrupt controller
*/

/**
    \ingroup cg_cpu_interrupt
    Create access to INTC controller.
	Depending on OS, CPU and platform implementation, this might require remapping physical
	address space into virtual address space. However, on some systems the virtual address is
	statically allocated, so this function should be left empty (for example, for Linux driver).

	In the 'standard' implementation, if CG_DRIVER_INT_BASE_VA is defined in platform.h, then
	a static virtual address should be used, otherwise,  the physical address (CG_DRIVER_INT_BASE_PA)
	is mapped to a virtual address.

	The function assigns pINT with the INT controller virtual address.

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuIntrCreate(void);

/**
    \ingroup cg_cpu_interrupt
    Destroy access to INTC

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuIntrDestroy(void);


/**
    \ingroup cg_cpu_interrupt
	Register IRQ interrupt as OS interrupt

    \param[in]			aIRQ				requested IRQ interrupt code
	\param[in]			apOSIntCode			requested software interrupt code
    \param[out]			apOSIntCode			registered OS interrupt code

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuIntrRequestOsInterrupt(U32 aIRQ, U32 *apOSIntCode);
/**
    \ingroup cg_cpu_interrupt
    Initialize interrupt

    \param[in]		aRequestCode	requested interrupt code (os specific)
    \param[in]		apEvent			event to tie with interrupt

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuIntrAttachEvent(U32 aRequestCode, void *apEvent);

/**
    \ingroup cg_cpu_interrupt
    Disable registered interrupt

    \param[in]	aInterruptCode	interrupt code (os specific)

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuIntrDisable(U32 aInterruptCode);


/**
    \ingroup cg_cpu_interrupt
    Signals to the kernel that interrupt processing has been completed

    \param[in]	aInterruptCode	interrupt code (os specific)

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/

TCgReturnCode CgCpuIntrDone(U32 aInterruptCode);


/** Interrupt Service Thread information structure */
typedef struct {
    void *hThread;					/**< Handle to thread object */
    U32 intCode;					/**< Interrupt code */
    void *event;					/**< Handle to Interrupt event */
    S32 priority;					/**< Thread priority */
} TCgCpuIST;


/**
    \ingroup cg_cpu_interrupt
	If apSharedISRInfo == NULL	===>		Create and start interrupt service thread
	Else						===>		Initialize an event, and connect it to the EXISTING ISR

    \param[in]	apIstInfo				Thread info structure
    \param[in]	aFunc					Main function for the thread
    \param[in]	aPriority				Requested thread priority
    \param[in]	aIntCode				Required interrupt code
    \param[in]	aIrqCode				Required IRQ code (may be 0)
    \param[in]	aParam					IST data structure pointer
	\param[in]	aEventName				Event name to use for interrupt/IST communication
										only need to supply name if the event is created by BSP.
										for systems where the event is internally tied to specific GPI interrupt,
										NULL can be set here
	\param[in] apSharedISRInfo			if the interrupt is shared, this will contain the info on
										the shared interrupt
	\param[in] apSharedISRLibrary		if the interrupt is shared, this will indicate what library
										holds the shared interrupt handler
	\param[in] apSharedISR				if the interrupt is shared, this will indicate the name of the
										interrupt service routine
	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuISTStart(TCgCpuIST *apIstInfo, void *aFunc, S32 aPriority, U32 aIntCode, U32 aIrqCode,
							void *aParam, const char *aEventName, void * apSharedISRInfo,
							const void * apSharedISRLibrary, const void * apSharedISR);


/**
    \ingroup cg_cpu_interrupt
    Stop interrupt service thread

    \param[in]	apIstInfo	Thread info structure

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuISTStop(TCgCpuIST *apIstInfo);



/*********************************************************** RTC ***************************/
/** \defgroup cg_cpu_rtc RTC control
	Access to RTC
*/

/**
    \ingroup cg_cpu_rtc
    Create access to RTC controller by mapping it to virtual memory.

	\note On some operating systems (Linux) and platforms, this step is static, i.e.,
	the virtual address is pre defined.
	CellGuide standard implementation of this function uses CG_DRIVER_RTC_BASE_VA
	and CG_DRIVER_RTC_BASE_PA defines to select the proper method.

	\if OS_WCE
		in windows CE, usually use VirtualAlloc/VirtualCopy to generate an access to the DMA registers.
		if a consist storage is needed, it should be global.
	\endif

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuRtcCreate(void);



/**
    \ingroup cg_cpu_rtc
    Destroy access to RTC controller by un-mapping virtual memory.

	\note CellGuide standard implementation of this function uses CG_DRIVER_RTC_BASE_VA
	and CG_DRIVER_RTC_BASE_PA defines to select the proper method.

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuRtcDestroy(void);


/**
    \ingroup cg_cpu_rtc
    RTC time structure
*/
typedef struct  {
	U32 year;		/**< Year (1970-2100) */
	U32 month;		/**< Month in year (0-11) */
	U32 day;		/**< Day in month (0-30) */
	U32 hour;		/**< Hour in day (0-23) */
	U32 minute;		/**< Minute in Hour (0-59) */
	U32 second;		/**< Second in minute (0-59) */
	U32 dayInWeek;	/**< Day in week (0-6) */
	U32 tick;		/**< internal tick counter (units are CPU specific) */
} TCgCpuRtcTime;

/**
    \ingroup cg_cpu_rtc

    Read current time from RTC unit

	\\attention Implementation is required

    \param[out]	apTime			Pointer to time structure

	\return System wide return code (see CgReturnCodes.h)

	\retval ECgOk			On success
*/
TCgReturnCode CgCpuRtcRead(TCgCpuRtcTime *apTime);

/*********************************************************** RF control *************************/

/** \defgroup cg_cpu_rf RF functions*/

/**
	\ingroup cg_cpu_rf

	Read one byte from RF control via specific protocol
    \param[out]	aValue			Pointer to received byte

	\return system error code
*/
TCgReturnCode CgCpuRFControlReadbyte(volatile unsigned char *aValue);

/**
	\ingroup cg_cpu_rf

	Write one byte to RF control via specific protocol
    \param[in]	aValue			Byte to send

	\return system error code
*/
TCgReturnCode CgCpuRFControlWriteByte(unsigned char aValue);

/*********************************************************** Miscellanies  *************************/
/** \defgroup cg_cpu_miscellanies Miscellanies functions*/

/**
    \ingroup cg_cpu_miscellanies
    Get last error code for this unit

	\return system error code
*/
U32 CgCpuLastErrorCode(void);



/**
    \ingroup cg_cpu_miscellanies
    Clear CPU cache

    \param[in]	pSource			Source memory address
    \param[in]	aDmaChannel		DMA channel


	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuClearCache(U32 pSource, U32 aDmaChannel);



/**
    \ingroup cg_cpu_miscellanies
    Set bits in bit-field register

    \param[in]	aReg			pointer to register
    \param[in]	aBitFieldDef	bit field definition
    \param[in]	aValue			value to set

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgSetRegisterBits(volatile U32 *aReg, TCgBitField aBitFieldDef, U32 aValue);

U32 CgGetU32Mask(U32 bitsInU32);


/**
    \ingroup cg_cpu_miscellanies
    Get bits from bit-field register

    \param[in]	aReg			register
    \param[in]	aBitFieldDef	bit field definition
    \param[in]	apValue			pointer to returned value

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgGetRegisterBits(volatile U32 aReg, TCgBitField aBitFieldDef, U32 *apValue);

/**
    \ingroup cg_cpu_miscellanies
    Global virtual memory allocation.
	This function is used on some platforms, where a global initialization of virtual memory is required.
	It is called as one of the first initialization steps in CgxDriverConstruct.

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuAllocateVa(void);

/**
    Reset a CPU sub-unit
    \param[in]	aUnitIndex		unit id (see specific CPU documentation)
	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuReset(U32 aUnitIndex);

/**
	Set the CGsnap IP Master reset to 'reset' state (device is not active)

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuIPMasterResetOn(void);

/**
	Set the CGsnap IP Master reset to 'clear' state (device is active)

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgCpuIPMasterResetClear(void);

/**
	Get CPU revision code.
	\note Used mainly for debug.

	\param[out]	apRevisionCode	CPU revision code

	\return CPU revision information as null terminated text.
*/
TCgReturnCode CgCpuRevision(char *apRevisionCode);


/**
	byte and word swap

	\param[in]	aSource			Pointer to source buffer
	\param[out]	aDest			Pointer to destination buffer
	\param[in]	aSizeInBytes	Size of memory, in bytes
	\param[in]	aFrom			Source byte order
	\param[in]	aTo				Destination byte order

	\return System wide return code (see CgReturnCodes.h)
	\retval ECgOk On success
*/
TCgReturnCode CgSwapBytesInBlock(void * aSource,void * aDest, long aSizeInBytes, TCgByteOrder aFrom, TCgByteOrder aTo);


#if defined(CGTEST) || defined(DEBUG)
	#define TRACE_ON
#endif
#define TRACE_ON

/**
	Debug facility - to show error messages
*/
#ifdef TRACE_ON
	TCgReturnCode CgxCpuMsg(const char *aFuncName, const char *aFileName, U32 aLineNum, char *aFormat, ...);
	TCgReturnCode CgxCpuPrint(char *aText);
	#define DBG_FUNC_NAME(name) const char *__funcname__ = name;
	#define DBGMSG(format) CgxCpuMsg(__funcname__, __FILE__, __LINE__, format);
	#define DBGMSG1(format,a) CgxCpuMsg(__funcname__, __FILE__, __LINE__, format, a);
	#define DBGMSG2(format,a,b) CgxCpuMsg(__funcname__, __FILE__, __LINE__, format, a,b);
	#define DBGMSG3(format,a,b,c) CgxCpuMsg(__funcname__, __FILE__, __LINE__, format, a,b,c);
	#define DBGMSG4(format,a,b,c,d) CgxCpuMsg(__funcname__, __FILE__, __LINE__, format, a,b,c,d);
	#define DBGMSG5(format,a,b,c,d,e) CgxCpuMsg(__funcname__, __FILE__, __LINE__, format, a,b,c,d,e);
	#define DBGMSG6(format,a,b,c,d,e,f) CgxCpuMsg(__funcname__, __FILE__, __LINE__, format, a,b,c,d,e,f);
	#define DBGMSG7(format,a,b,c,d,e,f,g) CgxCpuMsg(__funcname__, __FILE__, __LINE__, format, a,b,c,d,e,f,g);
	#define DBGMSG8(format,a,b,c,d,e,f,g,h) CgxCpuMsg(__funcname__, __FILE__, __LINE__, format, a,b,c,d,e,f,g,h);
	#define DBGMSG9(format,a,b,c,d,e,f,g,h,i) CgxCpuMsg(__funcname__, __FILE__, __LINE__, format, a,b,c,d,e,f,g,h,i);
	#define DBGDONE {DBGMSG("Done");}
	#define DBGSTAT(title,s) {DbgStat##s(title);}

#else
	#define DBG_FUNC_NAME(name)
	#define DBGMSG(format) do {} while(0)
	#define DBGMSG1(format,a) do {} while(0)
	#define DBGMSG2(format,a,b) do {} while(0)
	#define DBGMSG3(format,a,b,c) do {} while(0)
	#define DBGMSG4(format,a,b,c,d) do {} while(0)
	#define DBGMSG5(format,a,b,c,d,e) do {} while(0)
	#define DBGMSG6(format,a,b,c,d,e,f) do {} while(0)
	#define DBGMSG7(format,a,b,c,d,e,f,g) do {} while(0)
	#define DBGMSG8(format,a,b,c,d,e,f,g,h) do {} while(0)
	#define DBGMSG9(format,a,b,c,d,e,f,g,h,i) do {} while(0)
	#define DBGDONE do {} while(0)
	#define DBGSTAT(title,s) do {} while(0)
#endif

#define CgSwapBytesU32( aSource )\
	(((0xFF00FF00 & aSource) >> 8) | ((0x00FF00FF & aSource) << 8))


#define CgSwapWordsU32( aSource )\
	(((0xFFFF0000 & aSource) >> 16) | ((0x0000FFFF & aSource) << 16))


#define CgSwapBytesAndWordsU32( aSource )\
	( CgSwapWordsU32(CgSwapBytesU32(aSource)) )

#ifdef TRACE_ON
	#define DBGERR(rc)	if (!OK(rc)) { DBGMSG3("Error : (%ld) [%s:%d]", rc, __FILE__, __LINE__); }
#else
	#define DBGERR(rc)do {} while(0)
#endif


#ifdef TRACE_ON
	void DbgStatSPI(const char *aTitle);
	void DbgStatDMA(const char *aTitle);
	void DumpGpioState(U32 aGpio);
#else
	#define DbgStatSPI(x) do {} while(0)
	#define DbgStatDMA(x) do {} while(0)
	#define DumpGpioState(x) do {} while(0)
#endif


// ===========================================================================
#ifdef	__cplusplus
}
#endif /*__cplusplus*/

#endif
