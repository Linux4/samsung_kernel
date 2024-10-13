/*
 * Copyright (C) 2018,2020-2021, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright (C) 2018 The Android Open Source Project
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
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef ANDROID_HARDWARE_USB_GADGET_V1_2_USBGADGET_H
#define ANDROID_HARDWARE_USB_GADGET_V1_2_USBGADGET_H

#include <android/hardware/usb/gadget/1.2/IUsbGadget.h>
#include <hidl/Status.h>
#include <mutex>

namespace android {
namespace hardware {
namespace usb {
namespace gadget {
namespace V1_2 {
namespace implementation {

using ::android::hardware::Return;
using ::android::hardware::usb::gadget::MonitorFfs;

struct UsbGadget : public IUsbGadget {
  UsbGadget(const char* const gadget);

  Return<void> setCurrentUsbFunctions(uint64_t functions,
                                      const sp<V1_0::IUsbGadgetCallback>& callback,
                                      uint64_t timeout) override;

  Return<void> getCurrentUsbFunctions(
      const sp<V1_0::IUsbGadgetCallback>& callback) override;

  Return<Status> reset() override;

  Return<void> getUsbSpeed(const sp<V1_2::IUsbGadgetCallback>& callback) override;
private:
  V1_0::Status tearDownGadget();
  V1_0::Status setupFunctions(uint64_t functions,
                              const sp<V1_0::IUsbGadgetCallback>& callback,
                              uint64_t timeout);
  int addFunctionsFromPropString(std::string prop, bool &ffsEnabled, int &i);

  MonitorFfs mMonitorFfs;

  // Makes sure that only one request is processed at a time.
  std::mutex mLockSetCurrentFunction;

  uint64_t mCurrentUsbFunctions;
  bool mCurrentUsbFunctionsApplied;
};

}  // namespace implementation
}  // namespace V1_2
}  // namespace gadget
}  // namespace usb
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_USB_V1_2_USBGADGET_H
