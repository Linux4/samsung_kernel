/*
 * MELFAS MIP4 (MCS6000) Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4_fw_mcs6000.c : Firmware update functions for MCS6000
 *
 */

#include "mip4.h"

//Info
#define ISP_I2C_ADDR			0x7D
#define ISP_PAGE_SIZE			64

//Command
#define ISP_CMD_ERASE			0x02
#define ISP_CMD_PROGRAM		0x03
#define ISP_CMD_READ			0x04
#define ISP_CMD_READ_INFO		0x06
#define ISP_CMD_RESET			0x07
#define ISP_CMD_TIMING		0x0F

//Status
#define ISP_STATUS_ENTER		0x55
#define ISP_STATUS_ERASE		0x82
#define ISP_STATUS_PROGRAM	0x83
#define ISP_STATUS_READ		0x84
#define ISP_STATUS_TIMING		0x8F

//Timing value
#define ISP_TIMING_ERASE		{0x01, 0xD4, 0xC0}
#define ISP_TIMING_PROGRAM	{0x00, 0x00, 0x78}

//Binary info
#define MIP_BIN_TAIL_MARK		{0x4D, 0x42, 0x54, 0x01}	// M B T 0x01
#define MIP_BIN_TAIL_SIZE		64

//Calibration data
#define CAL_DATA_ADDR			0xF400
#define CAL_DATA_SIZE			6
#define CAL_DATA_DEFAULT		{0xAA, 0x0A, 0x00, 0x00, 0x00, 0x0A}
#define CAL_DATA_PATH			"/sdcard/mcscal.bin"

#if 1
typedef struct {
	uint32_t reg;
	uint32_t val;
} panel_pinmap_t;
#endif

panel_pinmap_t panel_rstpin_map_HWic[] = {
	{REG_PIN_SCL1, BIT_PIN_SLP_AP|BIT_PIN_WPUS|BITS_PIN_DS(1)|BITS_PIN_AF(0)|BIT_PIN_WPU|BIT_PIN_SLP_WPU|BIT_PIN_SLP_Z},
	{REG_PIN_SDA1, BIT_PIN_SLP_AP|BIT_PIN_WPUS|BITS_PIN_DS(1)|BITS_PIN_AF(0)|BIT_PIN_WPU|BIT_PIN_SLP_WPU|BIT_PIN_SLP_Z},
};

panel_pinmap_t panel_rstpin_map_gpio[] = {
	{REG_PIN_SCL1, BIT_PIN_SLP_AP|BIT_PIN_WPUS|BITS_PIN_DS(1)|BITS_PIN_AF(3)|BIT_PIN_WPU|BIT_PIN_SLP_WPU|BIT_PIN_SLP_Z},
	{REG_PIN_SDA1, BIT_PIN_SLP_AP|BIT_PIN_WPUS|BITS_PIN_DS(1)|BITS_PIN_AF(3)|BIT_PIN_WPU|BIT_PIN_SLP_WPU|BIT_PIN_SLP_Z},
};


/**
* Set GPIO (INTR) level
*/
void mip_set_gpio_intr(struct mip_ts_info *info, int level)
{
	/////////////////////////////////
	// MODIFY REQUIRED
	//
	
	gpio_direction_output(info->pdata->gpio_intr, level);
	
	dev_dbg(&info->client->dev, "%s - Level[%d]\n", __func__, level);	

	//
	/////////////////////////////////
}

/**
* Set GPIO (SCL) level
*/
void mip_set_gpio_scl(struct mip_ts_info *info, int level)
{
	/////////////////////////////////
	// MODIFY REQUIRED
	//
	
	gpio_direction_output(info->pdata->gpio_scl, level);

	dev_dbg(&info->client->dev, "%s - Level[%d]\n", __func__, level);	

	//
	/////////////////////////////////
}

/**
* Set GPIO (SDA) level
*/
void mip_set_gpio_sda(struct mip_ts_info *info, int level)
{
	/////////////////////////////////
	// MODIFY REQUIRED
	//
	
	gpio_direction_output(info->pdata->gpio_sda, level);

	dev_dbg(&info->client->dev, "%s - Level[%d]\n", __func__, level);	

	//
	/////////////////////////////////
}

