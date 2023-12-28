/******************************************************************************
 *  Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *      * Neither the name of The Linux Foundation nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 *  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#define LOG_TAG "CsipSetCoordinatorJni"

#define LOG_NDEBUG 0

#include "com_android_bluetooth.h"
#include "hardware/bt_csip.h"
#include "utils/Log.h"

#include <shared_mutex>
#include <string.h>

using bluetooth::Uuid;

#define UUID_PARAMS(uuid) uuid_lsb(uuid), uuid_msb(uuid)

static uint64_t uuid_lsb(const Uuid& uuid) {
  uint64_t lsb = 0;

  auto uu = uuid.To128BitBE();
  for (int i = 8; i <= 15; i++) {
    lsb <<= 8;
    lsb |= uu[i];
  }

  return lsb;
}

static uint64_t uuid_msb(const Uuid& uuid) {
  uint64_t msb = 0;

  auto uu = uuid.To128BitBE();
  for (int i = 0; i <= 7; i++) {
    msb <<= 8;
    msb |= uu[i];
  }

  return msb;
}

static Uuid from_java_uuid(jlong uuid_msb, jlong uuid_lsb) {
  std::array<uint8_t, Uuid::kNumBytes128> uu{};
  for (int i = 0; i < 8; i++) {
    uu[7 - i] = (uuid_msb >> (8 * i)) & 0xFF;
    uu[15 - i] = (uuid_lsb >> (8 * i)) & 0xFF;
  }
  return Uuid::From128BitBE(uu);
}

static RawAddress str2addr(JNIEnv* env, jstring address) {
  RawAddress bd_addr;
  const char* c_address = env->GetStringUTFChars(address, NULL);
  if (!c_address) return bd_addr;

  RawAddress::FromString(std::string(c_address), bd_addr);
  env->ReleaseStringUTFChars(address, c_address);

  return bd_addr;
}

namespace android {
static jmethodID method_onCsipAppRegistered;
static jmethodID method_onConnectionStateChanged;
static jmethodID method_onNewSetFound;
static jmethodID method_onNewSetMemberFound;
static jmethodID method_onLockStatusChanged;
static jmethodID method_onLockAvailable;
static jmethodID method_onSetSirkChanged;
static jmethodID method_onSetSizeChanged;
static jmethodID method_onRsiDataFound;

static const btcsip_interface_t* sBluetoothCsipInterface = NULL;
static jobject mCallbacksObj = NULL;
static std::shared_timed_mutex mCallbacks_mutex;

static jstring bdaddr2newjstr(JNIEnv* env, const RawAddress* bda) {
  char c_address[32];
  snprintf(c_address, sizeof(c_address), "%02X:%02X:%02X:%02X:%02X:%02X",
           bda->address[0], bda->address[1], bda->address[2], bda->address[3],
           bda->address[4], bda->address[5]);

  return env->NewStringUTF(c_address);
}

static void classInitNative(JNIEnv* env, jclass clazz) {
  method_onCsipAppRegistered =
      env->GetMethodID(clazz, "onCsipAppRegistered", "(IIJJ)V");

  method_onConnectionStateChanged =
      env->GetMethodID(clazz, "onConnectionStateChanged", "(ILjava/lang/String;II)V");

  method_onNewSetFound =
      env->GetMethodID(clazz, "onNewSetFound", "(ILjava/lang/String;I[BJJZ)V");

  method_onNewSetMemberFound =
      env->GetMethodID(clazz, "onNewSetMemberFound", "(ILjava/lang/String;)V");

  method_onLockStatusChanged =
      env->GetMethodID(clazz, "onLockStatusChanged", "(IIII[Ljava/lang/String;)V");

  method_onLockAvailable =
      env->GetMethodID(clazz, "onLockAvailable", "(IILjava/lang/String;)V");

  method_onSetSirkChanged =
      env->GetMethodID(clazz, "onSetSirkChanged", "(I[BLjava/lang/String;)V");

  method_onSetSizeChanged =
      env->GetMethodID(clazz, "onSetSizeChanged", "(IILjava/lang/String;)V");

  method_onRsiDataFound =
      env->GetMethodID(clazz, "onRsiDataFound", "([BLjava/lang/String;)V");

  ALOGI("%s: succeeds", __func__);
}

static void csip_app_registered_callback(uint8_t status, uint8_t app_id,
                                         const bluetooth::Uuid& uuid){
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onCsipAppRegistered,
                               status, app_id, UUID_PARAMS(uuid));
}

static void connection_state_changed_callback(uint8_t app_id, RawAddress& addr,
                                              uint8_t state, uint8_t status){
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  // address
  ScopedLocalRef<jstring> address(sCallbackEnv.get(),
                                  bdaddr2newjstr(sCallbackEnv.get(), &addr));

  sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onConnectionStateChanged,
                               app_id, address.get(), state, status);
}

static void new_set_found_callback(uint8_t set_id, RawAddress& bd_addr, uint8_t size,
                                   uint8_t* sirk, const bluetooth::Uuid& p_srvc_uuid,
                                   bool lock_support) {
  ALOGI("%s: ", __func__);
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  // address
  ScopedLocalRef<jstring> address(sCallbackEnv.get(),
                                  bdaddr2newjstr(sCallbackEnv.get(), &bd_addr));

  ALOGI("%s: new_set_found_callback: process sirk", __func__);
  ScopedLocalRef<jbyteArray> jb(sCallbackEnv.get(), NULL);
  jb.reset(sCallbackEnv->NewByteArray(16));
  sCallbackEnv->SetByteArrayRegion(jb.get(), 0, 16, (jbyte*)sirk);

  sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onNewSetFound, set_id, address.get(),
                               size, jb.get(), UUID_PARAMS(p_srvc_uuid), lock_support);
}

static void new_set_member_found_cb(uint8_t set_id, RawAddress& bd_addr) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;

  // address
  ScopedLocalRef<jstring> address(sCallbackEnv.get(),
                                  bdaddr2newjstr(sCallbackEnv.get(), &bd_addr));
  sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onNewSetMemberFound, set_id, address.get());
}

/** Callback for lock status changed event from stack
 */
