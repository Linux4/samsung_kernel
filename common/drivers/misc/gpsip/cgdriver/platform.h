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
	\brief SPRD_FPGA specific defines

	\defgroup CGX_DRIVER_PLATFORM_ANDROID SPRD_FPGA/Android platform specific defines
	\{
	The following values are specific to the SPRD_FPGA / Android implementation.
	For other platforms, please select appropriate values.

 */
#ifndef PLATFORM_H
#define PLATFORM_H

///
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <linux/dma-mapping.h>

#include <mach/hardware.h>
#include <linux/sprd_2351.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>


#define CGX_PLATFORM_NAME "SPRD_FPGA_FPGA"

/*************************************************************************************/

/*New add by paul*/

#define NUMBER_1K                       (1 *1024)
#define NUMBER_16K                      (16*1024)
#define NUMBER_64K                      (64*1024)


#define MAX_DMA_CHUNK_SIZE				(0x80000)	// 512K
#define MAX_DMA_TRANSFER_TASKS			(64)		/**< Maximum number of DMA tasks */


#define CG_DRIVER_GPIO_TCXO_EN	    (22)
#define CG_DRIVER_GPIO_GPS_MRSTN    (22)


/**
	CGsnap IP physical address
	\attention Need to be ported
	\ingroup SPRD_FPGA_cpu_specific

        TODO: modify by sprd
	
*/
#define CG_DRIVER_CGCORE_BASE_PA    0x21C00000 /* cg ip base */

#define CG_DRIVER_GPIO_BASE_PA		(0x8A000000)

#define CG_DRIVER_INTR_BASE_PA		(0xFC000000)


#define CG_DRIVER_CLK_BASE_PA		(0xFC802000)


/** 
	\name Memory Mapping (Virtual Memory Addresses)
	in Linux, the driver run in kernel space, and can access all physical addresses directly,
	so, VA=PA 
	\{
 */

#ifdef CONFIG_OF
struct gps_2351_addr
{
	u32 gps_base;
	u32 ahb_base;
	u32 irq_num;
	u32 lna_gpio;
	#ifdef CONFIG_ARCH_SCX30G
	u32 pmu_base;
	#endif
};

extern struct gps_2351_addr gps_2351;
extern u32 gps_get_core_base(void);
extern u32 gps_get_ahb_base(void);
extern u32 gps_get_irq(void);
extern u32 gps_get_lna_gpio(void);


#define CG_DRIVER_CGCORE_BASE_VA		gps_get_core_base()
#define CG_DRIVER_SCLK_VA				gps_get_ahb_base()
#define SPRD_GPS_INT					gps_get_irq()
#define SPRD_GPS_LNA_EN    			gps_get_lna_gpio()

#ifdef CONFIG_ARCH_SCX30G
extern u32 gps_get_pmu_base(void);

#define SPRD_GPS_CLK_SINEX			gps_get_pmu_base()
#define SPRD_GPS_CLK_AUTO_GATING	(SPRD_GPS_CLK_SINEX+0x114)
#define SPRD_GPS_CLK_SEL			(SPRD_GPS_CLK_SINEX+0x134)
#endif

#else

/** Virtual base address for CGsnap registers */
#define CG_DRIVER_CGCORE_BASE_VA	SPRD_GPS_BASE

#ifdef CONFIG_ARCH_SCX30G
#define SPRD_GPS_CLK_SINEX			SPRD_PMU_BASE
#define SPRD_GPS_CLK_AUTO_GATING	(SPRD_GPS_CLK_SINEX+0x114)
#define SPRD_GPS_CLK_SEL			(SPRD_GPS_CLK_SINEX+0x134)
#endif

/** Virtual base address for CGsnap sclk */
#define CG_DRIVER_SCLK_VA			SPRD_AHB_BASE
#define SPRD_GPS_INT				IRQ_GPS_INT

#if defined(CONFIG_DOLPHIN_CHIP_2351)
#define SPRD_GPS_LNA_EN    (35)
#elif defined(CONFIG_SHARK_CHIP_2351)
#define SPRD_GPS_LNA_EN    (50)
#endif
#endif


/** \} Memory Mapping */


/** 


	\name Interrupt Codes
	\{
 */


/** DMA interrupt event name

	For platforms where the interrupt is handled by BSP code, this is the event 
	name to be used by the driver.
	in all other cases, this should be defined 'NULL'
 */
#define CGX_DRIVE_DMA_INT_EVENT_NAME (NULL)

/** DMA completion exported sync-object name

	For platforms where the DMA interrupt is handled by BSP code/driver, this is the sync-object name
	exported by the operating system, that can be waited on, and that issues a signal when DMA
	x-fer is complete.
	in all other cases, this should not be defined
*/

/*modify bu paul for compile-----debug*/
//#define gDMACompletion NULL
// struct completion  gDMACompletion;
//#define CGX_DRIVE_DMA_INT_GLOBAL_SYNC_OBJECT (gDMACompletion)


/** GPS interrupt event name
	For platforms where the interrupt is handled by BSP code, this is the event 
	name to be used by the driver.
	in all other cases, this should be defined 'NULL'
 */
#define CGX_DRIVE_GPS_INT_EVENT_NAME	(NULL)

/** \} Interrupt codes */

/** 


	\name DMA Control 
	\{ 
*/


#define CG_DRIVER_DMA_CHANNEL_READ		(0) /**< DMA channel to use for snap transfer */



/** \} DMA Control */


/** 


	\name GPIO mapping
	\{
 */




/** GPIO assigned to RF front-end configuration data. 
	This line is used to configure the ACLYS-L, (send and receive data in I2C like protocol)
	a.k.a. SER2
	\attention Input & Output
 */
#define CG_DRIVER_GPIO_RF_DATA				(0 + 3)

/** GPIO assigned to RF front-end configuration clock. 
	This line is used to configure the ACLYS-L, (clocking the I2C like protocol)
	a.k.a. SER1
	\attention Output only
 */
#define CG_DRIVER_GPIO_RF_CLK				(0 + 2)


/** GPIO assigned to RF front-end power down. 
	This line is used to control RF FrontEnd power mode (i.e., select between power on and power down modes)
	This value is used on if CG_DRIVER_CGCORE_CONTROLS_RF_PD is FALSE
	\attention Output only
 */
#define CG_DRIVER_GPIO_RF_PD				(0 + 0)
#define CG_DRIVER_CG_CORE_CONTROLS_RF_PD	(TRUE)
#define RF_POWER_UP_VAL (0)					// ACLYS RF requires '0' for PU

#define CGX5000_DR							(0 + 18)

extern struct sprd_2351_interface *gps_rf_ops;



//#define CGX5000_INT		(CG_CPU_GPIO_GROUP_D + 9)

/** \} GPIO mapping */


/** \} */

#endif /* PLATFORM_H */
