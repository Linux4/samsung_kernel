/*
 * Copyright (C) 2011 Samsung Electronics.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/tcp.h>
#include <linux/hashtable.h>
#include <linux/linkforward.h>

#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_nat_l3proto.h>
#include <net/netfilter/nf_nat_l4proto.h>

#include "linkforward-offload.h"

struct nf_linkforward nf_linkfwd = {
	.use = false,
	.mode = 1,	/* RX only */
	.brdg.inner = NULL,
	.brdg.outter[0] = NULL, /* rmnet0 */
	.brdg.outter[1] = NULL,
	.brdg.outter[2] = NULL,
	.brdg.outter[3] = NULL,
	.brdg.outter[4] = NULL,
	.brdg.outter[5] = NULL,
	.brdg.outter[6] = NULL,
	.brdg.outter[7] = NULL, /* rmnet7 */
};

/* Test Devices & Addresses */
#define	TEST_PC_IP_ADDR		0xc0a82a75 /* 192.168.42.117 */
struct in_addr test_host_ip_addr = {
	.s_addr = htonl(TEST_PC_IP_ADDR)
};

/* Controlling link forward */
void set_linkforward_mode(int mode)
{
	nf_linkfwd.mode = mode;
}

void linkforward_enable(void)
{
	lf_info("%s\n", __func__);
	nf_linkfwd.use = 1;
}

void linkforward_disable(void)
{
	lf_info("%s\n", __func__);
	nf_linkfwd.use = 0;
}

void print_hash(void)
{
	int i;
	struct linkforward_connection *node;

	if (hash_empty(nf_linkfwd.h_mem_map)) {
		lf_err("no hash table\n");
		return;
	}

	hash_for_each(nf_linkfwd.h_mem_map, i, node, h_node) {
		lf_info("[%03d] key(%ld) name(%s) ip%pi4: port(%lu)\n",
				i,
				node->dst_port,
				node->netdev->name,
				&node->t_org->src.u3.ip,
				ntohs(node->t_org->src.u.all));
	}
}

struct linkforward_connection *find_conn(__be16 dst_port)
{
	struct linkforward_connection *node;

	hash_for_each_possible(nf_linkfwd.h_mem_map, node, h_node, dst_port) {
		if (node->dst_port == dst_port) {
#ifdef DEBUG_LINK_FORWARD
			lf_info("FOUND key(%ld) name(%s) ip%pi4: port(%lu)\n",
				node->dst_port,
				node->netdev->name,
				&node->t_org->src.u3.ip,
				ntohs(node->t_org->src.u.all));
#endif
			return node;
		}
	}

	return NULL;
}

int linkforward_add(__be16 dst_port, struct nf_conntrack_tuple *t_rpl,
		struct nf_conntrack_tuple *t_org, struct net_device *netdev)
{
	struct linkforward_connection *new_conn;

	if (ntohs(t_org->src.u.all) != ntohs(t_rpl->dst.u.all))
		return -1; /* No supporting port translation */

	new_conn = kzalloc(sizeof(struct linkforward_connection), GFP_ATOMIC);
	if (new_conn) {
		new_conn->enabled = true;
		new_conn->dst_port = dst_port;
		new_conn->netdev = netdev;
		new_conn->t_org = t_org;
		new_conn->t_rpl = t_rpl;
		hash_add(nf_linkfwd.h_mem_map, &new_conn->h_node, dst_port);
	} else {
		lf_err("ERROR: fail to get memory for new connection\n");
		return -1;
	}

#ifdef DEBUG_LINK_FORWARD
	lf_info("Add Connection: %s [%ld/%hu] %pI4:%hu -> %pI4:%hu (reply %pI4:%hu -> %pI4:%hu)\n",
		netdev->name,
		dst_port,
		ntohs(dst_port),
		&t_org->src.u3.ip,
		ntohs(t_org->src.u.all),
		&t_org->dst.u3.ip,
		ntohs(t_org->dst.u.all),
		&t_rpl->src.u3.ip,
		ntohs(t_rpl->src.u.all),
		&t_rpl->dst.u3.ip,
		ntohs(t_rpl->dst.u.all));

