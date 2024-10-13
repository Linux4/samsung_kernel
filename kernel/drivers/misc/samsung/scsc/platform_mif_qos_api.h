#include <linux/kconfig.h>
#if IS_ENABLED(CONFIG_SOC_S5E5515)
#include "s5e5515/platform_mif_qos_api.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8535)
#include "s5e8535/platform_mif_qos_api.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8825)
#include "s5e8825/platform_mif_qos_api.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8835)
#include "s5e8835/platform_mif_qos_api.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8845)
#include "s5e8845/platform_mif_qos_api.h"
#elif IS_ENABLED(CONFIG_SOC_S5E9945)
#include "s6165/platform_mif_qos_api.h"
#endif
