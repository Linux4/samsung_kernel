#include <linux/version.h>

#define SENSORHUB_VENDOR "MTK"
#define SENSORHUB_NAME   "MT6765"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
#include "../../misc/mediatek/scp/include/scp.h"
#else
#include "../../misc/mediatek/scp/mt6765/scp_helper.h"
#include "../../misc/mediatek/scp/mt6765/scp_ipi.h"
#endif

int sensorhub_probe(void);
int sensorhub_shutdown(void);
int sensorhub_refresh_func(void);
