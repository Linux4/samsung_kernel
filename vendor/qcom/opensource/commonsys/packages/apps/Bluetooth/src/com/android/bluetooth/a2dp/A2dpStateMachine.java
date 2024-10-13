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

/**
 * Bluetooth A2DP StateMachine. There is one instance per remote device.
 *  - "Disconnected" and "Connected" are steady states.
 *  - "Connecting" and "Disconnecting" are transient states until the
 *     connection / disconnection is completed.
 *
 *
 *                        (Disconnected)
 *                           |       ^
 *                   CONNECT |       | DISCONNECTED
 *                           V       |
 *                 (Connecting)<--->(Disconnecting)
 *                           |       ^
 *                 CONNECTED |       | DISCONNECT
 *                           V       |
 *                          (Connected)
 * NOTES:
 *  - If state machine is in "Connecting" state and the remote device sends
 *    DISCONNECT request, the state machine transitions to "Disconnecting" state.
 *  - Similarly, if the state machine is in "Disconnecting" state and the remote device
 *    sends CONNECT request, the state machine transitions to "Connecting" state.
 *
 *                    DISCONNECT
 *    (Connecting) ---------------> (Disconnecting)
 *                 <---------------
 *                      CONNECT
 *
 */

package com.android.bluetooth.a2dp;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothCodecConfig;
import android.bluetooth.BluetoothCodecStatus;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.content.Intent;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import com.android.bluetooth.BluetoothStatsLog;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.apm.ApmConstIntf;
import com.android.bluetooth.apm.DeviceProfileMapIntf;
import com.android.bluetooth.apm.MediaAudioIntf;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.List;
import java.util.Scanner;
import android.os.SystemProperties;
import com.android.bluetooth.btservice.AdapterService;

final class A2dpStateMachine extends StateMachine {
    private static final boolean DBG = true;
    private static final String TAG = "A2dpStateMachine";

    static final int CONNECT = 1;
    static final int DISCONNECT = 2;
    @VisibleForTesting
    static final int STACK_EVENT = 101;
    private static final int CONNECT_TIMEOUT = 201;

    // NOTE: the value is not "final" - it is modified in the unit tests
    @VisibleForTesting
    static int sConnectTimeoutMs = 30000;        // 30s

    private Disconnected mDisconnected;
    private Connecting mConnecting;
    private Disconnecting mDisconnecting;
    private Connected mConnected;
    private int mConnectionState = BluetoothProfile.STATE_DISCONNECTED;
    private int mLastConnectionState = -1;

    private A2dpService mA2dpService;
    private A2dpNativeInterface mA2dpNativeInterface;
    @VisibleForTesting
    boolean mA2dpOffloadEnabled = false;
    private final BluetoothDevice mDevice;
    private boolean mIsPlaying = false;
    private BluetoothCodecStatus mCodecStatus;
    private boolean mCodecConfigUpdated = false;

    A2dpStateMachine(BluetoothDevice device, A2dpService a2dpService,
                     A2dpNativeInterface a2dpNativeInterface, Looper looper) {
        super(TAG, looper);
        setDbg(DBG);
        mDevice = device;
        mCodecConfigUpdated = false;
        mA2dpService = a2dpService;
        mA2dpNativeInterface = a2dpNativeInterface;

        mDisconnected = new Disconnected();
        mConnecting = new Connecting();
        mDisconnecting = new Disconnecting();
        mConnected = new Connected();

        addState(mDisconnected);
        addState(mConnecting);
        addState(mDisconnecting);
        addState(mConnected);
        mA2dpOffloadEnabled = mA2dpService.mA2dpOffloadEnabled;

        setInitialState(mDisconnected);
    }

    static A2dpStateMachine make(BluetoothDevice device, A2dpService a2dpService,
                                 A2dpNativeInterface a2dpNativeInterface, Looper looper) {
        Log.i(TAG, "make for device " + device);
        A2dpStateMachine a2dpSm = new A2dpStateMachine(device, a2dpService, a2dpNativeInterface,
                                                       looper);
        a2dpSm.start();
        return a2dpSm;
    }

    public void doQuit() {
        log("doQuit for device " + mDevice);
        if (mIsPlaying) {
            // Stop if auido is still playing
            log("doQuit: stopped playing " + mDevice);
            mIsPlaying = false;
            mA2dpService.setAvrcpAudioState(BluetoothA2dp.STATE_NOT_PLAYING, mDevice);
            broadcastAudioState(BluetoothA2dp.STATE_NOT_PLAYING,
                                BluetoothA2dp.STATE_PLAYING);
        }
        quitNow();
    }

