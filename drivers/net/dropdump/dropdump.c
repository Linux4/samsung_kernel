/*
 * Monitoring code for network dropped packet alerts
 *
 * Copyright (C) 2018 SAMSUNG Electronics, Co,LTD
 */

#include <net/ip.h>
#if defined(CONFIG_ANDROID_VENDOR_HOOKS)
#include <trace/hooks/net.h>
#endif

int debug_drd = 0;
module_param(debug_drd, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(debug_drd, "dropdump debug flags");

#define drd_info(fmt, ...) pr_info("drd: %s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define drd_dbg(...) \
do { \
	if (unlikely(debug_drd)) { drd_info(__VA_ARGS__); } \
	else {} \
} while (0)

DEFINE_RATELIMIT_STATE(drd_ratelimit_state, 1 * HZ, 10);
#define drd_limit(...)				\
do {						\
	if (__ratelimit(&drd_ratelimit_state))	\
		drd_info(__VA_ARGS__);		\
} while (0)

static inline int deliver_skb(struct sk_buff *skb,
			      struct packet_type *pt_prev,
			      struct net_device *orig_dev)
{
	if (unlikely(skb_orphan_frags_rx(skb, GFP_ATOMIC)))
		return -ENOMEM;
	refcount_inc(&skb->users);
	return pt_prev->func(skb, skb->dev, pt_prev, orig_dev);
}

/* add definition for logging */
#define ETH_P_LOG	0x00FA

struct list_head ptype_log __read_mostly;
EXPORT_SYMBOL_GPL(ptype_log);

int support_dropdump;
EXPORT_SYMBOL_GPL(support_dropdump);

#define ST_MAX	20
#define ST_SIZE 0x30
static char save_stack[NR_CPUS][ST_SIZE * ST_MAX];

char *__stack(int depth) {
	char *func = NULL;
	switch (depth + 4) {
		case  4 :
			func = __builtin_return_address(4);
			break;
		case  5 :
			func = __builtin_return_address(5);
			break;
		case  6 :
			func = __builtin_return_address(6);
			break;
		case  7 :
			func = __builtin_return_address(7);
			break;
		case  8 :
			func = __builtin_return_address(8);
			break;
		case  9 :
			func = __builtin_return_address(9);
			break;
		case 10 :
			func = __builtin_return_address(10);
			break;
		case 11 :
			func = __builtin_return_address(11);
			break;
		case 12 :
			func = __builtin_return_address(12);
			break;
		case 13 :
			func = __builtin_return_address(13);
			break;
		case 14 :
			func = __builtin_return_address(14);
			break;
		case 15 :
			func = __builtin_return_address(15);
			break;
		case 16 :
			func = __builtin_return_address(16);
			break;
		case 17 :
			func = __builtin_return_address(17);
			break;
		case 18 :
			func = __builtin_return_address(18);
			break;
		case 19 :
			func = __builtin_return_address(19);
			break;
 		case 20 :
			func = __builtin_return_address(20);
			break;
		case 21 :
			func = __builtin_return_address(21);
			break;
		case 22 :
			func = __builtin_return_address(22);
			break;
		case 23 :
			func = __builtin_return_address(23);
			break;
		case 24 :
			func = __builtin_return_address(24);
			break;
		default :
			return NULL;
	}

	return func;
}


int chk_stack(char *pos, int net_pkt)
{
	/* stop tracing */
	if (!strncmp(pos, "unix", 4))	 // unix_xxx
		return -1;
	if (!strncmp(pos + 2, "tlin", 4))// netlink_xxx
		return -1;
	if (!strncmp(pos, "tpac", 4))	 // tpacket_rcv
		return -1;
	if (!strncmp(pos, "drd", 3))	 // drd_xxx
		return -1;
	if (!strncmp(pos + 2, "sk_d", 4))// __sk_destruct
		return -1;

	/* end of callstack */
	if (!strncmp(pos + 7, "_bh_", 4))// __local_bh_xxx
		return 1;
	if (!strncmp(pos + 7, "ftir", 4))// __do_softirq
		return 1;
	if (!strncmp(pos + 7, "rk_r", 4))// task_work_run
		return 1;
	if (!strncmp(pos, "SyS", 3))	 // SyS_xxx
		return 1;
	if (!strncmp(pos, "ret_", 4))	 // ret_from_xxx
		return 1;
	if (!strncmp(pos, "el0", 3))	 // el0_irq
		return 1;
	if (!strncmp(pos, "el1", 3))	 // el1_irq
		return 1;
	if (!strncmp(pos, "gic", 3))	 // gic_xxx
		return 1;

	/* network pkt */
	if (!net_pkt) {
		if (!strncmp(pos, "net", 3))
			return 2;
		if (!strncmp(pos, "tcp", 3))
			return 2;
		if (!strncmp(pos, "ip",  2))
			return 2;
		if (!strncmp(pos, "xfr", 3))
			return 2;
	}

	return 0;
}

int pr_stack(struct sk_buff *skb, char *dst)
{
	int n = 0, depth = 0, chk = 0, net_pkt = 0;
	char *pos, *st;

	for (depth = 0; depth < ST_MAX; depth++) {
		st = __stack(depth);
		if (st) {
			pos = (char *)(dst + ST_SIZE * depth);
			n = snprintf(pos, ST_SIZE, "%pF", st);
			memset(pos + n, 0, ST_SIZE - n);

			chk = chk_stack(pos, net_pkt);
			drd_dbg("[%2d:%d] <%s>\n", depth, chk, pos);
			if (chk < 0)
				return -1;
			if (chk == 1) 
				break;
			if (chk == 2)
				net_pkt = 1;
		} else {
			/* end of callstack */
			depth--;
			break;
		}
	}

	if (net_pkt == 0) {
		drd_dbg("not defined packet\n");
		return -3;
	}

	return depth > 5 ? depth - 4 : depth; /* do not use root stacks */
}

struct sk_buff *get_dummy(struct sk_buff *skb, char *pos, int st_depth)
{
	struct sk_buff *dummy = NULL;
	unsigned int stack_len = ST_SIZE * st_depth;

	/* initialize dummy packet with ipv4 : iphdr + udp_hdr + align */
	unsigned int hdr_len, hdr_tot_len, hdr_align;
	unsigned int data_len, data_offset;
	struct udphdr *udph;

	if (skb->protocol == htons(ETH_P_IP)) {
		hdr_len   = (unsigned int)sizeof(struct iphdr);
		hdr_align = 4;
	} else {
		hdr_len   = (unsigned int)sizeof(struct ipv6hdr);
		hdr_align = 0;
	}
	hdr_tot_len = hdr_len + (unsigned int)sizeof(struct udphdr);
	data_len    = hdr_tot_len + hdr_align;
	data_offset = data_len;

	/* expand data_len */
	data_len += stack_len;

	dummy = alloc_skb(data_len, GFP_ATOMIC);
	if (likely(dummy)) {
		/* set skb info */
		memset(dummy->data, 0, data_offset);
		dummy->dev              = skb->dev;
		dummy->hdr_len          = hdr_tot_len;
		dummy->len	        = data_len;
		dummy->transport_header = hdr_len;
		dummy->protocol         = skb->protocol;

		/* set ip_hdr info */
		if (dummy->protocol == htons(ETH_P_IP)) {
			struct iphdr *iph = (struct iphdr *)dummy->data;
			memcpy(dummy->data, ip_hdr(skb), hdr_len);
			
			iph->protocol = 17; // UDP
			iph->tot_len  = htons(data_len);
		} else {
			struct ipv6hdr *ip6h = (struct ipv6hdr *)dummy->data;
			memcpy(dummy->data, ipv6_hdr(skb), hdr_len);

			ip6h->nexthdr     = 17; // UDP
			ip6h->payload_len = htons(data_len - hdr_len);
		}

		/* set udp_hdr info */
		udph = udp_hdr(dummy);
		udph->len = htons(data_len - hdr_len);

		/* save callstack */
		memcpy(dummy->data + data_offset, pos, stack_len);
	} else {
		drd_dbg("fail to alloc dummy\n");
	}

	return dummy;
}

static void drd_print_skb(struct sk_buff *skb, unsigned int len)
{
	struct iphdr *ip4hdr = (struct iphdr *)skb_network_header(skb);

	if (ip4hdr->version == 4) {
		drd_limit("<%pF><%pF> src:%pI4 | dst:%pI4 | %*ph\n",
			__builtin_return_address(3), __builtin_return_address(4),
			&ip4hdr->saddr, &ip4hdr->daddr, len < 48 ? len : 48, ip4hdr);
	} else {
		struct ipv6hdr *ip6hdr = (struct ipv6hdr *)ip4hdr;
		drd_limit("<%pF><%pF> src:%pI6c | dst:%pI6c | %*ph\n",
			__builtin_return_address(3), __builtin_return_address(4),
			&ip6hdr->saddr, &ip6hdr->daddr, len < 48 ? len : 48, ip4hdr);
	}
}

extern struct list_head ptype_all;
struct sk_buff *drd_queue_skb(struct sk_buff *skb, int mode)
{
	struct packet_type *ptype;
	struct packet_type *pt_prev = NULL;
	struct sk_buff *skb2 = NULL;
	struct list_head *ptype_list = &ptype_log;
	struct net_device *dev = NULL;
	struct net_device *old_dev;
	struct iphdr *ip4hdr = (struct iphdr *)(skb_network_header(skb));
	struct ipv6hdr *ip6hdr;
	bool logged = false;
	unsigned int len, clen;
	static ktime_t tstamp_bck;

	int st_depth = 0;
	char *pos = (char *)save_stack + (ST_SIZE * ST_MAX) * (unsigned long)get_cpu();put_cpu();

	st_depth = pr_stack(skb, pos);
	if (st_depth < 0) {
		drd_dbg("can't trace [%d]\n", st_depth);
		return NULL;
	}

	if (ip4hdr->version == 4) {
		len = ntohs(ip4hdr->tot_len);
	} else {
		ip6hdr = (struct ipv6hdr *)ip4hdr;
		len = skb_network_header_len(skb) + ntohs(ip6hdr->payload_len);
	}
	if (mode) {
		clen = min(0x80u, len);
		tstamp_bck = skb->tstamp;
	} else {
		clen = len;
	}

	rcu_read_lock_bh();

	old_dev = skb->dev;
	if (skb->dev)
		dev = skb->dev;
	else if (skb_dst(skb) && skb_dst(skb)->dev)
		dev = skb_dst(skb)->dev;
	else
		dev = init_net.loopback_dev;

	skb->dev = dev;
	list_for_each_entry_rcu(ptype, ptype_list, list) {
		if (pt_prev) {
			deliver_skb(skb2, pt_prev, dev);
			pt_prev = ptype;
			logged = true;
			continue;
		}

		skb2 = netdev_alloc_skb(dev, clen);
		if (unlikely(!skb2)) {
			drd_dbg("alloc fail, %u\n", clen);
			goto out_unlock;
		}

		skb2->protocol = skb->protocol;
		skb2->tstamp   = mode ? skb->tstamp : tstamp_bck;

		skb_put(skb2, clen);
		skb_reset_mac_header(skb2);
		skb_reset_network_header(skb2);
		skb_set_transport_header(skb2, skb_network_header_len(skb));

		memcpy(skb_network_header(skb2), skb_network_header(skb), clen);

		pt_prev = ptype;
	}

	if (pt_prev) {
		if (!skb_orphan_frags_rx(skb2, GFP_ATOMIC)) {
			pt_prev->func(skb2, skb->dev, pt_prev, skb->dev);
			logged = true;
		} else
			__kfree_skb(skb2);
	}

out_unlock:
	skb->dev = old_dev;
	rcu_read_unlock_bh();

	if (mode && logged) {
		drd_print_skb(skb, len);
		return get_dummy(skb, pos, st_depth);
	}

	return NULL;
}

int skb_validate(struct sk_buff *skb)
{
	int err;

	if (unlikely(!virt_addr_valid(skb))) {
		err = -1;
	} else {
		struct iphdr *ip4hdr = (struct iphdr *)skb_network_header(skb);
		if (unlikely((ip4hdr->version != 4 && ip4hdr->version != 6)
				|| ip4hdr->id == 0x6b6b))
			err = -2;
		else if (unlikely(!skb->len))
			err = -3;
		else if (unlikely(skb->len > skb->tail))
			err = -4;
		else if (unlikely(skb->data < skb->head))
			err = -5;
		else if (unlikely(skb->tail > skb->end))
			err = -6;
		else if (unlikely(skb->pkt_type == PACKET_LOOPBACK))
			err = -7;
		else
			return 0;
	}
	return err;
}

void drd_kfree_skb(struct sk_buff *skb)
{
	struct sk_buff *dmy;

	if (unlikely(!support_dropdump))
		return;

	if (unlikely(skb_validate(skb)))
		return;

	dmy = drd_queue_skb(skb, 1);
	if (unlikely(dmy)) {
		drd_queue_skb(dmy, 0);
		__kfree_skb(dmy);
	}
}
EXPORT_SYMBOL_GPL(drd_kfree_skb);

void drd_ptype_head(const struct packet_type *pt, struct list_head *vendor_pt)
{
	if (pt->type == htons(ETH_P_LOG))
		vendor_pt->next = &ptype_log;
}
EXPORT_SYMBOL_GPL(drd_ptype_head);

#if defined(CONFIG_ANDROID_VENDOR_HOOKS)
static void drd_ptype_head_handler(void *data, const struct packet_type *pt, struct list_head *vendor_pt)
{
	drd_ptype_head(pt, vendor_pt);
}

static void drd_kfree_skb_handler(void *data, struct sk_buff *skb)
{
	drd_kfree_skb(skb);
}
#else
/* can't use macro directing the drd_xxx functions instead of lapper. *
 * because of have to use EXPORT_SYMBOL macro for module parts.       *
 * it should to be used at here with it's definition                  */
void trace_android_vh_ptype_head(const struct packet_type *pt, struct list_head *vendor_pt)
{
	drd_ptype_head(pt, vendor_pt);
}
EXPORT_SYMBOL_GPL(trace_android_vh_ptype_head);

void trace_android_vh_kfree_skb(struct sk_buff *skb)
{
	drd_kfree_skb(skb);
}
EXPORT_SYMBOL_GPL(trace_android_vh_kfree_skb);
#endif

static struct ctl_table drd_proc_table[] = {
	{
		.procname	= "support_dropdump",
		.data		= &support_dropdump,
		.maxlen 	= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{ }
};

static int __init init_net_drop_dump(void)
{
	int rc = 0;

	drd_info("\n");

	INIT_LIST_HEAD(&ptype_log);

	init_net.core.sysctl_hdr = register_net_sysctl(&init_net, "net/core", drd_proc_table);
	if (init_net.core.sysctl_hdr == NULL) {
		drd_info("init sysctrl failed\n");
		return -ENODEV;
	}

#if defined(CONFIG_ANDROID_VENDOR_HOOKS)
	rc  = register_trace_android_vh_ptype_head(drd_ptype_head_handler, NULL);
	rc += register_trace_android_vh_kfree_skb(drd_kfree_skb_handler, NULL);
	if (rc) {
		drd_info("fail to register android_trace\n");
		return -EIO;
	}
#endif

	support_dropdump = 0;

	return rc;
}

static void exit_net_drop_dump(void)
{
	drd_info("\n");

	support_dropdump = 0;
}

module_init(init_net_drop_dump);
module_exit(exit_net_drop_dump);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung dropdump module");

