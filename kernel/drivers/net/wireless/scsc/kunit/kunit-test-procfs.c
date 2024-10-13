#include <kunit/test.h>
#include "kunit-common.h"

#include "kunit-mock-kernel.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-traffic_monitor.h"
#include "../procfs.c"

/* test */
static void test_slsi_procfs_open_file_generic(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct proc_inode *pinode = kunit_kzalloc(test, sizeof(struct proc_inode), GFP_KERNEL);
	pinode->pde = kunit_kzalloc(test, sizeof(struct proc_dir_entry), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_open_file_generic(&pinode->vfs_inode, fp));
}

static void test_slsi_procfs_throughput_stats_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct hip_priv *priv;
	char __user user_buf[100];
	loff_t ppos = 0;
	int iface;

	priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	fp->private_data = (void *)sdev;

	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);
	ndev_vif->activated = true;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_throughput_stats_read(fp, user_buf, 10, &ppos));
	ndev_vif->activated = false;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_throughput_stats_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_sta_bss_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct hip_priv *priv;
	struct cfg80211_bss sta_bss;
	char __user user_buf[100];
	loff_t ppos = 0;
	int iface;

	priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	fp->private_data = (void *)sdev;
	SLSI_MUTEX_INIT(ndev_vif->vif_mutex);
	ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	ndev_vif->sta.sta_bss->ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies __rcu), GFP_KERNEL);
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;

	KUNIT_EXPECT_NE(test, 0, slsi_procfs_sta_bss_read(fp, user_buf, 10, &ppos));
}

#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
static void test_slsi_printf_mutex_stats(struct kunit *test)
{
	static const char str[100] = "kunit/test";
	static const char printf_padding[10] = "kunit";
	char buf[100];
	const size_t bufsz = 80;
	struct slsi_mutex *mutex_p = kunit_kzalloc(test, sizeof(struct slsi_mutex), GFP_KERNEL);

	mutex_p->owner = kunit_kzalloc(test, sizeof(struct task_struct), GFP_KERNEL);
	mutex_p->valid = true;
	mutex_p->file_name_before = str;
	mutex_p->file_name_after = str;
	strcpy(mutex_p->owner->comm, "1");

	KUNIT_EXPECT_NE(test, 0, slsi_printf_mutex_stats(buf, bufsz, printf_padding, mutex_p));
	mutex_init(&mutex_p->mutex);
	mutex_lock(&mutex_p->mutex);
	KUNIT_EXPECT_NE(test, 0, slsi_printf_mutex_stats(buf, bufsz, printf_padding, mutex_p));
}

static void test_slsi_procfs_mutex_stats_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	char __user user_buf[100];
	loff_t ppos = 0;
	int iface;

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	fp->private_data = (void *)sdev;

	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);
	ndev_vif->is_available = true;

	KUNIT_EXPECT_NE(test, 0, slsi_procfs_mutex_stats_read(fp, user_buf, 10, &ppos));
}
#endif

#ifdef CONFIG_SCSC_WLAN_LOG_2_USER_SP
static void test_slsi_procfs_conn_log_event_burst_to_us_write(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;

	strcpy(user_buf, "kunit");
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_conn_log_event_burst_to_us_write(fp, user_buf, 10, &ppos));

	strcpy(user_buf, "10");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_conn_log_event_burst_to_us_write(fp, user_buf, 10, &ppos));
}
#endif

static void test_slsi_procfs_big_data_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	char __user user_buf[10] = "kunit";
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;
	SLSI_MUTEX_INIT(ndev_vif->vif_mutex);
	ndev_vif->ap.last_disconnected_sta.address[0] = 0xaa;
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_big_data_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_status_show(struct kunit *test)
{
	struct seq_file m = {0};
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	int v = 10;
#ifdef CONFIG_SCSC_WLAN_HIP5
	struct hip5_hip_control *control = kunit_kzalloc(test, sizeof(struct hip5_hip_control), GFP_KERNEL);
#else
	struct hip4_hip_control *control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
#endif
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	sdev->hip.hip_control = control;
#ifdef CONFIG_SCSC_WLAN_HIP5
	control->init.conf_hip5_ver = 4;
#else
	control->init.conf_hip4_ver = 4;
#endif
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);
	m.private = sdev;
	sdev->device_state = SLSI_DEVICE_STATE_STARTED;
	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_status_show(&m, v));
#ifdef CONFIG_SCSC_WLAN_HIP5
	control->init.conf_hip5_ver = 5;
#else
	control->init.conf_hip4_ver = 5;
