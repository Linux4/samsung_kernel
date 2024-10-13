// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2017, 2019, 2021 The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt) "cnss_utils: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/etherdevice.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <net/cnss_utils.h>

#include <linux/dev_ril_bridge.h>

#define CNSS_MAX_CH_NUM 157
struct cnss_unsafe_channel_list {
	u16 unsafe_ch_count;
	u16 unsafe_ch_list[CNSS_MAX_CH_NUM];
};

struct cnss_dfs_nol_info {
	void *dfs_nol_info;
	u16 dfs_nol_info_len;
};

#define MAX_NO_OF_MAC_ADDR 4
#define MAC_PREFIX_LEN 2
struct cnss_wlan_mac_addr {
	u8 mac_addr[MAX_NO_OF_MAC_ADDR][ETH_ALEN];
	u32 no_of_mac_addr_set;
};

enum mac_type {
	CNSS_MAC_PROVISIONED,
	CNSS_MAC_DERIVED,
};

static struct cnss_utils_priv {
	struct cnss_unsafe_channel_list unsafe_channel_list;
	struct cnss_dfs_nol_info dfs_nol_info;
	/* generic mutex for unsafe channel */
	struct mutex unsafe_channel_list_lock;
	/* generic spin-lock for dfs_nol info */
	spinlock_t dfs_nol_info_lock;
	int driver_load_cnt;
	struct cnss_wlan_mac_addr wlan_mac_addr;
	struct cnss_wlan_mac_addr wlan_der_mac_addr;
	enum cnss_utils_cc_src cc_source;
	struct dentry *root_dentry;
} *cnss_utils_priv;

typedef enum {
    CP_NONE=0,
    CP_COEX_LTE=3,
    CP_COEX_NT5G=7
} rat_mode;

typedef enum {
	CP_NO_BAND,
    CP_LTE_BAND = CP_NO_BAND,
    CP_5G_BAND,
    CP_BAND_MAX,
    CP_BOTH_BAND
} CP_channel_info;

struct __packed LTE_5G_coex_cp_noti_info {
u8 rat;
u32 band;
u32 channel;
};

typedef struct {
	bool init_notifier;
	unsigned int cp_info_timeout;
	struct completion cp_info_comp_var;
	struct mutex cp_channel_info_mutex;	
	struct LTE_5G_coex_cp_noti_info cp_noti_info[CP_BAND_MAX];
	unsigned int cp_channel_info_ready;
} SS_cp_wlan_coex;

static SS_cp_wlan_coex g_cp_wlan_coex;
static int cp_sap_coex_testmode_enabled;
static int cp_sap_coex_cp_tc;

