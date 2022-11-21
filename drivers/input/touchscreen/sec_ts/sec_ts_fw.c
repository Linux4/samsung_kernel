#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/sec_sysfs.h>
#include <linux/irq.h>
#include <asm/gpio.h>
#include <linux/of_gpio.h>

#include <linux/time.h>



#include "sec_ts.h"

#define SEC_TS_FW_BLK_SIZE 256
 
#define SEC_TS_READ_FW_INFO 0xA2
#define SEC_TS_CMD_ERASE_FLASH 0x45
#define SEC_TS_READ_FLASH_ERASE_STATUS 0x59
#define SEC_TS_CMD_WRITE_FW_BLK 0x53
#define SEC_TS_CMD_WRITE_FW_SHORT 0x54
#define SEC_TS_CMD_WRITE_FW_LONG 0x5A
#define SEC_TS_CMD_ENTER_FW_MODE 0x57
#define SEC_TS_CMD_SW_RESET 0x42

#define SEC_TS_READ_FW_VERSION 0xA3
#define SEC_TS_READ_PARA_VERSION 0xA4

#define SEC_TS_FW_HEADER_SIGN 0x53494654
#define SEC_TS_FW_CHUNK_SIGN  0x53434654
#define SEC_TS_DEFAULT_FW_NAME "tsp_sec/s6smc43_s6_img_REL.bin"
//#define SEC_TS_DEFAULT_FW_NAME "tsp_sec/MC41_150114_1.bin"
#define SEC_TS_DEFAULT_PARA_NAME "tsp_sec/s6smc41_para_REL_DGA0_V0106_150114_193317.bin"
#define SEC_TS_DEFAULT_UMS_FW "/sdcard/lsi.bin"
#define SEC_TS_MAX_FW_PATH 64

#define SEC_TS_READ_BOOT_STATUS 0x55

enum {
        BUILT_IN = 0,
        UMS,
        NONE,
        FFU,
};


typedef struct 
{
	
	u32 signature;			//signature
	u32 version;			//version
	u32 totalsize;			//total size
	u32 checksum;			//checksum
	u32 img_ver;			//image file version
	u32 img_date;			//image file date
	u32 img_description;	//image file description
	u32 fw_ver;				//firmware version
	u32 fw_date;			//firmware date
	u32 fw_description;		//firmware description
	u32 para_ver;			//parameter version
	u32 para_date;			//parameter date
	u32 para_description;	//parameter description
	u32 num_chunk;			//number of chunk
	u32 reserved1;
	u32 reserved2;
}fw_header;


typedef struct
{
	u32 signature;
	u32 addr;
	u32 size;
	u32 reserved;
}fw_chunk;


static int sec_ts_enter_fw_mode(struct sec_ts_data *ts)
{
	int ret;
	u8 fw_update_mode_passwd[] = {0x55, 0xAC};
	u8 fw_status;
	u8 id[3];
	
	ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_ENTER_FW_MODE, fw_update_mode_passwd, sizeof(fw_update_mode_passwd));
	sec_ts_delay(20);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: write fail, enter_fw_mode\n", __func__);
		return 0;
	}
	tsp_debug_info(true, &ts->client->dev, "%s: write ok, enter_fw_mode - 0x%x 0x%x 0x%x\n", __func__, SEC_TS_CMD_ENTER_FW_MODE, fw_update_mode_passwd[0], fw_update_mode_passwd[1]);

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_BOOT_STATUS, &fw_status, 1);
	sec_ts_delay(10);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: read fail, read_boot_status\n", __func__);
		return 0;
	}
	if (fw_status != 0x10)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: enter fail! read_boot_status = 0x%x\n", __func__, fw_status);
		return 0;
	}
	tsp_debug_info(true, &ts->client->dev, "%s: Success! read_boot_status = 0x%x\n", __func__, fw_status);

	ret = ts->sec_ts_i2c_read(ts, 0x52, id, 3);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: read id fail\n", __func__);
		return 0;
	}
	tsp_debug_info(true, &ts->client->dev, "%s: read_id = %02X%02X%02X\n", __func__, id[0],id[1],id[2]);
	
	return 1;
}

static int sec_ts_sw_reset(struct sec_ts_data *ts)
{
	if (ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SW_RESET, NULL, 0) < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: write fail, sw_reset\n", __func__);
		return 0;
	}
	sec_ts_delay(500);
	tsp_debug_info(true, &ts->client->dev, "%s: sw_reset\n", __func__);

	return 1;
}

