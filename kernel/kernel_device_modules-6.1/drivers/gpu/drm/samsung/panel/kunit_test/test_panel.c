#include <linux/kernel.h>
#include "../maptbl.h"
#include "../panel_obj.h"
#include "../panel.h"
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../panel_condition.h"
#include "../panel_function.h"
#include "test_panel.h"

#define TEST_PANEL_DATE_REG			0xA1
#define TEST_PANEL_DATE_OFS			4
#define TEST_PANEL_DATE_LEN			(PANEL_DATE_LEN)

#define TEST_PANEL_COORDINATE_REG		0xA1
#define TEST_PANEL_COORDINATE_OFS		0
#define TEST_PANEL_COORDINATE_LEN		(PANEL_COORD_LEN)

#define TEST_PANEL_ID_REG				0x04
#define TEST_PANEL_ID_OFS				0
#define TEST_PANEL_ID_LEN				(PANEL_ID_LEN)

#define TEST_PANEL_CODE_REG			0xD6
#define TEST_PANEL_CODE_OFS			0
#define TEST_PANEL_CODE_LEN			(PANEL_CODE_LEN)

#define TEST_PANEL_OCTA_ID_REG			0xA1
#define TEST_PANEL_OCTA_ID_OFS			11
#define TEST_PANEL_OCTA_ID_LEN			(PANEL_OCTA_ID_LEN)

#define TEST_PANEL_CHIP_ID_REG			0xD6
#define TEST_PANEL_CHIP_ID_OFS			0
#define TEST_PANEL_CHIP_ID_LEN			5

#define TEST_PANEL_RDDPM_REG			0x0A
#define TEST_PANEL_RDDPM_OFS			0
#define TEST_PANEL_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define TEST_PANEL_RDDSM_REG			0x0E
#define TEST_PANEL_RDDSM_OFS			0
#define TEST_PANEL_RDDSM_LEN			(PANEL_RDDSM_LEN)

static int init_common_maptbl(struct maptbl *tbl)
{
	return 0;
}

static void copy_common_maptbl(struct maptbl *tbl, u8 *dst)
{
	int idx;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %pK, dst %pK)\n",
				tbl, dst);
		return;
	}

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		panel_err("failed to getidx %d\n", idx);
		return;
	}

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * maptbl_get_sizeof_copy(tbl));
	panel_dbg("copy from %s %d %d\n",
			maptbl_get_name(tbl), idx, maptbl_get_sizeof_copy(tbl));
}

static int getidx_acl_opr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;
	u32 layer, row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;

	layer = is_hbm_brightness(panel_bl, panel_bl->props.brightness) ? PANEL_HBM_ON : PANEL_HBM_OFF;
	row = panel_bl_get_acl_opr(panel_bl);
	if (row >= maptbl_get_countof_row(tbl)) {
		panel_err("row(%d) is out of maptbl range(%d)\n",
				row, maptbl_get_countof_row(tbl));
		row = 0;
	}
	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_dia_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	return maptbl_index(tbl, 0, panel_data->props.dia_mode, 0);
}

static int show_rddpm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddpm[TEST_PANEL_RDDPM_LEN] = { 0, };

	if (!res || ARRAY_SIZE(rddpm) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = copy_resource(rddpm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [0Ah:RDDPM] INFO (After DISPLAY_ON) ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddpm[0], ((rddpm[0] & 0x9C) == 0x9C) ? "GOOD" : "NG");
	panel_info("* Bootster Mode : %s\n", rddpm[0] & 0x80 ? "ON (GD)" : "OFF (NG)");
	panel_info("* Idle Mode     : %s\n", rddpm[0] & 0x40 ? "ON (NG)" : "OFF (GD)");
	panel_info("* Partial Mode  : %s\n", rddpm[0] & 0x20 ? "ON" : "OFF");
	panel_info("* Sleep Mode    : %s\n", rddpm[0] & 0x10 ? "OUT (GD)" : "IN (NG)");
	panel_info("* Normal Mode   : %s\n", rddpm[0] & 0x08 ? "OK (GD)" : "SLEEP (NG)");
	panel_info("* Display ON    : %s\n", rddpm[0] & 0x04 ? "ON (GD)" : "OFF (NG)");
	panel_info("=================================================\n");

	return 0;
}

