/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

/**
 * Bluetooth ATP Locator StateMachine. There is one instance per remote device.
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

package com.android.bluetooth.directionfinder;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import com.android.bluetooth.Utils;
import android.bluetooth.BluetoothLeDirectionFinder;
import android.content.Context;
import android.content.Intent;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import com.android.bluetooth.directionfinder.AtpEnableDirFinding;

import com.android.bluetooth.btservice.ProfileService;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;

final class AtpLocatorStateMachine extends StateMachine {
    private static final boolean DBG = true;
    private static final String TAG = "AtpLocatorStateMachine";

    static final int CONNECT = 1;
    static final int DISCONNECT = 2;
    static final int ENABLE_BLE_DIRECTION_FINDING = 3;

    @VisibleForTesting
    static final int STACK_EVENT = 101;
    private static final int CONNECT_TIMEOUT = 201;
    private static final int ENABLE_BLE_DIR_FINDING_TIMEOUT = 202;

    private static final int MAX_ERROR_RETRY_TIMES = 3;
    private static final int CMD_TIMEOUT_DELAY = 3000;


    // NOTE: the value is not "final" - it is modified in the unit tests
    @VisibleForTesting
    static int sConnectTimeoutMs = 30000;        // 30s

    private Disconnected mDisconnected;
    private Connecting mConnecting;
    private Disconnecting mDisconnecting;
    private Connected mConnected;
    private int mLastConnectionState = -1;

    private AtpLocator mAtpLocator;
    private AtpLocatorNativeInterface mNativeInterface;
    private Context mContext;

    private boolean mEnableBleDFInProgress;

    private final BluetoothDevice mDevice;

    AtpLocatorStateMachine(BluetoothDevice device, AtpLocator svc, Context context,
            AtpLocatorNativeInterface nativeInterface, Looper looper) {
        super(TAG, looper);
        mDevice = device;
        mAtpLocator = svc;
        mContext = context;
        mNativeInterface = nativeInterface;

        mDisconnected = new Disconnected();
        mConnecting = new Connecting();
        mDisconnecting = new Disconnecting();
        mConnected = new Connected();

        addState(mDisconnected);
        addState(mConnecting);
        addState(mDisconnecting);
        addState(mConnected);

        setInitialState(mDisconnected);
    }

    static AtpLocatorStateMachine make(BluetoothDevice device, AtpLocator svc,
            Context context, AtpLocatorNativeInterface nativeInterface, Looper looper) {
        Log.i(TAG, "make for device " + device);
        AtpLocatorStateMachine AtpLocatorSm =
                new AtpLocatorStateMachine(device, svc, context, nativeInterface, looper);
        AtpLocatorSm.start();
        return AtpLocatorSm;
    }

    public void doQuit() {
        log("doQuit for device " + mDevice);
        quitNow();
    }

    public void cleanup() {
        log("cleanup for device " + mDevice);
    }

    @VisibleForTesting
    class Disconnected extends State {
        @Override
        public void enter() {
            Log.i(TAG, "Enter Disconnected(" + mDevice + "): " + messageWhatToString(
                    getCurrentMessage().what));

            removeDeferredMessages(DISCONNECT);
            if (mLastConnectionState != -1) {
                // Don't broadcast during startup
                broadcastConnectionState(BluetoothProfile.STATE_DISCONNECTED,
                        mLastConnectionState);
            }

            cleanupDevice();
        }

        @Override
        public void exit() {
            log("Exit Disconnected(" + mDevice + "): " + messageWhatToString(
                    getCurrentMessage().what));
            mLastConnectionState = BluetoothProfile.STATE_DISCONNECTED;
        }

        @Override
        public boolean processMessage(Message message) {
            log("Disconnected process message(" + mDevice + "): " + messageWhatToString(
                    message.what));

            switch (message.what) {
                case CONNECT:
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    log("Connecting to " + device);

                    if (!mDevice.equals(device)) {
                        Log.e(TAG, "CONNECT failed, device=" + device + ", currentDev=" + mDevice);
                        break;
                    }

                    if (!mNativeInterface.connectAtp(mDevice, true)) {
                        Log.e(TAG, "Disconnected: error connecting to " + mDevice);
                        break;
                    }

                    transitionTo(mConnecting);
                    break;
                case DISCONNECT:
                    Log.w(TAG, "Disconnected: DISCONNECT ignored: " + mDevice);
                    break;
                case STACK_EVENT:
                    AtpStackEvent event = (AtpStackEvent) message.obj;
                    if (DBG) {
                        Log.d(TAG, "Disconnected: stack event: " + event);
                    }
                    if (!mDevice.equals(event.device)) {
                        Log.wtfStack(TAG, "Device(" + mDevice + "): event mismatch: " + event);
                        break;
                    }
                    switch (event.type) {
                        case AtpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(event.valueInt1);
                            break;
                        default:
                            Log.e(TAG, "Disconnected: ignoring stack event: " + event);
                            break;
                    }
                    break;
                default:
                    Log.e(TAG, "Unexpected msg " + messageWhatToString(message.what)
                            + ": " + message);
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        // in Disconnected state
        private void processConnectionEvent(int state) {
            switch (state) {
                case AtpStackEvent.CONNECTION_STATE_DISCONNECTED:
                    Log.w(TAG, "Ignore ATP DISCONNECTED event: " + mDevice);
                    break;
                case AtpStackEvent.CONNECTION_STATE_CONNECTING:
                    Log.i(TAG, "Incoming ATP Connecting request accepted: " + mDevice);
                    if (mAtpLocator.okToConnect(mDevice)) {
                        transitionTo(mConnecting);
                    } else {
                        // Reject the connection and stay in Disconnected state itself
                        Log.w(TAG, "Incoming ATP Connecting request rejected: " + mDevice);
                        mNativeInterface.disconnectAtp(mDevice);
                    }
                    break;
                case AtpStackEvent.CONNECTION_STATE_CONNECTED:
                    Log.w(TAG, "ATP Connected from Disconnected state: " + mDevice);
                    if (mAtpLocator.okToConnect(mDevice)) {
                        Log.i(TAG, "Incoming ATP Connected request accepted: " + mDevice);
                        transitionTo(mConnected);
                    } else {
                        // Reject the connection and stay in Disconnected state itself
                        Log.w(TAG, "Incoming ATP Connected request rejected: " + mDevice);
                        mNativeInterface.disconnectAtp(mDevice);
                    }
                    break;
                case AtpStackEvent.CONNECTION_STATE_DISCONNECTING:
                    Log.w(TAG, "Ignore ATP DISCONNECTING event: " + mDevice);
                    break;
                default:
                    Log.e(TAG, "Incorrect state: " + state + " device: " + mDevice);
                    break;
            }
        }
    }

    @VisibleForTesting
    class Connecting extends State {
        @Override
        public void enter() {
            Log.i(TAG, "Enter Connecting(" + mDevice + "): "
                    + messageWhatToString(getCurrentMessage().what));
            sendMessageDelayed(CONNECT_TIMEOUT, mDevice, sConnectTimeoutMs);
            broadcastConnectionState(BluetoothProfile.STATE_CONNECTING, mLastConnectionState);
        }

        @Override
        public void exit() {
            log("Exit Connecting(" + mDevice + "): "
                    + messageWhatToString(getCurrentMessage().what));
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
                case CONNECT_TIMEOUT:
                    Log.w(TAG, "Connecting connection timeout: " + mDevice);
                    mNativeInterface.disconnectAtp(mDevice);
                    // We timed out trying to connect, transition to Disconnected state
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        Log.e(TAG, "Unknown device timeout " + device);
                        break;
                    }
                    Log.w(TAG, "CONNECT_TIMEOUT");
                    transitionTo(mDisconnected);
                    break;
                case DISCONNECT:
                    log("Connecting: connection canceled to " + mDevice);
                    mNativeInterface.disconnectAtp(mDevice);
                    transitionTo(mDisconnected);
                    break;
                case ENABLE_BLE_DIRECTION_FINDING:
                    deferMessage(message);
                    break;
                case STACK_EVENT:
                    AtpStackEvent event = (AtpStackEvent) message.obj;
                    log("Connecting: stack event: " + event);
                    if (!mDevice.equals(event.device)) {
                        Log.wtfStack(TAG, "Device(" + mDevice + "): event mismatch: " + event);
                    }
                    switch (event.type) {
                        case AtpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(event.valueInt1);
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
        private void processConnectionEvent(int state) {
            switch (state) {
                case AtpStackEvent.CONNECTION_STATE_DISCONNECTED:
                    Log.w(TAG, "Connecting device disconnected: " + mDevice);
                    transitionTo(mDisconnected);
                    break;
                case AtpStackEvent.CONNECTION_STATE_CONNECTED:
                    transitionTo(mConnected);
                    break;
                case AtpStackEvent.CONNECTION_STATE_CONNECTING:
                    break;
                case AtpStackEvent.CONNECTION_STATE_DISCONNECTING:
                    Log.w(TAG, "Connecting interrupted: device is disconnecting: " + mDevice);
                    transitionTo(mDisconnecting);
                    break;
                default:
                    Log.e(TAG, "Incorrect state: " + state);
                    break;
            }
        }
    }

    @VisibleForTesting
    class Disconnecting extends State {
        @Override
        public void enter() {
            Log.i(TAG, "Enter Disconnecting(" + mDevice + "): "
                    + messageWhatToString(getCurrentMessage().what));
            sendMessageDelayed(CONNECT_TIMEOUT, mDevice, sConnectTimeoutMs);
            broadcastConnectionState(BluetoothProfile.STATE_DISCONNECTING, mLastConnectionState);
        }

        @Override
        public void exit() {
            log("Exit Disconnecting(" + mDevice + "): "
                    + messageWhatToString(getCurrentMessage().what));
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
                    mNativeInterface.disconnectAtp(mDevice);
                    // We timed out trying to connect, transition to Disconnected state
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        Log.e(TAG, "Unknown device timeout " + device);
                        break;
                    }
                    transitionTo(mDisconnected);
                    Log.w(TAG, "CONNECT_TIMEOUT");
                    break;
                }
                case DISCONNECT:
                    deferMessage(message);
                    break;
                case ENABLE_BLE_DIRECTION_FINDING:
                    deferMessage(message);
                    break;
                case STACK_EVENT:
                    AtpStackEvent event = (AtpStackEvent) message.obj;
                    log("Disconnecting: stack event: " + event);
                    if (!mDevice.equals(event.device)) {
                        Log.wtfStack(TAG, "Device(" + mDevice + "): event mismatch: " + event);
                    }
                    switch (event.type) {
                        case AtpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(event.valueInt1);
                            break;
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
        private void processConnectionEvent(int state) {
            switch (state) {
                case AtpStackEvent.CONNECTION_STATE_DISCONNECTED:
                    Log.i(TAG, "Disconnected: " + mDevice);
                    transitionTo(mDisconnected);
                    break;
                case AtpStackEvent.CONNECTION_STATE_CONNECTED:
                    if (mAtpLocator.okToConnect(mDevice)) {
                        Log.w(TAG, "Disconnecting interrupted: device is connected: " + mDevice);
                        transitionTo(mConnected);
                    } else {
                        // Reject the connection and stay in Disconnecting state
                        Log.w(TAG, "Incoming ATP Connected request rejected: " + mDevice);
                        mNativeInterface.disconnectAtp(mDevice);
                    }
                    break;
                case AtpStackEvent.CONNECTION_STATE_CONNECTING:
                    if (mAtpLocator.okToConnect(mDevice)) {
                        Log.i(TAG, "Disconnecting interrupted: try to reconnect: " + mDevice);
                        transitionTo(mConnecting);
                    } else {
                        // Reject the connection and stay in Disconnecting state
                        Log.w(TAG, "Incoming ATP Connecting request rejected: " + mDevice);
                        mNativeInterface.disconnectAtp(mDevice);
                    }
                    break;
                case AtpStackEvent.CONNECTION_STATE_DISCONNECTING:
                    break;
                default:
                    Log.e(TAG, "Incorrect state: " + state);
                    break;
            }
        }
    }

    @VisibleForTesting
    class Connected extends State {
        @Override
        public void enter() {
            Log.i(TAG, "Enter Connected(" + mDevice + "): "
                    + messageWhatToString(getCurrentMessage().what));
            removeDeferredMessages(CONNECT);
            broadcastConnectionState(BluetoothProfile.STATE_CONNECTED, mLastConnectionState);
        }

        @Override
        public void exit() {
            log("Exit Connected(" + mDevice + "): "
                    + messageWhatToString(getCurrentMessage().what));
            mLastConnectionState = BluetoothProfile.STATE_CONNECTED;
        }

        @Override
        public boolean processMessage(Message message) {
            log("Connected process message(" + mDevice + "): "
                    + messageWhatToString(message.what));

            switch (message.what) {
                case CONNECT: {
                    Log.w(TAG, "Connected: CONNECT ignored: " + mDevice);
                    break;
                }
                case DISCONNECT: {
                    log("Disconnecting from " + mDevice);
                    if (!mNativeInterface.disconnectAtp(mDevice)) {
                        // If error in the native stack, transition directly to Disconnected state.
                        Log.e(TAG, "Connected: error disconnecting from " + mDevice);
                        transitionTo(mDisconnected);
                        break;
                    }
                    transitionTo(mDisconnecting);
                    break;
                }
                case ENABLE_BLE_DIRECTION_FINDING: {
                    AtpEnableDirFinding atpEnableDfObj = (AtpEnableDirFinding) message.obj;
                    if (!mDevice.equals(atpEnableDfObj.device)) {
                        Log.w(TAG, "ENABLE_BLE_DIRECTION_FINDING failed "
                                + atpEnableDfObj.device
                                + " is not currentDevice");
                        break;
                    }
                    log("Enable BLE DIR FINDING for " + atpEnableDfObj.device);

                    processEnableBleDirectionFinding(atpEnableDfObj.samplingEnable,
                            atpEnableDfObj.slotDurations, atpEnableDfObj.enable,
                            atpEnableDfObj.cteReqInt, atpEnableDfObj.reqCteLen,
                            atpEnableDfObj.dirFindingType);
                    break;
                }
                case ENABLE_BLE_DIR_FINDING_TIMEOUT: {
                    BluetoothDevice device = (BluetoothDevice) message.obj;
                    if (!mDevice.equals(device)) {
                        Log.w(TAG, "Enable BLE Dir Finding timeout failed " + device
                                + " is not currentDevice");
                        break;
                    }

                    mEnableBleDFInProgress = false;
                    int errorStatus = -1;
                    processEnableBleDirFindingEvent(errorStatus);
                    break;
                }

                case STACK_EVENT:
                    AtpStackEvent event = (AtpStackEvent) message.obj;
                    log("Connected: stack event: " + event);
                    if (!mDevice.equals(event.device)) {
                        Log.wtfStack(TAG, "Device(" + mDevice + "): event mismatch: " + event);
                    }
                    switch (event.type) {
                        case AtpStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED:
                            processConnectionEvent(event.valueInt1);
                            break;
                        case AtpStackEvent.EVENT_TYPE_ENABLE_BLE_DIRECTION_FINDING:
                            removeMessages(ENABLE_BLE_DIR_FINDING_TIMEOUT);
                            mEnableBleDFInProgress = false;
                            processEnableBleDirFindingEvent(event.valueInt1);
                            break;
                        case AtpStackEvent.EVENT_TYPE_LE_AOA_RESULTS:
                            processLeAoaResultsEvent(event.valueInt1, event.valueDouble1, event.valueInt2,
                                    event.valueDouble2, event.valueInt3);
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
        private void processConnectionEvent(int state) {
            switch (state) {
                case AtpStackEvent.CONNECTION_STATE_DISCONNECTED:
                    Log.i(TAG, "Disconnected from " + mDevice);
                    transitionTo(mDisconnected);
                    break;
                case AtpStackEvent.CONNECTION_STATE_DISCONNECTING:
                    Log.i(TAG, "Disconnecting from " + mDevice);
                    transitionTo(mDisconnecting);
                    break;
                default:
                    Log.e(TAG, "Connection State Device: " + mDevice + " bad state: " + state);
                    break;
            }
        }
    }

    private void processEnableBleDirectionFinding(int samplingEnable, int slotDurations,
            int enable, int cteReqInt, int reqCteLen, int dirFindingType) {
        log("process enable Ble Direction Finding");

        if (mEnableBleDFInProgress) {
            Log.w(TAG, "There is already an enableBle Direction Finding in progress");
            return;
        }

        Log.d(TAG, "enableBleDirectionFinding");
        if (mNativeInterface.enableBleDirectionFinding(samplingEnable, slotDurations,
                enable, cteReqInt, reqCteLen, dirFindingType, mDevice)) {
            sendMessageDelayed(ENABLE_BLE_DIR_FINDING_TIMEOUT, mDevice,
                               CMD_TIMEOUT_DELAY);
            mEnableBleDFInProgress = true;
        } else {
            Log.e(TAG, "Enable Ble Direction Finding failed for device: " + mDevice);
        }
    }

    private void processEnableBleDirFindingEvent(int status) {
        log("processEnableBleDirFindingEvent, status:" + status);
        mAtpLocator.onEnableBleDirectionFinding(mDevice, status);
    }

    private void processLeAoaResultsEvent(int status, double azimuth, int azimuthUnc,
            double elevation, int elevationUnc) {
        log("processLeAoaResultsEvent");
        mAtpLocator.onLeAoaResults(mDevice, status, azimuth, azimuthUnc, elevation,
                elevationUnc);
    }

    int getConnectionState() {
        String currentState = getCurrentState().getName();
        switch (currentState) {
            case "Disconnected":
                return BluetoothProfile.STATE_DISCONNECTED;
            case "Connecting":
                return BluetoothProfile.STATE_CONNECTING;
            case "Connected":
                return BluetoothProfile.STATE_CONNECTED;
            case "Disconnecting":
                return BluetoothProfile.STATE_DISCONNECTING;
            default:
                Log.e(TAG, "Bad currentState: " + currentState);
                return BluetoothProfile.STATE_DISCONNECTED;
        }
    }

    BluetoothDevice getDevice() {
        return mDevice;
    }

    synchronized boolean isConnected() {
        return getCurrentState() == mConnected;
    }

    private void broadcastConnectionState(int newState, int prevState) {
        log("Connection state " + mDevice + ": " + profileStateToString(prevState)
                    + "->" + profileStateToString(newState));
        mAtpLocator.onConnectionStateChangedFromStateMachine(mDevice,
                newState, prevState);

        Intent intent;
        intent = new Intent(BluetoothLeDirectionFinder.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, prevState);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, newState);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, mDevice);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                        | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        mContext.sendBroadcast(intent, BLUETOOTH_CONNECT,
             Utils.getTempAllowlistBroadcastOptions());
    }

    private void cleanupDevice() {
        log("cleanup device " + mDevice);
        mEnableBleDFInProgress = false;
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
            case ENABLE_BLE_DIRECTION_FINDING:
                return "ENABLE_BLE_DIRECTION_FINDING";
            case ENABLE_BLE_DIR_FINDING_TIMEOUT:
                return "ENABLE_BLE_DIR_FINDING_TIMEOUT";
            default:
                return "UNKNOWN(" + what + ")";
        }
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

    @Override
    protected void log(String msg) {
        if (DBG) {
            super.log(msg);
        }
    }
}

