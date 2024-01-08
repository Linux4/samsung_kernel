/*
 * Copyright 2017 The Android Open Source Project
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

package com.android.bluetooth.hfp;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.RequiresPermission;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import com.android.bluetooth.telephony.BluetoothInCallService;
import android.telephony.TelephonyManager;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.media.AudioManager;
import android.os.PowerManager;
import android.os.SystemProperties;
import android.util.Log;

import com.android.bluetooth.apm.ApmConstIntf;
import com.android.bluetooth.apm.CallAudioIntf;
import com.android.bluetooth.apm.ActiveDeviceManagerServiceIntf;

import com.android.internal.annotations.VisibleForTesting;

import java.util.List;

/**
 * Defines system calls that is used by state machine/service to either send or receive
 * messages from the Android System.
 */
@VisibleForTesting
public class HeadsetSystemInterface {
    private static final String TAG = HeadsetSystemInterface.class.getSimpleName();
    private static final boolean DBG = true;

    private final HeadsetService mHeadsetService;
    private final AudioManager mAudioManager;
    private final HeadsetPhoneState mHeadsetPhoneState;
    private PowerManager.WakeLock mVoiceRecognitionWakeLock;
    private final TelephonyManager mTelephonyManager;
    private static final int ANSWERCALL_DELAY_DEFAULT = 100;
    private static int mAnswerCallDelay;

