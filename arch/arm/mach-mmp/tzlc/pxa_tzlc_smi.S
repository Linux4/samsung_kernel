#define TZLC_FLAG (0xE0000000)

.globl __tzlc_send_req
@ int32_t __tzlc_send_req(tzlc_req_param *param);
__tzlc_send_req:
	stmdb sp!, {r1-r12, lr}
	ldmia r0, {r1-r6}
	mov r7, r0
	ldr r0, =TZLC_FLAG
	.arch_extension sec
	smc #0
	cmp r8, #0
	stmneia r7, {r1-r6}
	ldmia sp!, {r1-r12, pc}
