/*
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>

#include "../panel_drv.h"
#include "display_profiler.h"

static int profiler_do_seqtbl_by_index_nolock(struct profiler_device *p, int index)
{
	int ret;
	struct seqinfo *tbl;
	struct panel_info *panel_data;
	struct panel_device *panel = container_of(p, struct panel_device, profiler);
	struct timespec cur_ts, last_ts, delta_ts;
	s64 elapsed_usec;

	if (panel == NULL) {
		profiler_error("%s, panel is null\n", __func__);
		return -EINVAL;
	}

	if (!IS_PANEL_ACTIVE(panel))
		return 0;

	panel_data = &panel->panel_data;
	tbl = panel->profiler.seqtbl;
	ktime_get_ts(&cur_ts);

#if 0
	ktime_get_ts(&last_ts);
#endif

	if (unlikely(index < 0 || index >= MAX_PROFILER_SEQ)) {
		profiler_error("%s, invalid paramter (panel %p, index %d)\n",
				__func__, panel, index);
		ret = -EINVAL;
		goto do_exit;
	}

#if 0
	delta_ts = timespec_sub(last_ts, cur_ts);
	elapsed_usec = timespec_to_ns(&delta_ts) / 1000;
	if (elapsed_usec > 34000)
		pr_debug("seq:%s warn:elapsed %lld.%03lld msec to acquire lock\n",
				tbl[index].name, elapsed_usec / 1000, elapsed_usec % 1000);
#endif
	ret = panel_do_seqtbl(panel, &tbl[index]);
	if (unlikely(ret < 0)) {
		profiler_error("%s, failed to excute seqtbl %s\n",
				__func__, tbl[index].name);
		ret = -EIO;
		goto do_exit;
	}

do_exit:
	ktime_get_ts(&last_ts);
	delta_ts = timespec_sub(last_ts, cur_ts);
	elapsed_usec = timespec_to_ns(&delta_ts) / 1000;
	pr_debug("seq:%s done (elapsed %2lld.%03lld msec)\n",
			tbl[index].name, elapsed_usec / 1000, elapsed_usec % 1000);

	return 0;
}

int profiler_do_seqtbl_by_index(struct profiler_device *p, int index)
{
	int ret = 0;
	struct panel_device *panel = container_of(p, struct panel_device, profiler);

	if (panel == NULL) {
		profiler_error("%s, panel is null\n", __func__);
		return -EINVAL;
	}

//	panel_dbg("%s : %d\n", __func__, index);

	mutex_lock(&panel->op_lock);
	ret = profiler_do_seqtbl_by_index_nolock(p, index);
	mutex_unlock(&panel->op_lock);

	return ret;
}


#define PROFILER_SYSTRACE_BUF_SIZE	40

extern int decon_systrace_enable;

void profiler_systrace_write(int pid, char id, char *str, int value)
{
	char buf[PROFILER_SYSTRACE_BUF_SIZE] = {0, };

	if (!decon_systrace_enable)
		return;

	 switch(id) {
		case 'B':
			snprintf(buf, PROFILER_SYSTRACE_BUF_SIZE, "B|%d|%s", pid, str);
			break;
		case 'E':
			strcpy(buf, "E");
			break;
		case 'C':
			snprintf(buf, PROFILER_SYSTRACE_BUF_SIZE,
				"C|%d|%s|%d", pid, str, 1);
			break;
		default:
			profiler_error("%s, wrong argument : %c\n", __func__, id);
			return;
	}
	panel_info("%s\n", buf);
	trace_printk("%s", buf);

}

#if 0

static int update_profile_win(struct profiler_device *p, void *arg)
{
	int ret = 0;

	int width, height;
	struct panel_device *panel;
	int cmd_idx = PROFILE_DISABLE_WIN_SEQ;
	struct decon_rect *up_region = (struct decon_rect *)arg;

	panel = container_of(p, struct panel_device, profiler);

	memcpy((struct decon_rect *)&profiler->win_rect, up_region, sizeof(struct decon_rect));

	width = profiler->win_rect.right - profiler->win_rect.left;
	height = profiler->win_rect.bottom - profiler->win_rect.top;

	panel_info("PROFILE_WIN_UPDATE region : %d:%d, %d:%d\n",
			profiler->win_rect.left, profiler->win_rect.top,
			profiler->win_rect.right, profiler->win_rect.bottom);

	if ((width == panel->panel_data.props.xres - 1) &&
		(height == panel->panel_data.props.yres - 1)) {
		cmd_idx = PROFILE_DISABLE_WIN_SEQ;
		panel_info("PROFILE_WIN_UPDATE full region : disable\n");
	} else {
		cmd_idx = PROFILE_WIN_UPDATE_SEQ;
	}

	ret = profiler_do_seqtbl_by_index(p, cmd_idx);

	return ret;
}
#endif

static int update_profile_hiber(struct profiler_device *p, bool enter_exit, s64 time_us)
{
	int ret = 0;
	struct profiler_hiber *hiber_info;

	hiber_info = &p->hiber_info;

	if (hiber_info->hiber_status == enter_exit) {
		profiler_error("%s, status already %s\n", __func__, enter_exit ? "ENTER" : "EXIT");
		return ret;
	}

	if (enter_exit) {
		hiber_info->hiber_enter_cnt++;
		hiber_info->hiber_enter_time = time_us;
	}
	else {
		hiber_info->hiber_exit_cnt++;
		hiber_info->hiber_exit_time = time_us;
	}

	prof_info(p, hiber_debug, "%s: status %s -> %s\n", __func__,
		hiber_info->hiber_status ? "ENTER" : "EXIT",
		enter_exit ? "ENTER" : "EXIT");

	hiber_info->hiber_status = enter_exit;

	return ret;
}

static int update_profile_te(struct profiler_device *p, s64 time_us)
{
	int ret = 0;
	struct profiler_te *te_info;
	te_info = &p->te_info;

	spin_lock(&te_info->slock);
	if (time_us == 0) {
		te_info->last_time = 0;
		te_info->last_diff = 0;
		spin_unlock(&te_info->slock);
		prof_info(p, te_debug, "%s:reset\n", __func__);
		return ret;
	}
	if (te_info->last_time == time_us) {
		spin_unlock(&te_info->slock);
		prof_info(p, te_debug, "%s:skipped\n", __func__);
		return ret;
	}

	if (te_info->last_time == 0) {
		te_info->last_time = time_us;
		spin_unlock(&te_info->slock);
		prof_info(p, te_debug, "%s:last time: %lld\n", __func__, te_info->last_time);
		return ret;
	}

	te_info->last_diff = time_us - te_info->last_time;
	te_info->last_time = time_us;
	spin_unlock(&te_info->slock);

	prof_info(p, te_debug, "%s:last time: %lld, diff: %lld\n", __func__, te_info->last_time, te_info->last_diff);
	te_info->times[te_info->idx++] = te_info->last_diff;
	te_info->idx = te_info->idx % MAX_TE_CNT;

	return ret;
}

static int update_profile_win_config(struct profiler_device *p)
{
	int ret = 0;

	s64 diff;
	ktime_t timestamp;

	timestamp = ktime_get();

	if (p->fps.frame_cnt < FPS_MAX)
		p->fps.frame_cnt++;
	else
		p->fps.frame_cnt = 0;

	if (ktime_to_us(p->fps.win_config_time) != 0) {
		diff = ktime_to_us(ktime_sub(timestamp, p->fps.win_config_time));
		p->fps.instant_fps = (int)(1000000 / diff);

		if (p->fps.instant_fps >= 59)
			p->fps.color = FPS_COLOR_RED;
		else
			p->fps.color = FPS_COLOR_GREEN;

		if (p->conf->systrace)
			decon_systrace(get_decon_drvdata(0), 'C', "fps_inst", (int)p->fps.instant_fps);

//		ret = profiler_do_seqtbl_by_index(p, PROFILE_SET_COLOR_SEQ);
	}
	p->fps.win_config_time = timestamp;

	return ret;
}

struct win_config_backup {
	int state;
	struct decon_frame src;
	struct decon_frame dst;
};


#if 0

static int update_profiler_circle(struct profiler_device *p, unsigned int color)
{
	int ret = 0;

	if (p->circle_color != color) {
		p->circle_color = color;
		ret = profiler_do_seqtbl_by_index(p, PROFILER_SET_CIRCLR_SEQ);
	}

	return ret;
}
#endif

static inline bool profiler_is_cmdlog_initialized(struct profiler_device *p)
{
	if (p->cmdlog_idx_head < 0)
		return false;
	return true;
}

int profiler_cmdlog_init(struct profiler_device *p)
{
	if (p->cmdlog_idx_head >= 0) {
		//already initialized
		p->cmdlog_idx_head = 0;
		p->cmdlog_idx_tail = 0;
		p->cmdlog_data_idx = 0;
		return 0;
	}

	p->cmdlog_list = kmalloc(sizeof(struct profiler_cmdlog_data) * PROFILER_CMDLOG_SIZE, GFP_KERNEL);
	if (!p->cmdlog_list)
		return -ENOMEM;

	p->cmdlog_data = kmalloc(sizeof(u8) * PROFILER_CMDLOG_BUF_SIZE, GFP_KERNEL);
	if (!p->cmdlog_data) {
		kfree(p->cmdlog_list);
		return -ENOMEM;
	}
	p->cmdlog_idx_head = 0;
	p->cmdlog_idx_tail = 0;
	p->cmdlog_data_idx = 0;
	return 0;
}

static int print_cmdlog(struct profiler_device *p)
{
	struct profiler_cmdlog_data *c;
	int h, t;
	s64 time = 0;
	u32 tmp;
	bool dir_read, dir_write;

	if (!profiler_is_cmdlog_initialized(p))
		return -EINVAL;

	h = p->cmdlog_idx_head;
	t = p->cmdlog_idx_tail;

	if (t == h) {
		//empty
		return 0;
	}

	while (t != h) {
		c = &p->cmdlog_list[t];
		if (PROFILER_DATALOG_MASK_PROTO(c->pkt_type) == PROFILER_DATALOG_CMD_DSI) {
			//command log print
			dir_read = PROFILER_DATALOG_MASK_DIR(c->pkt_type) == PROFILER_DATALOG_DIRECTION_READ;
			dir_write = PROFILER_DATALOG_MASK_DIR(c->pkt_type) == PROFILER_DATALOG_DIRECTION_WRITE;

			prof_info(p, cmdlog_disp, "cmdlog[%d] DSI_CMD 0x%02x %s%s reg 0x%02x, ofs 0x%02x, len %d\n",
				t, PROFILER_DATALOG_MASK_SUB(c->pkt_type), (dir_read ? "R":""), (dir_write ? "W":""),
				c->cmd, c->offset, c->size);
			//loglevel==2, c->data is not NULL
			if (c->data != NULL)
				print_hex_dump(KERN_ERR, "cmdlog_data ", DUMP_PREFIX_NONE, 16, 1, c->data, c->size, false);
		} else if (PROFILER_DATALOG_MASK_PROTO(c->pkt_type) == PROFILER_DATALOG_PANEL) {
			//panel log print
			tmp = PROFILER_DATALOG_MASK_SUB(c->pkt_type);
			if (tmp == PROFILER_DATALOG_PANEL_CMD_FLUSH_START) {
				time = c->time;
				prof_info(p, cmdlog_disp, "cmdlog[%d] PANEL_LOG FLUSH_START cmdcnt %d, payload %d, time %lld\n",
					t, c->cmd, c->size, c->time);
			} else if (tmp == PROFILER_DATALOG_PANEL_CMD_FLUSH_END) {
				if (time > 0)
					time = c->time - time;
				else
					time = 0;
				prof_info(p, cmdlog_disp, "cmdlog[%d] PANEL_LOG FLUSH_END cmdcnt %d, payload %d, elapsed: %lldus, time %lld\n",
					t, c->cmd, c->size, time / 1000, c->time);
			}
		}
		t = (t + 1) % PROFILER_CMDLOG_SIZE;
		dir_read = dir_write = false;
	}
	p->cmdlog_idx_tail = t;
	return 0;
}

static int insert_cmdlog(struct profiler_device *p, struct profiler_cmdlog_data *log)
{
	int ret = 0;
	struct profiler_config *conf = p->conf;
	struct profiler_cmdlog_data *c = NULL;
	struct timespec cur_ts, last_ts, delta_ts;
	s64 elapsed_nsec;

	if (!profiler_is_cmdlog_initialized(p)) {
		//not initialized
		return -EINVAL;
	}

	if ((PROFILER_DATALOG_MASK_PROTO(log->pkt_type) == PROFILER_DATALOG_CMD_DSI)
		&& prof_en(p, cmdlog_filter)) {
		if ((log->cmd & 0xFF) < PROFILER_CMDLOG_FILTER_SIZE+1) {
			if (profiler_cmdlog_filter_list[(log->cmd & 0xFF)] == false) {
				prof_info(p, cmdlog_debug, "filtered cmd 0x%02x, type 0x%02x ofs 0x%02x, len %d\n",
						 log->cmd, log->pkt_type, log->offset, log->size);
				return 0;
			}
		}
	}

	if (prof_en(p, timediff)) {
		ktime_get_ts(&cur_ts);
	}

	c = &p->cmdlog_list[p->cmdlog_idx_head];
	if (conf->cmdlog_level >= 1) {
		memcpy(c, log, sizeof(struct profiler_cmdlog_data));
		c->data = NULL;

		// cmdlog_disp=2 : print sync
		/*
		if (p->conf->cmdlog_disp >= 2) {
			prof_info(p, cmdlog_disp, "cmdlog[%d] type 0x%02x cmd 0x%02x, ofs 0x%02x, len %d, ktime %lld.%09llds\n",
				p->cmdlog_idx_head, c->pkt_type, c->cmd, c->offset, c->size, c->time / 1000000000, c->time % 1000000000);
		}
		*/
	}

	if (conf->cmdlog_level >= 2) {
		//rewind if buf overrun
		do {
			if (log->data == NULL) {
				prof_info(p, cmdlog_debug, "data is NULL. cmdlog[%d] type 0x%02x cmd 0x%02x, ofs 0x%02x, len %d\n",
					p->cmdlog_idx_head, c->pkt_type, c->cmd, c->offset, c->size);
				break;
			}
/*
			if (!(c->pkt_type & PROFILER_DATALOG_DIRECTION_WRITE)) {
				prof_info(p, cmdlog_debug, "only write cmd shows data dump\n");
				break;
			}
*/
			if (c->size > PROFILER_CMDLOG_BUF_SIZE / 2) {
				prof_info(p, cmdlog_debug, "data is too long(%d), skip logging > %d\n", c->size, PROFILER_CMDLOG_BUF_SIZE / 2);
				break;
			}

			if (p->cmdlog_data_idx + c->size > PROFILER_CMDLOG_BUF_SIZE) {
				prof_info(p, cmdlog_debug, "databuffer rewind %d %d\n", p->cmdlog_data_idx, c->size);
				p->cmdlog_data_idx = 0;
			}

			memcpy(p->cmdlog_data + p->cmdlog_data_idx, log->data, log->size);
			c->data = p->cmdlog_data + p->cmdlog_data_idx;
			p->cmdlog_data_idx += c->size;

			// cmdlog_disp=2 : print sync
			/*
			if (p->conf->cmdlog_disp >= 2) {
				print_hex_dump(KERN_ERR, "cmdlog_data ", DUMP_PREFIX_NONE, 16, 1, c->data, c->size, false);
			}
			*/
		} while (0);
	}
	if (!c) {
		prof_info(p, cmdlog_debug, "cmdlog level is 0(off)\n");
		return 0;
	}

	if (p->conf->cmdlog_disp == 2) {
		print_cmdlog(p);
	}

	if (prof_en(p, timediff)) {
		ktime_get_ts(&last_ts);
		delta_ts = timespec_sub(last_ts, cur_ts);
		elapsed_nsec = timespec_to_ns(&delta_ts);
		prof_info(p, timediff_en, "cmdlog elapsed = %lldns, 0x%02x len %d\n", elapsed_nsec, c->cmd, c->size);
	}

	p->cmdlog_idx_head = (p->cmdlog_idx_head + 1) % PROFILER_CMDLOG_SIZE;
	if (p->cmdlog_idx_head == p->cmdlog_idx_tail)
		p->cmdlog_idx_tail = (p->cmdlog_idx_head + 1) % PROFILER_CMDLOG_SIZE;
