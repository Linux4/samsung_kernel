/*
 * Arm specific backtracing code for MPDC
 *
 * Copyright 2007 Marvell Ltd.
 *
 * Arm oprofile backtrace code by Richard Purdie is referenced.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30))
#include <asm/stacktrace.h>
#endif

#include "PXD_css.h"
#include "ring_buffer.h"
#include "css_drv.h"
#include "common.h"

static pid_t g_current_pid;

DEFINE_SPINLOCK(bt_lock);

const int WRITE_CALL_STACK_ERR = -1;

/* prologue instruction pattern */
#define PROLOG_INSTR1 0xE92DD800   /* stmdb sp!, {fp, ip, lr, pc} */
#define PROLOG_INSTR1_MASK 0xFFFFF800
#define PROLOG_INSTR2 0xE24CB004  /* sub fp, ip, #4*/

#define GET_USERREG() ((struct pt_regs *)(THREAD_START_SP + (unsigned long)current_thread_info()) - 1)

#define SWI_INSTR(inst)                       ((inst & 0x0f000000) == 0x0f000000)
#define BL_INSTR(inst)                        ((inst & 0x0f000000) == 0x0b000000)              // BL
#define BLX_INSTR_reg(inst)                   ((inst & 0x0ffffff0) == 0x012fff30)              // BLX Rm
#define BLX_INSTR_label(inst)                 ((inst & 0xfe000000) == 0xfa000000)              // BLX <label>
#define BX_LR_INSTR(inst)                     ((inst & 0x0fffffff) == 0x012fff1e)              // BX(cond) LR
#define ADD_SP_SP_immed_INSTR(inst)           ((inst & 0xfffff000) == 0xe28dd000)              // ADD SP, SP, #immed
#define SUB_SP_SP_immed_INSTR(inst)           ((inst & 0xfffff000) == 0xe24dd000)              // SUB SP, SP, #immed
#define ADD_SP_FP_immed_INSTR(inst)           ((inst & 0xfffff000) == 0xe28bd000)              // ADD SP, FP, #immed
#define SUB_SP_FP_immed_INSTR(inst)           ((inst & 0xfffff000) == 0xe24bd000)              // SUB SP, FP, #immed
#define LDMIA_SP_WITH_LR_INSTR(inst)          ((inst & 0xffff4000) == 0xe8bd4000)              // LDMIA sp!, {Rx, ... lr}
#define LDMIA_SP_WITH_PC_INSTR(inst)          ((inst & 0xffff8000) == 0xe89d8000)              // LDMIA sp!, {Rx, ... pc}
#define LDMIA_SP_WITHOUT_LR_PC_INSTR(inst)    ((inst & 0xffffc000) == 0xe8bd0000)              // LDMIA sp!, {Rx, ... Ry} (without pc and lr)
#define LDMIA_1_SPBASE_INSTR(inst)            ((inst & 0xffdf0000) == 0xe89d0000)              // LDMIA sp{!}, {Rx, ... Ry}
#define STMDB_1_SPBASE_INSTR(inst)            ((inst & 0xffff0000) == 0xe92d0000)              // STMDB sp!, {Rx, ... Ry}
#define LDR_LR_SPBASE_immed_INSTR(inst)       ((inst & 0xfffff000) == 0xe59de000)              // LDR lr, [sp, #immed]
#define LDR_FP_SPBASE_immed_INSTR(inst)       ((inst & 0xfffff000) == 0xe59db000)              // LDR fp, [sp, #immed]
#define LDR_LR_SPBASE_immed_pre_INSTR(inst)   ((inst & 0xfffff000) == 0xe5bde000)              // LDR lr, [sp, #immed]!
#define LDR_LR_SPBASE_immed_post_INSTR(inst)  ((inst & 0xfffff000) == 0xe49de000)              // LDR lr, [sp], #immed
#define LDR_FP_SPBASE_immed_pre_INSTR(inst)   ((inst & 0xfffff000) == 0xe5bdb000)              // LDR fp, [sp, #immed]!
#define LDR_FP_SPBASE_immed_post_INSTR(inst)  ((inst & 0xfffff000) == 0xe49db000)              // LDR fp, [sp], #immed
#define MOV_PC_LR(inst)                       (inst == 0xe1a0f00e)                             // MOV pc, lr
#define VPOP_INSTR(inst)                      (((inst & 0x0fbf0f00) == 0x0cbd0b00) || ((inst & 0x0fbf0f00) == 0x0cbd0a00))            // VPOP
#define VPUSH_INSTR(inst)                     (((inst & 0x0fbf0f00) == 0x0d2d0b00) || ((inst & 0x0fbf0f00) == 0x0d2d0a00))            // VPUSH

