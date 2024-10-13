/*
 * Copyright (c) 2017 The Android Open Source Project
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
 * Defines the native inteface that is used by state machine/service to either or receive messages
 * from the native stack. This file is registered for the native methods in corresponding CPP file.
 */
package com.android.bluetooth.hfpclient;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;

/**
 * Defines native calls that are used by state machine/service to either send or receive
 * messages to/from the native stack. This file is registered for the native methods in
 * corresponding CPP file.
 */
public class NativeInterface {
    private static final String TAG = "NativeInterface";
    private static final boolean DBG = false;

    static {
        classInitNative();
    }

    private NativeInterface() {}
    private static NativeInterface sInterface;
    private static final Object INSTANCE_LOCK = new Object();

    /**
     * This class is a singleton because native library should only be loaded once
     *
     * @return default instance
     */
    public static NativeInterface getInstance() {
        synchronized (INSTANCE_LOCK) {
            if (sInterface == null) {
                sInterface = new NativeInterface();
            }
        }
        return sInterface;
    }

    // Native wrappers to help unit testing
    /**
     * Initialize native stack
     */
    @VisibleForTesting
    public void initialize() {
        initializeNative();
    }

    /**
     * Close and clean up native stack
     */
    @VisibleForTesting
    public void cleanup() {
        cleanupNative();
    }

    /**
     * Connect to the specified paired device
     *
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean connect(byte[] address) {
        return connectNative(address);
    }

    /**
     * Disconnect from the specified paired device
     *
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean disconnect(byte[] address) {
        return disconnectNative(address);
    }

    /**
     * Initiate audio connection to the specified paired device
     *
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean connectAudio(byte[] address) {
        return connectAudioNative(address);
    }

    /**
     * Close audio connection from the specified paired device
     *
     * @param address target device's address
     * @return True on success, False on failure
     */
    public boolean disconnectAudio(byte[] address) {
        return disconnectAudioNative(address);
    }

    /**
     * Initiate voice recognition to the specified paired device
     *
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean startVoiceRecognition(byte[] address) {
        return startVoiceRecognitionNative(address);
    }

    /**
     * Close voice recognition to the specified paired device
     *
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean stopVoiceRecognition(byte[] address) {
        return stopVoiceRecognitionNative(address);
    }

    /**
     * Set volume to the specified paired device
     *
     * @param volumeType type of volume as in
     *                  HeadsetClientHalConstants.VOLUME_TYPE_xxxx
     * @param volume  volume level
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean setVolume(byte[] address, int volumeType, int volume) {
        return setVolumeNative(address, volumeType, volume);
    }

    /**
     * dial number from the specified paired device
     *
     * @param number  phone number to be dialed
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean dial(byte[] address, String number) {
        return dialNative(address, number);
    }

    /**
     * Memory dialing from the specified paired device
     *
     * @param location  memory location
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean dialMemory(byte[] address, int location) {
        return dialMemoryNative(address, location);
    }

    /**
     * Apply action to call
     *
     * @action action (e.g. hold, terminate etc)
     * @index call index
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean handleCallAction(byte[] address, int action, int index) {
        return handleCallActionNative(address, action, index);
    }

    /**
     * Query current call status from the specified paired device
     *
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean queryCurrentCalls(byte[] address) {
        return queryCurrentCallsNative(address);
    }

    /**
     * Query operator name from the specified paired device
     *
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean queryCurrentOperatorName(byte[] address) {
        return queryCurrentOperatorNameNative(address);
    }

    /**
     * Retrieve subscriber number from the specified paired device
     *
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public  boolean retrieveSubscriberInfo(byte[] address) {
        return retrieveSubscriberInfoNative(address);
    }

    /**
     * Transmit DTMF code
     *
     * @param code DTMF code
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean sendDtmf(byte[] address, byte code) {
        return sendDtmfNative(address, code);
    }

    /**
     * Request last voice tag
     *
     * @param address target device's address
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean requestLastVoiceTagNumber(byte[] address) {
        return requestLastVoiceTagNumberNative(address);
    }

    /**
     * Send an AT command
     *
     * @param atCmd command code
     * @param val1 command specific argurment1
     * @param val2 command specific argurment2
     * @param arg other command specific argurments
     * @return True on success, False on failure
     */
    @VisibleForTesting
    public boolean sendATCmd(byte[] address, int atCmd, int val1, int val2, String arg) {
        return sendATCmdNative(address, atCmd, val1, val2, arg);
    }

