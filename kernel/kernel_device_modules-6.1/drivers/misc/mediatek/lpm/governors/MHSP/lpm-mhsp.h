/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#define MAXSAMPLES 5
#define MAX_IPI2IDLE	(500 * NSEC_PER_MSEC)
#define PRED_NS_SUB	(1 * NSEC_PER_USEC)
#define PRED_NS_ADD	(100 * NSEC_PER_USEC)

#define REMEDY_VALID_THR_NS	TICK_NSEC
#define REMEDY_VALID_POWER	5
#define REMEDY_VALID_MAX	(1u << REMEDY_VALID_POWER)

#define HRWKUP_MAX_SAMPLES	16

/* Prediction error code */
#define PREDICT_ERR_SLP_HIST_SAMPLE	(1u << 0)
#define PREDICT_ERR_SLP_HIST_SPECIFIC	(1u << 1)
#define PREDICT_ERR_SLP_HIST_NONE	(1u << 3)

#define PREDICT_ERR_IPI_HIST_SAMPLE	(1u << 4)
#define PREDICT_ERR_IPI_HIST_FAR	(1u << 5)
#define PREDICT_ERR_IPI_HIST_NONE	(1u << 7)

#define PREDICT_ERR_REMEDY_TIMES2	(1u << 8)
#define PREDICT_ERR_REMEDY_TIMES3	(1u << 9)
#define PREDICT_ERR_REMEDY_OFF_TIMES	(1u << 10)
#define PREDICT_ERR_REMEDY_THRES	(1u << 14)
#define PREDICT_ERR_REMEDY_NONE		(1u << 15)

#define PREDICT_ERR_HR_SEL_SHALLOW	(1u << 16)
#define PREDICT_ERR_HR_SEL_NEXT_EVENT	(1u << 17)
#define PREDICT_ERR_HR_EN		(1u << 19)

#define PREDICT_ERR_ONLY_ONE_STATE	(1u << 20)
#define PREDICT_ERR_LATENCY_REQ		(1u << 21)
#define PREDICT_ERR_NEXT_EVENT_SHORT	(1u << 22)
#define PREDICT_ERR_NO_STATE_ENABLE	(1u << 23)

enum cpuidle_wake_reason {
	CPUIDLE_WAKE_EVENT = 0,
	CPUIDLE_WAKE_IPI,
};

enum cpuidle_remedy {
	REMEDY_TIMES2 = 0,
	REMEDY_TIMES3,
	REMEDY_MAX
};

enum cpuidle_hstate {
	HSTATE_WFI = 0,
	HSTATE_NEXT,
	HSTATE_MAX
};

enum cpuidle_hr_sel_state {
	HR_SEL_SHALLOW = 0,
	HR_SEL_NEXT_EVENT,
	HR_SEL_MAX
};

struct gov_criteria {
	uint32_t stddev;
	uint32_t remedy_times2_ratio;
	uint32_t remedy_times3_ratio;
	uint32_t remedy_off_ratio;
};

struct shallow_handle {
	bool timer_need_cancel;
	int nsamp;
	s64 state_ns;
	uint32_t cur;
	uint32_t hstate_cnt[HR_SEL_MAX];
	uint32_t actual[HRWKUP_MAX_SAMPLES];
	struct hrtimer histtimer;
};

struct remedy {
	int nsamp;
	uint32_t cur;
	uint32_t cnt;
	uint32_t data[REMEDY_VALID_MAX];
	uint32_t hstate[REMEDY_VALID_MAX];
	uint32_t hstate_resi;
	uint32_t total_cnt;
	uint32_t match_cnt;
};

struct history_resi {
	uint64_t stddev;
	uint32_t resi[MAXSAMPLES];
	int nsamp;
	uint32_t cur;
	int wake_reason;

	uint32_t specific_cnt;
};

#if IS_ENABLED(CONFIG_MTK_CPU_IDLE_IPI_SUPPORT)
struct history_ipi {
	uint64_t stddev;
	uint32_t interval[MAXSAMPLES];
	int nsamp;
	uint32_t cur;

	uint32_t lock;
	ktime_t last_ts_ipi;
};
#endif

struct gov_info {
	bool htmr_wkup;
	atomic_t ipi_pending;
	atomic_t is_cpuidle;
	atomic_t ipi_wkup;

	int cpu;
	int enable;
	int last_cstate;
	int64_t latency_req;
	uint64_t predicted;
	uint64_t next_timer_ns;
	uint32_t deepest_state;
	uint32_t hist_invalid;
	uint32_t predict_info;

	struct notifier_block nb;
	struct shallow_handle shallow;
	struct history_resi slp_hist;
	struct remedy remedy;

#if IS_ENABLED(CONFIG_MTK_CPU_IDLE_IPI_SUPPORT)
	struct history_ipi ipi_hist;
#endif
};
