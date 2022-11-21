#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>

//#include "ssp.h"
#include "../staging/nanohub/chub.h"
#include "../staging/nanohub/chub_dbg.h"
#include "ssp_comm.h"
#include "ssp_dev.h"
#include "ssp_dump.h"
#include "ssp_firmware.h"

static void *platform_data;
struct ssp_data;
int no_response_reset_cnt = -2;

/* called from device driver */
int ssp_device_probe(struct platform_device *pdev)
{
	pr_info("ssp device probe");
	return ssp_probe(pdev);
}

void ssp_device_remove(struct platform_device *pdev)
{
	ssp_shutdown(pdev);
}

int ssp_device_suspend(struct device *dev)
{
	return ssp_suspend(dev);
}

void ssp_device_resume(struct device *dev)
{
	ssp_resume(dev);
}

int sensorhub_comms_write(void *ssp_data, u8 *buf, int length, int timeout)
{
	struct contexthub_ipc_info *ipc = platform_data;
	int ret = contexthub_ipc_write(ipc, buf, length, timeout);
	if (!ret) {
		ret = ERROR;
		pr_err("%s : write fail\n", __func__);
	}
	return ret;
}

int sensorhub_reset(void *ssp_data)
{
	int ret;
	struct contexthub_ipc_info *ipc = platform_data;

	if (atomic_read(&ipc->chub_status) == CHUB_ST_NO_POWER) { /* chub is no power status*/
		ret = contexthub_poweron(ipc);
		if (ret < 0) {
			ssp_errf("contexthub power on failed");
		} else {
			ssp_platform_start_refrsh_task();
		}
	} else {
		ret = contexthub_reset(ipc, 1, 0);
	}
	return ret;
}

int sensorhub_firmware_download(void *ssp_data)
{
	struct contexthub_ipc_info *ipc = platform_data;
	return contexthub_reset(ipc, 1, 0);
}

void ssp_platform_init(void *p_data)
{
	platform_data = p_data;
}

void ssp_handle_recv_packet(char *packet, int packet_len)
{
	struct ssp_data *data = get_ssp_data();
	handle_packet(data, packet, packet_len);
}

void ssp_platform_start_refrsh_task(void)
{
	struct ssp_data *data = get_ssp_data();
	queue_refresh_task(data, 0);
}

void save_ram_dump(void *ssp_data)
{
	struct contexthub_ipc_info *ipc = platform_data;
	struct ssp_data *data = (struct ssp_data *)ssp_data;

	if (atomic_read(&ipc->chub_status) == CHUB_ST_SHUTDOWN) {
		ssp_errf("chub status = shutdown, skip save dump");
		return;
	}

	if (contexthub_get_token(ipc)) {
		ssp_infof("get token, skip save dump");
		return;
	}

	ssp_infof("");
	write_ssp_dump_file(data, (char *)ipc_get_base(IPC_REG_DUMP), ipc_get_chub_mem_size(), 0, 1);
	contexthub_put_token(ipc);
}

int get_sensorhub_dump_size(void)
{
	return ipc_get_chub_mem_size();
}

void *get_sensorhub_dump_address(void)
{
	return ipc_get_base(IPC_REG_DUMP);
}

void ssp_dump_write_file(void *dump_data, int dump_size, int err_type)
{
	struct ssp_data *data = get_ssp_data();
	int dump_type;

	if (err_type == 0) {
		if (data->reset_type == RESET_TYPE_MAX && data->cnt_reset < 0)
			dump_type == CHUB_ERR_CHUB_NO_RESPONSE;
		else
			dump_type = DUMP_TYPE_BASE + data->reset_type;
	} else
		dump_type = err_type;

	if (dump_type == CHUB_ERR_CHUB_NO_RESPONSE) {
		if (no_response_reset_cnt != data->cnt_reset)
			no_response_reset_cnt = data->cnt_reset;
		else
			return;
	}

	write_ssp_dump_file(data, (char *)dump_data, dump_size, dump_type, 1);
}

bool is_sensorhub_working(void *ssp_data)
{
	struct ssp_data *data = (struct ssp_data *)ssp_data;
	struct contexthub_ipc_info *ipc = platform_data;

	if (data->is_probe_done && !work_busy(&data->work_reset) && atomic_read(&ipc->chub_status) == CHUB_ST_RUN &&
	    atomic_read(&ipc->in_reset) == 0) {
		return true;
	} else {
		return false;
	}
}

int ssp_download_firmware(void *addr)
{
	struct ssp_data *data = get_ssp_data();
	return download_sensorhub_firmware(data, addr);
}

void ssp_set_firmware_name(const char *fw_name)
{
	struct ssp_data *data = get_ssp_data();
	strcpy(data->fw_name, fw_name);
}

int sensorhub_platform_probe(void *ssp_data)
{
	return 0;
}

int sensorhub_platform_shutdown(void *ssp_data)
{
	return 0;
}

int sensorhub_refresh_func(void *ssp_data)
{
	return 0;
}

void sensorhub_fs_ready(void *ssp_data)
{
	struct ssp_data *data = (struct ssp_data *)ssp_data;
	struct contexthub_ipc_info *ipc = platform_data;

	contexthub_poweron(ipc);
	queue_refresh_task(data, 0);
}