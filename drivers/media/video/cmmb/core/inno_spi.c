
/*
 * innofidei if2xx demod spi communication driver
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
#include <asm/uaccess.h>
#include <linux/spi/spi.h>
#include "inno_reg.h"
#include "inno_cmd.h"
#include "inno_core.h"
#include "inno_spi_platform.h"
#include "inno_comm.h"
#include "inno_power.h"
#include "inno_spi.h"

#ifdef INNO_DEBUG_LOGTOFILE
extern unsigned char inno_log_buf[];
extern void inno_logfile_write(unsigned char*in_buf);
#undef pr_debug
#undef pr_err
#define pr_debug(fmt, ...) \
			{sprintf(inno_log_buf,"INFO:[INNO/core/inno_spi.c]" pr_fmt(fmt),##__VA_ARGS__); \
								inno_logfile_write(inno_log_buf);}
#define pr_err(fmt, ...) \
			{sprintf(inno_log_buf,"ERR:[INNO/core/inno_spi.c]" pr_fmt(fmt),##__VA_ARGS__); \
								inno_logfile_write(inno_log_buf);}
#else

#undef pr_debug
#define pr_debug(fmt, ...) \
printk(KERN_DEBUG "[INNO/core/inno_spi.c]" pr_fmt(fmt),##__VA_ARGS__)

#undef pr_err
#define pr_err(fmt, ...) \
printk(KERN_ERR "[INNO/core/inno_spi.c]" pr_fmt(fmt), ##__VA_ARGS__)

#endif

#define CMMB_SPI_MAX_SIZE 4096
static struct inno_platform plat;

static void *dummy_1;
static void *dummy_2;

static spinlock_t	spi_trans_lock;//add by liujinpeng 2012.03.02

static inline int
spi_txfer(struct spi_device *spi, const void *buf, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= buf,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(spi, &m);
}
static inline int
spi_rxfer(struct spi_device *spi, void *buf, size_t len)
{
	struct spi_transfer	t = {
			.rx_buf		= buf,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(spi, &m);
}
static int cmmb_spi_write(struct spi_device *spi, const u8 * buffer, int length)
{
	int offset = 0;
	int temp_size = 0;
	int ret;

	while (length) {
		temp_size =
		    (length >= CMMB_SPI_MAX_SIZE) ? CMMB_SPI_MAX_SIZE : length;

		ret = spi_txfer(spi, (const u8 *)buffer + offset, temp_size);
		if (ret) {
			pr_debug("%s: spi_write to cmmb device error!",
				 __func__);
			break;
		}

		offset += temp_size;
		length -= temp_size;
	}
	return length;
}


static int cmmb_spi_read(struct spi_device *spi, void *buffer, int length)
{
	int offset = 0;
	int temp_size = 0;
	int ret;

	while (length) {
		temp_size =
		    (length >= CMMB_SPI_MAX_SIZE) ? CMMB_SPI_MAX_SIZE : length;

		ret = spi_rxfer(spi, (u8 *) buffer + offset, (size_t) temp_size);
		if (ret) {
			pr_debug("%s: spi_read from cmmb device error!",
				 __func__);
			break;
		}
		offset += temp_size;
		length -= temp_size;
	}
	return length;
}
/**
 * inno_spi_sync - translate data need to send/receive to spi_message and send it
 * @data1:first data need to send/receive
 * @len1 :first data len
 * @dir1 :direction (send or receive)
 * @data2:second data need to send/receive
 * @len2 :second data len 
 * @dir2 :direction (send or receive)
 * @cs_change:is cs pin need change between data1 and data2?
 */