/*
	if (p->conf->cmdlog_disp >= 2)
		p->cmdlog_idx_tail = p->cmdlog_idx_head;
*/
	return ret;
}

static long profiler_v4l2_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
//	unsigned int color;
	struct panel_device *panel;
	struct profiler_device *p = container_of(sd, struct profiler_device, sd);

	if (p->conf == NULL) {
		return (long)ret;
	}

	panel = container_of(p, struct panel_device, profiler);

	switch(cmd) {
	case PROFILE_REG_DECON:
		panel_info("PROFILE_REG_DECON was called\n");
		break;

	case PROFILE_WIN_UPDATE:
#if 0
		if (p->win_rect.disp_en)
			ret = update_profile_win(p, arg);
#endif
		break;

	case PROFILE_WIN_CONFIG:
		if (prof_en(p, fps))
			ret = update_profile_win_config(p);
		break;
	case PROFILE_TE:
		if (prof_en(p, te))
			update_profile_te(p, (s64)arg);
		break;
	case PROFILE_HIBER_ENTER:
		if (prof_en(p, te))
			update_profile_te(p, 0);
		if (prof_en(p, hiber))
			update_profile_hiber(p, true, (s64)arg);
		break;
	case PROFILE_HIBER_EXIT:
		if (prof_en(p, te))
			update_profile_te(p, 0);
		if (prof_en(p, hiber))
			update_profile_hiber(p, false, (s64)arg);
		break;
	case PROFILE_DATALOG:
		if (prof_en(p, cmdlog)) {
			if (!profiler_is_cmdlog_initialized(p))
				profiler_cmdlog_init(p);
			insert_cmdlog(p, (struct profiler_cmdlog_data *)arg);
		}
		break;
	case PROFILER_SET_PID:
		p->systrace.pid = *(int *)arg;
		break;

	case PROFILER_COLOR_CIRCLE:
#if 0
		color = *(int *)arg;
		ret = update_profiler_circle(p, color);
#endif
		break;

	}

	return (long)ret;
}

