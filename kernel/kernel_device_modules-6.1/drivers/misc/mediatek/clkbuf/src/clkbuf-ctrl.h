/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#ifndef __CLK_CTRL_H
#define __CLK_CTRL_H

#include <linux/regmap.h>
#include <linux/platform_device.h>
#include "clkbuf-util.h"

#define CLKBUF_STATUS_INFO_SIZE 2048

struct clkbuf_operation {
	/*xo call back functions*/
	int (*get_pmrcen)(void *data, u32 *out);
	int (*dump_pmic_debug_regs)(void *data, char *buf, int len);
	int (*get_xo_cmd_hdlr)(void *data, int xo_id, char *buf, int len);
	int (*set_xo_cmd_hdlr)(void *data, int cmd, int xo_id, u32 input,
			       int perms);
	int (*set_pmic_common_hdlr)(void *data, int cmd, int arg, int perms);
	int (*get_pmic_common_hdlr)(void *data, char *buf, int len);

	/*src call back functions*/
	ssize_t (*dump_srclken_status)(void *data, char *buf);
	ssize_t (*dump_srclken_trace)(void *data, char *buf, int is_dump_max);
	int (*srclken_subsys_ctrl)(void *data, int cmd, int sub_id,
				      int perms);
	int (*get_rc_MXX_req_sta)(void *data, int sub_id, char *buf);
	int (*get_rc_MXX_cfg)(void *data, int sub_id, char *buf);

	/*pmif call back functions*/
	ssize_t (*dump_pmif_status)(void *data, char *buf);
	int (*set_pmif_inf)(void *data, int cmd, int pmif_id, int onoff);
	void (*spmi_dump_pmif_record)(void);
};

struct clkbuf_hdlr {
	void *data;
	spinlock_t *lock;
	struct clkbuf_operation *ops;
};

enum hw_type {
	PMIC = 1,
	SRCLKEN_CFG = 2,
	SRCLKEN_STA = 3,
	PMIF_M = 4,
	PMIF_P = 5,
};

#define IS_RC_HW(hw_type) ((hw_type == SRCLKEN_CFG) || (hw_type == SRCLKEN_STA))
#define IS_PMIC_HW(hw_type) (hw_type == PMIC)
#define IS_PMIF_HW(hw_type) ((hw_type == PMIF_M) || (hw_type == PMIF_P))

struct xo_dts_cmd {
	int xo_cmd;
	int cmd_val;
};

struct init_dts_cmd {
	int sub_req;
	int num_xo_cmd;
	struct xo_dts_cmd *xo_dts_cmd;
};

struct base_addr {
	void __iomem *cfg;
	void __iomem *sta;
	void __iomem *pmif_m;
	void __iomem *pmif_p;
	struct regmap *map;
};

struct clkbuf_hw {
	enum hw_type hw_type;
	struct base_addr base;
};

struct clkbuf_dts {
	char *comp;
	int num_xo;
	int num_sub;
	int num_pmif;
	char *subsys_name;
	char *xo_name;
	char *pmif_name;
	int xo_id;
	int sub_id;
	int pmif_id;
	int perms;
	int nums; //object manager numbers
	struct clkbuf_hw hw;
	struct clkbuf_hdlr *hdlr;
	struct init_dts_cmd init_dts_cmd;
};

extern int count_pmic_node(struct device_node *clkbuf_node);
extern int count_pmif_node(struct device_node *clkbuf_node);
extern int count_src_node(struct device_node *clkbuf_node);

struct clkbuf_dts *parse_pmic_dts(struct clkbuf_dts *array,
				  struct device_node *clkbuf_node, int nums);
struct clkbuf_dts *parse_pmif_dts(struct clkbuf_dts *array,
				  struct device_node *clkbuf_node, int nums);
struct clkbuf_dts *parse_srclken_dts(struct clkbuf_dts *array,
				     struct device_node *clkbuf_node, int nums);

extern int clkbuf_pmic_init(struct clkbuf_dts *array, struct device *dev);
extern int clkbuf_pmif_init(struct clkbuf_dts *array, struct device *dev);
extern int clkbuf_srclken_init(struct clkbuf_dts *array, struct device *dev);
extern int clkbuf_platform_init(struct clkbuf_dts *array, struct device *dev);
extern int clkbuf_debug_init(struct clkbuf_dts *array, struct device *dev);

int clkbuf_xo_ctrl(char *cmd, int xo_id, u32 input);
int clkbuf_srclken_ctrl(char *cmd, int sub_id);

#endif
