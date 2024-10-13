/*****************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "dev.h"          /* sdev access */
#include "src_sink.h"
#include "debug.h"
#include "fapi.h"
#include "mlme.h"

long slsi_src_sink_cdev_ioctl_cfg(struct slsi_dev *sdev, unsigned long arg)
{
	long                          r = 0;
	struct unifiio_src_sink_arg_t src_sink_arg;
	struct net_device             *dev;
	struct netdev_vif             *ndev_vif;
	struct sk_buff                *req = NULL;
	struct sk_buff                *ind = NULL;

	memset((void *)&src_sink_arg, 0, sizeof(struct unifiio_src_sink_arg_t));
	if (copy_from_user((void *)(&src_sink_arg), (void *)arg, sizeof(struct unifiio_src_sink_arg_t)))
		return -EFAULT;

	SLSI_DBG2(sdev, SLSI_SRC_SINK, "Source/Sink\n");
	SLSI_DBG2(sdev, SLSI_SRC_SINK, "====================================================\n");
	SLSI_DBG2(sdev, SLSI_SRC_SINK, "  #action    : [0x%04X]\n", src_sink_arg.common.action);
	SLSI_DBG2(sdev, SLSI_SRC_SINK, "  #direction : [0x%04X]\n", src_sink_arg.common.direction);
	SLSI_DBG2(sdev, SLSI_SRC_SINK, "  #vif       : [0x%04X]\n", src_sink_arg.common.vif);
	SLSI_DBG2(sdev, SLSI_SRC_SINK, "  #endpoint  : [0x%04X]\n", src_sink_arg.common.endpoint);
	SLSI_DBG2(sdev, SLSI_SRC_SINK, "====================================================\n");

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	dev = slsi_get_netdev_locked(sdev, src_sink_arg.common.vif);
	if (WARN_ON(!dev)) {
		SLSI_ERR(sdev, "netdev for input vif:%d is NULL\n", src_sink_arg.common.vif);
		r = -ENODEV;
		goto out_locked;
	}
	ndev_vif = netdev_priv(dev);

	switch (src_sink_arg.common.action) {
	case SRC_SINK_ACTION_SINK_START:
#ifdef CONFIG_SCSC_DEBUG_CODE_COMMENTED_OUT
		if ((src_sink_arg.u.config.interval == 0) && bot_control) {
			/* Reject a SINK_START_REQ if bot is enabled and interval=0 */
			SLSI_ERR(sdev, "SINK cancelled: balance of traffic should be disabled before running SINK with interval=0\n");
			r = -EIO;
			goto out_locked;
		}
#endif
		req = fapi_alloc(debug_pkt_sink_start_req, DEBUG_PKT_SINK_START_REQ, src_sink_arg.common.vif, 0);
		if (WARN_ON(!req)) {
			r = -ENODEV;
			goto out_locked;
		}
		fapi_set_u16(req, u.debug_pkt_sink_start_req.end_point, src_sink_arg.common.endpoint);
		fapi_set_u16(req, u.debug_pkt_sink_start_req.direction, src_sink_arg.common.direction);
		fapi_set_u32(req, u.debug_pkt_sink_start_req.interval, src_sink_arg.u.config.interval);
		fapi_set_u16(req, u.debug_pkt_sink_start_req.packets_per_interval, src_sink_arg.u.config.pkts_per_int);
		SLSI_DBG1(sdev, SLSI_SRC_SINK, "debug_pkt_sink_start_req->\n");
		r = slsi_mlme_req(sdev, dev, req);
		break;
	case SRC_SINK_ACTION_SINK_STOP:
		req = fapi_alloc(debug_pkt_sink_stop_req, DEBUG_PKT_SINK_STOP_REQ, src_sink_arg.common.vif, 0);
		if (WARN_ON(!req)) {
			r = -ENODEV;
			goto out_locked;
		}
		fapi_set_u16(req, u.debug_pkt_sink_stop_req.direction, src_sink_arg.common.direction);
		fapi_set_u16(req, u.debug_pkt_sink_stop_req.end_point, src_sink_arg.common.endpoint);
		SLSI_DBG1(sdev, SLSI_SRC_SINK, "debug_pkt_sink_stop_req->\n");
		r = slsi_mlme_req(sdev, dev, req);
		break;
	case SRC_SINK_ACTION_GEN_START:
		req = fapi_alloc(debug_pkt_gen_start_req, DEBUG_PKT_GEN_START_REQ, src_sink_arg.common.vif, 0);
		if (WARN_ON(!req)) {
			r = -ENODEV;
			goto out_locked;
		}
		fapi_set_u16(req, u.debug_pkt_gen_start_req.direction, src_sink_arg.common.direction);
		fapi_set_u16(req, u.debug_pkt_gen_start_req.end_point, src_sink_arg.common.endpoint);
		fapi_set_u32(req, u.debug_pkt_gen_start_req.interval, src_sink_arg.u.config.interval);
		fapi_set_u16(req, u.debug_pkt_gen_start_req.packets_per_interval, src_sink_arg.u.config.pkts_per_int);
		fapi_set_u16(req, u.debug_pkt_gen_start_req.size, src_sink_arg.u.config.u.gen.size);
		fapi_set_u16(req, u.debug_pkt_gen_start_req.use_streaming, src_sink_arg.u.config.u.gen.use_streaming);
		fapi_set_u32(req, u.debug_pkt_gen_start_req.ipv4destination_address, src_sink_arg.u.config.u.gen.ipv4_dest);
		fapi_set_u16(req, u.debug_pkt_gen_start_req.packets_per_interrupt, src_sink_arg.u.config.u.gen.pkts_per_intr);

		SLSI_DBG3(sdev, SLSI_SRC_SINK,
			  "int:%u, pkts_per_int:%u, vif:%u, size:%u, use_streaming:%u, ipv4_dest:0x%04X, pkts_per_intr:%u\n",
			  src_sink_arg.u.config.interval,
			  src_sink_arg.u.config.pkts_per_int,
			  src_sink_arg.common.vif,
			  src_sink_arg.u.config.u.gen.size,
			  src_sink_arg.u.config.u.gen.use_streaming,
			  src_sink_arg.u.config.u.gen.ipv4_dest,
			  src_sink_arg.u.config.u.gen.pkts_per_intr);
		SLSI_DBG1(sdev, SLSI_SRC_SINK, "debug_pkt_gen_start_req->\n");
		r = slsi_mlme_req(sdev, dev, req);
		break;
	case SRC_SINK_ACTION_GEN_STOP:
		req = fapi_alloc(debug_pkt_gen_stop_req, DEBUG_PKT_GEN_STOP_REQ, src_sink_arg.common.vif, 0);
		if (WARN_ON(!req)) {
			r = -ENODEV;
			goto out_locked;
		}
		fapi_set_u16(req, u.debug_pkt_gen_stop_req.direction, src_sink_arg.common.direction);
		fapi_set_u16(req, u.debug_pkt_gen_stop_req.end_point, src_sink_arg.common.endpoint);
		SLSI_DBG1(sdev, SLSI_SRC_SINK, "debug_pkt_gen_stop_req->\n");
		r = slsi_mlme_req(sdev, dev, req);
		break;
	case SRC_SINK_ACTION_SINK_REPORT:
		req = fapi_alloc(debug_pkt_sink_report_req, DEBUG_PKT_SINK_REPORT_REQ, src_sink_arg.common.vif, 0);
		if (WARN_ON(!req)) {
			r = -ENODEV;
			goto out_locked;
		}
		fapi_set_u16(req, u.debug_pkt_sink_report_req.end_point, src_sink_arg.common.endpoint);
		fapi_set_u16(req, u.debug_pkt_sink_report_req.direction, src_sink_arg.common.direction);
		fapi_set_u32(req, u.debug_pkt_sink_report_req.report_interval, src_sink_arg.u.report.interval);
		SLSI_DBG1(sdev, SLSI_SRC_SINK, "debug_pkt_sink_report_req->\n");

		ind = slsi_mlme_req_ind(sdev, dev, req, DEBUG_PKT_SINK_REPORT_IND);
		if (!ind) {
			SLSI_ERR(sdev, "slsi_mlme_req_ind [SINK_REPORT] failed\n");
			r = -EIO;
			break;
		}

		src_sink_arg.u.report.duration            = fapi_get_u32(ind, u.debug_pkt_sink_report_ind.duration);
		src_sink_arg.u.report.count               = fapi_get_u32(ind, u.debug_pkt_sink_report_ind.received_packets);
		src_sink_arg.u.report.octet               = fapi_get_u32(ind, u.debug_pkt_sink_report_ind.recieved_octets);
		src_sink_arg.u.report.kbps                = fapi_get_u32(ind, u.debug_pkt_sink_report_ind.kbps);
		src_sink_arg.u.report.idle_ratio          = fapi_get_u16(ind, u.debug_pkt_sink_report_ind.idle_ratio);
		src_sink_arg.u.report.interrupt_latency   = fapi_get_u16(ind, u.debug_pkt_sink_report_ind.int_latency);
		src_sink_arg.u.report.free_kbytes         = fapi_get_u16(ind, u.debug_pkt_sink_report_ind.free_kbytes);
		src_sink_arg.u.report.timestamp           = jiffies_to_msecs(jiffies);

		memset(&ndev_vif->src_sink_params.sink_report, 0, sizeof(struct unifiio_src_sink_report));

		/* copy the report to userspace */
		if (copy_to_user((void *)arg, (void *)(&src_sink_arg), sizeof(struct unifiio_src_sink_arg_t)))
			r = -EFAULT;
		slsi_kfree_skb(ind);
		break;
	case SRC_SINK_ACTION_GEN_REPORT:
		req = fapi_alloc(debug_pkt_gen_report_req, DEBUG_PKT_GEN_REPORT_REQ, src_sink_arg.common.vif, 0);
		if (WARN_ON(!req)) {
			r = -ENODEV;
			goto out_locked;
		}
		fapi_set_u16(req, u.debug_pkt_gen_report_req.end_point, src_sink_arg.common.endpoint);
		fapi_set_u16(req, u.debug_pkt_gen_report_req.direction, src_sink_arg.common.direction);
		fapi_set_u32(req, u.debug_pkt_gen_report_req.report_interval, src_sink_arg.u.report.interval);
		SLSI_DBG1(sdev, SLSI_SRC_SINK, "debug_pkt_gen_report_req->\n");

		ind = slsi_mlme_req_ind(sdev, dev, req, DEBUG_PKT_GEN_REPORT_IND);
		if (!ind) {
			SLSI_ERR(sdev, "slsi_mlme_req_ind [GEN_REPORT] failed\n");
			r = -EIO;
			break;
		}

		src_sink_arg.u.report.duration     = fapi_get_u32(ind, u.debug_pkt_gen_report_ind.duration);
		src_sink_arg.u.report.count        = fapi_get_u32(ind, u.debug_pkt_gen_report_ind.received_packets);
		src_sink_arg.u.report.failed_count = fapi_get_u32(ind, u.debug_pkt_gen_report_ind.failed_count);
		src_sink_arg.u.report.octet        = fapi_get_u32(ind, u.debug_pkt_gen_report_ind.recieved_octets);
		src_sink_arg.u.report.kbps         = fapi_get_u32(ind, u.debug_pkt_gen_report_ind.kbps);
		src_sink_arg.u.report.idle_ratio        = fapi_get_u16(ind, u.debug_pkt_gen_report_ind.idle_ratio);
		src_sink_arg.u.report.interrupt_latency = fapi_get_u16(ind, u.debug_pkt_gen_report_ind.int_latency);
		src_sink_arg.u.report.free_kbytes       = fapi_get_u16(ind, u.debug_pkt_gen_report_ind.free_kbytes);

		src_sink_arg.u.report.timestamp           = jiffies_to_msecs(jiffies);

		memset(&ndev_vif->src_sink_params.gen_report, 0, sizeof(struct unifiio_src_sink_report));

		/* copy the report to userspace */
		if (copy_to_user((void *)arg, (void *)(&src_sink_arg), sizeof(struct unifiio_src_sink_arg_t)))
			r = -EFAULT;
		slsi_kfree_skb(ind);
		break;
	case SRC_SINK_ACTION_SINK_REPORT_CACHED:
		SLSI_DBG1(sdev, SLSI_SRC_SINK, "cached sink_report\n");
		memcpy(&src_sink_arg.u.report, &ndev_vif->src_sink_params.sink_report, sizeof(struct unifiio_src_sink_report));
		memset(&ndev_vif->src_sink_params.sink_report, 0, sizeof(struct unifiio_src_sink_report));

		if (copy_to_user((void *)arg, (void *)(&src_sink_arg), sizeof(struct unifiio_src_sink_arg_t)))
			r = -EFAULT;

		break;
	case SRC_SINK_ACTION_GEN_REPORT_CACHED:
		SLSI_DBG1(sdev, SLSI_SRC_SINK, "cached gen_report\n");
		memcpy(&src_sink_arg.u.report, &ndev_vif->src_sink_params.gen_report, sizeof(struct unifiio_src_sink_report));
		memset(&ndev_vif->src_sink_params.gen_report, 0, sizeof(struct unifiio_src_sink_report));

		if (copy_to_user((void *)arg, (void *)(&src_sink_arg), sizeof(struct unifiio_src_sink_arg_t)))
			r = -EFAULT;
		break;
	default:
		SLSI_ERR(sdev, "Invalid input for action : 0x%04X\n", src_sink_arg.common.action);
		r = -EINVAL;
		break;
	}

out_locked:
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);
	if (r)
		SLSI_ERR(sdev, "slsi_src_sink_cdev_ioctl_cfg(vif:%d) failed with %ld\n", src_sink_arg.common.vif, r);
	return r;
}

