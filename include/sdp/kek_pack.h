/*
 * kek_pack.h
 *
 *  Created on: Mar 15, 2015
 *      Author: olic.moon@samsung.com
 */

#ifndef _SDP_KEK_PACK_H_
#define _SDP_KEK_PACK_H_

#include <sdp/dek_common.h>

void init_kek_pack(void);

int add_kek_pack(int userid);
void del_kek_pack(int userid);

int add_kek(int userid, kek_t *kek);
int del_kek(int userid, int kek_type);
kek_t *get_kek(int userid, int kek_type);

void put_kek(kek_t *kek);

int is_kek_pack(int userid);
int is_kek(int userid, int kek_type);

#endif /* _SDP_KEK_PACK_H_ */
