/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_KERNEL_H__
#define __KUNIT_MOCK_KERNEL_H__

#include <net/cfg80211.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/hardirq.h>
#include <linux/cpufreq.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/rtc.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/bitmap.h>
#include <linux/rcupdate.h>
#include <linux/skbuff.h>
#include <paean_scsc/scsc_wakelock.h>

#include "../dev.h"
#include "../mlme.h"
#include "../cfg80211_ops.h"

#ifdef _LINUX_KFIFO_H
#undef kfifo_get
#undef kfifo_put
#undef kfifo_is_full
#undef kfifo_to_user
#undef kfifo_alloc
#undef kfifo_free
#undef kfifo_is_empty
#undef kfifo_avail
#undef kfifo_len
#undef kfifo_out
#define kfifo_get(args...)				1
#define kfifo_put(args...)				1
#define kfifo_is_full(args...)				0
#define kfifo_to_user(args...)				1
#define kfifo_alloc(args...)				0
#define kfifo_free
#define kfifo_is_empty(args...)				1
#define kfifo_avail(args...)				1
#define kfifo_len(args...)				10UL
#define kfifo_out(args...)				1U
#endif

#ifdef __NET_CFG80211_H
#undef ieee80211_get_channel
#undef ieee80211_channel_to_frequency
#undef cfg80211_chandef_create
#undef cfg80211_find_vendor_ie
#undef cfg80211_vendor_cmd_alloc_reply_skb
#undef cfg80211_find_ie
#undef cfg80211_get_bss
#undef cfg80211_disconnected
#undef cfg80211_remain_on_channel_expired
#undef cfg80211_vendor_event_alloc
#undef cfg80211_vendor_event
#undef cfg80211_remain_on_channel_expired
#undef cfg80211_ready_on_channel
#undef cfg80211_chandef_valid
#undef cfg80211_ch_switch_notify
#undef cfg80211_mgmt_tx_status
#undef cfg80211_find_ext_ie
#undef cfg80211_scan_done
#undef cfg80211_register_netdevice
#undef cfg80211_unregister_netdevice
#undef cfg80211_inform_bss_frame
#undef cfg80211_sched_scan_results
#undef cfg80211_michael_mic_failure
#undef cfg80211_rx_mgmt
#undef cfg80211_conn_failed
#undef cfg80211_external_auth_request
#undef cfg80211_connect_timeout
#undef cfg80211_ref_bss
#undef cfg80211_connect_bss
#undef cfg80211_find_ext_elem
#undef cfg80211_roamed
#undef cfg80211_connect_result
#undef cfg80211_new_sta
#undef cfg80211_put_bss
#undef cfg80211_sched_scan_stopped
#undef cfg80211_del_sta
#undef wiphy_register
#undef wiphy_unregister
#undef regulatory_set_wiphy_regd
#undef wiphy_apply_custom_regulatory
#undef set_wiphy_dev
#define ieee80211_get_channel(args...)			kunit_mock_ieee80211_get_channel(args)
#define ieee80211_channel_to_frequency(args...)		kunit_ieee80211_channel_to_frequency(args)
#define cfg80211_chandef_create
#define cfg80211_find_vendor_ie(args...)		kunit_mock_cfg80211_find_vendor_ie(args)
#define cfg80211_vendor_cmd_alloc_reply_skb(args...)	kunit_mock_cfg80211_vendor_cmd_alloc_reply_skb(args)
#define cfg80211_vendor_cmd_reply(args...)		kunit_mock_cfg80211_vendor_cmd_reply(args)
#define cfg80211_find_ie(args...)			kunit_mock_cfg80211_find_ie(args)
#define cfg80211_get_bss(args...)			kunit_mock_cfg80211_get_bss(args)
#define cfg80211_disconnected(args...)			kunit_mock_cfg80211_disconnected(args)
#define cfg80211_remain_on_channel_expired
#define cfg80211_vendor_event_alloc(args...)		kunit_mock_cfg80211_vendor_event_alloc(args)
#define cfg80211_vendor_event(args...)			kunit_mock_cfg80211_vendor_event(args)
#define cfg80211_remain_on_channel_expired
#define cfg80211_ready_on_channel
#define cfg80211_chandef_valid(args...)			1
#define cfg80211_ch_switch_notify
#define cfg80211_mgmt_tx_status
#define cfg80211_find_ext_ie(args...)			kunit_cfg80211_find_ext_ie(args)
#define cfg80211_scan_done
#define cfg80211_register_netdevice(args...)		0
#define cfg80211_unregister_netdevice(args...)		0
#define cfg80211_inform_bss_frame(args...)		((void *)0)
#define cfg80211_sched_scan_results
#define cfg80211_michael_mic_failure
#define cfg80211_rx_mgmt(args...)			0
#define cfg80211_conn_failed
#define cfg80211_external_auth_request(args...)		1
#define cfg80211_connect_timeout
#define cfg80211_ref_bss
#define cfg80211_connect_bss
#define cfg80211_find_ext_elem(args...)			kunit_mock_cfg80211_find_ext_elem(args)
#define cfg80211_roamed
#define cfg80211_connect_result
#define cfg80211_new_sta
#define cfg80211_put_bss
#define cfg80211_sched_scan_stopped
#define cfg80211_del_sta
#define wiphy_register(args...)				0
#define wiphy_unregister
#define regulatory_set_wiphy_regd(args...)		0
#define wiphy_apply_custom_regulatory
#define set_wiphy_dev
#endif