    public void cleanup() {
        log("cleanup for device " + mDevice);
    }

    @VisibleForTesting
    class Disconnected extends State {
        @Override
        public void enter() {
            Message currentMessage = getCurrentMessage();
            Log.i(TAG, "Enter Disconnected(" + mDevice + "): " + (currentMessage == null ? "null"
                    : messageWhatToString(currentMessage.what)));
            mCodecConfigUpdated = false;
            synchronized (this) {
                mConnectionState = BluetoothProfile.STATE_DISCONNECTED;
            }
            removeDeferredMessages(DISCONNECT);

            if (mLastConnectionState != -1) {
                // Don't broadcast during startup
                if (mIsPlaying) {
                    Log.i(TAG, "Disconnected: stopped playing: " + mDevice);
                    mIsPlaying = false;
                    mA2dpService.setAvrcpAudioState(BluetoothA2dp.STATE_NOT_PLAYING, mDevice);
                    broadcastAudioState(BluetoothA2dp.STATE_NOT_PLAYING,
                                        BluetoothA2dp.STATE_PLAYING);
                }
                broadcastConnectionState(mConnectionState, mLastConnectionState);
                AdapterService adapterService = AdapterService.getAdapterService();
                if (adapterService.isVendorIntfEnabled() &&
                     adapterService.isTwsPlusDevice(mDevice)) {
                   mA2dpService.updateTwsChannelMode(BluetoothA2dp.STATE_NOT_PLAYING, mDevice);
                }
            }
        }

        @Override
        public void exit() {
            Message currentMessage = getCurrentMessage();
            log("Exit Disconnected(" + mDevice + "): " + (currentMessage == null ? "null"
                    : messageWhatToString(currentMessage.what)));
            mLastConnectionState = BluetoothProfile.STATE_DISCONNECTED;
        }

