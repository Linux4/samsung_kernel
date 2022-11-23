#ifndef __IST30XX_TRACKING_H__
#define __IST30XX_TRACKING_H__


#define IST30XX_RINGBUF_NO_ERR      (0)
#define IST30XX_RINGBUF_NOT_ENOUGH  (1)
#define IST30XX_RINGBUF_EMPTY       (2)
#define IST30XX_RINGBUF_FULL        (3)
#define IST30XX_RINGBUF_TIMEOUT     (4)
#define IST30XX_RINGBUF_SIZE        (1024)


typedef struct _IST30XX_RINGBUF {
	u16	RingBufCtr; // Number of characters in the ring buffer
	u16	RingBufInIdx;
	u16	RingBufOutIdx;
	u32	TimeBuf[IST30XX_RINGBUF_SIZE];          // Ring buffer for time
	u32	StatusBuf[IST30XX_RINGBUF_SIZE];        // Ring buffer for status
} IST30XX_RING_BUF;


void ist30xx_tracking_init(void);
void ist30xx_tracking_deinit(void);

int ist30xx_put_track(u32 ms, u32 status);
int ist30xx_get_track(u32 *ms, u32 *status);

int ist30xx_get_track_cnt(void);

int ist30xx_init_tracking_sysfs(void);

#endif  // __IST30XX_TRACKING_H__
