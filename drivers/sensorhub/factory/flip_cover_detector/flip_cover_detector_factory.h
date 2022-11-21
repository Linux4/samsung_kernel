#ifndef __FLIP_COVER_DETECTOR_FACTORY_H_
#define __FLIP_COVER_DETECTOR_FACTORY_H_

enum {
	OFF = 0,
	ON = 1
};

enum {
	X = 0,
	Y = 1,
	Z = 2,
	AXIS_MAX
};

#define ATTACH          "ATTACH"
#define DETACH          "DETACH"
#define FACTORY_PASS    "PASS"
#define FACTORY_FAIL    "FAIL"

#define AXIS_SELECT      X
#define THRESHOLD        700
#define DETACH_MARGIN    100
#define SATURATION_VALUE 4900
#define MAG_DELAY_MS     50

#define COVER_DETACH            0 // OPEN
#define COVER_ATTACH            1 // CLOSE
#define COVER_ATTACH_NFC_ACTIVE 2 // CLOSE

void check_cover_detection_factory(void);

#endif
