/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * Version history
 * 2017.06.27 : Initial draft
 * 2017.08.25 : Released
 * 2017.10.12 : Add BOOTUP_RSP
 */

#ifndef __VERTEX_MESSAGE_H__
#define __VERTEX_MESSAGE_H__

/* temp for previous host ver */
//#define AP_FOCAL_ZONE_SETTING		(0)
#define AP_FOCAL_ZONE_SETTING		(1)

//#define MAX_NUM_OF_INPUTS		(16)
#define MAX_NUM_OF_INPUTS		(24)
//#define MAX_NUM_OF_OUTPUTS		(12)
#define MAX_NUM_OF_OUTPUTS		(8)
//#define MAX_NUM_OF_USER_PARAMS	(8)
#define MAX_NUM_OF_USER_PARAMS		(180)

#define VERTEX_CM7_HEAP_SIZE_PREVIEW      (SZ_16M)
#define VERTEX_CM7_HEAP_SIZE_ENF          (SZ_64M)
#define VERTEX_CM7_HEAP_SIZE_CAPTURE      (SZ_128M)

enum {
	BOOTUP_RSP = 1,
	INIT_REQ,
	INIT_RSP,
	CREATE_GRAPH_REQ,
	CREATE_GRAPH_RSP,
	SET_GRAPH_REQ,
	SET_GRAPH_RSP,
	INVOKE_GRAPH_REQ,
	INVOKE_GRAPH_ACK,
	INVOKE_GRAPH_RSP,
	DESTROY_GRAPH_REQ,
	DESTROY_GRAPH_RSP,
	ABORT_GRAPH_REQ,
	ABORT_GRAPH_RSP,
	POWER_DOWN_REQ,
	POWER_DOWN_RSP,
	TEST_REQ,
	TEST_RSP,
	MAX_MSG_TYPE,
};

enum {
	SUCCESS = 0,
	ERROR_INVALID_GRAPH_ID = -1,
	ERROR_EXEC_PARAM_CORRUPTION = -2,
	ERROR_GRAPH_DATA_CORRUPTION = -3,
	ERROR_COMPILED_GRAPH_ENABLED = -4,
	ERROR_INVALID_MSG_TYPE = -5,
	ERROR_UNKNOWN = -6,
	ERROR_INVALID = -7,
	ERROR_MBX_BUSY = -8,
};

enum {
	SCENARIO_DE = 1,
	SCENARIO_SDOF_FULL,
	SCENARIO_DE_SDOF,
	// temp for previous host ver
	// SCENARIO_ENF,
	SCENARIO_DE_CAPTURE,
	SCENARIO_SDOF_CAPTURE,
	SCENARIO_DE_SDOF_CAPTURE,
	SCENARIO_GDC,
	SCENARIO_ENF,
	SCENARIO_ENF_UV,
	SCENARIO_ENF_YUV,	/*10*/
	SCENARIO_BLEND,
	SCENARIO_MAX,
};

struct vertex_bootup_rsp {
	int error;
};

/**
 * struct vertex_init_req
 * @p_cc_heap: pointer to CC heap region
 * @sz_cc_heap: size of CC heap region
 */
struct vertex_init_req {
	unsigned int p_cc_heap;
	unsigned int sz_cc_heap;
};

struct vertex_init_rsp {
	int error;
};

/**
 * struct vertex_create_graph_req
 * @p_graph: pointer to compiled graph
 * @sz_graph: size of compiled graph
 */
struct vertex_create_graph_req {
	unsigned int p_graph;
	unsigned int sz_graph;
};

/**
 * struct vertex_create_graph_rsp
 * @graph_id: graph id that should be used for invocation
 */
struct vertex_create_graph_rsp {
	int error;
	unsigned int graph_id;
};

