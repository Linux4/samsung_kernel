#include <linux/types.h>

/* represents the value in access_type column in guide */
enum phycal_seq_acctype {
	PHYCAL_READ = 0,
	PHYCAL_WRITE,
	PHYCAL_COND_READ,
	PHYCAL_COND_WRITE,
	PHYCAL_SAVE_RESTORE,
	PHYCAL_COND_SAVE_RESTORE,
	PHYCAL_WAIT,
	PHYCAL_WAIT_TWO,
	PHYCAL_CHECK_SKIP,
	PHYCAL_COND_CHECK_SKIP,
	PHYCAL_WRITE_WAIT,
	PHYCAL_WRITE_RETRY,
	PHYCAL_WRITE_RETRY_INV,
	PHYCAL_WRITE_RETURN,
	PHYCAL_SET_BIT_ATOMIC,
	PHYCAL_CLR_BIT_ATOMIC,
	PHYCAL_DELAY,
	PHYCAL_CLEAR_PEND,
	PHYCAL_COND_WRITE_RETRY,
	PHYCAL_COND_WRITE_RETRY_INV,
	PHYCAL_EXT_READ = 0x1000,
	PHYCAL_EXT_WRITE,
	PHYCAL_EXT_SET_BIT,
	PHYCAL_EXT_CLR_BIT,
	PHYCAL_EXT_CHK_BIT,
	PHYCAL_EXT_CHK_BIT_MASK,
	PHYCAL_EXT_DELAY,
};
/* represents each row in the PHY sequence guide */
struct phycal_seq {
	u32 access_type;
	u32 offset;
	u32 mask;
	u32 value;
	u32 cond_offset;
	u32 cond_mask;
	u32 cond_value;
	u32 cond0;
	u32 delay_num;
	u32 delay_val;
	u32 base_pa;
	u32 cond_base_pa;
	char sfr_name[52];
	char errlog[52];
	u32 need_restore;
	u32 need_skip;
	void __iomem *base_va;
	void __iomem *cond_base_va;
};

#define PHYCAL_SEQ_DESC(_access_type, _sfr_name, _base_pa, _offset,	\
			_mask, _value, _cond_base_pa, _cond_offset,	\
			_cond_mask, _cond_value, _cond0, _delay_num,	\
			_delay_val, _error_log) {			\
	.access_type = _access_type,					\
	.sfr_name = _sfr_name,						\
	.base_pa = _base_pa,						\
	.base_va = NULL,						\
	.offset = _offset,						\
	.mask = _mask,							\
	.value = _value,						\
	.cond_base_pa = _cond_base_pa,					\
	.cond_base_va = NULL,						\
	.cond_offset = _cond_offset,					\
	.cond_mask = _cond_mask,					\
	.cond_value = _cond_value,					\
	.need_restore = false,						\
	.need_skip = false,						\
	.cond0 = _cond0,						\
	.delay_num = _delay_num,					\
	.delay_val = _delay_val,					\
	.errlog = _error_log,						\
}

struct phycal_p2v_map {
	phys_addr_t pa;
	void __iomem *va;
};

#define DEFINE_PA(p) { .pa = p }

int exynos_phycal_phy2virt(struct phycal_p2v_map *p2vmap, unsigned int map_size,
			struct phycal_seq *seq, unsigned int seq_size);
void exynos_phycal_seq(struct phycal_seq *seq, u32 seq_size, u32 cond0);
