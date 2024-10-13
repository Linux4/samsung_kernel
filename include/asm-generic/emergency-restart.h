#ifndef _ASM_GENERIC_EMERGENCY_RESTART_H
#define _ASM_GENERIC_EMERGENCY_RESTART_H


#if defined(CONFIG_SEC_DEBUG)
/* for saving context when emergency restart is called */
extern void sec_debug_emergency_restart_handler(void);
#endif

static inline void machine_emergency_restart(void)
{
#if defined(CONFIG_SEC_DEBUG)
        sec_debug_emergency_restart_handler(); //sumit 
#endif
	machine_restart("panic");
}

#endif /* _ASM_GENERIC_EMERGENCY_RESTART_H */
