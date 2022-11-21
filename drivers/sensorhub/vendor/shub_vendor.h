#ifndef _SHUB_VENDOR_H__
#define _SHUB_VENDOR_H__

#ifdef CONFIG_SHUB_LSI
#include "shub_lsi.h"
#else
#include "shub_mtk.h"
#endif

int sensorhub_comms_write(u8 *buf, int length);
int sensorhub_reset(void);
int sensorhub_probe(void);
int sensorhub_shutdown(void);
int sensorhub_refresh_func(void);
void sensorhub_fs_ready(void);
bool sensorhub_is_working(void);

#ifdef CONFIG_SHUB_DUMP
int get_sensorhub_dump_size(void);
void sensorhub_save_ram_dump(void);
#endif

#endif
