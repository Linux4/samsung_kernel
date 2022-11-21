/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4_debug.c : Debug functions (Optional)
 *
 */
 
#include "mip4.h"

#if MIP_USE_DEV
#define USE_CTS_NODE 1
/**
* Dev node output to user
*/
static ssize_t mip_dev_fs_read(struct file *fp, char *rbuf, size_t cnt, loff_t *fpos)
{
	struct mip_ts_info *info = fp->private_data;
	int ret = 0;

	//dev_dbg(&info->client->dev, "%s [START]\n", __func__); 

	ret = copy_to_user(rbuf, info->dev_fs_buf, cnt);
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__); 

	return ret;
}

/**
* Dev node input from user
*/
static ssize_t mip_dev_fs_write(struct file *fp, const char *wbuf, size_t cnt, loff_t *fpos)
{
	struct mip_ts_info *info = fp->private_data;
	u8 *buf;
	int ret = 0;
	int cmd = 0;
	
	//dev_dbg(&info->client->dev, "%s [START]\n", __func__); 

	buf = kzalloc(cnt + 1, GFP_KERNEL);

	if ((buf == NULL) || copy_from_user(buf, wbuf, cnt)) {
		dev_err(&info->client->dev, "%s [ERROR] copy_from_user\n", __func__);
		ret = -EIO;
		goto EXIT;
	}

	cmd = buf[cnt - 1];

	if(cmd == 1){
		//dev_dbg(&info->client->dev, "%s - cmd[%d] w_len[%d] r_len[%d]\n", __func__, cmd, (cnt - 2), buf[cnt - 2]); 
		
		if(mip_i2c_read(info, buf, (cnt - 2), info->dev_fs_buf, buf[cnt - 2])){
			dev_err(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
		}		
		//print_hex_dump(KERN_ERR, MIP_DEVICE_NAME" : input ", DUMP_PREFIX_OFFSET, 16, 1, wbuf, cnt, false);
		//print_hex_dump(KERN_ERR, MIP_DEVICE_NAME" : output ", DUMP_PREFIX_OFFSET, 16, 1, info->dev_fs_buf, buf[cnt - 2], false);
	}
	else if(cmd == 2){
		//dev_dbg(&info->client->dev, "%s - cmd[%d] w_len[%d]\n", __func__, cmd, (cnt - 1)); 
		if(mip_i2c_write(info, buf, (cnt - 1))){
			dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
		}
		//print_hex_dump(KERN_ERR, MIP_DEVICE_NAME" : input ", DUMP_PREFIX_OFFSET, 16, 1, wbuf, cnt, false);
	}
	else{
		goto EXIT;
	}
	
EXIT:	
	kfree(buf);

	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__); 

	return ret;
}

/**
* Open dev node
*/
static int mip_dev_fs_open(struct inode *node, struct file *fp)
{
	struct mip_ts_info *info = container_of(node->i_cdev, struct mip_ts_info, cdev);

	//dev_dbg(&info->client->dev, "%s [START]\n", __func__); 
	
	fp->private_data = info;
	
	info->dev_fs_buf = kzalloc(1024 * 4, GFP_KERNEL);
	
	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__); 

	return 0;
}

/**
* Close dev node
*/
static int mip_dev_fs_release(struct inode *node, struct file *fp)
{
	struct mip_ts_info *info = fp->private_data;

	//dev_dbg(&info->client->dev, "%s [START]\n", __func__); 

	kfree(info->dev_fs_buf);

	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__); 

	return 0;
}

/**
* Dev node info
*/
static struct file_operations mip_dev_fops = {
	.owner	= THIS_MODULE,
	.open	= mip_dev_fs_open,
	.release	= mip_dev_fs_release,
	.read	= mip_dev_fs_read,
	.write	= mip_dev_fs_write,
};

/**
* Create dev node
*/
int mip_dev_create(struct mip_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0;
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__); 
	
	if (alloc_chrdev_region(&info->mip_dev, 0, 1, MIP_DEVICE_NAME)) {
		dev_err(&client->dev, "%s [ERROR] alloc_chrdev_region\n", __func__);
		ret = -ENOMEM;
		goto ERROR;
	}
	
	cdev_init(&info->cdev, &mip_dev_fops);
	info->cdev.owner = THIS_MODULE;

	if (cdev_add(&info->cdev, info->mip_dev, 1)) {
		dev_err(&client->dev, "%s [ERROR] cdev_add\n", __func__);
		ret = -EIO;
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__); 	
	return 0;
	
ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__); 	
	return 0;
}

#endif

#if MIP_USE_SYS || MIP_USE_CMD

