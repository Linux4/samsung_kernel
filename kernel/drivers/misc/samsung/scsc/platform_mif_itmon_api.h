#include <linux/kconfig.h>
#if IS_ENABLED(CONFIG_SOC_S5E5515)
#include "modap/s5e5515/platform_mif_itmon_api.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8535)
#include "modap/s5e8535/platform_mif_itmon_api.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8825)
#include "modap/s5e8825/platform_mif_itmon_api.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8835)
#include "modap/s5e8835/platform_mif_itmon_api.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8845)
#include "modap/s5e8845/platform_mif_itmon_api.h"
#elif IS_ENABLED(CONFIG_SOC_S5E5535)
#include "modap/s5e5535/platform_mif_itmon_api.h"
#endif
