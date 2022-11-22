/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/seq_file.h>
#include <net/tcp.h>

#include "dev.h"

/* TCP send buffer sizes */
extern int sysctl_tcp_wmem[3];

#ifndef __HIP4_SAMPLER_H__
#define __HIP4_SAMPLER_H__

#define HIP4_SAMPLER_SIGNAL_CTRLTX      0x20
#define HIP4_SAMPLER_SIGNAL_CTRLRX      0x21
#define HIP4_SAMPLER_THROUG             0x22
#define HIP4_SAMPLER_THROUG_K           0x23
#define HIP4_SAMPLER_THROUG_M           0x24
#define HIP4_SAMPLER_STOP_Q             0x25
#define HIP4_SAMPLER_START_Q            0x26
#define HIP4_SAMPLER_QREF               0x27
#define HIP4_SAMPLER_PEER               0x29
#define HIP4_SAMPLER_BOT_RX             0x2a
#define HIP4_SAMPLER_BOT_TX             0x2b
#define HIP4_SAMPLER_BOT_ADD            0x2c
#define HIP4_SAMPLER_BOT_REMOVE         0x2d
#define HIP4_SAMPLER_BOT_STOP_Q         0x2e
#define HIP4_SAMPLER_BOT_START_Q        0x2f
#define HIP4_SAMPLER_BOT_QMOD_RX        0x30
#define HIP4_SAMPLER_BOT_QMOD_TX        0x31
#define HIP4_SAMPLER_BOT_QMOD_STOP      0x32
#define HIP4_SAMPLER_BOT_QMOD_START     0x33
#define HIP4_SAMPLER_PKT_TX             0x40
#define HIP4_SAMPLER_PKT_TX_HIP4        0x41
#define HIP4_SAMPLER_PKT_TX_FB          0x42
#define HIP4_SAMPLER_SUSPEND            0x50
#define HIP4_SAMPLER_RESUME             0x51

#define HIP4_SAMPLER_TCP_SYN            0x60
#define HIP4_SAMPLER_TCP_FIN            0x61
#define HIP4_SAMPLER_TCP_DATA           0x62
#define HIP4_SAMPLER_TCP_ACK            0x63
#define HIP4_SAMPLER_TCP_RWND           0x64
#define HIP4_SAMPLER_TCP_CWND           0x65
#define HIP4_SAMPLER_TCP_SEND_BUF       0x66
#define HIP4_SAMPLER_TCP_DATA_IN        0x67
#define HIP4_SAMPLER_TCP_ACK_IN         0x68

#define HIP4_SAMPLER_MBULK              0xaa
#define HIP4_SAMPLER_QFULL              0xbb
#define HIP4_SAMPLER_MFULL              0xcc
#define HIP4_SAMPLER_INT                0xdd
#define HIP4_SAMPLER_INT_OUT            0xee
#define HIP4_SAMPLER_INT_BH             0xde
#define HIP4_SAMPLER_INT_OUT_BH		0xef
#define HIP4_SAMPLER_RESET              0xff

#define SCSC_HIP4_INTERFACES	1

#define SCSC_HIP4_STREAM_CH     1
#define SCSC_HIP4_OFFLINE_CH    SCSC_HIP4_STREAM_CH

#if (SCSC_HIP4_OFFLINE_CH != SCSC_HIP4_STREAM_CH)
#error "SCSC_HIP4_STREAM_CH has to be equal to SCSC_HIP4_OFFLINE_CH"
#endif

#define SCSC_HIP4_DEBUG_INTERFACES	((SCSC_HIP4_INTERFACES) * (SCSC_HIP4_STREAM_CH + SCSC_HIP4_OFFLINE_CH))

struct scsc_mx;

void hip4_sampler_create(struct slsi_dev *sdev, struct scsc_mx *mx);
void hip4_sampler_destroy(struct slsi_dev *sdev, struct scsc_mx *mx);

/* Register hip4 instance with the logger */
/* return char device minor associated with the maxwell instance*/
int hip4_sampler_register_hip(struct scsc_mx *mx);

void hip4_sampler_update_record(u32 minor, u8 param1, u8 param2, u8 param3, u8 param4, u32 param5);
void hip4_sampler_tcp_decode(struct slsi_dev *sdev, struct net_device *dev, u8 *frame, bool from_ba);