    HeadsetSystemInterface(HeadsetService headsetService) {
        if (headsetService == null) {
            Log.wtf(TAG, "HeadsetService parameter is null");
        }
        mHeadsetService = headsetService;
        mAudioManager = (AudioManager) mHeadsetService.getSystemService(Context.AUDIO_SERVICE);
        PowerManager powerManager =
                (PowerManager) mHeadsetService.getSystemService(Context.POWER_SERVICE);
        mVoiceRecognitionWakeLock =
                powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG + ":VoiceRecognition");
        mVoiceRecognitionWakeLock.setReferenceCounted(false);
        mHeadsetPhoneState = new com.android.bluetooth.hfp.HeadsetPhoneState(mHeadsetService);
        mTelephonyManager = (TelephonyManager) mHeadsetService.getSystemService(
                                                      Context.TELEPHONY_SERVICE);
        mAnswerCallDelay = SystemProperties.getInt("persist.vendor.bluetooth.answercalldelay",
                                                    ANSWERCALL_DELAY_DEFAULT);
    }

    private BluetoothInCallService getBluetoothInCallServiceInstance() {
        return BluetoothInCallService.getInstance();
    }

    /**
     * Special function for use by the system to resolve service
     * intents to system apps.  Throws an exception if there are
     * multiple potential matches to the Intent.  Returns null if
     * there are no matches.
     */
    private @Nullable ComponentName resolveSystemService(@NonNull PackageManager pm,
            int flags, Intent intent) {
        if (intent.getComponent() != null) {
            return intent.getComponent();
        }

        List<ResolveInfo> results = pm.queryIntentServices(intent, flags);
        if (results == null) {
            return null;
        }
        ComponentName comp = null;
        for (int i = 0; i < results.size(); i++) {
            ResolveInfo ri = results.get(i);
            if ((ri.serviceInfo.applicationInfo.flags& ApplicationInfo.FLAG_SYSTEM) == 0) {
                continue;
            }
            ComponentName foundComp = new ComponentName(ri.serviceInfo.applicationInfo.packageName,
                    ri.serviceInfo.name);
            if (comp != null) {
                throw new IllegalStateException("Multiple system services handle " + this
                        + ": " + comp + ", " + foundComp);
            }
            comp = foundComp;
        }
        return comp;
    }

    /**
     * Stop this system interface
     */
    public synchronized void stop() {
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        if (bluetoothInCallService != null) {
            bluetoothInCallService.cleanUp();
        } else {
            Log.e(TAG, "No Cleanup, as BluetoothInCallService is null");
        }
        mHeadsetPhoneState.cleanup();
    }

    /**
     * Get audio manager. Most audio manager oprations are pass through and therefore are not
     * individually managed by this class
     *
     * @return audio manager for setting audio parameters
     */
    @VisibleForTesting
    public AudioManager getAudioManager() {
        return mAudioManager;
    }

    /**
     * Get wake lock for voice recognition
     *
     * @return wake lock for voice recognition
     */
    @VisibleForTesting
    public PowerManager.WakeLock getVoiceRecognitionWakeLock() {
        return mVoiceRecognitionWakeLock;
    }

    /**
     * Get HeadsetPhoneState instance to interact with Telephony service
     *
     * @return HeadsetPhoneState interface to interact with Telephony service
     */
    @VisibleForTesting
    public HeadsetPhoneState getHeadsetPhoneState() {
        return mHeadsetPhoneState;
    }

    /**
     * Answer the current incoming call in Telecom service
     *
     * @param device the Bluetooth device used for answering this call
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public void answerCall(BluetoothDevice device, int profile) {
        if (device == null) {
            Log.w(TAG, "answerCall device is null");
            return;
        }
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        Log.w(TAG, "device: " + device + ", profile: " + profile);
        if (bluetoothInCallService != null) {
            if (profile == ApmConstIntf.AudioProfiles.HFP) {
                if((ApmConstIntf.getQtiLeAudioEnabled()) || (ApmConstIntf.getAospLeaEnabled())) {
                   Log.w(TAG, "LEA enabled: calling setActiveDeviceBlocking");
                   ActiveDeviceManagerServiceIntf mActiveDeviceManager =
                                         ActiveDeviceManagerServiceIntf.get();
                   mActiveDeviceManager.setActiveDeviceBlocking(device,
                                          ApmConstIntf.AudioFeatures.CALL_AUDIO);
                } else {
                  mHeadsetService.setActiveDevice(device);
                }
            }
            if (mAnswerCallDelay > 0) {
                Log.w(TAG, "Delay " + mAnswerCallDelay + " msec before calling answerCall");
                try {
                    Thread.sleep(mAnswerCallDelay);
                } catch (InterruptedException e) {
                    Log.e(TAG, "Thread sleep ", e);
                }
            }
            bluetoothInCallService.answerCall(profile);
        } else {
            Log.e(TAG, "Handsfree phone proxy null for answering call");
        }
    }

    /**
     * Hangup the current call, could either be Telecom call or virtual call
     *
     * @param device the Bluetooth device used for hanging up this call
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public void hangupCall(BluetoothDevice device, int profile) {
        if (device == null) {
            Log.w(TAG, "hangupCall device is null");
            return;
        }
        // Close the virtual call if active. Virtual call should be
        // terminated for CHUP callback event
        if (mHeadsetService.isVirtualCallStarted()) {
            if(ApmConstIntf.getQtiLeAudioEnabled()) {
                Log.d(TAG, "hangupCall remoteDisconnectVirtualVoiceCall Adv Audio enabled");
                CallAudioIntf mCallAudio = CallAudioIntf.get();
                if(mCallAudio != null) {
                    mCallAudio.remoteDisconnectVirtualVoiceCall(device);
                }
                return;
            }
            mHeadsetService.stopScoUsingVirtualVoiceCall();
        } else {
            BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
            if (bluetoothInCallService != null) {
                bluetoothInCallService.hangupCall(profile);
            } else {
                Log.e(TAG, "Handsfree phone proxy null for hanging up call");
            }
        }
    }

     /**
     * Terminate the call with given Index
     *
     * @param device the Bluetooth device used for terminating this call
     * @param index Index of the call to be terminated
     */
    @VisibleForTesting
    public void terminateCall(BluetoothDevice device, int index) {
        if (device == null) {
            Log.w(TAG, "terminateCall device is null");
            return;
        }
        // Close the virtual call if active. Virtual call should be
        // terminated for CHUP callback event
        if (mHeadsetService.isVirtualCallStarted()) {
            if(ApmConstIntf.getQtiLeAudioEnabled()) {
                Log.d(TAG, "terminateCall remoteDisconnectVirtualVoiceCall Adv Audio enabled");
                CallAudioIntf mCallAudio = CallAudioIntf.get();
                if(mCallAudio != null) {
                    mCallAudio.remoteDisconnectVirtualVoiceCall(device);
                }
                return;
            }
            mHeadsetService.stopScoUsingVirtualVoiceCall();
        } else {
            BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
            if (bluetoothInCallService != null) {
                bluetoothInCallService.terminateCall(index);
            } else {
                Log.e(TAG, "Handsfree phone proxy null for terminating the call: " + index);
            }
        }
    }

    /**
     * Instructs Telecom to play the specified DTMF tone for the current foreground call
     *
     * @param dtmf dtmf code
     * @param device the Bluetooth device that sent this code
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean sendDtmf(int dtmf, BluetoothDevice device) {
        if (device == null) {
            Log.w(TAG, "sendDtmf device is null");
            return false;
        }
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        if (bluetoothInCallService != null) {
            return bluetoothInCallService.sendDtmf(dtmf);
        } else {
            Log.e(TAG, "Handsfree phone proxy null for sending DTMF");
        }
        return false;
    }

    /**
     *
     * @param chld index of the call to hold
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean processChld(int chld, int profile) {
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        if (bluetoothInCallService != null) {
            return bluetoothInCallService.processChld(chld, profile);
        } else {
            Log.e(TAG, "processChld: bluetoothInCallService proxy null");
        }
        return false;
    }

    /**
     * Instructs Telecom to hold the call in given index
     *
     * @param index of the Call to be hold
     * @return true on success, false otherwise
     */
    @VisibleForTesting
    public boolean holdCall(int index) {
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        if (bluetoothInCallService != null) {
            return bluetoothInCallService.holdCall(index);
        } else {
            Log.e(TAG, "holdCall: bluetoothInCallService proxy null");
        }
        return false;
    }
    /**
     * Check for HD codec for voice call
     */
    @VisibleForTesting
    public boolean isHighDefCallInProgress() {
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        if (bluetoothInCallService != null) {
            return bluetoothInCallService.isHighDefCallInProgress();
        } else {
            Log.e(TAG, "Handsfree phone proxy null");
        }
        return false;
    }

    /**
     * Check for CS call
     */
    @VisibleForTesting
    public boolean isCsCallInProgress() {
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        if (bluetoothInCallService != null) {
            return bluetoothInCallService.isCsCallInProgress();
        } else {
            Log.e(TAG, "Handsfree phone proxy null");
        }
        return false;
    }

    /**
     * Get the the alphabetic name of current registered operator.
     *
     * @return null on error, empty string if not available
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public String getNetworkOperator() {
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        if (bluetoothInCallService == null) {
            Log.i(TAG, "mBluetoothInCallService is null,"+
                     " so fetching NetworkOperator Name from telephony directly");
            return mTelephonyManager.getNetworkOperatorName();
        }
        // Should never return null
        return bluetoothInCallService.getNetworkOperator();
    }

    /**
     * Get the phone number of this device
     *
     * @return null if unavailable
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public String getSubscriberNumber() {
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        if (bluetoothInCallService == null) {
            Log.i(TAG, "mBluetoothInCallService is null,"+
                     " so fetching Subscriber Number from telephony directly");
            String address = null;
            address = mTelephonyManager.getLine1Number();
            if (address == null) {
                address = "";
            }
            return address;
        }
        return bluetoothInCallService.getSubscriberNumber();
    }


    /**
     * Ask the Telecomm service to list current list of calls through CLCC response
     * {@link BluetoothHeadset#clccResponse(int, int, int, int, boolean, String, int)}
     *
     * @return
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public boolean listCurrentCalls(int profile) {
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        if (bluetoothInCallService == null) {
            Log.e(TAG, "listCurrentCalls() failed: mBluetoothInCallService is null");
            return false;
        }
        return bluetoothInCallService.listCurrentCalls(profile);
    }

    /**
     * Request Telecom service to send an update of the current call state to the headset service
     * through {@link BluetoothHeadset#phoneStateChanged(int, int, int, String, int)}
     */
    @VisibleForTesting
    @RequiresPermission(android.Manifest.permission.MODIFY_PHONE_STATE)
    public void queryPhoneState() {
        BluetoothInCallService bluetoothInCallService = getBluetoothInCallServiceInstance();
        if (bluetoothInCallService != null) {
            bluetoothInCallService.queryPhoneState();
        } else {
            Log.e(TAG, "Handsfree phone proxy null for query phone state");
        }
    }

    /**
     * Check if we are currently in a phone call
     *
     * @return True iff we are in a phone call
     */
    @VisibleForTesting
    public boolean isInCall() {
        return ((mHeadsetPhoneState.getNumActiveCall() > 0) || (mHeadsetPhoneState.getNumHeldCall()
                > 0) || (mHeadsetPhoneState.getCallState() != HeadsetHalConstants.CALL_STATE_IDLE));
    }

    /**
     * Check if there is currently an incoming call
     *
     * @return True iff there is an incoming call
     */
    @VisibleForTesting
    public boolean isRinging() {
        return mHeadsetPhoneState.getCallState() == HeadsetHalConstants.CALL_STATE_INCOMING;
    }

    /**
     * Check if call status is idle
     *
     * @return true if call state is neither ringing nor in call
     */
    @VisibleForTesting
    public boolean isCallIdle() {
        return !isInCall() && !isRinging();
    }

    /**
     * Activate voice recognition on Android system
     *
     * @return true if activation succeeds, caller should wait for
     * {@link BluetoothHeadset#startVoiceRecognition(BluetoothDevice)} callback that will then
     * trigger {@link HeadsetService#startVoiceRecognition(BluetoothDevice)}, false if failed to
     * activate
     */
    @VisibleForTesting
    public boolean activateVoiceRecognition() {
        Intent intent = new Intent(Intent.ACTION_VOICE_COMMAND);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            mHeadsetService.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "activateVoiceRecognition, failed due to activity not found for " + intent);
            return false;
        }
        return true;
    }

    /**
     * Deactivate voice recognition on Android system
     *
     * @return true if activation succeeds, caller should wait for
     * {@link BluetoothHeadset#stopVoiceRecognition(BluetoothDevice)} callback that will then
     * trigger {@link HeadsetService#stopVoiceRecognition(BluetoothDevice)}, false if failed to
     * activate
     */
    @VisibleForTesting
    public boolean deactivateVoiceRecognition() {
        // TODO: need a method to deactivate voice recognition on Android
        return true;
    }

}
