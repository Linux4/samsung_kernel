#ifndef __SHUB_COMM_H__
#define __SHUB_COMM_H__

#include <linux/kernel.h>
#include "shub_cmd.h"

int shub_send_command(u8 cmd, u8 type, u8 subcmd, char *send_buf, int send_buf_len);
int shub_send_command_wait(u8 cmd, u8 type, u8 subcmd, int timeout, char *send_buf, int send_buf_len,
			   char **receive_buf, int *receive_buf_len, bool reset);
void handle_packet(char *packet, int packet_size);

int get_cnt_comm_fail(void);
int get_cnt_timeout(void);

int init_comm_to_hub(void);
void exit_comm_to_hub(void);

#endif /* __SHUB_COMM_H__ */
