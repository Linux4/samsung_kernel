#ifndef __SHUB_HELPER_H
#define __SHUB_HELPER_H

#include <linux/kernel.h>
#include <linux/device.h>
#include "../sensorhub/shub_device_wrapper.h"

#define BOOTLOADER_FILE     "sensorhub/shub_nacho_bl.bin"

void shub_handle_recv_packet(char*, int);
void shub_start_refresh_task(void);
void shub_dump_write_file(void *dump_data, int dump_size, int err_type);
int shub_download_firmware(void *addr);
void shub_set_vendor_drvdata(void *p_data);
#endif
