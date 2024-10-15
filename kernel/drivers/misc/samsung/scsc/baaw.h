#include <linux/kconfig.h>

#if IS_ENABLED(CONFIG_SOC_S5E5515)
#include "modap/s5e5515/baaw.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8535)
#include "modap/s5e8535/baaw.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8825)
#include "modap/s5e8825/baaw.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8835)
#include "modap/s5e8835/baaw.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8845)
#include "modap/s5e8845/baaw.h"
#elif IS_ENABLED(CONFIG_SOC_S5E5535)
#include "modap/s5e5535/baaw.h"
#endif
