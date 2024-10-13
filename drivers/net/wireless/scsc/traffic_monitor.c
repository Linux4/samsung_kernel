/****************************************************************************
 *
 * Copyright (c) 2012 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/ktime.h>
#include <scsc/scsc_warn.h>
#include "dev.h"
#include "debug.h"
#include "traffic_monitor.h"
#include "mib.h"
#ifdef CONFIG_SCSC_WLAN_TPUT_MONITOR
#include "mlme.h"
#endif
#ifdef CONFIG_SCSC_WLAN_TX_API
#include "tx.h"
#endif

#ifdef CONFIG_SCSC_WLAN_TRACEPOINT_DEBUG
#include "slsi_tracepoint_debug.h"
#endif

#define TM_LOG_TX  (0x1)
#define TM_LOG_RX  (0x2)
#define TM_LOG_HIO (0x4)

#ifdef CONFIG_SCSC_WLAN_TPUT_MONITOR
static int tm_timer = 100;
module_param(tm_timer, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tm_timer, "Dynamic change traffic monitor work timer. Default : 100ms");

static int tm_logger_enable;
module_param(tm_logger_enable, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tm_logger_enable, "Print tput debug message");

static int tm_logger_mid = 100 * 1000 * 1000;
module_param(tm_logger_mid, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tm_logger_mid, "delayed workq stop level(bps)");

static int tm_logger_high = 200 * 1000 * 1000;
module_param(tm_logger_high, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tm_logger_high, "delayed workq start level(bps)");

struct traffic_stat {
#ifndef CONFIG_SCSC_WLAN_TX_API
	u32 tx_pool_inuse;
	u32 tx_pool_free;
#endif
};

struct tm_logger {
	struct delayed_work tm_work;
	struct traffic_stat tf_stat;
	struct slsi_dev *sdev;
	bool tm_enable;
};

struct tm_logger tm_data;
#endif

struct slsi_traffic_mon_client_entry {
	struct list_head q;
	void *client_ctx;
	u32 throughput;
	u32 state;
	u32 hysteresis;
	u32 mode;
	u32 mid_tput;
	u32 high_tput;
	u32 dir;
	void (*traffic_mon_client_cb)(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx);
};

static inline void traffic_mon_invoke_client_callback(struct slsi_dev *sdev, u32 tput_tx, u32 tput_rx, bool override)
{
	struct list_head       *pos, *n;
	struct slsi_traffic_mon_client_entry *traffic_client;

	list_for_each_safe(pos, n, &sdev->traffic_mon_clients.client_list) {
		traffic_client = list_entry(pos, struct slsi_traffic_mon_client_entry, q);

		if (override) {
			traffic_client->state = TRAFFIC_MON_CLIENT_STATE_OVERRIDE;
			if (traffic_client->traffic_mon_client_cb)
				traffic_client->traffic_mon_client_cb(traffic_client->client_ctx, TRAFFIC_MON_CLIENT_STATE_OVERRIDE, tput_tx, tput_rx);
		} else if (traffic_client->mode == TRAFFIC_MON_CLIENT_MODE_PERIODIC) {
			if (traffic_client->traffic_mon_client_cb)
				traffic_client->traffic_mon_client_cb(traffic_client->client_ctx, TRAFFIC_MON_CLIENT_STATE_LOW, tput_tx, tput_rx);
			traffic_client->throughput = (tput_tx + tput_rx);
		} else if (traffic_client->mode == TRAFFIC_MON_CLIENT_MODE_EVENTS) {
			u32 tput_comp;

			if (traffic_client->dir == TRAFFIC_MON_DIR_RX) {
				tput_comp = tput_rx;
			} else if (traffic_client->dir == TRAFFIC_MON_DIR_TX) {
				tput_comp = tput_tx;
			} else {
				tput_comp = tput_tx + tput_rx;
			}

			if ((traffic_client->high_tput) && (tput_comp > traffic_client->high_tput)) {
				if (traffic_client->state != TRAFFIC_MON_CLIENT_STATE_HIGH &&
				   (traffic_client->hysteresis++ > SLSI_TRAFFIC_MON_HYSTERESIS_HIGH)) {
					SLSI_DBG1(sdev, SLSI_HIP, "notify traffic event (tput:%u, state:%u --> HIGH)\n", tput_comp, traffic_client->state);
					traffic_client->hysteresis = 0;
					traffic_client->state = TRAFFIC_MON_CLIENT_STATE_HIGH;

					if (traffic_client->traffic_mon_client_cb)
						traffic_client->traffic_mon_client_cb(traffic_client->client_ctx, TRAFFIC_MON_CLIENT_STATE_HIGH, tput_tx, tput_rx);
					}
			} else if ((traffic_client->mid_tput) && (tput_comp > traffic_client->mid_tput)) {
				if (traffic_client->state != TRAFFIC_MON_CLIENT_STATE_MID) {
					if ((traffic_client->state == TRAFFIC_MON_CLIENT_STATE_LOW && (traffic_client->hysteresis++ > SLSI_TRAFFIC_MON_HYSTERESIS_HIGH)) ||
						(traffic_client->state == TRAFFIC_MON_CLIENT_STATE_HIGH && (traffic_client->hysteresis++ > SLSI_TRAFFIC_MON_HYSTERESIS_LOW))) {
						SLSI_DBG1(sdev, SLSI_HIP, "notify traffic event (tput:%u, state:%u --> MID)\n", tput_comp, traffic_client->state);
						traffic_client->hysteresis = 0;
						traffic_client->state = TRAFFIC_MON_CLIENT_STATE_MID;
						if (traffic_client->traffic_mon_client_cb)
							traffic_client->traffic_mon_client_cb(traffic_client->client_ctx, TRAFFIC_MON_CLIENT_STATE_MID, tput_tx, tput_rx);
					}
				}
			} else if (traffic_client->state != TRAFFIC_MON_CLIENT_STATE_LOW &&
					(traffic_client->hysteresis++ > SLSI_TRAFFIC_MON_HYSTERESIS_LOW)) {
				SLSI_DBG1(sdev, SLSI_HIP, "notify traffic event (tput:%u, state:%u --> LOW\n", tput_comp, traffic_client->state);
				traffic_client->hysteresis = 0;
				traffic_client->state = TRAFFIC_MON_CLIENT_STATE_LOW;
				if (traffic_client->traffic_mon_client_cb)
					traffic_client->traffic_mon_client_cb(traffic_client->client_ctx, TRAFFIC_MON_CLIENT_STATE_LOW, tput_tx, tput_rx);
			}
			traffic_client->throughput = tput_comp;
		}
	}
}

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
static void traffic_mon_timer(struct timer_list *t)
#else
static void traffic_mon_timer(unsigned long data)
#endif
{
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	struct slsi_traffic_mon_clients *clients = from_timer(clients, t, timer);
	struct slsi_dev *sdev = container_of(clients, typeof(*sdev), traffic_mon_clients);
#else
	struct slsi_dev *sdev = (struct slsi_dev *)data;
#endif
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	bool stop_monitor;
	u8 i;
	u32 tput_rx = 0;
	u32 tput_tx = 0;

	if (!sdev) {
		SLSI_ERR_NODEV("invalid sdev\n");
		return;
	}

	spin_lock_bh(&sdev->traffic_mon_clients.lock);

	for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
		dev = sdev->netdev[i];
		if (dev) {
			ndev_vif = netdev_priv(dev);

			if (ndev_vif) {
				u32 time_in_ms = 0;

				/* the Timer is jiffies based so resolution is not High and it may
				 * be off by a few ms. So to accurately measure the throughput find
				 * the time diff between last timer and this one
				 */
				time_in_ms = ktime_to_ms(ktime_sub(ktime_get(), ndev_vif->last_timer_time));

				/* the Timer may be any value but it still needs to calculate the
				 * throughput over a period of 1 second
				 */
				ndev_vif->num_bytes_rx_per_sec += ndev_vif->num_bytes_rx_per_timer;
				ndev_vif->num_bytes_tx_per_sec += ndev_vif->num_bytes_tx_per_timer;
				ndev_vif->report_time += time_in_ms;
				if (ndev_vif->report_time >= 1000) {
					ndev_vif->throughput_rx_bps = (ndev_vif->num_bytes_rx_per_sec * 8 / ndev_vif->report_time) * 1000;
					ndev_vif->throughput_tx_bps = (ndev_vif->num_bytes_tx_per_sec * 8 / ndev_vif->report_time) * 1000;
					ndev_vif->num_bytes_rx_per_sec = 0;
					ndev_vif->num_bytes_tx_per_sec = 0;
					ndev_vif->report_time = 0;
				}

				/* throughput per timer interval is measured but extrapolated to 1 sec */
				ndev_vif->throughput_tx = (ndev_vif->num_bytes_tx_per_timer * 8 / time_in_ms) * 1000;
				ndev_vif->throughput_rx = (ndev_vif->num_bytes_rx_per_timer * 8 / time_in_ms) * 1000;

				ndev_vif->num_bytes_tx_per_timer = 0;
				ndev_vif->num_bytes_rx_per_timer = 0;
				ndev_vif->last_timer_time = ktime_get();
				tput_tx += ndev_vif->throughput_tx;
				tput_rx += ndev_vif->throughput_rx;
			}
		}
	}

	traffic_mon_invoke_client_callback(sdev, tput_tx, tput_rx, false);
	stop_monitor = list_empty(&sdev->traffic_mon_clients.client_list);

	spin_unlock_bh(&sdev->traffic_mon_clients.lock);
	if (!stop_monitor)
		mod_timer(&sdev->traffic_mon_clients.timer, jiffies + msecs_to_jiffies(SLSI_TRAFFIC_MON_TIMER_PERIOD));
}

