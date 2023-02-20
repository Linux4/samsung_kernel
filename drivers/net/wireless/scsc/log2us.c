/***************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/time.h>
#include <linux/ktime.h>

#include "log2us.h"
#include "dev.h"
#include "debug.h"
#include "mgt.h"
#include "nl80211_vendor.h"
#include "mib.h"
#include "mlme.h"

#define MACSTR_NOMASK "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR_LOG(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

#ifdef CONFIG_SCSC_WLAN_LOG_2_USER_SP
enum slsi_wlan_vendor_attr_conn_log_event {
	SLSI_WLAN_VENDOR_ATTR_CONN_LOG_EVENT = 1,
	SLSI_WLAN_VENDOR_ATTR_CONN_LOG_BUFF_COUNT,
	SLSI_WLAN_VENDOR_ATTR_CONN_LOG_EVENT_MAX
};


static void delete_node(struct slsi_dev *sdev)
{
	struct buff_list *curr = NULL;

	curr = sdev->conn_log2us_ctx.list;
	if (curr) {
		sdev->conn_log2us_ctx.list = sdev->conn_log2us_ctx.list->next;
		kfree(curr);
	}
}

static int slsi_send_conn_log_event(struct slsi_dev *sdev)
{
	struct sk_buff *skb;
	int res;
	u32 buf_count = 0;
	struct buff_list *curr;
	struct nlattr *nla = NULL;
	u32 *count_p;

	while (!(__ratelimit(&sdev->conn_log2us_ctx.rs)))
		msleep(50);

	skb = cfg80211_vendor_event_alloc(sdev->wiphy, NULL, NLMSG_DEFAULT_SIZE,
					  SLSI_NL80211_VENDOR_CONNECTIVITY_LOG_EVENT,
					  GFP_KERNEL);
	if (!skb) {
		SLSI_ERR_NODEV("Failed to allocate SKB for vendor conn_log_event event\n");
		return -ENOMEM;
	}

	nla = nla_reserve(skb, SLSI_WLAN_VENDOR_ATTR_CONN_LOG_BUFF_COUNT, sizeof(buf_count));
	if (!nla) {
		SLSI_ERR_NODEV("failed in nla_reserve\n");
		kfree_skb(skb);
		return -1;
	}

	count_p = nla_data(nla);

	spin_lock(&sdev->conn_log2us_ctx.conn_lock);
	curr = sdev->conn_log2us_ctx.list;

	while (curr && (buf_count < 100)) {
		res = nla_put(skb, SLSI_WLAN_VENDOR_ATTR_CONN_LOG_EVENT, curr->len,
			      curr->str);
		if (res) {
			SLSI_ERR_NODEV("failed in nla_put for buffer no = %d, res = %d\n",
				       buf_count + 1, res);
			break;
		}

		buf_count++;
		delete_node(sdev);
		curr = sdev->conn_log2us_ctx.list;
	}

	spin_unlock(&sdev->conn_log2us_ctx.conn_lock);
	if (buf_count == 0) {
		SLSI_ERR_NODEV("Buffer count is 0\n");
		kfree_skb(skb);
		return -1;
	}

	*count_p = buf_count;
	cfg80211_vendor_event(skb, GFP_KERNEL);

	return 0;
}

static void enqueue_log_buffer(struct buff_list *new_node, struct conn_log2us *ctx)
{
	struct buff_list *list;

	spin_lock(&ctx->conn_lock);
	list = ctx->list;

	if (!list) {
		ctx->list = new_node;
		spin_unlock(&ctx->conn_lock);
		return;
	}
	while (list->next != NULL)
		list = list->next;

	list->next = new_node;

	spin_unlock(&ctx->conn_lock);
}

static void conn_log_worker(struct work_struct *work)
{
	struct conn_log2us *p = container_of(work, struct conn_log2us, log2us_work);
	struct slsi_dev *sdev = container_of(p, struct slsi_dev, conn_log2us_ctx);
	int status = 0;

	while (p->list && (!(p->stop))) {
		rtnl_lock();
		status = slsi_send_conn_log_event(sdev);
		if (status)
			SLSI_ERR_NODEV("slsi_send_conn_log_event failed status = %d\n", status);
		rtnl_unlock();
	}
}

void slsi_conn_log2us_init(struct slsi_dev *sdev)
{
	spin_lock_init(&sdev->conn_log2us_ctx.conn_lock);

	sdev->conn_log2us_ctx.log2us_workq = create_workqueue("conn_logger");

	INIT_WORK(&sdev->conn_log2us_ctx.log2us_work, conn_log_worker);

	sdev->conn_log2us_ctx.list = NULL;
	ratelimit_state_init(&sdev->conn_log2us_ctx.rs, 1 * HZ, SLSI_LOG2US_BURST);
	sdev->conn_log2us_ctx.stop = false;
}

void slsi_conn_log2us_deinit(struct slsi_dev *sdev)
{
	struct buff_list *tmp;

	sdev->conn_log2us_ctx.stop = true;
	cancel_work_sync(&sdev->conn_log2us_ctx.log2us_work);
	destroy_workqueue(sdev->conn_log2us_ctx.log2us_workq);
	spin_lock(&sdev->conn_log2us_ctx.conn_lock);
	while (sdev->conn_log2us_ctx.list) {
		tmp = sdev->conn_log2us_ctx.list;
		kfree(tmp);
		sdev->conn_log2us_ctx.list = sdev->conn_log2us_ctx.list->next;
	}
	spin_unlock(&sdev->conn_log2us_ctx.conn_lock);
}

u8 *get_eap_type_from_val(int val, u8 *str)
{
	switch (val) {
	case SLSI_EAP_TYPE_IDENTITY:
		memcpy(str, "IDENTITY", strlen("IDENTITY") + 1);
		return str;
	case SLSI_EAP_TYPE_NOTIFICATION:
		memcpy(str, "NOTIFICATION", strlen("NOTIFICATION") + 1);
		return str;
	case SLSI_EAP_TYPE_NAK:
		memcpy(str, "NAK", strlen("NAK") + 1);
		return str;
	case SLSI_EAP_TYPE_MD5:
		memcpy(str, "MD5", strlen("MD5") + 1);
		return str;
	case SLSI_EAP_TYPE_OTP:
		memcpy(str, "OTP", strlen("OTP") + 1);
		return str;
	case SLSI_EAP_TYPE_GTC:
		memcpy(str, "GTC", strlen("GTC") + 1);
		return str;
	case SLSI_EAP_TYPE_TLS:
		memcpy(str, "TLS", strlen("TLS") + 1);
		return str;
	case SLSI_EAP_TYPE_LEAP:
		memcpy(str, "LEAP", strlen("LEAP") + 1);
		return str;
	case SLSI_EAP_TYPE_SIM:
		memcpy(str, "SIM", strlen("SIM") + 1);
		return str;
	case SLSI_EAP_TYPE_TTLS:
		memcpy(str, "TTLS", strlen("TTLS") + 1);
		return str;
	case SLSI_EAP_TYPE_AKA:
		memcpy(str, "AKA", strlen("AKA") + 1);
		return str;
	case SLSI_EAP_TYPE_PEAP:
		memcpy(str, "PEAP", strlen("PEAP") + 1);
		return str;
	case SLSI_EAP_TYPE_MSCHAPV2:
		memcpy(str, "MSCHAPV2", strlen("MSCHAPV2") + 1);
		return str;
	case SLSI_EAP_TYPE_TLV:
		memcpy(str, "TLV", strlen("TLV") + 1);
		return str;
	case SLSI_EAP_TYPE_TNC:
		memcpy(str, "TNC", strlen("TNC") + 1);
		return str;
	case SLSI_EAP_TYPE_FAST:
		memcpy(str, "FAST", strlen("FAST") + 1);
		return str;
	case SLSI_EAP_TYPE_PAX:
		memcpy(str, "PAX", strlen("PAX") + 1);
		return str;
	case SLSI_EAP_TYPE_PSK:
		memcpy(str, "PSK", strlen("PSK") + 1);
		return str;
	case SLSI_EAP_TYPE_SAKE:
		memcpy(str, "SAKE", strlen("SAKE") + 1);
		return str;
	case SLSI_EAP_TYPE_IKEV2:
		memcpy(str, "IKEV2", strlen("IKEV2") + 1);
		return str;
	case SLSI_EAP_TYPE_AKA_PRIME:
		memcpy(str, "AKA PRIME", strlen("AKA PRIME") + 1);
		return str;
	case SLSI_EAP_TYPE_GPSK:
		memcpy(str, "GPSK", strlen("GPSK") + 1);
		return str;
	case SLSI_EAP_TYPE_PWD:
		memcpy(str, "PWD", strlen("PWD") + 1);
		return str;
	case SLSI_EAP_TYPE_EKE:
		memcpy(str, "EKE", strlen("EKE") + 1);
		return str;
	case SLSI_EAP_TYPE_EXPANDED:
		memcpy(str, "EXPANDED", strlen("EXPANDED") + 1);
		return str;
	default:
		memcpy(str, "UNKNOWN", strlen("UNKNOWN") + 1);
		return str;
	}
}

static int slsi_get_roam_reason_for_fw_reason(int roam_reason)
{
	switch (roam_reason) {
	case SLSI_WIFI_ROAMING_SEARCH_REASON_LOW_RSSI:
		return 1;
	case SLSI_WIFI_ROAMING_SEARCH_REASON_LINK_LOSS:
		return 3;
	case SLSI_WIFI_ROAMING_SEARCH_REASON_BTM_REQ:
		return 5;
	case SLSI_WIFI_ROAMING_SEARCH_REASON_CU_TRIGGER:
		return 2;
	case SLSI_WIFI_ROAMING_SEARCH_REASON_EMERGENCY:
		return 4;
	case SLSI_WIFI_ROAMING_SEARCH_REASON_IDLE:
		return 6;
	case SLSI_WIFI_ROAMING_SEARCH_REASON_SCAN_TIMER1_EXPIRY:
		return 9;
	case SLSI_WIFI_ROAMING_SEARCH_REASON_SCAN_TIMER2_EXPIRY:
		return 9;
	case SLSI_WIFI_ROAMING_SEARCH_REASON_INACTIVE_TIMER_EXPIRY:
		return 8;
	default:
		return 0;
	}
}

void slsi_eapol_eap_handle_tx_status(struct slsi_dev *sdev, struct netdev_vif *ndev_vif,
				     u16 host_tag, u16 tx_status)
{
	char *tx_status_str;

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	if (tx_status == FAPI_TRANSMISSIONSTATUS_SUCCESSFUL)
		tx_status_str = "ACK";
	else
		tx_status_str = "TX_FAIL";

	if (host_tag == sdev->conn_log2us_ctx.host_tag_eapol_m4) {
		slsi_conn_log2us_eapol_ptk_tx(sdev, 4, tx_status_str);
		sdev->conn_log2us_ctx.host_tag_eapol_m4 = 0;
	} else if (host_tag == sdev->conn_log2us_ctx.host_tag_eapol_m2) {
		if (sdev->conn_log2us_ctx.is_eapol_ptk) {
			slsi_conn_log2us_eapol_ptk_tx(sdev, 2, tx_status_str);
			sdev->conn_log2us_ctx.is_eapol_ptk = false;
		} else if (sdev->conn_log2us_ctx.is_eapol_gtk) {
			slsi_conn_log2us_eapol_gtk_tx(sdev, 2, tx_status_str);
			sdev->conn_log2us_ctx.is_eapol_gtk = false;
		}
		sdev->conn_log2us_ctx.host_tag_eapol_m2 = 0;
	} else if (host_tag == sdev->conn_log2us_ctx.host_tag_eap) {
		slsi_conn_log2us_eap_tx(sdev, sdev->conn_log2us_ctx.eap_resp_len,
					sdev->conn_log2us_ctx.eap_str_type, tx_status_str);
		sdev->conn_log2us_ctx.host_tag_eap = 0;
	}
}

static int slsi_log2us_get_rssi(struct slsi_dev *sdev, struct net_device *dev,
				int *mib_value, u16 psid)
{
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_value     *values = NULL;
	struct slsi_mib_get_entry get_values[] = { { psid, { 0, 0 } } };

	mibrsp.dataLength = 64;
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes\n", mibrsp.dataLength);
		kfree(mibrsp.data);
		return -ENOMEM;
	}

	values = slsi_read_mibs(sdev, dev, get_values, 1, &mibrsp);
	if (!values) {
		kfree(mibrsp.data);
		return -EINVAL;
	}

	if (values[0].type == SLSI_MIB_TYPE_INT)
		*mib_value = (int)(values->u.intValue);
	else if (values[0].type == SLSI_MIB_TYPE_UINT)
		*mib_value = (int)(values->u.uintValue);
	else
		SLSI_ERR(sdev, "Invalid type (%d) for SLSI_PSID_UNIFI_LAST_BSS_RSSI\n",
			 values[0].type);
	kfree(values);
	kfree(mibrsp.data);
	return 0;
}

static struct buff_list *slsi_conn_log2us_alloc_new_node(void)
{

	struct buff_list *new_node;

	new_node = kmalloc(sizeof(struct buff_list), GFP_KERNEL);
	if (!new_node) {
		SLSI_ERR_NODEV("Memory is not available\n");
		return NULL;
	}
	new_node->next = NULL;
	return new_node;
}

static struct buff_list *slsi_conn_log2us_alloc_atomic_new_node(void)
{
	struct buff_list *new_node;

	new_node = kmalloc(sizeof(*new_node), GFP_ATOMIC);
	if (!new_node) {
		SLSI_ERR_NODEV("Memory is not available\n");
		return NULL;
	}
	new_node->next = NULL;
	return new_node;
}

void slsi_conn_log2us_connecting(struct slsi_dev *sdev, struct net_device *dev,
				 const unsigned char *ssid,
				 const unsigned char *bssid,
				 const unsigned char *bssid_hint,
				 int freq, int freq_hint,
				 int pairwise, int group,
				 int akm, int auth_type,
				 const u8 *ie, int ie_len)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	int btcoex = 0;
	const u8 *rsn;
	int rsn_pos = 0;
	u8 pmf = 0;
	u32 group_mgmt = 0;
	int rsn_len = 0;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;

	log_buffer = new_node->str;
	get_kernel_timestamp(time);
	if (sdev->conn_log2us_ctx.btcoex_sco || sdev->conn_log2us_ctx.btcoex_scan ||
	    sdev->conn_log2us_ctx.btcoex_hid)
		btcoex = 1;

	rsn = cfg80211_find_ie(WLAN_EID_RSN, ie, ie_len);

	if (rsn) {
		/* element_id(1) + length(1) + version(2) + Group Data Cipher Suite(4)+ */
		/* Pairwise Cipher Suite Count(2) + Pairwise Cipher Suite List(count * 4) */
		if ((ie_len - (rsn - ie)) >= 10)
			rsn_pos = 7 + 2 + (rsn[8] * 4);
		/* AKM Suite Count(2) + AKM Suite List(count * 4) */
		if ((ie_len - (rsn - ie)) >= rsn[1] + 2) {
			rsn_pos = rsn_pos + 2 + (rsn[rsn_pos] * 4);
			rsn_len = rsn[1];
		}
		/* check for pmf which are RSN caps MFPR and MFPC */
		if ((rsn_pos + 2) <= rsn_len)
			pmf = pmf | (rsn[rsn_pos] << 7) | (rsn[rsn_pos] << 6);

		if (pmf) {
			rsn_pos = rsn_pos + 2; /* RSN caps(2) */
			rsn_pos = 2 + (rsn[rsn_pos] * 16); /* PMKID Count(2) + PMKID List(count * 16) */
			if ((rsn_pos + 4) <= rsn_len)
				memcpy(&group_mgmt, (rsn + rsn_pos), 4); /* last 4 octets are group_mgmt field. */
		}
	}

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d][CONN] CONNECTING ssid=\"%s \"",
			 time[0], time[1], ssid);
	if (bssid)
		pos += scnprintf(log_buffer + pos, buf_size - pos, " bssid=" MACSTR_NOMASK,
				 MAC2STR_LOG(bssid));
	if (bssid_hint)
		pos += scnprintf(log_buffer + pos, buf_size - pos, " bssid_hint=" MACSTR_NOMASK,
				 MAC2STR_LOG(bssid_hint));
	pos += scnprintf(log_buffer + pos, buf_size - pos, " freq=%d freq_hint=%d pairwise=0x%x group =0x%x ",
			 freq, freq_hint, pairwise, group);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "akm=0x%x auth_type=%d ", akm, auth_type);

	if (pmf)
		pos += scnprintf(log_buffer + pos, buf_size - pos, "group_mgmt=0x%x btcoex=%d", group_mgmt, btcoex);
	else
		pos += scnprintf(log_buffer + pos, buf_size - pos, "btcoex=%d", btcoex);

	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_connecting_fail(struct slsi_dev *sdev, struct net_device *dev,
				      const unsigned char *bssid,
				      int freq, int reason)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	char *str;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	if (1 == reason)
		str = "no probe response";
	else
		str = "unknown";

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d][CONN] CONNECTING FAIL", time[0], time[1]);
	if (bssid)
		pos += scnprintf(log_buffer + pos, buf_size - pos, " bssid=" MACSTR_NOMASK, MAC2STR_LOG(bssid));
	pos += scnprintf(log_buffer + pos, buf_size - pos, " freq=%d [reason=%d %s]", freq, reason, str);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_disconnect(struct slsi_dev *sdev, struct net_device *dev,
				 const unsigned char *bssid,
				 int reason)
{

	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	int rssi = 100;
	int status;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	sdev->conn_log2us_ctx.conn_flag = false;
	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	status = slsi_log2us_get_rssi(sdev, dev, &rssi, SLSI_PSID_UNIFI_LAST_BSS_RSSI);
	if (status)
		SLSI_ERR(sdev, "Could not get rssi status = %d\n", status);

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [CONN] DISCONN bssid="
			 MACSTR_NOMASK " rssi=%d "
			 "reason=%d", time[0], time[1], MAC2STR_LOG(bssid), rssi, reason);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);

}

