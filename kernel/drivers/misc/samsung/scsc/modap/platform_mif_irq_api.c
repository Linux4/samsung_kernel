#include "modap/platform_mif_irq_api.h"
#include "modap/platform_mif_intr_handler.h"
#include "modap/platform_mif_regmap_api.h"
#include "mif_reg.h"

#ifdef CONFIG_WLBT_KUNIT
#include "../kunit/kunit_platform_mif_irq_api.c"
#endif

inline void platform_mif_reg_write(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base + offset);
}

inline u32 platform_mif_reg_read(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base + offset);
}

inline void platform_mif_reg_write_wpan(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base_wpan + offset);
}

inline u32 platform_mif_reg_read_wpan(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base_wpan + offset);
}

#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
inline void platform_mif_reg_write_pmu(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base_pmu + offset);
}

inline u32 platform_mif_reg_read_pmu(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base_pmu + offset);
}
#endif

u32 platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0)) >> 16;
	else
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0)) >> 16;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Getting INTMR0 0x%x target %s\n", val,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
	return val;
}

u32 platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;

	/* Function has to return the interrupts that are enabled *AND* not masked */
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR0)) >> 16;
	else
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMSR0)) >> 16;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Getting INTMSR0 0x%x target %s\n", val,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
	return val;
}

void platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 reg;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}

	reg = INTGR1;
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(reg), (1 << bit_num));
	else
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(reg), (1 << bit_num));

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTGR1: bit %d on target %s\n", bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

void platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	/* WRITE : 1 = Clears Interrupt */
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), ((1 << bit_num) << 16));
	else
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), ((1 << bit_num) << 16));

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTCR0: bit %d on target %s\n", bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

void platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 1 = Mask Interrupt */
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val | ((1 << bit_num) << 16));
	} else {
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 1 = Mask Interrupt */
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), val | ((1 << bit_num) << 16));
	}
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTMR0: 0x%x bit %d on target %s\n", val | (1 << bit_num), bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

void platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 0 = Unmask Interrupt */
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val & ~((1 << bit_num) << 16));
	} else {
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 0 = Unmask Interrupt */
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), val & ~((1 << bit_num) << 16));
	}
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "UNMASK Setting INTMR0: 0x%x bit %d on target %s\n", val & ~((1 << bit_num) << 16), bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
static void platform_mif_irq_pmu_bit_mask(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read_pmu(platform, MAILBOX_WLBT_REG(INTMR0));
	/* WRITE : 1 = Mask Interrupt */
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR0), val | (1 << 16));
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTMR0: 0x%x bit 1 on target PMU\n", val | (1 << 16));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_pmu_bit_unmask(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read_pmu(platform, MAILBOX_WLBT_REG(INTMR0));
	/* WRITE : 0 = Unmask Interrupt */
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR0), val & ~(1 << 16));
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "UNMASK Setting INTMR0: 0x%x bit 1 on target PMU\n", val & ~(1 << 16));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}
#endif

/* Return the contents of the mask register */
u32 __platform_mif_irq_bit_mask_read(struct platform_mif *platform)
{
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read INTMR0: 0x%x\n", val);

	return val;
}

/* Return the contents of the mask register */
u32 __platform_mif_irq_bit_mask_read_wpan(struct platform_mif *platform)
{
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read INTMR0: 0x%x\n", val);

	return val;
}

/* Write the mask register, destroying previous contents */
void __platform_mif_irq_bit_mask_write(struct platform_mif *platform, u32 val)
{
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val);
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Write INTMR0: 0x%x\n", val);
}

void __platform_mif_irq_bit_mask_write_wpan(struct platform_mif *platform, u32 val)
{
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), val);
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Write INTMR0: 0x%x\n", val);
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

void platform_cfg_req_irq_clean_pending(struct platform_mif *platform)
{
	int irq;
	int ret;
	bool pending = 0;
	char *irqs_name = {"CFG_REQ"};

	irq = platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num;
	ret = irq_get_irqchip_state(irq, IRQCHIP_STATE_PENDING, &pending);

	if (!ret) {
		if(pending == 1){
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "IRQCHIP_STATE %d(%s): pending %d",
							  irq, irqs_name, pending);
			pending = 0;
			ret = irq_set_irqchip_state(irq, IRQCHIP_STATE_PENDING, pending);
		}
	}
}

irqreturn_t platform_cfg_req_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received, boot_state = %u\n", platform->boot_state);

	if (platform->boot_state == WLBT_BOOT_WAIT_CFG_REQ) {
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
		/* it is executed on process context. */
		queue_work(platform->cfgreq_workq, &platform->cfgreq_wq);
#else
		/* it is executed on interrupt context. */
		platform_set_wlbt_regs(platform);
#endif
	} else {
#if IS_ENABLED(CONFIG_SOC_S5E8835) || IS_ENABLED(CONFIG_SOC_S5E8845) || IS_ENABLED(CONFIG_SOC_S5E5535)
		pr_info("[%s] ????\n", __func__);
		return IRQ_HANDLED;
#else
		if (platform->boot_state != WLBT_BOOT_IN_RESET) {
			struct regmap *regmap = platform_mif_get_regmap(
				platform,
				BOOT_CFG);
			/* platform->boot_state = WLBT_BOOT_CFG_DONE; */
			if (platform->pmu_handler != platform_mif_irq_default_handler)
				platform->pmu_handler(irq, platform->irq_dev_pmu);
			else
				SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "MIF PMU Interrupt Handler not registered\n");
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated BOOT_CFG_ACK\n");
			regmap_write(regmap, PMU_BOOT_ACK, PMU_BOOT_COMPLETE);
		}
