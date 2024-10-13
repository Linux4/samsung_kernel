#ifndef __NET_DROPDUMP_H
#define __NET_DROPDUMP_H

#include <linux/netdevice.h>

/* vendor driver couldn't be used by builtin, with GKI.
   when using dropdump on GKI, check about that /trace/hoooks/net.h
   otherwise, by builtin driver, include /net/dropdump.h at /net/dst.h */

/* add definition for logging */
#define	ETH_P_LOG	0x00FA

#define	SKB_CLONE	0
#define	SKB_STACK	1
#define	SKB_DUMMY	2

#define	ST_MAX		20
#define	ST_SIZE		0x30
#define	ST_BUF_SIZE	(ST_SIZE * ST_MAX)
#define	ST_START	5

#define	DEBUG_LOG	0x01
#define	DEBUG_TRACE	0x02
#define	DEBUG_SAVE	0x10
#define	DEBUG_RESTORE	0x20
#define	DEBUG_RAW	0x40

#define	BUDGET_DEFAULT	64
#define	BUDGET_MAX	1024
#define	LIST_MAX	(BUDGET_MAX << 2)

#define	DUMMY_COPYLEN_MAX	0x100

#define	LIMIT_DEPTH_BIT		(1 << 0)
#define	UPDATE_TIME_BIT		(1 << 1)
#define	RESTORE_FAIL_BIT	(1 << 2)

struct _dmy_info {
	char magic[3];
	u8   depth;
	u8   flag;
	u8   reason_id;
	u16  count;
	u64  skb;
	char reason_str[16];
	u64  stack;
} __packed;

struct _drd_worker {
	struct workqueue_struct *wq;
	struct delayed_work dwork;
	struct list_head list;
	spinlock_t lock;
	u64 num;
};


#if IS_ENABLED(CONFIG_SUPPORT_DROPDUMP)
extern void trace_android_vh_ptype_head
            (const struct packet_type *pt, struct list_head *vendor_pt);
extern void trace_android_vh_kfree_skb(struct sk_buff *skb);
#else
#define trace_android_vh_ptype_head(pt, vendor_pt)
#define trace_android_vh_kfree_skb(skb)
#endif

#if !defined(DEFINE_DROP_REASON)
const char * const drop_reasons[] = {
	"NOT_SPECIFIED",
	"NO_SOCKET",
	"PKT_TOO_SMALL",
	"TCP_CSUM",
	"SOCKET_FILTER",
	"UDP_CSUM",
	"NETFILTER_DROP",
	"OTHERHOST",
	"IP_CSUM",
	"IP_INHDR",
	"IP_RPFILTER",
	"UNICAST_IN_L2_MULTICAST",
	"XFRM_POLICY",
	"IP_NOPROTO",
	"SOCKET_RCVBUFF",
	"PROTO_MEM",
	"TCP_MD5NOTFOUND",
	"TCP_MD5UNEXPECTED",
	"TCP_MD5FAILURE",
	"SOCKET_BACKLOG",
	"TCP_FLAGS",
	"TCP_ZEROWINDOW",
	"TCP_OLD_DATA",
	"TCP_OVERWINDOW",
	"TCP_OFOMERGE",
	"TCP_RFC7323_PAWS",
	"TCP_INVALID_SEQUENCE",
	"TCP_RESET",
	"TCP_INVALID_SYN",
	"TCP_CLOSE",
	"TCP_FASTOPEN",
	"TCP_OLD_ACK",
	"TCP_TOO_OLD_ACK",
	"TCP_ACK_UNSENT_DATA",
	"TCP_OFO_QUEUE_PRUNE",
	"TCP_OFO_DROP",
	"IP_OUTNOROUTES",
	"BPF_CGROUP_EGRESS",
	"IPV6DISABLED",
	"NEIGH_CREATEFAIL",
	"NEIGH_FAILED",
	"NEIGH_QUEUEFULL",
	"NEIGH_DEAD",
	"TC_EGRESS",
	"QDISC_DROP",
	"CPU_BACKLOG",
	"XDP",
	"TC_INGRESS",
	"UNHANDLED_PROTO",
	"SKB_CSUM",
	"SKB_GSO_SEG",
	"SKB_UCOPY_FAULT",
	"DEV_HDR",
	"DEV_READY",
	"FULL_RING",
	"NOMEM",
	"HDR_TRUNC",
	"TAP_FILTER",
	"TAP_TXFILTER",
	"ICMP_CSUM",
	"INVALID_PROTO",
	"IP_INADDRERRORS",
	"IP_INNOROUTES",
	"PKT_TOO_BIG",
};
#endif

#endif //__NET_DROPDUMP_H
