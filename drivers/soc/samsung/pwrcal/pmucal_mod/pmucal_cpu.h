#ifndef __PMUCAL_CPU_H__
#define __PMUCAL_CPU_H__
#include "pmucal_common.h"

struct pmucal_cpu {
	u32 id;
	struct pmucal_seq *on;
	struct pmucal_seq *off;
	struct pmucal_seq *status;
	u32 num_on;
	u32 num_off;
	u32 num_status;
};

/* Chip-independent enumeration for CPU core.
 * All cores should be described in here.
 */
enum pmucal_cpu_corenum {
	CPU_CORE0 = 0,
	CPU_CORE1,
	CPU_CORE2,
	CPU_CORE3,
	CPU_CORE4,
	CPU_CORE5,
	CPU_CORE6,
	CPU_CORE7,
	PMUCAL_NUM_CORES,
};

/* Chip-independent enumeration for CPU cluster
 * All clusters should be described in here.
 */
enum pmucal_cpu_clusternum {
	CPU_CLUSTER0 = 0,
	CPU_CLUSTER1,
	PMUCAL_NUM_CLUSTERS,
};

/* APIs to be supported to PWRCAL interface */
extern int pmucal_cpu_enable(unsigned int cpu);
extern int pmucal_cpu_disable(unsigned int cpu);
extern int pmucal_cpu_is_enabled(unsigned int cpu);
extern int pmucal_cpu_cluster_enable(unsigned int cluster);
extern int pmucal_cpu_cluster_disable(unsigned int cluster);
extern int pmucal_cpu_cluster_is_enabled(unsigned int cluster);
extern int pmucal_cpu_init(void);
#endif
