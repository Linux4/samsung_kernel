/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_CE_EXCEP_H
#define APU_CE_EXCEP_H

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define apusys_ce_aee_warn(module, reason) \
	do { \
		char mod_name[150];\
		if (snprintf(mod_name, 150, "%s_%s", reason, module) > 0) { \
			dev_info(dev, "%s: %s\n", reason, module); \
			aee_kernel_exception(mod_name, \
				"\nCRDISPATCH_KEY:%s\n", module); \
		} else { \
			dev_info(dev, "%s: snprintf fail(%d)\n", __func__, __LINE__); \
		} \
	} while (0)

	#define apusys_ce_exception_aee_warn(module) \
	do { \
		dev_info(dev, "APUSYS_CE_EXCEPTION: %s\n", module); \
		aee_kernel_exception("APUSYS_CE_EXCEPTION_APUSYS_CE", \
			"\nCRDISPATCH_KEY:%s\n", module); \
	} while (0)
#else
#define apusys_ce_aee_warn(module, reason)
#define apusys_ce_exception_aee_warn(module, reason)

#endif

uint32_t apu_ce_reg_dump(struct mtk_apu *apu);
uint32_t apu_ce_sram_dump(struct mtk_apu *apu);
int apu_ce_excep_init(struct platform_device *pdev, struct mtk_apu *apu);
void apu_ce_excep_remove(struct platform_device *pdev, struct mtk_apu *apu);
void apu_ce_start_timer_dump_reg(void);
void apu_ce_stop_timer_dump_reg(void);


