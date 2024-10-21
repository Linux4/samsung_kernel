// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/perf_event.h>
#include <linux/workqueue.h>

#include <swpm_perf_arm_pmu.h>

#define PMU_AI_EN 1

static unsigned int swpm_arm_pmu_status;
static unsigned int swpm_arm_dsu_pmu_status;
static unsigned int boundary;
static unsigned int pmu_dsu_support;
static unsigned int pmu_dsu_type;
static unsigned int pmu_ai_support;

static DEFINE_PER_CPU(struct perf_event *, l3dc_events);
static DEFINE_PER_CPU(struct perf_event *, cycle_events);

DEFINE_PER_CPU(struct perf_event *, inst_spec_events);
EXPORT_SYMBOL(inst_spec_events);

static DEFINE_PER_CPU(int, l3dc_idx);
static DEFINE_PER_CPU(int, inst_spec_idx);
static DEFINE_PER_CPU(int, cycle_idx);

#if PMU_AI_EN
static DEFINE_PER_CPU(struct perf_event *, pmu_event_3);
static DEFINE_PER_CPU(struct perf_event *, pmu_event_4);
static DEFINE_PER_CPU(struct perf_event *, pmu_event_5);
static DEFINE_PER_CPU(struct perf_event *, pmu_event_6);
static DEFINE_PER_CPU(struct perf_event *, pmu_event_7);
static DEFINE_PER_CPU(struct perf_event *, pmu_event_8);
static DEFINE_PER_CPU(struct perf_event *, pmu_event_9);
static DEFINE_PER_CPU(struct perf_event *, pmu_event_10);
static DEFINE_PER_CPU(struct perf_event *, pmu_event_11);
static DEFINE_PER_CPU(struct perf_event *, pmu_event_12);
static DEFINE_PER_CPU(struct perf_event *, pmu_event_13);

static DEFINE_PER_CPU(int, pmu_idx_3);
static DEFINE_PER_CPU(int, pmu_idx_4);
static DEFINE_PER_CPU(int, pmu_idx_5);
static DEFINE_PER_CPU(int, pmu_idx_6);
static DEFINE_PER_CPU(int, pmu_idx_7);
static DEFINE_PER_CPU(int, pmu_idx_8);
static DEFINE_PER_CPU(int, pmu_idx_9);
static DEFINE_PER_CPU(int, pmu_idx_10);
static DEFINE_PER_CPU(int, pmu_idx_11);
static DEFINE_PER_CPU(int, pmu_idx_12);
static DEFINE_PER_CPU(int, pmu_idx_13);


#endif

struct perf_event *dsu_cycle_events;
static int dsu_cycle_idx;

