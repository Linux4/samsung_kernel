// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <dt-bindings/mml/mml-mt6989.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "mtk-mml-drm-adaptor.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"

#define TOPOLOGY_PLATFORM	"mt6989"
#define AAL_MIN_WIDTH		50	/* TODO: define in tile? */
/* 2k size and pixel as upper bound */
#define MML_IR_WIDTH_2K		2560
#define MML_IR_HEIGHT_2K	1440
#define MML_IR_2K		(MML_IR_WIDTH_2K * MML_IR_HEIGHT_2K)
/* hd size and pixel as lower bound */
#define MML_IR_WIDTH		640
#define MML_IR_HEIGHT		480
#define MML_IR_MIN		(MML_IR_WIDTH * MML_IR_HEIGHT)
#define MML_IR_RSZ_MIN_RATIO	375	/* resize must lower than this ratio */
#define MML_OUT_MIN_W		2000
#define MML_DL_MAX_W		1920
#define MML_DL_MAX_H		1096
#define MML_DL_RROT_S_PX	(1920 * 1088)
#define MML_MIN_SIZE		720

/* use OPP index 0(229Mhz) 1(273Mhz) 2(458Mhz) */
#define MML_IR_MAX_OPP		2

/* max hrt in MB/s, see mmqos hrt support */
int mml_max_hrt = 6988;
module_param(mml_max_hrt, int, 0644);

int mml_force_rsz;
module_param(mml_force_rsz, int, 0644);

int mml_rgbrot;
module_param(mml_rgbrot, int, 0644);

int mml_path_mode;
module_param(mml_path_mode, int, 0644);

/* debug param
 * 0: (default)don't care, check dts property to enable racing
 * 1: force enable
 * 2: force disable
 */
int mml_racing;
module_param(mml_racing, int, 0644);

/* debug param
 * 0: (default)don't care, check dts property to enable racing
 * 1: force auto query (skip dts option check)
 * 2: force disable
 * 3: force enable
 */
int mml_dl = 1;
module_param(mml_dl, int, 0644);

int mml_rrot;
module_param(mml_rrot, int, 0644);

/* Single RROT support
 * 0: (default)auto query
 * 1: force enable and don't case performance
 * 2: force disable
 */
int mml_rrot_single = 2;
module_param(mml_rrot_single, int, 0644);

int mml_racing_rsz = 1;
module_param(mml_racing_rsz, int, 0644);

int mml_dpc;
module_param(mml_dpc, int, 0644);

/* 0: off
 * 1: on
 */
int mml_binning = 1;
module_param(mml_binning, int, 0644);

struct path_node {
	u8 eng;
	u8 next0;
	u8 next1;
};

/* !!Following code generate by topology parser (tpparser.py)!!
 * include: topology_scenario, path_map, engine_reset_bit
 */
enum topology_scenario {
	PATH_MML_NOPQ = 0,
	PATH_MML_PQ,
	PATH_MML_PQ_DL,
	PATH_MML_PQ_DD,
	PATH_MML_RR_NOPQ,
	PATH_MML_RR,
	PATH_MML_RR_DL,
	PATH_MML_2IN_2OUT,
	PATH_MML_RR_2IN_2OUT,
	PATH_MML_RR_DL_2IN_2OUT,
	PATH_MML_RR_DL2,
	PATH_MML_RR_DL2_2IN_2OUT,
	PATH_MML_RRS,
	PATH_MML_RRS_DL,
	PATH_MML_RRS_DL_2IN_2OUT,
	PATH_MML_RRS_DL2,
	PATH_MML_RRS_DL2_2IN_2OUT,
	PATH_MML_MAX
};