void mip_gpio_mode(struct mip_ts_info *info, bool mode)
{
	if(mode){	  ////Set GPIO mode 
		sci_glb_write(CTL_PIN_BASE + panel_rstpin_map_gpio[0].reg,	panel_rstpin_map_gpio[0].val, 0xffffffff);
		sci_glb_write(CTL_PIN_BASE + panel_rstpin_map_gpio[1].reg,	panel_rstpin_map_gpio[1].val, 0xffffffff);

	}
	else { //Set I2C mode
		sci_glb_write(CTL_PIN_BASE + panel_rstpin_map_HWic[0].reg,	panel_rstpin_map_HWic[0].val, 0xffffffff);
		sci_glb_write(CTL_PIN_BASE + panel_rstpin_map_HWic[1].reg,	panel_rstpin_map_HWic[1].val, 0xffffffff);
		gpio_free(info->pdata->gpio_sda);
		gpio_free(info->pdata->gpio_scl);
	}

}

/**
* ISP set timing value
*/
static int mip_isp_timing(struct mip_ts_info *info, u8 *value)
{
	int ret = 0;
	int retry = 3;
	int i;
	u8 rbuf[4];
	u8 wbuf[4];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);	

	//Write timing value
	wbuf[0] = ISP_CMD_TIMING;
	wbuf[1] = value[0];
	wbuf[2] = value[1];
	wbuf[3] = value[2];	
	
	for(i = 0; i < 4; i++){
		if(i2c_master_send(info->download_client, &wbuf[i], 1) != 1){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send[%d]\n", __func__, ret);
			goto ERROR;
		}		
		udelay(15);
	}

	udelay(500);
	
	//Check result
	retry = 3;
	while(retry--){
		if(i2c_master_recv(info->download_client, rbuf, 1) != 1){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_recv[%d]\n", __func__, ret);
		}
		
		dev_dbg(&info->client->dev, "%s - Status[0x%02X]\n", __func__, rbuf[0]);	
		
		if(rbuf[0] == ISP_STATUS_TIMING){
			break;
		}

		mdelay(1);
	}
	if(retry < 0){
		goto ERROR;
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return -1;
}

/**
* ISP erase command
*/
static int mip_isp_erase(struct mip_ts_info *info)
{
	int ret = 0;
	int retry = 3;
	u8 rbuf[4];
	u8 wbuf[4];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);	

	//Send erase command
	wbuf[0] = ISP_CMD_ERASE;
	if(i2c_master_send(info->download_client, wbuf, 1) != 1){
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_send[%d]\n", __func__, ret);
		goto ERROR;
	}	
	udelay(15);

	mdelay(100);
	
	//Check result
	retry = 5;
	while(retry--){
		if(i2c_master_recv(info->download_client, rbuf, 1) != 1){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_recv[%d]\n", __func__, ret);
		}
		
		dev_dbg(&info->client->dev, "%s - Status[0x%02X]\n", __func__, rbuf[0]);	
		
		if(rbuf[0] == ISP_STATUS_ERASE){
			break;
		}

		mdelay(5);
	}
	if(retry < 0){
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return -1;
}

/**
* ISP read command
*/
static int mip_isp_read(struct mip_ts_info *info, u8 *read_buf, u16 addr, u8 length)
{
	int ret = 0;
	int i;
	u8 rbuf[4];
	u8 wbuf[4];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);	

	//Send read command
	wbuf[0] = ISP_CMD_READ;
	wbuf[1] = (addr >> 8) & 0xFF;
	wbuf[2] = addr & 0xFF;
	wbuf[3] = length;
	
	for(i = 0; i < 4; i++){
		if(i2c_master_send(info->download_client, &wbuf[i], 1) != 1){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send[%d]\n", __func__, ret);
			goto ERROR;
		}		
		udelay(15);
	}

	//Check result
	if(i2c_master_recv(info->download_client, rbuf, 1) != 1){
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_recv[%d]\n", __func__, ret);
	}
	
	dev_dbg(&info->client->dev, "%s - Status[0x%02X]\n", __func__, rbuf[0]);	
	
	if(rbuf[0] != ISP_STATUS_READ){
		goto ERROR;
	}
	
	//Read data
	for(i = 0; i < length; i++){
		udelay(100);

		if(i2c_master_recv(info->download_client, &read_buf[i], 1) != 1){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_recv[%d]\n", __func__, ret);
			goto ERROR;
		}
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return -1;
}