static struct perf_event_attr l3dc_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = ARMV8_PMUV3_PERFCTR_L3D_CACHE, /* 0x2B */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr inst_spec_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x1B, */
	.config         = ARMV8_PMUV3_PERFCTR_INST_SPEC, /* 0x1B */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr cycle_event_attr = {
	.type           = PERF_TYPE_HARDWARE,
	.config         = PERF_COUNT_HW_CPU_CYCLES,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr dsu_cycle_event_attr = {
/*	.type           = 11,  from /sys/devices/arm_dsu_0/type */
	.config         = ARMV8_PMUV3_PERFCTR_CPU_CYCLES, /* 0x11 */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};

#if PMU_AI_EN
static struct perf_event_attr pmu_event_3_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x04,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr pmu_event_4_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x10,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr pmu_event_5_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x14,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr pmu_event_6_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x16,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr pmu_event_7_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x17,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr pmu_event_8_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x18,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr pmu_event_9_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x21,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr pmu_event_10_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x3B,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr pmu_event_11_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x3D,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr pmu_event_12_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x80C1,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr pmu_event_13_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config         = 0x80EF,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
#endif

static void swpm_dsu_pmu_start(void)
{
	struct perf_event *c_event = dsu_cycle_events;

	if (c_event) {
		perf_event_enable(c_event);
		dsu_cycle_idx = c_event->hw.idx;
	}
}

static void swpm_dsu_pmu_stop(void)
{
	struct perf_event *c_event = dsu_cycle_events;

	if (c_event) {
		perf_event_disable(c_event);
		dsu_cycle_idx = -1;
	}
}

static void swpm_pmu_start(int cpu)
{
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);


	if (l3_event) {
		perf_event_enable(l3_event);
		per_cpu(l3dc_idx, cpu) = l3_event->hw.idx;
	}
	if (i_event) {
		perf_event_enable(i_event);
		per_cpu(inst_spec_idx, cpu) = i_event->hw.idx;
	}
	if (c_event) {
		perf_event_enable(c_event);
		per_cpu(cycle_idx, cpu) = c_event->hw.idx;
	}

	if (pmu_ai_support) {
		struct perf_event *p3_event = per_cpu(pmu_event_3, cpu);
		struct perf_event *p4_event = per_cpu(pmu_event_4, cpu);
		struct perf_event *p5_event = per_cpu(pmu_event_5, cpu);
		struct perf_event *p6_event = per_cpu(pmu_event_6, cpu);
		struct perf_event *p7_event = per_cpu(pmu_event_7, cpu);
		struct perf_event *p8_event = per_cpu(pmu_event_8, cpu);
		struct perf_event *p9_event = per_cpu(pmu_event_9, cpu);
		struct perf_event *p10_event = per_cpu(pmu_event_10, cpu);
		struct perf_event *p11_event = per_cpu(pmu_event_11, cpu);
		struct perf_event *p12_event = per_cpu(pmu_event_12, cpu);
		struct perf_event *p13_event = per_cpu(pmu_event_13, cpu);

		if (p3_event) {
			perf_event_enable(p3_event);
			per_cpu(pmu_idx_3, cpu) = p3_event->hw.idx;
		}
		if (p4_event) {
			perf_event_enable(p4_event);
			per_cpu(pmu_idx_4, cpu) = p4_event->hw.idx;
		}
		if (p5_event) {
			perf_event_enable(p5_event);
			per_cpu(pmu_idx_5, cpu) = p5_event->hw.idx;
		}
		if (p6_event) {
			perf_event_enable(p6_event);
			per_cpu(pmu_idx_6, cpu) = p6_event->hw.idx;
		}
		if (p7_event) {
			perf_event_enable(p7_event);
			per_cpu(pmu_idx_7, cpu) = p7_event->hw.idx;
		}
		if (p8_event) {
			perf_event_enable(p8_event);
			per_cpu(pmu_idx_8, cpu) = p8_event->hw.idx;
		}
		if (p9_event) {
			perf_event_enable(p9_event);
			per_cpu(pmu_idx_9, cpu) = p9_event->hw.idx;
		}
		if (p10_event) {
			perf_event_enable(p10_event);
			per_cpu(pmu_idx_10, cpu) = p10_event->hw.idx;
		}
		if (p11_event) {
			perf_event_enable(p11_event);
			per_cpu(pmu_idx_11, cpu) = p11_event->hw.idx;
		}
		if (p12_event) {
			perf_event_enable(p12_event);
			per_cpu(pmu_idx_12, cpu) = p12_event->hw.idx;
		}
		if (p13_event) {
			perf_event_enable(p13_event);
			per_cpu(pmu_idx_13, cpu) = p13_event->hw.idx;
		}
	}
}

