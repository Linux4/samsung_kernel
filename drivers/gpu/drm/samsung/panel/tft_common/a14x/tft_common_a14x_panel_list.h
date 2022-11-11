#ifndef __TFT_COMMON_A14_PANEL_H__
#define __TFT_COMMON_A14_PANEL_H__

#include "nt36672c_a14x_00_panel.h"
#include "td4160_a14x_00_panel.h"

static inline void register_common_panel_list(void)
{
	register_common_panel(&nt36672c_a14x_00_panel_info);
	register_common_panel(&td4160_a14x_00_panel_info);
};

static inline void unregister_common_panel_list(void)
{
	deregister_common_panel(&nt36672c_a14x_00_panel_info);
	deregister_common_panel(&td4160_a14x_00_panel_info);
};

#endif
