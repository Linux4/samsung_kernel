/*
 * sec_cmd.c - samsung input command driver
 *
 * Copyright (C) 2014 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "sec_cmd.h"
#include "touch_feature.h"

/*M55 code for SR-QN6887A-01-400 by yuli at 202300824 start*/
#ifdef CONFIG_ARCH_X55
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>

extern int get_pwrkey_state(void);
extern int get_volumedown_state(void);
struct device *sec_key;
#define DEVT 230

int  get_volumeup_state(void)
{
    struct device_node *volup_np = NULL;
    struct gpio_desc *vol_up_gpiod = NULL;
    int key_vol_up = 0;
    int state = 0;

    volup_np = of_find_compatible_node(NULL, NULL, "gpio-keys");
    if (!volup_np) {
         pr_err("%s %s: could not get gpio-keys\n", SECLOG, __func__);
         goto err_end;
    }

    key_vol_up = of_get_named_gpio(volup_np->child, "gpios", 0);
    if (key_vol_up < 0) {
         pr_err("%s %s: could not get key_vol_up gpio\n", SECLOG, __func__);
         goto err_end;
    }

    vol_up_gpiod = gpio_to_desc(key_vol_up);
    if (!vol_up_gpiod) {
         pr_err("%s %s: gpio_to_desc fail\n", SECLOG, __func__);
         goto err_end;
    }

    state = gpiod_get_value_cansleep(vol_up_gpiod);
    if (state < 0) {
        pr_err("%s %s: failed to get gpio state: %d\n", SECLOG, __func__, state);
         goto err_end;
    }

    if (state) {
        return 1;
    }

err_end:
    return 0;
}
#endif    // CONFIG_ARCH_X55
/*M55 code for SR-QN6887A-01-400 by yuli at 202300824 end*/

bool g_tp_is_enabled = true;

struct class *sec_class;

static void sec_cmd_store_function(struct sec_cmd_data *data);

void sec_cmd_execution(struct sec_cmd_data *data, bool lock)
{
    if (lock)
        mutex_lock(&data->fs_lock);

    /* check lock    */
    mutex_lock(&data->cmd_lock);
    data->cmd_is_running = true;
    mutex_unlock(&data->cmd_lock);

    data->cmd_state = SEC_CMD_STATUS_RUNNING;
    sec_cmd_store_function(data);

    if (lock)
        mutex_unlock(&data->fs_lock);
}

void sec_cmd_set_cmd_exit(struct sec_cmd_data *data)
{
    mutex_lock(&data->fifo_lock);
    if (kfifo_len(&data->cmd_queue)) {
        pr_info("%s: %s %s: do next cmd, left cmd[%d]\n", dev_name(data->fac_dev), SECLOG, __func__,
            (int)(kfifo_len(&data->cmd_queue) / sizeof(struct command)));
        mutex_unlock(&data->fifo_lock);
        schedule_work(&data->cmd_work.work);
    } else {
        mutex_unlock(&data->fifo_lock);

        mutex_lock(&data->cmd_lock);
        data->cmd_is_running = false;
        mutex_unlock(&data->cmd_lock);
    }
    if (data->wait_cmd_result_done)
        complete_all(&data->cmd_result_done);
}
EXPORT_SYMBOL(sec_cmd_set_cmd_exit);

static void cmd_exit_work(struct work_struct *work)
{
    struct sec_cmd_data *data = container_of(work, struct sec_cmd_data, cmd_work.work);

    sec_cmd_execution(data, true);
}


void sec_cmd_set_default_result(struct sec_cmd_data *data)
{
    char *delim = ":";
    memset(data->cmd_result, 0x00, SEC_CMD_RESULT_STR_LEN_EXPAND);
    memcpy(data->cmd_result, data->cmd, SEC_CMD_STR_LEN);
    strlcat(data->cmd_result, delim, SEC_CMD_RESULT_STR_LEN_EXPAND);
}
EXPORT_SYMBOL(sec_cmd_set_default_result);