static int sec_ts_check_firmware_version(struct sec_ts_data *ts, const u8* fw_info)
{
	fw_header* fw_hd;
	u8 fw_ver[4];
	u8 para_ver[4];
	u8 buff[1];
	
	int ret;
	/*------------------------------------------
	- sec_ts_check_firmware_version -
	return value = 1 : firmware download needed,
	return value = 0 : skip firmware download
	------------------------------------------*/

	fw_hd = (fw_header *)fw_info;
	
	//firmware download if READ_BOOT_STATUS = 0x10
	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_BOOT_STATUS, buff, 1);
	if (ret < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: fail to read BootStatus\n",__func__);
		return -1;
	}

	if(buff[0] == 0x10){
		tsp_debug_info(true, &ts->client->dev, "%s: ReadBootStatus = 0x%x, Firmware download Start!\n", __func__, buff[0]);
		return 1;
	}	
	
	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_FW_VERSION, fw_ver, 4);
	if(ret < 0)
	{
		tsp_debug_info(true, &ts->client->dev, "%s: firmware version read error\n ", __func__);
		return -1;
	}
	tsp_debug_info(true, &ts->client->dev, "%s: firmware version info : %x.%x.%x.%x\n ", __func__, fw_ver[0], fw_ver[1], fw_ver[2], fw_ver[3]);
	if(((fw_hd->fw_ver)&0xff) != fw_ver[0])
	{
		tsp_debug_err(true, &ts->client->dev, "%s: F/W product is not equal: %x \n ", __func__, fw_ver[0]);
		return -1;
	}
	if(((fw_hd->fw_ver>>8)&0xff) != fw_ver[1])
	{
		tsp_debug_err(true, &ts->client->dev, "%s: F/W project is not equal : %x\n ", __func__, fw_ver[1]);
		return -1;
	}
	
	if((((fw_hd->fw_ver>>24)&0xff) > fw_ver[3]) ||
	  (((fw_hd->fw_ver>>16)&0xff) > fw_ver[2]))	
	{
		return 1;
	}
	ret = ts->sec_ts_i2c_read(ts, SEC_TS_READ_PARA_VERSION, para_ver, 4);	
	if(ret < 0)
	{
		tsp_debug_info(true, &ts->client->dev, "%s: parameter version read error\n ", __func__);
		return -1;
	}
	tsp_debug_info(true, &ts->client->dev, "%s: parameter version info : %x.%x.%x.%x\n ", __func__, para_ver[0], para_ver[1], para_ver[2], para_ver[3]);
	if(((fw_hd->para_ver)&0xff) != para_ver[0])
	{
		tsp_debug_err(true, &ts->client->dev, "%s: Parameter product is not equal: %x \n ", __func__, para_ver[0]);
		return -1;
	}
	if(((fw_hd->para_ver>>8)&0xff) != para_ver[1])
	{
		tsp_debug_err(true, &ts->client->dev, "%s: Parameter project is not equal: %x \n ", __func__, para_ver[1]);
		return -1;
	}
	
	if((((fw_hd->para_ver>>24)&0xff) > para_ver[3]) ||
	  (((fw_hd->para_ver>>16)&0xff) > para_ver[2]))	
	{
		return 1;
	}
	
	return 0;
}


static u8 sec_ts_checksum(u8* data, int offset, int size)
{
    int i;
    u8 checksum = 0;

    for (i = 0; i < size; i++) checksum += data[i + offset];

    return checksum;
}

static int sec_ts_flashpageerase(struct sec_ts_data *ts, u32 page_idx, u32 page_num)
{
	int ret;
    u8 tCmd[6];
	
    tCmd[0] = 0xD8;
    tCmd[1] = (u8)((page_idx >> 8) & 0xFF);
    tCmd[2] = (u8)((page_idx >> 0) & 0xFF);
    tCmd[3] = (u8)((page_num >> 8) & 0xFF);
    tCmd[4] = (u8)((page_num >> 0) & 0xFF);
    tCmd[5] = sec_ts_checksum(tCmd, 1, 4);

    ret = ts->sec_ts_i2c_write_burst(ts,tCmd,6);
	
	return ret;
}

static bool sec_ts_flashpagewrite(struct sec_ts_data *ts, u32 page_idx, u8* page_data)
{
	int ret;
	int i;
    u8 tCmd[1 + 2 + 256 + 1];

    tCmd[0] = 0xD9;
    tCmd[1] = (u8)((page_idx >> 8) & 0xFF);
    tCmd[2] = (u8)((page_idx >> 0) & 0xFF);
    for (i = 0; i < 256; i++) tCmd[3 + i] = page_data[i];
    tCmd[1 + 2 + 256] = sec_ts_checksum(tCmd, 1, 2 + 256);
	
	ret = ts->sec_ts_i2c_write_burst(ts,tCmd,1+2+256+1);
	
    return ret;
}