/**
* ISP program command
*/
static int mip_isp_program(struct mip_ts_info *info, u8 *data, u16 addr, u8 length)
{
	int ret = 0;
	int i;
	u8 rbuf[4];
	u8 wbuf[4];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);	
	
	//Send program command
	wbuf[0] = ISP_CMD_PROGRAM;
	wbuf[1] = (addr >> 8) & 0xFF;
	wbuf[2] = addr & 0xFF;
	wbuf[3] = length;
	
	for(i = 0; i < 4; i++){
		if(i2c_master_send(info->download_client, &wbuf[i], 1) != 1){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send[%d]\n", __func__, ret);
			goto ERROR;
		}		
		udelay(15);
	}

	//Check result
	if(i2c_master_recv(info->download_client, rbuf, 1) != 1){
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_recv[%d]\n", __func__, ret);
		goto ERROR;
	}
	
	dev_dbg(&info->client->dev, "%s - Status[0x%02X]\n", __func__, rbuf[0]);	
	
	if(rbuf[0] != ISP_STATUS_PROGRAM){
		goto ERROR;
	}
	
	udelay(150);
	
	//Write data
	for(i = 0; i < length; i += 2){
		if(i2c_master_send(info->download_client, &data[i + 1], 1) != 1){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send[%d]\n", __func__, ret);
			goto ERROR;
		}
		udelay(100);
		
		if(i2c_master_send(info->download_client, &data[i], 1) != 1){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send[%d]\n", __func__, ret);
			goto ERROR;
		}
		udelay(150);
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return -1;
}

/**
* ISP enter command
*/
static int mip_isp_enter(struct mip_ts_info *info)
{
	int ret = 0;
	int retry = 5;
	int i;
	u8 rbuf[4];
	u8 cmd[14] = {0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);	

	//Send command
	//Set GPIO mode
	mip_gpio_mode(info, true);
	
	for(i = 0; i < 14; i++){
		if(cmd[i] == 1){
			mip_set_gpio_intr(info, 1);
		}
		else{
			mip_set_gpio_intr(info, 0);
		}
		mip_set_gpio_scl(info, 1);
		udelay(15);
		
		mip_set_gpio_scl(info, 0);
		mip_set_gpio_intr(info, 0);
		udelay(100);
	}

	mip_set_gpio_scl(info, 1);
	udelay(100);
	
	mip_set_gpio_intr(info, 1);
	mdelay(2);
	
	//Check result	
	while(retry--){
		//Set I2C mode
		mip_gpio_mode(info, false);

		if(i2c_master_recv(info->download_client, rbuf, 1) != 1){
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_recv[%d]\n", __func__, ret);
		}
		
		dev_dbg(&info->client->dev, "%s - Status[0x%02X]\n", __func__, rbuf[0]);	
		
		if(rbuf[0] == ISP_STATUS_ENTER){
			break;
		}

		mdelay(2);
	}
	if(retry < 0){
		goto ERROR;
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return -1;
}

/**
* ISP exit command
*/
static int mip_isp_exit(struct mip_ts_info *info)
{
	int ret = 0;
	u8 wbuf[4];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);	

	//Send command
	wbuf[0] = ISP_CMD_RESET;
	if(i2c_master_send(info->download_client, wbuf, 1) != 1){
		dev_err(&info->client->dev, "%s [ERROR] i2c_master_send[%d]\n", __func__, ret);
		goto ERROR;
	}		
	
	mdelay(200);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return -1;
}

