#include "cam_ois_define.h"
#include "cam_ois_aois_if.h"
#include "cam_ois_common.h"

#include <linux/notifier.h>
#include <linux/delay.h>

static BLOCKING_NOTIFIER_HEAD(cam_ois_write_reg_data_nb_head);
static BLOCKING_NOTIFIER_HEAD(cam_ois_reg_read_data_nb_head);
static BLOCKING_NOTIFIER_HEAD(cam_ois_factory_mode_data_nb_head);

int cam_ois_factory_mode_notifier_call_chain(unsigned long val, void *v)
{
	int ret = NOTIFY_OK;

	ret = blocking_notifier_call_chain(&cam_ois_factory_mode_data_nb_head, val, v);
	if (ret & NOTIFY_BAD) {
		LOG_ERR("notify bad ret = 0x%x", ret);
		ret = 1;
	} else
		ret = 0;

	return ret;
}

int cam_ois_factory_mode_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&cam_ois_factory_mode_data_nb_head, nb);
}
EXPORT_SYMBOL(cam_ois_factory_mode_notifier_register);

int cam_ois_factory_mode_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&cam_ois_factory_mode_data_nb_head, nb);
}
EXPORT_SYMBOL(cam_ois_factory_mode_notifier_unregister);

int cam_ois_reg_read_notifier_call_chain(unsigned long val, unsigned short addr,
	unsigned char *data, int size)
{
	int ret = NOTIFY_OK;
	char buf[MAX_CMD_DATA_SIZE] = {0, };

	if (size > MAX_CMD_DATA_SIZE - 3 || size < 0) {
		LOG_ERR("size is not available %d", size);
		return NOTIFY_BAD;
	}

	memcpy(buf, &addr, sizeof(char) * 2);
	LOG_DBG("addr 0x%x 0x%x, buf 0x%x 0x%x 0x%x 0x%x", (addr & 0xFF00) >> 8, (addr & 0xFF), buf[0], buf[1], buf[2], buf[3]);
	buf[2] = size;

	ret = blocking_notifier_call_chain(&cam_ois_reg_read_data_nb_head, val, buf);
	if (ret & NOTIFY_BAD) {
		LOG_ERR("notify bad ret = 0x%x", ret);
		ret = -1;
	} else
		ret = 2;

	memcpy(data, &buf[3], size);

	return ret;
}

int cam_ois_reg_read_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&cam_ois_reg_read_data_nb_head, nb);
}
EXPORT_SYMBOL(cam_ois_reg_read_notifier_register);

int cam_ois_reg_read_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&cam_ois_reg_read_data_nb_head, nb);
}
EXPORT_SYMBOL(cam_ois_reg_read_notifier_unregister);

int cam_ois_cmd_notifier_call_chain(unsigned long val, unsigned short addr,
	unsigned char *data, int size)
{
	int ret = NOTIFY_OK;
	char buf[MAX_CMD_DATA_SIZE] = {0, };
	int i = 0;

	if (size > MAX_CMD_DATA_SIZE - 3) {
		LOG_INF("data size is %d", size);
		return 0;
	}

	memcpy(buf, &addr, sizeof(char) * 2);

	buf[2] = size;
	memcpy(&buf[3], data, size);

	LOG_DBG("addr 0x%x 0x%x, buf 0x%x 0x%x 0x%x", (addr & 0xFF00) >> 8, (addr & 0xFF), buf[0], buf[1], buf[2]);
	for (i = 0; i < size; i++)
		LOG_DBG("data(%d) = 0x%x, %d ", i, data[i], data[i]);

	ret = blocking_notifier_call_chain(&cam_ois_write_reg_data_nb_head, val, buf);

	if (ret & NOTIFY_BAD) {
		LOG_ERR("notify bad ret = 0x%x", ret);
		ret = 0;
	} else
		ret = 1;

	return ret;
}

int cam_ois_cmd_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&cam_ois_write_reg_data_nb_head, nb);
}
EXPORT_SYMBOL(cam_ois_cmd_notifier_register);

int cam_ois_cmd_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&cam_ois_write_reg_data_nb_head, nb);
}
EXPORT_SYMBOL(cam_ois_cmd_notifier_unregister);

int cam_ois_set_fac_mode(enum cam_ois_aois_fac_mode op)
{
	int ret = 0;
	char buf[MAX_CMD_DATA_SIZE] = {0, };

	LOG_INF(" - E, mode %d", op);

	buf[0] = op;
	buf[1] = -1;
	buf[2] = -1;
	buf[3] = -1;

	ret = cam_ois_factory_mode_notifier_call_chain(0, &buf);
	if (ret & NOTIFY_BAD) {
		LOG_ERR("notify bad ret = 0x%x", ret);
		ret = -1;
	} else
		ret = 0;

	LOG_INF(" - X, ret: %d", ret);
	return ret;
}

void cam_ois_set_aois_fac_mode_on(void)
{
	int ret = 0;

	ret = cam_ois_set_fac_mode(FACTORY_MODE_ON);
	if (ret < 0)
		LOG_INF("failed, FACTORY_MODE_ON ret: %d\n", ret);
	else
		LOG_INF("success, FACTORY_MODE_ON\n");

	msleep(30);
}

void cam_ois_set_aois_fac_mode_off(void)
{
	int ret = 0;

	ret = cam_ois_set_fac_mode(FACTORY_MODE_OFF);
	if (ret < 0)
		LOG_INF("failed, FACTORY_MODE_OFF ret: %d\n", ret);
	else
		LOG_INF("success, FACTORY_MODE_OFF\n");
}

void cam_ois_set_aois_fac_mode(enum cam_ois_aois_fac_mode fac_mode, unsigned int msec)
{
	int ret = 0;

	ret = cam_ois_set_fac_mode(fac_mode);
	if (ret < 0)
		LOG_INF("failed, mode: %d ret:%d\n", fac_mode, ret);
	else
		LOG_INF("success, mode: %d\n", fac_mode);

	usleep_range(msec * 1000, msec * 1000 + 1000);
}
