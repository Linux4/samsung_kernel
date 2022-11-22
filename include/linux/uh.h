#ifndef __UH_H__
#define __UH_H__

#ifndef __ASSEMBLY__

/* For uH Command */
#define	APP_INIT	0
#define	APP_SAMPLE	1
#define APP_RKP		2

#define UH_APP_INIT		UH_APPID(APP_INIT)
#define UH_APP_SAMPLE		UH_APPID(APP_SAMPLE)
#define UH_APP_RKP		UH_APPID(APP_RKP)

#define UH_PREFIX  UL(0xc300c000)
#define UH_APPID(APP_ID)  ((UL(APP_ID) & UL(0xFF)) | UH_PREFIX)

/* For uH Memory */
#define UH_NUM_MEM		0x00

#define UH_LOG_START		0xB1600000
#define UH_LOG_SIZE			0x40000

struct test_case_struct {
	int (* fn)(void); //test case func
	char * describe;
};

int uh_call(u64 app_id, u64 command, u64 arg0, u64 arg1, u64 arg2, u64 arg3);

#else //__ASSEMBLY__
.macro  uh_rkp_emulate_instr, cmdid, srcreg1, srcreg2, srcreg3
	stp	x1, x0, [sp, #-16]!
	stp	x3, x2, [sp, #-16]!
	stp	xzr, x30, [sp, #-16]!
	mov	x2, \srcreg1
	mov	x3, \srcreg2
	mov	x4, \srcreg3
	mov     x0, #0xc002
	movk    x0, #0xc300, lsl #16
	mov	x1, \cmdid
	bl	uh_call
	ldp	xzr, x30, [sp], #16
	ldp	x3, x2, [sp], #16
	ldp	x1, x0, [sp], #16
.endm

#endif //__ASSEMBLY__
#endif //__UH_H__
