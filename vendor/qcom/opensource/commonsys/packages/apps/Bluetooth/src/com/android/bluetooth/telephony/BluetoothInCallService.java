/*
 * Copyright (C) 2020 The Android Open Source Project
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
 */

package com.android.bluetooth.telephony;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import android.annotation.RequiresPermission;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Binder;
import android.os.IBinder;
import android.os.Bundle;
import android.os.UserHandle;
import android.telecom.Call;
import android.telecom.CallAudioState;
import android.telecom.Connection;
import android.telecom.InCallService;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telecom.VideoProfile;
import android.telephony.PhoneNumberUtils;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;

import com.android.bluetooth.hfp.BluetoothHeadsetProxy;
import com.android.bluetooth.hfp.HeadsetService;
import com.android.bluetooth.apm.ApmConstIntf;


import androidx.annotation.VisibleForTesting;
import com.android.internal.telephony.PhoneConstants;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executors;

/**
 * Used to receive updates about calls from the Telecom component. This service is bound to Telecom
 * while there exist calls which potentially require UI. This includes ringing (incoming), dialing
 * (outgoing), and active calls. When the last BluetoothCall is disconnected, Telecom will unbind
 * to the service triggering InCallActivity (via CallList) to finish soon after.
 */
public class BluetoothInCallService extends InCallService {

    private static final String TAG = "BluetoothInCallService";

    // match up with bthf_call_state_t of bt_hf.h
    private static final int CALL_STATE_ACTIVE = 0;
    private static final int CALL_STATE_HELD = 1;
    private static final int CALL_STATE_DIALING = 2;
    private static final int CALL_STATE_ALERTING = 3;
    private static final int CALL_STATE_INCOMING = 4;
    private static final int CALL_STATE_WAITING = 5;
    private static final int CALL_STATE_IDLE = 6;
    private static final int CALL_STATE_DISCONNECTED = 7;

    // match up with bthf_call_state_t of bt_hf.h
    // Terminate all held or set UDUB("busy") to a waiting call
    private static final int CHLD_TYPE_RELEASEHELD = 0;
    // Terminate all active calls and accepts a waiting/held call
    private static final int CHLD_TYPE_RELEASEACTIVE_ACCEPTHELD = 1;
    // Hold all active calls and accepts a waiting/held call
    private static final int CHLD_TYPE_HOLDACTIVE_ACCEPTHELD = 2;
    // Add all held calls to a conference
    private static final int CHLD_TYPE_ADDHELDTOCONF = 3;

    // Indicates that no BluetoothCall is ringing
    private static final int DEFAULT_RINGING_ADDRESS_TYPE = 128;

    private int mNumActiveCalls = 0;
    private int mNumHeldCalls = 0;
    private int mNumChildrenOfActiveCall = 0;
    private int mBluetoothCallState = CALL_STATE_IDLE;
    private String mRingingAddress = "";
    private int mRingingAddressType = DEFAULT_RINGING_ADDRESS_TYPE;
    private BluetoothCall mOldHeldCall = null;
    private boolean mHeadsetUpdatedRecently = false;
    private boolean mIsDisconnectedTonePlaying = false;

    private static final Object LOCK = new Object();
    private BluetoothHeadsetProxy mBluetoothHeadset;


    public static final String ACTION_DSDA_CALL_STATE_CHANGE =
            "android.bluetooth.dsda.action.DSDA_CALL_STATE_CHANGED";

    public static final int ANSWER_CALL = 1;
    public static final int HANGUP_CALL = 2;
    public static final int PROCESS_CHLD= 3;
    public static final int HELD_CALL   = 4;
    public static final int LIST_CLCC   = 5;
    public static final int QUERY_PHONE_STATE = 6;
    public static final int CLEAN_UP = 7;

    @VisibleForTesting
    public TelephonyManager mTelephonyManager;

    @VisibleForTesting
    public TelecomManager mTelecomManager;

    @VisibleForTesting
    public final HashMap<String, CallStateCallback> mCallbacks = new HashMap<>();

    @VisibleForTesting
    public final HashMap<String, BluetoothCall> mBluetoothCallHashMap = new HashMap<>();

    // A map from Calls to indexes used to identify calls for CLCC (C* List Current Calls).
    private final Map<BluetoothCall, Integer> mClccIndexMap = new HashMap<>();

    private static BluetoothInCallService sInstance;

    public CallInfo mCallInfo = new CallInfo();

    protected boolean mServiceCreated = false;

    /**
     * Listens to connections and disconnections of bluetooth headsets.  We need to save the current
     * bluetooth headset so that we know where to send BluetoothCall updates.
     */
    @VisibleForTesting
    public BluetoothProfile.ServiceListener mProfileListener =
            new BluetoothProfile.ServiceListener() {
                @Override
                public void onServiceConnected(int profile, BluetoothProfile proxy) {
                    synchronized (LOCK) {
                        setBluetoothHeadset(new BluetoothHeadsetProxy((BluetoothHeadset) proxy));
                        updateHeadsetWithCallState(true /* force */);
                    }
                }

                @Override
                public void onServiceDisconnected(int profile) {
                    synchronized (LOCK) {
                        setBluetoothHeadset(null);
                    }
                }
            };