/**
* Process table data
*/
static int mip_proc_table_data(struct mip_ts_info *info, u8 size, u8 data_type_size, u8 data_type_sign, u8 buf_addr_h, u8 buf_addr_l, u8 row_num, u8 col_num, u8 buf_col_num, u8 rotate, u8 key_num)
{
	char data[10];
	int i_col, i_row;
	int i_x, i_y;
	int lim_x, lim_y;
	int lim_col, lim_row;
	int max_x = 0;
	int max_y = 0;
	bool flip_x = false;
	int sValue = 0;
	unsigned int uValue = 0;
	int value = 0;
	u8 wbuf[8];
	u8 rbuf[512];
	unsigned int buf_addr;
	int offset;
	int data_size = data_type_size;
	int data_sign = data_type_sign;
	int has_key = 0;
	int size_screen = col_num * row_num;

	memset(data, 0, 10);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__); 

	//set axis
	if(rotate == 0){
		max_x = col_num;
		max_y = row_num;
		if(key_num > 0){
			max_y += 1;
			has_key = 1;
		}
		flip_x = false;
	}
	else if(rotate == 1){
		max_x = row_num;
		max_y = col_num;
		if(key_num > 0){
			max_y += 1;
			has_key = 1;
		}
		flip_x = true;
	}
	else{
		dev_err(&info->client->dev, "%s [ERROR] rotate [%d]\n", __func__, rotate); 
		goto ERROR;
	}

	//get table data
	lim_row = row_num + has_key;
	for(i_row = 0; i_row < lim_row; i_row++){
		//get line data
		offset = buf_col_num * data_type_size;
		size = col_num * data_type_size;
		
		buf_addr = (buf_addr_h << 8) | buf_addr_l | (offset * i_row);
		wbuf[0] = (buf_addr >> 8) & 0xFF;
		wbuf[1] = buf_addr & 0xFF;
		if(mip_i2c_read(info, wbuf, 2, rbuf, size)){
			dev_err(&info->client->dev, "%s [ERROR] Read data buffer\n", __func__); 
			goto ERROR;
		}

		//save data
		if((key_num > 0) && (i_row == (lim_row - 1))){
			lim_col = key_num;
		}
		else{
			lim_col = col_num;
		}
		for(i_col = 0; i_col < lim_col; i_col++){
			if(data_sign == 0){
				//unsigned
				if(data_size == 1){
					uValue = (u8)rbuf[i_col];
				}
				else if(data_size == 2){
					uValue = (u16)(rbuf[data_size * i_col] | (rbuf[data_size * i_col + 1] << 8));
				}
				else if(data_size == 4){
					uValue = (u32)(rbuf[data_size * i_col] | (rbuf[data_size * i_col + 1] << 8) | (rbuf[data_size * i_col + 2] << 16) | (rbuf[data_size * i_col + 3] << 24));
				}
				else{
					dev_err(&info->client->dev, "%s [ERROR] data_size [%d]\n", __func__, data_size); 
					goto ERROR;
				}				
				value = (int)uValue;
			}			
			else{
				//signed
				if(data_size == 1){
					sValue = (s8)rbuf[i_col];
				}
				else if(data_size == 2){
					sValue = (s16)(rbuf[data_size * i_col] | (rbuf[data_size * i_col + 1] << 8));
				}
				else if(data_size == 4){
					sValue = (s32)(rbuf[data_size * i_col] | (rbuf[data_size * i_col + 1] << 8) | (rbuf[data_size * i_col + 2] << 16) | (rbuf[data_size * i_col + 3] << 24));
				}
				else{
					dev_err(&info->client->dev, "%s [ERROR] data_size [%d]\n", __func__, data_size); 
					goto ERROR;
				}
				value = (int)sValue;
			}

			switch(rotate){
				case 0:
					info->image_buf[i_row * col_num + i_col] = value; 
					break;
				case 1:
					if((key_num > 0) && (i_row == (lim_row - 1))){
						info->image_buf[size_screen + i_col] = value; 
					}
					else{
						info->image_buf[i_col * row_num + (row_num - 1 - i_row)] = value; 
					}
					break;
				default:
					dev_err(&info->client->dev, "%s [ERROR] rotate [%d]\n", __func__, rotate); 
					goto ERROR;
					break;
			}
		}
	}	
	
	//print table header
	printk("    ");
	sprintf(data, "    ");
	strcat(info->print_buf, data);
	memset(data, 0, 10);	
	
	switch(data_size){
		case 1:
			for(i_x = 0; i_x < max_x; i_x++){ 	
				printk("[%2d]", i_x);
				sprintf(data, "[%2d]", i_x);
				strcat(info->print_buf, data);
				memset(data, 0, 10);
			}			
			break;
		case 2:
			for(i_x = 0; i_x < max_x; i_x++){ 	
				printk("[%4d]", i_x);
				sprintf(data, "[%4d]", i_x);
				strcat(info->print_buf, data);
				memset(data, 0, 10);
			}
			break;
		case 4:
			for(i_x = 0; i_x < max_x; i_x++){ 	
				printk("[%5d]", i_x);
				sprintf(data, "[%5d]", i_x);
				strcat(info->print_buf, data);
				memset(data, 0, 10);
			}
			break;
		default:
			dev_err(&info->client->dev, "%s [ERROR] data_size [%d]\n", __func__, data_size); 
			goto ERROR;
			break;
	}
	
	printk("\n");
	sprintf(data, "\n");
	strcat(info->print_buf, data);
	memset(data, 0, 10);	
	
	//print table
	lim_y = max_y;
	for(i_y = 0; i_y < lim_y; i_y++){
		//print line header
		if((key_num > 0) && (i_y == (lim_y -1))){
			printk("[TK]");
			sprintf(data, "[TK]");
		}
		else{
			printk("[%2d]", i_y);
			sprintf(data, "[%2d]", i_y);
		}
		strcat(info->print_buf, data);
		memset(data, 0, 10);
		
		//print line
		if((key_num > 0) && (i_y == (lim_y - 1))){
			lim_x = key_num;
		}
		else{
			lim_x = max_x;
		}
		for(i_x = 0; i_x < lim_x; i_x++){				
			switch(data_size){
				case 1:
					printk(" %3d", info->image_buf[i_y * max_x + i_x]);
					sprintf(data, " %3d", info->image_buf[i_y * max_x + i_x]);
					break;
				case 2:
					printk(" %5d", info->image_buf[i_y * max_x + i_x]);
					sprintf(data, " %5d", info->image_buf[i_y * max_x + i_x]);
					break;
				case 4:
					printk(" %6d", info->image_buf[i_y * max_x + i_x]);
					sprintf(data, " %6u", info->image_buf[i_y * max_x + i_x]);
					break;
				default:
					dev_err(&info->client->dev, "%s [ERROR] data_size [%d]\n", __func__, data_size); 
					goto ERROR;
					break;
			}				
			
			strcat(info->print_buf, data);
			memset(data, 0, 10);
		}
		
		printk("\n");
		sprintf(data, "\n");
		strcat(info->print_buf, data);
		memset(data, 0, 10);		
	}
	
	printk("\n");
	sprintf(data, "\n");
	strcat(info->print_buf, data);
	memset(data, 0, 10);		
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__); 
	return 0;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__); 
	return 1;
}

