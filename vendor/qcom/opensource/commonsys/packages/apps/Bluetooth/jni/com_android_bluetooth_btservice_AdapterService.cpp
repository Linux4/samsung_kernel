/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "BluetoothServiceJni"
#include "com_android_bluetooth.h"
#include "hardware/bt_sock.h"
#include "bt_features.h"
#ifdef ADV_AUDIO_FEATURE
#include "com_android_bluetooth_ext.h"
#endif
#include "utils/Log.h"
#include "utils/misc.h"

#include <cutils/properties.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/stat.h>

#include <hardware/bluetooth.h>
#include <nativehelper/JNIPlatformHelp.h>
#include <mutex>

#include <pthread.h>

using bluetooth::Uuid;

namespace android {
// Both
// OOB_ADDRESS_SIZE is 6 bytes address + 1 byte address type
#define OOB_ADDRESS_SIZE 7
#define OOB_C_SIZE 16
#define OOB_R_SIZE 16
#define OOB_NAME_MAX_SIZE 256
// Classic
#define OOB_DATA_LEN_SIZE 2
#define OOB_COD_SIZE 3
// LE
#define OOB_TK_SIZE 16
#define OOB_LE_FLAG_SIZE 1
#define OOB_LE_ROLE_SIZE 1
#define OOB_LE_APPEARANCE_SIZE 2

#define TRANSPORT_AUTO 0
#define TRANSPORT_BREDR 1
#define TRANSPORT_LE 2

const jint INVALID_FD = -1;

static jmethodID method_oobDataReceivedCallback;
static jmethodID method_stateChangeCallback;
static jmethodID method_adapterPropertyChangedCallback;
static jmethodID method_devicePropertyChangedCallback;
static jmethodID method_deviceFoundCallback;
static jmethodID method_pinRequestCallback;
static jmethodID method_sspRequestCallback;
static jmethodID method_bondStateChangeCallback;
static jmethodID method_aclStateChangeCallback;
static jmethodID method_discoveryStateChangeCallback;
static jmethodID method_setWakeAlarm;
static jmethodID method_acquireWakeLock;
static jmethodID method_releaseWakeLock;
static jmethodID method_energyInfo;

static struct {
  jclass clazz;
  jmethodID constructor;
} android_bluetooth_UidTraffic;

static const bt_interface_t *sBluetoothInterface = NULL;
static const btsock_interface_t *sBluetoothSocketInterface = NULL;
static JavaVM* vm = NULL;
static JNIEnv *callbackEnv = NULL;
static pthread_t sCallbackThread;
static bool sHaveCallbackThread;

static jobject sJniAdapterServiceObj = NULL;
static jobject sJniCallbacksObj = NULL;
static jfieldID sJniCallbacksField;

const bt_interface_t* getBluetoothInterface() { return sBluetoothInterface; }

JNIEnv* getCallbackEnv() { return callbackEnv; }

bool isCallbackThread() {
  return sHaveCallbackThread && pthread_equal(sCallbackThread, pthread_self());
}

static void adapter_state_change_callback(bt_state_t status) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;
  ALOGV("%s: Status is: %d", __func__, status);

  sCallbackEnv->CallVoidMethod(sJniCallbacksObj, method_stateChangeCallback,
                               (jint)status);
}

static int get_properties(int num_properties, bt_property_t* properties,
                          jintArray* types, jobjectArray* props) {
  for (int i = 0; i < num_properties; i++) {
    ScopedLocalRef<jbyteArray> propVal(
        callbackEnv, callbackEnv->NewByteArray(properties[i].len));
    if (!propVal.get()) {
      ALOGE("Error while allocation of array in %s", __func__);
      return -1;
    }

    callbackEnv->SetByteArrayRegion(propVal.get(), 0, properties[i].len,
                                    (jbyte*)properties[i].val);
    callbackEnv->SetObjectArrayElement(*props, i, propVal.get());
    callbackEnv->SetIntArrayRegion(*types, i, 1, (jint*)&properties[i].type);
  }
  return 0;
}

static void adapter_properties_callback(bt_status_t status, int num_properties,
                                        bt_property_t* properties) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  ALOGV("%s: Status is: %d, Properties: %d", __func__, status, num_properties);

  if (status != BT_STATUS_SUCCESS) {
    ALOGE("%s: Status %d is incorrect", __func__, status);
    return;
  }

  ScopedLocalRef<jbyteArray> val(
      sCallbackEnv.get(),
      (jbyteArray)sCallbackEnv->NewByteArray(num_properties));
  if (!val.get()) {
    ALOGE("%s: Error allocating byteArray", __func__);
    return;
  }

  ScopedLocalRef<jclass> mclass(sCallbackEnv.get(),
                                sCallbackEnv->GetObjectClass(val.get()));

  /* (BT) Initialize the jobjectArray and jintArray here itself and send the
   initialized array pointers alone to get_properties */

  ScopedLocalRef<jobjectArray> props(
      sCallbackEnv.get(),
      sCallbackEnv->NewObjectArray(num_properties, mclass.get(), NULL));
  if (!props.get()) {
    ALOGE("%s: Error allocating object Array for properties", __func__);
    return;
  }

  ScopedLocalRef<jintArray> types(
      sCallbackEnv.get(), (jintArray)sCallbackEnv->NewIntArray(num_properties));
  if (!types.get()) {
    ALOGE("%s: Error allocating int Array for values", __func__);
    return;
  }

  jintArray typesPtr = types.get();
  jobjectArray propsPtr = props.get();
  if (get_properties(num_properties, properties, &typesPtr, &propsPtr) < 0) {
    return;
  }

  sCallbackEnv->CallVoidMethod(sJniCallbacksObj,
                               method_adapterPropertyChangedCallback,
                               types.get(), props.get());
}

static void remote_device_properties_callback(bt_status_t status,
                                              RawAddress* bd_addr,
                                              int num_properties,
                                              bt_property_t* properties) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  ALOGV("%s: Status is: %d, Properties: %d", __func__, status, num_properties);

  if (status != BT_STATUS_SUCCESS) {
    ALOGE("%s: Status %d is incorrect", __func__, status);
    return;
  }

  ScopedLocalRef<jbyteArray> val(
      sCallbackEnv.get(),
      (jbyteArray)sCallbackEnv->NewByteArray(num_properties));
  if (!val.get()) {
    ALOGE("%s: Error allocating byteArray", __func__);
    return;
  }

  ScopedLocalRef<jclass> mclass(sCallbackEnv.get(),
                                sCallbackEnv->GetObjectClass(val.get()));

  /* Initialize the jobjectArray and jintArray here itself and send the
   initialized array pointers alone to get_properties */

  ScopedLocalRef<jobjectArray> props(
      sCallbackEnv.get(),
      sCallbackEnv->NewObjectArray(num_properties, mclass.get(), NULL));
  if (!props.get()) {
    ALOGE("%s: Error allocating object Array for properties", __func__);
    return;
  }

  ScopedLocalRef<jintArray> types(
      sCallbackEnv.get(), (jintArray)sCallbackEnv->NewIntArray(num_properties));
  if (!types.get()) {
    ALOGE("%s: Error allocating int Array for values", __func__);
    return;
  }

  ScopedLocalRef<jbyteArray> addr(
      sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(RawAddress)));
  if (!addr.get()) {
    ALOGE("Error while allocation byte array in %s", __func__);
    return;
  }

  sCallbackEnv->SetByteArrayRegion(addr.get(), 0, sizeof(RawAddress),
                                   (jbyte*)bd_addr);

  jintArray typesPtr = types.get();
  jobjectArray propsPtr = props.get();
  if (get_properties(num_properties, properties, &typesPtr, &propsPtr) < 0) {
    return;
  }

  sCallbackEnv->CallVoidMethod(sJniCallbacksObj,
                               method_devicePropertyChangedCallback, addr.get(),
                               types.get(), props.get());
}

