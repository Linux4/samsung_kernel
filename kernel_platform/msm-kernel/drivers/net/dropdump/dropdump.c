/*
 * Monitoring code for network dropped packet alerts
 *
 * Copyright (C) 2018 SAMSUNG Electronics, Co,LTD
 */

#include <net/ip.h>
#include <net/tcp.h>
#if defined(CONFIG_ANDROID_VENDOR_HOOKS)
#include <trace/hooks/net.h>
#endif
#include <trace/events/skb.h>
#include <net/dropdump.h>

int debug_drd = 0;
module_param(debug_drd, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(debug_drd, "dropdump debug flags");

unsigned int budget_limit = BUDGET_DEFAULT;
module_param(budget_limit, uint, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(budget_limit, "budget_limit");

#define drd_info(fmt, ...) pr_info("drd: %s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define drd_dbg(flag, ...) \
do { \
	if (unlikely(debug_drd & flag)) { drd_info(__VA_ARGS__); } \
	else {} \
} while (0)

DEFINE_RATELIMIT_STATE(drd_ratelimit_state, 1 * HZ, 10);
#define drd_limit(...)				\
do {						\
	if (__ratelimit(&drd_ratelimit_state))	\
		drd_info(__VA_ARGS__);		\
} while (0)

struct list_head ptype_log __read_mostly;
EXPORT_SYMBOL_GPL(ptype_log);

int support_dropdump;
EXPORT_SYMBOL_GPL(support_dropdump);

struct _drd_worker drd_worker;
__u16 dropped_count;

#define	PKTINFO_OFFSET(skb)	(*(u64 *)((u64)skb->data + skb->len - sizeof(u64)))

/* use direct call instead of recursive stack trace */
u64 __stack(int depth)
{
	u64 *func = NULL;
	switch (depth + ST_START) {
		case  3 :
			func = __builtin_return_address(3);
			break;
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
		case 25 :
			func = __builtin_return_address(25);
			break;
		default :
			return 0;
	}

	return (u64)func;
}


#define NOT_TRACE	(-1)
#define FIN_TRACE	1
#define ACK_TRACE	2
#define GET_TRACE	3

int chk_stack(char *pos, int net_pkt)
{
	/* stop tracing */
	if (!strncmp(pos, "unix", 4))	  // unix_xxx
		return NOT_TRACE;
	if (!strncmp(pos + 2, "tlin", 4)) // netlink_xxx
		return NOT_TRACE;
	if (!strncmp(pos, "tpac", 4))	  // tpacket_rcv
		return NOT_TRACE;
	if (!strncmp(pos, "drd", 3))	  // drd_xxx
		return NOT_TRACE;
	if (!strncmp(pos + 1, "_sk_d", 5))// __sk_destruct
		return NOT_TRACE;
#ifdef EXTENDED_DROPDUMP 
	/* ignore normally consumed packets on TX path */
	if (!strncmp(pos + 2, "it_on", 5))// xmit_one
		return NOT_TRACE;
	if (!strncmp(pos + 2, "t_tx_", 5))// net_tx_action
		return NOT_TRACE;
	if (!strncmp(pos, "dp_tx", 5))    //dp_tx_comp_xxx
		return NOT_TRACE;
	/* prevent recursive call by __kfree_skb() */
	if (!strncmp(pos + 4, "ree_s", 5))// __kfree_skb
		return NOT_TRACE;
#endif

	/* end of callstack */
	if (!strncmp(pos + 7, "_bh_", 4))// __local_bh_xxx
		return FIN_TRACE;
	if (!strncmp(pos + 7, "ftir", 4))// __do_softirq
		return FIN_TRACE;
	if (!strncmp(pos + 7, "rk_r", 4))// task_work_run
		return FIN_TRACE;
	if (!strncmp(pos, "SyS", 3))	 // SyS_xxx
		return FIN_TRACE;
	if (!strncmp(pos, "ret_", 4))	 // ret_from_xxx
		return FIN_TRACE;
	if (!strncmp(pos, "el0", 3))	 // el0_irq
		return FIN_TRACE;
	if (!strncmp(pos, "el1", 3))	 // el1_irq
		return FIN_TRACE;
	if (!strncmp(pos, "gic", 3))	 // gic_xxx
		return FIN_TRACE;
	if (!strncmp(pos + 4, "f_nbu", 5))// __qdf_nbuf_free
		return FIN_TRACE;

	/* network pkt */
	if (!net_pkt) {
		if (!strncmp(pos, "net", 3))
			return GET_TRACE;
		if (!strncmp(pos, "tcp", 3)) {
			// packet from tcp_drop() could be normal operation.
			// don't logging pure ack.
			if (!strncmp(pos, "tcp_drop", 8))
				return ACK_TRACE;
			return GET_TRACE;
		}
		if (!strncmp(pos, "ip",  2))
			return GET_TRACE;
		if (!strncmp(pos, "xfr", 3))
			return GET_TRACE;
	}

	return 0;
}


static bool _is_tcp_ack(struct sk_buff *skb)
{
	switch (skb->protocol) {
	/* TCPv4 ACKs */
	case htons(ETH_P_IP):
		if ((ip_hdr(skb)->protocol == IPPROTO_TCP) &&
			(ntohs(ip_hdr(skb)->tot_len) - (ip_hdr(skb)->ihl << 2) ==
			 tcp_hdr(skb)->doff << 2) &&
		    ((tcp_flag_word(tcp_hdr(skb)) &
		      cpu_to_be32(0x00FF0000)) == TCP_FLAG_ACK))
			return true;
		break;

	/* TCPv6 ACKs */
	case htons(ETH_P_IPV6):
		if ((ipv6_hdr(skb)->nexthdr == IPPROTO_TCP) &&
			(ntohs(ipv6_hdr(skb)->payload_len) ==
			 (tcp_hdr(skb)->doff) << 2) &&
		    ((tcp_flag_word(tcp_hdr(skb)) &
		      cpu_to_be32(0x00FF0000)) == TCP_FLAG_ACK))
			return true;
		break;
	}

	return false;
}

static inline bool is_tcp_ack(struct sk_buff *skb)
{
	if (skb_is_tcp_pure_ack(skb))
		return true;

	if (unlikely(_is_tcp_ack(skb)))
		return true;

	return false;
}

u8 get_stack(struct sk_buff *skb, struct sk_buff *dmy, unsigned int offset, unsigned int reason)
{
	u8 depth = 0, max_depth = ST_MAX;
	struct _dmy_info *dmy_info = (struct _dmy_info *)(dmy->data + offset);
	u64 *stack_base = &dmy_info->stack;

	// sometimes __builtin_return_address() returns invalid address for deep stack of
	// ksoftirq or kworker, and migration task. limit the maximun depth for them.
	if ((current->comm[0] == 'k' && (current->comm[4] == 't' || current->comm[4] == 'k')) ||
	    (current->comm[0] == 'm' &&  current->comm[4] == 'a')) {
		dmy_info->flag |= LIMIT_DEPTH_BIT;
		max_depth >>= 1;
	}

	if (skb->tstamp >> 48 < 5000) {
		// packet has kernel timestamp, not utc.
		// using a zero-value for updating to utc at tpacket_rcv()
		dmy_info->flag |= UPDATE_TIME_BIT;
		dmy->tstamp = 0;
	} else {
		// using utc of original packet
		dmy->tstamp = skb->tstamp;
	}

	drd_dbg(DEBUG_SAVE, "trace <%pK>\n", skb);
	for (depth = 0; depth < max_depth; depth++) {
		*stack_base = __stack(depth);
		drd_dbg(DEBUG_SAVE, "%02d: <%pK>\n", depth, (u64 *)*stack_base);
		if (*stack_base == 0) {
			depth--;
			break;
		}
		stack_base += (ST_SIZE / sizeof(u64));
	}

	snprintf(dmy_info->magic, 4, "DRD");
	dmy_info->depth = depth;
	dmy_info->count = __be16_to_cpu(++dropped_count);
	dmy_info->skb   = __be64_to_cpu((u64)skb);
	dmy_info->reason_id = reason;
#if 0 // disable temporary due to for abi check fail
	snprintf(dmy_info->reason_str,
		 min(16, (int)strlen(drop_reasons[reason])) + 1,
		 "%s", drop_reasons[reason]);
#endif
	drd_dbg(DEBUG_RAW, "<%pK:%pK> %*ph\n", dmy, dmy_info, 16, dmy_info);

	return depth;
}

int set_stack(struct sk_buff *skb, struct _dmy_info *dmy_info)
{
	int n = 0, chk = 0, net_pkt = 0;
	u8 depth;
	u64 *stack_base;

	drd_dbg(DEBUG_RAW, "<%pK:%pK> %*ph\n", skb, dmy_info, 16, dmy_info);

	if (strncmp(dmy_info->magic, "DRD", 3)) {
		drd_dbg(DEBUG_TRACE, "invalid magic <%pK>\n", skb);
		return -1;
	}

	stack_base = &dmy_info->stack;
	for (depth = 0; depth < dmy_info->depth; depth++) {
		n = snprintf((char *)stack_base, ST_SIZE, "%pS", (void *)*stack_base);

		chk = chk_stack((char *)stack_base, net_pkt);
		drd_dbg(DEBUG_RESTORE, "[%2d:%d] <%s>\n", depth, chk, (char *)stack_base);
		if (chk < 0) {
			drd_dbg(DEBUG_TRACE, "not target stack\n");
			return NOT_TRACE;
		}

		if (chk == FIN_TRACE)
			break;

		if (chk == ACK_TRACE) {
			if (is_tcp_ack(skb)) {
				drd_dbg(DEBUG_TRACE, "don't trace tcp ack\n");
				return NOT_TRACE;
			} else {
				net_pkt = 1;
			}
		}

		if (chk == GET_TRACE)
			net_pkt = 1;

		stack_base += (ST_SIZE / sizeof(u64));
	}

	if (net_pkt == 0) {
		drd_dbg(DEBUG_TRACE, "not defined packet\n");
		return -3;
	}

	return depth;
}

extern struct list_head ptype_all;
static void save_pkt(struct work_struct *ws)
{
	struct sk_buff *skb, *next;
	struct packet_type *ptype = NULL;
	struct _dmy_info *dmy_info = NULL;
	int st_depth = 0;
	u16 budget = 0;

	list_for_each_entry_safe(skb, next, &drd_worker.list, list) {
		spin_lock_bh(&drd_worker.lock);
		if (support_dropdump) {
			list_for_each_entry_rcu(ptype, &ptype_log, list) {
				if (ptype != NULL)
					break;
			}

			drd_dbg(DEBUG_LOG, "del %u:%llu <%llx>\n", budget, drd_worker.num, (u64)(skb));
			skb_list_del_init(skb);
			drd_worker.num--;
		} else {
			spin_unlock_bh(&drd_worker.lock);
			return;
		}
		spin_unlock_bh(&drd_worker.lock);

		if (ptype == NULL || list_empty(&ptype->list)) {
			drd_dbg(DEBUG_LOG,"pt list not ready\n");
			__kfree_skb(skb);
			continue;
		}

		dmy_info = (struct _dmy_info *)(skb->data + PKTINFO_OFFSET(skb));

		st_depth = set_stack(skb, dmy_info);
		if (st_depth > 0) {
			ptype->func(skb, skb->dev, ptype, skb->dev);
		} else {
			__kfree_skb(skb);
		}

		if (++budget >= budget_limit)
			break;
	}

	if (!list_empty(&drd_worker.list)) {
		if (budget_limit < BUDGET_MAX)
			budget_limit <<= 1;
		queue_delayed_work(drd_worker.wq, &drd_worker.dwork, msecs_to_jiffies(1));
		drd_dbg(DEBUG_LOG, "pkt remained(%llu), trigger again. budget:%d\n", drd_worker.num, budget_limit);
	} else {
		drd_worker.num = 0;
	}

	return;
}

int skb_validate(struct sk_buff *skb)
{
	if (virt_addr_valid(skb) && virt_addr_valid(skb->dev)) {
		struct iphdr *ip4hdr = (struct iphdr *)skb_network_header(skb);

		if (skb->protocol != htons(ETH_P_IPV6)
		    && skb->protocol != htons(ETH_P_IP))
			return -1;

		switch (skb->dev->name[0]) {
			case 'r': // rmnet*
			case 'v': // v4-rmnet*
			case 'l': // lo
			case 't': // tun
			case 'b': // bt*
#if 0
			case 'w': // wlan
			case 's': // swlan
#endif
				break;
			default:
				drd_dbg(DEBUG_LOG, "invalid dev: %s\n", skb->dev->name);
				return -2;
		}

		if (unlikely((ip4hdr->version != 4 && ip4hdr->version != 6)
				|| ip4hdr->id == 0x6b6b))
			return -3;

		if (unlikely(!skb->len))
			return -4;

		if (unlikely(skb->len > skb->tail))
			return -5;

		if (unlikely(skb->data <= skb->head))
			return -6;

		if (unlikely(skb->tail > skb->end))
			return -7;

		if (unlikely(skb->pkt_type == PACKET_LOOPBACK))
			return -8;

		return 0;
	}

	return -255;
}


struct sk_buff *get_dummy(struct sk_buff *skb, unsigned int reason)//, char *pos, int st_depth)
{
	struct sk_buff *dummy = NULL;
	unsigned int copy_len     = DUMMY_COPYLEN_MAX;
	unsigned int copy_buf_len = DUMMY_COPYLEN_MAX;
	unsigned int org_len, dummy_len;

	struct iphdr *ip4hdr = (struct iphdr *)(skb_network_header(skb));
	struct ipv6hdr *ip6hdr;

	if (ip4hdr->version == 4) {
		org_len = ntohs(ip4hdr->tot_len);
	} else {
		ip6hdr  = (struct ipv6hdr *)ip4hdr;
		org_len = skb_network_header_len(skb) + ntohs(ip6hdr->payload_len);
	}

	if (org_len < DUMMY_COPYLEN_MAX) {
		copy_len     = org_len;
		copy_buf_len = round_up(org_len, 0x10);
	}

	dummy_len = copy_buf_len + sizeof(struct _dmy_info) + ST_BUF_SIZE;

	dummy = alloc_skb(dummy_len, GFP_ATOMIC);
	if (unlikely(!dummy)) {
		drd_dbg(DEBUG_LOG, "alloc fail, %u\n", dummy_len);
		return NULL;
	}

	drd_dbg(DEBUG_SAVE, "skb->len:%u org_len:%u copy_len:%u copy_buf_len:%u dummy_len:%u\n",
		 skb->len, org_len, copy_len, copy_buf_len, dummy_len);

	dummy->dev = skb->dev;
	dummy->protocol = skb->protocol;
	dummy->ip_summed = CHECKSUM_UNNECESSARY;

	skb_put(dummy, dummy_len);
	skb_reset_mac_header(dummy);
	skb_reset_network_header(dummy);
	skb_set_transport_header(dummy, skb_network_header_len(skb));
	INIT_LIST_HEAD(&dummy->list);

	memcpy(dummy->data, skb_network_header(skb), copy_len);
	memset((void *)((u64)dummy->data + (u64)copy_len), 0,
		0x10 - (copy_len % 0x10) + sizeof(struct _dmy_info) + ST_BUF_SIZE);

	get_stack(skb, dummy, copy_buf_len, reason);
	PKTINFO_OFFSET(dummy) = copy_buf_len;

	return dummy;
}

void drd_kfree_skb(struct sk_buff *skb, unsigned int reason)
{
	struct sk_buff *dmy, *next;

	if (unlikely(!support_dropdump)) {
		if (drd_worker.num) {
			drd_dbg(DEBUG_LOG, "purge drd list\n");
			cancel_delayed_work(&drd_worker.dwork);
			spin_lock_bh(&drd_worker.lock);
			list_for_each_entry_safe(dmy, next, &drd_worker.list, list) {
				skb_list_del_init(dmy);
				__kfree_skb(dmy);
			}
			drd_worker.num = 0;
			spin_unlock_bh(&drd_worker.lock);
		}
		return;
	}

	if (unlikely(skb_validate(skb)))
		return;

	if (unlikely(drd_worker.num >= LIST_MAX - 1)) {
		drd_dbg(DEBUG_LOG, "drd list full\n");
		return;
	}

	dmy = get_dummy(skb, reason);
	if (dmy == NULL) {
		drd_dbg(DEBUG_LOG, "save pkt fail\n");
		return;
	}

	spin_lock_bh(&drd_worker.lock);
	if (support_dropdump) {
		list_add_tail(&dmy->list, &drd_worker.list);
		drd_worker.num++;
		drd_dbg(DEBUG_LOG, "add %llu <%pK>\n", drd_worker.num, dmy);
	}
	spin_unlock_bh(&drd_worker.lock);

	budget_limit = BUDGET_DEFAULT;
	queue_delayed_work(drd_worker.wq, &drd_worker.dwork, 0);
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
#else
/* can't use macro directing the drd_xxx functions instead of lapper. *
 * because of have to use EXPORT_SYMBOL macro for module parts.       *
 * it should to be used at here with it's definition                  */
void trace_android_vh_ptype_head(const struct packet_type *pt, struct list_head *vendor_pt)
{
	drd_ptype_head(pt, vendor_pt);
}
EXPORT_SYMBOL_GPL(trace_android_vh_ptype_head);
#endif

#if defined(TRACE_SKB_DROP_REASON) || defined(DEFINE_DROP_REASON)
static void drd_kfree_skb_handler(void *data, struct sk_buff *skb,
				  void *location, enum skb_drop_reason reason)
{
#else
static void drd_kfree_skb_handler(void *data, struct sk_buff *skb, void *location)
{
	unsigned int reason = 0;
#endif
	drd_kfree_skb(skb, (unsigned int)reason);
}

static struct ctl_table drd_proc_table[] = {
	{
		.procname	= "support_dropdump",
		.data		= &support_dropdump,
		.maxlen 	= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#ifdef EXTENDED_DROPDUMP
	{
		.procname	= "support_dropdump_ext",
		.data		= &support_dropdump,
		.maxlen 	= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
#endif
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
#endif
	rc += register_trace_kfree_skb(drd_kfree_skb_handler, NULL);
	if (rc) {
		drd_info("fail to register android_trace\n");
		return -EIO;
	}

	drd_worker.wq = create_workqueue("drd_work");
	if (!drd_worker.wq) {
		drd_info("fail to create wq\n");
		return -ENOMEM;
	}
	INIT_DELAYED_WORK(&drd_worker.dwork, save_pkt);
	INIT_LIST_HEAD(&drd_worker.list);
	spin_lock_init(&drd_worker.lock);
	drd_worker.num = 0;

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

