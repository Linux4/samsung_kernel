#ifndef __MCPS_CPU_H__
#define __MCPS_CPU_H__

#if defined(CONFIG_MCPS_MOCK)
#include "../kunit_test/mock/mock_dev.h"

static inline bool mcps_is_cpu_online(unsigned int cpu) { return mock_cpu_online(cpu); }
static inline bool mcps_is_cpu_possible(unsigned int cpu) { return mock_cpu_possible(cpu); }
#else
static inline bool mcps_is_cpu_online(unsigned int cpu) { return (cpu <= nr_cpu_ids) && cpu_online(cpu); }
static inline bool mcps_is_cpu_possible(unsigned int cpu) { return (cpu <= nr_cpu_ids) && cpu_possible(cpu); }
#endif

#endif
