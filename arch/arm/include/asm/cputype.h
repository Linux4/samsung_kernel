#ifndef __ASM_ARM_CPUTYPE_H
#define __ASM_ARM_CPUTYPE_H

#include <linux/stringify.h>
#include <linux/kernel.h>

#define CPUID_ID	0
#define CPUID_CACHETYPE	1
#define CPUID_TCM	2
#define CPUID_TLBTYPE	3
#define CPUID_MPIDR	5

#define CPUID_EXT_PFR0	"c1, 0"
#define CPUID_EXT_PFR1	"c1, 1"
#define CPUID_EXT_DFR0	"c1, 2"
#define CPUID_EXT_AFR0	"c1, 3"
#define CPUID_EXT_MMFR0	"c1, 4"
#define CPUID_EXT_MMFR1	"c1, 5"
#define CPUID_EXT_MMFR2	"c1, 6"
#define CPUID_EXT_MMFR3	"c1, 7"
#define CPUID_EXT_ISAR0	"c2, 0"
#define CPUID_EXT_ISAR1	"c2, 1"
#define CPUID_EXT_ISAR2	"c2, 2"
#define CPUID_EXT_ISAR3	"c2, 3"
#define CPUID_EXT_ISAR4	"c2, 4"
#define CPUID_EXT_ISAR5	"c2, 5"

#define MPIDR_SMP_BITMASK (0x3 << 30)
#define MPIDR_SMP_VALUE (0x2 << 30)

#define MPIDR_MT_BITMASK (0x1 << 24)

#define MPIDR_HWID_BITMASK 0xFFFFFF

#define MPIDR_INVALID (~MPIDR_HWID_BITMASK)

#define MPIDR_LEVEL0_MASK 0x3
#define MPIDR_LEVEL0_SHIFT 0

#define MPIDR_LEVEL1_MASK 0xF
#define MPIDR_LEVEL1_SHIFT 8

#define MPIDR_LEVEL2_MASK 0xFF
#define MPIDR_LEVEL2_SHIFT 16

#define MPIDR_LEVEL_BITS 8
#define MPIDR_LEVEL_MASK ((1 << MPIDR_LEVEL_BITS) - 1)

#define MPIDR_AFFINITY_LEVEL(mpidr, level) \
	((mpidr >> (MPIDR_LEVEL_BITS * level)) & MPIDR_LEVEL_MASK)

extern unsigned int processor_id;

#ifdef CONFIG_CPU_CP15
#define read_cpuid(reg)							\
	({								\
		unsigned int __val;					\
		asm("mrc	p15, 0, %0, c0, c0, " __stringify(reg)	\
		    : "=r" (__val)					\
		    :							\
		    : "cc");						\
		__val;							\
	})
#define read_cpuid_ext(ext_reg)						\
	({								\
		unsigned int __val;					\
		asm("mrc	p15, 0, %0, c0, " ext_reg		\
		    : "=r" (__val)					\
		    :							\
		    : "cc");						\
		__val;							\
	})
#else
#define read_cpuid(reg) (processor_id)
#define read_cpuid_ext(reg) 0
#endif

#if defined(CONFIG_EXYNOS5_MP) || defined(CONFIG_BL_SWITCHER)
static inline unsigned int read_cpuid_id(void)
{
	return read_cpuid(CPUID_ID);
}

static inline unsigned int read_cpuid_cachetype(void)
{
	return read_cpuid(CPUID_CACHETYPE);
}

static inline unsigned int read_cpuid_tcmstatus(void)
{
	return read_cpuid(CPUID_TCM);
}

static inline unsigned int read_cpuid_mpidr(void)
{
	return read_cpuid(CPUID_MPIDR);
}

#define CPU_NUM_IN_CLUSTER 4
#define CLUSTER_NUM 2
#include <mach/regs-pmu.h>
#include <asm/io.h>

static inline unsigned int cpuid_to_hw(int cpuid)
{
	return (cpuid + CPU_NUM_IN_CLUSTER) %
		(CPU_NUM_IN_CLUSTER * CLUSTER_NUM);
}

static inline unsigned int is_cpu_on(int cpuid)
{
	return (__raw_readl(EXYNOS_ARM_CORE_STATUS
			(cpuid_to_hw(cpuid))) & 0xf) ? 1 : 0;
}

static inline unsigned int get_clusterid(int cpuid)
{
	return (cpuid_to_hw(cpuid) < CPU_NUM_IN_CLUSTER) ? 0 : 1;
}

static inline unsigned int get_first_cpuid(int clusterid)
{
	return (clusterid == 0) ? CPU_NUM_IN_CLUSTER : 0;
}
#else
/*
 * The CPU ID never changes at run time, so we might as well tell the
 * compiler that it's constant.  Use this function to read the CPU ID
 * rather than directly reading processor_id or read_cpuid() directly.
 */
static inline unsigned int __attribute_const__ read_cpuid_id(void)
{
	return read_cpuid(CPUID_ID);
}

static inline unsigned int __attribute_const__ read_cpuid_cachetype(void)
{
	return read_cpuid(CPUID_CACHETYPE);
}

static inline unsigned int __attribute_const__ read_cpuid_tcmstatus(void)
{
	return read_cpuid(CPUID_TCM);
}

static inline unsigned int __attribute_const__ read_cpuid_mpidr(void)
{
	return read_cpuid(CPUID_MPIDR);
}
#endif

/*
 * Intel's XScale3 core supports some v6 features (supersections, L2)
 * but advertises itself as v5 as it does not support the v6 ISA.  For
 * this reason, we need a way to explicitly test for this type of CPU.
 */
#ifndef CONFIG_CPU_XSC3
#define cpu_is_xsc3()	0
#else
static inline int cpu_is_xsc3(void)
{
	unsigned int id;
	id = read_cpuid_id() & 0xffffe000;
	/* It covers both Intel ID and Marvell ID */
	if ((id == 0x69056000) || (id == 0x56056000))
		return 1;

	return 0;
}
#endif

#if !defined(CONFIG_CPU_XSCALE) && !defined(CONFIG_CPU_XSC3)
#define	cpu_is_xscale()	0
#else
#define	cpu_is_xscale()	1
#endif

#endif
