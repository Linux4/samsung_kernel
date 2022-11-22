
#include <linux/kernel.h>
#include <linux/rtc.h>
#include "ssp.h"
#include "ssp_dev.h"
#include "ssp_comm.h"
#include "ssp_dump.h"
#include "ssp_firmware.h"
#include "../staging/nanohub/chub.h"
#include "../staging/nanohub/chub_dbg.h"

int sensorhub_comms_read(void *ssp_data, u8 *buf, int length, int timeout)
{
	struct contexthub_ipc_info *ipc = ((struct ssp_data *)ssp_data)->platform_data;
	return contexthub_ipc_read(ipc, buf, length, timeout);
}

int sensorhub_comms_write(void *ssp_data, u8 *buf, int length, int timeout)
{
	struct contexthub_ipc_info *ipc = ((struct ssp_data *)ssp_data)->platform_data;
	int ret = contexthub_ipc_write(ipc, buf, length, timeout);
	if (!ret) {
		ret = ERROR;
		pr_err("%s : write fail\n", __func__);
	}
	return ret;
}

int sensorhub_reset(void *ssp_data)
{
	struct contexthub_ipc_info *ipc = ((struct ssp_data *)ssp_data)->platform_data;
	return contexthub_reset(ipc, 1, 0);
}

int sensorhub_firmware_download(void *ssp_data)
{
	struct contexthub_ipc_info *ipc = ((struct ssp_data *)ssp_data)->platform_data;
	return contexthub_reset(ipc, 1, 0);
}

int sensorhub_power_on(void *ssp_data)
{
	struct contexthub_ipc_info *ipc = ((struct ssp_data *)ssp_data)->platform_data;
	return contexthub_poweron(ipc);
}

int sensorhub_shutdown(void *ssp_data)
{
	struct contexthub_ipc_info *ipc = ((struct ssp_data *)ssp_data)->platform_data;
	int ret;
	ret = contexthub_ipc_write_event(ipc, MAILBOX_EVT_SHUTDOWN);
	if (ret)
	{
		ssp_errf("shutdonw fails, ret:%d\n", ret);
	}

	return ret;
}

void *ssp_device_probe(struct device *dev)
{
	return ssp_probe(dev);
}

void ssp_device_remove(void *ssp_data)
{
	struct ssp_data *data = ssp_data;
	ssp_remove(data);
}

void ssp_device_suspend(void *ssp_data)
{
	struct ssp_data *data = ssp_data;
	ssp_suspend(data);
}

void ssp_device_resume(void *ssp_data)
{
	struct ssp_data *data = ssp_data;
	ssp_resume(data);
}

void ssp_platform_init(void *ssp_data, void *platform_data)
{
	struct ssp_data *data = ssp_data;

	data->platform_data = platform_data;
}

void ssp_handle_recv_packet(void *ssp_data, char *packet, int packet_len)
{
	struct ssp_data *data = ssp_data;
	handle_packet(data, packet, packet_len);
}

void ssp_platform_start_refrsh_task(void *ssp_data)
{
	struct ssp_data *data = ssp_data;
	queue_refresh_task(data, 0);
}

void save_ram_dump(void *ssp_data, int reason)
{
	struct ssp_data *data = ssp_data;
	struct contexthub_ipc_info *ipc = data->platform_data;
	ssp_infof("");
	chub_dbg_dump_hw(ipc, CHUB_ERR_MAX + reason);
}

void ssp_dump_write_file(void *ssp_data, int sec_time, int reason, void *sram_buf, int sram_size)
{
	struct ssp_data *data = ssp_data;
	char dump_info[40] = {0,};

	snprintf(dump_info, sizeof(dump_info), "%06u-%02u", sec_time, reason);


	write_ssp_dump_file(data, dump_info, sram_buf, sram_size);

	if (reason < CHUB_ERR_MAX)
	{
		ssp_errf("reason %d, data->reset_type %d \n", reason, data->reset_type);
		data->reset_type = RESET_MCU_CRASHED;
		//sensorhub_reset(data);
	}
}

bool is_sensorhub_working(void *ssp_data)
{
	struct ssp_data *data = (struct ssp_data *)ssp_data;
	struct contexthub_ipc_info *ipc = ((struct ssp_data *)ssp_data)->platform_data;
	if(!work_busy(&data->work_reset) && atomic_read(&ipc->chub_status) == CHUB_ST_RUN && atomic_read(&ipc->in_reset) == 0)
		return true;
	else 
		return false;
}

int ssp_download_firmware(void *ssp_data, struct device* dev, void * addr)
{
	struct ssp_data *data = ssp_data;
	return download_sensorhub_firmware(data, dev, addr);
}

void ssp_set_fimware_name(void *ssp_data,const char *fw_name)
{
	struct ssp_data *data = ssp_data;
	strcpy(data->fw_name, fw_name);
}
