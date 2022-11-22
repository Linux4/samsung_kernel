
#ifndef _NTRIG_VIRTUAL_KEYS_DEF_H
#define _NTRIG_IRTUAL_KEYS_DEF_H

#include <linux/input.h>
#include "typedef-ntrig.h"


/**
 * Flag indicating if Virtual Keys are supported
 */
/*----------------------------------------------------------------------------*
 *--- VIRTUAL_KEYS_SUPPORTED should be #defined / #undefined  by the user ----*
 *----------------------------------------------------------------------------*/
/* #define VIRTUAL_KEYS_SUPPORTED 1 */
#undef VIRTUAL_KEYS_SUPPORTED

/******************************************************************************/

/* the following definitions take effect only if VIRTUAL_KEYES_SUPPORTED is
 * #defined */
#ifdef VIRTUAL_KEYS_SUPPORTED

	/**
	 * The device name the supports virtual keys
	 */

/*----------------------------------------------------------------------------*
 *---- ntrig_device_name value should be written by the user -----------------*
 *----------------------------------------------------------------------------*/
	#define NTRIG_DEVICE_NAME "N-trig Multi Touch"


	/**
	 * The borders of the touch screen sensor
	 */
/*----------------------------------------------------------------------------*
 *---- TOUCH_SCREEN_BORDER values should be written by the user --------------*
 * These are offsets from each side of the screen(border) to reserve for      *
 * virtual keys, in sensor units                                              *
 *----------------------------------------------------------------------------*/
	#define TOUCH_SCREEN_BORDER_DOWN 0
	#define TOUCH_SCREEN_BORDER_UP 0
	#define TOUCH_SCREEN_BORDER_LEFT 0
	#define TOUCH_SCREEN_BORDER_RIGHT 600

/*----------------------------------------------------------------------------*
 *---- TOUCH_SCREEN_BORDER_PEN values - the border values for pen-------------*
 * specify different values here in case the pen has different coordinate system
 * TODO this is a temporary hack and only for the screen border (capacitive keys
 * are specified in the touch coordinates above)
 * Basically, the pen and fingers must have the same coordinate system.
 * we can work around the problem while we have 2 separate input devices, but
 * once we have a consolidated input device it will not be possible!
 *----------------------------------------------------------------------------*/
	#define TOUCH_SCREEN_BORDER_PEN_DOWN 0
	#define TOUCH_SCREEN_BORDER_PEN_UP 0
	#define TOUCH_SCREEN_BORDER_PEN_LEFT 0
	#define TOUCH_SCREEN_BORDER_PEN_RIGHT 370

	/**
	 * Virtual Key structure - describes the data of a single virtual key
	 */
	struct _ntrig_virt_key {
		__u16 scan_code;	/* Key code of the virtual key */
		__u16 center_x;		/* Center of X axis of virtual key
					 * (in the sensor dimensions) */
		__u16 center_y;		/* Center of Y axis of virtual key
					 * (in the sensor dimensions) */
		__u16 width;		/* Width of virtual key */
		__u16 height;		/* Height of virtual key */
	};

	/**
	 * Number of Virtual Keys
	 */
/*----------------------------------------------------------------------------*
 *---- VIRTUAL_KEYS_NUM values should be written by the user -----------------*
 * This is the number of rows to be filled in _ntrig_virt_keys array below    *
 *----------------------------------------------------------------------------*/
	#define VIRTUAL_KEYS_NUM 3

	/**
	 * Virtual Keys actual data
	 */
/*----------------------------------------------------------------------------*
 *---- _ntrig_virt_keys values should be written by the user -----------------*
 *----------------------------------------------------------------------------*/
	struct _ntrig_virt_key _ntrig_virt_keys[VIRTUAL_KEYS_NUM] = {
		/* scan_code, center_x, center_y, width, height */
		{ 158, 9350, 2380, 500, 500}, /* 1st virtual key (158=BACK) */
		{ 139, 9350, 4880, 500, 500}, /* 2nd virtual key (139=MENU) */
		{ 102, 9350, 3660, 500, 500}, /* 3rd virtual key (102=HOME) */
		/*{ 217, 350, 6300, 700, 1800} //4th virtual key (217=SEARCH)*/
	};

#endif /* VIRTUAL_KEYS_SUPPORTED */


#endif /* _NTRIG_IRTUAL_KEYS_DEF_H */
