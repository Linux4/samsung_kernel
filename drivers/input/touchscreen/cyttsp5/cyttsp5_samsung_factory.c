/*
 * cyttsp5_samsung_factory.c
 * Cypress TrueTouch(TM) Standard Product V5 Device Access module.
 * Configuration and Test command/status user interface.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/slab.h>
#include <linux/err.h>

#include "cyttsp5_regs.h"

/************************************************************************
 * framework
 ************************************************************************/
typedef enum {
	FACTORYCMD_WAITING,
	FACTORYCMD_RUNNING,
	FACTORYCMD_OK,
	FACTORYCMD_FAIL,
	FACTORYCMD_NOT_APPLICABLE
} FACTORYCMD_STATE;

struct factory_cmd {
	struct list_head	list;
	const char	*cmd_name;
	void	(*cmd_func)(struct cyttsp5_samsung_factory_data* sfd);
};

#define FACTORY_CMD(name, func) .cmd_name = name, .cmd_func = func

struct factory_cmd factory_cmds[];

static void set_cmd_result(struct cyttsp5_samsung_factory_data* sfd,
	char *buff, int len)
{
	strncat(sfd->factory_cmd_result, buff, len);
}

static void set_default_result(struct cyttsp5_samsung_factory_data* sfd)
{
	char delim = ':';

	memset(sfd->factory_cmd_result, 0x00, ARRAY_SIZE(sfd->factory_cmd_result));
	memcpy(sfd->factory_cmd_result, sfd->factory_cmd, strlen(sfd->factory_cmd));
	strncat(sfd->factory_cmd_result, &delim, 1);
}

