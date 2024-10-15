#include "platform_mif_intr_handler.h"
#ifdef CONFIG_SCSC_PCIE
#include "platform_mif_pcie_api.h"
#endif

void platform_mif_suspend_reg_handler(struct scsc_mif_abs *interface,
					     int (*suspend)(struct scsc_mif_abs *abs, void *data),
					     void (*resume)(struct scsc_mif_abs *abs, void *data), void *data)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif suspend/resume handlers in %p %p\n", platform,
			  interface);
	platform->suspend_handler = suspend;
	platform->resume_handler = resume;
	platform->suspendresume_data = data;
}

void platform_mif_suspend_unreg_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif suspend/resume handlers in %p %p\n", platform,
			  interface);
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
void platform_recovery_disabled_reg(struct scsc_mif_abs *interface, bool (*handler)(void))
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif recovery %pS\n", handler);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->recovery_disabled = handler;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

void platform_recovery_disabled_unreg(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif recovery\n");
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->recovery_disabled = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}
#endif

void platform_mif_irq_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL, "INT handler not registered\n");
}

void platform_mif_irq_reset_request_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL, "INT reset_request handler not registered\n");
}

void platform_mif_wlan_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "IRQ received\n");

	if (platform->wlan_handler != platform_mif_irq_default_handler)
		platform->wlan_handler(irq, platform->irq_dev);
	else
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "MIF wlan Interrupt Handler not registered\n");
}

void platform_mif_wpan_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "IRQ received\n");

	if (platform->wpan_handler != platform_mif_irq_default_handler)
		platform->wpan_handler(irq, platform->irq_dev_wpan);
	else
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "MIF wpan Interrupt Handler not registered\n");
}

irqreturn_t platform_mbox_pmu_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "INT received, boot_state = %u\n", platform->boot_state);
	if (platform->boot_state == WLBT_BOOT_CFG_DONE) {
		if (platform->pmu_handler != platform_mif_irq_default_handler)
			platform->pmu_handler(irq, platform->irq_dev_pmu);
		else
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					"MIF PMU Int Handler not registered\n");
	} else {
		/* platform->boot_state == WLBT_BOOT_WAIT_CFG_REQ */
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Spurious mbox pmu irq during cfg_req phase\n");
	}
	return IRQ_HANDLED;
}

void platform_mif_intr_handler_api_init(struct platform_mif *platform)
{
	struct scsc_mif_abs *interface = &platform->interface;

	platform->pmu_handler = platform_mif_irq_default_handler;
	platform->wlan_handler = platform_mif_irq_default_handler;
	platform->wpan_handler = platform_mif_irq_default_handler;
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;

	interface->suspend_reg_handler = platform_mif_suspend_reg_handler;
	interface->suspend_unreg_handler = platform_mif_suspend_unreg_handler;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	interface->recovery_disabled_reg = platform_recovery_disabled_reg;
	interface->recovery_disabled_unreg = platform_recovery_disabled_unreg;
#endif

}