static const struct path_node path_map[PATH_MML_MAX][MML_MAX_PATH_NODES] = {
	[PATH_MML_NOPQ] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_PQ] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_C3D0,},
		{MML_C3D0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_PQ_DL] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_C3D0,},
		{MML_C3D0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_DLO0,},
		{MML_DLO0,},
	},
	[PATH_MML_PQ_DD] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_DLI0, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_C3D0,},
		{MML_C3D0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_DLO0,},
		{MML_DLO0,},
	},
	[PATH_MML_RR_NOPQ] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_RROT0_2ND, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_RR] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_RROT0_2ND, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_C3D0,},
		{MML_C3D0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_RR_DL] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_RROT0_2ND, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_C3D0,},
		{MML_C3D0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_DLO0,},
		{MML_DLO0,},
	},
	[PATH_MML_2IN_2OUT] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RDMA0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL, MML_RSZ2,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_C3D0,},
		{MML_C3D0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_RDMA2, MML_BIRSZ0,},
		{MML_BIRSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_WROT0,},
		{MML_RSZ2, MML_WROT2,},
		{MML_WROT0,},
		{MML_WROT2,},
	},
	[PATH_MML_RR_2IN_2OUT] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_RROT0_2ND, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL, MML_RSZ2,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_C3D0,},
		{MML_C3D0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_RDMA2, MML_BIRSZ0,},
		{MML_BIRSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_WROT0,},
		{MML_RSZ2, MML_WROT2,},
		{MML_WROT0,},
		{MML_WROT2,},
	},
	[PATH_MML_RR_DL_2IN_2OUT] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_RROT0_2ND, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL, MML_RSZ2,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_C3D0,},
		{MML_C3D0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_RDMA2, MML_BIRSZ0,},
		{MML_BIRSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_DLO0,},
		{MML_RSZ2, MML_WROT2,},
		{MML_DLO0,},
		{MML_WROT2,},
	},
	[PATH_MML_RR_DL2] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_RROT0_2ND, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_DLO2,},
		{MML_DLO2,},
	},
	[PATH_MML_RR_DL2_2IN_2OUT] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_RROT0_2ND, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL, MML_RSZ2,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_RDMA2, MML_BIRSZ0,},
		{MML_BIRSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_DLO2,},
		{MML_RSZ2, MML_WROT2,},
		{MML_DLO2,},
		{MML_WROT2,},
	},
	[PATH_MML_RRS] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_WROT0,},
		{MML_WROT0,},
	},
	[PATH_MML_RRS_DL] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_DLO0,},
		{MML_DLO0,},
	},
	[PATH_MML_RRS_DL_2IN_2OUT] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL, MML_RSZ2,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_RDMA2, MML_BIRSZ0,},
		{MML_BIRSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_DLO0,},
		{MML_RSZ2, MML_WROT2,},
		{MML_DLO0,},
		{MML_WROT2,},
	},
	[PATH_MML_RRS_DL2] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_DLO2,},
		{MML_DLO2,},
	},
	[PATH_MML_RRS_DL2_2IN_2OUT] = {
		{MML_MMLSYS,},
		{MML_MUTEX,},
		{MML_RROT0, MML_MERGE0,},
		{MML_MERGE0, MML_DMA0_SEL,},
		{MML_DMA0_SEL, MML_DLI0_SEL, MML_RSZ2,},
		{MML_DLI0_SEL, MML_FG0,},
		{MML_FG0, MML_HDR0,},
		{MML_HDR0, MML_AAL0,},
		{MML_AAL0, MML_PQ_AAL0_SEL,},
		{MML_PQ_AAL0_SEL, MML_RSZ0,},
		{MML_RSZ0, MML_TDSHP0,},
		{MML_RDMA2, MML_BIRSZ0,},
		{MML_BIRSZ0, MML_TDSHP0,},
		{MML_TDSHP0, MML_COLOR0,},
		{MML_COLOR0, MML_WROT0_SEL,},
		{MML_WROT0_SEL, MML_DLO2,},
		{MML_RSZ2, MML_WROT2,},
		{MML_DLO2,},
		{MML_WROT2,},
	},
};

/* reset bit to each engine,
 * reverse of MMSYS_SW0_RST_B and MMSYS_SW1_RST_B
 */
static u8 engine_reset_bit[MML_ENGINE_TOTAL] = {
	[MML_MUTEX] = 0,
	[MML_RDMA0] = 3,
	[MML_RDMA2] = 4,
	[MML_HDR0] = 5,
	[MML_AAL0] = 6,
	[MML_RSZ0] = 7,
	[MML_TDSHP0] = 8,
	[MML_COLOR0] = 9,
	[MML_WROT0] = 10,
	[MML_RSZ2] = 24,
	[MML_WROT2] = 25,
	[MML_BIRSZ0] = 3,
	[MML_RROT0] = 8,
	[MML_RROT0_2ND] = 9,
	[MML_MERGE0] = 10,
	[MML_C3D0] = 11,
	[MML_FG0] = 12,
};
/* !!Above code generate by topology parser (tpparser.py)!! */

static inline bool engine_input(u32 id)
{
	return id == MML_RDMA0 || id == MML_RDMA2 ||
		id == MML_RROT0 || id == MML_RROT0_2ND ||
		id == MML_DLI0;
}

/* check if engine is output dma engine */
static inline bool engine_wrot(u32 id)
{
	return id == MML_WROT0;
}

/* check if engine is input region pq rdma engine */
static inline bool engine_pq_rdma(u32 id)
{
	return id == MML_RDMA2;
}

/* check if engine is input region pq birsz engine */
static inline bool engine_pq_birsz(u32 id)
{
	return id == MML_BIRSZ0;
}

/* check if engine is region pq engine */
static inline bool engine_region_pq(u32 id)
{
	return id == MML_RDMA2 || id == MML_BIRSZ0;
}

/* check if engine is dma engine */
static inline bool engine_dma(u32 id)
{
	return id == MML_RDMA0 || id == MML_RROT0 || id == MML_RROT0_2ND || id == MML_RDMA2 ||
		id == MML_FG0 || id == MML_WROT0 || id == MML_WROT2;
}

static inline bool engine_tdshp(u32 id)
{
	return id == MML_TDSHP0;
}

enum cmdq_clt_usage {
	MML_CLT_PIPE0,
	MML_CLT_PIPE1,
	MML_CLT_MAX
};

static const u8 clt_dispatch[PATH_MML_MAX] = {
	[PATH_MML_NOPQ] = MML_CLT_PIPE0,
	[PATH_MML_PQ] = MML_CLT_PIPE0,
	[PATH_MML_PQ_DL] = MML_CLT_PIPE0,
	[PATH_MML_PQ_DD] = MML_CLT_PIPE0,
	[PATH_MML_RR_NOPQ] = MML_CLT_PIPE0,
	[PATH_MML_RR] = MML_CLT_PIPE0,
	[PATH_MML_RR_DL] = MML_CLT_PIPE0,
	[PATH_MML_2IN_2OUT] = MML_CLT_PIPE0,
	[PATH_MML_RR_2IN_2OUT] = MML_CLT_PIPE0,
	[PATH_MML_RR_DL_2IN_2OUT] = MML_CLT_PIPE0,
};

