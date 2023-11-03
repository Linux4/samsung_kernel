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

int add_kek_pack(int engine_id, int userid);
void del_kek_pack(int engine_id);

int add_kek(int engine_id, kek_t *kek);
int del_kek(int engine_id, int kek_type);
kek_t *get_kek(int engine_id, int kek_type, int *rc);

void put_kek(kek_t *kek);

int is_kek_pack(int engine_id);
int is_kek(int engine_id, int kek_type);

#endif /* _SDP_KEK_PACK_H_ */
