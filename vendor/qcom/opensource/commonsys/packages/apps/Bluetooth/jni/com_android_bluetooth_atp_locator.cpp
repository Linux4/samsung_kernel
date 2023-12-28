/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

#define LOG_TAG "BluetoothATPLocatorJni"

#define LOG_NDEBUG 0

#include "base/logging.h"
#include "com_android_bluetooth.h"
#include "hardware/bt_atp_locator.h"

#include <string.h>
#include <shared_mutex>

using bluetooth::atp_locator::ConnectionState;
using bluetooth::atp_locator::AtpLocatorCallbacks;
using bluetooth::atp_locator::AtpLocatorInterface;

namespace android {
static jmethodID method_onConnectionStateChanged;
static jmethodID method_onEnableBleDirectionFinding;
static jmethodID method_onLeAoaResults;

static AtpLocatorInterface* sAtpLocatorInterface = nullptr;
static std::shared_timed_mutex interface_mutex;

static jobject mCallbacksObj = nullptr;
static std::shared_timed_mutex callbacks_mutex;

class AtpLocatorCallbacksImpl : public AtpLocatorCallbacks {
 public:
  ~AtpLocatorCallbacksImpl() = default;

  void OnConnectionState(ConnectionState state,
                         const RawAddress& bd_addr) override {
    LOG(INFO) << __func__;

    std::shared_lock<std::shared_timed_mutex> lock(callbacks_mutex);
    CallbackEnv sCallbackEnv(__func__);
    if (!sCallbackEnv.valid() || mCallbacksObj == nullptr) return;

    ScopedLocalRef<jbyteArray> addr(
        sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(RawAddress)));
    if (!addr.get()) {
      LOG(ERROR) << "Failed to new jbyteArray bd addr for connection state";
      return;
    }

    sCallbackEnv->SetByteArrayRegion(addr.get(), 0, sizeof(RawAddress),
                                     (jbyte*)&bd_addr);
    sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onConnectionStateChanged,
                                 (jint)state, addr.get());
  }

  void OnEnableBleDirectionFinding(uint8_t status, const RawAddress& bd_addr) override {
    LOG(INFO) << __func__;

    std::shared_lock<std::shared_timed_mutex> lock(callbacks_mutex);
    CallbackEnv sCallbackEnv(__func__);
    if (!sCallbackEnv.valid() || mCallbacksObj == nullptr) return;

    ScopedLocalRef<jbyteArray> addr(
        sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(RawAddress)));
    if (!addr.get()) {
      LOG(ERROR) << "Failed to new jbyteArray bd addr for connection state";
      return;
    }

    sCallbackEnv->SetByteArrayRegion(addr.get(), 0, sizeof(RawAddress),
                                     (jbyte*)&bd_addr);
    sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onEnableBleDirectionFinding,
                                 (jint)status, addr.get());
  }

  void OnLeAoaResults(uint8_t status, double azimuth, uint8_t azimuth_unc,
                      double elevation, uint8_t elevation_unc,
                      const RawAddress& bd_addr) override {
    LOG(INFO) << __func__;

    std::shared_lock<std::shared_timed_mutex> lock(callbacks_mutex);
    CallbackEnv sCallbackEnv(__func__);
    if (!sCallbackEnv.valid() || mCallbacksObj == nullptr) return;

    ScopedLocalRef<jbyteArray> addr(
        sCallbackEnv.get(), sCallbackEnv->NewByteArray(sizeof(RawAddress)));
    if (!addr.get()) {
      LOG(ERROR) << "Failed to new jbyteArray bd addr for connection state";
      return;
    }

    sCallbackEnv->SetByteArrayRegion(addr.get(), 0, sizeof(RawAddress),
                                     (jbyte*)&bd_addr);
    sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onLeAoaResults,
                                 (jint)status, (jdouble)azimuth, (jint)azimuth_unc,
                                 (jdouble)elevation, (jint)elevation_unc, addr.get());
  }
};

static AtpLocatorCallbacksImpl sAtpLocatorCallbacks;