/* mux sof group of mmlsys mout/sel */
enum mux_sof_group {
	MUX_SOF_GRP0 = 0,
	MUX_SOF_GRP1,
	MUX_SOF_GRP2,
	MUX_SOF_GRP3,
	MUX_SOF_GRP4,
	MUX_SOF_GRP5,
	MUX_SOF_GRP6,
	MUX_SOF_GRP7,
};

static const u8 grp_dispatch[PATH_MML_MAX] = {
	[PATH_MML_NOPQ] = MUX_SOF_GRP1,
	[PATH_MML_PQ] = MUX_SOF_GRP1,
	[PATH_MML_PQ_DL] = MUX_SOF_GRP1,
	[PATH_MML_PQ_DD] = MUX_SOF_GRP1,
	[PATH_MML_RR_NOPQ] = MUX_SOF_GRP1,
	[PATH_MML_RR] = MUX_SOF_GRP1,
	[PATH_MML_RR_DL] = MUX_SOF_GRP1,
	[PATH_MML_2IN_2OUT] = MUX_SOF_GRP1,
	[PATH_MML_RR_2IN_2OUT] = MUX_SOF_GRP1,
	[PATH_MML_RR_DL_2IN_2OUT] = MUX_SOF_GRP1,
};

/* 6.6 ms as dc mode active time threshold by:
 * 1 / (fps * vblank) = 1000000 / 120 / 1.25 = 6666us
 */
#define MML_DC_ACT_DUR	6600
static u32 opp_pixel_table[MML_MAX_OPPS];

static void tp_dump_path(const struct mml_topology_path *path)
{
	u8 i;

	for (i = 0; i < path->node_cnt; i++) {
		mml_log(
			"[topology]%u engine %u (%p) prev %p %p next %p %p comp %p tile idx %u out %u",
			i, path->nodes[i].id, &path->nodes[i],
			path->nodes[i].prev[0], path->nodes[i].prev[1],
			path->nodes[i].next[0], path->nodes[i].next[1],
			path->nodes[i].comp,
			path->nodes[i].tile_eng_idx,
			path->nodes[i].out_idx);
	}
}

static void tp_dump_path_short(struct mml_topology_path *path)
{
	char path_desc[64];
	u32 len = 0;
	u8 i;

	for (i = 0; i < path->node_cnt; i++)
		len += snprintf(path_desc + len, sizeof(path_desc) - len, " %u",
			path->nodes[i].id);
	mml_log("[topology]engines:%s", path_desc);
}

static void tp_parse_connect_prev(const struct path_node *route, struct mml_path_node *nodes,
	u8 cur_idx)
{
	u32 i;
	u32 in_idx = 0;	/* current engine input index */
	u32 eng_id = nodes[cur_idx].id;

	for (i = 0; i < cur_idx && in_idx < 2; i++) {
		u32 prev_out_idx;	/* previous engine output index */

		if (route[i].next0 == eng_id)
			prev_out_idx = 0;
		else if (route[i].next1 == eng_id)
			prev_out_idx = 1;
		else
			continue;

		nodes[i].next[prev_out_idx] = &nodes[cur_idx];
		nodes[cur_idx].prev[in_idx++] = &nodes[i];

		if (nodes[i].out_idx || prev_out_idx)
			nodes[cur_idx].out_idx = 1;
	}

	if (!in_idx && !engine_input(eng_id))
		mml_err("[topology]connect fail idx:%u engine:%u", i, eng_id);
}