static const struct v4l2_subdev_core_ops profiler_v4l2_core_ops = {
	.ioctl = profiler_v4l2_ioctl,
};

static const struct v4l2_subdev_ops profiler_subdev_ops = {
	.core = &profiler_v4l2_core_ops,
};

static void profiler_init_v4l2_subdev(struct panel_device *panel)
{
	struct v4l2_subdev *sd;
	struct profiler_device *p = &panel->profiler;

	sd = &p->sd;

	v4l2_subdev_init(sd, &profiler_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = 0;

	snprintf(sd->name, sizeof(sd->name), "%s.%d", "panel-profiler", 0);

	v4l2_set_subdevdata(sd, p);
}


void profile_fps(struct profiler_device *p)
{
	s64 time_diff;
	unsigned int gap, c_frame_cnt;
	ktime_t c_time, p_time;
	struct profiler_fps *fps;

	fps = &p->fps;

	c_frame_cnt = fps->frame_cnt;
	c_time = ktime_get();

	if (c_frame_cnt >= fps->prev_frame_cnt)
		gap = c_frame_cnt - fps->prev_frame_cnt;
	else
		gap = (FPS_MAX - fps->prev_frame_cnt) + c_frame_cnt;

	p_time = fps->slot[fps->slot_cnt].timestamp;
	fps->slot[fps->slot_cnt].timestamp = c_time;

	fps->total_frame -= fps->slot[fps->slot_cnt].frame_cnt;
	fps->total_frame += gap;
	fps->slot[fps->slot_cnt].frame_cnt = gap;

	time_diff = ktime_to_us(ktime_sub(c_time, p_time));
	//panel_info("%s : diff : %llu : slot_cnt %d\n", __func__, time_diff, fps->slot_cnt);

	/*To Do.. after lcd off->on, must clear fps->slot data to zero and comp_fps, instan_fps set to 60Hz (default)*/
	if (ktime_to_us(p_time) != 0) {
		time_diff = ktime_to_us(ktime_sub(c_time, p_time));

		fps->average_fps = fps->total_frame;
		fps->comp_fps = (unsigned int)(((1000000000 / time_diff) * fps->total_frame) + 500) / 1000;

		prof_info(p, fps_debug, "avg fps : %d\n", fps->comp_fps);

		time_diff = ktime_to_us(ktime_sub(c_time, p->fps.win_config_time));
		if (time_diff >= 100000) {
			fps->instant_fps = fps->average_fps;
			if (p->conf->systrace)
				decon_systrace(get_decon_drvdata(0), 'C', "fps_inst", (int)fps->instant_fps);
		}
		if (p->conf->systrace)
			decon_systrace(get_decon_drvdata(0), 'C', "fps_aver", (int)fps->comp_fps);
	}

	fps->prev_frame_cnt = c_frame_cnt;
	fps->slot_cnt = (fps->slot_cnt + 1) % MAX_SLOT_CNT;
}

static int profiler_mprint_update(struct profiler_device *p)
{
	int ret = 0;
	struct panel_device *panel = container_of(p, struct panel_device, profiler);
	struct timespec cur_ts, last_ts, delta_ts;
	s64 elapsed_usec = 0;

	struct pktinfo PKTINFO(self_mprint_data) = {
        .name = "self_mprint_data",
        .type = DSI_PKT_TYPE_WR_SR,
        .data = p->mask_props.data,
        .offset = 0,
        .dlen = p->mask_props.data_size,
        .pktui = NULL,
        .nr_pktui = 0,
        .option = 0,
    };

    void *self_mprint_data_cmdtbl[] = {
        &PKTINFO(self_mprint_data),
    };

	struct seqinfo self_mprint_data_seq = SEQINFO_INIT("self_mprint_data_seq", self_mprint_data_cmdtbl);

	if (!IS_PANEL_ACTIVE(panel))
		return -EIO;

	panel_wake_lock(panel);
	ret = profiler_do_seqtbl_by_index(p, DISABLE_PROFILE_FPS_MASK_SEQ);
/*
	if (p->mask_config->debug)
		prof_info(p, mprint_debug, "disable fps mask ret %d\n", ret);
*/

	ret = profiler_do_seqtbl_by_index_nolock(p, WAIT_PROFILE_FPS_MASK_SEQ);
/*
	if (p->mask_config->debug)
		prof_info(p, mprint_debug, "wait fps mask ret %d\n", ret);
*/
	mutex_lock(&panel->op_lock);
	ret = profiler_do_seqtbl_by_index_nolock(p, MEM_SELECT_PROFILE_FPS_MASK_SEQ);

	//send mask data
	ktime_get_ts(&cur_ts);
	ret = panel_do_seqtbl(panel, &self_mprint_data_seq);
	if (unlikely(ret < 0)) {
		pr_err("[D_PROF] %s, failed to excute self_mprint_data_cmdtbl(%d)\n", __func__, p->mask_props.data_size);
		ret = -EIO;
		goto do_exit;
	}

//	if (p->mask_config->debug) {
	if (prof_en(p, timediff)) {
		ktime_get_ts(&last_ts);
		delta_ts = timespec_sub(last_ts, cur_ts);
		elapsed_usec = timespec_to_ns(&delta_ts) / 1000;
	}
	prof_info(p, timediff_en, "write mask image, elapsed = %lldus\n", elapsed_usec);
	prof_info(p, mprint_debug, "write mask image, size = %d\n", p->mask_props.data_size);

	ret = profiler_do_seqtbl_by_index_nolock(p, ENABLE_PROFILE_FPS_MASK_SEQ);
do_exit:
	mutex_unlock(&panel->op_lock);
	panel_wake_unlock(panel);

//	if (p->mask_config->debug)
	if (prof_dbg(p, mprint))
		prof_info(p, mprint_debug, "enable fps mask ret %d\n", ret);

	return ret;
}

#define PROFILER_TEXT_OUTPUT_LEN 32
static int profiler_thread(void *data)
{
	int ret;
	struct profiler_device *p = data;
	//struct profiler_fps *fps = &p->fps;
	struct mprint_props *mask_props;
	struct profiler_te *te;
	struct profiler_hiber *hiber;
	char text_old[PROFILER_TEXT_OUTPUT_LEN] = {0, };
	char text[PROFILER_TEXT_OUTPUT_LEN] = {0, };
	int len = 0;
	int cycle_time = 0;
	struct timespec cur_ts, last_ts, delta_ts;
	s64 elapsed_usec;

	if (p->conf == NULL) {
		profiler_error("%s, profiler config is null\n", __func__);
		return -EINVAL;
	}

	mask_props = &p->mask_props;
	te = &p->te_info;
	hiber = &p->hiber_info;

	while(!kthread_should_stop()) {
		cycle_time = p->conf->cycle_time > 5 ? p->conf->cycle_time : CYCLE_TIME_DEFAULT;
		schedule_timeout_interruptible(cycle_time);

		if (!prof_en(p, profiler)) {
			schedule_timeout_interruptible(msecs_to_jiffies(cycle_time * 10));
			continue;
		}

		len = 0;
		if (prof_disp(p, hiber)) {
			len += snprintf(text + len, ARRAY_SIZE(text) - len, "%c ",
				hiber->hiber_status ? 'H' : ' ');
		}

		if (prof_disp(p, fps)) {
			profile_fps(p);
			len += snprintf(text + len, ARRAY_SIZE(text) - len, "%3d ", p->fps.comp_fps);
		}

		if (prof_disp(p, te)) {
			if (te->last_diff > 0) {
				len += snprintf(text + len, ARRAY_SIZE(text) - len, "%3lld.%02lld ",
					te->last_diff / 1000, (te->last_diff % 1000) / 10);
			}
		}

		if (prof_en(p, mprint) && len > 0 && strncmp(text_old, text, len) != 0) {
			if (prof_en(p, timediff))
				ktime_get_ts(&cur_ts);

			ret = char_to_mask_img(mask_props, text);

			if (prof_en(p, timediff))
				ktime_get_ts(&last_ts);

			if (ret < 0) {
				profiler_error("%s, err on mask img gen '%s'\n", __func__, text);
				continue;
			}
			if (prof_en(p, timediff)) {
				delta_ts = timespec_sub(last_ts, cur_ts);
				elapsed_usec = timespec_to_ns(&delta_ts) / 1000;
			}
			prof_info(p, mprint_debug, "generated img by '%s', size %d, cyc %d\n",
				text, mask_props->data_size, cycle_time);

			prof_info(p, timediff_en, "generated elapsed = %lldus, '%s'\n", elapsed_usec, text);

			profiler_mprint_update(p);
			memcpy(text_old, text, ARRAY_SIZE(text));
		}

		if (profiler_is_cmdlog_initialized(p) && p->conf->cmdlog_disp == 1) {
			print_cmdlog(p);
		}

	}
	return 0;
}

static ssize_t prop_config_mprint_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	int *data;
	int i, len = 0;

	p = &panel->profiler;
	if (p == NULL) {
		profiler_error("%s, profiler is null\n", __func__);
		return -EINVAL;
	}

	if (p->mask_config == NULL) {
		profiler_error("%s, mask config is null\n", __func__);
		return -EINVAL;
	}

	data = (int *) p->mask_config;
	for (i=0; i<sizeof(struct mprint_config) / sizeof(int); i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%12s",
			(i < ARRAY_SIZE(mprint_config_names)) ? mprint_config_names[i] : "(null)");
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	for (i=0; i<sizeof(struct mprint_config) / sizeof(int); i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%12d", *(data + i));
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return strlen(buf);
}

static ssize_t prop_config_mprint_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	int *data;
	int val;
	int i, len = 0, read, ret;

	p = &panel->profiler;
	if (p == NULL) {
		profiler_error("%s, profiler is null\n", __func__);
		return -EINVAL;
	}

	if (p->mask_config == NULL) {
		profiler_error("%s, mask config is null\n", __func__);
		return -EINVAL;
	}

	data = (int *) p->mask_config;
	for (i=0; i<sizeof(struct mprint_config) / sizeof(int); i++) {
		ret = sscanf(buf + len, "%d%n", &val, &read);
		if (ret < 1)
			break;
		*(data+i) = val;
		len += read;
		panel_info("[D_PROF] %s:config[%d] set to %d\n", __func__, i, val);
	}

	return size;
}

