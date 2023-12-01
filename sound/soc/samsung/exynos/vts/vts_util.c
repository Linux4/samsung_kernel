
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/pcm.h>

#include "vts.h"
#include "vts_dbg.h"
#include "vts_util.h"

void __iomem *vts_devm_get_request_ioremap(struct platform_device *pdev, const char *name,
		phys_addr_t *phys_addr, size_t *size)
{
	struct resource *res;
	void __iomem *ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to get %s\n", name);
		return ERR_PTR(-EINVAL);
	}

	if (phys_addr)
		*phys_addr = res->start;
	if (size)
		*size = resource_size(res);

	res = devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), name);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to request %s\n", name);
		return ERR_PTR(-EFAULT);
	}

	ret = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(ret)) {
		dev_err(&pdev->dev, "Failed to map %s\n", name);
		return ERR_PTR(-EFAULT);
	}

	vts_dev_dbg(&pdev->dev, "%s: %s(%pK) is mapped on %pK with size of %zu", __func__, name,
			(void *)res->start, ret, (size_t)resource_size(res));
	vts_dev_info(&pdev->dev, "%s is mapped(size:%zu)", name, (size_t)resource_size(res));

	return ret;
}

int vts_devm_request_threaded_irq(
	struct device *dev, const char *irq_name,
	unsigned int hw_irq, irq_handler_t thread_fn)
{
	struct vts_data *data = dev_get_drvdata(dev);
	struct platform_device *pdev = data->pdev;
	int ret;

	data->irq[hw_irq] = platform_get_irq_byname(pdev, irq_name);
	if (data->irq[hw_irq] < 0) {
		vts_dev_err(dev, "Failed to get irq %s: %d\n",
			irq_name, data->irq[hw_irq]);

		return data->irq[hw_irq];
	}

	ret = devm_request_threaded_irq(dev, data->irq[hw_irq],
			NULL, thread_fn,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev->init_name,
			pdev);

	if (ret < 0)
		vts_dev_err(dev, "Unable to request irq %s: %d\n",
			irq_name, ret);

	return ret;
}

struct clk *vts_devm_clk_get_and_prepare(
	struct device *dev, const char *name)
{
	struct clk *clk;
	int ret;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		vts_dev_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	ret = clk_prepare(clk);
	if (ret < 0) {
		vts_dev_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

error:
	return clk;
}