static void classInitNative(JNIEnv* env, jclass clazz) {
  method_onConnectionStateChanged =
      env->GetMethodID(clazz, "onConnectionStateChanged", "(I[B)V");

  method_onEnableBleDirectionFinding =
        env->GetMethodID(clazz, "OnEnableBleDirectionFinding", "(I[B)V");
  method_onLeAoaResults =
          env->GetMethodID(clazz, "OnLeAoaResults", "(IDIDI[B)V");

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

  if (sAtpLocatorInterface != nullptr) {
    LOG(INFO) << "Cleaning up AtpLocator Interface before initializing...";
    sAtpLocatorInterface->Cleanup();
    sAtpLocatorInterface = nullptr;
  }

  if (mCallbacksObj != nullptr) {
    LOG(INFO) << "Cleaning up AtpLocator callback object";
    env->DeleteGlobalRef(mCallbacksObj);
    mCallbacksObj = nullptr;
  }

  if ((mCallbacksObj = env->NewGlobalRef(object)) == nullptr) {
    LOG(ERROR) << "Failed to allocate Global Ref for Atp Locator Callbacks";
    return;
  }

  sAtpLocatorInterface = (AtpLocatorInterface*)btInf->get_profile_interface(
      BT_PROFILE_ATP_LOCATOR_ID);
  if (sAtpLocatorInterface == nullptr) {
    LOG(ERROR) << "Failed to get Bluetooth ATP Locator Interface";
    return;
  }

  LOG(INFO) << "Calling sAtpLocatorInterface init";
  sAtpLocatorInterface->Init(&sAtpLocatorCallbacks);
}

static void cleanupNative(JNIEnv* env, jobject object) {
  std::unique_lock<std::shared_timed_mutex> interface_lock(interface_mutex);
  std::unique_lock<std::shared_timed_mutex> callbacks_lock(callbacks_mutex);

  const bt_interface_t* btInf = getBluetoothInterface();
  if (btInf == nullptr) {
    LOG(ERROR) << "Bluetooth module is not loaded";
    return;
  }

  if (sAtpLocatorInterface != nullptr) {
    sAtpLocatorInterface->Cleanup();
    sAtpLocatorInterface = nullptr;
  }

  if (mCallbacksObj != nullptr) {
    env->DeleteGlobalRef(mCallbacksObj);
    mCallbacksObj = nullptr;
  }
}

static jboolean connectAtpNative(JNIEnv* env, jobject object,
                                 jbyteArray address, jboolean isDirect) {
  LOG(INFO) << __func__;
  std::shared_lock<std::shared_timed_mutex> lock(interface_mutex);
  if (!sAtpLocatorInterface) return JNI_FALSE;

  jbyte* addr = env->GetByteArrayElements(address, nullptr);
  if (!addr) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  RawAddress* tmpraw = (RawAddress*)addr;
  sAtpLocatorInterface->Connect(*tmpraw, isDirect);
  env->ReleaseByteArrayElements(address, addr, 0);
  return JNI_TRUE;
}

static jboolean disconnectAtpNative(JNIEnv* env, jobject object,
                                    jbyteArray address) {
  LOG(INFO) << __func__;
  std::shared_lock<std::shared_timed_mutex> lock(interface_mutex);
  if (!sAtpLocatorInterface) return JNI_FALSE;

  jbyte* addr = env->GetByteArrayElements(address, nullptr);
  if (!addr) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  RawAddress* tmpraw = (RawAddress*)addr;
  sAtpLocatorInterface->Disconnect(*tmpraw);
  env->ReleaseByteArrayElements(address, addr, 0);
  return JNI_TRUE;
}

static jboolean enableBleDirFindingNative(JNIEnv* env, jobject object,
                                   jint sampling_enable, jint slot_durations,
                                   jint enable, jint cte_req_int, jint req_cte_len,
                                   jint direction_finding_type, jbyteArray address) {
  LOG(INFO) << __func__;
  std::shared_lock<std::shared_timed_mutex> lock(interface_mutex);
  if (!sAtpLocatorInterface) return JNI_FALSE;

  jbyte* addr = env->GetByteArrayElements(address, nullptr);
  if (!addr) {
    jniThrowIOException(env, EINVAL);
    return JNI_FALSE;
  }

  RawAddress* tmpraw = (RawAddress*)addr;
  sAtpLocatorInterface->EnableBleDirectionFinding(sampling_enable, slot_durations,
          enable, cte_req_int, req_cte_len, direction_finding_type, *tmpraw);
  env->ReleaseByteArrayElements(address, addr, 0);
  return JNI_TRUE;
}

static JNINativeMethod sMethods[] = {
    {"classInitNative", "()V", (void*)classInitNative},
    {"initNative", "()V", (void*)initNative},
    {"cleanupNative", "()V", (void*)cleanupNative},
    {"connectAtpNative", "([BZ)Z", (void*)connectAtpNative},
    {"disconnectAtpNative", "([B)Z", (void*)disconnectAtpNative},
    {"enableBleDirFindingNative", "(IIIIII[B)Z", (void*)enableBleDirFindingNative},
};

int register_com_android_bluetooth_atp_locator(JNIEnv* env) {
  return jniRegisterNativeMethods(
      env, "com/android/bluetooth/directionfinder/AtpLocatorNativeInterface",
      sMethods, NELEM(sMethods));
}
}  // namespace android

