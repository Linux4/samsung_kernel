// SPDX-License-Identifier: GPL-2.0
/*
 * (C) 2021 Carles Pey <cpey@pm.me>
 */

#include <linux/printk.h>

void func_void(void) {
	pr_info("print_hello: Hello World\n");
}

void func_char(char ch) {
	pr_info("print_char: Received value: %c\n", ch);
}

void func_char_bis(char ch) {
	pr_info("print_char_bis: Received value: %c\n", ch);
}

char func_char_ret_char(char ch) {
	pr_info("print_char_ret_char: Received value: %c\n", ch);
	return ch + 1;
}
