#include "shub_factory.h"
#include "../utility/shub_utility.h"

typedef void (*init_sensor_factory)(bool en);
init_sensor_factory init_sensor_factory_funcs[] = {
	initialize_accelerometer_factory,
	initialize_magnetometer_factory,
	initialize_gyroscope_factory,
	initialize_light_factory,
	initialize_proximity_factory,
	initialize_pressure_factory,
	initialize_flip_cover_detector_factory,
};

int initialize_factory(void)
{
	uint64_t i;

	for (i = 0; i < ARRAY_LEN(init_sensor_factory_funcs); i++)
		init_sensor_factory_funcs[i](true);

	return 0;
}

void remove_factory(void)
{
	uint64_t i;

	for (i = 0; i < ARRAY_LEN(init_sensor_factory_funcs); i++)
		init_sensor_factory_funcs[i](false);

}