static void tp_parse_path(struct mml_dev *mml, struct mml_topology_path *path,
	const struct path_node *route)
{
	u8 i, tile_idx, out_eng_idx;
	struct mml_path_node *pq_rdma = NULL;
	struct mml_path_node *pq_birsz = NULL;

	for (i = 0; i < MML_MAX_PATH_NODES; i++) {
		const u8 eng = route[i].eng;

		if (!route[i].eng) {
			path->node_cnt = i;
			break;
		}

		/* assign current engine */
		path->nodes[i].id = eng;
		path->nodes[i].comp = mml_dev_get_comp_by_id(mml, eng);
		if (!path->nodes[i].comp)
			mml_err("[topology]no comp idx:%u engine:%u", i, eng);

		/* assign reset bits for this path */
		path->reset_bits |= 1LL << engine_reset_bit[eng];

		if (eng == MML_MMLSYS) {
			path->mmlsys = path->nodes[i].comp;
			path->mmlsys_idx = i;
			continue;
		} else if (eng == MML_MUTEX) {
			path->mutex = path->nodes[i].comp;
			path->mutex_idx = i;
			continue;
		} else if (engine_pq_rdma(eng)) {
			pq_rdma = &path->nodes[i];
			continue;
		} else if (engine_pq_birsz(eng)) {
			pq_birsz = &path->nodes[i];
			continue;
		} else if (engine_tdshp(eng)) {
			path->tdshp_id = eng;
			if (pq_rdma && pq_birsz) {
				pq_birsz->prev[0] = pq_rdma;
				pq_rdma->next[0] = pq_birsz;
				pq_birsz->next[0] = &path->nodes[i];
				path->nodes[i].prev[1] = pq_birsz;
			}
		}

		/* find and connect previous engine to current node */
		tp_parse_connect_prev(route, path->nodes, i);

		/* for svp aid binding */
		if (engine_dma(eng) && path->aid_eng_cnt < MML_MAX_AID_COMPS)
			path->aid_engine_ids[path->aid_eng_cnt++] = eng;
	}
	path->node_cnt = i;

	/* 0: reset
	 * 1: not reset
	 * so we need to reverse the bits
	 */
	path->reset_bits = ~path->reset_bits;
	mml_msg("[topology]reset bits %#llx", path->reset_bits);

	/* collect tile engines */
	tile_idx = 0;
	for (i = 0; i < path->node_cnt; i++) {
		if ((!path->nodes[i].prev[0] && !path->nodes[i].next[0]) ||
		    engine_region_pq(path->nodes[i].id)) {
			path->nodes[i].tile_eng_idx = ~0;
			continue;
		}
		/* assume rrot0_2nd always follow rrot0 */
		if (path->nodes[i].id == MML_RROT0_2ND) {
			path->nodes[i].tile_eng_idx = tile_idx - 1;
			continue;
		}
		path->nodes[i].tile_eng_idx = tile_idx;
		path->tile_engines[tile_idx++] = i;
	}
	path->tile_engine_cnt = tile_idx;

	/* scan region pq in engines */
	for (i = 0; i < path->node_cnt; i++) {
		if (engine_pq_rdma(path->nodes[i].id)) {
			path->nodes[i].tile_eng_idx = path->tile_engine_cnt;
			if (path->tile_engine_cnt < MML_MAX_PATH_NODES)
				path->tile_engines[path->tile_engine_cnt] = i;
			else
				mml_err("[topology]RDMA tile_engines idx %d >= MML_MAX_PATH_NODES",
					path->tile_engine_cnt);
			if (path->pq_rdma_id)
				mml_err("[topology]multiple pq rdma engines: was %hhu now %hhu",
					path->pq_rdma_id,
					path->nodes[i].id);
			path->pq_rdma_id = path->nodes[i].id;
		} else if (engine_pq_birsz(path->nodes[i].id)) {
			path->nodes[i].tile_eng_idx = path->tile_engine_cnt + 1;
			if (path->tile_engine_cnt + 1 < MML_MAX_PATH_NODES)
				path->tile_engines[path->tile_engine_cnt + 1] = i;
			else
				mml_err("[topology]BIRSZ tile_engines idx %d >= MML_MAX_PATH_NODES",
					path->tile_engine_cnt + 1);
		}
	}

	/* scan out engines */
	for (i = 0; i < path->node_cnt; i++) {
		if (!engine_wrot(path->nodes[i].id))
			continue;
		out_eng_idx = path->nodes[i].out_idx;
		if (path->out_engine_ids[out_eng_idx])
			mml_err("[topology]multiple out engines: was %u now %u on out idx:%u",
				path->out_engine_ids[out_eng_idx],
				path->nodes[i].id, out_eng_idx);
		path->out_engine_ids[out_eng_idx] = path->nodes[i].id;
	}
}

static s32 tp_init_cache(struct mml_dev *mml, struct mml_topology_cache *cache,
	struct cmdq_client **clts, u32 clt_cnt)
{
	u32 i;

	if (clt_cnt < MML_CLT_MAX) {
		mml_err("[topology]%s not enough cmdq clients to all paths",
			__func__);
		return -ECHILD;
	}
	if (ARRAY_SIZE(cache->paths) < PATH_MML_MAX) {
		mml_err("[topology]%s not enough path cache for all paths",
			__func__);
		return -ECHILD;
	}

	for (i = 0; i < PATH_MML_MAX; i++) {
		struct mml_topology_path *path = &cache->paths[i];

		tp_parse_path(mml, path, path_map[i]);
		if (mtk_mml_msg) {
			mml_log("[topology]dump path %u count %u clt id %u",
				i, path->node_cnt, clt_dispatch[i]);
			tp_dump_path(path);
		}

		/* now dispatch cmdq client (channel) to path */
		path->clt = clts[clt_dispatch[i]];
		path->clt_id = clt_dispatch[i];
		path->mux_group = grp_dispatch[i];
	}

	return 0;
}

static inline bool tp_need_resize(struct mml_frame_info *info, bool *can_binning)
{
	u32 w = info->dest[0].data.width;
	u32 h = info->dest[0].data.height;
	u32 cw = info->dest[0].crop.r.width;
	u32 ch = info->dest[0].crop.r.height;

	if (info->dest[0].rotate == MML_ROT_90 ||
		info->dest[0].rotate == MML_ROT_270)
		swap(w, h);

	mml_msg("[topology]%s target %ux%u crop %ux%u",
		__func__, w, h, cw, ch);

	/* default binning off */
	if (can_binning)
		*can_binning = false;

	/* for binning */
	if (mml_binning && info->mode == MML_MODE_DIRECT_LINK) {
		if (can_binning && (cw >= w * 2 || ch >= h * 2))
			*can_binning = true;

		if (cw >= w * 2)
			cw = cw / 2;
		if (ch >= h * 2)
			ch = ch / 2;
	}

	return info->dest_cnt != 1 ||
		cw != w || ch != h ||
		info->dest[0].crop.x_sub_px ||
		info->dest[0].crop.y_sub_px ||
		info->dest[0].crop.w_sub_px ||
		info->dest[0].crop.h_sub_px ||
		info->dest[0].compose.width != info->dest[0].data.width ||
		info->dest[0].compose.height != info->dest[0].data.height;
}

