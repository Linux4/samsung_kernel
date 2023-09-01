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

package com.android.bluetooth.hfp;

import android.annotation.RequiresPermission;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Looper;
import android.telephony.PhoneStateListener;
import android.telephony.ServiceState;
import android.telephony.SignalStrength;
import android.telephony.SubscriptionManager;
import android.telephony.SubscriptionManager.OnSubscriptionsChangedListener;
import android.telephony.TelephonyManager;
import android.telephony.SubscriptionInfo;
import com.android.bluetooth.apm.ApmConstIntf;
import com.android.bluetooth.apm.CallAudioIntf;
import com.android.bluetooth.apm.CallControlIntf;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.telephony.IccCardConstants;
import com.android.internal.telephony.TelephonyIntents;
import com.android.internal.telephony.PhoneConstants;

import java.util.HashMap;
import java.util.Objects;

/**
 * Class that manages Telephony states
 *
 * Note:
 * The methods in this class are not thread safe, don't call them from
 * multiple threads. Call them from the HeadsetPhoneStateMachine message
 * handler only.
 */
public class HeadsetPhoneState {
    private static final String TAG = "HeadsetPhoneState";

    private final HeadsetService mHeadsetService;
    private final TelephonyManager mTelephonyManager;
    private final SubscriptionManager mSubscriptionManager;

    private ServiceState mServiceState;

    // HFP 1.6 CIND service value
    private int mCindService = HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE;
    // Check this before sending out service state to the device -- if the SIM isn't fully
    // loaded, don't expose that the network is available.
    private boolean mIsSimStateLoaded;
    // Number of active (foreground) calls
    private int mNumActive;
    // Current Call Setup State
    private int mCallState = HeadsetHalConstants.CALL_STATE_IDLE;
    // Number of held (background) calls
    private int mNumHeld;
    // HFP 1.6 CIND signal value
    private int mCindSignal;
    // HFP 1.6 CIND roam value
    private int mCindRoam = HeadsetHalConstants.SERVICE_TYPE_HOME;
    // HFP 1.6 CIND battchg value
    private int mCindBatteryCharge;
    // Current Call Number
    private String mCindNumber;
    //Current Phone Number Type
    private int mType = 0;
    // if its a CS call
    private boolean mIsCsCall = true;
    // SIM is absent
    private int SIM_ABSENT = 0;
    // SIM is present
    private int SIM_PRESENT = 1;
    // Array to keep the SIM status
    private int[] mSimStatus;

    private final HashMap<BluetoothDevice, Integer> mDeviceEventMap = new HashMap<>();
    private PhoneStateListener mPhoneStateListener;
    private final OnSubscriptionsChangedListener mOnSubscriptionsChangedListener;
    private final String CC_DUMMY_DEVICE_ADDR = " CC:CC:CC:CC:CC:CC";

    HeadsetPhoneState(HeadsetService headsetService) {
        Objects.requireNonNull(headsetService, "headsetService is null");
        mHeadsetService = headsetService;
        mTelephonyManager =
                (TelephonyManager) mHeadsetService.getSystemService(Context.TELEPHONY_SERVICE);
        Objects.requireNonNull(mTelephonyManager, "TELEPHONY_SERVICE is null");
        // Register for SubscriptionInfo list changes which is guaranteed to invoke
        // onSubscriptionInfoChanged and which in turns calls loadInBackgroud.
        mSubscriptionManager = (SubscriptionManager)  mHeadsetService.
                 getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        Objects.requireNonNull(mSubscriptionManager, "TELEPHONY_SUBSCRIPTION_SERVICE is null");
        // Initialize subscription on the handler thread
        mOnSubscriptionsChangedListener = new HeadsetPhoneStateOnSubscriptionChangedListener(
                headsetService.getStateMachinesThreadLooper());
        mSubscriptionManager.addOnSubscriptionsChangedListener(mOnSubscriptionsChangedListener);
        IntentFilter simStateChangedFilter =
                        new IntentFilter(TelephonyIntents.ACTION_SIM_STATE_CHANGED);
        mSimStatus = new int [mTelephonyManager.getPhoneCount()];
        //Record the SIM states upon BT reset
        try {
            for (int i = 0; i < mSimStatus.length; i++) {
                if (mTelephonyManager.getSimState(i) ==
                        TelephonyManager.SIM_STATE_READY) {
                    Log.d(TAG, "The sim in i: " + i + " is present");
                    mSimStatus[i] = SIM_PRESENT;
                } else {
                    Log.d(TAG, "The sim in i: " + i + " is absent");
                    mSimStatus[i] = SIM_ABSENT;
                }
            }
            mHeadsetService.registerReceiver(mPhoneStateChangeReceiver, simStateChangedFilter);
        } catch (Exception e) {
            Log.w(TAG, "Unable to register phone state change receiver and unable to get" +
                       " sim states", e);
        }
    }

