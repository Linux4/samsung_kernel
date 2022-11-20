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
#include <soc/sprd/gpio.h>
#include <linux/regulator/consumer.h>
#include <soc/sprd/regulator.h>
#include <linux/clk.h>
#include <linux/delay.h>

#include "CgxDriverApi.h"
#include "CgCpu.h"
#include "platform.h"
#include "CgReturnCodes.h"
#include "CgxDriverOs.h"
#include "CgxDriverCore.h"

#ifdef CONFIG_ARCH_SCX20
#include <linux/sprd_2351.h>
#endif

#ifndef CGCORE_ACCESS_VIA_SPI
extern TCgReturnCode CGCoreSclkEnable(int enable);
struct sprd_2351_interface *gps_rf_ops;
#endif

/** Native byte order for this CPU */
const TCgByteOrder CGX_DRIVER_NATIVE_BYTE_ORDER = ECG_BYTE_ORDER_4321;	// SPRD_FPGA is little endian

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
#ifndef CGCORE_ACCESS_VIA_SPI
	return SPRD_GPS_INT;
#else
	gpio_direction_input (CG_DRIVER_GPIO_GPS_INT);
	return gpio_to_irq(CG_DRIVER_GPIO_GPS_INT);
#endif
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

#ifdef CGCORE_ACCESS_VIA_SPI
static void CgxDriverRFInit(void)
{
	unsigned int value;

	value = 0x005f8420;
	gps_spi_write_bytes(1, 0x82*4, value);
	gps_spi_read_bytes(1, 0x82*4, &value);

	value = 0x00010003;
	gps_spi_write_bytes(1, 0x83*4, value);

	/*RF4in1 */
	value = 0x006f0609;
	gps_spi_write_bytes(1, 0x80*4, value);

	value = 0x007b8020;
	gps_spi_write_bytes(1, 0x80*4, value);

	value = 0x07000001;//GPS Mode 0:Idle 1:Rx
	gps_spi_write_bytes(1, 0x80*4, value);

#if 0
	value = 0x004af417;
	gps_spi_write_bytes(1, 0x80*4, value);
#endif
	value = 0x07385400;
	gps_spi_write_bytes(1, 0x80*4, value);

	value = 0x076c0721;
	gps_spi_write_bytes(1, 0x80*4, value);

	/*for 26M/16M TCXO AGC 1bit*/
	value = 0x07635140;//0x07635141;
	gps_spi_write_bytes(1, 0x80*4, value);

	value = 0x0704ef00;//max gain
	gps_spi_write_bytes(1, 0x80*4, value);

#if 1 //def CGCORE_GPS_26M  /*for 26M  TCXO*/
	value = 0x073c08d2;
	gps_spi_write_bytes(1, 0x80*4, value);

	value = 0x07023d61;
	gps_spi_write_bytes(1, 0x80*4, value);
#else  /*for 16M TCXO*/
	value = 0x073c0000;
	gps_spi_write_bytes(1, 0x80*4, value);

	value = 0x07026180;
	gps_spi_write_bytes(1, 0x80*4, value);
#endif

	//for dma block size
	gps_spi_sysreg_read_bytes(1,0x01,&value);;
	value = 0;
	value = ((1<<28)|(1<<26)|(1<<25));
	gps_spi_sysreg_write_bytes(1,0x01,value );
	gps_spi_sysreg_read_bytes(1,0x01,&value);

#if 1
	/*When for RX  Analog test such as GAIN and IF frequency ,
	we will add following  configuration*/
	value = 0x077b0112;//0x077b0101;
	gps_spi_write_bytes(1, 0x80*4, value);
#endif

#if 0 //for RF Test
	value = 0x075d88e6;
	gps_spi_write_bytes(1, 0x80*4, value);

	value = 0x070a7835;
	gps_spi_write_bytes(1, 0x80*4, value);
#endif
	
	//read reg
	value = 0x806f0000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x006f0609:0x%x\n",value);

	value = 0x807b0000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x007b8020:0x%x\n",value);

	value = 0x87000000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x07000001:0x%x\n",value);

	value = 0x87380000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x07385400:0x%x\n",value);

	value = 0x876c0000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x076c0721:0x%x\n",value);


	value = 0x87630000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x07635140:0x%x\n",value);

	value = 0x87040000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x0704ef00:0x%x\n",value);

#if 1   /*for 26M  TCXO*/
	value = 0x873c0000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x073c08d2:0x%x\n",value);

	value = 0x87020000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x07023d61:0x%x\n",value);
#else
	value = 0x873c0000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x073c0000:0x%x\n",value);

	value = 0x87020000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x07026180:0x%x\n",value);
#endif

	value = 0x877b0000;
	gps_spi_write_bytes(1, 0x80*4, value);
	gps_spi_read_bytes(1, 0x81*4, &value);
	printk("rf 0x077b0112:0x%x\n",value);

}

void gps_gpio_request(void)
{
	int ret;
	ret = gpio_request (CG_DRIVER_GPIO_GPS_PDN, "gps_pdn");
	if (ret){
		printk ("gps_pdn request err: %d\n", CG_DRIVER_GPIO_GPS_PDN);
	}

	ret = gpio_request (CG_DRIVER_GPIO_GPS_MRSTN, "gps_reset");
	if (ret){
		printk ("gps_reset request err: %d\n", CG_DRIVER_GPIO_GPS_MRSTN);
	}

	ret = gpio_request (CG_DRIVER_GPIO_GPS_SPI_CS, "gps_spi_cs");
	if (ret){
		printk ("gps_spi_cs request err: %d\n", CG_DRIVER_GPIO_GPS_SPI_CS);
	}

	ret = gpio_request (CG_DRIVER_GPIO_GPS_INT, "gps-req-int");
	if (ret)
	{
		printk ("gps-req- int err: %d\n", CG_DRIVER_GPIO_GPS_INT);
	}
}

