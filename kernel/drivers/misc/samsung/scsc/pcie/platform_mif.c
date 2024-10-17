/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Implements interface */

#include "platform_mif.h"

#include "pcie/platform_mif_intr_handler.h"
#include "pcie/platform_mif_irq_api.h"
#include "pcie/platform_mif_memory_api.h"
#include "pcie/platform_mif_pcie_api.h"
#include "pcie/platform_mif_pm_api.h"
#include "common/platform_mif_property_api.h"
#include "platform_mif_qos_api.h"

static unsigned long sharedmem_base;
static size_t sharedmem_size;

extern void acpm_init_eint_clk_req(u32 eint_num);
extern int exynos_pcie_rc_chk_link_status(int ch_num);

extern irqreturn_t platform_mif_set_wlbt_clk_handler(int irq, void *data);
extern int platform_mif_send_event_to_fsm_wait_completion(struct platform_mif *platform, enum pcie_event_type event);
extern void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated);
extern void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem);

#ifdef CONFIG_SCSC_QOS
extern void mifqos_mif_init(struct scsc_mif_abs *interface);
extern void mifqos_parse_dts(struct device *dev, struct device_node *np);
#endif
inline void platform_int_debug(struct platform_mif *platform);

#if !defined(CONFIG_SOC_S5E9945)
static int platform_mif_set_wlbt_clk(struct platform_mif *platform)
{
	int ret;
	u32 eint_num;
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "\n");
#if defined(SCSC_SEP_VERSION)
	eint_num = 15;
#elif defined(CONFIG_SOC_S5E9925)
	eint_num = 32;
#elif defined(CONFIG_SOC_S5E9935)
	eint_num = 33;
#endif
	acpm_init_eint_clk_req(eint_num);

	platform->clk_irq = gpio_to_irq(of_get_named_gpio(platform->dev->of_node,
				"wlbt,clk_gpio", 0));
	if (platform->clk_irq <= 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "cannot get clk_gpio\n");
		return -EINVAL;
	}
	ret = devm_request_threaded_irq(platform->dev, platform->clk_irq, NULL, platform_mif_set_wlbt_clk_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, DRV_NAME, platform);

	if (IS_ERR_VALUE((unsigned long)ret)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to register Clk handler: %d. Aborting.\n", ret);
		return -EINVAL;
	}

	return ret;
}
#endif

static void platform_mif_destroy(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
}

static char *platform_mif_get_uid(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
	return "0";
}

static int platform_load_pmu_fw(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "load_pmu_fw sz %u done\n", ka_patch_len);

	pcie_load_pmu_fw(platform->pcie, ka_patch, ka_patch_len);

	return 0;
}
static struct device *platform_mif_get_mif_device(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	return platform->dev;
}

static void platform_mif_irq_clear(void)
{
	/* Implement if required */
}

static void platform_mif_dump_register(struct scsc_mif_abs *interface)
{
}

inline void platform_int_debug(struct platform_mif *platform)
{
	int i;
	int irq;
	int ret;
	bool pending, active, masked;
	int irqs[] = { PLATFORM_MIF_MBOX, PLATFORM_MIF_WDOG };
	char *irqs_name[] = { "MBOX", "WDOG" };

	for (i = 0; i < (sizeof(irqs) / sizeof(int)); i++) {
		irq = platform->wlbt_irq[irqs[i]].irq_num;

		ret = irq_get_irqchip_state(irq, IRQCHIP_STATE_PENDING, &pending);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_ACTIVE, &active);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_MASKED, &masked);
		if (!ret)
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "IRQCHIP_STATE %d(%s): pending %d, active %d, masked %d\n", irq, irqs_name[i],
					  pending, active, masked);
	}
	platform_mif_dump_register(&platform->interface);
}

static void platform_mif_cleanup(struct scsc_mif_abs *interface)
{
}

static void platform_mif_restart(struct scsc_mif_abs *interface)
{
}