#define SWI_INSTR_THUMB(inst)                 ((inst & 0xff00) == 0xdf00)                      // THUMB: swi
#define BL_INSTR_THUMB(inst1, inst2)          (((inst1 & 0xf800) == 0xf000) && ((inst2 & 0xf800) == 0xf800))              // THUMB: BL <addr>
#define BLX_INSTR_THUMB(inst1, inst2)         (((inst1 & 0xf800) == 0xf000) && ((inst2 & 0xf800) == 0xe800))              // THUMB: BLX <addr>
#define BLX_Rm_INSTR_THUMB(inst)              ((inst & 0xff87) == 0x4780)                      // THUMB: BLX Rm

#define ADD_SP_immed_THUMB_INSTR(inst)        ((inst & 0xff80) == 0xb000)                      // THUMB: ADD SP, #immed
#define SUB_SP_immed_THUMB_INSTR(inst)        ((inst & 0xff80) == 0xb080)                      // THUMB: SUB SP, #immed
#define POP_THUMB_INSTR(inst)                 ((inst & 0xfe00) == 0xbc00)                      // THUMB: POP {Rx, ..., Ry}
#define PUSH_THUMB_INSTR(inst)                ((inst & 0xfe00) == 0xb400)                      // THUMB: PUSH {Rx, ..., Ry}
#define BX_LR_THUMB_INSTR(inst)               ((inst & 0xffff) == 0x4770)                      // THUMB: BX LR
#define BX_THUMB_INSTR(inst)                  ((inst & 0xffc7) == 0x4700)                      // THUMB: BX Rm
#define POP_WITH_PC_THUMB_INSTR(inst)         ((inst & 0xff00) == 0xbd00)                      // THUMB: POP {Rx, ..., pc}
#define MOV_PC_Rm_THUMB_INSTR(inst)           ((inst & 0xff87) == 0x4687)                      // THUMB: MOV PC, Rm

#define POP_THUMB32_INSTR(high, low)           ((high & 0xffff) == 0xe8bd)                                    // THUMB32: POP {Rx, ..., Ry}
#define POP_ONE_REG_THUMB32_INSTR(high, low)   (((high & 0xffff) == 0xf85d) && ((low & 0x0fff) == 0xb04))     // THUMB32: POP {Rx}
#define PUSH_THUMB32_INSTR(high, low)          ((high & 0xffff) == 0xe92d)                                    // THUMB32: PUSH {Rx, ..., Ry}
#define PUSH_ONE_REG_THUMB32_INSTR(high, low)  (((high & 0xffff) == 0xf84d) && ((low & 0x0fff) == 0xd04))     // THUMB32: PUSH {Rx}
#define ADDS_SP_imm_THUMB32_INSTR(high, low)   (((high & 0xfbef) == 0xf10d) && ((low & 0x8f00) == 0x0d00))    // THUMB32: ADD{S}.W SP, SP, #const
#define ADDW_SP_imm12_THUMB32_INSTR(high, low) (((high & 0xfbff) == 0xf20d) && ((low & 0x8f00) == 0x0d00))    // THUMB32: ADDW SP, SP, #imm12
#define VPOP_THUMB32_INSTR(high, low)          (((high & 0xffbf) == 0xecbd) && (((low & 0x0f00) == 0x0b00) || ((low & 0x0f00) == 0x0a00)))    // THUMB32: VPOP
#define VPUSH_THUMB32_INSTR(high, low)         (((high & 0xffbf) == 0xed2d) && (((low & 0x0f00) == 0x0b00) || ((low & 0x0f00) == 0x0a00)))    // THUMB32: VPUSH

bool ldm_stm_inst_update_base_reg(u32 inst)
{
	return (inst & 0x00200000);
}

//extern struct px_css_dsa *g_dsa;

struct unwind_regs
{
	unsigned int pc;
	unsigned int sp;
	unsigned int fp;
	unsigned int lr;
} __attribute__((packed));