/**
* Run test
*/
int mip_run_test(struct mip_ts_info *info, u8 test_type)
{
	int busy_cnt = 50;
	int wait_cnt = 50;
	u8 wbuf[8];
	u8 rbuf[512];
	u8 size = 0;
	u8 row_num;
	u8 col_num;
	u8 buffer_col_num;
	u8 rotate;
	u8 key_num;
	u8 data_type;
	u8 data_type_size;
	u8 data_type_sign;
	u8 buf_addr_h;
	u8 buf_addr_l;
	int ret = 0;
		
	dev_dbg(&info->client->dev, "%s [START]\n", __func__); 
	dev_dbg(&info->client->dev, "%s - test_type[%d]\n", __func__, test_type);	

	while(busy_cnt--){
		if(info->test_busy == false){
			break;
		}
		msleep(10);
	}
	mutex_lock(&info->lock);
	info->test_busy = true;
	mutex_unlock(&info->lock);

	memset(info->print_buf, 0, PAGE_SIZE);

	disable_irq(info->client->irq);
	mip_clear_input(info);

	//disable touch event
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MIP_CTRL_TRIGGER_NONE;
	if (mip_i2c_write(info, wbuf, 3)) {
		dev_err(&info->client->dev, "%s [ERROR] Disable event\n", __func__);
		goto ERROR;
	}

	//check test type
	switch(test_type){
		case MIP_TEST_TYPE_CM_DELTA:
			dev_dbg(&info->client->dev, "=== Cm Delta Test ===\n");
			sprintf(info->print_buf, "\n=== Cm Delta Test ===\n\n");
			break;
		case MIP_TEST_TYPE_CM_ABS:
			dev_dbg(&info->client->dev, "=== Cm Abs Test ===\n"); 
			sprintf(info->print_buf, "\n=== Cm Abs Test ===\n\n");
			break;
		case MIP_TEST_TYPE_CM_JITTER:
			dev_dbg(&info->client->dev, "=== Cm Jitter Test ===\n"); 
			sprintf(info->print_buf, "\n=== Cm Jitter Test ===\n\n");
			break;
		case MIP_TEST_TYPE_SHORT:
			dev_dbg(&info->client->dev, "=== Short Test ===\n"); 
			sprintf(info->print_buf, "\n=== Short Test ===\n\n");
			break;
		default:
			dev_err(&info->client->dev, "%s [ERROR] Unknown test type\n", __func__); 
			sprintf(info->print_buf, "\nERROR : Unknown test type\n\n");
			goto ERROR;
			break;
	}

	//set test mode
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_MODE;
	wbuf[2] = MIP_CTRL_MODE_TEST_CM;
	if(mip_i2c_write(info, wbuf, 3)){
		dev_err(&info->client->dev, "%s [ERROR] Write test mode\n", __func__); 
		goto ERROR;
	}

	//wait ready status
	wait_cnt = 50;
	while(wait_cnt--){
		if(mip_get_ready_status(info) == MIP_CTRL_STATUS_READY){
			break;
		}
		msleep(50);
		
		dev_dbg(&info->client->dev, "%s - wait [%d]\n", __func__, wait_cnt); 
	}
	
	if(wait_cnt <= 0){
		dev_err(&info->client->dev, "%s [ERROR] Wait timeout\n", __func__); 
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s - set control mode\n", __func__);	
	
	//set test type
	wbuf[0] = MIP_R0_TEST;
	wbuf[1] = MIP_R1_TEST_TYPE;
	wbuf[2] = test_type;
	if(mip_i2c_write(info, wbuf, 3)){
		dev_err(&info->client->dev, "%s [ERROR] Write test type\n", __func__); 
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s - set test type\n", __func__);	

	//wait ready status
	wait_cnt = 50;
	while(wait_cnt--){
		if(mip_get_ready_status(info) == MIP_CTRL_STATUS_READY){
			break;
		}
		msleep(10);
		
		dev_dbg(&info->client->dev, "%s - wait [%d]\n", __func__, wait_cnt); 
	}
	
	if(wait_cnt <= 0){
		dev_err(&info->client->dev, "%s [ERROR] Wait timeout\n", __func__); 
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s - ready\n", __func__);	
	
	//data format
	wbuf[0] = MIP_R0_TEST;
	wbuf[1] = MIP_R1_TEST_DATA_FORMAT;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 6)){
		dev_err(&info->client->dev, "%s [ERROR] Read data format\n", __func__); 
		goto ERROR;
	}
	row_num = rbuf[0];
	col_num = rbuf[1];
	buffer_col_num = rbuf[2];
	rotate = rbuf[3];
	key_num = rbuf[4];
	data_type = rbuf[5];
	
	data_type_sign = (data_type & 0x80) >> 7;
	data_type_size = data_type & 0x7F;	
	
	dev_dbg(&info->client->dev, "%s - row_num[%d] col_num[%d] buffer_col_num[%d] rotate[%d] key_num[%d]\n", __func__, row_num, col_num, buffer_col_num, rotate, key_num);	
	dev_dbg(&info->client->dev, "%s - data_type[0x%02X] data_sign[%d] data_size[%d]\n", __func__, data_type, data_type_sign, data_type_size);	
	
	//get buf addr
	wbuf[0] = MIP_R0_TEST;
	wbuf[1] = MIP_R1_TEST_BUF_ADDR;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 2)){
		dev_err(&info->client->dev, "%s [ERROR] Read buf addr\n", __func__); 
		goto ERROR;
	}
	
	buf_addr_l = rbuf[0];
	buf_addr_h = rbuf[1];
	dev_dbg(&info->client->dev, "%s - buf_addr[0x%02X 0x%02X]\n", __func__, buf_addr_h, buf_addr_l); 
	
	//print data
	if(mip_proc_table_data(info, size, data_type_size, data_type_sign, buf_addr_h, buf_addr_l, row_num, col_num, buffer_col_num, rotate, key_num)){
		dev_err(&info->client->dev, "%s [ERROR] mip_proc_table_data\n", __func__); 
		goto ERROR;
	}

	//set normal mode
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_MODE;
	wbuf[2] = MIP_CTRL_MODE_NORMAL;
	if(mip_i2c_write(info, wbuf, 3)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__); 
		goto ERROR;
	}

	//wait ready status
	wait_cnt = 50;
	while(wait_cnt--){
		if(mip_get_ready_status(info) == MIP_CTRL_STATUS_READY){
			break;
		}
		msleep(10);
		
		dev_dbg(&info->client->dev, "%s - wait [%d]\n", __func__, wait_cnt); 
	}
	
	if(wait_cnt <= 0){
		dev_err(&info->client->dev, "%s [ERROR] Wait timeout\n", __func__); 
		goto ERROR;
	}
	
	dev_dbg(&info->client->dev, "%s - set normal mode\n", __func__);	
	
	goto EXIT;