void sec_cmd_set_cmd_result_all(struct sec_cmd_data *data, char *buff, int len, char *item)
{
    char *delim1 = " ";
    char *delim2 = ":";
    int cmd_result_len;

    cmd_result_len = (int)strlen(data->cmd_result_all) + len + 2 + (int)strlen(item);

    if (cmd_result_len >= SEC_CMD_RESULT_STR_LEN) {
        pr_err("%s: %s %s: cmd length is over (%d)!!", dev_name(data->fac_dev), SECLOG, __func__, cmd_result_len);
        return;
    }

    data->item_count++;
    strlcat(data->cmd_result_all, delim1, sizeof(data->cmd_result_all));
    strlcat(data->cmd_result_all, item, sizeof(data->cmd_result_all));
    strlcat(data->cmd_result_all, delim2, sizeof(data->cmd_result_all));
    strlcat(data->cmd_result_all, buff, sizeof(data->cmd_result_all));
}
EXPORT_SYMBOL(sec_cmd_set_cmd_result_all);

void sec_cmd_set_cmd_result(struct sec_cmd_data *data, char *buff, int len)
{
    if (strlen(buff) >= (unsigned int)SEC_CMD_RESULT_STR_LEN_EXPAND) {
        pr_err("%s %s: cmd length is over (%d)!!", SECLOG, __func__, (int)strlen(buff));
        strlcat(data->cmd_result, "NG", SEC_CMD_RESULT_STR_LEN_EXPAND);
        return;
    }

    data->cmd_result_expand = (int)strlen(buff) / SEC_CMD_RESULT_STR_LEN;
    data->cmd_result_expand_count = 0;

    strlcat(data->cmd_result, buff, SEC_CMD_RESULT_STR_LEN_EXPAND);
}
EXPORT_SYMBOL(sec_cmd_set_cmd_result);

static void sec_cmd_store_function(struct sec_cmd_data *data)
{
    char *cur, *start, *end;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    int len, i;
    struct sec_cmd *sec_cmd_ptr = NULL;
    char delim = ',';
    bool cmd_found = false;
    int param_cnt = 0;
    int ret;
    const char *buf;
    size_t count;
    struct command cmd = {{0}};

    if (!data) {
        pr_err("%s %s: No platform data found\n", SECLOG, __func__);
        return;
    }

    mutex_lock(&data->fifo_lock);
    if (kfifo_len(&data->cmd_queue)) {
        ret = kfifo_out(&data->cmd_queue, &cmd, sizeof(struct command));
        if (!ret) {
            pr_err("%s: %s %s: kfifo_out failed, it seems empty, ret=%d\n", dev_name(data->fac_dev), SECLOG, __func__, ret);
            mutex_unlock(&data->fifo_lock);
            return;
        }
    } else {
        pr_err("%s: %s %s: left cmd is nothing\n", dev_name(data->fac_dev), SECLOG, __func__);
        mutex_unlock(&data->fifo_lock);
        mutex_lock(&data->cmd_lock);
        data->cmd_is_running = false;
        mutex_unlock(&data->cmd_lock);
        return;
    }
    mutex_unlock(&data->fifo_lock);

    buf = cmd.cmd;
    count = strlen(buf);

    for (i = 0; i < (int)ARRAY_SIZE(data->cmd_param); i++)
        data->cmd_param[i] = 0;

    len = (int)count;
    if (*(buf + len - 1) == '\n')
        len--;

    memset(data->cmd, 0x00, ARRAY_SIZE(data->cmd));
    memcpy(data->cmd, buf, len);

    cur = strchr(buf, (int)delim);
    if (cur)
        memcpy(buff, buf, cur - buf);
    else
        memcpy(buff, buf, len);

    pr_debug("%s: %s %s: COMMAND : %s\n", dev_name(data->fac_dev), SECLOG, __func__, buff);

    /* find command */
    list_for_each_entry(sec_cmd_ptr, &data->cmd_list_head, list) {
        if (!strncmp(buff, sec_cmd_ptr->cmd_name, SEC_CMD_STR_LEN)) {
            cmd_found = true;
            break;
        }
    }

    /* set not_support_cmd */
    if (!cmd_found) {
        list_for_each_entry(sec_cmd_ptr, &data->cmd_list_head, list) {
            if (!strncmp("not_support_cmd", sec_cmd_ptr->cmd_name,
                SEC_CMD_STR_LEN))
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
                *(buff + strnlen(buff, ARRAY_SIZE(buff))) = '\0';
                if (kstrtoint(buff, 10, data->cmd_param + param_cnt) < 0)
                    return;
                start = cur + 1;
                memset(buff, 0x00, ARRAY_SIZE(buff));
                param_cnt++;
            }
            cur++;
        } while ((cur - buf <= len) && (param_cnt < SEC_CMD_PARAM_NUM));
    }

    if (cmd_found) {
        pr_info("%s: %s %s: cmd = %s", dev_name(data->fac_dev), SECLOG, __func__, sec_cmd_ptr->cmd_name);
        for (i = 0; i < param_cnt; i++) {
            if (i == 0)
                pr_cont(" param =");
            pr_cont(" %d", data->cmd_param[i]);
        }
        pr_cont("\n");
    } else {
        pr_info("%s: %s %s: cmd = %s(%s)\n", dev_name(data->fac_dev), SECLOG, __func__, buff, sec_cmd_ptr->cmd_name);
    }

    sec_cmd_ptr->cmd_func(data);

    if (cmd_found && sec_cmd_ptr->cmd_log) {
        char tbuf[32];
        unsigned long long t;
        unsigned long nanosec_rem;

        memset(tbuf, 0x00, sizeof(tbuf));
        t = local_clock();
        nanosec_rem = do_div(t, 1000000000);
        snprintf(tbuf, sizeof(tbuf), "[r:%lu.%06lu]",
                (unsigned long)t,
                nanosec_rem / 1000);
    }
}