int inno_spi_sync(unsigned char *data1, int len1, int dir1,
                unsigned char *data2, int len2, int dir2,
                unsigned char cs_change)
{
	struct spi_message msg;
	struct spi_transfer t[5];
	int ret = 0;
	spi_message_init(&msg);
	memset(t, 0 ,2 * sizeof(struct spi_transfer));

	plat.pdata->cs_assert();
	spin_lock_irq(&spi_trans_lock); //by liujinpeng
	if (len1) {
		t[0].len = len1;
		t[0].cs_change = cs_change;
		if (dir1 == SPI_TX) {
			spin_unlock_irq(&spi_trans_lock);//add by liujinpeng 2012.03.02
			cmmb_spi_write(plat.spi,data1,len1);
//			if (len1 < 100) {
//				memcpy(dummy_1, data1, len1);
//				t[0].tx_buf = dummy_1;
//			} else {
//				t[0].tx_buf = data0;
//				cmmb_spi_write(plat.spi,data1,len1);
//			}
                } else {
			spin_unlock_irq(&spi_trans_lock);//add by liujinpeng 2012.03.02
			cmmb_spi_read(plat.spi,data1,len1);
//			if (len1 < 100)
//				t[0].rx_buf = dummy_1;
//			else
//				t[0].rx_buf = data1;
		}
//                spi_message_add_tail(&t[0], &msg);

        }
//	spin_unlock_irq(&spi_trans_lock);//add by liujinpeng 2012.03.02
//        plat.spi_transfer(&msg); 
	if (cs_change)
	{
		plat.pdata->cs_deassert();
		plat.pdata->cs_assert();
	}
	spi_message_init(&msg);

	spin_lock_irq(&spi_trans_lock); //by liujinpeng
	if (len2) {
		t[1].len = len2;
		if (dir2 == SPI_TX) {
			spin_unlock_irq(&spi_trans_lock);//add by liujinpeng 2012.03.02
			cmmb_spi_write(plat.spi,data2,len2);
			//			if (len2 < 100) {
			//				memcpy(dummy_2, data2, len2);
			//				t[1].tx_buf = dummy_2;
			//			} else {
			//				t[1].tx_buf = data2;
			//			}
		} else {
			spin_unlock_irq(&spi_trans_lock);//add by liujinpeng 2012.03.02
			cmmb_spi_read(plat.spi,data2,len2);
			//			if (len2 < 100)
			//				t[1].rx_buf = dummy_2;
			//			else
			//				t[1].rx_buf = data2;
		}
		//		spi_message_add_tail(&t[1], &msg);
	}
	//	spin_unlock_irq(&spi_trans_lock);//add by liujinpeng 2012.03.02
	//	plat.spi_transfer(&msg); 
	//	spin_lock_irq(&spi_trans_lock); 
	//
	//        if ((dir1 == SPI_RX) && len1 && len1 < 100) {
	//               memcpy(data1, dummy_1, len1);
	//        }
	//        if ((dir2 == SPI_RX) && len2 && len2 < 100) {
	//               memcpy(data2, dummy_2, len2);
	//        }
	//        
	//		spin_unlock_irq(&spi_trans_lock);//add by liujinpeng 2012.03.02
	plat.pdata->cs_deassert();
	return ret;
}

void inno_plat_power(unsigned char on)
{
        plat.power(on);
}

static struct {
        unsigned short fetch_len;
        unsigned short fetch_data;
}cmd_set[8] = {
        [0] = {
                .fetch_len = READ_LG0_LEN,
                .fetch_data = FETCH_LG0_DATA,
        }, 
        [1] = {
                .fetch_len = READ_LG1_LEN,
                .fetch_data = FETCH_LG1_DATA,
        }, 
        [2] = {
                .fetch_len = READ_LG2_LEN,
                .fetch_data = FETCH_LG2_DATA,
        }, 
        [3] = {
                .fetch_len = READ_LG3_LEN,
                .fetch_data = FETCH_LG3_DATA,
        }, 
        [4] = {
                .fetch_len = READ_LG4_LEN,
                .fetch_data = FETCH_LG4_DATA,
        }, 
        [5] = {
                .fetch_len = READ_LG5_LEN,
                .fetch_data = FETCH_LG5_DATA,
        }, 
        [6] = {
                .fetch_len = READ_LG6_LEN,
                .fetch_data = FETCH_LG6_DATA,
        }, 
        [7] = {
                .fetch_len = READ_LG7_LEN,
                .fetch_data = FETCH_LG7_DATA,
        }, 
};