#define APU_ARE_SRAMBASE                 (0x190a0000)
#define APU_ARE_REG_BASE                 (0x190b0000)
#define APU_ACE_SW_REQ_0                      (0x400)
#define APU_ACE_SW_REQ_1	                  (0x404)
#define APU_ACE_SW_REQ_2	                  (0x408)
#define APU_ACE_SW_REQ_3	                  (0x40c)
#define APU_ACE_SW_REQ_4	                  (0x410)
#define APU_ACE_SW_REQ_5	                  (0x414)
#define APU_ACE_SW_REQ_6	                  (0x418)
#define APU_ACE_SW_REQ_7	                  (0x41c)
#define APU_ACE_SW_REQ_0_USER	              (0x420)
#define APU_ACE_SW_REQ_1_USER	              (0x424)
#define APU_ACE_SW_REQ_2_USER	              (0x428)
#define APU_ACE_SW_REQ_3_USER	              (0x42c)
#define APU_ACE_SW_REQ_4_USER	              (0x430)
#define APU_ACE_SW_REQ_5_USER	              (0x434)
#define APU_ACE_SW_REQ_6_USER	              (0x438)
#define APU_ACE_SW_REQ_7_USER	              (0x43c)
#define APU_ACE_CMD_Q_STATUS_0	              (0x440)
#define APU_ACE_CMD_Q_STATUS_1	              (0x444)
#define APU_ACE_CMD_Q_STATUS_2	              (0x448)
#define APU_ACE_CMD_Q_STATUS_3	              (0x44c)
#define APU_ACE_CMD_Q_STATUS_4	              (0x450)
#define APU_ACE_CMD_Q_STATUS_5	              (0x454)
#define APU_ACE_CMD_Q_STATUS_6	              (0x458)
#define APU_ACE_CMD_Q_STATUS_7	              (0x45c)
#define APU_ACE_SW_DONE_FLAG_ALL	          (0x480)
#define APU_ACE_SW_ORDER_FLAG	              (0x484)
#define APU_ACE_SW_OCCUPIED_FLAG	          (0x488)
#define APU_ACE_SW_CMD_Q_0_DONE_FLAG	      (0x490)
#define APU_ACE_SW_CMD_Q_1_DONE_FLAG	      (0x494)
#define APU_ACE_SW_CMD_Q_2_DONE_FLAG	      (0x498)
#define APU_ACE_SW_CMD_Q_3_DONE_FLAG	      (0x49c)
#define APU_ACE_SW_CMD_Q_4_DONE_FLAG	      (0x4a0)
#define APU_ACE_SW_CMD_Q_5_DONE_FLAG	      (0x4a4)
#define APU_ACE_SW_CMD_Q_6_DONE_FLAG	      (0x4a8)
#define APU_ACE_SW_CMD_Q_7_DONE_FLAG	      (0x4ac)
#define APU_ACE_HW_JOB_BITMAP_0	              (0x4d0)
#define APU_ACE_HW_JOB_BITMAP_4	              (0x4d4)
#define APU_ACE_HW_JOB_BITMAP_8	              (0x4d8)
#define APU_ACE_HW_JOB_BITMAP_12	          (0x4dc)
#define APU_ACE_ABN_IRQ_FLAG_CE	              (0x4f0)
#define APU_ACE_ABN_IRQ_MASK_CE	              (0x4f8)
#define APU_ACE_ABN_IRQ_FLAG_ACE_SW	          (0x500)
#define APU_ACE_ABN_IRQ_FLAG_USER	          (0x504)
#define APU_ACE_ABN_IRQ_MASK_ACE_SW	          (0x508)
#define APU_ACE_ABN_IRQ_MASK_USER	          (0x50c)
#define APU_ACE_CE0_TASK_ING	              (0x510)
#define APU_ACE_CE1_TASK_ING	              (0x514)
#define APU_ACE_CE2_TASK_ING	              (0x518)
#define APU_ACE_CE3_TASK_ING	              (0x51c)
#define APU_ACE_CE0_TASK_ING_USER	          (0x520)
#define APU_ACE_CE1_TASK_ING_USER	          (0x524)
#define APU_ACE_CE2_TASK_ING_USER	          (0x528)
#define APU_ACE_CE3_TASK_ING_USER	          (0x52c)
#define APU_ACE_CE_CTRL_STATUS_0	          (0x530)
#define APU_ACE_MIRRORED_TIMESTAMP_0	      (0x538)
#define APU_ACE_MIRRORED_TIMESTAMP_1	      (0x53c)
#define APU_ACE_APB_MST_OUT_STATUS	          (0x540)
#define APU_ACE_APB_MST_OUT_STATUS_ERR	      (0x544)
#define APU_ACE_APB_MST_IN_STATUS	          (0x548)
#define APU_ACE_APB_MST_IN_STATUS_ERR	      (0x54C)
#define APU_ACE_HW_PEND_FLAG_0	              (0x550)
#define APU_ACE_HW_OCCUPIED_FLAG_0	          (0x558)
#define APU_ACE_HW_JOB_0_USER	              (0x560)
#define APU_ACE_HW_JOB_1_USER	              (0x564)
#define APU_ACE_HW_JOB_2_USER	              (0x568)
#define APU_ACE_HW_JOB_3_USER	              (0x56c)
#define APU_ACE_HW_JOB_4_USER	              (0x570)
#define APU_ACE_HW_JOB_5_USER	              (0x574)
#define APU_ACE_HW_JOB_6_USER	              (0x578)
#define APU_ACE_HW_JOB_7_USER	              (0x57c)
#define APU_ACE_HW_JOB_8_USER	              (0x580)
#define APU_ACE_HW_JOB_9_USER	              (0x584)
#define APU_ACE_HW_JOB_10_USER	              (0x588)
#define APU_ACE_HW_JOB_11_USER	              (0x58c)
#define APU_ACE_HW_JOB_12_USER	              (0x590)
#define APU_ACE_HW_JOB_13_USER	              (0x594)
#define APU_ACE_HW_JOB_14_USER	              (0x598)
#define APU_ACE_HW_JOB_15_USER	              (0x59c)
#define APU_ACE_JOB_TIEMOUT_CE0	              (0x5b0)
#define APU_ACE_JOB_TIEMOUT_CE1	              (0x5b4)
#define APU_ACE_JOB_TIEMOUT_CE2	              (0x5b8)
#define APU_ACE_JOB_TIEMOUT_CE3	              (0x5bc)
#define APU_ACE_SW_CMD_QUE_DIS	              (0x5d0)
#define APU_ACE_HW_FLAG_DIS	                  (0x5d4)
#define APU_ACE_SW_CMD_QUE_CLR	              (0x5d8)
#define APU_ACE_HW_FLAG_CLR	                  (0x5dc)
#define APU_ACE_SRAM_ENTRY_SEL	              (0x5e0)
#define APU_ACE_SRAM_ENTRY_RD	              (0x5e4)
#define APU_ACE_APB_MST_OUT_PADDR	          (0x5e8)
#define APU_ACE_APB_MST_IN_PADDR	          (0x5ec)
#define APU_ACE_IO_DEBUG	                  (0x5f0)
#define APU_CE0_ABORT	                      (0x610)
#define APU_CE0_IF_PC	                      (0x620)
#define APU_CE0_IF_STATUS	                  (0x624)
#define APU_CE0_RUN_STATUS	                  (0x628)
#define APU_CE0_RUN_PC	                      (0x62c)
#define APU_CE0_RUN_INSTR	                  (0x630)
#define APU_CE0_RUN_INSTR_63_32	              (0x634)
#define APU_CE0_R0	                          (0x640)
#define APU_CE0_R1	                          (0x644)
#define APU_CE0_R2	                          (0x648)
#define APU_CE0_R3	                          (0x64c)
#define APU_CE0_R4	                          (0x650)
#define APU_CE0_R5	                          (0x654)
#define APU_CE0_R6	                          (0x658)
#define APU_CE0_R7	                          (0x65c)
#define APU_CE0_BAPB	                      (0x668)
#define APU_CE0_BAXI	                      (0x66c)
#define APU_CE0_SP	                          (0x674)
#define APU_CE0_RA	                          (0x678)
#define APU_CE0_TIMEOUT_INSTR	              (0x680)
#define APU_CE0_TIMEOUT_PC	                  (0x684)
#define APU_CE0_INVLD_INSTR	                  (0x688)
#define APU_CE0_INVLD_PC	                  (0x68c)
#define APU_CE0_GO_NEXT	                      (0x6a0)
#define APU_CE0_STEP	                      (0x6a4)
#define APU_CE0_BREAKPOINT	                  (0x6a8)
#define APU_CE0_OVERFLOW	                  (0x6b0)
#define APU_CE1_ABORT	                      (0x710)
#define APU_CE1_IF_PC	                      (0x720)
#define APU_CE1_IF_STATUS	                  (0x724)
#define APU_CE1_RUN_STATUS	                  (0x728)
#define APU_CE1_RUN_PC	                      (0x72c)
#define APU_CE1_RUN_INSTR	                  (0x730)
#define APU_CE1_RUN_INSTR_63_32	              (0x734)
#define APU_CE1_R0	                          (0x740)
#define APU_CE1_R1	                          (0x744)
#define APU_CE1_R2	                          (0x748)
#define APU_CE1_R3	                          (0x74c)
#define APU_CE1_R4	                          (0x750)
#define APU_CE1_R5	                          (0x754)
#define APU_CE1_R6	                          (0x758)
#define APU_CE1_R7	                          (0x75c)
#define APU_CE1_BAPB	                      (0x768)
#define APU_CE1_BAXI	                      (0x76c)
#define APU_CE1_SP	                          (0x774)
#define APU_CE1_RA	                          (0x778)
#define APU_CE1_TIMEOUT_INSTR	              (0x780)
#define APU_CE1_TIMEOUT_PC	                  (0x784)
#define APU_CE1_INVLD_INSTR	                  (0x788)
#define APU_CE1_INVLD_PC	                  (0x78c)
#define APU_CE1_GO_NEXT	                      (0x7a0)
#define APU_CE1_STEP	                      (0x7a4)
#define APU_CE1_BREAKPOINT	                  (0x7a8)
#define APU_CE1_OVERFLOW	                  (0x7b0)
#define APU_CE2_ABORT	                      (0x810)
#define APU_CE2_IF_PC	                      (0x820)
#define APU_CE2_IF_STATUS	                  (0x824)
#define APU_CE2_RUN_STATUS	                  (0x828)
#define APU_CE2_RUN_PC	                      (0x82c)
#define APU_CE2_RUN_INSTR	                  (0x830)
#define APU_CE2_RUN_INSTR_63_32	              (0x834)
#define APU_CE2_R0	                          (0x840)
#define APU_CE2_R1	                          (0x844)
#define APU_CE2_R2	                          (0x848)
#define APU_CE2_R3	                          (0x84c)
#define APU_CE2_R4	                          (0x850)
#define APU_CE2_R5	                          (0x854)
#define APU_CE2_R6	                          (0x858)
#define APU_CE2_R7	                          (0x85c)
#define APU_CE2_BAPB	                      (0x868)
#define APU_CE2_BAXI	                      (0x86c)
#define APU_CE2_SP	                          (0x874)
#define APU_CE2_RA	                          (0x878)
#define APU_CE2_TIMEOUT_INSTR	              (0x880)
#define APU_CE2_TIMEOUT_PC	                  (0x884)
#define APU_CE2_INVLD_INSTR	                  (0x888)
#define APU_CE2_INVLD_PC	                  (0x88c)
#define APU_CE2_GO_NEXT	                      (0x8a0)
#define APU_CE2_STEP	                      (0x8a4)
#define APU_CE2_BREAKPOINT	                  (0x8a8)
#define APU_CE2_OVERFLOW	                  (0x8b0)
#define APU_CE3_ABORT	                      (0x910)
#define APU_CE3_IF_PC	                      (0x920)
#define APU_CE3_IF_STATUS	                  (0x924)
#define APU_CE3_RUN_STATUS	                  (0x928)
#define APU_CE3_RUN_PC	                      (0x92c)
#define APU_CE3_RUN_INSTR	                  (0x930)
#define APU_CE3_RUN_INSTR_63_32	              (0x934)
#define APU_CE3_R0	                          (0x940)
#define APU_CE3_R1	                          (0x944)
#define APU_CE3_R2	                          (0x948)
#define APU_CE3_R3	                          (0x94c)
#define APU_CE3_R4	                          (0x950)
#define APU_CE3_R5	                          (0x954)
#define APU_CE3_R6	                          (0x958)
#define APU_CE3_R7	                          (0x95c)
#define APU_CE3_BAPB	                      (0x968)
#define APU_CE3_BAXI	                      (0x96c)
#define APU_CE3_SP	                          (0x974)
#define APU_CE3_RA	                          (0x978)
#define APU_CE3_TIMEOUT_INSTR	              (0x980)
#define APU_CE3_TIMEOUT_PC	                  (0x984)
#define APU_CE3_INVLD_INSTR	                  (0x988)
#define APU_CE3_INVLD_PC	                  (0x98c)
#define APU_CE3_GO_NEXT	                      (0x9a0)
#define APU_CE3_STEP	                      (0x9a4)
#define APU_CE3_BREAKPOINT	                  (0x9a8)
#define APU_CE3_OVERFLOW	                  (0x9b0)

