#include "platform_mif.h"

#define REGMAP_BASE_LIST_SIZE 5
#ifdef CONFIG_SCSC_I3C
#define REGMAP_SCSC_I3C 1
#else
#define REGMAP_SCSC_I3C 0
#endif

#define REGMAP_LIST_SIZE REGMAP_BASE_LIST_SIZE + REGMAP_SCSC_I3C
#define REGMAP_LIST_LENGTH 40

enum regmap_name {
	PMUREG,
	DBUS_BAAW,
	BOOT_CFG,
	PBUS_BAAW,
	WLBT_REMAP,
#ifdef CONFIG_SCSC_I3C
	I3C_APM_PMIC,
#endif
};

struct regmap *platform_mif_get_regmap(
        struct platform_mif *platform,
        enum regmap_name _regmap_name);
int platform_mif_set_regmap(struct platform_mif *platform);
int platform_mif_set_dbus_baaw(struct platform_mif *platform);
int platform_mif_set_pbus_baaw(struct platform_mif *platform);
void platform_wlbt_regdump(struct scsc_mif_abs *interface);

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
void platform_mif_set_memlog_baaw(struct platform_mif *platform);
#endif

