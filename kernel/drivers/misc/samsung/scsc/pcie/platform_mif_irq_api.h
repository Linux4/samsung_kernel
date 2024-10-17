#include "../platform_mif.h"
#include "../platform_mif_intr_handler.h"

u32 platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target);
u32 platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target);
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

void platform_mif_irq_api_init(struct platform_mif *platform);