/*
 * The registers we're interested in are at the end of the variable
 * length saved register structure. The fp points at the end of this
 * structure so the address of this struct is:
 * (struct frame_tail_1 *)(xxx->fp)-1
 */
struct frame_tail_1 {
	unsigned long fp;
	unsigned long sp;
	unsigned long lr;
} __attribute__((packed));

struct frame_tail_2 {
	unsigned long fp;
	unsigned long lr;
} __attribute__((packed));

/* metrics support */
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30))
struct stackframe {
	unsigned long fp;
	unsigned long sp;
	unsigned long lr;
	unsigned long pc;
};
#endif

static bool is_thumb_address(unsigned int address)
{
	if (address & 0x1)
		return true;
	else
		return false;
}

static unsigned int arm_inst_size(void)
{
	return sizeof(u32);
}

static inline int valid_kernel_stack(unsigned long fp, struct pt_regs *regs)
{
	unsigned long stack = (unsigned long)regs;
	unsigned long stack_base = (stack & ~(THREAD_SIZE - 1)) + THREAD_SIZE;

	return (fp > stack) && (fp < stack_base) && (fp > regs->ARM_sp);
}

static inline int valid_user_stack(struct frame_tail_1 *tail, struct pt_regs *regs)
{
	/* check accessibility of one struct frame_tail_1 beyond.
	 *  Normally tail should be greater than sp. But for the first stack frame,
	 *  there is possibility that they are equal.
	 */
	return tail
		&& !((unsigned long) tail & 3)
		&& ((unsigned long) tail >= regs->ARM_sp)
		&& access_ok(VERIFY_READ, tail, sizeof(struct frame_tail_1) * 2);
}


/* Check if the interruptted instruction is in the prologue of a function
 * where call frame has not been pushed on to the stack.
 */
static inline int in_prologue(u32 instrs[2])
{
	return (   ((instrs[0] & PROLOG_INSTR1_MASK) == PROLOG_INSTR1)
	        || ((instrs[1] & PROLOG_INSTR1_MASK) == PROLOG_INSTR1)
			|| (instrs[0] == PROLOG_INSTR2));
}

static bool read_user_data(unsigned int address, void * data, unsigned int size)
{
	if (!access_ok(VERIFY_READ, address, size))
	{
		return false;
	}

	if (__copy_from_user_inatomic(data, (void *)address, size))
	{
		return false;
	}

	return true;
}

static bool read_kernel_data(unsigned int address, void * data, unsigned int size)
{
	memcpy(data, (void *)address, size);
	
	return true;
}

static bool read_data(unsigned int address, void * data, unsigned int size)
{
	if (address < TASK_SIZE)
		return read_user_data(address, data, size);
	else
		return read_kernel_data(address, data, size);
}

static int get_inst_size(unsigned int address)
{
	unsigned int data;

	if (is_thumb_address(address))
	{
		/* Thumb instruction */
		data = 0;

		if (!read_data(address & ~0x1, &data, THUMB16_INST_SIZE))
			return -1;

		if (((data & 0xe000) == 0xe000) && ((data & 0x1800) != 0))
		{
			/* it is a Thumb 32 bit instruction */
			return THUMB32_INST_SIZE;
		}
		else
		{
			/* it is a Thumb 16 bit instruction */
			return THUMB16_INST_SIZE;
		}
	}
	else
	{
		/* ARM instruction */
		return ARM_INST_SIZE;
	}

}

static bool read_instruction(unsigned int address, unsigned int * inst, unsigned int * inst_size)
{
	unsigned int size;
	
	if (inst == NULL)
		return false;

	*inst = 0;
	
	size = get_inst_size(address);
	
	if (size < 0)
		return false;

	if (inst_size != NULL)
	{
		*inst_size = size;
	}

	address = address & ~0x1;

	return read_data(address, inst, size);
}

#if 0
static bool get_last_inst(unsigned int pc, unsigned int *inst, unsigned int *inst_size)
{
	unsigned int data;
	unsigned int size;

	if (!read_instruction(pc - ARM_INST_SIZE, &data, &size))
	{
		return false;
	}

	if (size == THUMB16_INST_SIZE)
	{
		if (!read_instruction(pc - THUMB16_INST_SIZE, &data, &size))
		{
			return false;
		}
	}

	if (inst != NULL)
	{
		*inst = data;
	}
	
	if (inst_size != NULL)
	{
		*inst_size = size;
	}
	
	return true;
}
#endif

