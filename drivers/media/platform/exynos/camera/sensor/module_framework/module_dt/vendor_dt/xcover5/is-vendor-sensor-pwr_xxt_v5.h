#ifndef IS_VENDOR_SENSOR_PWR_XXT_V5_H
#define IS_VENDOR_SENSOR_PWR_XXT_V5_H

/***** This file is used to define sensor power pin name for only XXT_V5 *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - S5K2P6 *****/
#define RCAM_AF_VDD		<&l32_reg>		/* RCAM_AFVDD_2P8 - VLDO32*/
#define S5K2P6_AVDD		<&gpg2 7 0x1>	/* RCAM_AVDD : RCAM_AVDD_LDO_EN - RCAM_LDO_EN*/
#define S5K2P6_DVDD		<&l29_reg>		/* RCAM_DVDD_1P05 - VLDO29*/
#define S5K2P6_IOVDD	<&gpg3 0 0x1>	/* CAM_VDDIO_1P8 - RCAM_VDDIO_LDO_EN */
#define S5K2P6_RST		<&gpa2 3 0x1>	/* RCAM_RST_N */

/***** FRONT - GC5035 *****/
#define GC5035_IOVDD	<&gpg3 0 0x1>	/* FCAM_IOVDD - RCAM_IOVDD */
#define GC5035_LDO_EN	<&gpg2 6 0x1>	/* FCAM_LDO_EN */
#define GC5035_AVDD		GC5035_LDO_EN	/* FCAM_AVDD_2P8 - FCAM_LDO_EN */
#define GC5035_DVDD		GC5035_LDO_EN	/* FCAM_DVDD_1P2 - FCAM_LDO_EN */
#define GC5035_RST		<&gpa2 4 0x1>	/* FCAM_RST_N */

#endif /* IS_VENDOR_SENSOR_PWR_XXT_V5_H */