	print_hash();
#endif

	return 0;
}

int linkforward_delete(__be16 dst_port)
{
	struct linkforward_connection *conn;
	int ret;

	conn = find_conn(dst_port);
	if (conn) {
		hash_del(&conn->h_node);
		kfree(conn);
#ifdef DEBUG_LINK_FORWARD
		lf_info("Delete connection (port =%hu)\n", ntohs(dst_port));
		print_hash();
#endif
		ret = 0;
	} else {
		lf_err("%s - ERR cannot find dst_port in hash table\n",
				__func__);
		ret = -1;
	}

	return ret;
}

struct nf_conntrack_tuple *__linkforward_check_tuple(__be16 src_port,
		__be16 dst_port, enum linkforward_dir dir)
{
	__be16 port = dir ? dst_port : src_port;
	struct nf_conntrack_tuple *target;
	static __be16 last_port;
	bool print = true;
	struct linkforward_connection *conn;

	if (last_port != port) {
		last_port = port;
		print = true;
	}

#ifdef DEBUG_LINK_FORWARD
	if (print)
		lf_info("find Connection: [dir=%d] port:%hu\n",
			dir, dir ? ntohs(dst_port) : ntohs(src_port));
#endif

	conn = find_conn(dst_port);
	if (conn) {
		if (dir) {
			target = conn->t_org;
			conn->cnt_orign++;
		} else {
			target = conn->t_rpl;
			conn->cnt_reply++;
		}

#ifdef DEBUG_LINK_FORWARD
		if (print)
			lf_info("connection was found at %s\n\t[%d] %pI4:%hu -> %pI4:%hu\n",
					conn->netdev,
					ntohs(port),
					&target->src.u3.ip,
					ntohs(target->src.u.all),
					&target->dst.u3.ip,
					ntohs(target->dst.u.all));
#endif
		return target;

	} else {
#ifdef DEBUG_LINK_FORWARD
		if (print)
			lf_info("connection was NOT found\n");
#endif
		return NULL;
	}
}

ssize_t linkforward_get_state(char *buf)
{
	int i;
	ssize_t count = 0;
	struct linkforward_connection *node;

#ifdef USE_TETHEROFFLOAD
	lf_info("tetheroffload:%d\n", offload_enabled());
	count += sprintf(&buf[count], "tetheroffload:%d\n",
			offload_enabled());
#else
	lf_info("nf_linkfwd.use:%d\n", nf_linkfwd.use);
	count += sprintf(&buf[count], "nf_linkfwd.use:%d\n", nf_linkfwd.use);
#endif

	lf_info("forward_stat: rxBytes = %llu, txBytes = %llu\n",
			get_rx_offload_fwd_bytes(),
			get_tx_offload_fwd_bytes());
	count += sprintf(&buf[count], "forward_stat rxBytes:%llu txBytes:%llu\n",
			get_rx_offload_fwd_bytes(),
			get_tx_offload_fwd_bytes());

	if (hash_empty(nf_linkfwd.h_mem_map)) {
		count += sprintf(&buf[count], "No table\n");
		return count;
	}

	hash_for_each(nf_linkfwd.h_mem_map, i, node, h_node) {
		count += sprintf(&buf[count],
				"[%s] Port(%hu) PktCounts(rx = %u, tx = %u)\n",
				node->netdev->name, ntohs(node->dst_port),
				node->cnt_orign, node->cnt_reply);
	}

	return count;
}