static int cnss_cp_coex_ril_notifier(struct notifier_block *nb,
unsigned long size, void *buf)
{
	struct dev_ril_bridge_msg *msg;
	struct LTE_5G_coex_cp_noti_info *cp_noti_info;

	if (!g_cp_wlan_coex.init_notifier) {
		pr_err("[cnss_cp_coex] not init ril notifier");
		return NOTIFY_DONE;
	}

	pr_err("[cnss_cp_coex] ril notification size [%ld]", size);

	msg = (struct dev_ril_bridge_msg *)buf;
	pr_err("[cnss_cp_coex] dev_id : %d, data_len : %d",
	msg->dev_id, msg->data_len);

	if (msg->dev_id == IPC_SYSTEM_CP_CHANNEL_INFO
	&& msg->data_len == sizeof(struct LTE_5G_coex_cp_noti_info)) {
		cp_noti_info = (struct LTE_5G_coex_cp_noti_info *)msg->data;

		pr_err("[cnss_cp_coex] update CP channel info [%d,%d,%d]",
		cp_noti_info->rat, cp_noti_info->band, cp_noti_info->channel);
		
		//rat mode : LTE (3), NR5G (7)
		switch(cp_noti_info->rat)
			{
				case CP_COEX_LTE:
					g_cp_wlan_coex.cp_channel_info_ready |= (1 << CP_LTE_BAND);
					memcpy(&g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND], cp_noti_info, sizeof(struct LTE_5G_coex_cp_noti_info));
					break;
					
				case CP_COEX_NT5G:
					g_cp_wlan_coex.cp_channel_info_ready |= (1 << CP_5G_BAND);
					memcpy(&g_cp_wlan_coex.cp_noti_info[CP_5G_BAND], cp_noti_info, sizeof(struct LTE_5G_coex_cp_noti_info));
					break;

				default:
					pr_err("[cnss_cp_coex] discard channel info! rat=%d",cp_noti_info->rat);
					break;	
			}

		if(g_cp_wlan_coex.cp_channel_info_ready == CP_BOTH_BAND)
			{
				complete(&g_cp_wlan_coex.cp_info_comp_var);
				g_cp_wlan_coex.cp_channel_info_ready = CP_NO_BAND;
			}
			
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block g_ril_notifier_block = {
.notifier_call = cnss_cp_coex_ril_notifier,
};

static void cnss_cp_coex_register_ril_notifier(void)
{
	if (!g_cp_wlan_coex.init_notifier) {
	pr_err("[cnss_cp_coex] register ril notifier");
	init_completion(&g_cp_wlan_coex.cp_info_comp_var);
	memset(g_cp_wlan_coex.cp_noti_info, 0, sizeof(struct LTE_5G_coex_cp_noti_info)*2);
	g_cp_wlan_coex.cp_channel_info_ready = CP_NO_BAND;
	register_dev_ril_bridge_event_notifier(&g_ril_notifier_block);
	g_cp_wlan_coex.init_notifier = true;
	}
}

static void cnss_cp_coex_unregister_ril_notifier(void)
{
	if (g_cp_wlan_coex.init_notifier) {
	pr_err("[cnss_cp_coex] unregister ril notifier");
	//mutex_destroy(&g_cp_wlan_coex.cp_channel_info_mutex);
	unregister_dev_ril_bridge_event_notifier(&g_ril_notifier_block);
	complete_all(&g_cp_wlan_coex.cp_info_comp_var);
	g_cp_wlan_coex.init_notifier = false;
	}
}


/**
* cnss_utils_get_wlan_unsafe_channel_sap() - Get vendor unsafe ch freq ranges
* @dev: device
* @ch_avoid_ranges: unsafe freq channel ranges
*
* Get vendor specific unsafe channel frequency ranges
*
* Return: 0 for success
*         Non zero failure code for errors
*/

int cnss_utils_get_wlan_unsafe_channel_sap(struct device *dev,  struct cnss_ch_avoid_ind_type *ch_avoid_ranges)
{
	int val = 1;
	unsigned int rc = 0;
	unsigned int rc2 = 0;
	bool is_N41,is_B40,is_B41,is_N40;
	struct LTE_5G_coex_cp_noti_info lte_info;
	struct dev_ril_bridge_msg test_msg;

	is_N41 = is_B40 = is_B41 = is_N40 = false;
	cnss_cp_coex_register_ril_notifier();	
	rc = dev_ril_bridge_send_msg(IPC_SYSTEM_CP_CHANNEL_INFO, sizeof(int), &val);
	pr_err("[cnss_cp_coex] dev_ril_bridge_send_msg = %d",rc);
	//TODO Need to add error handling


	//Test mode 
	if(cp_sap_coex_testmode_enabled)
		{
			test_msg.dev_id = IPC_SYSTEM_CP_CHANNEL_INFO;
			test_msg.data_len = sizeof(struct LTE_5G_coex_cp_noti_info);
			test_msg.data =(void *)&lte_info;
			if(cp_sap_coex_cp_tc == 1){ //simulate a case to select ch11
				lte_info.rat = CP_COEX_LTE;
				lte_info.band = 130;
				lte_info.channel= 39000;	
				cnss_cp_coex_ril_notifier(&g_ril_notifier_block,sizeof(struct dev_ril_bridge_msg),&test_msg);
				lte_info.rat = CP_COEX_NT5G;
				lte_info.band = 0;
				lte_info.channel= 0;	
				cnss_cp_coex_ril_notifier(&g_ril_notifier_block,sizeof(struct dev_ril_bridge_msg),&test_msg);
			}
			else if(cp_sap_coex_cp_tc == 2){ //simulate a case to select ch1
				lte_info.rat = CP_COEX_LTE;
				lte_info.band = 131;
				lte_info.channel= 40000;	
				cnss_cp_coex_ril_notifier(&g_ril_notifier_block,sizeof(struct dev_ril_bridge_msg),&test_msg);
				lte_info.rat = CP_COEX_NT5G;
				lte_info.band = 0;
				lte_info.channel= 0;	
				cnss_cp_coex_ril_notifier(&g_ril_notifier_block,sizeof(struct dev_ril_bridge_msg),&test_msg);
			}
			else if(cp_sap_coex_cp_tc == 3){ //simulate a case to select ch6
				lte_info.rat = CP_COEX_LTE;
				lte_info.band = 130;
				lte_info.channel= 39000;	
				cnss_cp_coex_ril_notifier(&g_ril_notifier_block,sizeof(struct dev_ril_bridge_msg),&test_msg);
				lte_info.rat = CP_COEX_NT5G;
				lte_info.band = 296;
				lte_info.channel= 500000;	
				cnss_cp_coex_ril_notifier(&g_ril_notifier_block,sizeof(struct dev_ril_bridge_msg),&test_msg);
			}
			else 
				pr_err("[cnss_cp_coex] invalid TC = %d!",cp_sap_coex_cp_tc);
			
		}
	
	rc2 = wait_for_completion_timeout(&g_cp_wlan_coex.cp_info_comp_var,
					 msecs_to_jiffies(500));

	cnss_cp_coex_unregister_ril_notifier();
	
	if (!rc2 && 
		(g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].rat == CP_NONE) && 
		(g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].rat == CP_NONE)) {
		pr_err("[cnss_cp_coex] Timed out waiting for cp channel info! rc = %d",rc2);
		return -ETIMEDOUT;
	}

	// Rat mode : LTE (3), NR5G (7)
	// Band
	// LTE B40 : 130
	// LTE B41 : 131
	// 5G N41 : 296
	// 5G N40 : 295
	// channel
	// LTE/B40 only: 38650~39649 => Ch.11
	// LTE/B41 only: 39650~41589 => Ch.1
	// 5G/N41 only: 499200~537999 => Ch.1
	// 5G/N40 only: 460000~480000 => Ch.11
	// B40 && N41 or B41 && N40 => Ch.6
	pr_err("[cnss_cp_coex] g_cp_wlan_coex.cp_noti_info [%d,%d,%d]",g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].rat , 
	g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].band, g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].channel);
	
	pr_err("[cnss_cp_coex] g_cp_wlan_coex.cp_noti_info [%d,%d,%d]",g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].rat , 
	g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].band, g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].channel);

	if(g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].rat != CP_NONE)
		{
			if(g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].band == 130 && 
				(38650 <= g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].channel && g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].channel <= 39649))
				{
					is_B40 = true;
				}
			else if(g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].band == 131 && 
				(39650 <= g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].channel && g_cp_wlan_coex.cp_noti_info[CP_LTE_BAND].channel <= 41589))
				{
					is_B41 = true;
				}
		}

	if(g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].rat != CP_NONE)
		{
			if(g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].band == 296 && 
				(499200 <= g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].channel && g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].channel <= 537999))
				{
					is_N41= true;
				}
			else if(g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].band == 295 && 
				(460000 <= g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].channel && g_cp_wlan_coex.cp_noti_info[CP_5G_BAND].channel <= 480000))
				{
					is_N40 = true;
				}
		}	
	
	pr_err("[cnss_cp_coex] CP band info is_B40=%d,is_B41=%d,is_N41=%d, is_N40=%d",is_B40,is_B41,is_N41,is_N40);

	//Fill unsafe range
	//For ch 1: 2417- 2484, 5180-7115    
	//For ch 11: 2412-2457, 2467-2484, 5180-7115
	//For ch 6: 2412-2432, 2442-2484, 5180-7115

	if((is_N41 && !is_B40) || (is_B41 && !is_N40))
		{
			//ch1 preferred exclude others
			ch_avoid_ranges->avoid_freq_range[0].start_freq = 2417;
			ch_avoid_ranges->avoid_freq_range[0].end_freq = 2484;
			ch_avoid_ranges->avoid_freq_range[1].start_freq = 5180;
			ch_avoid_ranges->avoid_freq_range[1].end_freq = 7115;
			ch_avoid_ranges->ch_avoid_range_cnt=2;
		}
	else if((is_N41 && is_B40) || (is_B41 && is_N40))
		{
			//ch6 perferred exclude others
			ch_avoid_ranges->avoid_freq_range[0].start_freq = 2412;
			ch_avoid_ranges->avoid_freq_range[0].end_freq = 2432;
			ch_avoid_ranges->avoid_freq_range[1].start_freq = 2442;
			ch_avoid_ranges->avoid_freq_range[1].end_freq = 2484;
			ch_avoid_ranges->avoid_freq_range[2].start_freq = 5180;
			ch_avoid_ranges->avoid_freq_range[2].end_freq = 7115;	
			ch_avoid_ranges->ch_avoid_range_cnt=3;
		}
	else if((!is_N41 && is_B40) || (!is_B41 && is_N40))
		{
			//ch11 perferred exclude others
			ch_avoid_ranges->avoid_freq_range[0].start_freq = 2412;
			ch_avoid_ranges->avoid_freq_range[0].end_freq = 2457;
			ch_avoid_ranges->avoid_freq_range[1].start_freq = 2467;
			ch_avoid_ranges->avoid_freq_range[1].end_freq = 2484;
			ch_avoid_ranges->avoid_freq_range[2].start_freq = 5180;
			ch_avoid_ranges->avoid_freq_range[2].end_freq = 7115;	
			ch_avoid_ranges->ch_avoid_range_cnt=3;
		}
	else
		{
			//No effective Band
			ch_avoid_ranges->ch_avoid_range_cnt=0;
			return -ENODATA;
		}
 	
	return 0;
}
EXPORT_SYMBOL(cnss_utils_get_wlan_unsafe_channel_sap);