/* add css data to buffer, return true if the data has been successfully added to the buffer */
static void add_css_data(unsigned int cpu, PXD32_CSS_Call_Stack_V2 * css_data, u32 pc, unsigned int pid)
{
	unsigned int depth;

	depth = css_data->depth;
	
	css_data->cs[depth].address = pc & ~0x1;
	css_data->cs[depth].pid     = pid;
	
	css_data->depth++;
}

struct report_trace_data
{
	unsigned int cpu;
	pid_t        pid;
	pid_t        tid;
	PXD32_CSS_Call_Stack_V2 * css_data;
};

static int kernel_report_trace(struct stackframe *frame, void *d)
{
	struct report_trace_data *data = (struct report_trace_data *)d;

	if (data != NULL)
	{
		PXD32_CSS_Call_Stack_V2 *css_data = data->css_data;

		add_css_data(data->cpu, css_data, frame->pc, data->pid);

		if (css_data->depth >= MAX_CSS_DEPTH)
		{
			return 1;
		}
	}

	return 0;
}

#if 0
int unwind_frame(struct stackframe *frame)
{
	unsigned long high, low;
	unsigned long fp = frame->fp;

	/* only go to a higher address on the stack */
	low = frame->sp;
	high = ALIGN(low, THREAD_SIZE) + THREAD_SIZE;

	/* check current frame pointer is within bounds */
	if (fp < (low + 12) || fp + 4 >= high)
		return -EINVAL;

	/* restore the registers from the stack frame */
	frame->fp = *(unsigned long *)(fp - 12);
	frame->sp = *(unsigned long *)(fp - 8);
	frame->pc = *(unsigned long *)(fp - 4);

	return 0;
}
#endif

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30))
static void walk_stackframe(struct stackframe *frame, int (*fn)(struct stackframe *, void *), void *data)
{
	while (1)
	{
		int ret;

		if (fn(frame, data))
			break;

		ret = unwind_frame(frame);
		if (ret < 0)
			break;
	}
}
#endif

static unsigned int kernel_backtrace(struct pt_regs * const regs, unsigned int cpu, pid_t pid, pid_t tid, PXD32_CSS_Call_Stack_V2 * css_data)
{
	struct report_trace_data data;
	struct stackframe frame;

	data.pid   = pid;
	data.tid   = tid;
	data.cpu   = cpu;
	data.css_data = css_data;

	frame.fp = regs->ARM_fp;
	frame.sp = regs->ARM_sp;
	frame.lr = regs->ARM_lr;
	frame.pc = regs->ARM_pc;

	walk_stackframe(&frame, kernel_report_trace, &data);
	
	/* when it returns from walk_stackframe, the last stack frame data is not correct */
	if (data.css_data->depth > 1)
		data.css_data->depth--;

	return data.css_data->depth;
}

static unsigned int rotr(unsigned int value, int shift, int value_bit_num)
{
	return ((value >> shift) | (value << (value_bit_num - shift)));
}

static unsigned int get_immed_operand_for_data_proc_instr(unsigned int instr)
{
	u8 immed_8, rotate_imm;

	immed_8 = instr & 0xff;
	rotate_imm = instr & 0xf00;

	return rotr(immed_8, 2 * rotate_imm, 8);
}

/*
 * emulate executing one THUMB 16 bit instruction
 * return true if we need to continue to emulate the next instruction
 * return false if we need NOT to continue to emulate the next instrution
 */
