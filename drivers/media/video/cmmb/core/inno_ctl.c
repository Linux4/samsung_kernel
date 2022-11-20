
/*
 * innofidei if2xx demod ctl driver
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
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include "inno_core.h"
#include "inno_reg.h"
#include "inno_irq.h"
#include "inno_demod.h"
#include "inno_ctl.h"
#include "inno_cmd.h"
#include "inno_comm.h"
#include "inno_io.h"
#include "inno_power.h"

#ifdef INNO_DEBUG_LOGTOFILE
extern unsigned char inno_log_buf[];
extern void inno_logfile_write(unsigned char*in_buf);
#undef pr_debug
#undef pr_err
#define pr_debug(fmt, ...) \
		{sprintf(inno_log_buf,"INFO:[INNO/core/inno_ctl.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#define pr_err(fmt, ...) \
		{sprintf(inno_log_buf,"ERR:[INNO/core/inno_ctl.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#else

#undef pr_debug
#define pr_debug(fmt, ...) \
printk(KERN_DEBUG "[INNO/core/inno_ctl.c]" pr_fmt(fmt),##__VA_ARGS__)

#undef pr_err
#define pr_err(fmt, ...) \
printk(KERN_ERR "[INNO/core/inno_ctl.c]" pr_fmt(fmt), ##__VA_ARGS__)

#endif

struct {
        struct class    *cls;
        struct device   *parent;
        int             major;
        int             minor;
        dev_t           devt;

        wait_queue_head_t  wait;
        atomic_t           valid;
        atomic_t           ready;
}inno_ctl;

#ifdef IF206
#define LOCATE_EMM_LGX_ID  10
#endif

void inno_ctl_notify_avail (void)
{
        atomic_set(&inno_ctl.valid, 1);
        atomic_set(&inno_ctl.ready, 1);
        wake_up_interruptible(&inno_ctl.wait); 
}
EXPORT_SYMBOL_GPL(inno_ctl_notify_avail);

int get_fw_version(void *data)
{
        struct inno_cmd_frame cmd_frame = {0};
        struct inno_rsp_frame rsp_frame = {0};
        unsigned int version = 0;
        int ret = 0;

        pr_debug("%s\n", __func__);
        if (data == NULL) {
                pr_err("%s:input param invalid\n", __func__);
                ret = -EINVAL;
                goto done;
        }        

        cmd_frame.code = CMD_GET_FW_VER;
	 ret = inno_comm_send_cmd(&cmd_frame);        
	 if(ret == 0){               
                ret = inno_comm_get_rsp(&rsp_frame);
                if (ret < 0) 
                        goto done;                
        
                version = (rsp_frame.data[0]<<8) | rsp_frame.data[1];
        } else {               
                goto done;
        }

        *(unsigned int *)data = version;
done:
        return ret;
}

int set_frequency(void *data)
{
        struct inno_cmd_frame cmd_frame = {0};
        unsigned char freq_dot;

        pr_debug("%s\n", __func__);
        if (data == NULL)
                return -EINVAL;

        freq_dot = *(unsigned char *)data;

        cmd_frame.code = CMD_SET_FREQUENCY;
        cmd_frame.data[0] = freq_dot;        
		pr_debug("Set frequency, Value = 0x%02x\n",freq_dot);
        return (inno_comm_send_cmd(&cmd_frame));

}

int get_frequency(void *data)
{
        pr_debug("%s\n", __func__);
        if (data == NULL)
                return -EINVAL;        
        inno_comm_get_unit(FETCH_PER_COMM17, data, 1);
		pr_debug("Get frequency, Value = 0x%02x\n",(*(unsigned char *)data));
        return 0;
}

int set_ch_config(void *data)
{
        struct inno_cmd_frame cmd_frame = {0};
        struct ch_config *config = (struct ch_config *)data;
    
        pr_debug("%s (%d %d %d %x %d)\n", __func__, 
		config->ch_id, 
		config->start_timeslot,
		config->timeslot_count,
		config->demod_config,
		config->in_sub_frame_out_data_type);

        if (data == NULL)
                return -EINVAL;
        
        config->ch_id += 1;
        if (config->ch_id < 1)
                return -EINVAL;   
        
        cmd_frame.code = CMD_SET_CHANNEL_CONFIG;
        memcpy(cmd_frame.data, config, sizeof(*config));
        
        return (inno_comm_send_cmd(&cmd_frame));
}

int get_ch_config(void *data)
{
        struct inno_cmd_frame cmd_frame = {0};
        struct inno_rsp_frame rsp_frame = {0};
        struct ch_config *config = (struct ch_config *)data;
        int ret = 0;        
 
        pr_debug("%s\n", __func__);
        if (data == NULL)
                return -EINVAL;

        config->ch_id += 1;
        if (config->ch_id < 1) {
                ret = -EINVAL;
                goto done;
        }   

        cmd_frame.code  = CMD_GET_CHANNEL_CONFIG; 
        cmd_frame.data[0] = config->ch_id;

	 ret = inno_comm_send_cmd(&cmd_frame);        
        if(ret == 0) {
                ret = inno_comm_get_rsp(&rsp_frame);
                if (ret < 0)
                        goto done;                
                
                memcpy(config, rsp_frame.data, sizeof(*config));
                config->ch_id -= 1;
        } 

done:
        return ret;
}

//add by mahanghong 20110118
unsigned long ParseErrStatus(unsigned char status)
{
#ifdef IF206
	return status;
#else
	unsigned long ret = 0x9000;

	switch ( status )
	{
		case CAS_OK:
			ret = 0x9000;
			break;
		case NO_MATCHING_CAS:   //Can not find ECM data
			ret = NO_MATCHING_CAS;
			break;
		case CARD_OP_ERROR:    //Time out or err
			ret = CARD_OP_ERROR;
			break;
		case MAC_ERR:
			ret = 0x9862;
			break;
		case GSM_ERR:
			ret = 0x9864;
			break;
		case KEY_ERR:
			ret = 0x9865;
			break;
		case KS_NOT_FIND:
			ret = 0x6985;
			break;
		case KEY_NOT_FIND:
			ret = 0x6a88;
			break;
		case CMD_ERR:
			ret = 0x6f00;
			break;
		default:
			ret = status;//add by zengzheng
			break;			
	}
	
 	return ret;
#endif
}

int get_sys_status(void *data)
{
        struct sys_status *sys_status = (struct sys_status *)data;
        
//add by mahanghong 20110118
	//unsigned char status = 0;
        
        pr_debug("%s\n", __func__);
        if (data == NULL)
                return -EINVAL;
        
        inno_comm_get_unit(OFDM_SYNC_STATE, &sys_status->sync, 1);
        if (sys_status->sync & 0x08)
                sys_status->sync = 1;
        else
                sys_status->sync = 0;
                
        inno_comm_get_unit(FETCH_PER_COMM16, &sys_status->signal_strength, 1);
        inno_comm_get_unit(REG_SIGNAL_QUALITY, &sys_status->signal_quality, 1); 
        inno_comm_get_unit(FETCH_PER_COMM17, &sys_status->cur_freq, 1);
        inno_comm_get_unit(FETCH_PER_COMM20, &sys_status->ldpc_err_percent, 1);
        inno_comm_get_unit(FETCH_PER_COMM21, &sys_status->rs_err_percent, 1);

//add by mahanghong 20110118
//        inno_comm_get_unit(FETCH_PER_COMM29, &status, 1);
//        sys_status->err_status = ParseErrStatus(status);

        return 0;
}

int get_err_info(void *data)
{
        struct err_info *err_info = (struct err_info *)data;
        
        pr_debug("%s\n", __func__);
        if (data == NULL)
                return -EINVAL;
              
        inno_comm_get_unit(FETCH_LDPC_TOTAL, (unsigned char*)&err_info->ldpc_total_count, 4); 
        inno_comm_get_unit(FETCH_LDPC_ERR, (unsigned char*)&err_info->ldpc_error_count, 4); 
        inno_comm_get_unit(FETCH_RS_TOTAL, (unsigned char*)&err_info->rs_total_count, 4); 
        inno_comm_get_unit(FETCH_RS_ERR, (unsigned char*)&err_info->rs_error_count, 4); 
        inno_comm_get_unit(FETCH_PER_COMM22, (unsigned char*)&err_info->BER, 2); 
        inno_comm_get_unit(FETCH_PER_COMM18, (unsigned char*)&err_info->SNR, 2); 

        return 0;        
}

int get_chip_id(void *data)
{
        unsigned char data_high = 0, data_low = 0;
        unsigned int chip_id = 0;

        pr_debug("%s\n", __func__);
        if (data == NULL)
                return -EINVAL;
        
        inno_comm_get_unit(M0_REG_PLL1_CTR, &data_high, 1);    
        inno_comm_get_unit(M0_REG_PLL_STATUS, &data_low, 1);    

        chip_id = (data_high<<8) + (data_low & 0x70);
        *(unsigned int *)data = chip_id;
        return 0;
}

#ifdef IF206
int get_ca_card_sn (void* data)
{
	int ret = 0;
	unsigned char d_head[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	unsigned char cmd[8]    = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	
	cmd[0] = CMD_GET_CASRDSN;
	ret = inno_comm_send_cmd((struct inno_cmd_frame *)cmd);    
	if(ret == 0) {
		ret = inno_comm_get_rsp((struct inno_rsp_frame *)cmd);    
		if(ret == 0) {
			ret = inno_comm_get_unit (0x0000fc00, d_head, sizeof d_head);
			if ((d_head[3] > 0) && (0 != data))
			{
				struct card_sn_info* psn = (struct card_sn_info*)data;
				memset (psn, 0, sizeof(*psn));
				psn->len = d_head[3];
				ret = inno_comm_get_unit (0x0000fc08, psn->sn, psn->len);
			}
		}
	}
	
	return ret;
}

int set_ca_table (void* data)
{
	int ret = 0;
	struct raw_cat_info* pcat = (struct raw_cat_info*)data;

	unsigned char d_head[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	unsigned char cmd[8]    = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };


	ret = inno_comm_send_unit (0x0000fc08, pcat->data, pcat->len);
	d_head[0] = 0xaa;
	d_head[1] = 0x55;		
	d_head[2] = pcat->len & 0xff;
	d_head[3] = (pcat->len >> 8) & 0xff;
	ret = inno_comm_send_unit (0x0000fc00, d_head, sizeof d_head);

	cmd[0] = CMD_SET_CA_TABLE;
	ret = inno_comm_send_cmd ((struct inno_cmd_frame *)cmd);
	if(ret == 0) 
	{
		ret = inno_comm_get_rsp((struct inno_rsp_frame *)cmd);
		if(ret == 0)
		{
			pcat->emm_id = (cmd[1]<<8) | cmd[2];

			if ((cmd[4] == 0) && (cmd[3] == 0))
			{
				pcat->err = CAT_OPS_SUCCESS;
			}
			else if ((cmd[4] == 1) && (cmd[3] == 1))
			{
				pcat->err = CAT_OPS_COMM_ERROR;
			}
			else
			{
				pcat->err = CAT_OPS_CASID_MISMATCH;
			}
		}
	}

	return ret;
}

int set_emm_channel (void* data)
{
	int ret;
	struct emm_ch_config* pch = (struct emm_ch_config*)data;
	unsigned char cmd[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	pr_debug("%s (%d %d %d %x %d %d)\n", __func__, 
		pch->ch_id, 
		pch->start_timeslot,
		pch->timeslot_count,
		pch->demod_config,
		pch->sub_frame,
		pch->is_open);

	cmd[0] = CMD_SET_EMM_CHANNEL;
	cmd[1] = LOCATE_EMM_LGX_ID;
	cmd[2] = pch->start_timeslot;
	cmd[3] = pch->timeslot_count;
	cmd[4] = pch->demod_config;
	cmd[5] = pch->is_open;
	cmd[6] = pch->sub_frame;


	ret = inno_comm_send_cmd((struct inno_cmd_frame *)cmd);    
	if(ret == 0) 
	{
		ret = inno_comm_get_rsp((struct inno_rsp_frame *)cmd);     
	}

	return ret;
} 
#endif

int get_ca_err_state (void* data)
{
	int ret = 0;
	unsigned char err0;
	unsigned char err1;
	unsigned char err2;
	unsigned char err3;
	unsigned char err4;
	struct ca_err_stat* pstat = (struct ca_err_stat*)data;

	pr_debug("%s---CA ERR state ch_id=%d\n", __func__, pstat->ch_id);	
	
	switch(pstat->ch_id)
	{
		case 0:
		{
			ret = inno_comm_get_unit(LG1_CW_STATE_REG, &err0, sizeof err0);
			pstat->err[0] = (int) ParseErrStatus(err0);
			break;
		}
		case 1:
		{
			ret = inno_comm_get_unit(LG2_CW_STATE_REG, &err1, sizeof err1);
			pstat->err[1] = (int) ParseErrStatus(err1);
			break;
		}
		case 2:
		{
			ret = inno_comm_get_unit(LG3_CW_STATE_REG, &err2, sizeof err2);
			pstat->err[2] = (int) ParseErrStatus(err2);
			break;
		}
		case 3:
		{
			ret = inno_comm_get_unit(LG4_CW_STATE_REG, &err3, sizeof err3);
			pstat->err[3] = (int) ParseErrStatus(err3);
			break;
		}
		case 4:
		{
			ret = inno_comm_get_unit(LG5_CW_STATE_REG, &err4, sizeof err4);
			pstat->err[4] = (int) ParseErrStatus(err4);
			break;
		}
		case 0xff:  //Read all logic channel's err status
		{

			ret = inno_comm_get_unit(LG1_CW_STATE_REG, &err0, sizeof err0);
			ret = inno_comm_get_unit(LG2_CW_STATE_REG, &err1, sizeof err1);
			ret = inno_comm_get_unit(LG3_CW_STATE_REG, &err2, sizeof err2);
			ret = inno_comm_get_unit(LG4_CW_STATE_REG, &err3, sizeof err3);
			ret = inno_comm_get_unit(LG5_CW_STATE_REG, &err4, sizeof err4);
			
			pstat->err[0] = (int) ParseErrStatus(err0);
			pstat->err[1] = (int) ParseErrStatus(err1);
			pstat->err[2] = (int) ParseErrStatus(err2);
			pstat->err[3] = (int) ParseErrStatus(err3);
			pstat->err[4] = (int) ParseErrStatus(err4);
			break;
		}
		default:
			 return -EINVAL;		
	}
	pstat->intr = 0x40 << 16; //set intr as bit-22

	pr_debug("%s---CA ERR state (0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n", __func__, pstat->err[0], pstat->err[1], pstat->err[2], pstat->err[3], pstat->err[4] );
	return ret;
}

int enable_cw_detect_mode(void *data)
{

	struct inno_cmd_frame cmd_frame = {0};
        unsigned char *enable = (unsigned char *)data;
    
        pr_debug("%s\n", __func__);
        if (data == NULL)
                return -EINVAL;
          
 	if (*enable == 0)
	{
		cmd_frame.code = CMD_CW_DETECT_MODE_OFF;
	}
        else
	{
		cmd_frame.code = CMD_CW_DETECT_MODE_ON;
	}
        
        return (inno_comm_send_cmd(&cmd_frame));        
}

int get_cw_freq_offset(void *data)
{
	int *freq_offset = (int *)data;
	unsigned char buf[2];
        
        pr_debug("%s\n", __func__);
        if (data == NULL)
                return -EINVAL;
      
        inno_comm_get_unit(FETCH_PER_COMM24, (unsigned char*)buf, 2); 

	*freq_offset = buf[0] + (buf[1] << 8);

	return 0;
}

// no used
int set_system_sleep(void *data)
{
#if 0
	struct inno_cmd_frame cmd_frame = {0};
    
        pr_debug("%s\n", __func__);
           	
	cmd_frame.code = CMD_SET_SLEEP;
        
       return (inno_comm_send_cmd(&cmd_frame));     
#else
	return 0;
#endif
}
	
int inno_cmd_proc(unsigned int cmd, unsigned long arg)
{
        struct inno_req req;
        int ret = 0;

        switch(cmd) {
                case INNO_IO_GET_FW_VERSION:
                {
                        unsigned int version;        
                        req.handler = get_fw_version;
                        req.context = &version;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
                        if (copy_to_user((void __user *)arg, &version, sizeof(version)))
                                return -EFAULT;
                        break;
                }
                case INNO_IO_SET_FREQUENCY:
                {
                        unsigned char freq_dot;
                        if (copy_from_user(&freq_dot, (void __user *)arg, sizeof(freq_dot)))
                                return -EFAULT; 
                        req.handler = set_frequency;
                        req.context = &freq_dot;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
                        break;
                }
                case INNO_IO_GET_FREQUENCY:
                {
                        unsigned char freq_dot;
                        req.handler = get_frequency;
                        req.context = &freq_dot;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
                        if (copy_to_user((void __user *)arg, &freq_dot, sizeof(freq_dot)))
                                return -EFAULT;
                        break;
                }
                case INNO_IO_SET_CH_CONFIG:
                {
                        struct ch_config config;
                                
                        if (copy_from_user(&config, (void __user *)arg, sizeof(config)))
                                return -EFAULT; 
                        req.handler = set_ch_config;
                        req.context = &config;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
                        break;
                }
                case INNO_IO_GET_CH_CONFIG:
                {
                        struct ch_config config;
                        
                        if (!access_ok(VERIFY_READ, 
                                        (u8 __user *)(uintptr_t) arg,
                                        sizeof(config)))
                                return -EFAULT;

                        if (copy_from_user(&config, (void __user *)arg, sizeof(config)))
                                return -EFAULT; 
                        
                        req.handler = get_ch_config;
                        req.context = &config;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;

                        if (copy_to_user((void __user *)arg, &config, sizeof(config)))
                                return -EFAULT; 
                        break;
                }
                case INNO_IO_GET_SYS_STATUS:
                {
                        struct sys_status sys_status;
                        req.handler = get_sys_status;
                        req.context = &sys_status;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;

                        if(copy_to_user((void __user *)arg, &sys_status, sizeof(sys_status)))
                                return -EFAULT;
                        break;
                }
                case INNO_IO_GET_ERR_INFO:
                {
                        struct err_info info;
                        
                        req.handler = get_err_info;
                        req.context = &info;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
                        if(copy_to_user((void __user *)arg, &info, sizeof(info)))
                                return -EFAULT;
                        break;
                }
                case INNO_IO_GET_CHIP_ID:
                {
                        unsigned int id;
                        req.handler = get_chip_id;
                        req.context = &id;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
                        if(copy_to_user((void __user *)arg, &id, sizeof(id)))
                                return -EFAULT;
                        break;
                }

#ifdef IF206
                case INNO_IO_SET_CA_TABLE:
                {
                        struct raw_cat_info cat, cat2;
			if (copy_from_user(&cat, (void __user *)arg, sizeof(cat)))
				return -EFAULT;

			cat2.data = kmalloc (cat.len, GFP_KERNEL | __GFP_NOWARN); //checked
			if (!cat2.data) {
				return -ENOMEM;
			}

                        if (copy_from_user(cat2.data, (void __user *)cat.data, cat.len))
                                return -EFAULT;

			cat2.len = cat.len;
                        req.handler = set_ca_table;
                        req.context = &cat2;
                        ret = inno_req_sync(&req);
			kfree (cat2.data);
                        if (ret < 0)
                                break;

			cat.emm_id = cat2.emm_id;
			cat.err    = cat2.err;
                        if(copy_to_user((void __user *)arg, &cat, sizeof(cat)))
                                return -EFAULT;

                        break;
                }
                case INNO_IO_SET_EMM_CHANNEL:
                {
                        struct emm_ch_config ch;
                        if (copy_from_user(&ch, (void __user *)arg, sizeof(ch)))
                                return -EFAULT; 

                        req.handler = set_emm_channel;
                        req.context = &ch;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
                        break;
                }
                case INNO_IO_GET_CA_CARD_SN:
                {
                        struct card_sn_info sn;
                        req.handler = get_ca_card_sn;
                        req.context = &sn;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;

                        if(copy_to_user((void __user *)arg, &sn, sizeof(sn)))
                                return -EFAULT;
                        break;
                }
#endif

		  case INNO_IO_GET_CA_STATE:
                {
                        struct ca_err_stat castat;
			   if (copy_from_user(&castat, (void __user *)arg, sizeof(castat)))
                                return -EFAULT; 
                        req.handler = get_ca_err_state;
                        req.context = &castat;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;

                        if(copy_to_user((void __user *)arg, &castat, sizeof(castat)))
                                return -EFAULT;                        
                        break;
                }

		case INNO_IO_ENABLE_CW_DETECT_MODE:
		{
			unsigned char enable;
		   	if (copy_from_user(&enable, (void __user *)arg, sizeof(enable)))
                        	return -EFAULT; 
			req.handler = enable_cw_detect_mode;
                        req.context = &enable;
			ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
			break;
		}
		case INNO_IO_GET_CW_FREQ_OFFSET:
		{
			int freq_offset;
                        req.handler = get_cw_freq_offset;
                        req.context = &freq_offset;
                        ret = inno_req_sync(&req);
                        if (ret < 0)
                                break;
                        if (copy_to_user((void __user *)arg, &freq_offset, sizeof(freq_offset)))
                                return -EFAULT;
			break;
		}
		case INNO_IO_SET_SLEEP:
                        req.handler = set_system_sleep;
                        req.context = NULL;
                        ret = inno_req_sync(&req);
			   break;

                default:
                        return -EINVAL;
        }
        
        return ret;
}

extern struct inno_demod  *inno_demod;
//static int innoctl_ioctl(struct inode* inode, struct file* filp, unsigned int cmd, unsigned long arg)
long innoctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        void __user *argp = (void __user *)arg;
        unsigned char val;
        int     retval = 0;

        pr_debug("%s, cmd: %x\n", __func__, (unsigned int)cmd);

        switch(cmd){
                case INNO_IO_POWERENABLE:
                        if(copy_from_user(&val, argp, sizeof(val)))
                                return -EFAULT;
                        if (val)
                                retval = inno_demod_inc_ref();
                        else
                                inno_demod_dec_ref();
                        break;
                case INNO_IO_RESET:
                        if(copy_from_user(&val, argp, sizeof(val)))
                                return -EFAULT;
                        /* reset chip */ 
                        inno_plat_power(0);
                        mdelay(200);
                        inno_plat_power(1);
                        mdelay(200);
                        break;
		  case INNO_IO_SYS_STATUS_CHG:
#ifdef IF206
		  		{
		  	   int mode;
                        if (copy_from_user(&mode, (void __user *)arg, sizeof(mode)))
                                return -EFAULT; 
			   /*
			   * mode =1, power off
			   * mode =0, power on and download fw
			   */
			   pr_debug("%s, %s\n", __func__, mode?"sleep":"resume");
			   mode = mode?0:1;

			/* reset chip */ 
                        inno_plat_power(0);
                        mdelay(200);
                        inno_plat_power(1);
                        mdelay(200);

			   retval = inno_demod_power(inno_demod,mode); 
		  	}
#endif
		  	   break;
                case INNO_IO_GET_FW_VERSION:
                case INNO_IO_SET_FREQUENCY:
                case INNO_IO_GET_FREQUENCY:
                case INNO_IO_SET_CH_CONFIG:
                case INNO_IO_GET_CH_CONFIG:
                case INNO_IO_GET_SYS_STATUS:
		case INNO_IO_ENABLE_CW_DETECT_MODE:
		case INNO_IO_GET_CW_FREQ_OFFSET:
                case INNO_IO_GET_ERR_INFO:
                case INNO_IO_GET_CHIP_ID:
#ifdef IF206					
                case INNO_IO_SET_CA_TABLE:
                case INNO_IO_SET_EMM_CHANNEL:
                case INNO_IO_GET_CA_CARD_SN:
#endif					
                case INNO_IO_GET_CA_STATE:
		  case INNO_IO_SET_SLEEP:
                        BUG_ON(arg == 0);
                       return inno_cmd_proc(cmd, arg); 
                default:
                        return -EINVAL;
        }
        return retval;
}

static int innoctl_open(struct inode* inode, struct file* file)
{
        pr_debug("%s\n", __func__);
		pr_debug("Driver compiled in %s\n", __DATE__);
        
        /* inc demod ref */
        return inno_demod_inc_ref();
}

static int innoctl_release(struct inode *inode, struct file *filep)
{
        pr_debug("%s\n", __func__);
        inno_demod_dec_ref();
        return 0;
}

static ssize_t innoctl_read(struct file *filep, char __user *buf,
                        size_t count, loff_t *ppos)
{
        DECLARE_WAITQUEUE(wait, current);
        ssize_t retval;
        unsigned int valid;
        add_wait_queue(&inno_ctl.wait, &wait);

        do {
                set_current_state(TASK_INTERRUPTIBLE);
                valid = atomic_read(&inno_ctl.valid);
                if (valid) {
			struct ca_err_stat as;
			as.ch_id = 0xff;
                        atomic_set(&inno_ctl.valid, 0);
                        atomic_set(&inno_ctl.ready, 0);

			retval = get_ca_err_state (&as);
			if (retval < 0)
				break;

                        if (copy_to_user(buf, &as, sizeof as))
                                retval = -EFAULT;
                        else
                                retval = valid;
                        break;
                }

                if (filep->f_flags & O_NONBLOCK) {
                        retval = -EAGAIN;
                        break;
                }

                if (signal_pending(current)) {
                        retval = -ERESTARTSYS;
                        break;
                }
                schedule();
        } while (1);

        __set_current_state(TASK_RUNNING);
        remove_wait_queue(&inno_ctl.wait, &wait);

	return 0;
}