#ifdef _LINUX_NETDEVICE_H
#undef netif_stop_subqueue
#undef netif_wake_subqueue
#undef netif_dormant_on
#undef netif_carrier_on
#undef netif_carrier_off
#undef netif_dormant_on
#undef netif_tx_start_all_queues
#undef netif_tx_disable
#undef netif_tx_napi_add
#undef netif_tx_stop_all_queues
#undef __netif_subqueue_stopped
#undef netif_rx_ni
#undef netif_queue_stopped
#undef netif_receive_skb
#undef netif_dormant_off
#undef napi_gro_receive
#undef napi_gro_flush
#undef napi_schedule
#undef napi_complete_done
#undef napi_complete
#undef netdev_tx_sent_queue
#undef netdev_tx_completed_queue
#undef napi_enable
#undef register_netdevice
#undef unregister_netdevice
#undef dev_queue_xmit
#undef dev_alloc_name
#define netif_stop_subqueue
#define netif_wake_subqueue
#define netif_dormant_on
#define netif_carrier_on
#define netif_carrier_off
#define netif_dormant_on
#define netif_tx_start_all_queues
#define netif_tx_disable
#define netif_tx_napi_add
#define netif_tx_stop_all_queues
#define __netif_subqueue_stopped(args...)		1
#define netif_rx_ni(args...)				kunit_mock_netif_rx_ni(args)
#define netif_queue_stopped(args...)			1
#define netif_receive_skb(args...)			NET_RX_DROP
#define netif_dormant_off
#define napi_gro_receive(args...)			0
#define napi_gro_flush
#define napi_schedule
#define napi_complete_done(args...)			1
#define napi_complete					napi_complete_done
#define netdev_tx_sent_queue
#define netdev_tx_completed_queue
#define napi_enable
#define register_netdevice(args...)			1
#define unregister_netdevice(args...)			0
#define dev_queue_xmit(args...)				0
#define dev_alloc_name(args...)				0
#endif

#ifdef __CPUHOTPLUG_H
#undef cpuhp_state_add_instance_nocalls
#undef cpuhp_state_remove_instance_nocalls
#undef cpuhp_setup_state_multi
#undef cpuhp_remove_multi_state
#define cpuhp_state_add_instance_nocalls(args...)	0
#define cpuhp_state_remove_instance_nocalls(args...)	0
#define cpuhp_setup_state_multi(args...)		kunit_mock_cpuhp_setup_state_multi(args)
#define cpuhp_remove_multi_state
#endif

