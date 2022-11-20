#ifndef __TFT_COMMON_M33_PANEL_H__
#define __TFT_COMMON_M33_PANEL_H__

#include "nt36672c_m33_00_panel.h"
#include "nt36672c_m33_01_panel.h"

static inline void register_common_panel_list(void)
{
	register_common_panel(&nt36672c_m33x_00_panel_info);
	register_common_panel(&nt36672c_m33x_01_panel_info);
};

static inline void unregister_common_panel_list(void)
{
	deregister_common_panel(&nt36672c_m33x_00_panel_info);
	deregister_common_panel(&nt36672c_m33x_01_panel_info);
};

#endif /* __TFT_COMMON_M33_PANEL_H__ */
