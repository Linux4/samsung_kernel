// SPDX-License-Identifier: GPL-2.0
/*
 * (C) 2021 Carles Pey <cpey@pm.me>
 */

#ifndef HELPERS_H_
#define HELPERS_H_

#define HELPER_IOCTL_NUM 0xFF
#define CALL_FN1   _IO(HELPER_IOCTL_NUM, 0)
#define CALL_FN2   _IO(HELPER_IOCTL_NUM, 1)
#define CALL_FN3   _IO(HELPER_IOCTL_NUM, 2)
#define CALL_FN4   _IO(HELPER_IOCTL_NUM, 3)

void func_void(void);
void func_char(char);
void func_char_bis(char);
char func_char_ret_char(char);

#endif /* HELPERS_H_ */

