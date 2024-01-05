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
	\brief SPRD_FPGA GPIO support

	Functions to access GPIO unit in SPRD_FPGA CPU, allowing GPSenseEngine to control GPIO lines.
	\attention This file is not completed, and must be updated for SPRD_FPGA (Please verify all 'TODO' comments are handled)

*/


#ifdef __cplusplus
extern "C" {
#endif


#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <mach/board.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
// ===========================================================================
//Kernel Ioctl
#include "CgTypes.h"						/**< CellGuide Types */
#include "CgCpu.h"

/** OK status */
#define ECgOk                            (0)




// ===========================================================================






// ===========================================================================


/**
	Internal function, set/reset a GPIO.

	This function is used by API functions CgCpuGpioSet & CgCpuGpioReset
*/
#if 0
static TCgReturnCode GpioSet(U32 aPin, U32 aValue)
{
	//DBG_FUNC_NAME("GpioSet");
	TCgReturnCode rc = ECgOk;
       // unsigned long gpio_addr = 0;
	return rc;
}
#endif


TCgReturnCode CgCpuGpioSet(U32 aPin)
{

        U32 gpio_num;
        gpio_num = aPin;
       // __gpio_set_value(gpio_num,1);

        return ECgOk;
}


TCgReturnCode CgCpuGpioReset(U32 aPin)
{
        U32 gpio_num;

        gpio_num = aPin;
       // __gpio_set_value(gpio_num,0);
        return ECgOk ;
}


TCgReturnCode CgCpuGpioGet(U32 aPin, int *aValue)
{
        U32 rc = 0;
	//DBG_FUNC_NAME("CgCpuGpioGet");


      //  *aValue = __gpio_get_value(aPin);
		*aValue = 0;
	return rc;
}


TCgReturnCode CgCpuGpioModeSet(U32 aPin, TCgCpuGpioMode aMode)
{
    	//U32 extint = 0;
        TCgReturnCode rc = ECgOk;
       // unsigned long gpio_dir = 0;


    //DBG_FUNC_NAME("CgCpuGpioModeSet")

    return rc;

}

TCgReturnCode CgCpuGpioPull(U32 aPinCode, TCgCpuGpioPull aPull)
{
   // U32 group = (aPinCode / CG_CPU_GPIO_GROUP_SIZE) - 1;
   // U32 pin = aPinCode % CG_CPU_GPIO_GROUP_SIZE;

	return ECgOk;
}


TCgReturnCode CgCpuGpioStrength(U32 aPinCode, U32 aStrength)
{
    //U32 group = (aPinCode / CG_CPU_GPIO_GROUP_SIZE) - 1;
    //U32 pin = aPinCode % CG_CPU_GPIO_GROUP_SIZE;


	return ECgOk;
}


TCgReturnCode CgCpuGpioCreate(void)
{
	TCgReturnCode rc = ECgOk;

         #if 0

	gpio_request(GPIO_GPS_IRQ, "gps_irq");
	gpio_direction_input(GPIO_GPS_IRQ);

	gps_irq = gpio_to_irq(GPIO_RESET_IRQ);

	gpio_request(GPIO_GPS_RESET , "gps_rst");
	printk("%s oob_irq:%d\n", __func__, oob_irq);
        #endif

	return rc;
}



TCgReturnCode CgCpuGpioDestroy(void)
{
    u32 rc = 0;

        #if 0
	gpio_free(GPIO_GPS_IRQ);
	gpio_free(GPIO_GPS_RESET);
        #endif

	return rc;
}



int SPRD_FPGA_gpio_set_regvalue(unsigned long addr, unsigned int bit, unsigned int data)
{


	return 0;
}

int SPRD_FPGA_gpio_get_regvalue(unsigned long addr, unsigned int bit)
{
	unsigned int value = 0;


	return value;
}

#ifdef __cplusplus
}
#endif





