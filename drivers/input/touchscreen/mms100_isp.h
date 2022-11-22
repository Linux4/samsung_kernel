#ifndef _MMS100_ISP_H
#define _MMS100_ISP_H

//int mms_flash_fw(struct i2c_client *client, struct mms_platform_data *pdata);
int mms_flash_fw(struct i2c_client *client);
#if defined(CONFIG_MACH_HELANDELOS) || defined(CONFIG_MACH_ARUBA_TD) || defined(CONFIG_MACH_WARUBA) \
	|| defined(CONFIG_MACH_HARRISON) || defined(CONFIG_MACH_CS02) || defined(CONFIG_MACH_CS05)
int mms_flash_fw2(struct i2c_client *client,const char* fw_name);
int mms_fw_force_updated(struct i2c_client *client);
#endif

#endif