void slsi_conn_log2us_eapol_gtk(struct slsi_dev *sdev, struct net_device *dev,
				int eapol_msg_type)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [EAPOL] GTK M%d", time[0], time[1],
			 eapol_msg_type);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_eapol_gtk_tx(struct slsi_dev *sdev, int eapol_msg_type, char *tx_status_str)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [EAPOL] GTK M%d tx_status=%s",
			 time[0], time[1], eapol_msg_type, tx_status_str);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_eapol_ptk(struct slsi_dev *sdev, struct net_device *dev,
				int eapol_msg_type)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [EAPOL] 4WAY M%d", time[0], time[1],
			 eapol_msg_type);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_eapol_ptk_tx(struct slsi_dev *sdev,
				   int eapol_msg_type, char *tx_status_str)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [EAPOL] 4WAY M%d tx_status=%s",
			 time[0], time[1], eapol_msg_type, tx_status_str);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_roam_scan_start(struct slsi_dev *sdev, struct net_device *dev,
				      int reason, int roam_rssi_val,
				      int chan_utilisation,
				      int rssi_thresh, int timestamp)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	sdev->conn_log2us_ctx.roam_ap_count = 0;
	sdev->conn_log2us_ctx.cand_number = 0;
	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	reason = slsi_get_roam_reason_for_fw_reason(reason);

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [ROAM] SCAN_START reason=%d",
			 time[0], time[1], reason);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "rssi=%d cu=%d full_scan=%d rssi_thres=%d [fw_time=%d]",
			 roam_rssi_val, chan_utilisation, sdev->conn_log2us_ctx.full_scan_roam, rssi_thresh, timestamp);

	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_roam_result(struct slsi_dev *sdev, struct net_device *dev,
				  char *bssid, u32 timestamp, int status)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	char *res = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	if (status)
		res = "NO_ROAM";
	else
		res = "ROAM";
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] Timestamp][ROAM] RESULT %s bssid="
			 MACSTR_NOMASK "[fw_time=%d status=%d",
			 time[0], time[1], res, MAC2STR_LOG(bssid), timestamp, status);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_eap_with_len(struct slsi_dev *sdev, struct net_device *dev,
				   u8 *eap, int eap_length)
{
	int  pos         = 0;
	char *log_buffer = NULL;
	u32  time[2]     = { 0 };
	char *eap_type   = NULL;
	int buf_size = BUFF_SIZE;
	char str_type[15] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);

	switch (eap[SLSI_EAP_CODE_POS]) {
	case SLSI_EAP_PACKET_REQUEST:
		eap_type = "REQ";
		break;
	}

	get_eap_type_from_val(eap[SLSI_EAP_TYPE_POS], str_type);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [EAP] %s type=%s len=%d",
			 time[0], time[1], eap_type, str_type, eap_length);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_eap_tx(struct slsi_dev *sdev,
			     int eap_length, int eap_type,
			     char *tx_status_str)
{
	int  pos         = 0;
	char *log_buffer = NULL;
	u32  time[2]     = { 0 };
	int buf_size = BUFF_SIZE;
	struct buff_list *new_node = NULL;
	char eap_type_str[15] = {0};

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	get_eap_type_from_val(eap_type, eap_type_str);

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [EAP] RESP type=%s len=%d",
			 time[0], time[1], eap_type_str, eap_length);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "tx_status=%s", tx_status_str);

	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_eap(struct slsi_dev *sdev, struct net_device *dev, u8 *eap)
{
	int  pos         = 0;
	char *log_buffer = NULL;
	char *eap_type   = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);

	switch (eap[SLSI_EAP_CODE_POS]) {
	case SLSI_EAP_PACKET_SUCCESS:
		eap_type = "SUCC";
		break;
	case SLSI_EAP_PACKET_FAILURE:
		eap_type = "FAIL";
		break;
	}

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [EAP] %s",
			 time[0], time[1], eap_type);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_dhcp(struct slsi_dev *sdev, struct net_device *dev, char *str)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [DHCP] %s", time[0], time[1], str);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_dhcp_tx(struct slsi_dev *sdev, struct net_device *dev,
			      char *str, char *tx_status)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_atomic_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [DHCP] %s tx_status=%s",
			 time[0], time[1], str, tx_status);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_auth_req(struct slsi_dev *sdev, struct net_device *dev, const unsigned char *bssid,
			       int auth_algo, int sae_type, int sn, int status, u32 tx_status)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	char *tx_status_str;
	int rssi = 100;
	int res;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	if (tx_status == FAPI_RESULTCODE_AUTH_TX_FAIL)
		tx_status_str = "TX_FAIL";
	else if (tx_status == FAPI_RESULTCODE_AUTH_NO_ACK)
		tx_status_str = "NO_ACK";
	else
		tx_status_str = "ACK";
	get_kernel_timestamp(time);
	res = slsi_log2us_get_rssi(sdev, dev, &rssi, SLSI_PSID_UNIFI_RSSI);
	if (res)
		SLSI_ERR(sdev, "Could not get rssi status = %d\n", res);

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [CONN] AUTH REQ bssid=" MACSTR_NOMASK,
			 time[0], time[1], MAC2STR_LOG(bssid));
	pos += scnprintf(log_buffer + pos, buf_size - pos, " rssi=%d auth_algo=%d type=%d sn=%d status=%d",
			 rssi, auth_algo, sae_type, sn, status);
	pos += scnprintf(log_buffer + pos, buf_size - pos, " tx_status=%s", tx_status_str);

	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_auth_resp(struct slsi_dev *sdev, struct net_device *dev,
				const unsigned char *bssid, int auth_algo,
				int sae_type,
				int sn, int status)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [CONN] AUTH RESP bssid="
			 MACSTR_NOMASK " auth_algo=%d "
			 "type=%d sn=%d status=%d", time[0], time[1], MAC2STR_LOG(bssid),
			 auth_algo, sae_type, sn, status);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_assoc_req(struct slsi_dev *sdev, struct net_device *dev,
				const unsigned char *bssid, int sn, int tx_status)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	char *tx_status_str;
	int rssi = 100;
	int status;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	if (tx_status == FAPI_RESULTCODE_ASSOC_TX_FAIL)
		tx_status_str = "TX_FAIL";
	else if (tx_status == FAPI_RESULTCODE_ASSOC_NO_ACK)
		tx_status_str = "NO_ACK";
	else
		tx_status_str = "ACK";
	status = slsi_log2us_get_rssi(sdev, dev, &rssi, SLSI_PSID_UNIFI_RSSI);
	if (status)
		SLSI_ERR(sdev, "Could not get rssi status = %d\n", status);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [CONN] ASSOC REQ bssid="
			 MACSTR_NOMASK  " rssi=%d sn=%d "
			 "tx_status=%s", time[0], time[1], MAC2STR_LOG(bssid), rssi, sn,
			 tx_status_str);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_assoc_resp(struct slsi_dev *sdev, struct net_device *dev,
				 const unsigned char *bssid, int sn, int status)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [CONN] ASSOC RESP bssid="
			 MACSTR_NOMASK " sn=%d "
			 "status=%d", time[0], time[1], MAC2STR_LOG(bssid), sn, status);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_deauth(struct slsi_dev *sdev, struct net_device *dev, char *str_type,
			     const unsigned char *bssid,
			     int sn, int status)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	int rssi = 100;
	int res;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	sdev->conn_log2us_ctx.conn_flag = false;
	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	res = slsi_log2us_get_rssi(sdev, dev, &rssi, SLSI_PSID_UNIFI_LAST_BSS_RSSI);
	if (res)
		SLSI_ERR(sdev, "Could not get rssi status = %d\n", res);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [CONN] DEAUTH %s bssid="
			 MACSTR_NOMASK " rssi=%d sn=%d reason=%d", time[0], time[1],
			 str_type, MAC2STR_LOG(bssid), rssi, sn, status);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_disassoc(struct slsi_dev *sdev, struct net_device *dev, char *str_type,
			       const unsigned char *bssid,
			       int sn, int status)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	int rssi = 100;
	int res;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	sdev->conn_log2us_ctx.conn_flag = false;
	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	res = slsi_log2us_get_rssi(sdev, dev, &rssi, SLSI_PSID_UNIFI_LAST_BSS_RSSI);
	if (res)
		SLSI_ERR(sdev, "Could not get rssi status = %d\n", res);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [CONN] DISASSOC %s bssid="
			 MACSTR_NOMASK " rssi=%d sn=%d "
			 "reason=%d", time[0], time[1], str_type, MAC2STR_LOG(bssid),
			 rssi, sn, status);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_roam_scan_save(struct slsi_dev *sdev, struct net_device *dev,
				     int scan_type, int freq_count,
				     int *freq_list)
{
	int i;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	sdev->conn_log2us_ctx.roam_freq_count = freq_count;

	for (i = 0; i < freq_count; i++)
		sdev->conn_log2us_ctx.roam_freq_list[i] = *(freq_list + i);

	if (scan_type == FAPI_SCANTYPE_SOFT_CACHED_ROAMING_SCAN ||
	    scan_type == FAPI_SCANTYPE_HARD_CACHED_ROAMING_SCAN)
		sdev->conn_log2us_ctx.full_scan_roam = 0;
	else if (scan_type == FAPI_SCANTYPE_SOFT_FULL_ROAMING_SCAN ||
		 scan_type == FAPI_SCANTYPE_HARD_FULL_ROAMING_SCAN)
		sdev->conn_log2us_ctx.full_scan_roam = 1;
}