static ssize_t sec_cmd_store(struct device *dev, struct device_attribute *devattr,
               const char *buf, size_t count)
{
    struct sec_cmd_data *data = dev_get_drvdata(dev);
    struct command cmd = {{0}};
    struct sec_cmd *sec_cmd_ptr = NULL;
    int queue_size;

    if (!data) {
        pr_err("%s %s: No platform data found\n", SECLOG, __func__);
        return -EINVAL;
    }

    if (strnlen(buf, SEC_CMD_STR_LEN) >= SEC_CMD_STR_LEN) {
        pr_err("%s: %s %s: cmd length(strlen(buf)) is over (%d,%s)!!\n",
                dev_name(data->fac_dev), SECLOG, __func__, (int)strlen(buf), buf);
        return -EINVAL;
    }

    if (count >= (unsigned int)SEC_CMD_STR_LEN) {
        pr_err("%s: %s %s: cmd length(count) is over (%d,%s)!!\n",
                dev_name(data->fac_dev), SECLOG, __func__, (unsigned int)count, buf);
        return -EINVAL;
    }

    if (strnlen(buf, SEC_CMD_STR_LEN) == 0) {
        pr_err("%s: %s %s: cmd length is zero (%d,%s) count(%ld)!!\n",
                dev_name(data->fac_dev), SECLOG, __func__, (int)strlen(buf), buf, count);
        return -EINVAL;
    }

    strncpy(cmd.cmd, buf, count);

    if (data->wait_cmd_result_done) {
        int ret;

        mutex_lock(&data->wait_lock);
        if (!data->cmd_result_done.done)
            pr_info("%s: %s %s: %s - waiting prev cmd...\n", dev_name(data->fac_dev), SECLOG, __func__, cmd.cmd);
        ret = wait_for_completion_interruptible_timeout(&data->cmd_result_done, msecs_to_jiffies(2000));
        if (ret <= 0)
            pr_err("%s: %s %s: completion %d\n", dev_name(data->fac_dev), SECLOG, __func__, ret);

        reinit_completion(&data->cmd_result_done);
        mutex_unlock(&data->wait_lock);
    }

    list_for_each_entry(sec_cmd_ptr, &data->cmd_list_head, list) {
        if (!strncmp(cmd.cmd, sec_cmd_ptr->cmd_name, strlen(sec_cmd_ptr->cmd_name))) {
            if (sec_cmd_ptr->cmd_log) {
                char task_info[40];
                char tbuf[32];
                unsigned long long t;
                unsigned long nanosec_rem;

                memset(tbuf, 0x00, sizeof(tbuf));
                t = local_clock();
                nanosec_rem = do_div(t, 1000000000);
                snprintf(tbuf, sizeof(tbuf), "[q:%lu.%06lu]",
                        (unsigned long)t,
                        nanosec_rem / 1000);

                snprintf(task_info, 40, "\n[%d:%s:%s]", current->pid, current->comm, dev_name(data->fac_dev));
            }
            break;
        }
    }

    mutex_lock(&data->fifo_lock);
    queue_size = (kfifo_len(&data->cmd_queue) / sizeof(struct command));

    if (kfifo_avail(&data->cmd_queue) && (queue_size < SEC_CMD_MAX_QUEUE)) {
        kfifo_in(&data->cmd_queue, &cmd, sizeof(struct command));
        pr_info("%s: %s %s: push cmd: %s\n", dev_name(data->fac_dev), SECLOG, __func__, cmd.cmd);
    } else {
        pr_err("%s: %s %s: cmd_queue is full!!\n", dev_name(data->fac_dev), SECLOG, __func__);

        kfifo_reset(&data->cmd_queue);
        pr_err("%s: %s %s: cmd_queue is reset!!\n", dev_name(data->fac_dev), SECLOG, __func__);
        mutex_unlock(&data->fifo_lock);

        mutex_lock(&data->cmd_lock);
        data->cmd_is_running = false;
        mutex_unlock(&data->cmd_lock);

        if (data->wait_cmd_result_done)
            complete_all(&data->cmd_result_done);

        return -ENOSPC;
    }

    if (data->cmd_is_running == true) {
        pr_err("%s: %s %s: other cmd is running. Wait until previous cmd is done[%d]\n",
            dev_name(data->fac_dev), SECLOG, __func__, (int)(kfifo_len(&data->cmd_queue) / sizeof(struct command)));
        mutex_unlock(&data->fifo_lock);
        return count;
    }
    mutex_unlock(&data->fifo_lock);

    sec_cmd_execution(data, true);
    return count;
}

