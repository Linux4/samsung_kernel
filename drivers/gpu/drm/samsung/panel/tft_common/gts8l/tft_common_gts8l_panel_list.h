#ifndef __TFT_COMMON_GTS8L_PANEL_H__
#define __TFT_COMMON_GTS8L_PANEL_H__

#include "hx83102j_gts8l_00_panel.h"

static inline void register_common_panel_list(void)
{
	register_common_panel(&hx83102j_gts8l_00_panel_info);
};

static inline void unregister_common_panel_list(void)
{
	deregister_common_panel(&hx83102j_gts8l_00_panel_info);
};

#endif /* __TFT_COMMON_GTS8L_PANEL_H__ */
