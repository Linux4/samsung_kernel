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
import static android.Manifest.permission.MODIFY_PHONE_STATE;
import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;

import android.annotation.Nullable;
import android.annotation.RequiresPermission;
import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothDevice;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.IBluetoothHeadset;
import android.content.AttributionSource;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.HandlerThread;
import android.os.IDeviceIdleController;
import android.os.Looper;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.telecom.PhoneAccount;
import android.util.Log;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.BluetoothStatsLog;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.MetricsLogger;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.internal.annotations.VisibleForTesting;
import com.android.bluetooth.apm.DeviceProfileMapIntf;
import com.android.bluetooth.apm.ApmConstIntf;
import com.android.bluetooth.apm.ApmConst;
import com.android.bluetooth.apm.CallAudioIntf;
import com.android.bluetooth.apm.CallControlIntf;
import com.android.bluetooth.apm.ActiveDeviceManagerServiceIntf;
import com.android.modules.utils.SynchronousResultReceiver;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.concurrent.Executor;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;
import android.telecom.TelecomManager;


/**
 * Provides Bluetooth Headset and Handsfree profile, as a service in the Bluetooth application.
 *
 * Three modes for SCO audio:
 * Mode 1: Telecom call through {@link #phoneStateChanged(int, int, int, String, int, String,
 *         boolean)}
 * Mode 2: Virtual call through {@link #startScoUsingVirtualVoiceCall()}
 * Mode 3: Voice recognition through {@link #startVoiceRecognition(BluetoothDevice)}
 *
 * When one mode is active, other mode cannot be started. API user has to terminate existing modes
 * using the correct API or just {@link #disconnectAudio()} if user is a system service, before
 * starting a new mode.
 *
 * {@link #connectAudio()} will start SCO audio at one of the above modes, but won't change mode
 * {@link #disconnectAudio()} can happen in any mode to disconnect SCO
 *
 * When audio is disconnected, only Mode 1 Telecom call will be persisted, both Mode 2 virtual call
 * and Mode 3 voice call will be terminated upon SCO termination and client has to restart the mode.
 *
 * NOTE: SCO termination can either be initiated on the AG side or the HF side
 * TODO(b/79660380): As a workaround, voice recognition will be terminated if virtual call or
 * Telecom call is initiated while voice recognition is ongoing, in case calling app did not call
 * {@link #stopVoiceRecognition(BluetoothDevice)}
 *
 * AG - Audio Gateway, device running this {@link HeadsetService}, e.g. Android Phone
 * HF - Handsfree device, device running headset client, e.g. Wireless headphones or car kits
 */
public class HeadsetService extends ProfileService {
    private static final String TAG = "HeadsetService";
    private static final boolean DBG = true;
    private static final String DISABLE_INBAND_RINGING_PROPERTY =
            "persist.bluetooth.disableinbandringing";
    private static final ParcelUuid[] HEADSET_UUIDS = {BluetoothUuid.HSP, BluetoothUuid.HFP};
    private static final int[] CONNECTING_CONNECTED_STATES =
            {BluetoothProfile.STATE_CONNECTING, BluetoothProfile.STATE_CONNECTED};
    private static final int[] AUDIO_DISCONNECTING_AND_AUDIO_CONNECTED_STATES =
            {BluetoothHeadset.STATE_AUDIO_DISCONNECTING, BluetoothHeadset.STATE_AUDIO_CONNECTED, BluetoothHeadset.STATE_AUDIO_CONNECTING};
    private static final int DIALING_OUT_TIMEOUT_MS = 10000;

    private int mMaxHeadsetConnections = 1;
    private int mSetMaxConfig;
    private BluetoothDevice mActiveDevice;
    private BluetoothDevice mTempActiveDevice;
    private boolean mSHOStatus = false;
    private AdapterService mAdapterService;
    private DatabaseManager mDatabaseManager;
    private HandlerThread mStateMachinesThread;
    // This is also used as a lock for shared data in HeadsetService
    private final HashMap<BluetoothDevice, HeadsetStateMachine> mStateMachines = new HashMap<>();
    private HeadsetNativeInterface mNativeInterface;
    private static HeadsetSystemInterface mSystemInterface;
    private HeadsetA2dpSync mHfpA2dpSyncInterface;
    private boolean mAudioRouteAllowed = true;
    // Indicates whether SCO audio needs to be forced to open regardless ANY OTHER restrictions
    private boolean mForceScoAudio;
    private boolean mInbandRingingRuntimeDisable;
    private boolean mVirtualCallStarted;
    // Non null value indicates a pending dialing out event is going on
    private DialingOutTimeoutEvent mDialingOutTimeoutEvent;
    private boolean mVoiceRecognitionStarted;
    // Non null value indicates a pending voice recognition request from headset is going on
    private VoiceRecognitionTimeoutEvent mVoiceRecognitionTimeoutEvent;
    // Timeout when voice recognition is started by remote device
    @VisibleForTesting static int sStartVrTimeoutMs = 5000;
    private boolean mStarted;
    private boolean mCreated;
    private static HeadsetService sHeadsetService;
    private boolean mDisconnectAll;
    private boolean mIsTwsPlusEnabled = false;
    private boolean mIsTwsPlusShoEnabled = false;
    private vendorhfservice  mVendorHf;
    private Context mContext = null;
    private AudioServerStateCallback mServerStateCallback = new AudioServerStateCallback();
    private static final int AUDIO_CONNECTION_DELAY_DEFAULT = 100;

    @Override
    public IProfileServiceBinder initBinder() {
        return new BluetoothHeadsetBinder(this);
    }

    @Override
    protected void create() {
        Log.i(TAG, "create()");
        if (mCreated) {
            throw new IllegalStateException("create() called twice");
        }
        mCreated = true;
        mVendorHf = new vendorhfservice(this);
    }

    @Override
    protected boolean start() {
        Log.i(TAG, "start()");
        if (sHeadsetService != null) {
            Log.w(TAG, "HeadsetService is already running");
            return true;
        }
        // Step 1: Get AdapterService and DatabaseManager, should never be null
        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService(),
                "AdapterService cannot be null when HeadsetService starts");
        mDatabaseManager = Objects.requireNonNull(mAdapterService.getDatabase(),
                "DatabaseManager cannot be null when HeadsetService starts");
        // Step 2: Start handler thread for state machines
        mStateMachinesThread = new HandlerThread("HeadsetService.StateMachines");
        mStateMachinesThread.start();
        // Step 3: Initialize system interface
        mSystemInterface = HeadsetObjectsFactory.getInstance().makeSystemInterface(this);
        // Step 4: Initialize native interface
        mSetMaxConfig = mMaxHeadsetConnections = mAdapterService.getMaxConnectedAudioDevices();
        if(mAdapterService.isVendorIntfEnabled()) {
            String twsPlusEnabled = SystemProperties.get("persist.vendor.btstack.enable.twsplus");
            String twsPlusShoEnabled =
               SystemProperties.get("persist.vendor.btstack.enable.twsplussho");

            if (!twsPlusEnabled.isEmpty() && "true".equals(twsPlusEnabled)) {
                mIsTwsPlusEnabled = true;
            }
            if (!twsPlusShoEnabled.isEmpty() && "true".equals(twsPlusShoEnabled)) {
                if (mIsTwsPlusEnabled) {
                    mIsTwsPlusShoEnabled = true;
                } else {
                    Log.e(TAG, "no TWS+ SHO without TWS+ support!");
                    mIsTwsPlusShoEnabled = false;
                }
            }
            Log.i(TAG, "mIsTwsPlusEnabled: " + mIsTwsPlusEnabled);
            Log.i(TAG, "mIsTwsPlusShoEnabled: " + mIsTwsPlusShoEnabled);
            if (mIsTwsPlusEnabled && mMaxHeadsetConnections < 2){
               //set MaxConn to 2 if TWSPLUS enabled
               mMaxHeadsetConnections = 2;
            }
            if (mIsTwsPlusShoEnabled &&  mMaxHeadsetConnections < 3) {
               //set MaxConn to 3 if TWSPLUS enabled
               mMaxHeadsetConnections = 3;
            }
            //Only if the User set config 1 and TWS+ is enabled leaves
            //these maxConn at 2 and setMaxConfig to 1. this is to avoid
            //connecting to more than 1 legacy device even though max conns
            //is set 2 or 3because of TWS+ requirement
            Log.d(TAG, "Max_HFP_Connections  " + mMaxHeadsetConnections);
            Log.d(TAG, "mSetMaxConfig  " + mSetMaxConfig);
        }

