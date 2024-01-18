#ifndef _MBIM_QUEUE_H_
#define _MBIM_QUEUE_H_

#include <linux/kernel.h>
#include <linux/skbuff.h>

#define MAX_QUEUE_CNT		10000

struct mbim_devices_stats {
	unsigned long	rx_packets;
	unsigned long	tx_packets;
	unsigned long	rx_bytes;
	unsigned long	tx_bytes;
	unsigned long	rx_dropped;
	unsigned long	tx_dropped;
};

struct mbim_queue {

	wait_queue_head_t	dl_wait;
	atomic_t		open_excl;
	atomic_t			usb_enter_u3;
	int			queue_cnt;
	struct mbim_devices_stats	stats;
	/* make wakeup interrupt for pc */
	int			wakeup_gpio;
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
void mbim_enter_u3(void);
void mbim_exit_u3(void);
int mbim_read_u3(void);
void request_wakeup(void);
void register_mbim_gpio_info(int gpio);

extern int mbim_xmit(struct sk_buff *skb);
#endif
