#ifndef _HARSH_H
#define _HARSH_H

#define MAX_CORE 16

#include <linux/sched.h>

//#include <linux/uh.h>
/* uH_HARSH Command ID */
enum __HARSH_CMD_ID{
	HARSH_EVENT_INIT = 0x0,
	HARSH_EVENT_START,
	HARSH_EVENT_UNDEF_INST,
	HARSH_EVENT_UNDEF_INST_STATIC = 0x3
};

typedef struct core_info{
	u32 variant;
	u32 part;
	u32 revision;
} core_info_t;

typedef struct harsh_init {
	core_info_t core_info[MAX_CORE];
	u32 n_online_core;
} harsh_init_t;

typedef struct harsh_request {
	u32    instruction;
	u32    core;

/* task information */
	char   comm[TASK_COMM_LEN];
	pid_t  pid;

/* saved user registers - for emulation */
	struct pt_regs *regs;
} harsh_req_t;

enum {
	MIGRATION = 0x0,
	EMULATION,
	INSTRUCTION_ABORT = 0x2
};


#endif //_HARSH_H
