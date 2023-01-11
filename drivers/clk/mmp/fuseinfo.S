#include <linux/errno.h>
#include <linux/linkage.h>
#include <asm/asm-offsets.h>
#include <asm/assembler.h>
#ifdef CONFIG_ARM
#include <asm/opcodes-sec.h>
#endif

/*
 *  int smc_get_fuse_info(u64 function_id, void *arg)
 */
#ifdef CONFIG_ARM64
ENTRY(smc_get_fuse_info)
	sub 	sp, sp, #16
	str 	x19, [sp]
	mov	x19, x1
	smc	#0
	str	w1, [x19], #4
	str	w2, [x19], #4
	str	w3, [x19], #4
	str	w4, [x19], #4
	str	w5, [x19], #4
	str	w6, [x19], #4
	str	w7, [x19], #4
	ldr 	x19, [sp]
	add 	sp, sp, #16
	ret
ENDPROC(smc_get_fuse_info)
#else
ENTRY(smc_get_fuse_info)
	stmfd	sp!, {r4, r5, r6, r7, r8}
	mov     r8, r1
	__SMC(0)
	str	r1, [r8], #4
	str	r2, [r8], #4
	str	r3, [r8], #4
	str	r4, [r8], #4
	str	r5, [r8], #4
	str	r6, [r8], #4
	str	r7, [r8], #4
	ldmfd	sp!, {r4, r5, r6, r7, r8}
	mov	pc, lr
ENDPROC(smc_get_fuse_info)
#endif