static void device_found_callback(int num_properties,
                                  bt_property_t* properties) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  ScopedLocalRef<jbyteArray> addr(sCallbackEnv.get(), NULL);
  int addr_index;
  for (int i = 0; i < num_properties; i++) {
    if (properties[i].type == BT_PROPERTY_BDADDR) {
      addr.reset(sCallbackEnv->NewByteArray(properties[i].len));
      if (!addr.get()) {
        ALOGE("Address is NULL (unable to allocate) in %s", __func__);
        return;
      }
      sCallbackEnv->SetByteArrayRegion(addr.get(), 0, properties[i].len,
                                       (jbyte*)properties[i].val);
      addr_index = i;
    }
  }
  if (!addr.get()) {
    ALOGE("Address is NULL in %s", __func__);
    return;
  }

  ALOGV("%s: Properties: %d, Address: %s", __func__, num_properties,
        (const char*)properties[addr_index].val);

  remote_device_properties_callback(BT_STATUS_SUCCESS,
                                    (RawAddress*)properties[addr_index].val,
                                    num_properties, properties);

  sCallbackEnv->CallVoidMethod(sJniCallbacksObj, method_deviceFoundCallback,
                               addr.get());
}

static void bond_state_changed_callback(bt_status_t status, RawAddress* bd_addr,
                                        bt_bond_state_t state,
                                        int fail_reason) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  if (!bd_addr) {
    ALOGE("Address is null in %s", __func__);
    return;
  }

  ScopedLocalRef<jbyteArray> addr(
      sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(RawAddress)));
  if (!addr.get()) {
    ALOGE("Address allocation failed in %s", __func__);
    return;
  }
  sCallbackEnv->SetByteArrayRegion(addr.get(), 0, sizeof(RawAddress),
                                   (jbyte*)bd_addr);

  sCallbackEnv->CallVoidMethod(sJniCallbacksObj, method_bondStateChangeCallback,
                               (jint)status, addr.get(), (jint)state,
                               (jint)fail_reason);
}

static void acl_state_changed_callback(bt_status_t status, RawAddress* bd_addr,
                                       bt_acl_state_t state,
                                       int transport_link_type,
                                       bt_hci_error_code_t hci_reason) {
  if (!bd_addr) {
    ALOGE("Address is null in %s", __func__);
    return;
  }

  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  ScopedLocalRef<jbyteArray> addr(
      sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(RawAddress)));
  if (!addr.get()) {
    ALOGE("Address allocation failed in %s", __func__);
    return;
  }
  sCallbackEnv->SetByteArrayRegion(addr.get(), 0, sizeof(RawAddress),
                                   (jbyte*)bd_addr);

  sCallbackEnv->CallVoidMethod(sJniCallbacksObj, method_aclStateChangeCallback,
                               (jint)status, addr.get(), (jint)state,
                               (jint)transport_link_type, (jint)hci_reason);
}

static void discovery_state_changed_callback(bt_discovery_state_t state) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  ALOGV("%s: DiscoveryState:%d ", __func__, state);

  sCallbackEnv->CallVoidMethod(
      sJniCallbacksObj, method_discoveryStateChangeCallback, (jint)state);
}

static void pin_request_callback(RawAddress* bd_addr, bt_bdname_t* bdname,
                                 uint32_t cod, bool min_16_digits) {
  if (!bd_addr) {
    ALOGE("Address is null in %s", __func__);
    return;
  }

  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  ScopedLocalRef<jbyteArray> addr(
      sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(RawAddress)));
  if (!addr.get()) {
    ALOGE("Error while allocating in: %s", __func__);
    return;
  }

  sCallbackEnv->SetByteArrayRegion(addr.get(), 0, sizeof(RawAddress),
                                   (jbyte*)bd_addr);

  ScopedLocalRef<jbyteArray> devname(
      sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(bt_bdname_t)));
  if (!devname.get()) {
    ALOGE("Error while allocating in: %s", __func__);
    return;
  }

  sCallbackEnv->SetByteArrayRegion(devname.get(), 0, sizeof(bt_bdname_t),
                                   (jbyte*)bdname);

  sCallbackEnv->CallVoidMethod(sJniCallbacksObj, method_pinRequestCallback,
                               addr.get(), devname.get(), cod, min_16_digits);
}

static void ssp_request_callback(RawAddress* bd_addr, bt_bdname_t* bdname,
                                 uint32_t cod, bt_ssp_variant_t pairing_variant,
                                 uint32_t pass_key) {
  if (!bd_addr) {
    ALOGE("Address is null in %s", __func__);
    return;
  }
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  ScopedLocalRef<jbyteArray> addr(
      sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(RawAddress)));
  if (!addr.get()) {
    ALOGE("Error while allocating in: %s", __func__);
    return;
  }

  sCallbackEnv->SetByteArrayRegion(addr.get(), 0, sizeof(RawAddress),
                                   (jbyte*)bd_addr);

  ScopedLocalRef<jbyteArray> devname(
      sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(bt_bdname_t)));
  if (!devname.get()) {
    ALOGE("Error while allocating in: %s", __func__);
    return;
  }

  sCallbackEnv->SetByteArrayRegion(devname.get(), 0, sizeof(bt_bdname_t),
                                   (jbyte*)bdname);

  sCallbackEnv->CallVoidMethod(sJniCallbacksObj, method_sspRequestCallback,
                               addr.get(), devname.get(), cod,
                               (jint)pairing_variant, pass_key);
}

static jobject createClassicOobDataObject(JNIEnv* env, bt_oob_data_t oob_data) {
  ALOGV("%s", __func__);
  jclass classicBuilderClass =
      env->FindClass("android/bluetooth/OobData$ClassicBuilder");

  jbyteArray confirmationHash = env->NewByteArray(OOB_C_SIZE);
  env->SetByteArrayRegion(confirmationHash, 0, OOB_C_SIZE,
                          reinterpret_cast<jbyte*>(oob_data.c));

  jbyteArray oobDataLength = env->NewByteArray(OOB_DATA_LEN_SIZE);
  env->SetByteArrayRegion(oobDataLength, 0, OOB_DATA_LEN_SIZE,
                          reinterpret_cast<jbyte*>(oob_data.oob_data_length));

  jbyteArray address = env->NewByteArray(OOB_ADDRESS_SIZE);
  env->SetByteArrayRegion(address, 0, OOB_ADDRESS_SIZE,
                          reinterpret_cast<jbyte*>(oob_data.address));

  jmethodID classicBuilderConstructor =
      env->GetMethodID(classicBuilderClass, "<init>", "([B[B[B)V");

  jobject oobDataClassicBuilder =
      env->NewObject(classicBuilderClass, classicBuilderConstructor,
                     confirmationHash, oobDataLength, address);

  jmethodID setRMethod =
      env->GetMethodID(classicBuilderClass, "setRandomizerHash",
                       "([B)Landroid/bluetooth/OobData$ClassicBuilder;");
  jbyteArray randomizerHash = env->NewByteArray(OOB_R_SIZE);
  env->SetByteArrayRegion(randomizerHash, 0, OOB_R_SIZE,
                          reinterpret_cast<jbyte*>(oob_data.r));

  oobDataClassicBuilder =
      env->CallObjectMethod(oobDataClassicBuilder, setRMethod, randomizerHash);

  jmethodID setNameMethod =
      env->GetMethodID(classicBuilderClass, "setDeviceName",
                       "([B)Landroid/bluetooth/OobData$ClassicBuilder;");

  int name_char_count = 0;
  for (int i = 0; i < OOB_NAME_MAX_SIZE; i++) {
    if (oob_data.device_name[i] == 0) {
      name_char_count = i;
      break;
    }
  }

  jbyteArray deviceName = env->NewByteArray(name_char_count);
  env->SetByteArrayRegion(deviceName, 0, name_char_count,
                          reinterpret_cast<jbyte*>(oob_data.device_name));

  oobDataClassicBuilder =
      env->CallObjectMethod(oobDataClassicBuilder, setNameMethod, deviceName);

  jmethodID buildMethod = env->GetMethodID(classicBuilderClass, "build",
                                           "()Landroid/bluetooth/OobData;");

  return env->CallObjectMethod(oobDataClassicBuilder, buildMethod);
}

