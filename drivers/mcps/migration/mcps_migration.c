#include <linux/module.h>
#include <linux/slab.h>
#include <linux/netdevice.h>

#include "../utils/mcps_logger.h"
#include "../utils/mcps_cpu.h"
#include "../utils/mcps_utils.h"

#include "mcps_migration.h"

struct migration_manager __migration_manager;

static inline void lock_migration(struct migration_manager *manager) { spin_lock(&manager->lock); }
static inline void unlock_migration(struct migration_manager *manager) { spin_unlock(&manager->lock); }

__visible_for_testing long mcps_migration_min_interval __read_mostly = 1000000; // 1 ms
module_param(mcps_migration_min_interval, long, 0640);

bool mcps_is_qualified_interval(struct timespec64 *time)
{
	return (time->tv_sec > 0 || time->tv_nsec > mcps_migration_min_interval);
}

__visible_for_testing inline int __mcps_run_migration(struct migration_manager *manager)
{
	int count = 0;
	unsigned long flags;
	bool again = true;
	struct list_head queue;
	struct migration_request *req;

	INIT_LIST_HEAD(&queue);

	while (again) {
		while ((req = list_first_entry_or_null(&queue, struct migration_request, node))) {
			list_del(&req->node);

			count++;
			manager->request_handler(req->from, req->to, req->option);

			kfree(req);
		}

		local_irq_save(flags);
		lock_migration(manager);

		if (list_empty(&manager->request_queue)) {
			again = false;
		} else {
			list_splice_init(&manager->request_queue, &queue);
			manager->queue_size_locked = 0;
			again = true;
		}

		unlock_migration(manager);
		local_irq_restore(flags);
	}

	return count;
}

int mcps_run_migration(void)
{
	return __mcps_run_migration(&__migration_manager);
}

static inline struct migration_request *
mcps_dequeue_migration_request_locked(struct migration_manager *manager)
{
	struct migration_request *req = list_first_entry_or_null(&manager->request_queue, struct migration_request, node);
	
	if (req == NULL)
		return NULL;

	list_del(&req->node);
	manager->queue_size_locked--;

	return req;
}

#if defined(CONFIG_KUNIT)
__visible_for_testing struct migration_request *
__mcps_dequeue_migration_request(struct migration_manager *manager)
{
	struct migration_request *req;

	local_irq_disable();
	lock_migration(manager);

	req = mcps_dequeue_migration_request_locked(manager);

	unlock_migration(manager);
	local_irq_enable();

	return req;
}

__visible_for_testing struct migration_request *
mcps_dequeue_migration_request(void)
{
	return __mcps_dequeue_migration_request(&__migration_manager);
}
#endif

__visible_for_testing inline int 
__mcps_push_migration_request(struct migration_manager *manager, struct migration_request *req)
{
	local_irq_disable();
	lock_migration(manager);

	if (manager->queue_size_locked >= MAX_MIGRATION_REQUEST) {
		unlock_migration(manager);
		local_irq_enable();

		mcps_loge("too much migration request");
		return -EINVAL;
	}

	manager->queue_size_locked++;
	list_add_tail(&req->node, &manager->request_queue);

	unlock_migration(manager);
	local_irq_enable();

	return 0;
}

__visible_for_testing int mcps_push_migration_request(struct migration_request *req)
{
	return __mcps_push_migration_request(&__migration_manager, req);
}

__visible_for_testing struct migration_request *mcps_make_migration_request(unsigned int from, unsigned int to, unsigned int option)
{
	struct migration_request *req;

	if (option != MCPS_MIGRATION_FLOWID && !mcps_is_cpu_possible(from)) {
		mcps_loge("impossible cpu requested : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	if (!mcps_is_cpu_possible(to)) {
		mcps_loge("impossible cpu requested : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	if (!mcps_is_cpu_online(to)) {
		mcps_loge("offline cpu requested : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	if (!is_valid_migration_type(option)) {
		mcps_loge("invalid option requested : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	if (option != MCPS_MIGRATION_FLOWID && from == to) {
		mcps_loge("same cpu requested : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	req = kzalloc(sizeof(struct migration_request), GFP_KERNEL);

	if (!req) {
		mcps_loge("not enough memory : from [%u] to [%u] op [%u]", from, to, option);
		return NULL;
	}

	req->from = from;
	req->to = to;
	req->option = option;

	return req;
}

int mcps_request_migration(unsigned int from, unsigned int to, unsigned int option)
{
	struct migration_request *req;

	req = mcps_make_migration_request(from, to, option);

	if (!req)
		return -ENOMEM;

	if (mcps_push_migration_request(req)) {
		kfree(req);
		return -EINVAL;
	}

	return 0;
}

__visible_for_testing int mcps_migration_request_cb(const char *buf, const struct kernel_param *kp)
{
	int error = 0;
	char *temp, *copy, *sub;
	int from, to, option;

	temp = copy = kstrndup(buf, 14, GFP_KERNEL);

	if (!copy) {
		mcps_loge("not enough memory kstrndup %s", buf);
		goto fail;
	}

	mcps_logd("migration requested %s > %s", buf, copy);

	sub = strsep(&temp, " ");
	from = parseUInt(sub, &error);
	if (error)
		goto fail;

	sub = strsep(&temp, " ");
	to = parseUInt(sub, &error);
	if (error)
		goto fail;

	sub = strsep(&temp, " ");
	option = parseUInt(sub, &error);
	if (error)
		goto fail;

	if (mcps_request_migration(from, to, option))
		goto fail;

	kfree(copy);
	return strlen(buf);

fail:
	mcps_loge("fail to request migration : cmd - %s", buf);

	kfree(copy);
	return 0;
};

const struct kernel_param_ops mcps_migration_request_ops = {
	.set = &mcps_migration_request_cb,
};

int migration_request_node;
module_param_cb(mcps_migration,
			&mcps_migration_request_ops,
			&migration_request_node,
			0644);

__visible_for_testing void mcps_migration_no_handler(unsigned int from, unsigned int to, unsigned int option)
{
	mcps_loge("<BUG> No handler, migration request is discarded : from [%u] to [%u] op [%u]", from, to, option);
}

__visible_for_testing inline int __init_migration_manager(struct migration_manager *manager, void (*handler)(unsigned int, unsigned int, unsigned int))
{
	memset(manager, 0, sizeof(struct migration_manager));
	manager->lock = __SPIN_LOCK_UNLOCKED(lock);
	INIT_LIST_HEAD(&manager->request_queue);

	if (!handler) {
		mcps_bug();
		mcps_loge("migration handler is NULL. Test code ? Initializing with dummy handler");
		__init_migration_manager(&__migration_manager, mcps_migration_no_handler);

		return -EINVAL;
	}

	manager->request_handler = handler;

	return 0;
}

void init_migration_manager(void (*handler)(unsigned int, unsigned int, unsigned int))
{
	__init_migration_manager(&__migration_manager, handler);
}

__visible_for_testing inline void __release_migration_manager(struct migration_manager *manager)
{
	struct migration_request *req;

	local_irq_disable();
	lock_migration(manager);

	while ((req = mcps_dequeue_migration_request_locked(manager)))
		kfree(req);

	unlock_migration(manager);
	local_irq_enable();
}

void release_migration_manager(void)
{
	__release_migration_manager(&__migration_manager);
}