static void swpm_pmu_stop(int cpu)
{
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);


	if (l3_event) {
		perf_event_disable(l3_event);
		per_cpu(l3dc_idx, cpu) = -1;
	}
	if (i_event) {
		perf_event_disable(i_event);
		per_cpu(inst_spec_idx, cpu) = -1;
	}
	if (c_event) {
		perf_event_disable(c_event);
		per_cpu(cycle_idx, cpu) = -1;
	}
	if (pmu_ai_support) {
		struct perf_event *p3_event = per_cpu(pmu_event_3, cpu);
		struct perf_event *p4_event = per_cpu(pmu_event_4, cpu);
		struct perf_event *p5_event = per_cpu(pmu_event_5, cpu);
		struct perf_event *p6_event = per_cpu(pmu_event_6, cpu);
		struct perf_event *p7_event = per_cpu(pmu_event_7, cpu);
		struct perf_event *p8_event = per_cpu(pmu_event_8, cpu);
		struct perf_event *p9_event = per_cpu(pmu_event_9, cpu);
		struct perf_event *p10_event = per_cpu(pmu_event_10, cpu);
		struct perf_event *p11_event = per_cpu(pmu_event_11, cpu);
		struct perf_event *p12_event = per_cpu(pmu_event_12, cpu);
		struct perf_event *p13_event = per_cpu(pmu_event_13, cpu);

		if (p3_event) {
			perf_event_disable(p3_event);
			per_cpu(pmu_idx_3, cpu) = -1;
		}
		if (p4_event) {
			perf_event_disable(p4_event);
			per_cpu(pmu_idx_4, cpu) = -1;
		}
		if (p5_event) {
			perf_event_disable(p5_event);
			per_cpu(pmu_idx_5, cpu) = -1;
		}
		if (p6_event) {
			perf_event_disable(p6_event);
			per_cpu(pmu_idx_6, cpu) = -1;
		}
		if (p7_event) {
			perf_event_disable(p7_event);
			per_cpu(pmu_idx_7, cpu) = -1;
		}
		if (p8_event) {
			perf_event_disable(p8_event);
			per_cpu(pmu_idx_8, cpu) = -1;
		}
		if (p9_event) {
			perf_event_disable(p9_event);
			per_cpu(pmu_idx_9, cpu) = -1;
		}
		if (p10_event) {
			perf_event_disable(p10_event);
			per_cpu(pmu_idx_10, cpu) = -1;
		}
		if (p11_event) {
			perf_event_disable(p11_event);
			per_cpu(pmu_idx_11, cpu) = -1;
		}
		if (p12_event) {
			perf_event_disable(p12_event);
			per_cpu(pmu_idx_12, cpu) = -1;
		}
		if (p13_event) {
			perf_event_disable(p13_event);
			per_cpu(pmu_idx_13, cpu) = -1;
		}

	}

}

static void dummy_handler(struct perf_event *event, struct perf_sample_data *data,
			  struct pt_regs *regs)
{
	/*
	 * Required as perf_event_create_kernel_counter() requires an overflow handler,
	 * even though all we do is poll.
	 */
}

static int swpm_dsu_pmu_enable(int enable)
{
	struct perf_event *event;
	struct perf_event *c_event = dsu_cycle_events;

	if (enable) {
		if (!c_event) {
			event = perf_event_create_kernel_counter(
				&dsu_cycle_event_attr, 0, NULL, dummy_handler, NULL);
			if (IS_ERR(event)) {
				pr_notice("create dsu_cycle error (%d)\n",
					  (int)PTR_ERR(event));
				goto FAIL;
			}
			dsu_cycle_events = event;
		}
		swpm_dsu_pmu_start();
	} else {
		swpm_dsu_pmu_stop();

		if (c_event) {
			perf_event_release_kernel(c_event);
			dsu_cycle_events = NULL;
		}
	}

	return 0;
FAIL:
	return (int)PTR_ERR(event);
}