/**
* Check calibration data
*/
static bool mip_verify_cal_data(struct mip_ts_info *info, u8 *data)
{
	bool ret = true;
	int count = 0;
	int i;
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if(data[0] != 0xAA){
		//header mismatch
		ret = false;
	}
	else if(((data[1] + data[2] + data[3] + data[4]) & 0xFF) != data[5]){
		//checksum error
		ret = false;
	}
	else{
		count = 0;
		for(i = 0; i < 6; i++){
			if(data[i] == 0xFF){
				count += 1;
			}
		}		
		if(count >= 6){
			//flash empty
			ret = false;
		}
	}

	dev_dbg(&info->client->dev, "%s - result[%d]\n", __func__, ret);
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}

/**
* Backup calibration data
*/
static int mip_backup_cal_data(struct mip_ts_info *info, u8 *data, char *file_path)
{
	struct file *fp; 
	mm_segment_t old_fs;
	size_t size = CAL_DATA_SIZE;
	loff_t pos = 0;
	int ret = 0;
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	old_fs = get_fs();
	set_fs(KERNEL_DS);	
	
	fp = filp_open(file_path, O_WRONLY | O_CREAT, 0644);
	if(IS_ERR(fp)){
		dev_err(&info->client->dev, "%s [ERROR] filp_open - path[%s]\n", __func__, file_path);
		goto ERROR;
	}
	
	ret = vfs_write(fp, data, size, &pos);
	if(ret != size){
		dev_err(&info->client->dev, "%s [ERROR] vfs_write - size[%d]\n", __func__, ret);
		goto ERROR;

	}
	
	filp_close(fp, current->files);
	
	set_fs(old_fs); 
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return 0;

ERROR:
	set_fs(old_fs); 

	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return -1;
}

/**
* Restore calibration data
*/
static int mip_restore_cal_data(struct mip_ts_info *info, u8 *data, char *file_path)
{
	struct file *fp; 
	mm_segment_t old_fs;
	size_t size;
	unsigned char *file_data;
	int ret = 0;
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	old_fs = get_fs();
	set_fs(KERNEL_DS);	
	
	fp = filp_open(file_path, O_RDONLY, S_IRUSR);
	if(IS_ERR(fp)){
		dev_err(&info->client->dev, "%s [ERROR] file_open - path[%s]\n", __func__, file_path);
		goto ERROR;
	}
	
 	size = fp->f_path.dentry->d_inode->i_size;
	if(size >= CAL_DATA_SIZE){
		file_data = kzalloc(size, GFP_KERNEL);
		ret = vfs_read(fp, (char __user *)file_data, size, &fp->f_pos);
		dev_dbg(&info->client->dev, "%s - path[%s] size[%u]\n", __func__, file_path, size);
		
		if(ret != size){
			dev_err(&info->client->dev, "%s [ERROR] vfs_read - size[%d] read[%d]\n", __func__, size, ret);
			goto ERROR;
		}
		else{
			memcpy(data, file_data, CAL_DATA_SIZE);
		}
		
		kfree(file_data);
	}
	else{
		dev_err(&info->client->dev, "%s [ERROR] size [%d]\n", __func__, size);
		goto ERROR;
	}

	filp_close(fp, current->files);
	
	set_fs(old_fs); 
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return 0;

ERROR:
	set_fs(old_fs); 

	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return -1;
}