static ssize_t prop_partial_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);

	p = &panel->profiler;
	if (p == NULL) {
		profiler_error("%s, profiler is null\n", __func__);
		return -EINVAL;
	}

//	snprintf(buf, PAGE_SIZE, "%u\n", p->win_rect.disp_en);

	return strlen(buf);
}


static ssize_t prop_partial_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc;
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);

	p = &panel->profiler;
	if (p == NULL) {
		profiler_error("%s, profiler is null\n", __func__);
		return -EINVAL;
	}

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	return size;
}

static ssize_t prop_config_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	int *data;
	int i, len = 0, count;

	p = &panel->profiler;
	if (p == NULL) {
		profiler_error("%s, profiler is null\n", __func__);
		return -EINVAL;
	}

	if (p->conf == NULL) {
		profiler_error("%s, profiler config is null\n", __func__);
		return -EINVAL;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
		"DISPLAY_PROFILER_VER=%d\n", PROFILER_VERSION);

	count = sizeof(struct profiler_config) / sizeof(int);

	if (count != ARRAY_SIZE(profiler_config_names)) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"CONFIG SIZE MISMATCHED!! configurations are may be wrong(%d, %lu)\n",
			count, ARRAY_SIZE(profiler_config_names));
	}

	if (count > ARRAY_SIZE(profiler_config_names))
		count = ARRAY_SIZE(profiler_config_names);

	data = (int *) p->conf;

	for (i=0; i<count; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s=%d%s",
			(i < ARRAY_SIZE(profiler_config_names)) ? profiler_config_names[i] : "(null)",
			*(data+i),
			(i < count - 1) ? "," : "\n");
	}

	return strlen(buf);
}

