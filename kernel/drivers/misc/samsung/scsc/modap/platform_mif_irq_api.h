#ifndef __PLATFORM_MIF_IRQ_API_H__
#define __PLATFORM_MIF_IRQ_API_H__

#include "platform_mif.h"
#include "platform_mif_intr_handler.h"

inline void platform_mif_reg_write(struct platform_mif *platform, u16 offset, u32 value);
inline u32 platform_mif_reg_read(struct platform_mif *platform, u16 offset);
inline void platform_mif_reg_write_wpan(struct platform_mif *platform, u16 offset, u32 value);
inline u32 platform_mif_reg_read_wpan(struct platform_mif *platform, u16 offset);
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
inline void platform_mif_reg_write_pmu(struct platform_mif *platform, u16 offset, u32 value);
inline u32 platform_mif_reg_read_pmu(struct platform_mif *platform, u16 offset);
#endif

u32 platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target);
u32 platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target);
u32 __platform_mif_irq_bit_mask_read(struct platform_mif *platform);
u32 __platform_mif_irq_bit_mask_read_wpan(struct platform_mif *platform);
void __platform_mif_irq_bit_mask_write(struct platform_mif *platform, u32 val);
void __platform_mif_irq_bit_mask_write_wpan(struct platform_mif *platform, u32 val);
void platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void platform_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void platform_mif_irq_unreg_handler(struct scsc_mif_abs *interface);
void platform_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void platform_mif_irq_unreg_handler_wpan(struct scsc_mif_abs *interface);
void platform_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void platform_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface);
void platform_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void platform_mif_irq_reg_pmu_error_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);

void platform_mif_unregister_irq(struct platform_mif *platform);
int platform_mif_register_irq(struct platform_mif *platform);
void platform_cfg_req_irq_clean_pending(struct platform_mif *platform);

irqreturn_t platform_mif_isr(int irq, void *data);
irqreturn_t platform_mif_isr_wpan(int irq, void *data);
irqreturn_t platform_wdog_isr(int irq, void *data);
#ifdef CONFIG_SOC_S5E5535
irqreturn_t platform_alive_isr(int irq, void *data);
#endif

void platform_mif_irq_api_init(struct platform_mif *platform);
int platform_mif_irq_get_ioresource_mem(
	struct platform_device *pdev,
	struct platform_mif *platform);
int platform_mif_irq_get_ioresource_irq(
	struct platform_device *pdev,
	struct platform_mif *platform);

#endif
