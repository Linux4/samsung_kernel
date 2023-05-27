#include <linux/kernel.h>
#include <linux/slab.h>
#include "arm-smmu.h"

#define ARM_SMMU_DEBUG_STACK_DEPTH (0x10)
#define NR_GDSC_WAIT_LOG 0x10
#define NR_SUSPEND_LOG 0x10

struct arm_smmu_debug_log {
	ktime_t start;
	ktime_t end;
	ktime_t delta;
	unsigned long entries[ARM_SMMU_DEBUG_STACK_DEPTH];	
};

struct arm_smmu_debug_state {
	bool gpu_waiting;
	ktime_t gpu_waiting_start_ts;
	struct arm_smmu_debug_log gdsc_wait_log[NR_GDSC_WAIT_LOG];

	spinlock_t last_busy_lock;
	unsigned long last_busy_stack[ARM_SMMU_DEBUG_STACK_DEPTH];

	bool suspended;
	int suspend_idx;
	struct arm_smmu_debug_log suspend_log[NR_SUSPEND_LOG];
};

static struct arm_smmu_device *kgsl_smmu;
static struct arm_smmu_debug_state *arm_smmu_debug_state;

/*
 * GPU calls it from gen7_gmu_enable_gdsc
 */
void arm_smmu_debug_gdsc_cx_start_wait(void)
{
	struct arm_smmu_power_resources *pwr = kgsl_smmu->pwr;
	struct arm_smmu_debug_state *state = arm_smmu_debug_state;

	mutex_lock(&pwr->power_lock);
	if (pwr->power_count) {
		state->gpu_waiting = true;
		state->gpu_waiting_start_ts = ktime_get();
	}
	mutex_unlock(&pwr->power_lock);
}
EXPORT_SYMBOL_GPL(arm_smmu_debug_gdsc_cx_start_wait);

/* Called with pwr->mutex held and pwr->power_count == 0 */
void arm_smmu_debug_power_off(struct arm_smmu_device *smmu)
{
	ktime_t ts, delta;
	int i;
	struct arm_smmu_debug_log *victim = NULL;
	struct arm_smmu_debug_state *state = arm_smmu_debug_state;
	unsigned long flags;

	if (smmu != kgsl_smmu || !state->gpu_waiting)
		return;

	ts = ktime_get();
	delta = ts - state->gpu_waiting_start_ts;

	for (i = 0; i < NR_GDSC_WAIT_LOG; i++) {
		struct arm_smmu_debug_log *log = &state->gdsc_wait_log[i];

		if (delta > log->delta) {
			if (!victim || log->delta < victim->delta) {
				victim = log;
			}
		}
	}

	if (victim) {
		victim->start = state->gpu_waiting_start_ts;
		victim->end = ts;
		victim->delta = delta;

		spin_lock_irqsave(&state->last_busy_lock, flags);
		memcpy(victim->entries, state->last_busy_stack,
			sizeof(victim->entries));
		spin_unlock_irqrestore(&state->last_busy_lock, flags);
	}

	state->gpu_waiting = false;
}

/*
 * save stack trace if we are (probably) the last refcount
 */
void arm_smmu_debug_last_busy(struct arm_smmu_device *smmu)
{
	struct arm_smmu_debug_state *state = arm_smmu_debug_state;
	struct device *dev;
	int count;
	unsigned long flags;

	if (smmu != kgsl_smmu)
		return;

	dev = smmu->dev;
	count = atomic_read(&dev->power.usage_count);
	if (count)
		return;

	spin_lock_irqsave(&state->last_busy_lock, flags);
	stack_trace_save(state->last_busy_stack,
			 ARRAY_SIZE(state->last_busy_stack), 1);
	spin_unlock_irqrestore(&state->last_busy_lock, flags);
}

void arm_smmu_debug_suspend(struct arm_smmu_device *smmu)
{
	struct arm_smmu_debug_state *state = arm_smmu_debug_state;

	if (smmu != kgsl_smmu)
		return;

	state->suspended = true;	
}

void arm_smmu_debug_resume(struct arm_smmu_device *smmu)
{
	struct arm_smmu_debug_state *state = arm_smmu_debug_state;

	if (smmu != kgsl_smmu)
		return;

	state->suspended = false;	
}

void arm_smmu_debug_power_on(struct arm_smmu_device *smmu)
{
	struct arm_smmu_debug_log *log;
	struct arm_smmu_debug_state *state = arm_smmu_debug_state;

	if (smmu != kgsl_smmu || !state->suspended)
		return;

	if (state->suspend_idx >= NR_SUSPEND_LOG)
		return;

	log = &state->suspend_log[state->suspend_idx++];
	log->start = ktime_get();
	
	stack_trace_save(log->entries, ARRAY_SIZE(log->entries), 1);
}

/* Call from arm_smmu_probe, after it can no longer fail */
void arm_smmu_debug_setup(struct arm_smmu_device *smmu)
{
	struct arm_smmu_debug_state *state;

	if (!strstr(dev_name(smmu->dev), "kgsl-smmu"))
		return;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return;

	spin_lock_init(&state->last_busy_lock);
	arm_smmu_debug_state = state;
	kgsl_smmu = smmu;
}	