#endif
	}
	return IRQ_HANDLED;
}

inline void platform_int_debug(struct platform_mif *platform)
{
	int i;
	int irq;
	int ret;
	bool pending, active, masked;
	int irqs[] = {PLATFORM_MIF_MBOX, PLATFORM_MIF_WDOG};
	char *irqs_name[] = {"MBOX", "WDOG"};

	for (i = 0; i < (sizeof(irqs) / sizeof(int)); i++) {
		irq = platform->wlbt_irq[irqs[i]].irq_num;

		ret  = irq_get_irqchip_state(irq, IRQCHIP_STATE_PENDING, &pending);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_ACTIVE,  &active);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_MASKED,  &masked);
		if (!ret)
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "IRQCHIP_STATE %d(%s): pending %d, active %d, masked %d\n",
							  irq, irqs_name[i], pending, active, masked);
	}
	platform_wlbt_regdump(&platform->interface);
}

void platform_mif_unregister_irq(struct platform_mif *platform)
{
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering IRQs\n");

#ifdef CONFIG_SCSC_QOS
	/* clear affinity mask */
	irq_set_affinity_hint(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, NULL);
#endif
#ifdef CONFIG_SOC_S5E5535
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_ALIVE].irq_num, platform);
#endif
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num, platform);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform);
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_PMU].irq_num, platform);
#endif
	/* Reset irq_disabled_cnt for WDOG IRQ since the IRQ itself is here unregistered and disabled */
	atomic_set(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt, 0);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform);
}

int platform_mif_register_irq(struct platform_mif *platform)
{
	int err;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering IRQs\n");

#ifdef CONFIG_SOC_S5E5535
	/* Register ALIVE irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering ALIVE irq: %d flag 0x%x\n",
			  platform->wlbt_irq[PLATFORM_MIF_ALIVE].irq_num, platform->wlbt_irq[PLATFORM_MIF_ALIVE].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_ALIVE].irq_num, platform_alive_isr,
			       platform->wlbt_irq[PLATFORM_MIF_ALIVE].flags, IRQ_NAME_ALIVE, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register ALIVE handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}
#endif

	/* Register MBOX irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering MBOX irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform->wlbt_irq[PLATFORM_MIF_MBOX].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform_mif_isr,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX].flags, IRQ_NAME_MBOX, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Register MBOX irq WPAN */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering MBOX WPAN irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num, platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num, platform_mif_isr_wpan,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].flags, IRQ_NAME_MBOX_WPAN, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	/* Register MBOX irq PMU */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering MBOX PMU irq: %d flag 0x%x\n",
			  platform->wlbt_irq[PLATFORM_MIF_MBOX_PMU].irq_num,
			  platform->wlbt_irq[PLATFORM_MIF_MBOX_PMU].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_PMU].irq_num, platform_mbox_pmu_isr,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX_PMU].flags, IRQ_NAME_MBOX_PMU, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX PMU handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}
#endif

	/* Register WDOG irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering WDOG irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform->wlbt_irq[PLATFORM_MIF_WDOG].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform_wdog_isr,
			       platform->wlbt_irq[PLATFORM_MIF_WDOG].flags, IRQ_NAME_WDOG, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register WDOG handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Mark as WLBT in reset before enabling IRQ to guard against spurious IRQ */
	platform->boot_state = WLBT_BOOT_IN_RESET;
	smp_wmb(); /* commit before irq */

	/* Register WB2AP_CFG_REQ irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering CFG_REQ irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].flags);

	/* clean CFG_REQ PENDING interrupt. */
	platform_cfg_req_irq_clean_pending(platform);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform_cfg_req_isr,
			       platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].flags, DRV_NAME, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register CFG_REQ handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Leave disabled until ready to handle */
	disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	return 0;
}


irqreturn_t platform_mif_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "INT %pS\n", platform->wlan_handler);
	if (platform->wlan_handler != platform_mif_irq_default_handler) {
		platform->wlan_handler(irq, platform->irq_dev);
	}
	else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "MIF Interrupt Handler not registered.\n");
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), (0xffff << 16));
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), (0xffff << 16));
	}

	return IRQ_HANDLED;
}

irqreturn_t platform_mif_isr_wpan(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "INT %pS\n", platform->wpan_handler);
	if (platform->wpan_handler != platform_mif_irq_default_handler) {
		platform->wpan_handler(irq, platform->irq_dev_wpan);
	} else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "MIF Interrupt Handler not registered\n");
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), (0xffff << 16));
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), (0xffff << 16));
	}

	return IRQ_HANDLED;
}

