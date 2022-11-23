/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "ipc_gpio.h"
//#include <mach/modem_interface.h>
#include <linux/sprdmux.h>
#include <linux/mmc/sdio_channel.h>
#include <linux/kfifo.h>
#include "ipc_info.h"
#define  MUX_IPC_READ_DISABLE                    0x01
#define  MUX_IPC_WRITE_DISABLE                  0x02



//#define SDIO_READ_DEBUG
//#define SDIO_LOOPBACK_TEST
//#define SDIO_DEBUG_ENABLE

#define HEADER_TAG 0x7e7f
#define HEADER_TYPE 0xaa00

#define IPC_DRIVER_NAME "IPC_SDIO"

#define MAX_SDIO_TX_WAIT_TIMEOUT    1

#define MAX_SDIO_CP_AWAKE_TIMEOUT    800
#define MAX_SDIO_SLEEP_TIMEOUT    50
#define MAX_SDIO_CP_ACK_TIMEOUT    1000
#define MAX_SDIO_TX_WATERMARK    4
#define MAX_SDIO_RX_RETRY		3
#define MAX_SDIO_TX_RETRY		0xFFFFFFFF




#define MAX_MIPC_RX_FRAME_SIZE    	(64*1024)
#define MAX_MIPC_TX_FRAME_SIZE    	(16*1024)

#define MIN_MIPC_TX_FRAME_SIZE 		(4 * 1024)

#define MAX_MIPC_RX_CACHE_SIZE   MAX_MIPC_RX_FRAME_SIZE*4

//#define MAX_MIPC_TX_FIFO_SIZE    (1 << 7)   // 128
#define MAX_MIPC_TX_FIFO_SIZE    (1 << 4)   // 16


#define MAX_MIPC_TX_FRAME_NUM    (MAX_MIPC_TX_FIFO_SIZE - 1)

#define NEXT_TX_FIFO_IDX(idx) (((idx) + 1) & (MAX_MIPC_TX_FIFO_SIZE - 1))

#define MIPC_TX_REQ_FLAG ((s_mipc_tx_ctrl.fifo_wr != s_mipc_tx_ctrl.fifo_rd) && s_tx_flow_info)



//#define IPC_DBG(f, x...) 	pr_debug(IPC_DRIVER_NAME " [%s()]: " f, __func__,## x)
#define IPC_DBG printk


struct packet_header {
        u16 tag;
        u16 flag;
        u32 length;
        u32 frame_num;
        union {
                u32 emergency_cmd;
                u32 ul_frame_num;
        } u;

        u32 reserved[4];
} __packed;

typedef struct  MIPC_TRANSF_FRAME_Tag {
        u8*  buf_ptr;
        u32  buf_size;
        u32  pos;
        u32  flag;
        struct list_head  link;
} MIPC_TRANSF_FRAME_T;

typedef struct  MIPC_FRAME_LIST_Tag {
        struct list_head   frame_list_head;
        struct mutex       list_mutex;
        u32    counter;
} MIPC_FRAME_LIST_T;

typedef struct MIPC_TRANSFER_Tag {
        struct mutex                             transfer_mutex;
        struct list_head                        frame_fifo;
        MIPC_TRANSF_FRAME_T*        cur_frame_ptr;
        u32                                            counter;
} MIPC_TRANSFER_T;

typedef struct CP_AWAKE_STATUS_Tag {
        u32 ack_time;
        bool awake;
        spinlock_t lock;
        wait_queue_head_t wait;
} CP_AWAKE_STATUS_T;

typedef struct MIPC_TX_CTRL_Tag {
        MIPC_TRANSF_FRAME_T*        	fifo[MAX_MIPC_TX_FIFO_SIZE];
        volatile u32                 fifo_rd;
        volatile u32                 fifo_wr;
        u32                        	fifo_ack;
        u32							last_ack_frame;
} MIPC_TX_CTRL_T;

static struct kfifo  s_mipc_rx_cache_kfifo;

u8*   s_mipc_rx_buf = NULL;


MIPC_TRANSF_FRAME_T   s_tx_transfer_frame[MAX_MIPC_TX_FRAME_NUM];

static struct wake_lock   s_ipc_sdio_wake_lock;

static struct mutex ipc_mutex;
wait_queue_head_t s_mux_read_rts;
wait_queue_head_t s_rx_fifo_avail;
wait_queue_head_t s_modem_ready;

u32 s_mux_ipc_event_flags = 0;

static struct task_struct *s_mux_ipc_rx_thread;
static struct task_struct *s_mux_ipc_tx_thread;
static struct task_struct *s_mux_ipc_sdio_thread;


static  int   sdio_tx_wait_time = 10;
MIPC_FRAME_LIST_T  s_mipc_tx_free_frame_list;
MIPC_TRANSFER_T     s_mipc_tx_tansfer;

static u32  s_mux_ipc_enable = 0;

static wait_queue_head_t s_mux_ipc_tx_wq;
static wait_queue_head_t s_mux_ipc_rx_wq;



static u32  s_mux_ipc_module_inited = 0;

static int  sdio_transfer_crc_check_enable = 0;
static int  sdio_transfer_frame_check_enable = 0;

u32 s_mipc_rx_event_flags = 0;

static wait_queue_head_t s_mux_ipc_sdio_wq;
static u32 s_mipc_rx_req_flag = 0;
static CP_AWAKE_STATUS_T s_cp_awake_status;
//static u32 s_mipc_tx_above_watermark = 0;
static u32 s_tx_flow_info = 0xFF;
static u32 s_acked_tx_frame = 0;
static u32 s_last_tx_frame = 0;
static u32 s_tx_frame_counter; //tx frame number start from 1

static MIPC_TX_CTRL_T s_mipc_tx_ctrl;




static DEFINE_MUTEX(sdio_tx_lock);
static ssize_t mux_ipc_xmit_buf(const char *buf, ssize_t len);
static int mux_ipc_sdio_write( const char *buf, size_t  count);
static int mux_ipc_sdio_read(  char *buf, size_t  count);
static u32 _FreeAllTxTransferFrame(void);
static void _FreeFrame(MIPC_TRANSF_FRAME_T* frame_ptr, MIPC_FRAME_LIST_T*  frame_list_ptr);


#include<mach/hardware.h>

/* use this method we can get time everytime */
#define TIMER_REG(off) (SPRD_TIMER_BASE + (off))
#define SYSCNT_REG(off) (SPRD_SYSCNT_BASE + (off))
#define SYSCNT_COUNT    SYSCNT_REG(0x0004)

/* no need double-reading */
#define SYSCNT_SHADOW_COUNT    SYSCNT_REG(0x000c)

static u32 inline get_sys_cnt(void)
{
#if defined(CONFIG_ARCH_SC7710)
        return __raw_readl(SYSCNT_SHADOW_COUNT);
#else
        u32 val1, val2;
        val1 = __raw_readl(SYSCNT_COUNT);
        val2 = __raw_readl(SYSCNT_COUNT);
        while(val2 != val1) {
                val1 = val2;
                val2 = __raw_readl(SYSCNT_COUNT);
        }
        return val2;
#endif
}

