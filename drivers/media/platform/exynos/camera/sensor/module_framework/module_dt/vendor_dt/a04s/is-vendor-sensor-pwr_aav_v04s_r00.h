#ifndef IS_VENDOR_SENSOR_PWR_AAV_V04S_H
#define IS_VENDOR_SENSOR_PWR_AAV_V04S_H

/***** This file is used to define sensor power pin name for only AAV_V04S *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - JN1 *****/
#define RCAM_AF_VDD		<&gpa2 6 0x1>		/* RCAM1_AFVDD_2P8 */
#define JN1_IOVDD		<&gpa1 6 0x1>		/* CAM_VDDIO_1P8 is used for all camera commonly */
#define JN1_AVDD		<&l32_reg>			/* RCAM1_AVDD1_2P8 : RCAM1_LDO_EN*/   //VLDO32
#define JN1_DVDD		<&l29_reg>			/* RCAM1_DVDD_1P05 : VLDO29 */
#define JN1_RST			<&gpa2 3 0x1>

/***** REAR2 SUB - SC201 *****/
#define SC201_IOVDD		<&gpa1 6 0x1>		/* CAM_VDDIO_1P8 */
#define SC201_LDO_EN	<&gpg2 6 0x1>		/* RCAM2_LDO_EN */
#define SC201_AVDD		SC201_LDO_EN		/* RCAM2_AVDD_2P8 - RCAM2_LDO_EN */
#define SC201_DVDD		<&gpg2 5 0x1>		/* RCAM2_DVDD_1P2 - RCAM2_LDO_EN */
#define SC201_RST		<&gpa2 7 0x1>

/***** DUALIZED FRONT - SC501 *****/
#define SC501_IOVDD	<&gpa1 6 0x1>	/* VDD_CAM_IO_1P8 - */
#define SC501_AVDD		<&l25_reg>	/* VDD_FCAM_SENSOR_A2P8 - FCAM_SENSOR_LDO_EN */
#define SC501_DVDD		<&l36_reg>	/* VDD_FCAM_CORE_1P2 - FCAM_CORE_LDO_EN */
#define SC501_RST		<&gpa2 4 0x1>	/* FCAM_RST_N */

/***** DUALIZED REAR2 SUB - GC02M1 *****/
#define GC02M1_IOVDD	<&gpa1 6 0x1>		/* CAM_VDDIO_1P8 */
#define GC02M1_LDO_EN	<&gpg2 6 0x1>		/* RCAM2_LDO_EN */
#define GC02M1_AVDD		GC02M1_LDO_EN		/* RCAM2_AVDD_2P8 - RCAM2_LDO_EN */
#define GC02M1_DVDD		<&gpg2 5 0x1>		/* RCAM2_DVDD_1P2 - RCAM2_LDO_EN */
#define GC02M1_RST		<&gpa2 7 0x1>

/***** FRONT - HI556 *****/
#define HI556_IOVDD	<&gpa1 6 0x1>	/* VDD_CAM_IO_1P8 - */
#define HI556_AVDD		<&l25_reg>		/* VDD_FCAM_SENSOR_A2P8 - FCAM_SENSOR_LDO_EN */
#define HI556_DVDD		<&l36_reg>		/* VDD_FCAM_CORE_1P2 - FCAM_CORE_LDO_EN */
#define HI556_RST		<&gpa2 4 0x1>	/* FCAM_RST_N */

/***** REAR4 MACRO - GC02M1 *****/
#define GC02M1_MACRO_NOT_USED

/***** REAR4 MACRO - GC02M2 *****/
#define GC02M2_MACRO_IOVDD	<&gpa1 6 0x1>			/* CAM_VDDIO_1P8 - RCAM_VDDIO_LDO_EN */
#define GC02M2_MACRO_LDO_EN	<&gpg2 6 0x1>			/* RCAM3_4_LDO_EN */
#define GC02M2_MACRO_AVDD	GC02M2_MACRO_LDO_EN		/* RCAM3_4_AVDD_2P8 - RCAM3_4_LDO_EN */
#define GC02M2_MACRO_DVDD	<&gpg2 5 0x1>			/* RCAM3_4_DVDD_1P8 - RCAM3_4_LDO_EN */
#define GC02M2_MACRO_RST	<&gpa3 1 0x1>

/***** DUALIZED REAR4 MACRO - SC201 *****/
#define SC201_MACRO_IOVDD	<&gpa1 6 0x1>			/* CAM_VDDIO_1P8 - RCAM_VDDIO_LDO_EN */
#define SC201_MACRO_LDO_EN	<&gpg2 6 0x1>			/* RCAM3_4_LDO_EN */
#define SC201_MACRO_AVDD	SC201_MACRO_LDO_EN		/* RCAM3_4_AVDD_2P8 - RCAM3_4_LDO_EN */
#define SC201_MACRO_DVDD	<&gpg2 5 0x1>			/* RCAM3_4_DVDD_1P8 - RCAM3_4_LDO_EN */
#define SC201_MACRO_RST		<&gpa3 1 0x1>

/***** ETC Define related to sensor power *****/
#define MIPI_SEL		<&gpg2 7 0x1>		/* MIPI Select  : RCAM2 & RCAM4 */

#endif /* IS_VENDOR_SENSOR_PWR_AAV_V04S_H */