static void not_support_cmd(struct cyttsp5_samsung_factory_data* sfd)
{
	char buff[16] = {0};

	set_default_result(sfd);
	sprintf(buff, "%s", "NA");
	set_cmd_result(sfd, buff, strnlen(buff, sizeof(buff)));
	sfd->factory_cmd_state = FACTORYCMD_NOT_APPLICABLE;
	dev_info(sfd->dev, "%s: \"%s(%d)\"\n", __func__,
				buff, strnlen(buff, sizeof(buff)));
	return;
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	/*struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_samsung_factory_data *sfd = &cd->sfd;*/
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	//struct device *dev = sfd->dev;
	
	char buff[16] = {0};

	//printk("cyttsp5: show_cmd_status\n");
	//printk("cyttsp5: %s: dev=%p\n", __func__, dev);
	//printk("cyttsp5: %s: sfd->dev=%p\n", __func__, sfd->dev);
	dev_info(sfd->dev, "tsp cmd: status:%d, PAGE_SIZE=%ld\n",
			sfd->factory_cmd_state, PAGE_SIZE);
	//printk("cyttsp5: sfd->num_all_nodes=%d\n", sfd->num_all_nodes);
	
	if (sfd->factory_cmd_state == FACTORYCMD_WAITING)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (sfd->factory_cmd_state == FACTORYCMD_RUNNING)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (sfd->factory_cmd_state == FACTORYCMD_OK)
		snprintf(buff, sizeof(buff), "OK");

	else if (sfd->factory_cmd_state == FACTORYCMD_FAIL)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (sfd->factory_cmd_state == FACTORYCMD_NOT_APPLICABLE)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

	return snprintf(buf, PAGE_SIZE, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
				    *devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	
	dev_info(sfd->dev, "tsp cmd: result: %s\n", sfd->factory_cmd_result);
#if 1
	mutex_lock(&sfd->factory_cmd_lock);
	sfd->factory_cmd_is_running = false;
	mutex_unlock(&sfd->factory_cmd_lock);

	sfd->factory_cmd_state = FACTORYCMD_WAITING;
#endif
	//return 0;
#if 1
	return snprintf(buf, PAGE_SIZE, "%s\n", sfd->factory_cmd_result);
#endif
}

static ssize_t store_cmd(struct device *dev, struct device_attribute
				  *devattr, const char *buf, size_t count)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);

	char *cur, *start, *end;
	char buff[FACTORY_CMD_STR_LEN] = {0};
	int len, i;
	struct factory_cmd *factory_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;
	int ret;

	if (sfd->factory_cmd_is_running == true) {
		dev_err(sfd->dev, "factory_cmd: other cmd is running.\n");
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&sfd->factory_cmd_lock);
	sfd->factory_cmd_is_running = true;
	mutex_unlock(&sfd->factory_cmd_lock);

	sfd->factory_cmd_state = FACTORYCMD_RUNNING;

	for (i = 0; i < ARRAY_SIZE(sfd->factory_cmd_param); i++)
		sfd->factory_cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(sfd->factory_cmd, 0x00, ARRAY_SIZE(sfd->factory_cmd));
	memcpy(sfd->factory_cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(factory_cmd_ptr, &sfd->factory_cmd_list_head, list) {
		if (!strcmp(buff, factory_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	dev_dbg(sfd->dev, "cmd_found=%d\n", cmd_found);
	
	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(factory_cmd_ptr, &sfd->factory_cmd_list_head, list) {
			if (!strcmp("not_support_cmd", factory_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				ret = kstrtoint(buff, 10,\
						sfd->factory_cmd_param + param_cnt);
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(sfd->dev, "cmd = %s\n", factory_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(sfd->dev, "cmd param %d= %d\n", i,
							sfd->factory_cmd_param[i]);

	factory_cmd_ptr->cmd_func(sfd);

err_out:
	return count;
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);

static struct attribute *sec_touch_factory_attributes[] = {
		&dev_attr_cmd.attr,
		&dev_attr_cmd_status.attr,
		&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_factory_attributes,
};

extern struct class *sec_class;

/************************************************************************
 * commands
 ************************************************************************/
static void get_chip_vendor(struct cyttsp5_samsung_factory_data* sfd)
{
//	struct device *dev = sfd->dev;
	char buff[16] = {0};

	dev_dbg(sfd->dev, "%s, %d\n", __func__, __LINE__ );

	set_default_result(sfd);

	snprintf(buff, sizeof(buff), "%s", "Cypress");
	set_cmd_result(sfd, buff, strnlen(buff, sizeof(buff)));
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}


/************************************************************************
 * raw count, diff count, idac
 ************************************************************************/
#define MAX_NODE_NUM (30*30)
#define MAX_INPUT_HEADER_SIZE (12)

enum pwc_data_type_list {
	CY_PWC_MUT
};

static int retrieve_panel_scan(struct cyttsp5_samsung_factory_data* sfd,
							   u8* buf, u8 data_id, int num_all_nodes, u8* r_element_size)
{
	int rc = 0;
	int elem = num_all_nodes; // num elems to read
	int elem_offset = 0;
	u16 actual_read_len;
	u8 config;
	u16 length;
	u8 *buf_offset;
	u8 element_size = 0;

	buf_offset = buf;

	rc = sfd->corecmd->cmd->retrieve_panel_scan(sfd->dev, 0, elem_offset, elem,
			data_id, sfd->input_report_buf, &config, &actual_read_len, NULL);
	if (rc < 0)
		goto end;

	length = get_unaligned_le16(&sfd->input_report_buf[0]); // length of packet
	buf_offset = sfd->input_report_buf + length;

	element_size = config & CY_CMD_RET_PANEL_ELMNT_SZ_MASK;
	*r_element_size = element_size;
	
	elem -= actual_read_len;
	elem_offset = actual_read_len;
	while (elem > 0) {
		rc = sfd->corecmd->cmd->retrieve_panel_scan(sfd->dev, 0, elem_offset, elem,
				data_id, NULL, &config, &actual_read_len, buf_offset);
		if (rc < 0)
			goto end;

		if (!actual_read_len)
			break;

		length += actual_read_len * element_size;
		buf_offset = sfd->input_report_buf + length;
		elem -= actual_read_len;
		elem_offset += actual_read_len;
	}
end:
	return rc;
}

#if 0
static int retrieve_data(struct cyttsp5_samsung_factory_data* sfd,
						 u8* buf, u8 data_id, int num_all_nodes)
{
	int rc = 0;
	int elem = num_all_nodes; // num elems to read
	int elem_offset = 0;
	u16 actual_read_len;
	u8 config;
	u16 length;
	u8 *buf_offset;

	buf_offset = buf;

	rc = sfd->corecmd->cmd->retrieve_data(0, elem_offset, elem,
			data_id, &actual_read_len, sfd->input_report_buf);
	if (rc < 0)
		goto end;

	length = get_unaligned_le16(&sfd->input_report_buf[0]); // length of packet
	buf_offset = sfd->input_report_buf + length;

	elem -= actual_read_len;
	elem_offset = actual_read_len;
	while (elem > 0) {
		rc = sfd->corecmd->cmd->retrieve_data(0, elem_offset, elem,
				data_id, &actual_read_len, buf_offset);
		if (rc < 0)
			goto end;

		if (!actual_read_len)
			break;

		length += actual_read_len;
		buf_offset = sfd->input_report_buf + length;
		elem -= actual_read_len;
		elem_offset += actual_read_len;
	}
end:
	return rc;
}
#endif
static void find_max_min_s16(struct cyttsp5_samsung_factory_data* sfd,
                         u8* buf_offset, int num_all_nodes, u8 element_size)
{
	char buff[16] = {0};
	int i;
	s16 max_value = 0x8000, min_value = 0x7FFF;
	
	for (i = 0 ; i < num_all_nodes ; i++) {
		max_value = max((s16)max_value, (s16)get_unaligned_le16(buf_offset));
		min_value = min((s16)min_value, (s16)get_unaligned_le16(buf_offset));
		buf_offset += element_size;
	}
	snprintf(buff, sizeof(buff), "%d,%d", max_value, min_value);
	set_cmd_result(sfd, buff, strnlen(buff, sizeof(buff)));

	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}
#if 0
static void find_max_min_u8(struct cyttsp5_samsung_factory_data* sfd,
                         u8* buf_offset, int num_all_nodes, u8 element_size)
{
	char buff[16] = {0};
	int i;
	u8 max_value = 0x00, min_value = 0xff;
	
	for (i = 0 ; i < num_all_nodes ; i++) {
		max_value = max((u8)max_value, (u8)(*buf_offset));
		min_value = min((u8)min_value, (u8)(*buf_offset));
		buf_offset += element_size;
	}
	snprintf(buff, sizeof(buff), "%d,%d", max_value, min_value);
	set_cmd_result(sfd, buff, strnlen(buff, sizeof(buff)));

	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}
#endif

static void panel_scan_and_retrieve(struct cyttsp5_samsung_factory_data* sfd,
  								    u8 data_id)
{
	struct device *dev = sfd->dev;
	u8 element_size;
	int rc = 0;
	int rc1 = 0;

	dev_dbg(sfd->dev, "%s, line=%d\n", __func__, __LINE__ );

	set_default_result(sfd);
	
	rc = cyttsp5_request_exclusive(dev, CY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0)
		goto end;

	rc = sfd->corecmd->cmd->suspend_scanning(dev, 0);
	if (rc < 0)
	{
		dev_err(dev, "%s: suspend scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = sfd->corecmd->cmd->exec_panel_scan(dev, 0);
	if (rc < 0)
	{
		dev_err(dev, "%s: exec panel scan failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = retrieve_panel_scan(sfd, sfd->input_report_buf, data_id, sfd->num_all_nodes, &element_size);
	if (rc < 0)
	{
		dev_err(dev, "%s: retrieve_panel_scan failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}
	
	find_max_min_s16(sfd, sfd->input_report_buf + CY_CMD_RET_PANEL_HDR, sfd->num_all_nodes, element_size);

	rc = sfd->corecmd->cmd->resume_scanning(dev, 0);
	if (rc < 0)
		dev_err(dev, "%s: resume_scanning failed r=%d\n",
			__func__, rc);
			
release_exclusive:
	rc1 = cyttsp5_release_exclusive(dev);
	if (rc1 < 0)
		dev_err(dev, "%s: release_exclusive failed r=%d\n",
			__func__, rc);

	//mutex_unlock(&sfd->sysfs_lock);
end:
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_dbg(sfd->dev, "%s, line=%d, rc=%d\n", __func__, __LINE__, rc);
}

static void run_raw_count_read(struct cyttsp5_samsung_factory_data* sfd)
{
	dev_dbg(sfd->dev, "%s\n", __func__);
	panel_scan_and_retrieve(sfd, CY_MUT_RAW);
}

static void run_difference_read(struct cyttsp5_samsung_factory_data* sfd)
{
	dev_dbg(sfd->dev, "%s\n", __func__);
	panel_scan_and_retrieve(sfd, CY_MUT_DIFF);
}

#if 0
static void retrieve_pwc(struct cyttsp5_samsung_factory_data* sfd,
  						 u8 data_id)
{
	struct device *dev = sfd->dev;
	int rc = 0;
	int rc1 = 0;

	dev_dbg(sfd->dev, "%s, %d\n", __func__, __LINE__ );

	set_default_result(sfd);
	
	rc = cyttsp5_request_exclusive(dev, CY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0)
		goto end;

	rc = cyttsp5_request_nonhid_suspend_scanning(0);
	if (rc < 0)
	{
		dev_err(dev, "%s: suspend scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = retrieve_data(sfd, sfd->input_report_buf, data_id, (sfd->num_all_nodes + 1));
	if (rc < 0)
	{
		dev_err(dev, "%s: retrieve_panel_scan failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}
	
	find_max_min_u8(sfd, sfd->input_report_buf + CY_CMD_RET_PANEL_HDR + 1, (sfd->num_all_nodes), 1);

	rc = cyttsp5_request_nonhid_resume_scanning(0);
	if (rc < 0)
		dev_err(dev, "%s: resume_scanning failed r=%d\n",
			__func__, rc);
			
release_exclusive:
	rc1 = cyttsp5_release_exclusive(dev);
	if (rc1 < 0)
		dev_err(dev, "%s: release_exclusive failed r=%d\n",
			__func__, rc);

	//mutex_unlock(&sfd->sysfs_lock);
end:
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_dbg(sfd->dev, "%s, line=%d, rc=%d\n", __func__, __LINE__, rc);
}

static void run_idac_read(struct cyttsp5_samsung_factory_data* sfd)
{
	dev_dbg(sfd->dev, "%s\n", __func__);
	retrieve_pwc(sfd, CY_PWC_MUT);
}
#endif


/************************************************************************
 * cmd table
 ************************************************************************/
struct factory_cmd factory_cmds[] = {
	//{FACTORY_CMD("get_threshold", get_threshold),},
	//{FACTORY_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	//{FACTORY_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{FACTORY_CMD("get_chip_vendor", get_chip_vendor),},
	//{FACTORY_CMD("get_chip_name", get_chip_name),},
	//{FACTORY_CMD("get_x_num", get_x_num),},
	//{FACTORY_CMD("get_y_num", get_y_num),},
	{FACTORY_CMD("run_raw_count_read", run_raw_count_read),},
	{FACTORY_CMD("run_difference_read", run_difference_read),},
	//{FACTORY_CMD("run_idac_read", run_idac_read),},
	{FACTORY_CMD("not_support_cmd", not_support_cmd),},
};


/************************************************************************
 * init
 ************************************************************************/
int cyttsp5_samsung_factory_probe(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_samsung_factory_data *sfd = &cd->sfd;
	int rc = 0;
	int i;

	sfd->dev = dev;

	//printk("cyttsp5: %s: dev=%p\n", __func__, dev);
	
	sfd->corecmd = cyttsp5_get_commands();
	if (!sfd->corecmd)
	{
		dev_err(dev, "%s: core cmd not available\n", __func__);
		rc = -EINVAL;
		goto error_return;
	}

	sfd->si = _cyttsp5_request_sysinfo(dev);
	if (!sfd->si)
	{
		dev_err(dev, "%s: Fail get sysinfo pointer from core\n", __func__);
		rc = -EINVAL;
		goto error_return;
	}

	dev_dbg(dev, "%s: electrodes_x=%d\n", __func__, sfd->si->sensing_conf_data.electrodes_x);
	dev_dbg(dev, "%s: electrodes_y=%d\n", __func__, sfd->si->sensing_conf_data.electrodes_y);
	
	sfd->num_all_nodes = sfd->si->sensing_conf_data.electrodes_x * sfd->si->sensing_conf_data.electrodes_y;
	if (sfd->num_all_nodes > MAX_NODE_NUM)
	{
		dev_err(dev, "%s: sensor node num(%d) exceeds limits\n", __func__, sfd->num_all_nodes);
		rc = -EINVAL;
		goto error_return;
	}
#if 1
	sfd->input_report_buf = kzalloc((MAX_INPUT_HEADER_SIZE + sfd->num_all_nodes * 2), GFP_KERNEL);
	if (sfd->input_report_buf == NULL) {
		dev_err(dev, "%s: Error, kzalloc sfd->input_report_buf\n", __func__);
		rc = -ENOMEM;
		goto error_return;
	}
#endif
	INIT_LIST_HEAD(&sfd->factory_cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(factory_cmds); i++)
	list_add_tail(&factory_cmds[i].list, &sfd->factory_cmd_list_head);

	mutex_init(&sfd->factory_cmd_lock);
	sfd->factory_cmd_is_running = false;

//struct device *device_create(struct class *class, struct device *parent, dev_t devt, void *drvdata, const char *fmt, ...)
	sfd->factory_dev = device_create(sec_class, NULL, 0, sfd, "tsp");
	if (IS_ERR(sfd->factory_dev))
	{
		dev_err(sfd->dev, "Failed to create device for the sysfs\n");
		goto free_buf_and_exit;
	}
	
	rc = sysfs_create_group(&sfd->factory_dev->kobj, &sec_touch_factory_attr_group);
	if (rc)
	{
		dev_err(sfd->dev, "Failed to create sysfs group\n");
		goto device_destroy_and_exit;
	}
	
	sfd->sysfs_nodes_created = 1;

	dev_dbg(sfd->dev, "%s: end, rc=%d\n",__func__, rc);
	return 0;
	
device_destroy_and_exit:
	device_destroy(sec_class, 0);
free_buf_and_exit:
	kfree(sfd->input_report_buf);
error_return:
	dev_err(dev, "%s failed. rc=%d\n", __func__, rc);
	return rc;
}

int cyttsp5_samsung_factory_release(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_samsung_factory_data *sfd = &cd->sfd;

	dev_dbg(dev, "%s:\n",__func__);

	if (sfd->sysfs_nodes_created)
	{
//int sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp)
//void sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp)
		sysfs_remove_group(&sfd->factory_dev->kobj, &sec_touch_factory_attr_group);

//struct device *device_create(struct class *class, struct device *parent, dev_t devt, void *drvdata, const char *fmt, ...)
//void device_destroy(struct class *class, dev_t devt)
		device_destroy(sec_class, 0);
	}

	kfree(sfd->input_report_buf);
	dev_dbg(dev, "%s: end\n",__func__);
	return 0;
}

