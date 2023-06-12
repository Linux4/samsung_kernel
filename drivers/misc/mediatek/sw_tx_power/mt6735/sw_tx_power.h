#ifndef __SW_TX_POWER__
#define __SW_TX_POWER__

#include <mt-plat/mt_ccci_common.h>

/*======================= Description TX Power Table =========================*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
/*              |        4G          |        3G         |        2G         |*/
/*----------------------------------------------------------------------------*/
/*  ap_param    | Table 1 : Table 0  | Table 1 : Table 0 | Table 1 : Table 0 |*/
/*----------------------------------------------------------------------------*/
/*  0x00        |    X    :     X    |    X    :    X    |    X    :    X    |*/
/*  0x01        |    X    :     X    |    X    :    X    |    X    :    O    |*/
/*  0x02        |    X    :     X    |    X    :    X    |    O    :    X    |*/
/*  0x03        |    X    :     X    |    X    :    X    |    O    :    O    |*/
/*  0x04        |    X    :     X    |    X    :    O    |    X    :    X    |*/
/*  0x05        |    X    :     X    |    X    :    O    |    X    :    O    |*/
/*  0x06        |    X    :     X    |    X    :    O    |    O    :    X    |*/
/*  0x07        |    X    :     X    |    X    :    O    |    O    :    O    |*/
/*  0x08        |    X    :     X    |    O    :    X    |    X    :    X    |*/
/*  0x09        |    X    :     X    |    O    :    X    |    X    :    O    |*/
/*  0x0A        |    X    :     X    |    O    :    X    |    O    :    X    |*/
/*  0x0B        |    X    :     X    |    O    :    X    |    O    :    O    |*/
/*  0x0C        |    X    :     X    |    O    :    O    |    X    :    X    |*/
/*  0x0D        |    X    :     X    |    O    :    O    |    X    :    O    |*/
/*  0x0E        |    X    :     X    |    O    :    O    |    O    :    X    |*/
/*  0x0F        |    X    :     X    |    O    :    O    |    O    :    O    |*/
/*  0x10        |    X    :     O    |    X    :    X    |    X    :    X    |*/
/*  0x11        |    X    :     O    |    X    :    X    |    X    :    O    |*/
/*  0x12        |    X    :     O    |    X    :    X    |    O    :    X    |*/
/*  0x13        |    X    :     O    |    X    :    X    |    O    :    O    |*/
/*  0x14        |    X    :     O    |    X    :    O    |    X    :    X    |*/
/*  0x15        |    X    :     O    |    X    :    O    |    X    :    O    |*/
/*  0x16        |    X    :     O    |    X    :    O    |    O    :    X    |*/
/*  0x17        |    X    :     O    |    X    :    O    |    O    :    O    |*/
/*  0x18        |    X    :     O    |    O    :    X    |    X    :    X    |*/
/*  0x19        |    X    :     O    |    O    :    X    |    X    :    O    |*/
/*  0x1A        |    X    :     O    |    O    :    X    |    O    :    X    |*/
/*  0x1B        |    X    :     O    |    O    :    X    |    O    :    O    |*/
/*  0x1C        |    X    :     O    |    O    :    O    |    X    :    X    |*/
/*  0x1D        |    X    :     O    |    O    :    O    |    X    :    O    |*/
/*  0x1E        |    X    :     O    |    O    :    O    |    O    :    X    |*/
/*  0x1F        |    X    :     O    |    O    :    O    |    O    :    O    |*/
/*  0x20        |    O    :     X    |    X    :    X    |    X    :    X    |*/
/*  0x21        |    O    :     X    |    X    :    X    |    X    :    O    |*/
/*  0x22        |    O    :     X    |    X    :    X    |    O    :    X    |*/
/*  0x23        |    O    :     X    |    X    :    X    |    O    :    O    |*/
/*  0x24        |    O    :     X    |    X    :    O    |    X    :    X    |*/
/*  0x25        |    O    :     X    |    X    :    O    |    X    :    O    |*/
/*  0x26        |    O    :     X    |    X    :    O    |    O    :    X    |*/
/*  0x27        |    O    :     X    |    X    :    O    |    O    :    O    |*/
/*  0x28        |    O    :     X    |    O    :    X    |    X    :    X    |*/
/*  0x29        |    O    :     X    |    O    :    X    |    X    :    O    |*/
/*  0x2A        |    O    :     X    |    O    :    X    |    O    :    X    |*/
/*  0x2B        |    O    :     X    |    O    :    X    |    O    :    O    |*/
/*  0x2C        |    O    :     X    |    O    :    O    |    X    :    X    |*/
/*  0x2D        |    O    :     X    |    O    :    O    |    X    :    O    |*/
/*  0x2E        |    O    :     X    |    O    :    O    |    O    :    X    |*/
/*  0x2F        |    O    :     X    |    O    :    O    |    O    :    O    |*/
/*  0x30        |    O    :     O    |    X    :    X    |    X    :    X    |*/
/*  0x31        |    O    :     O    |    X    :    X    |    X    :    O    |*/
/*  0x32        |    O    :     O    |    X    :    X    |    O    :    X    |*/
/*  0x33        |    O    :     O    |    X    :    X    |    O    :    O    |*/
/*  0x34        |    O    :     O    |    X    :    O    |    X    :    X    |*/
/*  0x35        |    O    :     O    |    X    :    O    |    X    :    O    |*/
/*  0x36        |    O    :     O    |    X    :    O    |    O    :    X    |*/
/*  0x37        |    O    :     O    |    X    :    O    |    O    :    O    |*/
/*  0x38        |    O    :     O    |    O    :    X    |    X    :    X    |*/
/*  0x39        |    O    :     O    |    O    :    X    |    X    :    O    |*/
/*  0x3A        |    O    :     O    |    O    :    X    |    O    :    X    |*/
/*  0x3B        |    O    :     O    |    O    :    X    |    O    :    O    |*/
/*  0x3C        |    O    :     O    |    O    :    O    |    X    :    X    |*/
/*  0x3D        |    O    :     O    |    O    :    O    |    X    :    O    |*/
/*  0x3E        |    O    :     O    |    O    :    O    |    O    :    X    |*/
/*  0x3F        |    O    :     O    |    O    :    O    |    O    :    O    |*/
/*----------------------------------------------------------------------------*/
/*  others      |    invalid input                                           |*/
/*----------------------------------------------------------------------------*/
#define SWTX_DEBUG		/* if on, pr_debug -> pr_err */

