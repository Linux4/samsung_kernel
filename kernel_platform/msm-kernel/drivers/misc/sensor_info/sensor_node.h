#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/printk.h>

#define SMEM_SENSOR_INFO 140
struct light_info {
	int raw_ch0;
	int raw_ch1;
};
struct prox_info {
	int raw_data;
        int poffset;
};
typedef struct {
	struct light_info light;
	struct prox_info prox;
}sensor_info;