static int swpm_arm_pmu_enable(int cpu, int enable)
{
	struct perf_event *event;
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);


	if (enable) {
		if (!l3_event) {
			event = perf_event_create_kernel_counter(
				&l3dc_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) l3dc counter error (%d)\n",
					  cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(l3dc_events, cpu) = event;
		}
		if (!i_event && cpu >= boundary) {
			event = perf_event_create_kernel_counter(
				&inst_spec_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) inst_spec counter error (%d)\n",
					  cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(inst_spec_events, cpu) = event;
		}
		if (!c_event && cpu >= boundary) {
			event = perf_event_create_kernel_counter(
				&cycle_event_attr, cpu,	NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) cycle counter error (%d)\n",
					  cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(cycle_events, cpu) = event;
		}
		if (pmu_ai_support) {
			struct perf_event *p3_event = per_cpu(pmu_event_3, cpu);
			struct perf_event *p4_event = per_cpu(pmu_event_4, cpu);
			struct perf_event *p5_event = per_cpu(pmu_event_5, cpu);
			struct perf_event *p6_event = per_cpu(pmu_event_6, cpu);
			struct perf_event *p7_event = per_cpu(pmu_event_7, cpu);
			struct perf_event *p8_event = per_cpu(pmu_event_8, cpu);
			struct perf_event *p9_event = per_cpu(pmu_event_9, cpu);
			struct perf_event *p10_event = per_cpu(pmu_event_10, cpu);
			struct perf_event *p11_event = per_cpu(pmu_event_11, cpu);
			struct perf_event *p12_event = per_cpu(pmu_event_12, cpu);
			struct perf_event *p13_event = per_cpu(pmu_event_13, cpu);

			if (!p3_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_3_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p3_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_3, cpu) = event;
			}
			if (!p4_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_4_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p4_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_4, cpu) = event;
			}
			if (!p5_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_5_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p5_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_5, cpu) = event;
			}
			if (!p6_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_6_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p6_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_6, cpu) = event;
			}
			if (!p7_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_7_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p7_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_7, cpu) = event;
			}
			if (!p8_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_8_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p8_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_8, cpu) = event;
			}
			if (!p9_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_9_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p9_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_9, cpu) = event;
			}
			if (!p10_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_10_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p10_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_10, cpu) = event;
			}
			if (!p11_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_11_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p11_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_11, cpu) = event;
			}
			if (!p12_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_12_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p12_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_12, cpu) = event;
			}
			if (!p13_event && cpu >= boundary) {
				event = perf_event_create_kernel_counter(
					&pmu_event_13_attr, cpu, NULL, NULL, NULL);
				if (IS_ERR(event)) {
					pr_notice("create (%d) p13_event error (%d)\n",
						  cpu, (int)PTR_ERR(event));
					goto FAIL;
				}
				per_cpu(pmu_event_13, cpu) = event;
			}
		}

		swpm_pmu_start(cpu);
	} else {
		swpm_pmu_stop(cpu);

		if (l3_event) {
			perf_event_release_kernel(l3_event);
			per_cpu(l3dc_events, cpu) = NULL;
		}
		if (i_event) {
			perf_event_release_kernel(i_event);
			per_cpu(inst_spec_events, cpu) = NULL;
		}
		if (c_event) {
			perf_event_release_kernel(c_event);
			per_cpu(cycle_events, cpu) = NULL;
		}
		if (pmu_ai_support) {
			struct perf_event *p3_event = per_cpu(pmu_event_3, cpu);
			struct perf_event *p4_event = per_cpu(pmu_event_4, cpu);
			struct perf_event *p5_event = per_cpu(pmu_event_5, cpu);
			struct perf_event *p6_event = per_cpu(pmu_event_6, cpu);
			struct perf_event *p7_event = per_cpu(pmu_event_7, cpu);
			struct perf_event *p8_event = per_cpu(pmu_event_8, cpu);
			struct perf_event *p9_event = per_cpu(pmu_event_9, cpu);
			struct perf_event *p10_event = per_cpu(pmu_event_10, cpu);
			struct perf_event *p11_event = per_cpu(pmu_event_11, cpu);
			struct perf_event *p12_event = per_cpu(pmu_event_12, cpu);
			struct perf_event *p13_event = per_cpu(pmu_event_13, cpu);

			if (p3_event) {
				perf_event_release_kernel(p3_event);
				per_cpu(pmu_event_3, cpu) = NULL;
			}
			if (p4_event) {
				perf_event_release_kernel(p4_event);
				per_cpu(pmu_event_4, cpu) = NULL;
			}
			if (p5_event) {
				perf_event_release_kernel(p5_event);
				per_cpu(pmu_event_5, cpu) = NULL;
			}
			if (p6_event) {
				perf_event_release_kernel(p6_event);
				per_cpu(pmu_event_6, cpu) = NULL;
			}
			if (p7_event) {
				perf_event_release_kernel(p7_event);
				per_cpu(pmu_event_7, cpu) = NULL;
			}
			if (p8_event) {
				perf_event_release_kernel(p8_event);
				per_cpu(pmu_event_8, cpu) = NULL;
			}
			if (p9_event) {
				perf_event_release_kernel(p9_event);
				per_cpu(pmu_event_9, cpu) = NULL;
			}
			if (p10_event) {
				perf_event_release_kernel(p10_event);
				per_cpu(pmu_event_10, cpu) = NULL;
			}
			if (p11_event) {
				perf_event_release_kernel(p11_event);
				per_cpu(pmu_event_11, cpu) = NULL;
			}
			if (p12_event) {
				perf_event_release_kernel(p12_event);
				per_cpu(pmu_event_12, cpu) = NULL;
			}
			if (p13_event) {
				perf_event_release_kernel(p13_event);
				per_cpu(pmu_event_13, cpu) = NULL;
			}
		}
	}

	return 0;
