#ifndef _IMSBR_NETLINK_H
#define _IMSBR_NETLINK_H

#include <net/genetlink.h>

int imsbr_netlink_init(void);
void imsbr_netlink_exit(void);
int imsbr_spi_match(u32 spi);

struct esph {
	__u32 spi;
	__u32 seqno;
};

struct call_nl_function {
    int (*add_aptuple) (struct sk_buff *skb, struct genl_info *info);
    int (*do_call_state) (struct sk_buff *skb, struct genl_info *info);
	int (*do_lp_state) (struct sk_buff *skb, struct genl_info *info);
    int (*notify_remote_mac) (struct sk_buff *skb, struct genl_info *info);
};

void call_netlink_function(struct call_nl_function *cnf);

#endif
