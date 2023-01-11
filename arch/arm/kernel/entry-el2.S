#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/asm-offsets.h>

/* Fix for head entry arguments. Only support DTS. */
	.macro	head_arm64_arg_fix
	.word	0xaa0003e2	/*mov	x2, x0*/
	.word	0xd2800000	/*mov	x0, #0*/
	.word	0x92800001	/*mov	x1, #-1*/
	.endm

/*
 * Source code is copied from arch/arm64/kernel/head.s, el2_setup.
 * Only support little edian, and do some changes for EL1 aarch32
 */
	.macro	el2_setup
	/*Changes: r0-r2 has arguments, we need save r0, r1 */
	.word	0xaa0003e5	/*mov	x5, x0*/
	.word	0xaa0103e6	/*mov	x6, x1*/
	.word	0xd5384240	/*mrs	x0, currentel*/
	.word	0xf100201f	/*cmp	x0, #0x8*/
	.word	0xfa491804	/*ccmp	x0, #0x9, #0x4, ne*/
	.word	0x540000a1	/*b.ne	a0 <el2_setup+0x20>*/
	.word	0xd53c1000	/*mrs	x0, sctlr_el2*/
	.word	0x9266f800	/*and	x0, x0, #0xfffffffffdffffff*/
	.word	0xd51c1000	/*msr	sctlr_el2, x0*/
	.word	0x14000007	/*b	b8 <el2_setup+0x38>*/
	.word	0xd5381000	/*mrs	x0, sctlr_el1*/
	.word	0x9266f400	/*and	x0, x0, #0xfffffffffcffffff*/
	.word	0xd5181000	/*msr	sctlr_el1, x0*/
	.word	0x5281c234	/*mov	w20, #0xe11*/
	.word	0xd5033fdf	/*isb*/
	/* Changes: We donot support EL1 aarch64 */
	.word	0x14000000	/*b .*/
	/* Changes: We support el1 aarch32 not aarch64*/
	.word	0xd51c111f	/*msr	hcr_el2, xzr*/
	.word	0xd53ce100	/*mrs	x0, cnthctl_el2*/
	.word	0xb2400400	/*orr	x0, x0, #0x3*/
	.word	0xd51ce100	/*msr	cnthctl_el2, x0*/
	.word	0xd51ce07f	/*msr	cntvoff_el2, xzr*/
	.word	0xd5380000	/*mrs	x0, midr_el1*/
	.word	0xd53800a1	/*mrs	x1, mpidr_el1*/
	.word	0xd51c0000	/*msr	vpidr_el2, x0*/
	.word	0xd51c00a1	/*msr	vmpidr_el2, x1*/
	/* Changes: SCTLR_EL1 initialization */
	/* Need enable CP15EN, because the unzip in the zImage need it */
	/* Need set WFI/WFE to be 1, or we will have permision fault when enable MMU */
	.word	0xd2810400	/*mov	x0, #0x820*/
	.word	0xf2a618a0	/*movk	x0, #0x30c5, lsl #16*/
	.word	0xd5181000	/*msr	sctlr_el1, x0*/
	.word	0xd2867fe0	/*mov	x0, #0x33ff*/
	.word	0xd51c1140	/*msr	cptr_el2, x0*/
	.word	0xd51c117f	/*msr	hstr_el2, xzr*/
	.word	0xd51c211f	/*msr	vttbr_el2, xzr*/
	/* Changes: Do not set vbar_el2, uboot has set it. remove the code */
	/* Changes: fake EL1 aarch32*/
	.word	0xd2803a60	/*mov	x0, #0x1d3*/
	.word	0xd51c4000	/*msr	spsr_el2, x0*/
	.word	0xaa0503e0	/*mov	x0, x5*/
	.word	0xaa0603e1	/*mov	x1, x6*/
	.word	0x1000007e	/*adr	x30, el2_eret*/
	.word	0xd51c403e	/*msr	elr_el2, x30*/
	.word	0xd69f03e0	/*eret*/
el2_eret:
	.endm
