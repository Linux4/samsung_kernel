#ifndef __EXYNOS7570_H__
#define __EXYNOS7570_H__

#include "S5E7570-sfrbase.h"

#define CPUCL0_EMA_CON		((void *)(SYSREG_CPUCL0_BASE + 0x0330))
#define CPUCL0_EMA		((void *)(SYSREG_CPUCL0_BASE + 0x0340))
#define G3D_EMA_RA1_HS_CON	((void *)(SYSREG_G3D_BASE + 0x0304))
#define G3D_EMA_RF1_HS_CON	((void *)(SYSREG_G3D_BASE + 0x0314))
#define G3D_EMA_RF2_HS_CON	((void *)(SYSREG_G3D_BASE + 0x031C))
#define G3D_EMA_UHD_CON		((void *)(SYSREG_G3D_BASE + 0x0320))

#endif
