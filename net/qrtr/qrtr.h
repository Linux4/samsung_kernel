/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __QRTR_H_
#define __QRTR_H_

#include <linux/types.h>

struct sk_buff;

/* endpoint node id auto assignment */
#define QRTR_EP_NID_AUTO (-1)
#define QRTR_EP_NET_ID_AUTO (1)

#define QRTR_DEL_PROC_MAGIC	0xe111

/**
 * struct qrtr_endpoint - endpoint handle
 * @xmit: Callback for outgoing packets
 *
 * The socket buffer passed to the xmit function becomes owned by the endpoint
 * driver.  As such, when the driver is done with the buffer, it should
 * call kfree_skb() on failure, or consume_skb() on success.
 */
struct qrtr_endpoint {
	int (*xmit)(struct qrtr_endpoint *ep, struct sk_buff *skb);
	/* private: not for endpoint use */
	struct qrtr_node *node;
};

int qrtr_endpoint_register(struct qrtr_endpoint *ep, unsigned int net_id,
			   bool rt);

void qrtr_endpoint_unregister(struct qrtr_endpoint *ep);

int qrtr_endpoint_post(struct qrtr_endpoint *ep, const void *data, size_t len);

int qrtr_peek_pkt_size(const void *data);

#ifndef __IPC_SUB_IOCTL
#define __IPC_SUB_IOCTL

#define IPC_SUB_IOCTL_MAGIC (0xC8)
#define IPC_SUB_IOCTL_SUBSYS_GET_RESTART \
	_IOR(IPC_SUB_IOCTL_MAGIC, 0, struct msm_ipc_subsys_request)

enum {
	SUBSYS_CR_REQ = 0,
	SUBSYS_RES_REQ,
};

struct msm_ipc_subsys_request {
	char name[32];
	int request_id;
};
#endif
#endif