ERROR:
	ret = 1;
	
EXIT:
	//enable touch event
	wbuf[0] = MIP_R0_CTRL;
	wbuf[1] = MIP_R1_CTRL_EVENT_TRIGGER_TYPE;
	wbuf[2] = MIP_TRIGGER_TYPE_INTR;
	if (mip_i2c_write(info, wbuf, 3)) {
		dev_err(&info->client->dev, "%s [ERROR] Enable event\n", __func__);
		ret = 1;
	}
	
	mutex_lock(&info->lock);
	info->test_busy = false;
	mutex_unlock(&info->lock);

	mip_reboot(info);
	enable_irq(info->client->irq);

	if(!ret)
		dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	else
		dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
 
	return ret;
}

/**
* Read image data
*/
int mip_get_image(struct mip_ts_info *info, u8 image_type)
{
	int busy_cnt = 100;
	int wait_cnt = 100;
	u8 wbuf[8];
	u8 rbuf[512];
	u8 size = 0;
	u8 row_num;
	u8 col_num;
	u8 buffer_col_num;
	u8 rotate;
	u8 key_num;
	u8 data_type;
	u8 data_type_size;
	u8 data_type_sign;
	u8 buf_addr_h;
	u8 buf_addr_l;
		
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);	
	dev_dbg(&info->client->dev, "%s - image_type[%d]\n", __func__, image_type);	

	while(busy_cnt--){
		if(info->test_busy == false){
			break;
		}
		dev_dbg(&info->client->dev, "%s - busy_cnt[%d]\n", __func__, busy_cnt);	
		msleep(5);
	}
	mutex_lock(&info->lock);
	info->test_busy = true;
	mutex_unlock(&info->lock);

	memset(info->print_buf, 0, PAGE_SIZE);
	
	//check image type
	switch(image_type){
		case MIP_IMG_TYPE_INTENSITY:
			dev_dbg(&info->client->dev, "=== Intensity Image ===\n"); 
			sprintf(info->print_buf, "\n=== Intensity Image ===\n\n");
			break;
		case MIP_IMG_TYPE_RAWDATA:
			dev_dbg(&info->client->dev, "=== Rawdata Image ===\n"); 
			sprintf(info->print_buf, "\n=== Rawdata Image ===\n\n");
			break;
		default:
			dev_err(&info->client->dev, "%s [ERROR] Unknown image type\n", __func__); 
			sprintf(info->print_buf, "\nERROR : Unknown image type\n\n");
			goto ERROR;
			break;
	}
	
	//set image type
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_TYPE;
	wbuf[2] = image_type;
	if(mip_i2c_write(info, wbuf, 3)){
		dev_err(&info->client->dev, "%s [ERROR] Write image type\n", __func__); 
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s - set image type\n", __func__);	

	//wait ready status
	wait_cnt = 50;
	while(wait_cnt--){
		if(mip_get_ready_status(info) == MIP_CTRL_STATUS_READY){
			break;
		}
		msleep(10);
		
		dev_dbg(&info->client->dev, "%s - wait [%d]\n", __func__, wait_cnt); 
	}
	
	if(wait_cnt <= 0){
		dev_err(&info->client->dev, "%s [ERROR] Wait timeout\n", __func__); 
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s - ready\n", __func__);	
	
	//data format
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_DATA_FORMAT;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 6)){
		dev_err(&info->client->dev, "%s [ERROR] Read data format\n", __func__); 
		goto ERROR;
	}
	row_num = rbuf[0];
	col_num = rbuf[1];
	buffer_col_num = rbuf[2];
	rotate = rbuf[3];
	key_num = rbuf[4];
	data_type = rbuf[5];

	data_type_sign = (data_type & 0x80) >> 7;
	data_type_size = data_type & 0x7F;
	
	dev_dbg(&info->client->dev, "%s - row_num[%d] col_num[%d] buffer_col_num[%d] rotate[%d] key_num[%d]\n", __func__, row_num, col_num, buffer_col_num, rotate, key_num);	
	dev_dbg(&info->client->dev, "%s - data_type[0x%02X] data_sign[%d] data_size[%d]\n", __func__, data_type, data_type_sign, data_type_size);	

	//get buf addr
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_BUF_ADDR;
	if(mip_i2c_read(info, wbuf, 2, rbuf, 2)){
		dev_err(&info->client->dev, "%s [ERROR] Read buf addr\n", __func__); 
		goto ERROR;
	}
	
	buf_addr_l = rbuf[0];
	buf_addr_h = rbuf[1];
	dev_dbg(&info->client->dev, "%s - buf_addr[0x%02X 0x%02X]\n", __func__, buf_addr_h, buf_addr_l); 

	//print data
	if(mip_proc_table_data(info, size, data_type_size, data_type_sign, buf_addr_h, buf_addr_l, row_num, col_num, buffer_col_num, rotate, key_num)){
		dev_err(&info->client->dev, "%s [ERROR] mip_proc_table_data\n", __func__); 
		goto ERROR;
	}
	
	//clear image type
	wbuf[0] = MIP_R0_IMAGE;
	wbuf[1] = MIP_R1_IMAGE_TYPE;
	wbuf[2] = MIP_IMG_TYPE_NONE;
	if(mip_i2c_write(info, wbuf, 3)){
		dev_err(&info->client->dev, "%s [ERROR] Clear image type\n", __func__); 
		goto ERROR;
	}

	//exit
	mutex_lock(&info->lock);
	info->test_busy = false;
	mutex_unlock(&info->lock);
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);	
	return 0;
	
