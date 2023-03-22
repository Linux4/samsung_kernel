/*
 * Copyright 2020 The Android Open Source Project
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

#define LOG_TAG "BluetoothKeystoreServiceJni"

#include "base/logging.h"
#include "com_android_bluetooth.h"
#include "hardware/bt_keystore.h"

#include <string.h>
#include <shared_mutex>

using bluetooth::bluetooth_keystore::BluetoothKeystoreCallbacks;
using bluetooth::bluetooth_keystore::BluetoothKeystoreInterface;

namespace android {
static jmethodID method_setEncryptKeyOrRemoveKeyCallback;
static jmethodID method_getKeyCallback;

static BluetoothKeystoreInterface* sBluetoothKeystoreInterface = nullptr;
static std::shared_timed_mutex interface_mutex;

static jobject mCallbacksObj = nullptr;
static std::shared_timed_mutex callbacks_mutex;

class BluetoothKeystoreCallbacksImpl
    : public bluetooth::bluetooth_keystore::BluetoothKeystoreCallbacks {
 public:
  ~BluetoothKeystoreCallbacksImpl() = default;

  void set_encrypt_key_or_remove_key(
      const std::string prefixString,
      const std::string decryptedString) override {
    LOG(INFO) << __func__;

    std::shared_lock<std::shared_timed_mutex> lock(callbacks_mutex);
    CallbackEnv sCallbackEnv(__func__);
    if (!sCallbackEnv.valid() || mCallbacksObj == nullptr) return;

    jstring j_prefixString = sCallbackEnv->NewStringUTF(prefixString.c_str());
    jstring j_decryptedString =
        sCallbackEnv->NewStringUTF(decryptedString.c_str());

    sCallbackEnv->CallVoidMethod(mCallbacksObj,
                                 method_setEncryptKeyOrRemoveKeyCallback,
                                 j_prefixString, j_decryptedString);
  }

  std::string get_key(const std::string prefixString) override {
    LOG(INFO) << __func__;

    std::shared_lock<std::shared_timed_mutex> lock(callbacks_mutex);
    CallbackEnv sCallbackEnv(__func__);
    if (!sCallbackEnv.valid() || mCallbacksObj == nullptr) return "";

    jstring j_prefixString = sCallbackEnv->NewStringUTF(prefixString.c_str());

    jstring j_decrypt_str = (jstring)sCallbackEnv->CallObjectMethod(
        mCallbacksObj, method_getKeyCallback, j_prefixString);

    if (j_decrypt_str == nullptr) {
      ALOGE("%s: Got a null decrypt_str", __func__);
      return "";
    }

    const char* value = sCallbackEnv->GetStringUTFChars(j_decrypt_str, nullptr);
    std::string ret(value);
    sCallbackEnv->ReleaseStringUTFChars(j_decrypt_str, value);

    return ret;
  }
};

static BluetoothKeystoreCallbacksImpl sBluetoothKeystoreCallbacks;

static void classInitNative(JNIEnv* env, jclass clazz) {
  method_setEncryptKeyOrRemoveKeyCallback =
      env->GetMethodID(clazz, "setEncryptKeyOrRemoveKeyCallback",
                       "(Ljava/lang/String;Ljava/lang/String;)V");

  method_getKeyCallback = env->GetMethodID(
      clazz, "getKeyCallback", "(Ljava/lang/String;)Ljava/lang/String;");

  LOG(INFO) << __func__ << ": succeeds";
}

static void initNative(JNIEnv* env, jobject object) {
  std::unique_lock<std::shared_timed_mutex> interface_lock(interface_mutex);
  std::unique_lock<std::shared_timed_mutex> callbacks_lock(callbacks_mutex);

  const bt_interface_t* btInf = getBluetoothInterface();
  if (btInf == nullptr) {
    LOG(ERROR) << "Bluetooth module is not loaded";
    return;
  }

  if (sBluetoothKeystoreInterface != nullptr) {
    LOG(INFO)
        << "Cleaning up BluetoothKeystore Interface before initializing...";
    sBluetoothKeystoreInterface = nullptr;
  }

  if (mCallbacksObj != nullptr) {
    LOG(INFO) << "Cleaning up BluetoothKeystore callback object";
    env->DeleteGlobalRef(mCallbacksObj);
    mCallbacksObj = nullptr;
  }

  if ((mCallbacksObj = env->NewGlobalRef(object)) == nullptr) {
    LOG(ERROR)
        << "Failed to allocate Global Ref for BluetoothKeystore Callbacks";
    return;
  }

  sBluetoothKeystoreInterface =
      (BluetoothKeystoreInterface*)btInf->get_profile_interface(BT_KEYSTORE_ID);
  if (sBluetoothKeystoreInterface == nullptr) {
    LOG(ERROR) << "Failed to get BluetoothKeystore Interface";
    return;
  }

  sBluetoothKeystoreInterface->init(&sBluetoothKeystoreCallbacks);
}

static void cleanupNative(JNIEnv* env, jobject object) {
  std::unique_lock<std::shared_timed_mutex> interface_lock(interface_mutex);
  std::unique_lock<std::shared_timed_mutex> callbacks_lock(callbacks_mutex);

  const bt_interface_t* btInf = getBluetoothInterface();
  if (btInf == nullptr) {
    LOG(ERROR) << "Bluetooth module is not loaded";
    return;
  }

  if (sBluetoothKeystoreInterface != nullptr) {
    sBluetoothKeystoreInterface = nullptr;
  }

  if (mCallbacksObj != nullptr) {
    env->DeleteGlobalRef(mCallbacksObj);
    mCallbacksObj = nullptr;
  }
}

static JNINativeMethod sMethods[] = {
    {"classInitNative", "()V", (void*)classInitNative},
    {"initNative", "()V", (void*)initNative},
    {"cleanupNative", "()V", (void*)cleanupNative},
};

int register_com_android_bluetooth_btservice_BluetoothKeystore(JNIEnv* env) {
  return jniRegisterNativeMethods(
      env,
      "com/android/bluetooth/btservice/bluetoothkeystore/"
      "BluetoothKeystoreNativeInterface",
      sMethods, NELEM(sMethods));
}
}  // namespace android
