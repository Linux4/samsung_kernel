#include "platform_mif.h"

inline int platform_mif_update_users(enum pcie_event_type event, struct platform_mif *platform);
void platform_mif_pcie_control_fsm_prepare_fw(struct platform_mif *platform);
int platform_mif_pcie_control_fsm(void *data);
int __platform_mif_send_event_to_fsm(struct platform_mif *platform, int event, bool comp);
int platform_mif_send_event_to_fsm(struct platform_mif *platform, enum pcie_event_type event);
int platform_mif_send_event_to_fsm_wait_completion(struct platform_mif *platform, enum pcie_event_type event);
int platform_mif_hostif_wakeup(struct scsc_mif_abs *interface, int (*cb)(void *first, void *second), void *service, void *dev);
void platform_mif_get_msi_range(struct scsc_mif_abs *interface, u8 *start, u8 *end, enum scsc_mif_abs_target target);
__iomem void *platform_get_ramrp_ptr(struct scsc_mif_abs *interface);
int platform_get_scoreboard_ref(struct scsc_mif_abs *interface, __iomem void *addr, uintptr_t *addr_ref);
int scsc_pcie_complete(void);
void platform_mif_set_g_platform(struct platform_mif* platform);
int platform_get_ramrp_buff(struct scsc_mif_abs *interface, void** buff, int count, u64 offset);

