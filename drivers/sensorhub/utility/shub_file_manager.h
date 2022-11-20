#ifndef __SHUB_FILE_MANAGER_H_
#define __SHUB_FILE_MANAGER_H_
#include <linux/notifier.h>

int init_file_manager(void);
void remove_file_manager(void);
void register_file_manager_ready_callback(struct notifier_block *nb);

int shub_file_read(char *path, char *buf, int buf_len, long long pos);
int shub_file_write(char *path, char *buf, int buf_len, long long pos);
int shub_file_write_no_wait(char *path, char *buf, int buf_len, long long pos);

#endif
