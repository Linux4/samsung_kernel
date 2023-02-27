#include <linux/linkage.h>
#include <asm/assembler.h>

		.text

sum		.req	r0
src		.req	lr
data1	.req	r1
data2	.req	r2
data3	.req	r3
data4	.req	r4
limit		.req	r5

#define CACHE_LINE_SIZE 32
#define PAGE_SIZE	4096

ENTRY(calc_pagesum)
		pld	[r0, #(CACHE_LINE_SIZE * 1)]
		pld	[r0, #(CACHE_LINE_SIZE * 2)]
		stmfd	sp!, {r4, r5, lr}
		mov		src, r0
		add		limit, src, #PAGE_SIZE
		ldmia	src!, {data1, data2, data3, data4}
		adds	sum, data1, data2
		adcs	sum, sum, data3
		adcs	sum, sum, data4

		ldmia	src!, {data1, data2, data3, data4}
		adcs	sum, sum, data1
		adcs	sum, sum, data2
		adcs	sum, sum, data3
		adcs	sum, sum, data4

.loop_sum:
		pld	[r0, #(CACHE_LINE_SIZE * 2)]
		ldmia	src!, {data1, data2, data3, data4}
		adcs	sum, sum, data1
		adcs	sum, sum, data2
		adcs	sum, sum, data3
		adcs	sum, sum, data4

		ldmia	src!, {data1, data2, data3, data4}
		adcs	sum, sum, data1
		adcs	sum, sum, data2
		adcs	sum, sum, data3
		adcs	sum, sum, data4

		cmp		src, limit
		bne		.loop_sum

		ldmfd	sp!, {r4, r5, lr}
		bx		lr

ENDPROC(calc_pagesum)
