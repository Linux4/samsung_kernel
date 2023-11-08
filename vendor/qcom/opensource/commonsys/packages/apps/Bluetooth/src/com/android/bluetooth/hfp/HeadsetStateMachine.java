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
/* Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

package com.android.bluetooth.hfp;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import android.annotation.RequiresPermission;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothAssignedNumbers;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothProtoEnums;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.hfp.BluetoothHfpProtoEnums;
import android.content.Intent;
import android.media.AudioManager;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.telephony.PhoneNumberUtils;
import android.telephony.PhoneStateListener;
import android.text.TextUtils;
import android.util.Log;
import android.os.SystemProperties;
import android.net.ConnectivityManager;
import android.net.NetworkCapabilities;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.NetworkInfo;
import android.net.Network;
import android.os.Build;
import android.os.SystemProperties;

import com.android.bluetooth.BluetoothStatsLog;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.InteropUtil;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.apm.ApmConstIntf;
import com.android.bluetooth.apm.DeviceProfileMapIntf;
import com.android.bluetooth.apm.VolumeManagerIntf;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Objects;
import java.util.Scanner;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.Iterator;
import android.telecom.TelecomManager;

/**
 * A Bluetooth Handset StateMachine
 *                        (Disconnected)
 *                           |      ^
 *                   CONNECT |      | DISCONNECTED
 *                           V      |
 *                  (Connecting)   (Disconnecting)
 *                           |      ^
 *                 CONNECTED |      | DISCONNECT
 *                           V      |
 *                          (Connected)
 *                           |      ^
 *             CONNECT_AUDIO |      | AUDIO_DISCONNECTED
 *                           V      |
 *             (AudioConnecting)   (AudioDiconnecting)
 *                           |      ^
 *           AUDIO_CONNECTED |      | DISCONNECT_AUDIO
 *                           V      |
 *                           (AudioOn)
 */
@VisibleForTesting
public class HeadsetStateMachine extends StateMachine {
    private static final String TAG = "HeadsetStateMachine";
    private static final boolean DBG = true;
    // TODO(b/122040733) variable created as a placeholder to make build green after merge conflict; re-address
    private static final String MERGE_PLACEHOLDER = "";

    private static final String HEADSET_NAME = "bt_headset_name";
    private static final String HEADSET_NREC = "bt_headset_nrec";
    private static final String HEADSET_WBS = "bt_wbs";
    private static final String HEADSET_SWB = "bt_swb";
    private static final String HEADSET_SWB_DISABLE = "65535";
    private static final String HEADSET_AUDIO_FEATURE_ON = "on";
    private static final String HEADSET_AUDIO_FEATURE_OFF = "off";

    static final int CONNECT = 1;
    static final int DISCONNECT = 2;
    static final int CONNECT_AUDIO = 3;
    static final int DISCONNECT_AUDIO = 4;
    static final int VOICE_RECOGNITION_START = 5;
    static final int VOICE_RECOGNITION_STOP = 6;
    static final int HEADSET_SWB_MAX_CODEC_IDS = 8;

    // message.obj is an intent AudioManager.VOLUME_CHANGED_ACTION
    // EXTRA_VOLUME_STREAM_TYPE is STREAM_BLUETOOTH_SCO
    static final int INTENT_SCO_VOLUME_CHANGED = 7;
    static final int INTENT_CONNECTION_ACCESS_REPLY = 8;
    static final int CALL_STATE_CHANGED = 9;
    static final int DEVICE_STATE_CHANGED = 10;
    static final int SEND_CCLC_RESPONSE = 11;
    static final int SEND_VENDOR_SPECIFIC_RESULT_CODE = 12;
    static final int SEND_BSIR = 13;
    static final int DIALING_OUT_RESULT = 14;
    static final int VOICE_RECOGNITION_RESULT = 15;

    static final int QUERY_PHONE_STATE_AT_SLC = 18;
    static final int SEND_INCOMING_CALL_IND = 19;
    static final int VOIP_CALL_STATE_CHANGED_ALERTING = 20;
    static final int VOIP_CALL_STATE_CHANGED_ACTIVE = 21;
    static final int CS_CALL_STATE_CHANGED_ALERTING = 22;
    static final int CS_CALL_STATE_CHANGED_ACTIVE = 23;
    static final int A2DP_STATE_CHANGED = 24;
    static final int RESUME_A2DP = 26;
    static final int AUDIO_SERVER_UP = 27;
    static final int SCO_RETRIAL_NOT_REQ = 28;
    static final int SEND_CLCC_RESP_AFTER_VOIP_CALL = 29;

    static final int STACK_EVENT = 101;
    private static final int CLCC_RSP_TIMEOUT = 104;
    private static final int PROCESS_CPBR = 105;

    private static final int CONNECT_TIMEOUT = 201;

    private static final int CLCC_RSP_TIMEOUT_MS = 5000;
    private static final int QUERY_PHONE_STATE_CHANGED_DELAYED = 100;
    // NOTE: the value is not "final" - it is modified in the unit tests
    @VisibleForTesting static int sConnectTimeoutMs = 30000;

    private static final HeadsetAgIndicatorEnableState DEFAULT_AG_INDICATOR_ENABLE_STATE =
            new HeadsetAgIndicatorEnableState(true, true, true, true);

    // delay call indicators and some remote devices are not able to handle
    // indicators back to back, especially in VOIP scenarios.
    /* Delay between call dialling, alerting updates for VOIP call */
    private static final int VOIP_CALL_ALERTING_DELAY_TIME_MSEC = 800;
    /* Delay between call alerting, active updates for VOIP call */
    private static final int VOIP_CALL_ACTIVE_DELAY_TIME_MSEC =
                               VOIP_CALL_ALERTING_DELAY_TIME_MSEC + 50;
    private int CS_CALL_ALERTING_DELAY_TIME_MSEC = 800;
    private int CS_CALL_ACTIVE_DELAY_TIME_MSEC = 10;
    private static final int INCOMING_CALL_IND_DELAY = 200;
    private static final int MAX_RETRY_CONNECT_COUNT = 2;
    private static final String VOIP_CALL_NUMBER = "10000000";

    //VR app launched successfully
    private static final int VR_SUCCESS = 1;

    //VR app failed to launch
    private static final int VR_FAILURE = 0;

    private final BluetoothDevice mDevice;
    private int RETRY_SCO_CONNECTION_DELAY = 0;
    private int SCO_RETRIAL_REQ_TIMEOUT = 5000;

    // maintain call states in state machine as well
    private final HeadsetCallState mStateMachineCallState =
                 new HeadsetCallState(0, 0, 0, "", 0, "");

    private NetworkCallback mDefaultNetworkCallback = new NetworkCallback() {
        @Override
        public void onAvailable(Network network) {
            mIsAvailable = true;
            Log.d(TAG, "The current Network: "+network+" is avialable: "+mIsAvailable);
        }
        @Override
        public void onLost(Network network) {
           mIsAvailable = false;
           Log.d(TAG, "The current Network:"+network+" is lost, mIsAvailable: "
                 +mIsAvailable);
        }
    };

    // State machine states
    private final Disconnected mDisconnected = new Disconnected();
    private final Connecting mConnecting = new Connecting();
    private final Disconnecting mDisconnecting = new Disconnecting();
    private final Connected mConnected = new Connected();
    private final AudioOn mAudioOn = new AudioOn();
    private final AudioConnecting mAudioConnecting = new AudioConnecting();
    private final AudioDisconnecting mAudioDisconnecting = new AudioDisconnecting();
    private HeadsetStateBase mPrevState;
    private HeadsetStateBase mCurrentState;

    // used for synchronizing mCurrentState set/get
    private final Object mLock = new Object();

    // Run time dependencies
    private final HeadsetService mHeadsetService;
    private final AdapterService mAdapterService;
    private final HeadsetNativeInterface mNativeInterface;
    private final HeadsetSystemInterface mSystemInterface;
    private ConnectivityManager mConnectivityManager;

    // Runtime states
    private int mSpeakerVolume;
    private int mMicVolume;
    private boolean mDeviceSilenced;
    private HeadsetAgIndicatorEnableState mAgIndicatorEnableState;
    private boolean mA2dpSuspend;
    private boolean mIsCsCall = true;
    private boolean mIsCallIndDelay = false;
    private boolean mIsBlacklistedDevice = false;
    private boolean mIsBlacklistedForSCOAfterSLC = false;
    private int retryConnectCount = 0;
    private boolean mIsRetrySco = false;
    private boolean mIsBlacklistedDeviceforRetrySCO = false;
    private boolean mIsSwbSupportedByRemote = false;

    private static boolean mIsAvailable = false;

    //ConcurrentLinkeQueue is used so that it is threadsafe
    private ConcurrentLinkedQueue<HeadsetCallState> mPendingCallStates =
                             new ConcurrentLinkedQueue<HeadsetCallState>();
    private ConcurrentLinkedQueue<HeadsetCallState> mDelayedCSCallStates =
                             new ConcurrentLinkedQueue<HeadsetCallState>();
    // The timestamp when the device entered connecting/connected state
    private long mConnectingTimestampMs = Long.MIN_VALUE;
    // Audio Parameters like NREC
    private final HashMap<String, String> mAudioParams = new HashMap<>();
    // AT Phone book keeps a group of states used by AT+CPBR commands
    private final AtPhonebook mPhonebook;
    // HSP specific
    private boolean mNeedDialingOutReply;

    // Hash for storing the A2DP connection states
    private HashMap<BluetoothDevice, Integer> mA2dpConnState =
                                          new HashMap<BluetoothDevice, Integer>();
    // Hash for storing the A2DP play states
    private HashMap<BluetoothDevice, Integer> mA2dpPlayState =
                                          new HashMap<BluetoothDevice, Integer>();

    // Keys are AT commands, and values are the company IDs.
    private static final Map<String, Integer> VENDOR_SPECIFIC_AT_COMMAND_COMPANY_ID;

    /* Retry outgoing connection after this time if the first attempt fails */
    private static final int RETRY_CONNECT_TIME_SEC = 2500;

    static {
        VENDOR_SPECIFIC_AT_COMMAND_COMPANY_ID = new HashMap<>();
        VENDOR_SPECIFIC_AT_COMMAND_COMPANY_ID.put(
                BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_XEVENT,
                BluetoothAssignedNumbers.PLANTRONICS);
        VENDOR_SPECIFIC_AT_COMMAND_COMPANY_ID.put(
                BluetoothHeadset.VENDOR_RESULT_CODE_COMMAND_ANDROID,
                BluetoothAssignedNumbers.GOOGLE);
        VENDOR_SPECIFIC_AT_COMMAND_COMPANY_ID.put(
                BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_XAPL,
                BluetoothAssignedNumbers.APPLE);
        VENDOR_SPECIFIC_AT_COMMAND_COMPANY_ID.put(
                BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_IPHONEACCEV,
                BluetoothAssignedNumbers.APPLE);
    }

    private HeadsetStateMachine(BluetoothDevice device, Looper looper,
            HeadsetService headsetService, AdapterService adapterService,
            HeadsetNativeInterface nativeInterface, HeadsetSystemInterface systemInterface) {
        super(TAG, Objects.requireNonNull(looper, "looper cannot be null"));
        // Enable/Disable StateMachine debug logs
        setDbg(DBG);
        mDevice = Objects.requireNonNull(device, "device cannot be null");
        mHeadsetService = Objects.requireNonNull(headsetService, "headsetService cannot be null");
        mNativeInterface =
                Objects.requireNonNull(nativeInterface, "nativeInterface cannot be null");
        mSystemInterface =
                Objects.requireNonNull(systemInterface, "systemInterface cannot be null");
        mAdapterService = Objects.requireNonNull(adapterService, "AdapterService cannot be null");
        mDeviceSilenced = false;
        // Create phonebook helper
        mPhonebook = new AtPhonebook(mHeadsetService, mNativeInterface);
        mConnectivityManager = (ConnectivityManager)
                          mHeadsetService.getSystemService(mHeadsetService.CONNECTIVITY_SERVICE);
        // Initialize state machine
        addState(mDisconnected);
        addState(mConnecting);
        addState(mDisconnecting);
        addState(mConnected);
        addState(mAudioOn);
        addState(mAudioConnecting);
        addState(mAudioDisconnecting);
        setInitialState(mDisconnected);

        if (isDeviceBlacklistedForSendingCallIndsBackToBack()) {
            CS_CALL_ALERTING_DELAY_TIME_MSEC = 0;
            CS_CALL_ACTIVE_DELAY_TIME_MSEC = 0;
            Log.w(TAG, "alerting delay " + CS_CALL_ALERTING_DELAY_TIME_MSEC +
                      " active delay " + CS_CALL_ACTIVE_DELAY_TIME_MSEC);
        }


        mConnectivityManager.registerDefaultNetworkCallback(mDefaultNetworkCallback);
        Log.i(TAG," Exiting HeadsetStateMachine constructor for device :" + device);
    }

    static HeadsetStateMachine make(BluetoothDevice device, Looper looper,
            HeadsetService headsetService, AdapterService adapterService,
            HeadsetNativeInterface nativeInterface, HeadsetSystemInterface systemInterface) {
        HeadsetStateMachine stateMachine =
                new HeadsetStateMachine(device, looper, headsetService, adapterService,
                        nativeInterface, systemInterface);
        Log.i(TAG," Starting StateMachine  device: " + device);
        stateMachine.start();
        Log.i(TAG, "Created state machine " + stateMachine + " for " + device);
        return stateMachine;
    }

    static void destroy(HeadsetStateMachine stateMachine) {
        Log.i(TAG, "destroy");
        if (stateMachine == null) {
            Log.w(TAG, "destroy(), stateMachine is null");
            return;
        }
        stateMachine.cleanup();
        stateMachine.quitNow();
    }

    public void cleanup() {
        Log.i(TAG," destroy, current state " + getCurrentHeadsetStateMachineState());
        if (getCurrentHeadsetStateMachineState() == mAudioOn) {
            mAudioOn.broadcastAudioState(mDevice, BluetoothHeadset.STATE_AUDIO_CONNECTED,
                                BluetoothHeadset.STATE_AUDIO_DISCONNECTED);
            mAudioOn.broadcastConnectionState(mDevice, BluetoothProfile.STATE_CONNECTED,
                                BluetoothProfile.STATE_DISCONNECTED);
        }
        if(getCurrentHeadsetStateMachineState() == mConnected){
            mConnected.broadcastConnectionState(mDevice, BluetoothProfile.STATE_CONNECTED,
                                     BluetoothProfile.STATE_DISCONNECTED);
        }
        if(getCurrentHeadsetStateMachineState() == mConnecting){
            mConnecting.broadcastConnectionState(mDevice, BluetoothProfile.STATE_CONNECTING,
                                     BluetoothProfile.STATE_DISCONNECTED);
        }
        if (mPhonebook != null) {
            mPhonebook.cleanup();
        }
        if (getAudioState() == BluetoothHeadset.STATE_AUDIO_CONNECTED &&
                !mSystemInterface.getHeadsetPhoneState().getIsCsCall()) {
            sendVoipConnectivityNetworktype(false);
        }
        mConnectivityManager.unregisterNetworkCallback(mDefaultNetworkCallback);
        mAudioParams.clear();
    }