#ifdef _LINUX_KTHREAD_H
#undef cancel_delayed_work_sync
#define cancel_delayed_work_sync(args...)		kunit_mock_cancel_delayed_work_sync(args)
#endif

#ifdef __LINUX_COMPLETION_H
#undef complete_all
#undef complete
#undef reinit_completion
#undef wait_for_completion_timeout
#define complete_all
#define complete
#define reinit_completion
#define wait_for_completion_timeout(args...)		kunit_mock_wait_for_completion_timeout(args)
#endif

#ifdef _LINUX_TIMER_H
#undef mod_timer
#undef del_timer
#undef del_timer_sync
#undef timer_setup
#undef init_timer
#define mod_timer(args...)				0
#define del_timer(args...)				0
#define del_timer_sync(args...)				0
#define timer_setup
#define init_timer
#endif

#ifdef _LINUX_RATELIMIT_TYPES_H
#undef __ratelimit
#define __ratelimit(args...)				1
#endif

#ifdef _LINUX_WORKQUEUE_H
#undef queue_work
#undef schedule_work
#undef schedule_delayed_work
#undef cancel_work_sync
#undef delayed_work_pending
#undef create_workqueue
#undef destroy_workqueue
#undef queue_delayed_work
#undef cancel_delayed_work
#define queue_work(args...)				kunit_mock_queue_work(args)
#define schedule_work(args...)				1
#define schedule_delayed_work(args...)			1
#define cancel_work_sync(args...)			1
#define delayed_work_pending(args...)			1
#define create_workqueue(args...)			((void *)0)
#define destroy_workqueue
#define queue_delayed_work(args...)			0
#define cancel_delayed_work(args...)			0
#endif

#ifdef _DEVICE_H_
#undef device_create
#undef device_destroy
#define device_create(args...)				kunit_mock_device_create(args)
#define device_destroy
#endif

#ifdef _LINUX_MM_H
#undef remap_pfn_range
#define remap_pfn_range(args...)			kunit_mock_remap_pfn_range(args)
#endif

#ifdef __LINUX_MUTEX_H
#undef mutex_lock_interruptible
#define mutex_lock_interruptible(args...)		0
#endif

#ifdef _LINUX_WAIT_H
#undef wake_up_interruptible
#undef wait_event_interruptible
#define wake_up_interruptible
#define wait_event_interruptible(args...)		0
#endif

#ifdef _LINUX_DMA_MAPPING_H
#undef dma_set_mask_and_coherent
#undef dma_mapping_error
#undef dma_map_single
#undef dma_unmap_single
#define dma_set_mask_and_coherent(args...)		0
#define dma_mapping_error(args...)			0
#define dma_map_single(args...)				1
#define dma_unmap_single(args...)
#endif

#ifdef _LINUX_MATH_H
#undef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d)				0
#endif

#ifdef _LINUX_CDEV_H
#undef cdev_init
#undef cdev_add
#undef cdev_del
#define cdev_init(args...)				0
#define cdev_add(args...)				0
#define cdev_del(args...)				0
#endif

#ifdef _LINUX_INETDEVICE_H
#undef __in_dev_get_rtnl
#define __in_dev_get_rtnl(args...)			kunit_mock__in_dev_get_rtnl(args)
#endif

#ifdef _LINUX_FS_H
#undef alloc_chrdev_region
#undef unregister_chrdev_region
#define alloc_chrdev_region(args...)			0
#define unregister_chrdev_region
#endif