inline void slsi_traffic_mon_event_rx(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	/* Apply a correction to length to exclude IP and transport header.
	 * Can either peek into packets to derive the exact payload size
	 * or apply a rough correction to roughly calculate the throughput.
	 * rough correction  is applied with a number inbetween IP header (20 bytes) +
	 * UDP header (8 bytes) or TCP header (can be 20 bytes to 60 bytes) i.e. 40
	 */
	if (skb->len >= 40)
		ndev_vif->num_bytes_rx_per_timer += (skb->len - 40);
	else
		ndev_vif->num_bytes_rx_per_timer += skb->len;
}

inline void slsi_traffic_mon_event_tx(struct slsi_dev *sdev, struct net_device *dev, u32 len)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	/* Apply a correction to length to exclude IP and transport header.
	 * MSDU header (22 bytes) + IP header (20 bytes) +
	 * UDP (8 bytes) or TCP header (can be from 20 bytes to 60 bytes)
	 */
	if (len > 50)
		ndev_vif->num_bytes_tx_per_timer += len - 50;
	else
		ndev_vif->num_bytes_tx_per_timer += len;
}

void slsi_traffic_mon_override(struct slsi_dev *sdev)
{
	if (WLBT_WARN_ON(in_irq()))
		return;

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	if (!list_empty(&sdev->traffic_mon_clients.client_list)) {
		traffic_mon_invoke_client_callback(sdev, 0, 0, true);
	}
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);

}

