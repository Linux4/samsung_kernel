#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>

#include <media/map_hw.h>

int hw_manager_init(struct hw_manager *mngr)
{
	int ret = 0;
	return ret;
}
EXPORT_SYMBOL(hw_manager_init);

void hw_manager_clean(struct hw_manager *mngr)
{
	while (!list_empty(&mngr->resrc_pool)) {
		struct hw_resrc *res = list_first_entry(&mngr->resrc_pool,
							struct hw_resrc, hook);
		printk(KERN_ERR "MAP: hardware resource '%s' not released!\n",
			res->name);
		list_del(&res->hook);
		kfree(res);
	}
}
EXPORT_SYMBOL(hw_manager_clean);

struct hw_resrc *hw_res_register(struct device *dev,
	struct resource *res, struct hw_manager *mngr, const char *name,
	union agent_id mask, int res_id, void *handle, void *priv)
{
	struct hw_resrc *item;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (item == NULL)
		return NULL;

	item->name	= name ? name : res->name;
	item->dev	= dev;
	item->mask	= mask;
	item->id	= res_id;
	item->base	= res->start;
	item->size	= resource_size(res);
	item->handle	= handle;
	item->priv	= priv;
	switch (res->flags) {
	case IORESOURCE_MEM:
		item->type = MAP_RES_MEM;
		break;
	case IORESOURCE_IRQ:
		item->type = MAP_RES_IRQ;
		break;
	case IORESOURCE_DMA:
		item->type = MAP_RES_DMA;
		break;
	case IORESOURCE_BUS:
		item->type = MAP_RES_BUS;
		break;
	case IORESOURCE_IO:
		item->type = MAP_RES_IO;
		break;
	default:
		item->type = res->flags;
	}

	item->refcnt = 0;
	INIT_LIST_HEAD(&item->hook);
	list_add_tail(&item->hook, &mngr->resrc_pool);
	return item;
}

static inline int hw_mem_request(struct hw_resrc *mem)
{
	if (mem->refcnt == 0) {
		if (!devm_request_mem_region(mem->dev,
					mem->base, mem->size, mem->name))
			return -EBUSY;
		mem->handle = devm_ioremap_nocache(mem->dev,
					mem->base, mem->size);
		if (mem->handle == NULL) {
			devm_release_mem_region(mem->dev, mem->base, mem->size);
			return -ENOMEM;
		}
	}
	mem->refcnt++;
	return 0;
}

static inline void hw_mem_release(struct hw_resrc *mem)
{
	mem->refcnt--;
	if (mem->refcnt != 0)
		return;

	devm_iounmap(mem->dev, (void *)mem->handle);
	devm_release_mem_region(mem->dev, mem->base, mem->size);
}

static inline int hw_irq_request(struct hw_resrc *irq)
{
	int ret;
	/* irq handler is in irq->handle, irq context is in irq->priv */
	if (irq->refcnt == 0) {
		ret = devm_request_irq(irq->dev, irq->base,
			(irq_handler_t)irq->handle, IRQF_DISABLED|IRQF_SHARED,
			irq->name, irq->priv);
	}
	irq->refcnt++;
	return 0;
}

static inline void hw_irq_release(struct hw_resrc *irq)
{
	irq->refcnt--;
	if (irq->refcnt != 0)
		return;

	devm_free_irq(irq->dev, irq->base, irq->priv);
}

static inline int hw_clk_request(struct hw_resrc *clk)
{
	int ret;

	/* irq handler is in irq->handle, irq context is in irq->priv */
	if (clk->refcnt == 0) {
		clk->handle = devm_clk_get(clk->dev, clk->name);
		if (IS_ERR(clk->handle)) {
			dev_err(clk->dev, "get clock'%s' failed\n",
				(char const *)clk->base);
			ret = PTR_ERR(clk);
			return ret;
		}
	}
	clk->refcnt++;

	return 0;
}