#define CE_REG_DUMP_MAGIC_NUM            (0xA5A5A5A5)
#define CE_REG_DUMP_ACE_START      (APU_ACE_SW_REQ_0)
#define CE_REG_DUMP_ACE_END        (APU_CE3_OVERFLOW)
#define CE_REG_DUMP_HW_SEMA_START             (0xe08)
#define CE_REG_DUMP_HW_SEMA_END               (0xe0c)
#define CE_0_IRQ_MASK                           (0xF)
#define CE_1_IRQ_MASK                          (0xF0)
#define CE_2_IRQ_MASK                         (0xF00)
#define CE_3_IRQ_MASK                        (0xF000)
#define CE_TASK_JOB_SFT                        (0x10)
#define CE_TASK_JOB_MSK                        (0x1F)
#define CE_MISS_TYPE2_REQ_FLAG_0_MSK          (0x100)
#define CE_MISS_TYPE2_REQ_FLAG_1_MSK          (0x200)
#define CE_MISS_TYPE2_REQ_FLAG_2_MSK          (0x400)
#define CE_MISS_TYPE2_REQ_FLAG_3_MSK          (0x800)
#define CE_NON_ALIGNED_APB_FLAG_MSK       (0x6000000)
#define CE_NON_ALIGNED_APB_OUT_FLAG_MSK   (0x2000000)
#define CE_NON_ALIGNED_APB_IN_FLAG_MSK    (0x4000000)
#define CE_APB_ERR_STATUS_CE0_MSK               (0x1)
#define CE_APB_ERR_STATUS_CE1_MSK               (0x2)
#define CE_APB_ERR_STATUS_CE2_MSK               (0x4)
#define CE_APB_ERR_STATUS_CE3_MSK               (0x8)