static  u32  _is_mux_ipc_enable(void)
{
        //return s_mux_ipc_enable ;
        return 1;
}


void wait_cp_bootup(void)
{
        wait_event_interruptible(s_modem_ready, _is_mux_ipc_enable());
}

void init_cp_awake_status(void)
{
        s_cp_awake_status.ack_time = 0;
        s_cp_awake_status.awake = false;
        spin_lock_init(&s_cp_awake_status.lock);
        init_waitqueue_head(&s_cp_awake_status.wait);
}

void set_cp_awake(bool awake)
{
        unsigned long flags;
        bool last_status;

        spin_lock_irqsave(&s_cp_awake_status.lock, flags);
        last_status = s_cp_awake_status.awake;
        s_cp_awake_status.awake = awake;
        if(s_cp_awake_status.awake) {
                s_cp_awake_status.ack_time = jiffies;
        }
        spin_unlock_irqrestore(&s_cp_awake_status.lock, flags);

        if(!last_status && awake) {
                wake_up(&s_cp_awake_status.wait);
        }
}

bool get_cp_awake(void)
{
        unsigned long flags;
        bool ret = false;

        spin_lock_irqsave(&s_cp_awake_status.lock, flags);
        if(s_cp_awake_status.awake) {
                unsigned long diff = (long)jiffies - (long)s_cp_awake_status.ack_time;

                if(jiffies_to_msecs(diff) > MAX_SDIO_CP_AWAKE_TIMEOUT) {
                        s_cp_awake_status.awake = false;
                }
        }
        ret = s_cp_awake_status.awake;
        spin_unlock_irqrestore(&s_cp_awake_status.lock, flags);

        return ret;
}

bool wait_cp_awake(unsigned long timeout)
{
        if(get_cp_awake()) {
                return true;
        }
        if(!wait_event_timeout(s_cp_awake_status.wait,  s_cp_awake_status.awake,
                               timeout)) {
                printk("[MIPC] SDIO wait cp awake for a long time!\r\n");
        }

        return s_cp_awake_status.awake;
}

//MIPC_TX_CTRL_T ops
void init_mipc_tx_ctrl(void)
{
        memset(s_mipc_tx_ctrl.fifo, 0, sizeof(MIPC_TRANSF_FRAME_T*) * MAX_MIPC_TX_FIFO_SIZE);
        s_mipc_tx_ctrl.fifo_rd = 0;
        s_mipc_tx_ctrl.fifo_wr = 0;
        s_mipc_tx_ctrl.fifo_ack = 0;
        s_mipc_tx_ctrl.last_ack_frame = 0;
        s_tx_frame_counter = 1; //tx frame number start from 1
}

void put_to_sdio_tx_fifo(MIPC_TRANSF_FRAME_T* frame)
{
        s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_wr] = frame;
        smp_mb();
        s_mipc_tx_ctrl.fifo_wr = NEXT_TX_FIFO_IDX(s_mipc_tx_ctrl.fifo_wr);

        wake_up(&s_mux_ipc_sdio_wq);
}

MIPC_TRANSF_FRAME_T* get_from_sdio_tx_fifo(void)
{
        MIPC_TRANSF_FRAME_T* ret = NULL;

        if(s_mipc_tx_ctrl.fifo_rd != s_mipc_tx_ctrl.fifo_wr) {
                smp_mb();
                ret = s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_rd];
                smp_mb();
                s_mipc_tx_ctrl.fifo_rd = NEXT_TX_FIFO_IDX(s_mipc_tx_ctrl.fifo_rd);
        }

        return ret;
}

void ack_tx_frame_num(u32 frame_num)
{
        struct packet_header * pkt = NULL;

        while(s_mipc_tx_ctrl.fifo_ack != s_mipc_tx_ctrl.fifo_rd) {
                smp_mb();
                if(s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack]) {
                        pkt = (struct packet_header *)s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack]->buf_ptr;

                        if((long)(frame_num) - (long)(pkt->frame_num) >= 0) {
                                //calc tx payload
                                ipc_info_sdio_write_payload(pkt->length);
                                _FreeFrame(s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack], &s_mipc_tx_free_frame_list);

                                smp_mb();
                                s_mipc_tx_ctrl.fifo_ack = NEXT_TX_FIFO_IDX(s_mipc_tx_ctrl.fifo_ack);
                        } else {
                                break;
                        }
                } else {
                        printk("[MIPC] ERROR s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack] == 0!\r\n");
                        break;
                }
        }
        s_mipc_tx_ctrl.last_ack_frame = frame_num;
}


void nack_tx_frame_num()
{
        //test
        {
                u32 ack, rd;
                ack = s_mipc_tx_ctrl.fifo_ack;
                rd = s_mipc_tx_ctrl.fifo_rd;
                while(ack != rd) {
                        ipc_info_error_status(IPC_TX_CHANNEL, IPC_STATUS_INVALID_PACKET);
                        ack = NEXT_TX_FIFO_IDX(ack);
                }
        }

        printk("[mipc]before nack_tx_frame_num fifo_rd:%d\n", s_mipc_tx_ctrl.fifo_rd);
        s_mipc_tx_ctrl.fifo_rd = s_mipc_tx_ctrl.fifo_ack;
        printk("[mipc]nack_tx_frame_num fifo_rd:%d\n", s_mipc_tx_ctrl.fifo_rd);
}



bool implicit_ack_tx_frame(void)
{
        if(s_mipc_tx_ctrl.fifo_ack != s_mipc_tx_ctrl.fifo_rd) {
                if(!s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack]) {
                        printk("[MIPC] ERROR implicit_ack_tx_frame fifo[fifo_ack] == 0!\r\n");
                } else {
                        //calc tx payload
                        struct packet_header * pkt;
                        pkt = (struct packet_header *)s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack]->buf_ptr;
                        ipc_info_sdio_write_payload(pkt->length);

                        _FreeFrame(s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack], &s_mipc_tx_free_frame_list);
                        s_mipc_tx_ctrl.fifo_ack = NEXT_TX_FIFO_IDX(s_mipc_tx_ctrl.fifo_ack);
                        return true;
                }
        } else {
                printk("[MIPC] ERROR implicit_ack_tx_frame fifo_ack == fifo_rd!\r\n");
        }
        return false;
}