static bool tp_check_tput(struct mml_frame_info *info, struct mml_topology_cache *tp,
	u32 panel_width, u32 panel_height, bool *dual)
{
	u32 srcw, srch, binw, binh;
	u32 cropw = info->dest[0].crop.r.width;
	u32 croph = info->dest[0].crop.r.height;
	const u32 destw = info->dest[0].data.width;
	const u32 desth = info->dest[0].data.height;
	const enum mml_orientation rotate = info->dest[0].rotate;
	const u32 plane = MML_FMT_PLANE(info->src.format);
	u64 tput, pixel, hrt;

	/* always assign dual as default */
	*dual = true;

	/* disp not provide act time, assume throughput ok */
	if (!info->act_time)
		return true;

	if (!tp->opp_cnt) {
		mml_err("no opp table support");
		return false;
	}

	srcw = round_up(info->dest[0].crop.r.left + info->dest[0].crop.r.width, 32) -
		round_down(info->dest[0].crop.r.left, 32);
	srch = round_up(info->dest[0].crop.r.top + info->dest[0].crop.r.height, 16) -
		round_down(info->dest[0].crop.r.top, 16);
	binw = srcw;
	binh = srch;

	/* rotate source */
	if (rotate == MML_ROT_90 || rotate == MML_ROT_270) {
		swap(binw, binh);
		swap(cropw, croph);
	}

	/* binning case */
	if (MML_FMT_YUV420(info->src.format)) {
		if ((cropw >> 1) >= destw)
			binw = binw >> 1;
		if ((croph >> 1) >= desth)
			binh = binh >> 1;
	}

	/* not support if exceeding max throughput
	 * pixel per-pipe is:
	 *	pipe_pixel = pixel / 2 * 1.1
	 * and necessary throughput:
	 *	pipe_pixel / active_time(ns) * 1000
	 * so merge all constant:
	 *	tput = pixel / 2 * 11 / 10 / (act_time / 1000)
	 */
	pixel = max(binw, destw) * max(binh, desth);
	tput = pixel * 11 / 20 / (info->act_time / 1000);
	if (panel_width > destw)
		tput = tput * panel_width / destw;
	if (mml_rrot_single != 2 && binw * binh <= MML_DL_RROT_S_PX &&
		tput < tp->opp_speeds[tp->opp_cnt / 2]) {
		*dual = false;
		goto check_hrt;
	} else if (tput < tp->opp_speeds[tp->opp_cnt - 1]) {
		if (mml_rrot_single == 1)
			*dual = false;
		else
			*dual = true;
		goto check_hrt;
	}

	return false;

check_hrt:
	/* calculate source data size as bandwidth */
	hrt = mml_color_get_min_y_size(info->src.format, srcw, srch);
	if (!MML_FMT_COMPRESS(info->src.format) && plane > 1)
		hrt += (u64)mml_color_get_min_uv_size(info->src.format, srcw, srch) * (plane - 1);
	hrt = hrt * 1000 / info->act_time;
	mml_msg("%s tput %llu hrt %llu", __func__, tput, hrt);
	if (hrt <= mml_max_hrt)
		return true;

	return false;
}

static enum topology_scenario scene_to_ovl1(u32 ovlsys_id, enum topology_scenario scene)
{
	if (ovlsys_id == MML_DLO_OVLSYS0)
		return scene;

	switch (scene) {
	case PATH_MML_RR_DL:
		scene = PATH_MML_RR_DL2;
		break;
	case PATH_MML_RRS_DL:
		scene = PATH_MML_RRS_DL2;
		break;
	case PATH_MML_RR_DL_2IN_2OUT:
		scene = PATH_MML_RR_DL2_2IN_2OUT;
		break;
	case PATH_MML_RRS_DL_2IN_2OUT:
		scene = PATH_MML_RRS_DL2_2IN_2OUT;
		break;
	default:
		break;
	}

	return scene;
}

static void tp_select_path(struct mml_topology_cache *cache,
	struct mml_frame_config *cfg,
	struct mml_topology_path **path)
{
	enum topology_scenario scene = 0;
	bool en_rsz, en_pq, can_binning = false, dual = true;
	enum mml_color dest_fmt = cfg->info.dest[0].data.format;

	if (cfg->info.mode == MML_MODE_RACING) {
		/* always rdma to wrot for racing case */
		scene = PATH_MML_NOPQ;
		goto check_rr;
	} else if (cfg->info.mode == MML_MODE_APUDC) {
		scene = PATH_MML_NOPQ;
		goto check_rr;
	}

