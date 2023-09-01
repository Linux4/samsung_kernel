#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <net/tcp.h>
#include <net/if_inet6.h>
#include <net/addrconf.h>

struct dev_addr_info {
	struct list_head list;
	struct net_device *dev;
	int family;
	union {
		__be32 addr4;
		struct in6_addr addr6;
	} addr;
	unsigned int prefixlen;
};

static struct list_head dev_info;
static struct list_head cleanup_dev_info;
static void dev_monitor_deferred_cleanup(struct work_struct *work);
static DECLARE_DELAYED_WORK(cleanup_work, dev_monitor_deferred_cleanup);

static struct dev_addr_info *get_dev_info(struct list_head *head,
					  struct net_device *dev, int family)
{
	struct dev_addr_info *info;

	list_for_each_entry(info, head, list) {
		if (!strcmp(info->dev->name, dev->name) &&
		    info->family == family)
			return info;
	}

	return NULL;
}

static void del_dev_info(struct list_head *head, struct net_device *dev)
{
	struct dev_addr_info *info, *temp;

	list_for_each_entry_safe(info, temp, head, list) {
		if (!strcmp(info->dev->name, dev->name)) {
			list_del(&info->list);
			kfree(info);
		}
	}
}

static void dev_monitor_print_tcp_sk(void)
{
	// print all remained sk info from tcp ehash
	struct sock *sk;
	const struct hlist_nulls_node *node;
	struct inet_ehash_bucket *head;
	unsigned int slot;

	for (slot = 0; slot <= tcp_hashinfo.ehash_mask; slot++) {
		head = &tcp_hashinfo.ehash[slot];
		sk_nulls_for_each_rcu(sk, node, &head->chain) {
			if (sk->sk_family == AF_INET)
				pr_err("sk state = %d, %pI4 %pI4\n",
				       sk->sk_state,
				       &sk->sk_rcv_saddr, &sk->sk_daddr);
			else
				pr_err("sk state = %d, %pI6 %pI6\n",
				       sk->sk_state,
				       &sk->sk_v6_rcv_saddr, &sk->sk_v6_daddr);
		}
	}
}

static void dev_monitor_ipv4_cleanup(struct dev_addr_info *info)
{
	struct sock *sk;
	const struct hlist_nulls_node *node;
	struct inet_ehash_bucket *head;
	unsigned int slot;
	__be32 addr4, mask;
	unsigned int prefixlen;
	bool match;

	// at this point dev may not a valid pointer

	addr4 = info->addr.addr4;
	prefixlen = info->prefixlen;
	pr_err("%s() : %pI4/%d\n", __func__, &addr4, prefixlen);

	for (slot = 0; slot <= tcp_hashinfo.ehash_mask; slot++) {
		head = &tcp_hashinfo.ehash[slot];
		sk_nulls_for_each_rcu(sk, node, &head->chain) {
			if (sk->sk_family != AF_INET)
				continue;

			mask = inet_make_mask(prefixlen);

			match = (addr4 == sk->sk_rcv_saddr) ||
				!((addr4 ^ sk->sk_rcv_saddr) & mask);
			if (!match)
				continue;

			pr_err("sk state = %d\n", sk->sk_state);
			pr_err("addr4:%pI4 sk_rcv_saddr:%pI4\n",
			       &addr4, &sk->sk_rcv_saddr);
			pr_err("verdict: %d %d\n",
			       addr4 == sk->sk_rcv_saddr,
			       !((addr4 ^ sk->sk_rcv_saddr) & mask));

			if (sk->sk_state == TCP_FIN_WAIT1 ||
			    sk->sk_state == TCP_ESTABLISHED ||
			    sk->sk_state == TCP_LAST_ACK) {
				sk->sk_prot->disconnect(sk, 0);
				pr_err("disconnected\n");
			}
		}
	}
}

static void dev_monitor_ipv6_cleanup(struct dev_addr_info *info)
{
	struct sock *sk;
	const struct hlist_nulls_node *node;
	struct inet_ehash_bucket *head;
	unsigned int slot;
	struct in6_addr addr6;
	unsigned int prefixlen;
	bool match;

	// at this point dev may not a valid pointer

	addr6 = info->addr.addr6;
	prefixlen = info->prefixlen;
	pr_err("%s() : %pI6/%d\n", __func__, &addr6, prefixlen);

	for (slot = 0; slot <= tcp_hashinfo.ehash_mask; slot++) {
		head = &tcp_hashinfo.ehash[slot];
		sk_nulls_for_each_rcu(sk, node, &head->chain) {
			if (sk->sk_family != AF_INET6)
				continue;

			match = ipv6_addr_equal(&addr6,&sk->sk_v6_rcv_saddr) ||
				ipv6_prefix_equal(&addr6, &sk->sk_v6_rcv_saddr,
						  prefixlen);
			if (!match)
				continue;

			pr_err("sk state = %d\n", sk->sk_state);
			pr_err("addr6:%pI6 sk_v6_rcv_saddr:%pI6\n",
			       &addr6, &sk->sk_v6_rcv_saddr);
			pr_err("verdict: %d %d\n",
			       ipv6_addr_equal(&addr6,&sk->sk_v6_rcv_saddr),
			       ipv6_prefix_equal(&addr6, &sk->sk_v6_rcv_saddr,
						 prefixlen));

			if (sk->sk_state == TCP_FIN_WAIT1 ||
			    sk->sk_state == TCP_ESTABLISHED ||
			    sk->sk_state == TCP_LAST_ACK) {
				sk->sk_prot->disconnect(sk, 0);
				pr_err("disconnected\n");
			}
		}
	}
}

