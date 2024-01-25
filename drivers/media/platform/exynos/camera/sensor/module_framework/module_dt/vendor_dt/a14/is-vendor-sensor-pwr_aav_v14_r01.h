#ifndef IS_VENDOR_SENSOR_PWR_AAV_V14_H
#define IS_VENDOR_SENSOR_PWR_AAV_V14_H

/***** This file is used to define sensor power pin name for only AAV_V14 *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - JN1 *****/
#define RCAM_AF_VDD		<&l32_reg>		/* RCAM1_AFVDD_2P8 */
#define JN1_IOVDD		<&gpa3 5 0x1>		/* CAM_VDDIO_1P8 is used for all camera commonly */
#define JN1_AVDD		<&gpg2 6 0x1>		/* RCAM1_AVDD1_2P8 : RCAM1_LDO_EN*/
#define JN1_DVDD		<&l29_reg>		/* RCAM1_DVDD_1P05 : VLDO29 */
#define JN1_RST			<&gpa2 3 0x1>

/***** REAR2 UW - HI556 *****/
#define HI556_IOVDD		<&gpa3 5 0x1>		/* VDD_CAM_IO_1P8 - */
#define HI556_AVDD		<&gpg2 6 0x1>		/* AVDD*/
#define HI556_DVDD		<&gpa4 0 0x1>		/* VDD_RCAM_CORE_1P2 - FCAM_CORE_LDO_EN */
#define HI556_RST		<&gpa2 7 0x1>		/* RCAM2_RST_N */

/***** FRONT - HI1336 *****/
#define HI1336B_IOVDD	<&gpa3 5 0x1>		/* VDD_CAM_IO_1P8 - */
#define HI1336B_AVDD	<&gpg2 6 0x1>		/* FCAM_AVDD_2P8 - RCAM2_FCAM_AVDD_2P8_EN */
#define HI1336B_DVDD	<&l36_reg>		/* VDD_FCAM_DVDD_1P1 - VDD_FCAM_DVDD_1P1_EN */
#define HI1336B_RST		<&gpa2 4 0x1>		/* FCAM_RST_N */

/***** REAR4 MACRO - GC02M1 *****/
#define GC02M1_MACRO_NOT_USED

/***** REAR4 MACRO - GC02M2 *****/
#define GC02M2_MACRO_IOVDD	<&gpa3 5 0x1>		/* CAM_VDDIO_1P8 - RCAM_VDDIO_LDO_EN */
#define GC02M2_MACRO_LDO_EN	<&gpg2 6 0x1>		/* RCAM3_4_LDO_EN */
#define GC02M2_MACRO_AVDD	GC02M2_MACRO_LDO_EN	/* RCAM3_4_AVDD_2P8 - RCAM3_4_LDO_EN */
#define GC02M2_MACRO_DVDD	<>			/* Not used */
#define GC02M2_MACRO_RST	<&gpa3 1 0x1>

/***** DUALIZED REAR4 MACRO - SC201 *****/
#define SC201_MACRO_IOVDD	<&gpa3 5 0x1>		/* CAM_VDDIO_1P8 - RCAM_VDDIO_LDO_EN */
#define SC201_MACRO_LDO_EN	<&gpg2 6 0x1>		/* VLDO32 */
#define SC201_MACRO_AVDD	SC201_MACRO_LDO_EN	/* RCAM3_4_AVDD_2P8 - RCAM3_4_LDO_EN */
#define SC201_MACRO_DVDD	<>			/*Not used */
#define SC201_MACRO_RST		<&gpa3 1 0x1>

/***** ETC Define related to sensor power *****/
#define MIPI_SEL		<&gpg2 5 0x1>		/* MIPI Select  : RCAM2 & FCAM */

#define SC201_BOKEH_NOT_USED

#endif /* IS_VENDOR_SENSOR_PWR_AAV_V14_H */
