/*
 * Copyright 2019 The Android Open Source Project
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

/*
 * Defines utility inteface that is used by state machine/service to either send vendor specific AT
 * command or receive vendor specific response from the native stack.
 */
package com.android.bluetooth.hfpclient;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import android.bluetooth.BluetoothAssignedNumbers;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadsetClient;
import android.content.Intent;
import android.util.Log;

import com.android.bluetooth.Utils;

import java.util.HashMap;
import java.util.Map;

class VendorCommandResponseProcessor {

    private static final String TAG = "VendorCommandResponseProcessor";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

    private final HeadsetClientService mService;
    private final NativeInterface mNativeInterface;

    // Keys are AT commands (without payload), and values are the company IDs.
    private static final Map<String, Integer> SUPPORTED_VENDOR_AT_COMMANDS;
    static {
        SUPPORTED_VENDOR_AT_COMMANDS = new HashMap<>();
        SUPPORTED_VENDOR_AT_COMMANDS.put(
                "+XAPL=",
                BluetoothAssignedNumbers.APPLE);
        SUPPORTED_VENDOR_AT_COMMANDS.put(
                "+IPHONEACCEV=",
                BluetoothAssignedNumbers.APPLE);
        SUPPORTED_VENDOR_AT_COMMANDS.put(
                "+APLSIRI?",
                BluetoothAssignedNumbers.APPLE);
        SUPPORTED_VENDOR_AT_COMMANDS.put(
                "+APLEFM",
                BluetoothAssignedNumbers.APPLE);
    }

    // Keys are AT events (without payload), and values are the company IDs.
    private static final Map<String, Integer> SUPPORTED_VENDOR_EVENTS;
    static {
        SUPPORTED_VENDOR_EVENTS = new HashMap<>();
        SUPPORTED_VENDOR_EVENTS.put(
                "+APLSIRI:",
                BluetoothAssignedNumbers.APPLE);
        SUPPORTED_VENDOR_EVENTS.put(
                "+XAPL=",
                BluetoothAssignedNumbers.APPLE);
    }

    VendorCommandResponseProcessor(HeadsetClientService context, NativeInterface nativeInterface) {
        mService = context;
        mNativeInterface = nativeInterface;
    }

    public boolean sendCommand(int vendorId, String atCommand, BluetoothDevice device) {
        if (device == null) {
            Log.w(TAG, "processVendorCommand device is null");
            return false;
        }

        // Do not support more than one command at one line.
        // We simplify and say no ; allowed as well.
        int indexOfSemicolon = atCommand.indexOf(';');
        if (indexOfSemicolon > 0) {
            Log.e(TAG, "Do not support ; and more than one command:"
                    + atCommand);
            return false;
        }

        // Get command word
        int indexOfEqual = atCommand.indexOf('=');
        int indexOfQuestionMark = atCommand.indexOf('?');
        String commandWord;
        if (indexOfEqual > 0) {
            commandWord = atCommand.substring(0, indexOfEqual + 1);
        } else if (indexOfQuestionMark > 0) {
            commandWord = atCommand.substring(0, indexOfQuestionMark + 1);
        } else {
            commandWord = atCommand;
        }

        // replace all white spaces
        commandWord = commandWord.replaceAll("\\s+", "");

        if (SUPPORTED_VENDOR_AT_COMMANDS.get(commandWord) != (Integer) (vendorId)) {
            Log.e(TAG, "Invalid command " + atCommand + ", " + vendorId + ". Cand="
                    + commandWord);
            return false;
        }

        if (!mNativeInterface.sendATCmd(Utils.getBytesFromAddress(device.getAddress()),
                                        HeadsetClientHalConstants
                                        .HANDSFREECLIENT_AT_CMD_VENDOR_SPECIFIC_CMD,
                                        0, 0, atCommand)) {
            Log.e(TAG, "Failed to send vendor specific at command");
            return false;
        }
        logD("Send vendor command: " + atCommand);
        return true;
    }

    public boolean processEvent(String atString, BluetoothDevice device) {
        if (device == null) {
            Log.w(TAG, "processVendorEvent device is null");
            return false;
        }

        // Get event code
        int indexOfEqual = atString.indexOf('=');
        int indexOfColon = atString.indexOf(':');
        String eventCode;
        if (indexOfEqual > 0) {
            eventCode = atString.substring(0, indexOfEqual + 1);
        } else if (indexOfColon > 0) {
            eventCode = atString.substring(0, indexOfColon + 1);
        } else {
            eventCode = atString;
        }

        // replace all white spaces
        eventCode = eventCode.replaceAll("\\s+", "");

        Integer vendorId = SUPPORTED_VENDOR_EVENTS.get(eventCode);
        if (vendorId == null) {
            Log.e(TAG, "Invalid response: " + atString + ". " + eventCode);
            return false;
        }
        broadcastVendorSpecificEventIntent(vendorId, eventCode, atString, device);
        logD("process vendor event " + vendorId + ", " + eventCode + ", "
                + atString + " for device" + device);
        return true;
    }

    private void broadcastVendorSpecificEventIntent(int vendorId, String vendorEventCode,
            String vendorResponse, BluetoothDevice device) {
        logD("broadcastVendorSpecificEventIntent(" + vendorResponse + ")");
        Intent intent = new Intent(BluetoothHeadsetClient
                                   .ACTION_VENDOR_SPECIFIC_HEADSETCLIENT_EVENT);
        intent.putExtra(BluetoothHeadsetClient.EXTRA_VENDOR_ID, vendorId);
        intent.putExtra(BluetoothHeadsetClient.EXTRA_VENDOR_EVENT_CODE, vendorEventCode);
        intent.putExtra(BluetoothHeadsetClient.EXTRA_VENDOR_EVENT_FULL_ARGS, vendorResponse);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        mService.sendBroadcast(intent, BLUETOOTH_CONNECT, Utils.getTempAllowlistBroadcastOptions());
    }

    private void logD(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }
}