	en_rsz = tp_need_resize(&cfg->info, &can_binning);
	if (mml_force_rsz)
		en_rsz = true;
	en_pq = en_rsz || cfg->info.dest[0].pq_config.en ||
		(MML_FMT_ALPHA(dest_fmt) && MML_FMT_IS_YUV(dest_fmt));

	if (cfg->info.mode == MML_MODE_DDP_ADDON) {
		/* direct-link in/out for addon case */
		if (cfg->info.dest_cnt == 2) {
			/* TODO: ddp addon 2out */
			scene = PATH_MML_PQ_DD; /* PATH_MML_2OUT_DD0 */
		} else {
			scene = PATH_MML_PQ_DD;
		}
	} else if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		/* TODO: pq or not pq in direct link mode */
		scene = PATH_MML_PQ_DL;
	} else if (!en_pq) {
		/* rdma to wrot */
		scene = PATH_MML_NOPQ;
		if (mml_rgbrot &&
		    MML_FMT_IS_RGB(cfg->info.src.format) && MML_FMT_IS_RGB(dest_fmt)) {
			mml_msg("[topology]enable rgb rotate");
			cfg->rgbrot = true;
		}
	} else if (cfg->info.dest_cnt == 2) {
		if (cfg->info.dest[0].pq_config.en_region_pq) {
			scene = PATH_MML_2IN_2OUT;
		} else {
			mml_err("[topology]dest 2 out but no region pq, back to 1in1out");
			scene = PATH_MML_PQ;
		}
	} else if (mml_force_rsz == 2) {
		scene = PATH_MML_PQ;
	} else {
		if (cfg->info.dest[0].pq_config.en_region_pq) {
			mml_err("[topology]not support 2 in 1 out, back to 1in1out");
			cfg->info.dest[0].pq_config.en_region_pq = false;
			scene = PATH_MML_PQ;
		} else {
			/* 1 in 1 out with PQs */
			scene = PATH_MML_PQ;
		}
	}

check_rr:
	tp_check_tput(&cfg->info, cache, cfg->panel_w, cfg->panel_h, &dual);

	if (cfg->info.mode == MML_MODE_DIRECT_LINK && mml_rrot != 2) {
		/* direct link mode and not force disalbe, change to RROT */
		if (cfg->info.dest_cnt == 2 && cfg->info.dest[0].pq_config.en_region_pq)
			scene = dual ? PATH_MML_RR_DL_2IN_2OUT : PATH_MML_RRS_DL_2IN_2OUT;
		else
			scene = dual ? PATH_MML_RR_DL : PATH_MML_RRS_DL;
	} else if (cfg->info.mode == MML_MODE_MML_DECOUPLE && mml_rrot == 1) {
		/* dc mode but force enable RROT */
		if (scene == PATH_MML_PQ)
			scene = dual ? PATH_MML_RR : PATH_MML_RRS;
		else if (scene == PATH_MML_NOPQ)
			scene = dual ? PATH_MML_RR_NOPQ : PATH_MML_RRS;
		else if (scene == PATH_MML_2IN_2OUT)
			scene = dual ? PATH_MML_RR_2IN_2OUT : PATH_MML_RRS;
		else
			scene = dual ? PATH_MML_RR : PATH_MML_RRS;
	}

	/* check if connect to ovlsys1 */
	scene = scene_to_ovl1(cfg->info.ovlsys_id, scene);

	cfg->rrot_dual = dual;
	*path = &cache->paths[scene];
}

static s32 tp_select(struct mml_topology_cache *cache,
	struct mml_frame_config *cfg)
{
	struct mml_topology_path *path = NULL;

	if (cfg->info.mode == MML_MODE_DDP_ADDON) {
		cfg->framemode = true;
		cfg->nocmd = true;
	} else if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		cfg->framemode = true;
	}
	cfg->shadow = true;

	tp_select_path(cache, cfg, &path);

	if (!path)
		return -EPERM;

	cfg->path[0] = path;
	cfg->alpharsz = cfg->info.alpha && MML_FMT_ALPHA(cfg->info.src.format) &&
		(!cfg->info.dest[0].pq_config.en || cfg->info.mode == MML_MODE_DIRECT_LINK);

	if (mml_dpc && !cfg->disp_vdo && !mml_dpc_disable(cfg->mml) &&
	    !(cfg->info.dest[0].pq_config.en_ur ||
	      cfg->info.dest[0].pq_config.en_dc ||
	      cfg->info.dest[0].pq_config.en_hdr ||
	      cfg->info.dest[0].pq_config.en_ccorr ||
	      cfg->info.dest[0].pq_config.en_dre ||
	      cfg->info.dest[0].pq_config.en_region_pq ||
	      cfg->info.dest[0].pq_config.en_fg ||
	      cfg->info.dest[0].pq_config.en_c3d) &&
	    (cfg->info.mode == MML_MODE_DIRECT_LINK ||
	     cfg->info.mode == MML_MODE_RACING ||
	     cfg->info.mode == MML_MODE_DDP_ADDON))
		cfg->dpc = true;
	else
		cfg->dpc = false;

	tp_dump_path_short(path);

	return 0;
}