void slsi_conn_log2us_roam_scan_done(struct slsi_dev *sdev, struct net_device *dev, int timestamp)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	int i;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [ROAM] SCAN_DONE ap_count=%d freq[%d]= ",
			 time[0], time[1], sdev->conn_log2us_ctx.roam_ap_count,
			 sdev->conn_log2us_ctx.roam_freq_count);

	for (i = 0; i < sdev->conn_log2us_ctx.roam_freq_count; i++)
		pos += scnprintf(log_buffer + pos, buf_size - pos, "%d ",
				 sdev->conn_log2us_ctx.roam_freq_list[i]);

	pos += scnprintf(log_buffer + pos, buf_size - pos, "[fw_time=%d]", timestamp);

	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_roam_scan_result(struct slsi_dev *sdev, struct net_device *dev,
				       bool curr, char *bssid, int freq,
				       int rssi, int cu,
				       int score, int tp_score, int cand_number)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	sdev->conn_log2us_ctx.roam_ap_count++;
	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	if (curr) {
		pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [ROAM] SCORE_CUR_AP bssid="
				 MACSTR_NOMASK " "
				 "freq=%d rssi=%d cu=%d score=%d", time[0], time[1], MAC2STR_LOG(bssid), freq / 2,
				 rssi, cu, score);
	} else {
		pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [ROAM] SCORE_CANDI[%d] bssid="
				 MACSTR_NOMASK " "
				 "freq=%d rssi=%d cu=%d score=%d tp=%dkbps", time[0], time[1], cand_number,
				 MAC2STR_LOG(bssid), freq / 2, rssi, cu, score, tp_score);
		sdev->conn_log2us_ctx.cand_number++;
	}
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_btm_query(struct slsi_dev *sdev, struct net_device *dev,
				int dialog, int reason)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [BTM] QUERY token=%d reason=%d",
			 time[0], time[1], dialog, reason);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_btm_req(struct slsi_dev *sdev, struct net_device *dev,
			      int dialog, int btm_mode, int disassoc_timer,
			      int validity_time, int candidate_count)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	sdev->conn_log2us_ctx.cand_count = 0;
	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [BTM] REQ token=%d mode=%d disassoc=%d"
			 " validity=%d candidate_list_cnt=%d", time[0], time[1], dialog, btm_mode,
			 disassoc_timer, validity_time, candidate_count);
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_btm_resp(struct slsi_dev *sdev, struct net_device *dev, int dialog,
			       int btm_mode, int delay, char *bssid)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [BTM] RESP token=%d status=%d delay=%d"
			 " target=" MACSTR_NOMASK, time[0], time[1], dialog, btm_mode, delay, MAC2STR_LOG(bssid));
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

