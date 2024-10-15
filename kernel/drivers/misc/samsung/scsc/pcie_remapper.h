#include <linux/kconfig.h>
#if IS_ENABLED(CONFIG_SCSC_BB_PAEAN)
#include "pcie/s6165/pcie_remapper.h"
#elif IS_ENABLED(CONFIG_SCSC_BB_REDWOOD)
#include "pcie/s6175/pcie_remapper.h"
#endif
