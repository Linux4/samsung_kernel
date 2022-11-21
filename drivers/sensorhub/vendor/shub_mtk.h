#define SENSORHUB_VENDOR "MTK"
#define SENSORHUB_NAME   "MT6769T"

#include "../../misc/mediatek/scp/mt6768/scp_helper.h"
#include "../../misc/mediatek/scp/mt6768/scp_ipi.h"

int sensorhub_probe(void);
int sensorhub_shutdown(void);
int sensorhub_refresh_func(void);