static bool thumb16_unwind_emul_one_inst(u16 inst, struct unwind_regs *frame, u32 *regs)
{
	bool ret = true;
	u32 sp, fp, lr, pc;

	sp = frame->sp;
	fp = frame->fp;
	lr = frame->lr;
	pc = frame->pc;

	if (ADD_SP_immed_THUMB_INSTR(inst))                 // ADD SP, #immed
	{
		u8 immed;

		immed = inst & 0x7f;

		sp += (immed << 2);

	}
	else if (SUB_SP_immed_THUMB_INSTR(inst))            // SUB SP, #immed
	{
		u8 immed;

		immed = inst & 0x7f;

		sp -= (immed << 2);
	}
	else if (POP_THUMB_INSTR(inst))                     // POP {Rx, ..., Ry}
	{
		int i;

		for (i=0; i<8; i++)
		{
			if ((inst & 0xff) & (0x1 << i))
			{
				if (!read_user_data(sp, &regs[i], sizeof(u32)))
					return false;

				sp += 4;
			}
		}

		if (inst & 0x0100)
		{
			if (!read_user_data(sp, &pc, sizeof(u32)))
			{
				return false;
			}

			sp += 4;

			ret = false;
			goto end;
		}
	}
	else if (PUSH_THUMB_INSTR(inst))                     // PUSH {Rx, ..., Ry}
	{
		int i;

		for (i=0; i<=8; i++)
		{
			if ((inst & 0x1ff) & (0x1 << i))
			{
				sp -= 4;
			}
		}
	}
	else if (BX_LR_THUMB_INSTR(inst))                    // BX LR
	{
		pc = lr;

		ret = false;
		goto end;
	}
	else if (MOV_PC_Rm_THUMB_INSTR(inst))                // mov pc, Rx
	{
		int reg_num;

		reg_num = (inst & 0x78) >> 3;

		pc = regs[reg_num];

		ret = false;
		goto end;
	}
	else if (BX_THUMB_INSTR(inst))                       // BX Rm
	{
		int reg_num;

		reg_num = (inst & 0x78) >> 3;

		pc = regs[reg_num];

		ret = false;
		goto end;
	}

end:
	frame->fp = fp;
	frame->sp = sp;
	frame->lr = lr;
	frame->pc = pc;
	
	return ret;
}

/*
 * emulate executing one THUMB 32 bit instruction
 * return true if we need to continue to emulate the next instruction
 * return false if we need NOT to continue to emulate the next instrution
 */
static bool thumb32_unwind_emul_one_inst(u16 high_inst, u16 low_inst, struct unwind_regs *frame, u32 *regs)
{
	bool ret = true;
	u32 sp, fp, lr, pc;

	sp = frame->sp;
	fp = frame->fp;
	lr = frame->lr;
	pc = frame->pc;

	if (POP_THUMB32_INSTR(high_inst, low_inst))
	{
		int i;

		for (i=0; i<16; i++)
		{
			if (low_inst & (0x1 << i))
			{
				if (!read_user_data(sp, &regs[i], sizeof(u32)))
					return false;

				sp += 4;
			}

		}

		/* lr is in the list */
		if (low_inst & 0x4000)
		{
			lr = regs[14];
		}

		/* pc is in the list */
		if (low_inst & 0x8000)
		{
			pc = regs[15];
			
			ret = false;
			goto end;
		}
	}
	else if (POP_ONE_REG_THUMB32_INSTR(high_inst, low_inst))
	{
		unsigned int rt;
		unsigned int rt_value;
		
		rt = px_get_bits(low_inst, 12, 15);
		
		if (!read_user_data(sp, &rt_value, sizeof(u32)))
			return false;
		
		if (rt == 14)
		{
			lr = rt_value;
		}

		if (rt == 15)
		{
			pc = rt_value;

			ret = false;
			goto end;
		}
		
		sp += 4;
	}
	else if (PUSH_THUMB32_INSTR(high_inst, low_inst))
	{
		int i;

		for (i=0; i<16; i++)
		{
			if (low_inst & (0x1 << i))
			{
				sp -= 4;
			}

		}
	}
	else if (PUSH_ONE_REG_THUMB32_INSTR(high_inst, low_inst))
	{
		sp -= 4;
	}
	else if (ADDS_SP_imm_THUMB32_INSTR(high_inst, low_inst))
	{
		unsigned int i, imm3, imm8, imm32;
		
		i    = px_get_bit(high_inst, 10);
		imm3 = px_get_bits(low_inst, 12, 14);
		imm8 = px_get_bits(low_inst, 0, 7);
		
		imm32 = thumb_expand_imm((i << 11) | (imm3 << 8) | imm8);
		
		sp += imm32;
	}
	else if (ADDW_SP_imm12_THUMB32_INSTR(high_inst, low_inst))
	{
		unsigned int i, imm3, imm8, imm32;
		
		i    = px_get_bit(high_inst, 10);
		imm3 = px_get_bits(low_inst, 12, 14);
		imm8 = px_get_bits(low_inst, 0, 7);
		
		imm32 = (i << 11) | (imm3 << 8) | imm8;
		sp += imm32;
	}
	else if (VPOP_THUMB32_INSTR(high_inst, low_inst))
	{
		unsigned int imm8;

		imm8 = px_get_bits(low_inst, 0, 7);
		
		if (px_get_bits(low_inst, 8, 11) == 0xa)
		{
			sp += (imm8 / 2) * sizeof(u32);
		}
		else
		{
			sp += (imm8 / 2) * sizeof(u64);
		}
	}
	else if (VPUSH_THUMB32_INSTR(high_inst, low_inst))
	{
		unsigned int imm8;
		
		imm8 = px_get_bits(low_inst, 0, 7);
		
		if (px_get_bits(low_inst, 8, 11) == 0xa)
		{
			sp -= (imm8 / 2) * sizeof(u32);
		}
		else
		{
			sp -= (imm8 / 2) * sizeof(u64);
		}
	}
end:
	frame->fp = fp;
	frame->sp = sp;
	frame->lr = lr;
	frame->pc = pc;
	
	return ret;
}

