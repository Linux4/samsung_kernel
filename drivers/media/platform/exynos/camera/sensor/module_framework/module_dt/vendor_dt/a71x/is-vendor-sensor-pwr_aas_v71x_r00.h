#ifndef IS_VENDOR_SENSOR_PWR_AAS_V71X_H
#define IS_VENDOR_SENSOR_PWR_AAS_V71X_H

/***** This file is used to define sensor power pin name for only AAS_V71X *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - GW1 *****/
#define RCAM_AF_VDD	<&s2mpb03_0_l7>	/* RCAM1_AFVDD_2P8 */
#define GW1_IOVDD	<&s2mpb03_0_l3>	/* CAM_VDDIO_1P8 : CAM_VLDO3 is used for all camera commonly */
#define GW1_AVDD	<&s2mpb03_0_l5>	/* RCAM1_AVDD1_2P8 */
#define GW1_DVDD	<&s2mpb03_0_l2>	/* RCAM1_DVDD_1P1 */

/***** FRONT - IMX616 *****/
#define IMX616_IOVDD	<&s2mpb03_0_l3>	/* CAM_VDDIO_1P8 */
#define IMX616_AVDD	<&s2mpb03_0_l6>	/* IMX616_AVDD 2.8V */
#define IMX616_DVDD	<&s2mpb03_0_l1>	/* IMX616_DVDD 1.05V */

/***** REAR2 SUB - GC5035 *****/
#define GC5035_IOVDD	<&s2mpb03_0_l3>    /* CAM_VDDIO_1P8 */
#define GC5035_AVDD	"avdd_ldo_en"	/* RCAM4_AVDD_2P8 */
#define GC5035_DVDD	"dvdd_ldo_en"	/* RCAM4_DVDD_1P2 */

/***** REAR3 WIDE - HI1336 *****/
#define HI1336_IOVDD	<&s2mpb03_0_l3>	/* CAM_VDDIO_1P8  */
#define HI1336_AVDD	"avdd_ldo_en"	/* RCAM3_AVDD_2P8: GPP3[2] */
#define HI1336_DVDD	<&s2mpb03_0_l4>	/* RCAM3_DVDD_1P1 */

/***** REAR4 MACRO - GC5035 *****/
#define GC5035_MACRO_IOVDD	<&s2mpb03_0_l3>	/* CAM_VDDIO_1P8 */
#define GC5035_MACRO_AVDD	"avdd_ldo_en"	/* RCAM4_AVDD_2P8 */
#define GC5035_MACRO_DVDD	"dvdd_ldo_en"	/* RCAM4_DVDD_1P2 */


/***** ETC Define related to sensor power *****/

#define MIPI_SWITCH	"mipi_switch"	/* mipi_switch  */

#endif /* IS_VENDOR_SENSOR_PWR_AAS_V71X_H */