/* manipulation */
int __linkforward_manip_skb(struct sk_buff *skb, enum linkforward_dir dir)
{
	unsigned char *packet = skb->data;
	struct iphdr *iphdr;
	struct nf_conntrack_tuple *target;
	static __be16 last_oldport;
	bool print = false;
	__be16 *portptr, newport, oldport;

	/* check the version of IP */
	iphdr = (struct iphdr *)packet;

	if (iphdr->version == 4) {
		__be32 addr, addr_new;
		struct ethhdr *ehdr;

		/*iphdr->frag_off & htons(IP_OFFSET) ? 0 : */
		switch (iphdr->protocol) {
		case IPPROTO_TCP:
		{
			struct tcphdr *tcph;

			tcph = (void *)(packet + iphdr->ihl * 4);

			target = __linkforward_check_tuple(tcph->source,
					tcph->dest, dir);
			if (target) {
				if (dir == LINK_FORWARD_DIR_REPLY) {
					/* destination IP/Port manipulation */
					addr = iphdr->daddr;
					iphdr->daddr = target->src.u3.ip;
					csum_replace4(&iphdr->check, addr,
							target->src.u3.ip);

					/* ToDo: port translation */
					newport = target->src.u.tcp.port;
					portptr = &tcph->dest;

					oldport = *portptr;
					*portptr = newport;

					if (last_oldport != oldport) {
						last_oldport = oldport;
						print = true;
					}

#ifdef DEBUG_LINK_FORWARD
					if (print)
						lf_info("REPLY manip: [dir=%d] newport %pI4:%hu <- oldport %pI4:%hu (Seq=0x%08x, Ack=0x%08x)\n",
							dir, &iphdr->daddr,
							ntohs(newport),
							&addr, ntohs(oldport),
							ntohl(tcph->seq),
							ntohl(tcph->ack_seq));
#endif

					csum_replace2(&tcph->check, oldport,
							newport);
					csum_replace4(&tcph->check, addr,
							target->src.u3.ip);

				} else {
					/* source IP/Port manipulation */
					addr = iphdr->saddr;
					iphdr->saddr = target->dst.u3.ip;
					csum_replace4(&iphdr->check, addr,
							target->dst.u3.ip);

					/* ToDo: port translation */
					newport = target->dst.u.tcp.port;
					portptr = &tcph->source;

					oldport = *portptr;
					*portptr = newport;

					if (last_oldport != oldport) {
						last_oldport = oldport;
						print = true;
					}
#ifdef DEBUG_LINK_FORWARD
					if (print)
						lf_info("ORIGIN manip: [dir=%d] oldport %pI4:%hu -> newport %pI4:%hu (Seq=0x%08x, Ack=0x%08x)\n",
							dir,  &addr,
							ntohs(oldport),
							&iphdr->saddr,
							ntohs(newport),
							ntohl(tcph->seq),
							ntohl(tcph->ack_seq));
#endif

					csum_replace2(&tcph->check, oldport,
							newport);
					csum_replace4(&tcph->check, addr,
							target->dst.u3.ip);

				}

				skb->hash = tcph->dest;
				skb->sw_hash = 1;

			} else {
				return 0;
			}

			break;
		}
#ifdef DEBUG_TEST_UDP
		case IPPROTO_UDP:
		{
			struct udphdr *udph;

			udph = (void *)(packet + iphdr->ihl * 4);

			if (udph->dest != htons(5001)) /* TEST-ONLY for iperf */
				return 0;

			addr_new = test_host_ip_addr.s_addr;

			addr = iphdr->daddr;
			iphdr->daddr = addr_new;
			csum_replace4(&iphdr->check, addr, addr_new);

			/* ToDo: port translation */
			/*csum_replace2(sum, old, new);*/

			if (udph->check != 0000)
				csum_replace4(&udph->check, addr, addr_new);

			skb->hash = udph->dest;
			skb->sw_hash = 1;

			break;
		}
#endif
		case IPPROTO_ICMP:
		{
			struct icmphdr *icmph;

			return 0; /* Not handling ICMP FORCE RETURN */

			icmph = (void *)(packet + iphdr->ihl * 4);
			csum_replace4(&icmph->checksum, addr, addr_new);

			break;
		}
		default:
			return 0; /* FORCE RETURN */
		}

		skb->ip_summed = CHECKSUM_UNNECESSARY;
		skb->priority = iphdr->tos;

		if (dir == LINK_FORWARD_DIR_REPLY) {
			/* mac header */
			skb_reset_network_header(skb);
			ehdr = (struct ethhdr *)skb_push(skb, sizeof(struct ethhdr));
			memcpy(ehdr->h_dest, nf_linkfwd.brdg.ldev_mac, ETH_ALEN);
			memcpy(ehdr->h_source, nf_linkfwd.brdg.if_mac, ETH_ALEN);
			ehdr->h_proto = skb->protocol;
			skb_reset_mac_header(skb);
			skb->protocol = htons(ETH_P_ALL);
		} else {
			skb_reset_network_header(skb);
			skb_reset_mac_header(skb);
		}

		*((u32 *)&skb->cb) = dir ? SIGNATURE_LINK_FORWRD_REPLY :
			SIGNATURE_LINK_FORWRD_ORIGN;

		return 1;
	}

	return 0;
}

