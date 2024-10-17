
/*
 * flexpmu_dbg.h - flexpmu_dbg header for framework and plugins
 *
 * author : Yongjin Lee (yongjin0.lee@samsung.com)
 *	    Jeonghoon Jang (jnghn.jang@samsung.com)
 */

#ifndef __FLEXPMU_DBG_H__
#define __FLEXPMU_DBG_H__

/* dbg.h */
enum power_status{
	PWR_STATUS__OFF,
	PWR_STATUS__ON,
	NR_PWR_STATUS,
};

enum profiler_status{
	PROFILE_END,
	PROFILE_START,
	PROFILE_CLEAR,
};

enum profiler_ipc_cmd{
	LATENCY_PROFILER_DISABLE,
	LATENCY_PROFILER_ENABLE,
	REQUESTER_PROFILER_DISABLE,
	REQUESTER_PROFILER_ENABLE,
};

struct latency_info {
	u32 sum[NR_PWR_STATUS];	/* avg = sum/total_cnt */
	u32 cnt;
};

struct requester_info {
	u32 req_time;
	u32 rel_time;
	u32 total_time;
	u32 total_cnt;
};

struct flexpmu_dbg {
	u32 cpu_pm;
	u32 mif;

	/* pmucal list */
	u32 cpu_list;
	u32 cluster_list;
	u32 pd_list;
	u32 apsoc_list;
	u32 mif_list;

	u32 cpu_list_size;
	u32 cluster_list_size;
	u32 pd_list_size;
	u32 system_list_size;

	u8 enable_latency_profiler;
	u8 enable_requester_profiler;
	u32 eclkbuf;
	u32 latency_cpu;
	u32 latency_cluster;
	u32 latency_pd;
	u32 latency_soc;
	u32 latency_mif;
	u32 requester_mif;
};

/* cpu_pm.h */
#define CPU_INFORM_POWERMODE_NUM			(7)
struct flexpmu_cpu_pm {
	u32 cpu_status;						//bitmap
	u32 cpuhp_flag;						//bitmap
	u32 cluster_status;					//bitmap
	u8 apsoc_status;
	u8 curr_powermode;
	u32 p_apsoc_down_cnt;
	u32 p_apsoc_ewkup_cnt;
	u8 stop_idx;
	u8 mif_always_on;
	/* C2 */
	s32 (*exit_c2) (u32 cpu);
	s32 (*enter_c2) (u32 cpu);
	/* Common */
	s32 (*last_core_det) (u32 cpu, u32 scope);
	/* CPD */
	s32 (*exit_cpd) (u32 cluster);
	s32 (*enter_cpd) (u32 cluster);
	/* System Powermode */
	s32 (*apsoc_up) (void);					//up
	s32 (*noncpus_up) (void);				//up
	s32 (*set_tzpc) (void);					//up
	s32 (*pwmd_noti_up) (void);				//up
	s32 (*cpus_up) (void);					//up
	s32 (*pwmd_noti_down) (u32 powermode);			//down
	s32 (*noncpus_down) (u32 powermode);			//down
	s32 (*apsoc_down) (u32 powermode);			//down
	bool (*pwmd_wakeup_pending) (void);			//down
};

/* mif.h */
struct mif_req_info {
	u8 up;
	u8 down;
	u8 ack;
	struct requester_info dbg_info;
};

struct flexpmu_mif {
	u8 nr_users;
	u8 status;
	u32 requests;
	bool always_on;
	u32 p_down_cnt;
	u32 req_info;
	s32 (*enter_pre) (u32 idx, u32 powermode);
	s32 (*enter) (u32 idx, u32 powermode);
	s32 (*enter_post) (u32 idx, u32 powermode);
	s32 (*exit_pre) (u32 idx, u32 powermode);
	s32 (*exit) (u32 idx, u32 powermode);
	s32 (*exit_post) (u32 idx, u32 powermode);
};

struct ext_clk_log {
	u32 is_on;
	u32 timestamp;
};

struct ext_clk_log_buf {
	u32 ptr;
	u32 len;
	u32 p_logs;
};

struct flexpmu_ext_clk_buf {
	u8 nr_reqs;
	u32 p_log_bufs;
	u8 *eint_num_req;
	s32 (**init_req) (u32 eint_num);
	s32 (**control_req) (bool is_on);
};

/* flexpmu_cal/system.h */
struct pmucal_system_sequencer {
	u32 id;
	u32 up;
	u32 down;
	u32 num_up;
	u32 num_down;
	struct latency_info latency;
};

/* flexpmu_cal/cpu.h */
struct pmucal_cpu {
	u32 id;
	u32 on;
	u32 off;
	u32 status;
	u32 num_on;
	u32 num_off;
	u32 num_status;
	struct latency_info latency;
};

/* flexpmu_cal/local.h */
struct pmucal_pd {
	u32 id;
	u32 on;
	u32 off;
	u32 save;
	u32 status;
	u32 num_on;
	u32 num_off;
	u32 num_save;
	u32 num_status;
	u32 need_sac;
	struct latency_info latency;
};
#endif /* __FLEXPMU_DBG_H__ */