static bool thumb_unwind(struct unwind_regs *frame, unsigned int depth)
{
	u32 pc, sp, fp, lr;
	u32 cur;

	u32 reg_data[16];
	int count;
	unsigned int inst_size;
	u32 inst;
	struct unwind_regs regs;

	pc = frame->pc;
	sp = frame->sp;
	fp = frame->fp;
	lr = frame->lr;

	count = 0;

	cur = pc;

	regs.fp = frame->fp;
	regs.lr = frame->lr;
	regs.pc = frame->pc;
	regs.sp = frame->sp;

	while (true)
	{
		//u32 old_sp;
		bool continue_emul;
		
		if (!read_instruction(cur, &inst, &inst_size))
		{
			return false;
		}
#if 0
		if (count++ >= 0xffff)
		{
			return false;
		}
#endif
		regs.pc = cur;
		
		if (inst_size == THUMB16_INST_SIZE)
		{
			continue_emul = thumb16_unwind_emul_one_inst(inst, &regs, reg_data);
		}
		else
		{
			continue_emul = thumb32_unwind_emul_one_inst(inst & 0xffff, inst >> 16, &regs, reg_data);
		}

		if (!continue_emul)
		{
			break;
		}

		cur += inst_size;
	}

	frame->pc = regs.pc;
	frame->sp = regs.sp;
	frame->fp = regs.fp;
	frame->lr = regs.lr;
	
	return true;
}