void slsi_conn_log2us_btm_cand(struct slsi_dev *sdev, struct net_device *dev,
			       char *bssid, int prefer)
{
	int pos = 0;
	char *log_buffer = NULL;
	int buf_size = BUFF_SIZE;
	u32 time[2] = { 0 };
	struct buff_list *new_node = NULL;
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (ndev_vif->iftype != NL80211_IFTYPE_STATION)
		return;

	new_node = slsi_conn_log2us_alloc_new_node();
	if (!new_node)
		return;
	log_buffer = new_node->str;

	get_kernel_timestamp(time);
	pos += scnprintf(log_buffer + pos, buf_size - pos, "[%d.%d] [BTM] REQ_CANDI[%d] bssid="
			 MACSTR_NOMASK " preference=%d", time[0], time[1],
			 sdev->conn_log2us_ctx.cand_count, MAC2STR_LOG(bssid), prefer);
	if (sdev->conn_log2us_ctx.cand_count == 0)
		SLSI_ETHER_COPY(sdev->conn_log2us_ctx.target_mac, bssid);
	sdev->conn_log2us_ctx.cand_count++;
	new_node->len = pos + 1;
	enqueue_log_buffer(new_node, &sdev->conn_log2us_ctx);
	queue_work(sdev->conn_log2us_ctx.log2us_workq, &sdev->conn_log2us_ctx.log2us_work);
}

