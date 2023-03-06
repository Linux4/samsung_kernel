#ifndef __ASM_ARM_CP15_H
#define __ASM_ARM_CP15_H

#include <asm/barrier.h>

/*
 * CR1 bits (CP#15 CR1)
 */
#define CR_M	(1 << 0)	/* MMU enable				*/
#define CR_A	(1 << 1)	/* Alignment abort enable		*/
#define CR_C	(1 << 2)	/* Dcache enable			*/
#define CR_W	(1 << 3)	/* Write buffer enable			*/
#define CR_P	(1 << 4)	/* 32-bit exception handler		*/
#define CR_D	(1 << 5)	/* 32-bit data address range		*/
#define CR_L	(1 << 6)	/* Implementation defined		*/
#define CR_B	(1 << 7)	/* Big endian				*/
#define CR_S	(1 << 8)	/* System MMU protection		*/
#define CR_R	(1 << 9)	/* ROM MMU protection			*/
#define CR_F	(1 << 10)	/* Implementation defined		*/
#define CR_Z	(1 << 11)	/* Implementation defined		*/
#define CR_I	(1 << 12)	/* Icache enable			*/
#define CR_V	(1 << 13)	/* Vectors relocated to 0xffff0000	*/
#define CR_RR	(1 << 14)	/* Round Robin cache replacement	*/
#define CR_L4	(1 << 15)	/* LDR pc can set T bit			*/
#define CR_DT	(1 << 16)
#define CR_IT	(1 << 18)
#define CR_ST	(1 << 19)
#define CR_FI	(1 << 21)	/* Fast interrupt (lower latency mode)	*/
#define CR_U	(1 << 22)	/* Unaligned access operation		*/
#define CR_XP	(1 << 23)	/* Extended page tables			*/
#define CR_VE	(1 << 24)	/* Vectored interrupts			*/
#define CR_EE	(1 << 25)	/* Exception (Big) Endian		*/
#define CR_TRE	(1 << 28)	/* TEX remap enable			*/
#define CR_AFE	(1 << 29)	/* Access flag enable			*/
#define CR_TE	(1 << 30)	/* Thumb exception enable		*/

#ifndef __ASSEMBLY__

#if __LINUX_ARM_ARCH__ >= 4
#define vectors_high()	(cr_alignment & CR_V)
#else
#define vectors_high()	(0)
#endif

#ifdef CONFIG_CPU_CP15

extern unsigned long cr_no_alignment;	/* defined in entry-armv.S */
extern unsigned long cr_alignment;	/* defined in entry-armv.S */

static inline unsigned int get_cr(void)
{
	unsigned int val;
	asm("mrc p15, 0, %0, c1, c0, 0	@ get CR" : "=r" (val) : : "cc");
	return val;
}

static inline void set_cr(unsigned int val)
{
	asm volatile("mcr p15, 0, %0, c1, c0, 0	@ set CR"
	  : : "r" (val) : "cc");
	isb();
}

#ifndef CONFIG_SMP
extern void adjust_cr(unsigned long mask, unsigned long set);
#endif

#define CPACC_FULL(n)		(3 << (n * 2))
#define CPACC_SVC(n)		(1 << (n * 2))
#define CPACC_DISABLE(n)	(0 << (n * 2))

static inline unsigned int get_copro_access(void)
{
	unsigned int val;
	asm("mrc p15, 0, %0, c1, c0, 2 @ get copro access"
	  : "=r" (val) : : "cc");
	return val;
}

static inline void set_copro_access(unsigned int val)
{
	asm volatile("mcr p15, 0, %0, c1, c0, 2 @ set copro access"
	  : : "r" (val) : "cc");
	isb();
}

#else /* ifdef CONFIG_CPU_CP15 */

/*
 * cr_alignment and cr_no_alignment are tightly coupled to cp15 (at least in the
 * minds of the developers). Yielding 0 for machines without a cp15 (and making
 * it read-only) is fine for most cases and saves quite some #ifdeffery.
 */
#define cr_no_alignment	UL(0)
#define cr_alignment	UL(0)

#endif /* ifdef CONFIG_CPU_CP15 / else */

#ifdef CONFIG_TIMA_RKP
#define RKP_DEFERRED_INIT 0xe