static jobject createLeOobDataObject(JNIEnv* env, bt_oob_data_t oob_data) {
  ALOGV("%s", __func__);

  jclass leBuilderClass = env->FindClass("android/bluetooth/OobData$LeBuilder");

  jbyteArray confirmationHash = env->NewByteArray(OOB_C_SIZE);
  env->SetByteArrayRegion(confirmationHash, 0, OOB_C_SIZE,
                          reinterpret_cast<jbyte*>(oob_data.c));

  jbyteArray address = env->NewByteArray(OOB_ADDRESS_SIZE);
  env->SetByteArrayRegion(address, 0, OOB_ADDRESS_SIZE,
                          reinterpret_cast<jbyte*>(oob_data.address));

  jint le_role = (jint)oob_data.le_device_role;

  jmethodID leBuilderConstructor =
      env->GetMethodID(leBuilderClass, "<init>", "([B[BI)V");

  jobject oobDataLeBuilder = env->NewObject(
      leBuilderClass, leBuilderConstructor, confirmationHash, address, le_role);

  jmethodID setRMethod =
      env->GetMethodID(leBuilderClass, "setRandomizerHash",
                       "([B)Landroid/bluetooth/OobData$LeBuilder;");
  jbyteArray randomizerHash = env->NewByteArray(OOB_R_SIZE);
  env->SetByteArrayRegion(randomizerHash, 0, OOB_R_SIZE,
                          reinterpret_cast<jbyte*>(oob_data.r));

  oobDataLeBuilder =
      env->CallObjectMethod(oobDataLeBuilder, setRMethod, randomizerHash);

  jmethodID setNameMethod =
      env->GetMethodID(leBuilderClass, "setDeviceName",
                       "([B)Landroid/bluetooth/OobData$LeBuilder;");

  int name_char_count = 0;
  for (int i = 0; i < OOB_NAME_MAX_SIZE; i++) {
    if (oob_data.device_name[i] == 0) {
      name_char_count = i;
      break;
    }
  }

  jbyteArray deviceName = env->NewByteArray(name_char_count);
  env->SetByteArrayRegion(deviceName, 0, name_char_count,
                          reinterpret_cast<jbyte*>(oob_data.device_name));

  oobDataLeBuilder =
      env->CallObjectMethod(oobDataLeBuilder, setNameMethod, deviceName);

  jmethodID buildMethod = env->GetMethodID(leBuilderClass, "build",
                                           "()Landroid/bluetooth/OobData;");

  return env->CallObjectMethod(oobDataLeBuilder, buildMethod);
}

static void generate_local_oob_data_callback(tBT_TRANSPORT transport,
                                             bt_oob_data_t oob_data) {
  ALOGV("%s", __func__);
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  if (transport == TRANSPORT_BREDR) {
    sCallbackEnv->CallVoidMethod(
        sJniCallbacksObj, method_oobDataReceivedCallback, (jint)transport,
        ((oob_data.is_valid)
             ? createClassicOobDataObject(sCallbackEnv.get(), oob_data)
             : nullptr));
  } else if (transport == TRANSPORT_LE) {
    sCallbackEnv->CallVoidMethod(
        sJniCallbacksObj, method_oobDataReceivedCallback, (jint)transport,
        ((oob_data.is_valid)
             ? createLeOobDataObject(sCallbackEnv.get(), oob_data)
             : nullptr));
  } else {
    // TRANSPORT_AUTO is a concept, however, the host stack doesn't fully
    // implement it So passing it from the java layer is currently useless until
    // the implementation and concept of TRANSPORT_AUTO is fleshed out.
    ALOGE("TRANSPORT: %d not implemented", transport);
    sCallbackEnv->CallVoidMethod(sJniCallbacksObj,
                                 method_oobDataReceivedCallback,
                                 (jint)transport, nullptr);
  }
}

static void callback_thread_event(bt_cb_thread_evt event) {
  if (event == ASSOCIATE_JVM) {
    JavaVMAttachArgs args;
    char name[] = "BT Service Callback Thread";
    args.version = JNI_VERSION_1_6;
    args.name = name;
    args.group = NULL;
    vm->AttachCurrentThread(&callbackEnv, &args);
    sHaveCallbackThread = true;
    sCallbackThread = pthread_self();
    ALOGV("Callback thread attached: %p", callbackEnv);
  } else if (event == DISASSOCIATE_JVM) {
    if (!isCallbackThread()) {
      ALOGE("Callback: '%s' is not called on the correct thread", __func__);
      return;
    }
    vm->DetachCurrentThread();
    sHaveCallbackThread = false;
  }
}

static void dut_mode_recv_callback(uint16_t opcode, uint8_t* buf, uint8_t len) {

}

static void le_test_mode_recv_callback(bt_status_t status,
                                       uint16_t packet_count) {
  ALOGV("%s: status:%d packet_count:%d ", __func__, status, packet_count);
}

static void energy_info_recv_callback(bt_activity_energy_info* p_energy_info,
                                      bt_uid_traffic_t* uid_data) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  jsize len = 0;
  for (bt_uid_traffic_t* data = uid_data; data->app_uid != -1; data++) {
    len++;
  }

  ScopedLocalRef<jobjectArray> array(
      sCallbackEnv.get(), sCallbackEnv->NewObjectArray(
                              len, android_bluetooth_UidTraffic.clazz, NULL));
  jsize i = 0;
  for (bt_uid_traffic_t* data = uid_data; data->app_uid != -1; data++) {
    ScopedLocalRef<jobject> uidObj(
        sCallbackEnv.get(),
        sCallbackEnv->NewObject(android_bluetooth_UidTraffic.clazz,
                                android_bluetooth_UidTraffic.constructor,
                                (jint)data->app_uid, (jlong)data->rx_bytes,
                                (jlong)data->tx_bytes));
    sCallbackEnv->SetObjectArrayElement(array.get(), i++, uidObj.get());
  }

  sCallbackEnv->CallVoidMethod(
      sJniAdapterServiceObj, method_energyInfo, p_energy_info->status,
      p_energy_info->ctrl_state, p_energy_info->tx_time, p_energy_info->rx_time,
      p_energy_info->idle_time, p_energy_info->energy_used, array.get());
}

static void link_quality_report_callback(
    uint64_t timestamp, int report_id, int rssi, int snr,
    int retransmission_count, int packets_not_receive_count,
    int negative_acknowledgement_count) {
}

static void switch_buffer_size_callback(bool is_low_latency_buffer_size) {
}

static void switch_codec_callback(bool is_low_latency_buffer_size) {
}

static bt_callbacks_t sBluetoothCallbacks = {sizeof(sBluetoothCallbacks),
                                             adapter_state_change_callback,
                                             adapter_properties_callback,
                                             remote_device_properties_callback,
                                             device_found_callback,
                                             discovery_state_changed_callback,
                                             pin_request_callback,
                                             ssp_request_callback,
                                             bond_state_changed_callback,
                                             NULL,
                                             NULL,
                                             acl_state_changed_callback,
                                             callback_thread_event,
                                             dut_mode_recv_callback,
                                             le_test_mode_recv_callback,
                                             energy_info_recv_callback,
                                             link_quality_report_callback,
                                             generate_local_oob_data_callback,
                                             switch_buffer_size_callback,
                                             switch_codec_callback};

// The callback to call when the wake alarm fires.
static alarm_cb sAlarmCallback;

// The data to pass to the wake alarm callback.
static void* sAlarmCallbackData;

class JNIThreadAttacher {
 public:
  JNIThreadAttacher(JavaVM* vm) : vm_(vm), env_(nullptr) {
    status_ = vm_->GetEnv((void**)&env_, JNI_VERSION_1_6);

    if (status_ != JNI_OK && status_ != JNI_EDETACHED) {
      ALOGE(
          "JNIThreadAttacher: unable to get environment for JNI CALL, "
          "status: %d",
          status_);
      env_ = nullptr;
      return;
    }

    if (status_ == JNI_EDETACHED) {
      char name[17] = {0};
      if (prctl(PR_GET_NAME, (unsigned long)name) != 0) {
        ALOGE(
            "JNIThreadAttacher: unable to grab previous thread name, error: %s",
            strerror(errno));
        env_ = nullptr;
        return;
      }

      JavaVMAttachArgs args = {
          .version = JNI_VERSION_1_6, .name = name, .group = nullptr};
      if (vm_->AttachCurrentThread(&env_, &args) != 0) {
        ALOGE("JNIThreadAttacher: unable to attach thread to VM");
        env_ = nullptr;
        return;
      }
    }
  }

  ~JNIThreadAttacher() {
    if (status_ == JNI_EDETACHED) vm_->DetachCurrentThread();
  }

  JNIEnv* getEnv() { return env_; }

 private:
  JavaVM* vm_;
  JNIEnv* env_;
  jint status_;
};