#endif
	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_status_show(&m, v));
	sdev->device_state = SLSI_DEVICE_STATE_ATTACHING;
	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_status_show(&m, &v));
	sdev->device_state = SLSI_DEVICE_STATE_STOPPED;
	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_status_show(&m, v));
	sdev->device_state = SLSI_DEVICE_STATE_STARTING;
	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_status_show(&m, v));
	sdev->device_state = SLSI_DEVICE_STATE_STOPPING;
	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_status_show(&m, v));
}

static void test_slsi_procfs_build_show(struct kunit *test)
{
	struct seq_file m = {0};
	int v = 10;

	m.buf = kunit_kzalloc(test, sizeof(char) * 80, GFP_KERNEL);
	m.count = 0;
	m.size = 10;
	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_build_show(&m, &v));
}

static void test_slsi_procfs_vif_type_to_str(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, "STATION", slsi_procfs_vif_type_to_str(FAPI_VIFTYPE_STATION));
	KUNIT_EXPECT_STREQ(test, "AP", slsi_procfs_vif_type_to_str(FAPI_VIFTYPE_AP));
	KUNIT_EXPECT_STREQ(test, "DISCOVERY", slsi_procfs_vif_type_to_str(FAPI_VIFTYPE_DISCOVERY));
	KUNIT_EXPECT_STREQ(test, "PRECONNECT", slsi_procfs_vif_type_to_str(FAPI_VIFTYPE_PRECONNECT));
	KUNIT_EXPECT_STREQ(test, "?", slsi_procfs_vif_type_to_str(FAPI_VIFTYPE_DETECT));
}

static void test_slsi_procfs_vifs_show(struct kunit *test)
{
	struct seq_file m = {0};
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct netdev_vif *vif;
	int i, v = 0;
	int iface;

	m.private = sdev;
	m.buf = kunit_kzalloc(test, sizeof(char) * 80, GFP_KERNEL);
	m.count = 0;
	m.size = 10;
	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);
	sdev->netdev[SLSI_NET_INDEX_WLAN]->dev_addr = kunit_kzalloc(test, sizeof(char) * 10, GFP_KERNEL);
	vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);
	vif->activated = true;
	vif->vif_type = FAPI_VIFTYPE_STATION;

	for (i = 0; i < SLSI_ADHOC_PEER_CONNECTIONS_MAX; i++) {
		vif->peer_sta_record[i] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
		vif->peer_sta_record[i]->valid = true;
	}

	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_vifs_show(&m, &v));
}

static void test_slsi_procfs_read_int(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[100];
	loff_t ppos = 0;
	char extra[10] = "kunit";

	fp->private_data = (void *)sdev;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_read_int(fp, user_buf, 10, &ppos, 10, extra));
}