/* Device core functions */
static inline int __dev_linkforward_queue_skb(struct sk_buff *skb,
		struct Qdisc *q, enum linkforward_dir dir)
{
	spinlock_t *root_lock = qdisc_lock(q);
	struct sk_buff *to_free = NULL;
	bool contended;
	int rc;

	qdisc_calculate_pkt_len(skb, q);
	contended = qdisc_is_running(q);
	if (unlikely(contended))
		spin_lock(&q->busylock);

	spin_lock(root_lock);
	if (unlikely(test_bit(__QDISC_STATE_DEACTIVATED, &q->state))) {
		__qdisc_drop(skb, &to_free);
		rc = NET_XMIT_DROP;
	} else {
		rc = q->enqueue(skb, q, &to_free) & NET_XMIT_MASK;
		if (unlikely(contended)) {
			spin_unlock(&q->busylock);
			contended = false;
		}

		atomic_set(&nf_linkfwd.qmask[dir], 1);
		nf_linkfwd.q[dir] = q;

		/* Schedule NET_TX_SOFTIRQ directly */
		__netif_schedule(nf_linkfwd.q[dir]);
	}
	spin_unlock(root_lock);
	if (unlikely(to_free))
		kfree_skb_list(to_free);
	if (unlikely(contended))
		spin_unlock(&q->busylock);
	return rc;
}

static int __dev_linkforward_queue_xmit(struct sk_buff *skb, void *accel_priv)
{
	struct netdev_queue *txq;
	struct Qdisc *q;
	int rc = -ENOMEM;
	enum linkforward_dir dir;

	/*
	 * if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_SCHED_TSTAMP))
	 *	__skb_tstamp_tx(skb, NULL, skb->sk, SCM_TSTAMP_SCHED);
	 */

	/* Disable soft irqs for various locks below. Also
	 * stops preemption for RCU.
	 */
	rcu_read_lock_bh();

	/* Change dev handle from rmnet to rndis during rmnet RPS */
	dir = ((*((u32 *)&skb->cb)) == SIGNATURE_LINK_FORWRD_REPLY) ?
		LINK_FORWARD_DIR_REPLY : LINK_FORWARD_DIR_ORIGN;
	if (dir == LINK_FORWARD_DIR_REPLY)
		skb->dev = get_linkforwd_inner_dev();
	else
		skb->dev = get_linkforwd_outter_dev(0);

	qdisc_skb_cb(skb)->pkt_len = skb->len;

	txq = netdev_pick_tx(skb->dev, skb, accel_priv);
	q = rcu_dereference_bh(txq->qdisc);

	if (q->enqueue)
		rc = __dev_linkforward_queue_skb(skb, q, dir);

	rcu_read_unlock_bh();

	return rc;
}

