/***************************************************************
    A globalmem driver as an example of char device drivers

    The initial developer of the original code is Baohua Song
    <author@linuxdriver.cn>. All Rights Reserved.
****************************************************************/
#include <linux/types.h>
#include <linux/slab.h>
#include "ipc_info.h"

extern u32 mux_ipc_GetTxTransferSavedCount(void);

IPC_INFO_T   s_ipc_info[IPC_CHANNEL_NUM];



#include "./../ts0710.h"
#include "./../ts0710_mux.h"

/*For BP UART problem Begin*/
#ifdef TS0710SEQ2
#define FIRST_BP_SEQ_OFFSET 0	/*offset from start flag */
#define SECOND_BP_SEQ_OFFSET 0	/*offset from start flag */
#define FIRST_AP_SEQ_OFFSET 0	/*offset from start flag */
#define SECOND_AP_SEQ_OFFSET 0	/*offset from start flag */
#define SLIDE_BP_SEQ_OFFSET 0	/*offset from start flag */
#define SEQ_FIELD_SIZE 0
#else
#define SLIDE_BP_SEQ_OFFSET  1	/*offset from start flag */
#define SEQ_FIELD_SIZE    0
#endif

#define ADDRESS_FIELD_OFFSET (1 + SEQ_FIELD_SIZE)	/*offset from start flag */

#define TS0710MUX_MAX_TOTAL_FRAME_SIZE (DEF_TS0710_MTU + TS0710_MAX_HDR_SIZE + FLAG_SIZE)

extern __u32  mux_crc_check(__u8 * data, __u32 length, __u8 check_sum);

int sdio_frame_check(char* buf_ptr, size_t length)
{
        __u8* frame_start_ptr = NULL;
        __u8* end_ptr = NULL;
        short_frame* short_pkt = NULL;
        long_frame* long_pkt = NULL;
        int framelen = 0;
        __u32 crc_error = 0;
        __u8 *uih_data_start = NULL;
        __u32 uih_len = 0;
        int ret = 0;

        frame_start_ptr = (__u8*)buf_ptr;
        end_ptr =  (__u8*)buf_ptr + length;

        /*while(frame_start_ptr  <  end_ptr)
        {
        	short_pkt = NULL;
        	long_pkt = NULL;
        	crc_error = 0;
        	framelen = 0;
        	uih_data_start = NULL;
        	uih_len = 0;

        	if(TS0710_BASIC_FLAG != *frame_start_ptr)
        	{
        		printk("%s:Error,mux frame head error!\r\n", __func__);
        		ret = -1;
        		break;
        	}

        	short_pkt = (short_frame *) (frame_start_ptr + ADDRESS_FIELD_OFFSET);

        	if(short_pkt->h.length.ea == 1)  //short frame
        	{
        		framelen = TS0710_MAX_HDR_SIZE + short_pkt->h.length.len + 1 + SEQ_FIELD_SIZE;
        	}
        	else
        	{
        		long_pkt = (long_frame *) (frame_start_ptr +  ADDRESS_FIELD_OFFSET);
        		framelen = TS0710_MAX_HDR_SIZE + GET_LONG_LENGTH(long_pkt->h.length) + 2 + SEQ_FIELD_SIZE;
        	}

        	if (framelen > (TS0710MUX_MAX_TOTAL_FRAME_SIZE + SEQ_FIELD_SIZE))
        	{
        		printk("%s:Error,mux frame length:%d is bigger than Max frame size:%d\r\n",
        		__func__, framelen, (TS0710MUX_MAX_TOTAL_FRAME_SIZE + SEQ_FIELD_SIZE));
        		ret = -1;
        	break;
        	}

        	if (*(frame_start_ptr + framelen - 1) != TS0710_BASIC_FLAG)
        	{
        		int k = 0;
        		printk("%s:Error,mux frame end error!, framelen start addr:0x%x, framelen:%d, offset:%d, \r\n",
        			__func__, (__u32)frame_start_ptr, framelen,  (__u32)frame_start_ptr - (__u32)buf_ptr);

        		printk("\r\n =================\r\n0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X\r\n================\r\n",
        			frame_start_ptr[framelen -8],frame_start_ptr[framelen -7],frame_start_ptr[framelen -6],frame_start_ptr[framelen -5],
        			frame_start_ptr[framelen -4],frame_start_ptr[framelen -3],frame_start_ptr[framelen -2],frame_start_ptr[framelen -1]);

        		ret = -1;
        		break;
        	}


        	if ((short_pkt->h.length.ea) == 1)   //short frame
        	{
        		uih_len = short_pkt->h.length.len;
        		uih_data_start = short_pkt->data;
        		crc_error = mux_crc_check((__u8*) (frame_start_ptr + SLIDE_BP_SEQ_OFFSET),SHORT_CRC_CHECK + 1, *(uih_data_start + uih_len));
        	}
        	else
        	{
        		uih_len = GET_LONG_LENGTH(long_pkt->h.length);
        		uih_data_start = long_pkt->h.data;
        		crc_error =mux_crc_check((__u8*) (frame_start_ptr +SLIDE_BP_SEQ_OFFSET),LONG_CRC_CHECK + 1,*(uih_data_start +uih_len));
        	}

        	if(crc_error)
        	{
        		printk("%s:Error,mux frame crc error!\r\n", __func__);
        		ret = -1;
        		break;
        	}

        	frame_start_ptr += framelen;

        	if(frame_start_ptr > end_ptr)
        	{
        		printk("%s:Error,mux frame data error!\r\n", __func__);
        		ret = -1;
        		break;
        	}
        }*/

        return ret;
}