struct rkp_init_struct {
	volatile unsigned long _text;
	volatile unsigned long _stext;
	volatile unsigned long _etext;
	volatile unsigned long __v7_setup_stack;
} __attribute__((packed));

#define FC_ID 		0x83800000
#define BUILD_CMD_ID(cmdid)	(FC_ID | (((cmdid) & 0x3ff) << 12))
int tima_is_pg_protected(unsigned long va);

#define tima_cache_flush(x)		\
	__asm__ __volatile__(	"mcr     p15, 0, %0, c7, c14, 1\n"	\
							"dsb\n"								\
							"isb\n"								\
                			: : "r" (x))
#define tima_cache_inval(x)		\
	__asm__ __volatile__(	"mcr     p15, 0, %0, c7, c6, 1\n"	\
							"dsb\n"								\
							"isb\n"								\
                			: : "r" (x))

#ifdef CONFIG_HYP_RKP 
static inline void tima_send_cmd (unsigned int p1, unsigned int cmdid)
{
	volatile uint32_t tima_cmdid = BUILD_CMD_ID(cmdid); 
	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension virt\n"
#endif	
		"stmfd   sp!, {r0-r4}\n"   
		"mov     r2, %0\n" /* r2 carry mcr_val */
		"mov     r0, %1\n"
		"hvc     #1\n" 
        "ldmfd   sp!, {r0-r4}" 
		::"r"(p1), "r" (tima_cmdid) : "r0","r1","r2","r3","r4","cc");
}

/* p1 --> r2, p2 --> r3s */
static inline void tima_send_cmd2 (unsigned int p1, unsigned int p2, unsigned int cmdid)
{
	volatile uint32_t tima_cmdid = BUILD_CMD_ID(cmdid); 

	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension virt\n"
#endif	
	"stmfd   sp!, {r0-r4}\n"
    "mov     r2, %0\n"
	"mov     r3, %1\n"  
	"mov     r0, %2\n"
	"hvc     #2\n" 
	"ldmfd   sp!, {r0-r4}" : : "r" (p1), "r" (p2), "r" (tima_cmdid) : "r0","r1","r2","r3","r4","cc");
}

/* p1 --> r2, p2 --> r3 ,p3 --> r4*/
static inline void tima_send_cmd3 (unsigned int p1, unsigned int p2,unsigned int p3, unsigned int cmdid)
{
	volatile uint32_t tima_cmdid = BUILD_CMD_ID(cmdid); 

	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension virt\n"
#endif	
	"stmfd   sp!, {r0-r4}\n"
    "mov     r2, %0\n"
	"mov     r3, %1\n"  
	"mov     r4, %2\n"  
	"mov     r0, %3\n"
	"hvc     #2\n" 
	"ldmfd   sp!, {r0-r4}" : : "r" (p1), "r" (p2), "r" (p3),"r" (tima_cmdid) : "r0","r1","r2","r3","r4","cc");
}
static inline void tima_send_cmd4 (unsigned int p1, unsigned int p2, unsigned int p3, unsigned int p4, unsigned int cmdid)
{
	volatile uint32_t tima_cmdid = BUILD_CMD_ID(cmdid); 
	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension virt\n"
#endif	
		"stmfd   sp!, {r0-r5}\n"
		"mov     r2, %0\n"
		"mov     r3, %1\n"  
		"mov     r4, %2\n"  
		"mov     r5, %3\n"  
		"mov     r0, %4\n"
		"hvc     #1\n" 
        "ldmfd   sp!, {r0-r5}" : : "r" (p1), "r" (p2), "r" (p3), "r" (p4), "r" (tima_cmdid) : "r0","r2","r3","r4","r5","cc");
}
static inline void tima_send_cmd5 (unsigned int p1, unsigned int p2, unsigned int p3, unsigned int p4, unsigned int p5, unsigned int cmdid)
{
	volatile uint32_t tima_cmdid = BUILD_CMD_ID(cmdid); 
	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension virt\n"
#endif	
		"stmfd   sp!, {r0-r5}\n"
		"mov     r2, %0\n"
		"mov     r3, %1\n"  
		"mov     r4, %2\n"  
		"mov     r5, %3\n" 
		"mov     r1, %4\n" 
		"mov     r0, %5\n"
		"hvc     #1\n" 
        "ldmfd   sp!, {r0-r5}" : : "r" (p1), "r" (p2), "r" (p3), "r" (p4), "r" (p5), "r" (tima_cmdid) : "r0","r1","r2","r3","r4","r5","cc");
}
#else /* !CONFIG_HYP_RKP */

