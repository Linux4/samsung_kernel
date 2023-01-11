#ifndef __ASM_MACH_CPUTYPE_H
#define __ASM_MACH_CPUTYPE_H

#include <asm/cputype.h>
#include <linux/of.h>

static inline bool cpu_is_ca9(void)
{
	if ((read_cpuid_id() & 0xfff0) == 0xc090)
		return true;

	return false;
}

static inline bool cpu_is_ca7(void)
{
	if ((read_cpuid_id() & 0xfff0) == 0xc070)
		return true;

	return false;
}

/*
 *  CPU   Stepping   CPU_ID      CHIP_ID
 *
 * PXA168    S0    0x56158400   0x0000C910
 * PXA168    A0    0x56158400   0x00A0A168
 * PXA910    Y1    0x56158400   0x00F2C920
 * PXA910    A0    0x56158400   0x00F2C910
 * PXA910    A1    0x56158400   0x00A0C910
 * PXA920    Y0    0x56158400   0x00F2C920
 * PXA920    A0    0x56158400   0x00A0C920
 * PXA920    A1    0x56158400   0x00A1C920
 * MMP2	     Z0	   0x560f5811   0x00F00410
 * MMP2      Z1    0x560f5811   0x00E00410
 * MMP2      A0    0x560f5811   0x00A0A610
 */

extern unsigned int mmp_chip_id;

#ifdef CONFIG_CPU_PXA168
static inline int cpu_is_pxa168(void)
{
	return (((read_cpuid_id() >> 8) & 0xff) == 0x84) &&
	    ((mmp_chip_id & 0xfff) == 0x168);
}
#else
#define cpu_is_pxa168()	(0)
#endif

/* cpu_is_pxa910() is shared on both pxa910 and pxa920 */
#ifdef CONFIG_CPU_PXA910
static inline int cpu_is_pxa910(void)
{
	return (((read_cpuid_id() >> 8) & 0xff) == 0x84) &&
	    (((mmp_chip_id & 0xfff) == 0x910) ||
	     ((mmp_chip_id & 0xfff) == 0x920));
}
#else
#define cpu_is_pxa910()	(0)
#endif

#ifdef CONFIG_CPU_MMP2
static inline int cpu_is_mmp2(void)
{
	return ((read_cpuid_id() >> 8) & 0xff) == 0x58;
}
#else
#define cpu_is_mmp2()	(0)
#endif

static inline int cpu_is_pxa1U88(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc07) &&
	    (((mmp_chip_id & 0xffff) == 0x1098));
}

#ifdef CONFIG_CPU_PXA988
static inline int cpu_is_pxa1L88(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc07) &&
	    (((mmp_chip_id & 0xffff) == 0x1188));
}

static inline int cpu_is_pxa1L88_a0(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc07) &&
	    (((mmp_chip_id & 0xffffff) == 0xa01188));
}

static inline int cpu_is_pxa1L88_a0c(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc07) &&
	    (((mmp_chip_id & 0xffffff) == 0xb01188));
}
#else
#define cpu_is_pxa1L88()	(0)
#define cpu_is_pxa1L88_a0()	(0)
#define cpu_is_pxa1L88_a0c()	(0)
#endif

static inline int cpu_is_pxa1908(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xd03) &&
	    (((mmp_chip_id & 0xffff) == 0x1908));
}

static inline int cpu_is_pxa1936(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xd03) &&
	    (((mmp_chip_id & 0xffff) == 0x1936));
}

static inline int cpu_is_pxa1928_b0(void)
{
	return (mmp_chip_id & 0xffffff) == 0xb0c198;
}

static inline int cpu_is_pxa1928_a0(void)
{
	return (mmp_chip_id & 0xffffff) == 0xa0c198;
}

static inline int cpu_is_pxa1928(void)
{
	return cpu_is_pxa1928_b0() || cpu_is_pxa1928_a0();
}

static inline int pxa1928_is_a0(void)
{
	struct device_node *np;
	const char *str = NULL;
	static int is_a0 = -1;

	if (is_a0 != -1)
		return is_a0;

	np = of_find_node_by_name(NULL, "chip_version");
	if (!np)
		is_a0 = 0;

	if (np && !of_property_read_string(np, "version", &str)) {
		if (!strcmp(str, "a0"))
			is_a0 = 1;
		else
			is_a0 = 0;
	}

	return is_a0;
}

#endif /* __ASM_MACH_CPUTYPE_H */
