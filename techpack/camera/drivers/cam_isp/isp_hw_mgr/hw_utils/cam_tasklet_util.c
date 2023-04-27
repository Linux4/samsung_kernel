// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/ratelimit.h>
#include "cam_tasklet_util.h"
#include "cam_irq_controller.h"
#include "cam_debug_util.h"


/* Threshold for scheduling delay in ms */
#define CAM_TASKLET_SCHED_TIME_THRESHOLD        5

/* Threshold for execution delay in ms */
#define CAM_TASKLET_EXE_TIME_THRESHOLD          15

#define CAM_TASKLETQ_SIZE                          256

static void cam_tasklet_action(unsigned long data);

static void cam_tasklet_delay_detect(
	uint32_t idx, ktime_t start_timestamp, uint32_t threshold);


/**
 * struct cam_tasklet_queue_cmd:
 * @Brief:                  Structure associated with each slot in the
 *                          tasklet queue
 *
 * @list:                   list_head member for each entry in queue
 * @payload:                Payload structure for the event. This will be
 *                          passed to the handler function
 * @handler_priv:           Private data passed at event subscribe
 * @bottom_half_handler:    Function pointer for event handler in bottom
 *                          half context
 * @tasklet_enqueue_ts:     enqueue time of tasklet
 *
 */
struct cam_tasklet_queue_cmd {
	struct list_head                   list;
	void                              *payload;
	void                              *handler_priv;
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler;
	ktime_t                            tasklet_enqueue_ts;
};

/**
 * struct cam_tasklet_info:
 * @Brief:                  Tasklet private structure
 *
 * @list:                   list_head member for each tasklet
 * @index:                  Instance id for the tasklet
 * @tasklet_lock:           Spin lock
 * @tasklet_active:         Atomic variable to control tasklet state
 * @tasklet:                Tasklet structure used to schedule bottom half
 * @free_cmd_list:          List of free tasklet queue cmd for use
 * @used_cmd_list:          List of used tasklet queue cmd
 * @cmd_queue:              Array of tasklet cmd for storage
 * @ctx_priv:               Private data passed to the handling function
 *
 */
struct cam_tasklet_info {
	struct list_head                   list;
	uint32_t                           index;
	spinlock_t                         tasklet_lock;
	atomic_t                           tasklet_active;
	struct tasklet_struct              tasklet;

	struct list_head                   free_cmd_list;
	struct list_head                   used_cmd_list;
	struct cam_tasklet_queue_cmd       cmd_queue[CAM_TASKLETQ_SIZE];
	uint32_t                           magic_num;
	int32_t                            cmd_cnt;
	void                              *ctx_priv;
};

struct cam_irq_bh_api tasklet_bh_api = {
	.bottom_half_enqueue_func = cam_tasklet_enqueue_cmd,
	.get_bh_payload_func = cam_tasklet_get_cmd,
	.put_bh_payload_func = cam_tasklet_put_cmd,
};

int cam_tasklet_get_cmd(
	void                         *bottom_half,
	void                        **bh_cmd)
{
	int           rc = 0;
	unsigned long flags;
	struct cam_tasklet_info        *tasklet = bottom_half;
	struct cam_tasklet_queue_cmd   *tasklet_cmd = NULL;

	*bh_cmd = NULL;

	if (tasklet == NULL) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "tasklet is NULL");
		return -EINVAL;
	}

	if (!atomic_read(&tasklet->tasklet_active)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Tasklet idx:%d is not active",
			tasklet->index);
		rc = -EPIPE;
		return rc;
	}

	spin_lock_irqsave(&tasklet->tasklet_lock, flags);
	if (list_empty(&tasklet->free_cmd_list)) {
		CAM_ERR(CAM_ISP, "No more free tasklet cmd idx:%d num %u %p current_cmd %d",
			tasklet->index, tasklet->magic_num, &tasklet->tasklet, tasklet->cmd_cnt);
		rc = -ENODEV;
		goto spin_unlock;
	} else {
		tasklet_cmd = list_first_entry(&tasklet->free_cmd_list,
			struct cam_tasklet_queue_cmd, list);
		list_del_init(&(tasklet_cmd)->list);
		*bh_cmd = tasklet_cmd;
		tasklet->cmd_cnt++;
	}

spin_unlock:
	spin_unlock_irqrestore(&tasklet->tasklet_lock, flags);
	return rc;
}

void cam_tasklet_put_cmd(
	void                         *bottom_half,
	void                        **bh_cmd)
{
	unsigned long flags;
	struct cam_tasklet_info        *tasklet = bottom_half;
	struct cam_tasklet_queue_cmd   *tasklet_cmd = *bh_cmd;

	if (tasklet == NULL) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "tasklet is NULL");
		return;
	}

	if (tasklet_cmd == NULL) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid tasklet_cmd");
		return;
	}

	spin_lock_irqsave(&tasklet->tasklet_lock, flags);
	list_del_init(&tasklet_cmd->list);
	list_add_tail(&tasklet_cmd->list, &tasklet->free_cmd_list);
	*bh_cmd = NULL;
	tasklet->cmd_cnt--;
	spin_unlock_irqrestore(&tasklet->tasklet_lock, flags);
}