ERROR:
	mutex_lock(&info->lock);
	info->test_busy = false;
	mutex_unlock(&info->lock);

	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return 1;
}

#endif

#if MIP_USE_SYS

/**
* Firmware update (section)
*/
static ssize_t mip_sys_fw_update_section(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip_ts_info *info = i2c_get_clientdata(client);
	int result = 0;
	u8 data[255];
	int ret = 0;
	
	memset(info->print_buf, 0, PAGE_SIZE);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	ret = mip_fw_update_from_storage(info, false);
	
	switch(ret){
		case fw_err_none:
			sprintf(data, "F/W update success.\n");
			break;
		case fw_err_uptodate:
			sprintf(data, "F/W is already up-to-date.\n");
			break;
		case fw_err_download:
			sprintf(data, "F/W update failed : Download error\n");
			break;
		case fw_err_file_type:
			sprintf(data, "F/W update failed : File type error\n");
			break;
		case fw_err_file_open:			
			sprintf(data, "F/W update failed : File open error [%s]\n",
								info->pdata->ext_fw_name);
			break;
		case fw_err_file_read:
			sprintf(data, "F/W update failed : File read error\n");
			break;
		default:
			sprintf(data, "F/W update failed.\n");
			break;
	}
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	
	strcat(info->print_buf, data);
	result = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return result;
}