void slsi_rx_sink_report(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif              *ndev_vif = netdev_priv(dev);
	struct unifiio_src_sink_report *report = &ndev_vif->src_sink_params.sink_report;

	SLSI_DBG3(sdev, SLSI_SRC_SINK, "RX debug_pkt_sink_report_ind\n");

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	memset(report, 0, sizeof(struct unifiio_src_sink_report));
	report->duration            = fapi_get_u32(skb, u.debug_pkt_sink_report_ind.duration);
	report->count               = fapi_get_u32(skb, u.debug_pkt_sink_report_ind.received_packets);
	report->octet               = fapi_get_u32(skb, u.debug_pkt_sink_report_ind.recieved_octets);
	report->kbps                = fapi_get_u32(skb, u.debug_pkt_sink_report_ind.kbps);
	report->idle_ratio          = fapi_get_u16(skb, u.debug_pkt_sink_report_ind.idle_ratio);
	report->interrupt_latency   = fapi_get_u16(skb, u.debug_pkt_sink_report_ind.int_latency);
	report->free_kbytes         = fapi_get_u16(skb, u.debug_pkt_sink_report_ind.free_kbytes);
	report->timestamp           = jiffies_to_msecs(jiffies);
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);
	slsi_kfree_skb(skb);
}

void slsi_rx_gen_report(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif              *ndev_vif = netdev_priv(dev);
	struct unifiio_src_sink_report *report = &ndev_vif->src_sink_params.gen_report;

	SLSI_DBG3(sdev, SLSI_SRC_SINK, "RX debug_pkt_gen_report_ind\n");

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	memset(report, 0, sizeof(struct unifiio_src_sink_report));
	report->duration          = fapi_get_u32(skb, u.debug_pkt_gen_report_ind.duration);
	report->count             = fapi_get_u32(skb, u.debug_pkt_gen_report_ind.received_packets);
	report->failed_count      = fapi_get_u32(skb, u.debug_pkt_gen_report_ind.failed_count);
	report->octet             = fapi_get_u32(skb, u.debug_pkt_gen_report_ind.recieved_octets);
	report->kbps              = fapi_get_u32(skb, u.debug_pkt_gen_report_ind.kbps);
	report->idle_ratio        = fapi_get_u16(skb, u.debug_pkt_gen_report_ind.idle_ratio);
	report->interrupt_latency = fapi_get_u16(skb, u.debug_pkt_gen_report_ind.int_latency);
	report->free_kbytes       = fapi_get_u16(skb, u.debug_pkt_gen_report_ind.free_kbytes);
	report->timestamp         = jiffies_to_msecs(jiffies);
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);
	slsi_kfree_skb(skb);
}