static bool arm_unwind(struct unwind_regs *frame, unsigned int depth)
{
	u32 fp, sp, pc, lr;
	u32 cur;
	
	pc = frame->pc;
	sp = frame->sp;
	fp = frame->fp;
	lr = frame->lr;

	cur = pc;

	while (true)
	{
		u32 instr;

		if (!read_instruction(cur, &instr, NULL))
		{
			return false;
		}
#if 0
		if (count++ >= 0xffff)
		{
			return false;
		}
#endif
		if (ADD_SP_SP_immed_INSTR(instr))               // ADD SP, SP #immed
		{
			u8 shifter_operand;

			shifter_operand = get_immed_operand_for_data_proc_instr(instr);

			sp += shifter_operand;
		}
		else if (LDMIA_1_SPBASE_INSTR(instr))                 // LDMIA SP{!}, {Rx, ..., Ry}
		{
			int i;
			u32 regs[16];
			bool update_base_reg;
			u32 address;
			
			memset(regs, 0, sizeof(regs));

			update_base_reg = ldm_stm_inst_update_base_reg(instr);

			address = sp;

			for (i=0; i<=15; i++)
			{
				if (instr & (1 << i))
				{
					if (!read_user_data(address, &regs[i], arm_inst_size()))
					{
						return false;
					}

					address += arm_inst_size();
				}
			}

			if (update_base_reg)                // SP is base register and will be updated
			{
				sp += px_get_bit_count(instr & 0xffff) * arm_inst_size();
			}

			if (instr & (1 << 11))
			{
				fp = regs[11];
			}

			if (instr & (1 << 13))
			{
				sp = regs[13];
			}

			if (instr & (1 << 14))
			{
				lr = regs[14];
			}

			if (instr & (1 << 15))
			{
				pc = regs[15];
				break;
			}

		}
		else if (SUB_SP_SP_immed_INSTR(instr))           // SUB   SP, SP, #immed
		{
			u8 shifter_operand;

			shifter_operand = get_immed_operand_for_data_proc_instr(instr);

			sp -= shifter_operand;
		}
		else if (STMDB_1_SPBASE_INSTR(instr))                  // STMDB SP!, {Rx, ..., Ry}
		{
			sp -= px_get_bit_count(instr & 0xffff) * arm_inst_size();
		}
		else if (SUB_SP_FP_immed_INSTR(instr))           // SUB SP, FP, #immed
		{
			u8 shifter_operand;

			shifter_operand = get_immed_operand_for_data_proc_instr(instr);

			sp = fp - shifter_operand;
		}
		else if (ADD_SP_FP_immed_INSTR(instr))           // ADD SP, FP, #immed
		{
			u8 shifter_operand;

			shifter_operand = get_immed_operand_for_data_proc_instr(instr);

			sp = fp + shifter_operand;
		}
		else if (LDR_LR_SPBASE_immed_INSTR(instr))       // LDR lr, [sp, #immed]
		{
			u16 immed;

			immed = instr & 0xfff;

			if (!read_user_data(sp + immed, &lr, arm_inst_size()))
			{
				return false;
			}

		}
		else if (LDR_FP_SPBASE_immed_INSTR(instr))       // LDR fp, [sp, #immed]
		{
			u16 immed;

			immed = instr & 0xfff;

			if (!read_user_data(sp + immed, &fp, arm_inst_size()))
			{
				return false;
			}
		}
		else if (LDR_LR_SPBASE_immed_pre_INSTR(instr))  // LDR lr, [sp, #immed]!
		{
			u16 immed;

			immed = instr & 0xfff;

			sp += immed;

			if (!read_user_data(sp, &lr, arm_inst_size()))
			{
				return false;
			}
		}
		else if (LDR_FP_SPBASE_immed_pre_INSTR(instr))  // LDR fp, [sp, #immed]!
		{
			u16 immed;

			immed = instr & 0xfff;

			sp += immed;

			if (!read_user_data(sp, &fp, arm_inst_size()))
			{
				return false;
			}
		}
		else if (LDR_LR_SPBASE_immed_post_INSTR(instr))  // LDR lr, [sp], #immed
		{
			u16 immed;

			immed = instr & 0xfff;

			if (!read_user_data(sp, &lr, arm_inst_size()))
			{
				return false;
			}

			sp += immed;
		}
		else if (LDR_FP_SPBASE_immed_post_INSTR(instr))  // LDR fp, [sp], #immed
		{
			u16 immed;

			immed = instr & 0xfff;

			if (!read_user_data(sp, &fp, arm_inst_size()))
			{
				return false;
			}

			sp += immed;
		}
		else if (BX_LR_INSTR(instr))                     // BX lr
		{
			pc = lr;

			break;
		}
		else if (MOV_PC_LR(instr))                       // MOV  PC, LR
		{
			pc = lr;
			break;
		}
		else if (VPOP_INSTR(instr))
		{
			unsigned int imm8;
			
			imm8 = px_get_bits(instr, 0, 7);
			
			if (px_get_bits(instr, 8, 11) == 0xa)
			{
				sp += (imm8 / 2) * sizeof(u32);
			}
			else
			{
				sp += (imm8 / 2) * sizeof(u64);
			}
		}
		else if (VPUSH_INSTR(instr))
		{
			unsigned int imm8;
			
			imm8 = px_get_bits(instr, 0, 7);
			
			sp -= imm8 / 2;	

			if (px_get_bits(instr, 8, 11) == 0xa)
			{
				sp -= (imm8 / 2) * sizeof(u32);
			}
			else
			{
				sp -= (imm8 / 2) * sizeof(u64);
			}
		}

		cur += arm_inst_size();
	}

	frame->pc = pc;
	frame->sp = sp;
	frame->fp = fp;
	frame->lr = lr;
	
	return true;
}

static bool in_user_space(unsigned int address)
{
	return (address > 0) && (address < TASK_SIZE);
}