/**
* Print chip firmware version
*/
static ssize_t mip_sys_fw_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 rbuf[16];

	memset(info->print_buf, 0, PAGE_SIZE);

	if(mip_get_fw_version(info, rbuf)){
		dev_err(&info->client->dev, "%s [ERROR] mip_get_fw_version\n", __func__);

		sprintf(data, "F/W Version : ERROR\n");	
		goto ERROR;
	}
	
	dev_info(&info->client->dev, "%s - F/W Version : %02X.%02X_%02X.%02X_%02X.%02X_%02X.%02X\n", __func__, rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);	
	sprintf(data, "F/W Version : %02X.%02X_%02X.%02X_%02X.%02X_%02X.%02X\n", rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);	

ERROR:	
	strcat(info->print_buf, data);
		
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
* Set firmware path
*/
static ssize_t mip_sys_fw_path_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	
	dev_dbg(&info->client->dev, "%s [START] \n", __func__);

	if(count <= 1){
		dev_err(&info->client->dev, "%s [ERROR] Wrong value [%s]\n", __func__, buf);
		goto ERROR;
	}	
	
	dev_dbg(&info->client->dev, "%s - Path : %s\n", __func__, info->pdata->ext_fw_name);

	dev_dbg(&info->client->dev, "%s [DONE] \n", __func__);
	return count;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR] \n", __func__);
	return count;
}

/**
* Print firmware path
*/
static ssize_t mip_sys_fw_path_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	
	memset(info->print_buf, 0, PAGE_SIZE);

	dev_dbg(&info->client->dev, "%s [START] \n", __func__);

	sprintf(data, "Path : %s\n", info->pdata->ext_fw_name);

	dev_dbg(&info->client->dev, "%s [DONE] \n", __func__);

	strcat(info->print_buf, data);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
* Print channel info
*/
static ssize_t mip_sys_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 rbuf[32];
	u8 wbuf[8];
	int res_x, res_y;
	
	memset(info->print_buf, 0, PAGE_SIZE);

	sprintf(data, "\n");		
	strcat(info->print_buf, data);
	
	mip_get_fw_version(info, rbuf);
	sprintf(data, "F/W Version : %02X.%02X_%02X.%02X_%02X.%02X_%02X.%02X\n", rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);	
	strcat(info->print_buf, data);
	
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_PRODUCT_NAME;
	mip_i2c_read(info, wbuf, 2, rbuf, 16);
	sprintf(data, "Product Name : %s\n", rbuf);		
	strcat(info->print_buf, data);
	
	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	mip_i2c_read(info, wbuf, 2, rbuf, 7);
	res_x = (rbuf[0]) | (rbuf[1] << 8);
	res_y = (rbuf[2]) | (rbuf[3] << 8);
	sprintf(data, "Resolution : X[%d] Y[%d]\n", res_x, res_y);		
	strcat(info->print_buf, data);

	sprintf(data, "Node Num : X[%d] Y[%d] Key[%d]\n", rbuf[4], rbuf[5], rbuf[6]);		
	strcat(info->print_buf, data);
	
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
	
}

/**
* Power on
*/
static ssize_t mip_sys_power_on(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf,0,PAGE_SIZE);

	mip_power_on(info);

	dev_info(&client->dev, "%s", __func__);	
	
	sprintf(data, "Power : On\n");	
	strcat(info->print_buf,data);
	
	ret = snprintf(buf,PAGE_SIZE,"%s\n",info->print_buf);
	return ret;
	
}

/**
* Power off
*/
static ssize_t mip_sys_power_off(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf,0,PAGE_SIZE);

	mip_power_off(info);

	dev_info(&client->dev, "%s", __func__);	
	
	sprintf(data, "Power : Off\n");	
	strcat(info->print_buf,data);
	
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
	
}

/**
* Reboot chip
*/
static ssize_t mip_sys_reboot(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf, 0, PAGE_SIZE);

	dev_info(&client->dev, "%s", __func__); 
	
	disable_irq(info->irq);	
	mip_clear_input(info);	
	mip_reboot(info);	
	enable_irq(info->irq);
	
	sprintf(data, "Reboot\n");	
	strcat(info->print_buf,data);
		
	ret = snprintf(buf,PAGE_SIZE,"%s\n",info->print_buf);
	return ret;
	
}