    public class BluetoothAdapterReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            synchronized (LOCK) {
                if (intent.getAction() != BluetoothAdapter.ACTION_STATE_CHANGED) {
                    Log.w(TAG, "BluetoothAdapterReceiver: Intent action " + intent.getAction());
                    return;
                }
                int state = intent
                        .getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);
                Log.d(TAG, "Bluetooth Adapter state: " + state);
                if (state == BluetoothAdapter.STATE_ON) {
                    queryPhoneState();
                }
            }
        }
    };

    /**
     * Receives events for global state changes of the bluetooth adapter.
     */
    // TODO: The code is moved from Telecom stack. Since we're running in the BT process itself,
    // we may be able to simplify this in a future patch.
    @VisibleForTesting
    public BluetoothAdapterReceiver mBluetoothAdapterReceiver;

    @VisibleForTesting
    public class CallStateCallback extends Call.Callback {
        public int mLastState;

        public CallStateCallback(int initialState) {
            mLastState = initialState;
        }

        public int getLastState() {
            return mLastState;
        }

        public void onStateChanged(BluetoothCall call, int state) {
            if (mCallInfo.isNullCall(call)) {
                return;
            }
            if (call.isExternalCall()) {
                return;
            }

            if (state == Call.STATE_DISCONNECTING) {
                Log.w(TAG, "ignoring DISCONNECTING call state");
                mLastState = state;
                return;
            }
            // If a BluetoothCall is being put on hold because of a new connecting call, ignore the
            // CONNECTING since the BT state update needs to send out the numHeld = 1 + dialing
            // state atomically.
            // When the BluetoothCall later transitions to DIALING/DISCONNECTED we will then
            // send out the aggregated update.
            if (getLastState() == Call.STATE_ACTIVE && state == Call.STATE_HOLDING) {
                for (BluetoothCall otherCall : mCallInfo.getBluetoothCalls()) {
                    if (otherCall.getState() == Call.STATE_CONNECTING) {
                        mLastState = state;
                        return;
                    }
                }
            }

            // To have an active BluetoothCall and another dialing at the same time is an invalid BT
            // state. We can assume that the active BluetoothCall will be automatically held
            // which will send another update at which point we will be in the right state.
            BluetoothCall activeCall = mCallInfo.getActiveCall();
            if (!mCallInfo.isNullCall(activeCall)
                    && getLastState() == Call.STATE_CONNECTING
                    && (state == Call.STATE_DIALING || state == Call.STATE_PULLING_CALL)) {
                mLastState = state;
                return;
            }
            mLastState = state;
            updateHeadsetWithCallState(false /* force */);
        }

        @Override
        public void onStateChanged(Call call, int state) {
            super.onStateChanged(call, state);
            onStateChanged(getBluetoothCallById(call.getDetails().getTelecomCallId()), state);
        }

        public void onDetailsChanged(BluetoothCall call, Call.Details details) {
            if (mCallInfo.isNullCall(call)) {
                return;
            }
            Log.i(TAG, "onDetailsChanged call: " + call + "details: " + details);
            if (call.isExternalCall()) {
                onCallRemoved(call);
            } else {
                if (!mBluetoothCallHashMap.containsKey(call.getTelecomCallId())) {
                    onCallAdded(call);
                 }else{
                   Log.i(TAG, "onDetailsChanged call was already added");
                   if (details.getState() == Call.STATE_DISCONNECTING) {
                     Log.i(TAG, "Ignore Call STATE_DISCONNECTING");
                   } else {
                     updateHeadsetWithCallState(false /* force */);
                   }
                }
            }
        }

        @Override
        public void onDetailsChanged(Call call, Call.Details details) {
            super.onDetailsChanged(call, details);
            onDetailsChanged(getBluetoothCallById(call.getDetails().getTelecomCallId()), details);
        }

        public void onParentChanged(BluetoothCall call) {
            if (call.isExternalCall()) {
                return;
            }
            if (call.getParentId() != null) {
                // If this BluetoothCall is newly conferenced, ignore the callback.
                // We only care about the one sent for the parent conference call.
                Log.d(TAG,
                        "Ignoring onIsConferenceChanged from child BluetoothCall with new parent");
                return;
            }
            updateHeadsetWithCallState(false /* force */);
        }

        @Override
        public void onParentChanged(Call call, Call parent) {
            super.onParentChanged(call, parent);
            onParentChanged(
                    getBluetoothCallById(call.getDetails().getTelecomCallId()));
        }

        public void onChildrenChanged(BluetoothCall call, List<BluetoothCall> children) {
            if (mCallInfo.isNullCall(call)) {
                return;
            }
            if (call.isExternalCall()) {
                return;
            }
            if (call.getChildrenIds().size() == 1) {
                // If this is a parent BluetoothCall with only one child,
                // ignore the callback as well since the minimum number of child calls to
                // start a conference BluetoothCall is 2. We expect this to be called again
                // when the parent BluetoothCall has another child BluetoothCall added.
                Log.d(TAG,
                        "Ignoring onIsConferenceChanged from parent with only one child call");
                return;
            }
            updateHeadsetWithCallState(false /* force */);
        }

        @Override
        public void onChildrenChanged(Call call, List<Call> children) {
            super.onChildrenChanged(call, children);
            onChildrenChanged(
                    getBluetoothCallById(call.getDetails().getTelecomCallId()),
                    getBluetoothCallsByIds(BluetoothCall.getIds(children)));
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.i(TAG, "onBind. Intent: " + intent);
        BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
            Log.i(TAG, "Bluetooth is off");
            ComponentName componentName
                    = new ComponentName(getPackageName(), this.getClass().getName());
            getPackageManager().setComponentEnabledSetting(
                    componentName,
                    PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                    PackageManager.DONT_KILL_APP);
            return null;
        }
        IBinder binder = super.onBind(intent);
        mTelephonyManager = getSystemService(TelephonyManager.class);
        mTelecomManager = getSystemService(TelecomManager.class);
        return binder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.i(TAG, "onUnbind. Intent: " + intent);
        return super.onUnbind(intent);
    }

    public BluetoothInCallService() {
        Log.i(TAG, "BluetoothInCallService is created");
        sInstance = this;
    }

    public static BluetoothInCallService getInstance() {
        return sInstance;
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    protected void enforceModifyPermission() {
        enforceCallingOrSelfPermission(android.Manifest.permission.MODIFY_PHONE_STATE, null);
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean answerCall(int profile) {
        Log.d(TAG, "answer received");
        if (ApmConstIntf.AudioProfiles.HFP == profile) {
            Log.d(TAG, "answercall: hfp");
            Intent DsdaIntent = new Intent(ACTION_DSDA_CALL_STATE_CHANGE);
            DsdaIntent.putExtra("state", ANSWER_CALL);
            sendBroadcastAsUser(DsdaIntent, UserHandle.ALL);
            return true;
        }
        synchronized (LOCK) {
            enforceModifyPermission();
            Log.i(TAG, "BT - answering call");
            BluetoothCall call = mCallInfo.getRingingOrSimulatedRingingCall();
            if (mCallInfo.isNullCall(call)) {
                return false;
            }
            call.answer(VideoProfile.STATE_AUDIO_ONLY);
            return true;
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean hangupCall(int profile) {
        if (ApmConstIntf.AudioProfiles.HFP == profile) {
            Log.d(TAG, "hangup call: hfp");
            Intent DsdaIntent = new Intent(ACTION_DSDA_CALL_STATE_CHANGE);
            DsdaIntent.putExtra("state", HANGUP_CALL);
            sendBroadcastAsUser(DsdaIntent, UserHandle.ALL);
            return true;
        }
        synchronized (LOCK) {
            enforceModifyPermission();
            Log.i(TAG, "BT - hanging up call");
            BluetoothCall call = mCallInfo.getForegroundCall();
            if (mCallInfo.isNullCall(call)) {
                return false;
            }
            // release the parent if there is a conference call
            BluetoothCall conferenceCall = getBluetoothCallById(call.getParentId());
            if (!mCallInfo.isNullCall(conferenceCall)
                    && conferenceCall.getState() == Call.STATE_ACTIVE) {
                Log.i(TAG, "BT - hanging up conference call");
                call = conferenceCall;
            }
            if (call.getState() == Call.STATE_RINGING) {
                call.reject(false, "");
            } else {
                call.disconnect();
            }
            return true;
        }
    }

    public boolean terminateCall(int index) {
        boolean ret = false;
        synchronized (LOCK) {
            enforceModifyPermission();
            Log.i(TAG, "BT - terminate call: " + index);
            int state = -1;
            BluetoothCall call = null;
            for (Map.Entry<BluetoothCall, Integer> entry : mClccIndexMap.entrySet()) {
                if (index == entry.getValue()) {
                    state = entry.getKey().getState();
                    call = entry.getKey();
                }
            }
            if (state == -1) {
                Log.e(TAG, "no such call with Index");
                return false;
            }
           if (call.getState() == Call.STATE_RINGING) {
                call.reject(false, "");
            } else {
                call.disconnect();
            }
            ret = true;
            return ret;
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean sendDtmf(int dtmf) {
        synchronized (LOCK) {
            enforceModifyPermission();
            Log.i(TAG, "BT - sendDtmf " + dtmf);
            BluetoothCall call = mCallInfo.getForegroundCall();
            if (mCallInfo.isNullCall(call)) {
                return false;
            }
            // TODO: Consider making this a queue instead of starting/stopping
            // in quick succession.
            call.playDtmfTone((char) dtmf);
            call.stopDtmfTone();
            return true;
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public String getNetworkOperator()  {
        synchronized (LOCK) {
            enforceModifyPermission();
            Log.i(TAG, "getNetworkOperator");
            if (mTelecomManager != null) {
                PhoneAccount account = mCallInfo.getBestPhoneAccount();
                if (account != null && account.getLabel() != null) {
                    return account.getLabel().toString();
                }
            } else {
                Log.e(TAG, "TelecomManager is Null");
                mTelephonyManager = (TelephonyManager) HeadsetService.getHeadsetService()
                    .getSystemService(Context.TELEPHONY_SERVICE);
            }
            if (mTelephonyManager == null) {
                Log.e(TAG, "TelephonyManager is Null");
                return null;
            }
            // Finally, just get the network name from telephony.
            return mTelephonyManager.getNetworkOperatorName();
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public String getSubscriberNumber() {
        synchronized (LOCK) {
            enforceModifyPermission();
            Log.i(TAG, "getSubscriberNumber");
            String address = null;
            if (mTelecomManager != null) {
                PhoneAccount account = mCallInfo.getBestPhoneAccount();
                if (account != null) {
                    Uri addressUri = account.getAddress();
                    if (addressUri != null) {
                        address = addressUri.getSchemeSpecificPart();
                    }
                }
            } else {
                Log.e(TAG, "TelecomManager is Null");
                mTelephonyManager = (TelephonyManager) HeadsetService.getHeadsetService()
                    .getSystemService(Context.TELEPHONY_SERVICE);
            }
            if (mTelephonyManager == null) {
                Log.e(TAG, "TelephonyManager is Null");
                return null;
            }
            if (TextUtils.isEmpty(address)) {
                address = mTelephonyManager.getLine1Number();
                if (address == null) address = "";
            }
            return address;
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean listCurrentCalls(int profile) {
        if (mServiceCreated == false) {
            Log.w(TAG, "listCurrentCalls called when service is not created");
            return false;
        }
        if (ApmConstIntf.AudioProfiles.HFP == profile) {
            Log.d(TAG, "listCurrentCalls: hfp");
            Intent DsdaIntent = new Intent(ACTION_DSDA_CALL_STATE_CHANGE);
            DsdaIntent.putExtra("state", LIST_CLCC);
            sendBroadcastAsUser(DsdaIntent, UserHandle.ALL);
            return true;
        }

        synchronized (LOCK) {
            enforceModifyPermission();
            // only log if it is after we recently updated the headset state or else it can
            // clog the android log since this can be queried every second.
            boolean logQuery = mHeadsetUpdatedRecently;
            mHeadsetUpdatedRecently = false;

            if (logQuery) {
                Log.i(TAG, "listcurrentCalls");
            }

            sendListOfCalls(logQuery);
            return true;
        }
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean queryPhoneState() {
        Intent DsdaIntent = new Intent(ACTION_DSDA_CALL_STATE_CHANGE);
        DsdaIntent.putExtra("state", QUERY_PHONE_STATE);
        sendBroadcastAsUser(DsdaIntent, UserHandle.ALL);
        synchronized (LOCK) {
            enforceModifyPermission();
            Log.i(TAG, "queryPhoneState");
            updateHeadsetWithCallState(true);
            return true;
        }
    }

    public void cleanUp() {
        mBluetoothCallHashMap.clear();
        Intent DsdaIntent = new Intent(ACTION_DSDA_CALL_STATE_CHANGE);
        DsdaIntent.putExtra("state", CLEAN_UP);
        sendBroadcastAsUser(DsdaIntent, UserHandle.ALL);
        Log.i(TAG, "BluetoothCallHashMap Cleared");
    }

    public boolean isCsCallInProgress() {
        boolean isCsCall = false;
        BluetoothCall activeCall = mCallInfo.getActiveCall();
        if (mNumActiveCalls > 0) {
            isCsCall =  ((activeCall != null) &&
             !(activeCall.isHighDefAudio() ||
             activeCall.isWifi()));
        }
        Log.i(TAG, "isCsCallInProgress: "+ isCsCall);
        return isCsCall;
    }

    public boolean isHighDefCallInProgress() {
        boolean isHighDef = false;
        BluetoothCall ringingCall = mCallInfo.getRingingOrSimulatedRingingCall();
        BluetoothCall dialingCall = mCallInfo.getOutgoingCall();
        BluetoothCall activeCall = mCallInfo.getActiveCall();

        /* If its an incoming call we will have codec info in dialing state */
        if (ringingCall != null) {
            isHighDef = ringingCall.isHighDefAudio();
        } else if (dialingCall != null) { /* CS dialing call has codec info in dialing state */
            Bundle extras = dialingCall.getDetails().getExtras();
            if (extras != null) {
                int phoneType = extras.getInt(
                    TelecomManager.EXTRA_CALL_TECHNOLOGY_TYPE);
                if (phoneType == PhoneConstants.PHONE_TYPE_GSM
                    || phoneType == PhoneConstants.PHONE_TYPE_CDMA) {
                    isHighDef = dialingCall.isHighDefAudio();
                /* For IMS calls codec info is not present in dialing state */
                } else if (phoneType == PhoneConstants.PHONE_TYPE_IMS
                    || phoneType == PhoneConstants.PHONE_TYPE_CDMA_LTE) {
                    isHighDef = true;
                }
             }
        } else if (activeCall != null) {
            isHighDef = activeCall.isHighDefAudio();
        }
        Log.i(TAG, "isHighDefCallInProgress: Call is High Def " + isHighDef);
        return isHighDef;
    }

    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean processChld(int chld, int profile) {
        if (ApmConstIntf.AudioProfiles.HFP == profile) {
            Log.d(TAG, "processChld: hfp");
            Intent DsdaIntent = new Intent(ACTION_DSDA_CALL_STATE_CHANGE);
            DsdaIntent.putExtra("state", PROCESS_CHLD);
            DsdaIntent.putExtra("chld", chld);
            sendBroadcastAsUser(DsdaIntent, UserHandle.ALL);
            return true;
        }
        synchronized (LOCK) {
            enforceModifyPermission();
            long token = Binder.clearCallingIdentity();
            Log.i(TAG, "processChld " + chld);
            return _processChld(chld);
        }
    }

    public boolean holdCall(int index) {
        synchronized (LOCK) {
            enforceModifyPermission();
            long token = Binder.clearCallingIdentity();
            Log.i(TAG, "holdCall " + index);
            return _holdCall(index);
        }
    }

    public void onCallAdded(BluetoothCall call) {
        if (call.isExternalCall()) {
            return;
        }
        if (!mBluetoothCallHashMap.containsKey(call.getTelecomCallId())) {
            Log.d(TAG, "onCallAdded");
            CallStateCallback callback = new CallStateCallback(call.getState());
            mCallbacks.put(call.getTelecomCallId(), callback);
            call.registerCallback(callback);

            mBluetoothCallHashMap.put(call.getTelecomCallId(), call);
            updateHeadsetWithCallState(false /* force */);
        }
    }

    @Override
    public void onCallAdded(Call call) {
        super.onCallAdded(call);
        onCallAdded(new BluetoothCall(call));
    }

    public void onCallRemoved(BluetoothCall call) {
        if (call.isExternalCall()) {
            return;
        }
        Log.d(TAG, "onCallRemoved");
        CallStateCallback callback = getCallback(call);
        if (callback != null) {
            call.unregisterCallback(callback);
        }

        if (mBluetoothCallHashMap.containsKey(call.getTelecomCallId())) {
            mBluetoothCallHashMap.remove(call.getTelecomCallId());
        }

        mClccIndexMap.remove(call);
        updateHeadsetWithCallState(false /* force */);
    }

    @Override
    public void onCallRemoved(Call call) {
        super.onCallRemoved(call);
        BluetoothCall bluetoothCall = getBluetoothCallById(call.getDetails().getTelecomCallId());
        if (bluetoothCall == null) {
            Log.w(TAG, "onCallRemoved, BluetoothCall is removed before registered");
            return;
        }
        onCallRemoved(bluetoothCall);
    }

    @Override
    public void onCallAudioStateChanged(CallAudioState audioState) {
        super.onCallAudioStateChanged(audioState);
        Log.d(TAG, "onCallAudioStateChanged, audioState == " + audioState);
    }


    @Override
    public void onCreate() {
        Log.d(TAG, "onCreate");
        super.onCreate();
        BluetoothAdapter.getDefaultAdapter()
                .getProfileProxy(this, mProfileListener, BluetoothProfile.HEADSET);
        mBluetoothAdapterReceiver = new BluetoothAdapterReceiver();
        IntentFilter intentFilter = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
        registerReceiver(mBluetoothAdapterReceiver, intentFilter);
        mServiceCreated = true;
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy");
        synchronized (LOCK) {
          if (mBluetoothAdapterReceiver != null) {
              unregisterReceiver(mBluetoothAdapterReceiver);
              mBluetoothAdapterReceiver = null;
          }
          if (mBluetoothHeadset != null) {
              mBluetoothHeadset.closeBluetoothHeadsetProxy(this);
              mBluetoothHeadset = null;
          }
        }//synchronized (LOCK)
        mServiceCreated = false;
        super.onDestroy();
    }

    private void sendListOfCalls(boolean shouldLog) {
        Collection<BluetoothCall> calls = mCallInfo.getBluetoothCalls();
        for (BluetoothCall call : calls) {
            // We don't send the parent conference BluetoothCall to the bluetooth device.
            // We do, however want to send conferences that have no children to the bluetooth
            // device (e.g. IMS Conference).
            if (!call.isConference()
                    || (call.isConference()
                            && call.can(Connection.CAPABILITY_CONFERENCE_HAS_NO_CHILDREN))) {
                sendClccForCall(call, shouldLog);
            }
        }
        sendClccEndMarker();
    }

    private void sendClccEndMarker() {
        // End marker is recognized with an index value of 0. All other parameters are ignored.
        if (mBluetoothHeadset != null) {
            mBluetoothHeadset.clccResponse(0 /* index */, 0, 0, 0, false, null, 0);
        }
    }

    /**
     * Sends a single clcc (C* List Current Calls) event for the specified call.
     */
    private void sendClccForCall(BluetoothCall call, boolean shouldLog) {
        boolean isForeground = mCallInfo.getForegroundCall() == call;
        int state = getBtCallState(call, isForeground);
        boolean isPartOfConference = false;
        boolean isConferenceWithNoChildren = call.isConference()
                && call.can(Connection.CAPABILITY_CONFERENCE_HAS_NO_CHILDREN);

        if (state == CALL_STATE_IDLE) {
            return;
        }

        BluetoothCall conferenceCall = getBluetoothCallById(call.getParentId());
        if (!mCallInfo.isNullCall(conferenceCall)) {
            isPartOfConference = true;

            // Run some alternative states for Conference-level merge/swap support.
            // Basically, if BluetoothCall supports swapping or merging at the conference-level,
            // then we need to expose the calls as having distinct states
            // (ACTIVE vs CAPABILITY_HOLD) or
            // the functionality won't show up on the bluetooth device.

            // Before doing any special logic, ensure that we are dealing with an
            // ACTIVE BluetoothCall and that the conference itself has a notion of
            // the current "active" child call.
            BluetoothCall activeChild = getBluetoothCallById(
                    conferenceCall.getGenericConferenceActiveChildCallId());
            if (state == CALL_STATE_ACTIVE && !mCallInfo.isNullCall(activeChild)) {
                // Reevaluate state if we can MERGE or if we can SWAP without previously having
                // MERGED.
                boolean shouldReevaluateState =
                        conferenceCall.can(Connection.CAPABILITY_MERGE_CONFERENCE)
                                || (conferenceCall.can(Connection.CAPABILITY_SWAP_CONFERENCE)
                                        && !conferenceCall.wasConferencePreviouslyMerged());

                if (shouldReevaluateState) {
                    isPartOfConference = false;
                    if (call == activeChild) {
                        state = CALL_STATE_ACTIVE;
                    } else {
                        // At this point we know there is an "active" child and we know that it is
                        // not this call, so set it to HELD instead.
                        state = CALL_STATE_HELD;
                    }
                }
            }
            if (conferenceCall.getState() == Call.STATE_HOLDING
                    && conferenceCall.can(Connection.CAPABILITY_MANAGE_CONFERENCE)) {
                // If the parent IMS CEP conference BluetoothCall is on hold, we should mark
                // this BluetoothCall as being on hold regardless of what the other
                // children are doing.
                state = CALL_STATE_HELD;
            }
        } else if (isConferenceWithNoChildren) {
            // Handle the special case of an IMS conference BluetoothCall without conference
            // event package support.
            // The BluetoothCall will be marked as a conference, but the conference will not have
            // child calls where conference event packages are not used by the carrier.
            isPartOfConference = true;
        }

        int index = getIndexForCall(call);
        int direction = call.isIncoming() ? 1 : 0;
        final Uri addressUri;
        if (call.getGatewayInfo() != null) {
            addressUri = call.getGatewayInfo().getOriginalAddress();
        } else {
            addressUri = call.getHandle();
        }

        String address = addressUri == null ? null : addressUri.getSchemeSpecificPart();
        if (address != null) {
            address = PhoneNumberUtils.stripSeparators(address);
        }

        // Don't send host call information when IMS calls are conferenced
        String subsNum = getSubscriberNumber();
        if (subsNum != null && address != null) {
            Log.d(TAG, "subscriber number " + subsNum + " address " + address);
            if (subsNum.equals(address)) {
                Log.w(TAG, "return without sending host call in CLCC");
                  return;
            }
        }

        int addressType = address == null ? -1 : PhoneNumberUtils.toaFromString(address);

        if (shouldLog) {
            Log.i(TAG, "sending clcc for BluetoothCall "
                            + index + ", "
                            + direction + ", "
                            + state + ", "
                            + isPartOfConference + ", "
                            + addressType);
        }

        if (mBluetoothHeadset != null) {
            mBluetoothHeadset.clccResponse(
                    index, direction, state, 0, isPartOfConference, address, addressType);
        }
    }

    /**
     * Returns the caches index for the specified call.  If no such index exists, then an index is
     * given (smallest number starting from 1 that isn't already taken).
     */
    private int getIndexForCall(BluetoothCall call) {
        if (mClccIndexMap.containsKey(call)) {
            return mClccIndexMap.get(call);
        }

        int i = 1;  // Indexes for bluetooth clcc are 1-based.
        while (mClccIndexMap.containsValue(i)) {
            i++;
        }

        // NOTE: Indexes are removed in {@link #onCallRemoved}.
        mClccIndexMap.put(call, i);
        return i;
    }
    /*
     * Hold the active or Incoming call based on parameter received
     *
     * return true on success, fail otherwise
     */
    private boolean _holdCall(int index) {
        boolean ret = false;
        int state = -1;
        BluetoothCall call = null;
        for (Map.Entry<BluetoothCall, Integer> entry : mClccIndexMap.entrySet()) {
            if (index == entry.getValue()) {
                state = entry.getKey().getState();
                call = entry.getKey();
            }
        }
        if (state == -1) {
            Log.e(TAG, "no such call with Index");
            return false;
        }
        boolean isForeground = mCallInfo.getForegroundCall() == call;
        int btState = getBtCallState(call, isForeground);

        if (btState == CALL_STATE_INCOMING) {
            BluetoothCall ringingCall = mCallInfo.getRingingOrSimulatedRingingCall();
            if (ringingCall == null) {
                Log.i(TAG, "ringingCall null");
            } else {
               ringingCall.hold();
               ret = true;
            }
        } else if (btState == CALL_STATE_ACTIVE) {
            BluetoothCall activeCall = mCallInfo.getActiveCall();
            if (activeCall == null) {
                Log.i(TAG, "activeCall null");
            }
            Log.i(TAG, "activeCall.can(Connection.CAPABILITY_HOLD)" + activeCall.can(Connection.CAPABILITY_HOLD));
            if (!mCallInfo.isNullCall(activeCall)
                    && activeCall.can(Connection.CAPABILITY_HOLD)) {
                Log.i(TAG, "holding activeCall");
                activeCall.hold();
                ret = true;
            }

        }
        return ret;
    }

    private boolean _processChld(int chld) {
        BluetoothCall activeCall = mCallInfo.getActiveCall();
        BluetoothCall ringingCall = mCallInfo.getRingingOrSimulatedRingingCall();
        if (ringingCall == null) {
            Log.i(TAG, "asdf ringingCall null");
        } else {
            Log.i(TAG, "asdf ringingCall not null " + ringingCall.hashCode());
        }

        BluetoothCall heldCall = mCallInfo.getHeldCall();

        Log.i(TAG, "Active: " + activeCall
                + " Ringing: " + ringingCall
                + " Held: " + heldCall);
        Log.i(TAG, "asdf chld " + chld);

        if (chld == CHLD_TYPE_RELEASEHELD) {
            Log.i(TAG, "asdf CHLD_TYPE_RELEASEHELD");
            if (!mCallInfo.isNullCall(ringingCall)) {
                Log.i(TAG, "asdf reject " + ringingCall.hashCode());
                ringingCall.reject(false, null);
                return true;
            } else if (!mCallInfo.isNullCall(heldCall)) {
                heldCall.disconnect();
                return true;
            }
        } else if (chld == CHLD_TYPE_RELEASEACTIVE_ACCEPTHELD) {
            if (mCallInfo.isNullCall(activeCall)
                    && mCallInfo.isNullCall(ringingCall)
                    && mCallInfo.isNullCall(heldCall)) {
                return false;
            }
            if (!mCallInfo.isNullCall(activeCall)) {
                BluetoothCall conferenceCall = getBluetoothCallById(activeCall.getParentId());
                if (!mCallInfo.isNullCall(conferenceCall)
                        && conferenceCall.getState() == Call.STATE_ACTIVE) {
                    Log.i(TAG, "CHLD: disconnect conference call");
                    conferenceCall.disconnect();
                } else {
                    activeCall.disconnect();
                }
            }
            if (!mCallInfo.isNullCall(ringingCall)) {
                ringingCall.answer(ringingCall.getVideoState());
            } else if (!mCallInfo.isNullCall(heldCall)) {
                heldCall.unhold();
            }
            return true;
        } else if (chld == CHLD_TYPE_HOLDACTIVE_ACCEPTHELD) {
            if (!mCallInfo.isNullCall(activeCall)
                    && activeCall.can(Connection.CAPABILITY_SWAP_CONFERENCE)) {
                activeCall.swapConference();
                Log.i(TAG, "CDMA calls in conference swapped, updating headset");
                updateHeadsetWithCallState(true /* force */);
                return true;
            } else if (!mCallInfo.isNullCall(ringingCall)) {
                ringingCall.answer(VideoProfile.STATE_AUDIO_ONLY);
                return true;
            } else if (!mCallInfo.isNullCall(heldCall)) {
                // CallsManager will hold any active calls when unhold() is called on a
                // currently-held call.
                heldCall.unhold();
                return true;
            } else if (!mCallInfo.isNullCall(activeCall)
                    && (activeCall.can(Connection.CAPABILITY_HOLD)
                    ||  activeCall.can(Connection.CAPABILITY_SUPPORT_HOLD))) {
                activeCall.hold();
                return true;
            }
        } else if (chld == CHLD_TYPE_ADDHELDTOCONF) {
            if (!mCallInfo.isNullCall(activeCall)) {
                if (activeCall.can(Connection.CAPABILITY_MERGE_CONFERENCE)) {
                    activeCall.mergeConference();
                    return true;
                } else {
                    List<BluetoothCall> conferenceable = getBluetoothCallsByIds(
                            activeCall.getConferenceableCalls());
                    if (!conferenceable.isEmpty()) {
                        activeCall.conference(conferenceable.get(0));
                        return true;
                    }
                }
            }
        }
        return false;
    }

    /**
     * Sends an update of the current BluetoothCall state to the current Headset.
     *
     * @param force {@code true} if the headset state should be sent regardless if no changes to
     * the state have occurred, {@code false} if the state should only be sent if the state
     * has changed.
     */
    private void updateHeadsetWithCallState(boolean force) {
        BluetoothCall activeCall = mCallInfo.getActiveCall();
        BluetoothCall ringingCall = mCallInfo.getRingingOrSimulatedRingingCall();
        BluetoothCall heldCall = mCallInfo.getHeldCall();

        int bluetoothCallState = getBluetoothCallStateForUpdate();

        String ringingAddress = null;
        int ringingAddressType = DEFAULT_RINGING_ADDRESS_TYPE;
        String ringingName = null;
        if (!mCallInfo.isNullCall(ringingCall) && ringingCall.getHandle() != null
                && !ringingCall.isSilentRingingRequested()) {
            ringingAddress = ringingCall.getHandle().getSchemeSpecificPart();
            if (ringingAddress != null) {
                ringingAddressType = PhoneNumberUtils.toaFromString(ringingAddress);
            }
            ringingName = ringingCall.getCallerDisplayName();
            if (TextUtils.isEmpty(ringingName)) {
                ringingName = ringingCall.getContactDisplayName();
            }
        }
        if (ringingAddress == null) {
            ringingAddress = "";
        }

        int numActiveCalls = mCallInfo.isNullCall(activeCall) ? 0 : 1;
        int numHeldCalls = mCallInfo.getNumHeldCalls();
        int numChildrenOfActiveCall =
                mCallInfo.isNullCall(activeCall) ? 0 : activeCall.getChildrenIds().size();

        // Intermediate state for GSM calls which are in the process of being swapped.
        // TODO: Should we be hardcoding this value to 2 or should we check if all top level calls
        //       are held?
        boolean callsPendingSwitch = (numHeldCalls == 2);

        // For conference calls which support swapping the active BluetoothCall within the
        // conference (namely CDMA calls) we need to expose that as a held BluetoothCall
        // in order for the BT device to show "swap" and "merge" functionality.
        boolean ignoreHeldCallChange = false;
        if (!mCallInfo.isNullCall(activeCall) && activeCall.isConference()
                && !activeCall.can(Connection.CAPABILITY_CONFERENCE_HAS_NO_CHILDREN)) {
            if (activeCall.can(Connection.CAPABILITY_SWAP_CONFERENCE)) {
                // Indicate that BT device should show SWAP command by indicating that there is a
                // BluetoothCall on hold, but only if the conference wasn't previously merged.
                numHeldCalls = activeCall.wasConferencePreviouslyMerged() ? 0 : 1;
            } else if (activeCall.can(Connection.CAPABILITY_MERGE_CONFERENCE)) {
                numHeldCalls = 1;  // Merge is available, so expose via numHeldCalls.
            }

            for (String id : activeCall.getChildrenIds()) {
                // Held BluetoothCall has changed due to it being combined into a CDMA conference.
                // Keep track of this and ignore any future update since it doesn't really count
                // as a BluetoothCall change.
                if (mOldHeldCall != null && mOldHeldCall.getTelecomCallId() == id) {
                    ignoreHeldCallChange = true;
                    break;
                }
            }
        }
        synchronized (LOCK) {
          if (mBluetoothHeadset != null
                && (force
                    || (!callsPendingSwitch
                        && (numActiveCalls != mNumActiveCalls
                            || numChildrenOfActiveCall != mNumChildrenOfActiveCall
                            || numHeldCalls != mNumHeldCalls
                            || bluetoothCallState != mBluetoothCallState
                            || !TextUtils.equals(ringingAddress, mRingingAddress)
                            || ringingAddressType != mRingingAddressType
                            || (heldCall != mOldHeldCall && !ignoreHeldCallChange))))) {

            // If the BluetoothCall is transitioning into the alerting state, send DIALING first.
            // Some devices expect to see a DIALING state prior to seeing an ALERTING state
            // so we need to send it first.
            boolean sendDialingFirst = mBluetoothCallState != bluetoothCallState
                    && bluetoothCallState == CALL_STATE_ALERTING;

            mOldHeldCall = heldCall;
            mNumActiveCalls = numActiveCalls;
            mNumChildrenOfActiveCall = numChildrenOfActiveCall;
            mNumHeldCalls = numHeldCalls;
            mBluetoothCallState = bluetoothCallState;
            mRingingAddress = ringingAddress;
            mRingingAddressType = ringingAddressType;

            if (sendDialingFirst) {
                // Log in full to make logs easier to debug.
                Log.i(TAG, "updateHeadsetWithCallState "
                                + "numActive " + mNumActiveCalls + ", "
                                + "numHeld " + mNumHeldCalls + ", "
                                + "callState " + CALL_STATE_DIALING + ", "
                                + "ringing type " + mRingingAddressType);
                mBluetoothHeadset.phoneStateChanged(
                        mNumActiveCalls,
                        mNumHeldCalls,
                        CALL_STATE_DIALING,
                        mRingingAddress,
                        mRingingAddressType,
                        ringingName);
            }

            Log.i(TAG, "updateHeadsetWithCallState "
                    + "numActive " + mNumActiveCalls + ", "
                    + "numHeld " + mNumHeldCalls + ", "
                    + "callState " + mBluetoothCallState + ", "
                    + "ringing type " + mRingingAddressType);

            mBluetoothHeadset.phoneStateChanged(
                    mNumActiveCalls,
                    mNumHeldCalls,
                    mBluetoothCallState,
                    mRingingAddress,
                    mRingingAddressType,
                    ringingName);

            mHeadsetUpdatedRecently = true;
          }
        }//synchronized (LOCK)
    }

    private int getBluetoothCallStateForUpdate() {
        BluetoothCall ringingCall = mCallInfo.getRingingOrSimulatedRingingCall();
        BluetoothCall dialingCall = mCallInfo.getOutgoingCall();
        boolean hasOnlyDisconnectedCalls = mCallInfo.hasOnlyDisconnectedCalls();

        //
        // !! WARNING !!
        // You will note that CALL_STATE_WAITING, CALL_STATE_HELD, and CALL_STATE_ACTIVE are not
        // used in this version of the BluetoothCall state mappings.  This is on purpose.
        // phone_state_change() in btif_hf.c is not written to handle these states. Only with the
        // listCalls*() method are WAITING and ACTIVE used.
        // Using the unsupported states here caused problems with inconsistent state in some
        // bluetooth devices (like not getting out of ringing state after answering a call).
        //
        int bluetoothCallState = CALL_STATE_IDLE;
        if (!mCallInfo.isNullCall(ringingCall) && !ringingCall.isSilentRingingRequested()) {
            bluetoothCallState = CALL_STATE_INCOMING;
        } else if ((!mCallInfo.isNullCall(dialingCall))
                  &&(dialingCall.getState() == Call.STATE_DIALING)
                  &&(null != dialingCall.getDetails().getExtras())
                  &&(0 != dialingCall.getDetails().getExtras().getInt(TelecomManager.EXTRA_CALL_TECHNOLOGY_TYPE))) {
            bluetoothCallState = CALL_STATE_ALERTING;
            Log.i(TAG, "updateHeadsetWithCallState CALL_STATE_ALERTING");
        } else if (hasOnlyDisconnectedCalls || mIsDisconnectedTonePlaying) {
            // Keep the DISCONNECTED state until the disconnect tone's playback is done
            bluetoothCallState = CALL_STATE_DISCONNECTED;
        }
        return bluetoothCallState;
    }

    private int getBtCallState(BluetoothCall call, boolean isForeground) {
        switch (call.getState()) {
            case Call.STATE_NEW:
            case Call.STATE_DISCONNECTED:
            case Call.STATE_AUDIO_PROCESSING:
                return CALL_STATE_IDLE;

            case Call.STATE_ACTIVE:
                return CALL_STATE_ACTIVE;

            case Call.STATE_CONNECTING:
            case Call.STATE_SELECT_PHONE_ACCOUNT:
            case Call.STATE_DIALING:
            case Call.STATE_PULLING_CALL:
                // Yes, this is correctly returning ALERTING.
                // "Dialing" for BT means that we have sent information to the service provider
                // to place the BluetoothCall but there is no confirmation that the BluetoothCall
                // is going through. When there finally is confirmation, the ringback is
                // played which is referred to as an "alert" tone, thus, ALERTING.
                // TODO: We should consider using the ALERTING terms in Telecom because that
                // seems to be more industry-standard.
                return CALL_STATE_ALERTING;

            case Call.STATE_HOLDING:
                return CALL_STATE_HELD;

            case Call.STATE_RINGING:
            case Call.STATE_SIMULATED_RINGING:
                if (call.isSilentRingingRequested()) {
                    return CALL_STATE_IDLE;
                } else if (isForeground) {
                    return CALL_STATE_INCOMING;
                } else {
                    return CALL_STATE_WAITING;
                }
        }
        return CALL_STATE_IDLE;
    }

    @VisibleForTesting
    public CallStateCallback getCallback(BluetoothCall call) {
        return mCallbacks.get(call.getTelecomCallId());
    }

    @VisibleForTesting
    public void setBluetoothHeadset(BluetoothHeadsetProxy bluetoothHeadset) {
        mBluetoothHeadset = bluetoothHeadset;
    }

    @VisibleForTesting
    public BluetoothCall getBluetoothCallById(String id) {
        if (mBluetoothCallHashMap.containsKey(id)) {
            return mBluetoothCallHashMap.get(id);
        }
        return null;
    }

    @VisibleForTesting
    public List<BluetoothCall> getBluetoothCallsByIds(List<String> ids) {
        List<BluetoothCall> calls = new ArrayList<>();
        for (String id : ids) {
            BluetoothCall call = getBluetoothCallById(id);
            if (!mCallInfo.isNullCall(call)) {
                calls.add(call);
            }
        }
        return calls;
    }

    // extract call information functions out into this part, so we can mock it in testing
    @VisibleForTesting
    public class CallInfo {

        public BluetoothCall getForegroundCall() {
            LinkedHashSet<Integer> states = new LinkedHashSet<Integer>();
            BluetoothCall foregroundCall;

            states.add(Call.STATE_CONNECTING);
            foregroundCall = getCallByStates(states);
            if (!mCallInfo.isNullCall(foregroundCall)) {
                return foregroundCall;
            }

            states.clear();
            states.add(Call.STATE_ACTIVE);
            states.add(Call.STATE_DIALING);
            states.add(Call.STATE_PULLING_CALL);
            foregroundCall = getCallByStates(states);
            if (!mCallInfo.isNullCall(foregroundCall)) {
                return foregroundCall;
            }

            states.clear();
            states.add(Call.STATE_RINGING);
            foregroundCall = getCallByStates(states);
            if (!mCallInfo.isNullCall(foregroundCall)) {
                return foregroundCall;
            }

            return null;
        }

        public BluetoothCall getCallByStates(LinkedHashSet<Integer> states) {
            List<BluetoothCall> calls = getBluetoothCalls();
            for (BluetoothCall call : calls) {
                if (states.contains(call.getState())) {
                    return call;
                }
            }
            return null;
        }

        public BluetoothCall getCallByState(int state) {
            List<BluetoothCall> calls = getBluetoothCalls();
            for (BluetoothCall call : calls) {
                if (state == call.getState()) {
                    return call;
                }
            }
            return null;
        }

        public int getNumHeldCalls() {
            int number = 0;
            List<BluetoothCall> calls = getBluetoothCalls();
            for (BluetoothCall call : calls) {
                if (call.getState() == Call.STATE_HOLDING) {
                    number++;
                }
            }
            return number;
        }

        public boolean hasOnlyDisconnectedCalls() {
            List<BluetoothCall> calls = getBluetoothCalls();
            if (calls.size() == 0) {
                return false;
            }
            for (BluetoothCall call : calls) {
                if (call.getState() != Call.STATE_DISCONNECTED) {
                    return false;
                }
            }
            return true;
        }

        public List<BluetoothCall> getBluetoothCalls() {
            return getBluetoothCallsByIds(BluetoothCall.getIds(getCalls()));
        }

        public BluetoothCall getOutgoingCall() {
            LinkedHashSet<Integer> states = new LinkedHashSet<Integer>();
            states.add(Call.STATE_CONNECTING);
            states.add(Call.STATE_DIALING);
            states.add(Call.STATE_PULLING_CALL);
            return getCallByStates(states);
        }

        public BluetoothCall getRingingOrSimulatedRingingCall() {
            LinkedHashSet<Integer> states = new LinkedHashSet<Integer>();
            states.add(Call.STATE_RINGING);
            states.add(Call.STATE_SIMULATED_RINGING);
            return getCallByStates(states);
        }

        public BluetoothCall getActiveCall() {
            return getCallByState(Call.STATE_ACTIVE);
        }

        public BluetoothCall getHeldCall() {
            return getCallByState(Call.STATE_HOLDING);
        }

        /**
         * Returns the best phone account to use for the given state of all calls.
         * First, tries to return the phone account for the foreground call, second the default
         * phone account for PhoneAccount.SCHEME_TEL.
         */
        public PhoneAccount getBestPhoneAccount() {
            BluetoothCall call = getForegroundCall();

            PhoneAccount account = null;
            if (!mCallInfo.isNullCall(call)) {
                PhoneAccountHandle handle = call.getAccountHandle();
                if (handle != null) {
                    // First try to get the network name of the foreground call.
                    account = mTelecomManager.getPhoneAccount(handle);
                }
            }

            if (account == null) {
                // Second, Try to get the label for the default Phone Account.
                List<PhoneAccountHandle> handles =
                        mTelecomManager.getPhoneAccountsSupportingScheme(PhoneAccount.SCHEME_TEL);
                while (handles.iterator().hasNext()) {
                    account = mTelecomManager.getPhoneAccount(handles.iterator().next());
                    if (account != null) {
                        return account;
                    }
                }
            }
            return null;
        }

        public boolean isNullCall(BluetoothCall call) {
            return call == null || call.getCall() == null;
        }
    };
};
