/* Melfas MMS-100 seies firmware list */


#if defined(CONFIG_MACH_JASPER2)
#define FW_VERSION 0x2
#include "jasper2_fw.h"

#elif defined(CONFIG_MACH_STUNNER)
#define SEC_TKEY_FACTORY_TEST
#define FW_VERSION 0x20
#include "stunner_fw.h"

#elif defined(CONFIG_MACH_INGRAHAM2)
#define SEC_TKEY_FACTORY_TEST
#define FW_VERSION 0x1
#include "ingraham2_fw.h"

#else
#define FW_VERSION 0x0
const size_t MELFAS_binary_nLength = 0x00;
const  u8 MELFAS_binary[] = {

};
#endif

#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
extern void set_esd_enable(void);
extern void set_esd_disable(void);
#endif
