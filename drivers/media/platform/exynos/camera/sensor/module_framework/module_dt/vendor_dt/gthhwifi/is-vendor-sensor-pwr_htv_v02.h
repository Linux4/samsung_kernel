#ifndef IS_VENDOR_SENSOR_PWR_HTV_V02_H
#define IS_VENDOR_SENSOR_PWR_HTV_V02_H

/***** This file is used to define sensor power pin name for only XXT_V5 *****/

#define USE_VENDOR_PWR_PIN_NAME

/***** REAR MAIN - IMX355 *****/
#define RCAM_AF_VDD		<&gpg2 0 0x1>	/* VDD_RCAM_AF_2P8 - RCAM_AF_LDO_EN*/
#define IMX355_AVDD		<&gpg2 5 0x1>	/* VDD_RCAM_SENSOR_A2P8 - RCAM_SENSOR_LDO_EN*/
#define IMX355_DVDD		<&gpg1 7 0x1>	/* VDD_RCAM_DVDD_1P2 - RCAM_CORE_LDO_EN*/
#define IMX355_IOVDD    <&l26_reg>    /* VDD_CAM_IO_1P8 - CAM_IO_LDO_EN - VLDO26 */
#define IMX355_RST		<&gpg2 2 0x1>	/* RCAM_RST_N */

/***** FRONT - GC5035 *****/
#define GC5035_IOVDD	<&l26_reg>	/* VDD_CAM_IO_1P8 - CAM_IO_LDO_EN - VLDO26*/
#define GC5035_AVDD		<&gpg3 0 0x1>	/* VDD_FCAM_SENSOR_A2P8 - FCAM_SENSOR_LDO_EN */
#define GC5035_DVDD		<&gpg2 6 0x1>	/* VDD_FCAM_CORE_1P2 - FCAM_CORE_LDO_EN */
#define GC5035_RST		<&gpg2 3 0x1>	/* FCAM_RST_N */

#endif /* IS_VENDOR_SENSOR_PWR_HTV_V02_H */