void ipc_info_init(void)
{
        memset(&s_ipc_info[0], 0, sizeof(IPC_INFO_T)*IPC_CHANNEL_NUM);
}


void ipc_info_rate(u32 channel,  u32 value)
{
        if(channel >= IPC_CHANNEL_NUM) {
                return;
        }

        s_ipc_info[channel].rate = value;
}

void ipc_info_change_status(u32 channel,  u32 status)
{
        if(channel >= IPC_CHANNEL_NUM) {
                return;
        }

        s_ipc_info[channel].status = status;
}


void ipc_info_mux_write(u32 size)
{
        s_ipc_info[IPC_TX_CHANNEL].mux_count += size;
}


void ipc_info_mux_read(u32 size)
{
        s_ipc_info[IPC_RX_CHANNEL].mux_count += size;
}

void ipc_info_mux_read_overflow(u32 size)
{
        s_ipc_info[IPC_RX_CHANNEL].overflow_count += size;
}


void ipc_info_sdio_write(u32 size)
{
        s_ipc_info[IPC_TX_CHANNEL].sdio_count += size;
}


void ipc_info_sdio_write_payload(u32 size)
{
        s_ipc_info[IPC_TX_CHANNEL].payload_count += size;
}

void ipc_info_sdio_write_overflow(u32 size)
{
        s_ipc_info[IPC_TX_CHANNEL].overflow_count += size;
}

void ipc_info_sdio_read(u32 size)
{
        s_ipc_info[IPC_RX_CHANNEL].sdio_count += size;
}

void ipc_info_sdio_read_payload(u32 size)
{
        s_ipc_info[IPC_RX_CHANNEL].payload_count += size;
}



void ipc_info_sdio_read_saved_count(u32 size)
{
        s_ipc_info[IPC_RX_CHANNEL].saved_count = size;
}

void ipc_info_error_status(u32 channel,  u32 status)
{
        if(channel >= IPC_CHANNEL_NUM) {
                return;
        }

        s_ipc_info[channel].status = status;

        if (IPC_STATUS_FLOW_STOP == status) {
                s_ipc_info[channel].flow_stop_count++;
        } else if (IPC_STATUS_INVALID_PACKET == status) {
                s_ipc_info[channel].invalid_pkt_count++;
        } else if (IPC_STATUS_CRC_ERROR == status) {
                s_ipc_info[channel].crc_error_count++;
        } else if (IPC_STATUS_PACKET_ERROR == status) {
                s_ipc_info[channel].packet_error_count++;
        }
}


IPC_INFO_T*  ipc_info_getinfo(u32 channel)
{
        if(channel >= IPC_CHANNEL_NUM) {
                return NULL;
        }
        s_ipc_info[IPC_TX_CHANNEL].saved_count = mux_ipc_GetTxTransferSavedCount();
        return &s_ipc_info[channel];
}