#ifdef SWTX_DEBUG
#define SWTX_DBG(format, args...) pr_err("[SWTP][D]" format, ##args)
#else
#define SWTX_DBG(format, args...) pr_debug("[SWTP][D]" format, ##args)
#endif
#define SWTX_INF(format, args...) pr_info("[SWTP][I]" format, ##args)
#define SWTX_ERR(format, args...) pr_err("[SWTP][E]" format, ##args)

/* ap_param */
#define ENABLE_SW_TX_POWER_2G_NONE      0x00000000
#define ENABLE_SW_TX_POWER_2G_TABLE0    0x00000001
#define ENABLE_SW_TX_POWER_2G_TABLE1    0x00000002
#define ENABLE_SW_TX_POWER_2G_TABLEX    0x00000003

#define ENABLE_SW_TX_POWER_3G_NONE      0x00000000
#define ENABLE_SW_TX_POWER_3G_TABLE0    0x00000004
#define ENABLE_SW_TX_POWER_3G_TABLE1    0x00000008
#define ENABLE_SW_TX_POWER_3G_TABLEX    0x0000000C

#define ENABLE_SW_TX_POWER_4G_NONE      0x00000000
#define ENABLE_SW_TX_POWER_4G_TABLE0    0x00000010
#define ENABLE_SW_TX_POWER_4G_TABLE1    0x00000020
#define ENABLE_SW_TX_POWER_4G_TABLEX    0x00000030

#define MODE_SWTP(v, x, y)    ((ENABLE_SW_TX_POWER_##v)|(ENABLE_SW_TX_POWER_##x)|(ENABLE_SW_TX_POWER_##y))

#define	SWTP_SUPER_MODE    0x00001FFF
#define SWTP_NORMAL_MODE   0x000001FF

#define SWTP_MODE_ON       0x00000001
#define SWTP_MODE_OFF      0x00000000

#if 0 /* if use 2 RF connectors, should turn SWTP_2_RF_CON on */
#define SWTP_2_RF_CON
#endif

