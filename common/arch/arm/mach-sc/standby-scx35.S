/*
 * sc8830 standby mode
 *
 * Author: Wang Liwei
 *
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>

#ifndef CONFIG_NR_CPUS
#define CONFIG_NR_CPUS 1
#endif

#define CPU_SAVED_STATE_SIZE (4 * 11 + 4 * 11)
#define REG_AON_APB_EB0_VIR	0xf5250000
#define REG_AON_APB_EB0_PHY	0x402e0000
#define REG_UART1_ENABLE_VIR	0xf53b0000
#define REG_UART1_ENABLE_PHY	0x71300000
#define REG_UART1_ENABLE_BIT	(0x1 << 14)
#define REG_GLB_GEN0_PHY_ADDR	 (SPRD_APBREG_PHYS + 0x8)
#define REG_GLB_GEN0_VIR_ADDR	 (SPRD_APBREG_BASE + 0x8)
#define UART1_EN		 (0X1 << 14)
#define JTAG_DAP_EN		 (0X1 << 30)
#define UART1_CKD_PHY	(SPRD_UART1_PHYS + 0x24)
#define UART1_CTR0_PHY	(SPRD_UART1_PHYS + 0x18)
#define UART1_CKD_VIR	(SPRD_UART1_BASE + 0x24)
#define UART1_CTR0_VIR	(SPRD_UART1_BASE + 0x18)

	.macro	addruart,rx
	@ see if the MMU is enabled and select appropriate base address
	mrc	p15, 0, \rx, c1, c0
	tst	\rx, #1
	ldreq	\rx, =SPRD_UART1_PHYS
	ldrne	\rx, =SPRD_UART1_BASE
	.endm

	.macro	senduart,rd,rx
	and	\rd,\rd,#0xFF
	str	\rd, [\rx, #0x00]
	.endm

	.macro	waituart,rd,rx
	@ wait for TX_READY
1:	ldr	\rd, [\rx, #0x0C]
	mov	\rd,\rd,lsr #8
	and	\rd,\rd,#0xFF
	teq	\rd, #0x00
	bne	1b
	.endm

	.macro	busyuart,rd,rx
	@ wait for TX_READY
2:	ldr	\rd, [\rx, #0x0C]
	mov	\rd,\rd,lsr #8
	and	\rd,\rd,#0xFF
	teq	\rd, #0x00
	bne	2b
	.endm

	.text
	.align 12
ENTRY(sc8830_standby_iram)
sprd_pm_mapped_va:
	stmfd sp!, {lr}
	/* disable uart1 for idle deep sleep */
	ldr r0, =REG_UART1_ENABLE_VIR
	ldr r1, [r0]
	bic r1, #REG_UART1_ENABLE_BIT
	str r1, [r0]
	ldr r0, =sprd_pm_va_to_iram
	ldr r1, =sc8830_standby_iram
	ldr r2, =iram_start
	ldr r3, [r2, #0]
	sub r0, r0, r1
	add r0, r0, r3
	mov pc, r0
sprd_pm_va_to_iram:
	nop
	nop
	nop
	dsb
	isb
#if 0
4:  b 4b
#endif
	nop
	nop
	nop
	dsb
	isb
#ifndef CONFIG_ARCH_SCX30G
	wfi
	mov	r0, #0x400
	/*wait 60us for clk is safely closed*/
5:	sub	r0, r0, #0x1
	cmp	r0, #0x0
	bne	5b
#else
	/*enable icache*/
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #0x1000
	mcr	p15, 0, r0, c1, c0, 0

	/*   NOTE: here is some trick code, please pay more attention.
	 *   Put some code which before "wfi" executed into I-cache to avoid
	 * some hang-up problems caused by FSM of chip-sleep.
	 *   When code after "wfi" is executing, it means that cpu can not be
	 * powerred off. So we delay some time(must longer than 6*RTC_cycle,
	 * wait for FSM) in I-cache to avoid some access out of cpu.
	 */
	mov	r1, #0x0
9:	cmp	r1, #0x1
	wfieq
	movne	r0, #0x1
	moveq	r0, #0x20000
	/*wait 60us for clk is safely closed*/
5:	sub	r0, r0, #0x1
	cmp	r0, #0x0
	bne	5b

	add	r1, r1, #0x1
	cmp	r1, #0x1
	beq	9b
	/*disable icache*/
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #0x1000
	mcr	p15, 0, r0, c1, c0, 0
#endif
#if 0
6:  b 6b
#endif
	ldr r0, =REG_AON_APB_EB0_VIR
	ldr r1, [r0]
	orr r1, #JTAG_DAP_EN
	str r1, [r0]
	ldr r0, =REG_UART1_ENABLE_VIR
	ldr r1, [r0]
	orr r1, #REG_UART1_ENABLE_BIT
	str r1, [r0]
	ldmfd sp!, {lr}
#if 0
	ldr r0, =REG_UART1_ENABLE_VIR
	ldr r1, [r0]
	orr r1, r1, #REG_UART1_ENABLE_BIT
	str r1, [r0]
	ldr r0, =UART1_CKD_VIR
	ldr r1, =0xe2
	str r1, [r0]
	ldr r0, =UART1_CTR0_VIR
	ldr r1, =0x1c
	str r1, [r0]
loopq:
	mov r0, #'B'
	bl printch
	b loopq

#endif
	mov r0, #0
	mov pc, lr
	nop
	nop
	nop
	nop
	nop
	nop

ENDPROC(sc8830_standby_iram)

ENTRY(sc8830_standby_exit_iram)
	ldr r0, =REG_AON_APB_EB0_PHY
	ldr r1, [r0]
	orr r1, #JTAG_DAP_EN
	str r1, [r0]

	ldr r0, =0xFB0
	ldr r1, =0x55555555
	str r1, [r0]

	bl uart1_en
	mrs r0, cpsr
	and r0, r0, #0xff
	mov r6, r0
	ldr r1, =0x00d3
	cmp r0, r1
	msr cpsr_c, r1

	ldr r0, =0x20
	bl my_delay
#ifdef CONFIG_ARCH_SCX30G
	mov r2, #0x3
	ldr r0,=0x402b00c8
check_phy_fsm:
	ldr r1,[r0]
	and r1,r1,#0xf000
	cmp r1,#0x0
	bne check_phy_fsm
check_pub_fsm:
	ldr r1,[r0]
	and r1,r1,#0x00f0
	cmp r1,#0x0
	bne check_pub_fsm
	sub r2,r2,#0x1
	cmp r2,#0x0
	bne check_phy_fsm
	ldr r0,=0x50002404
	ldr r0,[r0]
	ldr r1,[r0]
	tst r1,#0x1
	bne next
	/*send ipi irq1 to cp0*/
	ldr r0,=0x402c0000
	mov r1,#0x4
	str r1,[r0]

1:	ldr r0,=0x50002404
	ldr r0,[r0]
	ldr r1,[r0]
	tst r1,#0x1
	beq 1b
next:
#endif

/* DLL calibration */
#if 0
      mov  r0,#0x30000004
      ldr    r1,[r0]
      and  r1,r1,#0x7
      cmp r1,#0x3
      bne  recover_context

      ldr    r0,=0x300001b0
      ldr    r1,[r0]
      bic   r1,r1,#0x1
      str   r1,[r0]

      ldr   r0,=0x30010004
      mov r1,#0x21
      str   r1,[r0]

      ldr   r0,=0x30010018
1:   ldr   r1,[r0]
      tst  r1,#0x1
      beq 1b

      ldr    r0,=0x300001b0
      ldr    r1,[r0]
      orr   r1,r1,#0x1
      str   r1,[r0]

      mov  r0,#0x100
2:   sub   r0,r0,#0x1
      cmp  r0,#0x0
      bne   2b
recover_context:
#endif
/* DLL calibration end */

#ifdef CONFIG_DDR_VALIDITY_TEST
	bl ddr_test
#endif
#if 0
5:	b 5b
#endif
	#blne dead
/*
	adr r0, sleep_asm_counter
	ldr r1, [r0]
	add r1, r1, #1
	str r1, [r0]
	cmp r1, #32
	bleq dead
*/

loopp:
#	adr	r0, str_deep
#	bl	printascii_phy
	/*
	b loopp
	*/

	/*
	*  convert to physical address,
	*  need to find a more elegent way.
	*/
context_restore:
	ldr	r0, =sp_pm_collapse_exit
	ldr	r1, =0xc0000000
	sub	r0, r0, r1
	ldr	r1, =(CONFIG_PHYS_OFFSET)
	add	r0, r0, r1
	mov pc, r0
ENDPROC(sc8830_standby_exit_iram)

#ifdef CONFIG_DDR_VALIDITY_TEST
#ifndef CONFIG_ARCH_SCX30G
ENTRY(ddr_test)
ddr_start:
	/* we will test 64kB ddr */
	mov r8, lr
	ldr r4, =0x10000
	ldr r3, =0x12345678
	ldr r5, =0x87654321
	mov r0, #0x100
	ldr r1, [r0]
	cmp r1, #0x0
	beq finish
	add r4, r1
loop:
	ldr r2, [r1]
	teq r2, r3
	bne bad
	str r5, [r1]
	ldr r2, [r1]
	teq r2, r5
	str r3, [r1]
	bne bad
	add r1, #0x4
	cmp r1, r4
	blt loop
#	adr r0, str_y
#	bl printascii_phy
	b finish
bad:
	str r1, [r0]
	str r2, [r0, #0x4]
#	adr r0, str_n
#	bl printascii_phy
#b ddr_start
	b dead
finish:
	mov pc, r8
ENDPROC(ddr_test)
#else
ENTRY(ddr_test)
start:
	/*ddr test, always read till it's right*/
	ldr r7, =0x12345678
	mov r8, lr
	mov r5, #0x100
	ldr r0, [r5]
	cmp r0, #0x0
	beq finish
	add r6, r0
	mov r2, #0x0
loop:
	ldr r1, [r0]
	teq r7, r1
	bne bad
	mov r6, r2
	mov r7, r0
#	adr r0, str_y
#	bl printascii_phy
	b finish
bad:
	add r2, #0x1
	ldr r1, [r0]
	teq r7, r1
	bne bad
	mov r6, r2
	mov r7, r0
#	adr r0, str_n
#	bl printascii_phy
finish:
	str r6, [r7, #0x4]
	mov pc, r8
ENDPROC(ddr_test)
#endif
#endif //CONFIG_DDR_VALIDITY_TEST

ENTRY(uart1_en)
	ldr r0, =REG_UART1_ENABLE_PHY
	ldr r1, [r0]
	orr r1, r1, #REG_UART1_ENABLE_BIT
	str r1, [r0]
	ldr r0, =UART1_CKD_PHY
	ldr r1, =0xe2
	str r1, [r0]
	ldr r0, =UART1_CTR0_PHY
	ldr r1, =0x1c
	str r1, [r0]
ENDPROC(uart1_en)

ENTRY(printascii_phy)
		addruart r3
		b	2f
1:		waituart r2, r3
		senduart r1, r3
		busyuart r2, r3
		teq	r1, #'\n'
		moveq	r1, #'\r'
		beq	1b
2:		teq	r0, #0
		ldrneb	r1, [r0], #1
		teqne	r1, #0
		bne	1b
		mov	pc, lr
ENDPROC(printascii_phy)

printch:
		addruart r3
		mov	r1, r0
		mov	r0, #0
		b	1b

ENTRY(my_delay)
1:  sub	r0, r0, #0x1
    cmp	r0, #0x0
    bne  1b
    mov pc, lr
ENDPROC(my_delay)

ENTRY(dead)
1:	adr	r0, str_err
	bl	printascii_phy
	ldr r0, =0x000fffff
	bl my_delay

	b 1b
ENDPROC(dead)

	.globl sleep_asm_counter
sleep_asm_counter:
	.long	0x0

str_y:	.asciz	"Y!\n"
str_n:	.asciz	"N!\n"
str_deep: .asciz	"from deep!"
str_err: .asciz	"DDR ERR!"

ENTRY(sc8830_standby_iram_end)

ENTRY(sp_arch_idle)
	wfi
	nop
	nop
	nop
	bx	lr
ENDPROC(sp_arch_idle)

ENTRY(sp_pm_collapse)
	mov r3, r1
	ldr	r0, =sp_saved_state	/* address of sp_saved_state ptr */
#if (CONFIG_NR_CPUS >= 2)
	mrc	p15, 0, r1, c0, c0, 5	/* MPIDR */
	ands	r1, r1, #15		/* What CPU am I */
	mov	r2, #CPU_SAVED_STATE_SIZE
	mul	r1, r1, r2
	add	r0, r0, r1
#endif

	stmia   r0!, {r4-r14}
#if 0
	mov r5, r0
	cmp	r3, #1
	bne	real_deep
	adr	r0, str_a6
	bl	printascii_phy
	b	go_on
real_deep:
	adr	r0, str_a7
	bl	printascii_phy
go_on:
	mov	r0, r5
#endif
#if 0
2:	b 2b
#endif
	mrc     p15, 0, r1, c1, c0, 0 /* MMU control */
	mrc     p15, 0, r2, c2, c0, 0 /* TTBR0 */
	mrc     p15, 0, r3, c3, c0, 0 /* dacr */
	mrc     p15, 0, r5, c10, c2, 0 /* PRRR */
	mrc     p15, 0, r6, c10, c2, 1 /* NMRR */
	mrc     p15, 0, r7, c1, c0, 1 /* ACTLR */
	mrc     p15, 0, r8, c2, c0, 1 /* TTBR1 */
	mrc     p15, 0, r9, c13, c0, 3 /* TPIDRURO */
	mrc     p15, 0, r10, c13, c0, 4 /* TPIDRURW */
	mrc     p15, 0, ip, c13, c0, 1 /* context ID */
	stmia   r0!, {r1-r10, ip}

	bl	v7_flush_dcache_all
#if 0
	blx	outer_flush_all
	blx		outer_disable
#endif
	dmb

	mrc	p15, 0, r0, c0, c0, 5	/* MPIDR */
	and	r0, r0, #15		/* what CPU am I */

	mrc     p15, 0, r4, c1, c0, 0    /* read current CR    */
	bic     r0, r4, #(1 << 2)        /* clear dcache bit   */
	bic     r0, r0, #(1 << 12)       /* clear icache bit   */
	mcr     p15, 0, r0, c1, c0, 0    /* disable d/i cache  */
	isb
#ifdef CONFIG_ARCH_SCX30G
	bl	v7_flush_icache_all
#endif
	bl sc8830_standby_iram

	mcr     p15, 0, r4, c1, c0, 0    /* restore d/i cache  */
	isb

	mrc	p15, 0, r0, c0, c0, 5 /* MPIDR */
	and	r0, r0, #15              /* what CPU am I                  */

#if 0
	blx		outer_resume
#endif
	dmb

	ldr     r0, =sp_saved_state	/* address of sp_saved_state ptr */
#if (CONFIG_NR_CPUS >= 2)
	mrc	p15, 0, r1, c0, c0, 5	/* MPIDR */
	ands	r1, r1, #15		/* What CPU am I */
	mov	r2, #CPU_SAVED_STATE_SIZE
	mul	r2, r2, r1
	add	r0, r0, r2
#endif
	ldmfd   r0, {r4-r14}		 /* restore registers */
	mov     r0, #0                   /* return power collapse failed */
	bx      lr
ENDPROC(sp_pm_collapse)
str_a6:	.asciz	"idle deep\n"
str_a7:	.asciz	"real deep\n"
ENTRY(sp_pm_collapse_exit)
	mov r0, #'Z'
	bl printch
	ldr     r1, =sp_saved_state
	ldr     r2, =sp_pm_collapse_exit
	adr     r3, sp_pm_collapse_exit
	add     r1, r1, r3
	sub     r1, r1, r2
	add	r1, r1, #CPU_SAVED_STATE_SIZE
#if (CONFIG_NR_CPUS >= 2)
	mrc	p15, 0, r2, c0, c0, 5	/* MPIDR */
	ands	r2, r2, #15		/* What CPU am I */
	mov	r3, #CPU_SAVED_STATE_SIZE
	mul	r2, r2, r3
	add	r1, r1, r2
#endif

	ldmdb   r1!, {r2-r12}
	mcr     p15, 0, r4, c3, c0, 0 /* dacr */
	mcr     p15, 0, r3, c2, c0, 0 /* TTBR0 */
	mcr     p15, 0, r6, c10, c2, 0 /* PRRR */
	mcr     p15, 0, r7, c10, c2, 1 /* NMRR */
	mcr     p15, 0, r8, c1, c0, 1 /* ACTLR */
	mcr     p15, 0, r9, c2, c0, 1 /* TTBR1 */
	mcr     p15, 0, r10, c13, c0, 3 /* TPIDRURO */
	mcr     p15, 0, r11, c13, c0, 4 /* TPIDRURW */
	mcr     p15, 0, r12, c13, c0, 1 /* context ID */
	isb
	ldmdb   r1!, {r4-r14}

#if 0
3:	b 3b
	mov r0, #'Y'
	bl printch
#endif

	/* Add 1:1 map in the PMD to allow smooth switch when turning on MMU */
	and     r3, r3, #~0x7F  /* mask off lower 7 bits of TTB */
	adr     r0, sp_pm_mapped_pa /* get address of the mapped instr */
	lsr     r1, r0, #20     /* get the addr range of addr in MB */
	lsl     r1, r1, #2      /* multiply by 4 to get to the pg index */
	add     r3, r3, r1      /* pgd + pgd_index(addr) */
	ldr     r1, [r3]        /* save current entry to r1 */
	lsr     r0, #20         /* align current addr to 1MB boundary */
	lsl     r0, #20
	/* Create new entry for this 1MB page */
	orr     r0, r0, #0x4     /* PMD_SECT_BUFFERED */
	orr     r0, r0, #0x400   /* PMD_SECT_AP_WRITE */
	orr     r0, r0, #0x2     /* PMD_TYPE_SECT|PMD_DOMAIN(DOMAIN_KERNEL) */
	str     r0, [r3]         /* put new entry into the MMU table */
	mcr     p15, 0, r3, c7, c10, 1  /* flush_pmd */
	dsb
	isb
	mcr     p15, 0, r2, c1, c0, 0   /* MMU control */
	isb
sp_pm_mapped_pa:
	/* Switch to virtual */
	adr     r2, sp_pm_pa_to_va
	ldr     r0, =sp_pm_pa_to_va
	mov     pc, r0
sp_pm_pa_to_va:
	sub     r0, r0, r2
	/* Restore r1 in MMU table */
	add     r3, r3, r0
	str     r1, [r3]
	mcr     p15, 0, r3, c7, c10, 1  /* flush_pmd */
	dsb
	isb
	mcr     p15, 0, r3, c8, c7, 0   /* UTLBIALL */
	mcr     p15, 0, r3, c7, c5, 6   /* BPIALL */
	dsb
	isb
#ifdef CONFIG_FIX_V7TAGRAM_BUG
	stmfd   sp!, {lr}
	ldr r0, =physical_from_idle
	mov r1, #0
	str r1, [r0]
	bl fix_tag_ram_bug
	ldmfd   sp!, {lr}
#endif

#if 0
	stmfd   sp!, {lr}
	blx		outer_resume
	dmb
	ldmfd   sp!, {lr}
#endif
#if 0
2:	b	2b
#endif
	mov     r0, #1
	bx      lr
	nop
	nop
	nop
	nop
	nop
1:	b       1b
ENDPROC(sp_pm_collapse_exit)

	.data
sp_saved_state:
	.space  CPU_SAVED_STATE_SIZE * CONFIG_NR_CPUS