void free_all_tx_frame_in_fifo(void)
{
        //free written items
        while(s_mipc_tx_ctrl.fifo_ack != s_mipc_tx_ctrl.fifo_rd) {
                if(s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack]) {
                        _FreeFrame(s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack], &s_mipc_tx_free_frame_list);
                        s_mipc_tx_ctrl.fifo_ack = NEXT_TX_FIFO_IDX(s_mipc_tx_ctrl.fifo_ack);

                }
        }
        //free un-written items
        while(s_mipc_tx_ctrl.fifo_rd != s_mipc_tx_ctrl.fifo_wr) {
                if(s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_rd]) {
                        _FreeFrame(s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_rd], &s_mipc_tx_free_frame_list);
                        s_mipc_tx_ctrl.fifo_rd = NEXT_TX_FIFO_IDX(s_mipc_tx_ctrl.fifo_rd);

                }
        }
        s_mipc_tx_ctrl.fifo_ack = 0;
        s_mipc_tx_ctrl.fifo_rd = 0;
        s_mipc_tx_ctrl.fifo_wr = 0;
}

#define MUX_IPC_DISABLE    0
#define MUX_IPC_ENABLE    1
void  sdio_ipc_enable(u8  is_enable)
{
        if(!s_mux_ipc_module_inited) {
                printk("Error: mux ipc module is not initialized!\r\n");
                return;
        }

        if(is_enable) {
                kfifo_reset(&s_mipc_rx_cache_kfifo);
                _FreeAllTxTransferFrame();
                //sdhci_resetconnect(MUX_IPC_DISABLE);
                s_mux_ipc_event_flags = 0;
                s_mux_ipc_enable = MUX_IPC_ENABLE;
                wake_up_interruptible(&s_modem_ready);
        } else {
                s_mux_ipc_enable = MUX_IPC_DISABLE;
        }

        printk("[mipc]mux ipc enable:0x%x, status:0x%x\r\n", is_enable, s_mux_ipc_enable);
}

int mux_ipc_sdio_stop(int mode)
{
        if(mode & SPRDMUX_READ) {
                s_mux_ipc_event_flags |=  MUX_IPC_READ_DISABLE;
        }

        if(mode & SPRDMUX_WRITE) {
                s_mux_ipc_event_flags |=  MUX_IPC_WRITE_DISABLE;
        }

        wake_up_interruptible(&s_mux_read_rts);

        return 0;
}

static char s_dbg_buf[MAX_MIPC_TX_FRAME_SIZE];

#if 0

unsigned char s_dbg_tx_seed_v0 = 0;
unsigned char s_dbg_tx_seed_v1 = 0;


void set_tx_buf_dbg_data(unsigned char* payload, unsigned long length)
{
        int i = 0;
        unsigned char v0 = s_dbg_tx_seed_v0;
        unsigned char v1 = s_dbg_tx_seed_v1;
        unsigned char val;

        for(i = 0; i < length; i++) {

                val = v0 + v1 + 1;
                v0 = v1;
                v1 = val;

                payload[i] = val;
        }
        s_dbg_tx_seed_v0 = v0;
        s_dbg_tx_seed_v1 = v1;
}

unsigned char s_dbg_tx_chk_seed_v0 = 0;
unsigned char s_dbg_tx_chk_seed_v1 = 0;
unsigned long s_dbg_rx_data_cnt = 0;



void chk_tx_buf_dbg_data(unsigned char* payload, unsigned long length)
{
        int i = 0;
        unsigned char v0 = s_dbg_tx_chk_seed_v0;
        unsigned char v1 = s_dbg_tx_chk_seed_v1;
        unsigned char val;

        for(i = 0; i < length; i++) {

                if(s_dbg_rx_data_cnt == ((64 * 1024 - 32) * 8)) {
                        s_dbg_tx_chk_seed_v0 = 0;
                        s_dbg_tx_chk_seed_v1 = 0;
                        v0 = s_dbg_tx_chk_seed_v0;
                        v1 = s_dbg_tx_chk_seed_v1;
                        s_dbg_rx_data_cnt = 0;
                }
                val = v0 + v1 + 1;
                v0 = v1;
                v1 = val;

                if(payload[i] != val) {
                        while(1) {
                                printk("[mipc]chk_tx_buf_dbg_data error, rx_cnt:%d!\r\n", s_dbg_rx_data_cnt);
                                msleep(200);
                        }
                }
                ++s_dbg_rx_data_cnt;
        }
        s_dbg_tx_chk_seed_v0 = v0;
        s_dbg_tx_chk_seed_v1 = v1;
}

#endif

#if 0

int mux_ipc_sdio_write(const char *buf, size_t  count)
{
        static int first = 1;
        IPC_DBG("[mipc]mux_ipc_sdio_write write len:%d\r\n", count);
        if(first) {
                int cnt = 0;
                first = 0;
                wait_cp_bootup();
                while(1) {
                        set_tx_buf_dbg_data(s_dbg_buf, MAX_MIPC_TX_FRAME_SIZE  - sizeof(struct packet_header));
                        mux_ipc_xmit_buf(s_dbg_buf, MAX_MIPC_TX_FRAME_SIZE  - sizeof(struct packet_header));
                        //set_tx_buf_dbg_data(s_dbg_buf, 50);
                        //mux_ipc_xmit_buf(s_dbg_buf, 50);
                        cnt++;
                        if(cnt == MAX_MIPC_TX_FRAME_SIZE)
                                //if(cnt == 4)
                        {
                                msleep(100);
                                cnt = 0;
                        }
                }
        }

        return count;

        //return mux_ipc_xmit_buf(buf, count);
}
#endif

#if 1

int mux_ipc_sdio_write(const char *buf, size_t  count)
{
        wait_cp_bootup();
        IPC_DBG("[mipc]mux_ipc_sdio_write write len:%d\r\n", count);
        return mux_ipc_xmit_buf(buf, count);
}
#endif

#if 0

int mux_ipc_sdio_write(const char *buf, size_t  count)
{
        return count;
}
#endif

#if 0



int do_mux_ipc_sdio_read(char *buf, size_t  count)
{
        int ret = 0;

        wait_event_interruptible(s_mux_read_rts, !kfifo_is_empty(&s_mipc_rx_cache_kfifo) || s_mux_ipc_event_flags);

        if(s_mux_ipc_event_flags & MUX_IPC_READ_DISABLE) {
                printk("[mipc] mux ipc  read disable!\r\n");
                return -1;
        }

        /*IPC_DBG*/printk("[mipc]mux_ipc_sdio_read read len:%d\r\n", count);
        ret = kfifo_out(&s_mipc_rx_cache_kfifo,buf,count);

        ipc_info_mux_read(ret);

        ipc_info_sdio_read_saved_count(kfifo_len(&s_mipc_rx_cache_kfifo));

        //wake up rx thread
        wake_up_interruptible(&s_rx_fifo_avail);
        return ret;
}

#define DBG_RX_BUF_LEN (16 * 1024)

unsigned char s_dbg_rx_buf[DBG_RX_BUF_LEN];

