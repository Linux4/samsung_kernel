#ifndef _ASM_GENERIC_EMERGENCY_RESTART_H
#define _ASM_GENERIC_EMERGENCY_RESTART_H

#include <asm/sec/sec_debug.h>

static inline void machine_emergency_restart(void)
{
	sec_debug_emergency_restart_handler(); //sumit
	machine_restart("panic");
}

#endif /* _ASM_GENERIC_EMERGENCY_RESTART_H */