    public void dump(StringBuilder sb) {
        ProfileService.println(sb, "  mCurrentDevice: " + mDevice);
        ProfileService.println(sb, "  mCurrentState: " + mCurrentState);
        ProfileService.println(sb, "  mPrevState: " + mPrevState);
        ProfileService.println(sb, "  mConnectionState: " + getConnectionState());
        ProfileService.println(sb, "  mAudioState: " + getAudioState());
        ProfileService.println(sb, "  mNeedDialingOutReply: " + mNeedDialingOutReply);
        ProfileService.println(sb, "  mSpeakerVolume: " + mSpeakerVolume);
        ProfileService.println(sb, "  mMicVolume: " + mMicVolume);
        ProfileService.println(sb,
                "  mConnectingTimestampMs(uptimeMillis): " + mConnectingTimestampMs);
        ProfileService.println(sb, "  StateMachine: " + this);
        // Dump the state machine logs
        StringWriter stringWriter = new StringWriter();
        PrintWriter printWriter = new PrintWriter(stringWriter);
        super.dump(new FileDescriptor(), printWriter, new String[]{});
        printWriter.flush();
        stringWriter.flush();
        ProfileService.println(sb, "  StateMachineLog:");
        Scanner scanner = new Scanner(stringWriter.toString());
        while (scanner.hasNextLine()) {
            String line = scanner.nextLine();
            ProfileService.println(sb, "    " + line);
        }
        scanner.close();
    }

    public boolean getIfDeviceBlacklistedForSCOAfterSLC() {
        Log.d(TAG, "getIfDeviceBlacklistedForSCOAfterSLC, returning " + mIsBlacklistedForSCOAfterSLC);
        return  mIsBlacklistedForSCOAfterSLC;
    }

    public boolean hasMessagesInQueue(int what) {
        return super.hasMessages(what);
    }

    public boolean hasDeferredMessagesInQueue(int what) {
        return super.hasDeferredMessages(what);
    }

    /**
     * Base class for states used in this state machine to share common infrastructures
     */
    private abstract class HeadsetStateBase extends State {
        @Override
        public void enter() {
            // Crash if mPrevState is null and state is not Disconnected
            if (!(this instanceof Disconnected) && mPrevState == null) {
                throw new IllegalStateException("mPrevState is null on enter()");
            }
            enforceValidConnectionStateTransition();

            synchronized(mLock) {
                mCurrentState = this;
                Log.e(TAG, "Setting mCurrentState as " + mCurrentState);
            }
        }

        @Override
        public void exit() {
            mPrevState = this;
        }

        @Override
        public String toString() {
            return getName();
        }

        /**
         * Broadcast audio and connection state changes to the system. This should be called at the
         * end of enter() method after all the setup is done
         */
        void broadcastStateTransitions() {
            if (mPrevState == null) {
                return;
            }
            // TODO: Add STATE_AUDIO_DISCONNECTING constant to get rid of the 2nd part of this logic
            if (getAudioStateInt() != mPrevState.getAudioStateInt() || (
                    mPrevState instanceof AudioDisconnecting && this instanceof AudioOn)) {
                stateLogD("audio state changed: " + mDevice + ": " + mPrevState + " -> " + this);
                broadcastAudioState(mDevice, mPrevState.getAudioStateInt(), getAudioStateInt());
            }
            if (getConnectionStateInt() != mPrevState.getConnectionStateInt()) {
                stateLogD(
                        "connection state changed: " + mDevice + ": " + mPrevState + " -> " + this);
                broadcastConnectionState(mDevice, mPrevState.getConnectionStateInt(),
                        getConnectionStateInt());
            }
        }

        // Should not be called from enter() method
        void broadcastConnectionState(BluetoothDevice device, int fromState, int toState) {
            stateLogD("broadcastConnectionState " + device + ": " + fromState + "->" + toState);
            if(mHeadsetService == null) {
                Log.e(TAG, "HeadsetService is null");
                return;
            }
            if(ApmConstIntf.getQtiLeAudioEnabled() || ApmConstIntf.getAospLeaEnabled()) {
                mHeadsetService.updateConnState(device, toState);
            }
            mHeadsetService.onConnectionStateChangedFromStateMachine(device, fromState, toState);
            if(!ApmConstIntf.getQtiLeAudioEnabled() &&
                    !(ApmConstIntf.getAospLeaEnabled() && mHeadsetService.isVoipLeaWarEnabled())) {
                Intent intent = new Intent(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
                intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, fromState);
                intent.putExtra(BluetoothProfile.EXTRA_STATE, toState);
                intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
                intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
                mHeadsetService.sendBroadcastAsUser(intent, UserHandle.ALL,
                    BLUETOOTH_CONNECT, Utils.getTempAllowlistBroadcastOptions());
            }
        }