static unsigned int user_backtrace(struct pt_regs * const regs, unsigned int cpu, pid_t pid, pid_t tid, unsigned int kernel_css_depth, PXD32_CSS_Call_Stack_V2 *css_data)
{
	unsigned int pc, sp, lr, fp;
	unsigned int prev_sp = 0;

	unsigned int depth = 0;
	struct unwind_regs frame;

	bool ret;

	if (!in_user_space(regs->ARM_pc))
	{
		return 0;
	}

	pc = regs->ARM_pc;
	sp = regs->ARM_sp;
	lr = regs->ARM_lr;
	fp = regs->ARM_fp;

	if (regs->ARM_cpsr & 0x20)            // It is an Thumb instruction when interrupt is triggered
	{
		// we need to set the lowest bit of the pc
		// to make sure it will be handled as a thumb instrution
		pc |= 0x1;
	}

	g_current_pid = pid;

	do
	{
		if (depth >= 1)
		{
			prev_sp = sp;
		}

		add_css_data(cpu, css_data, pc, pid);

		depth++;

		if (depth >= MAX_CSS_DEPTH - kernel_css_depth)
		{
			break;
		}

		frame.pc = pc;
		frame.sp = sp;
		frame.fp = fp;
		frame.lr = lr;

		if (is_thumb_address(pc))
			ret = thumb_unwind(&frame, depth);
		else
			ret = arm_unwind(&frame, depth);

		if (!ret)
		{
			return depth;
		}
		
		if ((frame.sp <= prev_sp) || !access_ok(VERIFY_READ, sp, sizeof(unsigned int)) || !in_user_space(frame.pc))
			return depth;

		sp = frame.sp;
		pc = frame.pc;
		fp = frame.fp;
		lr = frame.lr;
	} while (true);

	return depth;
}

void fill_css_data_head(PXD32_CSS_Call_Stack_V2 *css_data, pid_t pid, pid_t tid, unsigned int reg_id, unsigned long long ts)
{
	css_data->timestamp.low  = ts & 0xffffffff;
	css_data->timestamp.high = ts >> 32;
	css_data->pid            = pid;
	css_data->tid            = tid;
	css_data->registerId     = reg_id;
}

/*
 * return value:
 *      true:    buffer is full
 *      false:   buffer is not full
 */
bool write_css_data(unsigned int cpu, PXD32_CSS_Call_Stack_V2 * css_data)
{
	bool buffer_full;
	bool need_flush;
	unsigned int size;
	unsigned int depth;
	struct RingBufferInfo * sample_buffer = &per_cpu(g_sample_buffer_css, cpu);
	
	depth = css_data->depth;
	
	if (depth >= 1)
	{
		size = sizeof(PXD32_CSS_Call_Stack_V2) + (depth - 1) * sizeof(PXD32_CSS_Call);
	}
	else
	{
		/* for some reason, there is no call stack data saved */
		return false;
	}

	spin_lock(&bt_lock);
	write_ring_buffer(&sample_buffer->buffer, css_data, size, &buffer_full, &need_flush);
	spin_unlock(&bt_lock);
	
	if (need_flush && !sample_buffer->is_full_event_set)
	{
		sample_buffer->is_full_event_set = true;

		if (waitqueue_active(&pxcss_kd_wait))
			wake_up_interruptible(&pxcss_kd_wait);
	}

	return buffer_full;
}

#define GET_USERREG() ((struct pt_regs *)(THREAD_START_SP + (unsigned long)current_thread_info()) - 1)

void backtrace(struct pt_regs * const orig_regs, unsigned int cpu, pid_t pid, pid_t tid, PXD32_CSS_Call_Stack_V2 * css_data)
{
	unsigned int u_depth = 0;
	unsigned int k_depth = 0;
	struct pt_regs * regs = orig_regs;
	
	css_data->depth = 0;

	/* if it is a swapper process, we don't backtrace it. Or it will result in bug CPA-310 */
	if (current->tgid == 0)
	{
		css_data->depth = 1;
		css_data->cs[0].address = regs->ARM_pc;
		css_data->cs[0].pid     = pid;

		return;
	}
	
	/* kernel space call stack backtrace */
	if (!user_mode(regs))
	{
		k_depth = kernel_backtrace(regs, cpu, pid, tid, css_data);

		if (current->mm == NULL)
		{
			/* if it is a kernel thread, there is no need to do user backtrace */
			regs = NULL;
		}
		else
		{
			regs = task_pt_regs(current);
			//regs = GET_USERREG();
		}
			
	}

	if (regs != NULL)
		u_depth = user_backtrace(regs, cpu, pid, tid, k_depth, css_data);

	g_sample_count_css++;
}