static ssize_t sec_cmd_show_status(struct device *dev,
                 struct device_attribute *devattr, char *buf)
{
    struct sec_cmd_data *data = dev_get_drvdata(dev);
    char buff[16] = { 0 };

    if (!data) {
        pr_err("%s %s: No platform data found\n", SECLOG, __func__);
        return -EINVAL;
    }

    if (data->cmd_state == SEC_CMD_STATUS_WAITING)
        snprintf(buff, sizeof(buff), "WAITING");

    else if (data->cmd_state == SEC_CMD_STATUS_RUNNING)
        snprintf(buff, sizeof(buff), "RUNNING");

    else if (data->cmd_state == SEC_CMD_STATUS_OK)
        snprintf(buff, sizeof(buff), "OK");

    else if (data->cmd_state == SEC_CMD_STATUS_FAIL)
        snprintf(buff, sizeof(buff), "FAIL");

    else if (data->cmd_state == SEC_CMD_STATUS_EXPAND)
        snprintf(buff, sizeof(buff), "EXPAND");

    else if (data->cmd_state == SEC_CMD_STATUS_NOT_APPLICABLE)
        snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

    pr_debug("%s: %s %s: %d, %s\n", dev_name(data->fac_dev), SECLOG, __func__, data->cmd_state, buff);

    return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);
}

static ssize_t sec_cmd_show_status_all(struct device *dev,
                 struct device_attribute *devattr, char *buf)
{
    struct sec_cmd_data *data = dev_get_drvdata(dev);
    char buff[16] = { 0 };

    if (!data) {
        pr_err("%s %s: No platform data found\n", SECLOG, __func__);
        return -EINVAL;
    }

    if (data->cmd_all_factory_state == SEC_CMD_STATUS_WAITING)
        snprintf(buff, sizeof(buff), "WAITING");

    else if (data->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
        snprintf(buff, sizeof(buff), "RUNNING");

    else if (data->cmd_all_factory_state == SEC_CMD_STATUS_OK)
        snprintf(buff, sizeof(buff), "OK");

    else if (data->cmd_all_factory_state == SEC_CMD_STATUS_FAIL)
        snprintf(buff, sizeof(buff), "FAIL");

    else if (data->cmd_state == SEC_CMD_STATUS_EXPAND)
        snprintf(buff, sizeof(buff), "EXPAND");

    else if (data->cmd_all_factory_state == SEC_CMD_STATUS_NOT_APPLICABLE)
        snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

    pr_debug("%s: %s %s: %d, %s\n", dev_name(data->fac_dev), SECLOG, __func__, data->cmd_all_factory_state, buff);

    return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);
}

