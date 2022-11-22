#include <linux/kernel.h>
#include <linux/io.h>

#include "../misc/mediatek/scp/mt6853/scp_ipi.h"
#include "../misc/mediatek/scp/mt6853/scp_helper.h"

#include "ssp.h"
#include "ssp_dev.h"
#include "ssp_comm.h"
#include "ssp_dump.h"
#include "ssp_firmware.h"
#include "ssp_cmd_define.h"
#include <linux/delay.h>

#define IPI_SSP		IPI_SENSOR
#define IPI_DMA_LEN		8
static DEFINE_SPINLOCK(scp_state_lock);
static uint8_t scp_system_ready;

static phys_addr_t shub_dram_phys;
static phys_addr_t shub_dram_virt;


void scp_wdt_reset(enum scp_core_id cpu_id);


static void sensorhub_IPI_handler(int id,
	void *packet, unsigned int len)
{
	struct ssp_data *data = get_ssp_data();
	if (!is_sensorhub_working(data))
	{
		ssp_infof("ssp is not working");
		return;
	}

	if (id == IPI_SSP)
	{
		if (len == IPI_DMA_LEN) {
			uint32_t dma_info[2] = {0,};	/* address, size */

			memcpy(dma_info, packet, len);
			if (dma_info[0] == shub_dram_phys)
				handle_packet(data, (void *)shub_dram_virt, dma_info[1]);
			else
				ssp_errf("dma address is not matched 0x%lx len %d", dma_info[0], dma_info[1]);
		} else {
			handle_packet(data, packet, len);
		}
	}
}

static int notify_scp_state(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	unsigned long flags = 0;
	struct ssp_data *data = get_ssp_data();

	ssp_infof("notify event %d", event);

	if (event == SCP_EVENT_STOP) {
		spin_lock_irqsave(&scp_state_lock, flags);
		WRITE_ONCE(scp_system_ready, false);
		spin_unlock_irqrestore(&scp_state_lock, flags);
	}

	if (event == SCP_EVENT_READY) {
		spin_lock_irqsave(&scp_state_lock, flags);
		WRITE_ONCE(scp_system_ready, true);
		spin_unlock_irqrestore(&scp_state_lock, flags);
		queue_refresh_task(data, 0);
	}

	return NOTIFY_DONE;
}

static struct notifier_block scp_state_notifier = {
	.notifier_call = notify_scp_state,
};


int sensorhub_platform_probe(void *ssp_data)
{
	scp_ipi_registration(IPI_SSP,
		sensorhub_IPI_handler, "ssp_sensorhub");

	scp_A_register_notify(&scp_state_notifier);

	return 0;
}

int sensorhub_platform_shutdown(void *ssp_data)
{
	scp_A_unregister_notify(&scp_state_notifier);
	return 0;
}

int sensorhub_comms_write(void *ssp_data, u8 *buf, int length, int timeout)
{
	int status = 0, retry = 0;

	do {
		status = scp_ipi_send(IPI_SSP, (unsigned char *)buf, length, 0, SCP_A_ID);
		if (status == SCP_IPI_ERROR) {
			ssp_err("scp_ipi_send fail\n");
			return -1;
		}
		if (status == SCP_IPI_BUSY) {
			if (retry++ == 1000) {
				ssp_err("retry fail\n");
				return -1;
			}
			if (retry % 100 == 0)
				usleep_range(1000, 2000);
		}
	} while (status == SCP_IPI_BUSY);

	return status;
}

int sensorhub_refresh_func(void *ssp_data)
{
	struct ssp_data *data = (struct ssp_data*)ssp_data;
	int ret;

	shub_dram_phys = scp_get_reserve_mem_phys(SENS_MEM_ID);
	shub_dram_virt = scp_get_reserve_mem_virt(SENS_MEM_ID);

	ssp_infof("dma address phys 0x%llx virt 0x%llx", shub_dram_phys, shub_dram_virt);
	ret = ssp_send_command(data, CMD_SETVALUE, TYPE_MCU,
			       DMA_ADDRESS, 0, (char *)&shub_dram_phys, sizeof(shub_dram_phys), NULL, NULL);

	if (ret != SUCCESS) {
		ssp_err("DMA_ADDRESS CMD fail %d", ret);
	}

	return 0;
}

int sensorhub_reset(void *ssp_data)
{
	//reset_scp(SCP_A_REBOOT);
	scp_wdt_reset(SCP_A_ID);
	return 0;
}

int sensorhub_firmware_download(void *ssp_data)
{
	return 0;
}

void sensorhub_fs_ready(void *ssp_data)
{
	return;
}

bool is_sensorhub_working(void *ssp_data)
{
	struct ssp_data *data = (struct ssp_data *) ssp_data;

	return	READ_ONCE(scp_system_ready) && data->is_probe_done && !work_busy(&data->work_reset);
}
