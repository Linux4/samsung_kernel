#ifndef __D2199_DEBUG_H___
#define __D2199_DEBUG_H___

enum d2199_reg_id {
	STATUS_REGS = 0,
	GPIO_REGS,
	SEQ_REGS,
	LDO_REGS,
	MCTL_REGS,
	CTL_REGS,
	ADC_REGS,
	RTC_REGS,
	OTP_REGS,
	AUDIO_REGS,
	OTHER_REGS,
	ALL_REGS,
};

#ifdef CONFIG_SEC_PM_DEBUG
extern int show_d2199_block_regs(unsigned int block_id);
extern int show_d2199_block_info(unsigned int block_id);
#else /* !CONFIG_SEC_PM_DEBUG */
static int show_d2199_block_regs(unsigned int block_id) { return 0; }
static int show_d2199_block_info(unsigned int block_id) { return 0; }
#endif /* CONFIG_SEC_PM_DEBUG */

#endif /* __D2199_DEBUG_H___ */