u8 slsi_traffic_mon_is_running(struct slsi_dev *sdev)
{
	u8 is_running = 0;

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	if (!list_empty(&sdev->traffic_mon_clients.client_list))
		is_running = 1;
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);
	return is_running;
}

int slsi_traffic_mon_client_register(
	struct slsi_dev *sdev,
	void *client_ctx,
	u32 mode,
	u32 mid_tput,
	u32 high_tput,
	u32 dir,
	void (*traffic_mon_client_cb)(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx))
{
	struct slsi_traffic_mon_client_entry *traffic_mon_client;
	bool start_monitor;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	u8 i;

	if (!client_ctx) {
		SLSI_ERR(sdev, "A client context must be provided\n");
		return -EINVAL;
	}

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	SLSI_DBG1(sdev, SLSI_HIP, "client:%p, mode:%u, mid_tput:%u, high_tput:%u\n", client_ctx, mode, mid_tput, high_tput);
	start_monitor = list_empty(&sdev->traffic_mon_clients.client_list);
	traffic_mon_client = kmalloc(sizeof(*traffic_mon_client), GFP_ATOMIC);
	if (!traffic_mon_client) {
		SLSI_ERR(sdev, "could not allocate memory for Monitor client\n");
		spin_unlock_bh(&sdev->traffic_mon_clients.lock);
		return -ENOMEM;
	}

	traffic_mon_client->client_ctx = client_ctx;
	traffic_mon_client->state = TRAFFIC_MON_CLIENT_STATE_LOW;
	traffic_mon_client->hysteresis = 0;
	traffic_mon_client->mode = mode;
	traffic_mon_client->dir = dir;
	traffic_mon_client->mid_tput = mid_tput;
	traffic_mon_client->high_tput = high_tput;
	traffic_mon_client->traffic_mon_client_cb = traffic_mon_client_cb;

	/* Add to tail of monitor clients queue */
	list_add_tail(&traffic_mon_client->q, &sdev->traffic_mon_clients.client_list);

	if (start_monitor) {
		/* reset counters before starting Timer */
		for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
			dev = sdev->netdev[i];
			if (dev) {
				ndev_vif = netdev_priv(dev);

				if (ndev_vif) {
					ndev_vif->throughput_tx = 0;
					ndev_vif->throughput_rx = 0;
					ndev_vif->num_bytes_tx_per_timer = 0;
					ndev_vif->num_bytes_rx_per_timer = 0;
					ndev_vif->last_timer_time = ktime_get();
					ndev_vif->num_bytes_rx_per_sec = 0;
					ndev_vif->num_bytes_tx_per_sec = 0;
					ndev_vif->throughput_rx_bps = 0;
					ndev_vif->throughput_tx_bps = 0;
					ndev_vif->report_time = 0;
				}
			}
		}
		mod_timer(&sdev->traffic_mon_clients.timer, jiffies + msecs_to_jiffies(SLSI_TRAFFIC_MON_TIMER_PERIOD));
	}

	spin_unlock_bh(&sdev->traffic_mon_clients.lock);
	return 0;
}