static ssize_t sec_cmd_show_result(struct device *dev,
                 struct device_attribute *devattr, char *buf)
{
    struct sec_cmd_data *data = dev_get_drvdata(dev);
    int size;

    if (!data) {
        pr_err("%s %s: No platform data found\n", SECLOG, __func__);
        return -EINVAL;
    }

    size = snprintf(buf, SEC_CMD_RESULT_STR_LEN, "%s\n",
        data->cmd_result + (SEC_CMD_RESULT_STR_LEN - 1) * data->cmd_result_expand_count);

    if (data->cmd_result_expand_count != data->cmd_result_expand) {
        data->cmd_state = SEC_CMD_STATUS_EXPAND;
        data->cmd_result_expand_count++;
    } else {
        data->cmd_state = SEC_CMD_STATUS_WAITING;
    }

    pr_info("%s: %s %s: %s\n", dev_name(data->fac_dev), SECLOG, __func__, buf);

    sec_cmd_set_cmd_exit(data);

    return size;
}

static ssize_t sec_cmd_show_result_all(struct device *dev,
                 struct device_attribute *devattr, char *buf)
{
    struct sec_cmd_data *data = dev_get_drvdata(dev);
    int size;

    if (!data) {
        pr_err("%s %s: No platform data found\n", SECLOG, __func__);
        return -EINVAL;
    }

    data->cmd_state = SEC_CMD_STATUS_WAITING;
    pr_info("%s: %s %s: %d, %s\n", dev_name(data->fac_dev), SECLOG, __func__, data->item_count, data->cmd_result_all);
    size = snprintf(buf, SEC_CMD_RESULT_STR_LEN, "%d%s\n", data->item_count, data->cmd_result_all);

    sec_cmd_set_cmd_exit(data);

    data->item_count = 0;
    memset(data->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

    return size;
}

static ssize_t sec_cmd_list_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct sec_cmd_data *data = dev_get_drvdata(dev);
    struct sec_cmd *sec_cmd_ptr = NULL;
    char *buffer;
    char buffer_name[SEC_CMD_STR_LEN];
    int ret = 0;

    buffer = kzalloc(data->cmd_buffer_size + 30, GFP_KERNEL);
    if (!buffer)
        return -ENOMEM;

    snprintf(buffer, 30, "++factory command list++\n");

    list_for_each_entry(sec_cmd_ptr, &data->cmd_list_head, list) {
        if (strncmp(sec_cmd_ptr->cmd_name, "not_support_cmd", 15)) {
            snprintf(buffer_name, SEC_CMD_STR_LEN, "%s\n", sec_cmd_ptr->cmd_name);
            strlcat(buffer, buffer_name, data->cmd_buffer_size + 30);
        }
    }

    ret = snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buffer);
    kfree(buffer);

    return ret;
}


static ssize_t support_feature_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf,"%x\n", INPUT_FEATURE_ENABLE_SETTINGS_AOT);
}

/*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
extern  struct lcm_info *gs_common_lcm_info;

static ssize_t scrub_pos_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    char buff[SEC_CMD_STR_LEN] = { 0 };

    /*M55 code for P231023-06212 by yuli at 20231024 start*/
    if (!gs_common_lcm_info) {
        pr_info("%s %s: gs_common_lcm_info is NULL", SECLOG, __func__);
        return snprintf(buf, PAGE_SIZE, "%s", "NA\n");
    }

    if (!gs_common_lcm_info->ts_data_info) {
        pr_info("%s %s: ts_data_info is NULL", SECLOG, __func__);
        return snprintf(buf, PAGE_SIZE, "%s", "NA\n");
    }
   /*M55 code for P231023-06212 by yuli at 20231024 end*/

    pr_info("%s %s: id: %d, X:%d, Y:%d\n", SECLOG, __func__, gs_common_lcm_info->ts_data_info->scrub_id,
        gs_common_lcm_info->ts_data_info->scrub_x, gs_common_lcm_info->ts_data_info->scrub_y);
    snprintf(buff, sizeof(buff), "%d %d %d\n", gs_common_lcm_info->ts_data_info->scrub_id,
            gs_common_lcm_info->ts_data_info->scrub_x, gs_common_lcm_info->ts_data_info->scrub_y);

    gs_common_lcm_info->ts_data_info->scrub_x = 0;
    gs_common_lcm_info->ts_data_info->scrub_y = 0;

    return snprintf(buf, PAGE_SIZE, "%s", buff);
}