int mux_ipc_sdio_test_read(char *buf, size_t  c)
{
        int ret = 0;

        do {

                ret = do_mux_ipc_sdio_read(s_dbg_rx_buf, DBG_RX_BUF_LEN);
                if(ret >= 0) {
                        chk_tx_buf_dbg_data(s_dbg_rx_buf, ret);
                }
                //printk("[mipc]mux_ipc_sdio_test_read sleep 10ms\r\n");
                //msleep(10);
        } while(ret >= 0);
}


#endif

#if 0
int mux_ipc_sdio_read(char *buf, size_t  count)
{
        int ret = 0;

        return ret;
}
#endif


#if 1
int mux_ipc_sdio_read(char *buf, size_t  count)
{
        int ret = 0;

        wait_event_interruptible(s_mux_read_rts, !kfifo_is_empty(&s_mipc_rx_cache_kfifo) || s_mux_ipc_event_flags);

        if(s_mux_ipc_event_flags & MUX_IPC_READ_DISABLE) {
                printk("[mipc] mux ipc  read disable!\r\n");
                return -1;
        }

        IPC_DBG("[mipc]mux_ipc_sdio_read read len:%d\r\n", count);
        ret = kfifo_out(&s_mipc_rx_cache_kfifo,buf,count);

        ipc_info_mux_read(ret);

        ipc_info_sdio_read_saved_count(kfifo_len(&s_mipc_rx_cache_kfifo));

        return ret;
}
#endif

static void _FrameList_Init(MIPC_FRAME_LIST_T* frame_list_ptr)
{
        if(!frame_list_ptr) {
                panic("_Init_Frame_List: frame_list_ptr=NULL!\r\n");
                return;
        }

        INIT_LIST_HEAD(&frame_list_ptr->frame_list_head);
        mutex_init(&frame_list_ptr->list_mutex);
        frame_list_ptr->counter = 0;
}

static void _TxFreeFrameList_Init(MIPC_FRAME_LIST_T* frame_list_ptr)
{
        u32 t = 0;
        MIPC_TRANSF_FRAME_T* frame_ptr = NULL;

        _FrameList_Init(frame_list_ptr);

        for(t = 0; t < MAX_MIPC_TX_FRAME_NUM;  t++) {
                frame_ptr = &s_tx_transfer_frame[t];
                frame_ptr->buf_size =  MAX_MIPC_TX_FRAME_SIZE;
                frame_ptr->buf_ptr = (u8*)kmalloc(frame_ptr->buf_size , GFP_KERNEL);
                frame_ptr->pos = 0;
                frame_ptr->flag = 0;
                list_add_tail(&frame_ptr->link, &frame_list_ptr->frame_list_head);
                frame_list_ptr->counter++;
        }

}

static void _TransferInit(MIPC_TRANSFER_T* transfer_ptr)
{
        if(!transfer_ptr) {
                panic("_Init_Frame_List: frame_list_ptr=NULL!\r\n");
                return;
        }

        INIT_LIST_HEAD(&transfer_ptr->frame_fifo);
        mutex_init(&transfer_ptr->transfer_mutex);
        transfer_ptr->counter = 0;
        transfer_ptr->cur_frame_ptr = NULL;
}

static void _transfer_frame_init(void)
{
        s_mipc_rx_buf =  (u8*) __get_free_pages(GFP_KERNEL, get_order(MAX_MIPC_RX_FRAME_SIZE));

        WARN_ON(NULL == s_mipc_rx_buf);

        if(kfifo_alloc(&s_mipc_rx_cache_kfifo,MAX_MIPC_RX_CACHE_SIZE, GFP_KERNEL)) {
                printk("_transfer_frame_init: kfifo rx cache no memory!\r\n");
                panic("%s[%d]kfifo rx cache no memory", __FILE__, __LINE__);
        }
        _TxFreeFrameList_Init(&s_mipc_tx_free_frame_list);
        _TransferInit(&s_mipc_tx_tansfer);

}

MIPC_TRANSF_FRAME_T* _AllocFrame(MIPC_FRAME_LIST_T*  frame_list_ptr)
{
        MIPC_TRANSF_FRAME_T*  frame_ptr = NULL;

        if(!frame_list_ptr) {
                panic("%s[%d] frame_list_ptr = NULL!\r\n", __FILE__, __LINE__);
        }

        mutex_lock(&frame_list_ptr->list_mutex);/*get  lock */
        if(!list_empty(&frame_list_ptr->frame_list_head)) {
                frame_ptr = list_entry(frame_list_ptr->frame_list_head.next, MIPC_TRANSF_FRAME_T, link);
                list_del(&frame_ptr->link);
                frame_ptr->pos = sizeof(struct packet_header);
                frame_ptr->flag = 1;
                frame_list_ptr->counter--;
        }
        mutex_unlock(&frame_list_ptr->list_mutex);/*set  lock */
        pr_debug("_AllocFrame:0x%X\r\n", (u32)frame_ptr);
        return  frame_ptr;
}

static void _FreeFrame(MIPC_TRANSF_FRAME_T* frame_ptr, MIPC_FRAME_LIST_T*  frame_list_ptr)
{
        if(!frame_list_ptr || !frame_ptr) {
                panic("%s[%d] frame_list_ptr = NULL!\r\n", __FILE__, __LINE__);
        }
        pr_debug("_FreeFrame:0x%x\r\n", (u32)frame_ptr);
        mutex_lock(&frame_list_ptr->list_mutex);/*get	lock */
        frame_ptr->pos = 0;
        frame_ptr->flag = 0;
        list_add_tail(&frame_ptr->link, &frame_list_ptr->frame_list_head);
        frame_list_ptr->counter++;
        mutex_unlock(&frame_list_ptr->list_mutex);/*set  lock */
}

static  void _AddFrameToTxTransferFifo(MIPC_TRANSF_FRAME_T* frame_ptr, MIPC_TRANSFER_T*  transfer_ptr)
{
        struct packet_header*	header_ptr = ( struct packet_header* )frame_ptr->buf_ptr;
        header_ptr->tag = 0x7e7f;
        header_ptr->flag = 0xaa55;
        header_ptr->length =	 frame_ptr->pos - sizeof(struct packet_header);
        header_ptr->frame_num = s_tx_frame_counter++; //sending_cnt;
        header_ptr->u.emergency_cmd = 0xabcdef00;
        header_ptr->reserved[0] = 0x11112222;
        header_ptr->reserved[1] = 0x33334444;
        header_ptr->reserved[2] = 0x55556666;
        header_ptr->reserved[3] = 0x77778888;
        list_add_tail(&frame_ptr->link,  &transfer_ptr->frame_fifo);
}

static  u32 _GetFrameFromTxTransfer(MIPC_TRANSF_FRAME_T* * out_frame_ptr)
{
        MIPC_TRANSF_FRAME_T* frame_ptr = NULL;
        MIPC_TRANSFER_T*  transfer_ptr =  &s_mipc_tx_tansfer;
        mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */
        if(!list_empty(&transfer_ptr->frame_fifo)) {
                frame_ptr = list_entry(transfer_ptr->frame_fifo.next, MIPC_TRANSF_FRAME_T, link);
                list_del(&frame_ptr->link);
        }
        mutex_unlock(&transfer_ptr->transfer_mutex);/*set  lock */

        if(!frame_ptr) {
                return 1;
        }

        *out_frame_ptr = frame_ptr;
        return 0;
}