void slsi_traffic_mon_client_unregister(struct slsi_dev *sdev, void *client_ctx)
{
	struct list_head       *pos, *n;
	struct slsi_traffic_mon_client_entry *traffic_mon_client;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	u8 i;

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	SLSI_DBG1(sdev, SLSI_HIP, "client: %p\n", client_ctx);
	list_for_each_safe(pos, n, &sdev->traffic_mon_clients.client_list) {
		traffic_mon_client = list_entry(pos, struct slsi_traffic_mon_client_entry, q);
		if (traffic_mon_client->client_ctx == client_ctx) {
			SLSI_DBG1(sdev, SLSI_HIP, "delete: %p\n", traffic_mon_client->client_ctx);
			list_del(pos);
			kfree(traffic_mon_client);
		}
	}

	if (list_empty(&sdev->traffic_mon_clients.client_list)) {
		/* reset counters */
		for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
			dev = sdev->netdev[i];
			if (dev) {
				ndev_vif = netdev_priv(dev);

				if (ndev_vif) {
					ndev_vif->throughput_tx = 0;
					ndev_vif->throughput_rx = 0;
					ndev_vif->num_bytes_tx_per_timer = 0;
					ndev_vif->num_bytes_rx_per_timer = 0;
					ndev_vif->num_bytes_rx_per_sec = 0;
					ndev_vif->num_bytes_tx_per_sec = 0;
					ndev_vif->throughput_rx_bps = 0;
					ndev_vif->throughput_tx_bps = 0;
					ndev_vif->report_time = 0;
				}
			}
		}
		spin_unlock_bh(&sdev->traffic_mon_clients.lock);
		del_timer_sync(&sdev->traffic_mon_clients.timer);
		spin_lock_bh(&sdev->traffic_mon_clients.lock);
	}
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);
}

void slsi_traffic_mon_clients_init(struct slsi_dev *sdev)
{
	if (!sdev) {
		SLSI_ERR_NODEV("invalid sdev\n");
		return;
	}
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	timer_setup(&sdev->traffic_mon_clients.timer, traffic_mon_timer, 0);
#else
	setup_timer(&sdev->traffic_mon_clients.timer, traffic_mon_timer, (unsigned long)sdev);
#endif
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	spin_lock_init(&sdev->traffic_mon_clients.lock);
}

void slsi_traffic_mon_clients_deinit(struct slsi_dev *sdev)
{
	struct list_head       *pos, *n;
	struct slsi_traffic_mon_client_entry *traffic_mon_client;

	if (!sdev) {
		SLSI_ERR_NODEV("invalid sdev\n");
		return;
	}

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	list_for_each_safe(pos, n, &sdev->traffic_mon_clients.client_list) {
		traffic_mon_client = list_entry(pos, struct slsi_traffic_mon_client_entry, q);
		SLSI_DBG1(sdev, SLSI_HIP, "delete: %p\n", traffic_mon_client->client_ctx);
		list_del(pos);
		kfree(traffic_mon_client);
	}
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);
	del_timer_sync(&sdev->traffic_mon_clients.timer);
}

