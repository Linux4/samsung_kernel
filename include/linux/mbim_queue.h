#ifndef _MBIM_QUEUE_H_
#define _MBIM_QUEUE_H_

#include <linux/kernel.h>
#include <linux/skbuff.h>

struct mbim_queue {

	wait_queue_head_t	dl_wait;

	atomic_t		open_excl;
};

struct mbim_queue *mbim_queue_init(void);
int mbim_read_opened(void);
int mbim_queue_head(struct sk_buff *skb);
int mbim_queue_tail(struct sk_buff *skb);
struct sk_buff *mbim_dequeue(void);
struct sk_buff *mbim_dequeue_tail(void);
void mbim_queue_purge(void);
int mbim_queue_empty(void);
int mbim_send_skb_to_cpif(struct sk_buff *skb);

extern int mbim_xmit(struct sk_buff *skb);
#endif