FAIL:
	return (int)PTR_ERR(event);
}

int swpm_arm_pmu_get_idx(unsigned int evt_id, unsigned int cpu)
{
	switch (evt_id) {
	case L3DC_EVT:
		return per_cpu(l3dc_idx, cpu);
	case INST_SPEC_EVT:
		return per_cpu(inst_spec_idx, cpu);
	case CYCLES_EVT:
		return per_cpu(cycle_idx, cpu);
	case DSU_CYCLES_EVT:
		return dsu_cycle_idx;
	}

	if (pmu_ai_support) {
		switch (evt_id) {
		case PMU_3_EVT:
			return per_cpu(pmu_idx_3, cpu);
		case PMU_4_EVT:
			return per_cpu(pmu_idx_4, cpu);
		case PMU_5_EVT:
			return per_cpu(pmu_idx_5, cpu);
		case PMU_6_EVT:
			return per_cpu(pmu_idx_6, cpu);
		case PMU_7_EVT:
			return per_cpu(pmu_idx_7, cpu);
		case PMU_8_EVT:
			return per_cpu(pmu_idx_8, cpu);
		case PMU_9_EVT:
			return per_cpu(pmu_idx_9, cpu);
		case PMU_10_EVT:
			return per_cpu(pmu_idx_10, cpu);
		case PMU_11_EVT:
			return per_cpu(pmu_idx_11, cpu);
		case PMU_12_EVT:
			return per_cpu(pmu_idx_12, cpu);
		case PMU_13_EVT:
			return per_cpu(pmu_idx_13, cpu);
		}
	}

	return -1;
}
EXPORT_SYMBOL(swpm_arm_pmu_get_idx);

unsigned int swpm_arm_dsu_pmu_get_status(void)
{
	return swpm_arm_dsu_pmu_status;
}
EXPORT_SYMBOL(swpm_arm_dsu_pmu_get_status);

unsigned int swpm_arm_pmu_get_status(void)
{
	return (pmu_dsu_support << 24 |
		boundary << 20 |
		swpm_arm_pmu_status);
}
EXPORT_SYMBOL(swpm_arm_pmu_get_status);

unsigned int swpm_arm_dsu_pmu_get_type(void)
{
	return pmu_dsu_type;
}
EXPORT_SYMBOL(swpm_arm_dsu_pmu_get_type);

