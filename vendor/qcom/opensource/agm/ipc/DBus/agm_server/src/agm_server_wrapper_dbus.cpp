/*
** Copyright (c) 2020, The Linux Foundation. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above
**     copyright notice, this list of conditions and the following
**     disclaimer in the documentation and/or other materials provided
**     with the distribution.
**   * Neither the name of The Linux Foundation nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
** WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
** ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
** BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
** BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
** OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
** IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#define LOG_TAG "agm_server_wrapper_dbus"

#include <dbus/dbus.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <agm/agm_api.h>
#include "agm-dbus-utils.h"
#include "agm_server_wrapper_dbus.h"

#include "utils.h"

#define AGM_OBJECT_PATH "/org/qti/agm"
#define AGM_MODULE_IFACE "org.Qti.Agm"
#define AGM_SESSION_IFACE "org.Qti.Agm.Session"
#define AGM_DBUS_CONNECTION "org.Qti.AgmService"

using namespace std;

/* Module Level data */
typedef struct {
    /* Dbus path where agm module listens for connections */
    char *dbus_obj_path;
    /* Dbus connection */
    agm_dbus_connection *conn;
    /* Hashmap containing all sessions info */
    GHashTable *sessions;
} agm_module_dbus_data;

/* Session specific data */
typedef struct {
    /* Session id */
    uint32_t session_id;
    /* Session handle */
    uint64_t handle;
    /* Dbus path for session specific APIs */
    char *dbus_obj_path;
    /* List which maintains all the callbacks associated with a session id.
       Used to de-register callbacks when client dies abruptly */
    GList *callbacks;
} agm_session_data;

typedef struct {
    uint32_t session_id;
    uint32_t event_type;
    uint64_t client_data;
} agm_callback_data;

static agm_module_dbus_data *mdata = NULL;

enum AgmModuleMethods {
    AgmAifSetMediaConfig,
    AgmAifSetMetadata,
    AgmSessionAifSetMetadata,
    AgmSessionSetMetadata,
    AgmSessionSetLoopback,
    AgmSetParamsWithTag,
    AgmGetAifInfoListSize,
    AgmGetAifInfoList,
    AgmSessionSetParams,
    AgmSessionRegisterForEvents,
    AgmSessionRegisterCb,
    AgmSessionDeRegisterCb,
    AgmSessionSetEcRef,
    AgmSessionAifConnect,
    AgmSessionAifGetTagModuleInfo,
    AgmSessionAifGetTagModuleInfoSize,
    AgmSessionAifSetParams,
    AgmSessionAifSetCal,
    AgmSessionGetParams,
    AgmGetBufferTimestamp,
    AgmSessionOpen,
    AgmDbusModuleMethodMax
};

enum AgmSessionMethods {
    AgmSessionClose,
    AgmSessionPrepare,
    AgmSessionStart,
    AgmSessionStop,
    AgmSessionPause,
    AgmSessionResume,
    AgmSessionRead,
    AgmSessionWrite,
    AgmSessionSetConfig,
    AgmSessionEos,
    AgmSessionGetTime,
    AgmGetHwProcessedBufCount,
    AgmDbusSessionMethodMax
};

enum AgmEventSignals {
    AgmEventCb,
    AgmSignalMax
};

static void ipc_agm_audio_intf_set_metadata(DBusConnection *conn,
                                            DBusMessage *msg,
                                            void *userdata);
static void ipc_agm_audio_intf_set_media_config(DBusConnection *conn,
                                                DBusMessage *msg,
                                                void *userdata);
static void ipc_agm_session_audio_inf_set_metadata(DBusConnection *conn,
                                                   DBusMessage *msg,
                                                   void *userdata);
static void ipc_agm_session_set_metadata(DBusConnection *conn,
                                         DBusMessage *msg,
                                         void *userdata);
static void ipc_agm_session_set_loopback(DBusConnection *conn,
                                         DBusMessage *msg,
                                         void *userdata);
static void ipc_agm_set_params_with_tag(DBusConnection *conn,
                                        DBusMessage *msg,
                                        void *userdata);
static void ipc_agm_get_aif_info_list_size(DBusConnection *conn,
                                           DBusMessage *msg,
                                           void *userdata);
static void ipc_agm_get_aif_info_list(DBusConnection *conn,
                                      DBusMessage *msg,
                                      void *userdata);
static void ipc_agm_session_set_params(DBusConnection *conn,
                                       DBusMessage *msg,
                                       void *userdata);
static void ipc_agm_session_register_for_events(DBusConnection *conn,
                                                DBusMessage *msg,
                                                void *userdata);
static void ipc_agm_session_set_ec_ref(DBusConnection *conn,
                                       DBusMessage *msg,
                                       void *userdata);
static void ipc_agm_session_audio_inf_connect(DBusConnection *conn,
                                              DBusMessage *msg,
                                              void *userdata);
static void ipc_agm_session_aif_get_tag_module_info(DBusConnection *conn,
                                                    DBusMessage *msg,
                                                    void *userdata);
static void ipc_agm_session_aif_get_tag_module_info_size(DBusConnection *conn,
                                                         DBusMessage *msg,
                                                         void *userdata);
static void ipc_agm_session_aif_set_params(DBusConnection *conn,
                                           DBusMessage *msg,
                                           void *userdata);
static void ipc_agm_session_aif_set_cal(DBusConnection *conn,
                                        DBusMessage *msg,
                                        void *userdata);
static void ipc_agm_session_get_params(DBusConnection *conn,
                                       DBusMessage *msg,
                                       void *userdata);
static void ipc_agm_session_open(DBusConnection *conn,
                                 DBusMessage *msg,
                                 void *userdata);
static void ipc_agm_session_close(DBusConnection *conn,
                                  DBusMessage *msg,
                                  void *userdata);
static void ipc_agm_session_prepare(DBusConnection *conn,
                                    DBusMessage *msg,
                                    void *userdata);
static void ipc_agm_session_start(DBusConnection *conn,
                                  DBusMessage *msg,
                                  void *userdata);
static void ipc_agm_session_stop(DBusConnection *conn,
                                 DBusMessage *msg,
                                 void *userdata);
static void ipc_agm_session_pause(DBusConnection *conn,
                                  DBusMessage *msg,
                                  void *userdata);
static void ipc_agm_session_resume(DBusConnection *conn,
                                   DBusMessage *msg,
                                   void *userdata);
static void ipc_agm_session_read(DBusConnection *conn,
                                 DBusMessage *msg,
                                 void *userdata);
