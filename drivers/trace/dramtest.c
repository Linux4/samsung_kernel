#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/serial_s3c.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <asm/irq.h>
#include <linux/dma-mapping.h>

#define SIZE 0x200000

static char *drambase;
static struct device *dev_dramtest;

int validate(void)
{
	int i;
	char data;
	int cnt = 0;

	for (i = 0; i < SIZE; i++) {
		if (!(i % SIZE/10))
			dev_emerg(dev_dramtest, "dramtest info: addr(%p)\n", drambase + i);

		data = *(drambase + i);
		if (data != 0xff) {
			cnt++;
			dev_emerg(dev_dramtest, "dramtest info: addr(%p) data(0x%02x)\n", drambase + i, data);
		}
	}

	dev_emerg(dev_dramtest, "dramtest info: error_cnt(%d)\n", cnt);

	if (!cnt)
		return -1;

	return 0;
}

static int dramtest_probe(struct platform_device *pdev)
{

	long i;
	dma_addr_t dma_addr;

	drambase = dma_alloc_coherent(&pdev->dev, SIZE, &dma_addr, GFP_KERNEL);
	if (!drambase) {
		dev_err(&pdev->dev, "memory allocation failed!!\n");
		return -ENOMEM;
	}

	for (i = 0; i < SIZE; i++)
		*(drambase + i) = 0xff;

	dev_info(&pdev->dev, "paddr(0x%llx) vaddr(0x%p) size(0x%x)\n", dma_addr, drambase, SIZE);

	dev_dramtest = &pdev->dev;
	return 0;
}

static int dramtest_remove(struct platform_device *dev)
{

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dramtest_dt_match[] = {
	{ .compatible = "samsung,dramtest",
		.data = (void *)NULL },
	{},
};
MODULE_DEVICE_TABLE(of, s3c24xx_uart_dt_match);
#endif

static struct platform_driver samsung_dramtest_driver = {
	.probe		= dramtest_probe,
	.remove		= dramtest_remove,
	.driver		= {
		.name	= "samsung-dramtest",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(dramtest_dt_match),
	},
};

static int __init dramtest_modinit(void)
{
	return platform_driver_register(&samsung_dramtest_driver);
}

static void __exit dramtest_modexit(void)
{
	platform_driver_unregister(&samsung_dramtest_driver);
}

module_init(dramtest_modinit);
module_exit(dramtest_modexit);

MODULE_ALIAS("platform:samsung-dramtest");
MODULE_DESCRIPTION("Samsung DRAM test driver");
MODULE_AUTHOR("Sungjinn Chung <sungjinn.chung@samsung.com>");
MODULE_LICENSE("GPL v2");
