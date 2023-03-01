#ifndef IS_VENDOR_SENSOR_PWR_AAS_V71X_H
#define IS_VENDOR_SENSOR_PWR_AAS_V71X_H

/***** This file is used to define sensor power pin name for only AAS_V71X *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - IMX686 *****/
#define RCAM_AF_VDD	<&s2mpb03_0_l7>		/* RCAM1_AFVDD_2P8 */
#define IMX686_AVDD1	<&s2mpb03_0_l5>		/* RCAM1_AVDD1_2P9 */
#define IMX686_AVDD2	<&s2mpb03_1_l3>		/* RCAM1_AVDD2_1P8 */
#define IMX686_DVDD	<&s2mpb03_0_l2>		/* RCAM1_DVDD_1P1 */
#define IMX686_IOVDD	<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 : CAM0_VLDO3 is used for all camera commonly */
#define IMX686_RST	<&gpp3 2 0x1>

/***** REAR MAIN - GW1 *****/
/*#define RCAM_AF_VDD	"CAM0_VLDO7" */	/* RCAM1_AFVDD_2P8 */
#define GW1_IOVDD	<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 : CAM0_VLDO3 is used for all camera commonly */
#define GW1_AVDD	<&s2mpb03_0_l5>		/* RCAM1_AVDD1_2P8 */
#define GW1_DVDD	<&s2mpb03_0_l2>		/* RCAM1_DVDD_1P1 */

/***** FRONT - IMX616 *****/
#define IMX616_IOVDD	<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 */
#define IMX616_AVDD	<&s2mpb03_0_l6>		/* IMX616_AVDD 2.8V */
#define IMX616_DVDD	<&s2mpb03_0_l1>		/* IMX616_DVDD 1.05V */
#define IMX616_RST	<&gpp3 3 0x1>

/***** REAR2 SUB - GC5035 *****/
#define GC5035_IOVDD	<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 */
#define GC5035_AVDD	<&s2mpb03_1_l6>		/* RCAM2_AVDD_2P8 */
#define GC5035_DVDD	<&s2mpb03_0_l4>		/* RCAM2_DVDD_1P2 */

#define GC5035_RST	<&gpf1 4 0x1>

/***** REAR3 WIDE - HI1336 *****/
#define HI1336_IOVDD	<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8  */
#define HI1336_AVDD	<&s2mpb03_1_l5>		/* RCAM3_AVDD_2P8 */
#define HI1336_DVDD	<&s2mpb03_1_l1>		/* RCAM3_DVDD_1P1 */
#define HI1336_RST	<&gpf0 2 0x1>

/***** REAR4 MACRO - GC5035 *****/
#define GC5035_MACRO_IOVDD	<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 */ 
#define GC5035_MACRO_AVDD	<&s2mpb03_1_l7>		/* RCAM4_AVDD_2P8 */
#define GC5035_MACRO_DVDD	<&s2mpb03_1_l2>		/* RCAM4_DVDD_1P2 */
#define GC5035_MACRO_RST	<&gpf1 2 0x1>


/***** ETC Define related to sensor power *****/

#define MIPI_SWITCH	"mipi_switch"	/* mipi_switch  */

#endif /* IS_VENDOR_SENSOR_PWR_AAS_V71X_H */