/**
 * struct vertex_set_graph_req
 * @graph_id: graph id that should be used for invocation
 * @p_lll_bin: pointer to VIP binary in DRAM
 * @sz_lll_bin: size of VIP binary in DRAM
 * @num_inputs: number of inputs for the graph
 * @num_outputs: number of outputs for the graph
 * @input_width: array of input width sizes
 * @input_height: array of input height sizes
 * @input_depth: array of input depth sizes
 * @output_width: array of output width sizes
 * @output_height: array of output height sizes
 * @output_depth: array of output depth sizes
 * @p_temp: pointer to DRAM area for temporary buffers
 * @sz_temp: size of temporary buffer area
 */
struct vertex_set_graph_req {
	unsigned int graph_id;
	unsigned int p_lll_bin;
	unsigned int sz_lll_bin;
	int num_inputs;
	int num_outputs;

	unsigned int input_width[MAX_NUM_OF_INPUTS];
	unsigned int input_height[MAX_NUM_OF_INPUTS];
	unsigned int input_depth[MAX_NUM_OF_INPUTS];
	unsigned int output_width[MAX_NUM_OF_OUTPUTS];
	unsigned int output_height[MAX_NUM_OF_OUTPUTS];
	unsigned int output_depth[MAX_NUM_OF_OUTPUTS];

	unsigned int p_temp;
	unsigned int sz_temp;
};

struct vertex_set_graph_rsp {
	int error;
};

/**
 * struct vertex_invoke_graph_req
 * @graph_id: graph id that should be used for invocation
 * @num_inputs: number of inputs for the graph
 * @num_outputs: number of outputs for the graph
 * @p_input: array of input buffers
 * @p_output: array of output buffers
 * @user_params: array of user parameters
 */
struct vertex_invoke_graph_req {
	unsigned int graph_id;
	int num_inputs;
	int num_outputs;
	unsigned int p_input[MAX_NUM_OF_INPUTS];
	unsigned int p_output[MAX_NUM_OF_OUTPUTS];
#if AP_FOCAL_ZONE_SETTING
	unsigned int user_params[MAX_NUM_OF_USER_PARAMS];
#endif
};

struct vertex_invoke_graph_ack {
	int error;
	unsigned int job_id;
};

struct vertex_invoke_graph_rsp {
	int error;
	unsigned int job_id;
};

struct vertex_abort_graph_req {
	unsigned int job_id;
};

struct vertex_abort_graph_rsp {
	int error;
};

struct vertex_destroy_graph_req {
	unsigned int graph_id;
};

struct vertex_destroy_graph_rsp {
	int error;
};

struct vertex_powerdown_req {
	unsigned int valid;
};

struct vertex_powerdown_rsp {
	int error;
};

struct vertex_test_req {
	unsigned int test;
};

struct vertex_test_rsp {
	int error;
};

union vertex_message_payload {
	struct vertex_bootup_rsp		bootup_rsp;
	struct vertex_init_req			init_req;
	struct vertex_init_rsp			init_rsp;
	struct vertex_create_graph_req		create_graph_req;
	struct vertex_create_graph_rsp		create_graph_rsp;
	struct vertex_set_graph_req		set_graph_req;
	struct vertex_set_graph_rsp		set_graph_rsp;
	struct vertex_invoke_graph_req		invoke_graph_req;
	struct vertex_invoke_graph_ack		invoke_graph_ack;
	struct vertex_invoke_graph_rsp		invoke_graph_rsp;
	struct vertex_destroy_graph_req		destroy_graph_req;
	struct vertex_destroy_graph_rsp		destroy_graph_rsp;
	struct vertex_abort_graph_req		abort_graph_req;
	struct vertex_abort_graph_rsp		abort_graph_rsp;
	struct vertex_powerdown_req		powerdown_req;
	struct vertex_powerdown_rsp		powerdown_rsp;
	struct vertex_test_req			test_req;
	struct vertex_test_rsp			test_rsp;
};

struct vertex_message {
	/* TODO valid flag to be removed */
	unsigned int			valid;
	unsigned int			trans_id;
	unsigned int			type;
	union vertex_message_payload	payload;
};

#endif