static void ipc_agm_session_write(DBusConnection *conn,
                                  DBusMessage *msg,
                                  void *userdata);
static void ipc_agm_session_set_config(DBusConnection *conn,
                                       DBusMessage *msg,
                                       void *userdata);
static void ipc_agm_session_eos(DBusConnection *conn,
                                DBusMessage *msg,
                                void *userdata);
static void ipc_agm_get_session_time(DBusConnection *conn,
                                     DBusMessage *msg,
                                     void *userdata);
static void ipc_agm_get_hw_processed_buff_cnt(DBusConnection *conn,
                                              DBusMessage *msg,
                                              void *userdata);
static void ipc_agm_get_buffer_timestamp(DBusConnection *conn,
                                         DBusMessage *msg,
                                         void *userdata);
static void ipc_agm_session_register_cb(DBusConnection *conn,
                                        DBusMessage *msg,
                                        void *userdata);
static void ipc_agm_session_deregister_cb(DBusConnection *conn,
                                          DBusMessage *msg,
                                          void *userdata);

static agm_dbus_method agm_dbus_module_methods[AgmDbusModuleMethodMax] = {
    {"AgmAifSetMediaConfig", "u(uui)", ipc_agm_audio_intf_set_media_config},
    {"AgmAifSetMetadata", "uuay", ipc_agm_audio_intf_set_metadata},
    {"AgmSessionAifSetMetadata", "uuuay",
                                      ipc_agm_session_audio_inf_set_metadata},
    {"AgmSessionSetMetadata", "uuay", ipc_agm_session_set_metadata},
    {"AgmSessionSetLoopback", "uub", ipc_agm_session_set_loopback},
    {"AgmSetParamsWithTag", "uu(uua(uu))", ipc_agm_set_params_with_tag},
    {"AgmGetAifInfoListSize", "", ipc_agm_get_aif_info_list_size},
    {"AgmGetAifInfoList", "u", ipc_agm_get_aif_info_list},
    {"AgmSessionSetParams", "uuay", ipc_agm_session_set_params},
    {"AgmSessionRegisterForEvents", "u(uuuyay)",
                                         ipc_agm_session_register_for_events},
    {"AgmSessionRegisterCb", "uut", ipc_agm_session_register_cb},
    {"AgmSessionDeRegisterCb", "uut", ipc_agm_session_deregister_cb},
    {"AgmSessionSetEcRef", "uub", ipc_agm_session_set_ec_ref},
    {"AgmSessionAifConnect", "uub", ipc_agm_session_audio_inf_connect},
    {"AgmSessionAifGetTagModuleInfo", "uuu",
                                     ipc_agm_session_aif_get_tag_module_info},
    {"AgmSessionAifGetTagModuleInfoSize", "uuu",
                                  ipc_agm_session_aif_get_tag_module_info_size},
    {"AgmSessionAifSetParams", "uuuay", ipc_agm_session_aif_set_params},
    {"AgmSessionAifSetCal", "uuuay", ipc_agm_session_aif_set_cal},
    {"AgmSessionGetParams", "uuay", ipc_agm_session_get_params},
    {"AgmGetBufferTimestamp", "u", ipc_agm_get_buffer_timestamp},
    {"AgmSessionOpen", "u", ipc_agm_session_open}
};

static agm_dbus_method agm_dbus_session_methods[AgmDbusSessionMethodMax] = {
    {"AgmSessionClose", "", ipc_agm_session_close},
    {"AgmSessionPrepare", "", ipc_agm_session_prepare},
    {"AgmSessionStart", "", ipc_agm_session_start},
    {"AgmSessionStop", "", ipc_agm_session_stop},
    {"AgmSessionPause", "", ipc_agm_session_pause},
    {"AgmSessionResume", "", ipc_agm_session_resume},
    {"AgmSessionRead", "u", ipc_agm_session_read},
    {"AgmSessionWrite", "uay", ipc_agm_session_write},
    {"AgmSessionSetConfig", "(uuu)(uu)ay", ipc_agm_session_set_config},
    {"AgmSessionEos", "", ipc_agm_session_eos},
    {"AgmSessionGetTime", "", ipc_agm_get_session_time},
    {"AgmGetHwProcessedBufCount", "u", ipc_agm_get_hw_processed_buff_cnt}
};

static agm_dbus_signal event_callback[AgmSignalMax] = {
    {"AgmEventCb", "uuuay"}
};

agm_dbus_interface_info module_interface_info = {
    .name=AGM_MODULE_IFACE,
    .methods=agm_dbus_module_methods,
    .method_count=AgmDbusModuleMethodMax,
    .signals=NULL,
    .signal_count=0
};

agm_dbus_interface_info session_interface_info = {
    .name=AGM_SESSION_IFACE,
    .methods=agm_dbus_session_methods,
    .method_count=AgmDbusSessionMethodMax,
    .signals=event_callback,
    .signal_count=AgmSignalMax
};

