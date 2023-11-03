#ifndef _LINUX_DEK_AES_H
#define _LINUX_DEK_AES_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <sdp/dek_aes.h>

int dek_aes_encrypt(kek_t *kek, char *src, char *dst, int len);
int dek_aes_decrypt(kek_t *kek, char *src, char *dst, int len);

#endif
