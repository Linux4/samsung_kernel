#ifndef _ET300_DATA_TRANSFER_H_
#define _ET300_DATA_TRANSFER_H_

#include "et300.h"

int et300_io_mass_read(struct et300_data* et300, u8 addr, u8* buf, int read_len);
int et300_io_bulk_read(struct et300_data* et300, u8* addr, u8* buf, int read_len);
int et300_io_bulk_write(struct et300_data* et300, u8* buf, int write_len);
int et300_read_register(struct et300_data* et300, u8 addr, u8* buf);
int et300_write_register(struct et300_data* et300, u8 addr, u8 value);
int et300_get_one_image(struct et300_data *et300, u8 *buf, u8 *image_buf);

#endif
