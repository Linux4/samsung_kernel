#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../utility/shub_utility.h"
#include "../debug/shub_dump.h"
#include "shub_mtk.h"

#include <linux/delay.h>
#include <linux/notifier.h>

#define IPI_SSP	    IPI_SENSOR
#define IPI_DMA_LEN 8

static DEFINE_SPINLOCK(scp_state_lock);
static uint8_t scp_system_ready;

static phys_addr_t shub_dram_phys;
static phys_addr_t shub_dram_virt;

void scp_wdt_reset(enum scp_core_id cpu_id);

static void sensorhub_IPI_handler(int id, void *packet, unsigned int len)
{
	struct shub_data_t *data = get_shub_data();

	if (!data->is_probe_done) {
		shub_infof("shub is not working");
		return;
	}

	if (id == IPI_SSP) {
		if (len == IPI_DMA_LEN) {
			uint32_t dma_info[2] = {0, }; /* address, size */

			memcpy(dma_info, packet, len);
			if (dma_info[0] == shub_dram_phys)
				handle_packet((void *)shub_dram_virt, dma_info[1]);
			else
				shub_errf("dma address is not matched 0x%lx len %d", dma_info[0], dma_info[1]);
		} else {
			handle_packet(packet, len);
		}
	}
}

static int notify_scp_state(struct notifier_block *this, unsigned long event, void *ptr)
{
	unsigned long flags = 0;

	shub_infof("notify event %d", event);

	if (event == SCP_EVENT_STOP) { // 1
		spin_lock_irqsave(&scp_state_lock, flags);
		WRITE_ONCE(scp_system_ready, false);
		spin_unlock_irqrestore(&scp_state_lock, flags);
	}

	if (event == SCP_EVENT_READY) { // 0
		spin_lock_irqsave(&scp_state_lock, flags);
		WRITE_ONCE(scp_system_ready, true);
		spin_unlock_irqrestore(&scp_state_lock, flags);
		queue_refresh_task();
	}

	return NOTIFY_DONE;
}

static struct notifier_block scp_state_notifier = {
	.notifier_call = notify_scp_state,
};

int sensorhub_probe(void)
{
	scp_ipi_registration(IPI_SSP, sensorhub_IPI_handler, "shub_sensorhub");
	scp_A_register_notify(&scp_state_notifier);

	return 0;
}

int sensorhub_shutdown(void)
{
	scp_A_unregister_notify(&scp_state_notifier);
	return 0;
}

int sensorhub_refresh_func(void)
{
	int ret;

	shub_dram_phys = scp_get_reserve_mem_phys(SENS_MEM_ID);
	shub_dram_virt = scp_get_reserve_mem_virt(SENS_MEM_ID);

	shub_infof("dma address phys 0x%llx virt 0x%llx", shub_dram_phys, shub_dram_virt);
	ret = shub_send_command(CMD_SETVALUE, TYPE_MCU, DMA_ADDRESS, (char *)&shub_dram_phys, sizeof(shub_dram_phys));

	if (ret < 0)
		shub_errf("DMA_ADDRESS CMD fail %d", ret);

	return 0;
}

int sensorhub_comms_write(u8 *buf, int length)
{
	int status = 0, retry = 0;

	do {
		status = scp_ipi_send(IPI_SSP, (unsigned char *)buf, length, 0, SCP_A_ID);
		if (status == SCP_IPI_ERROR) {
			shub_errf("scp_ipi_send fail");
			return -1;
		}
		if (status == SCP_IPI_BUSY) {
			if (retry++ == 1000) {
				shub_errf("retry fail");
				return -1;
			}
			if (retry % 100 == 0)
				usleep_range(1000, 2000);
		}
	} while (status == SCP_IPI_BUSY);

	return status;
}

bool sensorhub_is_working(void)
{
	struct shub_data_t *data = get_shub_data();

	return	READ_ONCE(scp_system_ready) && data->is_probe_done && !work_busy(&data->work_reset);
}

int sensorhub_reset(void)
{
	scp_wdt_reset(SCP_A_ID);
	return 0;
}

void sensorhub_fs_ready(void)
{
}

void sensorhub_save_ram_dump(void)
{
}

void shub_dump_write_file(void *dump_data, int dump_size)
{
	struct shub_data_t *data = get_shub_data();
	int dump_type;

	if (!data) {
		shub_infof("shub is not probed yet !");
		return;
	}

	if (data->reset_type < RESET_TYPE_MAX)
		dump_type = DUMP_TYPE_BASE + data->reset_type;
	else
		dump_type = 1;

	write_shub_dump_file((char *)dump_data, dump_size, dump_type, 4);
}