static DBusHandlerResult disconnection_filter_cb(DBusConnection *conn,
                                                 DBusMessage *msg,
                                                 void *userdata) {
    agm_session_data *ses_data = (agm_session_data *) userdata;
    GList *node = NULL;
    agm_callback_data *cb_data = NULL;

    if (conn == NULL) {
        AGM_LOGE("Connection is NULL");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (msg == NULL) {
        AGM_LOGE("msg is NULL");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (userdata == NULL) {
        AGM_LOGE("Userdata is NULL ");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (dbus_message_is_signal(msg,
                               "org.freedesktop.DBus.Local",
                               "Disconnected")) {
        /* connection died close the session for which callback got triggered */
        AGM_LOGE("connection died for session %d", ses_data->session_id);

        for (node = ses_data->callbacks; node != NULL; node = node->next) {
            cb_data = (agm_callback_data *)node->data;
            if (agm_session_register_cb(cb_data->session_id,
                                        NULL,
                                        (enum event_type)cb_data->event_type,
                                        (void *)ses_data) != 0)
                AGM_LOGE("Deregistering callback failed.");
            free(node->data);
            node->data = NULL;
        }

        g_list_free(ses_data->callbacks);

        dbus_connection_remove_filter(conn, disconnection_filter_cb, ses_data);

        if (agm_session_close(ses_data->handle) != 0) {
            AGM_LOGE("agm_session_close failed.");
            agm_dbus_send_error(mdata->conn,
                                msg,
                                DBUS_ERROR_FAILED,
                                "agm_session_close failed.");
        }

        if (agm_dbus_remove_interface(mdata->conn,
                                      ses_data->dbus_obj_path,
                                      session_interface_info.name) != 0) {
            AGM_LOGE("Unable to remove interface");
            agm_dbus_send_error(mdata->conn,
                                msg,
                                DBUS_ERROR_FAILED,
                                "agm_session_close failed.");
        }

        g_hash_table_remove(mdata->sessions,
                            GUINT_TO_POINTER(ses_data->session_id));
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void agm_free_session(gpointer c_data) {
    agm_session_data *data = (agm_session_data *)data;

    if(data == NULL)
        return;

    if (data->dbus_obj_path != NULL) {
        if (agm_dbus_remove_interface(mdata->conn,
                                      data->dbus_obj_path,
                                      session_interface_info.name))
            AGM_LOGE("Unable to remove interface.");
        free(data->dbus_obj_path);
        data->dbus_obj_path = NULL;
    }

    free(data);
    data = NULL;
}

static agm_session_data * get_session_data(agm_module_dbus_data *mdata,
                                           uint32_t session_id) {
    agm_session_data *ses_data = NULL;
    stringstream ss;
    size_t obj_length = 0;

    if (mdata == NULL)
        return NULL;

      if ((ses_data = (agm_session_data *)
                        g_hash_table_lookup(mdata->sessions,
                                       GUINT_TO_POINTER(session_id))) == NULL) {
        ses_data = (agm_session_data *)malloc(sizeof(agm_session_data));
        ses_data->session_id = session_id;
        ss << ses_data->session_id;
        obj_length = sizeof(char)*(strlen(AGM_OBJECT_PATH)) +
                     strlen("/session_") +
                     ss.str().length() + 1;
        ses_data->dbus_obj_path = (char *)malloc(obj_length);
        snprintf(ses_data->dbus_obj_path,
                 obj_length,
                 "%s%s%d",
                 AGM_OBJECT_PATH,
                 "/session_",
                 session_id);
        ses_data->callbacks = NULL;

        if (agm_dbus_add_interface(mdata->conn,
                                   ses_data->dbus_obj_path,
                                   &session_interface_info,
                                   ses_data)) {
            free(ses_data->dbus_obj_path);
            ses_data->dbus_obj_path = NULL;
            free(ses_data);
            ses_data = NULL;
            return NULL;
        }

        g_hash_table_insert(mdata->sessions,
                            GUINT_TO_POINTER(session_id),
                            ses_data);
    }

    return ses_data;
}

static void agmevent_cb(uint32_t session_id,
                        struct agm_event_cb_params *event_params,
                        void *client_data) {
    DBusMessage *message = NULL;
    DBusMessageIter arg_i, array_i;
    dbus_uint32_t i, j;
    void *buf = NULL;
    agm_session_data *ses_data = (agm_session_data *)client_data;

    AGM_LOGE("%s: Received event for session %d", __func__, session_id);

    buf = malloc(event_params->event_payload_size);
    memcpy(buf, event_params->event_payload, event_params->event_payload_size);

    message = dbus_message_new_signal(ses_data->dbus_obj_path,
                                      session_interface_info.name,
                                      event_callback[AgmEventCb].method_name);

    dbus_message_iter_init_append(message, &arg_i);
    dbus_message_iter_append_basic(&arg_i,
                                   DBUS_TYPE_UINT32,
                                   &event_params->source_module_id);
    dbus_message_iter_append_basic(&arg_i,
                                   DBUS_TYPE_UINT32,
                                   &event_params->event_id);
    dbus_message_iter_append_basic(&arg_i,
                                   DBUS_TYPE_UINT32,
                                   &event_params->event_payload_size);
    dbus_message_iter_open_container(&arg_i,
                                     DBUS_TYPE_ARRAY,
                                     "y",
                                     &array_i);
    dbus_message_iter_append_fixed_array(&array_i,
                                         DBUS_TYPE_BYTE,
                                         &buf,
                                         event_params->event_payload_size);
    dbus_message_iter_close_container(&arg_i, &array_i);
    agm_dbus_send_signal(mdata->conn, message);
    dbus_message_unref(message);
    free(buf);
    buf = NULL;
}

static void ipc_agm_session_deregister_cb(DBusConnection *conn,
                                          DBusMessage *msg,
                                          void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i;
    uint32_t session_id;
    uint32_t evt_type;
    agm_session_data *ses_data = NULL;
    agm_module_dbus_data *mdata = (agm_module_dbus_data *)userdata;
    uint64_t client_data;
    GList *node = NULL;
    agm_callback_data *cb_data = NULL;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn,
                            msg, DBUS_ERROR_FAILED, "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_deregister_cb has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_deregister_cb has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uut")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_deregister_cb.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                        "Invalid signature for ipc_agm_session_deregister_cb.");
        return;
    }

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &evt_type);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &client_data);

    ses_data = get_session_data(mdata, session_id);

    if (agm_session_register_cb(session_id, NULL,
                                (enum event_type)evt_type,
                                (void *)ses_data) != 0) {
        AGM_LOGE("agm_session_register_cb failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
            "Failed to de-register callback. agm_session_register_cb failed.");
        agm_free_session(ses_data);
        return;
    }

    cb_data = (agm_callback_data *)malloc(sizeof(agm_callback_data));
    cb_data->session_id = session_id;
    cb_data->event_type = evt_type;
    cb_data->client_data = client_data;
    node = g_list_find(ses_data->callbacks, cb_data);
    if (node != NULL) {
        ses_data->callbacks = g_list_remove(ses_data->callbacks, cb_data);
        free(node->data);
        node->data = NULL;
    }
    free(cb_data);
    cb_data = NULL;

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &arg_i);
    dbus_message_iter_append_basic(&arg_i,
                                   DBUS_TYPE_OBJECT_PATH,
                                   &ses_data->dbus_obj_path);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_register_cb(DBusConnection *conn,
                                        DBusMessage *msg,
                                        void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i;
    uint32_t session_id;
    uint32_t evt_type;
    agm_session_data *ses_data = NULL;
    agm_module_dbus_data *mdata = (agm_module_dbus_data *)userdata;
    uint64_t client_data;
    agm_callback_data *cb_data = NULL;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_register_cb has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_register_cb has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uut")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_register_cb.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                          "Invalid signature for ipc_agm_session_register_cb.");
        return;
    }

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &evt_type);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &client_data);

    ses_data = get_session_data(mdata, session_id);

    if (agm_session_register_cb(session_id,
                                agmevent_cb,
                                (enum event_type)evt_type,
                                (void *)ses_data) != 0) {
        AGM_LOGE("agm_session_register_cb failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_register_cb failed.");
        agm_free_session(ses_data);
        return;
    }

    cb_data = (agm_callback_data *)malloc(sizeof(agm_callback_data));
    cb_data->session_id = session_id;
    cb_data->event_type = evt_type;
    cb_data->client_data = client_data;
    ses_data->callbacks = g_list_prepend(ses_data->callbacks, cb_data);

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &arg_i);
    dbus_message_iter_append_basic(&arg_i, DBUS_TYPE_OBJECT_PATH,
                                   &ses_data->dbus_obj_path);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}


