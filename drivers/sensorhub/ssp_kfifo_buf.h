#ifndef __SSP_KFIFO_BUF_H__
#define __SSP_KFIFO_BUF_H__

struct iio_buffer *ssp_iio_kfifo_allocate(void);
void ssp_iio_kfifo_free(struct iio_buffer *r);

#endif /* __SSP_KFIFO_BUF_H__ */
