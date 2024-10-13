/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WIFI_FCQ_H__
#define __KUNIT_MOCK_SCSC_WIFI_FCQ_H__

#include "../scsc_wifi_fcq.h"

#define scsc_wifi_fcq_update_smod(args...)		kunit_mock_scsc_wifi_fcq_update_smod(args)
#define scsc_wifi_fcq_ctrl_q_deinit(args...)		kunit_mock_scsc_wifi_fcq_ctrl_q_deinit(args)
#define scsc_wifi_fcq_multicast_qset_init(args...)	kunit_mock_scsc_wifi_fcq_multicast_qset_init(args)
#define scsc_wifi_fcq_unpause_queues(args...)		kunit_mock_scsc_wifi_fcq_unpause_queues(args)
#define scsc_wifi_fcq_receive_ctrl(args...)		kunit_mock_scsc_wifi_fcq_receive_ctrl(args)
#define scsc_wifi_fcq_pause_queues(args...)		kunit_mock_scsc_wifi_fcq_pause_queues(args)
#define scsc_wifi_pause_arp_q_all_vif(args...)		kunit_mock_scsc_wifi_pause_arp_q_all_vif(args)
#define scsc_wifi_fcq_stat_queue(args...)		kunit_mock_scsc_wifi_fcq_stat_queue(args)
#define scsc_wifi_fcq_transmit_data(args...)		kunit_mock_scsc_wifi_fcq_transmit_data(args)
#define scsc_wifi_fcq_stat_queueset(args...)		kunit_mock_scsc_wifi_fcq_stat_queueset(args)
#define scsc_wifi_fcq_8021x_port_state(args...)		kunit_mock_scsc_wifi_fcq_8021x_port_state(args)
#define scsc_wifi_fcq_qset_deinit(args...)		kunit_mock_scsc_wifi_fcq_qset_deinit(args)
#define scsc_wifi_fcq_ctrl_q_init(args...)		kunit_mock_scsc_wifi_fcq_ctrl_q_init(args)
#define scsc_wifi_fcq_unicast_qset_init(args...)	kunit_mock_scsc_wifi_fcq_unicast_qset_init(args)
#define scsc_wifi_fcq_transmit_ctrl(args...)		kunit_mock_scsc_wifi_fcq_transmit_ctrl(args)
#define scsc_wifi_fcq_receive_data(args...)		kunit_mock_scsc_wifi_fcq_receive_data(args)
#define scsc_wifi_fcq_receive_data_no_peer(args...)	kunit_mock_scsc_wifi_fcq_receive_data_no_peer(args)
#define scsc_wifi_unpause_arp_q_all_vif(args...)	kunit_mock_scsc_wifi_unpause_arp_q_all_vif(args)


static int kunit_mock_scsc_wifi_fcq_update_smod(struct scsc_wifi_fcq_data_qset *qs,
						enum scsc_wifi_fcq_ps_state peer_ps_state,
						enum scsc_wifi_fcq_queue_set_type type)
{
	return 0;
}

static void kunit_mock_scsc_wifi_fcq_ctrl_q_deinit(struct scsc_wifi_fcq_ctrl_q *queue)
{
	return;
}

static int kunit_mock_scsc_wifi_fcq_multicast_qset_init(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs,
							struct slsi_dev *sdev, u8 vif)
{
	return 0;
}

static void kunit_mock_scsc_wifi_fcq_unpause_queues(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_scsc_wifi_fcq_receive_ctrl(struct net_device *dev, struct scsc_wifi_fcq_ctrl_q *queue)
{
	return 0;
}

static void kunit_mock_scsc_wifi_fcq_pause_queues(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_scsc_wifi_pause_arp_q_all_vif(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_scsc_wifi_fcq_stat_queue(struct scsc_wifi_fcq_q_header *queue,
			     struct scsc_wifi_fcq_q_stat *queue_stat,
			     int *qmod, int *qcod)
{
	return 0;
}

static int kunit_mock_scsc_wifi_fcq_transmit_data(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs,
						  u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	return 0;
}

static int kunit_mock_scsc_wifi_fcq_stat_queueset(struct scsc_wifi_fcq_data_qset *queue_set,
				struct scsc_wifi_fcq_q_stat *queue_stat,
				int *smod, int *scod, enum scsc_wifi_fcq_8021x_state *cp_state,
				u32 *peer_ps_state_transitions)
{
	return 0;
}

static int kunit_mock_scsc_wifi_fcq_8021x_port_state(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs,
						     enum scsc_wifi_fcq_8021x_state state)
{
	return 0;
}

static void kunit_mock_scsc_wifi_fcq_qset_deinit(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs,
						 struct slsi_dev *sdev, u8 vif, struct slsi_peer *peer)
{
	return;
}

static int kunit_mock_scsc_wifi_fcq_ctrl_q_init(struct scsc_wifi_fcq_ctrl_q *queue)
{
	return 0;
}

static int kunit_mock_scsc_wifi_fcq_unicast_qset_init(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs,
						      u8 qs_num, struct slsi_dev *sdev, u8 vif, struct slsi_peer *peer)
{
	return 0;
}

static int kunit_mock_scsc_wifi_fcq_transmit_ctrl(struct net_device *dev, struct scsc_wifi_fcq_ctrl_q *queue)
{
	return 0;
}

static int kunit_mock_scsc_wifi_fcq_receive_data(struct net_device *dev, struct scsc_wifi_fcq_data_qset *qs,
						 u16 priority, struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	return 0;
}

static int kunit_mock_scsc_wifi_fcq_receive_data_no_peer(struct net_device *dev, u16 priority,
							 struct slsi_dev *sdev, u8 vif, u8 peer_index)
{
	return 0;
}

static void kunit_mock_scsc_wifi_unpause_arp_q_all_vif(struct slsi_dev *sdev)
{
	return;
}
#endif