static void ipc_agm_session_register_for_events(DBusConnection *conn,
                                                DBusMessage *msg,
                                                void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, struct_i, struct_ii, array_i;
    struct agm_tag_config tag_config;
    uint32_t session_id;
    uint32_t i = 0;
    uint8_t *value = NULL;
    uint8_t **addr_value = &value;
    int arg_type;
    int n_elements = 0;
    struct agm_event_reg_cfg *evt_reg_cfg;
    uint32_t module_instance_id;
    uint32_t event_id;
    uint32_t event_config_payload_size;
    uint8_t is_register;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_register_for_events has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                        "ipc_agm_session_register_for_events has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "u(uuuyay)")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_register_for_events.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                  "Invalid signature for ipc_agm_session_register_for_events.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &struct_i);
    dbus_message_iter_get_basic(&struct_i, &module_instance_id);
    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &event_id);
    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &event_config_payload_size);
    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &is_register);
    dbus_message_iter_next(&struct_i);
    dbus_message_iter_recurse(&struct_i, &array_i);
    dbus_message_iter_get_fixed_array(&array_i, addr_value, &n_elements);
    evt_reg_cfg = (struct agm_event_reg_cfg *)
                    calloc (1,(sizeof(struct agm_event_reg_cfg) +
                            (event_config_payload_size)*sizeof(uint8_t)));
    evt_reg_cfg->module_instance_id = module_instance_id;
    evt_reg_cfg->event_id = event_id;
    evt_reg_cfg->event_config_payload_size = event_config_payload_size;
    evt_reg_cfg->is_register = is_register;
    memcpy(&evt_reg_cfg->event_config_payload[0], value, n_elements);


    if (agm_session_register_for_events(session_id, evt_reg_cfg) != 0) {
        AGM_LOGE("agm_session_register_for_events failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_register_for_events failed.");
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_get_params(DBusConnection *conn,
                                       DBusMessage *msg,
                                       void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i;
    DBusMessageIter r_arg, r_array_i;
    uint32_t session_id, size;
    void *payload = NULL;
    char *value = NULL;
    char **addr_value = &value;
    int n_elements = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_get_params has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_get_params has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uuay")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_get_params.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                           "Invalid signature for ipc_agm_session_get_params.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &size);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &array_i);
    dbus_message_iter_get_fixed_array(&array_i, addr_value, &n_elements);
    payload = (void *)malloc(n_elements*sizeof(char));
    memcpy(payload, value, n_elements);

    if (agm_session_get_params(session_id, (void *)payload, size) != 0) {
        AGM_LOGE("agm_session_get_params failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_get_params failed.");
        free(payload);
        payload = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &r_arg);
    dbus_message_iter_open_container(&r_arg, DBUS_TYPE_ARRAY, "y", &r_array_i);
    dbus_message_iter_append_fixed_array(&r_array_i, DBUS_TYPE_BYTE, &payload,
                                         size);
    dbus_message_iter_close_container(&r_arg, &r_array_i);
    dbus_connection_send(conn, reply, NULL);
    free(payload);
    payload = NULL;
    dbus_message_unref(reply);
}

static void ipc_agm_session_aif_set_cal(DBusConnection *conn,
                                        DBusMessage *msg,
                                        void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i;
    struct agm_cal_config *cal_config = NULL;
    uint32_t session_id, aif_id, num_ckv;
    char *value = NULL;
    char **addr_value = &value;
    int n_elements = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_aif_set_cal has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_aif_set_cal has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uuuay")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_aif_set_cal.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                          "Invalid signature for ipc_agm_session_aif_set_cal.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &aif_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &num_ckv);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &array_i);
    dbus_message_iter_get_fixed_array(&array_i, addr_value, &n_elements);

    cal_config = (struct agm_cal_config *)
                        calloc (1, sizeof(struct agm_cal_config) +
                                num_ckv * sizeof(struct agm_key_value));
    cal_config->num_ckvs = num_ckv;
    memcpy(cal_config->kv, value,
                             cal_config->num_ckvs*sizeof(struct agm_key_value));

    if (agm_session_aif_set_cal(session_id, aif_id, cal_config)) {
        AGM_LOGE("agm_session_aif_set_cal failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_aif_set_cal failed.");
        free(cal_config);
        cal_config = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
    free(cal_config);
    cal_config = NULL;
}

static void ipc_agm_session_aif_set_params(DBusConnection *conn,
                                          DBusMessage *msg,
                                          void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i, r_arg;
    uint32_t session_id, aif_id, size;
    void *buf = NULL;
    char *value = NULL;
    char **addr_value = &value;
    int n_elements = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_aif_set_params has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_aif_set_params has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uuuay")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_aif_set_params.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                       "Invalid signature for ipc_agm_session_aif_set_params.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &aif_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &size);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &array_i);
    dbus_message_iter_get_fixed_array(&array_i, addr_value, &n_elements);
    buf = (void *)malloc(n_elements*sizeof(char));
    memcpy(buf, value, n_elements);

    if (agm_session_aif_set_params(session_id, aif_id, buf, size) != 0) {
        AGM_LOGE("agm_session_aif_set_params failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_aif_set_params failed.");
        free(buf);
        buf = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    free(buf);
    buf = NULL;
    dbus_message_unref(reply);
}

