/* Melfas MMS-100 seies firmware list */

#if defined(CONFIG_TOUCHSCREEN_MMS136) || \
	defined(CONFIG_TOUCHSCREEN_MMS136_TABLET)

#if defined(CONFIG_MACH_JASPER)
#define SEC_TKEY_FACTORY_TEST
#define FW_VERSION 0x23
#include "jasper_fw.h"
#elif defined(CONFIG_MACH_APEXQ)
#define FW_VERSION 0x1
#include "apexq_fw.h"
#elif defined(CONFIG_MACH_INFINITE)
#define SEC_TKEY_FACTORY_TEST
#define FW_VERSION 0x1
#include "infinite_fw.h"
#elif defined(CONFIG_MACH_GOGH)
#define FW_VERSION 0x11
#include "gogh_fw.h"
#elif defined(CONFIG_MACH_ESPRESSO_ATT) || defined(CONFIG_MACH_ESPRESSO_VZW) \
	|| defined(CONFIG_MACH_ESPRESSO_SPR)
#define FW_VERSION 0x16
#include "espresso_fw.h"
#else
#define FW_VERSION 0x0
const size_t MELFAS_binary_nLength = 0x00;
const  u8 MELFAS_binary[] = {

};
#endif

#elif defined(CONFIG_TOUCHSCREEN_MMS144)

#if defined(CONFIG_MACH_M2_VZW) || defined(CONFIG_MACH_M2_ATT)\
	|| defined(CONFIG_MACH_M2_SPR) || defined(CONFIG_MACH_M2_SKT)\
	|| defined(CONFIG_MACH_M2_DCM) || defined(CONFIG_MACH_STRETTO)\
	|| defined(CONFIG_MACH_SUPERIORLTE_SKT)\
	|| defined(CONFIG_MACH_M2_KDI)
/* 4.8" OCTA LCD */
#define FW_VERSION 0xBD
#include "d2_fw.h"
/* 4.65" OCTA LCD */
#define FW_465_VERSION 0xA8
#include "d2_465_fw.h"
#else
/* 4.8" OCTA LCD */
#define FW_VERSION 0x0
const size_t MELFAS_binary_nLength = 0x00;
const  u8 MELFAS_binary[] = {

};

/* 4.65" OCTA LCD */
#define FW_465_VERSION 0x0
const size_t MELFAS_465_binary_nLength = 0x00;
const  u8 MELFAS_465_binary[] = {

};
#endif
#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
extern void set_esd_enable(void);
extern void set_esd_disable(void);
#endif

#endif
