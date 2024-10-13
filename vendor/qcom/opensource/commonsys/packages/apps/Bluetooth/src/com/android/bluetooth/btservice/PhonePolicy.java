
/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.bluetooth.btservice;

import android.annotation.RequiresPermission;
import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothA2dpSink;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothCsipSetCoordinator;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothHearingAid;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.BluetoothLeAudio;
import android.bluetooth.BluetoothVolumeControl;
import android.bluetooth.DeviceGroup;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.Parcelable;
import android.os.SystemProperties;
import android.util.Log;

import com.android.bluetooth.le_audio.LeAudioService;
import com.android.bluetooth.a2dp.A2dpService;
import com.android.bluetooth.a2dpsink.A2dpSinkService;
import com.android.bluetooth.apm.ApmConstIntf;
import com.android.bluetooth.apm.MediaAudioIntf;
import com.android.bluetooth.apm.CallAudioIntf;
import com.android.bluetooth.groupclient.GroupService;
import android.bluetooth.DeviceGroup;
import com.android.bluetooth.btservice.InteropUtil;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.bluetooth.CsipWrapper;
import com.android.bluetooth.csip.CsipSetCoordinatorService;
import com.android.bluetooth.hearingaid.HearingAidService;
import com.android.bluetooth.hfp.HeadsetService;
import com.android.bluetooth.hid.HidHostService;
import com.android.bluetooth.lebroadcast.BassClientService;
import com.android.bluetooth.pan.PanService;
import com.android.bluetooth.vc.VolumeControlService;
import com.android.bluetooth.ba.BATService;
import com.android.internal.R;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.ArrayUtils;

///*_REF
import java.lang.reflect.*;
//_REF*/

import java.util.concurrent.CopyOnWriteArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.ArrayList;
import java.util.Objects;

// Describes the phone policy
//
// The policy should be as decoupled from the stack as possible. In an ideal world we should not
// need to have this policy talk with any non-public APIs and one way to enforce that would be to
// keep this file outside the Bluetooth process. Unfortunately, keeping a separate process alive is
// an expensive and a tedious task.
//
// Best practices:
// a) PhonePolicy should be ALL private methods
//    -- Use broadcasts which can be listened in on the BroadcastReceiver
// b) NEVER call from the PhonePolicy into the Java stack, unless public APIs. It is OK to call into
// the non public versions as long as public versions exist (so that a 3rd party policy can mimick)
// us.
//
// Policy description:
//
// Policies are usually governed by outside events that may warrant an action. We talk about various
// events and the resulting outcome from this policy:
//
// 1. Adapter turned ON: At this point we will try to auto-connect the (device, profile) pairs which
// have PRIORITY_AUTO_CONNECT. The fact that we *only* auto-connect Headset and A2DP is something
// that is hardcoded and specific to phone policy (see autoConnect() function)
// 2. When the profile connection-state changes: At this point if a new profile gets CONNECTED we
// will try to connect other profiles on the same device. This is to avoid collision if devices
// somehow end up trying to connect at same time or general connection issues.
class PhonePolicy {
    private static final boolean DBG = true;
    private static final String TAG = "BluetoothPhonePolicy";

    // Message types for the handler (internal messages generated by intents or timeouts)
    private static final int MESSAGE_PROFILE_CONNECTION_STATE_CHANGED = 1;
    private static final int MESSAGE_PROFILE_INIT_PRIORITIES = 2;
    private static final int MESSAGE_CONNECT_OTHER_PROFILES = 3;
    private static final int MESSAGE_ADAPTER_STATE_TURNED_ON = 4;
    private static final int MESSAGE_AUTO_CONNECT_PROFILES = 50;

    // Timeouts
    private static final int AUTO_CONNECT_PROFILES_TIMEOUT= 500;
    @VisibleForTesting static int sConnectOtherProfilesTimeoutMillis = 6000; // 6s
    private static final int CONNECT_OTHER_PROFILES_TIMEOUT_DELAYED = 10000; //10s
    private static final int CONNECT_OTHER_PROFILES_REDUCED_TIMEOUT_DELAYED = 2000; //2s

    private static final int MESSAGE_PROFILE_ACTIVE_DEVICE_CHANGED = 5;
    private static final int MESSAGE_DEVICE_CONNECTED = 6;

    private static final String PREFER_LE_AUDIO_ONLY_MODE =
            "persist.bluetooth.prefer_le_audio_only_mode";

    private static final int INVALID_SET_ID = 0x10;

    private DatabaseManager mDatabaseManager;
    private final AdapterService mAdapterService;
    private final ServiceFactory mFactory;
    private final Handler mHandler;
    private final HashSet<BluetoothDevice> mHeadsetRetrySet = new HashSet<>();
    private final HashSet<BluetoothDevice> mA2dpRetrySet = new HashSet<>();
    private final HashSet<BluetoothDevice> mConnectOtherProfilesDeviceSet = new HashSet<>();

    private Boolean mPreferLeAudioOnlyMode = false;
    ///*_REF
    Object mBCService = null;
    Method mBCGetService = null;
    Method mBCGetConnPolicy = null;
    Method mBCSetConnPolicy = null;
    Method mBCConnect = null;
    Method mBCDisconnect = null;
    Method mBCGetConnState = null;
    String mBCId = null;
    private static final String BC_ACTION_CONNECTION_STATE_CHANGED =
        "android.bluetooth.bc.profile.action.CONNECTION_STATE_CHANGED";
    //_REF*/
    Object mBroadcastService = null;
    Method mBroadcastGetAddr = null;
    Method mBroadcastIsActive = null;
    private String broadcastBDA = null;
    private void initBCReferences() {
        if (mAdapterService != null) {
            mBCService = mAdapterService.getBCService();
            mBCGetConnPolicy = mAdapterService.getBCGetConnPolicy();
            mBCSetConnPolicy = mAdapterService.getBCSetConnPolicy();
            mBCConnect = mAdapterService.getBCConnect();
            mBCId = mAdapterService.getBCId();
        }
    }