static inline void hw_clk_release(struct hw_resrc *clk)
{
	clk->refcnt--;
	if (clk->refcnt != 0)
		return;

	devm_clk_put(clk->dev, clk->handle);
	clk->handle = NULL;
}
#if 0
/* When all agents are registered and removed all at once,
 * use hw_res_dispatch() to distrubute resource to each agent */
int __attribute__((unused)) hw_res_dispatch(struct hw_manager *mngr)
{
	/* for each resource, allocate and dispatch to agent */
	struct hw_resrc *res;
	int ret, i;

	list_for_each_entry(res, &mngr->resrc_pool, hook) {
		struct hw_dispatch_entry *item;
		/* get resource handle */
		switch (res->type) {
		case MAP_RES_MEM:
			ret = hw_mem_request(res);
			if (ret < 0)
				return ret;
			break;
		case MAP_RES_CLK:
			ret = hw_clk_request(res);
			if (ret < 0)
				return ret;
			break;
		case MAP_RES_IRQ:
			ret = hw_irq_request(res);
			if (ret < 0)
				return ret;
			break;
		default:
			;
		}
		/* printk("resource[%d:%d] repared\n", res->type, res->id); */
		/* dispatch resource to agents */
		if (res->type == MAP_RES_IRQ) {
			/* IRQ can't be dispatched, instead, it generates
			 * an event and dispatch it to listening agents*/
			continue;
		}
		item = res->dispatch_list;
		for (i = 0; i < res->dispatch_list_sz; i++) {
			/* find agents by name, resource of a perticular device
			 * will be dispatch to agents that locates in it only */
			item->id.dev_type = res->mask.dev_type;
			item->id.dev_id = res->mask.dev_id;
			item->id.mod_type &= res->mask.mod_type;
			item->id.mod_id &= res->mask.mod_id;
			item->agent = hw_agent_find(mngr, item->id);
			if (item->agent == NULL) {
				printk(KERN_ERR "map: agent_id %08X invalid\n",
					item->id.ident);
				return -EINVAL;
			}
			switch (res->type) {
			case MAP_RES_MEM:
				item->agent->reg_base = res->handle
							+ (int)item->priv;
				break;
			case MAP_RES_IRQ:
				item->agent->irq_num = res->base;
				break;
			case MAP_RES_CLK:
				item->agent->clock = res->handle;
			default:
				;
			}
			item++;
		}
	}
	/* Maybe dump resource dispatch list here */
	return 0;
}

void __attribute__((unused)) hw_res_withdraw(struct hw_manager *mngr)
{
	/* for each resource, allocate and dispatch to agent */
	struct hw_resrc *res;
	int i;

	list_for_each_entry(res, &mngr->resrc_pool, hook) {
		struct hw_dispatch_entry *item = res->dispatch_list;
		if (item == NULL)
			continue;
		for (i = 0; i < res->dispatch_list_sz; i++) {
			/* if resource not allocated to this agent yet */
			if (item->agent == NULL)
				continue;

			switch (res->type) {
			case MAP_RES_MEM:
				item->agent->reg_base = NULL;
				break;
			case MAP_RES_IRQ:
				item->agent->irq_num = 0;
				break;
			case MAP_RES_CLK:
				item->agent->clock = NULL;
			default:
				;
			}
			item->agent = NULL;
			item++;
		}
		/* get resource handle */
		switch (res->type) {
		case MAP_RES_MEM:
			hw_mem_release(res);
			break;
		case MAP_RES_CLK:
			hw_clk_release(res);
			break;
		case MAP_RES_IRQ:
			hw_irq_release(res);
			break;
		default:
			;
		}
	}
};
#endif
/* When agents are expected to insert/remove dynamically, use following
 * two functions to dispatch and withdraw resource from agents */
