/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

package com.android.bluetooth.directionfinder;

import android.bluetooth.BluetoothDevice;

/**
 * Stack event sent via a callback from JNI to Java, or generated
 * internally by the ATP State Machine.
 */
public class AtpStackEvent {
    // Event types for STACK_EVENT message (coming from native)
    private static final int EVENT_TYPE_NONE = 0;
    public static final int EVENT_TYPE_CONNECTION_STATE_CHANGED = 1;
    public static final int EVENT_TYPE_ENABLE_BLE_DIRECTION_FINDING = 2;
    public static final int EVENT_TYPE_LE_AOA_RESULTS = 3;

    static final int CONNECTION_STATE_DISCONNECTED = 0;
    static final int CONNECTION_STATE_CONNECTING = 1;
    static final int CONNECTION_STATE_CONNECTED = 2;
    static final int CONNECTION_STATE_DISCONNECTING = 3;

    public int type;
    public BluetoothDevice device;
    public int valueInt1;
    public int valueInt2;
    public int valueInt3;
    public double valueDouble1;
    public double valueDouble2;

    AtpStackEvent(int type) {
        this.type = type;
    }

    @Override
    public String toString() {
        // event dump
        StringBuilder result = new StringBuilder();
        result.append("AtpStackEvent {type:" + eventTypeToString(type));
        result.append(", device:" + device);
        result.append(", value1:" + valueInt1);
        result.append(", value2:" + valueInt2);
        result.append("}");
        return result.toString();
    }

    private static String eventTypeToString(int type) {
        switch (type) {
            case EVENT_TYPE_NONE:
                return "EVENT_TYPE_NONE";
            case EVENT_TYPE_CONNECTION_STATE_CHANGED:
                return "EVENT_TYPE_CONNECTION_STATE_CHANGED";
            case EVENT_TYPE_ENABLE_BLE_DIRECTION_FINDING:
                return "EVENT_TYPE_ENABLE_BLE_DIRECTION_FINDING";
            case EVENT_TYPE_LE_AOA_RESULTS:
                return "EVENT_TYPE_LE_AOA_RESULTS";
            default:
                return "EVENT_TYPE_UNKNOWN:" + type;
        }
    }
}

