// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/time64.h>
#include "panel_drv.h"
#include "panel_firmware.h"
#include "panel_debug.h"
#include "panel_freq_hop.h"
#include "panel_obj.h"
#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
#include <linux/dev_ril_bridge.h>
#endif
#include <linux/sec_panel_notifier_v2.h>
#if defined(CONFIG_USDM_PANEL_JSON)
#include "ezop/json_writer.h"
#include "ezop/panel_json.h"
#endif

#define MAX_NAME_SIZE       32

const char *panel_debugfs_name[] = {
	[PANEL_DEBUGFS_LOG] = "log",
	[PANEL_DEBUGFS_CMD_LOG] = "cmd_log",
	[PANEL_DEBUGFS_SEQUENCE] = "sequence",
	[PANEL_DEBUGFS_RESOURCE] = "resource",
#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
	[PANEL_DEBUGFS_FREQ_HOP] = "freq_hop",
#endif
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	[PANEL_DEBUGFS_PANEL_EVENT] = "panel_event",
#endif
#if defined(CONFIG_USDM_PANEL_JSON)
	[PANEL_DEBUGFS_EZOP] = "ezop",
	[PANEL_DEBUGFS_FIRMWARE] = "firmware",
#endif
};

static int panel_debug_log_show(struct seq_file *s)
{
	seq_printf(s, "%d\n", panel_log_level);

	return 0;
}

static int panel_debug_cmd_log_show(struct seq_file *s)
{
	seq_printf(s, "%d\n", panel_cmd_log);

	return 0;
}

static int panel_debug_sequence_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	struct pnobj *pos;

	list_for_each_entry(pos, &panel->seq_list, list)
		seq_printf(s, "%s\n", get_pnobj_name(pos));

	return 0;
}

static int panel_debug_sequence_store(struct panel_device *panel,
		char *seqname)
{
	struct seqinfo *seq =
		find_panel_seq_by_name(panel, seqname);
	int ret;

	if (seq == NULL) {
		panel_info("sequence(%s) not found\n", seqname);
		return 0;
	}

	ret = panel_do_seqtbl_by_name(panel, seqname);
	if (ret < 0) {
		panel_err("failed to execute sequence(%s)\n", seqname);
		return ret;
	}

	return 0;
}

static int panel_debug_resource_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	struct pnobj *pos;

	list_for_each_entry(pos, &panel->res_list, list)
		seq_printf(s, "%s\n", get_pnobj_name(pos));

	return 0;
}

static int panel_debug_resource_store(struct panel_device *panel,
		char *resname)
{
	struct resinfo *resource =
		find_panel_resource(panel, resname);
	char buf[SZ_512];

	if (resource == NULL) {
		panel_info("resource(%s) not found\n", resname);
		return 0;
	}

	if (!is_valid_resource(resource)) {
		panel_warn("invalid resource(%s)\n", resname);
		return 0;
	}

	snprintf_resource(buf, ARRAY_SIZE(buf), resource);
	panel_info("\n%s", buf);

	return 0;
}

#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
static int panel_debug_freq_hop_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	struct panel_freq_hop *freq_hop = &panel->freq_hop;
	struct freq_hop_elem *elem;
	char buf[SZ_256] = { 0, };
	int i = 0;

	list_for_each_entry(elem, &freq_hop->head, list) {
		snprintf_freq_hop_elem(buf, SZ_256, elem);
		seq_printf(s, "%d: %s\n", i++, buf);
	}

	return 0;
}

