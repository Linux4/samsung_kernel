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

//#define SDIO_LOOP_TEST
#define SDIO_VARIABLE_DL_LEN
//#define SC9620_JTAG_DL_MOD
#define HEADER_TAG 0x7e7f
#define HEADER_TYPE 0xaa00

#define IPC_DRIVER_NAME "IPC_SDIO"

#define MAX_SDIO_TX_WAIT_TIMEOUT    100

#define MAX_SDIO_CP_AWAKE_TIMEOUT    500
#define MAX_SDIO_SLEEP_TIMEOUT    50
#define MAX_SDIO_CP_ACK_TIMEOUT    1000
#define MAX_SDIO_TX_WATERMARK    4
#define MAX_SDIO_RX_RETRY		3
#define MAX_SDIO_TX_RETRY		3

#define SDIO_GPIO_RES_FREE			0
#define SDIO_GPIO_RES_USING		1


#define MAX_MIPC_RX_FRAME_SIZE    	(64*1024)
#define MAX_MIPC_TX_FRAME_SIZE    	(32*1024)

#define MIN_MIPC_TX_FRAME_SIZE 		(4 * 1024)

#define MAX_MIPC_RX_CACHE_SIZE   MAX_MIPC_RX_FRAME_SIZE*4

//#define MAX_MIPC_TX_FIFO_SIZE    (1 << 7)   // 128
#define MAX_MIPC_TX_FIFO_SIZE    (1 << 3)   // 16


#define MAX_MIPC_TX_FRAME_NUM    (MAX_MIPC_TX_FIFO_SIZE - 1)

#ifdef SDIO_VARIABLE_DL_LEN
#define DEFAULT_RX_FRAME_SIZE   (4*1024)
#else
#define DEFAULT_RX_FRAME_SIZE   MAX_MIPC_RX_FRAME_SIZE
#endif

#define NEXT_TX_FIFO_IDX(idx) (((idx) + 1) & (MAX_MIPC_TX_FIFO_SIZE - 1))

#define MIPC_TX_REQ_FLAG ((s_mipc_tx_ctrl.fifo_wr != s_mipc_tx_ctrl.fifo_rd) && s_tx_flow_info)



//#define IPC_DBG(f, x...) 	pr_debug(IPC_DRIVER_NAME " [%s()]: " f, __func__,## x)
#define IPC_DBG printk

/* Log Macros */
#define IPC_SDIO_INFO(fmt...)  pr_debug("IPC_SDIO: " fmt)
#define IPC_SDIO_DEBUG(fmt...) pr_debug("IPC_SDIO: " fmt)
#define IPC_SDIO_WARN(fmt...)   pr_warn("IPC_SDIO: " fmt)
#define IPC_SDIO_ERR(fmt...)   pr_err("IPC_SDIO: Error: " fmt)



struct packet_header {
        u16 tag;
        u16 flag;
        u32 length;
        u32 frame_num;
        union {
                u32 emergency_cmd;
                u32 ul_frame_num;
        } u;
        u32 next_length;
        u32 reserved[3];
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
        wait_queue_head_t                       frame_free_wq;
        MIPC_TRANSF_FRAME_T*        cur_frame_ptr;
        u32                                     frame_count;
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

u8* s_mipc_rx_buf = NULL;
u32 s_mipc_next_rx_len = DEFAULT_RX_FRAME_SIZE;


MIPC_TRANSF_FRAME_T   s_tx_transfer_frame[MAX_MIPC_TX_FRAME_NUM];

static struct wake_lock   s_ipc_sdio_wake_lock;
static struct wake_lock   s_cp_req_wake_lock;


static struct mutex ipc_mutex;
wait_queue_head_t s_mux_read_rts;
wait_queue_head_t s_rx_fifo_avail;
wait_queue_head_t s_modem_ready;
wait_queue_head_t s_sdio_gpio_res_free;


u32 s_mux_ipc_event_flags = 0;

static struct task_struct *s_mux_ipc_rx_thread;
static struct task_struct *s_mux_ipc_tx_thread;
static struct task_struct *s_mux_ipc_sdio_thread;


static  int   sdio_tx_wait_time = 1;
MIPC_FRAME_LIST_T  s_mipc_tx_free_frame_list;
MIPC_TRANSFER_T     s_mipc_tx_tansfer;

static u32  s_mux_ipc_enable = 0;
static u32  s_sdio_gpio_res_status = SDIO_GPIO_RES_FREE;

static wait_queue_head_t s_mux_ipc_tx_wq;
static wait_queue_head_t s_mux_ipc_rx_wq;



static u32  s_mux_ipc_module_inited = 0;

static int  sdio_transfer_crc_check_enable = 0;
static int  sdio_transfer_frame_check_enable = 0;

u32 s_mipc_rx_event_flags = 0;

static wait_queue_head_t s_mux_ipc_sdio_wq;
static u32 s_mipc_rx_req_flag = 0;
static u32 s_mipc_enable_change_flag = 0;

static CP_AWAKE_STATUS_T s_cp_awake_status;
//static u32 s_mipc_tx_above_watermark = 0;
static u32 s_tx_flow_info = 0xFF;
static u32 s_acked_tx_frame = 0;
static u32 s_last_tx_frame = 0;
static u32 s_tx_frame_counter; //tx frame number start from 1

static MIPC_TX_CTRL_T s_mipc_tx_ctrl;

//static atomic_t		s_mipc_read_pending;



static DEFINE_MUTEX(sdio_tx_lock);
static int mux_ipc_xmit_buf(const char *buf, ssize_t len);
static int mux_ipc_sdio_write( const char *buf, size_t  count);
static int mux_ipc_sdio_read(  char *buf, size_t  count);
static u32 _FreeAllTxTransferFrame(void);
static void _FreeFrame(MIPC_TRANSF_FRAME_T* frame_ptr, MIPC_FRAME_LIST_T*  frame_list_ptr);
static u32 _FlushTxTransfer(void);
static void _WaitTxTransferFree(void);
static void _WakeTxTransfer(void);



#include<mach/hardware.h>

/* use this method we can get time everytime */
#define TIMER_REG(off) (SPRD_TIMER_BASE + (off))
#define SYSCNT_REG(off) (SPRD_SYSCNT_BASE + (off))
#define SYSCNT_COUNT    SYSCNT_REG(0x0004)

/* no need double-reading */
#define SYSCNT_SHADOW_COUNT    SYSCNT_REG(0x000c)

#if 0
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
#endif

static  u32  _is_mux_ipc_enable(void)
{
#ifdef SC9620_JTAG_DL_MOD
        return 1;
#else
        return s_mux_ipc_enable;
#endif
}


void wait_cp_bootup(void)
{
        wait_event(s_modem_ready, _is_mux_ipc_enable());
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
                IPC_SDIO_ERR("[MIPC] SDIO wait cp awake for a long time!\r\n");
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
                        IPC_SDIO_ERR("[MIPC] ERROR s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack] == 0!\r\n");
                        break;
                }
        }
        s_mipc_tx_ctrl.last_ack_frame = frame_num;
}


