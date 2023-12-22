/*
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
 */
package com.android.bluetooth.avrcpcontroller;

final class StackEvent {
    // Event types for STACK_EVENT message
    static final int EVENT_TYPE_NONE = 0;
    static final int EVENT_TYPE_CONNECTION_STATE_CHANGED = 1;
    static final int EVENT_TYPE_RC_FEATURES = 2;

    int mType = EVENT_TYPE_NONE;
    boolean mRemoteControlConnected;
    boolean mBrowsingConnected;
    int mFeatures;

    private StackEvent(int type) {
        this.mType = type;
    }

    @Override
    public String toString() {
        switch (mType) {
            case EVENT_TYPE_CONNECTION_STATE_CHANGED:
                return "EVENT_TYPE_CONNECTION_STATE_CHANGED " + mRemoteControlConnected;
            case EVENT_TYPE_RC_FEATURES:
                return "EVENT_TYPE_RC_FEATURES";
            default:
                return "Unknown";
        }
    }

    static StackEvent connectionStateChanged(boolean remoteControlConnected,
            boolean browsingConnected) {
        StackEvent event = new StackEvent(EVENT_TYPE_CONNECTION_STATE_CHANGED);
        event.mRemoteControlConnected = remoteControlConnected;
        event.mBrowsingConnected = browsingConnected;
        return event;
    }

    static StackEvent rcFeatures(int features) {
        StackEvent event = new StackEvent(EVENT_TYPE_RC_FEATURES);
        event.mFeatures = features;
        return event;
    }
}