extern bool hip4_sampler_sample_q;
extern bool hip4_sampler_sample_qref;
extern bool hip4_sampler_sample_int;
extern bool hip4_sampler_sample_fapi;
extern bool hip4_sampler_sample_through;
extern bool hip4_sampler_sample_tcp;
extern bool hip4_sampler_sample_start_stop_q;
extern bool hip4_sampler_sample_mbulk;
extern bool hip4_sampler_sample_qfull;
extern bool hip4_sampler_sample_mfull;
extern bool hip4_sampler_vif;
extern bool hip4_sampler_bot;
extern bool hip4_sampler_pkt_tx;
extern bool hip4_sampler_suspend_resume;

#ifdef CONFIG_SCSC_WLAN_HIP4_PROFILING
#define SCSC_HIP4_SAMPLER_Q(minor, q, idx_rw, value, rw) \
	do { \
		if (hip4_sampler_sample_q) { \
			hip4_sampler_update_record(minor, q, idx_rw, value, rw, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_QREF(minor, ref, q) \
	do { \
		if (hip4_sampler_sample_qref) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_QREF, (ref & 0xff0000) >> 16, (ref & 0xff00) >> 8, (ref & 0xf0) | q, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_SIGNAL_CTRLTX(minor, bytes16_h, bytes16_l) \
	do { \
		if (hip4_sampler_sample_fapi) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_SIGNAL_CTRLTX, 0, bytes16_h, bytes16_l, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_SIGNAL_CTRLRX(minor, bytes16_h, bytes16_l) \
	do { \
		if (hip4_sampler_sample_fapi) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_SIGNAL_CTRLRX, 0, bytes16_h, bytes16_l, 0); \
		} \
	} while (0)

#define SCSC_HIP4_SAMPLER_TCP_DECODE(sdev, dev, frame, from_ba) \
	do { \
		if (hip4_sampler_sample_tcp) { \
			hip4_sampler_tcp_decode(sdev, dev, frame, from_ba); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_THROUG(minor, rx_tx, bytes16_h, bytes16_l) \
	do { \
		if (hip4_sampler_sample_through) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_THROUG, rx_tx, bytes16_h, bytes16_l, 0);        \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_THROUG_K(minor, rx_tx, bytes16_h, bytes16_l) \
	do { \
		if (hip4_sampler_sample_through) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_THROUG_K, rx_tx, bytes16_h, bytes16_l, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_THROUG_M(minor, rx_tx, bytes16_h, bytes16_l) \
	do { \
		if (hip4_sampler_sample_through) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_THROUG_M, rx_tx, bytes16_h, bytes16_l, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_STOP_Q(minor, vif_id) \
	do { \
		if (hip4_sampler_sample_start_stop_q) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_STOP_Q, 0, 0, vif_id, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_START_Q(minor, vif_id) \
	do { \
		if (hip4_sampler_sample_start_stop_q) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_START_Q, 0, 0, vif_id, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_MBULK(minor, bytes16_h, bytes16_l, clas) \
	do { \
		if (hip4_sampler_sample_mbulk) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_MBULK, clas, bytes16_h, bytes16_l, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_QFULL(minor, q) \
	do { \
		if (hip4_sampler_sample_qfull) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_QFULL, 0, 0, q, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_MFULL(minor) \
	do { \
		if (hip4_sampler_sample_mfull) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_MFULL, 0, 0, 0, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_INT(minor, id) \
	do { \
		if (hip4_sampler_sample_int) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_INT, 0, 0, id, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_INT_OUT(minor, id) \
	do { \
		if (hip4_sampler_sample_int) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_INT_OUT, 0, 0, id, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_INT_BH(minor, id) \
	do { \
		if (hip4_sampler_sample_int) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_INT_BH, 0, 0, id, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_INT_OUT_BH(minor, id) \
	do { \
		if (hip4_sampler_sample_int) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_INT_OUT_BH, 0, 0, id, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_RESET(minor) \
	hip4_sampler_update_record(minor, HIP4_SAMPLER_RESET, 0, 0, 0, 0)

#define SCSC_HIP4_SAMPLER_VIF_PEER(minor, tx, vif, peer_index) \
	do { \
		if (hip4_sampler_vif) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_PEER, tx, vif, peer_index, 0); \
		} \
	} while (0)

#define SCSC_HIP4_SAMPLER_BOT_RX(minor, scod, smod, vif_peer) \
	do { \
		if (hip4_sampler_bot) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_BOT_RX, scod, smod, vif_peer, 0); \
		} \
	} while (0)

#define SCSC_HIP4_SAMPLER_BOT_TX(minor, scod, smod, vif_peer) \
	do { \
		if (hip4_sampler_bot) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_BOT_TX, scod, smod, vif_peer, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_BOT_ADD(minor, addr_1, addr_2, vif_peer) \
	do { \
		if (hip4_sampler_bot) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_BOT_ADD, addr_1, addr_2, vif_peer, 0); \
		} \
	} while (0)

#define SCSC_HIP4_SAMPLER_BOT_REMOVE(minor, addr_1, addr_2, vif_peer) \
	do { \
		if (hip4_sampler_bot) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_BOT_REMOVE, addr_1, addr_2, vif_peer, 0); \
		} \
	} while (0)

#define SCSC_HIP4_SAMPLER_BOT_START_Q(minor, vif_peer) \
	do { \
		if (hip4_sampler_bot) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_BOT_START_Q, 0, 0, vif_peer, 0); \
		} \
	} while (0)

#define SCSC_HIP4_SAMPLER_BOT_STOP_Q(minor, vif_peer) \
	do { \
		if (hip4_sampler_bot) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_BOT_STOP_Q, 0, 0, vif_peer, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_BOT_QMOD_RX(minor, qcod, qmod, vif_peer) \
	do { \
		if (hip4_sampler_bot) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_BOT_QMOD_RX, qcod, qmod, vif_peer, 0); \
		} \
	} while (0)

#define SCSC_HIP4_SAMPLER_BOT_QMOD_TX(minor, qcod, qmod, vif_peer) \
	do { \
		if (hip4_sampler_bot) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_BOT_QMOD_TX, qcod, qmod, vif_peer, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_BOT_QMOD_START(minor, vif_peer) \
	do { \
		if (hip4_sampler_sample_start_stop_q || hip4_sampler_bot) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_BOT_QMOD_START, 0, 0, vif_peer, 0); \
		} \
	} while (0)

#define SCSC_HIP4_SAMPLER_BOT_QMOD_STOP(minor, vif_peer) \
	do { \
		if (hip4_sampler_sample_start_stop_q || hip4_sampler_bot) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_BOT_QMOD_STOP, 0, 0, vif_peer, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_PKT_TX(minor, host_tag) \
	do { \
		if (hip4_sampler_pkt_tx) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_PKT_TX, 0, (host_tag >> 8) & 0xff, host_tag & 0xff, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_PKT_TX_HIP4(minor, host_tag) \
	do { \
		if (hip4_sampler_pkt_tx) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_PKT_TX_HIP4, 0, (host_tag >> 8) & 0xff, host_tag & 0xff, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_PKT_TX_FB(minor, host_tag) \
	do { \
		if (hip4_sampler_pkt_tx) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_PKT_TX_FB, 0, (host_tag >> 8) & 0xff, host_tag & 0xff, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_SUSPEND(minor) \
	do { \
		if (hip4_sampler_suspend_resume) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_SUSPEND, 0, 0, 0, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_RESUME(minor) \
	do { \
		if (hip4_sampler_suspend_resume) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_RESUME, 0, 0, 0, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_TCP_SYN(minor, id, mss) \
	do { \
		if (hip4_sampler_sample_tcp) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_TCP_SYN, id, 0, 0, mss); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_TCP_FIN(minor, id) \
	do { \
		if (hip4_sampler_sample_tcp) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_TCP_FIN, id, 0, 0, 0); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_TCP_DATA(minor, id, seq_num) \
	do { \
		if (hip4_sampler_sample_tcp) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_TCP_DATA, id, 0, 0, seq_num); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_TCP_ACK(minor, id, ack_num) \
	do { \
		if (hip4_sampler_sample_tcp) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_TCP_ACK, id, 0, 0, ack_num); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_TCP_DATA_IN(minor, id, seq_num) \
	do { \
		if (hip4_sampler_sample_tcp) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_TCP_DATA_IN, id, 0, 0, seq_num); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_TCP_ACK_IN(minor, id, ack_num) \
	do { \
		if (hip4_sampler_sample_tcp) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_TCP_ACK_IN, id, 0, 0, ack_num); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_TCP_RWND(minor, id, rwnd) \
	do { \
		if (hip4_sampler_sample_tcp) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_TCP_RWND, id, 0, 0, rwnd); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_TCP_CWND(minor, id, cwnd) \
	do { \
		if (hip4_sampler_sample_tcp) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_TCP_CWND, id, 0, 0, cwnd); \
		} \
	} while (0)
#define SCSC_HIP4_SAMPLER_TCP_SEND_BUF(minor, id, send_buff_size) \
	do { \
		if (hip4_sampler_sample_tcp) { \
			hip4_sampler_update_record(minor, HIP4_SAMPLER_TCP_SEND_BUF, id, 0, 0, send_buff_size); \
		} \
	} while (0)
#else
#define SCSC_HIP4_SAMPLER_Q(minor, q, idx_rw, value, rw)
#define SCSC_HIP4_SAMPLER_QREF(minor, ref, q)
#define SCSC_HIP4_SAMPLER_SIGNAL_CTRLTX(minor, bytes16_h, bytes16_l)
#define SCSC_HIP4_SAMPLER_SIGNAL_CTRLRX(minor, bytes16_h, bytes16_l)
#define SCSC_HIP4_SAMPLER_TPUT(minor, rx_tx, payload)
#define SCSC_HIP4_SAMPLER_THROUG(minor, bytes16_h, bytes16_l)
#define SCSC_HIP4_SAMPLER_THROUG_K(minor, bytes16_h, bytes16_l)
#define SCSC_HIP4_SAMPLER_THROUG_M(minor, bytes16_h, bytes16_l)
#define SCSC_HIP4_SAMPLER_MBULK(minor, bytes16_h, bytes16_l, clas)
#define SCSC_HIP4_SAMPLER_QFULL(minor, q)
#define SCSC_HIP4_SAMPLER_MFULL(minor)
#define SCSC_HIP4_SAMPLER_INT(minor, id)
#define SCSC_HIP4_SAMPLER_INT_BH(minor, id)
#define SCSC_HIP4_SAMPLER_INT_OUT(minor, id)
#define SCSC_HIP4_SAMPLER_INT_OUT_BH(minor, id)
#define SCSC_HIP4_SAMPLER_RESET(minor)
#define SCSC_HIP4_SAMPLER_VIF_PEER(minor, tx, vif, peer_index)
#define SCSC_HIP4_SAMPLER_BOT_RX(minor, scod, smod, vif_peer)
#define SCSC_HIP4_SAMPLER_BOT_TX(minor, scod, smod, vif_peer)
#define SCSC_HIP4_SAMPLER_BOT(minor, init, addr_1, addr_2, vif_peer)
#define SCSC_HIP4_SAMPLER_BOT_ADD(minor, addr_1, addr_2, vif_peer)
#define SCSC_HIP4_SAMPLER_BOT_REMOVE(minor, addr_1, addr_2, vif_peer)
#define SCSC_HIP4_SAMPLER_BOT_START_Q(minor, vif_peer)
#define SCSC_HIP4_SAMPLER_BOT_STOP_Q(minor, vif_peer)
#define SCSC_HIP4_SAMPLER_BOT_QMOD_RX(minor, qcod, qmod, vif_peer)
#define SCSC_HIP4_SAMPLER_BOT_QMOD_TX(minor, qcod, qmod, vif_peer)
#define SCSC_HIP4_SAMPLER_BOT_QMOD_START(minor, vif_peer)
#define SCSC_HIP4_SAMPLER_BOT_QMOD_STOP(minor, vif_peer)
#define SCSC_HIP4_SAMPLER_PKT_TX(minor, host_tag)
#define SCSC_HIP4_SAMPLER_PKT_TX_HIP4(minor, host_tag)
#define SCSC_HIP4_SAMPLER_PKT_TX_FB(minor, host_tag)
#define SCSC_HIP4_SAMPLER_SUSPEND(minor)
#define SCSC_HIP4_SAMPLER_RESUME(minor)
#define SCSC_HIP4_SAMPLER_TCP_SYN(minor, id, mss)
#define SCSC_HIP4_SAMPLER_TCP_FIN(minor, id)
#define SCSC_HIP4_SAMPLER_TCP_DATA(minor, id, seq_num)
#define SCSC_HIP4_SAMPLER_TCP_ACK(minor, id, ack_num)
#define SCSC_HIP4_SAMPLER_TCP_DATA_IN(minor, id, seq_num)
#define SCSC_HIP4_SAMPLER_TCP_ACK_IN(minor, id, ack_num)
#define SCSC_HIP4_SAMPLER_TCP_RWND(minor, id, rwnd)
#define SCSC_HIP4_SAMPLER_TCP_CWND(minor, id, cwnd)
#define SCSC_HIP4_SAMPLER_TCP_SEND_BUF(minor, id, send_buff_size)
#define SCSC_HIP4_SAMPLER_TCP_DECODE(sdev, dev, frame, from_ba)
#endif /* CONFIG_SCSC_WLAN_HIP4_PROFILING */

#endif /* __HIP4_SAMPLER_H__ */
