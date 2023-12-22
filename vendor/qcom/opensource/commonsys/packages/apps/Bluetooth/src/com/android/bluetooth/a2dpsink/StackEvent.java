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
package com.android.bluetooth.a2dpsink;

import android.bluetooth.BluetoothDevice;

final class StackEvent {
    // Event types for STACK_EVENT message
    static final int EVENT_TYPE_NONE = 0;
    static final int EVENT_TYPE_CONNECTION_STATE_CHANGED = 1;
    static final int EVENT_TYPE_AUDIO_STATE_CHANGED = 2;
    static final int EVENT_TYPE_AUDIO_CONFIG_CHANGED = 3;

    // match up with btav_connection_state_t enum of bt_av.h
    static final int CONNECTION_STATE_DISCONNECTED = 0;
    static final int CONNECTION_STATE_CONNECTING = 1;
    static final int CONNECTION_STATE_CONNECTED = 2;
    static final int CONNECTION_STATE_DISCONNECTING = 3;

    // match up with btav_audio_state_t enum of bt_av.h
    static final int AUDIO_STATE_REMOTE_SUSPEND = 0;
    static final int AUDIO_STATE_STOPPED = 1;
    static final int AUDIO_STATE_STARTED = 2;

    int mType = EVENT_TYPE_NONE;
    BluetoothDevice mDevice = null;
    int mState = 0;
    int mSampleRate = 0;
    int mChannelCount = 0;

    private StackEvent(int type) {
        this.mType = type;
    }

    @Override
    public String toString() {
        switch (mType) {
            case EVENT_TYPE_CONNECTION_STATE_CHANGED:
                return "EVENT_TYPE_CONNECTION_STATE_CHANGED " + mState;
            case EVENT_TYPE_AUDIO_STATE_CHANGED:
                return "EVENT_TYPE_AUDIO_STATE_CHANGED " + mState;
            case EVENT_TYPE_AUDIO_CONFIG_CHANGED:
                return "EVENT_TYPE_AUDIO_CONFIG_CHANGED " + mSampleRate + ":" + mChannelCount;
            default:
                return "Unknown";
        }
    }

    static StackEvent connectionStateChanged(BluetoothDevice device, int state) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        event.mDevice = device;
        event.mState = state;
        return event;
    }

    static StackEvent audioStateChanged(BluetoothDevice device, int state) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_AUDIO_STATE_CHANGED);
        event.mDevice = device;
        event.mState = state;
        return event;
    }

    static StackEvent audioConfigChanged(BluetoothDevice device, int sampleRate,
            int channelCount) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_AUDIO_CONFIG_CHANGED);
        event.mDevice = device;
        event.mSampleRate = sampleRate;
        event.mChannelCount = channelCount;
        return event;
    }
}
