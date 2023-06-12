#ifndef SPRDMUX_H
#define SPRDMUX_H


#define SPRDMUX_READ	0x01
#define SPRDMUX_WRITE	0x02
#define SPRDMUX_ALL	(SPRDMUX_READ | SPRDMUX_WRITE)	

#define SPRDMUX_MAX_NAME_LEN	(64)

typedef unsigned int mux_handle;

/* name defined in ts0710mux.c */
typedef enum SPRDMUX_ID_E_ {
	SPRDMUX_ID_SPI = 0,
	SPRDMUX_ID_SDIO,

	SPRDMUX_ID_MAX,
}SPRDMUX_ID_E;

typedef enum SPRDMUX_EVENT_COMPLETE_E_ {
	SPRDMUX_EVENT_COMPLETE_READ,
	SPRDMUX_EVENT_COMPLETE_WRITE,

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
	int	index;
	notify_func func;
	void *user_data;
};

extern mux_handle sprdmux_register(struct sprdmux *mux);
extern void sprdmux_unregister(SPRDMUX_ID_E mux_id);
extern int sprdmux_get_all_mux_name(char names[][SPRDMUX_MAX_NAME_LEN]);
extern const char *sprdmux_get_mux_name(SPRDMUX_ID_E mux_id);

extern int sprdmux_open(SPRDMUX_ID_E mux_id, int index);
extern void sprdmux_close(SPRDMUX_ID_E mux_id, int index);
extern int sprdmux_write(SPRDMUX_ID_E mux_id, int index, const unsigned char *buf, int count);
extern int sprdmux_register_notify_callback(SPRDMUX_ID_E mux_id, struct sprdmux_notify *notify);
extern mux_handle sprdmux_get_mux(SPRDMUX_ID_E mux_id);

#endif