static int show_rddsm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddsm[TEST_PANEL_RDDSM_LEN] = { 0, };

	if (!res || ARRAY_SIZE(rddsm) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = copy_resource(rddsm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddsm resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [0Eh:RDDSM] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddsm[0], (rddsm[0] == 0x80) ? "GOOD" : "NG");
	panel_info("* TE Mode : %s\n", rddsm[0] & 0x80 ? "ON(GD)" : "OFF(NG)");
	panel_info("=================================================\n");

	return 0;
}

static u8 TEST_PANEL_ID[TEST_PANEL_ID_LEN];
static u8 TEST_PANEL_COORDINATE[TEST_PANEL_COORDINATE_LEN];
static u8 TEST_PANEL_CODE[TEST_PANEL_CODE_LEN];
static u8 TEST_PANEL_DATE[TEST_PANEL_DATE_LEN];
static u8 TEST_PANEL_OCTA_ID[TEST_PANEL_OCTA_ID_LEN];
static u8 TEST_PANEL_RDDPM[TEST_PANEL_RDDPM_LEN];
static u8 TEST_PANEL_RDDSM[TEST_PANEL_RDDSM_LEN];
static u8 test_panel_dia_onoff_table[][1] = {
	{ 0x00 }, /* dia off */
	{ 0x02 }, /* dia on */
};

enum {
	TEST_PANEL_ACL_OPR_0,
	TEST_PANEL_ACL_OPR_1,
	TEST_PANEL_ACL_OPR_2,
	TEST_PANEL_ACL_OPR_3,
	MAX_TEST_PANEL_ACL_OPR
};

static u8 test_panel_acl_opr_table[MAX_PANEL_HBM][MAX_TEST_PANEL_ACL_OPR][1] = {
	[PANEL_HBM_OFF] = {
		[TEST_PANEL_ACL_OPR_0] = { 0x00 }, /* adaptive_control 0, 0% */
		[TEST_PANEL_ACL_OPR_1] = { 0x01 }, /* adaptive_control 1, 8% */
		[TEST_PANEL_ACL_OPR_2] = { 0x03 }, /* adaptive_control 2, 30% */
		[TEST_PANEL_ACL_OPR_3] = { 0x02 }, /* adaptive_control 3, 15% */
	},
	[PANEL_HBM_ON] = {
		[TEST_PANEL_ACL_OPR_0] = { 0x00 }, /* adaptive_control 0, 0% */
		[TEST_PANEL_ACL_OPR_1] = { 0x01 }, /* adaptive_control 1, 8% */
		[TEST_PANEL_ACL_OPR_2] = { 0x01 }, /* adaptive_control 2, 8% */
		[TEST_PANEL_ACL_OPR_3] = { 0x01 }, /* adaptive_control 3, 8% */
	},
};

static struct rdinfo test_panel_rditbl[] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, TEST_PANEL_ID_REG, TEST_PANEL_ID_OFS, TEST_PANEL_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, TEST_PANEL_COORDINATE_REG, TEST_PANEL_COORDINATE_OFS, TEST_PANEL_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, TEST_PANEL_CODE_REG, TEST_PANEL_CODE_OFS, TEST_PANEL_CODE_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, TEST_PANEL_DATE_REG, TEST_PANEL_DATE_OFS, TEST_PANEL_DATE_LEN),
	[READ_OCTA_ID] = RDINFO_INIT(octa_id, DSI_PKT_TYPE_RD, TEST_PANEL_OCTA_ID_REG, TEST_PANEL_OCTA_ID_OFS, TEST_PANEL_OCTA_ID_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, TEST_PANEL_RDDPM_REG, TEST_PANEL_RDDPM_OFS, TEST_PANEL_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, TEST_PANEL_RDDSM_REG, TEST_PANEL_RDDSM_OFS, TEST_PANEL_RDDSM_LEN),
};

