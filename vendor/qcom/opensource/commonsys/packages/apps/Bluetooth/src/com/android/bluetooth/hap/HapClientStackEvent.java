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

package com.android.bluetooth.hap;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.IBluetoothHapClient;

import java.math.BigInteger;
import java.util.ArrayList;
import java.util.List;

/**
 * Stack event sent via a callback from GATT Service client to main service module, or generated
 * internally by the Hearing Access Profile State Machine.
 */
public class HapClientStackEvent {
    // Event types for STACK_EVENT message (coming from native HAS client)
    private static final int EVENT_TYPE_NONE = 0;
    public static final int EVENT_TYPE_CONNECTION_STATE_CHANGED = 1;
    public static final int EVENT_TYPE_DEVICE_AVAILABLE = 2;
    public static final int EVENT_TYPE_DEVICE_FEATURES = 3;
    public static final int EVENT_TYPE_ON_ACTIVE_PRESET_SELECTED = 4;
    public static final int EVENT_TYPE_ON_ACTIVE_PRESET_SELECT_ERROR = 5;
    public static final int EVENT_TYPE_ON_PRESET_INFO = 6;
    public static final int EVENT_TYPE_ON_PRESET_NAME_SET_ERROR = 7;
    public static final int EVENT_TYPE_ON_PRESET_INFO_ERROR = 8;

    // Connection state values as defined in bt_has.h
    static final int CONNECTION_STATE_DISCONNECTED = 0;
    static final int CONNECTION_STATE_CONNECTING = 1;
    static final int CONNECTION_STATE_CONNECTED = 2;
    static final int CONNECTION_STATE_DISCONNECTING = 3;

    // Possible operation results
    /* WARNING: Matches status codes defined in bta_has.h */
    static final int STATUS_NO_ERROR = 0;
    static final int STATUS_SET_NAME_NOT_ALLOWED = 1;
    static final int STATUS_OPERATION_NOT_SUPPORTED = 2;
    static final int STATUS_OPERATION_NOT_POSSIBLE = 3;
    static final int STATUS_INVALID_PRESET_NAME_LENGTH = 4;
    static final int STATUS_INVALID_PRESET_INDEX = 5;
    static final int STATUS_GROUP_OPERATION_NOT_SUPPORTED = 6;
    static final int STATUS_PROCEDURE_ALREADY_IN_PROGRESS = 7;

    // Supported features
    public static final int FEATURE_BIT_NUM_TYPE_MONAURAL =
            IBluetoothHapClient.FEATURE_BIT_NUM_TYPE_MONAURAL;
    public static final int FEATURE_BIT_NUM_TYPE_BANDED =
            IBluetoothHapClient.FEATURE_BIT_NUM_TYPE_BANDED;
    public static final int FEATURE_BIT_NUM_SYNCHRONIZATED_PRESETS =
            IBluetoothHapClient.FEATURE_BIT_NUM_SYNCHRONIZATED_PRESETS;
    public static final int FEATURE_BIT_NUM_INDEPENDENT_PRESETS =
            IBluetoothHapClient.FEATURE_BIT_NUM_INDEPENDENT_PRESETS;
    public static final int FEATURE_BIT_NUM_DYNAMIC_PRESETS =
            IBluetoothHapClient.FEATURE_BIT_NUM_DYNAMIC_PRESETS;
    public static final int FEATURE_BIT_NUM_WRITABLE_PRESETS =
            IBluetoothHapClient.FEATURE_BIT_NUM_WRITABLE_PRESETS;

    // Preset Info notification reason
    /* WARNING: Matches status codes defined in bta_has.h */
    public static final int PRESET_INFO_REASON_ALL_PRESET_INFO = 0;
    public static final int PRESET_INFO_REASON_PRESET_INFO_UPDATE = 1;
    public static final int PRESET_INFO_REASON_PRESET_DELETED = 2;
    public static final int PRESET_INFO_REASON_PRESET_AVAILABILITY_CHANGED = 3;
    public static final int PRESET_INFO_REASON_PRESET_INFO_REQUEST_RESPONSE = 4;

    public int type;
    public BluetoothDevice device;
    public int valueInt1;
    public int valueInt2;
    public int valueInt3;
    public ArrayList valueList;