#ifdef ENABLE_JAVA_WAKE_LOCKS
static bool set_wake_alarm_callout(uint64_t delay_millis, bool should_wake,
                                   alarm_cb cb, void* data) {
  JNIThreadAttacher attacher(vm);
  JNIEnv* env = attacher.getEnv();
  jboolean ret = JNI_FALSE;

  if (env == nullptr) {
    ALOGE("%s: Unable to get JNI Env", __func__);
    return false;
  }

  sAlarmCallback = cb;
  sAlarmCallbackData = data;

  jboolean jshould_wake = should_wake ? JNI_TRUE : JNI_FALSE;
  if (sJniAdapterServiceObj) {
      ret = env->CallBooleanMethod(sJniAdapterServiceObj, method_setWakeAlarm,
                             (jlong)delay_millis, jshould_wake);
  } else {
       ALOGE("%s JNI ERROR : JNI reference already cleaned : set_wake_alarm_callout", __func__);
  }

  if (!ret) {
    ALOGE("%s setWakeAlarm failed:ret= %d ", __func__, ret);
    sAlarmCallback = NULL;
    sAlarmCallbackData = NULL;
  }

  return (ret == JNI_TRUE);
}

static int acquire_wake_lock_callout(const char* lock_name) {
  JNIThreadAttacher attacher(vm);
  JNIEnv* env = attacher.getEnv();

  if (env == nullptr) {
    ALOGE("%s: Unable to get JNI Env", __func__);
    return BT_STATUS_JNI_THREAD_ATTACH_ERROR;
  }

  jint ret = BT_STATUS_SUCCESS;
  {
    ScopedLocalRef<jstring> lock_name_jni(env, env->NewStringUTF(lock_name));
    if (lock_name_jni.get()) {
        if (sJniAdapterServiceObj) {
            bool acquired = env->CallBooleanMethod(
              sJniAdapterServiceObj, method_acquireWakeLock, lock_name_jni.get());
            if (!acquired) ret = BT_STATUS_WAKELOCK_ERROR;
        } else {
          ALOGE("%s JNI ERROR : JNI reference already cleaned : acquire_wake_lock_callout", __func__);
        }
    } else {
      ALOGE("%s unable to allocate string: %s", __func__, lock_name);
      ret = BT_STATUS_NOMEM;
    }
  }

  return ret;
}

static int release_wake_lock_callout(const char* lock_name) {
  JNIThreadAttacher attacher(vm);
  JNIEnv* env = attacher.getEnv();

  if (env == nullptr) {
    ALOGE("%s: Unable to get JNI Env", __func__);
    return BT_STATUS_JNI_THREAD_ATTACH_ERROR;
  }

  jint ret = BT_STATUS_SUCCESS;
  {
    ScopedLocalRef<jstring> lock_name_jni(env, env->NewStringUTF(lock_name));
    if (lock_name_jni.get()) {
        if (sJniAdapterServiceObj) {
            bool released = env->CallBooleanMethod(
               sJniAdapterServiceObj, method_releaseWakeLock, lock_name_jni.get());
            if (!released) ret = BT_STATUS_WAKELOCK_ERROR;
         } else {
          ALOGE("%s JNI ERROR : JNI reference already cleaned : release_wake_lock_callout", __func__);
        }
    } else {
      ALOGE("%s unable to allocate string: %s", __func__, lock_name);
      ret = BT_STATUS_NOMEM;
    }
  }

  return ret;
}
#endif
// Called by Java code when alarm is fired. A wake lock is held by the caller
// over the duration of this callback.
static void alarmFiredNative(JNIEnv* env, jobject obj) {
  if (sAlarmCallback) {
    sAlarmCallback(sAlarmCallbackData);
  } else {
    ALOGE("%s() - Alarm fired with callback not set!", __func__);
  }
}
#ifdef ENABLE_JAVA_WAKE_LOCKS
static bt_os_callouts_t sBluetoothOsCallouts = {
    sizeof(sBluetoothOsCallouts), set_wake_alarm_callout,
    acquire_wake_lock_callout, release_wake_lock_callout,
};
#endif

#define PROPERTY_BT_LIBRARY_NAME "ro.bluetooth.library_name"
#define DEFAULT_BT_LIBRARY_NAME "libbluetooth.so"

int hal_util_load_bt_library(const bt_interface_t** interface) {
  const char* sym = BLUETOOTH_INTERFACE_STRING;
  bt_interface_t* itf = nullptr;

  // The library name is not set by default, so the preset library name is used.
  char path[PROPERTY_VALUE_MAX] = "";
  property_get(PROPERTY_BT_LIBRARY_NAME, path, DEFAULT_BT_LIBRARY_NAME);
  void* handle = dlopen(path, RTLD_NOW);
  if (!handle) {
    const char* err_str = dlerror();
    ALOGE("%s: failed to load Bluetooth library, error=%s", __func__,
          err_str ? err_str : "error unknown");
    goto error;
  }

  // Get the address of the bt_interface_t.
  itf = (bt_interface_t*)dlsym(handle, sym);
  if (!itf) {
    ALOGE("%s: failed to load symbol from Bluetooth library %s", __func__, sym);
    goto error;
  }

  // Success.
  ALOGI("%s: loaded Bluetooth library successfully", __func__);
  *interface = itf;
  return 0;

error:
  *interface = NULL;
  if (handle) dlclose(handle);

  return -EINVAL;
}

static void classInitNative(JNIEnv* env, jclass clazz) {
  jclass jniUidTrafficClass = env->FindClass("android/bluetooth/UidTraffic");
  android_bluetooth_UidTraffic.constructor =
      env->GetMethodID(jniUidTrafficClass, "<init>", "(IJJ)V");

  jclass jniCallbackClass =
      env->FindClass("com/android/bluetooth/btservice/JniCallbacks");
  sJniCallbacksField = env->GetFieldID(
      clazz, "mJniCallbacks", "Lcom/android/bluetooth/btservice/JniCallbacks;");

  method_oobDataReceivedCallback =
      env->GetMethodID(jniCallbackClass, "oobDataReceivedCallback",
                       "(ILandroid/bluetooth/OobData;)V");

  method_stateChangeCallback =
      env->GetMethodID(jniCallbackClass, "stateChangeCallback", "(I)V");

  method_adapterPropertyChangedCallback = env->GetMethodID(
      jniCallbackClass, "adapterPropertyChangedCallback", "([I[[B)V");
  method_discoveryStateChangeCallback = env->GetMethodID(
      jniCallbackClass, "discoveryStateChangeCallback", "(I)V");

  method_devicePropertyChangedCallback = env->GetMethodID(
      jniCallbackClass, "devicePropertyChangedCallback", "([B[I[[B)V");
  method_deviceFoundCallback =
      env->GetMethodID(jniCallbackClass, "deviceFoundCallback", "([B)V");
  method_pinRequestCallback =
      env->GetMethodID(jniCallbackClass, "pinRequestCallback", "([B[BIZ)V");
  method_sspRequestCallback =
      env->GetMethodID(jniCallbackClass, "sspRequestCallback", "([B[BIII)V");

  method_bondStateChangeCallback =
      env->GetMethodID(jniCallbackClass, "bondStateChangeCallback", "(I[BII)V");

  method_aclStateChangeCallback =
      env->GetMethodID(jniCallbackClass, "aclStateChangeCallback", "(I[BIII)V");

  method_setWakeAlarm = env->GetMethodID(clazz, "setWakeAlarm", "(JZ)Z");
  method_acquireWakeLock =
      env->GetMethodID(clazz, "acquireWakeLock", "(Ljava/lang/String;)Z");
  method_releaseWakeLock =
      env->GetMethodID(clazz, "releaseWakeLock", "(Ljava/lang/String;)Z");
  method_energyInfo = env->GetMethodID(
      clazz, "energyInfoCallback", "(IIJJJJ[Landroid/bluetooth/UidTraffic;)V");

  if (env->GetJavaVM(&vm) != JNI_OK) {
    ALOGE("Could not get JavaVM");
  }

  if (hal_util_load_bt_library((bt_interface_t const**)&sBluetoothInterface)) {
    ALOGE("No Bluetooth Library found");
  }
}

