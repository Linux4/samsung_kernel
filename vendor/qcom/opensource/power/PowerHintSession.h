/*
* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef __POWERHINTSESSION__
#define __POWERHINTSESSION__

#include <unordered_map>
#include <set>
#include <mutex>
#include <aidl/android/hardware/power/WorkDuration.h>
#include <aidl/android/hardware/power/SessionHint.h>
#include <aidl/android/hardware/power/BnPowerHintSession.h>

enum LOAD_TYPE {
    LOAD_UP,
    LOAD_DOWN,
    LOAD_RESET,
    LOAD_RESUME
};

std::shared_ptr<aidl::android::hardware::power::IPowerHintSession> setPowerHintSession(int32_t tgid, int32_t uid, const std::vector<int32_t>& threadIds);
int64_t getSessionPreferredRate();

class PowerHintSessionImpl : public aidl::android::hardware::power::BnPowerHintSession{
public:
    explicit PowerHintSessionImpl(int32_t tgid, int32_t uid, const std::vector<int32_t>& threadIds);
    ~PowerHintSessionImpl();
    ndk::ScopedAStatus updateTargetWorkDuration(int64_t targetDurationNanos) override;
    ndk::ScopedAStatus reportActualWorkDuration(
            const std::vector<aidl::android::hardware::power::WorkDuration>& durations) override;
    ndk::ScopedAStatus pause() override;
    ndk::ScopedAStatus resume() override;
    ndk::ScopedAStatus close() override;
    ndk::ScopedAStatus sendHint(aidl::android::hardware::power::SessionHint hint) override;
    ndk::ScopedAStatus setThreads(const std::vector<int32_t>& threadIds) override;
    bool perfBoost(int boostVal, int hintType);
    int setThreadPipelining(int32_t tid);
    void removePipelining();
    void resumeThreadPipelining();
    void resetBoost();
private:
    int32_t mUid;
    int32_t mTgid;
    int mHandle;
    int mBoostSum;
    int mLastAction;
    std::unordered_map<int32_t, int> mThreadHandles;
};
#endif /* __POWERHINTSESSION__ */