void nack_tx_frame_num(void)
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

        IPC_SDIO_INFO("[mipc]before nack_tx_frame_num fifo_rd:%ld\n", s_mipc_tx_ctrl.fifo_rd);
        s_mipc_tx_ctrl.fifo_rd = s_mipc_tx_ctrl.fifo_ack;
        IPC_SDIO_INFO("[mipc]nack_tx_frame_num fifo_rd:%d\n", s_mipc_tx_ctrl.fifo_rd);
}



bool implicit_ack_tx_frame(void)
{
        if(s_mipc_tx_ctrl.fifo_ack != s_mipc_tx_ctrl.fifo_rd) {
                if(!s_mipc_tx_ctrl.fifo[s_mipc_tx_ctrl.fifo_ack]) {
                        IPC_SDIO_ERR("[MIPC] ERROR implicit_ack_tx_frame fifo[fifo_ack] == 0!\r\n");
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
                IPC_SDIO_ERR("[MIPC] ERROR implicit_ack_tx_frame fifo_ack == fifo_rd!\r\n");
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
                IPC_SDIO_ERR("Error: mux ipc module is not initialized!\r\n");
                return;
        }


        if(is_enable) {
                //msleep(1000);
                kfifo_reset(&s_mipc_rx_cache_kfifo);
                _FlushTxTransfer();
                _FreeAllTxTransferFrame();
                s_tx_flow_info = 0xFF;
                s_acked_tx_frame = 0;
                s_last_tx_frame = 0;
                s_tx_frame_counter = 1; //tx frame number start from 1
                s_mipc_next_rx_len = DEFAULT_RX_FRAME_SIZE;
                //sdhci_resetconnect(MUX_IPC_DISABLE);
                mutex_lock(&ipc_mutex);
                s_mux_ipc_event_flags = 0;
                mutex_unlock(&ipc_mutex);
                s_mipc_enable_change_flag = 0;
                s_mux_ipc_enable = MUX_IPC_ENABLE;
                wake_up(&s_modem_ready);
        } else {
                s_mux_ipc_enable = MUX_IPC_DISABLE;
                //wakeup sdio thread to release sdio_gpio_res
                s_mipc_enable_change_flag = 1;
                wake_up(&s_mux_ipc_sdio_wq);
                //wait all resources freed
                wait_event(s_sdio_gpio_res_free,
                                         (s_sdio_gpio_res_status == SDIO_GPIO_RES_FREE));
        }

        IPC_SDIO_WARN("[mipc]mux ipc enable:0x%x, status:0x%x\r\n", is_enable, s_mux_ipc_enable);
}

int mux_ipc_sdio_stop(int mode)
{
        mutex_lock(&ipc_mutex);
        if(mode & SPRDMUX_READ) {
                s_mux_ipc_event_flags |=  MUX_IPC_READ_DISABLE;
        }

        if(mode & SPRDMUX_WRITE) {
                s_mux_ipc_event_flags |=  MUX_IPC_WRITE_DISABLE;
        }
        mutex_unlock(&ipc_mutex);
        wake_up(&s_mux_read_rts);
        _WakeTxTransfer();
        return 0;
}

static char s_dbg_buf[MAX_MIPC_TX_FRAME_SIZE];

#ifdef SDIO_LOOP_TEST

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

                /*if(s_dbg_rx_data_cnt == ((64 * 1024 - 32) * 8)) {
                        s_dbg_tx_chk_seed_v0 = 0;
                        s_dbg_tx_chk_seed_v1 = 0;
                        v0 = s_dbg_tx_chk_seed_v0;
                        v1 = s_dbg_tx_chk_seed_v1;
                        s_dbg_rx_data_cnt = 0;
                }*/
                val = v0 + v1 + 1;
                v0 = v1;
                v1 = val;

                if(payload[i] != val) {
                        while(1) {
                                IPC_SDIO_ERR("[mipc]chk_tx_buf_dbg_data error, rx_cnt:%d!\r\n", s_dbg_rx_data_cnt);
                                msleep(200);
                        }
                }
                ++s_dbg_rx_data_cnt;
        }
        s_dbg_tx_chk_seed_v0 = v0;
        s_dbg_tx_chk_seed_v1 = v1;
}

