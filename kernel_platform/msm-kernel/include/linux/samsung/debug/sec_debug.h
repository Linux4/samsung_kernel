#ifndef __SEC_DEBUG_H__
#define __SEC_DEBUG_H__

#include <dt-bindings/samsung/debug/sec_debug.h>

#define SEC_DEBUG_CP_DEBUG_ON		0x5500
#define SEC_DEBUG_CP_DEBUG_OFF		0x55ff

#if IS_ENABLED(CONFIG_SEC_DEBUG)
extern unsigned int sec_debug_level(void);
extern bool sec_debug_is_enabled(void);
extern phys_addr_t sec_debug_get_dump_sink_phys(void);
#else
static inline unsigned int sec_debug_level(void) { return SEC_DEBUG_LEVEL_LOW; }
static inline bool sec_debug_is_enabled(void) { return false; }
static inline phys_addr_t sec_debug_get_dump_sink_phys(void) { return 0; }
#endif

#endif	/* __SEC_DEBUG_H__ */