static DEFINE_RESUI(id, &test_panel_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &test_panel_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &test_panel_rditbl[READ_CODE], 0);
static DEFINE_RESUI(date, &test_panel_rditbl[READ_DATE], 0);
static DEFINE_RESUI(octa_id, &test_panel_rditbl[READ_OCTA_ID], 0);
static DEFINE_RESUI(rddpm, &test_panel_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &test_panel_rditbl[READ_RDDSM], 0);

static struct resinfo test_panel_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, TEST_PANEL_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, TEST_PANEL_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, TEST_PANEL_CODE, RESUI(code)),
	[RES_DATE] = RESINFO_INIT(date, TEST_PANEL_DATE, RESUI(date)),
	[RES_OCTA_ID] = RESINFO_INIT(octa_id, TEST_PANEL_OCTA_ID, RESUI(octa_id)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, TEST_PANEL_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, TEST_PANEL_RDDSM, RESUI(rddsm)),
};

static DEFINE_PNOBJ_FUNC(dump_show_rddpm, show_rddpm);
static DEFINE_PNOBJ_FUNC(dump_show_rddsm, show_rddsm);

static struct dumpinfo test_panel_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT(rddpm, &test_panel_restbl[RES_RDDPM], &PNOBJ_FUNC(dump_show_rddpm)),
	[DUMP_RDDSM] = DUMPINFO_INIT(rddsm, &test_panel_restbl[RES_RDDSM], &PNOBJ_FUNC(dump_show_rddsm)),
};

DEFINE_PNOBJ_FUNC(init_common_maptbl, init_common_maptbl);
static DEFINE_PNOBJ_FUNC(getidx_acl_opr_table, getidx_acl_opr_table);
static DEFINE_PNOBJ_FUNC(getidx_dia_onoff_table, getidx_dia_onoff_table);
DEFINE_PNOBJ_FUNC(copy_common_maptbl, copy_common_maptbl);

static struct maptbl test_panel_maptbl[MAX_PANEL_MAPTBL] = {
	[ACL_OPR_MAPTBL] = DEFINE_3D_MAPTBL(test_panel_acl_opr_table, &PNOBJ_FUNC(init_common_maptbl), &PNOBJ_FUNC(getidx_acl_opr_table), &PNOBJ_FUNC(copy_common_maptbl)),
	[DIA_ONOFF_MAPTBL] = DEFINE_2D_MAPTBL(test_panel_dia_onoff_table, &PNOBJ_FUNC(init_common_maptbl), &PNOBJ_FUNC(getidx_dia_onoff_table), &PNOBJ_FUNC(copy_common_maptbl)),
};

static struct maptbl test_panel_dummy_maptbl =
	DEFINE_0D_MAPTBL(test_panel_dummy_table, &PNOBJ_FUNC(init_common_maptbl), NULL, &PNOBJ_FUNC(copy_common_maptbl));

static u8 TEST_PANEL_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 TEST_PANEL_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 TEST_PANEL_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 TEST_PANEL_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 TEST_PANEL_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 TEST_PANEL_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 TEST_PANEL_SLEEP_OUT[] = { 0x11 };
static u8 TEST_PANEL_SLEEP_IN[] = { 0x10 };
static u8 TEST_PANEL_DISPLAY_OFF[] = { 0x28 };
static u8 TEST_PANEL_DISPLAY_ON[] = { 0x29 };
static u8 TEST_PANEL_ACL_OPR[] = { 0x55, 0x01 };
static u8 TEST_PANEL_DIA_ONOFF[] = { 0x92, 0x00 };

