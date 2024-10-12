/*
 * Copyright (C) 2012 The Android Open Source Project
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
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 */

package com.android.bluetooth.btservice;

import android.bluetooth.BluetoothAdapter;
import android.content.ComponentName;
import android.content.pm.PackageManager;
import android.os.Message;
import android.os.SystemProperties;
import android.util.Log;

import com.android.internal.util.State;
import com.android.internal.util.StateMachine;
import com.android.bluetooth.R;
import com.android.bluetooth.telephony.BluetoothInCallService;

/**
 * This state machine handles Bluetooth Adapter State.
 * Stable States:
 *      {@link OffState}: Initial State
 *      {@link BleOnState} : Bluetooth Low Energy, Including GATT, is on
 *      {@link OnState} : Bluetooth is on (All supported profiles)
 *
 * Transition States:
 *      {@link TurningBleOnState} : OffState to BleOnState
 *      {@link TurningBleOffState} : BleOnState to OffState
 *      {@link TurningOnState} : BleOnState to OnState
 *      {@link TurningOffState} : OnState to BleOnState
 *
 *        +------   Off  <-----+
 *        |                    |
 *        v                    |
 * TurningBleOn   TO--->   TurningBleOff
 *        |                  ^ ^
 *        |                  | |
 *        +----->        ----+ |
 *                 BleOn       |
 *        +------        <---+ O
 *        v                  | T
 *    TurningOn  TO---->  TurningOff
 *        |                    ^
 *        |                    |
 *        +----->   On   ------+
 *
 */

final class AdapterState extends StateMachine {
    private static final boolean DBG = true;
    private static final String TAG = AdapterState.class.getSimpleName();

    static final int USER_TURN_ON = 1;
    static final int USER_TURN_OFF = 2;
    static final int BLE_TURN_ON = 3;
    static final int BLE_TURN_OFF = 4;
    static final int BREDR_STARTED = 5;
    static final int BREDR_STOPPED = 6;
    static final int BLE_STARTED = 7;
    static final int BLE_STOPPED = 8;
    static final int BREDR_START_TIMEOUT = 9;
    static final int BREDR_STOP_TIMEOUT = 10;
    static final int BLE_STOP_TIMEOUT = 11;
    static final int BLE_START_TIMEOUT = 12;
    static final int BEGIN_BREDR_STOP = 13;
    static final int STACK_DISABLED = 14;
    static final int STACK_DISABLE_TIMEOUT = 15;
    static final int BREDR_CLEANUP_TIMEOUT = 16;
    static final int BT_FORCEKILL_TIMEOUT = 17;

    // TODO: To be optimized : Increased BLE_START_TIMEOUT_DELAY to 6 sec
    // as OMR1 Total timeout value was 14 seconds
    static final int BLE_START_TIMEOUT_DELAY = 6000;
    // Increased BLE_START_TIMEOUT_DELAY to 15 sec for XMEM patch download.
    static final int BLE_START_DEFAULT_XMEM_TIMEOUT_DELAY = 15000;
    /* Increased STARTUP time to 23 sec for XMEM patch with download configuration
     * set to have rsp for every tlv download cmd.
     */
    static final int BLE_START_XMEM_TIMEOUT_DELAY = 23000;

    static final int BLE_STOP_TIMEOUT_DELAY = 1000;
    static final int BREDR_START_TIMEOUT_DELAY = 8000;
    static final int BREDR_STOP_TIMEOUT_DELAY = 8000;
    static final int BREDR_CLEANUP_TIMEOUT_DELAY = 2000;
    static final int STACK_DISABLE_TIMEOUT_DELAY = 8000;
    static final int BT_FORCEKILL_TIMEOUT_DELAY = 100;

    static final ComponentName BLUETOOTH_INCALLSERVICE_COMPONENT
            = new ComponentName("com.android.bluetooth",
            BluetoothInCallService.class.getCanonicalName());

    private AdapterService mAdapterService;
    private TurningOnState mTurningOnState = new TurningOnState();
    private TurningBleOnState mTurningBleOnState = new TurningBleOnState();
    private TurningOffState mTurningOffState = new TurningOffState();
    private TurningBleOffState mTurningBleOffState = new TurningBleOffState();
    private OnState mOnState = new OnState();
    private OffState mOffState = new OffState();
    private BleOnState mBleOnState = new BleOnState();

