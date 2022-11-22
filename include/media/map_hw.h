#ifndef _MAP_HARDWARE_H
#define _MAP_HARDWARE_H

/* This file declears all the data structures to support hardware part of
 * Manager-Agent-Pack model. */

/* MAP camera model aims to provide a framework to realize a batch of camera
 * driver solutions for a serial of SOCs, which use combinations of several
 * same camera IP cores as the camera H/W solution. */

/* SOC vendors may designe a family of SOC chips, which enclose different
 * configurations of a serial of camera IP cores, in order to provide
 * different capability of camera subsystem. Or these chips may expose
 * different capability due to different SOC H/W limitation (bandwidth, etc)
 */

/* By the time MAP camera model is designed, camera H/W evolution arrives at
 * an era when mobile camera is expected to provied at least two kind of
 * capability: front camera should be power-friendly / quality-insensitive,
 * back camera should be qulity-sensitive / fancy-featured / power-insensitive.
 * This pushes SOC designers to carefully breakdown camera controller to
 * several agents, and make them configurable to formup different H/W pipelines,
 * so that these H/W pipeline has different power / performace attributes.
 * Or, SOC designers provide more than one carefully designed camera controller,
 * each with different power / performance, so as to meet the various request.
 */

/* MediaController is designed to track this trend, so S/W can use concept
 * such as "entity", "pipeline" to describe the corresponding H/W concepts.
 */

/* MAP Camera Model push this effort one step further, to help camera driver
 * designers manage the power/clock/quality of each agents and implement
 * the multifarious use case */

#include <linux/io.h>
#include <linux/string.h>
#include <linux/platform_device.h>

#define MAX_AGENT_PER_SYSTEM	20
#define MAX_AGENT_PER_EVENT	10
#define MAX_CLOCK_PER_AGENT	5

enum map_agent_type {
	MAP_AGENT_NONE	= 0,
	MAP_AGENT_NORMAL,
	MAP_AGENT_DMA_OUT,
	MAP_AGENT_DMA_IN,
	MAP_AGENT_SENSOR,
};

enum hw_res_type {
	MAP_RES_IO = 0,
	MAP_RES_MEM,
	MAP_RES_IRQ,
	MAP_RES_DMA,
	MAP_RES_BUS,
	MAP_RES_CLK,
	MAP_RES_END,
};

struct hw_agent;
struct hw_manager;

struct hw_agent_ops {
	/* Called in H/W initialize */
	int	(*init)(struct hw_agent *agent);
	/* Called before H/W destroy */
	void	(*clean)(struct hw_agent *agent);
	/* Called upon H/W enable/disable, used to set clock rate, etc*/
	int	(*set_power)(struct hw_agent *agent, int level);
	int	(*set_clock)(struct hw_agent *agent, int rate);
	/* Called upon H/W function start (pipeline enable) */
	int	(*open)(struct hw_agent *agent);
	/* Called upon H/W function stop (pipeline disable) */
	void	(*close)(struct hw_agent *agent);
	/* Called upon H/W QoS update (ie: resolution change) */
	int	(*set_qos)(struct hw_agent *agent, int qos_id);
};

union agent_id {
	struct {
		u8	dev_type; /* What kind of device own this mod? */
		u8	dev_id; /* Which device instance owns it? */
		u8	mod_type; /* H/W mod type */
		u8	mod_id; /* H/W instance ID */
	};
	int	ident;
};

struct hw_res_req {
	enum hw_res_type	type;	/* Xlate from pdev::res::type*/
	int			id;
	/* Pointer to tell manager how to dispatch this resource to agent */
	/* For MAP_RES_MEM, it's the agent's offset from device base address*/
	void *priv;
};

/* Data structure to describe each H/W agent, which can be part of a
 * or an entire camera IP core */
struct hw_agent {
	union agent_id		id;
	char			*name;
	struct hw_manager	*mngr;

	/* platform resource */
	struct hw_res_req	*req_list;
	void __iomem		*reg_base;
	u32			irq_num;
	struct clk		*clock[MAX_CLOCK_PER_AGENT];
	int			clock_cnt;

	int		pwrcnt;
	struct mutex	mux_lock;	/* lock of HW reg/clock/power/qos/refcnt
					 * between irq and process */
	void		*plat_priv;	/* point to platform specific data */
	struct hw_agent_ops	*ops;
};

struct hw_resrc {
	const char		*name;	/* platform_device::resource::name*/
	struct device		*dev;
	int			type;	/* Xlate from pdev::res::type*/
	int			id;
	union agent_id		mask;	/* only allocate within mask */
	u32			base;
	u32			size;
	struct list_head	hook;	/* hook for manager::resrc_pool */
	void			*priv;	/* private pointer to none-standard
					 * data types that will be passed to
					 * resource handler */
	void			*handle;
	int			refcnt;
};