static bool initNative(JNIEnv* env, jobject obj, jboolean isGuest,
                       jboolean isCommonCriteriaMode,
                       int configCompareResult,
                       jboolean isAtvDevice,
                       jobjectArray initFlags) {
  ALOGV("%s", __func__);

  android_bluetooth_UidTraffic.clazz =
      (jclass)env->NewGlobalRef(env->FindClass("android/bluetooth/UidTraffic"));

  sJniAdapterServiceObj = env->NewGlobalRef(obj);
  sJniCallbacksObj =
      env->NewGlobalRef(env->GetObjectField(obj, sJniCallbacksField));

  if (!sBluetoothInterface) {
    return JNI_FALSE;
  }

  int flagCount = env->GetArrayLength(initFlags);
  jstring* flagObjs = new jstring[flagCount];
  const char** flags = nullptr;
  if (flagCount > 0) {
    flags = new const char*[flagCount + 1];
    flags[flagCount] = nullptr;
  }

  for (int i = 0; i < flagCount; i++) {
    flagObjs[i] = (jstring)env->GetObjectArrayElement(initFlags, i);
    flags[i] = env->GetStringUTFChars(flagObjs[i], NULL);
  }

  const char* userDataDirectory = "";
  int ret = sBluetoothInterface->init(&sBluetoothCallbacks,
                                      isGuest == JNI_TRUE ? 1 : 0,
                                      isCommonCriteriaMode == JNI_TRUE ? 1 : 0,
                                      configCompareResult, flags,
                                      isAtvDevice == JNI_TRUE ? 1 : 0,
                                      userDataDirectory);

  for (int i = 0; i < flagCount; i++) {
    env->ReleaseStringUTFChars(flagObjs[i], flags[i]);
  }

  delete[] flags;
  delete[] flagObjs;

  if (ret != BT_STATUS_SUCCESS && ret != BT_STATUS_DONE) {
    ALOGE("Error while setting the callbacks: %d\n", ret);
    sBluetoothInterface = NULL;
    return JNI_FALSE;
  }

  /*disable these os_callout settings, so that native wake_lock will be enabled*/
#ifdef ENABLE_JAVA_WAKE_LOCKS
  ret = sBluetoothInterface->set_os_callouts(&sBluetoothOsCallouts);
  if (ret != BT_STATUS_SUCCESS) {
    ALOGE("Error while setting Bluetooth callouts: %d\n", ret);
    sBluetoothInterface->cleanup();
    sBluetoothInterface = NULL;
    return JNI_FALSE;
  }
#endif

  sBluetoothSocketInterface =
      (btsock_interface_t*)sBluetoothInterface->get_profile_interface(
          BT_PROFILE_SOCKETS_ID);
  if (sBluetoothSocketInterface == NULL) {
    ALOGE("Error getting socket interface");
  }

  return JNI_TRUE;
}

static bool cleanupNative(JNIEnv* env, jobject obj) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  sBluetoothInterface->cleanup();
  ALOGI("%s: return from cleanup", __func__);

  if (sJniCallbacksObj) {
    env->DeleteGlobalRef(sJniCallbacksObj);
    sJniCallbacksObj = NULL;
  }

  if (sJniAdapterServiceObj) {
    env->DeleteGlobalRef(sJniAdapterServiceObj);
    sJniAdapterServiceObj = NULL;
  }

  if (android_bluetooth_UidTraffic.clazz) {
    env->DeleteGlobalRef(android_bluetooth_UidTraffic.clazz);
    android_bluetooth_UidTraffic.clazz = NULL;
  }
  return JNI_TRUE;
}

static jboolean enableNative(JNIEnv* env, jobject obj) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;
  int ret = sBluetoothInterface->enable();
  return (ret == BT_STATUS_SUCCESS || ret == BT_STATUS_DONE) ? JNI_TRUE
                                                             : JNI_FALSE;
}

static jboolean disableNative(JNIEnv* env, jobject obj) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  int ret = sBluetoothInterface->disable();
  /* Retrun JNI_FALSE only when BTIF explicitly reports
     BT_STATUS_FAIL. It is fine for the BT_STATUS_NOT_READY
     case which indicates that stack had not been enabled.
  */
  return (ret == BT_STATUS_FAIL) ? JNI_FALSE : JNI_TRUE;
}

static jboolean startDiscoveryNative(JNIEnv* env, jobject obj) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  int ret = sBluetoothInterface->start_discovery();
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean cancelDiscoveryNative(JNIEnv* env, jobject obj) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  int ret = sBluetoothInterface->cancel_discovery();
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean createBondNative(JNIEnv* env, jobject obj, jbyteArray address,
                                 jint transport) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  jbyte* addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  int ret = sBluetoothInterface->create_bond((RawAddress*)addr, transport);
  env->ReleaseByteArrayElements(address, addr, 0);
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jbyteArray callByteArrayGetter(JNIEnv* env, jobject object,
                                      const char* className,
                                      const char* methodName) {
  jclass myClass = env->FindClass(className);
  jmethodID myMethod = env->GetMethodID(myClass, methodName, "()[B");
  return (jbyteArray)env->CallObjectMethod(object, myMethod);
}

static jint callIntGetter(JNIEnv* env, jobject object, const char* className,
                          const char* methodName) {
  jclass myClass = env->FindClass(className);
  jmethodID myMethod = env->GetMethodID(myClass, methodName, "()I");
  return env->CallIntMethod(object, myMethod);
}

