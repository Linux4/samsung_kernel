#ifndef __ipc_info_H__
#define __ipc_info_H__

#define IPC_STATUS_IDLE                  0
#define IPC_STATUS_IRQ_IND               1
#define IPC_STATUS_CONNECT_REQ           2
#define IPC_STATUS_CONNECTED             3
#define IPC_STATUS_DISCONNECT_REQ        4
#define IPC_STATUS_DISCONNECTED          5
#define IPC_STATUS_FLOW_STOP       	     6
#define IPC_STATUS_INVALID_PACKET    	 7
#define IPC_STATUS_CRC_ERROR             8
#define IPC_STATUS_PACKET_ERROR          9
#define IPC_STATUS_INVAILD               10


#define IPC_TX_CHANNEL   0
#define IPC_RX_CHANNEL   1
#define IPC_CHANNEL_NUM  2

typedef struct IPC_INFO_Tag {
        u32 status;
        u32 rate;
        u32 mux_count;
        u32 sdio_count;
        u32 payload_count;
        u32 saved_count;
        u32 flow_stop_count;
        u32 invalid_pkt_count;
        u32 packet_error_count;
        u32 crc_error_count;
        u32 overflow_count;
} IPC_INFO_T;


void ipc_info_init(void);
void ipc_info_change_status(u32 channel,  u32 status);
void ipc_info_rate(u32 channel,  u32 value);
void ipc_info_mux_write(u32 size);
void ipc_info_mux_read(u32 size);
void ipc_info_mux_read_overflow(u32 size);
void ipc_info_sdio_write(u32 size);
void ipc_info_sdio_write_payload(u32 size);
void ipc_info_sdio_write_overflow(u32 size);
void ipc_info_sdio_read(u32 size);
void ipc_info_sdio_read_payload(u32 size);
void ipc_info_sdio_read_saved_count(u32 size);
void ipc_info_error_status(u32 channel,  u32 status);
void ipc_info_sdio_write_overflow(u32 size);
IPC_INFO_T*  ipc_info_getinfo(u32 channel);
int sdio_frame_check(char* buf_ptr, size_t length);
#endif /* __MICP_PERF_H__*/