static int panel_debug_fake_radio_noti(struct panel_device *panel, char *buf)
{
	struct dev_ril_bridge_msg msg;
	struct ril_noti_info rf_info;
	int rc;

	/*
	 * how to use
	 *
	 * tb74 { id = <N008>; range = <185000 188880>; dsi_freq = <1108000>; osc_freq = <1813000>; };
	 * tb75 { id = <N008>; range = <188881 191680>; dsi_freq = <1124000>; osc_freq = <1813000>; };
	 * tb76 { id = <N008>; range = <191681 191980>; dsi_freq = <1125000>; osc_freq = <1813000>; };
	 * #define N008		263
	 *
	 * echo 0 263 185000 > /d/panel/freq_hop
	 * echo 0 263 188881 > /d/panel/freq_hop
	 * echo 0 263 191681 > /d/panel/freq_hop
	 */

	if (!buf)
		return -EINVAL;

	rc = sscanf(buf, "%hhu %u %u",
			&rf_info.rat, &rf_info.band, &rf_info.channel);
	if (rc < 3) {
		panel_err("check your input. rc:(%d)\n", rc);
		return rc;
	}

	msg.dev_id = IPC_SYSTEM_CP_CHANNEL_INFO;
	msg.data_len = sizeof(struct ril_noti_info);
	msg.data = (void *)&rf_info;

	panel_info("radio_noti rat:%d band:%d channel:%d\n",
			rf_info.rat, rf_info.band, rf_info.channel);

	radio_notifier(&panel->freq_hop.radio_noti, 0, &msg);

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
static int panel_debug_panel_event_show(struct seq_file *s)
{
	seq_printf(s, "echo <event> <state> > /d/panel/panel_event\n");

	return 0;
}

static int panel_debug_panel_event_noti(struct panel_device *panel, char *buf)
{
	int rc, event, state;
	struct panel_notifier_event_data evt_data = {
		.display_index = 0U,
	};

	rc = sscanf(buf, "%d %d", &event, &state);
	if (rc < 2) {
		panel_err("check your input. rc:(%d)\n", rc);
		return rc;
	}

	if (event < 0 || event >= MAX_PANEL_EVENT) {
		panel_err("invalid panel event(%d)\n", event);
		return -EINVAL;
	}

	if (state < PANEL_EVENT_STATE_NONE ||
		event >= MAX_PANEL_EVENT_STATE) {
		panel_err("invalid panel event state(%d)\n", state);
		return -EINVAL;
	}
	evt_data.state = state;

	panel_notifier_call_chain(event, &evt_data);
	panel_info("event:%d state:%d\n", event, evt_data.state);

	return 0;
}
#endif

#if defined(CONFIG_USDM_PANEL_JSON)
char *json_buffer;
int panel_debug_ezop_jsonw_all(struct panel_device *panel, struct seq_file *s)
{
	json_writer_t *w;
	char *p;

	json_buffer = kvmalloc(SZ_16M, GFP_KERNEL);
	if (!json_buffer) {
		panel_err("failed to alloc json buffer\n");
		return -ENOMEM;
	}

	w = jsonw_new(json_buffer, SZ_16M);
	if (!w) {
		panel_err("failed to jsonw_new\n");
		return -EINVAL;
	}

	jsonw_pretty(w, true);
	jsonw_start_object(w);
	jsonw_function_list(w, &panel->func_list);
	jsonw_maptbl_list(w, &panel->maptbl_list);
	jsonw_delay_list(w, &panel->dly_list);
	jsonw_condition_list(w, &panel->cond_list);
	jsonw_power_ctrl_list(w, &panel->pwrctrl_list);
	jsonw_property_list(w, &panel->prop_list);
	jsonw_rx_packet_list(w, &panel->rdi_list);
	jsonw_tx_packet_list(w, &panel->pkt_list);
	jsonw_key_list(w, &panel->key_list);
	jsonw_resource_list(w, &panel->res_list);
	jsonw_dumpinfo_list(w, &panel->dump_list);
	jsonw_sequence_list(w, &panel->seq_list);
	jsonw_end_object(w);

	pr_info("dump json done\n");

	while ((p = strsep(&json_buffer, "\n")) != NULL) {
		if (!*p)
			continue;

		seq_printf(s, "%s\n", p);
	}

	jsonw_destroy(&w);
	kvfree(json_buffer);

	return 0;
}

static int panel_debug_ezop_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	int ret;

	ret = panel_debug_ezop_jsonw_all(panel, s);
	if (ret < 0)
		return 0;

	return 0;
}

