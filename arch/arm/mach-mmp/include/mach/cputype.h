#ifndef __ASM_MACH_CPUTYPE_H
#define __ASM_MACH_CPUTYPE_H

#include <asm/cputype.h>

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
 * MMP3	     A0    0x562f5840   0x00A0A620
 * MMP3	     B0    0x562f5840   0x00B0A620
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

#ifdef CONFIG_CPU_PXA988
static inline int cpu_is_pxa988(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09) &&
		(((mmp_chip_id & 0xffff) == 0xc988) ||
		((mmp_chip_id & 0xffff) == 0xc928));
}
static inline int cpu_is_pxa988_z1(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09) &&
		((mmp_chip_id & 0xffffff) == 0xf0c928);
}
static inline int cpu_is_pxa988_z2(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09) &&
		((mmp_chip_id & 0xffffff) == 0xf1c988);
}
static inline int cpu_is_pxa988_z3(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09) &&
		((mmp_chip_id & 0xffffff) == 0xf2c988);
}
static inline int cpu_is_pxa988_a0(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09) &&
		((mmp_chip_id & 0xffffff) == 0xa0c928);
}
static inline int cpu_is_pxa986(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09) &&
		(((mmp_chip_id & 0xffff) == 0xc986) ||
		((mmp_chip_id & 0xffff) == 0xc926));
}
static inline int cpu_is_pxa986_z1(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09) &&
		((mmp_chip_id & 0xffffff) == 0xf0c926);
}
static inline int cpu_is_pxa986_z2(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09) &&
		((mmp_chip_id & 0xffffff) == 0xf1c986);
}
static inline int cpu_is_pxa986_z3(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09) &&
		((mmp_chip_id & 0xffffff) == 0xf2c986);
}
static inline int cpu_is_pxa986_a0(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09) &&
		((mmp_chip_id & 0xffffff) == 0xa0c926);
}

static inline int cpu_is_pxa1088(void)
{
	return 0;
}

static inline int cpu_is_pxa1L88(void)
{
	return 0;
}

#define PXA98X_Z1	0x00
#define PXA98X_Z2	0x01
#define PXA98X_Z3	0x02
#define PXA98X_A0	0xA0

static inline int cpu_pxa98x_stepping(void)
{
	if ((mmp_chip_id & 0xf00000) == 0xf00000)
		return (mmp_chip_id & 0xf0000) >> 16;

	return (mmp_chip_id & 0xff0000) >> 16;
}
#else
#define cpu_is_pxa988()	(0)
#define cpu_is_pxa988_z1()	(0)
#define cpu_is_pxa988_z2()	(0)
#define cpu_is_pxa988_z3()	(0)
#define cpu_is_pxa988_a0()	(0)
#define cpu_is_pxa986()	(0)
#define cpu_is_pxa986_z1()	(0)
#define cpu_is_pxa986_z2()	(0)
#define cpu_is_pxa986_z3()	(0)
#define cpu_is_pxa986_a0()	(0)
#define cpu_is_pxa1088()	(0)
#define cpu_is_pxa1L88()	(0)
#define cpu_pxa98x_stepping()	(0)
#endif

#ifdef CONFIG_CPU_MMP2
static inline int cpu_is_mmp2(void)
{
	return (((read_cpuid_id() >> 8) & 0xff) == 0x58);
}
#else
#define cpu_is_mmp2()	(0)
#endif

#ifdef CONFIG_CPU_MMP3
static inline int cpu_is_mmp3(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0x584);
}
static inline int cpu_is_mmp3_a0(void)
{
	if (cpu_is_mmp3() && ((mmp_chip_id & 0x00ff0000) == 0x00a00000))
		return 1;
	else
		return 0;
}

static inline int cpu_is_mmp3_b0(void)
{
	if (cpu_is_mmp3() && ((mmp_chip_id & 0x00ff0000) == 0x00b00000))
		return 1;
	else
		return 0;
}
#else
#define cpu_is_mmp3()	(0)
#endif

#ifdef CONFIG_CPU_MMP3FPGASOC
static inline int cpu_is_mmp3fpgasoc(void)
{
	return ((((read_cpuid_id() >> 4) & 0xfff) == 0xc07) &&
		((mmp_chip_id & 0xffff) == 0xa620));
}
#else
#define cpu_is_mmp3fpgasoc()	(0)
#endif

#endif /* __ASM_MACH_CPUTYPE_H */
