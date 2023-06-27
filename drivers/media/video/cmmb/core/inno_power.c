
/*
 * innofidei if2xx demod power control driver
 * 
 * Copyright (C) 2010 Innofidei Corporation
 * Author:      sean <zhaoguangyu@innofidei.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include "inno_core.h"
#include "inno_cmd.h"
#include "inno_comm.h"
#include "inno_reg.h"
#include "inno_power.h"

#ifdef INNO_DEBUG_LOGTOFILE
extern unsigned char inno_log_buf[];
extern void inno_logfile_write(unsigned char*in_buf);
#undef pr_debug
#undef pr_err
#define pr_debug(fmt, ...) \
		{sprintf(inno_log_buf,"INFO:[INNO/core/inno_power.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#define pr_err(fmt, ...) \
		{sprintf(inno_log_buf,"ERR:[INNO/core/inno_power.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#else

#undef pr_debug
#define pr_debug(fmt, ...) \
printk(KERN_DEBUG "[INNO/core/inno_power.c]" pr_fmt(fmt),##__VA_ARGS__)

#undef pr_err
#define pr_err(fmt, ...) \
printk(KERN_ERR "[INNO/core/inno_power.c]" pr_fmt(fmt), ##__VA_ARGS__)

#endif
//unsigned const char fw_path[] = {
//	#include "firmware_if258.txt"
//};
#if 0
#define FW_REG 0x20000000

static void dump_mem(const unsigned char *buf, size_t len)
{
        int i;
        char tmp[10];
        int a, b, pos;
        a = (len / 8) * 8;
        b = len % 8;
        for (i = 0; i < a; i+=8) {
                pr_debug("%02x %02x %02x %02x %02x %02x %02x %02x\n",
                        buf[i+0], buf[i+1], buf[i+2], buf[i+3],
                        buf[i+4], buf[i+5], buf[i+6], buf[i+7]);
        }
        for (i = 0, pos = 0; i < b; i++) 
                pos += sprintf(&tmp[pos], "%02x ", buf[a+i]);
        tmp[pos] = '\0';
        pr_debug("%s\n", tmp);
}
#endif
static int checkdownload(unsigned char *buffer, int len)
{
        int ret = -1;
        struct inno_cmd_frame cmd_frm = {0};
        struct inno_rsp_frame rsp_frm = {0};
        unsigned short file_checksum = 0;
        unsigned short read_checksum = 0;
        int i = 0;

        /************1. calculate check sum of .bin ****************/
        if(len % 2 == 0){
                for(i = 0; i< len / 2; i++)             
                        file_checksum ^= *((unsigned short *)buffer + i);               

        }
        else{
                for(i = 0; i< (len - 1) / 2; i++)
                        file_checksum ^= *((unsigned short *)buffer + i);               
                file_checksum ^= *(buffer + len - 1) | (0xFF << 8);
                
        }

        pr_debug("\n%s:fw_checksum : 0x%x\n", __func__, file_checksum);

        /************2. Get check sum from demod *******************/
        cmd_frm.code = CMD_GET_FW_CHECK_SUM;          
        cmd_frm.data[0] = 0;
        cmd_frm.data[1] = 0;
        cmd_frm.data[2] = len&0xff;
        cmd_frm.data[3] = (len>>8)&0xff;

        ret = inno_comm_send_cmd(&cmd_frm);        
	if(ret == 0)
	{
		ret = inno_comm_get_rsp(&rsp_frm);
                if(ret < 0)
                        goto done;                
	}       
        else                
                goto done;


        read_checksum = (rsp_frm.data[1] << 8) | rsp_frm.data[0];

        pr_debug("%s:file_checksum = 0x%x, read_checksum = 0x%x\n", __func__, file_checksum, read_checksum);

        
        /*************** 3. Compare check sum *****************/
        if(file_checksum != read_checksum)
                ret = -EIO;

done:
	 
        return ret;
}

static int downloadfw(unsigned char *buf, int len)
{
        int ret = 0;
        unsigned long clk_ctr = 0x80000001, cpu_ctr = 0x00200000, value = 0x00000000;
        pr_debug("downloadfw ......");
	printk("============================================\n");
	printk("len = %d\n",len);
	printk("============================================\n");
        /* reset chip */ 
        inno_plat_power(1);
        mdelay(200);
        
        inno_comm_send_unit(M0_REG_CPU_CTR, (unsigned char *)&cpu_ctr, 4);
        
        inno_comm_send_unit(FW_BASE_ADDR, buf, len);
        
        inno_comm_send_unit(M0_REG_CLK_CTR, (unsigned char *)&clk_ctr, 4);

        inno_comm_send_unit(M0_REG_CPU_CTR, (unsigned char *)&value, 4);
		
        mdelay(500);
		
#ifdef IF206		
	 mdelay(2500);
#endif	 

        return ret;
}