#ifdef CONFIG_SCSC_WLAN_TPUT_MONITOR
static inline int traffic_mon_get_hostio_usage(struct slsi_dev *sdev, struct net_device *dev, char *buf)
{
	struct slsi_mib_data      mibrsp = { 0, NULL };
	struct slsi_mib_value     *values = NULL;
	struct slsi_mib_get_entry get_values[] = {{ SLSI_PSID_UNIFI_THROUGHPUT_DEBUG, { 21, 0 }},//outstanding_fh_mbulk
						  { SLSI_PSID_UNIFI_THROUGHPUT_DEBUG, { 22, 0 }},//outstanding_th_mbulk
						  { SLSI_PSID_UNIFI_THROUGHPUT_DEBUG, { 23, 0 }}};//fos_cpu_usage
	int pos = 0;

	mibrsp.dataLength = 15 * ARRAY_SIZE(get_values);
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes\n", mibrsp.dataLength);
		return -ENOMEM;
	}
	values = slsi_read_mibs(sdev, dev, get_values, ARRAY_SIZE(get_values), &mibrsp);
	if (!values) {
		kfree(mibrsp.data);
		return -EINVAL;
	}

	pos += sprintf(buf, "HIO:fh %d th %d cpu %d",
			values[0].u.uintValue, values[1].u.uintValue, values[2].u.uintValue);

	kfree(values);
	kfree(mibrsp.data);

	return pos;
}

#ifndef CONFIG_SCSC_WLAN_HIP5
static inline int traffic_mon_get_rx_usage(struct netdev_vif *ndev_vif, char *buf)
{
	struct napi_stat *stat = &ndev_vif->rx_stat;
	ktime_t napi_time;
	ktime_t napi_time_delta;
	int napi_cnt;
	int napi_cnt_delta;
	int napi_todo;
	int napi_todo_delta;
	int napi_done;
	int napi_done_delta;
	int pos = 0;

	napi_cnt = stat->napi_cnt;
	napi_todo = stat->napi_todo;
	napi_done = stat->napi_done;
	napi_time = stat->napi_time;

	napi_cnt_delta = napi_cnt - stat->napi_cnt_last;
	napi_todo_delta = napi_todo - stat->napi_todo_last;
	napi_done_delta = napi_done - stat->napi_done_last;
	napi_time_delta = ktime_sub(napi_time, stat->napi_time_last);

	stat->napi_cnt_last = napi_cnt;
	stat->napi_todo_last = napi_todo;
	stat->napi_done_last = napi_done;
	stat->napi_time_last = napi_time;
	pos += sprintf(buf, "RX:napi cnt %d time %lld todo %d done %d tp %u|",
			napi_cnt_delta,	ktime_to_ms(napi_time_delta), napi_todo_delta,
			napi_done_delta, ndev_vif->throughput_rx / 1000000);

	return pos;
}
#endif

#ifdef CONFIG_SCSC_WLAN_TX_API
static inline int traffic_mon_get_tx_usage(struct netdev_vif *ndev_vif, char *buf)
{
	struct tx_netdev_data *tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;
	struct txq_stats *qstat;
	ktime_t stop, wake, total;
	int pos = 0, qidx;
	u8 q_stop, q_wake;

	if (!tx_priv)
		return 0;

	pos = sprintf(buf, "TX:mod %d cod %d ", tx_priv->netdev_mod, tx_priv->netdev_cod);

	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++) {
		qstat = &tx_priv->qstat[qidx];

		if (qstat->last_cumulated_stop >= qstat->cumulated_stop)
			continue;

		stop = ktime_sub(qstat->cumulated_stop, qstat->last_cumulated_stop);
		wake = ktime_sub(qstat->cumulated_wake, qstat->last_cumulated_wake);
		total = ktime_add(stop, wake);
		q_stop = stop * 100 / total;
		q_wake = wake * 100 / total;

		pos += sprintf(buf + pos, "q[%d] stop %u wake %u ", qidx, q_stop, q_wake);

		qstat->last_cumulated_stop = qstat->cumulated_stop;
		qstat->last_cumulated_wake = qstat->cumulated_wake;
	}
	pos += sprintf(buf + pos, "tp %u|", ndev_vif->throughput_tx / 1000000);

	return pos;
}
#else
static inline int traffic_mon_get_mbulk_usage(char *buf)
{
	struct traffic_stat *st = &tm_data.tf_stat;
	int pos;

	if (mbulk_pool_get_count(MBULK_POOL_ID_DATA, MBULK_CLASS_FROM_HOST_DAT, &st->tx_pool_free, &st->tx_pool_inuse))
		return 0;

	pos = sprintf(buf, "mbulk:free %d inuse %d ", st->tx_pool_free, st->tx_pool_inuse);

	return pos;
}
#endif