    /**
     * Cleanup this instance. Instance can no longer be used after calling this method.
     */
    public void cleanup() {
        synchronized (mDeviceEventMap) {
            mDeviceEventMap.clear();
            stopListenForPhoneState();
        }
        mSubscriptionManager.removeOnSubscriptionsChangedListener(mOnSubscriptionsChangedListener);
        try {
             mHeadsetService.unregisterReceiver(mPhoneStateChangeReceiver);
        } catch (Exception e) {
            Log.w(TAG, "Unable to unregister phone state change receiver", e);
        }
    }

    @Override
    public String toString() {
        return "HeadsetPhoneState [mTelephonyServiceAvailability=" + mCindService + ", mNumActive="
                + mNumActive + ", mCallState=" + mCallState + ", mNumHeld=" + mNumHeld
                + ", mSignal=" + mCindSignal + ", mRoam=" + mCindRoam + ", mBatteryCharge="
                + mCindBatteryCharge + ", TelephonyEvents=" + getTelephonyEventsToListen() + "]";
    }

    private int getTelephonyEventsToListen() {
        synchronized (mDeviceEventMap) {
            return mDeviceEventMap.values()
                    .stream()
                    .reduce(PhoneStateListener.LISTEN_NONE, (a, b) -> a | b);
        }
    }

    /**
     * Start or stop listening for phone state change
     *
     * @param device remote device that subscribes to this phone state update
     * @param events events in {@link PhoneStateListener} to listen to
     */
    @VisibleForTesting
    public void listenForPhoneState(BluetoothDevice device, int events) {
        synchronized (mDeviceEventMap) {
            int prevEvents = getTelephonyEventsToListen();
            if (events == PhoneStateListener.LISTEN_NONE) {
                mDeviceEventMap.remove(device);
            } else {
                mDeviceEventMap.put(device, events);
            }
            int updatedEvents = getTelephonyEventsToListen();
            if (prevEvents != updatedEvents) {
                stopListenForPhoneState();
                startListenForPhoneState();
            }
            if (events != PhoneStateListener.LISTEN_NONE &&
                device.getAddress().equals(CC_DUMMY_DEVICE_ADDR)) {
                Log.d(TAG, "pushing initial events to CC");
                //push these events on registeration
                if ((ApmConstIntf.getQtiLeAudioEnabled()) || (ApmConstIntf.getAospLeaEnabled())) {
                    CallControlIntf mCallControl = CallControlIntf.get();
                    int networkType = mTelephonyManager.getNetworkType();
                    Log.d(TAG, "Adv Audio enabled: updateBearerTech:" + networkType);
                    mCallControl.updateBearerTechnology(networkType);
                    SubscriptionInfo subInfo = mSubscriptionManager.getDefaultVoiceSubscriptionInfo();
                    if (subInfo != null) {
                        Log.d(TAG, "Adv Audio enabled: updateBearerName " + subInfo.getDisplayName().toString());
                        mCallControl.updateBearerName(subInfo.getDisplayName().toString());
                    }
                }
            }
        }
    }