struct hw_manager {
	struct list_head	resrc_pool;
	struct hw_agent	*agent_addr[MAX_AGENT_PER_SYSTEM];
	int			(*get_slot)(union agent_id);
	void			*drv_priv;
};

struct hw_event;
typedef int __must_check (*event_handler_t)\
	(struct hw_agent *receiver, struct hw_event *event);

struct hw_dispatch_entry {
	struct hw_agent	*agent;
	event_handler_t	handler;
};
/* event us used to provide a tricky method, to send messages between agents,
 * could be useful for irq dispatching / agent behavior dispatching, etc */
struct hw_event {
	const char	*name;	/* event of this name */
	const char	*owner_name;
	struct hw_agent	*owner; /* sender agent */
	u32		type;	/* event type, could be irq number */
	u32		id;	/* event id as allocated by manager */
	struct hw_dispatch_entry	dispatch_list[MAX_AGENT_PER_EVENT];
	void		*msg;	/* event message */
};

struct hw_resrc *hw_res_register(struct device *dev,
	struct resource *res, struct hw_manager *mngr, const char *name,
	union agent_id mask, int res_id, void *handle, void *priv);
int hw_res_dispatch(struct hw_manager *mngr);
void hw_res_withdraw(struct hw_manager *mngr);

int hw_agent_register(struct hw_agent *agent, struct hw_manager *mngr);
void hw_agent_unregister(struct hw_agent *agent, struct hw_manager *mngr);
struct hw_agent *hw_agent_find(struct hw_manager *mngr, union agent_id id);
int hw_agent_init_default(struct hw_agent *agent);

static inline int hw_event_subscribe(struct hw_event *event,
					struct hw_agent *agent,
					event_handler_t handler)
{
	struct hw_dispatch_entry *item = event->dispatch_list;
	int i;
	/* Find a free slot to register the agent */
	for (i = 0; i < MAX_AGENT_PER_EVENT; i++, item++) {
		if (item->agent == NULL) {
			item->agent = agent;
			item->handler = handler;
			return 0;
		}
	}
	return -ENOMEM;
}
static inline int hw_event_unsubscribe(struct hw_event *event,
					struct hw_agent *agent)
{
	struct hw_dispatch_entry *item = event->dispatch_list;
	int i, ret = -EINVAL;

	for (i = 0; i < MAX_AGENT_PER_EVENT; i++, item++) {
		if (item->agent == agent) {
			item->agent = NULL;
			item->handler = NULL;
			ret = 0;
		}
	}
	return ret;
}
static inline struct hw_event *hw_local_event_find(struct hw_agent *agent,
					const char *name)
{
	struct hw_resrc *res;
	/* find event on local IP */
	list_for_each_entry(res, &agent->mngr->resrc_pool, hook) {
		if ((agent->id.ident == (res->mask.ident & agent->id.ident))
			&& (res->type == MAP_RES_IRQ)
			&& (res->name) && strlen(res->name) && name
			&& (!strcmp(res->name, name)))
			return res->priv;
	}
	return NULL;
}
int hw_event_dispatch(struct hw_agent *sender, struct hw_event *event);

int hw_manager_init(struct hw_manager *mngr);
void hw_manager_clean(struct hw_manager *mngr);

/* Helper functions */
int hw_tune_power(struct hw_agent *agent, int level);
int hw_enable(struct hw_agent *agent);
void hw_disable(struct hw_agent *agent);

/* MAP topology setup process:
 * 1> use probe function of a common platform_device to breakdown an camera
 *    H/W IP core into several agent
 * 2> each agent driver is then invoked by breakdown function to register
 *    itself to the agent list.
 * 3> Pipeline manager, which holes all the knowledge to describe the topology
 *    and setup/enable/disable/destroy a pipeline, is then initialized, and
 *    setup a default pipeline to accomodate legacy V4L2 behavior
 * 4> Pipeline manager accept requests to change/add/remove pipeline, and
 *    manipulate each agent according to input. Each agent is not
 *    aware of the existance of pipeline.
 */
static inline u32 agent_read(struct hw_agent *agent, const u32 addr)
{
	return readl(agent->reg_base + addr);
}

static inline void agent_write(struct hw_agent *agent,
				const u32 addr, const u32 value)
{
	writel(value, agent->reg_base + addr);
}

static inline void agent_set(struct hw_agent *agent,
				const u32 addr, const u32 mask)
{
	u32 val = readl(agent->reg_base + addr);
	val |= mask;
	writel(val, agent->reg_base + addr);
}

static inline void agent_clr(struct hw_agent *agent,
				const u32 addr, const u32 mask)
{
	u32 val = readl(agent->reg_base + addr);
	val &= ~mask;
	writel(val, agent->reg_base + addr);
}

#endif
