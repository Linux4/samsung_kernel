/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "ssp_sensorhub.h"

void ssp_sensorhub_log(const char *func_name,
				const char *data, int length)
{
	char buf[6];
	char *log_str;
	int log_size;
	int i;

	if (likely(length <= BIG_DATA_SIZE))
		log_size = length;
	else
		log_size = PRINT_TRUNCATE * 2 + 1;

	log_size = sizeof(buf) * log_size + 1;
	log_str = kzalloc(log_size, GFP_ATOMIC);
	if (unlikely(!log_str)) {
		ssp_errf("allocate memory for data log err");
		return;
	}

	for (i = 0; i < length; i++) {
		if (length < BIG_DATA_SIZE ||
			i < PRINT_TRUNCATE || i >= length - PRINT_TRUNCATE) {
			snprintf(buf, sizeof(buf), "%d", (signed char)data[i]);
			strlcat(log_str, buf, log_size);

			if (i < length - 1)
				strlcat(log_str, ", ", log_size);
		}
		if (length > BIG_DATA_SIZE && i == PRINT_TRUNCATE)
			strlcat(log_str, "..., ", log_size);
	}

	ssp_info("%s(%d): %s", func_name, length, log_str);
	kfree(log_str);
}

static int ssp_sensorhub_send_cmd(struct ssp_data *data,
					const char *buf, int count)
{
	int ret = 0;

	if (buf[2] < MSG2SSP_AP_STATUS_WAKEUP ||
		buf[2] >= MSG2SSP_AP_TEMPHUMIDITY_CAL_DONE) {
		ssp_errf("MSG2SSP_INST_LIB_NOTI err(%d)", buf[2]);
		return -EINVAL;
	}

	ret = ssp_send_cmd(data, buf[2], 0);

	if (buf[2] == MSG2SSP_AP_STATUS_WAKEUP ||
		buf[2] == MSG2SSP_AP_STATUS_SLEEP)
		data->uLastAPState = buf[2];

	if (buf[2] == MSG2SSP_AP_STATUS_SUSPEND ||
		buf[2] == MSG2SSP_AP_STATUS_RESUME)
		data->uLastResumeState = buf[2];

	return ret;
}

static int ssp_sensorhub_send_instruction(struct ssp_data *data,
					const char *buf, int count)
{
	unsigned char instruction = buf[0];

	if (buf[0] == MSG2SSP_INST_LIBRARY_REMOVE)
		instruction = REMOVE_LIBRARY;
	else if (buf[0] == MSG2SSP_INST_LIBRARY_ADD)
		instruction = ADD_LIBRARY;

	return send_instruction(data, instruction,
		(unsigned char)buf[1], (unsigned char *)(buf+2),
		(unsigned short)(count-2));
}

void handle_sensorhub_callstack_data(struct ssp_data *data, char *receive_data_frame , int *index)
{
    u8 size;
    int i;
    if (data->callstack_data != NULL) {
        kfree(data->callstack_data);
    }

    memcpy(&size, receive_data_frame + (*index), 1);
    *index += 1;
    data->callstack_data = kzalloc(CALLSTACK_ALL_DATA_SIZE, GFP_KERNEL);

    memcpy(data->callstack_data, receive_data_frame + (*index), CALLSTACK_ALL_DATA_SIZE);
    *index += CALLSTACK_ALL_DATA_SIZE;

    for (i = 0; i < CALLSTACK_ALL_DATA_SIZE / CALLSTACK_ONE_DATA_SIZE; i++) {
        u8 temp;
        temp = data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i];
        data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i] = data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i + 3];
        data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i + 3] = temp;
        temp = data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i + 1];
        data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i + 1] = data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i + 2];
        data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i + 2] = temp;
        ssp_infof("%x %x %x %x", data->callstack_data[4 * i], data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i + 1],
                  data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i + 2], data->callstack_data[CALLSTACK_ONE_DATA_SIZE * i + 3]);
    }
}

static ssize_t ssp_sensorhub_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	struct ssp_data *data = container_of(file->private_data, struct ssp_data, scontext_device);
	int ret = 0;
	char *buffer;

	if (data->is_ssp_shutdown) {
		ssp_errf("stop sending library data(shutdown)");
		return -EINVAL;
	}

	if (unlikely(count < 2)) {
		ssp_errf("library data length err(%d)", (int)count);
		return -EINVAL;
	}

	buffer = kzalloc(count * sizeof(char), GFP_KERNEL);
	if (unlikely(!buffer)) {
		ssp_errf("allocate memory for kernel buffer err");
		return -ENOMEM;
	}

	ret = copy_from_user(buffer, buf, count);
	if (unlikely(ret)) {
		ssp_errf("memcpy for kernel buffer err");
		ret = -EFAULT;
		goto exit;
	}

	ssp_sensorhub_log(__func__, buffer, count);

	if (unlikely(data->is_ssp_shutdown)) {
		ssp_errf("stop sending library data(shutdown)");
		ret = -EBUSY;
		goto exit;
	}

	if (buffer[0] == MSG2SSP_INST_LIB_NOTI)
		ret = ssp_sensorhub_send_cmd(data, buffer, count);
	else
		ret = ssp_sensorhub_send_instruction(data, buffer, count);

	if (unlikely(ret <= 0)) {
		ssp_errf("send library data err(%d)", ret);
		/* i2c transfer fail */
		if (ret == ERROR)
			ret = -EIO;
		/* i2c transfer done but no ack from MCU */
		else if (ret == FAIL)
			ret = -EAGAIN;

		goto exit;
	}

	ret = count;

exit:
	kfree(buffer);
	return ret;
}

static struct file_operations ssp_sensorhub_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.write = ssp_sensorhub_write,
};

int ssp_sensorhub_initialize(struct ssp_data *data)
{
	int ret;
	ssp_dbgf("----------");

	/* register scontext misc device */
	data->scontext_device.minor = MISC_DYNAMIC_MINOR;
	data->scontext_device.name = "ssp_sensorhub";
	data->scontext_device.fops = &ssp_sensorhub_fops;

	ret = misc_register(&data->scontext_device);
	if (ret < 0) {
		ssp_errf("register scontext misc device err(%d)", ret);
	}

	return ret;
}

void ssp_sensorhub_remove(struct ssp_data *data)
{
	ssp_sensorhub_fops.write = NULL;
	misc_deregister(&data->scontext_device);
}

MODULE_DESCRIPTION("Seamless Sensor Platform(SSP) sensorhub driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
