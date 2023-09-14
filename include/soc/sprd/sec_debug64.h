#ifndef SEC_DEBUG64_H
#define SEC_DEBUG64_H

#include <soc/sprd/iomap.h>
extern struct iotable_sprd io_addr_sprd;
#define SEC_DEBUG_MAGIC_VA              (io_addr_sprd.base + 0x00100f80)
#define SPRD_INFORM0                    (SEC_DEBUG_MAGIC_VA + 0x00000004)
#define SPRD_INFORM1                    (SPRD_INFORM0 + 0x00000004)
#define SPRD_INFORM2                    (SPRD_INFORM1 + 0x00000004)
#define SPRD_INFORM3                    (SPRD_INFORM2 + 0x00000004)
#define SPRD_INFORM4                    (SPRD_INFORM3 + 0x00000004)
#define SPRD_INFORM5                    (SPRD_INFORM4 + 0x00000004)
#define SPRD_INFORM6                    (SPRD_INFORM5 + 0x00000004)

union sec_debug_level_t {
	struct {
		u16 kernel_fault;
		u16 user_fault;
	} en;
	u32 uint_val;
};

#define RESET_REASON_NORMAL		0x1A2B3C00
#define RESET_REASON_SMPL		0x1A2B3C01
#define RESET_REASON_WSTR		0x1A2B3C02
#define RESET_REASON_WATCHDOG		0x1A2B3C03
#define RESET_REASON_PANIC		0x1A2B3C04
#define RESET_REASON_LPM		0x1A2B3C10
#define RESET_REASON_RECOVERY		0x1A2B3C11
#define RESET_REASON_FOTA		0x1A2B3C12

#define SEC_DEBUG_PANIC_SIGN_SET    0xFEFEFEFE
#define SEC_DEBUG_PANIC_SIGN_INIT   0x00000000

#define SEC_DEBUG_NR_CPU	NR_CPUS

enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT = 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC = 0x000000C8,
	UPLOAD_CAUSE_FORCED_UPLOAD = 0x00000022,
	UPLOAD_CAUSE_CP_ERROR_FATAL = 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT = 0x0000002F,
	UPLOAD_CAUSE_HSIC_DISCONNECTED = 0x000000DD,
};

struct sec_debug_mmu_reg_t {
	long SCTLR_EL1;
	long TTBR0_EL1;
	long TTBR1_EL1;
	long TCR_EL1;
	long ESR_EL1;
	long FAR_EL1;
	long CONTEXTIDR_EL1;
	long TPIDR_EL0;
	long TPIDRRO_EL0;
	long TPIDR_EL1;
	long MAIR_EL1;
};

int sec_debug_save_context(void *core_regs);

#endif //SEC_DEBUG64_H