    private int mPrevState = BluetoothAdapter.STATE_OFF;

    private AdapterState(AdapterService service) {
        super(TAG);
        addState(mOnState);
        addState(mBleOnState);
        addState(mOffState);
        addState(mTurningOnState);
        addState(mTurningOffState);
        addState(mTurningBleOnState);
        addState(mTurningBleOffState);
        mAdapterService = service;
        setInitialState(mOffState);
    }

    private String messageString(int message) {
        switch (message) {
            case BLE_TURN_ON: return "BLE_TURN_ON";
            case USER_TURN_ON: return "USER_TURN_ON";
            case BREDR_STARTED: return "BREDR_STARTED";
            case BLE_STARTED: return "BLE_STARTED";
            case USER_TURN_OFF: return "USER_TURN_OFF";
            case BLE_TURN_OFF: return "BLE_TURN_OFF";
            case BLE_STOPPED: return "BLE_STOPPED";
            case BREDR_STOPPED: return "BREDR_STOPPED";
            case BLE_START_TIMEOUT: return "BLE_START_TIMEOUT";
            case BLE_STOP_TIMEOUT: return "BLE_STOP_TIMEOUT";
            case BREDR_START_TIMEOUT: return "BREDR_START_TIMEOUT";
            case BREDR_STOP_TIMEOUT: return "BREDR_STOP_TIMEOUT";
            default: return "Unknown message (" + message + ")";
        }
    }

    public static AdapterState make(AdapterService service) {
        Log.d(TAG, "make() - Creating AdapterState");
        AdapterState as = new AdapterState(service);
        as.start();
        return as;
    }

    public void doQuit() {
        quitNow();
    }

    private void cleanup() {
        if (mAdapterService != null) {
            mAdapterService = null;
        }
    }

    @Override
    protected void onQuitting() {
        cleanup();
    }

    @Override
    protected String getLogRecString(Message msg) {
        return messageString(msg.what);
    }

    private abstract class BaseAdapterState extends State {

        abstract int getStateValue();

        @Override
        public void enter() {
            int currState = getStateValue();
            infoLog("entered ");
            mAdapterService.updateAdapterState(mPrevState, currState);
            mPrevState = currState;
        }

        void infoLog(String msg) {
            if (DBG) {
                Log.i(TAG, BluetoothAdapter.nameForState(getStateValue()) + " : " + msg);
            }
        }

        void errorLog(String msg) {
            Log.e(TAG, BluetoothAdapter.nameForState(getStateValue()) + " : " + msg);
        }
    }

    private class OffState extends BaseAdapterState {

