#ifndef _MMS100_ISP_H
#define _MMS100_ISP_H

//int mms_flash_fw(struct i2c_client *client, struct mms_platform_data *pdata);
int mms_flash_fw(struct i2c_client *client);
#ifdef CONFIG_MACH_ARUBA_TD	
int mms_flash_fw2(struct i2c_client *client,const char* fw_name);
#endif

#endif

