#ifndef TZ_HDM_H_
#define TZ_HDM_H_

#include <tee_client_api.h>
#define JWS_LEN 8192
#define NWS_INFO_LEN 128
#define MAX_KEY_BUFF 4096

#define CMD_STORE_APPLY_POLICY    0x00000001
#define CMD_LOAD_POLICY           0x00000002

typedef struct {
	uint8_t serial_number[NWS_INFO_LEN];
	uint8_t imei_0[NWS_INFO_LEN];
	uint8_t hash_imei[NWS_INFO_LEN];
	uint8_t mac_addr[NWS_INFO_LEN];
	uint8_t hdm_key[MAX_KEY_BUFF];
	uint32_t hdm_key_len;
	uint32_t is_wrapped_key;
} __attribute__ ((packed)) hdm_nwd_info_t;

typedef struct tz_msg_header {
	/** First 4 bytes should always be id: either cmd_id or resp_id */
	uint32_t id;
	uint32_t content_id;
	uint32_t len;
	uint32_t status;
} __attribute__ ((packed)) tz_msg_header_t;

typedef struct {
        uint32_t len;
        uint8_t data[JWS_LEN];
        uint32_t policy_value;
} __attribute__ ((packed)) tci_hdm_message_t;

typedef struct {
        tz_msg_header_t header;
        tci_hdm_message_t jws_message;
        hdm_nwd_info_t nwd_info;
} __attribute__ ((packed)) tciMessage_t;

#endif /* TZ_HDM_H_ */
