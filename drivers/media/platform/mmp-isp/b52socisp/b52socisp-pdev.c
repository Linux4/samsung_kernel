#include <linux/module.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>

#include <media/b52socisp/b52socisp-pdev.h>

static int trace;
module_param(trace, int, 0644);
MODULE_PARM_DESC(trace,
		"how many trace do you want to see? (0-4):"
		"0 - mute "
		"1 - only actual errors"
		"2 - milestone log"
		"3 - briefing log"
		"4 - detailed log");

struct isp_resrc *isp_resrc_register(struct device *dev,
	struct resource *res, struct list_head *pool, const char *name,
	struct block_id mask, int res_id, void *handle, void *priv)
{
	struct isp_resrc *item;
	unsigned long flags;

	item = devm_kzalloc(dev, sizeof(*item), GFP_KERNEL);
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

	if (res->flags == ISP_RESRC_CLK)
		flags = res->flags;
	else
		flags = res->flags & ~IORESOURCE_BITS;
	switch (flags) {
	case IORESOURCE_MEM:
		item->type = ISP_RESRC_MEM;
		break;
	case IORESOURCE_IRQ:
		item->type = ISP_RESRC_IRQ;
		break;
	case IORESOURCE_DMA:
		item->type = ISP_RESRC_DMA;
		break;
	case IORESOURCE_BUS:
		item->type = ISP_RESRC_BUS;
		break;
	case IORESOURCE_IO:
		item->type = ISP_RESRC_IO;
		break;
	case ISP_RESRC_CLK:
		item->type = ISP_RESRC_CLK;
		break;
	default:
		d_log("failed to add resource '%s'", name);
		break;
	}

	item->refcnt = 0;
	INIT_LIST_HEAD(&item->hook);
	list_add_tail(&item->hook, pool);
	d_log("type %ld resource '%s' added", res->flags, name);
	return item;
}
EXPORT_SYMBOL(isp_resrc_register);

static inline int isp_mem_request(struct isp_resrc *mem)
{
	if (mem->refcnt == 0) {
		mem->va = devm_ioremap_nocache(mem->dev,
					mem->base, mem->size);
		if (mem->va == NULL) {
			devm_release_mem_region(mem->dev, mem->base, mem->size);
			return -ENOMEM;
		}
	}
	mem->refcnt++;
	return 0;
}

static inline void isp_mem_release(struct isp_resrc *mem)
{
	mem->refcnt--;
	if (mem->refcnt != 0)
		return;

	devm_iounmap(mem->dev, (void *)mem->va);
}

static inline int isp_irq_request(struct isp_resrc *irq)
{
	int ret;
	/* irq handler is in irq->handle, irq context is in irq->priv */
	if (irq->refcnt == 0) {
		ret = devm_request_irq(irq->dev, irq->base,
			(irq_handler_t)irq->handle, IRQF_DISABLED|IRQF_SHARED,
			irq->name, irq->priv);
		if (ret < 0) {
			d_inf(1, "IRQ#%d request failed:%d", irq->base, ret);
			return ret;
		}
	}
	irq->refcnt++;
	return 0;
}

static inline void isp_irq_release(struct isp_resrc *irq)
{
	irq->refcnt--;
	if (irq->refcnt != 0)
		return;

	devm_free_irq(irq->dev, irq->base, irq->priv);
}

static inline int isp_clk_request(struct isp_resrc *clk)
{
	if (clk->refcnt == 0) {
		clk->clk = devm_clk_get(clk->dev, clk->name);
		if (IS_ERR(clk->clk)) {
			dev_err(clk->dev, "get clock'%s' failed\n",
				clk->name);
			return PTR_ERR(clk->clk);
		}
	}
	clk->refcnt++;

	return 0;
}

static inline void isp_clk_release(struct isp_resrc *clk)
{
	clk->refcnt--;
	if (clk->refcnt != 0)
		return;

	devm_clk_put(clk->dev, clk->clk);
	clk->clk = NULL;
}

static int isp_resrc_request(struct isp_block *block)
{
	struct isp_resrc *res;
	int ret = 0, i;

	if (block->req_list == NULL)
		return 0;
	block->clock_cnt = 0;

	for (i = 0;; i++) {
		struct isp_res_req *item = block->req_list + i;

		/* Any invalid id assumed be end-of-list mark */
		if ((item->type < 0) || (item->type >= ISP_RESRC_END))
			return ret;
		list_for_each_entry(res, block->resrc_pool, hook) {
			if (id_valid(block->id, res->mask)
			&& (item->type == res->type) && (item->id == res->id))
				goto resrc_found;
		}
		return -ENXIO;
resrc_found:
		/* get resource handle and dispatch to block */
		switch (res->type) {
		case ISP_RESRC_MEM:
			ret = isp_mem_request(res);
			if (ret < 0)
				return ret;
			block->reg_base = res->va + (item->priv - (void *) 0);
			break;
		case ISP_RESRC_CLK:
			if (!(res->name))
				break;
			ret = isp_clk_request(res);
			if (ret < 0)
				return ret;
			block->clock[block->clock_cnt++] = res->clk;
			break;
		case ISP_RESRC_IRQ:
			ret = isp_irq_request(res);
			if (ret < 0)
				return ret;
			block->irq_num = res->base;
			break;
		default:
			WARN_ON(1);
			continue;
		}
		d_inf(3, "res '%s' => mod '%s'", res->name, block->name);
	}
	return 0;
}

