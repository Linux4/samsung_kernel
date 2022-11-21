#ifndef SPRDMUX_H
#define SPRDMUX_H

#include <linux/poll.h>

#define SPRDMUX_READ	0x01
#define SPRDMUX_WRITE	0x02
#define SPRDMUX_ALL	(SPRDMUX_READ | SPRDMUX_WRITE)	

#define SPRDMUX_MAX_NAME_LEN	(64)

/* name defined in ts0710mux.c */
typedef enum SPRDMUX_ID_E_ {
	SPRDMUX_ID_SPI = 0,
	SPRDMUX_ID_SDIO,

	SPRDMUX_ID_MAX,
}SPRDMUX_ID_E;

typedef enum SPRDMUX_EVENT_COMPLETE_E_ {
	SPRDMUX_EVENT_COMPLETE_READ,
	SPRDMUX_EVENT_COMPLETE_WRITE,
	SPRDMUX_EVENT_READ_IND,

	SPRDMUX_EVENT_COMPLETE_MAX,
}SPRDMUX_EVENT_COMPLETE_E;

typedef int (*notify_func)(int tty_index, int event_id, __u8 *uih_data_start, __u32 uih_len, void * user_data);

struct sprdmux {
	int	id;
	int	(*io_write)(const char *buf, size_t len);
	int	(*io_read)(char *buf, size_t len);
	int	(*io_stop)(int mode);
};

struct sprdmux_notify {
	int				index;
	notify_func		func;
	void			*user_data;
};

struct mux_init_data {
	SPRDMUX_ID_E	id;
	char			*name;
	int				num;
};

extern int sprdmux_register(struct sprdmux *mux);
extern void sprdmux_unregister(SPRDMUX_ID_E mux_id);
extern int sprdmux_open(SPRDMUX_ID_E mux_id, int index);
extern void sprdmux_close(SPRDMUX_ID_E mux_id, int index);
extern int sprdmux_write(SPRDMUX_ID_E mux_id, int index, const unsigned char *buf, int count);
extern int sprdmux_register_notify_callback(SPRDMUX_ID_E mux_id, struct sprdmux_notify *notify);
extern int sprdmux_line_busy(SPRDMUX_ID_E mux_id, int line);
extern void sprdmux_set_line_notify(SPRDMUX_ID_E mux_id, int line, __u8 notify);

extern int ts0710_mux_create(int mux_id);
extern void ts0710_mux_destory(int mux_id);
extern int ts0710_mux_init(void);
extern void ts0710_mux_exit(void);
extern int ts0710_mux_status(int mux_id);
extern int ts0710_mux_open(int mux_id, int line);
extern void ts0710_mux_close(int mux_id, int line);
extern int ts0710_mux_write(int mux_id, int line, const unsigned char *buf, int count, int timeout);
extern int ts0710_mux_read(int mux_id, int line, const unsigned char *buf, int count, int timeout);
extern int ts0710_mux_poll_wait(int mux_id, int line, struct file *filp, poll_table *wait);
extern int ts0710_mux_mux_ioctl(int mux_id, int line, unsigned int cmd, unsigned long arg);
#endif