int cnss_utils_set_wlan_unsafe_channel(struct device *dev,
				       u16 *unsafe_ch_list, u16 ch_count)
{
	struct cnss_utils_priv *priv = cnss_utils_priv;

	if (!priv)
		return -EINVAL;

	mutex_lock(&priv->unsafe_channel_list_lock);
	if (!unsafe_ch_list || ch_count > CNSS_MAX_CH_NUM) {
		mutex_unlock(&priv->unsafe_channel_list_lock);
		return -EINVAL;
	}

	priv->unsafe_channel_list.unsafe_ch_count = ch_count;

	if (ch_count == 0)
		goto end;

	memcpy(priv->unsafe_channel_list.unsafe_ch_list,
	       unsafe_ch_list, ch_count * sizeof(u16));

end:
	mutex_unlock(&priv->unsafe_channel_list_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_utils_set_wlan_unsafe_channel);

int cnss_utils_get_wlan_unsafe_channel(struct device *dev,
				       u16 *unsafe_ch_list,
				       u16 *ch_count, u16 buf_len)
{
	struct cnss_utils_priv *priv = cnss_utils_priv;

	if (!priv)
		return -EINVAL;

	mutex_lock(&priv->unsafe_channel_list_lock);
	if (!unsafe_ch_list || !ch_count) {
		mutex_unlock(&priv->unsafe_channel_list_lock);
		return -EINVAL;
	}

	if (buf_len <
	    (priv->unsafe_channel_list.unsafe_ch_count * sizeof(u16))) {
		mutex_unlock(&priv->unsafe_channel_list_lock);
		return -ENOMEM;
	}

	*ch_count = priv->unsafe_channel_list.unsafe_ch_count;
	memcpy(unsafe_ch_list, priv->unsafe_channel_list.unsafe_ch_list,
	       priv->unsafe_channel_list.unsafe_ch_count * sizeof(u16));
	mutex_unlock(&priv->unsafe_channel_list_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_utils_get_wlan_unsafe_channel);

int cnss_utils_wlan_set_dfs_nol(struct device *dev,
				const void *info, u16 info_len)
{
	void *temp;
	void *old_nol_info;
	struct cnss_dfs_nol_info *dfs_info;
	struct cnss_utils_priv *priv = cnss_utils_priv;

	if (!priv)
		return -EINVAL;

	if (!info || !info_len)
		return -EINVAL;

	temp = kmemdup(info, info_len, GFP_ATOMIC);
	if (!temp)
		return -ENOMEM;

	spin_lock_bh(&priv->dfs_nol_info_lock);
	dfs_info = &priv->dfs_nol_info;
	old_nol_info = dfs_info->dfs_nol_info;
	dfs_info->dfs_nol_info = temp;
	dfs_info->dfs_nol_info_len = info_len;
	spin_unlock_bh(&priv->dfs_nol_info_lock);
	kfree(old_nol_info);

	return 0;
}
EXPORT_SYMBOL(cnss_utils_wlan_set_dfs_nol);

int cnss_utils_wlan_get_dfs_nol(struct device *dev,
				void *info, u16 info_len)
{
	int len;
	struct cnss_dfs_nol_info *dfs_info;
	struct cnss_utils_priv *priv = cnss_utils_priv;

	if (!priv)
		return -EINVAL;

	if (!info || !info_len)
		return -EINVAL;

	spin_lock_bh(&priv->dfs_nol_info_lock);

	dfs_info = &priv->dfs_nol_info;
	if (!dfs_info->dfs_nol_info ||
	    dfs_info->dfs_nol_info_len == 0) {
		spin_unlock_bh(&priv->dfs_nol_info_lock);
		return -ENOENT;
	}

	len = min(info_len, dfs_info->dfs_nol_info_len);
	memcpy(info, dfs_info->dfs_nol_info, len);
	spin_unlock_bh(&priv->dfs_nol_info_lock);

	return len;
}
EXPORT_SYMBOL(cnss_utils_wlan_get_dfs_nol);

void cnss_utils_increment_driver_load_cnt(struct device *dev)
{
	struct cnss_utils_priv *priv = cnss_utils_priv;

	if (!priv)
		return;

	++(priv->driver_load_cnt);
}
EXPORT_SYMBOL(cnss_utils_increment_driver_load_cnt);

int cnss_utils_get_driver_load_cnt(struct device *dev)
{
	struct cnss_utils_priv *priv = cnss_utils_priv;

	if (!priv)
		return -EINVAL;

	return priv->driver_load_cnt;
}
EXPORT_SYMBOL(cnss_utils_get_driver_load_cnt);

static int set_wlan_mac_address(const u8 *mac_list, const uint32_t len,
				enum mac_type type)
{
	struct cnss_utils_priv *priv = cnss_utils_priv;
	u32 no_of_mac_addr;
	struct cnss_wlan_mac_addr *addr = NULL;
	int iter;
	u8 *temp = NULL;

	if (!priv)
		return -EINVAL;

	if (len == 0 || (len % ETH_ALEN) != 0) {
		pr_err("Invalid length %d\n", len);
		return -EINVAL;
	}

	no_of_mac_addr = len / ETH_ALEN;
	if (no_of_mac_addr > MAX_NO_OF_MAC_ADDR) {
		pr_err("Exceed maximum supported MAC address %u %u\n",
		       MAX_NO_OF_MAC_ADDR, no_of_mac_addr);
		return -EINVAL;
	}

	if (type == CNSS_MAC_PROVISIONED)
		addr = &priv->wlan_mac_addr;
	else
		addr = &priv->wlan_der_mac_addr;

	if (addr->no_of_mac_addr_set) {
		pr_err("WLAN MAC address is already set, num %d type %d\n",
		       addr->no_of_mac_addr_set, type);
		return 0;
	}

	addr->no_of_mac_addr_set = no_of_mac_addr;
	temp = &addr->mac_addr[0][0];

	for (iter = 0; iter < no_of_mac_addr;
	     ++iter, temp += ETH_ALEN, mac_list += ETH_ALEN) {
		ether_addr_copy(temp, mac_list);
		pr_debug("MAC_ADDR:%02x:%02x:%02x:%02x:%02x:%02x\n",
			 temp[0], temp[1], temp[2],
			 temp[3], temp[4], temp[5]);
	}
	return 0;
}

int cnss_utils_set_wlan_mac_address(const u8 *mac_list, const uint32_t len)
{
	return set_wlan_mac_address(mac_list, len, CNSS_MAC_PROVISIONED);
}
EXPORT_SYMBOL(cnss_utils_set_wlan_mac_address);

int cnss_utils_set_wlan_derived_mac_address(const u8 *mac_list,
					    const uint32_t len)
{
	return set_wlan_mac_address(mac_list, len, CNSS_MAC_DERIVED);
}
EXPORT_SYMBOL(cnss_utils_set_wlan_derived_mac_address);

static u8 *get_wlan_mac_address(struct device *dev,
				u32 *num, enum mac_type type)
{
	struct cnss_utils_priv *priv = cnss_utils_priv;
	struct cnss_wlan_mac_addr *addr = NULL;

	if (!priv)
		goto out;

	if (type == CNSS_MAC_PROVISIONED)
		addr = &priv->wlan_mac_addr;
	else
		addr = &priv->wlan_der_mac_addr;

	if (!addr->no_of_mac_addr_set) {
		pr_err("WLAN MAC address is not set, type %d\n", type);
		goto out;
	}
	*num = addr->no_of_mac_addr_set;
	return &addr->mac_addr[0][0];

out:
	*num = 0;
	return NULL;
}

u8 *cnss_utils_get_wlan_mac_address(struct device *dev, uint32_t *num)
{
	return get_wlan_mac_address(dev, num, CNSS_MAC_PROVISIONED);
}
EXPORT_SYMBOL(cnss_utils_get_wlan_mac_address);

u8 *cnss_utils_get_wlan_derived_mac_address(struct device *dev,
					    uint32_t *num)
{
	return get_wlan_mac_address(dev, num, CNSS_MAC_DERIVED);
}
EXPORT_SYMBOL(cnss_utils_get_wlan_derived_mac_address);

void cnss_utils_set_cc_source(struct device *dev,
			      enum cnss_utils_cc_src cc_source)
{
	struct cnss_utils_priv *priv = cnss_utils_priv;

	if (!priv)
		return;

	priv->cc_source = cc_source;
}
EXPORT_SYMBOL(cnss_utils_set_cc_source);

enum cnss_utils_cc_src cnss_utils_get_cc_source(struct device *dev)
{
	struct cnss_utils_priv *priv = cnss_utils_priv;

	if (!priv)
		return -EINVAL;

	return priv->cc_source;
}
EXPORT_SYMBOL(cnss_utils_get_cc_source);

static ssize_t cnss_utils_mac_write(struct file *fp,
				    const char __user *user_buf,
				    size_t count, loff_t *off)
{
	struct cnss_utils_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[128];
	char *input, *mac_type, *mac_address;
	u8 *dest_mac;
	u8 val;
	const char *delim = "\n";
	size_t len = 0;
	char temp[3] = "";

	len = min_t(size_t, count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EINVAL;
	buf[len] = '\0';

	input = buf;

	mac_type = strsep(&input, delim);
	if (!mac_type)
		return -EINVAL;
	if (!input)
		return -EINVAL;

	mac_address = strsep(&input, delim);
	if (!mac_address)
		return -EINVAL;
	if (strcmp("0x", mac_address)) {
		pr_err("Invalid MAC prefix\n");
		return -EINVAL;
	}

	len = strlen(mac_address);
	mac_address += MAC_PREFIX_LEN;
	len -= MAC_PREFIX_LEN;
	if (len < ETH_ALEN * 2 || len > ETH_ALEN * 2 * MAX_NO_OF_MAC_ADDR ||
	    len % (ETH_ALEN * 2) != 0) {
		pr_err("Invalid MAC address length %zu\n", len);
		return -EINVAL;
	}

	if (!strcmp("provisioned", mac_type)) {
		dest_mac = &priv->wlan_mac_addr.mac_addr[0][0];
		priv->wlan_mac_addr.no_of_mac_addr_set = len / (ETH_ALEN * 2);
	} else if (!strcmp("derived", mac_type)) {
		dest_mac = &priv->wlan_der_mac_addr.mac_addr[0][0];
		priv->wlan_der_mac_addr.no_of_mac_addr_set =
			len / (ETH_ALEN * 2);
	} else {
		pr_err("Invalid MAC address type %s\n", mac_type);
		return -EINVAL;
	}

	while (len--) {
		temp[0] = *mac_address++;
		temp[1] = *mac_address++;
		if (kstrtou8(temp, 16, &val))
			return -EINVAL;
		*dest_mac++ = val;
	}
	return count;
}

static int cnss_utils_mac_show(struct seq_file *s, void *data)
{
	u8 mac[6];
	int i;
	struct cnss_utils_priv *priv = s->private;
	struct cnss_wlan_mac_addr *addr = NULL;

	addr = &priv->wlan_mac_addr;
	if (addr->no_of_mac_addr_set) {
		seq_puts(s, "\nProvisioned MAC addresseses\n");
		for (i = 0; i < addr->no_of_mac_addr_set; i++) {
			ether_addr_copy(mac, addr->mac_addr[i]);
			seq_printf(s, "MAC_ADDR:%02x:%02x:%02x:%02x:%02x:%02x\n",
				   mac[0], mac[1], mac[2],
				   mac[3], mac[4], mac[5]);
		}
	}

	addr = &priv->wlan_der_mac_addr;
	if (addr->no_of_mac_addr_set) {
		seq_puts(s, "\nDerived MAC addresseses\n");
		for (i = 0; i < addr->no_of_mac_addr_set; i++) {
			ether_addr_copy(mac, addr->mac_addr[i]);
			seq_printf(s, "MAC_ADDR:%02x:%02x:%02x:%02x:%02x:%02x\n",
				   mac[0], mac[1], mac[2],
				   mac[3], mac[4], mac[5]);
		}
	}

	return 0;
}

static int cnss_utils_mac_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_utils_mac_show, inode->i_private);
}

static const struct file_operations cnss_utils_mac_fops = {
	.read		= seq_read,
	.write		= cnss_utils_mac_write,
	.release	= single_release,
	.open		= cnss_utils_mac_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int cnss_utils_debugfs_create(struct cnss_utils_priv *priv)
{
	int ret = 0;
	struct dentry *root_dentry;

	root_dentry = debugfs_create_dir("cnss_utils", NULL);

	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		pr_err("Unable to create debugfs %d\n", ret);
		goto out;
	}
	priv->root_dentry = root_dentry;
	debugfs_create_file("mac_address", 0600, root_dentry, priv,
			    &cnss_utils_mac_fops);
out:
	return ret;
}

/**
 * cnss_utils_is_valid_dt_node_found - Check if valid device tree node present
 *
 * Valid device tree node means a node with "qcom,wlan" property present and
 * "status" property not disabled.
 *
 * Return: true if valid device tree node found, false if not found
 */
static bool cnss_utils_is_valid_dt_node_found(void)
{
	struct device_node *dn = NULL;

	for_each_node_with_property(dn, "qcom,wlan") {
		if (of_device_is_available(dn))
			break;
	}

	if (dn)
		return true;

	return false;
}

static int __init cnss_utils_init(void)
{
	struct cnss_utils_priv *priv = NULL;

	if (!cnss_utils_is_valid_dt_node_found())
		return -ENODEV;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->cc_source = CNSS_UTILS_SOURCE_CORE;

	mutex_init(&priv->unsafe_channel_list_lock);
	spin_lock_init(&priv->dfs_nol_info_lock);
	cnss_utils_debugfs_create(priv);
	cnss_utils_priv = priv;

	return 0;
}

static void __exit cnss_utils_exit(void)
{
	kfree(cnss_utils_priv);
	cnss_utils_priv = NULL;
}

module_param(cp_sap_coex_testmode_enabled, int, 0644);
module_param(cp_sap_coex_cp_tc, int, 0644);

module_init(cnss_utils_init);
module_exit(cnss_utils_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CNSS Utilities Driver");
