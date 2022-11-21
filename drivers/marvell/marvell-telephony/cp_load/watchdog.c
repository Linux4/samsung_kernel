#include "watchdog.h"
#include <linux/miscdevice.h>
#include <linux/export.h>
extern void __iomem *comm_timer_base_addr;

#define WATCHDOG_WRITE_ACCESS_ENABLE    { writel(WATCHDOG_1ST_ACCESS_KEY, comm_timer_base_addr + TMR_WFAR); \
					writel(WATCHDOG_2ND_ACCESS_KEY, comm_timer_base_addr + TMR_WSAR);}

void watchDogCountStop()
{
	unsigned int value;

	printk("stop watchdog\n");
	WATCHDOG_WRITE_ACCESS_ENABLE
	value = readl(comm_timer_base_addr + TMR_WMER);
	value &= ~WATCHDOG_COUNT_MASK ;	//distable watchdog
	writel(value,comm_timer_base_addr + TMR_WMER);
	printk("done!\n");
}
EXPORT_SYMBOL(watchDogCountStop);

void watchDogInterruptClear(void)
{
	printk("clear watchdog interrupt\n");
	WATCHDOG_WRITE_ACCESS_ENABLE
	writel(1,comm_timer_base_addr + TMR_WICR);
	printk("done!\n");
}
void watchDogHWKick(void)
{
	printk("kick watchdog\n");
	WATCHDOG_WRITE_ACCESS_ENABLE
	writel(1,comm_timer_base_addr + TMR_WCR);
	printk("dong!\n");
}

WATCHDOG_HW_RETURN_CODE watchDogDeactive(void)
{
	unsigned int wvc0, wvc1, match,wmer;

	wvc0 = readl(comm_timer_base_addr + TMR_WVR);
	wvc1 = readl(comm_timer_base_addr + TMR_WVR);
	match = readl(comm_timer_base_addr + TMR_WMR);
	printk("Watch Value Before stop_2:%x %x match=%x\n",wvc0,wvc1,match);
	watchDogCountStop();//watchdog count stop
	watchDogInterruptClear();
	watchDogHWKick();
	disable_irq_nosync(IRQ_COMM_WDT_ID);
	wvc0 = readl(comm_timer_base_addr + TMR_WVR);
	wvc1 = readl(comm_timer_base_addr + TMR_WVR);
	match = readl(comm_timer_base_addr + TMR_WMR);
	wmer = readl(comm_timer_base_addr + TMR_WMER);
	printk("Watch Value After stop_2:%x %x match=%x wmer=%x\n",wvc0,wvc1,match, wmer);
	return SUCCESSFUL;
}
EXPORT_SYMBOL(watchDogDeactive);