    public HapClientStackEvent(int type) {
        this.type = type;
    }

    @Override
    public String toString() {
        // event dump
        StringBuilder result = new StringBuilder();
        result.append("HearingAccessStackEvent {type:" + eventTypeToString(type));
        result.append(", device: " + device);
        result.append(", value1: " + eventTypeValueInt1ToString(type, valueInt1));
        result.append(", value2: " + eventTypeValueInt2ToString(type, valueInt2));
        result.append(", value3: " + eventTypeValueInt3ToString(type, valueInt3));
        result.append(", list: " + eventTypeValueListToString(type, valueList));

        result.append("}");
        return result.toString();
    }

    private String eventTypeValueListToString(int type, List value) {
        switch (type) {
            case EVENT_TYPE_ON_PRESET_INFO:
                return "{presets count: " + (value == null ? 0 : value.size()) + "}";
            default:
                return "{list: empty}";
        }
    }

    private String eventTypeValueInt1ToString(int type, int value) {
        switch (type) {
            case EVENT_TYPE_CONNECTION_STATE_CHANGED:
                return "{state: " + connectionStateValueToString(value) + "}";
            case EVENT_TYPE_DEVICE_AVAILABLE:
                return "{features: " + featuresToString(value) + "}";
            case EVENT_TYPE_DEVICE_FEATURES:
                return "{features: " + featuresToString(value) + "}";
            case EVENT_TYPE_ON_PRESET_NAME_SET_ERROR:
                return "{statusCode: " + statusCodeValueToString(value) + "}";
            case EVENT_TYPE_ON_PRESET_INFO_ERROR:
                return "{statusCode: " + statusCodeValueToString(value) + "}";
            case EVENT_TYPE_ON_ACTIVE_PRESET_SELECTED:
                return "{presetIndex: " + value + "}";
            case EVENT_TYPE_ON_ACTIVE_PRESET_SELECT_ERROR:
                return "{statusCode: " + statusCodeValueToString(value) + "}";
            case EVENT_TYPE_ON_PRESET_INFO:
            default:
                return "{unused: " + value + "}";
        }
    }

    private String infoReasonToString(int value) {
        switch (value) {
            case PRESET_INFO_REASON_ALL_PRESET_INFO:
                return "PRESET_INFO_REASON_ALL_PRESET_INFO";
            case PRESET_INFO_REASON_PRESET_INFO_UPDATE:
                return "PRESET_INFO_REASON_PRESET_INFO_UPDATE";
            case PRESET_INFO_REASON_PRESET_DELETED:
                return "PRESET_INFO_REASON_PRESET_DELETED";
            case PRESET_INFO_REASON_PRESET_AVAILABILITY_CHANGED:
                return "PRESET_INFO_REASON_PRESET_AVAILABILITY_CHANGED";
            case PRESET_INFO_REASON_PRESET_INFO_REQUEST_RESPONSE:
                return "PRESET_INFO_REASON_PRESET_INFO_REQUEST_RESPONSE";
            default:
                return "UNKNOWN";
        }
    }

    private String eventTypeValueInt2ToString(int type, int value) {
        switch (type) {
            case EVENT_TYPE_ON_PRESET_NAME_SET_ERROR:
                return "{presetIndex: " + value + "}";
            case EVENT_TYPE_ON_PRESET_INFO:
                return "{info_reason: " + infoReasonToString(value) + "}";
            case EVENT_TYPE_ON_PRESET_INFO_ERROR:
                return "{presetIndex: " + value + "}";
            case EVENT_TYPE_ON_ACTIVE_PRESET_SELECTED:
                return "{groupId: " + value + "}";
            case EVENT_TYPE_ON_ACTIVE_PRESET_SELECT_ERROR:
                return "{groupId: " + value + "}";
            default:
                return "{unused: " + value + "}";
        }
    }

    private String eventTypeValueInt3ToString(int type, int value) {
        switch (type) {
            case EVENT_TYPE_ON_PRESET_INFO:
            case EVENT_TYPE_ON_PRESET_INFO_ERROR:
            case EVENT_TYPE_ON_PRESET_NAME_SET_ERROR:
                return "{groupId: " + value + "}";
            default:
                return "{unused: " + value + "}";
        }
    }