static ssize_t fod_position_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int i = 0;
    char buff[3] = { 0 };

    pr_info("%s %s\n", SECLOG, __func__);
    if (!gs_common_lcm_info) {
        pr_info("%s %s: gs_common_lcm_info is NULL", SECLOG, __func__);
        goto NA;
    }

    if (gs_common_lcm_info->get_fod_pos != NULL) {
        gs_common_lcm_info->get_fod_pos();
    } else {
        pr_info("%s %s: get_fod_pos is NULL\n", SECLOG, __func__);
        goto NA;
    }

    if (!gs_common_lcm_info->ts_data_info->fod_info_bytes) {
        pr_info("%s %s: not read fod_info yet\n", SECLOG, __func__);
        goto NA;
    }

    for (i = 0; i < gs_common_lcm_info->ts_data_info->fod_info_bytes; i++) {
        snprintf(buff, 3, "%02X", gs_common_lcm_info->ts_data_info->fod_pos[i]);
        strlcat(buf, buff, SEC_CMD_BUF_SIZE);
    }

    return strlen(buf);
NA:
    return snprintf(buf, SEC_CMD_BUF_SIZE, "NA\n");
}

static ssize_t fod_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    pr_info("%s %s\n", SECLOG, __func__);

    if (!gs_common_lcm_info) {
        pr_info("%s %s: gs_common_lcm_info is NULL", SECLOG, __func__);
        goto NA;
    }

    if (gs_common_lcm_info->get_fod_info != NULL) {
        gs_common_lcm_info->get_fod_info();
    } else {
        pr_info("%s %s: get_fod_info is NULL\n", SECLOG, __func__);
        goto NA;
    }

    if (gs_common_lcm_info->ts_data_info->fod_info_tx < 0
        || gs_common_lcm_info->ts_data_info->fod_info_rx < 0) {
        pr_err("%s %s: TX/RX node num value is wrong\n", SECLOG, __func__);
        goto NA;
    }
    pr_info("%s %s: x:%d, y:%d, size:%d\n", SECLOG, __func__, gs_common_lcm_info->ts_data_info->fod_info_tx,
        gs_common_lcm_info->ts_data_info->fod_info_rx, gs_common_lcm_info->ts_data_info->fod_info_bytes);

    return snprintf(buf, SEC_CMD_BUF_SIZE, "%d,%d,%d\n", gs_common_lcm_info->ts_data_info->fod_info_tx,
        gs_common_lcm_info->ts_data_info->fod_info_rx, gs_common_lcm_info->ts_data_info->fod_info_bytes);
NA:
    return snprintf(buf, SEC_CMD_BUF_SIZE, "NA\n");
}

static DEVICE_ATTR(cmd, 0220, NULL, sec_cmd_store);
static DEVICE_ATTR(cmd_status, 0444, sec_cmd_show_status, NULL);
static DEVICE_ATTR(cmd_status_all, 0444, sec_cmd_show_status_all, NULL);
static DEVICE_ATTR(cmd_result, 0444, sec_cmd_show_result, NULL);
static DEVICE_ATTR(cmd_result_all, 0444, sec_cmd_show_result_all, NULL);
static DEVICE_ATTR(cmd_list, 0444, sec_cmd_list_show, NULL);

static DEVICE_ATTR(support_feature, 0444, support_feature_show, NULL);

static DEVICE_ATTR(scrub_pos, 0444, scrub_pos_show, NULL);
static DEVICE_ATTR(fod_pos, 0444, fod_position_show, NULL);
static DEVICE_ATTR(fod_info, 0444, fod_info_show, NULL);

