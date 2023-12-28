/*
 * Copyright (c) 2014 The Android Open Source Project
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
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

#ifndef COM_ANDROID_BLUETOOTH_H
#define COM_ANDROID_BLUETOOTH_H

#include "hardware/bluetooth.h"
#include "hardware/hardware.h"
#include "jni.h"
#include "jni_logging.h"
#include "nativehelper/ScopedLocalRef.h"
#include "utils/Log.h"

namespace android {

JNIEnv* getCallbackEnv();
bool isCallbackThread();

class CallbackEnv {
public:
    CallbackEnv(const char *methodName) : mName(methodName) {
        mCallbackEnv = getCallbackEnv();
    }

    ~CallbackEnv() {
      if (mCallbackEnv && mCallbackEnv->ExceptionCheck()) {
          ALOGE("An exception was thrown by callback '%s'.", mName);
          LOGE_EX(mCallbackEnv);
          mCallbackEnv->ExceptionClear();
      }
    }

    bool valid() const {
      if (!mCallbackEnv || !isCallbackThread()) {
        ALOGE("%s: Callback env fail", mName);
        return false;
      }
      return true;
    }

    // stolen from art/runtime/jni/check_jni.cc
    bool isValidUtf(const char* bytes) const {
      while (*bytes != '\0') {
        const uint8_t* utf8 = reinterpret_cast<const uint8_t*>(bytes++);
        // Switch on the high four bits.
        switch (*utf8 >> 4) {
          case 0x00:
          case 0x01:
          case 0x02:
          case 0x03:
          case 0x04:
          case 0x05:
          case 0x06:
          case 0x07:
            // Bit pattern 0xxx. No need for any extra bytes.
            break;
          case 0x08:
          case 0x09:
          case 0x0a:
          case 0x0b:
            // Bit patterns 10xx, which are illegal start bytes.
            return false;
          case 0x0f:
            // Bit pattern 1111, which might be the start of a 4 byte sequence.
            if ((*utf8 & 0x08) == 0) {
              // Bit pattern 1111 0xxx, which is the start of a 4 byte sequence.
              // We consume one continuation byte here, and fall through to
              // consume two more.
              utf8 = reinterpret_cast<const uint8_t*>(bytes++);
              if ((*utf8 & 0xc0) != 0x80) {
                return false;
              }
            } else {
              return false;
            }
            // Fall through to the cases below to consume two more continuation
            // bytes.
            FALLTHROUGH_INTENDED;
          case 0x0e:
            // Bit pattern 1110, so there are two additional bytes.
            utf8 = reinterpret_cast<const uint8_t*>(bytes++);
            if ((*utf8 & 0xc0) != 0x80) {
              return false;
            }
            // Fall through to consume one more continuation byte.
            FALLTHROUGH_INTENDED;
          case 0x0c:
          case 0x0d:
            // Bit pattern 110x, so there is one additional byte.
            utf8 = reinterpret_cast<const uint8_t*>(bytes++);
            if ((*utf8 & 0xc0) != 0x80) {
              return false;
            }
            break;
        }
      }
      return true;
    }

    JNIEnv *operator-> () const {
        return mCallbackEnv;
    }

    JNIEnv *get() const {
        return mCallbackEnv;
    }

private:
    JNIEnv *mCallbackEnv;
    const char *mName;

    DISALLOW_COPY_AND_ASSIGN(CallbackEnv);
};

const bt_interface_t* getBluetoothInterface();

int register_com_android_bluetooth_hfp(JNIEnv* env);

int register_com_android_bluetooth_hfpclient(JNIEnv* env);

int register_com_android_bluetooth_a2dp(JNIEnv* env);

int register_com_android_bluetooth_a2dp_sink(JNIEnv* env);

int register_com_android_bluetooth_avrcp(JNIEnv* env);

int register_com_android_bluetooth_avrcp_target(JNIEnv* env);

int register_com_android_bluetooth_avrcp_controller(JNIEnv* env);

int register_com_android_bluetooth_hid_host(JNIEnv* env);

int register_com_android_bluetooth_hid_device(JNIEnv* env);

int register_com_android_bluetooth_pan(JNIEnv* env);

int register_com_android_bluetooth_gatt (JNIEnv* env);

int register_com_android_bluetooth_sdp (JNIEnv* env);

int register_com_android_bluetooth_btservice_vendor (JNIEnv* env);

int register_com_android_bluetooth_btservice_vendor_socket(JNIEnv* env);

int register_com_android_bluetooth_hearing_aid(JNIEnv* env);

int register_com_android_bluetooth_avrcp_ext(JNIEnv* env);

int register_com_android_bluetooth_ba(JNIEnv* env);

int register_com_android_bluetooth_hfp_vendorhfservice(JNIEnv* env);

int register_com_android_bluetooth_btservice_BluetoothKeystore(JNIEnv* env);

int register_com_android_bluetooth_csip_client(JNIEnv* env);

int register_com_android_bluetooth_csip_setcoordinator(JNIEnv* env);

int register_com_android_bluetooth_atp_locator(JNIEnv* env);
}

#endif /* COM_ANDROID_BLUETOOTH_H */
