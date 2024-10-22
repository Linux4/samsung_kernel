/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _MTK_GEOFENCE_DEF_H_
#define _MTK_GEOFENCE_DEF_H_

#define GEOFENCE_TRANSITION_ENTERED     (1L<<0)
#define GEOFENCE_TRANSITION_EXITED      (1L<<1)
#define GEOFENCE_TRANSITION_UNCERTAIN   (1L<<2)

#define GEOFENCE_MONITOR_STATUS_UNAVAILABLE (1L<<0)
#define GEOFENCE_MONITOR_STATUS_AVAILABLE   (1L<<1)

#define GEOFENCE_RESULT_SUCCESS                       0
#define GEOFENCE_RESULT_ERROR                        -1
#define GEOFENCE_RESULT_INSUFFICIENT_MEMORY          -2
#define GEOFENCE_RESULT_TOO_MANY_GEOFENCES           -3
#define GEOFENCE_RESULT_ID_EXISTS                    -4
#define GEOFENCE_RESULT_ID_UNKNOWN                   -5
#define GEOFENCE_RESULT_INVALID_GEOFENCE_TRANSITION  -6


struct geofence_area {
	int32_t geofence_id;
	double latitude;
	double longitude;
	double radius_meters;
	int32_t last_transition;
	int32_t monitor_transitions;
	int32_t notification_responsiveness_ms;
	int32_t unknown_timer_ms;
	int32_t status;
};

struct elapsedrealtime {
	/**
	 * A set of flags indicating the validity of each field in this data structure.
	 *
	 * Fields may have invalid information in them, if not marked as valid by the
	 * corresponding bit in flags.
	 */
	uint16_t flags;

	/**
	 * Estimate of the elapsed time since boot value for the corresponding event in nanoseconds.
	 */
	uint64_t timestampNs;

	/**
	 * Estimate of the relative precision of the alignment of this SystemClock
	 * timestamp, with the reported measurements in nanoseconds (68% confidence).
	 */
	uint64_t timeUncertaintyNs;
};

struct geo_gnss_location {
	uint32_t flags;
	double lat;
	double lng;
	double alt;
	float speed;
	float bearing;
	float h_accuracy;  //horizontal
	float v_accuracy;  //vertical
	float s_accuracy;  //spedd
	float b_accuracy;  //bearing
	int64_t timestamp;
	uint32_t fix_type;
	int64_t utc_time;
	struct elapsedrealtime elapsed_realtime;
};

struct geofence_nlp_context {
	struct geo_gnss_location location;
	int64_t systime_ms;
};

struct scp2hal_geofence_ack {
	int32_t geofence_id;
	int32_t result;
};

struct scp2hal_geofence_add_area_ack {
	int32_t geofence_id;
	double latitude;
	double longitude;
	double radius_meters;
	int32_t last_transition;
	int32_t monitor_transitions;
	int32_t noitification_responsiveness_ms;
	int32_t unknown_timer_ms;
	int32_t status; //pause:0, working:1
	int32_t result;
};


struct scp2hal_geofence_transition {
	int32_t geofence_id;
	struct geo_gnss_location location;
	int32_t transition;
	int64_t timestamp;
};

struct scp2hal_geofence_status {
	int32_t status;
	struct geo_gnss_location last_location;
};

struct scp2hal_geofence_nlp_req {
	bool independentFromGnss;
	bool isUserEmergency;
};

struct geofence_resume_option {
	int32_t geofence_id;
	int32_t monitor_transitions;
};

/****************************************************/
/* Message Definition */
/****************************************************/
enum conn_geofence_hal2scp_msg_id {
	GEOFENCE_HAL2SCP_NONE = 0,
	GEOFENCE_HAL2SCP_INIT,
	GEOFENCE_HAL2SCP_ADD_AREA,
	GEOFENCE_HAL2SCP_REMOVE_AREA,
	GEOFENCE_HAL2SCP_PAUSE,
	GEOFENCE_HAL2SCP_RESUME,
	GEOFENCE_HAL2SCP_NLP_LOC,
};

enum conn_geofence_scp2hal_msg_id {
	GEOFENCE_SCP2HAL_NONE = 0,
	GEOFENCE_SCP2HAL_ADD_AREA_ACK,
	GEOFENCE_SCP2HAL_REMOVE_AREA_ACK,
	GEOFENCE_SCP2HAL_TRANSITION_CB,
	GEOFENCE_SCP2HAL_STATUS_CB,
	GEOFENCE_SCP2HAL_PAUSE_ACK,
	GEOFENCE_SCP2HAL_RESUME_ACK,
	GEOFENCE_SCP2HAL_RESTART,
	GEOFENCE_SCP2HAL_NLP_REQ,
};


#endif // _MTK_GEOFENCE_DEF_H_
