#ifndef IS_VENDOR_SENSOR_PWR_AAU_V12S_H
#define IS_VENDOR_SENSOR_PWR_AAU_V12S_H

/***** This file is used to define sensor power pin name for only AAU_V12S *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - JN1 *****/
#define RCAM_AF_VDD		<&s2mpb03_0_l7>		/* RCAM1_AFVDD_2P8 */
#define JN1_IOVDD		<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 is used for all camera commonly */
#define JN1_AVDD		<&s2mpb03_0_l5>		/* RCAM1_AVDD1_2P8 : RCAM1_LDO_EN*/
#define JN1_DVDD		<&s2mpb03_0_l1>		/* RCAM1_DVDD_1P05 : VLDO29 */
#define JN1_RST			<&gpa2 3 0x1>

/***** FRONT - GC08A4 *****/
#define GC08A3_IOVDD		<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 */
#define GC08A3_AVDD		<&s2mpb03_0_l6>		/* RCAM2_AVDD_2P8 - RCAM2_LDO_EN */
#define GC08A3_DVDD		<&s2mpb03_0_l2>		/* RCAM2_AVDD_2P8 - RCAM2_LDO_EN */
#define GC08A3_RST		<&gpa2 4 0x1>
#define GC08A3_MCLK		<&gpc0 1 0x1>

/***** REAR2 SUB - GC02M1 *****/
#define GC02M1_IOVDD		<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 */
#define GC02M1_LDO_EN		<&gpg2 6 0x1>		/* RCAM2_LDO_EN */
#define GC02M1_AVDD		<&l32_reg>		/* RCAM2_AVDD_2P8 - RCAM2_LDO_EN */
#define GC02M1_DVDD		GC02M1_LDO_EN		/* RCAM2_DVDD_1P2 - RCAM2_LDO_EN */
#define GC02M1_RST		<&gpa3 1 0x1>

/***** REAR3 WIDE - GC5035 *****/
#define GC5035_IOVDD		<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 - RCAM_VDDIO_LDO_EN */
#define GC5035_AVDD		<&l25_reg>		/* RCAM3_4_AVDD_2P8 - RCAM3_4_LDO_EN */
#define GC5035_DVDD		<&s2mpb03_0_l4>		/* RCAM3_4_DVDD_1P8 - RCAM3_4_LDO_EN */
#define GC5035_RST		<&gpa2 7 0x1>

/***** REAR4 MACRO - GC02M1 *****/
#define GC02M1_MACRO_IOVDD	<&s2mpb03_0_l3>		/* CAM_VDDIO_1P8 - RCAM_VDDIO_LDO_EN */
#define GC02M1_MACRO_LDO_EN	<&gpg2 6 0x1>		/* RCAM3_4_LDO_EN */
#define GC02M1_MACRO_AVDD	<&l32_reg>		/* RCAM3_4_AVDD_2P8 - RCAM3_4_LDO_EN */
#define GC02M1_MACRO_DVDD	GC02M1_MACRO_LDO_EN	/* RCAM3_4_DVDD_1P8 - RCAM3_4_LDO_EN */
#define GC02M1_MACRO_RST	<&gpa3 3 0x1>

/***** ETC Define related to sensor power *****/

#define MIPI_SEL1		<&gpg2 5 0x1>		/* MIPI Select 1 : RCAM3 & FRONT */
#define MIPI_SEL2		<&gpg2 7 0x1>		/* MIPI Select 2 : RCAM2 & RCAM4 */

#endif /* IS_VENDOR_SENSOR_PWR_AAU_V12S_H */
