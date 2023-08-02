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
	\brief platform/CPU specific code

	\defgroup CGX_DRIVER_PLATFORM_ANDROID CPU/Android platform specific defines
	\{
*/

#include <asm/uaccess.h>
#include <mach/gpio.h>

#include "CgxDriverApi.h"
#include "CgCpu.h"
#include "platform.h"
#include "CgReturnCodes.h"
#include "CgxDriverOs.h"
#include "CgxDriverCore.h"

extern TCgReturnCode CGCoreSclkEnable(int enable);

/** Native byte order for this CPU */
const TCgByteOrder CGX_DRIVER_NATIVE_BYTE_ORDER = ECG_BYTE_ORDER_4321;	// SPRD_FPGA is little endian
struct sprd_2351_interface *gps_rf_ops;

/*
    Reset the core and timers and initialize GPIO (direction and special function)
*/
TCgReturnCode CgxDriverInit(void *pDriver)
{
	// Check all addresses are defined
	/*
	#if CG_DRIVER_CGSNAP_BASE_PA == CG_UNDEFINED_ADDRESS
		#error please define CG_DRIVER_CGSNAP_BASE_PA with correct address
	#endif
	#if CG_DRIVER_REVISION_BASE_PA == CG_UNDEFINED_ADDRESS
		#error please define CG_DRIVER_REVISION_BASE_PA with correct address
	#endif
	#if CG_DRIVER_GPIO_BASE_PA == CG_UNDEFINED_ADDRESS
		#error please define CG_DRIVER_GPIO_BASE_PA with correct address
	#endif
	#if CG_DRIVER_DMA_BASE_PA == CG_UNDEFINED_ADDRESS
		#error please define CG_DRIVER_DMA_BASE_PA with correct address
	#endif
	#if CG_DRIVER_INTR_BASE_PA == CG_UNDEFINED_ADDRESS
		#error please define CG_DRIVER_INTR_BASE_PA with correct address
	#endif
	#if CG_DRIVER_CLK_BASE_PA == CG_UNDEFINED_ADDRESS
		#error please define CG_DRIVER_CLK_BASE_PA with correct address
	#endif
	*/
	// TODO 1st time initialization

    return ECgOk;
}






TCgReturnCode CgxDriverDataReadyPrepare(void)
{
	return CgCpuGpioModeSet(CGX5000_DR, ECG_CPU_GPIO_FALLING_INT);
}


TCgReturnCode CgxDriverInterruptPrepare(void)
{
	TCgReturnCode rc = ECgOk;

	// not using 'GPS' interrupt, so there is nothing to do

	return rc;
}

TCgReturnCode CgxDriverDmaInterruptPrepare(void)
{
	// TODO initial setup for DMA interrupt.
	// if no special work is required, leave this function empty
	return ECgOk;
}




TCgReturnCode CgxDriverGpsInterruptPrepare(void)
{
	// TODO return the interrupt code for 'CGsnap' interrupt
	// if no special work is required, leave this function empty
	return ECgOk;
}




U32 CgxDriverDataReadyInterruptCode(void)
{
	// TODO return the interrupt code for 'DMA end of transfer' interrupt

	return 0;
}


U32 CgxDriverGpsInterruptCode(void)
{
	// TODO return the interrupt code for 'CGsnap' interrupt
	return SPRD_GPS_INT;
}


TCgReturnCode CgxDriverWriteData(unsigned char *aSourceVirtualAddress, unsigned long aSourcePhysicalAddress, unsigned long aLengthBytes)
{
    //DBG_FUNC_NAME("CgxDriverWriteData")
    TCgReturnCode rc = ECgOk;

// 	#if WRITE_MODE == CG_CPU_SPI_MODE_DMA
// 		if (OK(rc)) rc = CgCpuSpiDmaStop(SPI_CHANNEL);
// 		CgCpuCacheSync();
// 		CgCpuSpiSetup(SPI_CHANNEL, SPI_PRESCALE, ECG_CPU_SPI_OUTPUT_DMA);
// 		CgCpuDmaSetupToDevice(aSourcePhysicalAddress,aLengthBytes,0,0);
// 		if (OK(rc)) rc = CgCpuSpiDmaStart(SPI_CHANNEL);
// 		if (OK(rc)) rc = CgCpuDmaWaitFinish(CG_DRIVER_DMA_CHANNEL_WRITE);
// 	#elif WRITE_MODE == CG_CPU_SPI_MODE_MANUAL
// 		U32 i;
// 		CgCpuSpiSetup(SPI_CHANNEL, SPI_PRESCALE, ECG_CPU_SPI_OUTPUT_POLLING);
//
// 		for(i = 0; i < aLengthBytes; i++) {
// 			CgCpuSpiTxFinishWait(SPI_CHANNEL);
// 			CgCpuSpiWriteByte(SPI_CHANNEL, aSourceVirtualAddress[i]);
// 		}
// 	#endif

    return rc;
}


TCgReturnCode CgxDriverGpioCode(int aControlLine, int *aGpioCode)
{
	*aGpioCode = 0;
	return ECgNotSupported;
}

TCgReturnCode CgxDriverReadData(unsigned char *aTargetVirtualAddress, unsigned long aTargetPhysicalAddress, unsigned long aLengthBytes)
{
	//DBG_FUNC_NAME("CgxDriverReadData")
	TCgReturnCode rc = ECgOk;

// 	#if READ_MODE==CG_CPU_SPI_MODE_DMA
// 		// memset(aTargetVirtualAddress, 0x55, aLengthBytes); //NG! debug
// 		CgCpuCacheSync();//NG! debug
// 		// Buffer data (not register data) is written in DMA mode
// 		if (OK(rc)) rc = CgCpuSpiDmaStop(SPI_CHANNEL);
//
// 		CgCpuCacheSync();
// 		if (OK(rc)) rc = CgCpuSpiSetup(SPI_CHANNEL, SPI_PRESCALE, ECG_CPU_SPI_INPUT_DMA);
// 		if (OK(rc)) rc = CgCpuDmaSetupFromDevice(aTargetPhysicalAddress, aLengthBytes, SPI_PRESCALE, CG_DRIVER_DMA_CHANNEL_READ, CG_DRIVER_DMA_CHANNEL_WRITE, SPI_CHANNEL);
// 		if (OK(rc)) rc = CgCpuSpiDmaStart(SPI_CHANNEL);
// 		// no need to wait for DMA to finish
//
// 	#elif READ_MODE==CG_CPU_SPI_MODE_MANUAL
// 		unsigned long i;
// 		CgCpuSpiSetup(SPI_CHANNEL, SPI_PRESCALE, ECG_CPU_SPI_INPUT_POLLING);
// 		for(i = 0; i < aLengthBytes; i++) {
// 			CgCpuSpiWriteByte(SPI_CHANNEL, 0); //dummy transmit
// 			CgCpuSpiRxFinishWait(SPI_CHANNEL);
// 			CgCpuSpiReadByte(SPI_CHANNEL, (volatile unsigned char *)(aTargetVirtualAddress + i));
// 			}
// 	#endif

	return rc;

}


TCgReturnCode CgxDriverClearData(unsigned char *aTargetVirtualAddress, unsigned long aTargetPhysicalAddress, unsigned long aLengthBytes)
{
	return CgxDriverReadData(aTargetVirtualAddress, aTargetPhysicalAddress, aLengthBytes);
}


TCgReturnCode CgxDriverIsDataReady(void)
{
	unsigned int val = 0;
	CgCpuGpioModeSet(CGX5000_DR, ECG_CPU_GPIO_INPUT);
	CgCpuGpioGet(CGX5000_DR, &val);
	return (val == 0) ? ECgOk : ECgGeneralFailure;
}


TCgReturnCode CgxDriverReadDataTail(unsigned char *aTargetVirtualAddress, unsigned long aTargetPhysicalAddress, unsigned long aLengthBytes)
{
	// no tail reading is required for SPRD_FPGA

	return ECgOk;
}

static TCgReturnCode CgxDriverRFInit(void)
{
	u32 value;
	gps_rf_ops->write_reg(0x0700,0x0001);
	gps_rf_ops->write_reg(0x004a,0xf417);
	gps_rf_ops->write_reg(0x076c,0x0721);
	gps_rf_ops->write_reg(0x0763,0x5141);
	gps_rf_ops->write_reg(0x0738,0x5400);
	gps_rf_ops->write_reg(0x0704,0xef00);
	gps_rf_ops->write_reg(0X077b,0x0102);

	#if 1 /*for 26M  TCXO*/
	gps_rf_ops->write_reg(0X073c,0x08d2);
	gps_rf_ops->write_reg(0X0702,0x3d61);
	#else /*for 16M  TCXO*/
	gps_rf_ops->write_reg(0x073c,0x0000);
	gps_rf_ops->write_reg(0X0702,0x6180);
	#endif

	/* for IF*/
	#if 0
	gps_rf_ops->write_reg(0x075d,0x88e6);
	gps_rf_ops->write_reg(0x070a,0x7835);
	#endif

#if 1
	/*for debug log */
	gps_rf_ops->read_reg(0x004a,&value);
	printk("0x004af417 value: %x\n", value);

	gps_rf_ops->read_reg(0x076c,&value);
	printk("0x076c0721 value: %x\n", value);

	gps_rf_ops->read_reg(0x0763,&value);
	printk("0x07635141 value: %x\n", value);

	gps_rf_ops->read_reg(0x0738,&value);
	printk("0x07385400 value: %x\n", value);

	gps_rf_ops->read_reg(0x0704,&value);
	printk("0x0704ef00 value: %x\n", value);

	gps_rf_ops->read_reg(0X077b,&value);
	printk("0x077b0102 value: %x\n", value);

	gps_rf_ops->read_reg(0x0700,&value);
	printk("0x07000001 value: %x\n", value);

	#if 1 /*for 26M  TCXO*/
	gps_rf_ops->read_reg(0X073c,&value);
	printk("0x073c08d2 value: %x\n", value);

	gps_rf_ops->read_reg(0X0702,&value);
	printk("0x07023d61 value: %x\n", value);
	#else /*for 16M  TCXO*/
	gps_rf_ops->read_reg(0x073c,&value);
	printk("0x073c0000 value: %x\n", value);

	gps_rf_ops->read_reg(0X0702,&value);
	printk("0x07026180 value: %x\n", value);
	#endif

	/* for IF*/
	#if 0
	gps_rf_ops->read_reg(0x075d,&value);
	printk("0x075d88e6 value: %x\n", value);

	gps_rf_ops->read_reg(0x070a,&value);
	printk("0x070a7835 value: %x\n", value);
	#endif
#endif

	return 0;

}

void gps_gpio_request(void)
{
	int ret;
	ret = gpio_request (SPRD_GPS_LNA_EN, "gps_lna");
	if (ret){
		printk ("GPS_LNE request err: %d\n", SPRD_GPS_LNA_EN);
	}
}

static void gps_lna_enable(void)
{
	gpio_direction_output(SPRD_GPS_LNA_EN,1);
}

static void gps_lna_disable(void)
{
	gpio_direction_output(SPRD_GPS_LNA_EN,0);
}

static void gps_reg_init(void)
{
#ifdef CONFIG_ARCH_SCX30G
	/*GPS Clock Select to CLK_SINE0*/
	sci_glb_clr(SPRD_GPS_CLK_SEL,BIT_3);

	/*Disable 26M Clock Gating*/
	sci_glb_clr(SPRD_GPS_CLK_AUTO_GATING,BIT_0);
#endif
}

TCgReturnCode CgxDriverRFPowerDown(void)
{
	//DBG_FUNC_NAME("CgxDriverRFPowerDown")
	TCgReturnCode rc = ECgOk;

	printk("%s\n",__func__);

	gps_rf_ops->mspi_enable();
	gps_rf_ops->write_reg(0x0700,0x0000);
	gps_lna_disable();
	gps_rf_ops->mspi_disable();
	return rc;
}

TCgReturnCode CgxDriverRFPowerUp(void)
{
	//DBG_FUNC_NAME("CgxDriverRFPowerDown")
	TCgReturnCode rc = ECgOk;

	printk("%s\n",__func__);
	gps_rf_ops->mspi_enable();
	gps_lna_enable();
	gps_reg_init();
	CgxDriverRFInit();
	gps_rf_ops->mspi_disable();
	return rc;
}

void gps_chip_power_on(void)
{
	printk("%s\n",__func__);

	//enable gps sysclk
	CGCoreSclkEnable(1);

	//gps core reset
	CgxCpuWriteMemory((U32)CG_DRIVER_CGCORE_BASE_VA, 0xfc,0xf);
	CgxDriverRFPowerUp();
}

void gps_chip_power_off(void)
{
	int value;

	printk("%s\n",__func__);

	CgxDriverRFPowerDown();

	//gps core reset
	CgxCpuWriteMemory((U32)CG_DRIVER_CGCORE_BASE_VA, 0xfc,0x0);
	CGCoreSclkEnable(0);
}


TCgReturnCode CgxDriverTcxoEnable(U32 aEnable)
{
	// default implementation assumes direct manipulation of TCXO LDO via a dedicated GPIO pin.
	// either CG_DRIVER_GPIO_TCXO_ENABLE or CG_DRIVER_GPIO_TCXO_DISABLE should be defined depending
	// on the LDO control level (active high / active low)

	#if defined(CG_DRIVER_GPIO_TCXO_ENABLE)
		if (aEnable)
			CgCpuGpioSet(CG_DRIVER_GPIO_TCXO_ENABLE);
		else
			CgCpuGpioReset(CG_DRIVER_GPIO_TCXO_ENABLE);
	#elif defined(CG_DRIVER_GPIO_TCXO_DISABLE)
		if (aEnable)
			CgCpuGpioReset(CG_DRIVER_GPIO_TCXO_DISABLE);
		else
			CgCpuGpioSet(CG_DRIVER_GPIO_TCXO_DISABLE);
	#endif

	return ECgOk;
}



TCgReturnCode CgCpuRevision(char *apRevisionCode)
{

    //sprintf(apRevisionCode,"SPRD_FPGA-R255",0);
    sprintf(apRevisionCode,"SPRD_FPGA-R255");

	return ECgOk;

}


TCgReturnCode CgCpuClockDestroy(void)
{
	TCgReturnCode rc = ECgOk;
	return rc;
}

TCgReturnCode CgCpuIntrDestroy(void)
{
 	TCgReturnCode rc = ECgOk;
 //   #if defined(CG_DRIVER_INTR_BASE_PA) && !defined(CG_DRIVER_INTR_BASE_VA)
//        rc = CgCpuVirtualFree((volatile void **)&gpINTR);
 //   #endif
	return rc;
}


TCgReturnCode CgCpuIPMasterResetClear(void)
{
	TCgReturnCode rc = ECgOk;
        DBG_FUNC_NAME("CgCpuIPMasterResetClear");

	DBGMSG("start");
	// Set master reset pin to '1' (reset off - device active)
   // CgCpuGpioSet(CG_DRIVER_GPIO_GPS_MRSTN);

	return rc;
}




TCgReturnCode CgCpuIPMasterResetOn(void)
{
	TCgReturnCode rc = ECgOk;
    DBG_FUNC_NAME("CgCpuIPMasterResetOn");

	DBGMSG("start");
	// Set master reset pin to '0' (reset on - device inactive)

    CgCpuGpioReset(CG_DRIVER_GPIO_GPS_MRSTN);

	return rc;
}


TCgReturnCode CgCpuAllocateVa(void)
{
	// Nothing to do for SPRD_FPGA
	return ECgOk;
}