    private void startListenForPhoneState() {
        if (mPhoneStateListener != null) {
            Log.w(TAG, "startListenForPhoneState, already listening");
            return;
        }
        int events = getTelephonyEventsToListen();
        if (events == PhoneStateListener.LISTEN_NONE) {
            Log.w(TAG, "startListenForPhoneState, no event to listen");
            return;
        }
        int subId = SubscriptionManager.getDefaultSubscriptionId();
        if (!SubscriptionManager.isValidSubscriptionId(subId)) {
            // Will retry listening for phone state in onSubscriptionsChanged() callback
            Log.w(TAG, "startListenForPhoneState, invalid subscription ID " + subId);
            return;
        }
        Log.i(TAG, "startListenForPhoneState(), subId=" + subId + ", enabled_events=" + events);
        if (mTelephonyManager == null) {
            Log.e(TAG, "mTelephonyManager is null, "
                 + "cannot start listening for phone state changes");
        } else {
            mPhoneStateListener = new HeadsetPhoneStateListener(
                    mHeadsetService.getStateMachinesThreadLooper());
            try {
                mTelephonyManager.listen(mPhoneStateListener, events);
            } catch (Exception e) {
                Log.w(TAG, "Exception while registering for signal strength notifications", e);
            }
        }
    }

    private void stopListenForPhoneState() {
        if (mPhoneStateListener == null) {
            Log.i(TAG, "stopListenForPhoneState(), no listener indicates nothing is listening");
            return;
        }
        Log.i(TAG, "stopListenForPhoneState(), stopping listener, enabled_events="
                + getTelephonyEventsToListen());
        if (mTelephonyManager == null) {
            Log.e(TAG, "mTelephonyManager is null, "
                + "cannot send request to stop listening or update radio to normal state");
        } else {
            try {
                mTelephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
            } catch (Exception e) {
                Log.w(TAG, "exception while registering for signal strength notifications", e);
            }
        }
        mPhoneStateListener = null;
    }

    public boolean isValidPhoneId(int phoneId) {
        return phoneId >= 0 && phoneId < mTelephonyManager.getPhoneCount();
    }

