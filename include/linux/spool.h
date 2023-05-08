#ifndef __SPOOL_H
#define __SPOOL_H

struct spool_init_data {
	char 		*name;
	uint8_t 	dst;
	uint8_t 	channel;
	uint32_t 	txblocknum;
	uint32_t 	txblocksize;
	uint32_t 	rxblocknum;
	uint32_t 	rxblocksize;
};

#endif