static inline void tima_send_cmd (unsigned int p1, unsigned int cmdid)
{
	volatile uint32_t tima_cmdid = BUILD_CMD_ID(cmdid); 
	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension sec\n"
#endif	
		"stmfd   sp!, {r0-r4}\n"   
		"mov     r2, %0\n" /* r2 carry mcr_val */
		"mov     r0, %1\n"
		"smc     #1\n" 
        "ldmfd   sp!, {r0-r4}" 
		::"r"(p1), "r" (tima_cmdid) : "r0","r1","r2","r3","r4","cc");
}

/* p1 --> r2, p2 --> r3s */
static inline void tima_send_cmd2 (unsigned int p1, unsigned int p2, unsigned int cmdid)
{
	volatile uint32_t tima_cmdid = BUILD_CMD_ID(cmdid); 

	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension sec\n"
#endif	
	"stmfd   sp!, {r0-r4}\n"
    "mov     r2, %0\n"
	"mov     r3, %1\n"  
	"mov     r0, %2\n"
	"smc     #2\n" 
	"ldmfd   sp!, {r0-r4}" : : "r" (p1), "r" (p2), "r" (tima_cmdid) : "r0","r1","r2","r3","r4","cc");
}

/* p1 --> r2, p2 --> r3 ,p3 --> r4*/
static inline void tima_send_cmd3 (unsigned int p1, unsigned int p2,unsigned int p3, unsigned int cmdid)
{
	volatile uint32_t tima_cmdid = BUILD_CMD_ID(cmdid); 

	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension virt\n"
#endif	
	"stmfd   sp!, {r0-r4}\n"
    "mov     r2, %0\n"
	"mov     r3, %1\n"  
	"mov     r4, %2\n"  
	"mov     r0, %3\n"
	"smc     #2\n" 
	"ldmfd   sp!, {r0-r4}" : : "r" (p1), "r" (p2), "r" (p3),"r" (tima_cmdid) : "r0","r1","r2","r3","r4","cc");
}
static inline void tima_send_cmd4 (unsigned int p1, unsigned int p2, unsigned int p3, unsigned int p4, unsigned int cmdid)
{
	volatile uint32_t tima_cmdid = BUILD_CMD_ID(cmdid); 
	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension sec\n"
#endif	
		"stmfd   sp!, {r0-r5}\n"
		"mov     r2, %0\n"
		"mov     r3, %1\n"  
		"mov     r4, %2\n"  
		"mov     r5, %3\n"  
		"mov     r0, %4\n"
		"smc     #1\n" 
        "ldmfd   sp!, {r0-r5}" : : "r" (p1), "r" (p2), "r" (p3), "r" (p4), "r" (tima_cmdid) : "r0","r2","r3","r4","r5","cc");
}
static inline void tima_send_cmd5 (unsigned int p1, unsigned int p2, unsigned int p3, unsigned int p4, unsigned int p5, unsigned int cmdid)
{
	volatile uint32_t tima_cmdid = BUILD_CMD_ID(cmdid); 
	asm volatile (
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
        ".arch_extension sec\n"
#endif	
		"stmfd   sp!, {r0-r5}\n"
		"mov     r2, %0\n"
		"mov     r3, %1\n"  
		"mov     r4, %2\n"  
		"mov     r5, %3\n" 
		"mov     r1, %4\n" 
		"mov     r0, %5\n"
		"smc     #1\n" 
        "ldmfd   sp!, {r0-r5}" : : "r" (p1), "r" (p2), "r" (p3), "r" (p4), "r" (p5), "r" (tima_cmdid) : "r0","r1","r2","r3","r4","r5","cc");
}
#endif /* CONFIG_HYP_RKP */
#endif /* CONFIG_TIMA_RKP */

#endif /* ifndef __ASSEMBLY__ */

#endif