static unsigned int innoctl_poll(struct file *filep, poll_table *wait)
{
        poll_wait(filep, &inno_ctl.wait, wait);
        if (atomic_read(&inno_ctl.ready)) {
                atomic_set(&inno_ctl.ready, 0);
                return POLLIN | POLLRDNORM;
        }
        return 0;
}

static struct file_operations innoctl_fops = {
        .owner          = THIS_MODULE,
        .open           = innoctl_open,
//        .ioctl          = innoctl_ioctl,
	.unlocked_ioctl = innoctl_ioctl,
        .read           = innoctl_read,
	.poll           = innoctl_poll,
        .release        = innoctl_release,
};


int inno_ctl_init(struct inno_demod *inno_demod)
{
        pr_debug("%s\n", __func__);

        inno_ctl.major = register_chrdev(0, "innoctl", &innoctl_fops);
        if (inno_ctl.major < 0){ 
                pr_err("failed to register character device.\n");     
                return inno_ctl.major; 
        }
        pr_debug("%s:ctl major = %d\n", __func__, inno_ctl.major); 

        inno_ctl.cls = &inno_demod->cls;
        inno_ctl.parent = inno_demod->dev;
        inno_ctl.minor = 0;
        inno_ctl.devt = MKDEV(inno_ctl.major, 0);

        device_create(inno_ctl.cls, inno_ctl.parent, inno_ctl.devt,
                       NULL, "innoctl");

        init_waitqueue_head(&inno_ctl.wait);   
        return 0;
}

void inno_ctl_exit(void)
{
        pr_debug("%s\n", __func__);
        unregister_chrdev(inno_ctl.major, "innoctl");
        device_destroy(inno_ctl.cls, inno_ctl.devt);
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sean.zhao <zhaoguangyu@innofidei.com>");
MODULE_DESCRIPTION("innofidei cmmb ctl");