void dev_monitor_cleanup_sock(struct net_device *dev)
{
	struct dev_addr_info *addr_info, *addr6_info;

	pr_err("%s(), %s\n", __func__, dev->name);

	addr_info = get_dev_info(&dev_info, dev, AF_INET);
	if (addr_info)
		dev_monitor_ipv4_cleanup(addr_info);

	addr6_info = get_dev_info(&dev_info, dev, AF_INET6);
	if (addr6_info)
		dev_monitor_ipv6_cleanup(addr6_info);

	del_dev_info(&dev_info, dev);
}

static void dev_monitor_deferred_cleanup(struct work_struct *work)
{
	struct dev_addr_info *info = NULL;

	pr_err("%s()\n", __func__);

	rtnl_lock();

	while (!list_empty(&cleanup_dev_info)) {
		info = list_first_entry(&cleanup_dev_info,
					struct dev_addr_info, list);

		if (info->family == AF_INET)
			dev_monitor_ipv4_cleanup(info);
		else if (info->family == AF_INET6)
			dev_monitor_ipv6_cleanup(info);

		list_del(&info->list);
		kfree(info);
	}

	rtnl_unlock();
}

static int dev_monitor_notifier_cb(struct notifier_block *nb,
				   unsigned long event, void *info)
{
	struct net_device *dev = netdev_notifier_info_to_dev(info);

	pr_err("dev : %s : event : %d\n", dev->name, event);
	switch (event) {
	//case NETDEV_DOWN:
	case NETDEV_UNREGISTER_FINAL:
		/* for given net device, address matched sock will be closed */
		if (netdev_refcnt_read(dev))
			dev_monitor_cleanup_sock(dev);

		/* check refcnt again and remove dev info from the list */
		if (!netdev_refcnt_read(dev)) {
			pr_err("dev : %s all refcnt has gone\n", dev->name);
			del_dev_info(&dev_info, dev);
		} else {
			dev_monitor_print_tcp_sk();
		}
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block dev_monitor_nb = {
	.notifier_call = dev_monitor_notifier_cb,
};

static int dev_monitor_inetaddr_event(struct notifier_block *this,
				      unsigned long event, void *ptr)
{
	const struct in_ifaddr *ifa = (struct in_ifaddr *)ptr;
	struct net_device *dev;
	struct dev_addr_info *addr_info, *n_addr_info;

	if (!ifa || !ifa->ifa_dev)
		return NOTIFY_DONE;


	switch (event) {
	case NETDEV_UP:
	case NETDEV_CHANGE:
	case NETDEV_CHANGEADDR:

		dev = ifa->ifa_dev->dev;

		addr_info = get_dev_info(&dev_info, dev, AF_INET);
		if (addr_info) {
			__be32 mask = inet_make_mask(addr_info->prefixlen);

			if (!((addr_info->addr.addr4 ^ ifa->ifa_local) & mask)) {
				pr_err("%s %s: %pI4/%d, updated:%pI4/%d\n", __func__, dev->name,
						&addr_info->addr.addr4, addr_info->prefixlen,
						&ifa->ifa_local, ifa->ifa_prefixlen);
				addr_info->prefixlen = ifa->ifa_prefixlen;

				return NOTIFY_DONE;
			} else {
				// move this addr info from dev_info to cleanup
				list_move_tail(&addr_info->list, &cleanup_dev_info);

				// trigger deferred clean up
				// tcp_disconnect calls might_sleep function
				// so it crashes when this routine calls
				// dev_monitor_cleanup_sock directly
				if (!delayed_work_pending(&cleanup_work))
					schedule_delayed_work(&cleanup_work, msecs_to_jiffies(1000));
			}
		}

		n_addr_info = kzalloc(sizeof(struct dev_addr_info), GFP_ATOMIC);
		if (!n_addr_info)
			break;
		n_addr_info->dev = dev;
		n_addr_info->family = AF_INET;
		n_addr_info->addr.addr4 = ifa->ifa_local;
		n_addr_info->prefixlen = ifa->ifa_prefixlen;

		INIT_LIST_HEAD(&n_addr_info->list);
		list_add(&n_addr_info->list, &dev_info);

		pr_err("%s %s: %pI4/%d, added:%pI4/%d\n", __func__, dev->name,
		       &ifa->ifa_local, ifa->ifa_prefixlen,
		       &n_addr_info->addr.addr4, n_addr_info->prefixlen);

		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block dev_monitor_inetaddr_nb = {
	.notifier_call = dev_monitor_inetaddr_event,
};

static int dev_monitor_inet6addr_event(struct notifier_block *this,
				      unsigned long event, void *ptr)
{
	const struct inet6_ifaddr *ifa = (struct inet6_ifaddr *)ptr;
	struct net_device *dev;
	struct dev_addr_info *addr_info, *n_addr_info;
	int addr_type;

	if (!ifa || !ifa->idev)
		return NOTIFY_DONE;

	addr_type = ipv6_addr_type(&ifa->addr);

	switch (event) {
	case NETDEV_UP:
	case NETDEV_CHANGE:
	case NETDEV_CHANGEADDR:
		if (ifa->scope > RT_SCOPE_LINK ||
		    addr_type == IPV6_ADDR_ANY ||
		    (addr_type & IPV6_ADDR_LOOPBACK) ||
		    (addr_type & IPV6_ADDR_LINKLOCAL))
			break;

		dev = ifa->idev->dev;

		addr_info = get_dev_info(&dev_info, dev, AF_INET6);
		if (addr_info) {
			if (ipv6_prefix_equal(&addr_info->addr.addr6, &ifa->addr, ifa->prefix_len)) {
				pr_err("%s %s: %pI6/%d, updated:%pI6/%d\n", __func__, dev->name,
						&addr_info->addr.addr6, addr_info->prefixlen,
						&ifa->addr, ifa->prefix_len);
				addr_info->prefixlen = ifa->prefix_len;

				return NOTIFY_DONE;
			} else {
				// move this addr info from dev_info to cleanup
				list_move_tail(&addr_info->list, &cleanup_dev_info);

				// trigger deferred clean up
				// tcp_disconnect calls might_sleep function
				// so it crashes when this routine calls
				// dev_monitor_cleanup_sock directly
				if (!delayed_work_pending(&cleanup_work))
					schedule_delayed_work(&cleanup_work, msecs_to_jiffies(1000));
			}
		}

		n_addr_info = kzalloc(sizeof(struct dev_addr_info), GFP_ATOMIC);
		if (!n_addr_info)
			break;

		n_addr_info->dev = dev;
		n_addr_info->family = AF_INET6;
		n_addr_info->addr.addr6 = ifa->addr;
		n_addr_info->prefixlen = ifa->prefix_len;

		INIT_LIST_HEAD(&n_addr_info->list);
		list_add(&n_addr_info->list, &dev_info);

		pr_err("%s %s: %pI6/%d, added:%pI6/%d\n", __func__, dev->name,
		       &ifa->addr, ifa->prefix_len,
		       &n_addr_info->addr.addr6, n_addr_info->prefixlen);

		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block dev_monitor_inet6addr_nb = {
	.notifier_call = dev_monitor_inet6addr_event,
};

static int __init dev_monitor_init(void)
{
	int ret;

	INIT_LIST_HEAD(&dev_info);
	INIT_LIST_HEAD(&cleanup_dev_info);
	ret = register_netdevice_notifier(&dev_monitor_nb);
	if (ret) {
		pr_err("%s: registering notifier error %d\n", __func__, ret);
		return ret;
	}

	ret = register_inetaddr_notifier(&dev_monitor_inetaddr_nb);
	if (ret) {
		pr_err("%s: registering inet notif error %d\n", __func__, ret);
		goto inetreg_err;
	}

	ret = register_inet6addr_notifier(&dev_monitor_inet6addr_nb);
	if (ret) {
		pr_err("%s: registering inet6 notif error %d\n", __func__, ret);
		goto inet6reg_err;
	}

	return 0;

inet6reg_err:
	unregister_inetaddr_notifier(&dev_monitor_inetaddr_nb);
inetreg_err:
	unregister_netdevice_notifier(&dev_monitor_nb);
	return ret;
}

static void __exit dev_monitor_exit(void)
{
	struct dev_addr_info *info, *temp;

	list_for_each_entry_safe(info, temp, &dev_info, list) {
		list_del(&info->list);
		kfree(info);
	}

	list_for_each_entry_safe(info, temp, &cleanup_dev_info, list) {
		list_del(&info->list);
		kfree(info);
	}

	unregister_inet6addr_notifier(&dev_monitor_inet6addr_nb);
	unregister_inetaddr_notifier(&dev_monitor_inetaddr_nb);
	unregister_netdevice_notifier(&dev_monitor_nb);
}

module_init(dev_monitor_init);
module_exit(dev_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter J. Park <gyujoon.park@samsung.com>");
