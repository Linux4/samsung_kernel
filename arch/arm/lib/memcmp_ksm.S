#include <linux/linkage.h>
#include <asm/assembler.h>

.text
.fpu	neon
.arm
#define CACHE_LINE_SIZE 64

ENTRY(page_memcmp)
memcmp_ksm: 
	stmfd	sp!, {r4, lr}
	pld	[r0, #(CACHE_LINE_SIZE * 1)]
	pld	[r1, #(CACHE_LINE_SIZE * 1)]
	mov	r3, r0

	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}

	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}
	ldr	r4, [r3], #4
	ldr	r0, [r1], #4
	subs	r0, r4, r0
	popne	{r4, pc}

	subs	r2, r2, #64

loop:
	pld	[r1, #(CACHE_LINE_SIZE * 1)]
	pld	[r3, #(CACHE_LINE_SIZE * 1)]

	vld1.8	{d0 - d3}, [r3]! 
	vld1.8	{d4 - d7}, [r1]!
	vsub.i8	q0, q2
	vsub.i8	q1, q3
	vorr	q2, q0, q1
	vorr	d4, d5      // q2 = [d4, d5]
	vmov	r0, r4, d4  // [r0, r4] = d4
	orrs	r0, r4      // r0 is return value
	popne	{r4, pc}

	vld1.8	{d0 - d3}, [r3]! 
	vld1.8	{d4 - d7}, [r1]!
	vsub.i8	q0, q2
	vsub.i8	q1, q3
	vorr	q2, q0, q1
	vorr	d4, d5      // q2 = [d4, d5]
	vmov	r0, r4, d4  // [r0, r4] = d4
	orrs	r0, r4      // r0 is return value
	popne	{r4, pc}

	subs	r2, r2, #64
	bne	loop

	mov	r0, #0
	ldmfd	sp!, {r4, lr}
	bx	lr
END(page_memcmp)
