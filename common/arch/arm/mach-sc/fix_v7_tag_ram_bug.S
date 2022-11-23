/*
*	fix arm-v7 tag ram bug
*
*/

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>

#define REG_HOLDING_PEN_VADDR	 (SPRD_AHB_BASE + 0x4c)
#define REG_CPU1_JUMP_VADDR	 (SPRD_AHB_BASE + 0x54)
#define REG_CA7_STANDBY_STATUS_VADDR	 (SPRD_AHB_BASE + 0x48)

	.data

@other_cpu_fix function phys
.globl other_cpu_fix_phys
other_cpu_fix_phys:
	.long	0x0

@invalidation_flag for other 3 cpus
invalidation_flag:
	.long	0x0
	.long 	0x0
	.long 	0x0
all_complete:
	.long	0x0

	.text

ENTRY(fix_tag_ram_bug)
	stmfd 	sp!, {r4-r12, lr}

	mrc     p15, 0, r1, c1, c0, 0
	bic     r0, r1, #(1 << 2)		@disable d-cache
	mcr     p15, 0, r0, c1, c0, 0
	bl      v7_flush_dcache_all		@flush d-cache
	isb

	mov	r2, #0x0
	ldr	r1, =invalidation_flag
	str 	r2, [r1]			@clean all invalidation flag
	str 	r2, [r1, #4]
	str 	r2, [r1, #8]
	ldr	r1, =all_complete
	str 	r2, [r1]			@clean complete flag

	ldr     r1, =other_cpu_fix_phys		@other_cpu_fix function virt_to_phys
	ldr	r3, [r1]
	cmp 	r3, #0				@function phys shouldn't be zero
	beq	finished
	ldr     r1, =REG_CPU1_JUMP_VADDR	@change other cpus jump address
        str     r3, [r1]
        str     r3, [r1, #4]
        str     r3, [r1, #8]
	ldr     r1, =REG_HOLDING_PEN_VADDR	@write holding pen for other cpus
	mov 	r2, #(0x02 + 0x04 + 0x08)
	str     r2, [r1]
	dsb

	mov	r0, #0x01			@power on other cpus
	bl	poweron_cpus
	mov	r0, #0x02
	bl	poweron_cpus
	mov	r0, #0x03
	bl	poweron_cpus
	ldr 	r0, =0x0000ffff
	bl	my_delay
	sev

loop_wfe:
	wfe
	ldr	r1, =invalidation_flag
	ldr 	r2, [r1]
	cmp	r2, #1
	bne	loop_wfe
	ldr 	r2, [r1, #4]
	cmp	r2, #1
	bne	loop_wfe
	ldr 	r2, [r1, #8]
	cmp	r2, #1
	bne	loop_wfe

	mov	r0, #0x1
	ldr	r1, =all_complete
	str 	r0, [r1]
	dsb
	sev
	wfe					@just ignore sev before
	nop
/*
	ldr 	r0, =0x00ffffff			@make sure other cpus go into wfi
	bl	my_delay
*/
#if 0	//uart debug
	adr 	r0, str_cpu0
	bl	printascii_phy
#endif

	ldr 	r2, =physical_from_idle		@wether or not from physical_from_idle
	ldr	r5, [r2]
	ldr 	r2, =REG_CA7_STANDBY_STATUS_VADDR
wait_others_wfi:
	ldr	r3, [r2]
	eor 	r3, r5
	and	r3, #0x0e
	teq	r3, #0x0e			@check wfi? which not from physical_from_idle
	bne 	wait_others_wfi

	tst	r5, #(1 << 1)
	mov 	r0, #0x1
	bleq	powerdown_cpus
	tst	r5, #(1 << 2)
	mov 	r0, #0x2
	bleq	powerdown_cpus
	tst	r5, #(1 << 3)
	mov 	r0, #0x3
	bleq	powerdown_cpus

finished:
	mrc     p15, 0, r1, c1, c0, 0
	orr     r0, r1, #(1 << 2)		@enable d-cache
	mcr     p15, 0, r0, c1, c0, 0

	ldmfd 	sp!, {r4-r12, pc}
ENDPROC(fix_tag_ram_bug)


ENTRY(other_cpu_fix)
	mov	r0, #0x0
	mcr     p15, 0, r0, c8, c3, 1
	isb
	dsb
	nop
	mrc	p15, 0, r1, c0, c0, 5
	and	r1, r1, #3			@ r1=cpu mpid
	cmp 	r1, #0x0
	beq	wfi_for_powerdown		@ mpid shouldn't be zero
	mov	r9, r1				@ r9=saved cpu mpid
	sub 	r1, #1
	lsl	r1, #2

	ldr	r6, =invalidation_flag
	ldr	r4, =other_cpu_fix
	adr	r5, other_cpu_fix
	add	r6, r6, r5
	sub	r6, r6, r4			@ r6=invalidation_flag phys
	ldr	r7, =all_complete
	add	r7, r7, r5
	sub	r7, r7, r4			@ r7=all_complete phys
	ldr	r8, =physical_from_idle
	add	r8, r8, r5
	sub	r8, r8, r4			@ r8=physical_from_idle phys

	mov	r2, #0x1
	str 	r2, [r6, r1]
	dsb
	sev

loop_wfe_other:
	wfe
	ldr	r0, [r7]
	cmp	r0, #0x1
	bne 	loop_wfe_other
#if 0	//uart debug
	adr 	r0, str_cpus
	bl	printascii_phy
#endif
	ldr 	r0, [r8]
	mov 	r2, #0x1
	lsl	r2, r9
	tst	r0, r2
	beq	wfi_for_powerdown
	mov	pc, #0x0			@if from idle, cpu pc should goto 0x0

wfi_for_powerdown:
	wfi
	b 	wfi_for_powerdown
ENDPROC(other_cpu_fix)

str_cpu0:	.asciz	"!! cpu0 !!\n"
str_cpus:	.asciz	"-others-\n"
