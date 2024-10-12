/*
 * Copyright 2021 HIMSA II K/S - www.himsa.com.
 * Represented by EHIMA - www.ehima.com
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

package com.android.bluetooth.csip;

import android.bluetooth.BluetoothDevice;

import java.util.UUID;

/**
 * CSIP Set Coordinator role stack event
 */
public class CsipSetCoordinatorStackEvent {
    // Event types for STACK_EVENT message (coming from native)
    private static final int EVENT_TYPE_NONE = 0;
    public static final int EVENT_TYPE_CONNECTION_STATE_CHANGED = 1;
    public static final int EVENT_TYPE_DEVICE_AVAILABLE = 2;
    public static final int EVENT_TYPE_SET_MEMBER_AVAILABLE = 3;
    public static final int EVENT_TYPE_GROUP_LOCK_CHANGED = 4;

    // Do not modify without updating the HAL bt_csis.h files.
    // Match up with enum class ConnectionState of bt_csis.h.
    static final int CONNECTION_STATE_DISCONNECTED = 0;
    static final int CONNECTION_STATE_CONNECTING = 1;
    static final int CONNECTION_STATE_CONNECTED = 2;
    static final int CONNECTION_STATE_DISCONNECTING = 3;

    static final int LOCK_STATUS_SUCCESS = 0;
    static final int LOCK_STATUS_FAILED_INVALID_GROUP = 1;
    static final int LOCK_STATUS_FAILED_GROUP_EMPTY = 2;
    static final int LOCK_STATUS_FAILED_GROUP_NOT_CONNECTED = 3;
    static final int LOCK_STATUS_FAILED_GROUP_LOCKED_BY_OTHERS = 5;
    static final int LOCK_STATUS_FAILED_OTHER_REASON = 6;
    static final int LOCK_STATUS_GROUP_MEMBER_LOST = 7;

    public int type = EVENT_TYPE_NONE;
    public BluetoothDevice device;
    public int valueInt1 = 0;
    public int valueInt2 = 0;
    public UUID valueUuid1;
    public byte[] sirk ;
    boolean lockSupport = false;
    public boolean valueBool1 = false;

    CsipSetCoordinatorStackEvent(int type) {
        this.type = type;
    }

    @Override
    public String toString() {
        // event dump
        StringBuilder result = new StringBuilder();
        result.append("CsipSetCoordinatorStackEvent {type:" + eventTypeToString(type));
        result.append(", device:" + device);
        result.append(", " + eventTypeValueInt1ToString(type, valueInt1));
        result.append(", " + eventTypeValueInt2ToString(type, valueInt2));
        result.append(", " + eventTypeValueBool1ToString(type, valueBool1));
        result.append(", " + eventTypeValueUuid1ToString(type, valueUuid1));
        result.append("}");
        return result.toString();
    }

    private static String eventTypeToString(int type) {
        switch (type) {
            case EVENT_TYPE_NONE:
                return "EVENT_TYPE_NONE";
            case EVENT_TYPE_CONNECTION_STATE_CHANGED:
                return "EVENT_TYPE_CONNECTION_STATE_CHANGED";
            case EVENT_TYPE_DEVICE_AVAILABLE:
                return "EVENT_TYPE_DEVICE_AVAILABLE";
            case EVENT_TYPE_SET_MEMBER_AVAILABLE:
                return "EVENT_TYPE_SET_MEMBER_AVAILABLE";
            case EVENT_TYPE_GROUP_LOCK_CHANGED:
                return "EVENT_TYPE_GROUP_LOCK_CHANGED";
            default:
                return "EVENT_TYPE_UNKNOWN:" + type;
        }
    }

    private static String connectionStateToString(int value) {
        switch (value) {
            case CONNECTION_STATE_DISCONNECTED:
                return "CONNECTION_STATE_DISCONNECTED";
            case CONNECTION_STATE_CONNECTING:
                return "CONNECTION_STATE_CONNECTING";
            case CONNECTION_STATE_CONNECTED:
                return "CONNECTION_STATE_CONNECTED";
            case CONNECTION_STATE_DISCONNECTING:
                return "CONNECTION_STATE_DISCONNECTING";
            default:
                return "STATE_UNKNOWN";
        }
    }

    private static String eventTypeValueBool1ToString(int evType, boolean value) {
        switch (evType) {
            case EVENT_TYPE_GROUP_LOCK_CHANGED:
                return "locked: " + Boolean.toString(value);
            default:
                return "<unused>";
        }
    }

    private static String eventTypeValueUuid1ToString(int evType, UUID value) {
        switch (evType) {
            case EVENT_TYPE_DEVICE_AVAILABLE:
                return "csis uuid: " + value;
            default:
                return "<unused>";
        }
    }

    private static String eventTypeValueInt1ToString(int evType, int value) {
        switch (evType) {
            case EVENT_TYPE_CONNECTION_STATE_CHANGED:
                return connectionStateToString(value);
            case EVENT_TYPE_DEVICE_AVAILABLE:
            case EVENT_TYPE_GROUP_LOCK_CHANGED:
            case EVENT_TYPE_SET_MEMBER_AVAILABLE:
                return "group id: " + value;
            default:
                return "<unused>";
        }
    }

    private static String eventTypeValueInt2ToString(int evType, int value) {
        switch (evType) {
            case EVENT_TYPE_DEVICE_AVAILABLE:
                return "group size: " + value;
            case EVENT_TYPE_GROUP_LOCK_CHANGED:
                return "status:" + csipLockStatusToString(value);
            default:
                return "<unused>";
        }
    }

    private static String csipLockStatusToString(int state) {
        switch (state) {
            case LOCK_STATUS_SUCCESS:
                return "LOCK_STATUS_SUCCESS";
            case LOCK_STATUS_FAILED_INVALID_GROUP:
                return "LOCK_STATUS_FAILED_INVALID_GROUP";
            case LOCK_STATUS_FAILED_GROUP_EMPTY:
                return "LOCK_STATUS_FAILED_GROUP_EMPTY";
            case LOCK_STATUS_FAILED_GROUP_NOT_CONNECTED:
                return "LOCK_STATUS_FAILED_GROUP_NOT_CONNECTED";
            case LOCK_STATUS_FAILED_GROUP_LOCKED_BY_OTHERS:
                return "LOCK_STATUS_FAILED_GROUP_LOCKED_BY_OTHERS";
            case LOCK_STATUS_FAILED_OTHER_REASON:
                return "LOCK_STATUS_FAILED_OTHER_REASON";
            case LOCK_STATUS_GROUP_MEMBER_LOST:
                return "LOCK_STATUS_GROUP_MEMBER_LOST";
            default:
                return "UNKNOWN";
        }
    }
}