static void lock_state_changed_callback(uint8_t app_id, uint8_t set_id,
                                        uint8_t value, uint8_t status,
                                        std::vector<RawAddress> addr_list) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;
  if (!mCallbacksObj) {
    ALOGE("mCallbacksObj is NULL. Return.");
    return;
  }

  int i;
  jstring bd_addr;
  jobjectArray device_list;
  jsize len = addr_list.size();
  device_list = sCallbackEnv->NewObjectArray(
                    len, sCallbackEnv->FindClass("java/lang/String"), 0);

  for(i = 0; i < len; i++)
  {
    bd_addr = sCallbackEnv->NewStringUTF(addr_list[i].ToString().c_str());
    sCallbackEnv->SetObjectArrayElement(device_list, i, bd_addr);
  }

  sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onLockStatusChanged,
                               app_id, set_id, value, status, device_list);
}

/** Callback when lock is available on earlier denying set member
 */
static void lock_available_callback(uint8_t app_id, uint8_t set_id,
                                    RawAddress& bd_addr) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;
  if (!mCallbacksObj) {
    ALOGE("mCallbacksObj is NULL. Return.");
    return;
  }

// address
  ScopedLocalRef<jstring> address(sCallbackEnv.get(),
                                  bdaddr2newjstr(sCallbackEnv.get(), &bd_addr));

  sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onLockAvailable, app_id, set_id, address.get());
}

/** Callback when size of coordinated set has been changed
 */
static void set_size_changed_callback(uint8_t set_id, uint8_t size,
                                      RawAddress& bd_addr) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;
  if (!mCallbacksObj) {
    ALOGE("mCallbacksObj is NULL. Return.");
    return;
  }

  // address
  ScopedLocalRef<jstring> address(sCallbackEnv.get(),
                                  bdaddr2newjstr(sCallbackEnv.get(), &bd_addr));
  sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onSetSizeChanged, set_id, size, address.get());
}