static struct regulator *greeneye_vdd = NULL;
static void greeneye_vddsim2_enable(bool on)
{
	int ret;
	if (greeneye_vdd == NULL) {
		greeneye_vdd = regulator_get(NULL, "vddsim2");

		if (IS_ERR(greeneye_vdd)) {
			pr_err("Get regulator of vddsim2  error!\n");
			return;
		}
	}
	if (on) {
		regulator_set_voltage(greeneye_vdd, 2800000, 2800000);
		ret = regulator_enable(greeneye_vdd);
	}
	else if (regulator_is_enabled(greeneye_vdd)) {
		ret = regulator_disable(greeneye_vdd);
	}
}


static void gps_clk_init(bool enable)
{
	static struct clk *gps_clk_32k=NULL;

	if (gps_clk_32k == NULL) {
		gps_clk_32k = clk_get(NULL, "ext_32k");
		if (IS_ERR(gps_clk_32k)) {
			printk("failed to get parent ext_32k\n");
			return;
		}
	}
	if (enable) {
		#ifndef CONFIG_OF
		clk_enable(gps_clk_32k);
		#else
		clk_prepare_enable(gps_clk_32k);
		#endif
	}else{
		#ifndef CONFIG_OF
		clk_disable(gps_clk_32k);
		#else
		clk_disable_unprepare(gps_clk_32k);
		#endif
	}
}

TCgReturnCode CgxDriverRFPowerDown(void)
{
	//DBG_FUNC_NAME("CgxDriverRFPowerDown")
	TCgReturnCode rc = ECgOk;
	unsigned int value;

	printk("%s\n",__func__);

	/*RF4in1 */
	value = 0x07000000;//GPS Mode 0:Idle 1:Rx
	gps_spi_write_bytes(1, 0x80*4, value);
	return rc;
}

TCgReturnCode CgxDriverRFPowerUp(void)
{
	//DBG_FUNC_NAME("CgxDriverRFPowerDown")
	TCgReturnCode rc = ECgOk;

	printk("%s\n",__func__);
	CgxDriverRFInit();
	return rc;
}

void gps_chip_power_on(void)
{
	printk("%s\n",__func__);

	greeneye_vddsim2_enable(1);
	gps_clk_init(1);

	//pdn pin pull high
	CgCpuGpioModeSet(CG_DRIVER_GPIO_GPS_PDN, ECG_CPU_GPIO_OUTPUT);
	CgCpuGpioSet(CG_DRIVER_GPIO_GPS_PDN);
	CgCpuDelay(1000);

	//reset
	CgCpuGpioModeSet(CG_DRIVER_GPIO_GPS_MRSTN, ECG_CPU_GPIO_OUTPUT);
	CgCpuIPMasterResetClear();
	msleep(1);
	CgCpuIPMasterResetOn();
	msleep(5);
	CgCpuIPMasterResetClear();
	msleep(1);

	//gps core reset
	gps_spi_write_bytes(1, 0Xfc, 0x0);
	gps_spi_write_bytes(1, 0Xfc, 0x0f);
	msleep(1);
	CgxDriverRFPowerUp();
}

void gps_chip_power_off(void)
{
	printk("%s\n",__func__);

	//pdn pin pull low
	CgCpuGpioModeSet(CG_DRIVER_GPIO_GPS_PDN, ECG_CPU_GPIO_OUTPUT);
	CgCpuGpioReset(CG_DRIVER_GPIO_GPS_PDN);
	CgCpuDelay(1000);
	msleep(1);

	//reset pin pull low
	CgCpuIPMasterResetOn();

	//spi2  pin pull low
	gpio_direction_output(CG_DRIVER_GPIO_GPS_SPI_CS, 0);
	greeneye_vddsim2_enable(0);
	gps_clk_init(0);
}

#else
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
#ifdef CONFIG_ARCH_SCX20
    rf2351_vddwpa_ctrl_power_enable(1);
#endif
	//enable gps sysclk
	CGCoreSclkEnable(1);

	//gps core reset
	CgxCpuWriteMemory((U32)CG_DRIVER_CGCORE_BASE_VA, 0xfc,0xf);
	CgxDriverRFPowerUp();
}

void gps_chip_power_off(void)
{
	printk("%s\n",__func__);
	CgxDriverRFPowerDown();

	//gps core reset
	CgxCpuWriteMemory((U32)CG_DRIVER_CGCORE_BASE_VA, 0xfc,0x0);
	CGCoreSclkEnable(0);
#ifdef CONFIG_ARCH_SCX20
    rf2351_vddwpa_ctrl_power_enable(0);
#endif
}
#endif

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
	#ifdef CGCORE_ACCESS_VIA_SPI
	CgCpuGpioSet(CG_DRIVER_GPIO_GPS_MRSTN);
	#else
   // CgCpuGpioSet(CG_DRIVER_GPIO_GPS_MRSTN);
	#endif

	return rc;
}




TCgReturnCode CgCpuIPMasterResetOn(void)
{
	TCgReturnCode rc = ECgOk;
    DBG_FUNC_NAME("CgCpuIPMasterResetOn");

	DBGMSG("start");
	// Set master reset pin to '0' (reset on - device inactive)

	#ifdef CGCORE_ACCESS_VIA_SPI
	CgCpuGpioReset(CG_DRIVER_GPIO_GPS_MRSTN);
	#else
    //CgCpuGpioReset(CG_DRIVER_GPIO_GPS_MRSTN);
	#endif

	return rc;
}


TCgReturnCode CgCpuAllocateVa(void)
{
	// Nothing to do for SPRD_FPGA
	return ECgOk;
}


