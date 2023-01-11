/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#ifndef _PSD_DATA_CHANNEL_H_
#define _PSD_DATA_CHANNEL_H_

#define MAX_CID_NUM    15

#define PSD_DATA_SEND_OK 0
#define PSD_DATA_SEND_BUSY -1
#define PSD_DATA_SEND_DROP -2

struct psd_user {
	void *priv;
	int (*on_receive)(void *priv, const unsigned char *packet,
			unsigned int len);
	void (*on_throttle)(void *priv, bool is_throttle);
};

int psd_register(const struct psd_user *user, int cid);
int psd_unregister(const struct psd_user *user, int cid);
void set_embms_cid(int cid);


int sendPSDData(int cid, struct sk_buff *skb);

#endif