static enum mml_mode tp_query_mode_dl(struct mml_dev *mml, struct mml_frame_info *info,
	u32 *reason, u32 panel_width, u32 panel_height)
{
	struct mml_topology_cache *tp;
	const struct mml_frame_dest *dest = &info->dest[0];
	const bool rotated = dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270;
	bool dual = true;

	if (unlikely(mml_dl)) {
		if (mml_dl == 2)
			goto decouple;
		else if (mml_dl == 3)
			goto dl_force;
	} else if (!mml_dl_enable(mml))
		goto decouple;

	/* no fg/c3d support for dl mode */
	if (dest->pq_config.en_fg ||
		dest->pq_config.en_c3d) {
		*reason = mml_query_pqen;
		goto decouple;
	}

	/* TODO: remove after AI region PQ DL mode enable */
	if (dest->pq_config.en_dc ||
		dest->pq_config.en_color ||
		dest->pq_config.en_hdr ||
		dest->pq_config.en_ccorr ||
		dest->pq_config.en_dre ||
		dest->pq_config.en_region_pq ||
		dest->pq_config.en_fg ||
		dest->pq_config.en_c3d) {
		*reason = mml_query_pqen;
		goto decouple;
	}

	if (info->src.width > MML_DL_MAX_W) {
		*reason = mml_query_inwidth;
		goto decouple;
	}

	if (info->src.height > MML_DL_MAX_H) {
		*reason = mml_query_inheight;
		goto decouple;
	}

	if ((!rotated && dest->crop.r.width < MML_MIN_SIZE) ||
		(rotated && dest->crop.r.height < MML_MIN_SIZE)) {
		*reason = mml_query_min_size;
		goto decouple;
	}

	/* destination width must cross display pipe width */
	if (dest->data.width < MML_OUT_MIN_W) {
		*reason = mml_query_outwidth;
		goto decouple;
	}

	/* get mid opp frequency */
	tp = mml_topology_get_cache(mml);
	if (!tp || !tp->opp_cnt) {
		mml_err("not support dl due to opp not ready");
		goto decouple;
	}

	if (!tp_check_tput(info, tp, panel_width, panel_height, &dual)) {
		*reason = mml_query_opp_out;
		goto decouple;
	}

dl_force:
	return MML_MODE_DIRECT_LINK;

decouple:
	return MML_MODE_MML_DECOUPLE;
}

static enum mml_mode tp_query_mode_racing(struct mml_dev *mml, struct mml_frame_info *info,
	u32 *reason)
{
	struct mml_topology_cache *tp;
	u32 pixel;

	if (unlikely(mml_racing)) {
		if (mml_racing == 2)
			goto decouple;
	} else if (!mml_racing_enable(mml))
		goto decouple;

	/* TODO: should REMOVE after inlinerot resize ready */
	if (unlikely(!mml_racing_rsz) && tp_need_resize(info, NULL)) {
		*reason = mml_query_norsz;
		goto decouple;
	}

	/* secure content cannot output to sram */
	if (info->src.secure || info->dest[0].data.secure) {
		*reason = mml_query_sec;
		goto decouple;
	}

	/* no pq support for racing mode */
	if (info->dest[0].pq_config.en_dc ||
		info->dest[0].pq_config.en_color ||
		info->dest[0].pq_config.en_hdr ||
		info->dest[0].pq_config.en_ccorr ||
		info->dest[0].pq_config.en_dre ||
		info->dest[0].pq_config.en_region_pq ||
		info->dest[0].pq_config.en_fg ||
		info->dest[0].pq_config.en_c3d) {
		*reason = mml_query_pqen;
		goto decouple;
	}

	/* get mid opp frequency */
	tp = mml_topology_get_cache(mml);
	if (!tp || !tp->opp_cnt) {
		mml_err("not support racing due to opp not ready");
		goto decouple;
	}

	pixel = max(info->src.width * info->src.height,
		info->dest[0].data.width * info->dest[0].data.height);

	if (info->act_time) {
		u32 i, dc_opp, ir_freq, ir_opp;
		u32 pipe_pixel = pixel / 2;

		if (!tp->opp_cnt) {
			mml_err("no opp table support");
			goto decouple;
		}

		if (!opp_pixel_table[0]) {
			for (i = 0; i < ARRAY_SIZE(opp_pixel_table); i++) {
				opp_pixel_table[i] = tp->opp_speeds[i] * MML_DC_ACT_DUR;
				mml_log("[topology]Racing pixel OPP %u: %u",
					i, opp_pixel_table[i]);
			}
		}
		for (i = 0; i < tp->opp_cnt; i++)
			if (pipe_pixel < opp_pixel_table[i])
				break;
		dc_opp = min_t(u32, i, ARRAY_SIZE(opp_pixel_table) - 1);
		if (dc_opp > MML_IR_MAX_OPP) {
			*reason = mml_query_opp_out;
			goto decouple;
		}

		ir_freq = pipe_pixel * 1000 / info->act_time;
		for (i = 0; i < tp->opp_cnt; i++)
			if (ir_freq < tp->opp_speeds[i])
				break;
		ir_opp = min_t(u32, i, ARRAY_SIZE(opp_pixel_table) - 1);

		/* simple check if ir mode need higher opp */
		if (ir_opp > dc_opp && ir_opp > 1) {
			*reason = mml_query_acttime;
			goto decouple;
		}
	}

	if (info->dest[0].crop.r.width > MML_IR_WIDTH_2K ||
		info->dest[0].crop.r.height > MML_IR_HEIGHT_2K ||
		pixel > MML_IR_2K) {
		*reason = mml_query_highpixel;
		goto decouple;
	}
	if (info->dest[0].crop.r.width < MML_IR_WIDTH ||
		info->dest[0].crop.r.height < MML_IR_HEIGHT ||
		pixel < MML_IR_MIN) {
		*reason = mml_query_lowpixel;
		goto decouple;
	}

	/* destination width must cross display pipe width */
	if (info->dest[0].data.width < MML_OUT_MIN_W) {
		*reason = mml_query_outwidth;
		goto decouple;
	}

	if (info->dest[0].data.width * info->dest[0].data.height * 1000 /
		info->dest[0].crop.r.width / info->dest[0].crop.r.height <
		MML_IR_RSZ_MIN_RATIO) {
		*reason = mml_query_rszratio;
		goto decouple;
	}

	return MML_MODE_RACING;

decouple:
	return MML_MODE_MML_DECOUPLE;
}