static int __attribute__((unused)) hw_res_request(struct hw_agent *agent)
{
	struct hw_manager *mngr = agent->mngr;
	struct hw_resrc *res;
	struct hw_event *event = NULL;
	int ret = 0, i;

	if (agent->req_list == NULL)
		return 0;
	agent->clock_cnt = 0;

	for (i = 0; ; i++) {
		struct hw_res_req *item = agent->req_list + i;

		/* Any invalid id assumed be end-of-list mark */
		if ((item->type < 0) || (item->type >= MAP_RES_END))
			return ret;
		list_for_each_entry(res, &mngr->resrc_pool, hook) {
			if ((agent->id.ident ==
				(res->mask.ident & agent->id.ident))
			&& (item->type == res->type) && (item->id == res->id))
				goto resrc_found;
		}
		return -ENXIO;
resrc_found:
		/* get resource handle and dispatch to agent */
		switch (res->type) {
		case MAP_RES_MEM:
			ret = hw_mem_request(res);
			if (ret < 0)
				return ret;
			agent->reg_base = res->handle + (int)item->priv;
			break;
		case MAP_RES_CLK:
			ret = hw_clk_request(res);
			if (ret < 0)
				return ret;
			agent->clock[agent->clock_cnt++] = res->handle;
			break;
		case MAP_RES_IRQ:
			event = res->priv;
			/* is this agent the owner of the irq event? */
			if ((event->owner_name)	&&
				!strcmp(event->owner_name, agent->name))
				event->owner = agent;
			ret = hw_irq_request(res);
			if (ret < 0)
				return ret;
			agent->irq_num = res->base;
			break;
		default:
			;
		}
	}
	return 0;
}

static int __attribute__((unused)) hw_res_release(struct hw_agent *agent)
{
	struct hw_manager *mngr = agent->mngr;
	struct hw_resrc *res;
	int i, ret = 0;

	for (i = 0; ; i++) {
		struct hw_res_req *item = agent->req_list + i;

		if ((item->type < 0) || (item->type >= MAP_RES_END))
			return ret;
		list_for_each_entry(res, &mngr->resrc_pool, hook) {
			if ((agent->id.ident ==
				(res->mask.ident & agent->id.ident))
			&& (item->type == res->type) && (item->id == res->id))
				goto res_match;
		}
		return -ENXIO;

res_match:
		switch (res->type) {
		case MAP_RES_MEM:
			hw_mem_release(res);
			agent->reg_base = 0;
			break;
		case MAP_RES_CLK:
			hw_clk_release(res);
			agent->clock[--agent->clock_cnt] = NULL;
			break;
		case MAP_RES_IRQ:
			hw_irq_release(res);
			agent->irq_num = 0;
			break;
		default:
			ret = -EINVAL;
		}
	}
return 0;
}

struct hw_resrc *hw_res_find(struct hw_manager *mngr, union agent_id ip_mask,
				int res_type, int res_id)
{
	struct hw_resrc *res;
	list_for_each_entry(res, &mngr->resrc_pool, hook) {
		if (res->type == res_type && res->id == res_id
			&& (res->mask.dev_type == ip_mask.dev_type)
			&& (res->mask.dev_id == ip_mask.dev_id))
			return res;
	}
	return NULL;
}

static int __attribute__((unused)) hw_agent_free(struct hw_agent *agent,
						struct hw_manager *mngr)
{
	return 0;
}

int hw_agent_register(struct hw_agent *agent, struct hw_manager *mngr)
{
	int ret = 0;
	int slot = (*mngr->get_slot)(agent->id);

	/* add to the manager's agent list */
	mngr->agent_addr[slot] = agent;
	agent->mngr = mngr;

	mutex_init(&agent->mux_lock);
	agent->pwrcnt = 0;
	if (agent->ops && agent->ops->init)
		ret = (*agent->ops->init)(agent);
	return ret;
}

void hw_agent_unregister(struct hw_agent *agent, struct hw_manager *mngr)
{
	int slot = (*mngr->get_slot)(agent->id);
	/* call agent clean */
	if (agent->ops && agent->ops->clean)
		(*agent->ops->clean)(agent);

	/* withdraw from agent pool*/
	agent->mngr = NULL;
	mngr->agent_addr[slot] = NULL;
}

struct hw_agent *hw_agent_find(struct hw_manager *mngr, union agent_id id)
{
	int slot;

	if (unlikely((mngr == NULL) || (mngr->get_slot == NULL)))
		return NULL;
	slot = (*mngr->get_slot)(id);
	if (unlikely(slot < 0))
		return NULL;
	return mngr->agent_addr[slot];
};