#else

void slsi_conn_log2us_init(struct slsi_dev *sdev)
{
}

void slsi_conn_log2us_deinit(struct slsi_dev *sdev)
{
}

void slsi_conn_log2us_connecting(struct slsi_dev *sdev, struct net_device *dev,
				 const unsigned char *ssid,
				 const unsigned char *bssid,
				 const unsigned char *bssid_hint,
				 int freq, int freq_hint,
				 int pairwise, int group,
				 int akm, int auth_type,
				 const u8 *ie, int ie_len)
{
}

void slsi_conn_log2us_connecting_fail(struct slsi_dev *sdev, struct net_device *dev,
				      const unsigned char *bssid,
				      int freq, int reason)
{
}

void slsi_conn_log2us_disconnect(struct slsi_dev *sdev, struct net_device *dev,
				 const unsigned char *bssid, int reason)
{
}

void slsi_conn_log2us_eapol_gtk(struct slsi_dev *sdev, struct net_device *dev,
				int eapol_msg_type)
{
}

void slsi_conn_log2us_eapol_ptk(struct slsi_dev *sdev, struct net_device *dev,
				int eapol_msg_type)
{
}

void slsi_conn_log2us_roam_scan_start(struct slsi_dev *sdev, struct net_device *dev,
				      int reason, int roam_rssi_val,
				      int chan_utilisation,
				      int rssi_thresh, int timestamp)
{
}