    private final BroadcastReceiver mPhoneStateChangeReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.d(TAG, "onReceive: " + intent.getAction());
            final String stateExtra = intent.getStringExtra(IccCardConstants.INTENT_KEY_ICC_STATE);
            if (TelephonyIntents.ACTION_SIM_STATE_CHANGED.equals(intent.getAction())) {
                // This is a sticky broadcast, so if it's already been loaded,
                // this'll execute immediately.

                // TODO (b/122116049) during platform update, a case emerged for an incoming
                // slotId value of -1; it would likely be beneficial to code defensively
                // because of this case's possibility from the caller, but the current root
                // cause is as of yet unidentified
                if (IccCardConstants.INTENT_VALUE_ICC_LOADED.equals(stateExtra)) {
                    final int phoneId = intent.getIntExtra(PhoneConstants.PHONE_KEY,
                                       SubscriptionManager.getDefaultVoicePhoneId());
                    if (isValidPhoneId(phoneId) != true) {
                        Log.d(TAG, "Received invalid phoneId " + phoneId +" for SIM loaded, no action");
                        return;
                    }
                    Log.d(TAG, "SIM loaded, making mIsSimStateLoaded to true for phoneId = "
                               + phoneId);
                    mSimStatus[phoneId] = SIM_PRESENT;
                    mIsSimStateLoaded = true;
                    sendDeviceStateChanged();
                } else if (IccCardConstants.INTENT_VALUE_ICC_ABSENT.equals(stateExtra)
                           || IccCardConstants.INTENT_VALUE_ICC_UNKNOWN.equals(stateExtra)
                           || IccCardConstants.INTENT_VALUE_ICC_CARD_IO_ERROR.equals(stateExtra)
                           || IccCardConstants.INTENT_VALUE_ICC_NOT_READY.equals(stateExtra)) {
                    final int phoneId = intent.getIntExtra(PhoneConstants.PHONE_KEY,
                                       SubscriptionManager.getDefaultVoicePhoneId());
                    if (isValidPhoneId(phoneId) != true) {
                        Log.d(TAG, "Received invalid phoneId " + phoneId +" for SIM unloaded, no action");
                        return;
                    }
                    Log.d(TAG, "SIM unloaded, making mIsSimStateLoaded to false for phoneId = "
                               + phoneId);
                    mSimStatus[phoneId] = SIM_ABSENT;
                    mIsSimStateLoaded = false;
                    for (int i = 0; i < mSimStatus.length; i++) {
                        if (mSimStatus[i] == SIM_PRESENT) {
                            Log.d(TAG, "SIM is loaded in either of the slots, making" +
                                  " mIsSimStateLoaded to true");
                              mIsSimStateLoaded = true;
                              break;
                         }
                    }
                    sendDeviceStateChanged();
                }
            }
        }
    };

    boolean getIsSimCardLoaded () {
      return mIsSimStateLoaded;
    }
    int getCindService() {
        return mCindService;
    }

    int getNumActiveCall() {
        return mNumActive;
    }

    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void setNumActiveCall(int numActive) {
        mNumActive = numActive;
    }

    boolean getIsCsCall() {
        Log.d(TAG, "In getIsCsCall, mIsCsCall: " + mIsCsCall);
        return mIsCsCall;
    }

    void setIsCsCall(boolean isCsCall) {
        Log.d(TAG, "In setIsCsCall, mIsCsCall: " + isCsCall);
        mIsCsCall = isCsCall;
    }

    int getCallState() {
        return mCallState;
    }

    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void setCallState(int callState) {
        mCallState = callState;
    }

    int getNumHeldCall() {
        return mNumHeld;
    }

    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void setNumHeldCall(int numHeldCall) {
        mNumHeld = numHeldCall;
    }

    int getCindSignal() {
        return mCindSignal;
    }

    void setNumber(String mNumberCall ) {
        mCindNumber = mNumberCall;
    }

    String getNumber() {
        return mCindNumber;
    }

    void setType(int mTypeCall) {
        mType = mTypeCall;
    }

    int getType() {
        return mType;
    }

    int getCindRoam() {
        return mCindRoam;
    }

    /**
     * Set battery level value used for +CIND result
     *
     * @param batteryLevel battery level value
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public void setCindBatteryCharge(int batteryLevel) {
        if (mCindBatteryCharge != batteryLevel) {
            mCindBatteryCharge = batteryLevel;
            sendDeviceStateChanged();
        }
    }

    int getCindBatteryCharge() {
        return mCindBatteryCharge;
    }

    boolean isInCall() {
        return (mNumActive >= 1);
    }

    private void sendDeviceStateChanged() {
        int service =
                mIsSimStateLoaded ? mCindService : HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE;
        // When out of service, send signal strength as 0. Some devices don't
        // use the service indicator, but only the signal indicator
        int signal = service == HeadsetHalConstants.NETWORK_STATE_AVAILABLE ? mCindSignal : 0;

        Log.d(TAG, "sendDeviceStateChanged. mService=" + service + " mIsSimStateLoaded="
                + mIsSimStateLoaded + " mSignal=" + signal + " mRoam=" + mCindRoam
                + " mBatteryCharge=" + mCindBatteryCharge);
        mHeadsetService.onDeviceStateChanged(
                new HeadsetDeviceState(service, mCindRoam, signal, mCindBatteryCharge));
    }

    @SuppressLint("AndroidFrameworkRequiresPermission")
    private class HeadsetPhoneStateOnSubscriptionChangedListener
            extends OnSubscriptionsChangedListener {
        HeadsetPhoneStateOnSubscriptionChangedListener(Looper looper) {
            super(looper);
        }

        @Override
        public void onSubscriptionsChanged() {
            synchronized (mDeviceEventMap) {
                int simState = mTelephonyManager.getSimState();
                if (simState != TelephonyManager.SIM_STATE_READY) {
                 mCindSignal = 0;
                 mCindService = HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE;
                 sendDeviceStateChanged();
                }
                stopListenForPhoneState();
                startListenForPhoneState();
                if ((ApmConstIntf.getQtiLeAudioEnabled()) || (ApmConstIntf.getAospLeaEnabled())) {
                   int networkType = mTelephonyManager.getNetworkType();
                   CallControlIntf mCallControl = CallControlIntf.get();
                   Log.d(TAG, "onSubscriptionsChanged: Adv Audio enabled: updateBearerTech:" + networkType);
                   mCallControl.updateBearerTechnology(networkType);
                   SubscriptionInfo subInfo = mSubscriptionManager.getDefaultVoiceSubscriptionInfo();
                   if (subInfo != null) {
                       Log.d(TAG, "onSubscriptionsChanged: Adv Audio enabled: updateBearerName" + subInfo.getDisplayName().toString());
                       mCallControl.updateBearerName(subInfo.getDisplayName().toString());
                   }
                }
            }
        }
    }

    @SuppressLint("AndroidFrameworkRequiresPermission")
    private class HeadsetPhoneStateListener extends PhoneStateListener {
        HeadsetPhoneStateListener(Looper looper) {
            super(looper);
        }

        @Override
        public synchronized void onServiceStateChanged(ServiceState serviceState) {
            Log.d(TAG, "Enter onServiceStateChanged");
            mServiceState = serviceState;
            int prevService = mCindService;
            int cindService = (serviceState.getState() == ServiceState.STATE_IN_SERVICE)
                    ? HeadsetHalConstants.NETWORK_STATE_AVAILABLE
                    : HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE;
            int newRoam = serviceState.getRoaming() ? HeadsetHalConstants.SERVICE_TYPE_ROAMING
                    : HeadsetHalConstants.SERVICE_TYPE_HOME;

            if (cindService == mCindService && newRoam == mCindRoam) {
                // De-bounce the state change
                return;
            }
            mCindService = cindService;
            mCindRoam = newRoam;

            // If this is due to a SIM insertion, we want to defer sending device state changed
            // until all the SIM config is loaded.
            if (cindService == HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE) {
                sendDeviceStateChanged();
            }

            //Update the signal strength when the service state changes
            if (prevService == HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE &&
                cindService == HeadsetHalConstants.NETWORK_STATE_AVAILABLE &&
                mCindSignal == 0) {
                Log.d(TAG, "Service is available and signal strength was zero, updating the "+
                           "current signal strength");
                mCindSignal = mTelephonyManager.getSignalStrength().getLevel() + 1;
                // +CIND "signal" indicator is always between 0 to 5
                mCindSignal = Integer.max(Integer.min(mCindSignal, 5), 0);
                sendDeviceStateChanged();
            }
            Log.d(TAG, "Exit onServiceStateChanged");
        }

        @Override
        public void onSignalStrengthsChanged(SignalStrength signalStrength) {
            int prevSignal = mCindSignal;
            if (mCindService == HeadsetHalConstants.NETWORK_STATE_NOT_AVAILABLE) {
                Log.d(TAG, "Service is not available, signal strength is set to zero");
                mCindSignal = 0;
            } else {
                mCindSignal = signalStrength.getLevel() + 1;
            }
            // +CIND "signal" indicator is always between 0 to 5
            mCindSignal = Integer.max(Integer.min(mCindSignal, 5), 0);
            // This results in a lot of duplicate messages, hence this check
            if (prevSignal != mCindSignal) {
                Log.d(TAG, "Updating the signal strength change to the apps");
                sendDeviceStateChanged();
            }
        }
    }
}
