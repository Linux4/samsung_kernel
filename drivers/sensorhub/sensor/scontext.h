#include "scontext_cmd.h"

void shub_report_scontext_data(char *data, u32 data_len);
void shub_report_scontext_notice_data(char notice);
void shub_scontext_log(const char *func_name, const char *data, int length);
int shub_scontext_send_instruction(const char *buf, int count);
int shub_scontext_send_cmd(const char *buf, int count);
void get_ss_sensor_name(int type, char *buf, int buf_size);
void init_scontext_enable_state(void);