void platform_mif_irq_interface_init(struct scsc_mif_abs *platform_if)
{
	platform_if->irq_bit_set = platform_mif_irq_bit_set;
	platform_if->irq_get = platform_mif_irq_get;
	platform_if->irq_bit_mask_status_get = platform_mif_irq_bit_mask_status_get;
	platform_if->irq_bit_clear = platform_mif_irq_bit_clear;
	platform_if->irq_bit_mask = platform_mif_irq_bit_mask;
	platform_if->irq_bit_unmask = platform_mif_irq_bit_unmask;
}

#ifdef CONFIG_OF_RESERVED_MEM
int __init platform_mif_wifibt_if_reserved_mem_setup(struct reserved_mem *remem)
{
	SCSC_TAG_DEBUG(PLAT_MIF, "memory reserved: mem_base=%#lx, mem_size=%zd\n", (unsigned long)remem->base,
		       (size_t)remem->size);

	sharedmem_base = remem->base;
	sharedmem_size = remem->size;
	return 0;
}
RESERVEDMEM_OF_DECLARE(wifibt_if, "exynos,wifibt_if", platform_mif_wifibt_if_reserved_mem_setup);
#endif
/*
static int platform_load_pmu_fw_flags(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len, u32 flags)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "load_pmu_fw sz %u done, flags 0x%x\n", ka_patch_len, flags);

	pcie_load_pmu_fw_flags(platform->pcie, ka_patch, ka_patch_len, flags);

	return 0;
}
*/
struct scsc_mif_abs *platform_mif_create(struct platform_device *pdev)
{
	struct scsc_mif_abs *platform_if;
	struct platform_mif *platform =
		(struct platform_mif *)devm_kzalloc(&pdev->dev, sizeof(struct platform_mif), GFP_KERNEL);
	struct pcie_mif *pcie;
	int err = 0;

	if (!platform)
		return NULL;

	/* TODO, this should be fixed and come from a mx instance!*/
	platform_mif_set_g_platform(platform);

	SCSC_TAG_INFO_DEV(PLAT_MIF, &pdev->dev, "Creating MIF platform device\n");

	platform_if = &platform->interface;

	/* initialise interface structure */
	platform_if->destroy = platform_mif_destroy;
	platform_if->get_uid = platform_mif_get_uid;
	platform_if->reset = platform_mif_reset;
	platform_if->map = platform_mif_map;

	platform_if = &platform->interface;

	/* initialise interface structure */
	platform_if->destroy = platform_mif_destroy;
	platform_if->get_uid = platform_mif_get_uid;
	platform_if->reset = platform_mif_reset;

	platform_mif_irq_api_init(platform);
	platform_mif_memory_api_init(platform_if);
	platform_mif_intr_handler_api_init(platform);
	platform_mif_irq_interface_init(platform_if);
#if IS_ENABLED(CONFIG_WLBT_PROPERTY_READ)
	platform_mif_wlbt_property_read_init(platform_if);
#endif
	platform_if->get_ramrp_ptr = platform_get_ramrp_ptr;
	platform_if->get_ramrp_buff = platform_get_ramrp_buff;
	platform_if->get_mbox_pmu = platform_mif_get_mbox_pmu;
	platform_if->set_mbox_pmu = platform_mif_set_mbox_pmu;
	platform_if->get_mbox_pmu_pcie_off = platform_mif_get_mbox_pmu_pcie_off;
	platform_if->set_mbox_pmu_pcie_off = platform_mif_set_mbox_pmu_pcie_off;
	platform_if->get_mbox_pmu_error = platform_mif_get_mbox_pmu_error;
	platform_if->get_mif_device = platform_mif_get_mif_device;
	platform_if->mif_dump_registers = platform_mif_dump_register;
	platform_if->irq_clear = platform_mif_irq_clear;
	platform_if->mif_cleanup = platform_mif_cleanup;
	platform_if->mif_restart = platform_mif_restart;
	platform_if->get_ramrp_buff = platform_get_ramrp_buff;
	platform_if->load_pmu_fw = platform_load_pmu_fw;
	//platform_if->load_pmu_fw_flags = platform_load_pmu_fw_flags;
	platform->irq_dev_pmu = NULL;
	/* Reset ka_patch pointer & size */
	platform->ka_patch_fw = NULL;
	platform->ka_patch_len = 0;
	/* Update state */
	platform->pdev = pdev;
	platform->dev = &pdev->dev;