        mNativeInterface = HeadsetObjectsFactory.getInstance().getNativeInterface();
        // Add 1 to allow a pending device to be connecting or disconnecting
        mNativeInterface.init(mMaxHeadsetConnections + 1, isInbandRingingEnabled());
        // Step 5: Check if state machine table is empty, crash if not
        if (mStateMachines.size() > 0) {
            throw new IllegalStateException(
                    "start(): mStateMachines is not empty, " + mStateMachines.size()
                            + " is already created. Was stop() called properly?");
        }
        // Step 6: Setup broadcast receivers
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_BATTERY_CHANGED);
        filter.addAction(AudioManager.VOLUME_CHANGED_ACTION);
        filter.addAction(BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY);
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        filter.addAction(BluetoothA2dp.ACTION_PLAYING_STATE_CHANGED);
        filter.addAction(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED);
        registerReceiver(mHeadsetReceiver, filter);
        // Step 7: Mark service as started

        setHeadsetService(this);
        mStarted = true;

        mHfpA2dpSyncInterface = new HeadsetA2dpSync(mSystemInterface, this);
        if (mVendorHf != null) {
            mVendorHf.init();
            mVendorHf.enableSwb(isSwbEnabled());
        }

        Log.d(TAG, "registering audio server state callback");
        mContext = getApplicationContext();
        Executor exec = mContext.getMainExecutor();
        mSystemInterface.getAudioManager().setAudioServerStateCallback(exec, mServerStateCallback);

        Log.i(TAG, " HeadsetService Started ");
        return true;
    }

    @Override
    protected boolean stop() {
        Log.i(TAG, "stop()");
        if (!mStarted) {
            Log.w(TAG, "stop() called before start()");
            // Still return true because it is considered "stopped" and doesn't have any functional
            // impact on the user
            return true;
        }
        // Step 7: Mark service as stopped
        mStarted = false;
        setHeadsetService(null);
        // Step 6: Tear down broadcast receivers
        unregisterReceiver(mHeadsetReceiver);
        synchronized (mStateMachines) {
            // Reset active device to null
            mActiveDevice = null;
            mInbandRingingRuntimeDisable = false;
            mForceScoAudio = false;
            mAudioRouteAllowed = true;
            if(mAdapterService.isVendorIntfEnabled()) {
                //to enable TWS
                if (mIsTwsPlusEnabled) {
                    mMaxHeadsetConnections = 2;
                } else {
                   mMaxHeadsetConnections = 1;
                }
            } else {
                mMaxHeadsetConnections = 1;
            }
            mVoiceRecognitionStarted = false;
            mVirtualCallStarted = false;
            if (mDialingOutTimeoutEvent != null) {
                if (mStateMachinesThread != null) {
                    mStateMachinesThread.getThreadHandler()
                      .removeCallbacks(mDialingOutTimeoutEvent);
                }
                mDialingOutTimeoutEvent = null;
            }
            if (mVoiceRecognitionTimeoutEvent != null) {
                if (mStateMachinesThread != null) {
                    mStateMachinesThread.getThreadHandler()
                        .removeCallbacks(mVoiceRecognitionTimeoutEvent);
                }
                mVoiceRecognitionTimeoutEvent = null;
                if (mSystemInterface.getVoiceRecognitionWakeLock().isHeld()) {
                    mSystemInterface.getVoiceRecognitionWakeLock().release();
                }
            }
            // Step 5: Destroy state machines
            for (HeadsetStateMachine stateMachine : mStateMachines.values()) {
                HeadsetObjectsFactory.getInstance().destroyStateMachine(stateMachine);
            }
            mStateMachines.clear();
        }
        // Reset A2DP suspend flag if bluetooth is turned off while call is already in progress
        Log.d(TAG,"Release A2DP during BT off");
        mHfpA2dpSyncInterface.releaseA2DP(null);
        // Step 4: Destroy native interface
        mNativeInterface.cleanup();
        // Step 3: Destroy system interface
        mSystemInterface.stop();
        synchronized (mStateMachines) {
            // Step 2: Stop handler thread
            if (mStateMachinesThread != null) {
                mStateMachinesThread.quitSafely();
            }
            mStateMachinesThread = null;
            // Step 1: Clear
            mAdapterService = null;
        }

        return true;
    }

    @Override
    protected void cleanup() {
        Log.i(TAG, "cleanup");
        if (!mCreated) {
            Log.w(TAG, "cleanup() called before create()");
        }
        if (mVendorHf != null) {
            mVendorHf.cleanup();
        }
        mCreated = false;
    }

    /**
     * Checks if this service object is able to accept binder calls
     *
     * @return True if the object can accept binder calls, False otherwise
     */
    public boolean isAlive() {
        return isAvailable() && mCreated && mStarted;
    }

    /**
     * Get the {@link Looper} for the state machine thread. This is used in testing and helper
     * objects
     *
     * @return {@link Looper} for the state machine thread
     */
    @VisibleForTesting
    public Looper getStateMachinesThreadLooper() {
        if (mStateMachinesThread != null) {
            return mStateMachinesThread.getLooper();
        }
        return null;
    }

    interface StateMachineTask {
        void execute(HeadsetStateMachine stateMachine);
    }

    // send message to all statemachines in connecting and connected state.
    private void doForEachConnectedConnectingStateMachine(StateMachineTask task) {
        synchronized (mStateMachines) {
            for (BluetoothDevice device :
                         getDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES)) {
                HeadsetStateMachine stateMachine = mStateMachines.get(device);
                if (stateMachine == null) {
                    continue;
                }
                task.execute(stateMachine);
            }
        }
    }

    private boolean doForStateMachine(BluetoothDevice device, StateMachineTask task) {
        synchronized (mStateMachines) {
            HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                return false;
            }
            task.execute(stateMachine);
        }
        return true;
    }

    private void doForEachConnectedStateMachine(StateMachineTask task) {
        synchronized (mStateMachines) {
            for (BluetoothDevice device : getConnectedDevices()) {
                HeadsetStateMachine stateMachine = mStateMachines.get(device);
                if (stateMachine == null) {
                    continue;
                }
                task.execute(stateMachine);
            }
        }
    }

    void onDeviceStateChanged(HeadsetDeviceState deviceState) {

        Log.d(TAG, "onDeviceStateChanged");
        CallControlIntf mCallControl = CallControlIntf.get();
        if (mCallControl != null)
           mCallControl.updateSignalStatus(deviceState.mSignal);
        else
            Log.w(TAG, "mCallControl is null");

        synchronized (mStateMachines) {
            doForEachConnectedStateMachine(
                stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.DEVICE_STATE_CHANGED,
                        deviceState));
        }
    }

    public  static HeadsetSystemInterface getSystemInterfaceObj() {
        return mSystemInterface;
    }

    /**
     * Handle messages from native (JNI) to Java. This needs to be synchronized to avoid posting
     * messages to state machine before start() is done
     *
     * @param stackEvent event from native stack
     */
    void messageFromNative(HeadsetStackEvent stackEvent) {
        Objects.requireNonNull(stackEvent.device,
                "Device should never be null, event: " + stackEvent);
        synchronized (mStateMachines) {
            HeadsetStateMachine stateMachine = mStateMachines.get(stackEvent.device);
            if (stackEvent.type == HeadsetStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED) {
                switch (stackEvent.valueInt) {
                    case HeadsetHalConstants.CONNECTION_STATE_CONNECTED:
                    case HeadsetHalConstants.CONNECTION_STATE_CONNECTING: {
                        // Create new state machine if none is found
                        if (stateMachine == null) {
                            if (mStateMachinesThread != null) {
                                stateMachine = HeadsetObjectsFactory.getInstance()
                                    .makeStateMachine(stackEvent.device,
                                            mStateMachinesThread.getLooper(), this, mAdapterService,
                                            mNativeInterface, mSystemInterface);
                                mStateMachines.put(stackEvent.device, stateMachine);
                            } else {
                                Log.w(TAG, "messageFromNative: mStateMachinesThread is null");
                            }
                        }
                        break;
                    }
                }
            }
            if (stateMachine == null) {
                throw new IllegalStateException(
                        "State machine not found for stack event: " + stackEvent);
            }
            stateMachine.sendMessage(HeadsetStateMachine.STACK_EVENT, stackEvent);
        }
    }

    private class AudioServerStateCallback extends AudioManager.AudioServerStateCallback {
        @Override
        public void onAudioServerDown() {
            Log.d(TAG, "notifying onAudioServerDown");
        }

        @Override
        public void onAudioServerUp() {
            Log.d(TAG, "notifying onAudioServerUp");
            if (isAudioOn()) {
                Log.d(TAG, "onAudioServerUp: Audio is On, Notify HeadsetStateMachine");
                synchronized (mStateMachines) {
                    for (HeadsetStateMachine stateMachine : mStateMachines.values()) {
                        if (stateMachine.getAudioState()
                                == BluetoothHeadset.STATE_AUDIO_CONNECTED) {
                            stateMachine.sendMessage(HeadsetStateMachine.AUDIO_SERVER_UP);
                            break;
                        }
                    }
                }
            }
        }
    }

    private final BroadcastReceiver mHeadsetReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action == null) {
                Log.w(TAG, "mHeadsetReceiver, action is null");
                return;
            }
            switch (action) {
                case Intent.ACTION_BATTERY_CHANGED: {
                    int batteryLevel = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
                    int scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
                    if (batteryLevel < 0 || scale <= 0) {
                        Log.e(TAG, "Bad Battery Changed intent: batteryLevel=" + batteryLevel
                                + ", scale=" + scale);
                        return;
                    }
                    int cindBatteryLevel = Math.round(batteryLevel * 5 / ((float) scale));
                    mSystemInterface.getHeadsetPhoneState().setCindBatteryCharge(cindBatteryLevel);
                    break;
                }
                case AudioManager.VOLUME_CHANGED_ACTION: {
                    int streamType = intent.getIntExtra(AudioManager.EXTRA_VOLUME_STREAM_TYPE, -1);
                    if (streamType == AudioManager.STREAM_BLUETOOTH_SCO) {
                        AdapterService adapterService = AdapterService.getAdapterService();
                        if(!ApmConstIntf.getQtiLeAudioEnabled()) {
                            setIntentScoVolume(intent);
                        }
                    }
                    break;
                }
                case BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY: {
                    int requestType = intent.getIntExtra(BluetoothDevice.EXTRA_ACCESS_REQUEST_TYPE,
                            BluetoothDevice.REQUEST_TYPE_PHONEBOOK_ACCESS);
                    BluetoothDevice device =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    logD("Received BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY, device=" + device
                            + ", type=" + requestType);
                    if (requestType == BluetoothDevice.REQUEST_TYPE_PHONEBOOK_ACCESS) {
                        synchronized (mStateMachines) {
                            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
                            if (stateMachine == null) {
                                Log.wtf(TAG, "Cannot find state machine for " + device);
                                return;
                            }
                            stateMachine.sendMessage(
                                    HeadsetStateMachine.INTENT_CONNECTION_ACCESS_REPLY, intent);
                        }
                    }
                    break;
                }
                case BluetoothDevice.ACTION_BOND_STATE_CHANGED: {
                    int state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                            BluetoothDevice.ERROR);
                    BluetoothDevice device = Objects.requireNonNull(
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE),
                            "ACTION_BOND_STATE_CHANGED with no EXTRA_DEVICE");
                    logD("Bond state changed for device: " + device + " state: " + state);
                    if (state != BluetoothDevice.BOND_NONE) {
                        break;
                    }
                    synchronized (mStateMachines) {
                        HeadsetStateMachine stateMachine = mStateMachines.get(device);
                        if (stateMachine == null) {
                            break;
                        }
                        if (stateMachine.getConnectionState()
                                != BluetoothProfile.STATE_DISCONNECTED) {
                            break;
                        }
                        removeStateMachine(device);
                    }
                    break;
                }
                case BluetoothA2dp.ACTION_PLAYING_STATE_CHANGED: {
                    logD("Received BluetoothA2dp Play State changed");
                    mHfpA2dpSyncInterface.updateA2DPPlayingState(intent);
                    break;
                }
                case BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED: {
                    logD("Received BluetoothA2dp Connection State changed");
                    mHfpA2dpSyncInterface.updateA2DPConnectionState(intent);
                    break;
                }
                default:
                    Log.w(TAG, "Unknown action " + action);
            }
        }
    };

    public void updateBroadcastState(int state) {
        mHfpA2dpSyncInterface.updateBroadcastState(state);
    }
    public void updateConnState(BluetoothDevice device, int newState) {
            CallAudioIntf mCallAudio = CallAudioIntf.get();
            mCallAudio.onConnStateChange(device, newState, ApmConstIntf.AudioProfiles.HFP);
        }

    public void updateAudioState(BluetoothDevice device, int mAudioState) {
            CallAudioIntf mCallAudio = CallAudioIntf.get();
            mCallAudio.onAudioStateChange(device, mAudioState);
    }

    public boolean isVoipLeaWarEnabled() {
        CallAudioIntf mCallAudio = CallAudioIntf.get();
        return mCallAudio.isVoipLeaWarEnabled();
    }

    public void setIntentScoVolume(Intent intent) {
        Log.w(TAG, "setIntentScoVolume");
        synchronized (mStateMachines) {
            doForEachConnectedStateMachine(stateMachine -> stateMachine.sendMessage(
                    HeadsetStateMachine.INTENT_SCO_VOLUME_CHANGED, intent));
        }
    }

    /**
     * Handlers for incoming service calls
     */
    private static class BluetoothHeadsetBinder extends IBluetoothHeadset.Stub
            implements IProfileServiceBinder {
        private volatile HeadsetService mService;

        BluetoothHeadsetBinder(HeadsetService svc) {
            mService = svc;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
        private HeadsetService getService(AttributionSource source) {
            if (!Utils.checkCallerIsSystemOrActiveUser(TAG)
                    || !Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkConnectPermissionForDataDelivery(mService, source, TAG)) {
                return null;
            }
            if (mService == null) {
                Log.w(TAG, "Service is null");
                return null;
            }
            if (!mService.isAlive()) {
                Log.w(TAG, "Service is not alive");
                return null;
            }
            return mService;
        }

        private boolean isAospLeaVoipWarEnabled() {
            boolean ret = false;
            if (ApmConstIntf.getAospLeaEnabled()) {
                CallAudioIntf mCallAudio = CallAudioIntf.get();
                ActiveDeviceManagerServiceIntf mActiveDeviceManager =
                        ActiveDeviceManagerServiceIntf.get();
                if (mCallAudio.isVoipLeaWarEnabled() &&
                        (mActiveDeviceManager.getActiveProfile(ApmConst.AudioFeatures.CALL_AUDIO)
                        == ApmConst.AudioProfiles.BAP_CALL ||
                        mActiveDeviceManager.getActiveProfile(ApmConst.AudioFeatures.CALL_AUDIO)
                        == ApmConst.AudioProfiles.TMAP_CALL)) {
                   ret = true;
                }
            }
            Log.i(TAG, "isAospLeaVoipWarEnabled: " + ret);
            return ret;
        }

        @Override
        public boolean connect(BluetoothDevice device) {
            if (ApmConstIntf.getQtiLeAudioEnabled()) {
                Log.d(TAG, "connect(): Adv Audio enabled");
                CallAudioIntf mCallAudio = CallAudioIntf.get();
                return mCallAudio.connect(device);
            }
            AttributionSource source = Utils.getCallingAttributionSource(mService);
            HeadsetService service = getService(source);
            if (service == null) {
                return false;
            }
            return service.connectHfp(device);
        }

        @Override
        public void connectWithAttribution(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                boolean defaultValue = false;
                if (ApmConstIntf.getQtiLeAudioEnabled()) {
                    Log.d(TAG, "connectWithAttribution(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    if (mCallAudio != null) {
                        defaultValue = mCallAudio.connect(device);
                    }
                    receiver.send(defaultValue);
                } else {
                    Log.w(TAG, "LE Audio not enabled");
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.connect(device);
                    }
                    receiver.send(defaultValue);
                }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public boolean disconnect(BluetoothDevice device) {
            if (ApmConstIntf.getQtiLeAudioEnabled()) {
                Log.d(TAG, "disconnect(): Adv Audio enabled");
                CallAudioIntf mCallAudio = CallAudioIntf.get();
                return mCallAudio.disconnect(device);
            }
            AttributionSource source = Utils.getCallingAttributionSource(mService);
            HeadsetService service = getService(source);
            if (service == null) {
                return false;
            }
            return service.disconnectHfp(device);
        }

        @Override
        public void disconnectWithAttribution(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                boolean defaultValue = false;
                if (ApmConstIntf.getQtiLeAudioEnabled()) {
                    Log.d(TAG, "disconnectWithAttribution(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    if (mCallAudio != null) {
                        defaultValue = mCallAudio.disconnect(device);
                    }
                    receiver.send(defaultValue);
                } else {
                    Log.w(TAG, "LE Audio not enabled");
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.disconnect(device);
                    }
                    receiver.send(defaultValue);
                }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public List<BluetoothDevice> getConnectedDevices() {
            if (ApmConstIntf.getQtiLeAudioEnabled() || isAospLeaVoipWarEnabled()) {
                Log.d(TAG, "getConnectedDevices(): Adv Audio enabled");
                CallAudioIntf mCallAudio = CallAudioIntf.get();
                return mCallAudio.getConnectedDevices();
            }
            AttributionSource source = Utils.getCallingAttributionSource(mService);
            HeadsetService service = getService(source);
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getConnectedDevices();
        }

        @Override
        public void getConnectedDevicesWithAttribution(AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                List<BluetoothDevice> defaultValue = new ArrayList<BluetoothDevice>(0);
                if (ApmConstIntf.getQtiLeAudioEnabled() || isAospLeaVoipWarEnabled()) {
                    Log.d(TAG, "getConnectedDevicesWithAttribution(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    if (mCallAudio != null) {
                        defaultValue = mCallAudio.getConnectedDevices();
                    }
                    receiver.send(defaultValue);
                } else {
                    Log.w(TAG, "LE Audio not enabled");
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.getConnectedDevices();
                    }
                    receiver.send(defaultValue);
                }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getDevicesMatchingConnectionStates(int[] states,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                List<BluetoothDevice> defaultValue = new ArrayList<BluetoothDevice>(0);
                if (ApmConstIntf.getQtiLeAudioEnabled() || isAospLeaVoipWarEnabled()) {
                    Log.d(TAG, "getDevicesMatchingConnectionStates(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    defaultValue = mCallAudio.getDevicesMatchingConnectionStates(states);
                    receiver.send(defaultValue);
                } else {
                    Log.w(TAG, "LE Audio not enabled");
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.getDevicesMatchingConnectionStates(states);
                    }
                    receiver.send(defaultValue);
               }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        public List<BluetoothDevice> getAllDevicesMatchingConnectionStates(int[] states) {
            HeadsetService service = getService(Utils.getCallingAttributionSource());
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getAllDevicesMatchingConnectionStates(states);
        }

        @Override
        public int getConnectionState(BluetoothDevice device) {
            if (ApmConstIntf.getQtiLeAudioEnabled() || isAospLeaVoipWarEnabled()) {
                Log.d(TAG, "getConnectionState(): Adv Audio enabled");
                CallAudioIntf mCallAudio = CallAudioIntf.get();
                return mCallAudio.getConnectionState(device);
            }
            AttributionSource source = Utils.getCallingAttributionSource(mService);
            HeadsetService service = getService(source);
            if (service == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return service.getConnectionState(device);
        }

        @Override
        public void getConnectionStateWithAttribution(BluetoothDevice device,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                int defaultValue = BluetoothProfile.STATE_DISCONNECTED;
                if (ApmConstIntf.getQtiLeAudioEnabled() || isAospLeaVoipWarEnabled()) {
                    Log.d(TAG, "getConnectionStateWithAttribution(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    if (mCallAudio != null) {
                        defaultValue = mCallAudio.getConnectionState(device);
                    }
                    receiver.send(defaultValue);
                } else {
                    Log.w(TAG, "LE Audio not enabled");
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.getConnectionState(device);
                    }
                    receiver.send(defaultValue);
                }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setConnectionPolicy(BluetoothDevice device, int connectionPolicy,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                boolean defaultValue = false;
                if (ApmConstIntf.getQtiLeAudioEnabled()) {
                    Log.d(TAG, "setConnectionPolicy(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    defaultValue =  mCallAudio.setConnectionPolicy(device, connectionPolicy);
                    receiver.send(defaultValue);
                } else {
                    Log.w(TAG, "LE Audio not enabled");
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.setConnectionPolicy(device, connectionPolicy);
                    }
                    receiver.send(defaultValue);
                }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectionPolicy(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                int defaultValue = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
                if (ApmConstIntf.getQtiLeAudioEnabled()) {
                    Log.d(TAG, "getConnectionPolicy Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    defaultValue = mCallAudio.getConnectionPolicy(device);
                    receiver.send(defaultValue);
                } else {
                    Log.w(TAG, "LE Audio not enabled");
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.getConnectionPolicy(device);
                    }
                    receiver.send(defaultValue);
                }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        public int getPriority(BluetoothDevice device, AttributionSource source) {
            if (ApmConstIntf.getQtiLeAudioEnabled()) {
                Log.d(TAG, "getPriority Adv Audio enabled");
                CallAudioIntf mCallAudio = CallAudioIntf.get();
                return mCallAudio.getConnectionPolicy(device);
            }
            HeadsetService service = getService(source);
            if (service == null) {
                return BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
            }
            return service.getConnectionPolicy(device);
        }

        @Override
        public void isNoiseReductionSupported(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HeadsetService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.isNoiseReductionSupported(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void isVoiceRecognitionSupported(BluetoothDevice device,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                HeadsetService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.isVoiceRecognitionSupported(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void startVoiceRecognition(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HeadsetService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.startVoiceRecognition(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void stopVoiceRecognition(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HeadsetService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.stopVoiceRecognition(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void isAudioOn(AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                boolean defaultValue = false;
                if(ApmConstIntf.getQtiLeAudioEnabled() || isAospLeaVoipWarEnabled()) {
                    Log.d(TAG, "isAudioOn(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    defaultValue = mCallAudio.isAudioOn();
                    receiver.send(defaultValue);
                } else {
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.isAudioOn();
                    }
                    receiver.send(defaultValue);
                }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void isAudioConnected(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HeadsetService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.isAudioConnected(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getAudioState(BluetoothDevice device, AttributionSource source,
                  SynchronousResultReceiver receiver) {
            try {
                int defaultValue = BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
                // Not fake getAudioState API for Aosp LeAudio VOIP WAR
                if (ApmConstIntf.getQtiLeAudioEnabled()) {
                     Log.d(TAG, "getAudioState(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    defaultValue = mCallAudio.getAudioState(device);
                    receiver.send(defaultValue);
                } else {
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.getAudioState(device);
                    }
                    receiver.send(defaultValue);
                }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void connectAudio(AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                int defaultValue = BluetoothStatusCodes.ERROR_PROFILE_SERVICE_NOT_BOUND;
                if(ApmConstIntf.getQtiLeAudioEnabled()) {
                     Log.d(TAG, "connectAudio(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    if (mCallAudio != null) {
                        defaultValue = mCallAudio.connectAudio();
                    }
                } else {
                    HeadsetService service = getService(source);
                    if (service != null) {
                        enforceBluetoothPrivilegedPermission(service);
                        defaultValue = service.connectAudio();
                    }
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void disconnectAudio(AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                int defaultValue = BluetoothStatusCodes.ERROR_PROFILE_SERVICE_NOT_BOUND;
                if(ApmConstIntf.getQtiLeAudioEnabled()) {
                     Log.d(TAG, "disconnectAudio(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    if (mCallAudio != null) {
                        defaultValue = mCallAudio.disconnectAudio();
                    }
                } else {
                    HeadsetService service = getService(source);
                    if (service != null) {
                        enforceBluetoothPrivilegedPermission(service);
                        defaultValue = service.disconnectAudio();
                    }
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setAudioRouteAllowed(boolean allowed, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HeadsetService service = getService(source);
                if (service != null) {
                    service.setAudioRouteAllowed(allowed);
                }
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getAudioRouteAllowed(AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HeadsetService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.getAudioRouteAllowed();
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setForceScoAudio(boolean forced, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HeadsetService service = getService(source);
                if (service != null) {
                    service.setForceScoAudio(forced);
                }
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void startScoUsingVirtualVoiceCall(AttributionSource source,
            SynchronousResultReceiver receiver) {
            try {
                boolean defaultValue = false;
                if(ApmConstIntf.getQtiLeAudioEnabled() || isAospLeaVoipWarEnabled()) {
                     Log.d(TAG, "startScoUsingVirtualVoiceCall(): Adv Audio enabled");
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    defaultValue = mCallAudio.startScoUsingVirtualVoiceCall();
                    receiver.send(defaultValue);
                } else {
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.startScoUsingVirtualVoiceCall();
                    }
                    receiver.send(defaultValue);
                }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void stopScoUsingVirtualVoiceCall(AttributionSource source,
            SynchronousResultReceiver receiver) {
            try {
                 boolean defaultValue = false;
                 if(ApmConstIntf.getQtiLeAudioEnabled() || isAospLeaVoipWarEnabled()) {
                      Log.d(TAG, "stopScoUsingVirtualVoiceCall(): Adv Audio enabled");
                     CallAudioIntf mCallAudio = CallAudioIntf.get();
                     defaultValue = mCallAudio.stopScoUsingVirtualVoiceCall();
                     receiver.send(defaultValue);
                 } else {
                     HeadsetService service = getService(source);
                     if (service != null) {
                         defaultValue = service.stopScoUsingVirtualVoiceCall();
                     }
                     receiver.send(defaultValue);
                 }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void phoneStateChanged(int numActive, int numHeld, int callState, String number,
            int type, String name, AttributionSource source) {

            Log.d(TAG, "phoneStateChanged()");
            CallControlIntf mCallControl = CallControlIntf.get();
            if (mCallControl != null)
                mCallControl.phoneStateChanged(numActive, numHeld, callState, number, type, name, false);
            else
                Log.w(TAG, "mCallControl is null");

        }

        @Override
        public void clccResponse(int index, int direction, int status, int mode, boolean mpty,
                String number, int type, AttributionSource source,
                SynchronousResultReceiver receiver) {
            Log.d(TAG, "clccResponse()");
            try {
              CallControlIntf mCallControl = CallControlIntf.get();
              if (mCallControl != null)
                 mCallControl.clccResponse(index, direction, status, mode, mpty, number, type);
              else
                  Log.w(TAG, "mCallControl is null");
              receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void sendVendorSpecificResultCode(BluetoothDevice device, String command,
                String arg, AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                HeadsetService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.sendVendorSpecificResultCode(device, command, arg);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setActiveDevice(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                boolean defaultValue = false;
                if(ApmConstIntf.getQtiLeAudioEnabled()) {
                    ActiveDeviceManagerServiceIntf activeDeviceManager = ActiveDeviceManagerServiceIntf.get();
                    defaultValue = activeDeviceManager.setActiveDevice(device,
                         ApmConstIntf.AudioFeatures.CALL_AUDIO, true);
                    receiver.send(defaultValue);
                } else {
                    HeadsetService service = getService(source);
                    if (service != null) {
                        defaultValue = service.setActiveDevice(device);
                    }
                    receiver.send(defaultValue);
                }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getActiveDevice(AttributionSource source, SynchronousResultReceiver receiver) {
            try {
               BluetoothDevice defaultValue = null;
               if(ApmConstIntf.getQtiLeAudioEnabled() || isAospLeaVoipWarEnabled()) {
                   ActiveDeviceManagerServiceIntf activeDeviceManager = ActiveDeviceManagerServiceIntf.get();
                   defaultValue = activeDeviceManager.getActiveAbsoluteDevice(ApmConstIntf.AudioFeatures.CALL_AUDIO);
                   receiver.send(defaultValue);
               } else {
                   HeadsetService service = getService(source);
                   if (service != null) {
                       defaultValue = service.getActiveDevice();
                   }
                   receiver.send(defaultValue);
               }
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void isInbandRingingEnabled(AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HeadsetService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.isInbandRingingEnabled();
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void phoneStateChangedDsDa(int numActive, int numHeld, int callState, String number,
                int type, String name, AttributionSource source) {
            if (mService == null || !mService.isAlive()) {
                Log.w(TAG, "mService is unavailable: " + mService);
                return;
            }
            mService.phoneStateChanged(numActive, numHeld, callState, number, type, name, false);
        }

        @Override
        public void clccResponseDsDa(int index, int direction, int status, int mode, boolean mpty,
                String number, int type, AttributionSource source) {
            if (mService == null || !mService.isAlive()) {
                Log.w(TAG, "mService is unavailable: " + mService);
                return;
            }
            mService.clccResponse(index, direction, status, mode, mpty, number, type);
        }
    }

    // API methods
    public static synchronized HeadsetService getHeadsetService() {
        if (sHeadsetService == null) {
            Log.w(TAG, "getHeadsetService(): service is NULL");
            return null;
        }
        if (!sHeadsetService.isAvailable()) {
            Log.w(TAG, "getHeadsetService(): service is not available");
            return null;
        }
        logD("getHeadsetService(): returning " + sHeadsetService);
        return sHeadsetService;
    }

    private static synchronized void setHeadsetService(HeadsetService instance) {
        logD("setHeadsetService(): set to: " + instance);
        sHeadsetService = instance;
    }

    public BluetoothDevice getTwsPlusConnectedPeer(BluetoothDevice device) {
        AdapterService adapterService = AdapterService.getAdapterService();
        if (!adapterService.isTwsPlusDevice(device)) {
            logD("getTwsPlusConnectedPeer: Not a TWS+ device");
            return null;
        }
        List<BluetoothDevice> connDevices = getConnectedDevices();

        int size = connDevices.size();
        for(int i = 0; i < size; i++) {
            BluetoothDevice ConnectedDevice = connDevices.get(i);
            if (adapterService.getTwsPlusPeerAddress(device).equals(ConnectedDevice.getAddress())) {
                return ConnectedDevice;
            }
        }
        return null;
    }

    private BluetoothDevice getConnectedOrConnectingTwspDevice() {
        List<BluetoothDevice> connDevices =
            getAllDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
        int size = connDevices.size();
        for(int i = 0; i < size; i++) {
            BluetoothDevice ConnectedDevice = connDevices.get(i);
            if (mAdapterService.isTwsPlusDevice(ConnectedDevice)) {
                logD("getConnectedorConnectingTwspDevice: found" + ConnectedDevice);
                return ConnectedDevice;
            }
        }
        return null;
    }

    /*
     * This function determines possible connections allowed with both Legacy
     * TWS+ earbuds.
     * N is the maximum audio connections set (defaults to 5)
     * In TWS+ connections
     *    - There can be only ONE set of TWS+ earbud connected at any point
     *      of time
     *    - Once one of the TWS+ earbud is connected, another slot will be
     *      reserved for the TWS+ peer earbud. Hence there can be maximum
     *      of N-2 legacy device connections when an earbud is connected.
     *    - If user wants to connect to another TWS+ earbud set. Existing TWS+
     *      connection need to be removed explicitly
     * In Legacy(Non-TWS+) Connections
     *    -Maximum allowed connections N set by user is used to determine number
     * of Legacy connections
     */
    private boolean isConnectionAllowed(BluetoothDevice device,
                                           List<BluetoothDevice> connDevices
                                           ) {
        AdapterService adapterService = AdapterService.getAdapterService();
        boolean allowSecondHfConnection = false;
        int reservedSlotForTwspPeer = 0;

        if (!mIsTwsPlusEnabled && adapterService.isTwsPlusDevice(device)) {
           logD("No TWSPLUS connections as It is not Enabled");
           return false;
        }

        if (connDevices.size() == 0) {
            allowSecondHfConnection = true;
        } else {
            BluetoothDevice connectedOrConnectingTwspDev =
                    getConnectedOrConnectingTwspDevice();
            if (connectedOrConnectingTwspDev != null) {
                // There is TWSP connected earbud
                if (adapterService.isTwsPlusDevice(device)) {
                   if (adapterService.getTwsPlusPeerAddress
                           (device).equals(
                             connectedOrConnectingTwspDev.getAddress())) {
                       //Allow connection only if the outgoing
                       //is peer of TWS connected earbud
                       allowSecondHfConnection = true;
                   } else {
                       allowSecondHfConnection = false;
                   }
                } else {
                   reservedSlotForTwspPeer = 0;
                   if (getTwsPlusConnectedPeer(
                                connectedOrConnectingTwspDev) == null) {
                       //Peer of Connected Tws+ device is not Connected
                       //yet, reserve one slot
                       reservedSlotForTwspPeer = 1;
                   }
                   if (connDevices.size() <
                          (mMaxHeadsetConnections - reservedSlotForTwspPeer)
                          && mIsTwsPlusShoEnabled) {
                       allowSecondHfConnection = true;
                   } else {
                           allowSecondHfConnection = false;
                           if (!mIsTwsPlusShoEnabled) {
                               logD("Not Allowed as TWS+ SHO is not enabled");
                           } else {
                               logD("Max Connections have reached");
                           }
                   }
                }
            } else {
                //There is no TWSP connected device
                if (adapterService.isTwsPlusDevice(device)) {
                    if (mIsTwsPlusShoEnabled) {
                        //outgoing connection is TWSP
                        if ((mMaxHeadsetConnections-connDevices.size()) >= 2) {
                            allowSecondHfConnection = true;
                        } else {
                            allowSecondHfConnection = false;
                            logD("Not enough available slots for TWSP");
                        }
                    } else {
                        allowSecondHfConnection = false;
                    }
                } else {
                    //Outgoing connection is legacy device
                    //For legacy case use the config set by User
                   if (connDevices.size() < mSetMaxConfig) {
                      allowSecondHfConnection = true;
                   } else {
                      allowSecondHfConnection = false;
                   }
               }
            }
            Log.v(TAG, "isTwsPlusDevice for " + device +
                     "is"+ adapterService.isTwsPlusDevice(device));
            Log.v(TAG, "TWS Peer Addr: " +
                      adapterService.getTwsPlusPeerAddress(device));
            if (connectedOrConnectingTwspDev != null) {
                Log.v(TAG, "Connected or Connecting device"
                         + connectedOrConnectingTwspDev.getAddress());
            } else {
                Log.v(TAG, "No Connected TWSP devices");
            }
        }

        Log.v(TAG, "allowSecondHfConnection: " + allowSecondHfConnection);
        Log.v(TAG, "DisconnectAll: " + mDisconnectAll);
        return allowSecondHfConnection;
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean connect(BluetoothDevice device) {
         if(ApmConstIntf.getQtiLeAudioEnabled()) {
            Log.d(TAG, "connect Adv Audio enabled");
            CallAudioIntf mCallAudio = CallAudioIntf.get();
            return mCallAudio.connect(device);
        }
        return connectHfp(device);
    }
    public boolean connectHfp(BluetoothDevice device) {

        if (getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.w(TAG, "connect: CONNECTION_POLICY_FORBIDDEN, device=" + device + ", "
                    + Utils.getUidPidString());
            return false;
        }
        synchronized (mStateMachines) {
            ParcelUuid[] featureUuids = mAdapterService.getRemoteUuids(device);
            if (!BluetoothUuid.containsAnyUuid(featureUuids, HEADSET_UUIDS)) {
                Log.e(TAG, "connect: Cannot connect to " + device + ": no headset UUID, "
                    + Utils.getUidPidString());
                return false;
            }
            Log.i(TAG, "connect: device=" + device + ", " + Utils.getUidPidString());
            HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                if (mStateMachinesThread != null) {
                    stateMachine = HeadsetObjectsFactory.getInstance()
                        .makeStateMachine(device, mStateMachinesThread.getLooper(), this,
                                mAdapterService, mNativeInterface, mSystemInterface);
                    mStateMachines.put(device, stateMachine);
                } else {
                    Log.w(TAG, "connect: mStateMachinesThread is null");
                }
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState == BluetoothProfile.STATE_CONNECTED
                    || connectionState == BluetoothProfile.STATE_CONNECTING) {
                Log.w(TAG, "connect: device " + device
                        + " is already connected/connecting, connectionState=" + connectionState);
                return false;
            }
            List<BluetoothDevice> connectingConnectedDevices =
                    getAllDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
            boolean disconnectExisting = false;
            mDisconnectAll = false;
            if (connectingConnectedDevices.size() == 0) {
                 Log.e(TAG, "No Connected devices!");
            }
            if (!isConnectionAllowed(device, connectingConnectedDevices)) {
                // When there is maximum one device, we automatically disconnect the current one
                if (mSetMaxConfig == 1) {
                    if (!mIsTwsPlusEnabled && mAdapterService.isTwsPlusDevice(device)) {
                        Log.w(TAG, "Connection attemp to TWS+ when not enabled, Rejecting it");
                        return false;
                    } else {
                        disconnectExisting = true;
                    }
                } else if (mDisconnectAll) {
                    //In Dual HF case
                    disconnectExisting = true;
                } else {
                    Log.w(TAG, "Max connection has reached, rejecting connection to " + device);
                    return false;
                }
            }
            if (disconnectExisting) {
                for (BluetoothDevice connectingConnectedDevice : connectingConnectedDevices) {
                    disconnect(connectingConnectedDevice);
                }
                setActiveDevice(null);
            }
            stateMachine.sendMessage(HeadsetStateMachine.CONNECT, device);
        }
        return true;
    }

    public boolean disconnect(BluetoothDevice device) {

         if(ApmConstIntf.getQtiLeAudioEnabled()) {
            Log.d(TAG, "connect Adv Audio enabled");
            CallAudioIntf mCallAudio = CallAudioIntf.get();
            return mCallAudio.disconnect(device);
        }
        return disconnectHfp(device);
    }

    /**
     * Disconnects hfp from the passed in device
     *
     * @param device is the device with which we will disconnect hfp
     * @return true if hfp is disconnected, false if the device is not connected
     */
    public boolean disconnectHfp(BluetoothDevice device) {

        Log.i(TAG, "disconnect: device=" + device + ", " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "disconnect: device " + device + " not ever connected/connecting");
                return false;
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState != BluetoothProfile.STATE_CONNECTED
                    && connectionState != BluetoothProfile.STATE_CONNECTING) {
                Log.w(TAG, "disconnect: device " + device
                        + " not connected/connecting, connectionState=" + connectionState);
                return false;
            }
            stateMachine.sendMessage(HeadsetStateMachine.DISCONNECT, device);
        }
        return true;
    }

    public boolean isInCall() {
        boolean isCallOngoing = mSystemInterface.isInCall();
        Log.d(TAG," isInCall " + isCallOngoing);
        return isCallOngoing;
    }

    public boolean isRinging() {
        boolean isRingOngoing = mSystemInterface.isRinging();
        Log.d(TAG," isRinging " + isRingOngoing);
        return isRingOngoing;
    }

    public List<BluetoothDevice> getConnectedDevices() {

        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        synchronized (mStateMachines) {
            for (HeadsetStateMachine stateMachine : mStateMachines.values()) {
                if (stateMachine.getConnectionState() == BluetoothProfile.STATE_CONNECTED) {
                    devices.add(stateMachine.getDevice());
                }
            }
        }
        return devices;
    }
    /**
     * Helper method to get all devices with matching audio states
     *
     */
    private List<BluetoothDevice> getAllDevicesMatchingAudioStates(int[] states) {

        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if (states == null) {
            Log.e(TAG, "->States is null");
            return devices;
        }
        synchronized (mStateMachines) {
            final BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
            if (bondedDevices == null) {
                Log.e(TAG, "->Bonded device is null");
                return devices;
            }
            for (BluetoothDevice device : bondedDevices) {

                int audioState = getAudioState(device);
                Log.e(TAG, "audio state for: " + device + "is" + audioState);
                for (int state : states) {
                    if (audioState == state) {
                        devices.add(device);
                        Log.e(TAG, "getAllDevicesMatchingAudioStates:Adding device: " + device);
                        break;
                    }
                }
            }
        }
        return devices;
    }

    /**
     * Helper method to get all devices with matching connection state
     *
     */
    private List<BluetoothDevice> getAllDevicesMatchingConnectionStates(int[] states) {

        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if (states == null) {
            Log.e(TAG, "->States is null");
            return devices;
        }
        synchronized (mStateMachines) {
            final BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
            if (bondedDevices == null) {
                Log.e(TAG, "->Bonded device is null");
                return devices;
            }
            for (BluetoothDevice device : bondedDevices) {

                int connectionState = getConnectionState(device);
                Log.e(TAG, "Connec state for: " + device + "is" + connectionState);
                for (int state : states) {
                    if (connectionState == state) {
                        devices.add(device);
                        Log.e(TAG, "Adding device: " + device);
                        break;
                    }
                }
            }
        }
        return devices;
    }

    /**
     * Same as the API method {@link BluetoothHeadset#getDevicesMatchingConnectionStates(int[])}
     *
     * @param states an array of states from {@link BluetoothProfile}
     * @return a list of devices matching the array of connection states
     */
    @VisibleForTesting
    public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {

        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        synchronized (mStateMachines) {
            if (states == null || mAdapterService == null) {
                return devices;
            }
            final BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
            if (bondedDevices == null) {
                return devices;
            }
            for (BluetoothDevice device : bondedDevices) {

                int connectionState = getConnectionState(device);
                for (int state : states) {
                    if (connectionState == state) {
                        devices.add(device);
                        break;
                    }
                }
            }
        }
        return devices;
    }

    public int getConnectionState(BluetoothDevice device) {

        synchronized (mStateMachines) {
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return stateMachine.getConnectionState();
        }
    }

    /**
     * Set connection policy of the profile and connects it if connectionPolicy is
     * {@link BluetoothProfile#CONNECTION_POLICY_ALLOWED} or disconnects if connectionPolicy is
     * {@link BluetoothProfile#CONNECTION_POLICY_FORBIDDEN}
     *
     * <p> The device should already be paired.
     * Connection policy can be one of:
     * {@link BluetoothProfile#CONNECTION_POLICY_ALLOWED},
     * {@link BluetoothProfile#CONNECTION_POLICY_FORBIDDEN},
     * {@link BluetoothProfile#CONNECTION_POLICY_UNKNOWN}
     *
     * @param device Paired bluetooth device
     * @param connectionPolicy is the connection policy to set to for this profile
     * @return true if connectionPolicy is set, false on error
     */
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        Log.i(TAG, "setConnectionPolicy: device=" + device
                + ", connectionPolicy=" + connectionPolicy + ", " + Utils.getUidPidString());

        if (!mDatabaseManager.setProfileConnectionPolicy(device, BluetoothProfile.HEADSET,
                  connectionPolicy)) {
            return false;
        }
        if (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            connect(device);
        } else if (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            disconnect(device);
        }
        return true;
    }

    /**
     * Get the connection policy of the profile.
     *
     * <p> The connection policy can be any of:
     * {@link BluetoothProfile#CONNECTION_POLICY_ALLOWED},
     * {@link BluetoothProfile#CONNECTION_POLICY_FORBIDDEN},
     * {@link BluetoothProfile#CONNECTION_POLICY_UNKNOWN}
     *
     * @param device Bluetooth device
     * @return connection policy of the device
     * @hide
     */
    public int getConnectionPolicy(BluetoothDevice device) {
        return mDatabaseManager
                .getProfileConnectionPolicy(device, BluetoothProfile.HEADSET);
    }

    boolean isNoiseReductionSupported(BluetoothDevice device) {
        return mNativeInterface.isNoiseReductionSupported(device);
    }

    boolean isVoiceRecognitionSupported(BluetoothDevice device) {
        return mNativeInterface.isVoiceRecognitionSupported(device);
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    boolean startVoiceRecognition(BluetoothDevice device) {

        Log.i(TAG, "startVoiceRecognition: device=" + device + ", " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            // TODO(b/79660380): Workaround in case voice recognition was not terminated properly
            if (mVoiceRecognitionStarted) {
                boolean status = stopVoiceRecognition(mActiveDevice);
                Log.w(TAG, "startVoiceRecognition: voice recognition is still active, just called "
                        + "stopVoiceRecognition, returned " + status + " on " + mActiveDevice
                        + ", please try again");
                mVoiceRecognitionStarted = false;
                return false;
            }
            if (!isAudioModeIdle()) {
                Log.w(TAG, "startVoiceRecognition: audio mode not idle, active device is "
                        + mActiveDevice);
                return false;
            }
            // Audio should not be on when no audio mode is active
            if (isAudioOn()) {
                // Disconnect audio so that API user can try later
                int status = disconnectAudio();
                Log.w(TAG, "startVoiceRecognition: audio is still active, please wait for audio to"
                        + " be disconnected, disconnectAudio() returned " + status
                        + ", active device is " + mActiveDevice);
                return false;
            }
            if (device == null) {
                Log.i(TAG, "device is null, use active device " + mActiveDevice + " instead");
                device = mActiveDevice;
            }
            boolean pendingRequestByHeadset = false;
            if (mVoiceRecognitionTimeoutEvent != null) {
                if (!mVoiceRecognitionTimeoutEvent.mVoiceRecognitionDevice.equals(device)) {
                    // TODO(b/79660380): Workaround when target device != requesting device
                    Log.w(TAG, "startVoiceRecognition: device " + device
                            + " is not the same as requesting device "
                            + mVoiceRecognitionTimeoutEvent.mVoiceRecognitionDevice
                            + ", fall back to requesting device");
                    device = mVoiceRecognitionTimeoutEvent.mVoiceRecognitionDevice;
                }
                if (mStateMachinesThread != null) {
                    mStateMachinesThread.getThreadHandler()
                           .removeCallbacks(mVoiceRecognitionTimeoutEvent);
                } else {
                    Log.w(TAG, "startVoiceRecognition: mStateMachinesThread is null");
                }
                mVoiceRecognitionTimeoutEvent = null;
                if (mSystemInterface.getVoiceRecognitionWakeLock().isHeld()) {
                    mSystemInterface.getVoiceRecognitionWakeLock().release();
                }
                pendingRequestByHeadset = true;
            }
            if (!Objects.equals(device, mActiveDevice) &&
                  !mAdapterService.isTwsPlusDevice(device) && !setActiveDevice(device)) {
                Log.w(TAG, "startVoiceRecognition: failed to set " + device + " as active");
                return false;
            }
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "startVoiceRecognition: " + device + " is never connected");
                return false;
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState != BluetoothProfile.STATE_CONNECTED
                    && connectionState != BluetoothProfile.STATE_CONNECTING) {
                Log.w(TAG, "startVoiceRecognition: " + device + " is not connected or connecting");
                return false;
            }
            mVoiceRecognitionStarted = true;
            if (pendingRequestByHeadset) {
                stateMachine.sendMessage(HeadsetStateMachine.VOICE_RECOGNITION_RESULT,
                        1 /* success */, 0, device);
            } else {
                stateMachine.sendMessage(HeadsetStateMachine.VOICE_RECOGNITION_START, device);
            }
        }
        /* VR calls always use SWB if supported on remote*/
        if(isSwbEnabled()) {
            enableSwbCodec(true);
        }
        return true;
    }

    boolean stopVoiceRecognition(BluetoothDevice device) {

        Log.i(TAG, "stopVoiceRecognition: device=" + device + ", " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            if (!Objects.equals(mActiveDevice, device)) {
                Log.w(TAG, "startVoiceRecognition: requested device " + device
                        + " is not active, use active device " + mActiveDevice + " instead");
                device = mActiveDevice;
            }
            if(device == null) {
               Log.w(TAG, "Requested device is null. resume A2DP");
               mVoiceRecognitionStarted = false;
               mHfpA2dpSyncInterface.releaseA2DP(null);
               return false;
            }
            if (mAdapterService.isTwsPlusDevice(device) &&
                    !isAudioConnected(device)) {
                BluetoothDevice peerDevice = getTwsPlusConnectedPeer(device);
                if (peerDevice != null && isAudioConnected(peerDevice)) {
                    Log.w(TAG, "startVoiceRecognition: requested TWS+ device " + device
                            + " is not audio connected, use TWS+ peer device " + peerDevice
                            + " instead");
                    device = peerDevice;
                } else {
                    Log.w(TAG, "stopVoiceRecognition: both earbuds are not audio connected, resume A2DP");
                    mVoiceRecognitionStarted = false;
                    mHfpA2dpSyncInterface.releaseA2DP(null);
                    return false;
                }
            }
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "stopVoiceRecognition: " + device + " is never connected");
                return false;
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState != BluetoothProfile.STATE_CONNECTED
                    && connectionState != BluetoothProfile.STATE_CONNECTING) {
                Log.w(TAG, "stopVoiceRecognition: " + device + " is not connected or connecting");
                return false;
            }
            if (!mVoiceRecognitionStarted) {
                Log.w(TAG, "stopVoiceRecognition: voice recognition was not started");
                return false;
            }
            mVoiceRecognitionStarted = false;
            stateMachine.sendMessage(HeadsetStateMachine.VOICE_RECOGNITION_STOP, device);

            if (isAudioOn()) {
                stateMachine.sendMessage(HeadsetStateMachine.DISCONNECT_AUDIO, device);
            } else {
                Log.w(TAG, "SCO is not connected and VR stopped, resuming A2DP");
                stateMachine.sendMessage(HeadsetStateMachine.RESUME_A2DP);
            }
        }
        return true;
    }

    public boolean isAudioOn() {

        int numConnectedAudioDevices = getNonIdleAudioDevices().size();
        Log.d(TAG," isAudioOn: The number of audio connected devices "
                 + numConnectedAudioDevices);
        return numConnectedAudioDevices > 0;
    }

    public boolean isScoOrCallActive() {
      Log.d(TAG, "isScoOrCallActive(): Call Active:" + mSystemInterface.isInCall() +
                                       "Call is Ringing:" + mSystemInterface.isInCall() +
                                       "SCO is Active:" + isAudioOn());
      if (mSystemInterface.isInCall() || (mSystemInterface.isRinging()) || isAudioOn()) {
          return true;
      } else {
          return false;
      }
    }
    boolean isAudioConnected(BluetoothDevice device) {

        synchronized (mStateMachines) {
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                return false;
            }
            return stateMachine.getAudioState() == BluetoothHeadset.STATE_AUDIO_CONNECTED;
        }
    }

    int getAudioState(BluetoothDevice device) {

        synchronized (mStateMachines) {
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                return BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
            }
            return stateMachine.getAudioState();
        }
    }

    public void setAudioRouteAllowed(boolean allowed) {

        Log.i(TAG, "setAudioRouteAllowed: allowed=" + allowed + ", " + Utils.getUidPidString());
        mAudioRouteAllowed = allowed;
        mNativeInterface.setScoAllowed(allowed);
    }

    public boolean getAudioRouteAllowed() {

        return mAudioRouteAllowed;
    }

    public void setForceScoAudio(boolean forced) {

        Log.i(TAG, "setForceScoAudio: forced=" + forced + ", " + Utils.getUidPidString());
        mForceScoAudio = forced;
    }

    @VisibleForTesting
    public boolean getForceScoAudio() {
        return mForceScoAudio;
    }

    /**
     * Get first available device for SCO audio
     *
     * @return first connected headset device
     */
    @VisibleForTesting
    @Nullable
    public BluetoothDevice getFirstConnectedAudioDevice() {
        ArrayList<HeadsetStateMachine> stateMachines = new ArrayList<>();
        synchronized (mStateMachines) {
            List<BluetoothDevice> availableDevices =
                    getDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
            for (BluetoothDevice device : availableDevices) {
                final HeadsetStateMachine stateMachine = mStateMachines.get(device);
                if (stateMachine == null) {
                    continue;
                }
                stateMachines.add(stateMachine);
            }
        }
        stateMachines.sort(Comparator.comparingLong(HeadsetStateMachine::getConnectingTimestampMs));
        if (stateMachines.size() > 0) {
            return stateMachines.get(0).getDevice();
        }
        return null;
    }

    /**
     * Process a change in the silence mode for a {@link BluetoothDevice}.
     *
     * @param device the device to change silence mode
     * @param silence true to enable silence mode, false to disable.
     * @return true on success, false on error
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean setSilenceMode(BluetoothDevice device, boolean silence) {
        Log.d(TAG, "setSilenceMode(" + device + "): " + silence);

        if (silence && Objects.equals(mActiveDevice, device)) {
            setActiveDevice(null);
        } else if (!silence && mActiveDevice == null) {
            // Set the device as the active device if currently no active device.
            setActiveDevice(device);
        }
        synchronized (mStateMachines) {
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "setSilenceMode: device " + device
                        + " was never connected/connecting");
                return false;
            }
            stateMachine.setSilenceDevice(silence);
        }

        return true;
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean setActiveDevice(BluetoothDevice device) {
        if((ApmConstIntf.getQtiLeAudioEnabled()) || (ApmConstIntf.getAospLeaEnabled())) {
            ActiveDeviceManagerServiceIntf mActiveDeviceManager =
                    ActiveDeviceManagerServiceIntf.get();
            /*Precautionary Change: Force Active Device Manager
             * to always return false as same as Media Audio*/
            mActiveDeviceManager.setActiveDevice(device,
                    ApmConstIntf.AudioFeatures.CALL_AUDIO, false);
            return true;
        } else {
            int ret = setActiveDeviceHF(device);
            if (ret == ActiveDeviceManagerServiceIntf.SHO_FAILED) {
              return false;
            } else {
              return true;
            }
        }
    }

    /**
     * Set the active device.
     *
     * @param device the active device
     * @return true on success, otherwise false
     */
    public int setActiveDeviceHF(BluetoothDevice device) {
        Log.i(TAG, "setActiveDeviceHF: device=" + device + ", " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            if (device == null) {
                // Clear the active device
                if (mVoiceRecognitionStarted) {
                    if (!stopVoiceRecognition(mActiveDevice)) {
                        Log.w(TAG, "setActiveDevice: fail to stopVoiceRecognition from "
                                + mActiveDevice);
                    }
                }
                if (mVirtualCallStarted) {
                    if (ApmConstIntf.getQtiLeAudioEnabled()) {
                        Log.d(TAG, "stopScoUsingVirtualVoiceCall Adv Audio enabled");
                        CallAudioIntf mCallAudio = CallAudioIntf.get();
                        if (mCallAudio != null) {
                            mCallAudio.stopScoUsingVirtualVoiceCall();
                        }
                    } else if (!stopScoUsingVirtualVoiceCall()) {
                        Log.w(TAG, "setActiveDevice: fail to stopScoUsingVirtualVoiceCall from "
                                + mActiveDevice);
                    }
                }
                if (getAudioState(mActiveDevice) != BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                    int disconnectStatus = disconnectAudio();
                    if (disconnectStatus != BluetoothStatusCodes.SUCCESS) {
                        Log.w(TAG, "setActiveDevice: disconnectAudio failed on " + mActiveDevice);
                    }
                }
                if (!mNativeInterface.setActiveDevice(null)) {
                    Log.w(TAG, "setActiveDevice: Cannot set active device as null in native layer");
                }
                List<BluetoothDevice> audioInProgressDevices =
                    getAllDevicesMatchingAudioStates(AUDIO_DISCONNECTING_AND_AUDIO_CONNECTED_STATES);
                //If there is any device whose audio is still in progress
                if (audioInProgressDevices.size() != 0)
                {
                    if (ApmConstIntf.getQtiLeAudioEnabled() || ApmConstIntf.getAospLeaEnabled()) {
                        mSHOStatus = true;
                        mTempActiveDevice = device;
                        mActiveDevice = null;
                        return ActiveDeviceManagerServiceIntf.SHO_PENDING;
                    }
                }
                mActiveDevice = null;
                if (!(ApmConstIntf.getQtiLeAudioEnabled() || ApmConstIntf.getAospLeaEnabled())) {
                   broadcastActiveDevice(null);
                }
                return ActiveDeviceManagerServiceIntf.SHO_SUCCESS;
            }
            if (device.equals(mActiveDevice)) {
                Log.i(TAG, "setActiveDevice: device " + device + " is already active");
                return ActiveDeviceManagerServiceIntf.SHO_SUCCESS;
            }
            if (getConnectionState(device) != BluetoothProfile.STATE_CONNECTED) {
                Log.e(TAG, "setActiveDevice: Cannot set " + device
                        + " as active, device is not connected");
                return ActiveDeviceManagerServiceIntf.SHO_FAILED;
            }
            if (mActiveDevice != null && mAdapterService.isTwsPlusDevice(device) &&
                mAdapterService.isTwsPlusDevice(mActiveDevice) &&
                !Objects.equals(device, mActiveDevice) &&
                getConnectionState(mActiveDevice) == BluetoothProfile.STATE_CONNECTED) {
                Log.d(TAG,"Ignore setActiveDevice request");
                return ActiveDeviceManagerServiceIntf.SHO_FAILED;
            }

            if (!mNativeInterface.setActiveDevice(device)) {
                Log.e(TAG, "setActiveDevice: Cannot set " + device + " as active in native layer");
                return ActiveDeviceManagerServiceIntf.SHO_FAILED;
            }
            BluetoothDevice previousActiveDevice = mActiveDevice;
            mActiveDevice = device;
            int audioStateOfPrevActiveDevice = BluetoothHeadset.STATE_AUDIO_DISCONNECTED;
            boolean activeSwitchBetweenEbs = false;
            if (previousActiveDevice != null &&
                    mAdapterService.isTwsPlusDevice(previousActiveDevice)) {
                BluetoothDevice peerDevice =
                           getTwsPlusConnectedPeer(previousActiveDevice);
                if (mActiveDevice != null &&
                      mAdapterService.isTwsPlusDevice(mActiveDevice)) {
                      Log.d(TAG, "Active device switch b/n ebs");
                      activeSwitchBetweenEbs = true;
                } else {
                    if (getAudioState(previousActiveDevice) !=
                               BluetoothHeadset.STATE_AUDIO_DISCONNECTED ||
                        getAudioState(peerDevice) !=
                               BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                            audioStateOfPrevActiveDevice =
                                   BluetoothHeadset.STATE_AUDIO_CONNECTED;
                    }
                }
                if (audioStateOfPrevActiveDevice ==
                        BluetoothHeadset.STATE_AUDIO_CONNECTED &&
                        getAudioState(previousActiveDevice) ==
                        BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                    Log.w(TAG, "Update previousActiveDevice with" + peerDevice);
                    previousActiveDevice = peerDevice;
                }
            } else {
                audioStateOfPrevActiveDevice =
                               getAudioState(previousActiveDevice);
            }
            if (!activeSwitchBetweenEbs && audioStateOfPrevActiveDevice !=
                               BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                int disconnectStatus = disconnectAudio(previousActiveDevice);
                if (disconnectStatus != BluetoothStatusCodes.SUCCESS) {
                    Log.e(TAG, "setActiveDevice: fail to disconnectAudio from "
                            + previousActiveDevice);
                    mActiveDevice = previousActiveDevice;
                    mNativeInterface.setActiveDevice(previousActiveDevice);
                    return ActiveDeviceManagerServiceIntf.SHO_FAILED;
                }
                if (!(ApmConstIntf.getQtiLeAudioEnabled() || ApmConstIntf.getAospLeaEnabled())) {
                    broadcastActiveDevice(mActiveDevice);
                }
            } else if (shouldPersistAudio()) {
                boolean isPts = SystemProperties.getBoolean("vendor.bt.pts.certification", false);
                if (!isPts) {
                    int connectStatus = connectAudio(mActiveDevice);
                    if (connectStatus != BluetoothStatusCodes.SUCCESS) {
                        Log.e(TAG, "setActiveDevice: fail to connectAudio to " + mActiveDevice);
                        mActiveDevice = previousActiveDevice;
                        mNativeInterface.setActiveDevice(previousActiveDevice);
                        return ActiveDeviceManagerServiceIntf.SHO_FAILED;
                    }
                }
                if (!(ApmConstIntf.getQtiLeAudioEnabled() || ApmConstIntf.getAospLeaEnabled())) {
                    broadcastActiveDevice(mActiveDevice);
                }
            } else {
                if (!(ApmConstIntf.getQtiLeAudioEnabled() || ApmConstIntf.getAospLeaEnabled())) {
                    broadcastActiveDevice(mActiveDevice);
                }
            }
        }
        return ActiveDeviceManagerServiceIntf.SHO_SUCCESS;
    }

    /**
     * Get the active device.
     *
     * @return the active device or null if no device is active
     */
    public BluetoothDevice getActiveDevice() {

        synchronized (mStateMachines) {
            return mActiveDevice;
        }
    }

    int connectAudio() {

        synchronized (mStateMachines) {
            BluetoothDevice device = mActiveDevice;
            if (device == null) {
                Log.w(TAG, "connectAudio: no active device, " + Utils.getUidPidString());
                return BluetoothStatusCodes.ERROR_NO_ACTIVE_DEVICES;
            }
            return connectAudio(device);
        }
    }

    public int connectAudio(BluetoothDevice device) {

        int connDelay = SystemProperties.getInt("persist.vendor.bluetooth.audioconnect.delay",
                AUDIO_CONNECTION_DELAY_DEFAULT);

        Log.i(TAG, "connectAudio: device=" + device + ", " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            int scoConnectionAllowedState = isScoAcceptable(device);
            if (scoConnectionAllowedState != BluetoothStatusCodes.SUCCESS) {
                Log.w(TAG, "connectAudio, rejected SCO request to " + device);
                return scoConnectionAllowedState;
            }
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "connectAudio: device " + device + " was never connected/connecting");
                return BluetoothStatusCodes.ERROR_PROFILE_NOT_CONNECTED;
            }
            if (stateMachine.getConnectionState() != BluetoothProfile.STATE_CONNECTED) {
                Log.w(TAG, "connectAudio: profile not connected");
                return BluetoothStatusCodes.ERROR_PROFILE_NOT_CONNECTED;
            }
            if (stateMachine.getAudioState() != BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                logD("connectAudio: audio is not idle for device " + device);
                /**
                 * add for case that device disconnecting audio has been set active again,
                 * then send CONNECT_AUDIO if not contained in queue and should persist audio
                 */
                if (mActiveDevice != null && mActiveDevice.equals(device) &&
                        stateMachine.getAudioState() == BluetoothHeadset.STATE_AUDIO_DISCONNECTING
                        && !stateMachine.hasMessagesInQueue(HeadsetStateMachine.CONNECT_AUDIO) &&
                        !stateMachine.hasDeferredMessagesInQueue(HeadsetStateMachine.CONNECT_AUDIO)
                        && shouldPersistAudio()) {
                    if (stateMachine.getIfDeviceBlacklistedForSCOAfterSLC() == true)
                        connDelay = 0;

                    Log.i(TAG, "connectAudio: active again and connect audio after "
                            + connDelay + " ms");
                    stateMachine.sendMessageDelayed(HeadsetStateMachine.CONNECT_AUDIO,
                            device, connDelay);
                }
                return BluetoothStatusCodes.SUCCESS;
            }
            if (isAudioOn()) {
                //SCO is connecting or connected.
                //Return true to telephony
                Log.w(TAG, "connectAudio: audio is not idle, current audio devices are: "
                        + Arrays.toString(getNonIdleAudioDevices().toArray()) +
                        " ,returning true");
                return BluetoothStatusCodes.SUCCESS;
            }

            if (stateMachine.getIfDeviceBlacklistedForSCOAfterSLC() == true)
                connDelay = 0;

            Log.i(TAG, "connectAudio: connect audio after " + connDelay + " ms");
            stateMachine.sendMessageDelayed(HeadsetStateMachine.CONNECT_AUDIO, device, connDelay);
        }
        return BluetoothStatusCodes.SUCCESS;
    }

    private List<BluetoothDevice> getNonIdleAudioDevices() {
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        synchronized (mStateMachines) {
            for (HeadsetStateMachine stateMachine : mStateMachines.values()) {
                if (stateMachine.getAudioState() != BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                    devices.add(stateMachine.getDevice());
                }
            }
        }
        return devices;
    }

    public int disconnectAudio() {
        int disconnectResult = BluetoothStatusCodes.ERROR_NO_ACTIVE_DEVICES;
        synchronized (mStateMachines) {
            for (BluetoothDevice device : getNonIdleAudioDevices()) {
                disconnectResult = disconnectAudio(device);
                if (disconnectResult == BluetoothStatusCodes.SUCCESS) {
                    return disconnectResult;
                } else {
                    Log.e(TAG, "disconnectAudio() from " + device + " failed with status code "
                            + disconnectResult);
                }
            }
        }
        Log.d(TAG, "disconnectAudio() no active audio connection");
        return disconnectResult;
    }

    int disconnectAudio(BluetoothDevice device) {
        synchronized (mStateMachines) {
            Log.i(TAG, "disconnectAudio: device=" + device + ", " + Utils.getUidPidString());
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "disconnectAudio: device " + device + " was never connected/connecting");
                return BluetoothStatusCodes.ERROR_PROFILE_NOT_CONNECTED;
            }
            if (stateMachine.getAudioState() == BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                Log.w(TAG, "disconnectAudio, audio is already disconnected for " + device);
                return BluetoothStatusCodes.ERROR_AUDIO_DEVICE_ALREADY_DISCONNECTED;
            }
            stateMachine.sendMessage(HeadsetStateMachine.DISCONNECT_AUDIO, device);
        }
        return BluetoothStatusCodes.SUCCESS;
    }

    boolean isVirtualCallStarted() {

        synchronized (mStateMachines) {
            return mVirtualCallStarted;
        }
    }

    boolean isVRStarted() {

        synchronized (mStateMachines) {
            return mVoiceRecognitionStarted;
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean startScoUsingVirtualVoiceCall() {

        Log.i(TAG, "startScoUsingVirtualVoiceCall: " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            // TODO(b/79660380): Workaround in case voice recognition was not terminated properly
            if (mVoiceRecognitionStarted) {
                boolean status = stopVoiceRecognition(mActiveDevice);
                Log.w(TAG, "startScoUsingVirtualVoiceCall: voice recognition is still active, "
                        + "just called stopVoiceRecognition, returned " + status + " on "
                        + mActiveDevice + ", please try again");
                mVoiceRecognitionStarted = false;
                return false;
            }
            if (!isAudioModeIdle()) {
                Log.w(TAG, "startScoUsingVirtualVoiceCall: audio mode not idle, active device is "
                        + mActiveDevice);
                return false;
            }
            // Audio should not be on when no audio mode is active
            if (isAudioOn()) {
                // Disconnect audio so that API user can try later
                int status = disconnectAudio();
                Log.w(TAG, "startScoUsingVirtualVoiceCall: audio is still active, please wait for "
                        + "audio to be disconnected, disconnectAudio() returned " + status
                        + ", active device is " + mActiveDevice);
                return false;
            }
            if (mActiveDevice == null) {
                Log.w(TAG, "startScoUsingVirtualVoiceCall: no active device");
                return false;
            }
            mVirtualCallStarted = true;
            mSystemInterface.getHeadsetPhoneState().setIsCsCall(false);
            // Send virtual phone state changed to initialize SCO
            phoneStateChanged(0, 0, HeadsetHalConstants.CALL_STATE_DIALING, "", 0, "", true);
            phoneStateChanged(0, 0, HeadsetHalConstants.CALL_STATE_ALERTING, "", 0, "", true);
            phoneStateChanged(1, 0, HeadsetHalConstants.CALL_STATE_IDLE, "", 0, "", true);
            return true;
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean stopScoUsingVirtualVoiceCall() {

        Log.i(TAG, "stopScoUsingVirtualVoiceCall: " + Utils.getUidPidString());
        synchronized (mStateMachines) {
            // 1. Check if virtual call has already started
            if (!mVirtualCallStarted) {
                Log.w(TAG, "stopScoUsingVirtualVoiceCall: virtual call not started");
                return false;
            }
            mVirtualCallStarted = false;
            // 2. Send virtual phone state changed to close SCO
            phoneStateChanged(0, 0, HeadsetHalConstants.CALL_STATE_IDLE, "", 0, "", true);
        }
        return true;
    }

    class DialingOutTimeoutEvent implements Runnable {
        BluetoothDevice mDialingOutDevice;

        DialingOutTimeoutEvent(BluetoothDevice fromDevice) {
            mDialingOutDevice = fromDevice;
        }

        @Override
        public void run() {
            synchronized (mStateMachines) {
                mDialingOutTimeoutEvent = null;
                if ((ApmConstIntf.getQtiLeAudioEnabled()) || (ApmConstIntf.getAospLeaEnabled())) {
                   Log.d(TAG, "Adv Audio enabled: clccResponse");
                   CallControlIntf mCallControl = CallControlIntf.get();
                   mCallControl.updateOriginateResult(mDialingOutDevice, 0/*unused*/, 0/*fail*/);
                }
                doForStateMachine(mDialingOutDevice, stateMachine -> stateMachine.sendMessage(
                        HeadsetStateMachine.DIALING_OUT_RESULT, 0 /* fail */, 0,
                        mDialingOutDevice));
            }
        }

        @Override
        public String toString() {
            return "DialingOutTimeoutEvent[" + mDialingOutDevice + "]";
        }
    }

    public boolean isTwsPlusActive(BluetoothDevice device) {
        boolean ret = false;
        if (mAdapterService.isTwsPlusDevice(device)) {
            if (device.equals(getActiveDevice())) {
                ret = true;
            } else {
                BluetoothDevice peerTwsDevice = mAdapterService.getTwsPlusPeerDevice(device);
                if (peerTwsDevice != null &&
                    peerTwsDevice.equals(getActiveDevice())) {
                    ret = true;
                }
            }
        }
        Log.d(TAG, "isTwsPlusActive returns" + ret);
        return ret;
    }

    /**
     * Dial an outgoing call as requested by the remote device
     *
     * @param fromDevice remote device that initiated this dial out action
     * @param dialNumber number to dial
     * @return true on successful dial out
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean dialOutgoingCall(BluetoothDevice fromDevice, String dialNumber) {
        synchronized (mStateMachines) {
            Log.i(TAG, "dialOutgoingCall: from " + fromDevice);
            if (!isOnStateMachineThread()) {
                Log.e(TAG, "dialOutgoingCall must be called from state machine thread");
                return false;
            }

            return dialOutgoingCallInternal(fromDevice, dialNumber);
        }
    }

    public boolean dialOutgoingCallInternal(BluetoothDevice fromDevice, String dialNumber) {
        synchronized (mStateMachines) {
            if (mDialingOutTimeoutEvent != null) {
                Log.e(TAG, "dialOutgoingCall, already dialing by " + mDialingOutTimeoutEvent);
                return false;
            }
            if (isVirtualCallStarted()) {
                if (ApmConstIntf.getQtiLeAudioEnabled()) {
                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    if (mCallAudio != null) {
                        if (!mCallAudio.stopScoUsingVirtualVoiceCall()) {
                            Log.e(TAG, "dialOutgoingCall LE enabled failed to stop VOIP call");
                            return false;
                        }
                    }
                } else if (!stopScoUsingVirtualVoiceCall()) {
                    Log.e(TAG, "dialOutgoingCall failed to stop current virtual call");
                    return false;
                }
            }
            if (fromDevice == null) {
                Log.e(TAG, "dialOutgoingCall, fromDevice is null");
                return false;
            }
            if ((!fromDevice.equals(mActiveDevice)) && !isTwsPlusActive(fromDevice) &&
                !setActiveDevice(fromDevice)) {
                Log.e(TAG, "dialOutgoingCall failed to set active device to " + fromDevice);
                return false;
            }
            Intent intent = new Intent(Intent.ACTION_CALL_PRIVILEGED,
                    Uri.fromParts(PhoneAccount.SCHEME_TEL, dialNumber, null));
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
            mDialingOutTimeoutEvent = new DialingOutTimeoutEvent(fromDevice);
            if (mStateMachinesThread != null) {
                mStateMachinesThread.getThreadHandler()
                    .postDelayed(mDialingOutTimeoutEvent, DIALING_OUT_TIMEOUT_MS);
            } else {
                Log.w(TAG, "dialOutgoingCall: mStateMachinesThread is null");
            }
            return true;
        }
    }

    /**
     * Check if any connected headset has started dialing calls
     *
     * @return true if some device has started dialing calls
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean hasDeviceInitiatedDialingOut() {
        synchronized (mStateMachines) {
            return mDialingOutTimeoutEvent != null;
        }
    }

    class VoiceRecognitionTimeoutEvent implements Runnable {
        BluetoothDevice mVoiceRecognitionDevice;

        VoiceRecognitionTimeoutEvent(BluetoothDevice device) {
            mVoiceRecognitionDevice = device;
        }

        @Override
        public void run() {
            synchronized (mStateMachines) {
                if (mSystemInterface.getVoiceRecognitionWakeLock().isHeld()) {
                    mSystemInterface.getVoiceRecognitionWakeLock().release();
                }
                mVoiceRecognitionTimeoutEvent = null;
                doForStateMachine(mVoiceRecognitionDevice, stateMachine -> stateMachine.sendMessage(
                        HeadsetStateMachine.VOICE_RECOGNITION_RESULT, 0 /* fail */, 0,
                        mVoiceRecognitionDevice));
            }
        }

        @Override
        public String toString() {
            return "VoiceRecognitionTimeoutEvent[" + mVoiceRecognitionDevice + "]";
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    boolean startVoiceRecognitionByHeadset(BluetoothDevice fromDevice) {
        synchronized (mStateMachines) {
            Log.i(TAG, "startVoiceRecognitionByHeadset: from " + fromDevice);
            // TODO(b/79660380): Workaround in case voice recognition was not terminated properly
            if (mVoiceRecognitionStarted) {
                boolean status = stopVoiceRecognition(mActiveDevice);
                Log.w(TAG, "startVoiceRecognitionByHeadset: voice recognition is still active, "
                        + "just called stopVoiceRecognition, returned " + status + " on "
                        + mActiveDevice + ", please try again");
                mVoiceRecognitionStarted = false;
                return false;
            }
            if (fromDevice == null) {
                Log.e(TAG, "startVoiceRecognitionByHeadset: fromDevice is null");
                return false;
            }
            if (!isAudioModeIdle()) {
                Log.w(TAG, "startVoiceRecognitionByHeadset: audio mode not idle, active device is "
                        + mActiveDevice);
                return false;
            }
            // Audio should not be on when no audio mode is active
            if (isAudioOn()) {
                // Disconnect audio so that user can try later
                int status = disconnectAudio();
                Log.w(TAG, "startVoiceRecognitionByHeadset: audio is still active, please wait for"
                        + " audio to be disconnected, disconnectAudio() returned " + status
                        + ", active device is " + mActiveDevice);
                return false;
            }
            // Do not start new request until the current one is finished or timeout
            if (mVoiceRecognitionTimeoutEvent != null) {
                Log.w(TAG, "startVoiceRecognitionByHeadset: failed request from " + fromDevice
                        + ", already pending by " + mVoiceRecognitionTimeoutEvent);
                return false;
            }
            if (!isTwsPlusActive(fromDevice) && !setActiveDevice(fromDevice)) {
                Log.w(TAG, "startVoiceRecognitionByHeadset: failed to set " + fromDevice
                        + " as active");
                return false;
            }
            IDeviceIdleController deviceIdleController = IDeviceIdleController.Stub.asInterface(
                    ServiceManager.getService(Context.DEVICE_IDLE_CONTROLLER));
            if (deviceIdleController == null) {
                Log.w(TAG, "startVoiceRecognitionByHeadset: deviceIdleController is null, device="
                        + fromDevice);
                return false;
            }
            try {
                deviceIdleController.exitIdle("voice-command");
            } catch (RemoteException e) {
                Log.w(TAG,
                        "startVoiceRecognitionByHeadset: failed to exit idle, device=" + fromDevice
                                + ", error=" + e.getMessage());
                return false;
            }
            if (!mSystemInterface.activateVoiceRecognition()) {
                Log.w(TAG, "startVoiceRecognitionByHeadset: failed request from " + fromDevice);
                return false;
            }
            mVoiceRecognitionTimeoutEvent = new VoiceRecognitionTimeoutEvent(fromDevice);
            if (mStateMachinesThread != null) {
                mStateMachinesThread.getThreadHandler()
                    .postDelayed(mVoiceRecognitionTimeoutEvent, sStartVrTimeoutMs);
            } else {
                Log.w(TAG, "startVoiceRecognitionByHeadset: mStateMachinesThread is null");
            }
            if (!mSystemInterface.getVoiceRecognitionWakeLock().isHeld()) {
                mSystemInterface.getVoiceRecognitionWakeLock().acquire(sStartVrTimeoutMs);
            }
            /* VR calls always use SWB if supported on remote*/
            if(isSwbEnabled()) {
                enableSwbCodec(true);
            }
            return true;
        }
    }

    boolean stopVoiceRecognitionByHeadset(BluetoothDevice fromDevice) {
        synchronized (mStateMachines) {
            Log.i(TAG, "stopVoiceRecognitionByHeadset: from " + fromDevice);
            if (!Objects.equals(fromDevice, mActiveDevice)) {
                Log.w(TAG, "stopVoiceRecognitionByHeadset: " + fromDevice
                        + " is not active, active device is " + mActiveDevice);
                return false;
            }
            if (!mVoiceRecognitionStarted && mVoiceRecognitionTimeoutEvent == null) {
                Log.w(TAG, "stopVoiceRecognitionByHeadset: voice recognition not started, device="
                        + fromDevice);
                return false;
            }
            if (mVoiceRecognitionTimeoutEvent != null) {
                if (mSystemInterface.getVoiceRecognitionWakeLock().isHeld()) {
                    mSystemInterface.getVoiceRecognitionWakeLock().release();
                }
                if (mStateMachinesThread != null) {
                    mStateMachinesThread.getThreadHandler()
                        .removeCallbacks(mVoiceRecognitionTimeoutEvent);
                } else {
                    Log.w(TAG, "stopVoiceRecognitionByHeadset: mStateMachinesThread is null");
                }
                mVoiceRecognitionTimeoutEvent = null;
            }
            if (mVoiceRecognitionStarted) {
                if (isAudioOn()) {
                    int disconnectStatus = disconnectAudio();
                    if (disconnectStatus != BluetoothStatusCodes.SUCCESS) {
                        Log.w(TAG, "stopVoiceRecognitionByHeadset: failed to disconnect audio from "
                            + fromDevice);
                    }
                } else {
                    Log.w(TAG, "stopVoiceRecognitionByHeadset: No SCO connected, resume A2DP");
                    mHfpA2dpSyncInterface.releaseA2DP(null);
                }
                mVoiceRecognitionStarted = false;
            }
            if (!mSystemInterface.deactivateVoiceRecognition()) {
                Log.w(TAG, "stopVoiceRecognitionByHeadset: failed request from " + fromDevice);
                return false;
            }
            return true;
        }
    }

    void phoneStateChanged(int numActive, int numHeld, int callState, String number,
            int type, String name, boolean isVirtualCall) {
        synchronized (mStateMachines) {
            if (mStateMachinesThread == null) {
                Log.w(TAG, "mStateMachinesThread is null, returning");
                return;
            }
            // Should stop all other audio mode in this case
            if ((numActive + numHeld) > 0 || callState != HeadsetHalConstants.CALL_STATE_IDLE) {
                if (!isVirtualCall && mVirtualCallStarted) {
                    // stop virtual voice call if there is an incoming Telecom call update
                    if (ApmConstIntf.getQtiLeAudioEnabled()) {
                        CallAudioIntf mCallAudio = CallAudioIntf.get();
                       if (mCallAudio != null) {
                           mCallAudio.stopScoUsingVirtualVoiceCall();
                       }
                    } else {
                        stopScoUsingVirtualVoiceCall();
                        // send delayed message for all connected devices
                        doForEachConnectedStateMachine(
                             stateMachine -> stateMachine.sendMessageDelayed(
                             HeadsetStateMachine.SEND_CLCC_RESP_AFTER_VOIP_CALL, 100));
                    }
                }
                if (mVoiceRecognitionStarted) {
                    // stop voice recognition if there is any incoming call
                    stopVoiceRecognition(mActiveDevice);
                }
            } else {
                // ignore CS non-call state update when virtual call started
                if (!isVirtualCall && mVirtualCallStarted) {
                    Log.i(TAG, "Ignore CS non-call state update");
                    return;
                }
            }
            if (mDialingOutTimeoutEvent != null) {
                // Send result to state machine when dialing starts
                if (callState == HeadsetHalConstants.CALL_STATE_DIALING) {
                    mStateMachinesThread.getThreadHandler()
                            .removeCallbacks(mDialingOutTimeoutEvent);
                    if ((ApmConstIntf.getQtiLeAudioEnabled()) || (ApmConstIntf.getAospLeaEnabled())) {
                        Log.d(TAG, "Adv Audio enabled: updateOriginateResult");
                        CallControlIntf mCallControl = CallControlIntf.get();
                        mCallControl.updateOriginateResult(mDialingOutTimeoutEvent.mDialingOutDevice, 0, 1/* success */);
                    }
                    doForStateMachine(mDialingOutTimeoutEvent.mDialingOutDevice,
                            stateMachine -> stateMachine.sendMessage(
                                    HeadsetStateMachine.DIALING_OUT_RESULT, 1 /* success */, 0,
                                    mDialingOutTimeoutEvent.mDialingOutDevice));
                } else if (callState == HeadsetHalConstants.CALL_STATE_ACTIVE
                        || callState == HeadsetHalConstants.CALL_STATE_IDLE) {
                    // Clear the timeout event when the call is connected or disconnected
                    if (!mStateMachinesThread.getThreadHandler()
                            .hasCallbacks(mDialingOutTimeoutEvent)) {
                        mDialingOutTimeoutEvent = null;
                    }
                }
            }
            List<BluetoothDevice> availableDevices =
                        getDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
            if(availableDevices.size() > 0) {
                Log.i(TAG, "Update the phoneStateChanged status to connecting and " +
                           "connected devices");
                 mStateMachinesThread.getThreadHandler().post(() -> {
                    mSystemInterface.getHeadsetPhoneState().setNumActiveCall(numActive);
                    mSystemInterface.getHeadsetPhoneState().setNumHeldCall(numHeld);
                    mSystemInterface.getHeadsetPhoneState().setCallState(callState);
                    doForEachConnectedConnectingStateMachine(
                   stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.CALL_STATE_CHANGED,
                        new HeadsetCallState(numActive, numHeld, callState, number, type, name)));
                    if (!(mSystemInterface.isInCall() || mSystemInterface.isRinging()
                       || isAudioOn())) {
                        Log.i(TAG, "no call, sending resume A2DP message to state machines");
                        for (BluetoothDevice device : availableDevices) {
                            HeadsetStateMachine stateMachine = mStateMachines.get(device);
                            if (stateMachine == null) {
                                Log.w(TAG, "phoneStateChanged: device " + device +
                                       " was never connected/connecting");
                                continue;
                            }
                            stateMachine.sendMessage(HeadsetStateMachine.RESUME_A2DP);
                        }
                    }
                });
            } else {
                  mStateMachinesThread.getThreadHandler().post(() -> {
                    mSystemInterface.getHeadsetPhoneState().setNumActiveCall(numActive);
                    mSystemInterface.getHeadsetPhoneState().setNumHeldCall(numHeld);
                    mSystemInterface.getHeadsetPhoneState().setCallState(callState);
                    if (!(mSystemInterface.isInCall() || mSystemInterface.isRinging())) {
                        //If no device is connected, resume A2DP if there is no call
                        Log.i(TAG, "No device is connected and no call, " +
                                   "set A2DPsuspended to false");
                        mHfpA2dpSyncInterface.releaseA2DP(null);
                    } else {
                        //if call/ ring is ongoing, suspendA2DP to true
                        Log.i(TAG, "No device is connected and call/ring is ongoing, " +
                                   "set A2DPsuspended to true");
                        mHfpA2dpSyncInterface.suspendA2DP(HeadsetA2dpSync.
                                                      A2DP_SUSPENDED_BY_CS_CALL, null);
                }
             });
           }
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    private void clccResponse(int index, int direction, int status, int mode, boolean mpty,
            String number, int type) {
        synchronized (mStateMachines) {
           doForEachConnectedStateMachine(
                stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.SEND_CCLC_RESPONSE,
                        new HeadsetClccResponse(index, direction, status, mode, mpty, number,
                                type)));
        }
    }

    private boolean sendVendorSpecificResultCode(BluetoothDevice device, String command,
            String arg) {

        synchronized (mStateMachines) {
            final HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "sendVendorSpecificResultCode: device " + device
                        + " was never connected/connecting");
                return false;
            }
            int connectionState = stateMachine.getConnectionState();
            if (connectionState != BluetoothProfile.STATE_CONNECTED) {
                return false;
            }
            // Currently we support only "+ANDROID".
            if (!command.equals(BluetoothHeadset.VENDOR_RESULT_CODE_COMMAND_ANDROID)) {
                Log.w(TAG, "Disallowed unsolicited result code command: " + command);
                return false;
            }
            stateMachine.sendMessage(HeadsetStateMachine.SEND_VENDOR_SPECIFIC_RESULT_CODE,
                    new HeadsetVendorSpecificResultCode(device, command, arg));
        }
        return true;
    }

    boolean isInbandRingingEnabled() {
        boolean returnVal;

        boolean isInbandRingingSupported = getResources().getBoolean(
                com.android.bluetooth.R.bool.config_bluetooth_hfp_inband_ringing_support);

        returnVal = isInbandRingingSupported && !SystemProperties.getBoolean(
                DISABLE_INBAND_RINGING_PROPERTY, true) && !mInbandRingingRuntimeDisable;
        Log.d(TAG, "isInbandRingingEnabled returning: " + returnVal);
        return returnVal;
    }

    /**
     * Called from {@link HeadsetStateMachine} in state machine thread when there is a connection
     * state change
     *
     * @param device remote device
     * @param fromState from which connection state is the change
     * @param toState to which connection state is the change
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public void onConnectionStateChangedFromStateMachine(BluetoothDevice device, int fromState,
            int toState) {
        synchronized (mStateMachines) {
            List<BluetoothDevice> audioConnectableDevices =
                                            getConnectedDevices();
            if (fromState != BluetoothProfile.STATE_CONNECTED
                    && toState == BluetoothProfile.STATE_CONNECTED) {
                boolean isInbandRingingSupported = getResources().getBoolean(
                        com.android.bluetooth.R.bool.config_bluetooth_hfp_inband_ringing_support);
                if (audioConnectableDevices.size() > 1 && isInbandRingingSupported &&
                     !SystemProperties.getBoolean(DISABLE_INBAND_RINGING_PROPERTY, true) ) {
                    mInbandRingingRuntimeDisable = true;
                    doForEachConnectedStateMachine(
                            stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.SEND_BSIR,
                                    0));
                }
                MetricsLogger.logProfileConnectionEvent(BluetoothMetricsProto.ProfileId.HEADSET);
            }
            if (fromState != BluetoothProfile.STATE_DISCONNECTED
                    && toState == BluetoothProfile.STATE_DISCONNECTED) {
                if (audioConnectableDevices.size() <= 1 ) {
                    mInbandRingingRuntimeDisable = false;
                }
                if (device.equals(mActiveDevice)) {
                    AdapterService adapterService = AdapterService.getAdapterService();
                    if (adapterService.isTwsPlusDevice(device)) {
                        //if the disconnected device is a tws+ device
                        // and if peer device is connected, set the peer
                        // as an active device
                        BluetoothDevice peerDevice = getTwsPlusConnectedPeer(device);
                        if (peerDevice != null) {
                            setActiveDevice(peerDevice);
                        }
                    }else if (!ApmConstIntf.getQtiLeAudioEnabled()) {
                        setActiveDevice(null);
                    }
                }
            }
        }

        // if active device is null, SLC connected, make this device as active.
        /*if (!ApmConstIntf.getQtiLeAudioEnabled()) {
          if (fromState == BluetoothProfile.STATE_CONNECTING &&
             toState == BluetoothProfile.STATE_CONNECTED &&
             mActiveDevice == null) {
             Log.i(TAG, "onConnectionStateChangedFromStateMachine: SLC connected, no active"
                              + " is present. Setting active device to " + device);
             setActiveDevice(device);
         }
      }*/
    }

    public HeadsetA2dpSync getHfpA2DPSyncInterface(){
        return mHfpA2dpSyncInterface;
    }

    public void sendA2dpStateChangeUpdate(int state) {
        Log.d(TAG," sendA2dpStateChange newState = " + state);
        doForEachConnectedConnectingStateMachine(
              stateMachine -> stateMachine.sendMessage(HeadsetStateMachine.A2DP_STATE_CHANGED,
                                    state));
    }

    /**
     * Check if no audio mode is active
     *
     * @return false if virtual call, voice recognition, or Telecom call is active, true if all idle
     */
    private boolean isAudioModeIdle() {
        synchronized (mStateMachines) {
            if (mVoiceRecognitionStarted || mVirtualCallStarted || !mSystemInterface.isCallIdle()) {
                Log.i(TAG, "isAudioModeIdle: not idle, mVoiceRecognitionStarted="
                        + mVoiceRecognitionStarted + ", mVirtualCallStarted=" + mVirtualCallStarted
                        + ", isCallIdle=" + mSystemInterface.isCallIdle());
                return false;
            }
            return true;
        }
    }

    public boolean shouldCallAudioBeActive() {
        boolean retVal = false;
        // When the call is active/held, the call audio must be active
        if (mSystemInterface.getHeadsetPhoneState().getNumActiveCall() > 0 ||
            mSystemInterface.getHeadsetPhoneState().getNumHeldCall() > 0 ) {
            Log.d(TAG, "shouldCallAudioBeActive(): returning true, since call is active/held");
            return true;
        }
        // When call is in  ringing state, SCO should not be accepted if
        // in-band ringtone is not enabled
        retVal = (mSystemInterface.isInCall() && !mSystemInterface.isRinging() )||
                 (mSystemInterface.isRinging() && isInbandRingingEnabled());
        Log.d(TAG, "shouldCallAudioBeActive() returning " + retVal);
        return retVal;
    }

    /**
     * Only persist audio during active device switch when call audio is supposed to be active and
     * virtual call has not been started. Virtual call is ignored because AudioService and
     * applications should reconnect SCO during active device switch and forcing SCO connection
     * here will make AudioService think SCO is started externally instead of by one of its SCO
     * clients.
     *
     * @return true if call audio should be active and no virtual call is going on
     */
    private boolean shouldPersistAudio() {
        return !mVirtualCallStarted && shouldCallAudioBeActive();
    }

    /**
     * Called from {@link HeadsetStateMachine} in state machine thread when there is a audio
     * connection state change
     *
     * @param device remote device
     * @param fromState from which audio connection state is the change
     * @param toState to which audio connection state is the change
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public void onAudioStateChangedFromStateMachine(BluetoothDevice device, int fromState,
            int toState) {
        Log.w(TAG, "onAudioStateChangedFromStateMachine: " + fromState + "->" + toState);
        synchronized (mStateMachines) {
            if (toState == BluetoothHeadset.STATE_AUDIO_DISCONNECTED) {
                if (mSHOStatus) {
                   ActiveDeviceManagerServiceIntf mActiveDeviceManager =
                         ActiveDeviceManagerServiceIntf.get();
                   mActiveDeviceManager.onActiveDeviceChange(mTempActiveDevice,
                                           ApmConstIntf.AudioFeatures.CALL_AUDIO);
                   mSHOStatus = false;
                }
                //Transfer SCO is not needed for TWS+ devices
                if (mAdapterService != null && mAdapterService.isTwsPlusDevice(device) &&
                   mActiveDevice != null &&
                   mAdapterService.isTwsPlusDevice(mActiveDevice) &&
                   isAudioOn()) {
                    Log.w(TAG, "Sco transfer is not needed btween earbuds");
                } else {
                    // trigger SCO after SCO disconnected with previous active
                    // device
                    Log.w(TAG, "onAudioStateChangedFromStateMachine:"
                            + "shouldPersistAudio() returns"
                            + shouldPersistAudio());
                    if (mAdapterService != null && mAdapterService.isTwsPlusDevice(device) &&
                                   isAudioOn()) {
                        Log.w(TAG, "TWS: Don't stop VR or VOIP");
                    } else {
                        if (mVoiceRecognitionStarted) {
                            if (!stopVoiceRecognitionByHeadset(device)) {
                                Log.w(TAG,"onAudioStateChangedFromStateMachine:"
                                    + " failed to stop voice"
                                    + " recognition");
                            } else {
                                final HeadsetStateMachine stateMachine
                                                  = mStateMachines.get(device);
                                if (stateMachine != null) {
                                Log.d(TAG, "onAudioStateChanged" +
                                        "FromStateMachine: send +bvra:0");
                                  stateMachine.sendMessage(
                                    HeadsetStateMachine.VOICE_RECOGNITION_STOP,
                                    device);
                                }
                            }
                        }
                        if (mVirtualCallStarted) {
                            if(ApmConstIntf.getQtiLeAudioEnabled()) {
                                CallAudioIntf mCallAudio = CallAudioIntf.get();
                                if(mCallAudio != null) {
                                    mCallAudio.stopScoUsingVirtualVoiceCall();
                                }
                            } else {
                                if (!stopScoUsingVirtualVoiceCall()) {
                                    Log.w(TAG,"onAudioStateChangedFromStateMachine:"
                                        + " failed to stop virtual "
                                        + " voice call");
                                }
                            }
                        }
                    }

                    if (mActiveDevice != null &&
                                 !mActiveDevice.equals(device) &&
                                 shouldPersistAudio()) {
                       if (mAdapterService != null && mAdapterService.isTwsPlusDevice(device)
                                                   && isAudioOn()) {
                           Log.d(TAG, "TWS: Wait for both eSCO closed");
                       } else {
                           if (mAdapterService != null && mAdapterService.isTwsPlusDevice(device) &&
                               isTwsPlusActive(mActiveDevice)) {
                               /* If the device for which SCO got disconnected
                                  is a TwsPlus device and TWS+ set is active
                                  device. This should be the case where User
                                  transferred from BT to Phone Speaker from
                                  Call UI*/
                                  Log.d(TAG,"don't transfer SCO. It is an" +
                                             "explicit voice transfer from UI");
                                  return;
                           }
                           Log.d(TAG, "onAudioStateChangedFromStateMachine:"
                              + " triggering SCO with device "
                              + mActiveDevice);
                           int connectStatus = connectAudio(mActiveDevice);
                           if (connectStatus != BluetoothStatusCodes.SUCCESS) {
                               Log.w(TAG, "onAudioStateChangedFromStateMachine,"
                               + " failed to connect"
                               + " audio to new " + "active device "
                               + mActiveDevice
                               + ", after " + device
                               + " is disconnected from SCO");
                           }
                       }
                    }
                }
            }
        }
    }

    private void broadcastActiveDevice(BluetoothDevice device) {
        logD("broadcastActiveDevice: " + device);
        BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_ACTIVE_DEVICE_CHANGED,
                BluetoothProfile.HEADSET, mAdapterService.obfuscateAddress(device), 0);
        Intent intent = new Intent(BluetoothHeadset.ACTION_ACTIVE_DEVICE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        sendBroadcastAsUser(intent, UserHandle.ALL, BLUETOOTH_CONNECT,
                Utils.getTempAllowlistBroadcastOptions());
    }

    /**
     * Check whether it is OK to accept a headset connection from a remote device
     *
     * @param device remote device that initiates the connection
     * @return true if the connection is acceptable
     */
    public boolean okToAcceptConnection(BluetoothDevice device) {
        // Check if this is an incoming connection in Quiet mode.
        boolean isPts = SystemProperties.getBoolean("vendor.bt.pts.certification", false);
        if (mAdapterService.isQuietModeEnabled()) {
            Log.w(TAG, "okToAcceptConnection: return false as quiet mode enabled");
            return false;
        }
        if(!isPts) {
            // Check priority and accept or reject the connection.
            // Note: Logic can be simplified, but keeping it this way for readability
            int connectionPolicy = getConnectionPolicy(device);
            int bondState = mAdapterService.getBondState(device);
            // If priority is undefined, it is likely that service discovery has not completed and peer
            // initiated the connection. Allow this connection only if the device is bonded or bonding
            boolean serviceDiscoveryPending = (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_UNKNOWN) && (
                    bondState == BluetoothDevice.BOND_BONDING
                            || bondState == BluetoothDevice.BOND_BONDED);
            // Also allow connection when device is bonded/bonding and priority is ON/AUTO_CONNECT.
            boolean isEnabled = (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED
                    || connectionPolicy == BluetoothProfile.PRIORITY_AUTO_CONNECT) && (
                    bondState == BluetoothDevice.BOND_BONDED
                            || bondState == BluetoothDevice.BOND_BONDING);
            if (!serviceDiscoveryPending && !isEnabled) {
                // Otherwise, reject the connection if no service discovery is pending and priority is
                // neither PRIORITY_ON nor PRIORITY_AUTO_CONNECT
                Log.w(TAG,
                        "okToConnect: return false, connection policy = " + connectionPolicy + ", bondState=" + bondState);
                return false;
            }
        // Check connection policy and accept or reject the connection.
        // Allow this connection only if the device is bonded. Any attempt to connect while
        // bonding would potentially lead to an unauthorized connection.
            // Otherwise, reject the connection if connection policy is not valid.
        }
        List<BluetoothDevice> connectingConnectedDevices =
                getAllDevicesMatchingConnectionStates(CONNECTING_CONNECTED_STATES);
        if (!isConnectionAllowed(device, connectingConnectedDevices)) {
            Log.w(TAG, "Maximum number of connections " + mSetMaxConfig
                    + " was reached, rejecting connection from " + device);
            return false;
        }
        return true;
    }
    /**
     * Checks if SCO should be connected at current system state. Returns
     * {@link BluetoothStatusCodes#SUCCESS} if SCO is allowed to be connected or an error code on
     * failure.
     *
     * @param device device for SCO to be connected
     * @return whether SCO can be connected
     */
    public int isScoAcceptable(BluetoothDevice device) {
        synchronized (mStateMachines) {
            //allow 2nd eSCO from non-active tws+ earbud as well
            if (!mAdapterService.isTwsPlusDevice(device)) {
                if (device == null || !device.equals(mActiveDevice)) {
                    Log.w(TAG, "isScoAcceptable: rejected SCO since " + device
                        + " is not the current active device " + mActiveDevice);
                    return BluetoothStatusCodes.ERROR_NOT_ACTIVE_DEVICE;
                }
            }
            if (mForceScoAudio) {
                return BluetoothStatusCodes.SUCCESS;
            }
            if (!mAudioRouteAllowed) {
                Log.w(TAG, "isScoAcceptable: rejected SCO since audio route is not allowed");
                return BluetoothStatusCodes.ERROR_AUDIO_ROUTE_BLOCKED;
            }
            /* if in-band ringtone is not enabled and if there is
                no active/held/dialling/alerting call, return false */
            if (isRinging() && !isInbandRingingEnabled() &&
                !(mSystemInterface.getHeadsetPhoneState().getNumActiveCall() > 0 ||
                  mSystemInterface.getHeadsetPhoneState().getNumHeldCall() > 0 )) {
                Log.w(TAG, "isScoAcceptable: rejected SCO since MT call in ringing," +
                            "in-band ringing not enabled");
                return BluetoothStatusCodes.ERROR_CALL_ACTIVE;
            }
            if (mVoiceRecognitionStarted || mVirtualCallStarted) {
                return BluetoothStatusCodes.SUCCESS;
            }
            if (shouldCallAudioBeActive()) {
                return BluetoothStatusCodes.SUCCESS;
            }
            Log.w(TAG, "isScoAcceptable: rejected SCO, inCall=" + mSystemInterface.isInCall()
                    + ", voiceRecognition=" + mVoiceRecognitionStarted + ", ringing="
                    + mSystemInterface.isRinging() + ", inbandRinging=" + isInbandRingingEnabled()
                    + ", isVirtualCallStarted=" + mVirtualCallStarted);
            return BluetoothStatusCodes.ERROR_CALL_ACTIVE;
        }
    }

    /**
     * Remove state machine in {@link #mStateMachines} for a {@link BluetoothDevice}
     *
     * @param device device whose state machine is to be removed.
     */
    void removeStateMachine(BluetoothDevice device) {
        synchronized (mStateMachines) {
            HeadsetStateMachine stateMachine = mStateMachines.get(device);
            if (stateMachine == null) {
                Log.w(TAG, "removeStateMachine(), " + device + " does not have a state machine");
                return;
            }
            Log.i(TAG, "removeStateMachine(), removing state machine for device: " + device);
            HeadsetObjectsFactory.getInstance().destroyStateMachine(stateMachine);
            mStateMachines.remove(device);
        }
    }

    private boolean isOnStateMachineThread() {
        final Looper myLooper = Looper.myLooper();
        return myLooper != null && (mStateMachinesThread != null) && (myLooper.getThread().getId()
                == mStateMachinesThread.getId());
    }

    @Override
    public void dump(StringBuilder sb) {
        boolean isScoOn = mSystemInterface.getAudioManager().isBluetoothScoOn();
        synchronized (mStateMachines) {
            super.dump(sb);
            ProfileService.println(sb, "mMaxHeadsetConnections: " + mMaxHeadsetConnections);
            ProfileService.println(sb, "DefaultMaxHeadsetConnections: "
                    + mAdapterService.getMaxConnectedAudioDevices());
            ProfileService.println(sb, "mActiveDevice: " + mActiveDevice);
            ProfileService.println(sb, "isInbandRingingEnabled: " + isInbandRingingEnabled());
            boolean isInbandRingingSupported = getResources().getBoolean(
                    com.android.bluetooth.R.bool.config_bluetooth_hfp_inband_ringing_support);
            ProfileService.println(sb,
                    "isInbandRingingSupported: " + isInbandRingingSupported);
            ProfileService.println(sb,
                    "mInbandRingingRuntimeDisable: " + mInbandRingingRuntimeDisable);
            ProfileService.println(sb, "mAudioRouteAllowed: " + mAudioRouteAllowed);
            ProfileService.println(sb, "mVoiceRecognitionStarted: " + mVoiceRecognitionStarted);
            ProfileService.println(sb,
                    "mVoiceRecognitionTimeoutEvent: " + mVoiceRecognitionTimeoutEvent);
            ProfileService.println(sb, "mVirtualCallStarted: " + mVirtualCallStarted);
            ProfileService.println(sb, "mDialingOutTimeoutEvent: " + mDialingOutTimeoutEvent);
            ProfileService.println(sb, "mForceScoAudio: " + mForceScoAudio);
            ProfileService.println(sb, "mCreated: " + mCreated);
            ProfileService.println(sb, "mStarted: " + mStarted);
            ProfileService.println(sb, "AudioManager.isBluetoothScoOn(): " + isScoOn);
            ProfileService.println(sb, "Telecom.isInCall(): " + mSystemInterface.isInCall());
            ProfileService.println(sb, "Telecom.isRinging(): " + mSystemInterface.isRinging());
            for (HeadsetStateMachine stateMachine : mStateMachines.values()) {
                ProfileService.println(sb,
                        "==== StateMachine for " + stateMachine.getDevice() + " ====");
                stateMachine.dump(sb);
            }
        }
    }

    public void enableSwbCodec(boolean enable) {
        mVendorHf.enableSwb(enable);
    }

    public boolean isSwbEnabled() {
    if(mAdapterService.isSWBVoicewithAptxAdaptiveAG()) {
            return mAdapterService.isSwbEnabled();
        }
        return false;
    }

    public boolean isSwbPmEnabled() {
        if(mAdapterService.isSWBVoicewithAptxAdaptiveAG() &&
           mAdapterService.isSwbEnabled()) {
            return mAdapterService.isSwbPmEnabled();
        }
        return false;
    }

    private static void logD(String message) {
        if (DBG) {
            Log.d(TAG, message);
        }
    }
}
