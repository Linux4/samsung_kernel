#include <linux/version.h>

#define SENSORHUB_VENDOR "MTK"
#define SENSORHUB_NAME   "MT6769T"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
#include "../../misc/mediatek/scp/mt6768/scp_helper.h"
#include "../../misc/mediatek/scp/mt6768/scp_ipi.h"
#include "../../misc/mediatek/scp/mt6768/scp_excep.h"
#else
#include "./../misc/mediatek/scp/include/scp.h"
#endif

int sensorhub_probe(void);
int sensorhub_shutdown(void);
int sensorhub_refresh_func(void);