#define FETCH_LEN_DELAY_LIMITED 500
#define FETCH_DATA_DELAY_LIMITED 900
/**
 * inno_lgx_fetch_data -fetch logic channel data
 * @lgx:lgx_device
 *
 * this func is used by inno_lgx.c, when a channel data is comming, 
 * inno_irq_req_handler will notify inno_lgx.c ,
 * then inno_lgx.c fetch data by call inno_lgx_fetch_data
 */
int inno_lgx_fetch_data(struct lgx_device *lgx)
{
        unsigned int    len;
        unsigned char cmd_fetch_len, cmd_fetch_data;
        union {
                unsigned int    len;
                unsigned char   data[4];
        }rsp = {0};
		char * tmpdat;

        pr_debug("%s: in  @ %d\n", __func__, jiffies_to_msecs(jiffies));
        /* get data length need to fetch */
        cmd_fetch_len = cmd_set[lgx->id + 1].fetch_len;
        cmd_fetch_data  = cmd_set[lgx->id + 1].fetch_data;

        inno_spi_sync(&cmd_fetch_len, 1, SPI_TX, rsp.data, 3, SPI_RX, 1);
        len = rsp.len;
        
	if (time_after(jiffies, lgx->irq_jif + msecs_to_jiffies(FETCH_LEN_DELAY_LIMITED))) {
			pr_err("%s:fetch len is to late, context maybe had been destroyed by next irq, drop this frame  @ %d\n", __func__, jiffies_to_msecs(jiffies));
		return -EBUSY;
	}
       
        pr_debug("%s:lg%d len = 0x%x  @ %d\n", __func__, lgx->id, len, jiffies_to_msecs(jiffies));
        if (len > 0 && len <= lgx->buf_len) {
                inno_spi_sync(&cmd_fetch_data, 1, SPI_TX, lgx->buf, len, SPI_RX, 1);
				tmpdat = (char *)lgx->buf;
				pr_debug("%s:got data:0x%02x%02x%02x%02x%02x\n", __func__, tmpdat[0],tmpdat[1],tmpdat[2],tmpdat[3],tmpdat[4]);

		if (time_after(jiffies, lgx->irq_jif + msecs_to_jiffies(FETCH_DATA_DELAY_LIMITED))) {
					pr_err("%s:fetch data is to late, context maybe had been destroyed by next irq, drop this frame  @ %d\n", __func__, jiffies_to_msecs(jiffies));
			lgx->valid = 0;		
			return -EBUSY;
		}
                lgx->valid = len;
                pr_debug("%s: out  @ %d\n", __func__, jiffies_to_msecs(jiffies));
                return 0;
        } 
        return -EINVAL;
}
EXPORT_SYMBOL_GPL(inno_lgx_fetch_data);

int inno_get_intr_ch(unsigned long *ch_bit)
{
        unsigned char cmd[3];
        unsigned char rsp[3]; 
	unsigned long tmp=0;
	unsigned char i=0;

        pr_debug("%s\n", __func__);

        cmd[0] = READ_INT0_STATUS;
        cmd[1] = READ_INT1_STATUS;
        cmd[2] = READ_INT2_STATUS;

        for (i = 0 ; i < 3 ; i++) 
        {
                inno_spi_sync(&cmd[i], 1, SPI_TX, &rsp[i], 1, SPI_RX, 1);
        }

	tmp = (((unsigned long) rsp[0]) & 0xff) |((((unsigned long) rsp[1]) & 0xff) << 8);	
	*ch_bit = (tmp >> 1) + ((rsp[2] & 0x80)? (0x1 << UAM_BIT_SHIFT) : 0) + ((rsp[2] & 0x40)? (0x1 << CA_BIT_SHIFT) : 0);
	
        return 0;
}


static void packcmd(unsigned int addr,unsigned char cmd, unsigned char* buf)
{
    buf[0] = cmd;
    buf[1] = (addr>>24)&0xff;
    buf[2] = (addr>>16)&0xff;
    buf[3] = (addr>>8)&0xff;
    buf[4] = (addr)&0xff;
}

