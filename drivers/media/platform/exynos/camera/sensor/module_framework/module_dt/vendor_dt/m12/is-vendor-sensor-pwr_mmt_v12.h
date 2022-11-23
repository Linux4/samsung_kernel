#ifndef IS_VENDOR_SENSOR_PWR_MMT_V12_H
#define IS_VENDOR_SENSOR_PWR_MMT_V12_H

/***** This file is used to define sensor power pin name for only MMT_V12 *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - GM2 *****/
#define RCAM_AF_VDD		<&l32_reg>			/* RCAM1_AFVDD_2P8 */
#define GM2_IOVDD		<&gpa3 5 0x1>		/* CAM_VDDIO_1P8 is used for all camera commonly */
#define GM2_AVDD		<&gpa2 6 0x1>		/* RCAM1_AVDD1_2P8 : RCAM1_LDO_EN*/
#define GM2_DVDD		<&l29_reg>			/* RCAM1_DVDD_1P05 : VLDO29 */
#define GM2_RST			<&gpa2 3 0x1>

/***** FRONT - SR846 *****/
#define SR846_IOVDD		<&gpa3 5 0x1>		/* CAM_VDDIO_1P8 */
#define SR846_LDO_EN	<&gpa3 6 0x1>		/* FCAM_LDO_EN */
#define SR846_AVDD		SR846_LDO_EN		/* FRONT_AVDD_2P8 */
#define SR846_DVDD		SR846_LDO_EN		/* FCAM_DVDD_1P2 */
#define SR846_RST		<&gpa2 4 0x1>

/***** REAR2 SUB - GC02M1 *****/
#define GC02M1_IOVDD	<&gpa3 5 0x1>		/* CAM_VDDIO_1P8 */
#define GC02M1_LDO_EN	<&gpg2 6 0x1>		/* RCAM2_LDO_EN */
#define GC02M1_AVDD		GC02M1_LDO_EN		/* RCAM2_AVDD_2P8 - RCAM2_LDO_EN */
#define GC02M1_DVDD		GC02M1_LDO_EN		/* RCAM2_DVDD_1P2 - RCAM2_LDO_EN */
#define GC02M1_RST		<&gpa3 1 0x1>

/***** REAR3 WIDE - GC5035 *****/
#define GC5035_IOVDD	<&gpa3 5 0x1>		/* CAM_VDDIO_1P8 - RCAM_VDDIO_LDO_EN */
#define GC5035_LDO_EN	<&gpg3 0 0x1>		/* RCAM3_4_LDO_EN */
#define GC5035_AVDD		GC5035_LDO_EN		/* RCAM3_4_AVDD_2P8 - RCAM3_4_LDO_EN */
#define GC5035_DVDD		GC5035_LDO_EN		/* RCAM3_4_DVDD_1P8 - RCAM3_4_LDO_EN */
#define GC5035_RST		<&gpa2 7 0x1>

/***** REAR4 MACRO - GC02M1 *****/
#define GC02M1_MACRO_IOVDD	<&gpa3 5 0x1>			/* CAM_VDDIO_1P8 - RCAM_VDDIO_LDO_EN */
#define GC02M1_MACRO_LDO_EN	<&gpg2 6 0x1>			/* RCAM3_4_LDO_EN */
#define GC02M1_MACRO_AVDD	GC02M1_MACRO_LDO_EN		/* RCAM3_4_AVDD_2P8 - RCAM3_4_LDO_EN */
#define GC02M1_MACRO_DVDD	GC02M1_MACRO_LDO_EN		/* RCAM3_4_DVDD_1P8 - RCAM3_4_LDO_EN */
#define GC02M1_MACRO_RST	<&gpa3 3 0x1>


/***** ETC Define related to sensor power *****/

#define MIPI_SEL1	<&gpg2 5 0x1>				/* MIPI Select 1 : RCAM3 & FRONT */
#define MIPI_SEL2	<&gpg2 7 0x1>				/* MIPI Select 2 : RCAM2 & RCAM4 */


#endif /* IS_VENDOR_SENSOR_PWR_MMT_V12_H */