/**
* Set mode
*/
static ssize_t mip_sys_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	u8 wbuf[8];
	u8 value = 0;
	
	dev_dbg(&info->client->dev, "%s [START] \n", __func__);

	wbuf[0] = MIP_R0_CTRL;

	if (!strcmp(attr->attr.name, "mode_glove")){		
		wbuf[1] = MIP_R1_CTRL_GLOVE_MODE;
	}
	else if (!strcmp(attr->attr.name, "mode_charger")){
		wbuf[1] = MIP_R1_CTRL_CHARGER_MODE;
	}
	else if (!strcmp(attr->attr.name, "mode_cover_window")){
		wbuf[1] = MIP_R1_CTRL_WINDOW_MODE;
	}
	else if (!strcmp(attr->attr.name, "mode_palm_rejection")){
		wbuf[1] = MIP_R1_CTRL_PALM_REJECTION;
	}
	else {
		dev_err(&info->client->dev, "%s [ERROR] Unknown mode [%s]\n", __func__, attr->attr.name);		
		goto ERROR;
	}

	if(buf[0] == 48){
		value = 0;
	}
	else if(buf[0] == 49){
		value = 1;
	}
	else{
		dev_err(&info->client->dev, "%s [ERROR] Unknown value [%c]\n", __func__, buf[0]);
		goto EXIT;
	}	
	wbuf[2] = value;
	
	if(mip_i2c_write(info, wbuf, 3)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
	}
	else{
		dev_info(&info->client->dev, "%s - value[%d]\n", __func__, value);
	}
		
EXIT:
	dev_dbg(&info->client->dev, "%s [DONE] \n", __func__);
	return count;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR] \n", __func__);
	return count;
}

/**
* Print mode
*/
static ssize_t mip_sys_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];
	u8 rbuf[4];
	
	memset(info->print_buf, 0, PAGE_SIZE);

	dev_dbg(&info->client->dev, "%s [START] \n", __func__);

	wbuf[0] = MIP_R0_CTRL;
	
	if (!strcmp(attr->attr.name, "mode_glove")){		
		wbuf[1] = MIP_R1_CTRL_GLOVE_MODE;
	}
	else if (!strcmp(attr->attr.name, "mode_charger")){
		wbuf[1] = MIP_R1_CTRL_CHARGER_MODE;
	}
	else if (!strcmp(attr->attr.name, "mode_cover_window")){
		wbuf[1] = MIP_R1_CTRL_WINDOW_MODE;
	}
	else if (!strcmp(attr->attr.name, "mode_palm_rejection")){
		wbuf[1] = MIP_R1_CTRL_PALM_REJECTION;
	}
	else {
		dev_err(&info->client->dev, "%s [ERROR] Unknown mode [%s]\n", __func__, attr->attr.name);		
		sprintf(data, "%s : Unknown Mode\n", attr->attr.name);
		goto EXIT;
	}
	
	if(mip_i2c_read(info, wbuf, 2, rbuf, 1)){
		dev_err(&info->client->dev, "%s [ERROR] mip_i2c_read\n", __func__);
		sprintf(data, "%s : ERROR\n", attr->attr.name);
	}
	else{
		dev_info(&info->client->dev, "%s - value[%d]\n", __func__, rbuf[0]);
		sprintf(data, "%s : %d\n", attr->attr.name, rbuf[0]);
	}
	
	dev_dbg(&info->client->dev, "%s [DONE] \n", __func__);

EXIT:	
	strcat(info->print_buf, data);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
}

/**
* Sysfs print image
*/
static ssize_t mip_sys_image(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	int ret;
	u8 type;

	dev_dbg(&info->client->dev, "%s [START] \n", __func__);

	if (!strcmp(attr->attr.name, "image_intensity")){		
		type = MIP_IMG_TYPE_INTENSITY;
	}
	else if (!strcmp(attr->attr.name, "image_rawdata")){
		type = MIP_IMG_TYPE_RAWDATA;
	}
	else {
		dev_err(&info->client->dev, "%s [ERROR] Unknown image [%s]\n", __func__, attr->attr.name);		
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "ERROR : Unknown image type");
		goto ERROR;
	}

	if(mip_get_image(info, type)){
		dev_err(&info->client->dev, "%s [ERROR] mip_get_image\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "ERROR");
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE] \n", __func__);
	
	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;
	
ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return ret;
}