static  u32  _IsTransferFifoEmpty(MIPC_TRANSFER_T* transfer_ptr)
{
        u32  ret = 0;
        mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */
        ret = list_empty(&transfer_ptr->frame_fifo);
        mutex_unlock(&transfer_ptr->transfer_mutex);/*set  lock */
        return ret;
}

u32 s_need_wake_up_tx_thread = 0;

static u32 _AddDataToTxTransfer(u8* data_ptr, u32 len)
{
        u32 ret = 0;
        MIPC_TRANSF_FRAME_T* frame_ptr = NULL;
        MIPC_TRANSF_FRAME_T* new_frame_ptr = NULL;
        MIPC_TRANSFER_T* transfer_ptr = &s_mipc_tx_tansfer;
        mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */

        if(0 == transfer_ptr->counter) {
                s_need_wake_up_tx_thread = 1;
        }
        do {
                frame_ptr = transfer_ptr->cur_frame_ptr;
                if(!frame_ptr) {
                        frame_ptr =  _AllocFrame(&s_mipc_tx_free_frame_list);
                }
                if(!frame_ptr) {
                        // printk("%s[%d]_AddDataToFrame No Empty Frame!\r\n", __FILE__, __LINE__);
                        ret = 0;
                        break;
                }
                if(len > (frame_ptr->buf_size - sizeof( struct packet_header))) {
                        printk("%s[%d]_AddDataToFrame	data too len!\r\n", __FILE__, __LINE__);
                        ret = 0;
                        break;
                }
                if((len + frame_ptr->pos) > frame_ptr->buf_size) {
                        new_frame_ptr =  _AllocFrame(&s_mipc_tx_free_frame_list);
                        if(!new_frame_ptr) {
                                // printk("%s[%d]_AddDataToFrame No Empty Frame : pos:%d, data len:%d\r\n", "ipc_sdio.c", __LINE__, frame_ptr->pos, len);
                                ret = 0;
                                break;
                        }
                        _AddFrameToTxTransferFifo(frame_ptr, transfer_ptr);
                        frame_ptr = new_frame_ptr;
                }
                memcpy(&frame_ptr->buf_ptr[frame_ptr->pos], data_ptr, len);
                frame_ptr->pos  += len;
                transfer_ptr->counter += len;
                transfer_ptr->cur_frame_ptr = frame_ptr;
                ret = len;
        } while(0);

        mutex_unlock(&transfer_ptr->transfer_mutex);/*set	lock */

        return ret;
}

static u32 _DelDataFromTxTransfer(MIPC_TRANSF_FRAME_T* frame_ptr)
{
        MIPC_TRANSFER_T* transfer_ptr  = &s_mipc_tx_tansfer;
        mutex_lock(&transfer_ptr->transfer_mutex);
        transfer_ptr->counter -=  frame_ptr->pos - sizeof(struct packet_header);
        mutex_unlock(&transfer_ptr->transfer_mutex);
        //_FreeFrame(frame_ptr, &s_mipc_tx_free_frame_list); //sdio opt 1, do not free here
        return 0;
}


static u32 _FreeAllTxTransferFrame(void)
{
        MIPC_TRANSF_FRAME_T*  frame_ptr = NULL;
        free_all_tx_frame_in_fifo();
        while(!_GetFrameFromTxTransfer(&frame_ptr)) {
                _DelDataFromTxTransfer(frame_ptr);
                _FreeFrame(frame_ptr, &s_mipc_tx_free_frame_list);
        }
        return 0;
}


static  u32 _FlushTxTransfer(void)
{
        u32 ret = 0;
        MIPC_TRANSFER_T* transfer_ptr  = &s_mipc_tx_tansfer;
        mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */
        do {
                if((transfer_ptr->counter == 0) || !transfer_ptr->cur_frame_ptr ||
                    (transfer_ptr->cur_frame_ptr->pos == sizeof( struct packet_header))) {
                        ret = 1;
                        break;
                }
                _AddFrameToTxTransferFifo(transfer_ptr->cur_frame_ptr, transfer_ptr);
                transfer_ptr->cur_frame_ptr = NULL;
        } while(0);
        mutex_unlock(&transfer_ptr->transfer_mutex);/*set	lock */
        return  ret;
}

u32 mux_ipc_GetTxTransferSavedCount(void)
{
        u32 count = 0;
        MIPC_TRANSFER_T* transfer_ptr  = &s_mipc_tx_tansfer;
        mutex_lock(&transfer_ptr->transfer_mutex);
        count = transfer_ptr->counter;
        mutex_unlock(&transfer_ptr->transfer_mutex);
        return count;
}
static size_t sdio_write_modem_data(const u8 * buf, u32 len)
{
        size_t ret = 0;
        u32  result =  SDHCI_TRANSFER_OK;
        u32 resend_count = 0;

        wake_lock(&s_ipc_sdio_wake_lock);

        do {
                printk("SDIO  WRITE START\n");

                ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_CONNECTED);
                ret = sprd_sdio_channel_tx(buf, len, (resend_count & ((1 << 17) - 1)));
                set_cp_awake(true);
                if(!ret) {
                        result =  SDHCI_TRANSFER_OK;
                        ret = len;
                } else {
                        ipc_info_error_status(IPC_TX_CHANNEL, IPC_STATUS_CRC_ERROR);
                        result =  SDHCI_TRANSFER_ERROR;
                        printk("SDIO  WRITE FAIL\n");
                        ret = 0;
                }


                ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_DISCONNECTED);
                if(result) {
                        resend_count++;
                        msleep(200);
                }
        } while(result && (resend_count < MAX_SDIO_TX_RETRY));

        wake_unlock(&s_ipc_sdio_wake_lock);

        ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_IDLE);
        if(resend_count >= MAX_SDIO_TX_RETRY) {
                while(1) {
                        msleep(2000);
                        printk("sdio_write_modem_data  write fail 3 times \n");
                }
        }

        return ret;

}

static int sdio_read_modem_data(u8 *buf, int len, int addr)
{
        int ret = 0;
        IPC_DBG(" sdio_read_modem_data entery:buf:0x%X, len:%x\r\n", (u32)buf, len);
        ret = sprd_sdio_channel_rx(buf, len, addr);
        return ret;
}

extern  __u8   mux_calc_crc(__u8 * data, __u32 length);

