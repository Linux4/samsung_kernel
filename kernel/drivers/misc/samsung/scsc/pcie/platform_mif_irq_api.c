#include "platform_mif_irq_api.h"
#include "platform_mif_intr_handler.h"
#include "platform_mif_pcie_api.h"

u32 platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val = 0;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Getting INTMR0 0x%x target %s\n", val, (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");
	return val;
}

u32 platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val = 0;

	val = pcie_irq_get(platform->pcie, target);
	return val;
}

void platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pcie_irq_bit_set(platform->pcie, bit_num, target);
}

void platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pcie_irq_bit_clear(platform->pcie, bit_num, target);
}

void platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pcie_irq_bit_mask(platform->pcie, bit_num, target);
}

void platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pcie_irq_bit_unmask(platform->pcie, bit_num, target);
}



void platform_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
					 void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif int handler %pS in %p %p\n", handler, platform,
			  interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wlan_handler = handler;
	platform->irq_dev = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

void platform_mif_irq_unreg_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif int handler %pS\n", interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wlan_handler = platform_mif_irq_default_handler;
	platform->irq_dev = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

void platform_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
					      void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif int handler for WPAN %pS in %p %p\n", handler,
			  platform, interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wpan_handler = handler;
	platform->irq_dev_wpan = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

void platform_mif_irq_unreg_handler_wpan(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif int handler for WPAN %pS\n", interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wpan_handler = platform_mif_irq_default_handler;
	platform->irq_dev_wpan = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

void platform_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface,
						       void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif reset_request int handler %pS in %p %p\n", handler,
			  platform, interface);
	platform->reset_request_handler = handler;
	platform->irq_reset_request_dev = dev;
	if (atomic_read(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Default WDOG handler disabled by spurios IRQ...re-enabling.\n");
		enable_irq(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_set(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt, 0);
	}
}

void platform_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "UnRegistering mif reset_request int handler %pS\n", interface);
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->irq_reset_request_dev = NULL;
}


void platform_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
					     void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif pmu int handler %pS in %p %p\n", handler, platform,
			  interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->pmu_handler = handler;
	platform->irq_dev_pmu = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

void platform_mif_irq_reg_pmu_error_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
					     void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif pmu pcie_error int handler %pS in %p %p\n", handler, platform,
			  interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->pmu_error_handler = handler;
	platform->irq_dev_pmu = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irqdump(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	pcie_s6165_irqdump(platform->pcie);
}

void platform_mif_irq_api_init(struct platform_mif *platform)
{
	struct scsc_mif_abs *interface = &platform->interface;

	platform->irq_dev = NULL;
	platform->irq_reset_request_dev = NULL;

	interface->irq_reg_handler = platform_mif_irq_reg_handler;
	interface->irq_unreg_handler = platform_mif_irq_unreg_handler;
	interface->irq_reg_handler_wpan = platform_mif_irq_reg_handler_wpan;
	interface->irq_unreg_handler_wpan = platform_mif_irq_unreg_handler_wpan;
	interface->irq_reg_reset_request_handler = platform_mif_irq_reg_reset_request_handler;
	interface->irq_unreg_reset_request_handler = platform_mif_irq_unreg_reset_request_handler;
	interface->irq_reg_pmu_handler = platform_mif_irq_reg_pmu_handler;
	interface->irq_reg_pmu_error_handler = platform_mif_irq_reg_pmu_error_handler;
	interface->wlbt_irqdump = platform_mif_irqdump;
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	interface->irq_pmu_bit_mask = platform_mif_irq_pmu_bit_mask;
    interface->irq_pmu_bit_unmask = platform_mif_irq_pmu_bit_unmask;
#endif
}