void slsi_conn_log2us_roam_result(struct slsi_dev *sdev, struct net_device *dev,
				  char *bssid, u32 timestamp, int status)
{
}

void slsi_conn_log2us_eap(struct slsi_dev *sdev, struct net_device *dev, u8 *eap_type)
{
}

void slsi_conn_log2us_dhcp(struct slsi_dev *sdev, struct net_device *dev, char *str)
{
}

void slsi_conn_log2us_dhcp_tx(struct slsi_dev *sdev, struct net_device *dev,
			      char *str, char *tx_status)
{
}

void slsi_conn_log2us_eap_with_len(struct slsi_dev *sdev, struct net_device *dev,
				   u8 *eap_type, int eap_length)
{
}

void slsi_conn_log2us_auth_req(struct slsi_dev *sdev, struct net_device *dev,
			       const unsigned char *bssid,
			       int auth_algo, int sae_type, int sn, int status, u32 tx_status)
{
}

void slsi_conn_log2us_auth_resp(struct slsi_dev *sdev, struct net_device *dev,
				const unsigned char *bssid, int auth_algo,
				int sae_type,
				int sn, int status)
{
}

void slsi_conn_log2us_assoc_req(struct slsi_dev *sdev, struct net_device *dev,
				const unsigned char *bssid, int sn, int tx_status)
{
}

