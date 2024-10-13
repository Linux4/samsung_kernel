#ifndef _MACH_SC_CPUIDLE_H
#define _MACH_SC_CPUIDLE_H

/* cpuidle events */
#define SC_CPUIDLE_PREPARE    0x01 /* Going to light sleep status */

extern int register_sc_cpuidle_notifier(struct notifier_block *nb);
extern int unregister_sc_cpuidle_notifier(struct notifier_block *nb);

#endif