    private String connectionStateValueToString(int value) {
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
                return "CONNECTION_STATE_UNKNOWN!";
        }
    }

    private String statusCodeValueToString(int value) {
        switch (value) {
            case STATUS_NO_ERROR:
                return "STATUS_NO_ERROR";
            case STATUS_SET_NAME_NOT_ALLOWED:
                return "STATUS_SET_NAME_NOT_ALLOWED";
            case STATUS_OPERATION_NOT_SUPPORTED:
                return "STATUS_OPERATION_NOT_SUPPORTED";
            case STATUS_OPERATION_NOT_POSSIBLE:
                return "STATUS_OPERATION_NOT_POSSIBLE";
            case STATUS_INVALID_PRESET_NAME_LENGTH:
                return "STATUS_INVALID_PRESET_NAME_LENGTH";
            case STATUS_INVALID_PRESET_INDEX:
                return "STATUS_INVALID_PRESET_INDEX";
            case STATUS_GROUP_OPERATION_NOT_SUPPORTED:
                return "STATUS_GROUP_OPERATION_NOT_SUPPORTED";
            case STATUS_PROCEDURE_ALREADY_IN_PROGRESS:
                return "STATUS_PROCEDURE_ALREADY_IN_PROGRESS";
            default:
                return "ERROR_UNKNOWN";
        }
    }

    private String featuresToString(int value) {
        String features_str = "";
        if (BigInteger.valueOf(value).testBit(FEATURE_BIT_NUM_TYPE_MONAURAL)) {
            features_str += "TYPE_MONAURAL";
        } else if (BigInteger.valueOf(value).testBit(FEATURE_BIT_NUM_TYPE_BANDED)) {
            features_str += "TYPE_BANDED";
        } else {
            features_str += "TYPE_BINAURAL";
        }

        if (BigInteger.valueOf(value).testBit(FEATURE_BIT_NUM_SYNCHRONIZATED_PRESETS)) {
            features_str += ", SYNCHRONIZATED_PRESETS";
        }
        if (BigInteger.valueOf(value).testBit(FEATURE_BIT_NUM_INDEPENDENT_PRESETS)) {
            features_str += ", INDEPENDENT_PRESETS";
        }
        if (BigInteger.valueOf(value).testBit(FEATURE_BIT_NUM_DYNAMIC_PRESETS)) {
            features_str += ", DYNAMIC_PRESETS";
        }
        if (BigInteger.valueOf(value).testBit(FEATURE_BIT_NUM_WRITABLE_PRESETS)) {
            features_str += ", WRITABLE_PRESETS";
        }

        return features_str;
    }

    private static String eventTypeToString(int type) {
        switch (type) {
            case EVENT_TYPE_NONE:
                return "EVENT_TYPE_NONE";
            case EVENT_TYPE_CONNECTION_STATE_CHANGED:
                return "EVENT_TYPE_CONNECTION_STATE_CHANGED";
            case EVENT_TYPE_DEVICE_AVAILABLE:
                return "EVENT_TYPE_DEVICE_AVAILABLE";
            case EVENT_TYPE_DEVICE_FEATURES:
                return "EVENT_TYPE_DEVICE_FEATURES";
            case EVENT_TYPE_ON_ACTIVE_PRESET_SELECTED:
                return "EVENT_TYPE_ON_ACTIVE_PRESET_SELECTED";
            case EVENT_TYPE_ON_ACTIVE_PRESET_SELECT_ERROR:
                return "EVENT_TYPE_ON_ACTIVE_PRESET_SELECT_ERROR";
            case EVENT_TYPE_ON_PRESET_INFO:
                return "EVENT_TYPE_ON_PRESET_INFO";
            case EVENT_TYPE_ON_PRESET_NAME_SET_ERROR:
                return "EVENT_TYPE_ON_PRESET_NAME_SET_ERROR";
            case EVENT_TYPE_ON_PRESET_INFO_ERROR:
                return "EVENT_TYPE_ON_PRESET_INFO_ERROR";
            default:
                return "EVENT_TYPE_UNKNOWN:" + type;
        }
    }
}
