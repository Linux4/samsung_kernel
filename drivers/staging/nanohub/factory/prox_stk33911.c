#include "../ssp.h"
#include "sensors.h"

/*************************************************************************/
/* Functions                                                             */
/*************************************************************************/
static ssize_t proximity_position_show(struct device *dev, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	switch(data->ap_type) {
		case 0:
			return snprintf(buf, PAGE_SIZE, "45.1 8.0 2.4\n");
		case 1:
			return snprintf(buf, PAGE_SIZE, "42.6 8.0 2.4\n");
		case 2:
			return snprintf(buf, PAGE_SIZE, "43.8 8.0 2.4\n");
		default:
			return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
	}

	return snprintf(buf, PAGE_SIZE, "0.0 0.0 0.0\n");
}

struct proximity_t prox_stk33911 = {
	.name = "STK33911",
	.vendor = "Sitronix",
	.get_prox_position = proximity_position_show
};

struct proximity_t* get_prox_stk33911(void){
	return &prox_stk33911;
}