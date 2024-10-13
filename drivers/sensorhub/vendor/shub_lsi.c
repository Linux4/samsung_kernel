#include "../../staging/nanohub/chub.h"
#include "../../staging/nanohub/chub_dbg.h"
#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensorhub/shub_firmware.h"
#include "../utility/shub_utility.h"
#include "../debug/shub_dump.h"

static struct contexthub_ipc_info *chub_data;

int sensorhub_comms_write(u8 *buf, int length)
{
	struct contexthub_ipc_info *ipc = chub_data;
	int ret;

	ret = contexthub_ipc_write(ipc, buf, length, 0);
	if (ret == 0) {
		shub_errf("fail %d", ret);
		ret = -EIO;
	}
	return ret;
}

void shub_start_refresh_task(void)
{
	queue_refresh_task();
}

int sensorhub_reset(void)
{
	int ret;
	struct contexthub_ipc_info *ipc = chub_data;

	if (atomic_read(&ipc->chub_status) == CHUB_ST_NO_POWER) { /* chub is no power status*/
		ret = contexthub_poweron(ipc);
		if (ret < 0)
			shub_errf("contexthub power on failed");
		else
			shub_start_refresh_task();
	} else
		ret = contexthub_reset(ipc, 1, 0);
	return ret;
}

void sensorhub_save_ram_dump(void)
{
	struct contexthub_ipc_info *ipc = chub_data;

	if (contexthub_get_token(ipc)) {
		shub_infof("get token, skip save dump");
		return;
	}

	shub_infof("");
	write_shub_dump_file((char *)ipc_get_base(IPC_REG_DUMP), ipc_get_chub_mem_size(), 0, 1);
	contexthub_put_token(ipc);
}

bool sensorhub_is_working(void)
{
	struct contexthub_ipc_info *ipc = chub_data;

	if (atomic_read(&ipc->chub_status) == CHUB_ST_RUN && atomic_read(&ipc->in_reset) == 0)
		return true;
	else
		return false;
}

void shub_handle_recv_packet(char *packet, int packet_len)
{
	handle_packet(packet, packet_len);
}

void shub_dump_write_file(void *dump_data, int dump_size, int err_type)
{

	struct shub_data_t *data = get_shub_data();
	int dump_type;

	if (err_type == 0 && data->reset_type < RESET_TYPE_MAX)
		dump_type = DUMP_TYPE_BASE + data->reset_type;
	else
		dump_type = err_type;

	write_shub_dump_file((char *)dump_data, dump_size, dump_type, 1);
}

int shub_download_firmware(void *addr)
{
	struct shub_data_t *data = get_shub_data();

	return download_sensorhub_firmware(&data->pdev->dev, addr);
}

int sensorhub_refresh_func(void)
{
	return 0;
}

int sensorhub_probe(void)
{
	enable_sensor_vdd();
	return 0;
}

int sensorhub_shutdown(void)
{
	return 0;
}

void shub_set_vendor_drvdata(void *p_data)
{
	chub_data = p_data;
}

void sensorhub_fs_ready(void)
{
	struct contexthub_ipc_info *ipc = chub_data;

	if (contexthub_poweron(ipc) < 0)
		shub_errf("contexthub power on failed");
	else
		shub_start_refresh_task();
}

int sensorhub_get_dump_size(void)
{
	return ipc_get_chub_mem_size();
}