#ifndef SPRD_WLAN_COMMON_H_
#define SPRD_WLAN_COMMON_H_

#include <linux/proc_fs.h>
#include <linux/sipc.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/ieee80211.h>
#include <linux/printk.h>
#include <linux/inetdevice.h>
#include <linux/spinlock.h>
#include <net/cfg80211.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/ieee80211_radiotap.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <linux/inetdevice.h>
#include <asm/byteorder.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/completion.h>
#include <asm/atomic.h>
#include <linux/ieee80211.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#if defined(CONFIG_ARCH_SCX35LT8)
#include <asm/gpio.h>
#else
#include <mach/gpio.h>
#endif
#include <linux/of.h>
#include <linux/of_gpio.h>
#include "wlan_fifo.h"
#include "wlan_cmd.h"
#include "wlan_cfg80211.h"
#include "wlan_msg_q.h"

/* #define WLAN_LESS_WAKEUP_CP */
#define WLAN_THREAD_SLEPP_POLICE
#define WIFI_DRV_WAPI

#define INCR_RING_BUFF_INDX(indx, max_num)   ((((indx) + 1) < (max_num)) ? ((indx) + 1) : (0))
#define KERNEL_VERSION(a, b, c)              (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE                   KERNEL_VERSION(3, 10, 0)
#define SDIO_ALIGN_SIZE                      (1024)
#define ALIGN_4BYTE(a)                       ((((a)+3)&(~3)))
#define MAX_TX_BUFFER_ID                     (12)
#define TEST_BIT(a, k)                       ((a>>k)&1)
#define CLEAR_BIT(a, k)                      ({a = (a&(~(1<<k))); 0; })
#define SET_BIT(a, k)                        ({a = (a | (1<<k)); 0; })
#define WLAN_SYSTEM_DBG                      TEST_BIT(g_dbg, 1)
#define WLAN_PATH_DBG                        TEST_BIT(g_dbg, 2)
#define WLAN_HEX_DBG                         TEST_BIT(g_dbg, 3)
#define ETH_PCAP                             TEST_BIT(g_dbg, 4)
#define MAC_PCAP                             TEST_BIT(g_dbg, 5)
#define ETH_ALEN		                     (6)
#define SIOGETSSID                           (0x89F2)

/* HW_TX_SIZE, HW_RX_SIZE and PKT_AGGR_NUM must keep pace with CP
 * TX: CP discrp number 38, use 3 blocks, 13k per block
 * PKT_AGGR_NUM 12 = 38 / 3
 */
#define OK                                   (0)
#define ERROR                                (-1)
#define HW_TX_SIZE                           (13312)
#define HW_RX_SIZE                           (12288)
#define PKT_AGGR_NUM                         (12)

#define MAX_TCP_SESSION                      (10)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
#define KERNEL_DEBUG_LEVE     "\001" "0"
#else
#define KERNEL_DEBUG_LEVE       "<0>"
#endif

#define printkd(fmt, ...)     ({if (WLAN_SYSTEM_DBG)printk("[SC2331]" fmt, ##__VA_ARGS__); 0; })
#define printkp(fmt, ...)     ({if (WLAN_PATH_DBG)printk("[SC2331]" fmt, ##__VA_ARGS__); 0; })
#define printke(fmt, ...)     ({printk(KERNEL_DEBUG_LEVE "[SC2331]" fmt, ##__VA_ARGS__); 0; })
#define ASSERT(fmt, ...)      ({printk(KERNEL_DEBUG_LEVE  "[SC2331][ASSERT][%s][%d]" fmt "\n", __func__, __LINE__,  ##__VA_ARGS__); 0; })

#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(a) ((a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5])
#define STR2MAC(a) ((unsigned int *)&(a)[0], (unsigned int *)&(a)[1], \
		(unsigned int *)&(a)[2], (unsigned int *)&(a)[3], \
		(unsigned int *)&(a)[4], (unsigned int *)&(a)[5])

typedef struct {
	unsigned char chn[16];
	unsigned char num;
	unsigned short bit_map;
	spinlock_t lock;
	int gpio_high;
	unsigned long timeout;
	unsigned long timeout_time;
	bool timeout_flag;
} sdio_chn_t;

typedef enum {
	EVENT_Q_ID_0 = 0,
	EVENT_Q_ID_1 = 1,
	EVENT_Q_ID_2 = 2,
	EVENT_Q_ID_3 = 3,
	EVENT_Q_ID_4 = 4,
	EVENT_Q_MAX_ID = 5,
} EVENT_Q_ID_T;

typedef enum {
	NETIF_0_ID = 0,
	NETIF_1_ID = 1,
	WLAN_MAX_ID = 2,
} NETIF_ID_T;

typedef struct {
	int exit;
	struct semaphore sem;
} drv_sync_t;

typedef struct {
	sdio_chn_t sdio_tx_chn;
	sdio_chn_t sdio_rx_chn;
	unsigned int tx_cnt;
	unsigned int rx_cnt;
	unsigned int rx_record;
	struct wake_lock wlan_lock;
	struct early_suspend early_suspend;
	int wakeup;
	struct timer_list wakeup_timer;
	unsigned long wakeup_time;
	int can_sleep;
	u32 rx_gpio;
} hw_info_t;

typedef struct {
	struct task_struct *task;
	struct semaphore sem;
	int null_run;
	int max_null_run;
	int idle_sleep;
	int prio;
	unsigned int need_tx;
	unsigned int done_tx;
	unsigned int need_rx;
	unsigned int done_rx;
	atomic_t retry;
} wlan_thread_t;

typedef struct {
	unsigned char mac[ETH_ALEN];
	unsigned char netif_id;
	unsigned char prio;
	unsigned char wmm_supp;
	unsigned char status;
} net_connect_dev_t;

