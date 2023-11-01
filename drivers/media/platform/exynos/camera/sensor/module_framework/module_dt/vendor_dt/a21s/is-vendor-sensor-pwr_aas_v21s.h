#ifndef IS_VENDOR_SENSOR_PWR_AAS_V21S_H
#define IS_VENDOR_SENSOR_PWR_AAS_V21S_H

/***** This file is used to define sensor power pin name for only AAS_V21S *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - GM2 *****/
#define RCAM_AF_VDD		<&l32_reg>			/* RCAM1_AFVDD_2P8 */
#define GM2_AVDD		<&s2mpb03_0_l6>		/* RCAM1_AVDD1_2P8 : CAM1_VLDO6*/
#define GM2_DVDD		<&s2mpb03_0_l1>		/* RCAM1_DVDD_1P05 : CAM1_VLDO1*/
#define GM2_IOVDD		<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 : CAM1_VLDO3 is used for all camera commonly */
#define GM2_RST			<&gpa2 3 0x1>

/***** FRONT - HI1336B *****/
#define HI1336B_IOVDD	<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 */
#define HI1336B_AVDD	<&s2mpb03_0_l7>		/* FRONT_AVDD 2.8V */
#define HI1336B_DVDD	<&s2mpb03_0_l2>		/* FRONT_DVDD 1.1V */
#define HI1336B_RST		<&gpa2 4 0x1>

/***** REAR2 SUB - GC02M1 *****/
#define GC02M1_IOVDD	<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 */
#define GC02M1_AVDD		<&l31_reg>			/* RCAM2_AVDD_2P8 */
#define GC02M1_DVDD		<&gpg3 0 0x1>			/* RCAM2_DVDD_1P2 */
#define GC5035_RST		<&gpf1 4 0x1>

/***** REAR3 WIDE - S5K4HA *****/
#define S5K4HA_IOVDD	<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8  */
#define S5K4HA_AVDD		<&s2mpb03_0_l5>		/* RCAM3_AVDD_2P8 */
#define S5K4HA_DVDD		<&s2mpb03_0_l4>		/* RCAM3_DVDD_1P2 */
#define S5K4HA_RST		<&gpa3 1 0x1>

/***** REAR3 WIDE - SR846 *****/
#define SR846_IOVDD		<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8  */
#define SR846_AVDD		<&s2mpb03_0_l5>		/* RCAM3_AVDD_2P8 */
#define SR846_DVDD		<&s2mpb03_0_l4>		/* RCAM3_DVDD_1P2 */
#define SR846_RST		<&gpa3 1 0x1>

/***** REAR4 MACRO - GC02M1 *****/
#define GC02M1_MACRO_IOVDD	<&s2mpb03_0_l3>	/* CAM_VDDIO_1P8 */ 
#define GC02M1_MACRO_AVDD	<&gpg2 6 0x1>		/* RCAM4_AVDD_2P8 */
#define GC02M1_MACRO_DVDD	<&gpg1 6 0x1>		/* RCAM4_DVDD_1P2 */
#define GC02M1_MACRO_RST	<&gpa3 3 0x1>


/***** ETC Define related to sensor power *****/

#define MIPI_SEL1	<&gpg2 5 0x1>				/* MIPI Select 1 : RCAM3 & FRONT  */
#define MIPI_SEL2	<&gpg2 7 0x1>				/* MIPI Select 2 : RCAM2 & RCAM4 */


#endif /* IS_VENDOR_SENSOR_PWR_AAS_V21S_H */
