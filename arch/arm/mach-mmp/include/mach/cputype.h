#ifndef __ASM_MACH_CPUTYPE_H
#define __ASM_MACH_CPUTYPE_H

#include <asm/cputype.h>
#include <asm/io.h>

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
#endif

#ifdef CONFIG_CPU_PXA1088
static inline int cpu_is_pxa1088(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc07) &&
		(((mmp_chip_id & 0xffff) == 0x1088));
}

static inline int cpu_is_pxa1920(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc07) &&
		(((mmp_chip_id & 0xffff) == 0x1188));
}

#define BOOT_ROM_VER 0xFFE00028
#define BOOT_ROM_A0 0x11122012
#define BOOT_ROM_A1 0x01282013

static inline unsigned long get_bootrom_ver(void)
{
	volatile u32 *bootrom_ver_p;
	static u32 bootrom_ver;
	static int first_n;

	if (!first_n) {
		first_n = 1;
		bootrom_ver_p = ioremap(BOOT_ROM_VER, 4);
		bootrom_ver = __raw_readl(bootrom_ver_p);
		iounmap(bootrom_ver_p);
	}

	return bootrom_ver;
}

static inline int cpu_is_pxa1088_a0(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc07) &&
		(((mmp_chip_id & 0xffff) == 0x1088)) &&
		(get_bootrom_ver() == BOOT_ROM_A0);
}

static inline int cpu_is_pxa1088_a1(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc07) &&
		(((mmp_chip_id & 0xffff) == 0x1088)) &&
		(get_bootrom_ver() == BOOT_ROM_A1);
}
#else
#define cpu_is_pxa1088()	(0)
#define cpu_is_pxa1088_a0()	(0)
#define cpu_is_pxa1088_a1()	(0)
#define cpu_is_pxa1920()	(0)
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

#ifdef CONFIG_CPU_EDEN
static inline int cpu_is_eden(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc07) &&
		((mmp_chip_id & 0xffff) == 0xc192);
}
#else
#define cpu_is_eden(id)	(0)
#endif

static inline int cpu_is_armv7_a7(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc07);
}

static inline int cpu_is_armv7_a9(void)
{
	return (((read_cpuid_id() >> 4) & 0xfff) == 0xc09);
}


#endif /* __ASM_MACH_CPUTYPE_H */