        @Override
        public boolean processMessage(Message message) {
            log("Disconnected process message(" + mDevice + "): "
                    + messageWhatToString(message.what));

            switch (message.what) {
                case CONNECT:
                    Log.i(TAG, "Connecting to " + mDevice);
                    if (!mA2dpNativeInterface.connectA2dp(mDevice)) {
                        Log.e(TAG, "Disconnected: error connecting to " + mDevice);
                        break;
                    }
                    if (mA2dpService.okToConnect(mDevice, true)) {
                        transitionTo(mConnecting);
                    } else {
                        // Reject the request and stay in Disconnected state
                        Log.w(TAG, "Outgoing A2DP Connecting request rejected: " + mDevice);
                    }
                    break;
                case DISCONNECT:
                    Log.w(TAG, "Disconnected: DISCONNECT ignored: " + mDevice);
                    break;
                case STACK_EVENT:
                    A2dpStackEvent event = (A2dpStackEvent) message.obj;
                    log("Disconnected: stack event: " + event);
                    if (!mDevice.equals(event.device)) {
                        Log.wtf(TAG, "Device(" + mDevice + "): event mismatch: " + event);
                    }
                    switch (event.type) {
                        case A2dpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(event.valueInt);
                            break;
                        case A2dpStackEvent.EVENT_TYPE_CODEC_CONFIG_CHANGED:
                            processCodecConfigEvent(event.codecStatus);
                            break;
                        default:
                            Log.e(TAG, "Disconnected: ignoring stack event: " + event);
                            break;
                    }
                    break;
                default:
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        // in Disconnected state
        private void processConnectionEvent(int event) {
            switch (event) {
                case A2dpStackEvent.CONNECTION_STATE_DISCONNECTED:
                    Log.w(TAG, "Ignore A2DP DISCONNECTED event: " + mDevice);
                    break;
                case A2dpStackEvent.CONNECTION_STATE_CONNECTING:
                    if (mA2dpService.okToConnect(mDevice, false)) {
                        Log.i(TAG, "Incoming A2DP Connecting request accepted: " + mDevice);
                        transitionTo(mConnecting);
                        if (mA2dpService.isQtiLeAudioEnabled()) {
                            MediaAudioIntf mMediaAudio = MediaAudioIntf.get();
                            mMediaAudio.autoConnect(mDevice);
                        }
                    } else {
                        // Reject the connection and stay in Disconnected state itself
                        Log.w(TAG, "Incoming A2DP Connecting request rejected: " + mDevice);
                        mA2dpNativeInterface.disconnectA2dp(mDevice);
                    }
                    break;
                case A2dpStackEvent.CONNECTION_STATE_CONNECTED:
                    Log.w(TAG, "A2DP Connected from Disconnected state: " + mDevice);
                    if (mA2dpService.okToConnect(mDevice, false)) {
                        Log.i(TAG, "Incoming A2DP Connected request accepted: " + mDevice);
                        transitionTo(mConnected);
                        if (mA2dpService.isQtiLeAudioEnabled()) {
                            MediaAudioIntf mMediaAudio = MediaAudioIntf.get();
                            mMediaAudio.autoConnect(mDevice);
                        }
                    } else {
                        // Reject the connection and stay in Disconnected state itself
                        Log.w(TAG, "Incoming A2DP Connected request rejected: " + mDevice);
                        mA2dpNativeInterface.disconnectA2dp(mDevice);
                    }
                    break;
                case A2dpStackEvent.CONNECTION_STATE_DISCONNECTING:
                    Log.w(TAG, "Ignore A2DP DISCONNECTING event: " + mDevice);
                    break;
                default:
                    Log.e(TAG, "Incorrect event: " + event + " device: " + mDevice);
                    break;
            }
        }
    }

    @VisibleForTesting
    class Connecting extends State {
        @Override
        public void enter() {
            Message currentMessage = getCurrentMessage();
            Log.i(TAG, "Enter Connecting(" + mDevice + "): " + (currentMessage == null ? "null"
                    : messageWhatToString(currentMessage.what)));
            sendMessageDelayed(CONNECT_TIMEOUT, sConnectTimeoutMs);
            synchronized (this) {
                mConnectionState = BluetoothProfile.STATE_CONNECTING;
            }
            broadcastConnectionState(mConnectionState, mLastConnectionState);
        }

        @Override
        public void exit() {
            Message currentMessage = getCurrentMessage();
            log("Exit Connecting(" + mDevice + "): " + (currentMessage == null ? "null"
                    : messageWhatToString(currentMessage.what)));
            mLastConnectionState = BluetoothProfile.STATE_CONNECTING;
            removeMessages(CONNECT_TIMEOUT);
        }

        @Override
        public boolean processMessage(Message message) {
            log("Connecting process message(" + mDevice + "): "
                    + messageWhatToString(message.what));

            switch (message.what) {
                case CONNECT:
                    deferMessage(message);
                    break;
                case CONNECT_TIMEOUT: {
                    Log.w(TAG, "Connecting connection timeout: " + mDevice);
                    mA2dpNativeInterface.disconnectA2dp(mDevice);
                    A2dpStackEvent event =
                            new A2dpStackEvent(A2dpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
                    event.device = mDevice;
                    event.valueInt = A2dpStackEvent.CONNECTION_STATE_DISCONNECTED;
                    sendMessage(STACK_EVENT, event);
                    break;
                }
                case DISCONNECT:
                    // Cancel connection
                    Log.i(TAG, "Connecting: connection canceled to " + mDevice);
                    mA2dpNativeInterface.disconnectA2dp(mDevice);
                    transitionTo(mDisconnected);
                    break;
                case STACK_EVENT:
                    A2dpStackEvent event = (A2dpStackEvent) message.obj;
                    log("Connecting: stack event: " + event);
                    if (!mDevice.equals(event.device)) {
                        Log.wtf(TAG, "Device(" + mDevice + "): event mismatch: " + event);
                    }
                    switch (event.type) {
                        case A2dpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(event.valueInt);
                            break;
                        case A2dpStackEvent.EVENT_TYPE_CODEC_CONFIG_CHANGED:
                            processCodecConfigEvent(event.codecStatus);
                            break;
                        case A2dpStackEvent.EVENT_TYPE_AUDIO_STATE_CHANGED:
                            break;
                        default:
                            Log.e(TAG, "Connecting: ignoring stack event: " + event);
                            break;
                    }
                    break;
                default:
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        // in Connecting state
        private void processConnectionEvent(int event) {
            switch (event) {
                case A2dpStackEvent.CONNECTION_STATE_DISCONNECTED:
                    Log.w(TAG, "Connecting device disconnected: " + mDevice);
                    transitionTo(mDisconnected);
                    break;
                case A2dpStackEvent.CONNECTION_STATE_CONNECTED:
                    transitionTo(mConnected);
                    break;
                case A2dpStackEvent.CONNECTION_STATE_CONNECTING:
                    // Ignored - probably an event that the outgoing connection was initiated
                    break;
                case A2dpStackEvent.CONNECTION_STATE_DISCONNECTING:
                    Log.w(TAG, "Connecting interrupted: device is disconnecting: " + mDevice);
                    transitionTo(mDisconnecting);
                    break;
                default:
                    Log.e(TAG, "Incorrect event: " + event);
                    break;
            }
        }
    }

    @VisibleForTesting
    class Disconnecting extends State {
        @Override
        public void enter() {
            Message currentMessage = getCurrentMessage();
            Log.i(TAG, "Enter Disconnecting(" + mDevice + "): " + (currentMessage == null ? "null"
                    : messageWhatToString(currentMessage.what)));
            mCodecConfigUpdated = false;
            sendMessageDelayed(CONNECT_TIMEOUT, sConnectTimeoutMs);
            synchronized (this) {
                mConnectionState = BluetoothProfile.STATE_DISCONNECTING;
            }
            broadcastConnectionState(mConnectionState, mLastConnectionState);
        }

        @Override
        public void exit() {
            Message currentMessage = getCurrentMessage();
            log("Exit Disconnecting(" + mDevice + "): " + (currentMessage == null ? "null"
                    : messageWhatToString(currentMessage.what)));
            mLastConnectionState = BluetoothProfile.STATE_DISCONNECTING;
            removeMessages(CONNECT_TIMEOUT);
        }

        @Override
        public boolean processMessage(Message message) {
            log("Disconnecting process message(" + mDevice + "): "
                    + messageWhatToString(message.what));

            switch (message.what) {
                case CONNECT:
                    deferMessage(message);
                    break;
                case CONNECT_TIMEOUT: {
                    Log.w(TAG, "Disconnecting connection timeout: " + mDevice);
                    mA2dpNativeInterface.disconnectA2dp(mDevice);
                    A2dpStackEvent event =
                            new A2dpStackEvent(A2dpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED);
                    event.device = mDevice;
                    event.valueInt = A2dpStackEvent.CONNECTION_STATE_DISCONNECTED;
                    sendMessage(STACK_EVENT, event);
                    break;
                }
                case DISCONNECT:
                    deferMessage(message);
                    break;
                case STACK_EVENT:
                    A2dpStackEvent event = (A2dpStackEvent) message.obj;
                    log("Disconnecting: stack event: " + event);
                    if (!mDevice.equals(event.device)) {
                        Log.wtf(TAG, "Device(" + mDevice + "): event mismatch: " + event);
                    }
                    switch (event.type) {
                        case A2dpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(event.valueInt);
                            break;
                        case A2dpStackEvent.EVENT_TYPE_CODEC_CONFIG_CHANGED:
                            processCodecConfigEvent(event.codecStatus);
                            break;
                        case A2dpStackEvent.EVENT_TYPE_AUDIO_STATE_CHANGED:
                        default:
                            Log.e(TAG, "Disconnecting: ignoring stack event: " + event);
                            break;
                    }
                    break;
                default:
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        // in Disconnecting state
        private void processConnectionEvent(int event) {
            switch (event) {
                case A2dpStackEvent.CONNECTION_STATE_DISCONNECTED:
                    Log.i(TAG, "Disconnected: " + mDevice);
                    transitionTo(mDisconnected);
                    break;
                case A2dpStackEvent.CONNECTION_STATE_CONNECTED:
                    if (mA2dpService.okToConnect(mDevice, false)) {
                        Log.w(TAG, "Disconnecting interrupted: device is connected: " + mDevice);
                        transitionTo(mConnected);
                        if (mA2dpService.isQtiLeAudioEnabled()) {
                            MediaAudioIntf mMediaAudio = MediaAudioIntf.get();
                            mMediaAudio.autoConnect(mDevice);
                        }
                    } else {
                        // Reject the connection and stay in Disconnecting state
                        Log.w(TAG, "Incoming A2DP Connected request rejected: " + mDevice);
                        mA2dpNativeInterface.disconnectA2dp(mDevice);
                    }
                    break;
                case A2dpStackEvent.CONNECTION_STATE_CONNECTING:
                    if (mA2dpService.okToConnect(mDevice, false)) {
                        Log.i(TAG, "Disconnecting interrupted: try to reconnect: " + mDevice);
                        transitionTo(mConnecting);
                        if (mA2dpService.isQtiLeAudioEnabled()) {
                            MediaAudioIntf mMediaAudio = MediaAudioIntf.get();
                            mMediaAudio.autoConnect(mDevice);
                        }
                    } else {
                        // Reject the connection and stay in Disconnecting state
                        Log.w(TAG, "Incoming A2DP Connecting request rejected: " + mDevice);
                        mA2dpNativeInterface.disconnectA2dp(mDevice);
                    }
                    break;
                case A2dpStackEvent.CONNECTION_STATE_DISCONNECTING:
                    // We are already disconnecting, do nothing
                    Log.i(TAG, " already disconnecting, do nothing ");
                    break;
                default:
                    Log.e(TAG, "Incorrect event: " + event);
                    break;
            }
        }
    }

    @VisibleForTesting
    class Connected extends State {
        @Override
        public void enter() {
            Message currentMessage = getCurrentMessage();
            Log.i(TAG, "Enter Connected(" + mDevice + "): " + (currentMessage == null ? "null"
                    : messageWhatToString(currentMessage.what)));
            synchronized (this) {
                mConnectionState = BluetoothProfile.STATE_CONNECTED;
            }
            removeDeferredMessages(CONNECT);
            DeviceProfileMapIntf dpm = DeviceProfileMapIntf.getDeviceProfileMapInstance();
            dpm.profileConnectionUpdate(mDevice, ApmConstIntf.AudioFeatures.MEDIA_AUDIO,
                    ApmConstIntf.AudioProfiles.A2DP, true);
            // Each time a device connects, we want to re-check if it supports optional
            // codecs (perhaps it's had a firmware update, etc.) and save that state if
            // it differs from what we had saved before.
            mA2dpService.updateOptionalCodecsSupport(mDevice);
            broadcastConnectionState(mConnectionState, mLastConnectionState);
            // Upon connected, the audio starts out as stopped
            broadcastAudioState(BluetoothA2dp.STATE_NOT_PLAYING,
                                BluetoothA2dp.STATE_PLAYING);
        }

        @Override
        public void exit() {
            Message currentMessage = getCurrentMessage();
            log("Exit Connected(" + mDevice + "): " + (currentMessage == null ? "null"
                    : messageWhatToString(currentMessage.what)));
            mLastConnectionState = BluetoothProfile.STATE_CONNECTED;
        }

        @Override
        public boolean processMessage(Message message) {
            log("Connected process message(" + mDevice + "): " + messageWhatToString(message.what));

            switch (message.what) {
                case CONNECT:
                    Log.w(TAG, "Connected: CONNECT ignored: " + mDevice);
                    break;
                case DISCONNECT: {
                    Log.i(TAG, "Disconnecting from " + mDevice);
                    if (!mA2dpNativeInterface.disconnectA2dp(mDevice)) {
                        // If error in the native stack, transition directly to Disconnected state.
                        Log.e(TAG, "Connected: error disconnecting from " + mDevice);
                        transitionTo(mDisconnected);
                        break;
                    }
                    transitionTo(mDisconnecting);
                }
                break;
                case STACK_EVENT:
                    A2dpStackEvent event = (A2dpStackEvent) message.obj;
                    log("Connected: stack event: " + event);
                    if (!mDevice.equals(event.device)) {
                        Log.wtf(TAG, "Device(" + mDevice + "): event mismatch: " + event);
                    }
                    switch (event.type) {
                        case A2dpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(event.valueInt);
                            break;
                        case A2dpStackEvent.EVENT_TYPE_AUDIO_STATE_CHANGED:
                            processAudioStateEvent(event.valueInt);
                            break;
                        case A2dpStackEvent.EVENT_TYPE_CODEC_CONFIG_CHANGED:
                            processCodecConfigEvent(event.codecStatus);
                            break;
                        default:
                            Log.e(TAG, "Connected: ignoring stack event: " + event);
                            break;
                    }
                    break;
                default:
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        // in Connected state
        private void processConnectionEvent(int event) {
            switch (event) {
                case A2dpStackEvent.CONNECTION_STATE_DISCONNECTED:
                    Log.i(TAG, "Disconnected from " + mDevice);
                    transitionTo(mDisconnected);
                    break;
                case A2dpStackEvent.CONNECTION_STATE_CONNECTED:
                    Log.w(TAG, "Ignore A2DP CONNECTED event: " + mDevice);
                    break;
                case A2dpStackEvent.CONNECTION_STATE_CONNECTING:
                    Log.w(TAG, "Ignore A2DP CONNECTING event: " + mDevice);
                    break;
                case A2dpStackEvent.CONNECTION_STATE_DISCONNECTING:
                    Log.i(TAG, "Disconnecting from " + mDevice);
                    transitionTo(mDisconnecting);
                    break;
                default:
                    Log.e(TAG, "Connection State Device: " + mDevice + " bad event: " + event);
                    break;
            }
        }

        // in Connected state
        private void processAudioStateEvent(int state) {
            Log.i(TAG, "Connected: processAudioStateEvent: state: " + state + "mIsPlaying: " + mIsPlaying);
            switch (state) {
                case A2dpStackEvent.AUDIO_STATE_STARTED:
                    synchronized (this) {
                        if (!mIsPlaying) {
                            Log.i(TAG, "Connected: started playing: " + mDevice);
                            mIsPlaying = true;
                            mA2dpService.setAvrcpAudioState(BluetoothA2dp.STATE_PLAYING, mDevice);
                            broadcastAudioState(BluetoothA2dp.STATE_PLAYING,
                                                BluetoothA2dp.STATE_NOT_PLAYING);
                            Log.i(TAG,"state:AUDIO_STATE_STARTED");
                        }
                        AdapterService adapterService = AdapterService.getAdapterService();
                        if (adapterService.isVendorIntfEnabled() &&
                            adapterService.isTwsPlusDevice(mDevice)) {
                            mA2dpService.updateTwsChannelMode(BluetoothA2dp.STATE_PLAYING, mDevice);
                        }
                    }
                    break;
                case A2dpStackEvent.AUDIO_STATE_REMOTE_SUSPEND:
                case A2dpStackEvent.AUDIO_STATE_STOPPED:
                    synchronized (this) {
                        if (mIsPlaying) {
                            Log.i(TAG, "Connected: stopped playing: " + mDevice);
                            mIsPlaying = false;
                            mA2dpService.setAvrcpAudioState(BluetoothA2dp.STATE_NOT_PLAYING, mDevice);
                            broadcastAudioState(BluetoothA2dp.STATE_NOT_PLAYING,
                                                BluetoothA2dp.STATE_PLAYING);
                        }
                    }
                    break;
                default:
                    Log.e(TAG, "Audio State Device: " + mDevice + " bad state: " + state);
                    break;
            }
        }
    }

    int getConnectionState() {
        return mConnectionState;
    }

    BluetoothDevice getDevice() {
        return mDevice;
    }

    boolean isConnected() {
        synchronized (this) {
            return (mConnectionState == BluetoothProfile.STATE_CONNECTED);
        }
    }

    boolean isPlaying() {
        synchronized (this) {
            return mIsPlaying;
        }
    }

    BluetoothCodecStatus getCodecStatus() {
        synchronized (this) {
            return mCodecStatus;
        }
    }

    // NOTE: This event is processed in any state
    @VisibleForTesting
    void processCodecConfigEvent(BluetoothCodecStatus newCodecStatus) {
        BluetoothCodecConfig prevCodecConfig = null;
        BluetoothCodecStatus prevCodecStatus = mCodecStatus;

        int new_codec_type = newCodecStatus.getCodecConfig().getCodecType();

        // Split A2dp will be enabled by default
        boolean isSplitA2dpEnabled = true;
        AdapterService adapterService = AdapterService.getAdapterService();

        if (adapterService != null){
            isSplitA2dpEnabled = adapterService.isSplitA2dpEnabled();
            Log.v(TAG,"isSplitA2dpEnabled: " + isSplitA2dpEnabled);
        } else {
            Log.e(TAG,"adapterService is null");
        }

        Log.w(TAG,"processCodecConfigEvent: new_codec_type = " + new_codec_type);

        if (isSplitA2dpEnabled) {
            if (new_codec_type  == BluetoothCodecConfig.SOURCE_QVA_CODEC_TYPE_MAX) {
                if (adapterService.isVendorIntfEnabled() &&
                    adapterService.isTwsPlusDevice(mDevice)) {
                    Log.d(TAG,"TWSP device streaming,not calling reconfig");
                    mCodecStatus = newCodecStatus;
                    return;
                }
                mA2dpService.broadcastReconfigureA2dp();
                Log.w(TAG,"Split A2dp enabled rcfg send to Audio for codec max");
                return;
            }
        }
        synchronized (this) {
            if (mCodecStatus != null) {
                prevCodecConfig = mCodecStatus.getCodecConfig();
            }
            mCodecStatus = newCodecStatus;
        }
        if (DBG) {
            Log.d(TAG, "A2DP Codec Config: " + prevCodecConfig + "->"
                    + newCodecStatus.getCodecConfig());
            for (BluetoothCodecConfig codecConfig :
                     newCodecStatus.getCodecsLocalCapabilities()) {
                Log.d(TAG, "A2DP Codec Local Capability: " + codecConfig);
            }
            for (BluetoothCodecConfig codecConfig :
                     newCodecStatus.getCodecsSelectableCapabilities()) {
                Log.d(TAG, "A2DP Codec Selectable Capability: " + codecConfig);
            }
        }

        if (isConnected() && !sameSelectableCodec(prevCodecStatus, mCodecStatus)) {
            // Remote selectable codec could be changed if codec config changed
            // in connected state, we need to re-check optional codec status
            // for this codec change event.
            mA2dpService.updateOptionalCodecsSupport(mDevice);
        }
        Log.d(TAG, " mA2dpOffloadEnabled: " + mA2dpOffloadEnabled);
        if (mA2dpOffloadEnabled) {
            boolean update = false;
            BluetoothCodecConfig newCodecConfig = mCodecStatus.getCodecConfig();
            if ((prevCodecConfig != null)
                    && (prevCodecConfig.getCodecType() != newCodecConfig.getCodecType())) {
                Log.d(TAG, " previous codec is different from new codec ");
                update = true;
            } else if (!newCodecConfig.sameAudioFeedingParameters(prevCodecConfig)) {
                Log.d(TAG, " codec config parameters mismatched from previous config ");
                update = true;
            } else if ((newCodecConfig.getCodecType()
                        == BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC)
                    && (prevCodecConfig != null)
                    && (prevCodecConfig.getCodecSpecific1()
                        != newCodecConfig.getCodecSpecific1())) {
                Log.d(TAG, "LDAC: codec config parameters mismatched from previous config ");
                update = true;
            } else if (prevCodecStatus != null &&
                       newCodecStatus.getCodecsSelectableCapabilities().size() !=
                       prevCodecStatus.getCodecsSelectableCapabilities().size()){
                Log.d(TAG, " codec selectable caps mismatched from previous config ");
                update = true;
            } else if (!mCodecConfigUpdated) {
                Log.d(TAG, " mCodecConfigUpdated is false, codecConfigUpdated is required");
                update = true;
            }
            Log.d(TAG, " update: " + update);
            if (update) {
                mA2dpService.codecConfigUpdated(mDevice, mCodecStatus, false);
                mCodecConfigUpdated = true;
            }
            return;
        }

        Log.d(TAG, " isSplitA2dpEnabled: " + isSplitA2dpEnabled);
        if (!isSplitA2dpEnabled) {
            boolean isUpdateRequired = false;
            if ((prevCodecConfig != null) && (prevCodecConfig.getCodecType() != new_codec_type)) {
                Log.d(TAG, "previous codec is differs from new codec");
                isUpdateRequired = true;
            } else if (!newCodecStatus.getCodecConfig().sameAudioFeedingParameters(prevCodecConfig)) {
                Log.d(TAG, "codec config parameters mismatched with previous config: ");
                isUpdateRequired = true;
            } else if ((newCodecStatus.getCodecConfig().getCodecType()
                        == BluetoothCodecConfig.SOURCE_CODEC_TYPE_LDAC)
                    && (prevCodecConfig != null)
                    && (prevCodecConfig.getCodecSpecific1()
                        != newCodecStatus.getCodecConfig().getCodecSpecific1())) {
                Log.d(TAG, "LDAC: codec config parameters mismatched with previous config: ");
                isUpdateRequired = true;
            } else if(!mCodecConfigUpdated) {
                Log.d(TAG, " mCodecConfigUpdated is false, codecConfigUpdated is required ");
                isUpdateRequired = true;
            }
            Log.d(TAG, "isUpdateRequired: " + isUpdateRequired);
            //update MM only when previous and current codec config has been changed
            // OR reconnection has happened
            if (isUpdateRequired) {
                mA2dpService.codecConfigUpdated(mDevice, mCodecStatus, false);
                mCodecConfigUpdated = true;
            }
        }
    }

    // This method does not check for error conditon (newState == prevState)
    private void broadcastConnectionState(int newState, int prevState) {
        log("Connection state " + mDevice + ": " + profileStateToString(prevState)
                    + "->" + profileStateToString(newState));

        if(mA2dpService.isQtiLeAudioEnabled() || ApmConstIntf.getAospLeaEnabled()) {
            mA2dpService.updateConnState(mDevice, newState);
        }

        if (!mA2dpService.isQtiLeAudioEnabled()) {
            Intent intent = new Intent(BluetoothA2dp.ACTION_CONNECTION_STATE_CHANGED);
            intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, prevState);
            intent.putExtra(BluetoothProfile.EXTRA_STATE, newState);
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, mDevice);
            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                            | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
            mA2dpService.sendBroadcast(intent, BLUETOOTH_CONNECT,
                    Utils.getTempAllowlistBroadcastOptions());
        }
    }

    private void broadcastAudioState(int newState, int prevState) {
        log("A2DP Playing state : device: " + mDevice + " State:" + audioStateToString(prevState)
                + "->" + audioStateToString(newState));

        if(mA2dpService.isQtiLeAudioEnabled()) {
            mA2dpService.updateStreamState(mDevice, newState);
            return;
        }

        BluetoothStatsLog.write(BluetoothStatsLog.BLUETOOTH_A2DP_PLAYBACK_STATE_CHANGED, newState);
        Intent intent = new Intent(BluetoothA2dp.ACTION_PLAYING_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, mDevice);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, prevState);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, newState);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        mA2dpService.sendBroadcast(intent, BLUETOOTH_CONNECT,
                Utils.getTempAllowlistBroadcastOptions());
    }

    @Override
    protected String getLogRecString(Message msg) {
        StringBuilder builder = new StringBuilder();
        builder.append(messageWhatToString(msg.what));
        builder.append(": ");
        builder.append("arg1=")
                .append(msg.arg1)
                .append(", arg2=")
                .append(msg.arg2)
                .append(", obj=")
                .append(msg.obj);
        return builder.toString();
    }

    private static boolean sameSelectableCodec(BluetoothCodecStatus prevCodecStatus,
            BluetoothCodecStatus newCodecStatus) {
        if (prevCodecStatus == null || newCodecStatus == null) {
            return false;
        }
        List<BluetoothCodecConfig> c1 = prevCodecStatus.getCodecsSelectableCapabilities();
        List<BluetoothCodecConfig> c2 = newCodecStatus.getCodecsSelectableCapabilities();
        if (c1 == null) {
            return (c2 == null);
        }
        if (c2 == null) {
            return false;
        }
        if (c1.size() != c2.size()) {
            return false;
        }
        return c1.containsAll(c2);
    }

    private static String messageWhatToString(int what) {
        switch (what) {
            case CONNECT:
                return "CONNECT";
            case DISCONNECT:
                return "DISCONNECT";
            case STACK_EVENT:
                return "STACK_EVENT";
            case CONNECT_TIMEOUT:
                return "CONNECT_TIMEOUT";
            default:
                break;
        }
        return Integer.toString(what);
    }

    private static String profileStateToString(int state) {
        switch (state) {
            case BluetoothProfile.STATE_DISCONNECTED:
                return "DISCONNECTED";
            case BluetoothProfile.STATE_CONNECTING:
                return "CONNECTING";
            case BluetoothProfile.STATE_CONNECTED:
                return "CONNECTED";
            case BluetoothProfile.STATE_DISCONNECTING:
                return "DISCONNECTING";
            default:
                break;
        }
        return Integer.toString(state);
    }

    private static String audioStateToString(int state) {
        switch (state) {
            case BluetoothA2dp.STATE_PLAYING:
                return "PLAYING";
            case BluetoothA2dp.STATE_NOT_PLAYING:
                return "NOT_PLAYING";
            default:
                break;
        }
        return Integer.toString(state);
    }

    public void dump(StringBuilder sb) {
        ProfileService.println(sb, "mDevice: " + mDevice);
        ProfileService.println(sb, "  StateMachine: " + this.toString());
        ProfileService.println(sb, "  mIsPlaying: " + mIsPlaying);
        synchronized (this) {
            if (mCodecStatus != null) {
                ProfileService.println(sb, "  mCodecConfig: " + mCodecStatus.getCodecConfig());
            }
        }
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

    @Override
    protected void log(String msg) {
        if (DBG) {
            super.log(msg);
        }
    }
}
