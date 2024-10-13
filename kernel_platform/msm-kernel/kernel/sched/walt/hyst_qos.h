
#ifndef _QOS_H
#define _QOS_H

#include "walt.h"

#define MIN_DEFAULT_VALUE 0
#define MAX_DEFAULT_VALUE 0

#define HQTAG " [Hyst QoS] "

enum qos_ctrl_type {
	PM_QOS_MIN_LIMIT = 0,
	PM_QOS_MAX_LIMIT,
	TYPE_END,
};

struct user_req {
	char				*name;
	int					values[TYPE_END];
	int					res_type;
	u64					residency_time;
	struct list_head	list;
};

extern unsigned int busy_hyst_qos_value;

void hyst_add_request(struct user_req *req, int value, char *name);
void hyst_update_request(struct user_req *req, int type, unsigned int value);
void hyst_request_enable(struct user_req *req, bool enabled);
void hyst_remove_request(struct user_req *req);
void hyst_init(void);

#endif /* _QOS_H */
