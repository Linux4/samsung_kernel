#include <linux/version.h>

#define SENSORHUB_VENDOR "MTK"
#define SENSORHUB_NAME   "MT6989"

#include "scp.h"

int sensorhub_probe(void);
int sensorhub_shutdown(void);
int sensorhub_refresh_func(void);