static bool VerifyPacketHeader(struct packet_header *header, u32 *tx_flow_info,
                               u32 *acked_tx_frame)
{
        if(0) { //(sdio_transfer_crc_check_enable)
                __u8*  data_ptr = (__u8*)(header + 1);
                __u8   crc_value = mux_calc_crc(data_ptr,  header->length );
                if ( (header->tag != HEADER_TAG) || ((header->flag & 0xFF00) != HEADER_TYPE)
                     || (header->length > MAX_MIPC_RX_FRAME_SIZE)
                     ||(header->frame_num != crc_value)) {
                        printk("[mipc]:%s error, tag:0x%X, type:0x%X, len:0x%X, crc:0x%X, calc_crc:0x%X\r\n",
                               __func__, header->tag , header->flag, header->length,header->frame_num, crc_value);
                        return false;
                }
        } else {
                if ( (header->tag != HEADER_TAG) || ((header->flag & 0xFF00) != HEADER_TYPE)
                     || (header->length > MAX_MIPC_RX_FRAME_SIZE)) {
                        printk("[mipc]:%s error, tag:0x%X, type:0x%X, len:0x%X, frame_num:0x%X\r\n",
                               __func__, header->tag , header->flag, header->length,header->frame_num);
                        return false;
                } else {
                        *tx_flow_info = header->flag & 0xFF;
                        *acked_tx_frame = header->u.ul_frame_num;
                }
        }

        return true;
}

static inline bool have_buffer_to_read(void)
{
        return (kfifo_avail(&s_mipc_rx_cache_kfifo) >= MAX_MIPC_RX_FRAME_SIZE);
}
u32  process_modem_packet(unsigned long data)
{
        u32  send_cnt = 0;

        if(!have_buffer_to_read()) {
                printk("[MIPC] MIPC Rx Cache Full!\r\n");
        }
        wait_event_interruptible(s_rx_fifo_avail, have_buffer_to_read());

        //send sdio read request
        printk("[MIPC] MIPC Rx Cache Free!\r\n");
        s_mipc_rx_event_flags = 0;
        s_mipc_rx_req_flag = 1;
        wake_up(&s_mux_ipc_sdio_wq);

        return 0;
}

unsigned long s_dbg_cp2ap_cnt = 0;
int s_dbg_cp2ap_last_val;

/*static int mux_test_cp2ap_rdy(void)
{
	int val;
	val = cp2ap_rdy();

	printk("[mipc_test] cp2ap_rdy value after write data:%d\r\n", val);
	if(s_dbg_cp2ap_cnt > 10)
	{
		if(s_dbg_cp2ap_last_val == val)
		{
			while(1)
			{
				printk("[mipc_test] s_dbg_cp2ap_last_val == val\r\n");
				msleep(20);
			}
		}
	}
	s_dbg_cp2ap_cnt++;
	s_dbg_cp2ap_last_val = val;

	return 0;
}*/

static int mux_ipc_tx_thread(void *data)
{
        MIPC_TRANSF_FRAME_T*  frame_ptr = NULL;
        struct sched_param	 param = {.sched_priority = 30};

        printk(KERN_INFO "mux_ipc_tx_thread");
        sched_setscheduler(current, SCHED_FIFO, &param);

        //msleep(10000);
        //while(1) msleep(500);


        while (!kthread_should_stop()) {
                wait_event(s_mux_ipc_tx_wq,  s_mipc_tx_tansfer.counter);

                if(!s_mipc_tx_tansfer.counter || (s_mipc_tx_tansfer.counter > MAX_MIPC_TX_FRAME_SIZE)) {
                        printk("count:%d\r\n", s_mipc_tx_tansfer.counter);
                }

                while(s_mipc_tx_tansfer.counter) {
                        //u32 prv_time = get_sys_cnt();
                        if(_IsTransferFifoEmpty(&s_mipc_tx_tansfer)) {
                                if(sdio_tx_wait_time > 0) {
                                        if(sdio_tx_wait_time > MAX_SDIO_TX_WAIT_TIMEOUT) {
                                                sdio_tx_wait_time = MAX_SDIO_TX_WAIT_TIMEOUT;
                                        }
                                        msleep(sdio_tx_wait_time);
                                }

                                if(_FlushTxTransfer()) {
                                        printk("No Data To Send\n");
                                        break;
                                }
                        }
                        do {
                                if(_GetFrameFromTxTransfer(&frame_ptr)) {
                                        printk("Error: Flush Empty Frame \n");
                                        break;
                                }

                                put_to_sdio_tx_fifo(frame_ptr);

                                _DelDataFromTxTransfer(frame_ptr);

                        } while(0);

                        //printk("[mipc] write data time:%d\r\n", (get_sys_cnt() - prv_time));
                        if(!_is_mux_ipc_enable()) {
                                printk("[mipc] ipc reset free all tx data!\r\n");
                                _FreeAllTxTransferFrame();
                                while(!_is_mux_ipc_enable()) {
                                        printk("mux ipc tx Thread Wait enable!\r\n");
                                        msleep(40);
                                }
                                printk("mux ipc tx Thread Wait enable Finished!\r\n");
                                break;
                        }
                }
        }

        sprd_sdio_channel_close();
        sdhci_hal_gpio_exit();

        return 0;
}



static ssize_t mux_ipc_xmit_buf(const char *buf, ssize_t len)
{
        ssize_t ret = 0;

        mutex_lock(&sdio_tx_lock);
        do {
                ret = _AddDataToTxTransfer((u8*)buf, len);
                if(!ret) {
                        ipc_info_sdio_write_overflow(1);
                        msleep(8);
                }
        } while(!ret);

        ipc_info_mux_write(len);

        if(s_need_wake_up_tx_thread) {
                s_need_wake_up_tx_thread = 0;
                wake_up(&s_mux_ipc_tx_wq);
        }
        mutex_unlock(&sdio_tx_lock);
        return ret;
}

static int mux_ipc_create_tx_thread(void)
{
        printk("mux_ipc_create_tx_thread enter.\n");
        init_waitqueue_head(&s_mux_ipc_tx_wq);
        s_mux_ipc_tx_thread = kthread_create(mux_ipc_tx_thread, NULL, "mipc_tx_thread");

        if (IS_ERR(s_mux_ipc_tx_thread)) {
                printk("mux_ipc_tx_thread error!.\n");
                return -1;
        }

        wake_up_process(s_mux_ipc_tx_thread);

        return 0;
}


u32  wake_up_mipc_rx_thread(u32   even_flag)
{
        u32 status = 0;
        u32 ipc_status = _is_mux_ipc_enable();
        if(ipc_status && even_flag) {
                s_mipc_rx_event_flags = even_flag;
                wake_up(&s_mux_ipc_rx_wq);
                status = 0;
        } else {
                printk("mux ipc rx invaild wakeup, ipc status:%u, flag:%d\r\n", ipc_status, even_flag);
                status = -1;
        }
        return  status;
}

