#ifndef _SMART_MTP_S6E63M0_H_
#define _SMART_MTP_S6E63M0_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <asm/div64.h>

#define SMART_DIMMING

#define MAX_GRADATION		300
#define PANEL_ID_MAX		3
#define GAMMA_300CD_MAX	3


enum {
	CI_RED,
	CI_GREEN,
	CI_BLUE,
	CI_MAX,
};


enum {
	IV_1,
	IV_19,
	IV_43,
	IV_87,
	IV_171,
	IV_255,
	IV_MAX,
	IV_TABLE_MAX,
};


enum {
	AD_IV0,
	AD_IV1,
	AD_IV19,
	AD_IV43,
	AD_IV87,
	AD_IV171,
	AD_IV255,
	AD_IVMAX,
};


struct str_voltage_entry {
	/* u32 g22_value; */
	u32 v[CI_MAX];
};


struct str_table_info {
	/* et : start gray value */
	u8 st;
	/* end gray value, st + count */
	u8 et;
	u8 count;
	const u8 *offset_table;
	/* rv : ratio value */
	u32 rv;
};


struct str_flookup_table {
	u16 entry;
	u16 count;
};

struct str_smart_dim {
	u8 panelid[PANEL_ID_MAX];
	s16 mtp[CI_MAX][IV_MAX];
	struct str_voltage_entry ve[256];
	const u8 *default_gamma;
	struct str_table_info t_info[IV_TABLE_MAX];
	const struct str_flookup_table *flooktbl;
	const u32 *g2x_tbl;
	const u32 *g22_tbl;
	const u32 *g19_tbl;
	const u32 *g300_gra_tbl;
	u32 adjust_volt[CI_MAX][AD_IVMAX];
};

int init_table_info_22(struct str_smart_dim *smart);
u8 calc_voltage_table(struct str_smart_dim *smart, const u8 *mtp);
u32 calc_gamma_table_22(struct str_smart_dim *smart, u32 gv, u8 result[]);
#endif