static struct attribute *sec_fac_attrs[] = {
    &dev_attr_cmd.attr,
    &dev_attr_cmd_status.attr,
    &dev_attr_cmd_status_all.attr,
    &dev_attr_cmd_result.attr,
    &dev_attr_cmd_result_all.attr,
    &dev_attr_cmd_list.attr,
    &dev_attr_support_feature.attr,
    &dev_attr_scrub_pos.attr,
    &dev_attr_fod_pos.attr,
    &dev_attr_fod_info.attr,
    NULL,
};
/*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/

static struct attribute_group sec_fac_attr_group = {
    .attrs = sec_fac_attrs,
};

static ssize_t enabled_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
    pr_info("%s %s()", SECLOG, __func__);
    return sprintf(buf,"%d", g_tp_is_enabled);
}
static ssize_t enabled_store(struct kobject *kobj, struct kobj_attribute *attr,  const char *buf, size_t count)
{
    bool val = false;
    int ret = 0;

    pr_info("%s %s()", SECLOG, __func__);

    ret = strtobool(buf, &val);
    if (ret < 0) {
        return ret;
    }

    if (val) {
        g_tp_is_enabled = true;
        /*M55 code for QN6887A-3421 by yuli at 20231220 start*/
        if (gs_common_lcm_info) {
            if (gs_common_lcm_info->tp_resume_work != NULL) {
                gs_common_lcm_info->tp_resume_work();
            }
        } else {
            pr_info("%s %s: gs_common_lcm_info is NULL", SECLOG, __func__);
        }
        /*M55 code for QN6887A-3421 by yuli at 20231220 end*/
    } else {
        g_tp_is_enabled = false;
    }
    return count;
}

static const struct kobj_attribute enabled_attr = __ATTR(enabled,0644,enabled_show,enabled_store);

/*M55 code for SR-QN6887A-01-400 by yuli at 202300824 start*/
#ifdef CONFIG_ARCH_X55
static ssize_t keycode_pressed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d:%d,%d:%d,%d:%d\n",KEY_POWER,
        get_pwrkey_state(),KEY_VOLUMEDOWN,get_volumedown_state(),KEY_VOLUMEUP,get_volumeup_state());
}
static DEVICE_ATTR(keycode_pressed, 0444, keycode_pressed_show, NULL);

static struct attribute *sec_key_attrs[] = {
    &dev_attr_keycode_pressed.attr,
    NULL,
};

static struct attribute_group sec_key_attr_group = {
    .attrs = sec_key_attrs,
};
#endif    // CONFIG_ARCH_X55
/*M55 code for SR-QN6887A-01-400 by yuli at 202300824 end*/