#ifdef _LINUX_SKBUFF_H
#undef skb_copy
#undef skb_copy_expand
#undef skb_realloc_headroom
#undef skb_queue_head
#undef skb_queue_tail
#undef skb_push
#define skb_copy(args...)				kunit_mock_skb_copy(args)
#define skb_copy_expand(args...)			kunit_mock_skb_copy_expand(args)
#define skb_realloc_headroom(args...)			kunit_mock_skb_realloc_headroom(args)
#define skb_queue_head
#define skb_queue_tail
#define skb_push
#endif

#ifdef _LINUX_NET_H
#undef net_ratelimit
#define net_ratelimit()					1
#endif

#ifdef __LINUX_RTNETLINK_H
#undef rtnl_is_locked
#define rtnl_is_locked()				1
#endif

#ifdef __LINUX_SMP_H
#undef smp_call_function_single_async
#define smp_call_function_single_async(args...)		0
#endif

#ifdef _LINUX_INTERRUPT_H
#undef tasklet_hi_schedule
#define tasklet_hi_schedule
#endif

#ifdef _LINUX_ETHERDEVICE_H
#undef eth_type_trans
#define eth_type_trans(args...)				kunit_mock_eth_type_trans(args)
#endif

#ifdef _LINUX_RTC_H_
#undef rtc_class_open
#undef rtc_read_time
#undef rtc_class_close
#define rtc_class_open(args...)				((void *)1)
#define rtc_read_time(args...)				0
#define rtc_class_close
#endif

#ifdef __LINUX_SPINLOCK_H
#undef spin_lock_bh
#undef spin_unlock_bh
#define spin_lock_bh
#define spin_unlock_bh
#endif

#ifdef _LINUX_WAKELOCK_H
#undef scsc_wake_lock_init
#define scsc_wake_lock_init
#endif

#ifdef __LINUX_RCUPDATE_H
#undef RCU_INIT_POINTER
#define RCU_INIT_POINTER
#endif

#ifdef _DEVICE_CLASS_H_
#undef class_create
#undef class_destroy
#define class_create(args...)				kunit_mock_class_create(args)
#define class_destroy(args...)				kunit_mock_class_destroy(args)
#endif


static struct in_device *kunit_mock__in_dev_get_rtnl(struct net_device *dev)
{
	return dev->ip_ptr;
}

static struct ieee80211_channel *kunit_mock_ieee80211_get_channel(struct wiphy *wiphy, int freq)
{
	enum nl80211_band band;
	struct ieee80211_supported_band *sband;
	int i;

	if (!wiphy)
		return NULL;

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		sband = wiphy->bands[band];

		if (!sband)
			continue;

		for (i = 0; i < sband->n_channels; i++) {
			struct ieee80211_channel *chan = &sband->channels[i];

			if (chan->orig_mag == -345)
				return NULL;

			if ((chan->center_freq + chan->freq_offset) == freq)
				return chan;
		}
	}

	return NULL;
}

static inline bool kunit_mock_cancel_delayed_work_sync(struct delayed_work *dwork)
{
	if (dwork == NULL) return false;
	else return true;
}

static u8 *kunit_mock_cfg80211_find_ie(u8 eid, const u8 *ies, int len)
{
	u8 *res = ies;
	int tag, tag_len;

	if (len < 1 || !ies)
		return NULL;

	if (((eid == SLSI_WLAN_EID_INTERWORKING || eid == SLSI_WLAN_EID_EXTENSION) && len > 5) ||
			(eid == WLAN_EID_VENDOR_SPECIFIC && len > 100)) {
		return 0;
	} else if (eid == WLAN_EID_RSN || (eid == WLAN_EID_COUNTRY && len < 20) ||
			(eid == WLAN_EID_HT_CAPABILITY && len > 20) ||
			eid == WLAN_EID_SUPP_RATES || eid == SLSI_WLAN_EID_VENDOR_SPECIFIC) {
		return res;
	} else if (eid == WLAN_EID_SSID) {
		if (res[0] == 83 && res[1] == 79 && res[2] == 78 && res[3] == 71 && res[4] == 84 && res[5] == 65)
			return NULL;
		else if (res)
			return res;
		else
			return ((u8 *)1);
	} else if (eid == WLAN_EID_HT_OPERATION) {
		if (len > 100)
			return NULL;
		else if (res)
			return res;
		else
			return NULL;
	} else if (eid == WLAN_EID_COUNTRY && len > 100) {
		return 0;
	}

	if (!res || (res[0] == 0x0 && res[1] == 0x0 && res[2] == 0x0))
		return 0;

	return res;
}

