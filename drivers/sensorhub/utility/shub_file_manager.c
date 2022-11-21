#include "../comm/shub_iio.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "shub_dev_core.h"
#include "shub_utility.h"

#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/notifier.h>

#define FILE_MANAGER	0xFB
enum {
	FM_READ = 0,
	FM_WRITE,
	FM_READY,
};

static BLOCKING_NOTIFIER_HEAD(fm_ready_notifier_list);
struct mutex fm_ready_notifier_mutex;
struct work_struct fm_nofifier_work;

struct mutex fm_mutex;
struct completion fm_done;
bool is_fm_ready;

bool fm_wait_result;
int32_t fm_result;
int32_t fm_rx_buf_size;
char fm_rx_buf[PAGE_SIZE];
int32_t fm_tx_buf_size;
char fm_tx_buf[PAGE_SIZE];

static void fm_notifier_work_func(struct work_struct *work)
{
	mutex_lock(&fm_ready_notifier_mutex);
	is_fm_ready = true;
	shub_infof("");
	blocking_notifier_call_chain(&fm_ready_notifier_list, FM_READY, NULL);
	mutex_unlock(&fm_ready_notifier_mutex);
}

static void file_manager_ready(void)
{
	shub_infof("");
	shub_queue_work(&fm_nofifier_work);
}

void unregister_file_manager_ready_callback(struct notifier_block *nb)
{
	mutex_lock(&fm_ready_notifier_mutex);
	blocking_notifier_chain_unregister(&fm_ready_notifier_list, nb);
	mutex_unlock(&fm_ready_notifier_mutex);
}

void register_file_manager_ready_callback(struct notifier_block *nb)
{
	mutex_lock(&fm_ready_notifier_mutex);
	blocking_notifier_chain_register(&fm_ready_notifier_list, nb);
	shub_infof("");
	if (is_fm_ready)
		nb->notifier_call(nb, FM_READY, NULL);
	mutex_unlock(&fm_ready_notifier_mutex);
}

static int _shub_file_rw(char type, bool wait)
{
	char send_buf[2];
	int ret, result = 0;

	send_buf[0] = FILE_MANAGER;
	send_buf[1] = type;

	if (wait)
		init_completion(&fm_done);

	shub_report_sensordata(SENSOR_TYPE_SENSORHUB, get_current_timestamp(), send_buf, sizeof(send_buf));

	if (wait) {
		ret = wait_for_completion_timeout(&fm_done, msecs_to_jiffies(1000));
		if (!ret) {
			memset(fm_tx_buf, 0, fm_tx_buf_size);
			fm_tx_buf_size = 0;
			shub_errf("time out");
			result = -EIO;
		} else {
			result = fm_result;
		}
	}

	return result;
}

static int _shub_file_write(char *path, char *buf, int buf_len, long long pos, bool wait)
{
	int ret;

	if (!is_fm_ready)
		return -ENODEV;

	mutex_lock(&fm_mutex);
	fm_tx_buf_size = snprintf(fm_tx_buf, sizeof(fm_tx_buf), "%s,%d,%lld,%d,%s", path, buf_len, pos, wait, buf);
	ret = _shub_file_rw(FM_WRITE, wait);
	mutex_unlock(&fm_mutex);

	return ret;
}

int shub_file_write_no_wait(char *path, char *buf, int buf_len, long long pos)
{
	int ret = 0;

	ret = _shub_file_write(path, buf, buf_len, pos, false);

	return (ret < 0) ? ret : buf_len;
}

int shub_file_write(char *path, char *buf, int buf_len, long long pos)
{
	return _shub_file_write(path, buf, buf_len, pos, true);
}

int shub_file_read(char *path, char *buf, int buf_len, long long pos)
{
	int ret;

	if (!is_fm_ready)
		return -ENODEV;

	mutex_lock(&fm_mutex);
	fm_tx_buf_size = snprintf(fm_tx_buf, sizeof(fm_tx_buf), "%s,%d,%lld", path, buf_len, pos);
	ret = _shub_file_rw(FM_READ, true);
	memcpy(buf, fm_rx_buf, buf_len);
	mutex_unlock(&fm_mutex);

	return ret;
}

ssize_t shub_file_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (fm_tx_buf_size == 0)
		return -EINVAL;

	memcpy(buf, fm_tx_buf, fm_tx_buf_size);
	return fm_tx_buf_size;
}

ssize_t shub_file_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if (size < 1) {
		shub_errf("error %d", size);
		return -EINVAL;
	}

	if (!is_fm_ready) {
		if (buf[0] == FM_READY)
			file_manager_ready();

	} else {
		if (size < 5) {
			shub_errf("error %d", size);
			return -EINVAL;
		}

		memset(fm_tx_buf, 0, fm_tx_buf_size);
		fm_tx_buf_size = 0;

		memcpy(&fm_result, &buf[1], sizeof(int32_t));
		if (buf[0] == FM_READ && fm_result > 0) {
			fm_rx_buf_size = fm_result;
			memcpy(fm_rx_buf, &buf[5], fm_rx_buf_size);
		}

		complete(&fm_done);
	}

	return size;
}

static DEVICE_ATTR(shub_file, 0660, shub_file_show, shub_file_store);
static struct device_attribute *shub_attrs[] = {
	&dev_attr_shub_file,
	NULL,
};

int init_file_manager(void)
{
	int ret;
	struct shub_data_t *data = get_shub_data();

	mutex_init(&fm_mutex);
	mutex_init(&fm_ready_notifier_mutex);
	INIT_WORK(&fm_nofifier_work, fm_notifier_work_func);

	ret = sensor_device_create(&data->sysfs_dev, data, "ssp_sensor");
	if (ret < 0) {
		shub_errf("fail to creat ssp_sensor device");
		return ret;
	}

	ret = add_sensor_device_attr(data->sysfs_dev, shub_attrs);
	if (ret < 0)
		shub_errf("fail to add shub device attr");

	return ret;
}

void remove_file_manager(void)
{
	struct shub_data_t *data = get_shub_data();

	cancel_work_sync(&fm_nofifier_work);
	mutex_destroy(&fm_ready_notifier_mutex);
	mutex_destroy(&fm_mutex);
	remove_sensor_device_attr(data->sysfs_dev, shub_attrs);
	sensor_device_destroy(data->sysfs_dev);
}

