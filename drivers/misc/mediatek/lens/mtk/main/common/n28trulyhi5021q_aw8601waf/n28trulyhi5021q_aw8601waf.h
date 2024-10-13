#ifndef _ST_HI5021Q_AW8601WAF_H_
#define _ST_HI5021Q_AW8601WAF_H_

#define AF_DRVNAME "N28TRULYHI5021Q_AW8601WAF_DRV"

/* Log Format */
#define AWLOGI(format, ...) \
	pr_info("[%s][%04d]%s: " format "\n", AF_DRVNAME, __LINE__, __func__, \
								##__VA_ARGS__)
#define AWLOGD(format, ...) \
	pr_debug("[%s][%04d]%s: " format "\n", AF_DRVNAME, __LINE__, __func__, \
								##__VA_ARGS__)
#define AWLOGE(format, ...) \
	pr_err("[%s][%04d]%s: " format "\n", AF_DRVNAME, __LINE__, __func__, \
								##__VA_ARGS__)

/* Maximum limit position */
#define AW_LIMITPOS_MAX		(1023)
#define AW_LIMITPOS_MIN		(0)
#define AW_EOORHANDLE_LOOP	(5)
#define AW_I2C_READ_MSG_NUM	(2)

#define AW_SUCCESS		(0)
#define AW_ERROR		(-1)

/* Register */
#define AW_REG_VCM_MSB		(0x03)
#define AW_REG_VCM_LSB		(0x04)

#endif