static int mux_ipc_rx_thread(void *data)
{
        struct sched_param	 param = {.sched_priority = 25};
        printk(KERN_INFO "mux_ipc_rx_thread enter\r\n");
        sched_setscheduler(current, SCHED_FIFO, &param);

        wait_cp_bootup();
        msleep(500);

        printk(KERN_INFO "mux_ipc_rx_thread start----\r\n");
        //check CP status when start up
        if(0 == cp2ap_rdy()) {
                s_mipc_rx_event_flags = 1;
        }
        //working loop
        while (!kthread_should_stop()) {
                wait_event(s_mux_ipc_rx_wq,  s_mipc_rx_event_flags);
                process_modem_packet(0);
        }

        return 0;
}

static int mux_ipc_create_rx_thread(void)
{
        printk("mux_ipc_rx_create_thread enter.\n");
        init_waitqueue_head(&s_mux_ipc_rx_wq);
        s_mux_ipc_rx_thread = kthread_create(mux_ipc_rx_thread, NULL, "mipc_rx_thread");

        if (IS_ERR(s_mux_ipc_rx_thread)) {
                printk("ipc_sdio.c:mux_ipc_rx_thread error!.\n");
                return -1;
        }

        wake_up_process(s_mux_ipc_rx_thread);

        return 0;
}

unsigned char s_dbg_seed = 0;
static int do_sdio_rx(u32 *tx_flow_info, u32 *acked_tx_frame)
{
        u32 receve_len = 0;
        int ret = 0;
        int result = SDHCI_TRANSFER_OK;
        int resend_count = 0;
        struct packet_header *packet = NULL;

        wake_lock(&s_ipc_sdio_wake_lock);
        do {
                IPC_DBG(" sdio_read_modem_data xxxx................\r\n");

                ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_CONNECTED);

                memset(s_mipc_rx_buf, 0xaa, MAX_MIPC_RX_FRAME_SIZE);

                if(!have_buffer_to_read()) {
                        panic("do_sdio_rx have no buffer to read!\r\n");
                }

                ret = sdio_read_modem_data(s_mipc_rx_buf,  MAX_MIPC_RX_FRAME_SIZE, (resend_count != 0));
                set_cp_awake(true);
                if (!ret) {
                        //calc phy read speed
                        ipc_info_sdio_read(MAX_MIPC_RX_FRAME_SIZE);

                        packet=(struct packet_header *)s_mipc_rx_buf;
                        if(VerifyPacketHeader(packet, tx_flow_info, acked_tx_frame)) {
                                result = SDHCI_TRANSFER_OK;
                        } else {
                                ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_PACKET_ERROR);
                                result = SDHCI_TRANSFER_ERROR;
                                printk("[mipc] Sdio Rx Packet check error tag:0x%X, len:%d, ul_frame_num:0x%x\n", packet->tag, packet->length, packet->u.ul_frame_num);
                        }
                } else {
                        ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_CRC_ERROR);
                        result = SDHCI_TRANSFER_ERROR;
                        printk("[mipc]SDIO READ FAIL, ret:%d\r \n", ret);
                }


                ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_DISCONNECTED);

                if(result) {
                        resend_count++;
                        msleep(2);
                }

        } while(result && (resend_count < MAX_SDIO_RX_RETRY));

        ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_IDLE);

        wake_unlock(&s_ipc_sdio_wake_lock);

        if(!result) {
                u32  send_cnt = 0;

                ipc_info_rate(IPC_RX_CHANNEL, packet->length*1000/MAX_MIPC_RX_FRAME_SIZE);
                ipc_info_sdio_read_payload(packet->length);
                receve_len  = packet->length;

                printk("[mipc] read data len:%d\r\n", receve_len);

                if(sdio_transfer_frame_check_enable && sdio_frame_check(&s_mipc_rx_buf[sizeof(struct packet_header )], packet->length)) {
                        printk("[mipc]:sdio receved data frame error!\r\n");
                }

                if(packet->length) {
                        kfifo_in(&s_mipc_rx_cache_kfifo,&s_mipc_rx_buf[sizeof(struct packet_header )], packet->length);

                        wake_up_interruptible(&s_mux_read_rts);
                } else {
                        ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_INVALID_PACKET);
                }
        } else {
                receve_len  = 0;
                printk("[mipc] receive data fail! result:%d\r\n", result);
        }
        return result;
}


static int do_sdio_tx(MIPC_TRANSF_FRAME_T* frame_ptr)
{
        u32 write_len = 0;
        struct packet_header*	header_ptr = ( struct packet_header* )frame_ptr->buf_ptr;

        //write len shall multi - 512 bytes,and min len = 4096
        write_len = (frame_ptr->pos <= MIN_MIPC_TX_FRAME_SIZE) ?
                    MIN_MIPC_TX_FRAME_SIZE : ((frame_ptr->pos + 0x1FF) & (~0x1FF));

        s_last_tx_frame = header_ptr->frame_num;
        printk("[mipc] write data ptr: 0x%x, len:%d\r\n", frame_ptr->buf_ptr, frame_ptr->pos);
        write_len = sdio_write_modem_data(frame_ptr->buf_ptr, write_len);

        if(write_len) {
                ipc_info_rate(IPC_TX_CHANNEL, (frame_ptr->pos - sizeof(struct packet_header))*1000/write_len);
                ipc_info_sdio_write(write_len);
        }

        return write_len;
}

