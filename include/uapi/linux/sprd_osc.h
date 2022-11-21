#ifndef __SPRD_OSC_H__
#define __SPRD_OSC_H__

#define SPRD_OSC_BASE	'O'

#define SPRD_OSC_START	_IO(SPRD_OSC_BASE, 0x01)
#define SPRD_OSC_STOP	_IO(SPRD_OSC_BASE, 0x02)
#define SPRD_OSC_SET_CHAIN	_IOW(SPRD_OSC_BASE, 0x03, unsigned int)
#define SPRD_OSC_CLK_NUM_SET	_IOW(SPRD_OSC_BASE, 0x04, unsigned int)

#endif
