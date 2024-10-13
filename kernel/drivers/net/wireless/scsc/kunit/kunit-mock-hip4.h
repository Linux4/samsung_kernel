/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_HIP4_H__
#define __KUNIT_MOCK_HIP4_H__

#include <linux/version.h>
#include <linux/if_ether.h>
#include <linux/rtc.h>

#include "../fapi.h"
#include "../hip4.h"

#define slsi_hip_free_control_slots_count(args...)	kunit_mock_slsi_hip_free_control_slots_count(args)
#define slsi_hip_setup(args...)				kunit_mock_slsi_hip_setup(args)
#define slsi_hip_sched_wq_ctrl(args...)			kunit_mock_slsi_hip_sched_wq_ctrl(args)
#define slsi_hip_resume(args...)			kunit_mock_slsi_hip_resume(args)
#define slsi_hip_freeze(args...)			kunit_mock_slsi_hip_freeze(args)
#define slsi_hip_from_host_intr_set(args...)		kunit_mock_slsi_hip_from_host_intr_set(args)
#define slsi_hip_deinit(args...)			kunit_mock_slsi_hip_deinit(args)
#define hip4_sched_wq(args...)				kunit_mock_hip4_sched_wq(args)
#define slsi_hip_suspend(args...)			kunit_mock_slsi_hip_suspend(args)
#define slsi_hip_transmit_frame(args...)		kunit_mock_slsi_hip_transmit_frame(args)
#define slsi_hip_init(args...)				kunit_mock_slsi_hip_init(args)
#define slsi_hip_wlan_get_rtc_time(args...)		kunit_mock_slsi_hip_wlan_get_rtc_time(args)
#define slsi_hip_napi_cpu_set(args...)			kunit_mock_slsi_hip_napi_cpu_set(args)


static int kunit_mock_slsi_hip_free_control_slots_count(struct slsi_hip *hip)
{
	if (!hip || hip->card_info.sdio_block_size > 300)
		return -EIO;

	return hip->card_info.sdio_block_size;
}

static int kunit_mock_slsi_hip_setup(struct slsi_hip *hip)
{
	return 0;
}

static void kunit_mock_slsi_hip_sched_wq_ctrl(struct slsi_hip *hip)
{
	return;
}

static void kunit_mock_slsi_hip_resume(struct slsi_hip *hip)
{
	return;
}

static void kunit_mock_slsi_hip_freeze(struct slsi_hip *hip)
{
	return;
}

static void kunit_mock_slsi_hip_from_host_intr_set(struct scsc_service *service, struct slsi_hip *hip)
{
	return;
}

static void kunit_mock_slsi_hip_deinit(struct slsi_hip *hip)
{
	return;
}

static void kunit_mock_hip4_sched_wq(struct slsi_hip *hip)
{
	return;
}

static void kunit_mock_slsi_hip_suspend(struct slsi_hip *hip)
{
	return;
}

static int kunit_mock_slsi_hip_transmit_frame(struct slsi_hip *hip, struct sk_buff *skb, bool ctrl_packet,
					      u8 vif_index, u8 peer_index, u8 priority)
{
	if (!hip || !skb)
		return -EINVAL;

	if (skb->data && ((struct fapi_signal *)(skb)->data)->id == FAPI_SAP_TYPE_MA)
		return NETDEV_TX_OK;
	else
		return NETDEV_TX_BUSY;
}

static int kunit_mock_slsi_hip_init(struct slsi_hip *hip)
{
	return 0;
}

static int kunit_mock_slsi_hip_wlan_get_rtc_time(struct rtc_time *tm)
{
	return 0;
}

static void kunit_mock_slsi_hip_napi_cpu_set(struct slsi_hip *hip, u8 napi_cpu, bool perf_mode)
{
	return;
}
#endif