        @Override
        int getStateValue() {
            return BluetoothAdapter.STATE_OFF;
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case BLE_TURN_ON:
                    transitionTo(mTurningBleOnState);
                    break;

                case BT_FORCEKILL_TIMEOUT:
                    errorLog("Killing the process to force a restart as part of cleanup");
                    android.os.Process.killProcess(android.os.Process.myPid());
                    break;

                default:
                    infoLog("Unhandled message - " + messageString(msg.what));
                    return false;
            }
            return true;
        }
    }

    private class BleOnState extends BaseAdapterState {

        @Override
        int getStateValue() {
            return BluetoothAdapter.STATE_BLE_ON;
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case USER_TURN_ON:
                    mAdapterService.startBrEdrStartup();
                    transitionTo(mTurningOnState);
                    break;

                case BLE_TURN_OFF:
                    transitionTo(mTurningBleOffState);
                    break;

                case BT_FORCEKILL_TIMEOUT:
                    transitionTo(mOffState);
                    errorLog("Killing the process to force a restart as part of cleanup");
                    android.os.Process.killProcess(android.os.Process.myPid());
                    break;

                default:
                    infoLog("Unhandled message - " + messageString(msg.what));
                    return false;
            }
            return true;
        }
    }

    private class OnState extends BaseAdapterState {

        @Override
        int getStateValue() {
            return BluetoothAdapter.STATE_ON;
        }

        @Override
        public void enter() {
            super.enter();
            mAdapterService.getPackageManager().setComponentEnabledSetting(
                    BLUETOOTH_INCALLSERVICE_COMPONENT,
                    PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                    PackageManager.DONT_KILL_APP);
        }

        @Override
        public void exit() {
            mAdapterService.getPackageManager().setComponentEnabledSetting(
                    BLUETOOTH_INCALLSERVICE_COMPONENT,
                    PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                    PackageManager.DONT_KILL_APP);
            super.exit();
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case USER_TURN_OFF:
                    transitionTo(mTurningOffState);
                    break;

                case BT_FORCEKILL_TIMEOUT:
                    transitionTo(mOffState);
                    errorLog("Killing the process to force a restart as part of cleanup");
                    android.os.Process.killProcess(android.os.Process.myPid());
                    break;

                default:
                    infoLog("Unhandled message - " + messageString(msg.what));
                    return false;
            }
            return true;
        }
    }

    private class TurningBleOnState extends BaseAdapterState {

        @Override
        int getStateValue() {
            return BluetoothAdapter.STATE_BLE_TURNING_ON;
        }

        @Override
        public void enter() {
            super.enter();
            int timeout;
            String value = SystemProperties.get("persist.vendor.bluetooth.enable_XMEM", "0");
            if (value.equals("1")) {
                infoLog("XMEM enabled: " + value);
                timeout = BLE_START_DEFAULT_XMEM_TIMEOUT_DELAY;
            } else if (value.equals("2")) {
                infoLog("XMEM enabled: " + value);
                timeout = BLE_START_XMEM_TIMEOUT_DELAY;
            } else {
                timeout = BLE_START_TIMEOUT_DELAY;
            }
            sendMessageDelayed(BLE_START_TIMEOUT, timeout);
            mAdapterService.bringUpBle();
        }

        @Override
        public void exit() {
            removeMessages(BLE_START_TIMEOUT);
            super.exit();
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case BLE_STARTED:
                    transitionTo(mBleOnState);
                    break;

                case BLE_START_TIMEOUT:
                    errorLog(messageString(msg.what));
                    mAdapterService.informTimeoutToHidl();
                    mAdapterService.disableProfileServices(true);
                    mAdapterService.StartHCIClose();
                    errorLog("BLE_START_TIMEOUT is going to kill the process as part of cleanup");
                    sendMessageDelayed(BT_FORCEKILL_TIMEOUT, BT_FORCEKILL_TIMEOUT_DELAY);
                    break;

                case BT_FORCEKILL_TIMEOUT:
                    transitionTo(mOffState);
                    errorLog("Killing the process to force a restart as part of cleanup");
                    android.os.Process.killProcess(android.os.Process.myPid());
                    break;

                default:
                    infoLog("Unhandled message - " + messageString(msg.what));
                    return false;
            }
            return true;
        }
    }

    private class TurningOnState extends BaseAdapterState {

        @Override
        int getStateValue() {
            return BluetoothAdapter.STATE_TURNING_ON;
        }

        @Override
        public void enter() {
            super.enter();
            sendMessageDelayed(BREDR_START_TIMEOUT, BREDR_START_TIMEOUT_DELAY);
            mAdapterService.startProfileServices();
        }

        @Override
        public void exit() {
            removeMessages(BREDR_START_TIMEOUT);
            super.exit();
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case BREDR_STARTED:
                    transitionTo(mOnState);
                    break;

                case BREDR_START_TIMEOUT:
                    errorLog(messageString(msg.what));
                    mAdapterService.informTimeoutToHidl();
                    mAdapterService.disableProfileServices(false);
                    mAdapterService.StartHCIClose();
                    errorLog("BREDR_START_TIMEOUT is going to kill the process as part of cleanup");
                    sendMessageDelayed(BT_FORCEKILL_TIMEOUT, BT_FORCEKILL_TIMEOUT_DELAY);
                    break;

                case BT_FORCEKILL_TIMEOUT:
                    transitionTo(mOffState);
                    errorLog("Killing the process to force a restart as part of cleanup");
                    android.os.Process.killProcess(android.os.Process.myPid());
                    break;

                default:
                    infoLog("Unhandled message - " + messageString(msg.what));
                    return false;
            }
            return true;
        }
    }

    private class TurningOffState extends BaseAdapterState {

        @Override
        int getStateValue() {
            return BluetoothAdapter.STATE_TURNING_OFF;
        }

        @Override
        public void enter() {
            super.enter();
            Log.w(TAG,"Calling startBrEdrCleanup");
            sendMessageDelayed(BREDR_CLEANUP_TIMEOUT, BREDR_CLEANUP_TIMEOUT_DELAY);
            mAdapterService.startBrEdrCleanup();
        }

        @Override
        public void exit() {
            removeMessages(BREDR_STOP_TIMEOUT);
            super.exit();
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case BREDR_STOPPED:
                    transitionTo(mBleOnState);
                    break;

                case BREDR_STOP_TIMEOUT:
                    errorLog(messageString(msg.what));
                    mAdapterService.informTimeoutToHidl();
                    mAdapterService.disableProfileServices(false);
                    mAdapterService.StartHCIClose();
                    errorLog("BREDR_STOP_TIMEOUT is going to kill the process as part of cleanup");
                    sendMessageDelayed(BT_FORCEKILL_TIMEOUT, BT_FORCEKILL_TIMEOUT_DELAY);
                    break;

                case BT_FORCEKILL_TIMEOUT:
                    transitionTo(mOffState);
                    errorLog("Killing the process to force a restart as part of cleanup");
                    android.os.Process.killProcess(android.os.Process.myPid());
                    break;

                case BREDR_CLEANUP_TIMEOUT:
                    errorLog("Error cleaningup Bluetooth profiles (cleanup timeout)");
                    mAdapterService.informTimeoutToHidl();
                    mAdapterService.disableProfileServices(false);
                    mAdapterService.StartHCIClose();
                    errorLog("BREDR_CLEANUP_TIMEOUT going to kill the process as part of cleanup");
                    sendMessageDelayed(BT_FORCEKILL_TIMEOUT, BT_FORCEKILL_TIMEOUT_DELAY);
                    break;

                case BEGIN_BREDR_STOP:
                    removeMessages(BREDR_CLEANUP_TIMEOUT);
                    sendMessageDelayed(BREDR_STOP_TIMEOUT, BREDR_STOP_TIMEOUT_DELAY);
                    mAdapterService.stopProfileServices();
                    break;

                default:
                    infoLog("Unhandled message - " + messageString(msg.what));
                    return false;
            }
            return true;
        }
    }

    private class TurningBleOffState extends BaseAdapterState {

        @Override
        int getStateValue() {
            return BluetoothAdapter.STATE_BLE_TURNING_OFF;
        }

        @Override
        public void enter() {
            super.enter();
            mAdapterService.unregGattIds();
            sendMessageDelayed(STACK_DISABLE_TIMEOUT, STACK_DISABLE_TIMEOUT_DELAY);
            boolean ret = mAdapterService.disableNative();
            if (!ret) {
               removeMessages(STACK_DISABLE_TIMEOUT);
               errorLog("Error while calling disableNative");
               transitionTo(mBleOnState);
            }
        }

        @Override
        public void exit() {
            removeMessages(BLE_STOP_TIMEOUT);
            super.exit();
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case BLE_STOPPED:
                    transitionTo(mOffState);
                    break;

                case BLE_STOP_TIMEOUT:
                    errorLog("Error stopping Bluetooth profiles (BLE stop timeout)");
                    errorLog(messageString(msg.what));
                    transitionTo(mOffState);
                    break;

                case STACK_DISABLED:
                    removeMessages(STACK_DISABLE_TIMEOUT);
                    sendMessageDelayed(BLE_STOP_TIMEOUT, BLE_STOP_TIMEOUT_DELAY);
                    mAdapterService.bringDownBle();
                    break;

                case STACK_DISABLE_TIMEOUT:
                    mAdapterService.informTimeoutToHidl();
                    mAdapterService.disableProfileServices(true);
                    mAdapterService.StartHCIClose();
                    errorLog("STACK_DISABLE_TIMEOUT going to kill the process as part of cleanup");
                    sendMessageDelayed(BT_FORCEKILL_TIMEOUT, BT_FORCEKILL_TIMEOUT_DELAY);
                    break;

                case BT_FORCEKILL_TIMEOUT:
                    transitionTo(mOffState);
                    errorLog("Killing the process to force a restart as part of cleanup");
                    android.os.Process.killProcess(android.os.Process.myPid());
                    break;

                default:
                    infoLog("Unhandled message - " + messageString(msg.what));
                    return false;
            }
            return true;
        }
    }
}
