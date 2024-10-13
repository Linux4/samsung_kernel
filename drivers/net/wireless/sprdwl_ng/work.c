/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Dong Xiang <dong.xiang@spreadtrum.com>
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

#include "sprdwl.h"
#include "work.h"

struct sprdwl_work *sprdwl_alloc_work(int len)
{
	struct sprdwl_work *sprdwl_work;
	int size = sizeof(*sprdwl_work) + len;

	sprdwl_work = kzalloc(size, GFP_ATOMIC);
	if (sprdwl_work) {
		INIT_LIST_HEAD(&sprdwl_work->list);
		sprdwl_work->len = len;
	}

	return sprdwl_work;
}

static struct sprdwl_work *sprdwl_get_work(struct sprdwl_priv *priv)
{
	struct sprdwl_work *sprdwl_work = NULL;

	spin_lock_bh(&priv->work_lock);
	if (!list_empty(&priv->work_list)) {
		sprdwl_work = list_first_entry(&priv->work_list,
					       struct sprdwl_work, list);
		list_del(&sprdwl_work->list);
	}
	spin_unlock_bh(&priv->work_lock);

	return sprdwl_work;
}

static void sprdwl_do_work(struct work_struct *work)
{
	struct sprdwl_work *sprdwl_work;
	struct sprdwl_reg_mgmt *reg_mgmt;
	struct sprdwl_data2mgmt *data;
	struct sprdwl_vif *vif;
	struct sprdwl_priv *priv = container_of(work, struct sprdwl_priv, work);

	while (1) {
		sprdwl_work = sprdwl_get_work(priv);
		if (!sprdwl_work)
			return;

		vif = sprdwl_work->vif;
		netdev_dbg(vif->ndev, "process delayed work: %d\n",
			   sprdwl_work->id);

		switch (sprdwl_work->id) {
		case SPRDWL_WORK_REG_MGMT:
			reg_mgmt = (struct sprdwl_reg_mgmt *)sprdwl_work->data;
			sprdwl_register_frame(priv, vif->mode,
					      reg_mgmt->type,
					      reg_mgmt->reg ? 1 : 0);
			break;
		case SPRDWL_WORK_DEAUTH:
			cfg80211_send_unprot_deauth(vif->ndev,
						    sprdwl_work->data,
						    sprdwl_work->len);
			break;
		case SPRDWL_WORK_DISASSOC:
			cfg80211_send_unprot_disassoc(vif->ndev,
						      sprdwl_work->data,
						      sprdwl_work->len);
			break;
		case SPRDWL_WORK_MC_FILTER:
			if (vif->mc_filter->mc_change)
				sprdwl_set_mc_filter(priv, vif->mode,
						     vif->mc_filter->subtype,
						     vif->mc_filter->mac_num,
						     vif->mc_filter->mac_addr);
			break;
		case SPRDWL_WORK_NOTIFY_IP:
			sprdwl_notify_ip(priv, vif->mode, SPRDWL_IPV6,
					 sprdwl_work->data);
			break;
		case SPRDWL_WORK_DATA2MGMT:
			data = (struct sprdwl_data2mgmt *)sprdwl_work->data;

			sprdwl_xmit_data2mgmt(data->skb, data->ndev);
			break;
		case SPRDWL_WORK_DEL_STA:
			sprdwl_del_station(vif->priv, vif->mode,
					   sprdwl_work->data,
					   WLAN_REASON_DEAUTH_LEAVING);
			break;
		default:
			netdev_dbg(vif->ndev, "Unknown delayed work: %d\n",
				   sprdwl_work->id);
			break;
		}

		kfree(sprdwl_work);
	}
}

void sprdwl_queue_work(struct sprdwl_priv *priv,
		       struct sprdwl_work *sprdwl_work)
{
	spin_lock_bh(&priv->work_lock);
	list_add_tail(&sprdwl_work->list, &priv->work_list);
	spin_unlock_bh(&priv->work_lock);

	if (!work_pending(&priv->work))
		schedule_work(&priv->work);
}

void sprdwl_cancle_work(struct sprdwl_priv *priv, struct sprdwl_vif *vif)
{
	struct sprdwl_work *sprdwl_work, *pos;

	spin_lock_bh(&priv->work_lock);
	list_for_each_entry_safe(sprdwl_work, pos, &priv->work_list, list) {
		if (vif == sprdwl_work->vif) {
			list_del(&sprdwl_work->list);
			kfree(sprdwl_work);
		}
	}
	spin_unlock_bh(&priv->work_lock);

	flush_work(&priv->work);
}

void sprdwl_init_work(struct sprdwl_priv *priv)
{
	spin_lock_init(&priv->work_lock);
	INIT_LIST_HEAD(&priv->work_list);
	INIT_WORK(&priv->work, sprdwl_do_work);
}

void sprdwl_deinit_work(struct sprdwl_priv *priv)
{
	struct sprdwl_work *sprdwl_work, *pos;

	cancel_work_sync(&priv->work);

	list_for_each_entry_safe(sprdwl_work, pos, &priv->work_list, list) {
		list_del(&sprdwl_work->list);
		kfree(sprdwl_work);
	}
}