int dev_linkforward_queue_xmit(struct sk_buff *skb)
{
	int ret;

	/* ToDo: IP_UPD_PO_STATS(net, IPSTATS_MIB_OUT, skb->len); */
	ret = __dev_linkforward_queue_xmit(skb, NULL);
	switch (ret) {
	case NET_XMIT_SUCCESS:
		/* Supprt only rmnet to rndis RX path*/
		offload_update_rx_stat(skb->len);
		break;
	case NETDEV_TX_BUSY:
		kfree_skb(skb);
	case NET_XMIT_DROP:
	case NET_XMIT_CN:
	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL(dev_linkforward_queue_xmit);

/* Netfilter linkforward API */
int nf_linkforward_add(struct nf_conn *ct)
{
	struct nf_conntrack_tuple_hash *hash = &ct->tuplehash[IP_CT_DIR_REPLY];
	struct nf_conntrack_tuple *t = &hash->tuple;
	struct nf_conntrack_tuple_hash *org_h = &ct->tuplehash[IP_CT_DIR_ORIGINAL];
	int ret;

#ifdef DEBUG_LINK_FORWARD
	lf_info("add linkforward [%s] %pI4:%hu -> %pI4:%hu\n",
		ct->netdev->name,
		&t->src.u3.ip,		/* __be32 */
		ntohs(t->src.u.all),	/* __be16 */
		&t->dst.u3.ip,
		ntohs(t->dst.u.all));
#endif
	ret = linkforward_add(t->dst.u.all, t, &org_h->tuple, ct->netdev);
	if (ret == 0)
		ct->linkforward_registered = true;

	return ret;
}

int nf_linkforward_delete(struct nf_conn *ct)
{
	struct nf_conntrack_tuple_hash *hash = &ct->tuplehash[IP_CT_DIR_REPLY];
	struct nf_conntrack_tuple *t = &hash->tuple;

	if (!ct->linkforward_registered)
		return -1;

#ifdef DEBUG_LINK_FORWARD
	lf_info("delete linkforward: %pI4:%hu -> %pI4:%hu\n",
		&t->src.u3.ip,		/* __be32 */
		ntohs(t->src.u.all),	/* __be16 */
		&t->dst.u3.ip,
		ntohs(t->dst.u.all));
#endif

	linkforward_delete(t->dst.u.all);

	ct->linkforward_registered = false;

	return 0;
}

void nf_linkforward_monitor(struct nf_conn *ct)
{
	struct nf_conntrack_tuple_hash *hash = &ct->tuplehash[0];
	struct nf_conntrack_tuple *t = &hash->tuple;

	/* Check IPv4 */
	if (t->src.l3num != AF_INET)
		return;

	/* Check TCP Protocol */
	if (t->dst.protonum != SOL_TCP)
		return;

#ifdef USE_TETHEROFFLOAD
	if (offload_enabled() == false || nf_linkfwd.brdg.inner != ct->netdev)
		return;
#else
	if (!nf_linkfwd.use || nf_linkfwd.brdg.inner != ct->netdev)
		return;
#endif

#ifdef DEBUG_LINK_FORWARD
	lf_info("[%d] tuple %pI4:%hu -> %pI4:%hu\n",
		ct->packet_count,
		&t->src.u3.ip,		/* __be32 */
		ntohs(t->src.u.all),	/* __be16 */
		&t->dst.u3.ip,
		ntohs(t->dst.u.all));
#endif

	/* Increase packet count */
	ct->packet_count++;

	if (ct->linkforward_registered == false &&
			ct->packet_count >= THRESHOLD_LINK_FORWARD)
		nf_linkforward_add(ct);
}

/* Net Device Notifier */
static int netdev_linkforward_event(struct notifier_block *this,
					  unsigned long event, void *ptr)
{
	struct net_device *net = netdev_notifier_info_to_dev(ptr);
	enum linkforward_netdev netdev;
	long netdev_num;
	char *netdev_name;

	if (strncmp(net->name, NET_NAME_RMNET, strlen(NET_NAME_RMNET)) == 0) {
		netdev = RMNET;
		netdev_name = NET_NAME_RMNET;
	}
	else if (strncmp(net->name, NET_NAME_V4RMNET, strlen(NET_NAME_V4RMNET)) == 0) {
		netdev = V4RMNET;
		netdev_name = NET_NAME_V4RMNET;
	}
	else if (strncmp(net->name, NET_NAME_RNDIS, strlen(NET_NAME_RNDIS)) == 0) {
		netdev = RNDIS;
		netdev_name = NET_NAME_RNDIS;
	}
	else if (strncmp(net->name, NET_NAME_NCM, strlen(NET_NAME_NCM)) == 0) {
		netdev = NCM;
		netdev_name = NET_NAME_NCM;
	} else
		return NOTIFY_DONE;

	if (kstrtol(net->name + strlen(netdev_name), 10, &netdev_num) != 0) {
		lf_err("Fail to get device number for %s\n", netdev_name);
		netdev_num = -1;
	}

	switch (event) {
	case NETDEV_GOING_DOWN:
	case NETDEV_DOWN:
		lf_info("%s:%s\n", net->name, "NETDEV_DOWN");
		if (netdev == RNDIS || netdev == NCM) {
			linkforward_disable();
			set_linkforwd_inner_dev(NULL);
		} else if (netdev == RMNET)
			set_linkforwd_outter_dev(netdev_num, NULL);
		break;

	case NETDEV_UP:
		lf_info("%s:%s\n", net->name, "NETDEV_UP");
		if (netdev == RNDIS || netdev == NCM) {
			memcpy(nf_linkfwd.brdg.if_mac, net->dev_addr, ETH_ALEN);
			gether_get_host_addr_u8(net, nf_linkfwd.brdg.ldev_mac);
#ifdef DEBUG_LINK_FORWARD
			lf_info(" %s ldev MAC %pM\n", net->name,
					nf_linkfwd.brdg.ldev_mac);
			lf_info(" %s ifac MAC %pM\n", net->name,
					nf_linkfwd.brdg.if_mac);
#endif
			set_linkforwd_inner_dev(net);
			linkforward_enable();
		} else if (netdev == RMNET)
			set_linkforwd_outter_dev(netdev_num, net);
		break;

	case NETDEV_UNREGISTER:
		lf_info("%s:%s\n", net->name, "NETDEV_UNREGISTER");
		if (netdev == RNDIS || netdev == NCM) {
			linkforward_disable();
			set_linkforwd_inner_dev(NULL);
		} else if (netdev == RMNET)
			set_linkforwd_outter_dev(netdev_num, NULL);
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block netdev_linkforward_notifier = {
	.notifier_call	= netdev_linkforward_event,
};

void linkforward_init(void)
{
	int ret;

	atomic_set(&nf_linkfwd.qmask[LINK_FORWARD_DIR_ORIGN], 0);
	atomic_set(&nf_linkfwd.qmask[LINK_FORWARD_DIR_REPLY], 0);
	atomic_set(&nf_linkfwd.brdg.outter_dev, 0);

	nf_linkfwd.q[LINK_FORWARD_DIR_ORIGN] = NULL;
	nf_linkfwd.q[LINK_FORWARD_DIR_REPLY] = NULL;

	hash_init(nf_linkfwd.h_mem_map);

	ret = register_netdevice_notifier(&netdev_linkforward_notifier);
	if (ret)
		lf_info("linkforward_init - register_notifier() ret:%d\n", ret);

	set_linkforward_mode(1);
	linkforward_enable();

	ret = offload_initialize();
	lf_info("offload_initialize: %d\n", ret);
}
