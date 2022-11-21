#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include "hyst_qos.h"

unsigned int busy_hyst_qos_value;
int initialized;
unsigned int cur_max_val = U32_MAX, cur_min_val;
spinlock_t hyst_qos_lock;
struct mutex qos_lock;
struct list_head hyst_req_list;

void hyst_add_request(struct user_req *req, int res_type, char *name)
{
	unsigned long flags;
	struct user_req *iter_req;

	if (!initialized)
		hyst_init();

	spin_lock_irqsave(&hyst_qos_lock, flags);
	list_for_each_entry_rcu(iter_req, &hyst_req_list, list) {
		if (list_empty(&hyst_req_list))
			break;

		if (!iter_req)
			continue;

		if (iter_req == req) {
			pr_err(HQTAG " Try to add duplicated Req\n");
			spin_unlock_irqrestore(&hyst_qos_lock, flags);
			return;
		}
	}
	list_add_tail_rcu(&(req->list), &hyst_req_list);
	spin_unlock_irqrestore(&hyst_qos_lock, flags);

	req->name = name;
	req->res_type = res_type;
}
EXPORT_SYMBOL_GPL(hyst_add_request);

void hyst_update_request(struct user_req *req, int type, unsigned int value)
{
	unsigned long flags;
	unsigned int qos_value = 0, max_of_min_reqval = 0, min_of_max_reqval = U32_MAX;
	struct user_req *iter_req;

	if (!initialized)
		hyst_init();

	mutex_lock(&qos_lock);

	req->values[type] = value;

	/* Update Min Limit */
	if (type == PM_QOS_MIN_LIMIT) {
		spin_lock_irqsave(&hyst_qos_lock, flags);
		list_for_each_entry_rcu(iter_req, &hyst_req_list, list) {
			if (list_empty(&hyst_req_list))
				break;

			if (!iter_req || req->res_type != iter_req->res_type)
				continue;

			if (iter_req->values[PM_QOS_MIN_LIMIT] > max_of_min_reqval)
				max_of_min_reqval = iter_req->values[PM_QOS_MIN_LIMIT];
		}
		qos_value = (cur_max_val < max_of_min_reqval) ? cur_max_val : max_of_min_reqval;

		cur_min_val = max_of_min_reqval;
		spin_unlock_irqrestore(&hyst_qos_lock, flags);

		pr_info(HQTAG" %s ::: Rqst Val(%u), Type(%d), Qos Value(%u)", req->name, value, req->res_type, qos_value);
	} else if (type == PM_QOS_MAX_LIMIT) {
		/* Update Max Limit */
		spin_lock_irqsave(&hyst_qos_lock, flags);
		list_for_each_entry_rcu(iter_req, &hyst_req_list, list) {
			if (list_empty(&hyst_req_list))
				break;

			if (!iter_req || req->res_type != iter_req->res_type)
				continue;

			if (iter_req->values[PM_QOS_MAX_LIMIT] < min_of_max_reqval)
				min_of_max_reqval = iter_req->values[PM_QOS_MAX_LIMIT];
		}
		qos_value = (cur_min_val > min_of_max_reqval) ? min_of_max_reqval : cur_min_val;

		cur_max_val = min_of_max_reqval;
		spin_unlock_irqrestore(&hyst_qos_lock, flags);
	}
	mutex_unlock(&qos_lock);

	busy_hyst_qos_value = qos_value;

	sched_update_hyst_times();

}
EXPORT_SYMBOL_GPL(hyst_update_request);

void hyst_remove_request(struct user_req *req)
{

	unsigned long flags;

	hyst_update_request(req, PM_QOS_MIN_LIMIT, MIN_DEFAULT_VALUE);
	hyst_update_request(req, PM_QOS_MAX_LIMIT, MAX_DEFAULT_VALUE);

	spin_lock_irqsave(&hyst_qos_lock, flags);
	list_del_rcu(&(req->list));
	spin_unlock_irqrestore(&hyst_qos_lock, flags);
}
EXPORT_SYMBOL_GPL(hyst_remove_request);

void hyst_init(void)
{
	INIT_LIST_HEAD(&hyst_req_list);
	spin_lock_init(&hyst_qos_lock);
	mutex_init(&qos_lock);
	initialized = true;
}
