/*
* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "PowerHintSession.h"
#include "utils.h"
#include "hint-data.h"
#include "performance.h"

#define CPU_BOOST_HINT      0x0000104E
#define THREAD_LOW_LATENCY  0x40CD0000
#define MAX_BOOST 200
#define MIN_BOOST -200

#include <android-base/logging.h>
#define LOG_TAG "QTI PowerHAL"

std::unordered_map<PowerHintSessionImpl*, int32_t> mPowerHintSessions;
std::mutex mSessionLock;

static int validateBoost(int boostVal, int boostSum) {
    boostSum += boostVal;
    if(boostSum > MAX_BOOST)
        return MAX_BOOST;
    else if(boostSum < MIN_BOOST)
        return MIN_BOOST;
    return boostSum;
}

void PowerHintSessionImpl::resetBoost() {
    if(mHandle > 0)
        release_request(mHandle);
    mHandle = -1;
    mLastAction = LOAD_RESET;
}

bool PowerHintSessionImpl::perfBoost(int boostVal, int hintType) {
    int tBoostSum = mBoostSum;
    int mHandlePerfHint = -1;

    if(hintType == LOAD_RESET){
        resetBoost();
        return true;
    }

    if(hintType == LOAD_RESUME && mLastAction != LOAD_RESET) {
        tBoostSum = 0;
    }

    tBoostSum = validateBoost(boostVal, tBoostSum);
    if(tBoostSum != 0) {
        mHandlePerfHint = perf_hint_enable(CPU_BOOST_HINT, tBoostSum);
        if(mHandlePerfHint < 0) {
            LOG(ERROR) << "Unable to acquire Perf hint for" << CPU_BOOST_HINT;
            return false;
        }
    }

    if(mHandle > 0)
        release_request(mHandle);
    mBoostSum = tBoostSum;
    mHandle = mHandlePerfHint;
    mLastAction = hintType;
    return true;
}

bool isSessionAlive(PowerHintSessionImpl* session) {
    if(mPowerHintSessions.find(session) != mPowerHintSessions.end())
        return true;
    return false;
}

bool isSessionActive(PowerHintSessionImpl* session) {
    if(!isSessionAlive(session))
        return false;
    if(mPowerHintSessions[session] == 1)
        return true;
    return false;
}

std::shared_ptr<aidl::android::hardware::power::IPowerHintSession> setPowerHintSession(int32_t tgid, int32_t uid, const std::vector<int32_t>& threadIds){
    LOG(INFO) << "setPowerHintSession ";
    std::shared_ptr<aidl::android::hardware::power::IPowerHintSession> mPowerSession = ndk::SharedRefBase::make<PowerHintSessionImpl>(tgid, uid, threadIds);

    if(mPowerSession == nullptr) {
        return nullptr;
    }
    return mPowerSession;
}

int64_t getSessionPreferredRate(){
    return 16666666L;
}

void setSessionActivity(PowerHintSessionImpl* session, bool flag) {
    std::lock_guard<std::mutex> mLockGuard(mSessionLock);
    if(flag)
        mPowerHintSessions[session] = 1;
    else
        mPowerHintSessions[session] = 0;
}

PowerHintSessionImpl::PowerHintSessionImpl(int32_t tgid, int32_t uid, const std::vector<int32_t>& threadIds){
     mUid = uid;
     mTgid = tgid;
     mBoostSum = 0;
     mHandle = -1;
     mLastAction = -1;
     setSessionActivity(this, true);
     for(auto tid: threadIds)
         mThreadHandles[tid] = setThreadPipelining(tid);
}

PowerHintSessionImpl::~PowerHintSessionImpl(){
     close();
}

int PowerHintSessionImpl::setThreadPipelining(int32_t tid) {
    int mHandleTid = -1;
    int args[2] = {THREAD_LOW_LATENCY, tid};

    mHandleTid = interaction_with_handle(0, 0, 2, args);
    if(mHandleTid < 0) {
            LOG(ERROR) << "Unable to put this thread tid into pipeline" << tid;
            return -1;
    }

    LOG(INFO) << "Thread with tid " << tid << " Handle " << mHandleTid;
    return mHandleTid;
}

void PowerHintSessionImpl::removePipelining() {
    for(auto& it: mThreadHandles) {
        if(it.second > 0) {
            release_request(it.second);
	    LOG(INFO) << "Thread with tid " << it.first << " Handle " << it.second << " released";
	}
        it.second = -1;
    }
}

void PowerHintSessionImpl::resumeThreadPipelining() {
   for(auto& it: mThreadHandles) {
       it.second = setThreadPipelining(it.first);
   }
}

ndk::ScopedAStatus PowerHintSessionImpl::updateTargetWorkDuration(int64_t in_targetDurationNanos){
    LOG(INFO) << "updateTargetWorkDuration " << in_targetDurationNanos;
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus PowerHintSessionImpl::reportActualWorkDuration(const std::vector<::aidl::android::hardware::power::WorkDuration>& in_durations){
    LOG(INFO) << "reportActualWorkDuration ";
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus PowerHintSessionImpl::pause(){
    LOG(INFO) << "PowerHintSessionImpl::pause ";
    if(isSessionAlive(this)) {
        setSessionActivity(this, false);
        sendHint(aidl::android::hardware::power::SessionHint::CPU_LOAD_RESET);
        removePipelining();
    }
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus PowerHintSessionImpl::resume(){
    LOG(INFO) << "PowerHintSessionImpl::resume ";
    if(isSessionAlive(this)) {
        sendHint(aidl::android::hardware::power::SessionHint::CPU_LOAD_RESUME);
        resumeThreadPipelining();
        setSessionActivity(this, true);
    }
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus PowerHintSessionImpl::close(){
    LOG(INFO) << "PowerHintSessionImpl::close ";

    if(isSessionAlive(this)) {
        sendHint(aidl::android::hardware::power::SessionHint::CPU_LOAD_RESET);
        removePipelining();
        mThreadHandles.clear();

        mSessionLock.lock();
        mPowerHintSessions.erase(this);
        mSessionLock.unlock();
    }
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus PowerHintSessionImpl::sendHint(aidl::android::hardware::power::SessionHint hint){
    LOG(INFO) << "PowerHintSessionImpl::sendHint ";
    if(!isSessionActive(this))
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    switch(hint)
    {
        case aidl::android::hardware::power::SessionHint::CPU_LOAD_UP:
            perfBoost(20, LOAD_UP);
            break;
        case aidl::android::hardware::power::SessionHint::CPU_LOAD_DOWN:
            perfBoost(-20, LOAD_DOWN);
            break;
        case aidl::android::hardware::power::SessionHint::CPU_LOAD_RESET:
            perfBoost(0, LOAD_RESET);
            break;
        case aidl::android::hardware::power::SessionHint::CPU_LOAD_RESUME:
            perfBoost(0, LOAD_RESUME);
            break;
        case aidl::android::hardware::power::SessionHint::POWER_EFFICIENCY:
            perfBoost(-20, LOAD_DOWN);
            break;
    }
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus PowerHintSessionImpl::setThreads(const std::vector<int32_t>& threadIds){
    LOG(INFO) << "PowerHintSessionImpl::setThreads ";
    if (threadIds.size() == 0) {
        LOG(ERROR) << "Error: threadIds.size() shouldn't be " << threadIds.size();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    for(auto tid: threadIds) {
        if(mThreadHandles.find(tid) == mThreadHandles.end())
            mThreadHandles[tid] = setThreadPipelining(tid);
    }

    for(auto it: mThreadHandles) {
        if(find(threadIds.begin(), threadIds.end(), it.first) == threadIds.end()) {
            if(it.second > 0)
                release_request(it.second);
            mThreadHandles.erase(it.first);
        }
    }

    return ndk::ScopedAStatus::ok();
}
