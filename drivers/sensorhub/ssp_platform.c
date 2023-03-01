#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/device.h>

//#include "ssp.h"
#include "ssp_dev.h"
#include "ssp_comm.h"
#include "ssp_dump.h"
#include "ssp_firmware.h"
#include "../staging/nanohub/chub.h"
#include "../staging/nanohub/chub_dbg.h"

static void *platform_data;
struct ssp_data;

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

        if(atomic_read(&ipc->chub_status) == CHUB_ST_NO_POWER) { /* chub is no power status*/
                ret = contexthub_poweron(ipc);
                if(ret < 0) {
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

void save_ram_dump(void *ssp_data, int reason)
{
        struct contexthub_ipc_info *ipc = platform_data;
        ssp_infof("");
        chub_dbg_dump_hw(ipc, CHUB_ERR_MAX + reason);
}

void ssp_dump_write_file(int sec_time, int reason, void *sram_buf, int sram_size)
{
        char dump_info[40] = { 0, };
        struct ssp_data *data = get_ssp_data();

        snprintf(dump_info, sizeof(dump_info), "%06u-%02u", sec_time, reason);

        write_ssp_dump_file(data, dump_info, sram_buf, sram_size);

        if (reason < CHUB_ERR_MAX) {
                ssp_errf("reason %d, data->reset_type %d \n", reason, data->reset_type);
                data->reset_type = RESET_MCU_CRASHED;
                //sensorhub_reset(data);
        }
}

bool is_sensorhub_working(void *ssp_data)
{
        struct ssp_data *data = (struct ssp_data*) ssp_data;
        struct contexthub_ipc_info *ipc = platform_data;

        if (data->is_probe_done && !work_busy(&data->work_reset) && atomic_read(&ipc->chub_status) == CHUB_ST_RUN && atomic_read(&ipc->in_reset) == 0) {
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
