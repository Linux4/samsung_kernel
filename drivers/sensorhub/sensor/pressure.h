#ifndef __SHUB_PRESSURE_H_
#define __SHUB_PRESSURE_H_

struct pressure_event {
	s32 pressure;
	s16 temperature;
	s32 pressure_cal;
	s32 pressure_sealevel;
} __attribute__((__packed__));

#endif /* __SHUB_PRESSURE_H_ */