static void ipc_agm_session_aif_get_tag_module_info_size(DBusConnection *conn,
                                                         DBusMessage *msg,
                                                         void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i, r_arg;
    uint32_t session_id, aif_id, size;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
               "ipc_agm_session_aif_get_tag_module_info_size has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uuu")) {
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
         "Invalid signature for ipc_agm_session_aif_get_tag_module_info_size.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &aif_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &size);

    if (agm_session_aif_get_tag_module_info(session_id,
                                            aif_id,
                                            NULL,
                                            &size) != 0) {
        AGM_LOGE("agm_session_aif_get_tag_module_info failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_aif_get_tag_module_info failed.");
        return;
    }

    dbus_message_iter_init_append(reply, &r_arg);
    dbus_message_iter_append_basic(&r_arg, DBUS_TYPE_UINT32, &size);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_aif_get_tag_module_info(DBusConnection *conn,
                                                    DBusMessage *msg,
                                                    void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i, r_arg, r_array_i;
    uint32_t session_id, aif_id, size;
    void *buf = NULL;
    char *value = NULL;
    char **addr_value = &value;
    int n_elements = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_aif_get_tag_module_info has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                    "ipc_agm_session_aif_get_tag_module_info has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uuu")) {
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
              "Invalid signature for ipc_agm_session_aif_get_tag_module_info.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &aif_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &size);

    buf = calloc(size, sizeof(uint8_t));

    if (agm_session_aif_get_tag_module_info(session_id,
                                            aif_id,
                                            buf,
                                            &size) != 0) {
        AGM_LOGE("agm_session_aif_get_tag_module_info failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_aif_get_tag_module_info failed.");
        free(buf);
        buf = NULL;
        return;
    }

    dbus_message_iter_init_append(reply, &r_arg);
    dbus_message_iter_append_basic(&r_arg, DBUS_TYPE_UINT32, &size);
    dbus_message_iter_open_container(&r_arg, DBUS_TYPE_ARRAY, "y", &r_array_i);
    dbus_message_iter_append_fixed_array(&r_array_i, DBUS_TYPE_BYTE, &buf,
                                         size);
    dbus_message_iter_close_container(&r_arg, &r_array_i);
    dbus_connection_send(conn, reply, NULL);
    free(buf);
    buf = NULL;
    dbus_message_unref(reply);
}