/**
* Sysfs run test
*/
static ssize_t mip_sys_test(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	int ret;
	u8 test_type;
	
	dev_dbg(&info->client->dev, "%s [START] \n", __func__);

	if (!strcmp(attr->attr.name, "test_cm_delta")){		
		test_type = MIP_TEST_TYPE_CM_DELTA;
	}
	else if(!strcmp(attr->attr.name, "test_cm_abs")){
		test_type = MIP_TEST_TYPE_CM_ABS;
	}
	else if (!strcmp(attr->attr.name, "test_cm_jitter")){
		test_type = MIP_TEST_TYPE_CM_JITTER;
	}
	else if (!strcmp(attr->attr.name, "test_short")){
		test_type = MIP_TEST_TYPE_SHORT;
	}
	else {
		dev_err(&info->client->dev, "%s [ERROR] Unknown test [%s]\n", __func__, attr->attr.name);		
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "ERROR : Unknown test type");
		goto ERROR;
	}

	if(mip_run_test(info, test_type)){
		dev_err(&info->client->dev, "%s [ERROR] mip_run_test\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "%s\n", "ERROR");
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->print_buf);
	return ret;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return ret;
}

/**
* Print wake-up gesture code
*/
static ssize_t mip_sys_wakeup_gesture(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 data[255];
	int ret;

	memset(info->print_buf, 0, PAGE_SIZE);

	dev_info(&client->dev, "%s", __func__); 
		
	sprintf(data, "gesture:%d\n", info->wakeup_gesture_code);	
	strcat(info->print_buf,data);
	
	ret = snprintf(buf,PAGE_SIZE,"%s\n",info->print_buf);
	return ret;
	
}

/**
* Sysfs functions
*/
#if USE_CTS_NODE
static DEVICE_ATTR(fw_update_section, S_IRUGO, mip_sys_fw_update_section, NULL);
static DEVICE_ATTR(fw_version, S_IRUGO, mip_sys_fw_version, NULL);
static DEVICE_ATTR(fw_path, S_IRUGO, mip_sys_fw_path_show, mip_sys_fw_path_store);
static DEVICE_ATTR(info, S_IRUGO, mip_sys_info, NULL);
static DEVICE_ATTR(power_on, S_IRUGO, mip_sys_power_on, NULL);
static DEVICE_ATTR(power_off, S_IRUGO, mip_sys_power_off, NULL);
static DEVICE_ATTR(reboot, S_IRUGO, mip_sys_reboot, NULL);
static DEVICE_ATTR(mode_glove, S_IRUGO, mip_sys_mode_show, mip_sys_mode_store);
static DEVICE_ATTR(mode_charger, S_IRUGO, mip_sys_mode_show, mip_sys_mode_store);
static DEVICE_ATTR(mode_cover_window, S_IRUGO, mip_sys_mode_show, mip_sys_mode_store);
static DEVICE_ATTR(mode_palm_rejection, S_IRUGO, mip_sys_mode_show, mip_sys_mode_store);
#endif
static DEVICE_ATTR(image_intensity, S_IRUGO, mip_sys_image, NULL);
static DEVICE_ATTR(image_rawdata, S_IRUGO, mip_sys_image, NULL);
static DEVICE_ATTR(test_cm_delta, S_IRUGO, mip_sys_test, NULL);
static DEVICE_ATTR(test_cm_jitter, S_IRUGO, mip_sys_test, NULL);
#if USE_CTS_NODE
static DEVICE_ATTR(test_short, S_IRUGO, mip_sys_test, NULL);
static DEVICE_ATTR(wakeup_gesture, S_IRUGO, mip_sys_wakeup_gesture, NULL);
#endif

/**
* Sysfs attr list info
*/
static struct attribute *mip_sys_attr[] = {
#if USE_CTS_NODE
	&dev_attr_fw_update_section.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_fw_path.attr,
	&dev_attr_info.attr,
	&dev_attr_power_on.attr,
	&dev_attr_power_off.attr,
	&dev_attr_reboot.attr,
	&dev_attr_mode_glove.attr,
	&dev_attr_mode_charger.attr,
	&dev_attr_mode_cover_window.attr,
	&dev_attr_mode_palm_rejection.attr,
#endif
	&dev_attr_image_intensity.attr,
	&dev_attr_image_rawdata.attr,
	&dev_attr_test_cm_delta.attr,
	&dev_attr_test_cm_jitter.attr,
#if USE_CTS_NODE
	&dev_attr_test_short.attr,
	&dev_attr_wakeup_gesture.attr,
#endif
	NULL,
};

/**
* Sysfs attr group info
*/
static const struct attribute_group mip_sys_attr_group = {
	.attrs = mip_sys_attr,
};

/**
* Create sysfs test functions
*/
int mip_sysfs_create(struct mip_ts_info *info)
{
	struct i2c_client *client = info->client;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (sysfs_create_group(&client->dev.kobj, &mip_sys_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}
	
	info->print_buf = kzalloc(sizeof(u8) * 4096, GFP_KERNEL);
	info->image_buf = kzalloc(sizeof(int) * ((info->node_x * info->node_y) + info->node_key), GFP_KERNEL);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return 0;
}

/**
* Remove sysfs test functions
*/
void mip_sysfs_remove(struct mip_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	
	sysfs_remove_group(&info->client->dev.kobj, &mip_sys_attr_group);

	kfree(info->image_buf);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	
	return;
}

#endif

