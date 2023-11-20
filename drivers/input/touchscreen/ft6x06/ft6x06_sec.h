#ifndef __LINUX_FT6X06_SEC_H__
#define __LINUX_FT6X06_SEC_H__

#include "ft6x06_ts.h"
#include "ft6x06_ex_fun.h"

enum {
	BUILT_IN = 0,
	UMS,
};

/* Touch Screen */
#define TSP_CMD_STR_LEN			   	32
#define TSP_CMD_RESULT_STR_LEN		512
#define TSP_CMD_PARAM_NUM			8
#define TSP_CMD_X_NUM				19
#define TSP_CMD_Y_NUM				10
#define TSP_CMD_NODE_NUM			(TSP_CMD_X_NUM * TSP_CMD_Y_NUM)

struct tsp_factory_info {
	struct list_head cmd_list_head;
	char cmd[TSP_CMD_STR_LEN];
	char cmd_param[TSP_CMD_PARAM_NUM];
	char cmd_result[TSP_CMD_RESULT_STR_LEN];
	char cmd_buff[TSP_CMD_RESULT_STR_LEN];
	struct mutex cmd_lock;
	bool cmd_is_running;
	u8 cmd_state;
};

struct tsp_raw_data { 
	s16 dnd_data[TSP_CMD_NODE_NUM];
	s16 hfdnd_data[TSP_CMD_NODE_NUM];
	s32 hfdnd_data_sum[TSP_CMD_NODE_NUM];
	s16 delta_data[TSP_CMD_NODE_NUM];
	s16 vgap_data[TSP_CMD_NODE_NUM];
	s16 hgap_data[TSP_CMD_NODE_NUM];
};

int init_sec_factory(struct ft6x06_ts_data *info);

#endif