/**
* Flash chip firmware (main function)
*/
int mip_flash_fw(struct mip_ts_info *info, const u8 *fw_data, size_t fw_size, bool force, bool section)
{
	struct i2c_client *client = info->client;	
	struct mip_bin_tail *bin_info;
	int ret = 0;
	int retry = 3;
	int i;
	u8 rbuf[ISP_PAGE_SIZE];
	u8 timing_erase[3] = ISP_TIMING_ERASE;
	u8 timing_program[3] = ISP_TIMING_PROGRAM;
	int offset = 0;
	int offset_start = 0;
	int bin_size = 0;
	u8 *bin_data;
	u16 tail_size = 0;
	u8 tail_mark[4] = MIP_BIN_TAIL_MARK;
	u16 ver_chip[MIP_FW_MAX_SECT_NUM];
	u8 cal_data[6];
	u8 cal_data_default[6] = CAL_DATA_DEFAULT;
	bool cal_backup = true;
	bool cal_reset = false;
	
	//Create I2C client for ISP
	//struct i2c_adapter *download_adapter = i2c_get_adapter(3);	// I2C Bus Number = 3	
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);
	struct i2c_client *download_client = i2c_new_dummy(adapter, ISP_I2C_ADDR);
	info->download_client = download_client;

	dev_dbg(&client->dev, "%s [START]\n", __func__);	

	//Check bin size
	if(fw_size > 61 * 1024){
		dev_err(&client->dev, "%s [ERROR] F/W size overflow : size[%d]\n", __func__, fw_size);
		ret = fw_err_file_type;
		goto ERROR;
	}
	
	//Check tail size
	tail_size = (fw_data[fw_size - 5] << 8) | fw_data[fw_size - 6];
	if(tail_size != MIP_BIN_TAIL_SIZE){
		dev_err(&client->dev, "%s [ERROR] wrong tail size [%d]\n", __func__, tail_size);
		ret = fw_err_file_type;
		goto ERROR;
	}
	
	//Check bin format	
	if(memcmp(&fw_data[fw_size - tail_size], tail_mark, 4)){
		dev_err(&client->dev, "%s [ERROR] wrong tail mark\n", __func__);
		ret = fw_err_file_type;
		goto ERROR;
	}

	//Read bin info
	bin_info = (struct mip_bin_tail *)&fw_data[fw_size - tail_size];

	dev_dbg(&client->dev, "%s - bin_info : bin_len[%d] hw_cat[0x%2X] date[%4X] time[%4X] tail_size[%d]\n", __func__, bin_info->bin_length, bin_info->hw_category, bin_info->build_date, bin_info->build_time, bin_info->tail_size);