/**
 * cam_tasklet_dequeue_cmd()
 *
 * @brief:              Initialize the tasklet info structure
 *
 * @hw_mgr_ctx:         Private Ctx data that will be passed to the handler
 *                      function
 * @idx:                Index of tasklet used as identity
 * @tasklet_action:     Tasklet callback function that will be called
 *                      when tasklet runs on CPU
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
static int cam_tasklet_dequeue_cmd(
	struct cam_tasklet_info        *tasklet,
	struct cam_tasklet_queue_cmd  **tasklet_cmd)
{
	int rc = 0;
	unsigned long flags;

	*tasklet_cmd = NULL;

	CAM_DBG(CAM_ISP, "Dequeue before lock tasklet idx:%d", tasklet->index);
	spin_lock_irqsave(&tasklet->tasklet_lock, flags);
	if (list_empty(&tasklet->used_cmd_list)) {
		CAM_DBG(CAM_ISP, "End of list reached. Exit");
		rc = -ENODEV;
		goto spin_unlock;
	} else {
		*tasklet_cmd = list_first_entry(&tasklet->used_cmd_list,
			struct cam_tasklet_queue_cmd, list);
		list_del_init(&(*tasklet_cmd)->list);
		CAM_DBG(CAM_ISP, "Dequeue Successful");
	}

spin_unlock:
	spin_unlock_irqrestore(&tasklet->tasklet_lock, flags);
	return rc;
}

void cam_tasklet_enqueue_cmd(
	void                              *bottom_half,
	void                              *bh_cmd,
	void                              *handler_priv,
	void                              *evt_payload_priv,
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler)
{
	unsigned long                  flags;
	struct cam_tasklet_queue_cmd  *tasklet_cmd = bh_cmd;
	struct cam_tasklet_info       *tasklet = bottom_half;

	if (!bottom_half) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "NULL bottom half");
		return;
	}

	if (!bh_cmd) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "NULL bh cmd");
		return;
	}

	if (!atomic_read(&tasklet->tasklet_active)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Tasklet is not active idx:%d",
			tasklet->index);
		return;
	}

	CAM_DBG(CAM_ISP, "Enqueue tasklet cmd idx:%d", tasklet->index);
	tasklet_cmd->bottom_half_handler = bottom_half_handler;
	tasklet_cmd->payload = evt_payload_priv;
	tasklet_cmd->handler_priv = handler_priv;
	tasklet_cmd->tasklet_enqueue_ts = ktime_get();
	spin_lock_irqsave(&tasklet->tasklet_lock, flags);
	list_add_tail(&tasklet_cmd->list,
		&tasklet->used_cmd_list);
	spin_unlock_irqrestore(&tasklet->tasklet_lock, flags);

	if ((tasklet->cmd_cnt % 100) == 0)
		CAM_WARN(CAM_ISP, "Anomaly detected Tasklet %u magic_num %u %p enqueued cmds %d",
			tasklet->index, tasklet->magic_num, &tasklet->tasklet, tasklet->cmd_cnt);
	tasklet_hi_schedule(&tasklet->tasklet);
}

int cam_tasklet_init(
	void                    **tasklet_info,
	void                     *hw_mgr_ctx,
	uint32_t                  idx)
{
	int i;
	struct cam_tasklet_info  *tasklet = NULL;

	tasklet = kzalloc(sizeof(struct cam_tasklet_info), GFP_KERNEL);
	if (!tasklet) {
		CAM_DBG(CAM_ISP,
			"Error! Unable to allocate memory for tasklet");
		*tasklet_info = NULL;
		return -ENOMEM;
	}

	tasklet->ctx_priv = hw_mgr_ctx;
	tasklet->index = idx;
	tasklet->magic_num = (idx * 59) + 13;
	tasklet->cmd_cnt = 0;
	spin_lock_init(&tasklet->tasklet_lock);
	memset(tasklet->cmd_queue, 0, sizeof(tasklet->cmd_queue));
	INIT_LIST_HEAD(&tasklet->free_cmd_list);
	INIT_LIST_HEAD(&tasklet->used_cmd_list);
	for (i = 0; i < CAM_TASKLETQ_SIZE; i++) {
		INIT_LIST_HEAD(&tasklet->cmd_queue[i].list);
		list_add_tail(&tasklet->cmd_queue[i].list,
			&tasklet->free_cmd_list);
	}
	tasklet_init(&tasklet->tasklet, cam_tasklet_action,
		(unsigned long)tasklet);
	tasklet_disable(&tasklet->tasklet);

	*tasklet_info = tasklet;

	CAM_INFO(CAM_ISP, "idx %u magic_num %u %p",
		tasklet->index, tasklet->magic_num, &tasklet->tasklet);
	return 0;
}

void cam_tasklet_deinit(void    **tasklet_info)
{
	struct cam_tasklet_info *tasklet = *tasklet_info;

	if (atomic_read(&tasklet->tasklet_active)) {
		atomic_set(&tasklet->tasklet_active, 0);
		tasklet_kill(&tasklet->tasklet);
		tasklet_disable(&tasklet->tasklet);
	}
	kfree(tasklet);
	*tasklet_info = NULL;
}

static inline void cam_tasklet_flush(struct cam_tasklet_info *tasklet_info)
{
	CAM_INFO(CAM_ISP, "Enter idx %u num %u %p cmd_cnt %d",
		tasklet_info->index, tasklet_info->magic_num,
		&tasklet_info->tasklet, tasklet_info->cmd_cnt);
	cam_tasklet_action((unsigned long) tasklet_info);
	CAM_INFO(CAM_ISP, "Exit idx %u",
		tasklet_info->index);
}

int cam_tasklet_start(void  *tasklet_info)
{
	struct cam_tasklet_info       *tasklet = tasklet_info;
	int i = 0;

	if (atomic_read(&tasklet->tasklet_active)) {
		CAM_ERR(CAM_ISP, "Tasklet already active idx:%d",
			tasklet->index);
		return -EBUSY;
	}

	/* clean up the command queue first */
	for (i = 0; i < CAM_TASKLETQ_SIZE; i++) {
		list_del_init(&tasklet->cmd_queue[i].list);
		list_add_tail(&tasklet->cmd_queue[i].list,
			&tasklet->free_cmd_list);
	}

	CAM_INFO(CAM_ISP, "idx %u magic_num %u %p",
		tasklet->index, tasklet->magic_num, &tasklet->tasklet);
	atomic_set(&tasklet->tasklet_active, 1);

	tasklet_enable(&tasklet->tasklet);

	return 0;
}

