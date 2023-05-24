#ifndef __INTERNAL__SEC_QC_PARAM_H__
#define __INTERNAL__SEC_QC_PARAM_H__

#include <linux/samsung/bsp/sec_sysup.h>

struct sec_qc_param_data {
	unsigned int debuglevel;
	unsigned int uartsel;
	unsigned int rory_control;
	unsigned int product_device;   /* product/dev device */
	unsigned int reserved1;
	unsigned int cp_debuglevel;
	unsigned int reserved2;
	unsigned int sapa[3];
	unsigned int normal_poweroff;
	unsigned int wireless_ic;
	char used0[80];
	char used1[80];
	char used2[80];
	char used3[80];
	char used4[80];
	unsigned int wireless_charging_mode;
	unsigned int afc_disable;
	unsigned int cp_reserved_mem;
	char used5[4];
	char used6[4];
	char reserved8[8];
	char used7[16];
	unsigned int api_gpio_test;
	char api_gpio_test_result[256];
	char reboot_recovery_cause[256];
	unsigned int user_partition_flashed;
	unsigned int force_upload_flag;
	unsigned int cp_reserved_mem_backup;
	unsigned int FMM_lock;
	unsigned int dump_sink;
	unsigned int fiemap_update;
	struct fiemap_p fiemap_result;
	char used8[80];
	char window_color[2];
	char VrrStatus[16];
	unsigned int pd_disable;
};

#endif /* __INTERNAL__SEC_QC_PARAM_H__ */