enum {
	SWTP_CTRL_USER_SET0,
	SWTP_CTRL_USER_SET1,
	SWTP_CTRL_USER_SET2,
	SWTP_CTRL_USER_SET3,
	SWTP_CTRL_USER_SET4,
	SWTP_CTRL_USER_SET5,
	SWTP_CTRL_USER_SET6,
	SWTP_CTRL_USER_SET7,
	SWTP_CTRL_USER_SET8,
	SWTP_CTRL_USER_SET9,
	SWTP_CTRL_USER_SET10,
	SWTP_CTRL_USER_SET11,
	SWTP_CTRL_USER_SET12,
	SWTP_CTRL_USER_SET13,
	SWTP_CTRL_USER_SET14,
	SWTP_CTRL_USER_SET15,
	SWTP_CTRL_USER_SET16,
	SWTP_CTRL_USER_SET17,
	SWTP_CTRL_USER_SET18,
	SWTP_CTRL_USER_SET19,
	SWTP_CTRL_USER_SET20,
	SWTP_CTRL_USER_SET21,
	SWTP_CTRL_USER_SET22,
	SWTP_CTRL_USER_SET23,
	SWTP_CTRL_USER_SET24,
	SWTP_CTRL_USER_SET25,
	SWTP_CTRL_USER_SET26,
	SWTP_CTRL_USER_SET27,
	SWTP_CTRL_USER_SET28,
	SWTP_CTRL_USER_SET29,
	SWTP_CTRL_USER_SET30,
	SWTP_CTRL_USER_SET31,
	SWTP_CTRL_USER_SET32,
	SWTP_CTRL_USER_SET33,
	SWTP_CTRL_USER_SET34,
	SWTP_CTRL_USER_SET35,
	SWTP_CTRL_USER_SET36,
	SWTP_CTRL_USER_SET37,
	SWTP_CTRL_USER_SET38,
	SWTP_CTRL_USER_SET39,
	SWTP_CTRL_USER_SET40,
	SWTP_CTRL_USER_SET41,
	SWTP_CTRL_USER_SET42,
	SWTP_CTRL_USER_SET43,
	SWTP_CTRL_USER_SET44,
	SWTP_CTRL_USER_SET45,
	SWTP_CTRL_USER_SET46,
	SWTP_CTRL_USER_SET47,
	SWTP_CTRL_USER_SET48,
	SWTP_CTRL_USER_SET49,
	SWTP_CTRL_USER_SET50,
	SWTP_CTRL_USER_SET51,
	SWTP_CTRL_USER_SET52,
	SWTP_CTRL_USER_SET53,
	SWTP_CTRL_USER_SET54,
	SWTP_CTRL_USER_SET55,
	SWTP_CTRL_USER_SET56,
	SWTP_CTRL_USER_SET57,
	SWTP_CTRL_USER_SET58,
	SWTP_CTRL_USER_SET59,
	SWTP_CTRL_USER_SET60,
	SWTP_CTRL_USER_SET61,
	SWTP_CTRL_USER_SET62,
	SWTP_CTRL_USER_SET63,
	SWTP_CTRL_SUPER_SET,
	SWTP_CTRL_MAX_STATE
};

typedef struct swtp_state {
	unsigned int enable;
	unsigned int mode;
	unsigned int setvalue;
} swtp_state_type;

extern struct of_device_id swtp_of_match[];
extern int switch_MD1_Tx_Power(unsigned int mode);
extern int switch_MD2_Tx_Power(unsigned int mode);
extern int swtp_reset_tx_power(void);
extern int swtp_rfcable_tx_power(void);
extern int swtp_set_mode_unlocked(unsigned int ctrid, unsigned int enable);
extern int swtp_set_mode(unsigned int ctrid, unsigned int enable);
extern int swtp_reset_mode(void);
extern unsigned int swtp_get_mode(swtp_state_type *swtp_super_state,
				  swtp_state_type *swtp_normal_state);
extern int swtp_mod_eint_enable(int);
extern int swtp_mod_eint_init(struct platform_device *dev);
extern int swtp_mod_eint_read(void);
extern int register_ccci_sys_call_back(int md_id, unsigned int id, ccci_sys_cb_func_t func);

#endif				/* __SW_TX_POWER__ */