#define IF228_CHIPID    0x00 //0x18
#define IF238_CHIPID    0x05//0x05
#define IF206_CHIPID	0x01
#define IF206N_CHIPID	0x04
#if 0
static int inno_get_chipid(unsigned int * chipID)
{
#if 0
	unsigned int id = 0;
	unsigned char tmpchar = 0;

	inno_comm_get_unit(40004048,&tmpchar,1);
	id = (tmpchar&0x00000070)>>4;
	//inno_comm_get_unit(VERSION_ID_MONTH,&tmpchar,1);
	//id = (id << 8)|tmpchar;
	//inno_comm_get_unit(VERSION_ID_MAIN,&tmpchar,1);
	//id = (id << 8)|tmpchar;
	//inno_comm_get_unit(VERSION_ID_SUB,&tmpchar,1);
	//id = (id << 8)|tmpchar;

	pr_debug("Got chip ID = 0x%08x\n",id);
	*chipID = id;

	return 0;

#else
	unsigned char data = 0;
	inno_comm_get_unit(M0_REG_PLL_STATUS, &data, 1);
	*chipID = (unsigned int) (data&0x70) >> 4;

	return 0;
	/*
	if (IF238_CHIPID == *chipID)
	{
		pr_debug("ChipID is 0x%x\n",*chipID);
		return 0;
	}
	else
	{
		data = 0;
		*chipID = 0;
		inno_comm_get_unit(M0_REG_PLL1_CTR, &data, 1);

		*chipID = (unsigned int)data;
		pr_debug("ChipID is 0x%x\n",*chipID);
		if(IF228_CHIPID == *chipID)
		{
			return 0;
		}
		else
		{
			pr_debug("read ChipID err!!\n");
			return -1;
		}
	}*/
#endif
}
#endif

#if 0
static int inno_LNA_init(void)
{
	const char tmpbuf[]={LG8_CW_DEF_VALUE,LG9_CW_DEF_VALUE,LG10_CW_DEF_VALUE,LG11_CW_DEF_VALUE,RF108_STATE_REG0_VALUE,RF108_STATE_REG1_VALUE,RF108_STATE_REG2_VALUE,RF108_STATE_REG3_VALUE};
    return inno_comm_send_unit(LG8_CW_STATE_REG, (unsigned char *)tmpbuf, 8);
}
#endif
//////////////inno change begin from liugge 20120816 ///////////////////
//static unsigned char firmware_file_buffer[1024*50];//50k for firmware file, if the file is bigger, this number should be edit
///////////////inno change end from liuge 20120816//////////////////////
int inno_demod_power(struct inno_demod *demod, unsigned char on)
{
        int ret = 0;
        const struct firmware *fw;
        unsigned char *fw_buf;
		//unsigned char fw_name[32]="inno_demod_fw.bin";
//////////////inno change begin from liugge 20120816 ///////////////////
		//unsigned int chip_id;
///////////////inno change end from liuge 20120816//////////////////////

        pr_debug("%s: %s\n", __func__, on?"on":"off");
        if (!on) {
                inno_plat_power(0);
                goto out;
        }
		//add by liujinpeng
		/*
        inno_plat_power(1);
        mdelay(200);

		if (inno_get_chipid(&chip_id) < 0x00) {return -ENODEV;}	
		switch (chip_id)
		{
			case IF228_CHIPID:
				//sprintf(fw_name,"inno_IF228_fw.bin");
				pr_debug("Chip is IF228\n");
				ret = request_firmware(&fw, "inno_IF228_fw.bin", demod->dev);
				if (ret) {pr_debug("Firmware file inno_IF228_fw.bin not found.\n");}
				break;
			case IF238_CHIPID:
				//sprintf(fw_name,"inno_IF238_fw.bin");
				pr_debug("Chip is IF238\n");
				ret = request_firmware(&fw, "inno_IF238_fw.bin", demod->dev);
				if (ret) {pr_debug("Firmware file inno_IF238_fw.bin not found.\n");}
				break;
			default:
				ret = request_firmware(&fw, "inno_demod_fw.bin", demod->dev);
				pr_debug("Unknow Chip!\n");
				//return -ENODEV;
				break;
		}
		*/
		//end


        ret = request_firmware(&fw, "inno_demod_fw.bin", demod->dev);
        //if (ret) {ret = request_firmware(&fw, "inno_demod_fw.bin", demod->dev);}

        if (ret) {
                pr_err("request firmware timeout, place inno_demod_fw.bin into firmware folder please\n");
                goto out;
        }

        pr_debug("%s:fw size = %d\n", __func__,fw->size);
////////////////inno change begin from liugge 20120816 ///////////////////
        fw_buf = kzalloc(fw->size, GFP_KERNEL | GFP_DMA);//checked
//		//fw_buf = firmware_file_buffer;
/////////////////inno change end from liuge 20120816//////////////////////
		if (fw_buf == NULL) {return -ENOMEM;}
        memcpy(fw_buf, fw->data, fw->size);
        //dump_mem(fw->data, 20);
       
        downloadfw(fw_buf, fw->size);
//        downloadfw(fw_path, sizeof(fw_path));
        ret = checkdownload(fw_buf, fw->size);
//        ret = checkdownload(fw_path, sizeof(fw_path));
        if (ret < 0) {
                pr_debug("%s:fw download failed\n", __func__);        
                inno_plat_power(0);
        }

		//inno_LNA_init();//add by liujinpeng
        release_firmware(fw);
//////////////inno change begin from liugge 20120816 ///////////////////
        kfree(fw_buf);
///////////////inno change end from liuge 20120816//////////////////////
out:
        
        return ret;     
}
EXPORT_SYMBOL_GPL(inno_demod_power);  

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sean.zhao <zhaoguangyu@innofidei.com>");
MODULE_DESCRIPTION("innofidei cmmb fw download");

