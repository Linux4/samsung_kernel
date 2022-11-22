#ifndef __MODEM_INTF_DRIVER_H__
#define __MODEM_INTF_DRIVER_H__
#include <mach/modem_interface.h>


#define MODEM_INTF_EVENT_ASSERT                 0
#define MODEM_INTF_EVENT_ALIVE                  1
#define MODEM_INTF_EVENT_CP_REQ_RESET           2
#define MODEM_INTF_EVENT_SHUTDOWN               4
#define MODEM_INTF_EVENT_FORCE_RESET            8
#define MODEM_INTF_EVENT_WDG_RESET				16





enum   TRAN_DIR_t{
	TRAN_DIR_IN,
	TRAN_DIR_OUT,
	TRAN_DIR_NONE,
};

enum MSG_SRC_type {
	SRC_GPIO,
	SRC_SPI,
	SRC_MUX,
	SRC_DLOADER,
	SRC_USER
};

enum MODEM_MSG_type {
    MODEM_TRANSFER_REQ, // high layer(mux /download driver) request to send.
    MODEM_TRANSFER_END,// transfer complete
    MODEM_SLAVE_RTS,   // MODEM request to send data
    MODEM_SLAVE_RDY,   // indicate MODEM is ready to receive.
    MODEM_SET_MODE,
    MODEM_CTRL_GPIO_CHG,
    MODEM_USER_REQ,
    MODEM_BOOT_CMP,
    MODEM_OPEN_DEVICE
};

enum MSG_CTRL_GPIO_TYPE {
	GPIO_ALIVE = 0,
	GPIO_RESET,
	GPIO_WATCHDOG,
	CTRL_GPIO_MAX
};


struct modem_message_node{
        struct list_head	link;
	int			buno;
        enum MSG_SRC_type	src;
        enum MODEM_MSG_type	type;
        int			parameter1;
        int			parameter2;
};

struct modem_device_operation {
	enum MODEM_device_type	dev;
        int (*open) (void*para);
	int (*write)(char *buffer, int size);
	int (*read)(char *buffer, int size);
};

typedef struct modem_intf_device {
	struct modem_intf_platform_data	modem_config;
	enum MODEM_Mode_type		mode;
	struct modem_device_operation	*op;
	struct modem_buffer		send_buffer;
	struct modem_buffer		recv_buffer;
	enum TRAN_DIR_t			direction;
	int				buffer_index;
	int				status;
	int				out_transfering;
	int				out_transfer_pending;
} MODEM_DEV_T;

extern void boot_protocol(struct modem_message_node *msg);
extern void normal_protocol(struct modem_message_node *msg);
extern void dump_memory_protocol(struct modem_message_node *msg);
extern char *modem_intf_msg_string(enum MODEM_MSG_type msg);
#endif
