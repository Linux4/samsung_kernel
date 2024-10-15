/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "Power.h"
#include "PowerHintSession.h"

#include "applaunch_hint.h"

#include <android-base/logging.h>
#include <sys/system_properties.h>

#include <OperatorFactory.h>

#include <unistd.h>
#include <fcntl.h>

using namespace epic;

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace impl {
namespace exynos {

const std::vector<Boost> BOOST_RANGE{ndk::enum_range<Boost>().begin(),
                                     ndk::enum_range<Boost>().end()};
const std::vector<Mode> MODE_RANGE{ndk::enum_range<Mode>().begin(), ndk::enum_range<Mode>().end()};

Power::Power()
{
    mTouchBooster = nullptr;
    mAppLaunchBooster = nullptr;
    mTouchBoosterDuration = nullptr;

    checkPropertyAvailable("vendor.power.touch", mEnableTouchBooster);
    checkPropertyAvailable("vendor.power.applaunch", mEnableAppLaunchBooster);
    checkPropertyAvailable("vendor.power.applaunchrelease", mEnableAppLaunchReleaseHint);

    setCurMode(MODE_DEF);
}

Power::~Power()
{
}

ndk::ScopedAStatus Power::setMode(Mode type, bool enabled) {
    LOG(INFO) << "Power setMode: " << static_cast<int32_t>(type) << " to: " << enabled;

    if (!checkEpicOperator())
        return ndk::ScopedAStatus::ok();

    switch (type) {
        case android::hardware::power::Mode::LAUNCH: {
            if (mEnableAppLaunchBooster) {
                if (enabled) {
                    setCurMode(static_cast<int32_t>(type));
                    bool ret = mAppLaunchBooster->doAction(eAcquire, nullptr);
                    if (!ret)
                        LOG(INFO) << "PowerHAL Malfunction at AppLaunch Boost";
                }
                else {
                    if (mEnableAppLaunchReleaseHint) {
                        bool ret = mAppLaunchBooster->doAction(eRelease, nullptr);
                        if (!ret)
                            LOG(INFO) << "PowerHAL malfunction at AppLaunchRelease Boost";
                    }
                    setCurMode(MODE_DEF);
                }
            }

            setApplaunch(enabled);
            break;
        }
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::isModeSupported(Mode type, bool* _aidl_return) {
    LOG(INFO) << "Power isModeSupported: " << static_cast<int32_t>(type);
    *_aidl_return = type >= MODE_RANGE.front() && type <= MODE_RANGE.back();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::setBoost(Boost type, int32_t durationMs) {
    if (getCurMode() != MODE_DEF)
        return ndk::ScopedAStatus::ok();

    if (!checkEpicOperator())
        return ndk::ScopedAStatus::ok();

    LOG(INFO) << "Power setBoost: " << static_cast<int32_t>(type)
                 << ", duration: " << durationMs;

    switch (type) {
        case android::hardware::power::Boost::INTERACTION: {
            if (mEnableTouchBooster) {
                bool ret = false;
                if (durationMs) {
                    int32_t arg_array[2] = { static_cast<int32_t>(100110), static_cast<int32_t>(durationMs * 1000) };
                    ret = mTouchBoosterDuration->doAction(eAcquireOption, arg_array);
                }
                else
                    ret = mTouchBooster->doAction(eAcquire, nullptr);

                if (!ret)
                    LOG(INFO) << "PowerHAL malfunction at Touch Boost";
            }
            break;
        }
        default:
            break;
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::isBoostSupported(Boost type, bool* _aidl_return) {
    LOG(INFO) << "Power isBoostSupported: " << static_cast<int32_t>(type);
    *_aidl_return = type >= BOOST_RANGE.front() && type <= BOOST_RANGE.back();
    return ndk::ScopedAStatus::ok();
}

void Power::checkPropertyAvailable(const std::string property, bool &flag)
{
    char propertyValue[PROP_VALUE_MAX];
    __system_property_get(property.c_str(), propertyValue);

    flag = (strcmp(propertyValue, "1") == 0);
}

void Power::setCurMode(int32_t type)
{
    curMode = type;
}

int32_t Power::getCurMode()
{
    return curMode;
}

bool Power::checkEpicOperator()
{
    if (mTouchBooster == nullptr) {
        mTouchBooster = std::shared_ptr<IEpicOperator>(reinterpret_cast<IEpicOperator*>(createScenarioOperator(eCommon, 100100)));
        LOG(INFO) << "Power Touch Operator set";
    }

    if (mAppLaunchBooster == nullptr) {
        mAppLaunchBooster = std::shared_ptr<IEpicOperator>(reinterpret_cast<IEpicOperator*>(createScenarioOperator(eCommon, 100200)));
        LOG(INFO) << "Power AppLaunch Operator set";
    }


    if (mTouchBoosterDuration == nullptr) {
        mTouchBoosterDuration = std::shared_ptr<IEpicOperator>(reinterpret_cast<IEpicOperator*>(createScenarioOperator(eCommon, 100110)));
        LOG(INFO) << "Power Touch Duration Operator set";
    }

    if (mTouchBooster == nullptr || mAppLaunchBooster == nullptr || mTouchBoosterDuration == nullptr)
        return false;

    return true;
}

ndk::ScopedAStatus Power::createHintSession(int32_t, int32_t, const std::vector<int32_t>& tids, int64_t,
                                            std::shared_ptr<IPowerHintSession>* _aidl_return) {
    if (tids.size() == 0) {
        *_aidl_return = nullptr;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::shared_ptr<IPowerHintSession> powerHintSession =
            ndk::SharedRefBase::make<PowerHintSession>();
    mPowerHintSessions.push_back(powerHintSession);
    *_aidl_return = powerHintSession;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::getHintSessionPreferredRate(int64_t* outNanoseconds) {
    *outNanoseconds = 16666666L;
    return ndk::ScopedAStatus::ok();
}

}  // namespace exynos
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