typedef struct {
	wait_queue_head_t waitQ;
	int wakeup;
	struct mutex cmd_lock;
	struct mutex mem_lock;
	unsigned char *mem;
} wlan_cmd_t;

typedef struct {
	struct work_struct work;
	unsigned short frame_type;
	bool reg;
	void *vif;
} register_frame_param_t;

typedef struct {
	struct wake_lock scan_done_lock;
	atomic_t scan_status;
	struct cfg80211_scan_request *scan_request;
	struct timer_list scan_timeout;
	int connect_status;
	int ssid_len;
	unsigned char ssid[IEEE80211_MAX_SSID_LEN];
	unsigned char bssid[ETH_ALEN];
	unsigned char cipher_type;
	unsigned char key_index[2];
	unsigned char key[2][4][WLAN_MAX_KEY_LEN];
	unsigned char key_len[2][4];
	unsigned char key_txrsc[2][WLAN_MAX_KEY_LEN];
	unsigned char *scan_frame_array;
	int p2p_mode;
	register_frame_param_t register_frame;
} wlan_cfg80211_t;

/* Best not to use the work to send deauth cmd
 * FIXME in the future
 */
struct deauth_info {
	struct work_struct work;
	/* 60 length is enough, maybe FIXME */
	unsigned char mac[60];
	unsigned short len;
};

typedef struct {
	int tid;
	int active;
	unsigned int data_seq;
	unsigned int ack_seq;
	struct timeval data_time;
	struct timeval ack_time;
	msg_q_t msg_q;
} wlan_tcp_session_t;

typedef struct {
	struct net_device *ndev;
	struct wireless_dev wdev;
	unsigned short id;
	unsigned char mac[ETH_ALEN];
	u8 beacon_loss;
	int mode;
	wlan_cfg80211_t cfg80211;
	net_connect_dev_t connect_dev[8];
	struct deauth_info deauth_info;
	txfifo_t txfifo;
	msg_q_t msg_q[2];
	bool tcp_ack_suppress;
	wlan_tcp_session_t tcp_session[MAX_TCP_SESSION];
} wlan_vif_t;

typedef struct {
	struct wiphy *wiphy;
	struct device *dev;
	wlan_thread_t wlan_core;
	wlan_thread_t wlan_trans;
	hw_info_t hw;
	wlan_cmd_t cmd;
	drv_sync_t sync;
	wlan_vif_t netif[2];
	rxfifo_t rxfifo;
} wlan_info_t;

static inline wlan_vif_t *ndev_to_vif(struct net_device *ndev)
{
	return *(wlan_vif_t **)netdev_priv(ndev);
}


extern void core_down(void);
extern void core_up(void);
extern void trans_down(void);
extern void trans_up(void);
extern void up_wlan_rx_trans(void);
extern int wlan_module_init(struct device *dev);
extern int wlan_module_exit(struct device *dev);
extern int hex_dump(unsigned char *name, unsigned short nLen,
		    unsigned char *pData, unsigned short len);
extern void init_register_frame_param(wlan_vif_t *vif);
extern void init_send_deauth_work(wlan_vif_t *vif);
extern wlan_vif_t *ndev_to_vif(struct net_device *ndev);
extern wlan_vif_t *id_to_vif(unsigned char id);
extern int hostap_conf_load(char *filename, unsigned char *key_val);
extern int mac_addr_cfg(wlan_vif_t *vif, unsigned char vif_id);
extern int wlan_vif_init(wlan_vif_t *vif, int type, const char *name,
			 void *ops);
extern int wlan_wiphy_new(wlan_info_t *wlan);
extern int wlan_vif_free(wlan_vif_t *vif);
extern int wlan_wiphy_free(wlan_info_t *wlan);
extern void wlan_nl_init(void);
extern void wlan_nl_deinit(void);

extern bool get_sdiohal_status(void);
extern int  sdio_chn_status(unsigned short chn, unsigned short *status);
extern int  sdio_dev_read(unsigned int chn, void *read_buf, unsigned int *count);
extern int  sdio_dev_write(unsigned int chn, void *data_buf, unsigned int count);
extern int  sdiodev_readchn_init(int chn, void *callback, bool with_para);
extern int sdio_read_wlan(unsigned int chn, void *read_buf,
			  unsigned int *count);
extern int sdiodev_readchn_uninit(unsigned int chn);
extern void mdbg_at_cmd_read(void);
extern void mdbg_loopcheck_read(void);
extern void mdbg_assert_read(void);
extern void mdbg_assert_interface(void);


extern void mdbg_sdio_read(void);
extern void marlin_pa_enable(bool enable);
extern int set_wlan_status(int status);
extern int set_marlin_wakeup(unsigned int chn, unsigned int user_id);
extern int set_marlin_sleep(unsigned int chn, unsigned int user_id);
extern char *get_cmd_name(int id);
extern unsigned int g_dbg;
extern wlan_info_t g_wlan;
extern msg_q_t *wlan_tcpack_q(wlan_vif_t *vif, unsigned char *frame,
			      unsigned int len);
extern int wlan_tcpack_tx(wlan_vif_t *vif, int *done);
extern int wlan_tcpack_buf_malloc(wlan_vif_t *vif);
extern int wlan_tcpack_buf_free(wlan_vif_t *vif);
extern int wlan_rx_buf_decode(unsigned char *buf, unsigned int max_len);
extern int wlan_tx_buf_decode(unsigned char *buf, unsigned int max_len);
extern void seq_point(unsigned char *frame, unsigned int len);
extern void ack_timeout_point(unsigned char *frame, unsigned int len);
extern void tcp_session_cfg(int cmd, int value);
#endif
