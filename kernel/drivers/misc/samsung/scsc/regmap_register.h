#include <linux/kconfig.h>

#if IS_ENABLED(CONFIG_SOC_S5E5515)
#include "s5e5515/regmap_register.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8535)
#include "s5e8535/regmap_register.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8825)
#include "s5e8825/regmap_register.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8835)
#include "s5e8835/regmap_register.h"
#elif IS_ENABLED(CONFIG_SOC_S5E8845)
#include "s5e8845/regmap_register.h"
#endif