    // Broadcast receiver for all changes to states of various profiles
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action == null) {
                errorLog("Received intent with null action");
                return;
            }
            switch (action) {
                case BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_CONNECTION_STATE_CHANGED,
                            BluetoothProfile.HEADSET, -1, // No-op argument
                            intent).sendToTarget();
                    break;
                case BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_CONNECTION_STATE_CHANGED,
                            BluetoothProfile.A2DP, -1, // No-op argument
                            intent).sendToTarget();
                    break;
                case BluetoothCsipSetCoordinator.ACTION_CSIS_CONNECTION_STATE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_CONNECTION_STATE_CHANGED,
                            BluetoothProfile.CSIP_SET_COORDINATOR, -1, // No-op argument
                            intent).sendToTarget();
                    break;
                case BluetoothA2dpSink.ACTION_CONNECTION_STATE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_CONNECTION_STATE_CHANGED,
                            BluetoothProfile.A2DP_SINK,-1, // No-op argument
                            intent).sendToTarget();
                    break;
                case BluetoothA2dp.ACTION_ACTIVE_DEVICE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_ACTIVE_DEVICE_CHANGED,
                            BluetoothProfile.A2DP, -1, // No-op argument
                            intent).sendToTarget();
                    break;
                case BluetoothHeadset.ACTION_ACTIVE_DEVICE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_ACTIVE_DEVICE_CHANGED,
                            BluetoothProfile.HEADSET, -1, // No-op argument
                            intent).sendToTarget();
                    break;
                case BluetoothHearingAid.ACTION_ACTIVE_DEVICE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_ACTIVE_DEVICE_CHANGED,
                            BluetoothProfile.HEARING_AID, -1, // No-op argument
                            intent).sendToTarget();
                    break;
                case BluetoothAdapter.ACTION_STATE_CHANGED:
                    // Only pass the message on if the adapter has actually changed state from
                    // non-ON to ON. NOTE: ON is the state depicting BREDR ON and not just BLE ON.
                    int newState = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1);
                    if (newState == BluetoothAdapter.STATE_ON) {
                        mHandler.obtainMessage(MESSAGE_ADAPTER_STATE_TURNED_ON).sendToTarget();
                    }
                    break;
                case BluetoothDevice.ACTION_UUID:
                    mHandler.obtainMessage(MESSAGE_PROFILE_INIT_PRIORITIES, intent).sendToTarget();
                    break;
                case BluetoothDevice.ACTION_ACL_CONNECTED:
                    mHandler.obtainMessage(MESSAGE_DEVICE_CONNECTED, intent).sendToTarget();
                    break;
                case BC_ACTION_CONNECTION_STATE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_CONNECTION_STATE_CHANGED,
                            BluetoothProfile.BC_PROFILE,-1, // No-op argument
                            intent).sendToTarget();
                    break;
                case BluetoothLeAudio.ACTION_LE_AUDIO_CONNECTION_STATE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_CONNECTION_STATE_CHANGED,
                            BluetoothProfile.LE_AUDIO, -1, // No-op argument
                            intent).sendToTarget();
                    break;
                case BluetoothLeAudio.ACTION_LE_AUDIO_ACTIVE_DEVICE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_ACTIVE_DEVICE_CHANGED,
                            BluetoothProfile.LE_AUDIO, -1, // No-op argument
                            intent).sendToTarget();
                    break;
                case BluetoothVolumeControl.ACTION_CONNECTION_STATE_CHANGED:
                    mHandler.obtainMessage(MESSAGE_PROFILE_CONNECTION_STATE_CHANGED,
                            BluetoothProfile.VOLUME_CONTROL, -1, // No-op argument
                            intent).sendToTarget();
                    break;
                default:
                    Log.e(TAG, "Received unexpected intent, action=" + action);
                    break;
            }
        }
    };

    @VisibleForTesting
    BroadcastReceiver getBroadcastReceiver() {
        return mReceiver;
    }

    // Handler to handoff intents to class thread
    class PhonePolicyHandler extends Handler {
        PhonePolicyHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MESSAGE_PROFILE_INIT_PRIORITIES: {
                    Intent intent = (Intent) msg.obj;
                    BluetoothDevice device =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    Parcelable[] uuids = intent.getParcelableArrayExtra(BluetoothDevice.EXTRA_UUID);
                    debugLog("Received ACTION_UUID for device " + device);
                    if (uuids != null) {
                        ParcelUuid[] uuidsToSend = new ParcelUuid[uuids.length];
                        for (int i = 0; i < uuidsToSend.length; i++) {
                            uuidsToSend[i] = (ParcelUuid) uuids[i];
                            debugLog("index=" + i + "uuid=" + uuidsToSend[i]);
                        }
                        processInitProfilePriorities(device, uuidsToSend);
                    }
                }
                break;

                case MESSAGE_PROFILE_CONNECTION_STATE_CHANGED: {
                    Intent intent = (Intent) msg.obj;
                    BluetoothDevice device =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    if (device.getAddress().equals(BATService.mBAAddress)) {
                        Log.d(TAG," Update from BA, bail out");
                        break;
                    }
                    int prevState = intent.getIntExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, -1);
                    int nextState = intent.getIntExtra(BluetoothProfile.EXTRA_STATE, -1);
                    processProfileStateChanged(device, msg.arg1, nextState, prevState);
                }
                break;

                case MESSAGE_PROFILE_ACTIVE_DEVICE_CHANGED: {
                    Intent intent = (Intent) msg.obj;
                    BluetoothDevice activeDevice =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    processActiveDeviceChanged(activeDevice, msg.arg1);
                }
                break;

                case MESSAGE_CONNECT_OTHER_PROFILES: {
                    // Called when we try connect some profiles in processConnectOtherProfiles but
                    // we send a delayed message to try connecting the remaining profiles
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    processConnectOtherProfiles(device);
                    mConnectOtherProfilesDeviceSet.remove(device);
                    break;
                }
                case MESSAGE_ADAPTER_STATE_TURNED_ON:
                    // Call auto connect when adapter switches state to ON
                    resetStates();
                    autoConnect();
                    break;
                case MESSAGE_AUTO_CONNECT_PROFILES: {
                    if (DBG) debugLog( "MESSAGE_AUTO_CONNECT_PROFILES");
                    autoConnectProfilesDelayed();
                    break;
                }
                case MESSAGE_DEVICE_CONNECTED:
                    Intent intent = (Intent) msg.obj;
                    BluetoothDevice device =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                    processDeviceConnected(device);
            }
        }
    }

    ;

    // Policy API functions for lifecycle management (protected)
    protected void start() {
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
        filter.addAction(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED);
        filter.addAction(BluetoothDevice.ACTION_ACL_CONNECTED);
        filter.addAction(BluetoothA2dpSink.ACTION_CONNECTION_STATE_CHANGED);
        filter.addAction(BluetoothDevice.ACTION_UUID);
        filter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
        filter.addAction(BluetoothA2dp.ACTION_ACTIVE_DEVICE_CHANGED);
        filter.addAction(BluetoothHeadset.ACTION_ACTIVE_DEVICE_CHANGED);
        filter.addAction(BluetoothHearingAid.ACTION_ACTIVE_DEVICE_CHANGED);
        filter.addAction(BC_ACTION_CONNECTION_STATE_CHANGED);
        filter.addAction(BluetoothLeAudio.ACTION_LE_AUDIO_CONNECTION_STATE_CHANGED);
        filter.addAction(BluetoothCsipSetCoordinator.ACTION_CSIS_CONNECTION_STATE_CHANGED);
        filter.addAction(BluetoothLeAudio.ACTION_LE_AUDIO_ACTIVE_DEVICE_CHANGED);
        filter.addAction(BluetoothVolumeControl.ACTION_CONNECTION_STATE_CHANGED);
        mAdapterService.registerReceiver(mReceiver, filter);
    }

    protected void cleanup() {
        mAdapterService.unregisterReceiver(mReceiver);
        resetStates();
    }

    PhonePolicy(AdapterService service, ServiceFactory factory) {
        mAdapterService = service;
        mDatabaseManager = Objects.requireNonNull(mAdapterService.getDatabase(),
                "DatabaseManager cannot be null when PhonePolicy starts");
        mFactory = factory;
        mHandler = new PhonePolicyHandler(service.getMainLooper());
        mPreferLeAudioOnlyMode = SystemProperties.getBoolean(PREFER_LE_AUDIO_ONLY_MODE, true);
    }

    // Policy implementation, all functions MUST be private
    @RequiresPermission(allOf = {
            android.Manifest.permission.BLUETOOTH_CONNECT,
            android.Manifest.permission.BLUETOOTH_PRIVILEGED,
    })
    private void processInitProfilePriorities(BluetoothDevice device, ParcelUuid[] uuids) {

        ParcelUuid ADV_AUDIO_T_MEDIA =
            ParcelUuid.fromString("00006AD0-0000-1000-8000-00805F9B34FB");

        ParcelUuid ADV_AUDIO_HEARINGAID =
            ParcelUuid.fromString("00006AD2-0000-1000-8000-00805F9B34FB");

        ParcelUuid ADV_AUDIO_P_MEDIA =
            ParcelUuid.fromString("00006AD1-0000-1000-8000-00805F9B34FB");

        ParcelUuid ADV_AUDIO_P_VOICE =
            ParcelUuid.fromString("00006AD4-0000-1000-8000-00805F9B34FB");

        ParcelUuid ADV_AUDIO_T_VOICE =
            ParcelUuid.fromString("00006AD5-0000-1000-8000-00805F9B34FB");

        ParcelUuid ADV_AUDIO_G_MEDIA =
            ParcelUuid.fromString("12994B7E-6d47-4215-8C9E-AAE9A1095BA3");

        ParcelUuid ADV_AUDIO_W_MEDIA =
            ParcelUuid.fromString("2587db3c-ce70-4fc9-935f-777ab4188fd7");

        ParcelUuid ADV_AUDIO_G_VBC =
            ParcelUuid.fromString("00006AD6-0000-1000-8000-00805F9B34FB");

        debugLog("processInitProfilePriorities() - device " + device);
        HidHostService hidService = mFactory.getHidHostService();
        A2dpService a2dpService = mFactory.getA2dpService();
        A2dpSinkService a2dpSinkService = mFactory.getA2dpSinkService();
        HeadsetService headsetService = mFactory.getHeadsetService();
        PanService panService = mFactory.getPanService();
        HearingAidService hearingAidService = mFactory.getHearingAidService();
        LeAudioService leAudioService = mFactory.getLeAudioService();
        CsipSetCoordinatorService csipSetCooridnatorService =
            mFactory.getCsipSetCoordinatorService();
        VolumeControlService volumeControlService = mFactory.getVolumeControlService();
        boolean isQtiLeAudioEnabled = ApmConstIntf.getQtiLeAudioEnabled();
        boolean isAospLeAudioEnabled = ApmConstIntf.getAospLeaEnabled();
        BassClientService bcService = mFactory.getBassClientService();

        BluetoothDevice peerTwsDevice = null;
        if (mAdapterService.isTwsPlusDevice(device)) {
            peerTwsDevice = mAdapterService.getTwsPlusPeerDevice(device);
        }

        // Set profile priorities only for the profiles discovered on the remote device.
        // This avoids needless auto-connect attempts to profiles non-existent on the remote device
        if ((hidService != null) && (ArrayUtils.contains(uuids, BluetoothUuid.HID)
                || ArrayUtils.contains(uuids, BluetoothUuid.HOGP)) && (
                hidService.getConnectionPolicy(device)
                        == BluetoothProfile.CONNECTION_POLICY_UNKNOWN)) {
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.HID_HOST, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        }

        // If we do not have a stored priority for HFP/A2DP (all roles) then default to on.
        if ((headsetService != null) && ((ArrayUtils.contains(uuids, BluetoothUuid.HSP)
                || ArrayUtils.contains(uuids, ADV_AUDIO_P_VOICE)
                || ArrayUtils.contains(uuids, ADV_AUDIO_T_VOICE)
                || ArrayUtils.contains(uuids, BluetoothUuid.HFP)) && (
                headsetService.getConnectionPolicy(device)
                        == BluetoothProfile.CONNECTION_POLICY_UNKNOWN))) {
            debugLog("setting peer device to connection policy on for hfp" + device);
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.HEADSET, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            if (peerTwsDevice != null) {
                debugLog("setting peer earbud to connection policy on for hfp" + peerTwsDevice);
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.HEADSET, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            }
        }

        if ((a2dpService != null) && (ArrayUtils.contains(uuids, BluetoothUuid.A2DP_SINK)
                || ArrayUtils.contains(uuids, BluetoothUuid.ADV_AUDIO_DIST)
                || ArrayUtils.contains(uuids, ADV_AUDIO_T_MEDIA)
                || ArrayUtils.contains(uuids, ADV_AUDIO_W_MEDIA)
                || ArrayUtils.contains(uuids, ADV_AUDIO_HEARINGAID)
                || ArrayUtils.contains(uuids, ADV_AUDIO_P_MEDIA)) && (
                a2dpService.getConnectionPolicy(device)
                        == BluetoothProfile.CONNECTION_POLICY_UNKNOWN)) {
            debugLog("setting peer device to connection policy on for a2dp" + device);
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.A2DP, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            if (peerTwsDevice != null) {
                debugLog("setting peer earbud to connection policy on for a2dp" + peerTwsDevice);
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.A2DP, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            }
        }

        if ((csipSetCooridnatorService != null)
                && (ArrayUtils.contains(uuids, BluetoothUuid.COORDINATED_SET))
                && (csipSetCooridnatorService.getConnectionPolicy(device)
                        == BluetoothProfile.CONNECTION_POLICY_UNKNOWN)) {
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.CSIP_SET_COORDINATOR,BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        }

        if ((a2dpSinkService != null)
                && (ArrayUtils.contains(uuids, BluetoothUuid.A2DP_SOURCE)
                || ArrayUtils.contains(uuids, BluetoothUuid.ADV_AUDIO_DIST)) && (
                a2dpSinkService.getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_UNKNOWN)) {
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.A2DP_SINK, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        }

        if ((panService != null) && (ArrayUtils.contains(uuids, BluetoothUuid.PANU) && (
                panService.getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_UNKNOWN)
                && mAdapterService.getResources()
                .getBoolean(com.android.bluetooth.R.bool.config_bluetooth_pan_enable_autoconnect))) {
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.PAN, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        }

        if ((hearingAidService != null) && ArrayUtils.contains(uuids,
                BluetoothUuid.HEARING_AID) && (hearingAidService.getConnectionPolicy(device)
                == BluetoothProfile.CONNECTION_POLICY_UNKNOWN)) {
            debugLog("setting hearing aid profile connection policy for device " + device);
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.HEARING_AID, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        }

        if (isAospLeAudioEnabled &&
            (leAudioService != null) && ArrayUtils.contains(uuids,
                BluetoothUuid.LE_AUDIO) && (leAudioService.getConnectionPolicy(device)
                == BluetoothProfile.CONNECTION_POLICY_UNKNOWN)) {
            debugLog("setting le audio profile priority for device " + device);
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.LE_AUDIO, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            if (mPreferLeAudioOnlyMode) {
                if (mAdapterService.getDatabase()
                        .getProfileConnectionPolicy(device, BluetoothProfile.A2DP)
                        >  BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
                    debugLog("clear a2dp profile priority for the le audio dual mode device "
                            + device);
                    mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                            BluetoothProfile.A2DP, BluetoothProfile.CONNECTION_POLICY_FORBIDDEN);
                }
                if (mAdapterService.getDatabase()
                        .getProfileConnectionPolicy(device, BluetoothProfile.HEADSET)
                        >  BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
                    debugLog("clear hfp profile priority for the le audio dual mode device "
                            + device);
                    mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                            BluetoothProfile.HEADSET, BluetoothProfile.CONNECTION_POLICY_FORBIDDEN);
                }
            }
        }
        if ((bcService != null) && ArrayUtils.contains(uuids,
                BluetoothUuid.BASS) && (bcService.getConnectionPolicy(device)
                == BluetoothProfile.CONNECTION_POLICY_UNKNOWN)) {
            debugLog("setting broadcast assistant profile priority for device " + device);
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.LE_AUDIO_BROADCAST_ASSISTANT,
                    BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        }

        if (!isQtiLeAudioEnabled &&
            (volumeControlService != null) && ArrayUtils.contains(uuids,
                BluetoothUuid.VOLUME_CONTROL) && (volumeControlService.getConnectionPolicy(device)
                == BluetoothProfile.CONNECTION_POLICY_UNKNOWN)) {
            debugLog("setting volume control profile priority for device " + device);
            mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                    BluetoothProfile.VOLUME_CONTROL, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
        }

        ///*_REF
        initBCReferences();
        if (mBCService != null && mBCId != null && ArrayUtils.contains(uuids,
                ParcelUuid.fromString(mBCId)) &&
                mBCGetConnPolicy != null && mBCSetConnPolicy != null) {
            int connPolicy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
            try {
               connPolicy = (int) mBCGetConnPolicy.invoke(mBCService, device);
            } catch(IllegalAccessException e) {
               Log.e(TAG, "BC:connPolicy IllegalAccessException");
            } catch (InvocationTargetException e) {
               Log.e(TAG, "BC:connPolicy InvocationTargetException");
            }
            debugLog("setting BC connection policy for device " + device);
            if (connPolicy == BluetoothProfile.CONNECTION_POLICY_UNKNOWN) {
                try {
                  mBCSetConnPolicy.invoke(mBCService, device,BluetoothProfile.CONNECTION_POLICY_ALLOWED);
                } catch(IllegalAccessException e) {
                   Log.e(TAG, "BC:Connect IllegalAccessException");
                } catch (InvocationTargetException e) {
                  Log.e(TAG, "BC:Connect InvocationTargetException");
                }
            }
        }
        //_REF*/
    }

    @RequiresPermission(allOf = {
            android.Manifest.permission.BLUETOOTH_CONNECT,
            android.Manifest.permission.BLUETOOTH_PRIVILEGED,
    })
    private void processProfileStateChanged(BluetoothDevice device, int profileId, int nextState,
            int prevState) {
        debugLog("processProfileStateChanged, device=" + device + ", profile=" + profileId + ", "
                + prevState + " -> " + nextState);
        if ((profileId == BluetoothProfile.A2DP) ||
            (profileId == BluetoothProfile.HEADSET) ||
            (profileId == BluetoothProfile.A2DP_SINK) ||
            (profileId == BluetoothProfile.BC_PROFILE) ||
            (profileId == BluetoothProfile.LE_AUDIO) ||
            (profileId == BluetoothProfile.CSIP_SET_COORDINATOR)) {
            BluetoothDevice peerTwsDevice =
                    (mAdapterService != null && mAdapterService.isTwsPlusDevice(device)) ?
                    mAdapterService.getTwsPlusPeerDevice(device):null;
            if (nextState == BluetoothProfile.STATE_CONNECTED) {
                debugLog("processProfileStateChanged: isTwsDevice: " +
                                      mAdapterService.isTwsPlusDevice(device));
                switch (profileId) {
                    //case BluetoothProfile.LE_AUDIO:
                    case BluetoothProfile.A2DP:
                        mA2dpRetrySet.remove(device);
                        break;
                    case BluetoothProfile.HEADSET:
                        mHeadsetRetrySet.remove(device);
                        break;
                    case BluetoothProfile.A2DP_SINK:
                        mDatabaseManager.setConnectionForA2dpSrc(device);
                        break;
                    case BluetoothProfile.BC_PROFILE:
                        mDatabaseManager.setConnectionStateForBc(device, nextState);
                        break;
                }
                connectOtherProfile(device);
            }
            if (nextState == BluetoothProfile.STATE_DISCONNECTED) {
                /* Ignore A2DP/HFP state change intent received during BT OFF */
                if (mAdapterService != null &&
                         mAdapterService.getState() == BluetoothAdapter.STATE_ON) {
                    if (profileId == BluetoothProfile.A2DP/* ||
                        profileId == BluetoothProfile.LE_AUDIO*/) {
                        Log.w(TAG, "processProfileStateChanged: Calling setDisconnectionA2dp "
                                    + " for device "+ device);
                        mDatabaseManager.setDisconnection(device);
                    } else if (profileId == BluetoothProfile.HEADSET) {
                        Log.w(TAG, "processProfileStateChanged: Calling setDisconnectionForHfp "
                                    + " for device "+ device);
                        mDatabaseManager.setDisconnectionForHfp(device);
                    }

                    boolean isAospLeAudioEnabled = ApmConstIntf.getAospLeaEnabled();
                    debugLog("processProfileStateChanged: isAospLeAudioEnabled: " +
                                                                isAospLeAudioEnabled);

                    if (isAospLeAudioEnabled &&
                        (profileId == BluetoothProfile.LE_AUDIO)) {
                        Log.w(TAG, "processProfileStateChanged: Calling " +
                                   " setDisconnectionForLeAudio for device "+ device);
                        mDatabaseManager.setDisconnectionForLeAudio(device);
                    }

                    if (profileId == BluetoothProfile.A2DP_SINK) {
                        Log.w(TAG, "processProfileStateChanged: Calling setDisconnectionForA2dpSrc "
                                    + " for device "+ device);
                        mDatabaseManager.setDisconnectionForA2dpSrc(device);
                    }

                    if (profileId == BluetoothProfile.BC_PROFILE) {
                        mDatabaseManager.setConnectionStateForBc(device, nextState);
                    }
                }
                handleAllProfilesDisconnected(device);
            }
        }
    }

    /**
     * Updates the last connection date in the connection order database for the newly active device
     * if connected to a2dp profile
     *
     * @param device is the device we just made the active device
     */
    private void processActiveDeviceChanged(BluetoothDevice device, int profileId) {
        debugLog("processActiveDeviceChanged, device=" + device + ", profile=" + profileId);
        boolean isAospLeAudioEnabled = ApmConstIntf.getAospLeaEnabled();
        if (mAdapterService != null && mBroadcastService == null) {
            mBroadcastService = mAdapterService.getBroadcastService();
            mBroadcastGetAddr = mAdapterService.getBroadcastAddress();
            mBroadcastIsActive = mAdapterService.getBroadcastActive();
        }
        if (mBroadcastService != null && mBroadcastGetAddr != null &&
            mBroadcastIsActive != null && broadcastBDA == null) {
            boolean is_broadcast_active = false;
            try {
                is_broadcast_active = (boolean) mBroadcastIsActive.invoke(mBroadcastService);
            } catch(IllegalAccessException | InvocationTargetException e) {
                Log.e(TAG, "Broadcast:IsActive Illegal Exception");
            }
            try {
                broadcastBDA = (String) mBroadcastGetAddr.invoke(mBroadcastService);
            } catch(IllegalAccessException | InvocationTargetException e) {
                Log.e(TAG, "Broadcast:GetAddr Illegal Exception");
            }
        }
        if (device != null && broadcastBDA != null) {
            if (device.getAddress().equals(broadcastBDA)) {
                Log.d(TAG," Update from broadcast, bail out");
                return;
            }
        }

        if (device != null) {
            mDatabaseManager.setConnection(device, profileId == BluetoothProfile.A2DP);

            if (profileId == BluetoothProfile.HEADSET) {
                Log.w(TAG, "processActiveDeviceChanged: Calling setConnectionForHfp for device "
                            + device);
                mDatabaseManager.setConnectionForHfp(device);
            }

            debugLog("processActiveDeviceChanged: isAospLeAudioEnabled: " +
                                                           isAospLeAudioEnabled);

            if (isAospLeAudioEnabled && (profileId == BluetoothProfile.LE_AUDIO)) {
                Log.w(TAG, "processActiveDeviceChanged: Calling " +
                           " setConnectionForLeAudio for device " + device);
                mDatabaseManager.setConnectionForLeAudio(device);
            }

            if (isAospLeAudioEnabled) {
                if (!mPreferLeAudioOnlyMode) {
                    debugLog("processActiveDeviceChanged: Preferred duel mode, return");
                    return;
                }

                if (profileId == BluetoothProfile.LE_AUDIO) {
                    HeadsetService hsService = mFactory.getHeadsetService();
                    if (hsService != null) {
                        if ((hsService.getConnectionPolicy(device)
                                != BluetoothProfile.CONNECTION_POLICY_ALLOWED)
                                && (hsService.getConnectionState(device)
                                == BluetoothProfile.STATE_CONNECTED)) {
                            debugLog("Disconnect HFP for the LE audio dual mode device " + device);
                            hsService.disconnect(device);
                        }
                    }

                    A2dpService a2dpService = mFactory.getA2dpService();
                    if (a2dpService != null) {
                        if ((a2dpService.getConnectionPolicy(device)
                                != BluetoothProfile.CONNECTION_POLICY_ALLOWED)
                                && (a2dpService.getConnectionState(device)
                                == BluetoothProfile.STATE_CONNECTED)) {
                            debugLog("Disconnect A2DP for the LE audio dual mode device " + device);
                            a2dpService.disconnect(device);
                        }
                    }
                }
            }
        }
    }

    private void processDeviceConnected(BluetoothDevice device) {
        debugLog("processDeviceConnected, device=" + device);
        mDatabaseManager.setConnection(device, false);
    }

    @RequiresPermission(allOf = {
            android.Manifest.permission.BLUETOOTH_CONNECT,
            android.Manifest.permission.BLUETOOTH_PRIVILEGED,
    })
    private boolean handleAllProfilesDisconnected(BluetoothDevice device) {
        boolean atLeastOneProfileConnectedForDevice = false;
        boolean allProfilesEmpty = true;
        HeadsetService hsService = mFactory.getHeadsetService();
        A2dpService a2dpService = mFactory.getA2dpService();
        PanService panService = mFactory.getPanService();
        A2dpSinkService a2dpSinkService = mFactory.getA2dpSinkService();
        LeAudioService leAudioService = mFactory.getLeAudioService();
        CsipSetCoordinatorService csipSetCooridnatorService =
            mFactory.getCsipSetCoordinatorService();
        boolean isQtiLeAudioEnabled = ApmConstIntf.getQtiLeAudioEnabled();

        if (hsService != null) {
            List<BluetoothDevice> hsConnDevList = hsService.getConnectedDevices();
            allProfilesEmpty &= hsConnDevList.isEmpty();
            atLeastOneProfileConnectedForDevice |= hsConnDevList.contains(device);
        }
        if (a2dpService != null) {
            List<BluetoothDevice> a2dpConnDevList = a2dpService.getConnectedDevices();
            allProfilesEmpty &= a2dpConnDevList.isEmpty();
            atLeastOneProfileConnectedForDevice |= a2dpConnDevList.contains(device);
        }
        if (a2dpSinkService != null) {
            List<BluetoothDevice> a2dpSinkConnDevList = a2dpSinkService.getConnectedDevices();
            allProfilesEmpty &= a2dpSinkConnDevList.isEmpty();
            atLeastOneProfileConnectedForDevice |= a2dpSinkConnDevList.contains(device);
        }
        if (csipSetCooridnatorService != null) {
            List<BluetoothDevice> csipConnDevList = csipSetCooridnatorService.getConnectedDevices();
            allProfilesEmpty &= csipConnDevList.isEmpty();
            atLeastOneProfileConnectedForDevice |= csipConnDevList.contains(device);
        }
        if (panService != null) {
            List<BluetoothDevice> panConnDevList = panService.getConnectedDevices();
            allProfilesEmpty &= panConnDevList.isEmpty();
            atLeastOneProfileConnectedForDevice |= panConnDevList.contains(device);
        }
        if (!isQtiLeAudioEnabled && leAudioService != null) {
            List<BluetoothDevice> leAudioConnDevList = leAudioService.getConnectedDevices();
            allProfilesEmpty &= leAudioConnDevList.isEmpty();
            atLeastOneProfileConnectedForDevice |= leAudioConnDevList.contains(device);
        }

        if (!atLeastOneProfileConnectedForDevice) {
            // Consider this device as fully disconnected, don't bother connecting others
            debugLog("handleAllProfilesDisconnected: all profiles disconnected for " + device);
            mHeadsetRetrySet.remove(device);
            mA2dpRetrySet.remove(device);
            if (allProfilesEmpty) {
                debugLog("handleAllProfilesDisconnected: all profiles disconnected for all"
                        + " devices");
                // reset retry status so that in the next round we can start retrying connections
                resetStates();
            }
            return true;
        }
        return false;
    }

    private void resetStates() {
        mHeadsetRetrySet.clear();
        mA2dpRetrySet.clear();
    }

    // Delaying Auto Connect to make sure that all clients
    // are up and running, specially BluetoothHeadset.
    @RequiresPermission(allOf = {
            android.Manifest.permission.BLUETOOTH_CONNECT,
            android.Manifest.permission.MODIFY_PHONE_STATE,
    })
    public void autoConnect() {
        debugLog( "delay auto connect by 500 ms");
        if ((mHandler.hasMessages(MESSAGE_AUTO_CONNECT_PROFILES) == false) &&
            (mAdapterService.isQuietModeEnabled()== false)) {
            Message m = mHandler.obtainMessage(MESSAGE_AUTO_CONNECT_PROFILES);
            mHandler.sendMessageDelayed(m,AUTO_CONNECT_PROFILES_TIMEOUT);
        }
    }

    private void autoConnectProfilesDelayed() {
        if (mAdapterService.getState() != BluetoothAdapter.STATE_ON) {
            errorLog("autoConnect: BT is not ON. Exiting autoConnect");
            return;
        }

        if (!mAdapterService.isQuietModeEnabled()) {
            debugLog("autoConnect: Initiate auto connection on BT on...");
            final BluetoothDevice mostRecentlyActiveA2dpDevice =
                    mDatabaseManager.getMostRecentlyConnectedA2dpDevice();
            final BluetoothDevice mostRecentlyActiveHfpDevice =
                    mDatabaseManager.getMostRecentlyConnectedHfpDevice();
            final BluetoothDevice mostRecentlyConnectedA2dpSrcDevice =
                    mDatabaseManager.getMostRecentlyConnectedA2dpSrcDevice();
            debugLog("autoConnect: mostRecentlyActiveA2dpDevice: " +
                                                mostRecentlyActiveA2dpDevice);
            debugLog("autoConnect: mostRecentlyActiveHfpDevice: " +
                                                mostRecentlyActiveHfpDevice);
            debugLog("autoConnect: mostRecentlyConnectedA2dpSrcDevice: " +
                                                mostRecentlyConnectedA2dpSrcDevice);
            autoConnectBC(true, null);
            //Initiate auto-connection for latest connected a2dp source device.
            if (mostRecentlyConnectedA2dpSrcDevice != null) {
               debugLog("autoConnect: attempting auto connection for recently"+
                        " connected A2DP Source device:" + mostRecentlyConnectedA2dpSrcDevice);
               autoConnectA2dpSink(mostRecentlyConnectedA2dpSrcDevice);
            }

            boolean isAospLeAudioEnabled = ApmConstIntf.getAospLeaEnabled();
            debugLog("autoConnect: isAospLeAudioEnabled: " + isAospLeAudioEnabled);

            if (isAospLeAudioEnabled) {
                final BluetoothDevice mostRecentlyActiveLeAudioDevice =
                        mDatabaseManager.getMostRecentlyConnectedLeAudioDevice();
                debugLog("autoConnect: mostRecentlyActiveLeAudioDevice: " +
                                                mostRecentlyActiveLeAudioDevice);
                if (mostRecentlyActiveLeAudioDevice != null) {
                    debugLog("autoConnect: recently connected LeAudio active device " +
                                                      mostRecentlyActiveLeAudioDevice +
                             " attempting auto connection for LeAudio");
                    autoConnectLeAudioSet(mostRecentlyActiveLeAudioDevice);
                }
            }

            if (mostRecentlyActiveA2dpDevice == null &&
                mostRecentlyActiveHfpDevice == null) {
                errorLog("autoConnect: most recently active a2dp and hfp devices are null");
                return;
            }

            BluetoothDevice device = (mostRecentlyActiveA2dpDevice != null) ?
                    mostRecentlyActiveA2dpDevice : mostRecentlyActiveHfpDevice;

            BluetoothDevice peerTwsDevice = (mAdapterService != null && device != null
                    && mAdapterService.isTwsPlusDevice(device)) ?
                    mAdapterService.getTwsPlusPeerDevice(device) : null;

            if (mostRecentlyActiveA2dpDevice != null) {
                debugLog("autoConnect: recently connected A2DP active Device " +
                    mostRecentlyActiveA2dpDevice + " attempting auto connection for A2DP, HFP");
                autoConnectHeadset(mostRecentlyActiveA2dpDevice);
                autoConnectA2dp(mostRecentlyActiveA2dpDevice);
                debugLog("autoConnect: attempting auto connection for recently"+
                        " connected HID device:" + mostRecentlyConnectedA2dpSrcDevice);
                autoConnectHidHost(mostRecentlyActiveA2dpDevice);
                if (peerTwsDevice != null) {
                    debugLog("autoConnect: 2nd pair TWS+ EB");
                    autoConnectHeadset(peerTwsDevice);
                    autoConnectA2dp(peerTwsDevice);
                }
            } else if (mostRecentlyActiveHfpDevice != null) {
                debugLog("autoConnect: recently connected A2DP active device is null." +
                     " recently connected HFP Device " + mostRecentlyActiveHfpDevice
                    + " attempting auto connection for HFP");
                autoConnectHeadset(mostRecentlyActiveHfpDevice);
                if (peerTwsDevice != null) {
                    debugLog("autoConnectHF: 2nd pair TWS+ EB");
                    autoConnectHeadset(peerTwsDevice);
                }
            }
        } else {
            debugLog("autoConnect() - BT is in quiet mode. Not initiating auto connections");
        }
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    private void autoConnectA2dp(BluetoothDevice device) {
        final A2dpService a2dpService = mFactory.getA2dpService();
        if (a2dpService == null) {
            warnLog("autoConnectA2dp: service is null, failed to connect to " + device);
            return;
        }
        int a2dpConnectionPolicy = a2dpService.getConnectionPolicy(device);
        if (a2dpConnectionPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            debugLog("autoConnectA2dp: connecting A2DP with " + device);
            if(ApmConstIntf.getQtiLeAudioEnabled()) {
                MediaAudioIntf mMediaAudio = MediaAudioIntf.get();
                mMediaAudio.autoConnect(device);
            } else {
                a2dpService.connect(device);
            }
        } else {
            debugLog("autoConnectA2dp: skipped auto-connect A2DP with device " + device
                    + " a2dpConnectionPolicy " + a2dpConnectionPolicy);
        }
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    private void autoConnectLeAudio(BluetoothDevice device) {
        final LeAudioService leAudioService = mFactory.getLeAudioService();
        if (leAudioService == null) {
            warnLog("autoConnectLeAudio: service is null, failed to connect to " + device);
            return;
        }
        int leAudioConnectionPolicy = leAudioService.getConnectionPolicy(device);
        if (leAudioConnectionPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            debugLog("autoConnectLeAudio: connecting leaudio with " + device);
            leAudioService.connect(device);
        } else {
            debugLog("autoConnectLeAudio: skipped auto-connect LE-AUDIO with device " + device
                    + " leAudioConnectionPolicy " + leAudioConnectionPolicy);
        }
    }

    private void autoConnectLeAudioSet(BluetoothDevice device) {
        debugLog("autoConnectLeAudioSet: " + device);
        final LeAudioService leAudioService = mFactory.getLeAudioService();
        if (leAudioService == null) {
            warnLog("autoConnectLeAudioSet: service is null, failed to connect to " + device);
            return;
        }

        List<BluetoothDevice> deviceSet = null;
        CsipWrapper csipWrapper = CsipWrapper.getInstance();
        int setId = csipWrapper.getRemoteDeviceGroupId(device, null);
        if (setId >= 0 && setId != INVALID_SET_ID) {
            DeviceGroup set = csipWrapper.getCoordinatedSet(setId);
            if (set != null) {
                deviceSet = set.getDeviceGroupMembers();
            }
        }
        if (deviceSet == null) {
            deviceSet = new CopyOnWriteArrayList<BluetoothDevice>();
            deviceSet.add(device);
        }

        for (BluetoothDevice dev : deviceSet) {
            autoConnectLeAudio(dev);
        }
    }

    @RequiresPermission(allOf = {
            android.Manifest.permission.BLUETOOTH_CONNECT,
            android.Manifest.permission.MODIFY_PHONE_STATE,
    })
    private void autoConnectHeadset(BluetoothDevice device) {
        final HeadsetService hsService = mFactory.getHeadsetService();
        if (hsService == null) {
            warnLog("autoConnectHeadset: service is null, failed to connect to " + device);
            return;
        }
        int headsetConnectionPolicy = hsService.getConnectionPolicy(device);
        if (headsetConnectionPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            debugLog("autoConnectHeadset: Connecting HFP with " + device);
            if(ApmConstIntf.getQtiLeAudioEnabled()) {
                CallAudioIntf mCallAudio = CallAudioIntf.get();
                mCallAudio.autoConnect(device);
            } else {
                hsService.connect(device);
            }
        } else {
            debugLog("autoConnectHeadset: skipped auto-connect HFP with device " + device
                    + " headsetConnectionPolicy " + headsetConnectionPolicy);
        }
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    private void autoConnectHidHost(BluetoothDevice device) {
        final HidHostService hidHostService = mFactory.getHidHostService();
        if (hidHostService == null) {
            warnLog("autoConnectHidHost: service is null, failed to connect to " + device);
            return;
        }
        int hidHostConnectionPolicy = hidHostService.getConnectionPolicy(device);
        if (hidHostConnectionPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            debugLog("autoConnectHidHost: Connecting HID with " + device);
            hidHostService.connect(device);
        } else {
            debugLog("autoConnectHidHost: skipped auto-connect HID with device " + device
                    + " connectionPolicy " + hidHostConnectionPolicy);
        }
    }

    private void autoConnectA2dpSink(BluetoothDevice device) {
        A2dpSinkService a2dpSinkService = A2dpSinkService.getA2dpSinkService();
        if (a2dpSinkService == null) {
            errorLog("autoConnectA2dpSink, service is null");
            return;
        }

         int priority = a2dpSinkService.getConnectionPolicy(device);
         debugLog("autoConnectA2dpSink, attempt auto-connect with device " + device
                 + " priority " + priority);
        if (priority == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            debugLog("autoConnectA2dpSink() - Connecting A2DP Sink with " + device.toString());
            a2dpSinkService.connect(device);
        }

    }
    ///*_REF
    private void autoConnectBC(boolean autoconnect, BluetoothDevice mDevice) {
        if (autoconnect == false) {
            if (mDatabaseManager.deviceSupportsBCprofile(mDevice)) {
                connectBC(mDevice);
            }
            return;
        }
        BluetoothDevice bondedDevices[] =  mAdapterService.getBondedDevices();
        for (BluetoothDevice device : bondedDevices) {
            if (mDatabaseManager.wasBCConnectedDevice(device) == false) {
                Log.d(TAG, "not a BC connected device earlier, Ignoring");
                continue;
            }
            final BluetoothDevice mostRecentlyActiveA2dpDevice =
                mDatabaseManager.getMostRecentlyConnectedA2dpDevice();
            if (Objects.equals(mostRecentlyActiveA2dpDevice, device)) {
                GroupService setCoordinator = GroupService.getGroupService();
                List<BluetoothDevice> listOfDevices = new ArrayList<BluetoothDevice>();
                if (setCoordinator != null) {
                    int setId = setCoordinator.getRemoteDeviceGroupId(device, null);
                    DeviceGroup devGrp = setCoordinator.getCoordinatedSet(setId);
                    if (devGrp != null) {
                        listOfDevices = devGrp.getDeviceGroupMembers();
                    } else {
                        Log.d(TAG, "Failed to get dev Group instance");
                        listOfDevices.add(device);
                    }
                } else {
                    Log.d(TAG, "Fail to get CSIP instance");
                    listOfDevices.add(device);
                }

                for (BluetoothDevice dev : listOfDevices) {
                    Log.d(TAG, "connectBC : " + dev);
                    connectBC(dev);
                }
            }
        }
    }
    private void connectBC(BluetoothDevice device) {
        if (mBCGetConnPolicy == null ||  mBCConnect == null ) {
            Log.e(TAG, "BC reference are null");
            return;
        }
        if (device == null) return;
        int connPolicy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
        try {
            connPolicy = (int) mBCGetConnPolicy.invoke(mBCService, device);
        } catch(IllegalAccessException | InvocationTargetException e) {
            Log.e(TAG, "BC:connPolicy IllegalAccessException");
        } 
        debugLog("ConnectBC, attempt connection with device " + device
                 + " connPolicy " + connPolicy);
        if (connPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            debugLog("autoConnectBC() - Connecting BC with " + device.toString());
            try {
                mBCConnect.invoke(mBCService, device);
            } catch(IllegalAccessException | InvocationTargetException e) {
                Log.e(TAG, "autoConnectBC:connect IllegalAccessException");
            }
        }
    }
    //_REF*/

    private boolean isConnectTimeoutDelayApplicable(BluetoothDevice device) {
        if (device == null) return false;

        boolean matched = InteropUtil.interopMatchAddrOrName(
            InteropUtil.InteropFeature.INTEROP_PHONE_POLICY_INCREASED_DELAY_CONNECT_OTHER_PROFILES,
            device.getAddress());

        return matched;
    }

    private boolean isConnectReducedTimeoutDelayApplicable(BluetoothDevice device) {
        if (device == null) return false;

        boolean matched = InteropUtil.interopMatchAddrOrName(
            InteropUtil.InteropFeature.INTEROP_PHONE_POLICY_REDUCED_DELAY_CONNECT_OTHER_PROFILES,
            device.getAddress());

        return matched;
    }

    private void connectOtherProfile(BluetoothDevice device) {
        if (mAdapterService.isQuietModeEnabled()) {
            debugLog("connectOtherProfile: in quiet mode, skip connect other profile " + device);
            return;
        }
        if (mConnectOtherProfilesDeviceSet.contains(device)) {
            debugLog("connectOtherProfile: already scheduled callback for " + device);
            return;
        }
        mConnectOtherProfilesDeviceSet.add(device);
        Message m = mHandler.obtainMessage(MESSAGE_CONNECT_OTHER_PROFILES);
        m.obj = device;
        if (isConnectTimeoutDelayApplicable(device))
            mHandler.sendMessageDelayed(m,CONNECT_OTHER_PROFILES_TIMEOUT_DELAYED);
        else if (isConnectReducedTimeoutDelayApplicable(device))
            mHandler.sendMessageDelayed(m,CONNECT_OTHER_PROFILES_REDUCED_TIMEOUT_DELAYED);
        else
            mHandler.sendMessageDelayed(m, sConnectOtherProfilesTimeoutMillis);
    }

    // This function is called whenever a profile is connected.  This allows any other bluetooth
    // profiles which are not already connected or in the process of connecting to attempt to
    // connect to the device that initiated the connection.  In the event that this function is
    // invoked and there are no current bluetooth connections no new profiles will be connected.
    @RequiresPermission(allOf = {
            android.Manifest.permission.BLUETOOTH_CONNECT,
            android.Manifest.permission.BLUETOOTH_PRIVILEGED,
            android.Manifest.permission.MODIFY_PHONE_STATE,
    })
    private void processConnectOtherProfiles(BluetoothDevice device) {
        debugLog("processConnectOtherProfiles, device=" + device);
        if (mAdapterService.getState() != BluetoothAdapter.STATE_ON) {
            warnLog("processConnectOtherProfiles, adapter is not ON " + mAdapterService.getState());
            return;
        }
        if (handleAllProfilesDisconnected(device)) {
            debugLog("processConnectOtherProfiles: all profiles disconnected for " + device);
            return;
        }

        HeadsetService hsService = mFactory.getHeadsetService();
        A2dpService a2dpService = mFactory.getA2dpService();
        PanService panService = mFactory.getPanService();
        A2dpSinkService a2dpSinkService = mFactory.getA2dpSinkService();
        HidHostService hidHostService = mFactory.getHidHostService();
        LeAudioService leAudioService = mFactory.getLeAudioService();
        CsipSetCoordinatorService csipSetCooridnatorService = mFactory.getCsipSetCoordinatorService();
        VolumeControlService volumeControlService = mFactory.getVolumeControlService();
        boolean isQtiLeAudioEnabled = ApmConstIntf.getQtiLeAudioEnabled();

        List<BluetoothDevice> hsConnDevList = null;
        List<BluetoothDevice> a2dpConnDevList = null;
        List<BluetoothDevice> a2dpSinkConnDevList = null;
        if (hsService != null) {
            hsConnDevList = hsService.getConnectedDevices();
        }
        if (a2dpService != null) {
            a2dpConnDevList = a2dpService.getConnectedDevices();
        }
        if (a2dpSinkService != null) {
            a2dpSinkConnDevList = a2dpSinkService.getConnectedDevices();
        }

        boolean a2dpConnected = false;
        boolean hsConnected = false;
        if(a2dpConnDevList != null && !a2dpConnDevList.isEmpty()) {
            for (BluetoothDevice a2dpDevice : a2dpConnDevList) {
                if(a2dpDevice.equals(device)) {
                    a2dpConnected = true;
                }
            }
        }
        if(hsConnDevList != null && !hsConnDevList.isEmpty()) {
            for (BluetoothDevice hsDevice : hsConnDevList) {
                if(hsDevice.equals(device)) {
                    hsConnected = true;
                }
            }
        }

        // This change makes sure that we try to re-connect
        // the profile if its connection failed and priority
        // for desired profile is ON.
        debugLog("HF connected for device : " + device + " " +
                (hsConnDevList == null ? false :hsConnDevList.contains(device)));
        debugLog("A2DP connected for device : " + device + " " +
                (a2dpConnDevList == null ? false :a2dpConnDevList.contains(device)));
        debugLog("A2DPSink connected for device : " + device + " " +
                (a2dpSinkConnDevList == null ? false :a2dpSinkConnDevList.contains(device)));

        if (hsService != null) {
            if ((hsConnDevList.isEmpty() || !(hsConnDevList.contains(device)))
                    && (!mHeadsetRetrySet.contains(device))
                    && (hsService.getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_ALLOWED)
                    && (hsService.getConnectionState(device)
                               == BluetoothProfile.STATE_DISCONNECTED)
                    && (a2dpConnected || (a2dpService != null &&
                        a2dpService.getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN))) {

                debugLog("Retrying connection to HS with device " + device);
                int maxConnections = mAdapterService.getMaxConnectedAudioDevices();

                if (!hsConnDevList.isEmpty() && maxConnections == 1) {
                    Log.v(TAG,"HFP is already connected, ignore");
                    return;
                }

                // proceed connection only if a2dp is connected to this device
                // add here as if is already overloaded
                if (a2dpConnDevList.contains(device) ||
                     (hsService.getConnectionPolicy(device) >= BluetoothProfile.CONNECTION_POLICY_ALLOWED)) {
                    debugLog("Retrying connection to HS with device " + device);
                    mHeadsetRetrySet.add(device);
                    if (ApmConstIntf.getQtiLeAudioEnabled()) {
                        CallAudioIntf mCallAudio = CallAudioIntf.get();
                        mCallAudio.connect(device);
                    } else {
                        hsService.connect(device);
                    }
                } else {
                    debugLog("do not initiate connect as A2dp is not connected");
                }
            }
        }

        if (a2dpService != null) {
            if ((a2dpConnDevList.isEmpty() || !(a2dpConnDevList.contains(device)))
                    && (!mA2dpRetrySet.contains(device))
                    && (a2dpService.getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_ALLOWED ||
                        mAdapterService.isTwsPlusDevice(device))
                    && (a2dpService.getConnectionState(device)
                               == BluetoothProfile.STATE_DISCONNECTED)
                    && (hsConnected || (hsService != null &&
                        hsService.getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN))) {
                debugLog("Retrying connection to A2DP with device " + device);
                int maxConnections = mAdapterService.getMaxConnectedAudioDevices();

                if (!a2dpConnDevList.isEmpty() && maxConnections == 1) {
                    Log.v(TAG,"a2dp is already connected, ignore");
                    return;
                }

                // proceed connection only if HFP is connected to this device
                // add here as if is already overloaded
                if (hsConnDevList.contains(device) ||
                    (a2dpService.getConnectionPolicy(device) >= BluetoothProfile.CONNECTION_POLICY_ALLOWED)) {
                    debugLog("Retrying connection to A2DP with device " + device);
                    mA2dpRetrySet.add(device);
                    if (ApmConstIntf.getQtiLeAudioEnabled()) {
                        MediaAudioIntf mMediaAudio = MediaAudioIntf.get();
                        mMediaAudio.connect(device);
                    } else {
                        a2dpService.connect(device);
                    }
                    debugLog("do not initiate connect as HFP is not connected");
                }
            }
        }
        if (panService != null) {
            List<BluetoothDevice> panConnDevList = panService.getConnectedDevices();
            // TODO: the panConnDevList.isEmpty() check below should be removed once
            // Multi-PAN is supported.
            if (panConnDevList.isEmpty() && (panService.getConnectionPolicy(device)
                    == BluetoothProfile.CONNECTION_POLICY_ALLOWED)
                    && (panService.getConnectionState(device)
                    == BluetoothProfile.STATE_DISCONNECTED)) {
                debugLog("Retrying connection to PAN with device " + device);
                panService.connect(device);
            }
        }
        if (csipSetCooridnatorService != null) {
            List<BluetoothDevice> csipConnDevList = csipSetCooridnatorService.getConnectedDevices();
            if (!csipConnDevList.contains(device)
                    && (csipSetCooridnatorService.getConnectionPolicy(device)
                    == BluetoothProfile.CONNECTION_POLICY_ALLOWED)
                    && (csipSetCooridnatorService.getConnectionState(device)
                    == BluetoothProfile.STATE_DISCONNECTED)) {
                debugLog("Retrying connection to CSIP with device " + device);
                csipSetCooridnatorService.connect(csipSetCooridnatorService.getAppId(),device);
            }
        }
        // Connect A2DP Sink Service if HS is connected
        if (a2dpSinkService != null) {
            List<BluetoothDevice> sinkConnDevList = a2dpSinkService.getConnectedDevices();
            if (sinkConnDevList.isEmpty() &&
                    (a2dpSinkService.getConnectionPolicy(device) >= BluetoothProfile.CONNECTION_POLICY_ALLOWED) &&
                    (a2dpSinkService.getConnectionState(device) ==
                            BluetoothProfile.STATE_DISCONNECTED) &&
                    (hsConnected || (hsService != null &&
                         hsService.getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN))) {
                debugLog("Retrying connection for A2dpSink with device " + device);
                a2dpSinkService.connect(device);
            }
        }

         if (hidHostService != null) {
            if ((hidHostService.getConnectionPolicy(device)
                    == BluetoothProfile.CONNECTION_POLICY_ALLOWED)
                    && (hidHostService.getConnectionState(device)
                    == BluetoothProfile.STATE_DISCONNECTED)) {
                debugLog("Retrying connection to HID with device " + device);
                hidHostService.connect(device);
            }
        }

        if (!isQtiLeAudioEnabled && leAudioService != null) {
            List<BluetoothDevice> leAudioConnDevList = leAudioService.getConnectedDevices();
            debugLog("le audio device: " + device + " connection state: " +
                                     leAudioService.getConnectionState(device));
            if (!leAudioConnDevList.contains(device) && (leAudioService.getConnectionPolicy(device)
                    == BluetoothProfile.CONNECTION_POLICY_ALLOWED)
                    && (leAudioService.getConnectionState(device)
                    == BluetoothProfile.STATE_DISCONNECTED)) {
                debugLog("Retrying connection to LEAudio with device " + device);
                leAudioService.connect(device);
            }
        }

        if (!isQtiLeAudioEnabled && volumeControlService != null) {
            List<BluetoothDevice> vcConnDevList = volumeControlService.getConnectedDevices();
            debugLog("vol control device: " + device + " connection state: " +
                                     volumeControlService.getConnectionState(device));
            if (!vcConnDevList.contains(device) && (volumeControlService.getConnectionPolicy(device)
                    == BluetoothProfile.CONNECTION_POLICY_ALLOWED)
                    && (volumeControlService.getConnectionState(device)
                    == BluetoothProfile.STATE_DISCONNECTED)) {
                debugLog("Retrying connection to VCP with device " + device);
                volumeControlService.connect(device);
            }
        }
        autoConnectBC(false, device);
    }

    private static void debugLog(String msg) {
        if (DBG) {
            Log.i(TAG, msg);
        }
    }

    private static void warnLog(String msg) {
        Log.w(TAG, msg);
    }

    private static void errorLog(String msg) {
        Log.e(TAG, msg);
    }
}