static int mux_ipc_sdio_thread(void *data)
{
        int rval;
        struct sched_param	 param = {.sched_priority = 35};
        u32							continue_tx_cnt = 0;
        printk(KERN_INFO "mux_ipc_sdio_thread enter\r\n");
        sched_setscheduler(current, SCHED_FIFO, &param);


        sdhci_hal_gpio_init();

        //while(1) msleep(500);
        wait_cp_bootup();
        do {
                rval = sprd_sdio_channel_open();
                printk(KERN_INFO "%s() sdio channel opened %d\n", __func__, rval);
        } while(rval);

        sdhci_hal_gpio_irq_init();

        printk(KERN_INFO "mux_ipc_sdio_thread start----\r\n");
        while (!kthread_should_stop()) {
                bool force_tx = false;

                wait_event_timeout(s_mux_ipc_sdio_wq, (s_mipc_rx_req_flag || MIPC_TX_REQ_FLAG),
                                   msecs_to_jiffies(MAX_SDIO_SLEEP_TIMEOUT));
                if(!(s_mipc_rx_req_flag || MIPC_TX_REQ_FLAG)) {
                        //set ap inactive to let cp sleep
                        ap2cp_rts_enable();
                        set_cp_awake(false);

                        //wait for requests
                        wait_event(s_mux_ipc_sdio_wq,  (s_mipc_rx_req_flag || MIPC_TX_REQ_FLAG));
                }

                //got sdio request
                //check modem status
                if(!_is_mux_ipc_enable()) {
                        printk("[mipc] ipc reset free all tx data!\r\n");
                        _FreeAllTxTransferFrame();
                        while(!_is_mux_ipc_enable()) {
                                printk("mux ipc tx Thread Wait enable!\r\n");
                                msleep(40);
                        }
                        printk("mux ipc tx Thread Wait enable Finished!\r\n");
                }



                //make sure cp is awake
                if(!get_cp_awake()) {
                        ap2cp_rts_disable();

                        //wait cp ack
                        if(!wait_cp_awake(msecs_to_jiffies(MAX_SDIO_CP_ACK_TIMEOUT))) {
                                printk("[MIPC] SDIO wait cp awake fail!\r\n");

                                //cp no ack, just discard a request
                                ap2cp_rts_enable();
                                msleep(2);

                                continue;

                        }
                        set_cp_awake(true);
                        if(MIPC_TX_REQ_FLAG) {
                                force_tx = true;
                        }
                }

                //do rx first, if not force to do tx
                if(s_mipc_rx_req_flag && !force_tx) {
                        int result;

                        result = do_sdio_rx(&s_tx_flow_info, &s_acked_tx_frame);
                        s_mipc_rx_req_flag = 0;
                        continue_tx_cnt = 0;

                        if(SDHCI_TRANSFER_OK == result) {
                                if(s_mipc_tx_ctrl.last_ack_frame != s_acked_tx_frame) {
                                        ack_tx_frame_num(s_acked_tx_frame);
                                }

                                if(s_acked_tx_frame != s_last_tx_frame) {
                                        nack_tx_frame_num();
                                }

                                if(s_tx_flow_info == 0) {
                                        ipc_info_error_status(IPC_TX_CHANNEL, IPC_STATUS_FLOW_STOP);
                                }
                        }

                }

                //do tx
                if(MIPC_TX_REQ_FLAG) {

                        MIPC_TRANSF_FRAME_T* frame = get_from_sdio_tx_fifo();

                        if(frame) {
                                do_sdio_tx(frame);
                                continue_tx_cnt++;
                        }

                        force_tx = false;
                }

                //check cp read request after one cmd53
                if(0 == cp2ap_rdy()) {
                        continue_tx_cnt = 0;
                        if(have_buffer_to_read()) {
                                s_mipc_rx_req_flag = 1;
                        } else {
                                wake_up_mipc_rx_thread(1);
                        }
                }

                if(continue_tx_cnt >= 2) {
                        continue_tx_cnt--;
                        implicit_ack_tx_frame();
                }



        }

        return 0;
}

static int mux_ipc_create_sdio_thread(void)
{
        printk("mux_ipc_create_sdio_thread enter.\n");
        init_waitqueue_head(&s_mux_ipc_sdio_wq);
        s_mux_ipc_sdio_thread = kthread_create(mux_ipc_sdio_thread, NULL, "mipc_sdio_thread");

        if (IS_ERR(s_mux_ipc_sdio_thread)) {
                printk("ipc_sdio.c:s_mux_ipc_sdio_thread error!.\n");
                return -1;
        }

        wake_up_process(s_mux_ipc_sdio_thread);

        return 0;
}

#if 0

static struct task_struct *s_mux_ipc_test_rx_thread;

static int mux_ipc_test_rx_thread(void *data)
{
        struct sched_param	 param = {.sched_priority = 15};
        printk(KERN_INFO "mux_ipc_sdio_thread enter\r\n");
        sched_setscheduler(current, SCHED_FIFO, &param);



        printk(KERN_INFO "mux_ipc_sdio_thread start----\r\n");
        while (!kthread_should_stop()) {
                mux_ipc_sdio_test_read(NULL, 0);
        }

}


static int mux_ipc_create_test_rx_thread(void)
{
        printk("mux_ipc_create_test_rx_thread enter.\n");
        s_mux_ipc_test_rx_thread = kthread_create(mux_ipc_test_rx_thread, NULL, "mipc_test_rx_thrd");

        if (IS_ERR(s_mux_ipc_test_rx_thread)) {
                printk("ipc_sdio.c:s_mux_ipc_sdio_thread error!.\n");
                return -1;
        }

        wake_up_process(s_mux_ipc_test_rx_thread);

        return 0;
}

#endif


static int modem_sdio_probe(struct platform_device *pdev)
{
        int retval = 0;

        printk("modem_sdio_probe\n");
        //debug{
        ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_FLOW_STOP);
        //}debug

        wake_lock_init(&s_ipc_sdio_wake_lock, WAKE_LOCK_SUSPEND, "ipc_sdio_lock");
        mutex_init(&ipc_mutex);
        init_cp_awake_status();
        init_mipc_tx_ctrl();
        mux_ipc_create_sdio_thread();
        mux_ipc_create_tx_thread();
        mux_ipc_create_rx_thread();

        //mux_ipc_create_test_rx_thread();
        return retval;
}

static int modem_sdio_remove(struct platform_device *pdev)
{
        return 0;
}


static struct platform_driver modem_sdio_driver = {
        .probe = modem_sdio_probe,
        .remove = modem_sdio_remove,
        .driver = {
                .name = "ipc_sdio",
        }
};

static struct sprdmux sprd_iomux = {
        .id		= 1,
        .io_read	= mux_ipc_sdio_read,
        .io_write	= mux_ipc_sdio_write,
        .io_stop  =  mux_ipc_sdio_stop,
};

static __iomem void *base_pinreg = (__iomem void *)SPRD_PIN_BASE;

void set_sdio_pins(void)
{
        //debug{
        __raw_writel(0x1b4, base_pinreg + 0x0444); //hot plug
        __raw_writel(0x1b4, base_pinreg + 0x0448); //ap req
        __raw_writel(0x1ba, base_pinreg + 0x044C); //cp rdy
        //}debug
}
static int __init mux_ipc_sdio_init(void)
{

        int retval;
        printk("mux_ipc_sdio_init\n");

        //set_sdio_pins();

        _transfer_frame_init();
        init_waitqueue_head(&s_mux_read_rts);
        init_waitqueue_head(&s_rx_fifo_avail);
        init_waitqueue_head(&s_modem_ready);
        sprdmux_register(&sprd_iomux);
        retval = platform_driver_register(&modem_sdio_driver);
        if (retval) {
                printk(KERN_ERR "[sdio]: register modem_sdio_driver error:%d\r\n", retval);
        }

        s_mux_ipc_module_inited = 1;

        return retval;
}

static void __exit mux_ipc_sdio_exit(void)
{
        platform_driver_unregister(&modem_sdio_driver);
}


module_param_named(sdio_transfer_crc_check_enable, sdio_transfer_crc_check_enable, int, S_IRUGO | S_IWUSR);
module_param_named(sdio_transfer_frame_check_enable, sdio_transfer_frame_check_enable, int, S_IRUGO | S_IWUSR);
module_param_named(sdio_tx_wait_time, sdio_tx_wait_time, int, S_IRUGO | S_IWUSR);



module_init(mux_ipc_sdio_init);
module_exit(mux_ipc_sdio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bin.Xu<bin.xu@spreadtrum.com>");
MODULE_DESCRIPTION("MUX SDIO Driver");