static void ipc_agm_session_audio_inf_connect(DBusConnection *conn,
                                              DBusMessage *msg,
                                              void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i;
    uint32_t session_id, aif_id;
    dbus_bool_t state;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_audio_inf_connect has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                          "ipc_agm_session_audio_inf_connect has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uub")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_audio_inf_connect.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                    "Invalid signature for ipc_agm_session_audio_inf_connect.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &aif_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &state);

    if (agm_session_aif_connect(session_id, aif_id, (bool)state) != 0) {
        AGM_LOGE("agm_session_aif_connect failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_aif_connect failed.");
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_set_ec_ref(DBusConnection *conn,
                                       DBusMessage *msg,
                                       void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i;
    uint32_t capture_session_id, aif_id;
    dbus_bool_t state;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_set_ec_ref has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_set_ec_ref has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uub")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_set_ec_ref.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                           "Invalid signature for ipc_agm_session_set_ec_ref.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &capture_session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &aif_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &state);

    if (agm_session_set_ec_ref(capture_session_id, aif_id, (bool)state) != 0) {
        AGM_LOGE("agm_session_set_ec_ref failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_set_ec_ref failed.");
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_set_params(DBusConnection *conn,
                                       DBusMessage *msg,
                                       void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i;
    uint32_t session_id, size;
    void *payload = NULL;
    char *value = NULL;
    char **addr_value = &value;
    int n_elements = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_set_params has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_set_params has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uuay")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_set_params.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                           "Invalid signature for ipc_agm_session_set_params.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &size);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &array_i);
    dbus_message_iter_get_fixed_array(&array_i, addr_value, &n_elements);
    payload = (void *)malloc(n_elements*sizeof(char));
    memcpy(payload, value, n_elements);

    if (agm_session_set_params(session_id, (void *)payload, size) != 0) {
        AGM_LOGE("agm_session_set_params failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_set_params failed.");
        free(payload);
        payload = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    free(payload);
    payload = NULL;
    dbus_message_unref(reply);
}

static void ipc_agm_get_aif_info_list_size(DBusConnection *conn,
                                           DBusMessage *msg,
                                           void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter r_arg;
    agm_module_dbus_data *mdata = (agm_module_dbus_data *)userdata;
    size_t num_aif_info = 0;

    if (agm_get_aif_info_list(NULL, &num_aif_info) != 0) {
        AGM_LOGE("agm_get_aif_info_list failed");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_get_aif_info_list_size failed");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &r_arg);
    dbus_message_iter_append_basic(&r_arg, DBUS_TYPE_UINT32, &num_aif_info);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_get_aif_info_list(DBusConnection *conn,
                                      DBusMessage *msg,
                                      void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i;
    DBusMessageIter r_arg, array_i, struct_i;
    agm_module_dbus_data *mdata = (agm_module_dbus_data *)userdata;
    size_t num_aif_info;
    struct aif_info *aifinfo = NULL;
    char *name;
    int i = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_get_aif_info_list has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_get_aif_info_list has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "u")) {
        AGM_LOGE("Invalid signature for ipc_agm_get_aif_info_list.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "Invalid signature for ipc_agm_get_aif_info_list.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &num_aif_info);

    aifinfo = (struct aif_info *)calloc(num_aif_info, sizeof(struct aif_info));

    if (agm_get_aif_info_list(aifinfo, &num_aif_info)) {
        AGM_LOGE("agm_get_aif_info_list failed");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_get_aif_info_list failed");
        free(aifinfo);
        aifinfo = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &r_arg);
    dbus_message_iter_open_container(&r_arg, DBUS_TYPE_ARRAY, "(us)", &array_i);
    for (i = 0; i < num_aif_info; i++) {
        dbus_message_iter_open_container(&array_i, DBUS_TYPE_STRUCT, NULL,
                                         &struct_i);
        dbus_message_iter_append_basic(&struct_i, DBUS_TYPE_UINT32,
                                       &aifinfo[i].dir);
        name = aifinfo[i].aif_name;
        dbus_message_iter_append_basic(&struct_i, DBUS_TYPE_STRING, &name);
        dbus_message_iter_close_container(&array_i, &struct_i);
    }
    dbus_message_iter_close_container(&r_arg, &array_i);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
    free(aifinfo);
    aifinfo = NULL;
}

static void ipc_agm_set_params_with_tag(DBusConnection *conn,
                                        DBusMessage *msg,
                                        void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, struct_i, struct_ii, array_i;
    struct agm_tag_config *tag_config;
    uint32_t tag;
    uint32_t num_tkvs;
    uint32_t session_id, aif_id;
    uint32_t i = 0;
    int arg_type;
    size_t size_local = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_set_params_with_tag has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_set_params_with_tag has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uu(uua(uu))")) {
        AGM_LOGE("Invalid signature for ipc_agm_set_params_with_tag.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                          "Invalid signature for ipc_agm_set_params_with_tag.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &aif_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &struct_i);
    dbus_message_iter_get_basic(&struct_i, &tag);
    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &num_tkvs);
    dbus_message_iter_next(&struct_i);
    size_local = (sizeof(struct agm_tag_config) +
                        (num_tkvs) * sizeof(agm_key_value));
    tag_config = (struct agm_tag_config *) calloc(1,size_local);
    tag_config->tag = tag;
    tag_config->num_tkvs = num_tkvs;
    dbus_message_iter_recurse(&struct_i, &array_i);
    while (((arg_type = dbus_message_iter_get_arg_type(&array_i))
                                                != DBUS_TYPE_INVALID)
             && (i < tag_config->num_tkvs)) {
        dbus_message_iter_recurse(&array_i, &struct_ii);
        dbus_message_iter_get_basic(&struct_ii, &tag_config->kv[i].key);
        dbus_message_iter_next(&struct_ii);
        dbus_message_iter_get_basic(&struct_ii, &tag_config->kv[i].value);
        dbus_message_iter_next(&array_i);
        i++;
    }

    if (agm_set_params_with_tag(session_id, aif_id, tag_config)) {
        AGM_LOGE("agm_set_params_with_tag failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_set_params_with_tag failed.");
        free(tag_config);
        tag_config = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
    free(tag_config);
    tag_config = NULL;
}

static void ipc_agm_session_set_loopback(DBusConnection *conn,
                                         DBusMessage *msg,
                                         void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i;
    uint32_t capture_session_id, playback_session_id;
    dbus_bool_t flag;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_set_loopback has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_set_loopback has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uub")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_set_loopback.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                         "Invalid signature for ipc_agm_session_set_loopback.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &capture_session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &playback_session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &flag);

    if (agm_session_set_loopback(capture_session_id,
                                 playback_session_id,
                                 (bool)flag) != 0) {
        AGM_LOGE("agm_session_set_loopback failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_set_loopback failed.");
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_set_metadata(DBusConnection *conn,
                                         DBusMessage *msg,
                                         void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i;
    uint32_t session_id, size;
    void *metadata;
    char *value = NULL;
    char **addr_value = &value;
    int n_elements = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_set_metadata has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_set_metadata has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uuay")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_set_metadata.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                         "Invalid signature for ipc_agm_session_set_metadata.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &size);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &array_i);
    dbus_message_iter_get_fixed_array(&array_i, addr_value, &n_elements);
    metadata = (void *)malloc(n_elements*sizeof(char));
    memcpy(metadata, value, n_elements);

    if (agm_session_set_metadata(session_id, size, (uint8_t *)metadata) != 0) {
        AGM_LOGE("agm_session_set_metadata failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_set_metadata failed.");
        free(metadata);
        metadata = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    free(metadata);
    metadata = NULL;
    dbus_message_unref(reply);
}

static void ipc_agm_session_audio_inf_set_metadata(DBusConnection *conn,
                                                   DBusMessage *msg,
                                                   void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i;
    uint32_t session_id, aif_id, size;
    void *metadata;
    char *value = NULL;
    char **addr_value = &value;
    int n_elements = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_audio_inf_set_metadata has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                     "ipc_agm_session_audio_inf_set_metadata has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uuuay")) {
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
               "Invalid signature for ipc_agm_session_audio_inf_set_metadata.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &aif_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &size);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &array_i);
    dbus_message_iter_get_fixed_array(&array_i, addr_value, &n_elements);
    metadata = (void *)malloc(n_elements*sizeof(char));
    memcpy(metadata, value, n_elements);

    if (agm_session_aif_set_metadata(session_id,
                                     aif_id,
                                     size,
                                     (uint8_t *)metadata) != 0) {
        AGM_LOGE("agm_session_aif_set_metadata failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_aif_set_metadata failed.");
        free(metadata);
        metadata = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    free(metadata);
    metadata = NULL;
    dbus_message_unref(reply);
}

static void ipc_agm_audio_intf_set_metadata(DBusConnection *conn,
                                            DBusMessage *msg,
                                            void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i;
    uint32_t aif_id, size;
    void *metadata;
    char *value = NULL;
    char **addr_value = &value;
    int n_elements = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_audio_intf_set_metadata has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_audio_intf_set_metadata has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uuay")) {
        AGM_LOGE("Invalid signature for ipc_agm_audio_intf_set_metadata.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                      "Invalid signature for ipc_agm_audio_intf_set_metadata.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &aif_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_get_basic(&arg_i, &size);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &array_i);
    dbus_message_iter_get_fixed_array(&array_i, addr_value, &n_elements);
    metadata = (void *)malloc(n_elements*sizeof(char));
    memcpy(metadata, value, n_elements);

    if (agm_aif_set_metadata(aif_id, size, (uint8_t *)metadata) != 0) {
        AGM_LOGE("agm_aif_set_metadata failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_aif_set_metadata failed.");
        free(metadata);
        metadata = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    free(metadata);
    metadata = NULL;
    dbus_message_unref(reply);
}

static void ipc_agm_audio_intf_set_media_config(DBusConnection *conn,
                                                DBusMessage *msg,
                                                void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, struct_i;
    struct agm_media_config media_config;
    uint32_t aif_id;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_audio_intf_set_media_config has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                        "ipc_agm_audio_intf_set_media_config has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "u(uui)")) {
        AGM_LOGE("Invalid signature for ipc_agm_audio_intf_set_media_config.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                  "Invalid signature for ipc_agm_audio_intf_set_media_config.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &aif_id);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &struct_i);
    dbus_message_iter_get_basic(&struct_i, &media_config.rate);
    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &media_config.channels);
    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &media_config.format);

    if (agm_aif_set_media_config(aif_id, &media_config)) {
        AGM_LOGE("agm_aif_set_media_config failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_aif_set_media_config failed.");
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_get_buffer_timestamp(DBusConnection *conn,
                                         DBusMessage *msg,
                                         void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i;
    DBusMessageIter r_arg, r_array_i;
    uint64_t timestamp = 0;
    uint32_t session_id;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_get_buffer_timestamp has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_get_buffer_timestamp has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "u")) {
        AGM_LOGE("Invalid signature for ipc_agm_get_buffer_timestamp.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                         "Invalid signature for ipc_agm_get_buffer_timestamp.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);

    if (agm_get_buffer_timestamp(session_id, &timestamp)) {
        AGM_LOGE("agm_get_buffer_timestamp failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_get_buffer_timestamp failed.");
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &r_arg);
    dbus_message_iter_append_basic(&r_arg, DBUS_TYPE_UINT64, &timestamp);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_get_hw_processed_buff_cnt(DBusConnection *conn,
                                              DBusMessage *msg,
                                              void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i;
    DBusMessageIter r_arg, r_array_i;
    agm_session_data *ses_data = (agm_session_data *)userdata;
    uint32_t direction;
    size_t buf_count;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_get_hw_processed_buff_cnt has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                          "ipc_agm_get_hw_processed_buff_cnt has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "u")) {
        AGM_LOGE("Invalid signature for ipc_agm_get_hw_processed_buff_cnt.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                    "Invalid signature for ipc_agm_get_hw_processed_buff_cnt.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &direction);

    buf_count = agm_get_hw_processed_buff_cnt(ses_data->handle,
                                              (enum direction)direction);

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &r_arg);
    dbus_message_iter_append_basic(&r_arg, DBUS_TYPE_INT32, &buf_count);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_get_session_time(DBusConnection *conn,
                                     DBusMessage *msg,
                                     void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter r_arg;
    agm_session_data *ses_data = (agm_session_data *)userdata;
    uint64_t timestamp;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (agm_get_session_time(ses_data->handle, &timestamp)) {
        AGM_LOGE("agm_get_session_time failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_get_session_time failed.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &r_arg);
    dbus_message_iter_append_basic(&r_arg, DBUS_TYPE_UINT64, &timestamp);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_set_config(DBusConnection *conn,
                                       DBusMessage *msg,
                                       void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i, struct_i, struct_ii;
    agm_session_data *ses_data = (agm_session_data *)userdata;
    char *value = NULL;
    char **addr_value = &value;
    int n_elements = 0;
    struct agm_session_config session_config;
    struct agm_media_config media_config;
    struct agm_buffer_config buffer_config;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_set_config has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_set_config has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "(uuu)(uu)ay")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_set_config.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                           "Invalid signature for ipc_agm_session_set_config.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_recurse(&arg_i, &struct_i);
    dbus_message_iter_get_basic(&struct_i, &media_config.rate);
    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &media_config.channels);
    dbus_message_iter_next(&struct_i);
    dbus_message_iter_get_basic(&struct_i, &media_config.format);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &struct_ii);
    dbus_message_iter_get_basic(&struct_ii, &buffer_config.count);
    dbus_message_iter_next(&struct_ii);
    dbus_message_iter_get_basic(&struct_ii, &buffer_config.size);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &array_i);
    dbus_message_iter_get_fixed_array(&array_i, addr_value, &n_elements);
    memcpy(&session_config, value, n_elements);

    if (agm_session_set_config(ses_data->handle,
                               &session_config,
                               &media_config,
                               &buffer_config)) {
        AGM_LOGE("agm_session_set_config failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_set_config failed.");
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_eos(DBusConnection *conn,
                                DBusMessage *msg,
                                void *userdata) {
    DBusMessage *reply = NULL;
    agm_session_data *ses_data = (agm_session_data *)userdata;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (agm_session_eos(ses_data->handle)) {
        AGM_LOGE("agm_session_eos failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_eos failed.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_write(DBusConnection *conn,
                                  DBusMessage *msg,
                                  void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i, array_i, r_arg;
    agm_session_data *ses_data = (agm_session_data *)userdata;
    uint32_t buf_size;
    void *buf;
    char *value = NULL;
    char **addr_value = &value;
    int n_elements = 0;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_write has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_write has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "uay")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_write.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "Invalid signature for ipc_agm_session_write.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &buf_size);
    dbus_message_iter_next(&arg_i);
    dbus_message_iter_recurse(&arg_i, &array_i);
    dbus_message_iter_get_fixed_array(&array_i, addr_value, &n_elements);
    buf = (void *)malloc(n_elements*sizeof(char));
    memcpy(buf, value, n_elements);

    if (agm_session_write(ses_data->handle, buf, (size_t *) &buf_size)) {
        AGM_LOGE("agm_session_write failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_write failed.");
        free(buf);
        buf = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &r_arg);
    dbus_message_iter_append_basic(&r_arg, DBUS_TYPE_UINT32, &buf_size);
    dbus_connection_send(conn, reply, NULL);
    free(buf);
    buf = NULL;
    dbus_message_unref(reply);
}

static void ipc_agm_session_read(DBusConnection *conn,
                                 DBusMessage *msg,
                                 void *userdata) {
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i;
    DBusMessageIter r_arg, r_array_i;
    uint32_t buf_size;
    agm_session_data *ses_data = (agm_session_data *)userdata;
    void *buf = NULL;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_read has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_read has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "u")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_read.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "Invalid signature for ipc_agm_session_read.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &buf_size);

    buf = (void *)malloc(buf_size);

    if (agm_session_read(ses_data->handle, buf, (size_t *) &buf_size)) {
        AGM_LOGE("agm_session_read failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_read failed.");
        free(buf);
        buf = NULL;
        return;
    }

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &r_arg);
    dbus_message_iter_open_container(&r_arg, DBUS_TYPE_ARRAY, "y", &r_array_i);
    dbus_message_iter_append_fixed_array(&r_array_i, DBUS_TYPE_BYTE, &buf,
                                         buf_size);
    dbus_message_iter_close_container(&r_arg, &r_array_i);
    dbus_connection_send(conn, reply, NULL);
    free(buf);
    buf = NULL;
    dbus_message_unref(reply);
}

static void ipc_agm_session_resume(DBusConnection *conn,
                                   DBusMessage *msg,
                                   void *userdata) {
    DBusMessage *reply = NULL;
    agm_session_data *ses_data = (agm_session_data *)userdata;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (agm_session_resume(ses_data->handle)) {
        AGM_LOGE("agm_session_resume failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_resume failed.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_pause(DBusConnection *conn,
                                  DBusMessage *msg,
                                  void *userdata) {
    DBusMessage *reply = NULL;
    agm_session_data *ses_data = (agm_session_data *)userdata;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (agm_session_pause(ses_data->handle)) {
        AGM_LOGE("agm_session_pause failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_pause failed.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_stop(DBusConnection *conn,
                                 DBusMessage *msg,
                                 void *userdata) {
    DBusMessage *reply = NULL;
    agm_session_data *ses_data = (agm_session_data *)userdata;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (agm_session_stop(ses_data->handle)) {
        AGM_LOGE("agm_session_stop failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_stop failed.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_start(DBusConnection *conn,
                                  DBusMessage *msg,
                                  void *userdata) {
    DBusMessage *reply = NULL;
    agm_session_data *ses_data = (agm_session_data *)userdata;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (agm_session_start(ses_data->handle)) {
        AGM_LOGE("agm_session_start failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_start failed.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_prepare(DBusConnection *conn,
                                    DBusMessage *msg,
                                    void *userdata) {
    DBusMessage *reply = NULL;
    agm_session_data *ses_data = (agm_session_data *)userdata;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (agm_session_prepare(ses_data->handle)) {
        AGM_LOGE("agm_session_prepare failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_prepare failed.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_close(DBusConnection *conn,
                                  DBusMessage *msg,
                                  void *userdata) {
    DBusMessage *reply = NULL;
    agm_session_data *ses_data = (agm_session_data *)userdata;
    GList *node = NULL;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_connection_remove_filter(conn, disconnection_filter_cb, ses_data);

    if (agm_session_close(ses_data->handle)) {
        AGM_LOGE("agm_session_close failed.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_close failed.");
        return;
    }

    for (node = ses_data->callbacks; node != NULL; node = node->next) {
        if (node->data != NULL) {
            free(node->data);
            node->data = NULL;
        }
    }

    g_list_free(ses_data->callbacks);

    if (agm_dbus_remove_interface(mdata->conn,
                                  ses_data->dbus_obj_path,
                                  session_interface_info.name)) {
        AGM_LOGE("Unable to remove interface");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                       "agm_session_close failed. Failed to remove interface.");
        return;
    }

    g_hash_table_remove(mdata->sessions,
                        GUINT_TO_POINTER(ses_data->session_id));
    reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void ipc_agm_session_open(DBusConnection *conn,
                                 DBusMessage *msg,
                                 void *userdata) {
    agm_module_dbus_data *mdata = (agm_module_dbus_data *)userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter arg_i;
    uint32_t session_id;
    char *dbus_obj_path = NULL;
    agm_session_data *ses_data = NULL;

    if (userdata == NULL) {
        AGM_LOGE("Invalid userdata");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "userdata is NULL");
        return;
    }

    if (!dbus_message_iter_init(msg, &arg_i)) {
        AGM_LOGE("ipc_agm_session_open has no arguments");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "ipc_agm_session_open has no arguments");
        return;
    }

    if (strcmp(dbus_message_get_signature(msg), "u")) {
        AGM_LOGE("Invalid signature for ipc_agm_session_open.");
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "Invalid signature for ipc_agm_session_open.");
        return;
    }

    AGM_LOGV("%s : ", __func__);

    dbus_message_iter_get_basic(&arg_i, &session_id);

    ses_data = get_session_data(mdata, session_id);

    if (agm_session_open(session_id, &ses_data->handle)) {
        agm_free_session(ses_data);
        agm_dbus_send_error(mdata->conn, msg, DBUS_ERROR_FAILED,
                            "agm_session_open failed.");
        return;
    }

    if (!dbus_connection_add_filter(conn,
                                    disconnection_filter_cb,
                                    ses_data,
                                    NULL))
        AGM_LOGE("Unable to add death notification filter");

    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &arg_i);
    dbus_message_iter_append_basic(&arg_i,
                                   DBUS_TYPE_OBJECT_PATH,
                                   &ses_data->dbus_obj_path);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

/* Initialize module data. Get dbus connection and register module interface
    with the connection */
int ipc_agm_init() {
    DBusError err;
    int rc = 0;

    AGM_LOGV("%s : ", __func__);

    mdata = (agm_module_dbus_data *)malloc(sizeof(agm_module_dbus_data));
    mdata->dbus_obj_path =
                    (char *)malloc(sizeof(char)*(strlen(AGM_OBJECT_PATH) + 1));
    memcpy(mdata->dbus_obj_path, AGM_OBJECT_PATH, strlen(AGM_OBJECT_PATH)+1);

    mdata->conn = agm_dbus_new_connection();

    dbus_error_init(&err);
    rc = dbus_bus_request_name(mdata->conn->conn,
                               AGM_DBUS_CONNECTION,
                               DBUS_NAME_FLAG_REPLACE_EXISTING,
                               &err);

    if (rc != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        AGM_LOGE("Failed to request name on bus: %s", err.message);
        agm_dbus_connection_free(mdata->conn);
        free(mdata->dbus_obj_path);
        mdata->dbus_obj_path = NULL;
        free(mdata);
        mdata = NULL;
        rc = -EINVAL;
        return rc;
    }

    if (agm_setup_dbus_with_main_loop(mdata->conn)) {
        AGM_LOGE("Failed to setup main loop");
        agm_dbus_connection_free(mdata->conn);
        free(mdata->dbus_obj_path);
        mdata->dbus_obj_path = NULL;
        free(mdata);
        mdata = NULL;
        rc = -EINVAL;
        return rc;
    }

    if ((rc = agm_dbus_add_interface(mdata->conn,
                                     mdata->dbus_obj_path,
                                     &module_interface_info,
                                     mdata)) != 0) {
        AGM_LOGE("Failed to add interface");
        agm_dbus_connection_free(mdata->conn);
        free(mdata->dbus_obj_path);
        mdata->dbus_obj_path = NULL;
        free(mdata);
        mdata = NULL;
        rc = -EINVAL;
        return rc;
    }

    mdata->sessions = g_hash_table_new_full(g_direct_hash,
                                            g_direct_equal,
                                            NULL,
                                            agm_free_session);

    if ((rc = agm_init()) != 0) {
        AGM_LOGE("agm initialization failed");
        agm_dbus_connection_free(mdata->conn);
        free(mdata->dbus_obj_path);
        mdata->dbus_obj_path = NULL;
        free(mdata);
        mdata = NULL;
        return rc;
    }

    dbus_error_free(&err);
    return rc;
}

void ipc_agm_deinit() {
    AGM_LOGV("%s : ", __func__);

    if (mdata == NULL) {
        AGM_LOGE("ipc_agm_deinit failed");
        g_hash_table_remove_all(mdata->sessions);
        g_hash_table_unref(mdata->sessions);
    }

    if (mdata->dbus_obj_path != NULL) {
        if (agm_dbus_remove_interface(mdata->conn,
                                      mdata->dbus_obj_path,
                                      module_interface_info.name) != 0)
            AGM_LOGE("Unable to remove interface");
        free(mdata->dbus_obj_path);
        mdata->dbus_obj_path = NULL;
    }

    agm_dbus_connection_free(mdata->conn);
    free(mdata);
    mdata = NULL;

    if (agm_deinit() != 0)
        AGM_LOGE("agm deinitialization failed");
}