    // Native methods that call into the JNI interface
    private static native void classInitNative();

    private native void initializeNative();

    private native void cleanupNative();

    private static native boolean connectNative(byte[] address);

    private static native boolean disconnectNative(byte[] address);

    private static native boolean connectAudioNative(byte[] address);

    private static native boolean disconnectAudioNative(byte[] address);

    private static native boolean startVoiceRecognitionNative(byte[] address);

    private static native boolean stopVoiceRecognitionNative(byte[] address);

    private static native boolean setVolumeNative(byte[] address, int volumeType, int volume);

    private static native boolean dialNative(byte[] address, String number);

    private static native boolean dialMemoryNative(byte[] address, int location);

    private static native boolean handleCallActionNative(byte[] address, int action, int index);

    private static native boolean queryCurrentCallsNative(byte[] address);

    private static native boolean queryCurrentOperatorNameNative(byte[] address);

    private static native boolean retrieveSubscriberInfoNative(byte[] address);

    private static native boolean sendDtmfNative(byte[] address, byte code);

    private static native boolean requestLastVoiceTagNumberNative(byte[] address);

    private static native boolean sendATCmdNative(byte[] address, int atCmd, int val1, int val2,
            String arg);

    private BluetoothDevice getDevice(byte[] address) {
        return BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
    }