static int sec_ts_flashwrite(struct sec_ts_data *ts, u32 mem_addr, u8* mem_data, u32 mem_size)
{
	int ret;
	int page_idx;
	int size_left;
    int size_copy;
	u32 flash_page_size;
    u32 page_idx_start;
    u32 page_idx_end;
    u32 page_num;
	u8 page_buf[256];

	if (0 == mem_size) return 0;
	
    flash_page_size = 256;
    page_idx_start = mem_addr / flash_page_size;
    page_idx_end = (mem_addr + mem_size - 1) / flash_page_size;
    page_num = page_idx_end - page_idx_start + 1;

    ret = sec_ts_flashpageerase(ts, page_idx_start, page_num);
	if(ret < 0) 
	{
		tsp_debug_err(true, &ts->client->dev, "%s fw erase failed, mem_addr= %08X, pagenum = %d\n", __func__,mem_addr, page_num);
		return -EIO;	
    }
	 
	sec_ts_delay(page_num+10);
    
    size_left = (int)mem_size;
    size_copy = (int)(mem_size % flash_page_size);
    if (size_copy == 0) size_copy = (int)flash_page_size;
	
	memset(page_buf, 0, 256);
	
    for (page_idx = (int)page_num - 1; page_idx >= 0; page_idx--)
    {
		memcpy(page_buf, mem_data + (page_idx * flash_page_size), size_copy);
		
		ret = sec_ts_flashpagewrite(ts, (u32)(page_idx + page_idx_start), page_buf);
		if( ret < 0) 
		{		
			tsp_debug_err(true, &ts->client->dev, "%s fw write failed, page_idx = %d\n", __func__, page_idx);
			goto err;
		}
		
        size_copy = (int)flash_page_size;
		sec_ts_delay(5);
    }
	
	return mem_size;
err:	
    return -EIO;
}

static int sec_ts_memoryblockread(struct sec_ts_data *ts,u32 mem_addr, u32 mem_size, u8* buf)
 {
     int ret;
     u8 cmd[5];
	 
     if (mem_size >= 64 * 1024) return -1;

     cmd[0] = (u8) 0xD0;
     cmd[1] = (u8)((mem_addr >> 24) & 0xff);
     cmd[2] = (u8)((mem_addr >> 16) & 0xff);
     cmd[3] = (u8)((mem_addr >>  8) & 0xff);
     cmd[4] = (u8)((mem_addr >>  0) & 0xff);
	 ret = ts->sec_ts_i2c_write_burst(ts,cmd,5);
	 if(ret < 0)
	 {
		tsp_debug_err(true, &ts->client->dev, "%s send command failed, %02X\n", __func__, cmd[0]);
		return -1;
	 }
	 udelay(10);
     cmd[0] = (u8) 0xD1;
     cmd[1] = (u8)((mem_size >> 8) & 0xff);
     cmd[2] = (u8)((mem_size >> 0) & 0xff);  
	 ret = ts->sec_ts_i2c_write_burst(ts,cmd,3);
	 if(ret < 0)
	 {
		tsp_debug_err(true, &ts->client->dev, "%s send command failed, %02X\n", __func__, cmd[0]);
		return -1;
	 }
	 udelay(10);	
     cmd[0] = (u8) 0xD2;	 
	 ret = ts->sec_ts_i2c_read(ts, cmd[0], buf, mem_size);	 
	 if (ret < 0)
	 {	
	 	tsp_debug_err(true, &ts->client->dev, "%s memory read failed\n", __func__);
		return -1;
	 }
	 
	return 0;
 }
	 
static  int sec_ts_memoryread(struct sec_ts_data *ts,u32 mem_addr, u8* mem_data, u32 mem_size)
{
 	int ret;
	int retry = 20;
	int read_size = 0;
    int unit_size;
    int max_size = 1024;
    int read_left = (int)mem_size;    

    while (read_left > 0)
    {
        unit_size = (read_left > max_size) ? max_size : read_left;
		retry = 20;	
        do{        	 
        	ret = sec_ts_memoryblockread(ts, mem_addr, (u32)unit_size, mem_data + read_size);        
        	if(retry-- == 0){
				tsp_debug_err(true, &ts->client->dev, "%s fw read fail mem_addr=%08X,unit_size=%d\n", __func__,mem_addr,unit_size);
				return -1;    
        	}
        }while(ret < 0);

        mem_addr += unit_size;
        read_size += unit_size;
        read_left -= unit_size;	 
    }

    return read_size;
}