static jboolean set_data(JNIEnv* env, bt_oob_data_t& oob_data, jobject oobData,
                         jint transport) {
  // Need both arguments to be non NULL
  if (oobData == NULL) {
    ALOGE("%s: oobData is null! Nothing to do.", __func__);
    return JNI_FALSE;
  }

  memset(&oob_data, 0, sizeof(oob_data));

  jbyteArray address = callByteArrayGetter(
      env, oobData, "android/bluetooth/OobData", "getDeviceAddressWithType");

  // Check the data
  int len = env->GetArrayLength(address);
  if (len != OOB_ADDRESS_SIZE) {
    ALOGE("%s: addressBytes must be 7 bytes in length (address plus type) 6+1!",
          __func__);
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  // Convert the address from byte[]
  jbyte* addressBytes = env->GetByteArrayElements(address, NULL);
  if (addressBytes == NULL) {
    ALOGE("%s: addressBytes cannot be null!", __func__);
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }
  memcpy(oob_data.address, addressBytes, len);

  // Get the device name byte[] java object
  jbyteArray deviceName = callByteArrayGetter(
      env, oobData, "android/bluetooth/OobData", "getDeviceName");

  // Optional
  // Convert it to a jbyte* and copy it to the struct
  jbyte* deviceNameBytes = NULL;
  if (deviceName != NULL) {
    deviceNameBytes = env->GetByteArrayElements(deviceName, NULL);
    int len = env->GetArrayLength(deviceName);
    if (len > OOB_NAME_MAX_SIZE) {
      ALOGI(
          "%s: wrong length of deviceName, should be empty or less than or "
          "equal to %d bytes.",
          __func__, OOB_NAME_MAX_SIZE);
      jniThrowIOException(env, EINVAL);
      env->ReleaseByteArrayElements(deviceName, deviceNameBytes, 0);
      return JNI_FALSE;
    }
    memcpy(oob_data.device_name, deviceNameBytes, len);
    env->ReleaseByteArrayElements(deviceName, deviceNameBytes, 0);
  }
  // Used by both classic and LE
  jbyteArray confirmation = callByteArrayGetter(
      env, oobData, "android/bluetooth/OobData", "getConfirmationHash");
  if (confirmation == NULL) {
    ALOGE("%s: confirmation cannot be null!", __func__);
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  // Confirmation is mandatory
  jbyte* confirmationBytes = NULL;
  confirmationBytes = env->GetByteArrayElements(confirmation, NULL);
  len = env->GetArrayLength(confirmation);
  if (confirmationBytes == NULL || len != OOB_C_SIZE) {
    ALOGI(
        "%s: wrong length of Confirmation, should be empty or %d "
        "bytes.",
        __func__, OOB_C_SIZE);
    jniThrowIOException(env, EINVAL);
    env->ReleaseByteArrayElements(confirmation, confirmationBytes, 0);
    return JNI_FALSE;
  }
  memcpy(oob_data.c, confirmationBytes, len);
  env->ReleaseByteArrayElements(confirmation, confirmationBytes, 0);

  // Random is supposedly optional according to the specification
  jbyteArray randomizer = callByteArrayGetter(
      env, oobData, "android/bluetooth/OobData", "getRandomizerHash");
  jbyte* randomizerBytes = NULL;
  if (randomizer != NULL) {
    randomizerBytes = env->GetByteArrayElements(randomizer, NULL);
    int len = env->GetArrayLength(randomizer);
    if (randomizerBytes == NULL || len != OOB_R_SIZE) {
      ALOGI("%s: wrong length of Random, should be empty or %d bytes.",
            __func__, OOB_R_SIZE);
      jniThrowIOException(env, EINVAL);
      env->ReleaseByteArrayElements(randomizer, randomizerBytes, 0);
      return JNI_FALSE;
    }
    memcpy(oob_data.r, randomizerBytes, len);
    env->ReleaseByteArrayElements(randomizer, randomizerBytes, 0);
  }

  // Transport specific data fetching/setting
  if (transport == TRANSPORT_BREDR) {
    // Classic
    // Not optional
    jbyteArray oobDataLength = callByteArrayGetter(
        env, oobData, "android/bluetooth/OobData", "getClassicLength");
    jbyte* oobDataLengthBytes = NULL;
    if (oobDataLength == NULL ||
        env->GetArrayLength(oobDataLength) != OOB_DATA_LEN_SIZE) {
      ALOGI("%s: wrong length of oobDataLength, should be empty or %d bytes.",
            __func__, OOB_DATA_LEN_SIZE);
      jniThrowIOException(env, EINVAL);
      env->ReleaseByteArrayElements(oobDataLength, oobDataLengthBytes, 0);
      return JNI_FALSE;
    }

    oobDataLengthBytes = env->GetByteArrayElements(oobDataLength, NULL);
    memcpy(oob_data.oob_data_length, oobDataLengthBytes, len);
    env->ReleaseByteArrayElements(oobDataLength, oobDataLengthBytes, 0);

    // Optional
    jbyteArray classOfDevice = callByteArrayGetter(
        env, oobData, "android/bluetooth/OobData", "getClassOfDevice");
    jbyte* classOfDeviceBytes = NULL;
    if (classOfDevice != NULL) {
      classOfDeviceBytes = env->GetByteArrayElements(classOfDevice, NULL);
      int len = env->GetArrayLength(classOfDevice);
      if (len != OOB_COD_SIZE) {
        ALOGI("%s: wrong length of classOfDevice, should be empty or %d bytes.",
              __func__, OOB_COD_SIZE);
        jniThrowIOException(env, EINVAL);
        env->ReleaseByteArrayElements(classOfDevice, classOfDeviceBytes, 0);
        return JNI_FALSE;
      }
      memcpy(oob_data.class_of_device, classOfDeviceBytes, len);
      env->ReleaseByteArrayElements(classOfDevice, classOfDeviceBytes, 0);
    }
  } else if (transport == TRANSPORT_LE) {
    // LE
    jbyteArray temporaryKey = callByteArrayGetter(
        env, oobData, "android/bluetooth/OobData", "getLeTemporaryKey");
    jbyte* temporaryKeyBytes = NULL;
    if (temporaryKey != NULL) {
      temporaryKeyBytes = env->GetByteArrayElements(temporaryKey, NULL);
      int len = env->GetArrayLength(temporaryKey);
      if (len != OOB_TK_SIZE) {
        ALOGI("%s: wrong length of temporaryKey, should be empty or %d bytes.",
              __func__, OOB_TK_SIZE);
        jniThrowIOException(env, EINVAL);
        env->ReleaseByteArrayElements(temporaryKey, temporaryKeyBytes, 0);
        return JNI_FALSE;
      }
      memcpy(oob_data.sm_tk, temporaryKeyBytes, len);
      env->ReleaseByteArrayElements(temporaryKey, temporaryKeyBytes, 0);
    }

    jbyteArray leAppearance = callByteArrayGetter(
        env, oobData, "android/bluetooth/OobData", "getLeAppearance");
    jbyte* leAppearanceBytes = NULL;
    if (leAppearance != NULL) {
      leAppearanceBytes = env->GetByteArrayElements(leAppearance, NULL);
      int len = env->GetArrayLength(leAppearance);
      if (len != OOB_LE_APPEARANCE_SIZE) {
        ALOGI("%s: wrong length of leAppearance, should be empty or %d bytes.",
              __func__, OOB_LE_APPEARANCE_SIZE);
        jniThrowIOException(env, EINVAL);
        env->ReleaseByteArrayElements(leAppearance, leAppearanceBytes, 0);
        return JNI_FALSE;
      }
      memcpy(oob_data.le_appearance, leAppearanceBytes, len);
      env->ReleaseByteArrayElements(leAppearance, leAppearanceBytes, 0);
    }

    jint leRole = callIntGetter(env, oobData, "android/bluetooth/OobData",
                                "getLeDeviceRole");
    oob_data.le_device_role = leRole;

    jint leFlag =
        callIntGetter(env, oobData, "android/bluetooth/OobData", "getLeFlags");
    oob_data.le_flags = leFlag;
  }
  return JNI_TRUE;
}

static void generateLocalOobDataNative(JNIEnv* env, jobject obj,
                                       jint transport) {
  // No BT interface? Can't do anything.
  if (!sBluetoothInterface) return;

  if (sBluetoothInterface->generate_local_oob_data(transport) !=
      BT_STATUS_SUCCESS) {
    ALOGE("%s: Call to generate_local_oob_data failed!", __func__);
    bt_oob_data_t oob_data;
    oob_data.is_valid = false;
    generate_local_oob_data_callback(transport, oob_data);
  }
}

static jboolean createBondOutOfBandNative(JNIEnv* env, jobject obj,
                                          jbyteArray address, jint transport,
                                          jobject p192Data, jobject p256Data) {
  // No BT interface? Can't do anything.
  if (!sBluetoothInterface) return JNI_FALSE;

  // No data? Can't do anything
  if (p192Data == NULL && p256Data == NULL) {
    ALOGE("%s: All OOB Data are null! Nothing to do.", __func__);
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  // This address is already reversed which is why its being passed...
  // In the future we want to remove this and just reverse the address
  // for the oobdata in the host stack.
  if (address == NULL) {
    ALOGE("%s: Address cannot be null! Nothing to do.", __func__);
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  // Check the data
  int len = env->GetArrayLength(address);
  if (len != 6) {
    ALOGE("%s: addressBytes must be 6 bytes in length (address plus type) 6+1!",
          __func__);
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  jbyte* addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  // Convert P192 data from Java POJO to C Struct
  bt_oob_data_t p192_data;
  if (p192Data != NULL) {
    if (set_data(env, p192_data, p192Data, transport) == JNI_FALSE) {
      jniThrowIOException(env, EINVAL);
      return JNI_FALSE;
    }
  }

  // Convert P256 data from Java POJO to C Struct
  bt_oob_data_t p256_data;
  if (p256Data != NULL) {
    if (set_data(env, p256_data, p256Data, transport) == JNI_FALSE) {
      jniThrowIOException(env, EINVAL);
      return JNI_FALSE;
    }
  }

  return ((sBluetoothInterface->create_bond_out_of_band(
              (RawAddress*)addr, transport, &p192_data, &p256_data)) ==
          BT_STATUS_SUCCESS)
             ? JNI_TRUE
             : JNI_FALSE;
}

static jboolean removeBondNative(JNIEnv* env, jobject obj, jbyteArray address) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  jbyte* addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  int ret = sBluetoothInterface->remove_bond((RawAddress*)addr);
  env->ReleaseByteArrayElements(address, addr, 0);

  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean cancelBondNative(JNIEnv* env, jobject obj, jbyteArray address) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  jbyte* addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  int ret = sBluetoothInterface->cancel_bond((RawAddress*)addr);
  env->ReleaseByteArrayElements(address, addr, 0);
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static int getConnectionStateNative(JNIEnv* env, jobject obj,
                                    jbyteArray address) {
  ALOGV("%s", __func__);
  if (!sBluetoothInterface) return JNI_FALSE;

  jbyte* addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  int ret = sBluetoothInterface->get_connection_state((RawAddress*)addr);
  env->ReleaseByteArrayElements(address, addr, 0);

  return ret;
}

static jboolean pinReplyNative(JNIEnv* env, jobject obj, jbyteArray address,
                               jboolean accept, jint len, jbyteArray pinArray) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  jbyte* addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  jbyte* pinPtr = NULL;
  if (accept) {
    pinPtr = env->GetByteArrayElements(pinArray, NULL);
    if (pinPtr == NULL) {
      jniThrowIOException(env, EINVAL);
      env->ReleaseByteArrayElements(address, addr, 0);
      return JNI_FALSE;
    }
  }

  int ret = sBluetoothInterface->pin_reply((RawAddress*)addr, accept, len,
                                           (bt_pin_code_t*)pinPtr);
  env->ReleaseByteArrayElements(address, addr, 0);
  env->ReleaseByteArrayElements(pinArray, pinPtr, 0);

  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean sspReplyNative(JNIEnv* env, jobject obj, jbyteArray address,
                               jint type, jboolean accept, jint passkey) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  jbyte* addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  int ret = sBluetoothInterface->ssp_reply(
      (RawAddress*)addr, (bt_ssp_variant_t)type, accept, passkey);
  env->ReleaseByteArrayElements(address, addr, 0);

  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean setAdapterPropertyNative(JNIEnv* env, jobject obj, jint type,
                                         jbyteArray value) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  jbyte* val = env->GetByteArrayElements(value, NULL);
  bt_property_t prop;
  prop.type = (bt_property_type_t)type;
  prop.len = env->GetArrayLength(value);
  prop.val = val;

  int ret = sBluetoothInterface->set_adapter_property(&prop);
  env->ReleaseByteArrayElements(value, val, 0);

  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean getAdapterPropertiesNative(JNIEnv* env, jobject obj) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  int ret = sBluetoothInterface->get_adapter_properties();
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean getAdapterPropertyNative(JNIEnv* env, jobject obj, jint type) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  int ret = sBluetoothInterface->get_adapter_property((bt_property_type_t)type);
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean getDevicePropertyNative(JNIEnv* env, jobject obj,
                                        jbyteArray address, jint type) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  jbyte* addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  int ret = sBluetoothInterface->get_remote_device_property(
      (RawAddress*)addr, (bt_property_type_t)type);
  env->ReleaseByteArrayElements(address, addr, 0);
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean setDevicePropertyNative(JNIEnv* env, jobject obj,
                                        jbyteArray address, jint type,
                                        jbyteArray value) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  jbyte* val = env->GetByteArrayElements(value, NULL);
  if (val == NULL) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  jbyte* addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    env->ReleaseByteArrayElements(value, val, 0);
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  bt_property_t prop;
  prop.type = (bt_property_type_t)type;
  prop.len = env->GetArrayLength(value);
  prop.val = val;

  int ret =
      sBluetoothInterface->set_remote_device_property((RawAddress*)addr, &prop);
  env->ReleaseByteArrayElements(value, val, 0);
  env->ReleaseByteArrayElements(address, addr, 0);

  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean getRemoteServicesNative(JNIEnv* env, jobject obj,
                                        jbyteArray address, jint transport) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  jbyte* addr = addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  int ret = 
      sBluetoothInterface->get_remote_services((RawAddress*)addr, transport);
  env->ReleaseByteArrayElements(address, addr, 0);
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static int readEnergyInfo() {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;
  int ret = sBluetoothInterface->read_energy_info();
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static void dumpNative(JNIEnv* env, jobject obj, jobject fdObj,
                       jobjectArray argArray) {
  ALOGV("%s", __func__);
  if (!sBluetoothInterface) return;

  int fd = jniGetFDFromFileDescriptor(env, fdObj);
  if (fd < 0) return;

  int numArgs = env->GetArrayLength(argArray);

  jstring* argObjs = new jstring[numArgs];
  const char** args = nullptr;
  if (numArgs > 0) args = new const char*[numArgs];

  if (!args || !argObjs) {
    ALOGE("%s: not have enough memeory", __func__);
    return;
  }

  for (int i = 0; i < numArgs; i++) {
    argObjs[i] = (jstring)env->GetObjectArrayElement(argArray, i);
    args[i] = env->GetStringUTFChars(argObjs[i], NULL);
  }

  sBluetoothInterface->dump(fd, args);

  for (int i = 0; i < numArgs; i++) {
    env->ReleaseStringUTFChars(argObjs[i], args[i]);
  }

  delete[] args;
  delete[] argObjs;
}

static jbyteArray dumpMetricsNative(JNIEnv* env, jobject obj) {
  ALOGI("%s", __func__);
  if (!sBluetoothInterface) return env->NewByteArray(0);

  std::string output;
  sBluetoothInterface->dumpMetrics(&output);
  jsize output_size = output.size() * sizeof(char);
  jbyteArray output_bytes = env->NewByteArray(output_size);
  env->SetByteArrayRegion(output_bytes, 0, output_size,
                          (const jbyte*)output.data());
  return output_bytes;
}

static jboolean factoryResetNative(JNIEnv* env, jobject obj) {
  ALOGV("%s", __func__);
  if (!sBluetoothInterface) return JNI_FALSE;
  int ret = sBluetoothInterface->config_clear();
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static void interopDatabaseClearNative(JNIEnv* env, jobject obj) {
  ALOGV("%s", __func__);
  if (!sBluetoothInterface) return;
  sBluetoothInterface->interop_database_clear();
}

static void interopDatabaseAddNative(JNIEnv* env, jobject obj, int feature,
                                     jbyteArray address, int length) {
  ALOGV("%s", __func__);
  if (!sBluetoothInterface) return;

  jbyte* addr = env->GetByteArrayElements(address, NULL);
  if (addr == NULL) {
    jniThrowIOException(env, EINVAL);
    return;
  }

  sBluetoothInterface->interop_database_add(feature, (RawAddress*)addr, length);
  env->ReleaseByteArrayElements(address, addr, 0);
}

static jbyteArray obfuscateAddressNative(JNIEnv* env, jobject obj,
                                         jbyteArray address) {
  ALOGV("%s", __func__);
  if (!sBluetoothInterface) return env->NewByteArray(0);
  jbyte* addr = env->GetByteArrayElements(address, nullptr);
  if (addr == nullptr) {
    jniThrowIOException(env, EINVAL);
    return env->NewByteArray(0);
  }
  RawAddress addr_obj = {};
  addr_obj.FromOctets((uint8_t*)addr);
  std::string output = sBluetoothInterface->obfuscate_address(addr_obj);
  jsize output_size = output.size() * sizeof(char);
  jbyteArray output_bytes = env->NewByteArray(output_size);
  env->SetByteArrayRegion(output_bytes, 0, output_size,
                          (const jbyte*)output.data());
  return output_bytes;
}

static jboolean setBufferLengthMillisNative(JNIEnv* env, jobject obj,
                                            jint codec, jint size) {
  ALOGV("%s", __func__);

  if (!sBluetoothInterface) return JNI_FALSE;

  int ret = sBluetoothInterface->set_dynamic_audio_buffer_size(codec, size);
  return (ret == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jint connectSocketNative(JNIEnv* env, jobject obj, jbyteArray address,
                                jint type, jbyteArray uuid, jint port,
                                jint flag, jint callingUid) {
  int socket_fd = INVALID_FD;
  jbyte* addr = nullptr;
  jbyte* uuidBytes = nullptr;
  Uuid btUuid;

  if (!sBluetoothSocketInterface) {
    goto done;
  }
  addr = env->GetByteArrayElements(address, nullptr);
  uuidBytes = env->GetByteArrayElements(uuid, nullptr);
  if (addr == nullptr || uuidBytes == nullptr) {
    jniThrowIOException(env, EINVAL);
    goto done;
  }

  btUuid = Uuid::From128BitBE((uint8_t*)uuidBytes);
  if (sBluetoothSocketInterface->connect((RawAddress*)addr, (btsock_type_t)type,
                                         &btUuid, port, &socket_fd, flag,
                                         callingUid) != BT_STATUS_SUCCESS) {
    socket_fd = INVALID_FD;
  }

done:
  if (addr) env->ReleaseByteArrayElements(address, addr, 0);
  if (uuidBytes) env->ReleaseByteArrayElements(uuid, uuidBytes, 0);
  return socket_fd;
}

static jint createSocketChannelNative(JNIEnv* env, jobject obj, jint type,
                                      jstring serviceName, jbyteArray uuid,
                                      jint port, jint flag, jint callingUid) {
  int socket_fd = INVALID_FD;
  jbyte* uuidBytes = nullptr;
  Uuid btUuid;
  const char* nativeServiceName = nullptr;

  if (!sBluetoothSocketInterface) {
    goto done;
  }
  uuidBytes = env->GetByteArrayElements(uuid, nullptr);
  if (serviceName != nullptr) {
    nativeServiceName = env->GetStringUTFChars(serviceName, nullptr);
  }
  if (uuidBytes == nullptr) {
    jniThrowIOException(env, EINVAL);
    goto done;
  }
  btUuid = Uuid::From128BitBE((uint8_t*)uuidBytes);

  if (sBluetoothSocketInterface->listen((btsock_type_t)type, nativeServiceName,
                                        &btUuid, port, &socket_fd, flag,
                                        callingUid) != BT_STATUS_SUCCESS) {
    socket_fd = INVALID_FD;
  }

done:
  if (uuidBytes) env->ReleaseByteArrayElements(uuid, uuidBytes, 0);
  if (nativeServiceName)
    env->ReleaseStringUTFChars(serviceName, nativeServiceName);
  return socket_fd;
}

static void requestMaximumTxDataLengthNative(JNIEnv* env, jobject obj,
                                             jbyteArray address) {
  if (!sBluetoothSocketInterface) {
    return;
  }
  jbyte* addr = env->GetByteArrayElements(address, nullptr);
  if (addr == nullptr) {
    jniThrowIOException(env, EINVAL);
    return;
  }

  RawAddress addressVar = *(RawAddress*)addr;
  sBluetoothSocketInterface->request_max_tx_data_length(addressVar);
  env->ReleaseByteArrayElements(address, addr, 1);
}

static int getMetricIdNative(JNIEnv* env, jobject obj, jbyteArray address) {
  ALOGV("%s", __func__);
  if (!sBluetoothInterface) return 0;  // 0 is invalid id
  jbyte* addr = env->GetByteArrayElements(address, nullptr);
  if (addr == nullptr) {
    jniThrowIOException(env, EINVAL);
    return 0;
  }
  RawAddress addr_obj = {};
  addr_obj.FromOctets((uint8_t*)addr);
  return sBluetoothInterface->get_metric_id(addr_obj);
}

static JNINativeMethod sMethods[] = {
    /* name, signature, funcPtr */
    {"classInitNative", "()V", (void*)classInitNative},
    {"initNative", "(ZZIZ[Ljava/lang/String;)Z", (void*)initNative},
    {"cleanupNative", "()V", (void*)cleanupNative},
    {"enableNative", "()Z", (void*)enableNative},
    {"disableNative", "()Z", (void*)disableNative},
    {"setAdapterPropertyNative", "(I[B)Z", (void*)setAdapterPropertyNative},
    {"getAdapterPropertiesNative", "()Z", (void*)getAdapterPropertiesNative},
    {"getAdapterPropertyNative", "(I)Z", (void*)getAdapterPropertyNative},
    {"getDevicePropertyNative", "([BI)Z", (void*)getDevicePropertyNative},
    {"setDevicePropertyNative", "([BI[B)Z", (void*)setDevicePropertyNative},
    {"startDiscoveryNative", "()Z", (void*)startDiscoveryNative},
    {"cancelDiscoveryNative", "()Z", (void*)cancelDiscoveryNative},
    {"createBondNative", "([BI)Z", (void*)createBondNative},
    {"createBondOutOfBandNative",
     "([BILandroid/bluetooth/OobData;Landroid/bluetooth/OobData;)Z",
     (void*)createBondOutOfBandNative},
    {"removeBondNative", "([B)Z", (void*)removeBondNative},
    {"cancelBondNative", "([B)Z", (void*)cancelBondNative},
    {"generateLocalOobDataNative", "(I)V", (void*)generateLocalOobDataNative},
    {"getConnectionStateNative", "([B)I", (void*)getConnectionStateNative},
    {"pinReplyNative", "([BZI[B)Z", (void*)pinReplyNative},
    {"sspReplyNative", "([BIZI)Z", (void*)sspReplyNative},
    {"getRemoteServicesNative", "([B)Z", (void*)getRemoteServicesNative},
    {"alarmFiredNative", "()V", (void*)alarmFiredNative},
    {"readEnergyInfo", "()I", (void*)readEnergyInfo},
    {"dumpNative", "(Ljava/io/FileDescriptor;[Ljava/lang/String;)V",
     (void*)dumpNative},
    {"dumpMetricsNative", "()[B", (void*)dumpMetricsNative},
    {"factoryResetNative", "()Z", (void*)factoryResetNative},
    {"interopDatabaseClearNative", "()V", (void*)interopDatabaseClearNative},
    {"interopDatabaseAddNative", "(I[BI)V", (void*)interopDatabaseAddNative},
    {"obfuscateAddressNative", "([B)[B", (void*)obfuscateAddressNative},
    {"setBufferLengthMillisNative", "(II)Z", (void*)setBufferLengthMillisNative},
	{"getMetricIdNative", "([B)I", (void*)getMetricIdNative},
    {"connectSocketNative", "([BI[BIII)I", (void*)connectSocketNative},
    {"createSocketChannelNative", "(ILjava/lang/String;[BIII)I",
     (void*)createSocketChannelNative},
    {"requestMaximumTxDataLengthNative", "([B)V",
     (void*)requestMaximumTxDataLengthNative}};

int register_com_android_bluetooth_btservice_AdapterService(JNIEnv* env) {
  return jniRegisterNativeMethods(
      env, "com/android/bluetooth/btservice/AdapterService", sMethods,
      NELEM(sMethods));
}

} /* namespace android */

/*
 * JNI Initialization
 */
jint JNI_OnLoad(JavaVM *jvm, void *reserved) {
    JNIEnv *e;
    int status;

    ALOGV("Bluetooth Adapter Service : loading JNI\n");

    // Check JNI version
    if (jvm->GetEnv((void **)&e, JNI_VERSION_1_6)) {
        ALOGE("JNI version mismatch error");
        return JNI_ERR;
    }

  status = android::register_com_android_bluetooth_btservice_AdapterService(e);
  if (status < 0) {
    ALOGE("jni adapter service registration failure, status: %d", status);
    return JNI_ERR;
  }

  status =
      android::register_com_android_bluetooth_btservice_BluetoothKeystore(e);
  if (status < 0) {
    ALOGE("jni BluetoothKeyStore registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_hfp(e);
  if (status < 0) {
    ALOGE("jni hfp registration failure, status: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_hfp_vendorhfservice(e);
  if (status < 0) {
    ALOGE("jni vendor hfp service registration failure, status: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_hfpclient(e);
  if (status < 0) {
    ALOGE("jni hfp client registration failure, status: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_a2dp(e);
  if (status < 0) {
    ALOGE("jni a2dp source registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_ba(e);
  if (status < 0) {
      ALOGE("jni BA Transmitter registration failure: %d", status);
      return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_a2dp_sink(e);
  if (status < 0) {
    ALOGE("jni a2dp sink registration failure: %d", status);
    return JNI_ERR;
  }


  status = android::register_com_android_bluetooth_avrcp_target(e);
  if (status < 0) {
    ALOGE("jni new avrcp target registration failure: %d", status);
  }

  status = android::register_com_android_bluetooth_avrcp_controller(e);
  if (status < 0) {
    ALOGE("jni avrcp controller registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_hid_host(e);
  if (status < 0) {
    ALOGE("jni hid registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_hid_device(e);
  if (status < 0) {
    ALOGE("jni hidd registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_pan(e);
  if (status < 0) {
    ALOGE("jni pan registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_gatt(e);
  if (status < 0) {
    ALOGE("jni gatt registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_sdp(e);
  if (status < 0) {
    ALOGE("jni sdp registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_btservice_vendor(e);
  if (status < 0) {
    ALOGE("jni vendor registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_btservice_vendor_socket(e);
  if (status < 0) {
    ALOGE("jni vendor socket registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_hearing_aid(e);
  if (status < 0) {
    ALOGE("jni hearing aid registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_avrcp_ext(e);
  if (status < 0) {
    ALOGE("jni avrcp_ext registration failure: %d", status);
    return JNI_ERR;
  }


#ifdef ADV_AUDIO_FEATURE

  status = android::register_com_android_bluetooth_csip_client(e);
  if (status < 0) {
    ALOGE("jni csip registration failure: %d", status);
    return JNI_ERR;
  }

  status = android::register_com_android_bluetooth_csip_setcoordinator(e);
  if (status < 0) {
    ALOGE("jni csip set coordinator registration failure: %d", status);
    return JNI_ERR;
  }


  status = android::register_com_android_bluetooth_adv_audio_profiles(e);
  if (status < 0) {
    ALOGE("jni advance audio profile registration failure: %d", status);
    return JNI_ERR;
  }
#endif
  return JNI_VERSION_1_6;
}
