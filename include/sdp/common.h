#ifndef SDP_COMMON_H__
#define SDP_COMMON_H__

#define PER_USER_RANGE 100000

#define KNOX_PERSONA_BASE_ID 100

#define BASE_ID KNOX_PERSONA_BASE_ID
#define GET_ARR_IDX(__userid) (__userid - BASE_ID)
#define KEK_MAX_LEN KEK_MAXLEN
#define SDP_MAX_USERS 5

#endif