static int sec_ts_chunk_update(struct sec_ts_data *ts, u32 addr, u32 size, u8* data)
{
		
	u32 fw_size, fw_size4;
	u32 write_size;	
	
	u8* mem_data;
	u8* mem_rb;	

	fw_size = size;
	fw_size4 = (fw_size + 3) & ~3;     /* 4-bytes align */
	
	mem_data = (u8 *)kmalloc(sizeof(u8)*fw_size4, GFP_KERNEL);
	if(!mem_data)
	{
		tsp_debug_err(true, &ts->client->dev, "%s kmalloc failed\n", __func__);
		return -1;
	}

	memcpy(mem_data, data, sizeof(u8) * fw_size);
 	//return -1;
	
	write_size = sec_ts_flashwrite(ts, addr, mem_data, fw_size4);
	if (write_size != fw_size4)
	{
		tsp_debug_err(true, &ts->client->dev, "%s fw write failed\n", __func__);
		return -1;
	}
		
	mem_rb = (u8 *)kmalloc(fw_size4,GFP_KERNEL);
	if(!mem_rb)
	{
		tsp_debug_err(true, &ts->client->dev, "%s kmalloc failed\n", __func__);
		return -1;
	}
	if (sec_ts_memoryread(ts, addr, mem_rb, fw_size4) >= 0)
	{
		u32 ii;
		for (ii = 0; ii < fw_size4; ii++)
		{
			if (mem_data[ii] != mem_rb[ii])
			{
				break;
			}
		}
		if (fw_size4 != ii)
		{
			tsp_debug_err(true, &ts->client->dev, "%s fw verify fail\n", __func__);
			return -1;
		}
	}
	else
	{
		return -1;
	}
	kfree(mem_data);
	kfree(mem_rb);	
	
	sec_ts_delay(10);		
	
	return 0;
	
}


static int sec_ts_firmware_update(struct sec_ts_data *ts, const u8 *data, size_t size)
{
	int i;
	int ret;
	fw_header *fw_hd;
	fw_chunk *fw_ch;


	u8 fw_status = 0;
	u8* fd = (u8 *)data;
	
	// Check whether CRC is appended or not.	
	//Enter Firmware Update Mode--------------
	if (!sec_ts_enter_fw_mode(ts))
	{
		tsp_debug_err(true, &ts->client->dev, "%s firmware mode failed\n", __func__);
		return -1;
	}

	fw_hd = (fw_header *)fd;
	fd += sizeof(fw_header);

	if(fw_hd->signature != SEC_TS_FW_HEADER_SIGN )
	{	
		tsp_debug_err(true, &ts->client->dev, "%s firmware header error = %08X\n", __func__,fw_hd->signature);
		return -1;
	}
	
	for(i=0;i<fw_hd->num_chunk;i++)
	{
		fw_ch = (fw_chunk *)fd;
		if(fw_ch->signature != SEC_TS_FW_CHUNK_SIGN )
		{
			tsp_debug_err(true, &ts->client->dev, "%s firmware chunk error = %08X\n", __func__,fw_ch->signature);
			return -1;
		}
		fd += sizeof(fw_chunk);		
		ret = sec_ts_chunk_update(ts,fw_ch->addr,fw_ch->size,fd);
		if(ret < 0 ){
			tsp_debug_err(true, &ts->client->dev, "%s firmware chunk write failed, addr=%08X, size = %d\n", __func__, fw_ch->addr, fw_ch->size);
			return -1;
		}
		fd += fw_ch->size;
	}

	// SW reset
	sec_ts_sw_reset(ts);


	if (ts->sec_ts_i2c_read(ts, SEC_TS_READ_BOOT_STATUS, &fw_status, 1) < 0)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: read fail, read_boot_status = 0x%x\n", __func__, fw_status);
		return -1;
	}
	if (fw_status != 0x20)
	{
		tsp_debug_err(true, &ts->client->dev, "%s: fw update sequence done, BUT read_boot_status = 0x%x\n", __func__, fw_status);
		return -1;
	}
	tsp_debug_info(true, &ts->client->dev, "%s: fw update Success! read_boot_status = 0x%x\n", __func__, fw_status);

	
	return 0;
	
}

