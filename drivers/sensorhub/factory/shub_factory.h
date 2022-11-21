#ifndef __SHUB_FACTORY_H_
#define __SHUB_FACTORY_H_
#include <linux/types.h>

int initialize_factory(void);
void remove_factory(void);

void initialize_accelerometer_factory(bool en);
void initialize_gyroscope_factory(bool en);
void initialize_light_factory(bool en);
void initialize_magnetometer_factory(bool en);
void initialize_pressure_factory(bool en);
void initialize_proximity_factory(bool en);
void initialize_flip_cover_detector_factory(bool en);

#endif