/**
 * inno_comm_send_unit - send data to addr
 * @addr:target address data will be sent
 * @buf :data
 * @len :len
 */
int inno_comm_send_unit(unsigned int addr, unsigned char *buf, int len)
{
        unsigned char cmd[5];
        packcmd(addr, WRITE_AHBM2, cmd);
        inno_spi_sync(cmd, 5, SPI_TX, buf, len, SPI_TX, 0);
        return 0;
}
EXPORT_SYMBOL_GPL(inno_comm_send_unit);

/**
 * inno_comm_get_unit - get data from addr
 * @addr:target address data will read from
 * @buf :data receive buf
 * @len :len need to read
 */
//static spi_receive_buffer[0xFFFF];//should bug enough for lgx data
int inno_comm_get_unit(unsigned int addr, unsigned char *buf, int len)
{
        unsigned char *tmp;
        unsigned char cmd[5];
        int i;
        packcmd(addr, READ_AHBM2, cmd);
        tmp = kzalloc(len + 1, GFP_KERNEL | GFP_DMA);//checked
		//tmp = spi_receive_buffer;
		if (tmp == NULL) {return -ENOMEM;}
        inno_spi_sync(cmd, 5, SPI_TX, tmp, len + 1, SPI_RX, 0);
        for(i = 0; i < len; i++) {
                buf[i] = tmp[i+1];
        }
        kfree(tmp);
        return 0;        

}
EXPORT_SYMBOL_GPL(inno_comm_get_unit);

int inno_comm_init(void)
{
	dummy_1 = kmalloc(100, GFP_KERNEL);//checked
	dummy_2 = kmalloc(100, GFP_KERNEL);//checked
	if ( dummy_1 == NULL || dummy_2 == NULL ) {return -ENOMEM;}
        plat.irq_handler = (irq_handler_t)inno_demod_irq_handler;
        inno_platform_init(&plat);
		spin_lock_init(&spi_trans_lock);
        return 0;
}

void inno_comm_exit(void)
{
	kfree(dummy_1);
	kfree(dummy_2);
}


//just for debug

/*
 * add by liujinpeng ,for debug of signal qualiy
 * */
void inno_signal_print(void)
{
	unsigned char sig_power,snr,ldpc_err,rs_err,sig_qlt,tmp;
	unsigned int  tuner_stat=0;
	unsigned int i;

	inno_comm_get_unit(FETCH_PER_COMM16,&sig_power,1);
	//pr_debug("Signal_power = 0x%x\n",sig_power);
	inno_comm_get_unit(FETCH_PER_COMM18,&snr,1);
	//pr_debug("SNR = 0x%x\n",tmp);
	inno_comm_get_unit(FETCH_PER_COMM20,&ldpc_err,1);
	//pr_debug("LDPC_ERR_RATE = 0x%x\n",tmp);
	inno_comm_get_unit(FETCH_PER_COMM21,&rs_err,1);
	//pr_debug("RS_ERR_RATE = 0x%x\n",tmp);
	inno_comm_get_unit(FETCH_PER_COMM30,&sig_qlt,1);
	//pr_debug("SIGNAL_QUALITY = 0x%x\n",tmp);


	
	for (i=0;i<sizeof(unsigned int);i++)
	{
		inno_comm_get_unit((0x0000FBFC+i),&tmp,1);
		//pr_debug("Read from 0x%08x,value is 0x%02x\n",(0x0000FBFC+i),tmp);
		tuner_stat = tuner_stat|((int)tmp<<(8*i));
		//pr_debug("Now tmpint=0x%08x\n",tmpint);
	}
	

	
	//inno_comm_get_unit(0x0000FBFC,&tmpint,4);
	pr_debug("Signal_power=0x%x,SNR=0x%x,LDPC_ERR_RATE=0x%x,RS_ERR_RATE=0x%x,SIGNAL_QUALITY=0x%x,Tuner_state = 0x%08x\n", \
				sig_power,snr,ldpc_err,rs_err,sig_qlt,tuner_stat);

	return;

}
EXPORT_SYMBOL_GPL(inno_signal_print);