int sec_ts_firmware_update_on_probe(struct sec_ts_data *ts)
{
        const struct firmware *fw_entry;
        char fw_path[SEC_TS_MAX_FW_PATH];
        int result = -1;

        disable_irq(ts->client->irq);

        if (!ts->plat_data->firmware_name)
                snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", SEC_TS_DEFAULT_FW_NAME);
        else
                snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);

        tsp_debug_info(true, &ts->client->dev, "%s: initial firmware update  %s\n", __func__, fw_path);

        //Loading Firmware------------------------------------------
        if (request_firmware(&fw_entry, fw_path, &ts->client->dev) !=  0)
        {
                tsp_debug_err(true, &ts->client->dev, "%s: firmware is not available\n", __func__);
                goto err_request_fw;
        }
        tsp_debug_info(true, &ts->client->dev, "%s: request firmware done! size = %d\n", __func__, (int)fw_entry->size);

        result = sec_ts_check_firmware_version(ts, fw_entry->data);
        if( result <= 0 )
                goto err_request_fw;

        if (sec_ts_firmware_update(ts, fw_entry->data, fw_entry->size) < 0)
                result = -1;
        else
                result = 0;

err_request_fw:
        release_firmware(fw_entry);
        enable_irq(ts->client->irq);
        return result;
}

static int sec_ts_load_fw_from_ums(struct sec_ts_data *ts)
{
	fw_header* fw_hd;
        struct file *fp;
        mm_segment_t old_fs;
        long fw_size, nread;
        int error = 0;

        old_fs = get_fs();
        set_fs(KERNEL_DS);

        fp = filp_open(SEC_TS_DEFAULT_UMS_FW, O_RDONLY, S_IRUSR);
        if (IS_ERR(fp)) {
                tsp_debug_err(true, ts->dev, "%s: failed to open %s.\n", __func__,
                        SEC_TS_DEFAULT_UMS_FW);
                error = -ENOENT;
                goto open_err;
        }

        fw_size = fp->f_path.dentry->d_inode->i_size;

        if (0 < fw_size) {
                unsigned char *fw_data;
                fw_data = kzalloc(fw_size, GFP_KERNEL);
                nread = vfs_read(fp, (char __user *)fw_data,
                                 fw_size, &fp->f_pos);

                tsp_debug_info(true, ts->dev,
                         "%s: start, file path %s, size %ld Bytes\n",
                         __func__, SEC_TS_DEFAULT_UMS_FW, fw_size);

                if (nread != fw_size) {
                        tsp_debug_err(true, ts->dev,
                                "%s: failed to read firmware file, nread %ld Bytes\n",
                                __func__, nread);
                        error = -EIO;
                } else {
			fw_hd = (fw_header *)fw_data;
        
                	tsp_debug_info(true, &ts->client->dev, "%s: firmware version %08X\n ", __func__, fw_hd->fw_ver);
			tsp_debug_info(true, &ts->client->dev, "%s: parameter version %08X\n ", __func__, fw_hd->para_ver);

			if (ts->irq)
				disable_irq(ts->irq);
		        if (sec_ts_firmware_update(ts, fw_data, fw_size) < 0)
				goto done;
                        if (ts->irq)
                        	enable_irq(ts->irq);
                }

                if (error < 0)
                        tsp_debug_err(true, ts->dev, "%s: failed update firmware\n",
                                __func__);

done:
                kfree(fw_data);
        }

        filp_close(fp, NULL);

 open_err:
        set_fs(old_fs);
        return error;
}



int sec_ts_firmware_update_on_hidden_menu(struct sec_ts_data *ts, int update_type)
{
	int ret = 0;

        /* Factory cmd for firmware update
         * argument represent what is source of firmware like below.
         *
         * 0 : [BUILT_IN] Getting firmware which is for user.
         * 1 : [UMS] Getting firmware from sd card.
         * 2 : none
         * 3 : [FFU] Getting firmware from air.
         */

	switch (update_type) {
	case BUILT_IN:
		ret = sec_ts_firmware_update_on_probe(ts); 
		break;
	case UMS:
		ret = sec_ts_load_fw_from_ums(ts);
		break;
	/*case FFU:
		ret = 
		break;*/
	default:
		tsp_debug_err(true, ts->dev, "%s: Not support command[%d]\n",
			__func__, update_type);
		break;
	}
	return ret;
}
EXPORT_SYMBOL(sec_ts_firmware_update_on_hidden_menu);