int sec_cmd_init(struct sec_cmd_data *data, struct sec_cmd *cmds,
            int len, int devt)
{
    const char *dev_name;
    int ret, i;
    struct kobject *sec_input_kobj;

    INIT_LIST_HEAD(&data->cmd_list_head);

    data->cmd_buffer_size = 0;
    for (i = 0; i < len; i++) {
        list_add_tail(&cmds[i].list, &data->cmd_list_head);
        if (cmds[i].cmd_name)
            data->cmd_buffer_size += strlen(cmds[i].cmd_name) + 1;
    }

    mutex_init(&data->cmd_lock);
    mutex_init(&data->fs_lock);

    mutex_lock(&data->cmd_lock);
    data->cmd_is_running = false;
    mutex_unlock(&data->cmd_lock);

    data->cmd_result = kzalloc(SEC_CMD_RESULT_STR_LEN_EXPAND, GFP_KERNEL);
    if (!data->cmd_result)
        goto err_alloc_cmd_result;

    if (kfifo_alloc(&data->cmd_queue,
        SEC_CMD_MAX_QUEUE * sizeof(struct command), GFP_KERNEL)) {
        pr_err("%s %s: failed to alloc queue for cmd\n", SECLOG, __func__);
        goto err_alloc_queue;
    }
    mutex_init(&data->fifo_lock);
    mutex_init(&data->wait_lock);
    init_completion(&data->cmd_result_done);
    complete_all(&data->cmd_result_done);

    INIT_DELAYED_WORK(&data->cmd_work, cmd_exit_work);

    switch (devt) {
    case SEC_CLASS_DEVT_TSP:
        dev_name = SEC_CLASS_DEV_NAME_TSP;
        break;
    default:
        pr_err("%s %s: not defined devt=%d\n", SECLOG, __func__, devt);
        goto err_get_dev_name;
    }
    /*M55 code for P230918-03913 by suyurui at 20230923 start*/
    data->fac_dev = sec_device_create(data, dev_name);
    /*M55 code for P230918-03913 by suyurui at 20230923 end*/

    if (IS_ERR(data->fac_dev)) {
        pr_err("%s %s: failed to create device for the sysfs\n", SECLOG, __func__);
        goto err_sysfs_device;
    }

    dev_set_drvdata(data->fac_dev, data);

    ret = sysfs_create_group(&data->fac_dev->kobj, &sec_fac_attr_group);
    if (ret < 0) {
        pr_err("%s %s: failed to create sysfs group\n", SECLOG, __func__);
        goto err_sysfs_group;
    }

    /*M55 code for SR-QN6887A-01-400 by yuli at 202300824 start*/
    #ifdef CONFIG_ARCH_X55
    /*M55 code for P230918-03913 by suyurui at 20230923 start*/
    sec_key = sec_device_create(NULL, "sec_key");
    /*M55 code for P230918-03913 by suyurui at 20230923 end*/
    if (IS_ERR(sec_key)) {
        pr_err("Failed to create device(sec_key)!\n");
        goto err_sysfs_device;
    }

    ret = sysfs_create_group(&sec_key->kobj, &sec_key_attr_group);
    if (ret < 0) {
        pr_err("Unable to create sysfs_group, error: %d\n",ret);
        goto err_sysfs_group;
    }
    #endif    // CONFIG_ARCH_X55
    /*M55 code for SR-QN6887A-01-400 by yuli at 202300824 end*/

    sec_input_kobj = kobject_create_and_add("input",&data->fac_dev->kobj);
    if (!sec_input_kobj) {
        pr_err("%s %s:sec_input_kobj(input dir) create fail!\n",SECLOG, __func__);
        goto put_sec_input_kobj;
    } else {
        ret = sysfs_create_file(sec_input_kobj,&enabled_attr.attr);
        if (ret < 0) {
            pr_err("%s %s:sec input dir create enabled node fail!\n",SECLOG, __func__);
        }
    }

    pr_info("%s: %s %s: done\n", dev_name, SECLOG, __func__);

    return 0;


put_sec_input_kobj:
    kobject_put(sec_input_kobj);
err_sysfs_group:
    /*M55 code for P230918-03913 by suyurui at 20230923 start*/
    sec_device_destroy(data->fac_dev->devt);
    /*M55 code for P230918-03913 by suyurui at 20230923 end*/
err_sysfs_device:
err_get_dev_name:
    mutex_destroy(&data->fifo_lock);
    kfifo_free(&data->cmd_queue);
    mutex_destroy(&data->wait_lock);
err_alloc_queue:
    kfree(data->cmd_result);
err_alloc_cmd_result:
    mutex_destroy(&data->cmd_lock);
    list_del(&data->cmd_list_head);
    return -ENODEV;
}
EXPORT_SYMBOL(sec_cmd_init);

void sec_cmd_exit(struct sec_cmd_data *data, int devt)
{
    struct command cmd = {{0}};
    int ret;

    pr_info("%s: %s %s\n", dev_name(data->fac_dev), SECLOG, __func__);
    sysfs_remove_group(&data->fac_dev->kobj, &sec_fac_attr_group);
    dev_set_drvdata(data->fac_dev, NULL);
    /*M55 code for P230918-03913 by suyurui at 20230923 start*/
    sec_device_destroy(data->fac_dev->devt);
    /*M55 code for P230918-03913 by suyurui at 20230923 end*/

    mutex_lock(&data->fifo_lock);
    while (kfifo_len(&data->cmd_queue)) {
        ret = kfifo_out(&data->cmd_queue, &cmd, sizeof(struct command));
        if (!ret) {
            pr_err("%s %s: kfifo_out failed, it seems empty, ret=%d\n", SECLOG, __func__, ret);
        }
        pr_info("%s %s: remove pending commands: %s", SECLOG, __func__, cmd.cmd);
    }
    mutex_unlock(&data->fifo_lock);
    mutex_destroy(&data->fifo_lock);
    kfifo_free(&data->cmd_queue);
    mutex_destroy(&data->wait_lock);

    cancel_delayed_work_sync(&data->cmd_work);
    flush_delayed_work(&data->cmd_work);

    data->fac_dev = NULL;
    kfree(data->cmd_result);
    mutex_destroy(&data->cmd_lock);
    list_del(&data->cmd_list_head);
}
EXPORT_SYMBOL(sec_cmd_exit);
MODULE_DESCRIPTION("SS input command");
MODULE_LICENSE("GPL");

