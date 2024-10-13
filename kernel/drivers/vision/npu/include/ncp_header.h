
/* Get NCP_VERSION value from platform specific configuration */
#if (CONFIG_NPU_NCP_VERSION == 26)
#include "ncp_header_v26.h"
#elif (CONFIG_NPU_NCP_VERSION == 27)
#include "ncp_header_v27.h"
#else
#error NCP_VERSION is not defined or not supported
#endif