	platform->wlan_handler = platform_mif_irq_default_handler;
	platform->wpan_handler = platform_mif_irq_default_handler;
	platform->irq_dev = NULL;
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->irq_reset_request_dev = NULL;
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;

	platform->np = pdev->dev.of_node;
	platform_if->hostif_wakeup = platform_mif_hostif_wakeup;
	platform_if->get_msi_range = platform_mif_get_msi_range;

#ifdef CONFIG_SCSC_BB_PAEAN
	platform_if->acpm_write_reg = platform_mif_acpm_write_reg; //SPMI write
	mutex_init(&platform->acpm_lock);
#elif CONFIG_SCSC_BB_REDWOOD
	platform_if->control_suspend_gpio = platform_mif_control_suspend_gpio;
#endif
	platform_if->get_scan2mem_mode = platform_mif_get_scan2mem_mode;
	platform_if->set_scan2mem_mode = platform_mif_set_scan2mem_mode;
	platform_if->get_s2m_size_octets = platform_mif_get_s2m_size_octets;
	platform_if->set_s2m_dram_offset = platform_mif_set_s2m_dram_offset;
	platform_if->get_mem_start = platform_mif_get_mem_start;

	register_pm_notifier(&platform->pm_nb);

#ifdef CONFIG_OF_RESERVED_MEM
	if (!sharedmem_base) {
		struct device_node *np;

		np = of_parse_phandle(platform->dev->of_node, "memory-region", 0);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "module build register sharedmem np %x\n", np);
		if (np) {
			platform->mem_start = of_reserved_mem_lookup(np)->base;
			platform->mem_size = of_reserved_mem_lookup(np)->size;
		}
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "built-in register sharedmem\n");
		platform->mem_start = sharedmem_base;
		platform->mem_size = sharedmem_size;
	}
#else
	/* If CONFIG_OF_RESERVED_MEM is not defined, sharedmem values should be
	 * parsed from the scsc_wifibt binding
	 */
	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-base", &sharedmem_base)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_start = sharedmem_base;

	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-size", &sharedmem_size)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_size = sharedmem_size;
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "platform->mem_start 0x%x platform->mem_size 0x%x\n",
			  (u32)platform->mem_start, (u32)platform->mem_size);
	if (platform->mem_start == 0)
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "platform->mem_start is 0");

	if (platform->mem_size == 0) {
		/* We return return if mem_size is 0 as it does not make any
		 * sense. This may be an indication of an incorrect platform
		 * device binding.
		 */
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "platform->mem_size is 0");
		err = -EINVAL;
		goto error_exit;
	}

	if (of_property_read_u32(platform->dev->of_node, "pci_ch_num", &platform->pcie_ch_num)) {
		err = -EINVAL;
		goto error_exit;
	}
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			"platform->pcie_ch_num : %d\n", platform->pcie_ch_num);

	platform->pmic_gpio = of_get_gpio(platform->dev->of_node, 0);
	if (platform->pmic_gpio < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "cannot get pmic_gpio\n");
	} else {
		if (devm_gpio_request_one(platform->dev, platform->pmic_gpio,
					GPIOF_OUT_INIT_LOW, dev_name(platform->dev))) {
			err = -EINVAL;
			goto error_exit;
		}
	}
#if defined(CONFIG_SCSC_BB_REDWOOD)
	platform->reset_gpio = of_get_named_gpio(platform->dev->of_node, "wlbt,reset_gpio", 0);
	platform->suspend_gpio = of_get_named_gpio(platform->dev->of_node, "wlbt,suspend_gpio", 0);
