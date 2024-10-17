#include "../platform_mif.h"

int platform_mif_reset(struct scsc_mif_abs *interface, bool reset);
int platform_mif_suspend(struct scsc_mif_abs *interface);
void platform_mif_resume(struct scsc_mif_abs *interface);
int platform_mif_acpm_write_reg(struct platform_mif *platform, u8 reg, u8 value);
bool platform_mif_get_scan2mem_mode(struct scsc_mif_abs *interface);
void platform_mif_set_scan2mem_mode(struct scsc_mif_abs *interface, bool enable);
void platform_mif_set_s2m_dram_offset(struct scsc_mif_abs *interface, u32 offset);
u32 platform_mif_get_s2m_size_octets(struct scsc_mif_abs *interface);
unsigned long platform_mif_get_mem_start(struct scsc_mif_abs *interface);
#ifdef CONFIG_SCSC_BB_REDWOOD
void platform_mif_control_suspend_gpio(struct scsc_mif_abs *interface, u8 value);
#endif