static DEFINE_STATIC_PACKET(test_panel_level1_key_enable, DSI_PKT_TYPE_WR, TEST_PANEL_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(test_panel_level2_key_enable, DSI_PKT_TYPE_WR, TEST_PANEL_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(test_panel_level3_key_enable, DSI_PKT_TYPE_WR, TEST_PANEL_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(test_panel_level1_key_disable, DSI_PKT_TYPE_WR, TEST_PANEL_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(test_panel_level2_key_disable, DSI_PKT_TYPE_WR, TEST_PANEL_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(test_panel_level3_key_disable, DSI_PKT_TYPE_WR, TEST_PANEL_KEY3_DISABLE, 0);
static DEFINE_STATIC_PACKET(test_panel_sleep_out, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(test_panel_sleep_in, DSI_PKT_TYPE_WR, TEST_PANEL_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(test_panel_display_on, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(test_panel_display_off, DSI_PKT_TYPE_WR, TEST_PANEL_DISPLAY_OFF, 0);

static DECLARE_PKTUI(test_panel_acl_opr) = {
	{ .offset = 1, .maptbl = &test_panel_maptbl[ACL_OPR_MAPTBL] },
	{ .offset = 1, .maptbl = &test_panel_dummy_maptbl },
};
static DEFINE_VARIABLE_PACKET(test_panel_acl_opr, DSI_PKT_TYPE_WR, TEST_PANEL_ACL_OPR, 0);
static DEFINE_PKTUI(test_panel_dia_onoff, &test_panel_maptbl[DIA_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(test_panel_dia_onoff, DSI_PKT_TYPE_WR, TEST_PANEL_DIA_ONOFF, 0x195);

static DEFINE_PANEL_KEY(test_panel_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(test_panel_level1_key_enable));
static DEFINE_PANEL_KEY(test_panel_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(test_panel_level2_key_enable));
static DEFINE_PANEL_KEY(test_panel_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(test_panel_level3_key_enable));
static DEFINE_PANEL_KEY(test_panel_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(test_panel_level1_key_disable));
static DEFINE_PANEL_KEY(test_panel_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(test_panel_level2_key_disable));
static DEFINE_PANEL_KEY(test_panel_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(test_panel_level3_key_disable));

static DEFINE_PANEL_MDELAY(test_panel_wait_17msec, 17);
static DEFINE_PANEL_TIMER_MDELAY(test_panel_wait_sleep_out_110msec, 110);
static DEFINE_PANEL_TIMER_BEGIN(test_panel_wait_sleep_out_110msec,
		TIMER_DLYINFO(&test_panel_wait_sleep_out_110msec));
static DEFINE_PANEL_MDELAY(test_panel_wait_sleep_in, 120);

static inline bool cond_is_true(struct panel_device *panel) { return true; }
static DEFINE_PNOBJ_FUNC(cond_is_true, cond_is_true);

static DEFINE_FUNC_BASED_COND(test_panel_cond_is_true, &PNOBJ_FUNC(cond_is_true));

static DEFINE_PCTRL(test_panel_fast_discharge_enable, "panel_fd_enable");
static DEFINE_PCTRL(test_panel_fast_discharge_disable, "panel_fd_disable");

static DEFINE_PNOBJ_CONFIG(test_panel_set_wait_tx_done_property_off, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
static DEFINE_PNOBJ_CONFIG(test_panel_set_wait_tx_done_property_auto, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

static void *test_panel_set_bl_cmdtbl[] = {
	&PKTINFO(test_panel_acl_opr),
	&PKTINFO(test_panel_dia_onoff),
};
static DEFINE_SEQINFO(test_panel_set_bl_seq, test_panel_set_bl_cmdtbl);

#if defined(CONFIG_USDM_LPD_AUTO_BR)
u8 watch_brt_table[10][2] = {
	{ 0x00, 0x1F },
	{ 0x00, 0x45 },
	{ 0x00, 0x5A },
	{ 0x00, 0x66 },
	{ 0x00, 0x70 },
	{ 0x00, 0x7A },
	{ 0x00, 0xAC },
	{ 0x00, 0xDE },
	{ 0x01, 0x10 },
	{ 0x01, 0x47 },
};

u8 watch_02_brt_table[10][2] = {
	{ 0x00, 0x1F },
	{ 0x00, 0x3D },
	{ 0x00, 0x4C },
	{ 0x00, 0x54 },
	{ 0x00, 0x5C },
	{ 0x00, 0x64 },
	{ 0x00, 0x8A },
	{ 0x00, 0xB1 },
	{ 0x00, 0xD8 },
	{ 0x01, 0x78 },
};

unsigned int lpd_br_table[10] = { 0, 10, 20, 30, 40, 50, 100, 150, 200, 255 };
static int getidx_lpd_br_maptbl(struct maptbl *m)
{
	struct panel_device *panel = m->pdata;
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	int i;

	for (i = 0; i < ARRAY_SIZE(lpd_br_table); i++) {
		if (panel_bl->props.brightness <= lpd_br_table[i])
			break;
	}

	if (i == ARRAY_SIZE(lpd_br_table))
		i = ARRAY_SIZE(lpd_br_table) - 1;

	return maptbl_index(m, 0, i, 0);
}
static DEFINE_PNOBJ_FUNC(getidx_lpd_br_maptbl, getidx_lpd_br_maptbl);
static DEFINE_RULE_BASED_COND(watch_cond_is_id_ge_02, PANEL_PROPERTY_PANEL_ID_3, GE, 0x02);
static u8 WATCH_02_WRDISBV[] = { 0x51, 0x35, 0x00 };
static struct maptbl watch_02_brt_maptbl = DEFINE_2D_MAPTBL(watch_02_brt_table,
			&PNOBJ_FUNC(init_common_maptbl), &PNOBJ_FUNC(getidx_lpd_br_maptbl), &PNOBJ_FUNC(copy_common_maptbl));
static DEFINE_PKTUI(watch_02_wrdisbv, &watch_02_brt_maptbl, 1);
static DEFINE_VARIABLE_PACKET(watch_02_wrdisbv, DSI_PKT_TYPE_WR, WATCH_02_WRDISBV, 0);
static u8 WATCH_WRDISBV[] = { 0x51, 0x35, 0x00 };
static struct maptbl watch_brt_maptbl = DEFINE_2D_MAPTBL(watch_brt_table,
			&PNOBJ_FUNC(init_common_maptbl), &PNOBJ_FUNC(getidx_lpd_br_maptbl), &PNOBJ_FUNC(copy_common_maptbl));
static DEFINE_PKTUI(watch_wrdisbv, &watch_brt_maptbl, 1);
static DEFINE_VARIABLE_PACKET(watch_wrdisbv, DSI_PKT_TYPE_WR, WATCH_WRDISBV, 0);
static void *watch_lpd_br_cmdtbl[] = {
	&CONDINFO_IF(watch_cond_is_id_ge_02),
	&PKTINFO(watch_02_wrdisbv),
	&CONDINFO_EL(watch_cond_is_id_ge_02),
	&PKTINFO(watch_wrdisbv),
	&CONDINFO_FI(watch_cond_is_id_ge_02),
};
#endif

static void *test_panel_init_cmdtbl[] = {
	&KEYINFO(test_panel_level1_key_enable),
	&KEYINFO(test_panel_level2_key_enable),
	&KEYINFO(test_panel_level3_key_enable),
	&CONDINFO_IF(test_panel_cond_is_true),
	&PKTINFO(test_panel_sleep_out),
	&SEQINFO(test_panel_set_bl_seq),
	&DLYINFO(test_panel_wait_17msec),
	&TIMER_DLYINFO_BEGIN(test_panel_wait_sleep_out_110msec),
	&TIMER_DLYINFO(test_panel_wait_sleep_out_110msec),
	&CONDINFO_FI(test_panel_cond_is_true),
	&KEYINFO(test_panel_level3_key_disable),
	&KEYINFO(test_panel_level2_key_disable),
	&KEYINFO(test_panel_level1_key_disable),
	&PWRCTRL(test_panel_fast_discharge_enable),
};

static void *test_panel_res_init_cmdtbl[] = {
	&KEYINFO(test_panel_level1_key_enable),
	&KEYINFO(test_panel_level2_key_enable),
	&KEYINFO(test_panel_level3_key_enable),
	&test_panel_restbl[RES_COORDINATE],
	&test_panel_restbl[RES_CODE],
	&test_panel_restbl[RES_DATE],
	&test_panel_restbl[RES_OCTA_ID],
	&KEYINFO(test_panel_level3_key_disable),
	&KEYINFO(test_panel_level2_key_disable),
	&KEYINFO(test_panel_level1_key_disable),
};

static void *test_panel_display_on_cmdtbl[] = {
	&PNOBJ_CONFIG(test_panel_set_wait_tx_done_property_off),
	&KEYINFO(test_panel_level1_key_enable),
	&PKTINFO(test_panel_display_on),
	&KEYINFO(test_panel_level1_key_disable),
	&PNOBJ_CONFIG(test_panel_set_wait_tx_done_property_auto),
};

static void *test_panel_display_off_cmdtbl[] = {
	&KEYINFO(test_panel_level1_key_enable),
	&PKTINFO(test_panel_display_off),
	&KEYINFO(test_panel_level1_key_disable),
};

static void *test_panel_exit_cmdtbl[] = {
	&KEYINFO(test_panel_level1_key_enable),
	&KEYINFO(test_panel_level2_key_enable),
	&test_panel_dmptbl[DUMP_RDDPM],
	&test_panel_dmptbl[DUMP_RDDSM],
	&KEYINFO(test_panel_level2_key_disable),
	&PKTINFO(test_panel_sleep_in),
	&KEYINFO(test_panel_level1_key_disable),
	&DLYINFO(test_panel_wait_sleep_in),
	&PWRCTRL(test_panel_fast_discharge_disable),
};

static struct seqinfo test_panel_seqtbl[] = {
	SEQINFO_INIT(PANEL_INIT_SEQ, test_panel_init_cmdtbl),
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, test_panel_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, test_panel_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, test_panel_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, test_panel_exit_cmdtbl),
#if defined(CONFIG_USDM_LPD_AUTO_BR)
	SEQINFO_INIT(PANEL_LPD_BR_SEQ, watch_lpd_br_cmdtbl),
#endif
};

struct common_panel_info test_panel_info = {
	.ldi_name = "test_ddi",
	.name = "test_panel",
	.vendor = "SDC",

	/* panel internal object */
	.maptbl = test_panel_maptbl,
	.nr_maptbl = ARRAY_SIZE(test_panel_maptbl),
	.seqtbl = test_panel_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(test_panel_seqtbl),
	.rditbl = test_panel_rditbl,
	.nr_rditbl = ARRAY_SIZE(test_panel_rditbl),
	.restbl = test_panel_restbl,
	.nr_restbl = ARRAY_SIZE(test_panel_restbl),
	.dumpinfo = test_panel_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(test_panel_dmptbl),
};

static int __init test_panel_init(void)
{
	panel_function_insert(&PNOBJ_FUNC(dump_show_rddpm));
	panel_function_insert(&PNOBJ_FUNC(dump_show_rddsm));
	panel_function_insert(&PNOBJ_FUNC(init_common_maptbl));
	panel_function_insert(&PNOBJ_FUNC(getidx_acl_opr_table));
	panel_function_insert(&PNOBJ_FUNC(getidx_dia_onoff_table));
	panel_function_insert(&PNOBJ_FUNC(copy_common_maptbl));
	panel_function_insert(&PNOBJ_FUNC(cond_is_true));
#if defined(CONFIG_USDM_LPD_AUTO_BR)
	panel_function_insert(&PNOBJ_FUNC(getidx_lpd_br_maptbl));
#endif

	register_common_panel(&test_panel_info);

	return 0;
}

static void __exit test_panel_exit(void)
{
	deregister_common_panel(&test_panel_info);
}

module_init(test_panel_init)
module_exit(test_panel_exit)