static void test_slsi_procfs_uapsd_write(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;

	strcpy(user_buf, "kunit");
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_uapsd_write(fp, user_buf, 10, &ppos));

	strcpy(user_buf, "10");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_uapsd_write(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_ap_cert_disable_ht_vht_write(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;

	strcpy(user_buf, "kunit");
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_ap_cert_disable_ht_vht_write(fp, user_buf, 10, &ppos));

	strcpy(user_buf, "10");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_ap_cert_disable_ht_vht_write(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_p2p_certif_write(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;

	strcpy(user_buf, "kunit");
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_p2p_certif_write(fp, user_buf, 10, &ppos));

	strcpy(user_buf, "10");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_p2p_certif_write(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_p2p_certif_read(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10] = "kunit";
	loff_t ppos = 0;

	fp->private_data = (void *)sdev;
	sdev->p2p_certif = true;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_p2p_certif_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_mac_addr_show(struct kunit *test)
{
	struct seq_file m = {0};
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	int v = 10;

	m.private = sdev;
	m.buf = kunit_kzalloc(test, sizeof(char) * 80, GFP_KERNEL);
	m.count = 0;
	m.size = 10;
	memset(sdev->hw_addr, 0, ETH_ALEN);
	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_mac_addr_show(&m, &v));
}

static void test_slsi_procfs_create_tspec_read(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;
	sdev->current_tspec_id = 0;

	KUNIT_EXPECT_NE(test, 0, slsi_procfs_create_tspec_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_create_tspec_write(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;

	strcpy(user_buf, "kunit");
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_create_tspec_write(fp, user_buf, 10, &ppos));

	strcpy(user_buf, "10");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_create_tspec_write(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_confg_tspec_read(struct kunit *test)
{

	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;
	sdev->current_tspec_id = 0;

	KUNIT_EXPECT_NE(test, 0, slsi_procfs_confg_tspec_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_confg_tspec_write(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;

	strcpy(user_buf, "kunit");
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_confg_tspec_write(fp, user_buf, 10, &ppos));

	strcpy(user_buf, "10");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_confg_tspec_write(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_send_addts_read(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;
	sdev->current_tspec_id = 0;

	KUNIT_EXPECT_NE(test, 0, slsi_procfs_send_addts_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_send_addts_write(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
		loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;

	strcpy(user_buf, "kunit");
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_send_addts_write(fp, user_buf, 10, &ppos));

	strcpy(user_buf, "10");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_send_addts_write(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_send_delts_read(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;
	sdev->current_tspec_id = 0;

	KUNIT_EXPECT_NE(test, 0, slsi_procfs_send_delts_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_send_delts_write(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;

	strcpy(user_buf, "kunit");
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_send_delts_write(fp, user_buf, 10, &ppos));

	strcpy(user_buf, "10");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_send_delts_write(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_del_tspec_read(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;
	sdev->current_tspec_id = 0;

	KUNIT_EXPECT_NE(test, 0, slsi_procfs_del_tspec_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_del_tspec_write(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;

	strcpy(user_buf, "kunit");
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_del_tspec_write(fp, user_buf, 10, &ppos));

	strcpy(user_buf, "10");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_del_tspec_write(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_tput_read(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	struct netdev_vif *ndev_vif;
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;
	sdev->current_tspec_id = 0;
	ndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);

	ndev_vif->throughput_tx_bps = 500;
	ndev_vif->throughput_rx_bps = 500;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_tput_read(fp, user_buf, 10, &ppos));

	ndev_vif->throughput_tx_bps = 1000;
	ndev_vif->throughput_rx_bps = 1000;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_tput_read(fp, user_buf, 10, &ppos));

	ndev_vif->throughput_tx_bps = 1000 * 1000;
	ndev_vif->throughput_rx_bps = 1000 * 1000;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_tput_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_tput_write(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;

	strcpy(user_buf, "0");
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_tput_write(fp, user_buf, 1, &ppos));
	strcpy(user_buf, "1");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_tput_write(fp, user_buf, 1, &ppos));
	strcpy(user_buf, "2");
	ppos = 0;
	KUNIT_EXPECT_NE(test, 0, slsi_procfs_tput_write(fp, user_buf, 1, &ppos));
}

static void test_slsi_procfs_inc_node(struct kunit *test)
{
	slsi_procfs_inc_node();
}

static void test_slsi_procfs_dec_node(struct kunit *test)
{
	slsi_procfs_dec_node();
}

static void test_slsi_procfs_fd_opened_read(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;
	sdev->current_tspec_id = 0;

	KUNIT_EXPECT_NE(test, 0, slsi_procfs_fd_opened_read(fp, user_buf, 10, &ppos));
}

#ifndef CONFIG_SCSC_WLAN_TX_API
static void test_slsi_procfs_fcq_show(struct kunit *test)
{
	struct seq_file m = {0};
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct netdev_vif *vif;
	int i, v = 0;
	int iface;

	m.private = sdev;
	m.buf = kunit_kzalloc(test, sizeof(char) * 80, GFP_KERNEL);
	m.count = 0;
	m.size = 10;
	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);
	vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);
	vif->activated = true;
	vif->vif_type = FAPI_VIFTYPE_STATION;

	for (i = 0; i < SLSI_ADHOC_PEER_CONNECTIONS_MAX; i++) {
		vif->peer_sta_record[i] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
		vif->peer_sta_record[i]->valid = true;
	}

	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_fcq_show(&m, &v));
}
#endif

static void test_slsi_procfs_ba_stats_show(struct kunit *test)
{
	struct seq_file m = {0};
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct netdev_vif *vif;
	int i, v = 0;
	int iface;

	m.private = sdev;
	m.buf = kunit_kzalloc(test, sizeof(char) * 80, GFP_KERNEL);
	m.count = 0;
	m.size = 10;

	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);

	vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);
	vif->activated = true;
	vif->vif_type = FAPI_VIFTYPE_STATION;

	for (i = 0; i < SLSI_ADHOC_PEER_CONNECTIONS_MAX; i++) {
		vif->peer_sta_record[i] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
		vif->peer_sta_record[i]->valid = true;
	}

	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_ba_stats_show(&m, &v));
}
static void test_slsi_procfs_tcp_ack_suppression_show(struct kunit *test)
{
	struct seq_file m = {0};
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct netdev_vif *vif;
	int i, v = 0;
	int iface;

	m.private = sdev;
	m.buf = kunit_kzalloc(test, sizeof(char) * 80, GFP_KERNEL);
	m.count = 0;
	m.size = 10;
	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);

	vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);
	vif->activated = true;
	vif->vif_type = FAPI_VIFTYPE_STATION;

	for (i = 0; i < SLSI_ADHOC_PEER_CONNECTIONS_MAX; i++) {
		vif->peer_sta_record[i] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
		vif->peer_sta_record[i]->valid = true;
	}

	KUNIT_EXPECT_EQ(test, 0, slsi_procfs_tcp_ack_suppression_show(&m, &v));
}

static void test_slsi_procfs_nan_mac_addr_read(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;
	sdev->current_tspec_id = 0;

	KUNIT_EXPECT_NE(test, 0, slsi_procfs_nan_mac_addr_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_procfs_dscp_mapping_read(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[10];
	loff_t ppos = 0;
	int iface;

	fp->private_data = (void *)sdev;
	sdev->current_tspec_id = 0;

	KUNIT_EXPECT_NE(test, 0, slsi_procfs_dscp_mapping_read(fp, user_buf, 10, &ppos));
}

static void test_slsi_create_proc_dir(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char __user user_buf[100];
	loff_t ppos = 0;

	sdev->procfs_instance = 10;
	KUNIT_EXPECT_EQ(test, 0, slsi_create_proc_dir(sdev));
	slsi_remove_proc_dir(sdev);
}

static void test_slsi_remove_proc_dir(struct kunit *test)
{
	struct file *fp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct proc_dir_entry *pde;
	char __user user_buf[100];
	char dir[16];
	loff_t ppos = 0;

	sdev->procfs_instance = 10;
	pde = kunit_kzalloc(test, sizeof(*pde), GFP_KERNEL);

	slsi_create_proc_dir(sdev);
	slsi_remove_proc_dir(sdev);
}

/* Test fictures*/
static int procfs_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void procfs_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case procfs_test_cases[] = {
	/* procfs.c */
	KUNIT_CASE(test_slsi_procfs_open_file_generic),
#ifdef CONFIG_SCSC_WLAN_MUTEX_DEBUG
	KUNIT_CASE(test_slsi_printf_mutex_stats),
	KUNIT_CASE(test_slsi_procfs_mutex_stats_read),
#endif
	KUNIT_CASE(test_slsi_procfs_throughput_stats_read),
	KUNIT_CASE(test_slsi_procfs_sta_bss_read),
#ifdef CONFIG_SCSC_WLAN_LOG_2_USER_SP
	KUNIT_CASE(test_slsi_procfs_conn_log_event_burst_to_us_write),
#endif
	KUNIT_CASE(test_slsi_procfs_big_data_read),
	KUNIT_CASE(test_slsi_procfs_status_show),
	KUNIT_CASE(test_slsi_procfs_build_show),
	KUNIT_CASE(test_slsi_procfs_vif_type_to_str),
	KUNIT_CASE(test_slsi_procfs_vifs_show),
	KUNIT_CASE(test_slsi_procfs_read_int),
	KUNIT_CASE(test_slsi_procfs_uapsd_write),
	KUNIT_CASE(test_slsi_procfs_ap_cert_disable_ht_vht_write),
	KUNIT_CASE(test_slsi_procfs_p2p_certif_write),
	KUNIT_CASE(test_slsi_procfs_p2p_certif_read),
	KUNIT_CASE(test_slsi_procfs_mac_addr_show),
	KUNIT_CASE(test_slsi_procfs_create_tspec_read),
	KUNIT_CASE(test_slsi_procfs_create_tspec_write),
	KUNIT_CASE(test_slsi_procfs_confg_tspec_read),
	KUNIT_CASE(test_slsi_procfs_confg_tspec_write),
	KUNIT_CASE(test_slsi_procfs_send_addts_read),
	KUNIT_CASE(test_slsi_procfs_send_addts_write),
	KUNIT_CASE(test_slsi_procfs_send_delts_read),
	KUNIT_CASE(test_slsi_procfs_send_delts_write),
	KUNIT_CASE(test_slsi_procfs_del_tspec_read),
	KUNIT_CASE(test_slsi_procfs_del_tspec_write),
	KUNIT_CASE(test_slsi_procfs_tput_read),
	KUNIT_CASE(test_slsi_procfs_tput_write),
	KUNIT_CASE(test_slsi_procfs_inc_node),
	KUNIT_CASE(test_slsi_procfs_dec_node),
	KUNIT_CASE(test_slsi_procfs_fd_opened_read),
	KUNIT_CASE(test_slsi_procfs_ba_stats_show),
	KUNIT_CASE(test_slsi_procfs_tcp_ack_suppression_show),
	KUNIT_CASE(test_slsi_procfs_nan_mac_addr_read),
	KUNIT_CASE(test_slsi_procfs_dscp_mapping_read),
	KUNIT_CASE(test_slsi_create_proc_dir),
	KUNIT_CASE(test_slsi_remove_proc_dir),
	{}
};

static struct kunit_suite procfs_test_suite[] = {
	{
		.name = "kunit-procfs-test",
		.test_cases = procfs_test_cases,
		.init = procfs_test_init,
		.exit = procfs_test_exit,
	}
};

kunit_test_suites(procfs_test_suite);