irqreturn_t platform_wdog_isr(int irq, void *data)
{

	int ret = 0;
	struct platform_mif *platform = (struct platform_mif *)data;
	struct regmap *regmap = platform_mif_get_regmap(platform, PMUREG);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received %d\n", irq);
	platform_int_debug(platform);

	if (platform->reset_request_handler != platform_mif_irq_reset_request_default_handler) {
		if (platform->boot_state == WLBT_BOOT_WAIT_CFG_REQ) {
			/* Spurious interrupt from the SOC during CFG_REQ phase, just consume it */
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Spurious wdog irq during cfg_req phase\n");
			return IRQ_HANDLED;
		} else {
			disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
			platform->reset_request_handler(irq, platform->irq_reset_request_dev);
		}
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WDOG Interrupt reset_request_handler not registered\n");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Disabling unhandled WDOG IRQ.\n");
		disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_inc(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt);
	}

	/* The wakeup source isn't cleared until WLBT is reset, so change the interrupt type to suppress this */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	if (platform->recovery_disabled && platform->recovery_disabled()) {
#else
	if (mxman_recovery_disabled()) {
#endif
		ret = regmap_update_bits(regmap, WAKEUP_INT_TYPE,
					 RESETREQ_WLBT, 0);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Set RESETREQ_WLBT wakeup interrput type to EDGE.\n");
		if (ret < 0)
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Failed to Set WAKEUP_INT_IN[RESETREQ_WLBT]: %d\n", ret);
	}

	return IRQ_HANDLED;
}

/* TODO: ALIVE IRQ seems not connected ., set to if 0*/
/* #ifdef CONFIG_SCSC_ENABLE_ALIVE_IRQ */
#ifdef CONFIG_SOC_S5E5535
irqreturn_t platform_alive_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received\n");

	return IRQ_HANDLED;
}
#endif

int platform_mif_irq_get_ioresource_mem(
	struct platform_device *pdev,
	struct platform_mif *platform)
{
	struct resource *reg_res;

	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	platform->base = devm_ioremap_resource(&pdev->dev, reg_res);
	if (IS_ERR(platform->base)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_WLAN\n");
		return PTR_ERR(platform->base);
	}

	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	platform->base_wpan = devm_ioremap_resource(&pdev->dev, reg_res);
	if (IS_ERR(platform->base_wpan)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_WPAN\n");
		return PTR_ERR(platform->base_wpan);
	}

#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	platform->base_pmu = devm_ioremap_resource(&pdev->dev, reg_res);
	if (IS_ERR(platform->base_pmu)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_PMU\n");
		return PTR_ERR(platform->base_pmu);
        }
#endif
	return 0;
}

int platform_mif_irq_get_ioresource_irq(
	struct platform_device *pdev,
	struct platform_mif *platform)
{
	int i;

	/* Get the IRQ_RESOURCE_COUNT IRQ resources */
	for (i = 0; i < IRQ_RESOURCE_COUNT; i++) {

		int irq_num;
		int             irqtag = 0;
		struct resource irq_res;
		irq_num = of_irq_to_resource(platform->dev->of_node, i, &irq_res);
		if (irq_num <= 0) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "invalid irq_num %d for index %d\n", irq_num, i);
			continue;
		}
		if (!strcmp(irq_res.name, "MBOX_WLAN")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_WLAN irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_MBOX;
		} else if (!strcmp(irq_res.name, "MBOX_WPAN")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_WPAN irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_MBOX_WPAN;
		} else if (!strcmp(irq_res.name, "ALIVE")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "ALIVE irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_ALIVE;
		} else if (!strcmp(irq_res.name, "WDOG")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "WDOG irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_WDOG;
		} else if (!strcmp(irq_res.name, "CFG_REQ")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "CFG_REQ irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_CFG_REQ;
		} else if (!strcmp(irq_res.name, "MBOX_PMU")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_PMU irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_MBOX_PMU;
		} else {
			SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev,
					 "Invalid irq res name: %s\n",
				irq_res.name);
		}
		platform->wlbt_irq[irqtag].irq_num = irq_res.start;
		platform->wlbt_irq[irqtag].flags = (irq_res.flags & IRQF_TRIGGER_MASK);
		atomic_set(&platform->wlbt_irq[irqtag].irq_disabled_cnt, 0);
	}

	return 0;
}

void platform_mif_irqdump(struct scsc_mif_abs *interface)
{
	platform_mif_irq_bit_mask_status_get(interface, SCSC_MIF_ABS_TARGET_WLAN);
	platform_mif_irq_bit_mask_status_get(interface, SCSC_MIF_ABS_TARGET_WPAN);
	platform_mif_irq_get(interface, SCSC_MIF_ABS_TARGET_WLAN);
	platform_mif_irq_get(interface, SCSC_MIF_ABS_TARGET_WPAN);
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
	interface->wlbt_irqdump = platform_mif_irqdump;
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	interface->irq_pmu_bit_mask = platform_mif_irq_pmu_bit_mask;
        interface->irq_pmu_bit_unmask = platform_mif_irq_pmu_bit_unmask;
#endif
}