#if MIP_FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MIP_DEVICE_NAME " Bin Info : ", DUMP_PREFIX_OFFSET, 16, 1, bin_info, tail_size, false);
#endif

	//Check chip code
	if(memcmp(bin_info->chip_name, CHIP_FW_CODE, 4)){
		dev_err(&client->dev, "%s [ERROR] F/W file is not for %s\n", __func__, CHIP_NAME);
		ret = fw_err_file_type;
		goto ERROR;
	}

	//Check F/W version
	dev_info(&client->dev, "%s - F/W file version [0x%04X]\n", __func__, bin_info->ver_app);
	if(force == true){
		//Force update
		dev_info(&client->dev, "%s - Skip chip firmware version check\n", __func__);
	}
	else{
		//Read firmware version from chip
		while(retry--){
			if(!mip_get_fw_version_u16(info, ver_chip)){
				break;
			}
			else{
				mip_reboot(info);
			}
		}
		if(retry < 0){
			dev_err(&client->dev, "%s [ERROR] Unknown chip firmware version\n", __func__);
		}
		else{
			dev_info(&client->dev, "%s - Chip firmware version [0x%04X]\n", __func__, ver_chip[2]);

			//Compare version
			if((ver_chip[0] == bin_info->ver_boot) && (ver_chip[1] == bin_info->ver_core) && (ver_chip[2] == bin_info->ver_app) && (ver_chip[3] == bin_info->ver_param)){
				dev_info(&client->dev, "%s - Chip firmware is already up-to-date\n", __func__);
				ret = fw_err_uptodate;
				goto ERROR;
			}
		}
	}
		
	//Read bin data
	bin_size = bin_info->bin_length;
	bin_data = kzalloc(sizeof(u8) * (bin_size), GFP_KERNEL);
	memcpy(bin_data, fw_data, bin_size);
	
	//Enter ISP mode
	dev_info(&client->dev,"%s - Enter\n", __func__);	
	retry = 3;
	while(retry--){
		//Set GPIO mode 
		mip_gpio_mode(info, true);

		//Set GPIO level
		mip_set_gpio_intr(info, 0);
		mip_set_gpio_scl(info, 0);

		//Reset chip
		mip_reboot(info);
		
		//Send command
		if(!mip_isp_enter(info)){
			break;
		}
	}
	if(retry < 0){
		dev_err(&client->dev,"%s [ERROR] mip_isp_enter\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	}

	//Set I2C mode
	mip_gpio_mode(info, false);
	//Read calibration data
	dev_info(&client->dev,"%s - Read calibration data\n", __func__);
	if(mip_isp_read(info, rbuf, CAL_DATA_ADDR, CAL_DATA_SIZE)){
		dev_err(&client->dev, "%s [ERROR] mip_isp_read : offset[0x%04X]\n", __func__, offset);
		ret = fw_err_download;
		goto ERROR;
	}
	dev_info(&client->dev, "%s - calibration data [0x%02X 0x%02X]\n", __func__, rbuf[0], rbuf[1]);

	//Check calibration data
	if(mip_verify_cal_data(info, rbuf)){
		cal_backup = true;
		cal_reset = false;
	}
	else{
		cal_backup = false;
		cal_reset = true;
	}
	//Backup calibration data
	if(cal_backup){
		memcpy(cal_data, rbuf, CAL_DATA_SIZE);
		//if(mip_backup_cal_data(info, cal_data, CAL_DATA_PATH)){
		//	dev_err(&client->dev, "%s [ERROR] mip_backup_cal_data : path[%s]\n", __func__, CAL_DATA_PATH);
		//	ret = fw_err_download;
		//	goto ERROR;
		//}
	}

	//Set timing for erase
	dev_dbg(&client->dev,"%s - Timing (Erase)\n", __func__);
	retry = 3;
	while(retry--){
		if(!mip_isp_timing(info, timing_erase)){
			break;
		}
	}
	if(retry < 0){
		dev_err(&client->dev,"%s [ERROR] mip_isp_timing\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	}

	mdelay(1);
		
	//Erase
	dev_dbg(&client->dev,"%s - Erase\n", __func__);
	retry = 3;
	while(retry--){
		if(!mip_isp_erase(info)){
			break;
		}
	}
	if(retry < 0){
		dev_err(&client->dev,"%s [ERROR] mip_isp_erase\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	}

	mdelay(1);
	
	//Verify erase
	dev_dbg(&client->dev,"%s - Verify (Erase)\n", __func__);
	retry = 3;
	while(retry--){
		if(!mip_isp_read(info, rbuf, 0, 16)){
			break;
		}
	}
	if(retry < 0){
		dev_err(&client->dev,"%s [ERROR] mip_isp_read\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	}

#if MIP_FW_UPDATE_DEBUG
	print_hex_dump(KERN_ERR, MIP_DEVICE_NAME " Verify Erase : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, 16, false);
#endif

	for(i = 0; i < 16; i++){
		if(rbuf[i] != 0xFF){
			dev_err(&client->dev, "%s [ERROR] Verify (erase) failed : data[0x%02X]\n", __func__, rbuf[i]);
			ret = fw_err_download;
			goto ERROR;
		}
	}
	
	mdelay(5);

	//Set timing for program
	dev_dbg(&client->dev, "%s - Timing (Program)\n", __func__);
	retry = 3;
	while(retry--){
		if(!mip_isp_timing(info, timing_program)){
			break;
		}
	}
	if(retry < 0){
		dev_err(&client->dev, "%s [ERROR] mip_isp_timing\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	}

	mdelay(1);

	//Prepare calibration data
	if((cal_backup == true) || (cal_reset == true)){	
		//if(mip_restore_cal_data(info, cal_data, CAL_DATA_PATH)){
		//	dev_err(&client->dev, "%s [ERROR] mip_restore_cal_data\n", __func__);
		//	cal_reset = true;
		//}
		//else{
			if(mip_verify_cal_data(info, cal_data))
				cal_reset = false;
			else
				cal_reset = true;
		//}
	}
	if(cal_reset){
		memcpy(cal_data, cal_data_default, CAL_DATA_SIZE);
	}

	//Program calibration data	
	dev_dbg(&client->dev, "%s - Program calibration data\n", __func__);
	retry = 3;
	while(retry--){
		if(!mip_isp_program(info, cal_data, CAL_DATA_ADDR, CAL_DATA_SIZE)){
			break;
		}
	}	
	if(retry < 0){
		dev_err(&client->dev, "%s [ERROR] mip_isp_program\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	}
	
	mdelay(1);
	
	//Verify calibration data
	dev_dbg(&client->dev,"%s - Verify calibration data\n", __func__);
	if(mip_isp_read(info, rbuf, CAL_DATA_ADDR, CAL_DATA_SIZE)){
		dev_err(&client->dev, "%s [ERROR] mip_isp_read : offset[0x%04X]\n", __func__, offset);
		ret = fw_err_download;
		goto ERROR;
	}
	if(memcmp(cal_data, rbuf, CAL_DATA_SIZE)){
		dev_err(&client->dev, "%s [ERROR] Verify (calibration data) failed : offset[0x%04X]\n", __func__, offset);
		ret = fw_err_download;
		goto ERROR; 		
	}

	mdelay(1);
	//Program & Verify
	dev_info(&client->dev, "%s - Program & Verify\n", __func__);
 
	offset_start = 0;
	
	for(offset = offset_start; offset < bin_size; offset += ISP_PAGE_SIZE){
		//Program
		if(mip_isp_program(info, &bin_data[offset], offset, ISP_PAGE_SIZE)){
			dev_err(&client->dev, "%s [ERROR] mip_isp_program : offset[0x%04X]\n", __func__, offset);
			ret = fw_err_download;
			goto ERROR;
		}
		dev_dbg(&client->dev, "%s - mip_isp_program : offset[0x%04X]\n", __func__, offset);
		udelay(500);

		//Verify
		if(mip_isp_read(info, rbuf, offset, ISP_PAGE_SIZE)){
			dev_err(&client->dev, "%s [ERROR] mip_isp_read : offset[0x%04X]\n", __func__, offset);
			ret = fw_err_download;
			goto ERROR;
		}
		dev_dbg(&client->dev, "%s - mip_isp_read : offset[0x%04X]\n", __func__, offset);

#if MIP_FW_UPDATE_DEBUG
		print_hex_dump(KERN_ERR, MIP_DEVICE_NAME " F/W File : ", DUMP_PREFIX_OFFSET, 16, 1, &bin_data[offset], ISP_PAGE_SIZE, false);
		print_hex_dump(KERN_ERR, MIP_DEVICE_NAME " F/W Chip : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, ISP_PAGE_SIZE, false);
#endif

		if(memcmp(rbuf, &bin_data[offset], ISP_PAGE_SIZE)){
			dev_err(&client->dev, "%s [ERROR] Verify (program) failed : offset[0x%04X]\n", __func__, offset);
			ret = fw_err_download;
			goto ERROR; 		
		}
		
		udelay(500);		
	}

	
	//Exit ISP mode
	dev_info(&client->dev, "%s - Exit\n", __func__);
	mip_isp_exit(info);

	//Reset chip
	mip_reboot(info);
	
	//Check chip firmware version
	if(mip_get_fw_version_u16(info, ver_chip)){
		dev_err(&client->dev, "%s [ERROR] Unknown chip firmware version\n", __func__);
		ret = fw_err_download;
		goto ERROR;
	} 
	else{
		if(ver_chip[2] != bin_info->ver_app){
			dev_err(&client->dev, "%s [ERROR] Version mismatch after flash. Chip[0x%04X] File[0x%04X]\n", __func__, ver_chip[2], bin_info->ver_app);
			ret = fw_err_download;
			goto ERROR;
		}
	}
		
	goto EXIT;
	
ERROR:
	//Reset chip
	mip_reboot(info);

	dev_err(&client->dev, "%s [ERROR]\n", __func__);

EXIT:	
	//Clear I2C client
	i2c_unregister_device(download_client);
	info->download_client = NULL;
	
	//Set I2C mode
	mip_gpio_mode(info, false);

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);	

	return ret;	
}