#endif
	platform->gpio_num = of_get_named_gpio(platform->dev->of_node, "wlbt,wakeup_gpio", 0);
	platform->wakeup_irq = gpio_to_irq(of_get_named_gpio(platform->dev->of_node,
				"wlbt,wakeup_gpio", 0));

	if (platform->wakeup_irq <= 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "cannot get wakeup_gpio\n");
		err = -EINVAL;
		goto error_exit;
	}

#if !defined(CONFIG_SOC_S5E9945)
	err = platform_mif_set_wlbt_clk(platform);
	if (err) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "cannot set wlbt clk");
		goto error_exit;
	}
#endif

#ifdef CONFIG_SCSC_QOS
	platform_mif_qos_init(platform);
#endif
	/* Initialize spinlock */
	spin_lock_init(&platform->mif_spinlock);

	pcie_init();
	/* get the singleton instance of pcie */
	pcie = pcie_get_instance();
	if (!pcie) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev, "Getting PCIE instance\n");
		return NULL;
	}

	pcie_set_mem_range(pcie, platform->mem_start, platform->mem_size);

	pcie_set_ch_num(pcie, platform->pcie_ch_num);

	platform->pcie = pcie;
	pcie_irq_reg_pmu_handler(pcie, platform_mif_pmu_isr, (void *)platform);
	pcie_irq_reg_pmu_pcie_off_handler(pcie, platform_mif_pmu_pcie_off_isr, (void *)platform);
	pcie_irq_reg_pmu_error_handler(pcie, platform_mif_pmu_error_isr, (void *)platform);
	pcie_irq_reg_wlan_handler(pcie, platform_mif_wlan_isr, (void *)platform);
	pcie_irq_reg_wpan_handler(pcie, platform_mif_wpan_isr, (void *)platform);
	platform_mif_set_scan2mem_mode(platform_if, false);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	wake_lock_init(NULL, &(platform->wakehash_wakelock.ws), "wakehash_wakelock");
#else
	wake_lock_init(&platform->wakehash_wakelock, WAKE_LOCK_SUSPEND, "wakehash_wakelock");
#endif
	spin_lock_init(&platform->kfifo_lock);
	spin_lock_init(&platform->cb_sync);
	init_completion(&platform->pcie_on);
	init_waitqueue_head(&platform->event_wait_queue);

	err = kfifo_alloc(&platform->ev_fifo, 256 * sizeof(struct event_record), GFP_KERNEL);
	if (err)
		goto error_exit;

	platform->t = kthread_run(platform_mif_pcie_control_fsm, platform, "pcie_control_fsm");
	if (IS_ERR(platform->t)) {
		err = PTR_ERR(platform->t);
		kfifo_free(&platform->ev_fifo);
		SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev, "%s: Failed to start pcie_control_fsm (%d)\n",
			__func__, err);
		goto error_exit;
	}

	return platform_if;

error_exit:
	devm_kfree(&pdev->dev, platform);
	return NULL;
}

void platform_mif_destroy_platform(struct platform_device *pdev, struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	wake_lock_destroy(&platform->wakehash_wakelock);
	unregister_pm_notifier(&platform->pm_nb);
	kthread_stop(platform->t);
	platform->t = NULL;

	kfifo_free(&platform->ev_fifo);
	devm_kfree(&pdev->dev, platform);

	pcie_deinit();
}
struct platform_device *platform_mif_get_platform_dev(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	BUG_ON(!interface || !platform);

	return platform->pdev;
}

void platform_mif_complete(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Complete Resume\n");

	kthread_unpark(platform->t);
}


int wlbt_rc_pm_notifier(struct notifier_block *nb, unsigned long mode, void *_unused)
{
	struct platform_mif *platform = container_of(nb, struct platform_mif, pm_nb);

	switch (mode) {
	case PM_SUSPEND_PREPARE:
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "kthread_park\n");
		kthread_park(platform->t);
		break;
	case PM_POST_SUSPEND:
		kthread_unpark(platform->t);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "kthread_unpark\n");
		if (platform->resume_handler)
			platform->resume_handler(&platform->interface, platform->suspendresume_data);
		break;
	default:
		break;
	}

	return 0;
}