static struct cfg80211_bss *kunit_mock_cfg80211_get_bss(struct wiphy *wiphy,
							struct ieee80211_channel *channel,
							const u8 *bssid,
							const u8 *ssid, size_t ssid_len,
							enum ieee80211_bss_type bss_type,
							enum ieee80211_privacy privacy)
{
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *sdev_ndev_vif;

	if (wiphy->interface_modes == NL80211_IFTYPE_P2P_CLIENT) {
		if (sdev->netdev[SLSI_NET_INDEX_P2P]) {
			sdev_ndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_P2P]);
			if (sdev_ndev_vif->sta.sta_bss) {
				if (sdev_ndev_vif->sta.sta_bss->signal == 21) {
					bssid = NULL;
					return sdev_ndev_vif->sta.sta_bss;
				}
				else if (sdev_ndev_vif->sta.sta_bss->signal == 2) {
					bssid = NULL;
					return NULL;
				}
				else if (sdev_ndev_vif->sta.sta_bss->signal == 4) {
					bssid = NULL;
					sdev_ndev_vif->sta.sta_bss->signal = 21;
					return NULL;
				}
			}
			else
				return NULL;
		}
	} else if (wiphy->interface_modes == NL80211_IFTYPE_P2P_GO) {
		if (sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN]) {
			sdev_ndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN]);
			if (sdev_ndev_vif->sta.sta_bss) {
				if (sdev_ndev_vif->sta.sta_bss->signal == 21) {
					bssid = NULL;
					return sdev_ndev_vif->sta.sta_bss;
				}
				else if (sdev_ndev_vif->sta.sta_bss->signal == 2) {
					bssid = NULL;
					return NULL;
				}
				else if (sdev_ndev_vif->sta.sta_bss->signal == 4) {
					bssid = NULL;
					sdev_ndev_vif->sta.sta_bss->signal = 21;
					return NULL;
				}
			}
		}
	}

	return NULL;
}


static void kunit_mock_cfg80211_disconnected(struct net_device *dev, u16 reason,
					     const u8 *ie, size_t ie_len,
					     bool locally_generated, gfp_t gfp)
{
	return;
}

static struct sk_buff *kunit_mock_cfg80211_vendor_event_alloc(struct wiphy *wiphy,
							      struct wireless_dev *wdev,
							      int approxlen,
							      int event_idx,
							      gfp_t gfp)
{
	struct sk_buff *skb;

	if (!wiphy)
		return NULL;

	if (WARN_ON(event_idx < 0 || event_idx >= wiphy->n_vendor_events))
			return NULL;

	skb = nlmsg_new(approxlen + 100, gfp);
	if (!skb)
		return NULL;

	return skb;
}

static unsigned long kunit_mock_vmalloc_to_pfn(const void *addr)
{
	return 0;
}

static int kunit_mock_remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
				      unsigned long pfn, unsigned long size, pgprot_t prot)
{
	return 0;
}

static void kunit_mock_cfg80211_vendor_event(struct sk_buff *skb, gfp_t gfp)
{
	kfree_skb(skb);
}

static bool kunit_mock_queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	return true;
}

static struct device *kunit_mock_device_create(struct class *cls, struct device *parent,
					       dev_t devt, void *drvdata, const char *fmt, ...)
{
	return (struct device *)1;
}

static int kunit_mock_wait_for_completion_timeout(struct completion *rsc, unsigned long timeout)
{
	if (timeout == 13)	// sdev->recovery_timeout = 123;
		return -EINVAL;

	return 0;
}