int swpm_arm_dsu_pmu_set_type(unsigned int type)
{
	int ret = 0;

	if (pmu_dsu_support && !swpm_arm_dsu_pmu_status) {
		pmu_dsu_type = type;
		dsu_cycle_event_attr.type = pmu_dsu_type;
	} else {
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL(swpm_arm_dsu_pmu_set_type);

int swpm_arm_dsu_pmu_enable(unsigned int enable)
{
	int ret = 0;

	if (pmu_dsu_support)
		ret |= swpm_dsu_pmu_enable(!!enable);

	if (!ret)
		swpm_arm_dsu_pmu_status = !!enable && pmu_dsu_support;

	return ret;
}
EXPORT_SYMBOL(swpm_arm_dsu_pmu_enable);

unsigned int swpm_arm_ai_pmu_get(void)
{
	return pmu_ai_support;
}
EXPORT_SYMBOL(swpm_arm_ai_pmu_get);

void swpm_arm_ai_pmu_set(unsigned int ai_enable)
{
	pmu_ai_support = ai_enable;
}
EXPORT_SYMBOL(swpm_arm_ai_pmu_set);

int swpm_arm_pmu_enable_all(unsigned int enable)
{
	int i, ret = 0;

	for (i = 0; i < num_possible_cpus(); i++)
		ret |= swpm_arm_pmu_enable(i, !!enable);

	if (!ret)
		swpm_arm_pmu_status = !!enable;

	return ret;
}
EXPORT_SYMBOL(swpm_arm_pmu_enable_all);

int __init swpm_arm_pmu_init(void)
{
	int ret, i;
	struct device_node *node = NULL;
	char swpm_arm_pmu_desc[] = "mediatek,mtk-swpm";

	node = of_find_compatible_node(NULL, NULL, swpm_arm_pmu_desc);

	if (!node) {
		pr_notice("of_find_compatible_node unable to find swpm device node\n");
		goto END;
	}
	/* device node, device name, offset, variable */
	ret = of_property_read_u32_index(node, "pmu-boundary-num",
					 0, &boundary);
	if (ret) {
		pr_notice("failed to get pmu_boundary_num index from dts\n");
		goto END;
	}
	/* device node, device name, offset, variable */
	ret = of_property_read_u32_index(node, "pmu-dsu-support",
					 0, &pmu_dsu_support);
	if (ret) {
		pr_notice("failed to get pmu_dsu_support index from dts\n");
		goto END;
	}
	/* device node, device name, offset, variable */
	ret = of_property_read_u32_index(node, "pmu-dsu-type",
					 0, &pmu_dsu_type);
	if (ret) {
		pr_notice("failed to get pmu_dsu_type index from dts\n");
		goto END;
	}
	dsu_cycle_event_attr.type = pmu_dsu_type;

	/* device node, device name, offset, variable */
	pmu_ai_support = 0;
	ret = of_property_read_u32_index(node, "pmu-ai-support",
					 0, &pmu_ai_support);
	if (ret)
		pr_notice("failed to get pmu_dsu_type index from dts\n");

END:
	for (i = 0; i < num_possible_cpus(); i++) {
		per_cpu(l3dc_idx, i) = -1;
		per_cpu(inst_spec_idx, i) = -1;
		per_cpu(cycle_idx, i) = -1;
		if (pmu_ai_support) {
			per_cpu(pmu_idx_3, i) = -1;
			per_cpu(pmu_idx_4, i) = -1;
			per_cpu(pmu_idx_5, i) = -1;
			per_cpu(pmu_idx_6, i) = -1;
			per_cpu(pmu_idx_7, i) = -1;
			per_cpu(pmu_idx_8, i) = -1;
			per_cpu(pmu_idx_9, i) = -1;
			per_cpu(pmu_idx_10, i) = -1;
			per_cpu(pmu_idx_11, i) = -1;
			per_cpu(pmu_idx_12, i) = -1;
			per_cpu(pmu_idx_13, i) = -1;
		}
	}
	dsu_cycle_idx = -1;

	swpm_arm_pmu_enable_all(1);
	swpm_arm_dsu_pmu_enable(1);

	return 0;
}
module_init(swpm_arm_pmu_init);

void __exit swpm_arm_pmu_exit(void)
{
}
module_exit(swpm_arm_pmu_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SWPM ARM PMU Module");
MODULE_AUTHOR("MediaTek Inc.");