    // Callbacks from the native back into the java framework. All callbacks are routed via the
    // Service which will disambiguate which state machine the message should be routed through.
    private void onConnectionStateChanged(int state, int peerFeat, int chldFeat, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
        event.valueInt = state;
        event.valueInt2 = peerFeat;
        event.valueInt3 = chldFeat;
        event.device = getDevice(address);
        // BluetoothAdapter.getDefaultAdapter().getRemoteDevice(Utils.getAddressStringFromByte
        // (address));
        if (DBG) {
            Log.d(TAG, "Device addr " + event.device.getAddress() + " State " + state);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "Ignoring message because service not available: " + event);
        }
    }

    private void onAudioStateChanged(int state, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_AUDIO_STATE_CHANGED);
        event.valueInt = state;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onAudioStateChanged: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onAudioStateChanged: Ignoring message because service not available: "
                    + event);
        }
    }

    private void onVrStateChanged(int state, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_VR_STATE_CHANGED);
        event.valueInt = state;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onVrStateChanged: address " + address + " event " + event);
        }

        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG,
                    "onVrStateChanged: Ignoring message because service not available: " + event);
        }
    }

    private void onNetworkState(int state, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_NETWORK_STATE);
        event.valueInt = state;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onNetworkStateChanged: address " + address + " event " + event);
        }

        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG,
                    "onNetworkStateChanged: Ignoring message because service not available: "
                            + event);
        }
    }

    private void onNetworkRoaming(int state, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_ROAMING_STATE);
        event.valueInt = state;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onNetworkRoaming: incoming: " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG,
                    "onNetworkRoaming: Ignoring message because service not available: " + event);
        }
    }

    private void onNetworkSignal(int signal, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_NETWORK_SIGNAL);
        event.valueInt = signal;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onNetworkSignal: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onNetworkSignal: Ignoring message because service not available: " + event);
        }
    }

    private void onBatteryLevel(int level, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_BATTERY_LEVEL);
        event.valueInt = level;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onBatteryLevel: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onBatteryLevel: Ignoring message because service not available: " + event);
        }
    }

    private void onCurrentOperator(String name, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_OPERATOR_NAME);
        event.valueString = name;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onCurrentOperator: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG,
                    "onCurrentOperator: Ignoring message because service not available: " + event);
        }
    }

    private void onCall(int call, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_CALL);
        event.valueInt = call;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onCall: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onCall: Ignoring message because service not available: " + event);
        }
    }

    /**
     * CIEV (Call indicators) notifying if call(s) are getting set up.
     *
     * Values include:
     * 0 - No current call is in setup
     * 1 - Incoming call process ongoing
     * 2 - Outgoing call process ongoing
     * 3 - Remote party being alerted for outgoing call
     */
    private void onCallSetup(int callsetup, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_CALLSETUP);
        event.valueInt = callsetup;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onCallSetup: addr " + address + " device" + event.device);
            Log.d(TAG, "onCallSetup: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onCallSetup: Ignoring message because service not available: " + event);
        }
    }

    /**
     * CIEV (Call indicators) notifying call held states.
     *
     * Values include:
     * 0 - No calls held
     * 1 - Call is placed on hold or active/held calls wapped (The AG has both an ACTIVE and HELD
     * call)
     * 2 - Call on hold, no active call
     */
    private void onCallHeld(int callheld, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_CALLHELD);
        event.valueInt = callheld;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onCallHeld: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onCallHeld: Ignoring message because service not available: " + event);
        }
    }

    private void onRespAndHold(int respAndHold, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_RESP_AND_HOLD);
        event.valueInt = respAndHold;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onRespAndHold: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onRespAndHold: Ignoring message because service not available: " + event);
        }
    }

    private void onClip(String number, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_CLIP);
        event.valueString = number;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onClip: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onClip: Ignoring message because service not available: " + event);
        }
    }

    private void onCallWaiting(String number, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_CALL_WAITING);
        event.valueString = number;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onCallWaiting: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onCallWaiting: Ignoring message because service not available: " + event);
        }
    }

    private void onCurrentCalls(int index, int dir, int state, int mparty, String number,
            byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_CURRENT_CALLS);
        event.valueInt = index;
        event.valueInt2 = dir;
        event.valueInt3 = state;
        event.valueInt4 = mparty;
        event.valueString = number;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onCurrentCalls: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onCurrentCalls: Ignoring message because service not available: " + event);
        }
    }

    private void onVolumeChange(int type, int volume, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_VOLUME_CHANGED);
        event.valueInt = type;
        event.valueInt2 = volume;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onVolumeChange: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onVolumeChange: Ignoring message because service not available: " + event);
        }
    }

    private void onCmdResult(int type, int cme, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_CMD_RESULT);
        event.valueInt = type;
        event.valueInt2 = cme;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onCmdResult: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG, "onCmdResult: Ignoring message because service not available: " + event);
        }
    }

    private void onSubscriberInfo(String number, int type, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_SUBSCRIBER_INFO);
        event.valueInt = type;
        event.valueString = number;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onSubscriberInfo: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG,
                    "onSubscriberInfo: Ignoring message because service not available: " + event);
        }
    }

    private void onInBandRing(int inBand, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_IN_BAND_RINGTONE);
        event.valueInt = inBand;
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onInBandRing: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG,
                    "onInBandRing: Ignoring message because service not available: " + event);
        }
    }

    private void onLastVoiceTagNumber(String number, byte[] address) {
        Log.w(TAG, "onLastVoiceTagNumber not supported");
    }

    private void onRingIndication(byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_RING_INDICATION);
        event.device = getDevice(address);
        if (DBG) {
            Log.d(TAG, "onRingIndication: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG,
                    "onRingIndication: Ignoring message because service not available: " + event);
        }
    }

    private void onUnknownEvent(String eventString, byte[] address) {
        StackEvent event = new StackEvent(StackEvent.EVENT_TYPE_UNKNOWN_EVENT);
        event.device = getDevice(address);
        event.valueString = eventString;
        if (DBG) {
            Log.d(TAG, "onUnknownEvent: address " + address + " event " + event);
        }
        HeadsetClientService service = HeadsetClientService.getHeadsetClientService();
        if (service != null) {
            service.messageFromNative(event);
        } else {
            Log.w(TAG,
                    "onUnknowEvent: Ignoring message because service not available: " + event);
        }
    }

}