int mux_ipc_sdio_write_test(const char *buf, size_t  count)
{
        static int first = 1;
        IPC_SDIO_INFO("[mipc]mux_ipc_sdio_write write len:%d\r\n", count);
        if(first) {
                int cnt = 0;
                first = 0;
                wait_cp_bootup();
                while(1) {
                        set_tx_buf_dbg_data(s_dbg_buf, MAX_MIPC_TX_FRAME_SIZE  - sizeof(struct packet_header));
                        mux_ipc_xmit_buf(s_dbg_buf, MAX_MIPC_TX_FRAME_SIZE  - sizeof(struct packet_header));

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

#ifndef SDIO_LOOP_TEST

int mux_ipc_sdio_write(const char *buf, size_t  count)
{
        if(s_mux_ipc_event_flags & MUX_IPC_WRITE_DISABLE) {
                IPC_SDIO_ERR("[mipc]mux_ipc_sdio_write write disabled!\r\n");
                return -1;
        }
        wait_cp_bootup();
        IPC_SDIO_INFO("[mipc]mux_ipc_sdio_write write len:%d\r\n", count);
        return mux_ipc_xmit_buf(buf, count);
}

#else

int mux_ipc_sdio_write(const char *buf, size_t  count)
{
        wait_cp_bootup();
        IPC_SDIO_INFO("[mipc]mux_ipc_sdio_write write len:%d\r\n", count);
        return count;
}
#endif

#ifdef SDIO_LOOP_TEST

int do_mux_ipc_sdio_read(char *buf, size_t  count)
{
        int ret = 0;

        wait_event(s_mux_read_rts, !kfifo_is_empty(&s_mipc_rx_cache_kfifo) || s_mux_ipc_event_flags);

        if(s_mux_ipc_event_flags & MUX_IPC_READ_DISABLE) {
                IPC_SDIO_ERR("[mipc] mux ipc  read disable!\r\n");
                return -1;
        }

        ret = kfifo_out(&s_mipc_rx_cache_kfifo,buf,count);
        IPC_SDIO_INFO("[mipc]mux_ipc_sdio_read read len:%d\r\n", ret);

        ipc_info_mux_read(ret);

        ipc_info_sdio_read_saved_count(kfifo_len(&s_mipc_rx_cache_kfifo));

        //wake up rx thread
        wake_up(&s_rx_fifo_avail);
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

        return 0;
}


#endif

#ifdef SDIO_LOOP_TEST
int mux_ipc_sdio_read(char *buf, size_t  count)
{
        int ret = 0;

        return ret;
}

#else

int mux_ipc_sdio_read(char *buf, size_t  count)
{
        int ret = 0;

        //incrase read count
        //atomic_inc(&s_mipc_read_pending);

        wait_event(s_mux_read_rts, !kfifo_is_empty(&s_mipc_rx_cache_kfifo) || s_mux_ipc_event_flags);

        //decrase read count
        //atomic_dec(&s_mipc_read_pending);
        if(s_mux_ipc_event_flags & MUX_IPC_READ_DISABLE) {
                IPC_SDIO_ERR("[mipc] mux ipc  read disable!\r\n");
                //if(atomic_read(&s_mipc_read_pending) == 0) {
                mutex_lock(&ipc_mutex);
                s_mux_ipc_event_flags = 0;
                mutex_unlock(&ipc_mutex);
                //}
                return -1;
        }


        ret = kfifo_out(&s_mipc_rx_cache_kfifo,buf,count);

        IPC_SDIO_INFO("[mipc]mux_ipc_sdio_read read len:%d\r\n", ret);

        ipc_info_mux_read(ret);

        ipc_info_sdio_read_saved_count(kfifo_len(&s_mipc_rx_cache_kfifo));

        //wake up rx thread
        wake_up(&s_rx_fifo_avail);

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
        transfer_ptr->frame_count = 0;
        init_waitqueue_head(&transfer_ptr->frame_free_wq);
}

static void _transfer_frame_init(void)
{
        s_mipc_rx_buf =  (u8*) __get_free_pages(GFP_KERNEL, get_order(MAX_MIPC_RX_FRAME_SIZE));

        WARN_ON(NULL == s_mipc_rx_buf);

        if(kfifo_alloc(&s_mipc_rx_cache_kfifo,MAX_MIPC_RX_CACHE_SIZE, GFP_KERNEL)) {
                IPC_SDIO_ERR("_transfer_frame_init: kfifo rx cache no memory!\r\n");
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
        IPC_SDIO_DEBUG("_AllocFrame:0x%X\r\n", (u32)frame_ptr);
        return  frame_ptr;
}

static void _FreeFrame(MIPC_TRANSF_FRAME_T* frame_ptr, MIPC_FRAME_LIST_T*  frame_list_ptr)
{
        if(!frame_list_ptr || !frame_ptr) {
                panic("%s[%d] frame_list_ptr = NULL!\r\n", __FILE__, __LINE__);
        }
        IPC_SDIO_DEBUG("_FreeFrame:0x%x\r\n", (u32)frame_ptr);
        mutex_lock(&frame_list_ptr->list_mutex);/*get	lock */
        frame_ptr->pos = 0;
        frame_ptr->flag = 0;
        list_add_tail(&frame_ptr->link, &frame_list_ptr->frame_list_head);
        frame_list_ptr->counter++;
        mutex_unlock(&frame_list_ptr->list_mutex);/*set  lock */
        _WakeTxTransfer();
}

static  void _AddFrameToTxTransferFifo(MIPC_TRANSF_FRAME_T* frame_ptr, MIPC_TRANSFER_T*  transfer_ptr)
{
        struct packet_header*	header_ptr = ( struct packet_header* )frame_ptr->buf_ptr;
        header_ptr->tag = 0x7e7f;
        header_ptr->flag = 0xaa55;
        header_ptr->length =	 frame_ptr->pos - sizeof(struct packet_header);
        header_ptr->frame_num = s_tx_frame_counter++; //sending_cnt;
        header_ptr->u.emergency_cmd = 0xabcdef00;
        header_ptr->next_length = 0xFFFFEEEE;
        header_ptr->reserved[0] = 0x11112222;
        header_ptr->reserved[1] = 0x33334444;
        header_ptr->reserved[2] = 0x55556666;
        list_add_tail(&frame_ptr->link,  &transfer_ptr->frame_fifo);
        transfer_ptr->frame_count++;
}

static  u32 _GetFrameFromTxTransfer(MIPC_TRANSF_FRAME_T* * out_frame_ptr)
{
        MIPC_TRANSF_FRAME_T* frame_ptr = NULL;
        MIPC_TRANSFER_T*  transfer_ptr =  &s_mipc_tx_tansfer;
        mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */
        if(!list_empty(&transfer_ptr->frame_fifo)) {
                frame_ptr = list_entry(transfer_ptr->frame_fifo.next, MIPC_TRANSF_FRAME_T, link);
                list_del(&frame_ptr->link);
                transfer_ptr->frame_count--;
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
                        IPC_SDIO_ERR("[mipc] _AddDataToFrame	data too long!\r\n");
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
                        s_need_wake_up_tx_thread = 1;
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

static void _WaitTxTransferFree(void)
{
        wait_event(s_mipc_tx_tansfer.frame_free_wq, (s_mipc_tx_free_frame_list.counter ||
                                 (s_mux_ipc_event_flags & MUX_IPC_WRITE_DISABLE)));
}

static void _WakeTxTransfer(void)
{
        wake_up(&s_mipc_tx_tansfer.frame_free_wq);
}

static u32 _DelDataFromTxTransfer(MIPC_TRANSF_FRAME_T* frame_ptr)
{
        MIPC_TRANSFER_T* transfer_ptr  = &s_mipc_tx_tansfer;
        mutex_lock(&transfer_ptr->transfer_mutex);
        transfer_ptr->counter -=  frame_ptr->pos - sizeof(struct packet_header);
        IPC_SDIO_DEBUG("[mipc] _DelDataFromTxTransfer transfer_ptr->counter = %d\n", transfer_ptr->counter);
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
                IPC_SDIO_INFO("SDIO  WRITE START\n");

                ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_CONNECTED);
                ret = sprd_sdio_channel_tx(buf, len, (resend_count & ((1 << 17) - 1)));
                if(!ret) {
                        set_cp_awake(true);
                        IPC_SDIO_INFO("SDIO  WRITE SUCESS\n");
                        result =  SDHCI_TRANSFER_OK;
                        ret = len;
                } else {
                        ipc_info_error_status(IPC_TX_CHANNEL, IPC_STATUS_CRC_ERROR);
                        result =  SDHCI_TRANSFER_ERROR;
                        IPC_SDIO_ERR("SDIO  WRITE FAIL ret= %d\n", ret);
                        ret = 0;
                }


                ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_DISCONNECTED);

                if(!_is_mux_ipc_enable()) {
                        IPC_SDIO_ERR("[mipc] Found mux ipc disabled in do_sdio_tx\r \n", ret);
                        break;
                }
                if(result) {
                        resend_count++;
                        msleep(200);
                }
        } while(result && (resend_count < MAX_SDIO_TX_RETRY));

        wake_unlock(&s_ipc_sdio_wake_lock);

        ipc_info_change_status(IPC_TX_CHANNEL, IPC_STATUS_IDLE);
        if(resend_count >= MAX_SDIO_TX_RETRY) {
#if 0
                while(1) {
                        msleep(2000);
                        printk("sdio_write_modem_data  write fail 3 times \n");
                }
#else
                IPC_SDIO_ERR("sdio_write_modem_data  write fail 3 times \n");
#endif
        }

        return ret;

}

static int sdio_read_modem_data(u8 *buf, int len, int addr)
{
        int ret = 0;
        IPC_SDIO_INFO("[mipc] sdio_read_modem_data entery:buf:0x%X, len:%x\r\n", (u32)buf, len);
        ret = sprd_sdio_channel_rx(buf, len, addr);
        return ret;
}

extern  __u8   mux_calc_crc(__u8 * data, __u32 length);

unsigned long s_dbg_dl_frame_num = 0;
static bool VerifyPacketHeader(struct packet_header *header, u32 *tx_flow_info,
                               u32 *acked_tx_frame)
{
        if(0) { //(sdio_transfer_crc_check_enable)
                __u8*  data_ptr = (__u8*)(header + 1);
                __u8   crc_value = mux_calc_crc(data_ptr,  header->length );
                if ( (header->tag != HEADER_TAG) || ((header->flag & 0xFF00) != HEADER_TYPE)
                     || (header->length > MAX_MIPC_RX_FRAME_SIZE)
                     ||(header->frame_num != crc_value)) {
                        IPC_SDIO_ERR("[mipc]:%s error, tag:0x%X, type:0x%X, len:0x%X, crc:0x%X, calc_crc:0x%X\r\n",
                                     __func__, header->tag , header->flag, header->length,header->frame_num, crc_value);
                        return false;
                }
        } else {
                if ( (header->tag != HEADER_TAG) || ((header->flag & 0xFF00) != HEADER_TYPE)
                     || (header->length > MAX_MIPC_RX_FRAME_SIZE)) {
                        IPC_SDIO_ERR("[mipc]:%s error, tag:0x%X, type:0x%X, len:0x%X, frame_num:0x%X\r\n",
                                     __func__, header->tag , header->flag, header->length,header->frame_num);
                        return false;
                } else {
                        *tx_flow_info = header->flag & 0xFF;
                        *acked_tx_frame = header->u.ul_frame_num;
                        if((s_dbg_dl_frame_num + 1) != header->frame_num) {
                                IPC_SDIO_ERR("[mipc]:(s_dbg_dl_frame_num + 1) != header->frame_num!!\r\n");
                        }
                        s_dbg_dl_frame_num = header->frame_num;
                        IPC_SDIO_DEBUG("[mipc]:s_dbg_dl_frame_num:%d!\r\n", s_dbg_dl_frame_num);
                        s_mipc_next_rx_len = header->next_length;
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
        if(!have_buffer_to_read()) {
                IPC_SDIO_ERR("[MIPC] MIPC Rx Cache Full!\r\n");
        }
        wait_event(s_rx_fifo_avail, have_buffer_to_read());

        //send sdio read request
        IPC_SDIO_DEBUG("[MIPC] MIPC Rx Cache Free!\r\n");
        s_mipc_rx_event_flags = 0;
        s_mipc_rx_req_flag = 1;
        wake_up(&s_mux_ipc_sdio_wq);

        return 0;
}

#ifdef SDIO_GPIO_TEST

unsigned long s_dbg_cp2ap_cnt = 0;
int s_dbg_cp2ap_last_val;

static int mux_test_cp2ap_rdy(void)
{
        int val;
        val = cp2ap_req();

        IPC_SDIO_DEBUG("[mipc_test] cp2ap_req value after write data:%d\r\n", val);
        if(s_dbg_cp2ap_cnt > 10) {
                if(s_dbg_cp2ap_last_val == val) {
                        while(1) {
                                IPC_SDIO_ERR("[mipc_test] s_dbg_cp2ap_last_val == val\r\n");
                                msleep(20);
                        }
                }
        }
        s_dbg_cp2ap_cnt++;
        s_dbg_cp2ap_last_val = val;

        return 0;
}

#endif

#define TX_THRD_GATHER_COND (s_mipc_tx_tansfer.frame_count || ((s_mipc_tx_ctrl.fifo_wr == s_mipc_tx_ctrl.fifo_rd)&& s_tx_flow_info))
static int mux_ipc_tx_thread(void *data)
{
        MIPC_TRANSF_FRAME_T*  frame_ptr = NULL;
        struct sched_param	 param = {.sched_priority = 10};

        IPC_SDIO_INFO("mux_ipc_tx_thread");
        sched_setscheduler(current, SCHED_FIFO, &param);


        while (!kthread_should_stop()) {
                wait_event(s_mux_ipc_tx_wq,  s_mipc_tx_tansfer.counter);

                if(!s_mipc_tx_tansfer.counter || (s_mipc_tx_tansfer.counter > MAX_MIPC_TX_FRAME_SIZE)) {
                        IPC_SDIO_DEBUG("[mipc] s_mipc_tx_tansfer.counter:%d\r\n", s_mipc_tx_tansfer.counter);
                }

                while(s_mipc_tx_tansfer.counter) {
                        if(_IsTransferFifoEmpty(&s_mipc_tx_tansfer)) {
                                //do not send right now, because now sdio is pending
                                wait_event(s_mux_ipc_tx_wq, TX_THRD_GATHER_COND);
                                IPC_SDIO_DEBUG("[mipc] tx thread wait TX_THRD_GATHER_COND OK!\r\n");
                        }
                        do {

                                if(_IsTransferFifoEmpty(&s_mipc_tx_tansfer)) {
                                        if(_FlushTxTransfer()) {
                                                IPC_SDIO_DEBUG("[mipc] s_mipc_tx_tansfer.counter: %d\n", s_mipc_tx_tansfer.counter);
                                                IPC_SDIO_DEBUG("[mipc] transfer_ptr->cur_frame_ptr: 0x%X\n", s_mipc_tx_tansfer.cur_frame_ptr);
                                                if(s_mipc_tx_tansfer.cur_frame_ptr) {
                                                        IPC_SDIO_DEBUG("[mipc] cur_frame_ptr->pos: %d\n", s_mipc_tx_tansfer.cur_frame_ptr->pos);
                                                }
                                                IPC_SDIO_WARN("[mipc] No Data To Send\n");
                                                break;
                                        }
                                }

                                if(_GetFrameFromTxTransfer(&frame_ptr)) {
                                        IPC_SDIO_ERR("[mipc] Error: Flush Empty Frame \n");
                                        break;
                                }

                                _DelDataFromTxTransfer(frame_ptr);

                                IPC_SDIO_DEBUG("[mipc] sending frame count: %d\n", (MAX_MIPC_TX_FRAME_NUM - s_mipc_tx_free_frame_list.counter));
                                put_to_sdio_tx_fifo(frame_ptr);
                        } while(0);

                }
        }

        sprd_sdio_channel_close();
        sdhci_hal_gpio_exit();

        return 0;
}



static int mux_ipc_xmit_buf(const char *buf, ssize_t len)
{
        ssize_t ret = -1;

        mutex_lock(&sdio_tx_lock);
        do {
                ret = _AddDataToTxTransfer((u8*)buf, len);
                if(!ret) {
                        ipc_info_sdio_write_overflow(1);
                        IPC_SDIO_WARN("[mipc] mux_ipc_xmit_buf begin _WaitTxTransferFree\n");
                        _WaitTxTransferFree();
                        IPC_SDIO_WARN("[mipc] mux_ipc_xmit_buf end _WaitTxTransferFree\n");
                        if(s_mux_ipc_event_flags & MUX_IPC_WRITE_DISABLE) {
                                break;
                        }
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
        IPC_SDIO_INFO("mux_ipc_create_tx_thread enter.\n");
        init_waitqueue_head(&s_mux_ipc_tx_wq);
        s_mux_ipc_tx_thread = kthread_create(mux_ipc_tx_thread, NULL, "mipc_tx_thread");

        if (IS_ERR(s_mux_ipc_tx_thread)) {
                IPC_SDIO_ERR("mux_ipc_tx_thread error!.\n");
                return -1;
        }

        wake_up_process(s_mux_ipc_tx_thread);

        return 0;
}


u32  wake_up_mipc_rx_thread(u32   even_flag)
{
        u32 status = 0;
        u32 ipc_status = _is_mux_ipc_enable();

        wake_lock_timeout(&s_cp_req_wake_lock, HZ / 2);
        if(ipc_status && even_flag) {
                s_mipc_rx_event_flags = even_flag;
                wake_up(&s_mux_ipc_rx_wq);
                status = 0;
        } else {
                IPC_SDIO_ERR("mux ipc rx invaild wakeup, ipc status:%u, flag:%d\r\n", ipc_status, even_flag);
                status = -1;
        }
        return  status;
}

static int mux_ipc_rx_thread(void *data)
{
        struct sched_param	 param = {.sched_priority = 25};
        IPC_SDIO_INFO("mux_ipc_rx_thread enter\r\n");
        sched_setscheduler(current, SCHED_FIFO, &param);

        wait_cp_bootup();
        msleep(500);

        IPC_SDIO_INFO("mux_ipc_rx_thread start----\r\n");
        //check CP status when start up
        if(cp2ap_req()) {
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
        IPC_SDIO_INFO("mux_ipc_rx_create_thread enter.\n");
        init_waitqueue_head(&s_mux_ipc_rx_wq);
        s_mux_ipc_rx_thread = kthread_create(mux_ipc_rx_thread, NULL, "mipc_rx_thread");

        if (IS_ERR(s_mux_ipc_rx_thread)) {
                IPC_SDIO_ERR("ipc_sdio.c:mux_ipc_rx_thread error!.\n");
                return -1;
        }

        wake_up_process(s_mux_ipc_rx_thread);

        return 0;
}

static int do_sdio_rx(u32 *tx_flow_info, u32 *acked_tx_frame)
{
        u32 receve_len = 0;
        int ret = 0;
        int result = SDHCI_TRANSFER_OK;
        int resend_count = 0;
        struct packet_header *packet = NULL;
        u32 rx_buf_len;

        rx_buf_len = s_mipc_next_rx_len;
        wake_lock(&s_ipc_sdio_wake_lock);
        do {
                IPC_SDIO_DEBUG(" sdio_read_modem_data xxxx................\r\n");

                ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_CONNECTED);

                memset(s_mipc_rx_buf, 0xaa, MAX_MIPC_RX_FRAME_SIZE);

                if(!have_buffer_to_read()) {
                        panic("do_sdio_rx have no buffer to read!\r\n");
                }

#ifdef SDIO_VARIABLE_DL_LEN
                ret = sdio_read_modem_data(s_mipc_rx_buf,  rx_buf_len, (resend_count != 0));
#else
                ret = sdio_read_modem_data(s_mipc_rx_buf,  MAX_MIPC_RX_FRAME_SIZE, (resend_count != 0));
#endif
                if (!ret) {
                        set_cp_awake(true);
                        //calc phy read speed
#ifdef SDIO_VARIABLE_DL_LEN
                        ipc_info_sdio_read(rx_buf_len);
#else
                        ipc_info_sdio_read(MAX_MIPC_RX_FRAME_SIZE);
#endif



                        packet=(struct packet_header *)s_mipc_rx_buf;
                        if(VerifyPacketHeader(packet, tx_flow_info, acked_tx_frame)) {
                                result = SDHCI_TRANSFER_OK;
                        } else {
                                ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_PACKET_ERROR);
                                result = SDHCI_TRANSFER_ERROR;
                                IPC_SDIO_ERR("[mipc] Sdio Rx Packet check error tag:0x%X, len:%d, ul_frame_num:0x%x\n", packet->tag, packet->length, packet->u.ul_frame_num);
                        }
#ifdef SDIO_VARIABLE_DL_LEN
                        if (packet->length > rx_buf_len) {
                                result = SDHCI_TRANSFER_ERROR;
                                IPC_SDIO_ERR("[mipc]SDIO READ FAIL, packet->length(%d) > rx_buf_len(%d)\r \n", packet->length, rx_buf_len);
                                rx_buf_len = MAX_MIPC_RX_FRAME_SIZE;
                        }
#endif
                } else {
                        ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_CRC_ERROR);
                        result = SDHCI_TRANSFER_ERROR;
                        IPC_SDIO_ERR("[mipc]SDIO READ FAIL, ret:%d\r \n", ret);
                }


                ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_DISCONNECTED);

                if(!_is_mux_ipc_enable()) {
                        IPC_SDIO_ERR("[mipc] Found mux ipc disabled in do_sdio_rx\r \n", ret);
                        break;
                }
                if(result) {
                        resend_count++;
                        msleep(2);
                }

        } while(result && (resend_count < MAX_SDIO_RX_RETRY));

        ipc_info_change_status(IPC_RX_CHANNEL, IPC_STATUS_IDLE);

        wake_unlock(&s_ipc_sdio_wake_lock);

        if(!result) {

                ipc_info_rate(IPC_RX_CHANNEL, packet->length*1000/MAX_MIPC_RX_FRAME_SIZE);
                ipc_info_sdio_read_payload(packet->length);
                receve_len  = packet->length;

                IPC_SDIO_INFO("[mipc] read data len:%d\r\n", receve_len);

                if(sdio_transfer_frame_check_enable && sdio_frame_check(&s_mipc_rx_buf[sizeof(struct packet_header )], packet->length)) {
                        IPC_SDIO_ERR("[mipc]:sdio receved data frame error!\r\n");
                }

                if(packet->length) {
                        kfifo_in(&s_mipc_rx_cache_kfifo,&s_mipc_rx_buf[sizeof(struct packet_header )], packet->length);

                        wake_up(&s_mux_read_rts);
                } else {
                        ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_INVALID_PACKET);
                }
        } else {
                receve_len  = 0;
                IPC_SDIO_ERR("[mipc] receive data fail! result:%d\r\n", result);
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
        IPC_SDIO_INFO("[mipc] write data ptr: 0x%x, len:%d\r\n", frame_ptr->buf_ptr, frame_ptr->pos);

        IPC_SDIO_DEBUG("[mipc] write data: 0x%x, 0x%x, 0x%x, 0x%x,\r\n", frame_ptr->buf_ptr[8],
                       frame_ptr->buf_ptr[9],
                       frame_ptr->buf_ptr[10],
                       frame_ptr->buf_ptr[11]);
        write_len = sdio_write_modem_data(frame_ptr->buf_ptr, write_len);

        if(write_len) {
                ipc_info_rate(IPC_TX_CHANNEL, (frame_ptr->pos - sizeof(struct packet_header))*1000/write_len);
                ipc_info_sdio_write(write_len);
        }

        return write_len;
}

static void init_sdio_gpio_res()
{
        int rval;

        if(s_sdio_gpio_res_status != SDIO_GPIO_RES_USING) {
                sdhci_hal_gpio_init();
                do {
                        rval = sprd_sdio_channel_open();
                        IPC_SDIO_INFO("%s() sdio channel opened %d\n", __func__, rval);
                } while(rval);

                sdhci_hal_gpio_irq_init();
                s_sdio_gpio_res_status = SDIO_GPIO_RES_USING;
        }

}

static void free_sdio_gpio_res()
{
        if(s_sdio_gpio_res_status != SDIO_GPIO_RES_FREE) {
                sdhci_hal_gpio_exit();
                sprd_sdio_channel_close();
                s_sdio_gpio_res_status = SDIO_GPIO_RES_FREE;
                wake_up(&s_sdio_gpio_res_free);
        }
}

static int mux_ipc_sdio_thread(void *data)
{
        struct sched_param	 param = {.sched_priority = 35};
        u32							continue_tx_cnt = 0;
        IPC_SDIO_INFO("mux_ipc_sdio_thread enter\r\n");
        sched_setscheduler(current, SCHED_FIFO, &param);

        IPC_SDIO_INFO("mux_ipc_sdio_thread start----\r\n");
        while (!kthread_should_stop()) {
                bool force_tx = false;

                //check mux_ipc driver is enabled
                if(!_is_mux_ipc_enable()) {

                        IPC_SDIO_WARN("[mipc] found s_mux_ipc_enable == false\r\n");

                        free_sdio_gpio_res();

                        IPC_SDIO_WARN("[mipc] start to wait bootup in mux_ipc_sdio_thread\r\n");

                        wait_cp_bootup();

                        IPC_SDIO_WARN("[mipc] wait bootup success in mux_ipc_sdio_thread\r\n");

                        init_sdio_gpio_res();
                }

                wait_event_timeout(s_mux_ipc_sdio_wq, (s_mipc_enable_change_flag || s_mipc_rx_req_flag || MIPC_TX_REQ_FLAG),
                                   msecs_to_jiffies(MAX_SDIO_SLEEP_TIMEOUT));
                if(s_mipc_enable_change_flag) {
                        s_mipc_enable_change_flag = 0;
                        continue;
                }

                IPC_SDIO_DEBUG("[mipc] wait_event_timeout rx_req tx_req\r\n");
                if(!(s_mipc_rx_req_flag || MIPC_TX_REQ_FLAG)) {
                        //set ap inactive to let cp sleep
                        ap2cp_req_disable();
                        set_cp_awake(false);

                        //wait for requests
                        IPC_SDIO_INFO("[mipc] start wait_event rx_req tx_req forever\r\n");
                        wait_event(s_mux_ipc_sdio_wq,  (s_mipc_enable_change_flag || s_mipc_rx_req_flag || MIPC_TX_REQ_FLAG));
                        if(s_mipc_enable_change_flag) {
                                s_mipc_enable_change_flag = 0;
                                continue;
                        }
                }

                //check modem status
                if(!_is_mux_ipc_enable()) {
                        continue;
                }

		//set ap2cp active before transfer
		ap2cp_req_enable();
                //make sure cp is awake
                if(!get_cp_awake()) {

                        //wait cp ack
                        if(!wait_cp_awake(msecs_to_jiffies(MAX_SDIO_CP_ACK_TIMEOUT))) {
                                IPC_SDIO_ERR("[MIPC] SDIO wait cp awake fail!\r\n");

                                //cp no ack, just discard a request
                                ap2cp_req_disable();
                                msleep(2);

                                continue;

                        }
                        set_cp_awake(true);
                        if(MIPC_TX_REQ_FLAG) {
                                force_tx = true;
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
                //check modem status
                if(!_is_mux_ipc_enable()) {
                        continue;
                }

                //do tx,check again
                if(MIPC_TX_REQ_FLAG) {

                        MIPC_TRANSF_FRAME_T* frame = get_from_sdio_tx_fifo();

                        if(frame) {
                                do_sdio_tx(frame);
                                continue_tx_cnt++;
                        }

                        force_tx = false;
                }
                //check modem status
                if(!_is_mux_ipc_enable()) {
                        continue;
                }

                //do rx
                if(s_mipc_rx_req_flag && !force_tx) {
                        int result;
                        u32 last_tx_flow_info = s_tx_flow_info;

                        result = do_sdio_rx(&s_tx_flow_info, &s_acked_tx_frame);
                        s_mipc_rx_req_flag = 0;
                        continue_tx_cnt = 0;

                        if(SDHCI_TRANSFER_OK == result) {
                                //out of flow ctrl, wake up tx thread
                                if(!last_tx_flow_info && s_tx_flow_info) {
                                        wake_up(&s_mux_ipc_tx_wq);
                                        printk("[mipc] exit ul flow ctrl, s_tx_flow_info:%d\n", s_tx_flow_info);
                                } else if(!s_tx_flow_info && last_tx_flow_info) {
                                        printk("[mipc] enter ul flow ctrl,s_tx_flow_info:%d\n", s_tx_flow_info);
                                }

                                IPC_SDIO_DEBUG("[mipc] last_ack_frame:%d, s_acked_tx_frame:%d, s_last_tx_frame:%d\n",
                                               s_mipc_tx_ctrl.last_ack_frame,
                                               s_acked_tx_frame,
                                               s_last_tx_frame);

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

                //check modem status
                if(!_is_mux_ipc_enable()) {
                        continue;
                }



                //check cp read request after one cmd53
                if(cp2ap_req()) {
                        continue_tx_cnt = 0;
                        if(have_buffer_to_read()) {
                                s_mipc_rx_req_flag = 1;
                        } else {
                                wake_up_mipc_rx_thread(1);
                        }
                }

                if(continue_tx_cnt >= 2) {
                        continue_tx_cnt--;
                        //implicit_ack_tx_frame();
                }

                //check need to wake tx_thread, nothing to send, wake up tx thread
                if((s_mipc_tx_ctrl.fifo_wr == s_mipc_tx_ctrl.fifo_rd)
                    && s_tx_flow_info
                    && s_mipc_tx_tansfer.counter) {
                        wake_up(&s_mux_ipc_tx_wq);
                }

        }

        return 0;
}

static int mux_ipc_create_sdio_thread(void)
{
        IPC_SDIO_INFO("mux_ipc_create_sdio_thread enter.\n");
        init_waitqueue_head(&s_mux_ipc_sdio_wq);
        s_mux_ipc_sdio_thread = kthread_create(mux_ipc_sdio_thread, NULL, "mipc_sdio_thread");

        if (IS_ERR(s_mux_ipc_sdio_thread)) {
                IPC_SDIO_ERR("ipc_sdio.c:s_mux_ipc_sdio_thread error!.\n");
                return -1;
        }

        wake_up_process(s_mux_ipc_sdio_thread);

        return 0;
}

#ifdef SDIO_LOOP_TEST

static struct task_struct *s_mux_ipc_test_rx_thread;
static struct task_struct *s_mux_ipc_test_tx_thread;


static int mux_ipc_test_rx_thread(void *data)
{
        struct sched_param	 param = {.sched_priority = 15};
        IPC_SDIO_INFO("mux_ipc_test_rx_thread enter\r\n");
        sched_setscheduler(current, SCHED_FIFO, &param);



        IPC_SDIO_INFO("mux_ipc_test_rx_thread start----\r\n");
        while (!kthread_should_stop()) {
                mux_ipc_sdio_test_read(NULL, 0);
        }
        return 0;
}

static int mux_ipc_test_tx_thread(void *data)
{
        struct sched_param	 param = {.sched_priority = 15};
        IPC_SDIO_INFO("mux_ipc_test_tx_thread enter\r\n");
        sched_setscheduler(current, SCHED_FIFO, &param);



        IPC_SDIO_INFO("mux_ipc_test_tx_thread start----\r\n");

        msleep(100 * 1000);
        while (!kthread_should_stop()) {
                mux_ipc_sdio_write_test(NULL, 0);
                msleep(200);
        }

        return 0;
}

static int mux_ipc_create_test_tx_thread(void)
{
        IPC_SDIO_INFO("mux_ipc_create_test_tx_thread enter.\n");
        s_mux_ipc_test_tx_thread = kthread_create(mux_ipc_test_tx_thread, NULL, "mipc_test_tx_thrd");

        if (IS_ERR(s_mux_ipc_test_tx_thread)) {
                IPC_SDIO_ERR("ipc_sdio.c:s_mux_ipc_sdio_thread error!.\n");
                return -1;
        }

        wake_up_process(s_mux_ipc_test_tx_thread);

        return 0;
}

static int mux_ipc_create_test_rx_thread(void)
{
        IPC_SDIO_INFO("mux_ipc_create_test_rx_thread enter.\n");
        s_mux_ipc_test_rx_thread = kthread_create(mux_ipc_test_rx_thread, NULL, "mipc_test_rx_thrd");

        if (IS_ERR(s_mux_ipc_test_rx_thread)) {
                IPC_SDIO_ERR("ipc_sdio.c:s_mux_ipc_sdio_thread error!.\n");
                return -1;
        }

        wake_up_process(s_mux_ipc_test_rx_thread);

        return 0;
}

#endif


static int modem_sdio_probe(struct platform_device *pdev)
{
        int retval = 0;

        IPC_SDIO_INFO("modem_sdio_probe\n");
        //debug{
        ipc_info_error_status(IPC_RX_CHANNEL, IPC_STATUS_FLOW_STOP);
        //}debug

        wake_lock_init(&s_ipc_sdio_wake_lock, WAKE_LOCK_SUSPEND, "ipc_sdio_lock");
        wake_lock_init(&s_cp_req_wake_lock, WAKE_LOCK_SUSPEND, "sdio_cp_req_lock");
        mutex_init(&ipc_mutex);
        //atomic_set(&s_mipc_read_pending, 0);
        init_cp_awake_status();
        init_mipc_tx_ctrl();
        mux_ipc_create_sdio_thread();
        mux_ipc_create_tx_thread();
        mux_ipc_create_rx_thread();

#ifdef SDIO_LOOP_TEST
        mux_ipc_create_test_rx_thread();
        mux_ipc_create_test_tx_thread();
#endif
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

static int __init mux_ipc_sdio_init(void)
{

        int retval;
        IPC_SDIO_INFO("mux_ipc_sdio_init\n");

        _transfer_frame_init();
        init_waitqueue_head(&s_mux_read_rts);
        init_waitqueue_head(&s_rx_fifo_avail);
        init_waitqueue_head(&s_modem_ready);
        init_waitqueue_head(&s_sdio_gpio_res_free);
        sprdmux_register(&sprd_iomux);
        retval = platform_driver_register(&modem_sdio_driver);
        if (retval) {
                IPC_SDIO_ERR("[sdio]: register modem_sdio_driver error:%d\r\n", retval);
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