void slsi_conn_log2us_assoc_resp(struct slsi_dev *sdev, struct net_device *dev,
				 const unsigned char *bssid, int sn, int status)
{
}

void slsi_conn_log2us_deauth(struct slsi_dev *sdev, struct net_device *dev, char *str_type,
			     const unsigned char *bssid, int sn, int status)
{
}

void slsi_conn_log2us_disassoc(struct slsi_dev *sdev, struct net_device *dev, char *str_type,
			       const unsigned char *bssid,
			       int sn, int status)
{
}

void slsi_conn_log2us_roam_scan_done(struct slsi_dev *sdev, struct net_device *dev, int timestamp)
{
}

void slsi_conn_log2us_roam_scan_result(struct slsi_dev *sdev, struct net_device *dev,
				       bool curr, char *bssid, int freq,
				       int rssi, int cu,
				       int score, int tp_score, int cand_number)
{
}

void slsi_conn_log2us_btm_query(struct slsi_dev *sdev, struct net_device *dev,
				int dialog, int reason)
{
}

void slsi_conn_log2us_btm_req(struct slsi_dev *sdev, struct net_device *dev,
			      int dialog, int btm_mode, int disassoc_timer,
			      int validity_time, int candidate_count)
{
}

void slsi_conn_log2us_btm_resp(struct slsi_dev *sdev, struct net_device *dev,
			       int dialog, int btm_mode, int delay, char *bssid)
{
}

u8 *get_eap_type_from_val(int val, u8 *str)
{
	return NULL;
}

void slsi_conn_log2us_eapol_ptk_tx(struct slsi_dev *sdev, int eapol_msg_type, char *tx_status_str)
{
}

void slsi_conn_log2us_eapol_gtk_tx(struct slsi_dev *sdev, int eapol_msg_type, char *tx_status_str)
{
}

void slsi_conn_log2us_eap_tx(struct slsi_dev *sdev, int eap_length, int eap_type,
			     char *tx_status_str)
{
}

void slsi_conn_log2us_btm_cand(struct slsi_dev *sdev, struct net_device *dev,
			       char *bssid, int prefer)
{
}

void slsi_eapol_eap_handle_tx_status(struct slsi_dev *sdev, struct netdev_vif *ndev_vif,
				     u16 a, u16 b)
{
}

void slsi_conn_log2us_roam_scan_save(struct slsi_dev *sdev, struct net_device *dev,
				     int scan_type, int freq_count, int *freq_list)
{
}

#endif