static int panel_debug_ezop_load_json(struct panel_device *panel,
		char *json_filename)
{
	int ret;
	struct list_head new_pnobj_list;

	INIT_LIST_HEAD(&new_pnobj_list);

	ret = panel_firmware_load(panel, json_filename, &new_pnobj_list);
	if (ret != 0) {
		panel_info("firmware not exist\n");
		return 0;
	}

	return panel_reprobe_with_pnobj_list(panel, &new_pnobj_list);
}

static int panel_debug_firmware_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	struct timespec64 cur_ts, load_ts, delta_ts;
	u64 load_time_ms, elapsed_ms;

	ktime_get_ts64(&cur_ts);
	load_ts = panel_firmware_get_load_time(panel);
	load_time_ms = timespec64_to_ns(&cur_ts) / 1000000;
	delta_ts = timespec64_sub(cur_ts, load_ts);
	elapsed_ms = timespec64_to_ns(&delta_ts) / 1000000;

	seq_printf(s, "file: %s\n", panel_firmware_get_name(panel));
	seq_printf(s, "time: %lld.%lld sec (%lld.%lld sec ago)\n",
			load_time_ms / 1000UL, load_time_ms % 1000UL / 100UL,
			elapsed_ms / 1000UL, elapsed_ms % 1000UL / 100UL);
	seq_printf(s, "count: %d\n", panel_firmware_get_load_count(panel));
	seq_printf(s, "status: %s\n", is_panel_firmwarel_load_success(panel) ? "OK" : "NOK");
	seq_printf(s, "checksum: 0x%08llx\n", panel_firmware_get_csum(panel));

	return 0;
}
#endif

static int panel_debug_simple_show(struct seq_file *s, void *unused)
{
	struct panel_debugfs *debugfs = s->private;

	switch (debugfs->id) {
	case PANEL_DEBUGFS_LOG:
		panel_debug_log_show(s);
		break;
	case PANEL_DEBUGFS_CMD_LOG:
		panel_debug_cmd_log_show(s);
		break;
	case PANEL_DEBUGFS_SEQUENCE:
		panel_debug_sequence_show(s);
		break;
	case PANEL_DEBUGFS_RESOURCE:
		panel_debug_resource_show(s);
		break;
#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
	case PANEL_DEBUGFS_FREQ_HOP:
		panel_debug_freq_hop_show(s);
		break;
#endif
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	case PANEL_DEBUGFS_PANEL_EVENT:
		panel_debug_panel_event_show(s);
		break;
#endif
#if defined(CONFIG_USDM_PANEL_JSON)
	case PANEL_DEBUGFS_EZOP:
		panel_debug_ezop_show(s);
		break;
	case PANEL_DEBUGFS_FIRMWARE:
		panel_debug_firmware_show(s);
		break;
#endif
	default:
		break;
	}

	return 0;
}

