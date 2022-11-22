#ifndef __SHUB_KFIFO_BUF_H__
#define __SHUB_KFIFO_BUF_H__

struct iio_buffer *shub_iio_kfifo_allocate(void);
void shub_iio_kfifo_free(struct iio_buffer *r);

#endif /* __SHUB_KFIFO_BUF_H__ */