static ssize_t prop_config_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	char str[1024] = { 0, };
	char *strbuf, *tok, *field, *value;
	int *data;
	int val;
	int i, ret;

	p = &panel->profiler;
	if (p == NULL) {
		profiler_error("%s, profiler is null\n", __func__);
		return -EINVAL;
	}

	if (p->conf == NULL) {
		profiler_error("%s, profiler config is null\n", __func__);
		return -EINVAL;
	}

	data = (int *) p->conf;

	memcpy(str, buf, size < 1024 ? size : 1024);
	strbuf = str;

	while ((tok = strsep(&strbuf, ",")) != NULL) {
		field = strsep(&tok, "=");
		if (field == NULL) {
			profiler_error("%s, invalid field\n", __func__);
			return -EINVAL;
		}
		field = strim(field);
		if (strlen(field) < 1) {
			profiler_error("%s, invalid field\n", __func__);
			return -EINVAL;
		}
		value = strsep(&tok, "=");
		if (value == NULL) {
			profiler_error("%s, invalid value with field %s\n", __func__, field);
			return -EINVAL;
		}
		prof_info(p, profiler_debug, "[D_PROF] %s: field %s with value %s\n", __func__, field, value);
		ret = kstrtouint(strim(value), 0, &val);
		if (ret < 0) {
			profiler_error("%s, invalid value %s, ret %d\n", __func__, value, ret);
			return ret;
		}
		for (i=0; i<ARRAY_SIZE(profiler_config_names); i++) {
			if (!strncmp(field, profiler_config_names[i], strlen(profiler_config_names[i])))
				break;
		}
		if (i < ARRAY_SIZE(profiler_config_names)) {
			*(data + i) = val;
			panel_info("[D_PROF] %s: config set %s->%d\n",
				__func__, profiler_config_names[i], val);
		}
	}

	return size;
}

