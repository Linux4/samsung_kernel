#ifndef __WLAN_MSG_Q_H__
#define __WLAN_MSG_Q_H__
#include <linux/spinlock.h>

typedef struct {
	unsigned int   wt;
	unsigned int   rd;
	unsigned char *mem;
	unsigned int   num;
	unsigned int   size;
	spinlock_t     wt_lock;
	unsigned int   wt_cnt;
	unsigned int   rd_cnt;
} msg_q_t;

extern int msg_q_in(msg_q_t *msg_q, void *msg);
extern unsigned char *msg_get(msg_q_t *msg_q);
extern int msg_num(msg_q_t *msg_q);
extern int msg_free(msg_q_t *msg_q, void *msg);
extern int msg_q_alloc(msg_q_t *msg_q, unsigned short size, unsigned short num);
extern int msg_q_free(msg_q_t *msg_q);

#endif