static int isp_resrc_release(struct isp_block *block)
{
	struct isp_resrc *res;
	int i, ret = 0;

	if (block->req_list == NULL)
		return 0;
	/* Find the last resource request */
	for (i = 0; ; i++) {
		struct isp_res_req *item = block->req_list + i;
		if ((item->type < 0) || (item->type >= ISP_RESRC_END))
			break;
	}
	/* And release resource in reverse order */
	for (i--; i >= 0; i--) {
		struct isp_res_req *item = block->req_list + i;

		if ((item->type < 0) || (item->type >= ISP_RESRC_END))
			return ret;
		list_for_each_entry(res, block->resrc_pool, hook) {
			if (id_valid(block->id, res->mask)
			&& (item->type == res->type) && (item->id == res->id))
				goto res_match;
		}
		return -ENXIO;

res_match:
		switch (res->type) {
		case ISP_RESRC_MEM:
			isp_mem_release(res);
			block->reg_base = 0;
			break;
		case ISP_RESRC_CLK:
			if (!(res->name))
				break;
			isp_clk_release(res);
			block->clock[--block->clock_cnt] = NULL;
			break;
		case ISP_RESRC_IRQ:
			isp_irq_release(res);
			block->irq_num = 0;
			break;
		default:
			continue;
		}
		d_inf(3, "res '%s' X module '%s'", res->name, block->name);
	}
	return 0;
}

int isp_block_register(struct isp_block *block, struct list_head *pool)
{
	int ret = 0;

	/* add to the manager's block list */
	block->resrc_pool = pool;

	mutex_init(&block->mux_lock);
	block->pwrcnt = 0;
	if (block->ops && block->ops->init)
		ret = (*block->ops->init)(block);
	return ret;
}
EXPORT_SYMBOL(isp_block_register);

void isp_block_unregister(struct isp_block *block)
{
	/* call block clean */
	if (block->ops && block->ops->clean)
		(*block->ops->clean)(block);

	/* withdraw from block pool*/
	block->resrc_pool = NULL;
}
EXPORT_SYMBOL(isp_block_unregister);

int isp_block_init_default(struct isp_block *block)
{
	d_inf(2, "hardware block '%s' initialized", block->name);
	return 0;
}
EXPORT_SYMBOL(isp_block_init_default);

int isp_block_tune_power(struct isp_block *block, unsigned int level)
{
	int i, ret = 0;

	/* TODO: actually, formal way is pass a QoS table, use profile id to
	 * find the QoS item, get power/clock/pin setting, and then apply to
	 * each domain. This is just a temporary solution here */
	mutex_lock(&block->mux_lock);
	/* FIXME: assume level == 0 means power off, other means power on, but
	 * but none-zero value maybe used for suspend process, that way there
	 * maybe bug here! */
	if (level) {
		if (block->pwrcnt++ >= 1) {
			/* Actually, the right thing to do here is merge power
			 * requests, then select the max value as new value */
			ret = 0;
			goto power_on_done;
		}
		d_inf(3, "activate block '%s' and alloc resource", block->name);
		ret = isp_resrc_request(block);
		if (ret < 0)
			goto exit;

		/* and then check if new value == current value here */

		/* Typically, for H/W IP Core, and sensors, power should be
		 * applied before clock is on, QoS is set the last */
		/* power_state = on | off | suspend | resume */
		if (block->ops && block->ops->set_power)
			ret = (*block->ops->set_power)(block, level);
		if (ret < 0)
			goto err_power;

		if (block->ops && block->ops->set_clock)
			ret = (*block->ops->set_clock)(block, level);
		if (ret < 0)
			goto err_clock;
		for (i = 0; i < block->clock_cnt; i++)
			clk_prepare_enable(block->clock[i]);

		if (block->ops && block->ops->open)
			ret = (*block->ops->open)(block);
		if (ret < 0)
			goto err_open;
power_on_done:
		d_inf(4, "block '%s' refcnt = %d", block->name, block->pwrcnt);
		goto exit;
	}

	if (block->pwrcnt != 1) {
		ret = 0;
		goto power_off_done;
	}

	d_inf(3, "de-activate block '%s' and retreive resource", block->name);
	/* for power off sequence, just ignore error and close everything
	 * if possible*/
	if (block->ops && block->ops->close)
		(*block->ops->close)(block);
err_open:
	for (i = block->clock_cnt - 1; i >= 0; i--)
		clk_disable_unprepare(block->clock[i]);
	if (block->ops && block->ops->set_clock)
		(*block->ops->set_clock)(block, 0);
err_clock:
	if (block->ops && block->ops->set_power)
		(*block->ops->set_power)(block, 0);
	/* release all the resources requested*/
	isp_resrc_release(block);
power_off_done:
	/* At this point, we call it a successful(?) power off */
	block->pwrcnt--;
	d_inf(4, "block '%s' refcnt = %d", block->name, block->pwrcnt);
	if (unlikely(WARN_ON(block->pwrcnt < 0)))
		block->pwrcnt = 0;
err_power:
exit:
	mutex_unlock(&block->mux_lock);
	return ret;
}
EXPORT_SYMBOL(isp_block_tune_power);

void isp_block_pool_init(struct list_head *pool)
{
	INIT_LIST_HEAD(pool);
}
EXPORT_SYMBOL(isp_block_pool_init);

void isp_block_pool_clean(struct list_head *pool)
{
	while (!list_empty(pool)) {
		struct isp_resrc *res =
			list_first_entry(pool, struct isp_resrc, hook);
		if (res->refcnt) {
			d_inf(1, "hardware resource '%s' not released!",
				res->name);
			switch (res->type) {
			case ISP_RESRC_MEM:
				isp_mem_release(res);
				break;
			case ISP_RESRC_CLK:
				isp_clk_release(res);
				break;
			case ISP_RESRC_IRQ:
				isp_irq_release(res);
				break;
			default:
				continue;
			}
		}
		list_del(&res->hook);
		devm_kfree(res->dev, res);
	}
}
EXPORT_SYMBOL(isp_block_pool_clean);