/** Callback when SIRK of coordinated set has been changed
 */
static void set_sirk_changed_callback(uint8_t set_id, uint8_t* sirk,
                                      RawAddress& bd_addr) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;
  if (!mCallbacksObj) {
    ALOGE("mCallbacksObj is NULL. Return.");
    return;
  }

  ScopedLocalRef<jbyteArray> jb(sCallbackEnv.get(), NULL);
  jb.reset(sCallbackEnv->NewByteArray(24));
  sCallbackEnv->SetByteArrayRegion(jb.get(), 0, 24, (jbyte*)sirk);

  // address
  ScopedLocalRef<jstring> address(sCallbackEnv.get(),
                                  bdaddr2newjstr(sCallbackEnv.get(), &bd_addr));
  sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onSetSirkChanged, set_id, jb.get(), address.get());
}

/** Callback when new SIRK of coordinated set has been found
 */
static void rsi_data_found_callback(uint8_t* rsi,
        const RawAddress& bd_addr) {
  CallbackEnv sCallbackEnv(__func__);
  if (!sCallbackEnv.valid()) return;
  if (!mCallbacksObj) {
    ALOGE("mCallbacksObj is NULL. Return.");
    return;
  }
  ScopedLocalRef<jbyteArray> jb(sCallbackEnv.get(), NULL);
  jb.reset(sCallbackEnv->NewByteArray(6));
  sCallbackEnv->SetByteArrayRegion(jb.get(), 0, 6, (jbyte*)rsi);
  // address
  ScopedLocalRef<jstring> address(sCallbackEnv.get(),
                                  bdaddr2newjstr(sCallbackEnv.get(), &bd_addr));
  sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onRsiDataFound, jb.get(), address.get());
}

static btcsip_callbacks_t sBluetoothCsipCallbacks = {
  sizeof(sBluetoothCsipCallbacks),
  csip_app_registered_callback,
  connection_state_changed_callback,
  new_set_found_callback,
  new_set_member_found_cb,
  lock_state_changed_callback,
  lock_available_callback,
  set_size_changed_callback,
  set_sirk_changed_callback,
  rsi_data_found_callback,
};

static void initNative(JNIEnv* env, jobject object) {
  ALOGI("%s: initNative()", __func__);

  std::unique_lock<std::shared_timed_mutex> lock(mCallbacks_mutex);
  const bt_interface_t* btInf = getBluetoothInterface();
  if (btInf == NULL) {
    ALOGE("Bluetooth module is not loaded");
    return;
  }

  if (sBluetoothCsipInterface != NULL) {
    ALOGW("Cleaning up Bluetooth CSIP CLIENT Interface before initializing...");
    sBluetoothCsipInterface->cleanup();
    sBluetoothCsipInterface = NULL;
  }

  if (mCallbacksObj != NULL) {
    ALOGW("Cleaning up Bluetooth CSIP callback object");
    env->DeleteGlobalRef(mCallbacksObj);
    mCallbacksObj = NULL;
  }

  sBluetoothCsipInterface =
      (btcsip_interface_t*)btInf->get_profile_interface(BT_PROFILE_CSIP_CLIENT_ID);
  if (sBluetoothCsipInterface == NULL) {
    ALOGE("Failed to get Bluetooth CSIPInterface");
    return;
  }

  bt_status_t status = sBluetoothCsipInterface->init(&sBluetoothCsipCallbacks);
  if (status != BT_STATUS_SUCCESS) {
    ALOGE("Failed to initialize Bluetooth CSIP Client, status: %d", status);
    sBluetoothCsipInterface = NULL;
    return;
  }

  mCallbacksObj = env->NewGlobalRef(object);
}

static void cleanupNative(JNIEnv* env, jobject object) {
  ALOGI("%s: cleanupNative()", __func__);
  if (!sBluetoothCsipInterface) return;

  sBluetoothCsipInterface->cleanup();
}

