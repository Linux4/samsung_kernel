#ifndef CRC32_H
#define CRC32_H

/* unit of count is 2byte */
unsigned long getCRC(volatile unsigned short *mem, signed long size,
	volatile unsigned short *crcH, volatile unsigned short *crcL);

#endif
