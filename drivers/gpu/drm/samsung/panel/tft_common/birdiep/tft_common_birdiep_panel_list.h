#ifndef __TFT_COMMON_BIRDIEP_PANEL_H__
#define __TFT_COMMON_BIRDIEP_PANEL_H__

#include "ft8203_birdiep_00_panel.h"

static inline void register_common_panel_list(void)
{
	register_common_panel(&ft8203_birdiep_00_panel_info);
};

static inline void unregister_common_panel_list(void)
{
	deregister_common_panel(&ft8203_birdiep_00_panel_info);
};

#endif
