#include "platform_mif_intr_handler.h"
#include "platform_mif_pcie_api.h"

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

#if !defined(CONFIG_SOC_S5E9945)
irqreturn_t platform_mif_set_wlbt_clk_handler(int irq, void *data)
{
	/*This isr can't be called because of clearing pending intr by ohters(ex ACPM, etc) */

	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "irq : %d\n", irq);

	return IRQ_HANDLED;
}
#endif

irqreturn_t platform_wlbt_wakeup_handler(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	bool wake_val = gpio_get_value(platform->gpio_num);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "wake_val %d\n", wake_val);
	if (wake_val == PCIE_LINK_ON) {
		wake_lock(&platform->wakehash_wakelock);
		platform_mif_send_event_to_fsm(platform, PCIE_EVT_PMU_ON_REQ);
	}

	return IRQ_HANDLED;
}

void platform_mif_unregister_wakeup_irq(struct platform_mif *platform)
{
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering IRQs\n");

	disable_irq_wake(platform->wakeup_irq);
	devm_free_irq(platform->dev, platform->wakeup_irq, platform);
}

int platform_mif_register_wakeup_irq(struct platform_mif *platform)
{
	int err;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering IRQs\n");

	err = devm_request_irq(platform->dev, platform->wakeup_irq, platform_wlbt_wakeup_handler,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT, DRV_NAME, platform);

	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to register Wakeup handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	enable_irq_wake(platform->wakeup_irq);

	return 0;
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

void platform_mif_pmu_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "IRQ received\n");

	if (platform->pmu_handler != platform_mif_irq_default_handler)
		platform->pmu_handler(irq, platform->irq_dev_pmu);
	else
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "MIF PMU Interrupt Handler not registered\n");
}

void platform_mif_pmu_pcie_off_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;
	u32 cmd = pcie_get_mbox_pmu_pcie_off(platform->pcie);
	unsigned long flags;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "PCIE OFF IRQ received\n");

	switch (cmd) {
	case PMU_AP_MSG_PCIE_OFF_REQ:
		SCSC_TAG_INFO(PLAT_MIF, "Received PMU IRQ cmd [PCIE OFF REQ]\n");
		spin_lock_irqsave(&platform->cb_sync, flags);
		platform->off_req = true;
		platform_mif_send_event_to_fsm(platform, PCIE_EVT_PMU_OFF_REQ);
		spin_unlock_irqrestore(&platform->cb_sync, flags);
		return; /* don't proceed to pmu_handler as we handle PCIE OFF REQ only here */
	default:
		break;
	}

	if (platform->pmu_pcie_off_handler != platform_mif_irq_default_handler)
		platform->pmu_pcie_off_handler(irq, platform->irq_dev_pmu);
	else
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "MIF PMU pcie_off Interrupt Handler not registered\n");

}

void platform_mif_pmu_error_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "PMU ERROR IRQ received\n");

	if (platform->pmu_error_handler != platform_mif_irq_default_handler)
		platform->pmu_error_handler(irq, platform->irq_dev_pmu);
	else
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "MIF PMU pcie_err Interrupt Handler not registered\n");

}

void platform_mif_intr_handler_api_init(struct platform_mif *platform)
{
	struct scsc_mif_abs *interface = &platform->interface;

	platform->pmu_handler = platform_mif_irq_default_handler;
	platform->pmu_pcie_off_handler = platform_mif_irq_default_handler;
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
