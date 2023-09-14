#ifndef __WLAN_QOS_H__
#define __WLAN_QOS_H__
#include "wlan_msg_q.h"

typedef struct
{
	int      enable;
	msg_q_t *msg_q;
	int      index;
	int      going[4];
	int      count[4][2];
        int      wmm_report_flag;
}qos_t;

char qos_match_q(qos_t *qos, unsigned char *frame, unsigned int len);
void qos_sched(qos_t *qos, msg_q_t **q, int *num);
void qos_enable(qos_t *qos, int flag);
#endif
