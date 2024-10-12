/*
 * drivers/soc/samsung/exynos-hdcp/exynos-hdcp2-protocol-msg.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>

#include "exynos-hdcp2-config.h"
#include "exynos-hdcp2-protocol-msg.h"
#include "exynos-hdcp2-teeif.h"
#include "exynos-hdcp2.h"
#include "exynos-hdcp2-log.h"
#include "exynos-hdcp2-dplink-protocol-msg.h"

int ske_generate_sessionkey(uint32_t lk_type, uint8_t *enc_skey, int share_skey)
{
	int ret;

	ret = teei_generate_skey(lk_type,
			enc_skey, HDCP_SKE_SKEY_LEN,
			share_skey);
	if (ret) {
		hdcp_err("generate_session_key() is failed with %x\n", ret);
		return ERR_GENERATE_SESSION_KEY;
	}

	return 0;
}

int ske_generate_riv(uint8_t *out)
{
	int ret;

	ret = teei_generate_riv(out, HDCP_RTX_BYTE_LEN);
	if (ret) {
		hdcp_err("teei_generate_riv() is failed with %x\n", ret);
		return ERR_GENERATE_NON_SECKEY;
	}

	return 0;
}

int lc_generate_rn(uint8_t *out, size_t out_len)
{
	int ret;

	ret = teei_gen_rn(out, out_len);
	if (ret) {
		hdcp_err("lc_generate_rn() is failed with %x\n", ret);
		return ERR_GENERATE_NON_SECKEY;
	}

	return 0;
}

int lc_compare_hmac(uint8_t *rx_hmac, size_t hmac_len)
{
	int ret;

	ret = teei_compare_lc_hmac(rx_hmac, hmac_len);
	if (ret) {
		hdcp_err("compare_lc_hmac_val() is failed with %x\n", ret);
		return ERR_COMPARE_LC_HMAC;
	}

	return ret;
}

int ake_verify_cert(uint8_t *cert, size_t cert_len,
		uint8_t *rrx, size_t rrx_len,
		uint8_t *rx_caps, size_t rx_caps_len)
{
	int ret;

	ret = teei_verify_cert(cert, cert_len,
				rrx, rrx_len,
				rx_caps, rx_caps_len);
	if (ret) {
		hdcp_err("teei_verify_cert() is failed with %x\n", ret);
		return ERR_VERIFY_CERT;
	}

	return 0;
}

int ake_generate_masterkey(uint32_t lk_type, uint8_t *enc_mkey, size_t elen)
{
	int ret;

	/* Generate Encrypted & Wrapped Master Key */
	ret = teei_generate_master_key(lk_type, enc_mkey, elen);
	if (ret) {
		hdcp_err("generate_master_key() is failed with %x\n", ret);
		return ERR_GENERATE_MASTERKEY;
	}

	return ret;
}

int ake_compare_hmac(uint8_t *rx_hmac, size_t rx_hmac_len)
{
	int ret;

	ret = teei_compare_ake_hmac(rx_hmac, rx_hmac_len);
	if (ret) {
		hdcp_err("teei_compare_hmac() is failed with %x\n", ret);
		return ERR_COMPUTE_AKE_HMAC;
	}

	return 0;
}

int ake_store_master_key(uint8_t *ekh_mkey, size_t ekh_mkey_len)
{
	int ret;

	ret = teei_set_pairing_info(ekh_mkey, ekh_mkey_len);
	if (ret) {
		hdcp_err("teei_store_pairing_info() is failed with %x\n", ret);
		return ERR_STORE_MASTERKEY;
	}

	return 0;
}

int ake_find_masterkey(int *found_km,
		uint8_t *ekh_mkey, size_t ekh_mkey_len,
		uint8_t *m, size_t m_len)
{
	int ret;

	ret = teei_get_pairing_info(ekh_mkey, ekh_mkey_len, m, m_len);
	if (ret) {
		if (ret == E_HDCP_PRO_INVALID_RCV_ID) {
			hdcp_info("RCV id is not found\n");
			*found_km = 0;
			return 0;
		} else {
			*found_km = 0;
			hdcp_err("teei_store_pairing_info() is failed with %x\n", ret);
			return ERR_FIND_MASTERKEY;
		}
	}

	*found_km = 1;

	return 0;
}

int parse_rcvid_list(uint8_t *msg, struct hdcp_tx_ctx *tx_ctx)
{
        /* get PRE META values */
        tx_ctx->rpauth_info.devs_exd = (uint8_t)*msg;
        tx_ctx->rpauth_info.cascade_exd = (uint8_t)*(msg + 1);

        /* get META values */
        msg += HDCP_RP_RCV_LIST_PRE_META_LEN;
        tx_ctx->rpauth_info.devs_count = (uint8_t)*msg;
        tx_ctx->rpauth_info.depth = (uint8_t)*(msg + 1);
        tx_ctx->rpauth_info.hdcp2_down = (uint8_t)*(msg + 2);
        tx_ctx->rpauth_info.hdcp1_down = (uint8_t)*(msg + 3);
        memcpy(tx_ctx->rpauth_info.seq_num_v, (uint8_t *)(msg + 4), 3);
        memcpy(tx_ctx->rpauth_info.v_prime, (uint8_t *)(msg + 7), 16);

        /* get receiver ID list */
        msg += HDCP_RP_RCV_LIST_META_LEN;
	if (tx_ctx->rpauth_info.devs_count > HDCP_RCV_DEVS_COUNT_MAX) {
		hdcp_err("invalid DEVS count (%d)\n", tx_ctx->rpauth_info.devs_count);
		return -1;
	}

        memcpy(tx_ctx->rpauth_info.u_rcvid.arr, msg, tx_ctx->rpauth_info.devs_count * HDCP_RCV_ID_LEN);

	return 0;
}

void convert_rcvlist2authmsg(struct hdcp_rpauth_info *rpauth_info, uint8_t *src_msg, size_t *msg_len)
{
        int i;
        *msg_len = 0;

        for (i = 0; i < rpauth_info->devs_count; i++) {
                memcpy(src_msg + *msg_len, rpauth_info->u_rcvid.arr[i], HDCP_RCV_ID_LEN);
                *msg_len += HDCP_RCV_ID_LEN;
        }

        /* concatinate DEPTH */
        memcpy(src_msg + *msg_len, &rpauth_info->depth, 1);
        *msg_len += 1;

        /* concatinate DEVICE COUNT */
        memcpy(src_msg + *msg_len, &rpauth_info->devs_count, 1);
        *msg_len += 1;

        /* concatinate MAX DEVS EXCEEDED */
        memcpy(src_msg + *msg_len, &rpauth_info->devs_exd, 1);
        *msg_len += 1;

        /* concatinate MAX CASCADE EXCEEDED */
        memcpy(src_msg + *msg_len, &rpauth_info->cascade_exd, 1);
        *msg_len += 1;

        /* concatinate HDCP2 REPEATER DOWNSTREAM */
        memcpy(src_msg + *msg_len, &rpauth_info->hdcp2_down, 1);
        *msg_len += 1;

        /* concatinate HDCP1 DEVICE DOWNSTREAM */
        memcpy(src_msg + *msg_len, &rpauth_info->hdcp1_down, 1);
        *msg_len += 1;

        /* concatinate seq_num_v */
        memcpy(src_msg + *msg_len, &rpauth_info->seq_num_v, 3);
        *msg_len += 3;
}

MODULE_LICENSE("GPL");