static ssize_t prop_config_cmdlog_filter_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	int *data;
	int i, len = 0, count;
	int start;

	p = &panel->profiler;
	if (p == NULL) {
		profiler_error("%s, profiler is null\n", __func__);
		return -EINVAL;
	}

	if (p->conf == NULL) {
		profiler_error("%s, profiler config is null\n", __func__);
		return -EINVAL;
	}

	count = ARRAY_SIZE(profiler_cmdlog_filter_list);

	data = (int *) p->conf;
	start = -1;
	len += snprintf(buf + len, PAGE_SIZE - len, "cmdlog filter: %s, cmds:\nnone\n",
		p->conf->cmdlog_filter_en ? "enabled" : "disabled");
	len -= 5;
	for (i=0; i<=count; i++) {
		if (i < count && profiler_cmdlog_filter_list[i]) {
			if (start == -1)
				start = i;
		} else {
			if (start == -1)
				continue;
			if (start == (i - 1))
				len += snprintf(buf + len, PAGE_SIZE - len, "0x%02x\n", start);
			else
				len += snprintf(buf + len, PAGE_SIZE - len, "0x%02x:0x%02x\n", start, (i - 1));
			start = -1;
		}
	}

	return strlen(buf);
}

static ssize_t prop_config_cmdlog_filter_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	char str[1024] = { 0, };
	char *strbuf, *tok;
	int *data;
	int val;
	int i, ret;

	p = &panel->profiler;
	if (p == NULL) {
		profiler_error("%s, profiler is null\n", __func__);
		return -EINVAL;
	}

	if (p->conf == NULL) {
		profiler_error("%s, profiler config is null\n", __func__);
		return -EINVAL;
	}

	data = (int *) p->conf;

	memcpy(str, buf, size < 1024 ? size : 1024);
	strbuf = str;

	//all on
	if (!strncmp(str, "allon", 5)) {
		for (i=0; i<ARRAY_SIZE(profiler_cmdlog_filter_list); i++)
			profiler_cmdlog_filter_list[i] = true;
		return size;
	}
	if (!strncmp(str, "alloff", 6)) {
		for (i=0; i<ARRAY_SIZE(profiler_cmdlog_filter_list); i++)
			profiler_cmdlog_filter_list[i] = false;
		return size;
	}

	while ((tok = strsep(&strbuf, ",")) != NULL) {
		ret = kstrtouint(strim(tok), 0, &val);
		if (ret < 0) {
			profiler_error("%s, invalid value %s, ret %d\n", __func__, tok, ret);
			return ret;
		}
		if (val < 0 || val >= ARRAY_SIZE(profiler_cmdlog_filter_list)) {
			profiler_error("%s, invalid value %s, ret %d\n", __func__, tok, ret);
			continue;
		}
		profiler_cmdlog_filter_list[val] = true;
	}

	return size;
}