int hw_agent_init_default(struct hw_agent *agent)
{
	printk(KERN_INFO "MAP: hardware agent '%s' initialized\n", agent->name);
	return 0;
}

int hw_event_register(struct hw_event *event, struct hw_manager *mngr)
{
	/* find sender agent by ower_name */
	/* add to event list of owner agent */
	/* drop to manager event pool */
	return 0;
}

int hw_event_dispatch(struct hw_agent *sender, struct hw_event *event)
{
	/* get 1st agent, and use it to clear irq */
	/* for each attached agent, invoke handler with irq status */
	int i, ret = 0;
	struct hw_dispatch_entry *item;

	for (i = 0; i < MAX_AGENT_PER_EVENT; i++) {
		item = &event->dispatch_list[i];
		if (item->agent == NULL)
			/* some agent may stop listening to this event,
			 * so a NULL slot don't mean the end of list*/
			continue;
printk(KERN_INFO "agent %d = %p\n", i, item->agent);
		ret = (*item->handler)(item->agent, event);
		if (ret < 0) {
			printk(KERN_ERR "map: error when dispatch event '%s' "\
			"to '%s': %d\n", event->name, item->agent->name, ret);
			return ret;
		}
	}
	return 0;
}

int hw_tune_power(struct hw_agent *agent, int level)
{
	int i, ret = 0;

	/* TODO: actually, formal way is pass a QoS table, use profile id to
	 * find the QoS item, get power/clock/pin setting, and then apply to
	 * each domain. This is just a temporary solution here */
	mutex_lock(&agent->mux_lock);
	/* FIXME: assume level == 0 means power off, other means power on, but
	 * but none-zero value maybe used for suspend process, that way there
	 * maybe bug here! */
	if (level) {
		if (agent->pwrcnt >= 1) {
			/* Actually, the right thing to do here is merge power
			 * requests, then select the max value as new value */
			ret = 0;
			goto power_on_done;
		}
		ret = hw_res_request(agent);
		if (ret < 0)
			return ret;

		/* and then check if new value == current value here */

		/* Typically, for H/W IP Core, and sensors, power should be
		 * applied before clock is on, QoS is set the last */
		/* power_state = on | off | suspend | resume */
		if (agent->ops && agent->ops->set_power)
			ret = (*agent->ops->set_power)(agent, level);
		if (ret < 0)
			goto err_power;

		if (agent->ops && agent->ops->set_clock)
			ret = (*agent->ops->set_clock)(agent, level);
		if (ret < 0)
			goto err_clock;
		for (i = 0; i < agent->clock_cnt; i++)
			clk_enable(agent->clock[i]);

		if (agent->ops && agent->ops->set_qos)
			ret = (*agent->ops->set_qos)(agent, level);
		if (ret < 0)
			goto err_qos;
power_on_done:
		agent->pwrcnt++;
		goto exit;
	}

	if (agent->pwrcnt != 1) {
		ret = 0;
		goto power_off_done;
	}

	/* for power off sequence, just ignore error and close everything
	 * if possible*/
	if (agent->ops && agent->ops->set_qos)
		ret = (*agent->ops->set_qos)(agent, level);
err_qos:
	for (i = agent->clock_cnt - 1; i >= 0; i--)
		clk_disable(agent->clock[i]);
	if (agent->ops && agent->ops->set_clock)
		ret |= (*agent->ops->set_clock)(agent, level);
err_clock:
	if (agent->ops && agent->ops->set_power)
		ret |= (*agent->ops->set_power)(agent, level);
	/* release all the resources requested*/
	hw_res_release(agent);
power_off_done:
	/* At this point, we call it a successful(?) power off */
	agent->pwrcnt--;
	if (unlikely(WARN_ON(agent->pwrcnt < 0)))
		agent->pwrcnt = 0;
err_power:
exit:
	mutex_unlock(&agent->mux_lock);
	return ret;
}
EXPORT_SYMBOL(hw_tune_power);
