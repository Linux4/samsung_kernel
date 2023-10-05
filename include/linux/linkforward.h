/* Copyright (C) 2015 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifdef CONFIG_LINK_FORWARD
#ifndef __LINKFORWARD_H__
#define __LINKFORWARD_H__

#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_nat_l3proto.h>
#include <net/netfilter/nf_nat_l4proto.h>

/* #define DEBUG_LINK_FORWARD */
/* #define DEBUG_LINK_FORWARD_LOW */
/* #define DEBUG_TEST_UDP */

/* LOG print */
#define LOG_TAG_LINKFORWARD	"linkforward: "

#define lf_err(fmt, ...) \
	pr_err(LOG_TAG_LINKFORWARD "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define lf_err_limited(fmt, ...) \
	printk_ratelimited(KERN_ERR LOG_TAG_LINKFORWARD "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define lf_info(fmt, ...) \
	pr_info(LOG_TAG_LINKFORWARD "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)

#ifdef DEBUG_LINK_FORWARD
#define lf_debug(fmt, ...) lf_info(fmt, ##__VA_ARGS__)
#ifdef DEBUG_LINK_FORWARD_LOW
#define lf_debug_low(fmt, ...)	lf_info(fmt, ##__VA_ARGS__)
#else
#define lf_debug_low(fmt, ...)
#endif
#else
#define lf_debug(fmt, ...)
#define lf_debug_low(fmt, ...)
#endif

/* If this configure is enabled, tether-offload controls linkforward on/off */
#define USE_TETHEROFFLOAD

#define SIGNATURE_LINK_FORWRD_REPLY	0xD01070B0
#define SIGNATURE_LINK_FORWRD_ORIGN	0xD01070A0
#define SIGNATURE_LINK_FORWRD_MASK	0xD0107000

#define CHECK_LINK_FORWARD(x)	(((x) & 0xFFFFFF00) == SIGNATURE_LINK_FORWRD_MASK)

#define THRESHOLD_LINK_FORWARD	30  /* Threshold for linkforward */

#define LINK_CLAT_DEV_MAX	2

#define MAX_RMNET_COUNT		8

enum linkforward_dir {
	LINK_FORWARD_DIR_ORIGN,
	LINK_FORWARD_DIR_REPLY,
	LINK_FORWARD_DIR_MAX
};

enum linkforward_netdev {
	RMNET,
	V4RMNET,
	RNDIS,
	NCM,
};

#define NET_NAME_RMNET		"rmnet"
#define NET_NAME_V4RMNET	"v4-rmnet"
#define NET_NAME_RNDIS		"rndis"
#define NET_NAME_NCM		"ncm"

struct forward_bridge {
	struct net_device	*inner;
	struct net_device	*outter[MAX_RMNET_COUNT];
	atomic_t outter_dev;
	char	if_mac[ETH_ALEN];	/* inner interface device mac address */
	char	ldev_mac[ETH_ALEN];	/* inner linked/connected ethernet devices mac */
};

struct tunnel_if {
	struct net_device *dev;
	union {
		struct in6_addr		addr6;
		struct in_addr		addr4;
	} in;
};

struct clat_tunnel {
	int enable;
	struct tunnel_if v6;
	struct tunnel_if v4;
};

/* Table structures */
struct linkforward_connection {
	bool enabled;
	__be16 dst_port;
	u32 cnt_reply;
	u32 cnt_orign;
	struct net_device *netdev;
	struct nf_conntrack_tuple t[IP_CT_DIR_MAX];

	struct hlist_node h_node;
	struct nf_conntrack_tuple *t_org;
	struct nf_conntrack_tuple *t_rpl;
};

struct nf_linkforward {
	int	use;
	int	mode;
	atomic_t	qmask[LINK_FORWARD_DIR_MAX];
	struct Qdisc	*q[LINK_FORWARD_DIR_MAX];
	struct forward_bridge	brdg;

	struct in6_addr		plat_prfix; /* the plat /96 prefix */
	struct clat_tunnel	ctun[LINK_CLAT_DEV_MAX];

	DECLARE_HASHTABLE(h_mem_map, 9); /* hash table size: 512 (2^9) */
};

extern struct nf_linkforward nf_linkfwd;
void set_linkforward_mode(int mode);
void linkforward_enable(void);
void linkforward_disable(void);
void clat_enable_dev(char *dev_name);
void clat_disable_dev(char *dev_name);
void linkforward_table_init(void);
ssize_t linkforward_get_state(char *buf);
bool offload_keeping_bw(void);
int linkforward_add(__be16 src_port, struct nf_conntrack_tuple *t_org,
		struct nf_conntrack_tuple *t_rpl, struct net_device *netdev);
int linkforward_delete(__be16 dst_port);
void linkforward_init(void);

/* Device core functions */
int dev_linkforward_queue_xmit(struct sk_buff *skb);

/* Netfilter linkforward API */
int nf_linkforward_add(struct nf_conn *ct);
int nf_linkforward_delete(struct nf_conn *ct);
void nf_linkforward_monitor(struct nf_conn *ct);

/* extern function call */
extern void gether_get_host_addr_u8(struct net_device *net, u8 host_mac[ETH_ALEN]);

/* check linkforward enable by tetheroffload hal */
bool offload_enabled(void);

static inline int get_linkforward_mode(void)
{
	if (nf_linkfwd.mode)
		offload_keeping_bw();
	return nf_linkfwd.mode;
}

static inline struct net_device *get_linkforwd_inner_dev(void)
{
	return nf_linkfwd.brdg.inner;
}

static inline struct net_device *get_linkforwd_outter_dev(int idx)
{
	return nf_linkfwd.brdg.outter[idx];
}

static inline void set_linkforwd_inner_dev(struct net_device *dev)
{
	nf_linkfwd.brdg.inner = dev;
}

static inline void set_linkforwd_outter_dev(int idx, struct net_device *dev)
{
	int outter_dev = atomic_read(&nf_linkfwd.brdg.outter_dev);

	if (dev)
		outter_dev |= (1 << idx);
	else
		outter_dev &= ~(1 << idx);
	atomic_set(&nf_linkfwd.brdg.outter_dev, outter_dev);

	nf_linkfwd.brdg.outter[idx] = dev;
}

int __linkforward_manip_skb(struct sk_buff *skb, enum linkforward_dir dir);

static inline int linkforward_manip_skb(struct sk_buff *skb, enum linkforward_dir dir)
{
#ifdef USE_TETHEROFFLOAD
	if (!(offload_enabled() && get_linkforwd_inner_dev() &&
				atomic_read(&nf_linkfwd.brdg.outter_dev)))
		return 0;
#else
	if (!(nf_linkfwd.use && get_linkforwd_inner_dev() &&
				atomic_read(&nf_linkfwd.brdg.outter_dev)))
		return 0;
#endif
	return __linkforward_manip_skb(skb, dir);
}

static inline int linkforward_get_tether_mode(void)
{
	if (!(nf_linkfwd.use && get_linkforwd_inner_dev() &&
				atomic_read(&nf_linkfwd.brdg.outter_dev))) {
		/* check is_tethering_enabled */
		return 0;
	}

	return 1;
}
#endif /*__LINKFORWARD_H__*/
#endif