static inline const u8 *kunit_mock_cfg80211_find_vendor_ie(unsigned int oui, int oui_type,
							   const u8 *ies, unsigned int len)
{
	if ((signed int)len < 0)
		return 0;
	return ies;
}

static struct sk_buff *kunit_mock_cfg80211_vendor_cmd_alloc_reply_skb(struct wiphy *wiphy, int approxlen)
{
	struct slsi_dev	*sdev = SDEV_FROM_WIPHY(wiphy);
	struct sk_buff	*skb;

	if (!wiphy)
		return NULL;

	if (!sdev)
		return NULL;

	if (sdev->device_config.qos_info == 981)
		return NULL;

	skb = nlmsg_new(approxlen + 100, GFP_KERNEL);
	if (!skb)
		return NULL;

	return skb;
}

static int kunit_mock_cfg80211_vendor_cmd_reply(struct sk_buff *skb)
{
	kfree_skb(skb);

	return 1;
}

static u32 kunit_ieee80211_channel_to_frequency(int chan, enum nl80211_band band)
{
	/* see 802.11 17.3.8.3.2 and Annex J
	 * there are overlapping channel numbers in 5GHz and 2GHz bands */
	if (chan <= 0)
		return 0; /* not supported */
	switch (band) {
	case NL80211_BAND_2GHZ:
	case NL80211_BAND_LC:
		if (chan == 14)
			return 2484;
		else if (chan < 14)
			return (2407 + chan * 5);
		break;
	case NL80211_BAND_5GHZ:
		if (chan >= 182 && chan <= 196)
			return (4000 + chan * 5);
		else
			return (5000 + chan * 5);
		break;
	case NL80211_BAND_6GHZ:
		/* see 802.11ax D6.1 27.3.23.2 */
		if (chan == 2)
			return (5935);
		if (chan <= 233)
			return (5950 + chan * 5);
		break;
	case NL80211_BAND_60GHZ:
		if (chan < 7)
			return (56160 + chan * 2160);
		break;
	case NL80211_BAND_S1GHZ:
		return 902000 + chan * 500;
	default:
		;
	}
	return 0; /* not supported */
}

static struct sk_buff *kunit_mock_skb_copy(const struct sk_buff *skb, gfp_t gfp_mask)
{
	return skb;
}

static struct sk_buff *kunit_mock_skb_copy_expand(const struct sk_buff *skb, int newheadroom,
				     int newtailroom, gfp_t priority)
{
	return skb ? (skb->next) : NULL;
}

static struct sk_buff *kunit_mock_skb_realloc_headroom(struct sk_buff *skb, unsigned int headroom)
{
	return skb ? (skb->next) : NULL;
}

static inline const u8 *kunit_cfg80211_find_ext_ie(u8 ext_eid, const u8 *ies, int len)
{
	if (len < 1 || !ies)
		return NULL;

	if (ext_eid == WLAN_EID_EXT_HE_CAPABILITY || ext_eid == WLAN_EID_EXT_HE_MU_EDCA)
		return ies;

	return NULL;
}

static __be16 kunit_mock_eth_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	return skb->protocol;
}

static int kunit_mock_netif_rx_ni(struct sk_buff *skb)
{
	return skb->skb_iif;
}

static struct element *kunit_mock_cfg80211_find_ext_elem(u8 ext_eid, u8 *ies, int len)
{
	return NULL;
}

static int kunit_mock_cpuhp_setup_state_multi(enum cpuhp_state state, const char *name,
					      int (*startup)(unsigned int cpu, struct hlist_node *node),
					      int (*teardown)(unsigned int cpu, struct hlist_node *node))
{
	return 1;
}

static struct class *class_create(struct module *owner, const char *name)
{
	return (struct class *)1;
}

static void class_destroy(struct class *cls)
{
	return;
}
#endif