        // Should not be called from enter() method
        void broadcastAudioState(BluetoothDevice device, int fromState, int toState) {
            stateLogD("broadcastAudioState: " + device + ": " + fromState + "->" + toState);
            if(mHeadsetService == null) {
                Log.e(TAG, "HeadsetService is null");
                return;
            }
            if(ApmConstIntf.getQtiLeAudioEnabled()) {
                mHeadsetService.updateAudioState(device, toState);
            }
            BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_SCO_CONNECTION_STATE_CHANGED,
                    mAdapterService.obfuscateAddress(device),
                    getConnectionStateFromAudioState(toState),
                    TextUtils.equals(mAudioParams.get(HEADSET_WBS), HEADSET_AUDIO_FEATURE_ON)
                            ? BluetoothHfpProtoEnums.SCO_CODEC_MSBC
                            : BluetoothHfpProtoEnums.SCO_CODEC_CVSD);
            mHeadsetService.onAudioStateChangedFromStateMachine(device, fromState, toState);
            if(!ApmConstIntf.getQtiLeAudioEnabled()) {
                Intent intent = new Intent(BluetoothHeadset.ACTION_AUDIO_STATE_CHANGED);
                intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, fromState);
                intent.putExtra(BluetoothProfile.EXTRA_STATE, toState);
                intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
                mHeadsetService.sendBroadcastAsUser(intent, UserHandle.ALL,
                    BLUETOOTH_CONNECT, Utils.getTempAllowlistBroadcastOptions());
            }
        }

        /**
         * Verify if the current state transition is legal. This is supposed to be called from
         * enter() method and crash if the state transition is out of the specification
         *
         * Note:
         * This method uses state objects to verify transition because these objects should be final
         * and any other instances are invalid
         */
        void enforceValidConnectionStateTransition() {
            boolean result = false;
            if (this == mDisconnected) {
                result = mPrevState == null || mPrevState == mConnecting
                        || mPrevState == mDisconnecting
                        // TODO: edges to be removed after native stack refactoring
                        // all transitions to disconnected state should go through a pending state
                        // also, states should not go directly from an active audio state to
                        // disconnected state
                        || mPrevState == mConnected || mPrevState == mAudioOn
                        || mPrevState == mAudioConnecting || mPrevState == mAudioDisconnecting;
            } else if (this == mConnecting) {
                result = mPrevState == mDisconnected;
            } else if (this == mDisconnecting) {
                result = mPrevState == mConnected
                        // TODO: edges to be removed after native stack refactoring
                        // all transitions to disconnecting state should go through connected state
                        || mPrevState == mAudioConnecting || mPrevState == mAudioOn
                        || mPrevState == mAudioDisconnecting;
            } else if (this == mConnected) {
                result = mPrevState == mConnecting || mPrevState == mAudioDisconnecting
                        || mPrevState == mDisconnecting || mPrevState == mAudioConnecting
                        // TODO: edges to be removed after native stack refactoring
                        // all transitions to connected state should go through a pending state
                        || mPrevState == mAudioOn || mPrevState == mDisconnected;
            } else if (this == mAudioConnecting) {
                result = mPrevState == mConnected;
            } else if (this == mAudioDisconnecting) {
                result = mPrevState == mAudioOn;
            } else if (this == mAudioOn) {
                result = mPrevState == mAudioConnecting || mPrevState == mAudioDisconnecting
                        // TODO: edges to be removed after native stack refactoring
                        // all transitions to audio connected state should go through a pending
                        // state
                        || mPrevState == mConnected;
            }
            if (!result) {
                throw new IllegalStateException(
                        "Invalid state transition from " + mPrevState + " to " + this
                                + " for device " + mDevice);
            }
        }

        void stateLogD(String msg) {
            log(getName() + ": currentDevice=" + mDevice + ", msg=" + msg);
        }

        void stateLogW(String msg) {
            logw(getName() + ": currentDevice=" + mDevice + ", msg=" + msg);
        }

        void stateLogE(String msg) {
            loge(getName() + ": currentDevice=" + mDevice + ", msg=" + msg);
        }

        void stateLogV(String msg) {
            logv(getName() + ": currentDevice=" + mDevice + ", msg=" + msg);
        }

        void stateLogI(String msg) {
            logi(getName() + ": currentDevice=" + mDevice + ", msg=" + msg);
        }

        void stateLogWtf(String msg) {
            Log.wtf(TAG, getName() + ": " + msg);
        }

        /**
         * Process connection event
         *
         * @param message the current message for the event
         * @param state connection state to transition to
         */
        public abstract void processConnectionEvent(Message message, int state);

        /**
         * Get a state value from {@link BluetoothProfile} that represents the connection state of
         * this headset state
         *
         * @return a value in {@link BluetoothProfile#STATE_DISCONNECTED},
         * {@link BluetoothProfile#STATE_CONNECTING}, {@link BluetoothProfile#STATE_CONNECTED}, or
         * {@link BluetoothProfile#STATE_DISCONNECTING}
         */
        abstract int getConnectionStateInt();

        /**
         * Get an audio state value from {@link BluetoothHeadset}
         * @return a value in {@link BluetoothHeadset#STATE_AUDIO_DISCONNECTED},
         * {@link BluetoothHeadset#STATE_AUDIO_CONNECTING}, or
         * {@link BluetoothHeadset#STATE_AUDIO_CONNECTED}
         */
        abstract int getAudioStateInt();

    }

    public HeadsetStateBase getCurrentHeadsetStateMachineState() {
        synchronized(mLock) {
            Log.e(TAG, "returning mCurrentState as " + mCurrentState);
            return mCurrentState;
        }
    }

    class Disconnected extends HeadsetStateBase {
        @Override
        int getConnectionStateInt() {
            return BluetoothProfile.STATE_DISCONNECTED;
        }

        @Override
        int getAudioStateInt() {
            return BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
        }

        @Override
        public void enter() {
            super.enter();
            mConnectingTimestampMs = Long.MIN_VALUE;
            mPhonebook.resetAtState();
            updateAgIndicatorEnableState(null);
            mNeedDialingOutReply = false;
            mAudioParams.clear();

            // reset call information
            mStateMachineCallState.mNumActive = 0;
            mStateMachineCallState.mNumHeld = 0;
            mStateMachineCallState.mCallState = 0;
            mStateMachineCallState.mNumber = "";
            mStateMachineCallState.mType = 0;

            // clear pending call states
            while (mDelayedCSCallStates.isEmpty() != true)
            {
               mDelayedCSCallStates.poll();
            }

            while (mPendingCallStates.isEmpty() != true)
            {
               mPendingCallStates.poll();
            }

            broadcastStateTransitions();
            DeviceProfileMapIntf dpm = DeviceProfileMapIntf.getDeviceProfileMapInstance();
            dpm.profileConnectionUpdate(mDevice, ApmConstIntf.AudioFeatures.CALL_AUDIO,
                             ApmConstIntf.AudioProfiles.HFP, false);
            dpm.profileConnectionUpdate(mDevice, ApmConstIntf.AudioFeatures.CALL_CONTROL,
                             ApmConstIntf.AudioProfiles.HFP, false);
            dpm.profileConnectionUpdate(mDevice, ApmConstIntf.AudioFeatures.CALL_VOLUME_CONTROL,
                             ApmConstIntf.AudioProfiles.HFP, false);
            VolumeManagerIntf mVolumeManager = VolumeManagerIntf.get();
            mVolumeManager.onConnStateChange(mDevice, BluetoothProfile.STATE_DISCONNECTED,
                             ApmConstIntf.AudioProfiles.HFP);
            // Remove the state machine for unbonded devices
            if (mPrevState != null
                    && mAdapterService.getBondState(mDevice) == BluetoothDevice.BOND_NONE) {
                getHandler().post(() -> mHeadsetService.removeStateMachine(mDevice));
            }
            mIsBlacklistedDevice = false;
            mIsRetrySco = false;
            mIsBlacklistedDeviceforRetrySCO = false;
            mIsSwbSupportedByRemote = false;
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CONNECT:
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    stateLogD("Connecting to " + device);
                    if (!mDevice.equals(device)) {
                        stateLogE(
                                "CONNECT failed, device=" + device + ", currentDevice=" + mDevice);
                        break;
                    }

                    stateLogD(" retryConnectCount = " + retryConnectCount);
                    if (retryConnectCount >= MAX_RETRY_CONNECT_COUNT) {
                        // max attempts reached, reset it to 0
                        retryConnectCount = 0;
                        break;
                    }
                    if (!mNativeInterface.connectHfp(device)) {
                        stateLogE("CONNECT failed for connectHfp(" + device + ")");
                        // No state transition is involved, fire broadcast immediately
                        broadcastConnectionState(device, BluetoothProfile.STATE_DISCONNECTED,
                                BluetoothProfile.STATE_DISCONNECTED);
                        break;
                    }
                    retryConnectCount++;
                    transitionTo(mConnecting);
                    break;
                case DISCONNECT:
                    // ignore
                    break;
                case CALL_STATE_CHANGED:
                    stateLogD("Ignoring CALL_STATE_CHANGED event");
                    break;
                case DEVICE_STATE_CHANGED:
                    stateLogD("Ignoring DEVICE_STATE_CHANGED event");
                    break;
                case STACK_EVENT:
                    HeadsetStackEvent event = (HeadsetStackEvent) message.obj;
                    stateLogD("STACK_EVENT: " + event);
                    if (!mDevice.equals(event.device)) {
                        stateLogE("Event device does not match currentDevice[" + mDevice
                                + "], event: " + event);
                        break;
                    }
                    switch (event.type) {
                        case HeadsetStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(message, event.valueInt);
                            break;
                        default:
                            stateLogE("Unexpected stack event: " + event);
                            break;
                    }
                    break;
                default:
                    stateLogE("Unexpected msg " + getMessageName(message.what) + ": " + message);
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        @Override
        public void processConnectionEvent(Message message, int state) {
            stateLogD("processConnectionEvent, state=" + state);
            switch (state) {
                case HeadsetHalConstants.CONNECTION_STATE_DISCONNECTED:
                    stateLogW("ignore DISCONNECTED event");
                    break;
                // Both events result in Connecting state as SLC establishment is still required
                case HeadsetHalConstants.CONNECTION_STATE_CONNECTED:
                case HeadsetHalConstants.CONNECTION_STATE_CONNECTING:
                    if (mHeadsetService.okToAcceptConnection(mDevice)) {
                        stateLogI("accept incoming connection");
                        transitionTo(mConnecting);
                    } else {
                        stateLogI("rejected incoming HF, connectionPolicy="
                                + mHeadsetService.getConnectionPolicy(mDevice) + " bondState="
                                + mAdapterService.getBondState(mDevice));
                        // Reject the connection and stay in Disconnected state itself
                        if (!mNativeInterface.disconnectHfp(mDevice)) {
                            stateLogE("failed to disconnect");
                        }
                        // Indicate rejection to other components.
                        broadcastConnectionState(mDevice, BluetoothProfile.STATE_DISCONNECTED,
                                BluetoothProfile.STATE_DISCONNECTED);
                    }
                    break;
                case HeadsetHalConstants.CONNECTION_STATE_DISCONNECTING:
                    stateLogW("Ignore DISCONNECTING event");
                    break;
                default:
                    stateLogE("Incorrect state: " + state);
                    break;
            }
        }
    }

    // Per HFP 1.7.1 spec page 23/144, Pending state needs to handle
    //      AT+BRSF, AT+CIND, AT+CMER, AT+BIND, AT+CHLD
    // commands during SLC establishment
    // AT+CHLD=? will be handled by statck directly
    class Connecting extends HeadsetStateBase {
        @Override
        int getConnectionStateInt() {
            return BluetoothProfile.STATE_CONNECTING;
        }

        @Override
        int getAudioStateInt() {
            return BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
        }

        @Override
        public void enter() {
            super.enter();
            mConnectingTimestampMs = SystemClock.uptimeMillis();
            sendMessageDelayed(CONNECT_TIMEOUT, mDevice, sConnectTimeoutMs);
            mSystemInterface.queryPhoneState();
            // update call states in StateMachine
            mStateMachineCallState.mNumActive =
                   mSystemInterface.getHeadsetPhoneState().getNumActiveCall();
            mStateMachineCallState.mNumHeld =
                   mSystemInterface.getHeadsetPhoneState().getNumHeldCall();
            mStateMachineCallState.mCallState =
                   mSystemInterface.getHeadsetPhoneState().getCallState();
            mStateMachineCallState.mNumber =
                   mSystemInterface.getHeadsetPhoneState().getNumber();
            mStateMachineCallState.mType =
                   mSystemInterface.getHeadsetPhoneState().getType();

            broadcastStateTransitions();
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CONNECT:
                case CONNECT_AUDIO:
                case DISCONNECT:
                    deferMessage(message);
                    break;
                case CONNECT_TIMEOUT: {
                    // We timed out trying to connect, transition to Disconnected state
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogE("Unknown device timeout " + device);
                        break;
                    }
                    stateLogW("CONNECT_TIMEOUT");
                    transitionTo(mDisconnected);
                    break;
                }
                case DEVICE_STATE_CHANGED:
                    stateLogD("ignoring DEVICE_STATE_CHANGED event");
                    break;
                case A2DP_STATE_CHANGED:
                    stateLogD("A2DP_STATE_CHANGED event");
                    processIntentA2dpPlayStateChanged(message.arg1);
                    break;
                case CALL_STATE_CHANGED: {
                    // update the call type here
                     updateCallType();
                     HeadsetCallState callState = (HeadsetCallState) message.obj;
                     processCallState(callState, false);
                     break;
                }
                case RESUME_A2DP: {
                     /* If the call started/ended by the time A2DP suspend ack
                      * is received, send the call indicators before resuming
                      * A2DP.
                      */
                     if (mPendingCallStates.size() == 0) {
                         stateLogD("RESUME_A2DP evt, resuming A2DP");
                         mHeadsetService.getHfpA2DPSyncInterface().releaseA2DP(mDevice);
                     } else {
                         stateLogW("RESUME_A2DP evt, pending call states to be sent, not resuming");
                     }
                     break;
                }
                case STACK_EVENT:
                    HeadsetStackEvent event = (HeadsetStackEvent) message.obj;
                    stateLogD("STACK_EVENT: " + event);
                    if (!mDevice.equals(event.device)) {
                        stateLogE("Event device does not match currentDevice[" + mDevice
                                + "], event: " + event);
                        break;
                    }
                    switch (event.type) {
                        case HeadsetStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(message, event.valueInt);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_AT_CIND:
                            processAtCind(event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_WBS:
                            processWBSEvent(event.valueInt);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_BIND:
                            processAtBind(event.valueString, event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_TWSP_BATTERY_STATE:
                            processTwsBatteryState(event.valueString,
                                                  event.device);
                            break;
                        // Unexpected AT commands, we only handle them for comparability reasons
                        case HeadsetStackEvent.EVENT_TYPE_VR_STATE_CHANGED:
                            stateLogW("Unexpected VR event, device=" + event.device + ", state="
                                    + event.valueInt);
                            processVrEvent(event.valueInt);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_DIAL_CALL:
                            stateLogW("Unexpected dial event, device=" + event.device);
                            processDialCall(event.valueString);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_SUBSCRIBER_NUMBER_REQUEST:
                            stateLogW("Unexpected subscriber number event for" + event.device
                                    + ", state=" + event.valueInt);
                            processSubscriberNumberRequest(event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_AT_COPS:
                            stateLogW("Unexpected COPS event for " + event.device);
                            processAtCops(event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_AT_CLCC:
                            stateLogW("Connecting: Unexpected CLCC event for" + event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_UNKNOWN_AT:
                            stateLogW("Unexpected unknown AT event for" + event.device + ", cmd="
                                    + event.valueString);
                            processUnknownAt(event.valueString, event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_KEY_PRESSED:
                            stateLogW("Unexpected key-press event for " + event.device);
                            processKeyPressed(event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_BIEV:
                            stateLogW("Unexpected BIEV event for " + event.device + ", indId="
                                    + event.valueInt + ", indVal=" + event.valueInt2);
                            processAtBiev(event.valueInt, event.valueInt2, event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_VOLUME_CHANGED:
                            stateLogW("Unexpected volume event for " + event.device);
                            processVolumeEvent(event.valueInt, event.valueInt2);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_ANSWER_CALL:
                            stateLogW("Unexpected answer event for " + event.device);
                            mSystemInterface.answerCall(event.device, ApmConstIntf.AudioProfiles.HFP);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_HANGUP_CALL:
                            stateLogW("Unexpected hangup event for " + event.device);
                            mSystemInterface.hangupCall(event.device, ApmConstIntf.AudioProfiles.HFP);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_SWB:
                            Log.w(TAG, "Remote supports SWB. setting mIsSwbSupportedByRemote to true");
                            mIsSwbSupportedByRemote = true;
                            processSWBEvent(event.valueInt);
                            break;
                        default:
                            stateLogE("Unexpected event: " + event);
                            break;
                    }
                    break;
                default:
                    stateLogE("Unexpected msg " + getMessageName(message.what) + ": " + message);
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        @Override
        public void processConnectionEvent(Message message, int state) {
            stateLogD("processConnectionEvent, state=" + state);
            switch (state) {
                case HeadsetHalConstants.CONNECTION_STATE_DISCONNECTED:
                    stateLogW("Disconnected");
                    processWBSEvent(HeadsetHalConstants.BTHF_WBS_NO);
                    stateLogD(" retryConnectCount = " + retryConnectCount);
                    if(retryConnectCount == 1 && !hasDeferredMessages(DISCONNECT)) {
                        Log.d(TAG,"No deferred Disconnect, retry once more ");
                        sendMessageDelayed(CONNECT, mDevice, RETRY_CONNECT_TIME_SEC);
                    } else if (retryConnectCount >= MAX_RETRY_CONNECT_COUNT ||
                            hasDeferredMessages(DISCONNECT)) {
                        // we already tried twice.
                        Log.d(TAG,"Already tried twice or has deferred Disconnect");
                        retryConnectCount = 0;
                    }
                    transitionTo(mDisconnected);
                    break;
                case HeadsetHalConstants.CONNECTION_STATE_CONNECTED:
                    stateLogD("RFCOMM connected");
                    break;
                case HeadsetHalConstants.CONNECTION_STATE_SLC_CONNECTED:
                    stateLogD("SLC connected");
                    retryConnectCount = 0;
                    transitionTo(mConnected);
                    break;
                case HeadsetHalConstants.CONNECTION_STATE_CONNECTING:
                    // Ignored
                    break;
                case HeadsetHalConstants.CONNECTION_STATE_DISCONNECTING:
                    stateLogW("Disconnecting");
                    break;
                default:
                    stateLogE("Incorrect state " + state);
                    break;
            }
        }

        @Override
        public void exit() {
            removeMessages(CONNECT_TIMEOUT);
            super.exit();
        }
    }

    class Disconnecting extends HeadsetStateBase {
        @Override
        int getConnectionStateInt() {
            return BluetoothProfile.STATE_DISCONNECTING;
        }

        @Override
        int getAudioStateInt() {
            return BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
        }

        @Override
        public void enter() {
            super.enter();
            sendMessageDelayed(CONNECT_TIMEOUT, mDevice, sConnectTimeoutMs);
            broadcastStateTransitions();
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CONNECT:
                case CONNECT_AUDIO:
                case DISCONNECT:
                    deferMessage(message);
                    break;
                case CONNECT_TIMEOUT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogE("Unknown device timeout " + device);
                        break;
                    }
                    stateLogE("timeout");
                    transitionTo(mDisconnected);
                    break;
                }
                case STACK_EVENT:
                    HeadsetStackEvent event = (HeadsetStackEvent) message.obj;
                    stateLogD("STACK_EVENT: " + event);
                    if (!mDevice.equals(event.device)) {
                        stateLogE("Event device does not match currentDevice[" + mDevice
                                + "], event: " + event);
                        break;
                    }
                    switch (event.type) {
                        case HeadsetStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(message, event.valueInt);
                            break;
                        default:
                            stateLogE("Unexpected event: " + event);
                            break;
                    }
                    break;
                case RESUME_A2DP:
                      /* If the call started/ended by the time A2DP suspend ack
                      * is received, send the call indicators before resuming
                      * A2DP.
                      */
                     if (mPendingCallStates.size() == 0) {
                         stateLogD("RESUME_A2DP evt, resuming A2DP");
                         mHeadsetService.getHfpA2DPSyncInterface().releaseA2DP(mDevice);
                     } else {
                         stateLogW("RESUME_A2DP evt, pending call states to be sent, not resuming");
                     }
                     break;
                default:
                    stateLogE("Unexpected msg " + getMessageName(message.what) + ": " + message);
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        // in Disconnecting state
        @Override
        public void processConnectionEvent(Message message, int state) {
            switch (state) {
                case HeadsetHalConstants.CONNECTION_STATE_DISCONNECTED:
                    stateLogD("processConnectionEvent: Disconnected");
                    transitionTo(mDisconnected);
                    break;
                case HeadsetHalConstants.CONNECTION_STATE_SLC_CONNECTED:
                    stateLogD("processConnectionEvent: Connected");
                    transitionTo(mConnected);
                    break;
                default:
                    stateLogE("processConnectionEvent: Bad state: " + state);
                    break;
            }
        }

        @Override
        public void exit() {
            removeMessages(CONNECT_TIMEOUT);
            super.exit();
        }
    }

    /**
     * Base class for Connected, AudioConnecting, AudioOn, AudioDisconnecting states
     */
    private abstract class ConnectedBase extends HeadsetStateBase {
        @Override
        int getConnectionStateInt() {
            return BluetoothProfile.STATE_CONNECTED;
        }

        /**
         * Handle common messages in connected states. However, state specific messages must be
         * handled individually.
         *
         * @param message Incoming message to handle
         * @return True if handled successfully, False otherwise
         */
        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CONNECT:
                case DISCONNECT:
                case CONNECT_AUDIO:
                case DISCONNECT_AUDIO:
                case CONNECT_TIMEOUT:
                    throw new IllegalStateException(
                            "Illegal message in generic handler: " + message);
                case VOICE_RECOGNITION_START: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogW("VOICE_RECOGNITION_START failed " + device
                                + " is not currentDevice");
                        break;
                    }
                    if (!mNativeInterface.startVoiceRecognition(mDevice)) {
                        stateLogW("Failed to start voice recognition");
                        break;
                    }

                    if (mHeadsetService.getHfpA2DPSyncInterface().suspendA2DP(
                          HeadsetA2dpSync.A2DP_SUSPENDED_BY_VR, mDevice) == true) {
                       Log.d(TAG, "mesg VOICE_RECOGNITION_START: A2DP is playing,"+
                             " return and establish SCO after A2DP supended");
                        break;
                    }
                    // create SCO since there is no A2DP playback
                    mNativeInterface.connectAudio(mDevice);
                    break;
                }
                case VOICE_RECOGNITION_STOP: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogW("VOICE_RECOGNITION_STOP failed " + device
                                + " is not currentDevice");
                        break;
                    }
                    if (!mNativeInterface.stopVoiceRecognition(mDevice)) {
                        stateLogW("Failed to stop voice recognition");
                        break;
                    }
                    break;
                }
                case CALL_STATE_CHANGED: {
                    // update the call type here
                    updateCallType();
                    if (mDeviceSilenced) break;

                    boolean isPts = SystemProperties.getBoolean("vendor.bt.pts.certification", false);

                    HeadsetCallState callState = (HeadsetCallState) message.obj;
                    // for PTS, send the indicators as is
                    if (isPts) {
                        if (!mNativeInterface.phoneStateChange(mDevice, callState)) {
                            stateLogW("processCallState: failed to update call state " + callState);
                            break;
                        }
                    }
                    else
                        processCallStatesDelayed(callState, false);
                    break;
                }
                case CS_CALL_STATE_CHANGED_ALERTING: {
                    // get the top of the Q
                    HeadsetCallState tempCallState = mDelayedCSCallStates.peek();
                    // top of the queue is call alerting
                    if(tempCallState != null &&
                        tempCallState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING)
                    {
                        stateLogD("alerting message timer expired, send alerting update");
                        //dequeue the alerting call state;
                        mDelayedCSCallStates.poll();
                        processCallState(tempCallState, false);
                    }

                    // top of the queue == call active
                    tempCallState = mDelayedCSCallStates.peek();
                    if (tempCallState != null &&
                         tempCallState.mCallState == HeadsetHalConstants.CALL_STATE_IDLE)
                    {
                        stateLogD("alerting message timer expired, send delayed active mesg");
                        //send delayed message for call active;
                        Message msg = obtainMessage(CS_CALL_STATE_CHANGED_ACTIVE);
                        msg.arg1 = 0;
                        sendMessageDelayed(msg, CS_CALL_ACTIVE_DELAY_TIME_MSEC);
                    }
                    break;
                }
                case CS_CALL_STATE_CHANGED_ACTIVE: {
                    // get the top of the Q
                    // top of the queue == call active
                    HeadsetCallState tempCallState = mDelayedCSCallStates.peek();
                    if (tempCallState != null &&
                         tempCallState.mCallState == HeadsetHalConstants.CALL_STATE_IDLE)
                    {
                        stateLogD("active message timer expired, send active update");
                        //dequeue the active call state;
                        mDelayedCSCallStates.poll();
                        processCallState(tempCallState, false);
                    }
                }
                    break;
                case A2DP_STATE_CHANGED:
                    stateLogD("A2DP_STATE_CHANGED event");
                    processIntentA2dpPlayStateChanged(message.arg1);
                    break;
                case DEVICE_STATE_CHANGED:
                    mNativeInterface.notifyDeviceStatus(mDevice, (HeadsetDeviceState) message.obj);
                    break;
                case SEND_CCLC_RESPONSE:
                    processSendClccResponse((HeadsetClccResponse) message.obj);
                    break;
                case CLCC_RSP_TIMEOUT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogW("CLCC_RSP_TIMEOUT failed " + device + " is not currentDevice");
                        break;
                    }
                    mNativeInterface.clccResponse(device, 0, 0, 0, 0, false, "", 0);
                }
                break;
                case SEND_VENDOR_SPECIFIC_RESULT_CODE:
                    processSendVendorSpecificResultCode(
                            (HeadsetVendorSpecificResultCode) message.obj);
                    break;
                case SEND_BSIR:
                    mNativeInterface.sendBsir(mDevice, message.arg1 == 1);
                    break;
                case VOICE_RECOGNITION_RESULT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogW("VOICE_RECOGNITION_RESULT failed " + device
                                + " is not currentDevice");
                        break;
                    }
                    mNativeInterface.atResponseCode(mDevice,
                            message.arg1 == VR_SUCCESS ? HeadsetHalConstants.AT_RESPONSE_OK
                                    : HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
                    if (message.arg1 != VR_SUCCESS) {
                       Log.d(TAG, "VOICE_RECOGNITION_RESULT: not creating SCO since VR app"+
                                  " failed to start VR");
                       break;
                    }

                    if (mHeadsetService.getHfpA2DPSyncInterface().suspendA2DP(
                          HeadsetA2dpSync.A2DP_SUSPENDED_BY_VR, mDevice) == true) {
                       Log.d(TAG, "mesg VOICE_RECOGNITION_START: A2DP is playing,"+
                             " return and establish SCO after A2DP supended");
                        break;
                    }
                    // create SCO since there is no A2DP playback
                    mNativeInterface.connectAudio(mDevice);
                    break;
                }
                case DIALING_OUT_RESULT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogW("DIALING_OUT_RESULT failed " + device + " is not currentDevice");
                        break;
                    }
                    if (mNeedDialingOutReply) {
                        mNeedDialingOutReply = false;
                        mNativeInterface.atResponseCode(mDevice,
                                message.arg1 == 1 ? HeadsetHalConstants.AT_RESPONSE_OK
                                        : HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
                    }
                }
                break;
                case INTENT_CONNECTION_ACCESS_REPLY:
                    handleAccessPermissionResult((Intent) message.obj);
                    break;
                case PROCESS_CPBR:
                    Intent intent = (Intent) message.obj;
                    processCpbr(intent);
                    break;
                case SEND_INCOMING_CALL_IND:
                    HeadsetCallState callState =
                        new HeadsetCallState(0, 0, HeadsetHalConstants.CALL_STATE_INCOMING,
                                 mSystemInterface.getHeadsetPhoneState().getNumber(),
                                 mSystemInterface.getHeadsetPhoneState().getType(),
                                 MERGE_PLACEHOLDER);
                    mNativeInterface.phoneStateChange(mDevice, callState);
                    break;
                case QUERY_PHONE_STATE_AT_SLC:
                    stateLogD("Update call states after SLC is up");
                    mSystemInterface.queryPhoneState();
                    break;
                case STACK_EVENT:
                    HeadsetStackEvent event = (HeadsetStackEvent) message.obj;
                    stateLogD("STACK_EVENT: " + event);
                    if (!mDevice.equals(event.device)) {
                        stateLogE("Event device does not match currentDevice[" + mDevice
                                + "], event: " + event);
                        break;
                    }
                    switch (event.type) {
                        case HeadsetStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(message, event.valueInt);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_AUDIO_STATE_CHANGED:
                            processAudioEvent(event.valueInt);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_VR_STATE_CHANGED:
                            processVrEvent(event.valueInt);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_ANSWER_CALL:
                            mSystemInterface.answerCall(event.device, ApmConstIntf.AudioProfiles.HFP);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_HANGUP_CALL:
                            mSystemInterface.hangupCall(event.device, ApmConstIntf.AudioProfiles.HFP);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_VOLUME_CHANGED:
                            processVolumeEvent(event.valueInt, event.valueInt2);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_DIAL_CALL:
                            processDialCall(event.valueString);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_SEND_DTMF:
                            mSystemInterface.sendDtmf(event.valueInt, event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_NOISE_REDUCTION:
                            processNoiseReductionEvent(event.valueInt == 1);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_WBS:
                            processWBSEvent(event.valueInt);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_AT_CHLD:
                            processAtChld(event.valueInt, event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_SUBSCRIBER_NUMBER_REQUEST:
                            processSubscriberNumberRequest(event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_AT_CIND:
                            processAtCind(event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_AT_COPS:
                            processAtCops(event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_AT_CLCC:
                            processAtClcc(event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_UNKNOWN_AT:
                            processUnknownAt(event.valueString, event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_KEY_PRESSED:
                            processKeyPressed(event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_BIND:
                            processAtBind(event.valueString, event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_TWSP_BATTERY_STATE:
                            processTwsBatteryState(event.valueString,
                                                  event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_BIEV:
                            processAtBiev(event.valueInt, event.valueInt2, event.device);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_BIA:
                            updateAgIndicatorEnableState(
                                    (HeadsetAgIndicatorEnableState) event.valueObject);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_SWB:
                            processSWBEvent(event.valueInt);
                            break;
                        default:
                            stateLogE("Unknown stack event: " + event);
                            break;
                    }
                    break;
                default:
                    stateLogE("Unexpected msg " + getMessageName(message.what) + ": " + message);
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        @Override
        public void processConnectionEvent(Message message, int state) {
            stateLogD("processConnectionEvent, state=" + state);
            switch (state) {
                case HeadsetHalConstants.CONNECTION_STATE_CONNECTED:
                    stateLogE("processConnectionEvent: RFCOMM connected again, shouldn't happen");
                    break;
                case HeadsetHalConstants.CONNECTION_STATE_SLC_CONNECTED:
                    stateLogE("processConnectionEvent: SLC connected again, shouldn't happen");
                    retryConnectCount = 0;
                    break;
                case HeadsetHalConstants.CONNECTION_STATE_DISCONNECTING:
                    stateLogI("processConnectionEvent: Disconnecting");
                    transitionTo(mDisconnecting);
                    break;
                case HeadsetHalConstants.CONNECTION_STATE_DISCONNECTED:
                    stateLogI("processConnectionEvent: Disconnected");
                    processWBSEvent(HeadsetHalConstants.BTHF_WBS_NO);
                    transitionTo(mDisconnected);
                    break;
                default:
                    stateLogE("processConnectionEvent: bad state: " + state);
                    break;
            }
        }

        /**
         * Each state should handle audio events differently
         *
         * @param state audio state
         */
        public abstract void processAudioEvent(int state);
    }

    class Connected extends ConnectedBase {
        @Override
        int getAudioStateInt() {
            return BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
        }

        @Override
        public void enter() {
            super.enter();
            if (mPrevState == mConnecting) {
                // Reset AG indicator subscriptions, HF can set this later using AT+BIA command
                updateAgIndicatorEnableState(DEFAULT_AG_INDICATOR_ENABLE_STATE);
                // Reset NREC on connect event. Headset will override later
                processNoiseReductionEvent(true);
                // Query phone state for initial setup
                sendMessageDelayed(QUERY_PHONE_STATE_AT_SLC, QUERY_PHONE_STATE_CHANGED_DELAYED);
                // Checking for the Blacklisted device Addresses
                mIsBlacklistedDevice = isConnectedDeviceBlacklistedforIncomingCall();
                // Checking for the Blacklisted device Addresses
                mIsBlacklistedForSCOAfterSLC = isSCONeededImmediatelyAfterSLC();
                // Checking for the Blacklisted device Addresses
                mIsBlacklistedDeviceforRetrySCO = isConnectedDeviceBlacklistedforRetrySco();
                if (mSystemInterface.isInCall() || mSystemInterface.isRinging()) {
                   stateLogW("Connected: enter: suspending A2DP for Call since SLC connected");
                   // suspend A2DP since call is there
                   mHeadsetService.getHfpA2DPSyncInterface().suspendA2DP(
                           HeadsetA2dpSync.A2DP_SUSPENDED_BY_CS_CALL, mDevice);
                }
                // Remove pending connection attempts that were deferred during the pending
                // state. This is to prevent auto connect attempts from disconnecting
                // devices that previously successfully connected.
                removeDeferredMessages(CONNECT);
            }
            broadcastStateTransitions();
            DeviceProfileMapIntf dpm = DeviceProfileMapIntf.getDeviceProfileMapInstance();
            dpm.profileConnectionUpdate(mDevice, ApmConstIntf.AudioFeatures.CALL_AUDIO,
                             ApmConstIntf.AudioProfiles.HFP, true);
            dpm.profileConnectionUpdate(mDevice, ApmConstIntf.AudioFeatures.CALL_CONTROL,
                             ApmConstIntf.AudioProfiles.HFP, true);
            dpm.profileConnectionUpdate(mDevice, ApmConstIntf.AudioFeatures.CALL_VOLUME_CONTROL,
                             ApmConstIntf.AudioProfiles.HFP, true);
            VolumeManagerIntf mVolumeManager = VolumeManagerIntf.get();
            mVolumeManager.onConnStateChange(mDevice, BluetoothProfile.STATE_CONNECTED,
                             ApmConstIntf.AudioProfiles.HFP);
            if ((mPrevState == mAudioOn) || (mPrevState == mAudioDisconnecting)||
                 (mPrevState == mAudioConnecting)) {
                if (!(mSystemInterface.isInCall() || mSystemInterface.isRinging())) {
                        // SCO disconnected, resume A2DP if there is no call
                        stateLogD("SCO disconnected, set A2DPsuspended to false");
                        sendMessage(RESUME_A2DP);
                }
            }
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CONNECT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    stateLogW("CONNECT, ignored, device=" + device + ", currentDevice" + mDevice);
                    break;
                }
                case DISCONNECT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    stateLogD("DISCONNECT from device=" + device);
                    if (!mDevice.equals(device)) {
                        stateLogW("DISCONNECT, device " + device + " not connected");
                        break;
                    }
                    if (!mNativeInterface.disconnectHfp(device)) {
                        // broadcast immediately as no state transition is involved
                        stateLogE("DISCONNECT from " + device + " failed");
                        broadcastConnectionState(device, BluetoothProfile.STATE_CONNECTED,
                                BluetoothProfile.STATE_CONNECTED);
                        break;
                    }
                    transitionTo(mDisconnecting);
                }
                break;
                case CONNECT_AUDIO:
                    stateLogD("CONNECT_AUDIO, device=" + mDevice);
                    int a2dpState = mHeadsetService.getHfpA2DPSyncInterface().isA2dpPlaying();
                    if ((mHeadsetService.isScoAcceptable(mDevice) !=
                                BluetoothStatusCodes.SUCCESS) || (a2dpState == HeadsetA2dpSync.A2DP_PLAYING)) {
                        stateLogW("No Active/Held call, no call setup,and no in-band ringing,"
                                  + " or A2Dp is playing, not allowing SCO, device=" + mDevice);
                        break;
                    }

                    Log.w(TAG, "mIsSwbSupportedByRemote is " + mIsSwbSupportedByRemote);

                    if (mIsSwbSupportedByRemote && mHeadsetService.isSwbEnabled() &&
                            mHeadsetService.isSwbPmEnabled()) {
                        if (!mHeadsetService.isVirtualCallStarted() &&
                             mSystemInterface.isHighDefCallInProgress()) {
                           log("CONNECT_AUDIO: enable SWB for HD call ");
                           mHeadsetService.enableSwbCodec(true);
                        } else {
                           log("CONNECT_AUDIO: disable SWB for non-HD or Voip calls");
                           mHeadsetService.enableSwbCodec(false);
                        }
                    }

                    if (!mNativeInterface.connectAudio(mDevice)) {
                        stateLogE("Failed to connect SCO audio for " + mDevice);
                        // No state change involved, fire broadcast immediately
                        broadcastAudioState(mDevice, BluetoothHeadset.STATE_AUDIO_DISCONNECTED,
                                BluetoothHeadset.STATE_AUDIO_DISCONNECTED);
                        break;
                    }
                    transitionTo(mAudioConnecting);
                    break;
                case DISCONNECT_AUDIO:
                    stateLogD("ignore DISCONNECT_AUDIO, device=" + mDevice);
                    // ignore
                    break;
                case RESUME_A2DP: {
                     /* If the call started/ended by the time A2DP suspend ack
                      * is received, send the call indicators before resuming
                      * A2DP.
                      */
                     if (mPendingCallStates.size() == 0) {
                         stateLogD("RESUME_A2DP evt, resuming A2DP");
                         mHeadsetService.getHfpA2DPSyncInterface().releaseA2DP(mDevice);
                     } else {
                         stateLogW("RESUME_A2DP evt, pending call states to be sent, not resuming");
                     }
                     break;
                }
                default:
                    return super.processMessage(message);
            }
            return HANDLED;
        }

        @Override
        public void processAudioEvent(int state) {
            stateLogD("processAudioEvent, state=" + state);
            switch (state) {
                case HeadsetHalConstants.AUDIO_STATE_CONNECTED:
                    if (mHeadsetService.isScoAcceptable(mDevice) !=
                                BluetoothStatusCodes.SUCCESS) {
                        stateLogW("processAudioEvent: reject incoming audio connection");
                        if (!mNativeInterface.disconnectAudio(mDevice)) {
                            stateLogE("processAudioEvent: failed to disconnect audio");
                        }
                        // Indicate rejection to other components.
                        broadcastAudioState(mDevice, BluetoothHeadset.STATE_AUDIO_DISCONNECTED,
                                BluetoothHeadset.STATE_AUDIO_DISCONNECTED);
                        break;
                    }
                    if (!mSystemInterface.getHeadsetPhoneState().getIsCsCall()) {
                        stateLogI("Sco connected for call other than CS, check network type");
                        sendVoipConnectivityNetworktype(true);
                    } else {
                        stateLogI("Sco connected for CS call, do not check network type");
                    }
                    stateLogI("processAudioEvent: audio connected");
                    transitionTo(mAudioOn);
                    break;
                case HeadsetHalConstants.AUDIO_STATE_CONNECTING:
                    if (mHeadsetService.isScoAcceptable(mDevice) !=
                            BluetoothStatusCodes.SUCCESS) {
                        stateLogW("processAudioEvent: reject incoming pending audio connection");
                        if (!mNativeInterface.disconnectAudio(mDevice)) {
                            stateLogE("processAudioEvent: failed to disconnect pending audio");
                        }
                        // Indicate rejection to other components.
                        broadcastAudioState(mDevice, BluetoothHeadset.STATE_AUDIO_DISCONNECTED,
                                BluetoothHeadset.STATE_AUDIO_DISCONNECTED);
                        break;
                    }
                    stateLogI("processAudioEvent: audio connecting");
                    transitionTo(mAudioConnecting);
                    break;
                case HeadsetHalConstants.AUDIO_STATE_DISCONNECTED:
                    if (!(mSystemInterface.isInCall() || mSystemInterface.isRinging())) {
                        /* If the call started/ended by the time A2DP suspend ack
                         * is received, send the call indicators before resuming
                         * A2DP.
                         */
                        if (mPendingCallStates.size() == 0) {
                          stateLogD("processAudioEvent, resuming A2DP after SCO disconnected");
                          mHeadsetService.getHfpA2DPSyncInterface().releaseA2DP(mDevice);
                        } else {
                          stateLogW("processAudioEvent,not resuming due to pending call states");
                        }
                    }
                    break;
                case HeadsetHalConstants.AUDIO_STATE_DISCONNECTING:
                    // ignore
                    break;
                default:
                    stateLogE("processAudioEvent: bad state: " + state);
                    break;
            }
        }
    }

    class AudioConnecting extends ConnectedBase {
        @Override
        int getAudioStateInt() {
            return BluetoothHeadset.STATE_AUDIO_CONNECTING;
        }

        @Override
        public void enter() {
            super.enter();
            sendMessageDelayed(CONNECT_TIMEOUT, mDevice, sConnectTimeoutMs);
            broadcastStateTransitions();
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CONNECT:
                case DISCONNECT:
                case CONNECT_AUDIO:
                case DISCONNECT_AUDIO:
                    deferMessage(message);
                    break;
                case CONNECT_TIMEOUT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogW("CONNECT_TIMEOUT for unknown device " + device);
                        break;
                    }
                    stateLogW("CONNECT_TIMEOUT");
                    transitionTo(mConnected);
                    break;
                }
                default:
                    return super.processMessage(message);
            }
            return HANDLED;
        }

        @Override
        public void processAudioEvent(int state) {
            switch (state) {
                case HeadsetHalConstants.AUDIO_STATE_DISCONNECTED:
                    stateLogW("processAudioEvent: audio connection failed");
                    if ((mIsBlacklistedDeviceforRetrySCO == true) && (mIsRetrySco == true)) {
                        Log.d(TAG, "blacklisted device, retry SCO after " +
                                     RETRY_SCO_CONNECTION_DELAY + " msec");
                       Message m = obtainMessage(CONNECT_AUDIO);
                       sendMessageDelayed(m, RETRY_SCO_CONNECTION_DELAY);
                       mIsRetrySco = false;
                    }
                    transitionTo(mConnected);
                    break;
                case HeadsetHalConstants.AUDIO_STATE_CONNECTING:
                    // ignore, already in audio connecting state
                    break;
                case HeadsetHalConstants.AUDIO_STATE_DISCONNECTING:
                    // ignore, there is no BluetoothHeadset.STATE_AUDIO_DISCONNECTING
                    break;
                case HeadsetHalConstants.AUDIO_STATE_CONNECTED:
                    if (!mSystemInterface.getHeadsetPhoneState().getIsCsCall()) {
                        stateLogI("Sco connected for call other than CS, check network type");
                        sendVoipConnectivityNetworktype(true);
                    } else {
                        stateLogI("Sco connected for CS call, do not check network type");
                    }
                    stateLogI("processAudioEvent: audio connected");
                    transitionTo(mAudioOn);
                    break;
                default:
                    stateLogE("processAudioEvent: bad state: " + state);
                    break;
            }
        }

        @Override
        public void exit() {
            removeMessages(CONNECT_TIMEOUT);
            super.exit();
        }
    }

    class AudioOn extends ConnectedBase {
        @Override
        int getAudioStateInt() {
            return BluetoothHeadset.STATE_AUDIO_CONNECTED;
        }

        @Override
        public void enter() {
            super.enter();
            removeDeferredMessages(CONNECT_AUDIO);
            // Set active device to current active SCO device when the current active device
            // is different from mCurrentDevice. This is to accommodate active device state
            // mis-match between native and Java.
            if (!mDevice.equals(mHeadsetService.getActiveDevice())
                    && !hasDeferredMessages(DISCONNECT_AUDIO)) {
                mHeadsetService.setActiveDevice(mDevice);
            }
            // If current device is TWSPLUS device and peer TWSPLUS device is already
            // has SCO, dont need to update teh Audio Manager

            setAudioParameters();
            mSystemInterface.getAudioManager().setBluetoothScoOn(true);
            Message m = obtainMessage(SCO_RETRIAL_NOT_REQ);
            sendMessageDelayed(m, SCO_RETRIAL_REQ_TIMEOUT);

            broadcastStateTransitions();
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CONNECT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    stateLogW("CONNECT, ignored, device=" + device + ", currentDevice" + mDevice);
                    break;
                }
                case DISCONNECT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    stateLogD("DISCONNECT, device=" + device);
                    if (!mDevice.equals(device)) {
                        stateLogW("DISCONNECT, device " + device + " not connected");
                        break;
                    }
                    if (mAdapterService.isTwsPlusDevice(device)) {
                        //for twsplus device, don't disconnect the SCO for app
                        //force the disocnnect of HF and in turn let SCO shutdown
                        //so that SCO SM handles it gracefully
                        if (!mNativeInterface.disconnectHfp(device)) {
                            stateLogW("DISCONNECT failed TWS case, device=" + mDevice);
                        }
                        transitionTo(mDisconnecting);
                    } else {
                        // Disconnect BT SCO first
                        if (!mNativeInterface.disconnectAudio(mDevice)) {
                            stateLogW("DISCONNECT failed, device=" + mDevice);
                            // if disconnect BT SCO failed, transition to mConnected state to force
                            // disconnect device
                        }
                        deferMessage(obtainMessage(DISCONNECT, mDevice));
                        transitionTo(mAudioDisconnecting);
                    }
                    break;
                }
                case CONNECT_AUDIO: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogW("CONNECT_AUDIO device is not connected " + device);
                        break;
                    }
                    stateLogW("CONNECT_AUDIO device auido is already connected " + device);
                    break;
                }
                case DISCONNECT_AUDIO: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogW("DISCONNECT_AUDIO, failed, device=" + device + ", currentDevice="
                                + mDevice);
                        break;
                    }
                    if (mNativeInterface.disconnectAudio(mDevice)) {
                        stateLogD("DISCONNECT_AUDIO, device=" + mDevice);
                        transitionTo(mAudioDisconnecting);
                    } else {
                        stateLogW("DISCONNECT_AUDIO failed, device=" + mDevice);
                        broadcastAudioState(mDevice, BluetoothHeadset.STATE_AUDIO_CONNECTED,
                                BluetoothHeadset.STATE_AUDIO_CONNECTED);
                    }
                    break;
                }
                case INTENT_SCO_VOLUME_CHANGED:
                    processIntentScoVolume((Intent) message.obj, mDevice);
                    break;
                case AUDIO_SERVER_UP:
                    stateLogD("AUDIO_SERVER_UP event");
                    processAudioServerUp();
                    break;
                case SCO_RETRIAL_NOT_REQ:
                     //after this timeout, sco retrial is not required
                     stateLogD("SCO_RETRIIAL_NOT_REQ: ");
                     mIsRetrySco = false;
                     break;
                case STACK_EVENT:
                    HeadsetStackEvent event = (HeadsetStackEvent) message.obj;
                    stateLogD("STACK_EVENT: " + event);
                    if (!mDevice.equals(event.device)) {
                        stateLogE("Event device does not match currentDevice[" + mDevice
                                + "], event: " + event);
                        break;
                    }
                    switch (event.type) {
                        case HeadsetStackEvent.EVENT_TYPE_WBS:
                            stateLogE("Cannot change WBS state when audio is connected: " + event);
                            break;
                        case HeadsetStackEvent.EVENT_TYPE_SWB:
                            stateLogE("Cannot change SWB state when audio is connected: " + event);
                            break;
                        default:
                            super.processMessage(message);
                            break;
                    }
                    break;
                default:
                    return super.processMessage(message);
            }
            return HANDLED;
        }

        @Override
        public void processAudioEvent(int state) {
            switch (state) {
                case HeadsetHalConstants.AUDIO_STATE_DISCONNECTED:
                    stateLogI("processAudioEvent: audio disconnected by remote");
                    if (mAdapterService.isTwsPlusDevice(mDevice) && mHeadsetService.isAudioOn()) {
                        //If It is TWSP device, make sure SCO is not active on
                        //any devices before letting Audio knowing about it
                        stateLogI("TWS+ device and other SCO is still Active, no BT_SCO=off");
                    }
                    if (!mSystemInterface.getHeadsetPhoneState().getIsCsCall()) {
                        stateLogI("Sco disconnected for call other than CS, check network type");
                        sendVoipConnectivityNetworktype(false);
                        mSystemInterface.getHeadsetPhoneState().setIsCsCall(true);
                    } else {
                        stateLogI("Sco disconnected for CS call, do not check network type");
                    }
                    if ((mIsBlacklistedDeviceforRetrySCO == true) && (mIsRetrySco == true)) {
                        Log.d(TAG, "blacklisted device, retry SCO after " +
                                     RETRY_SCO_CONNECTION_DELAY + " msec");
                        Message m = obtainMessage(CONNECT_AUDIO);
                        sendMessageDelayed(m, RETRY_SCO_CONNECTION_DELAY);
                        mIsRetrySco = false;
                    }
                    transitionTo(mConnected);
                    break;
                case HeadsetHalConstants.AUDIO_STATE_DISCONNECTING:
                    stateLogI("processAudioEvent: audio being disconnected by remote");
                    transitionTo(mAudioDisconnecting);
                    break;
                default:
                    stateLogE("processAudioEvent: bad state: " + state);
                    break;
            }
        }

        private void processIntentScoVolume(Intent intent, BluetoothDevice device) {
            int volumeValue = intent.getIntExtra(AudioManager.EXTRA_VOLUME_STREAM_VALUE, 0);
            stateLogD(" mSpeakerVolume = " + mSpeakerVolume + " volValue = " + volumeValue);
            if (mSpeakerVolume != volumeValue) {
                mSpeakerVolume = volumeValue;
                mNativeInterface.setVolume(device, HeadsetHalConstants.VOLUME_TYPE_SPK,
                    mSpeakerVolume);
            }
        }
    }

    class AudioDisconnecting extends ConnectedBase {
        @Override
        int getAudioStateInt() {
            // TODO: need BluetoothHeadset.STATE_AUDIO_DISCONNECTING
            return BluetoothHeadset.STATE_AUDIO_CONNECTED;
        }

        @Override
        public void enter() {
            super.enter();
            sendMessageDelayed(CONNECT_TIMEOUT, mDevice, sConnectTimeoutMs);
            broadcastStateTransitions();
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CONNECT:
                case DISCONNECT:
                case CONNECT_AUDIO:
                case DISCONNECT_AUDIO:
                    deferMessage(message);
                    break;
                case CONNECT_TIMEOUT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        stateLogW("CONNECT_TIMEOUT for unknown device " + device);
                        break;
                    }
                    stateLogW("CONNECT_TIMEOUT");
                    transitionTo(mConnected);
                    break;
                }
                default:
                    return super.processMessage(message);
            }
            return HANDLED;
        }

        @Override
        public void processAudioEvent(int state) {
            switch (state) {
                case HeadsetHalConstants.AUDIO_STATE_DISCONNECTED:
                    stateLogI("processAudioEvent: audio disconnected");
                    if (mAdapterService.isTwsPlusDevice(mDevice) && mHeadsetService.isAudioOn()) {
                         //If It is TWSP device, make sure SCO is not active on
                         //any devices before letting Audio knowing about it
                         stateLogI("TWS+ device and other SCO is still Active, no BT_SCO=off");
                    }
                    transitionTo(mConnected);
                    break;
                case HeadsetHalConstants.AUDIO_STATE_DISCONNECTING:
                    // ignore
                    break;
                case HeadsetHalConstants.AUDIO_STATE_CONNECTED:
                    stateLogW("processAudioEvent: audio disconnection failed");
                    transitionTo(mAudioOn);
                    break;
                case HeadsetHalConstants.AUDIO_STATE_CONNECTING:
                    // ignore, see if it goes into connected state, otherwise, timeout
                    break;
                default:
                    stateLogE("processAudioEvent: bad state: " + state);
                    break;
            }
        }

        @Override
        public void exit() {
            removeMessages(CONNECT_TIMEOUT);
            super.exit();
        }
    }

    /**
     * Get the underlying device tracked by this state machine
     *
     * @return device in focus
     */
    @VisibleForTesting
    public synchronized BluetoothDevice getDevice() {
        return mDevice;
    }

    /**
     * Get the current connection state of this state machine
     *
     * @return current connection state, one of {@link BluetoothProfile#STATE_DISCONNECTED},
     * {@link BluetoothProfile#STATE_CONNECTING}, {@link BluetoothProfile#STATE_CONNECTED}, or
     * {@link BluetoothProfile#STATE_DISCONNECTING}
     */
    @VisibleForTesting
    public synchronized int getConnectionState() {
        //getCurrentState()
        HeadsetStateBase state = (HeadsetStateBase) getCurrentHeadsetStateMachineState();
        if (state == null) {
            return BluetoothHeadset.STATE_DISCONNECTED;
        }
        return state.getConnectionStateInt();
    }

    /**
     * Get the current audio state of this state machine
     *
     * @return current audio state, one of {@link BluetoothHeadset#STATE_AUDIO_DISCONNECTED},
     * {@link BluetoothHeadset#STATE_AUDIO_CONNECTING}, or
     * {@link BluetoothHeadset#STATE_AUDIO_CONNECTED}
     */
    public synchronized int getAudioState() {
        //getCurrentState()
        HeadsetStateBase state = (HeadsetStateBase) getCurrentHeadsetStateMachineState();
        if (state == null) {
            return BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
        }
        return state.getAudioStateInt();
    }

    public long getConnectingTimestampMs() {
        return mConnectingTimestampMs;
    }

    /**
     * Set the silence mode status of this state machine
     *
     * @param silence true to enter silence mode, false on exit
     * @return true on success, false on error
     */
    @VisibleForTesting
    public boolean setSilenceDevice(boolean silence) {
        if (silence == mDeviceSilenced) {
            return false;
        }
        if (silence) {
            mSystemInterface.getHeadsetPhoneState().listenForPhoneState(mDevice,
                    PhoneStateListener.LISTEN_NONE);
        } else {
            updateAgIndicatorEnableState(mAgIndicatorEnableState);
        }
        mDeviceSilenced = silence;
        return true;
    }

    private void processAudioServerUp() {
        Log.i(TAG, "onAudioSeverUp: restore audio parameters");
        mSystemInterface.getAudioManager().setBluetoothScoOn(false);
        mSystemInterface.getAudioManager().setParameters("A2dpSuspended=true");
        setAudioParameters();
        mSystemInterface.getAudioManager().setBluetoothScoOn(true);
    }

    /*
     * Put the AT command, company ID, arguments, and device in an Intent and broadcast it.
     */
    private void broadcastVendorSpecificEventIntent(String command, int companyId, int commandType,
            Object[] arguments, BluetoothDevice device) {
        log("broadcastVendorSpecificEventIntent(" + command + ")");
        Intent intent = new Intent(BluetoothHeadset.ACTION_VENDOR_SPECIFIC_HEADSET_EVENT);
        intent.putExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_CMD, command);
        intent.putExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_CMD_TYPE, commandType);
        // assert: all elements of args are Serializable
        intent.putExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_ARGS, arguments);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.addCategory(BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_COMPANY_ID_CATEGORY + "."
                + Integer.toString(companyId));
        mHeadsetService.sendBroadcastAsUser(intent, UserHandle.ALL, BLUETOOTH_CONNECT,
                Utils.getTempAllowlistBroadcastOptions());
    }

    private void setAudioParameters() {
        String keyValuePairs = String.join(";", new String[]{
                HEADSET_NAME + "=" + getCurrentDeviceName(),
                HEADSET_NREC + "=" + mAudioParams.getOrDefault(HEADSET_NREC,
                        HEADSET_AUDIO_FEATURE_OFF),
                HEADSET_WBS + "=" + mAudioParams.getOrDefault(HEADSET_WBS,
                        HEADSET_AUDIO_FEATURE_OFF),
                HEADSET_SWB + "=" + mAudioParams.getOrDefault(HEADSET_SWB,
                        HEADSET_SWB_DISABLE)
        });
        Log.i(TAG, "setAudioParameters for " + mDevice + ": " + keyValuePairs);
        mSystemInterface.getAudioManager().setParameters(keyValuePairs);
    }

    private String parseUnknownAt(String atString) {
        StringBuilder atCommand = new StringBuilder(atString.length());

        for (int i = 0; i < atString.length(); i++) {
            char c = atString.charAt(i);
            if (c == '"') {
                int j = atString.indexOf('"', i + 1); // search for closing "
                if (j == -1) { // unmatched ", insert one.
                    atCommand.append(atString.substring(i, atString.length()));
                    atCommand.append('"');
                    break;
                }
                atCommand.append(atString.substring(i, j + 1));
                i = j;
            } else if (c != ' ') {
                atCommand.append(Character.toUpperCase(c));
            }
        }
        return atCommand.toString();
    }

    private int getAtCommandType(String atCommand) {
        int commandType = AtPhonebook.TYPE_UNKNOWN;
        String atString = null;
        atCommand = atCommand.trim();
        if (atCommand.length() > 5) {
            atString = atCommand.substring(5);
            if (atString.startsWith("?")) { // Read
                commandType = AtPhonebook.TYPE_READ;
            } else if (atString.startsWith("=?")) { // Test
                commandType = AtPhonebook.TYPE_TEST;
            } else if (atString.startsWith("=")) { // Set
                commandType = AtPhonebook.TYPE_SET;
            } else {
                commandType = AtPhonebook.TYPE_UNKNOWN;
            }
        }
        return commandType;
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    private void processDialCall(String number) {
        String dialNumber;
        if (mHeadsetService.hasDeviceInitiatedDialingOut()) {
            Log.w(TAG, "processDialCall, already dialling");
            mNativeInterface.atResponseCode(mDevice, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
            return;
        }
        if ((number == null) || (number.length() == 0)) {
            dialNumber = mPhonebook.getLastDialledNumber();
            log("dialNumber: " + dialNumber);
            if ((dialNumber == null) || (dialNumber.length() == 0)) {
                Log.w(TAG, "processDialCall, last dial number null or empty ");
                mNativeInterface.atResponseCode(mDevice, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
                return;
            }
        } else if (number.charAt(0) == '>') {
            // Yuck - memory dialling requested.
            // Just dial last number for now
            if (number.startsWith(">9999")) { // for PTS test
                Log.w(TAG, "Number is too big");
                mNativeInterface.atResponseCode(mDevice, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
                return;
            }
            log("processDialCall, memory dial do last dial for now");
            dialNumber = mPhonebook.getLastDialledNumber();
            if (dialNumber == null) {
                Log.w(TAG, "processDialCall, last dial number null");
                mNativeInterface.atResponseCode(mDevice, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
                return;
            }
        } else {
            // Remove trailing ';'
            if (number.charAt(number.length() - 1) == ';') {
                number = number.substring(0, number.length() - 1);
            }
            dialNumber = PhoneNumberUtils.convertPreDial(number);
        }
        if (!mHeadsetService.dialOutgoingCall(mDevice, dialNumber)) {
            Log.w(TAG, "processDialCall, failed to dial in service");
            mNativeInterface.atResponseCode(mDevice, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
            return;
        }
        mNeedDialingOutReply = true;
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    private void processVrEvent(int state) {
        if (state == HeadsetHalConstants.VR_STATE_STARTED) {
            if (!mHeadsetService.startVoiceRecognitionByHeadset(mDevice)) {
                mNativeInterface.atResponseCode(mDevice, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
            }
        } else if (state == HeadsetHalConstants.VR_STATE_STOPPED) {
            if (mHeadsetService.stopVoiceRecognitionByHeadset(mDevice)) {
                mNativeInterface.atResponseCode(mDevice, HeadsetHalConstants.AT_RESPONSE_OK, 0);
            } else {
                mNativeInterface.atResponseCode(mDevice, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
            }
        } else {
            mNativeInterface.atResponseCode(mDevice, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
        }
    }

    private void processVolumeEvent(int volumeType, int volume) {
        // Only current active device can change SCO volume
        if (!mHeadsetService.isTwsPlusActive(mDevice) &&
            !mDevice.equals(mHeadsetService.getActiveDevice())) {
            Log.w(TAG, "processVolumeEvent, ignored because " + mDevice + " is not active");
            return;
        }
        if (volumeType == HeadsetHalConstants.VOLUME_TYPE_SPK) {
            mSpeakerVolume = volume;
            int flag = (getCurrentHeadsetStateMachineState() == mAudioOn)
                        ? AudioManager.FLAG_SHOW_UI : 0;
            mSystemInterface.getAudioManager()
                    .setStreamVolume(AudioManager.STREAM_BLUETOOTH_SCO, volume, flag);
        } else if (volumeType == HeadsetHalConstants.VOLUME_TYPE_MIC) {
            // Not used currently
            mMicVolume = volume;
        } else {
            Log.e(TAG, "Bad volume type: " + volumeType);
        }
    }

    private void processCallStatesDelayed(HeadsetCallState callState, boolean isVirtualCall)
    {
        log("Enter processCallStatesDelayed");
        final HeadsetPhoneState mPhoneState = mSystemInterface.getHeadsetPhoneState();
        if (callState.mCallState == HeadsetHalConstants.CALL_STATE_DIALING)
        {
            // at this point, queue should be empty.
            processCallState(callState, false);
        }
        // update is for call alerting
        else if (callState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING &&
                  mStateMachineCallState.mNumActive == callState.mNumActive &&
                  mStateMachineCallState.mNumHeld == callState.mNumHeld &&
                  mStateMachineCallState.mCallState == HeadsetHalConstants.CALL_STATE_DIALING)
        {
            log("Queue alerting update, send alerting delayed mesg");
            //Q the call state;
            mDelayedCSCallStates.add(callState);

            //send delayed message for call alerting;
            Message msg = obtainMessage(CS_CALL_STATE_CHANGED_ALERTING);
            msg.arg1 = 0;
            sendMessageDelayed(msg, CS_CALL_ALERTING_DELAY_TIME_MSEC);
        }
        // call moved to active from alerting state
        else if (mStateMachineCallState.mNumActive == 0 &&
                 callState.mNumActive == 1 &&
                 mStateMachineCallState.mNumHeld == callState.mNumHeld &&
                 (mStateMachineCallState.mCallState == HeadsetHalConstants.CALL_STATE_DIALING ||
                  mStateMachineCallState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING ))
        {
            log("Call moved to active state from alerting");
            // get the top of the Q
            HeadsetCallState tempCallState = mDelayedCSCallStates.peek();

            //if (top of the Q == alerting)
            if( tempCallState != null &&
                 tempCallState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING)
            {
                log("Call is active, Queue it, top of Queue is alerting");
                //Q active update;
                mDelayedCSCallStates.add(callState);
            }
            else
            // Q is empty
            {
                log("is Q empty " + mDelayedCSCallStates.isEmpty());
                log("Call is active, Queue it, send delayed active mesg");
                //Q active update;
                mDelayedCSCallStates.add(callState);
                //send delayed message for call active;
                Message msg = obtainMessage(CS_CALL_STATE_CHANGED_ACTIVE);
                msg.arg1 = 0;
                sendMessageDelayed(msg, CS_CALL_ACTIVE_DELAY_TIME_MSEC);
            }
        }
        // call setup or call ended
        else if((mStateMachineCallState.mCallState == HeadsetHalConstants.CALL_STATE_DIALING ||
                  mStateMachineCallState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING ) &&
                  callState.mCallState == HeadsetHalConstants.CALL_STATE_IDLE &&
                  mStateMachineCallState.mNumActive == callState.mNumActive &&
                  mStateMachineCallState.mNumHeld == callState.mNumHeld)
        {
            log("call setup or call is ended");
            // get the top of the Q
            HeadsetCallState tempCallState = mDelayedCSCallStates.peek();

            //if (top of the Q == alerting)
            if(tempCallState != null &&
                tempCallState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING)
            {
                log("Call is ended, remove delayed alerting mesg");
                removeMessages(CS_CALL_STATE_CHANGED_ALERTING);
                //DeQ(alerting);
                mDelayedCSCallStates.poll();
                // send 2,3 although the call is ended to make sure that we are sending 2,3 always
                processCallState(tempCallState, false);

                // update the top of the Q entry so that we process the active
                // call entry from the Q below
                tempCallState = mDelayedCSCallStates.peek();
            }

            //if (top of the Q == active)
            if (tempCallState != null &&
                 tempCallState.mCallState == HeadsetHalConstants.CALL_STATE_IDLE)
            {
                log("Call is ended, remove delayed active mesg");
                removeMessages(CS_CALL_STATE_CHANGED_ACTIVE);
                //DeQ(active);
                mDelayedCSCallStates.poll();
            }
            // send current call state which will take care of sending call end indicator
            processCallState(callState, false);
        } else {
            HeadsetCallState tempCallState;

            // if there are pending call states to be sent, send them now
            if (mDelayedCSCallStates.isEmpty() != true)
            {
                log("new call update, removing pending alerting, active messages");
                // remove pending delayed call states
                removeMessages(CS_CALL_STATE_CHANGED_ALERTING);
                removeMessages(CS_CALL_STATE_CHANGED_ACTIVE);
            }

            while (mDelayedCSCallStates.isEmpty() != true)
            {
                tempCallState = mDelayedCSCallStates.poll();
                if (tempCallState != null)
                {
                    processCallState(tempCallState, false);
                }
            }
            // it is incoming call or MO call in non-alerting, non-active state.
            processCallState(callState, isVirtualCall);
        }
        log("Exit processCallStatesDelayed");
    }

    private void processCallState(HeadsetCallState callState, boolean isVirtualCall) {
        /* If active call is ended, no held call is present, disconnect SCO
         * and fake the MT Call indicators. */
        boolean isPts =
                SystemProperties.getBoolean("vendor.bt.pts.certification", false);
        if (!isPts) {
            log("mIsBlacklistedDevice:" + mIsBlacklistedDevice);
            if (mIsBlacklistedDevice &&
                mStateMachineCallState.mNumActive == 1 &&
                callState.mNumActive == 0 &&
                callState.mNumHeld == 0 &&
                callState.mCallState == HeadsetHalConstants.CALL_STATE_INCOMING) {

                log("Disconnect SCO since active call is ended," +
                                    "only waiting call is there");
                Message m = obtainMessage(DISCONNECT_AUDIO);
                m.obj = mDevice;
                sendMessage(m);

                log("Send Idle call indicators once Active call disconnected.");
                // TODO: cross check this
                mStateMachineCallState.mCallState =
                                               HeadsetHalConstants.CALL_STATE_IDLE;
                HeadsetCallState updateCallState = new HeadsetCallState(callState.mNumActive,
                                 callState.mNumHeld,
                                 HeadsetHalConstants.CALL_STATE_IDLE,
                                 callState.mNumber,
                                 callState.mType, MERGE_PLACEHOLDER);
                mNativeInterface.phoneStateChange(mDevice, updateCallState);
                mIsCallIndDelay = true;
            }
            
            /* The device is blacklisted for sending incoming call setup
             * indicator after SCO disconnection and sending active call end
             * indicator. While the incoming call setup indicator is in queue,
             * waiting call moved to active state. Send call setup update first
             * and remove it from queue. Create SCO since SCO might be in
             * disconnecting/disconnected state */
            if (mIsBlacklistedDevice &&
                callState.mNumActive == 1 &&
                callState.mNumHeld == 0 &&
                callState.mCallState == HeadsetHalConstants.CALL_STATE_IDLE &&
                hasMessages(SEND_INCOMING_CALL_IND)) {

                Log.w(TAG, "waiting call moved to active state while incoming call");
                Log.w(TAG, "setup indicator is in queue. Send it first and create SCO");
                //remove call setup indicator from queue.
                removeMessages(SEND_INCOMING_CALL_IND);

                HeadsetCallState incomingCallSetupState =
                        new HeadsetCallState(0, 0, HeadsetHalConstants.CALL_STATE_INCOMING,
                               mSystemInterface.getHeadsetPhoneState().getNumber(),
                                 mSystemInterface.getHeadsetPhoneState().getType(),
                                 "");
                mNativeInterface.phoneStateChange(mDevice, incomingCallSetupState);

                if (mDevice.equals(mHeadsetService.getActiveDevice())) {
                    Message m = obtainMessage(CONNECT_AUDIO);
                    m.obj = mDevice;
                    sendMessage(m);
                }
            }
        }

        /* If device is blacklisted, set retry SCO flag true before creating SCO for 1st time */
        Log.d(TAG, "mIsBlacklistedDeviceforRetrySCO: " + mIsBlacklistedDeviceforRetrySCO);
        if (mIsBlacklistedDeviceforRetrySCO) {
           if (((mStateMachineCallState.mNumActive == 0 && callState.mNumActive == 1) ||
                (mStateMachineCallState.mNumHeld == 0 && callState.mNumHeld == 1)) &&
                (mStateMachineCallState.mCallState == HeadsetHalConstants.CALL_STATE_INCOMING &&
                 callState.mCallState == HeadsetHalConstants.CALL_STATE_IDLE)) {
              Log.d(TAG, "Incoming call is accepted as Active or Held call");
              mIsRetrySco = true;
              RETRY_SCO_CONNECTION_DELAY =
                      SystemProperties.getInt("persist.vendor.btstack.MT.RETRY_SCO.interval", 2000);
           } else if ((callState.mNumActive == 0 && callState.mNumHeld == 0) &&
                     (mStateMachineCallState.mCallState == HeadsetHalConstants.CALL_STATE_IDLE &&
                     (callState.mCallState == HeadsetHalConstants.CALL_STATE_DIALING ||
                     callState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING)))
           {
               Log.d(TAG, "Dialing or Alerting indication");
               mIsRetrySco = true;
               RETRY_SCO_CONNECTION_DELAY =
                       SystemProperties.getInt("persist.vendor.btstack.MO.RETRY_SCO.interval", 2000);
           }
        }
        mStateMachineCallState.mNumActive = callState.mNumActive;
        mStateMachineCallState.mNumHeld = callState.mNumHeld;
        // get the top of the Q
        HeadsetCallState tempCallState = mDelayedCSCallStates.peek();

        if ( !isVirtualCall && tempCallState != null &&
             tempCallState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING &&
             callState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING) {
             log("update call state as dialing since alerting update is in Q");
             log("current call state is " + mStateMachineCallState.mCallState);
             callState.mCallState = HeadsetHalConstants.CALL_STATE_DIALING;
        }

        mStateMachineCallState.mCallState = callState.mCallState;
        mStateMachineCallState.mNumber = callState.mNumber;
        mStateMachineCallState.mType = callState.mType;

        log("processCallState: mNumActive: " + callState.mNumActive + " mNumHeld: "
                + callState.mNumHeld + " mCallState: " + callState.mCallState);
        log("processCallState: mNumber: " + callState.mNumber + " mType: " + callState.mType);

        Log.w(TAG, "processCallState: mIsSwbSupportedByRemote is " + mIsSwbSupportedByRemote);

        if (mIsSwbSupportedByRemote && mHeadsetService.isSwbEnabled() &&
               mHeadsetService.isSwbPmEnabled()) {
            if (mHeadsetService.isVirtualCallStarted()) {
                 log("processCallState: enable SWB for all voip calls ");
                 mHeadsetService.enableSwbCodec(true);
            } else if((callState.mCallState == HeadsetHalConstants.CALL_STATE_DIALING) ||
               (callState.mCallState == HeadsetHalConstants.CALL_STATE_INCOMING) ||
                ((callState.mCallState == HeadsetHalConstants.CALL_STATE_IDLE) &&
                 (callState.mNumActive > 0))) {
                 if (!mSystemInterface.isHighDefCallInProgress()) {
                    log("processCallState: disable SWB for non-HD call ");
                    mHeadsetService.enableSwbCodec(false);
                    mAudioParams.put(HEADSET_SWB, HEADSET_SWB_DISABLE);
                 } else {
                    log("processCallState: enable SWB for HD call ");
                    mHeadsetService.enableSwbCodec(true);
                    mAudioParams.put(HEADSET_SWB, "0");
                 }
            }
        }

        processA2dpState(callState);
    }

    /* This function makes sure that we send a2dp suspend before updating on Incomming call status.
       There may problem with some headsets if send ring and a2dp is not suspended,
       so here we suspend stream if active before updating remote.We resume streaming once
       callstate is idle and there are no active or held calls. */

    private void processA2dpState(HeadsetCallState callState) {
        int a2dpState = mHeadsetService.getHfpA2DPSyncInterface().isA2dpPlaying();
        Log.d(TAG, "processA2dpState: isA2dpPlaying() " + a2dpState);

        if ((mSystemInterface.isInCall() || mSystemInterface.isRinging()) &&
              getConnectionState() == BluetoothHeadset.STATE_CONNECTED) {
            // if A2DP is playing, add CS call states and return
            if (mHeadsetService.getHfpA2DPSyncInterface().suspendA2DP(
                 HeadsetA2dpSync.A2DP_SUSPENDED_BY_CS_CALL, mDevice) == true) {
                 Log.d(TAG, "processA2dpState: A2DP is playing, suspending it,"+
                             "cache the call state for future");
                 mPendingCallStates.add(callState);
                 return;
            }
        }

        if (getCurrentHeadsetStateMachineState() != mDisconnected) {
            log("No A2dp playing to suspend, mIsCallIndDelay: " + mIsCallIndDelay +
                " mPendingCallStates.size(): " + mPendingCallStates.size());
            //When MO call creation and disconnection done back to back, Make sure to send
            //the call indicators in a sequential way to remote
            if (mPendingCallStates.size() != 0) {
                Log.d(TAG, "Cache the call state, PendingCallStates list is not empty");
                mPendingCallStates.add(callState);
                return;
            }
            if (mIsCallIndDelay) {
                mIsCallIndDelay = false;
                sendMessageDelayed(SEND_INCOMING_CALL_IND, INCOMING_CALL_IND_DELAY);
            } else {
                mNativeInterface.phoneStateChange(mDevice, callState);
            }
        }
    }

    private void updateCallType() {
        boolean isCsCall = mSystemInterface.isCsCallInProgress();
        Log.d(TAG, "updateCallType " + isCsCall);
        final HeadsetPhoneState mPhoneState = mSystemInterface.getHeadsetPhoneState();
        mPhoneState.setIsCsCall(isCsCall);
        if (getAudioState() == BluetoothHeadset.STATE_AUDIO_CONNECTED) {
            if (!mPhoneState.getIsCsCall()) {
                log("updateCallType, Non CS call, check for network type");
                sendVoipConnectivityNetworktype(true);
            } else {
                log("updateCallType, CS call, do not check for network type");
            }
        } else {
            log("updateCallType: Sco not yet connected");
        }
    }

    private void processIntentA2dpPlayStateChanged(int a2dpState) {
        Log.d(TAG, "Enter processIntentA2dpPlayStateChanged(): a2dp state "+
                  a2dpState);
        if (mHeadsetService.isVRStarted()) {
            Log.d(TAG, "VR is in started state");
            if (mDevice.equals(mHeadsetService.getActiveDevice())) {
               Log.d(TAG, "creating SCO for " + mDevice);
               mNativeInterface.connectAudio(mDevice);
            }
        } else if (mSystemInterface.isInCall() || mHeadsetService.isVirtualCallStarted()){
            //send incoming phone status to remote device
            Log.d(TAG, "A2dp is suspended, updating phone states");
            Iterator<HeadsetCallState> it = mPendingCallStates.iterator();
            if (it != null) {
               while (it.hasNext()) {
                  HeadsetCallState callState = it.next();
                  Log.d(TAG, "mIsCallIndDelay: " + mIsCallIndDelay);
                  mNativeInterface.phoneStateChange(mDevice, callState);
                  it.remove();
               }
            } else {
               Log.d(TAG, "There are no pending call state changes");
            }
        } else {
            Log.d(TAG, "A2DP suspended when there is no CS/VOIP calls or VR, resuming A2DP");
            //When A2DP is suspended and the call is terminated,
            //clean up the PendingCallStates list
            Iterator<HeadsetCallState> it = mPendingCallStates.iterator();
            if (it != null) {
               while (it.hasNext()) {
                  HeadsetCallState callState = it.next();
                  mNativeInterface.phoneStateChange(mDevice, callState);
                  it.remove();
               }
            }
            Log.d(TAG, "Resume A2DP by sending RESUME_A2DP message");
            sendMessage(RESUME_A2DP);
        }
        Log.d(TAG, "Exit processIntentA2dpPlayStateChanged()");
    }

    private void processNoiseReductionEvent(boolean enable) {
        String prevNrec = mAudioParams.getOrDefault(HEADSET_NREC, HEADSET_AUDIO_FEATURE_OFF);
        String newNrec = enable ? HEADSET_AUDIO_FEATURE_ON : HEADSET_AUDIO_FEATURE_OFF;
        mAudioParams.put(HEADSET_NREC, newNrec);
        log("processNoiseReductionEvent: " + HEADSET_NREC + " change " + prevNrec + " -> "
                + newNrec);
        if (getAudioState() == BluetoothHeadset.STATE_AUDIO_CONNECTED) {
            setAudioParameters();
        }
    }

    private void processWBSEvent(int wbsConfig) {
        String prevWbs = mAudioParams.getOrDefault(HEADSET_WBS, HEADSET_AUDIO_FEATURE_OFF);
        switch (wbsConfig) {
            case HeadsetHalConstants.BTHF_WBS_YES:
                mAudioParams.put(HEADSET_WBS, HEADSET_AUDIO_FEATURE_ON);
                if (mHeadsetService.isSwbEnabled()) {
                    mAudioParams.put(HEADSET_SWB, HEADSET_SWB_DISABLE);
                }
                break;
            case HeadsetHalConstants.BTHF_WBS_NO:
            case HeadsetHalConstants.BTHF_WBS_NONE:
                mAudioParams.put(HEADSET_WBS, HEADSET_AUDIO_FEATURE_OFF);
                break;
            default:
                Log.e(TAG, "processWBSEvent: unknown wbsConfig " + wbsConfig);
                return;
        }
        log("processWBSEvent: " + HEADSET_NREC + " change " + prevWbs + " -> " + mAudioParams.get(
                HEADSET_WBS));
    }

    private void processSWBEvent(int swbConfig) {
        if (swbConfig < HEADSET_SWB_MAX_CODEC_IDS) {
                mAudioParams.put(HEADSET_SWB, "0");
                mAudioParams.put(HEADSET_WBS, HEADSET_AUDIO_FEATURE_OFF);
        } else {
                mAudioParams.put(HEADSET_SWB, HEADSET_SWB_DISABLE);
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    private void processAtChld(int chld, BluetoothDevice device) {
        if (mSystemInterface.processChld(chld, ApmConstIntf.AudioProfiles.HFP)) {
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_OK, 0);
        } else {
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    private void processSubscriberNumberRequest(BluetoothDevice device) {
        String number = mSystemInterface.getSubscriberNumber();
        if (number != null) {
            mNativeInterface.atResponseString(device,
                    "+CNUM: ,\"" + number + "\"," + PhoneNumberUtils.toaFromString(number) + ",,4");
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_OK, 0);
        } else {
            Log.e(TAG, "getSubscriberNumber returns null");
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
        }
    }

    private void processAtCind(BluetoothDevice device) {
        int call, callSetup, call_state, service, signal;
         // get the top of the Q
        HeadsetCallState tempCallState = mDelayedCSCallStates.peek();
        final HeadsetPhoneState phoneState = mSystemInterface.getHeadsetPhoneState();

        /* Handsfree carkits expect that +CIND is properly responded to
         Hence we ensure that a proper response is sent
         for the virtual call too.*/
        if (mHeadsetService.isVirtualCallStarted()) {
            call = mStateMachineCallState.mNumActive;
            callSetup = 0;
        } else {
            // regular phone call
            call = mStateMachineCallState.mNumActive;
            callSetup = mStateMachineCallState.mNumHeld;
        }
        if(tempCallState != null &&
            tempCallState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING)
              call_state = HeadsetHalConstants.CALL_STATE_DIALING;
        else
              call_state = mStateMachineCallState.mCallState;
        log("sending call state in CIND resp as " + call_state);

        /* Some Handsfree devices or carkits expect the +CIND to be properly
           responded with the correct service availablity and signal strength,
           while the regular call is active or held or in progress.*/
         if(((!mHeadsetService.isVirtualCallStarted()) &&
            (mStateMachineCallState.mNumActive > 0) || (mStateMachineCallState.mNumHeld > 0) ||
             mStateMachineCallState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING ||
             mStateMachineCallState.mCallState == HeadsetHalConstants.CALL_STATE_DIALING ||
             mStateMachineCallState.mCallState == HeadsetHalConstants.CALL_STATE_INCOMING) &&
            (phoneState.getCindService() == HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE)) {
             log("processAtCind: If regular call is in process/active/held while RD connection " +
                   "during BT-ON, update service availablity and signal strength");
             service = HeadsetHalConstants.NETWORK_STATE_AVAILABLE;
             signal = 3;
         } else {
             service = phoneState.getCindService();
             signal = phoneState.getCindSignal();
        }

        mNativeInterface.cindResponse(device, service, call, callSetup,
                call_state, signal, phoneState.getCindRoam(),
                phoneState.getCindBatteryCharge());
        log("Exit processAtCind()");
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    private void processAtCops(BluetoothDevice device) {
        String operatorName = mSystemInterface.getNetworkOperator();
        if (operatorName == null || operatorName.equals("")) {
            operatorName = "No operator";
        }
        mNativeInterface.copsResponse(device, operatorName);
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    private void processAtClcc(BluetoothDevice device) {
        if (mHeadsetService.isVirtualCallStarted()) {
            // In virtual call, send our phone number instead of remote phone number
            // some carkits cross-check subscriber number( fetched by AT+CNUM) against
            // number sent in clcc and reject sco connection.
            String phoneNumber = VOIP_CALL_NUMBER;
            if (phoneNumber == null) {
                phoneNumber = "";
            }
            int type = PhoneNumberUtils.toaFromString(phoneNumber);
            log(" processAtClcc phonenumber = "+ phoneNumber + " type = " + type);
            // call still in dialling or alerting state
            if (mStateMachineCallState.mNumActive == 0) {
                mNativeInterface.clccResponse(device, 1, 0, mStateMachineCallState.mCallState, 0,
                                              false, phoneNumber, type);
            } else {
                mNativeInterface.clccResponse(device, 1, 0, 0, 0, false, phoneNumber, type);
            }
            mNativeInterface.clccResponse(device, 0, 0, 0, 0, false, "", 0);
        } else if (hasMessages(SEND_CLCC_RESP_AFTER_VOIP_CALL)) {
            Log.w(TAG, "processAtClcc: send OK response as VOIP call ended just now");
            mNativeInterface.clccResponse(device, 0, 0, 0, 0, false, "", 0);
        } else {
            // In Telecom call, ask Telecom to send send remote phone number
            if (!mSystemInterface.listCurrentCalls(ApmConstIntf.AudioProfiles.HFP)) {
                Log.e(TAG, "processAtClcc: failed to list current calls for " + device);
                mNativeInterface.clccResponse(device, 0, 0, 0, 0, false, "", 0);
            } else {
                sendMessageDelayed(CLCC_RSP_TIMEOUT, device, CLCC_RSP_TIMEOUT_MS);
            }
        }
    }

    private void processAtCscs(String atString, int type, BluetoothDevice device) {
        log("processAtCscs - atString = " + atString);
        if (mPhonebook != null) {
            mPhonebook.handleCscsCommand(atString, type, device);
        } else {
            Log.e(TAG, "Phonebook handle null for At+CSCS");
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
        }
    }

    private void processAtCpbs(String atString, int type, BluetoothDevice device) {
        log("processAtCpbs - atString = " + atString);
        if (mPhonebook != null) {
            if (atString.equals("SM") && !mSystemInterface.getHeadsetPhoneState().getIsSimCardLoaded()){
               Log.e(TAG, " SM is not loaded");
               mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
            } else {
              mPhonebook.handleCpbsCommand(atString, type, device);
            }
        } else {
            Log.e(TAG, "Phonebook handle null for At+CPBS");
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
        }
    }

    private void processAtCpbr(String atString, int type, BluetoothDevice device) {
        log("processAtCpbr - atString = " + atString);
        if (mPhonebook != null) {
            mPhonebook.handleCpbrCommand(atString, type, device);
        } else {
            Log.e(TAG, "Phonebook handle null for At+CPBR");
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
        }
    }

    /**
     * Find a character ch, ignoring quoted sections.
     * Return input.length() if not found.
     */
    private static int findChar(char ch, String input, int fromIndex) {
        for (int i = fromIndex; i < input.length(); i++) {
            char c = input.charAt(i);
            if (c == '"') {
                i = input.indexOf('"', i + 1);
                if (i == -1) {
                    return input.length();
                }
            } else if (c == ch) {
                return i;
            }
        }
        return input.length();
    }

    /**
     * Break an argument string into individual arguments (comma delimited).
     * Integer arguments are turned into Integer objects. Otherwise a String
     * object is used.
     */
    private static Object[] generateArgs(String input) {
        int i = 0;
        int j;
        ArrayList<Object> out = new ArrayList<Object>();
        while (i <= input.length()) {
            j = findChar(',', input, i);

            String arg = input.substring(i, j);
            try {
                out.add(new Integer(arg));
            } catch (NumberFormatException e) {
                out.add(arg);
            }

            i = j + 1; // move past comma
        }
        return out.toArray();
    }

    /**
     * Process vendor specific AT commands
     *
     * @param atString AT command after the "AT+" prefix
     * @param device Remote device that has sent this command
     */
    private void processVendorSpecificAt(String atString, BluetoothDevice device) {
        log("processVendorSpecificAt - atString = " + atString);

        // Currently we accept only SET type commands.
        int indexOfEqual = atString.indexOf("=");
        if (indexOfEqual == -1) {
            Log.w(TAG, "processVendorSpecificAt: command type error in " + atString);
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
            return;
        }

        String command = atString.substring(0, indexOfEqual);
        Integer companyId = VENDOR_SPECIFIC_AT_COMMAND_COMPANY_ID.get(command);
        if (companyId == null) {
            Log.i(TAG, "processVendorSpecificAt: unsupported command: " + atString);
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
            return;
        }

        String arg = atString.substring(indexOfEqual + 1);
        if (arg.startsWith("?")) {
            Log.w(TAG, "processVendorSpecificAt: command type error in " + atString);
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
            return;
        }

        Object[] args = generateArgs(arg);
        if (command.equals(BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_XAPL)) {
            processAtXapl(args, device);
        }
        broadcastVendorSpecificEventIntent(command, companyId, BluetoothHeadset.AT_CMD_TYPE_SET,
                args, device);
        mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_OK, 0);
    }

    /**
     * Process AT+XAPL AT command
     *
     * @param args command arguments after the equal sign
     * @param device Remote device that has sent this command
     */
    private void processAtXapl(Object[] args, BluetoothDevice device) {
        if (args.length != 2) {
            Log.w(TAG, "processAtXapl() args length must be 2: " + String.valueOf(args.length));
            return;
        }
        if (!(args[0] instanceof String) || !(args[1] instanceof Integer)) {
            Log.w(TAG, "processAtXapl() argument types not match");
            return;
        }
        String[] deviceInfo = ((String) args[0]).split("-");
        if (deviceInfo.length != 3) {
            Log.w(TAG, "processAtXapl() deviceInfo length " + deviceInfo.length + " is wrong");
            return;
        }
        String vendorId = deviceInfo[0];
        String productId = deviceInfo[1];
        String version = deviceInfo[2];
        /*BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_DEVICE_INFO_REPORTED,
                mAdapterService.obfuscateAddress(device), BluetoothProtoEnums.DEVICE_INFO_INTERNAL,
                BluetoothHeadset.VENDOR_SPECIFIC_HEADSET_EVENT_XAPL, vendorId, productId, version,
                null, 0);*/
        // feature = 2 indicates that we support battery level reporting only
        mNativeInterface.atResponseString(device, "+XAPL=iPhone," + String.valueOf(2));
    }

    private void processUnknownAt(String atString, BluetoothDevice device) {
        if (device == null) {
            Log.w(TAG, "processUnknownAt device is null");
            return;
        }
        log("processUnknownAt - atString = " + atString);
        String atCommand = parseUnknownAt(atString);
        int commandType = getAtCommandType(atCommand);
        if (atCommand.startsWith("+CSCS")) {
            processAtCscs(atCommand.substring(5), commandType, device);
        } else if (atCommand.startsWith("+CPBS")) {
            processAtCpbs(atCommand.substring(5), commandType, device);
        } else if (atCommand.startsWith("+CPBR")) {
            processAtCpbr(atCommand.substring(5), commandType, device);
        } else if (atCommand.startsWith("+CSQ")) {
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
        } else if (atCommand.equals("+CGMI")) {
            mNativeInterface.atResponseString(device, "+CGMI: \"" + Build.MANUFACTURER + "\"");
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_OK, 0);
        } else if (atCommand.equals("+CGMM")) {
            mNativeInterface.atResponseString(device, "+CGMM: " + Build.MODEL);
            mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_OK, 0);
        } else {
            processVendorSpecificAt(atCommand, device);
        }
    }

    // HSP +CKPD command
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    private void processKeyPressed(BluetoothDevice device) {
        if (mSystemInterface.isRinging()) {
            mSystemInterface.answerCall(device, ApmConstIntf.AudioProfiles.HFP);
        } else if (getAudioState() != BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
            if (!mNativeInterface.disconnectAudio(mDevice)) {
                Log.w(TAG, "processKeyPressed, failed to disconnect audio from " + mDevice);
            }
        } else if (mSystemInterface.isInCall()) {
            if (getAudioState() == BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                // Should connect audio as well
                if (mDevice.equals(mHeadsetService.getActiveDevice())) {
                    Log.w(TAG, "processKeyPressed: device "+ mDevice+" is active, create SCO");
                    mNativeInterface.connectAudio(mDevice);
                } else {
                    //Set active device and create SCO
                    if (!mHeadsetService.setActiveDevice(mDevice)) {
                        Log.w(TAG, "processKeyPressed, failed to set active device to "
                              + mDevice);
                    }
                }
            }
        } else {
            // We have already replied OK to this HSP command, no feedback is needed
            if (mHeadsetService.hasDeviceInitiatedDialingOut()) {
                Log.w(TAG, "processKeyPressed, already dialling");
                return;
            }
            String dialNumber = mPhonebook.getLastDialledNumber();
            if (dialNumber == null) {
                Log.w(TAG, "processKeyPressed, last dial number null");
                return;
            }
            if (!mHeadsetService.dialOutgoingCall(mDevice, dialNumber)) {
                Log.w(TAG, "processKeyPressed, failed to call in service");
                return;
            }
        }
    }

    /**
     * Send HF indicator value changed intent
     *
     * @param device Device whose HF indicator value has changed
     * @param indId Indicator ID [0-65535]
     * @param indValue Indicator Value [0-65535], -1 means invalid but indId is supported
     */
    private void sendIndicatorIntent(BluetoothDevice device, int indId, int indValue) {
        Intent intent = new Intent(BluetoothHeadset.ACTION_HF_INDICATORS_VALUE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothHeadset.EXTRA_HF_INDICATORS_IND_ID, indId);
        intent.putExtra(BluetoothHeadset.EXTRA_HF_INDICATORS_IND_VALUE, indValue);

        mHeadsetService.sendBroadcast(intent, BLUETOOTH_CONNECT,
                Utils.getTempAllowlistBroadcastOptions());
    }

    private void processAtBind(String atString, BluetoothDevice device) {
        log("processAtBind: " + atString);

        for (String id : atString.split(",")) {

            int indId;

            try {
                indId = Integer.parseInt(id);
            } catch (NumberFormatException e) {
                Log.e(TAG, Log.getStackTraceString(new Throwable()));
                continue;
            }

            switch (indId) {
                case HeadsetHalConstants.HF_INDICATOR_ENHANCED_DRIVER_SAFETY:
                    log("Send Broadcast intent for the Enhanced Driver Safety indicator.");
                    sendIndicatorIntent(device, indId, -1);
                    break;
                case HeadsetHalConstants.HF_INDICATOR_BATTERY_LEVEL_STATUS:
                    log("Send Broadcast intent for the Battery Level indicator.");
                    sendIndicatorIntent(device, indId, -1);
                    break;
                default:
                    log("Invalid HF Indicator Received");
                    break;
            }
        }
    }

   /**
     * Send TWSP Battery State changed intent
     *
     * @param device Device whose Battery State Changed
     * @param batteryState Charging/Discharging [0/1]
     * @param batteryLevel battery percentage, -1 means invalid
     */
    private void sendTwsBatteryStateIntent(BluetoothDevice device,
                                         int batteryState, int batteryLevel) {
        Intent intent = new Intent(
                        BluetoothHeadset.ACTION_HF_TWSP_BATTERY_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothHeadset.EXTRA_HF_TWSP_BATTERY_STATE,
                    batteryState);
        intent.putExtra(BluetoothHeadset.EXTRA_HF_TWSP_BATTERY_LEVEL,
                    batteryLevel);
        mHeadsetService.sendBroadcast(intent, BLUETOOTH_CONNECT);
    }

    private void processTwsBatteryState(String atString, BluetoothDevice device) {
        log("processTwsBatteryState: " + atString);
        int batteryState;
        int batteryLevel;

        String parts[] =  atString.split(",");
        if (parts.length != 2) {
            Log.e(TAG, "Invalid battery status event");
            return;
        }

        try {
            batteryState = Integer.parseInt(parts[0]);
        } catch (NumberFormatException e) {
            Log.e(TAG, Log.getStackTraceString(new Throwable()));
            return;
        }

        try {
            batteryLevel = Integer.parseInt(parts[1]);
        } catch (NumberFormatException e) {
            Log.e(TAG, Log.getStackTraceString(new Throwable()));
            return;
        }

        log("processTwsBatteryState: batteryState:" + batteryState
                                      + "batteryLevel:" + batteryLevel);
        sendTwsBatteryStateIntent(device, batteryState, batteryLevel);
    }

    private void processAtBiev(int indId, int indValue, BluetoothDevice device) {
        log("processAtBiev: ind_id=" + indId + ", ind_value=" + indValue);
        sendIndicatorIntent(device, indId, indValue);
    }

    private void processCpbr(Intent intent)
    {
        int atCommandResult = 0;
        int atCommandErrorCode = 0;
        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        log("Enter processCpbr()");
        // ASSERT: (headset != null) && headSet.isConnected()
        // REASON: mCheckingAccessPermission is true, otherwise resetAtState
        // has set mCheckingAccessPermission to false
        if (intent.getAction().equals(BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY)) {
            if (intent.getIntExtra(BluetoothDevice.EXTRA_CONNECTION_ACCESS_RESULT,
                                   BluetoothDevice.CONNECTION_ACCESS_NO)
                    == BluetoothDevice.CONNECTION_ACCESS_YES) {
                if (intent.getBooleanExtra(BluetoothDevice.EXTRA_ALWAYS_ALLOWED, false)) {
                    mDevice.setPhonebookAccessPermission(BluetoothDevice.ACCESS_ALLOWED);
                }
                atCommandResult = mPhonebook.processCpbrCommand(device);
            } else {
                if (intent.getBooleanExtra(BluetoothDevice.EXTRA_ALWAYS_ALLOWED, false)) {
                    mDevice.setPhonebookAccessPermission(
                            BluetoothDevice.ACCESS_REJECTED);
                }
            }
        }
        mPhonebook.setCpbrIndex(-1);
        mPhonebook.setCheckingAccessPermission(false);

        if (atCommandResult >= 0) {
            mNativeInterface.atResponseCode(device, atCommandResult, atCommandErrorCode);
        } else {
            log("processCpbr - RESULT_NONE");
        }
        Log.d(TAG, "Exit processCpbr()");
    }

    private void processSendClccResponse(HeadsetClccResponse clcc) {
        if (!hasMessages(CLCC_RSP_TIMEOUT)) {
            return;
        }
        if (clcc.mIndex == 0) {
            removeMessages(CLCC_RSP_TIMEOUT);
        }
        // get the top of the Q
        HeadsetCallState tempCallState = mDelayedCSCallStates.peek();

        /* Send call state DIALING if call alerting update is still in the Q */
        if (clcc.mStatus == HeadsetHalConstants.CALL_STATE_ALERTING &&
            tempCallState != null &&
            tempCallState.mCallState == HeadsetHalConstants.CALL_STATE_ALERTING) {
            log("sending call status as DIALING");
            mNativeInterface.clccResponse(mDevice, clcc.mIndex, clcc.mDirection,
                                          HeadsetHalConstants.CALL_STATE_DIALING,
                                   clcc.mMode, clcc.mMpty, clcc.mNumber, clcc.mType);
        } else {
            log("sending call status as " + clcc.mStatus);
            mNativeInterface.clccResponse(mDevice, clcc.mIndex, clcc.mDirection,
                                          clcc.mStatus,
                                   clcc.mMode, clcc.mMpty, clcc.mNumber, clcc.mType);
        }
        log("Exit processSendClccResponse()");
    }

    private void processSendVendorSpecificResultCode(HeadsetVendorSpecificResultCode resultCode) {
        String stringToSend = resultCode.mCommand + ": ";
        if (resultCode.mArg != null) {
            stringToSend += resultCode.mArg;
        }
        mNativeInterface.atResponseString(resultCode.mDevice, stringToSend);
    }

    private String getCurrentDeviceName() {
        String deviceName = mAdapterService.getRemoteName(mDevice);
        if (deviceName == null) {
            return "<unknown>";
        }
        return deviceName;
    }

    private void updateAgIndicatorEnableState(
            HeadsetAgIndicatorEnableState agIndicatorEnableState) {
        if (!mDeviceSilenced
                && Objects.equals(mAgIndicatorEnableState, agIndicatorEnableState)) {
            Log.i(TAG, "updateAgIndicatorEnableState, no change in indicator state "
                    + mAgIndicatorEnableState);
            return;
        }
        mAgIndicatorEnableState = agIndicatorEnableState;
        int events = PhoneStateListener.LISTEN_NONE;
        if (mAgIndicatorEnableState != null && mAgIndicatorEnableState.service) {
            events |= PhoneStateListener.LISTEN_SERVICE_STATE;
        }
        if (mAgIndicatorEnableState != null && mAgIndicatorEnableState.signal) {
            events |= PhoneStateListener.LISTEN_SIGNAL_STRENGTHS;
        }
        mSystemInterface.getHeadsetPhoneState().listenForPhoneState(mDevice, events);
    }

    boolean isConnectedDeviceBlacklistedforIncomingCall() {
        boolean matched = InteropUtil.interopMatchAddrOrName(
            InteropUtil.InteropFeature.INTEROP_HFP_FAKE_INCOMING_CALL_INDICATOR,
            mDevice.getAddress());

        return matched;
    }

    boolean isConnectedDeviceBlacklistedforRetrySco() {
       boolean matched = InteropUtil.interopMatchAddrOrName(
           InteropUtil.InteropFeature.INTEROP_RETRY_SCO_AFTER_REMOTE_REJECT_SCO,
           mDevice.getAddress());
       return matched;
    }

    boolean isDeviceBlacklistedForSendingCallIndsBackToBack() {
        boolean matched = InteropUtil.interopMatchAddrOrName(
            InteropUtil.InteropFeature.INTEROP_HFP_SEND_CALL_INDICATORS_BACK_TO_BACK,
            mDevice.getAddress());

        return matched;
    }

    boolean isSCONeededImmediatelyAfterSLC() {
        boolean matched = InteropUtil.interopMatchAddrOrName(
            InteropUtil.InteropFeature.INTEROP_SETUP_SCO_WITH_NO_DELAY_AFTER_SLC_DURING_CALL,
            mDevice.getAddress());

        return matched;
    }

    private void sendVoipConnectivityNetworktype(boolean isVoipStarted) {
        Log.d(TAG, "Enter sendVoipConnectivityNetworktype()");
        Network network = mConnectivityManager.getActiveNetwork();
        if (network == null) {
            Log.d(TAG, "No default network is currently active");
            return;
        }
        NetworkCapabilities networkCapabilities =
                                   mConnectivityManager.getNetworkCapabilities(network);
        if (mIsAvailable == false) {
            Log.d(TAG, "No connected/available connectivity network, don't update soc");
            return;
        }
        if (networkCapabilities == null) {
            Log.d(TAG, "The capabilities of active network is NULL");
            return;
        }
        if (networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) {
            Log.d(TAG, "Voip/VoLTE started/stopped on n/w TRANSPORT_CELLULAR, don't update to soc");
        } else if (networkCapabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
            Log.d(TAG, "Voip/VoLTE started/stopped on n/w TRANSPORT_WIFI, "+
                       "update n/w type & start/stop to soc");
            mAdapterService.voipNetworkWifiInfo(isVoipStarted, true);
        } else {
            Log.d(TAG, "Voip/VoLTE started/stopped on some other n/w, don't update to soc");
        }
        Log.d(TAG, "Exit sendVoipConnectivityNetworktype()");
    }

    @Override
    protected void log(String msg) {
        if (DBG) {
            super.log(msg);
        }
    }

    @Override
    protected String getLogRecString(Message msg) {
        StringBuilder builder = new StringBuilder();
        builder.append(getMessageName(msg.what));
        builder.append(": ");
        builder.append("arg1=")
                .append(msg.arg1)
                .append(", arg2=")
                .append(msg.arg2)
                .append(", obj=");
        if (msg.obj instanceof HeadsetMessageObject) {
            HeadsetMessageObject object = (HeadsetMessageObject) msg.obj;
            object.buildString(builder);
        } else {
            builder.append(msg.obj);
        }
        return builder.toString();
    }

    private void handleAccessPermissionResult(Intent intent) {
        log("Enter handleAccessPermissionResult");
        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        if (mPhonebook != null) {
            if (!mPhonebook.getCheckingAccessPermission()) {
                return;
            }

            Message m = obtainMessage(PROCESS_CPBR);
            m.obj = intent;
            sendMessage(m);
        } else {
            Log.e(TAG, "Phonebook handle null");
            if (device != null) {
                mNativeInterface.atResponseCode(device, HeadsetHalConstants.AT_RESPONSE_ERROR, 0);
            }
        }
        log("Exit handleAccessPermissionResult()");
    }

    private static int getConnectionStateFromAudioState(int audioState) {
        switch (audioState) {
            case BluetoothHeadset.STATE_AUDIO_CONNECTED:
                return BluetoothAdapter.STATE_CONNECTED;
            case BluetoothHeadset.STATE_AUDIO_CONNECTING:
                return BluetoothAdapter.STATE_CONNECTING;
            case BluetoothHeadset.STATE_AUDIO_DISCONNECTED:
                return BluetoothAdapter.STATE_DISCONNECTED;
        }
        return BluetoothAdapter.STATE_DISCONNECTED;
    }

    private static String getMessageName(int what) {
        switch (what) {
            case CONNECT:
                return "CONNECT";
            case DISCONNECT:
                return "DISCONNECT";
            case CONNECT_AUDIO:
                return "CONNECT_AUDIO";
            case DISCONNECT_AUDIO:
                return "DISCONNECT_AUDIO";
            case VOICE_RECOGNITION_START:
                return "VOICE_RECOGNITION_START";
            case VOICE_RECOGNITION_STOP:
                return "VOICE_RECOGNITION_STOP";
            case INTENT_SCO_VOLUME_CHANGED:
                return "INTENT_SCO_VOLUME_CHANGED";
            case INTENT_CONNECTION_ACCESS_REPLY:
                return "INTENT_CONNECTION_ACCESS_REPLY";
            case CALL_STATE_CHANGED:
                return "CALL_STATE_CHANGED";
            case DEVICE_STATE_CHANGED:
                return "DEVICE_STATE_CHANGED";
            case SEND_CCLC_RESPONSE:
                return "SEND_CCLC_RESPONSE";
            case SEND_VENDOR_SPECIFIC_RESULT_CODE:
                return "SEND_VENDOR_SPECIFIC_RESULT_CODE";
            case STACK_EVENT:
                return "STACK_EVENT";
            case VOICE_RECOGNITION_RESULT:
                return "VOICE_RECOGNITION_RESULT";
            case DIALING_OUT_RESULT:
                return "DIALING_OUT_RESULT";
            case CLCC_RSP_TIMEOUT:
                return "CLCC_RSP_TIMEOUT";
            case CONNECT_TIMEOUT:
                return "CONNECT_TIMEOUT";
            case AUDIO_SERVER_UP:
                return "AUDIO_SERVER_UP";
            default:
                return "UNKNOWN(" + what + ")";
        }
    }
}