#define GET_SMC_OP(op1, op2, op3) (op1 | op2 << (8 * 1) | op3 << (8 * 2))

enum {
	SMC_OP_APU_CE_NULL = 0,
	SMC_OP_APU_ACE_ABN_IRQ_FLAG_CE,
	SMC_OP_APU_ACE_ABN_IRQ_FLAG_ACE_SW,
	SMC_OP_APU_ACE_ABN_IRQ_FLAG_USER,
	SMC_OP_APU_ACE_CE0_TASK_ING,
	SMC_OP_APU_ACE_CE1_TASK_ING,
	SMC_OP_APU_ACE_CE2_TASK_ING,
	SMC_OP_APU_ACE_CE3_TASK_ING,
	SMC_OP_APU_CE0_RUN_INSTR,
	SMC_OP_APU_CE1_RUN_INSTR,
	SMC_OP_APU_CE2_RUN_INSTR,
	SMC_OP_APU_CE3_RUN_INSTR,
	SMC_OP_APU_CE0_TIMEOUT_INSTR,
	SMC_OP_APU_CE1_TIMEOUT_INSTR,
	SMC_OP_APU_CE2_TIMEOUT_INSTR,
	SMC_OP_APU_CE3_TIMEOUT_INSTR,
	SMC_OP_APU_ACE_APB_MST_OUT_STATUS_ERR,
	SMC_OP_APU_ACE_APB_MST_IN_STATUS_ERR,
	SMC_OP_APU_CE0_RUN_PC,
	SMC_OP_APU_CE1_RUN_PC,
	SMC_OP_APU_CE2_RUN_PC,
	SMC_OP_APU_CE3_RUN_PC,
	SMC_OP_APU_APU_ACE_CMD_Q_STATUS_0,
	SMC_OP_APU_APU_ACE_CMD_Q_STATUS_1,
	SMC_OP_APU_APU_ACE_CMD_Q_STATUS_2,
	SMC_OP_APU_APU_ACE_CMD_Q_STATUS_3,
	SMC_OP_APU_APU_ACE_CMD_Q_STATUS_4,
	SMC_OP_APU_APU_ACE_CMD_Q_STATUS_5,
	SMC_OP_APU_APU_ACE_CMD_Q_STATUS_6,
	SMC_OP_APU_APU_ACE_CMD_Q_STATUS_7,
	SMC_OP_APU_CE_NUM
};

#endif /* APU_CE_EXCEP_H */
