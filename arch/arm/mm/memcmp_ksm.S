#include <linux/linkage.h>
#include <asm/assembler.h>

.text
.fpu	neon
.arm
.arch	armv7-a

#define CACHE_LINE_SIZE 32

ENTRY(memcmp_ksm)
memcmp_ksm:
	stmfd	sp!, {r4, lr}
	pld	[r0, #(CACHE_LINE_SIZE * 0)]
	pld	[r1, #(CACHE_LINE_SIZE * 0)]
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

	subs	r2, r2, #32
	pld	[r3, #(CACHE_LINE_SIZE * 0)]
	pld	[r1, #(CACHE_LINE_SIZE * 0)]
	pld	[r3, #(CACHE_LINE_SIZE * 1)]
	pld	[r1, #(CACHE_LINE_SIZE * 1)]
	pld	[r3, #(CACHE_LINE_SIZE * 2)]
	pld	[r1, #(CACHE_LINE_SIZE * 2)]

loop:
// q is 128-bit(16bytes), d is 64bit(8bytes), registers are 32bit(4bytes).
	vld1.8	{d0 - d3}, [r3]!             // load 64bit * 4 = 256bit(32bytes)
	pld	[r3, #(CACHE_LINE_SIZE * 2)] // load 32bytes
	vld1.8	{d4 - d7}, [r1]!
	pld	[r1, #(CACHE_LINE_SIZE * 2)]

	vsub.i8	q0, q2
	vsub.i8	q1, q3
	vorr	q2, q0, q1
	vorr	d4, d5      // q2 = [d4, d5]
	vmov	r0, r4, d4  // [r0, r4] = d4
	orrs	r0, r4      // r0 is return value
	popne	{r4, pc}

	subs	r2, r2, #32
	bne	loop

	mov	r0, #0
	ldmfd	sp!, {r4, lr}
	bx	lr
END(memcmp_ksm)
