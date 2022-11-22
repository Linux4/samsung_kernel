/*
 * Copyright (c) 2012-2014, 2017-2018 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined(WLAN_HDD_IOCTL_H)
#define WLAN_HDD_IOCTL_H

#include <linux/netdevice.h>
#include <uapi/linux/if.h>
#include "wlan_hdd_main.h"

extern struct sock *cesium_nl_srv_sock;

int hdd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
int wlan_hdd_set_mc_rate(struct hdd_adapter *adapter, int targetRate);

/**
 * hdd_update_smps_antenna_mode() - set smps and antenna mode
 * @hdd_ctx: Pointer to hdd context
 * @mode: antenna mode
 *
 * This function will set smps and antenna mode.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_update_smps_antenna_mode(struct hdd_context *hdd_ctx, int mode);

/**
 * hdd_set_antenna_mode() - SET ANTENNA MODE command handler
 * @adapter: Pointer to network adapter
 * @hdd_ctx: Pointer to hdd context
 * @mode: new anteena mode
 */
int hdd_set_antenna_mode(struct hdd_adapter *adapter,
			  struct hdd_context *hdd_ctx, int mode);

#ifdef SEC_CONFIG_POWER_BACKOFF
#define SAR_POWER_LIMIT_FOR_GRIP_SENSOR	0
#define SAR_POWER_LIMIT_FOR_DBS		1
int hdd_set_sar_power_limit(struct hdd_context *hdd_ctx, uint8_t index, bool enable);
#ifdef SEC_CONFIG_WLAN_BEACON_CHECK
void hdd_skip_bmiss_set_timer_handler(void *data);
#endif
#endif /* SEC_CONFIG_POWER_BACKOFF */

#endif /* end #if !defined(WLAN_HDD_IOCTL_H) */