static void slsi_traffic_mon_work(struct work_struct *work)
{
	struct tm_logger *tm = &tm_data;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	int pos = 0, idx;
	char buf[512] = {'\0',};

	if (!tm_logger_enable || !tm || !tm->tm_enable || !tm->sdev)
		return;

#ifndef CONFIG_SCSC_WLAN_TX_API
	pos += traffic_mon_get_mbulk_usage(buf);
#endif

	for (idx = 1; idx < CONFIG_SCSC_WLAN_MAX_INTERFACES; idx++) {
		dev = slsi_get_netdev(tm->sdev, idx);
		if (!dev)
			continue;

		ndev_vif = netdev_priv(dev);
		if (!ndev_vif || !ndev_vif->activated)
			continue;

		pos += sprintf(buf + pos, "%s::", dev->name);

#ifndef CONFIG_SCSC_WLAN_HIP5
		if (idx == 1 && tm_logger_enable & TM_LOG_RX)
			pos += traffic_mon_get_rx_usage(ndev_vif, buf + pos);
#endif
#ifdef CONFIG_SCSC_WLAN_TX_API
		if (tm_logger_enable & TM_LOG_TX)
			pos += traffic_mon_get_tx_usage(ndev_vif, buf + pos);
#endif
		if (tm_logger_enable & TM_LOG_HIO)
			pos += traffic_mon_get_hostio_usage(tm->sdev, dev, buf + pos);
	}

	/* TODO:
	 * Need to find how to save tput related logs to avoid accupying kernel message excessively
	 */
	SLSI_INFO_NODEV("%s\n", buf);

	schedule_delayed_work(&tm_data.tm_work, msecs_to_jiffies(tm_timer));
}

static void slsi_traffic_mon_logger_cb(void *ctx, u32 state, u32 tput_tx, u32 tput_rx)
{
	struct tm_logger *tm = (struct tm_logger *)ctx;

	if (!tm->sdev || !tm_logger_enable)
		return;

	SLSI_INFO_NODEV("tm_logger state %d\n", state);
	if (!tm->tm_enable && state == TRAFFIC_MON_CLIENT_STATE_HIGH) {
		tm->tm_enable = true;
		schedule_delayed_work(&tm->tm_work, msecs_to_jiffies(tm_timer));
#ifdef CONFIG_SCSC_WLAN_TRACEPOINT_DEBUG
		slsi_tracepoint_log_enable(true);
#endif
	} else if (tm->tm_enable && (state == TRAFFIC_MON_CLIENT_STATE_MID || state == TRAFFIC_MON_CLIENT_STATE_LOW)) {
		tm->tm_enable = false;
#ifdef CONFIG_SCSC_WLAN_TRACEPOINT_DEBUG
		slsi_tracepoint_log_enable(false);
#endif
	}
}

void slsi_traffic_mon_init(struct slsi_dev *sdev)
{
#ifndef CONFIG_SCSC_WLAN_TX_API
	struct traffic_stat *stat = &tm_data.tf_stat;

	stat->tx_pool_inuse = 0;
	stat->tx_pool_free = 0;
#endif

	tm_data.sdev = sdev;
	INIT_DELAYED_WORK(&tm_data.tm_work, slsi_traffic_mon_work);

	slsi_traffic_mon_client_register(sdev, (void *)&tm_data, TRAFFIC_MON_CLIENT_MODE_EVENTS,
					 tm_logger_mid, tm_logger_high, TRAFFIC_MON_DIR_DEFAULT,
					 slsi_traffic_mon_logger_cb);
}

void slsi_traffic_mon_deinit(struct slsi_dev *sdev)
{
#ifndef CONFIG_SCSC_WLAN_TX_API
	struct traffic_stat *stat = &tm_data.tf_stat;

	stat->tx_pool_inuse = 0;
	stat->tx_pool_free = 0;
#endif

	slsi_traffic_mon_client_unregister(sdev, &tm_data);
	tm_data.sdev = NULL;
}
#endif