static enum mml_mode tp_query_mode(struct mml_dev *mml, struct mml_frame_info *info,
	u32 *reason, u32 panel_width, u32 panel_height)
{
	if (unlikely(mml_path_mode))
		return mml_path_mode;

	/* for alpha support */
	if (info->alpha) {
		*reason = mml_query_alpha;
		if (!MML_FMT_ALPHA(info->src.format) ||
		    info->src.width <= 9 ||
		    info->dest_cnt != 1 ||
		    info->dest[0].crop.r.width <= 9 ||
		    info->dest[0].compose.width <= 9)
			goto not_support;
		return MML_MODE_MML_DECOUPLE;
	}

	/* skip all racing mode check if use prefer dc */
	if (info->mode == MML_MODE_MML_DECOUPLE ||
		info->mode == MML_MODE_MDP_DECOUPLE) {
		*reason = mml_query_userdc;
		goto decouple_user;
	}

	if (info->mode == MML_MODE_APUDC) {
		*reason = mml_query_apudc;
		goto decouple_user;
	}

	if (!MML_FMT_COMPRESS(info->src.format)) {
		*reason = mml_query_format;
		return MML_MODE_MML_DECOUPLE;
	}

	if ((MML_FMT_AFBC(info->src.format) && MML_FMT_IS_ARGB(info->src.format)) ||
	    info->src.format == MML_FMT_P010_HYFBC) {
		*reason = mml_query_format;
		return MML_MODE_MML_DECOUPLE;
	}

	/* rotate go to racing (inline rotate) */
	if (mml_racing == 1 &&
		(info->dest[0].rotate == MML_ROT_90 || info->dest[0].rotate == MML_ROT_270))
		return tp_query_mode_racing(mml, info, reason);

	return tp_query_mode_dl(mml, info, reason, panel_width, panel_height);

decouple_user:
	return info->mode;

not_support:
	return MML_MODE_NOT_SUPPORT;
}

static struct cmdq_client *get_racing_clt(struct mml_topology_cache *cache, u32 pipe)
{
	/* use NO PQ path as inline rot path for this platform */
	return cache->paths[PATH_MML_NOPQ + pipe].clt;
}

static const struct mml_topology_path *tp_get_dl_path(struct mml_topology_cache *cache,
	struct mml_frame_info *info, u32 pipe, struct mml_frame_size *panel)
{
	u32 scene;
	bool dual = true;

	if (mml_rrot == 0 || mml_rrot == 1)
		goto use_rrot;

	if (!info)
		return &cache->paths[PATH_MML_PQ_DL];

	if (info->dest_cnt == 2 && info->dest[0].pq_config.en_region_pq)
		return &cache->paths[PATH_MML_RR_DL_2IN_2OUT];

	return &cache->paths[PATH_MML_PQ_DL];

use_rrot:
	if (!info)
		return &cache->paths[PATH_MML_RR_DL];

	tp_check_tput(info, cache, panel->width, panel->height, &dual);

	if (info->dest_cnt == 2 && info->dest[0].pq_config.en_region_pq)
		scene = dual ? PATH_MML_RR_DL_2IN_2OUT : PATH_MML_RRS_DL_2IN_2OUT;
	else
		scene = dual ? PATH_MML_RR_DL : PATH_MML_RRS_DL;

	/* check if connect to ovlsys1 */
	scene = scene_to_ovl1(info->ovlsys_id, scene);

	return &cache->paths[scene];
}

static const struct mml_topology_ops tp_ops_mt6989 = {
	.query_mode2 = tp_query_mode,
	.init_cache = tp_init_cache,
	.select = tp_select,
	.get_racing_clt = get_racing_clt,
	.get_dl_path = tp_get_dl_path,
};

static __init int mml_topology_ip_init(void)
{
	/* init hrt mode as max ostd */
	mtk_mml_hrt_mode = MML_HRT_OSTD_MAX;

	return mml_topology_register_ip(TOPOLOGY_PLATFORM, &tp_ops_mt6989);
}
module_init(mml_topology_ip_init);

static __exit void mml_topology_ip_exit(void)
{
	mml_topology_unregister_ip(TOPOLOGY_PLATFORM);
}
module_exit(mml_topology_ip_exit);

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML for MT6989");
MODULE_LICENSE("GPL");