void cam_tasklet_stop(void  *tasklet_info)
{
	struct cam_tasklet_info  *tasklet = tasklet_info;

	if (!atomic_read(&tasklet->tasklet_active))
		return;

	CAM_INFO(CAM_ISP, "Enter idx %u magic_num %u %p",
		tasklet->index, tasklet->magic_num, &tasklet->tasklet);
	atomic_set(&tasklet->tasklet_active, 0);
	tasklet_kill(&tasklet->tasklet);
	CAM_INFO(CAM_ISP, "Kill done idx %u", tasklet->index);
	tasklet_disable(&tasklet->tasklet);
	cam_tasklet_flush(tasklet);
	CAM_INFO(CAM_ISP, "Exit idx %u magic_num %u %p",
		tasklet->index, tasklet->magic_num, &tasklet->tasklet);
}

/*
 * cam_tasklet_action()
 *
 * @brief:              Process function that will be called  when tasklet runs
 *                      on CPU
 *
 * @data:               Tasklet Info structure that is passed in tasklet_init
 *
 * @return:             Void
 */
static void cam_tasklet_action(unsigned long data)
{
	//bool flag = false;
	struct cam_tasklet_info          *tasklet_info = NULL;
	struct cam_tasklet_queue_cmd     *tasklet_cmd = NULL;
	ktime_t                           curr_time;

	tasklet_info = (struct cam_tasklet_info *)data;

	while (!cam_tasklet_dequeue_cmd(tasklet_info, &tasklet_cmd)) {
		cam_tasklet_delay_detect(tasklet_info->index, tasklet_cmd->tasklet_enqueue_ts,
			CAM_TASKLET_SCHED_TIME_THRESHOLD);
		curr_time = ktime_get();

		tasklet_cmd->bottom_half_handler(tasklet_cmd->handler_priv,
			tasklet_cmd->payload);

		cam_tasklet_delay_detect(tasklet_info->index, curr_time,
			CAM_TASKLET_EXE_TIME_THRESHOLD);
		cam_tasklet_put_cmd(tasklet_info, (void **)(&tasklet_cmd));
		//flag = true;
	}
}

/*
 * cam_tasklet_delay_detect()
 *
 * @brief:              Function to detect delay
 *
 * @start_timestamp:    start timestamp
 *
 * @threshold:          threshold limit
 *
 * @return:             Void
 */

static void cam_tasklet_delay_detect(
	uint32_t idx, ktime_t start_timestamp, uint32_t threshold)
{
	uint64_t                         diff;
	ktime_t                          cur_time;
	struct timespec64                cur_ts;
	struct timespec64                start_ts;

	cur_time = ktime_get();
	diff = ktime_ms_delta(cur_time, start_timestamp);
	start_ts  = ktime_to_timespec64(start_timestamp);
	cur_ts = ktime_to_timespec64(cur_time);

	if (diff > threshold) {
			CAM_WARN_RATE_LIMIT_CUSTOM(CAM_ISP, 1, 1,
				"tasklet %u %s delay detected %ld:%06ld, curr %ld:%06ld, diff %ld:",
				idx,
				(threshold == CAM_TASKLET_EXE_TIME_THRESHOLD) ? "execution" : "scheduling",
				start_ts.tv_sec,
				start_ts.tv_nsec/NSEC_PER_USEC,
				cur_ts.tv_sec, cur_ts.tv_nsec/NSEC_PER_USEC,
				diff);
	}
}
