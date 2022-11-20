#ifndef __PANEL_INFO_H__
#define __PANEL_INFO_H__

#if defined(CONFIG_EXYNOS3475_DECON_LCD_S6D78A)
#include "s6d78a_param.h"
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_SC7798D)
#include "sc7798d_param.h"
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_SC7798D_J2)
#include "sc7798d_param_j2.h"
#include "s6e8aa0_param_j2.h"
extern struct dsim_panel_ops S6E88A0_panel_ops;
#elif defined(CONFIG_PANEL_S6D7AA0_DYNAMIC)
#include "s6d7aa0_param.h"
#else
#error "ERROR !! Check LCD Panel Header File"
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#endif
#endif