struct device_attribute profiler_attrs[] = {
	__PANEL_ATTR_RW(prop_partial, 0660),
	__PANEL_ATTR_RW(prop_config, 0660),
	__PANEL_ATTR_RW(prop_config_mprint, 0660),
	__PANEL_ATTR_RW(prop_config_cmdlog_filter, 0660)
};

int profiler_probe(struct panel_device *panel, struct profiler_tune *tune)
{
	int i;
	int ret = 0;
	struct lcd_device *lcd;
	struct profiler_device *p;

	if (!panel) {
		profiler_error("%s, panel is not exist\n", __func__);
		return -EINVAL;
	}

	if (!tune) {
		profiler_error("%s, tune is null\n", __func__);
		return -EINVAL;
	}

	lcd = panel->lcd;
	if (unlikely(!lcd)) {
		profiler_error("%s, lcd device not exist\n", __func__);
		return -ENODEV;
	}

	if (tune->conf == NULL) {
		profiler_error("%s, profiler config is null\n", __func__);
		return -EINVAL;
	}

	p = &panel->profiler;
	profiler_init_v4l2_subdev(panel);

	p->seqtbl = tune->seqtbl;
	p->nr_seqtbl = tune->nr_seqtbl;
	p->maptbl = tune->maptbl;
	p->nr_maptbl = tune->nr_maptbl;

	for (i = 0; i < p->nr_maptbl; i++) {
		p->maptbl[i].pdata = p;
		maptbl_init(&p->maptbl[i]);
	}

	for (i = 0; i < ARRAY_SIZE(profiler_attrs); i++) {
		ret = device_create_file(&lcd->dev, &profiler_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->dev, "%s, failed to add %s sysfs entries, %d\n",
					__func__, profiler_attrs[i].attr.name, ret);
			return -ENODEV;
		}
	}

	spin_lock_init(&p->te_info.slock);

	p->cmdlog_idx_head = -1;
	p->cmdlog_data_idx = -1;
	p->conf = tune->conf;

// mask config
	p->mask_config = tune->mprint_config;

// mask props
	p->mask_props.conf = p->mask_config;
	p->mask_props.pkts_pos = 0;
	p->mask_props.pkts_size = 0;

	if (p->mask_config->max_len < 2) {
		p->mask_config->max_len = MASK_DATA_SIZE_DEFAULT;
	}

	p->mask_props.data = kmalloc(p->mask_config->max_len, GFP_KERNEL);
	p->mask_props.data_max = p->mask_config->max_len;

	p->mask_props.pkts_max = p->mask_props.data_max / 2;
	p->mask_props.pkts = kmalloc(sizeof(struct mprint_packet) * p->mask_props.pkts_max, GFP_KERNEL);
	if (!p->mask_props.pkts) {
		profiler_error("%s, failed to allocate mask packet buffer\n", __func__);
		goto err;
	}

	p->thread = kthread_run(profiler_thread, p, "profiler");
	if (IS_ERR_OR_NULL(p->thread)) {
		profiler_error("%s, failed to run thread\n", __func__);
		ret = PTR_ERR(p->thread);
		goto err;
	}
	p->initialized = true;
err:
	return ret;
}