static ssize_t panel_debug_simple_write(struct file *file,
		const char __user *buf, size_t count, loff_t *f_ops)
{
	struct seq_file *s;
	struct panel_debugfs *debugfs;
	struct panel_device *panel;
	char argbuf[SZ_128];
	int rc = 0;
	int res = 0;

	s = file->private_data;
	debugfs = s->private;
	panel = debugfs->private;

	switch (debugfs->id) {
	case PANEL_DEBUGFS_LOG:
		rc = kstrtoint_from_user(buf, count, 10, &res);
		if (rc)
			return rc;

		panel_log_level = res;
		panel_info("panel_log_level: %d\n", panel_log_level);
		break;
	case PANEL_DEBUGFS_CMD_LOG:
		rc = kstrtoint_from_user(buf, count, 10, &res);
		if (rc)
			return rc;

		panel_cmd_log = res;
		panel_info("panel_cmd_log: %d\n", panel_cmd_log);
		break;
	case PANEL_DEBUGFS_SEQUENCE:
		if (count >= ARRAY_SIZE(argbuf))
			return -EINVAL;

		if (copy_from_user(argbuf, buf, count))
			return -EOVERFLOW;

		argbuf[count] = '\0';
		rc = panel_debug_sequence_store(panel, strstrip(argbuf));
		if (rc)
			return rc;

		break;
	case PANEL_DEBUGFS_RESOURCE:
		if (count >= ARRAY_SIZE(argbuf))
			return -EINVAL;

		if (copy_from_user(argbuf, buf, count))
			return -EOVERFLOW;

		argbuf[count] = '\0';
		rc = panel_debug_resource_store(panel, strstrip(argbuf));
		if (rc)
			return rc;

		break;
#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
	case PANEL_DEBUGFS_FREQ_HOP:
		if (count >= ARRAY_SIZE(argbuf))
			return -EINVAL;

		if (copy_from_user(argbuf, buf, count))
			return -EOVERFLOW;

		argbuf[count] = '\0';
		rc = panel_debug_fake_radio_noti(panel, strstrip(argbuf));
		if (rc)
			return rc;

		break;
#endif
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	case PANEL_DEBUGFS_PANEL_EVENT:
		if (count >= ARRAY_SIZE(argbuf))
			return -EINVAL;

		if (copy_from_user(argbuf, buf, count))
			return -EOVERFLOW;

		argbuf[count] = '\0';
		rc = panel_debug_panel_event_noti(panel, strstrip(argbuf));
		if (rc)
			return rc;

		break;
#endif
#if defined(CONFIG_USDM_PANEL_JSON)
	case PANEL_DEBUGFS_EZOP:
		if (count >= ARRAY_SIZE(argbuf))
			return -EINVAL;

		if (copy_from_user(argbuf, buf, count))
			return -EOVERFLOW;

		argbuf[count] = '\0';
		rc = panel_debug_ezop_load_json(panel, strstrip(argbuf));
		if (rc)
			return rc;

		break;
#endif
	default:
		break;
	}

	return count;
}

static int panel_debug_simple_open(struct inode *inode, struct file *file)
{
	return single_open(file, panel_debug_simple_show, inode->i_private);
}

static const struct file_operations panel_debugfs_simple_fops = {
	.open = panel_debug_simple_open,
	.write = panel_debug_simple_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int panel_create_debugfs(struct panel_device *panel)
{
	struct panel_debugfs *debugfs;
	char name[MAX_NAME_SIZE];
	int i;

	if (!panel)
		return -EINVAL;

	if (panel->id == 0)
		snprintf(name, MAX_NAME_SIZE, "panel");
	else
		snprintf(name, MAX_NAME_SIZE, "panel%d", panel->id);

	panel->d.dir = debugfs_create_dir(name, NULL);
	if (!panel->d.dir) {
		panel_err("failed to create debugfs directory(%s).\n", name);
		return -ENOENT;
	}

	for (i = 0; i < MAX_PANEL_DEBUGFS; i++) {
		debugfs = kmalloc(sizeof(struct panel_debugfs), GFP_KERNEL);
		if (!debugfs)
			return -ENOMEM;

		debugfs->private = panel;
		debugfs->id = i;
		debugfs->file = debugfs_create_file(panel_debugfs_name[i], 0660,
				panel->d.dir, debugfs, &panel_debugfs_simple_fops);
		if (!debugfs->file) {
			panel_err("failed to create debugfs file(%s)\n",
					panel_debugfs_name[i]);
			kfree(debugfs);
			return -ENOMEM;
		}
		panel->d.debugfs[i] = debugfs;
	}

	return 0;
}

void panel_destroy_debugfs(struct panel_device *panel)
{
	int i;

	debugfs_remove_recursive(panel->d.dir);
	for (i = 0; i < MAX_PANEL_DEBUGFS; i++) {
		kfree(panel->d.debugfs[i]);
		panel->d.debugfs[i] = NULL;
	}
}