static jboolean connectSetDeviceNative(JNIEnv* env, jobject object,
                                       jint app_id, jbyteArray address) {
  if (!sBluetoothCsipInterface) return JNI_FALSE;

  ALOGI("%s: connectSetDeviceNative()", __func__);
  jboolean ret = JNI_TRUE;
  jbyte* addr = env->GetByteArrayElements(address, nullptr);
  if (!addr) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  RawAddress bd_addr;
  bd_addr.FromOctets(reinterpret_cast<const uint8_t*>(addr));
  sBluetoothCsipInterface->connect(app_id, &bd_addr);
  return ret;
}

static jboolean disconnectSetDeviceNative(JNIEnv* env, jobject object,
                                            jint app_id, jbyteArray address) {
  if (!sBluetoothCsipInterface) return JNI_FALSE;

  jboolean ret = JNI_TRUE;
  ALOGI("%s: disconnectSetDeviceNative()", __func__);

  jbyte* addr = env->GetByteArrayElements(address, nullptr);
  if (!addr) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  RawAddress bd_addr;
  bd_addr.FromOctets(reinterpret_cast<const uint8_t*>(addr));
  sBluetoothCsipInterface->disconnect(app_id, &bd_addr);
  return ret;
}

static void registerCsipAppNative(JNIEnv* env, jobject object,
                                  jlong app_uuid_lsb, jlong app_uuid_msb) {
  if (!sBluetoothCsipInterface) return;

  Uuid uuid = from_java_uuid(app_uuid_msb, app_uuid_lsb);
  sBluetoothCsipInterface->register_csip_app(uuid);
}

static void unregisterCsipAppNative(JNIEnv* env, jobject object, jint app_id) {
  if (!sBluetoothCsipInterface) return;

  sBluetoothCsipInterface->unregister_csip_app(app_id);
}

static void setLockValueNative(JNIEnv* env, jobject object, jint app_id,
                               jint set_id, jint value, jobjectArray devicesList) {
  if (!sBluetoothCsipInterface) return;

  std::vector<RawAddress> lock_list;
  int listCount = env->GetArrayLength(devicesList);
  for (int i=0; i < listCount; i++) {
    jstring address = (jstring) (env->GetObjectArrayElement(devicesList, i));
    RawAddress bd_addr = str2addr(env, address);
    lock_list.push_back(bd_addr);
    env->DeleteLocalRef(address);
  }

  sBluetoothCsipInterface->set_lock_value(app_id, set_id, value, lock_list);
}

static void setOpportunisticScanNative(JNIEnv* env, jobject obj, jboolean isStart) {
  ALOGV("%s:  isStart : %d", __func__, isStart);
  if (!sBluetoothCsipInterface) {
    return;
  }
  sBluetoothCsipInterface->set_opportunistic_scan(isStart == JNI_TRUE ? 1 : 0);
}

static JNINativeMethod sMethods[] = {
    {"classInitNative", "()V", (void*)classInitNative},
    {"initNative", "()V", (void*)initNative},
    {"cleanupNative", "()V", (void*)cleanupNative},
    {"connectSetDeviceNative", "(I[B)Z", (void*)connectSetDeviceNative},
    {"disconnectSetDeviceNative", "(I[B)Z", (void*)disconnectSetDeviceNative},
    {"registerCsipAppNative", "(JJ)V", (void*)registerCsipAppNative},
    {"unregisterCsipAppNative", "(I)V", (void*)unregisterCsipAppNative},
    {"setLockValueNative", "(III[Ljava/lang/String;)V", (void*)setLockValueNative},
    {"setOpportunisticScanNative", "(Z)V", (void*)setOpportunisticScanNative}
};

int register_com_android_bluetooth_csip_setcoordinator(JNIEnv* env) {
  return jniRegisterNativeMethods(
      env, "com/android/bluetooth/csip/CsipSetCoordinatorNativeInterface",
      sMethods, NELEM(sMethods));
}
}
