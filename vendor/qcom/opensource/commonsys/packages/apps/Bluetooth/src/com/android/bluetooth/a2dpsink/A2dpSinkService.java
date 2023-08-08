/*
 * Copyright (C) 2014 The Android Open Source Project
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
package com.android.bluetooth.a2dpsink;

import android.annotation.RequiresPermission;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothAudioConfig;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.IBluetoothA2dpSink;
import android.content.AttributionSource;
import android.util.Log;
import android.os.SystemProperties;


import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.internal.annotations.VisibleForTesting;
import com.android.bluetooth.avrcpcontroller.AvrcpControllerService;
import com.android.modules.utils.SynchronousResultReceiver;


import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Provides Bluetooth A2DP Sink profile, as a service in the Bluetooth application.
 * @hide
 */
public class A2dpSinkService extends ProfileService {
    private static final String TAG = "A2dpSinkService";
    private static final boolean DBG = true;
    //static final int MAX_ALLOWED_SINK_CONNECTIONS = 2;

    private final BluetoothAdapter mAdapter;
    private DatabaseManager mDatabaseManager;
    protected static Map<BluetoothDevice, A2dpSinkStateMachine> mDeviceStateMap =
            new ConcurrentHashMap<>(1);

    private  static final Object mBtA2dpLock = new Object();
    private static A2dpSinkStreamHandler mA2dpSinkStreamHandler;
    private static A2dpSinkService sService;
    protected static BluetoothDevice mStreamingDevice;

    private static int mMaxA2dpSinkConnections = 1;
    public static final int MAX_ALLOWED_SINK_CONNECTIONS = 2;
    static {
        classInitNative();
    }

    @Override
    protected boolean start() {
        mDatabaseManager = Objects.requireNonNull(AdapterService.getAdapterService().getDatabase(),
                "DatabaseManager cannot be null when A2dpSinkService starts");

        initNative();
        sService = this;
        mA2dpSinkStreamHandler = new A2dpSinkStreamHandler(this, this);
        mMaxA2dpSinkConnections = Math.min(
                SystemProperties.getInt("persist.vendor.bt.a2dp.sink_conn", 1),
                MAX_ALLOWED_SINK_CONNECTIONS);
        return true;
    }

    @Override
    protected boolean stop() {
        synchronized (mBtA2dpLock) {
             for (A2dpSinkStateMachine stateMachine : mDeviceStateMap.values()) {
                 stateMachine.quitNow();
            }
    }
        sService = null;
        return true;
    }

    public static A2dpSinkService getA2dpSinkService() {
        return sService;
    }

    public A2dpSinkService() {
        mAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    protected A2dpSinkStateMachine newStateMachine(BluetoothDevice device) {
        return new A2dpSinkStateMachine(device, this);
    }

    protected synchronized A2dpSinkStateMachine getStateMachine(BluetoothDevice device) {
        return mDeviceStateMap.get(device);
    }

    /**
     * Request audio focus such that the designated device can stream audio
     */
    public void requestAudioFocus(BluetoothDevice device, boolean request) {
        A2dpSinkStateMachine stateMachine = null;
        synchronized (mBtA2dpLock) {
            stateMachine = mDeviceStateMap.get(device);
            if (stateMachine == null) {
                return;
            }
        }
        mA2dpSinkStreamHandler.requestAudioFocus(request);
    }

    @Override
    protected IProfileServiceBinder initBinder() {
        return new A2dpSinkServiceBinder(this);
    }

    //Binder object: Must be static class or memory leak may occur
    private static class A2dpSinkServiceBinder extends IBluetoothA2dpSink.Stub
            implements IProfileServiceBinder {
        private A2dpSinkService mService;

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
        private A2dpSinkService getService(AttributionSource source) {
            if (!Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkCallerIsSystemOrActiveOrManagedUser(mService, TAG)
                    || !Utils.checkConnectPermissionForDataDelivery(mService, source, TAG)) {
                return null;
            }

            if (mService != null) {
                return mService;
            }
            return null;
        }

        A2dpSinkServiceBinder(A2dpSinkService svc) {
            mService = svc;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public void connect(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                A2dpSinkService service = getService(source);
                boolean result = false;
                if (service != null) {
                    result = service.connect(device);
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void disconnect(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                A2dpSinkService service = getService(source);
                boolean result = false;
                if (service != null) {
                    result = service.disconnect(device);
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectedDevices(AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                A2dpSinkService service = getService(source);
                List<BluetoothDevice> result = new ArrayList<BluetoothDevice>(0);
                if (service != null) {
                    result = service.getConnectedDevices();
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getDevicesMatchingConnectionStates(int[] states,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                A2dpSinkService service = getService(source);
                List<BluetoothDevice> result = new ArrayList<BluetoothDevice>(0);
                if (service != null) {
                    result = service.getDevicesMatchingConnectionStates(states);
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectionState(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                A2dpSinkService service = getService(source);
                int result = BluetoothProfile.STATE_DISCONNECTED;
                if (service != null) {
                    result = service.getConnectionState(device);
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setConnectionPolicy(BluetoothDevice device, int connectionPolicy,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                A2dpSinkService service = getService(source);
                boolean result = false;
                if (service != null) {
                    result = service.setConnectionPolicy(device, connectionPolicy);
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectionPolicy(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                A2dpSinkService service = getService(source);
                int result = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
                if (service != null) {
                    result = service.getConnectionPolicy(device);
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void isA2dpPlaying(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                A2dpSinkService service = getService(source);
                boolean result = false;
                if (service != null) {
                    result = service.isA2dpPlaying(device);
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getAudioConfig(BluetoothDevice device,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                A2dpSinkService service = getService(source);
                BluetoothAudioConfig result = null;
                if (service != null) {
                    result = service.getAudioConfig(device);
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
    }

    /* Generic Profile Code */

    /**
     * Connect the given Bluetooth device.
     *
     * @return true if connection is successful, false otherwise.
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_PRIVILEGED)
    public synchronized boolean connect(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (device == null) {
            throw new IllegalArgumentException("Null device");
        }
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, " connect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }
        if (getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.w(TAG, "Connection not allowed: <" + device.getAddress()
                    + "> is CONNECTION_POLICY_FORBIDDEN");
            return false;
        }
        A2dpSinkStateMachine stateMachine = getOrCreateStateMachine(device);
        if (stateMachine != null) {
            stateMachine.connect();
            return true;
        } else {
            // a state machine instance doesn't exist yet, and the max has been reached.
            Log.e(TAG, "Maxed out on the number of allowed MAP connections. "
                    + "Connect request rejected on " + device);
            return false;
        }
    }

    /**
     * Disconnect the given Bluetooth device.
     *
     * @return true if disconnect is successful, false otherwise.
     */
    public synchronized boolean disconnect(BluetoothDevice device) {
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "A2DP disconnect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }
        A2dpSinkStateMachine stateMachine = mDeviceStateMap.get(device);
        // a state machine instance doesn't exist. maybe it is already gone?
        if (stateMachine == null) {
            return false;
        }
        int connectionState = stateMachine.getState();
        if (connectionState == BluetoothProfile.STATE_DISCONNECTED
                || connectionState == BluetoothProfile.STATE_DISCONNECTING) {
            return false;
        }
        // upon completion of disconnect, the state machine will remove itself from the available
        // devices map
        stateMachine.disconnect();
        return true;
    }

    void removeStateMachine(A2dpSinkStateMachine stateMachine) {
        mDeviceStateMap.remove(stateMachine.getDevice());
    }

    public List<BluetoothDevice> getConnectedDevices() {
        return getDevicesMatchingConnectionStates(new int[]{BluetoothAdapter.STATE_CONNECTED});
    }

    protected A2dpSinkStateMachine getOrCreateStateMachine(BluetoothDevice device) {
        if (device == null) {
            Log.e(TAG, "getOrCreateStateMachine failed: device cannot be null");
            return null;
        }
        synchronized (mBtA2dpLock) {
            A2dpSinkStateMachine stateMachine = mDeviceStateMap.get(device);
            if (stateMachine == null) {
                // Limit the maximum number of state machines to avoid DoS
                if (mDeviceStateMap.size() >= mMaxA2dpSinkConnections) {
                    Log.e(TAG, "Maximum number of A2DP Sink Connections reached: "
                            + mMaxA2dpSinkConnections);
                    return null;
                }
                Log.d(TAG, "Creating a new state machine for " + device);
                stateMachine = newStateMachine(device);
                mDeviceStateMap.put(device, stateMachine);
                stateMachine.start();
            }
            return stateMachine;
       }
    }

    public static BluetoothDevice getCurrentStreamingDevice() {
        return mStreamingDevice;
    }

    public  void informTGStatePlaying(BluetoothDevice device, boolean isPlaying) {
        Log.d(TAG, "informTGStatePlaying: device: " + device
                + ", mStreamingDevice:" + mStreamingDevice);
        A2dpSinkStateMachine mStateMachine = null;
        synchronized (mBtA2dpLock) {
            mStateMachine = mDeviceStateMap.get(device);
            if (mStateMachine == null) {
                return;
            }
        }
        if (mStateMachine != null) {
            if (!isPlaying) {
                //mStateMachine.sendMessage(A2dpSinkStateMachine.EVENT_AVRCP_TG_PAUSE);
                mA2dpSinkStreamHandler.obtainMessage(
                    A2dpSinkStreamHandler.SRC_PAUSE).sendToTarget();
            } else {
                // Soft-Handoff from AVRCP Cmd (if received before AVDTP_START)
                initiateHandoffOperations(device);
                if (mStreamingDevice != null && !mStreamingDevice.equals(device)) {
                    Log.d(TAG, "updating streaming device after avrcp status command");
                    mStreamingDevice = device;
                }
                //mStateMachine.sendMessage(A2dpSinkStateMachine.EVENT_AVRCP_TG_PLAY);
                mA2dpSinkStreamHandler.obtainMessage(
                    A2dpSinkStreamHandler.SRC_PLAY).sendToTarget();
            }
        }
    }

    /* This API performs all the operations required for doing soft-Handoff */
    public synchronized  void initiateHandoffOperations(BluetoothDevice device) {
        if (mStreamingDevice != null && !mStreamingDevice.equals(device)) {
           Log.d(TAG, "Soft-Handoff. Prev Device:" + mStreamingDevice + ", New: " + device);

           for (A2dpSinkStateMachine otherSm: mDeviceStateMap.values()) {
               BluetoothDevice otherDevice = otherSm.getDevice();
               if (mStreamingDevice.equals(otherDevice)) {
                   Log.d(TAG, "Release Audio Focus for " + otherDevice);
                   mA2dpSinkStreamHandler.obtainMessage(
                    A2dpSinkStreamHandler.RELEASE_FOCUS).sendToTarget();
                   // Send Passthrough Command for PAUSE
                   AvrcpControllerService avrcpService =
                           AvrcpControllerService.getAvrcpControllerService();
                  avrcpService.sendPassThroughCommandNative(Utils.getByteAddress(mStreamingDevice),
                              AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE,
                               AvrcpControllerService.KEY_STATE_PRESSED);
                  avrcpService.sendPassThroughCommandNative(Utils.getByteAddress(mStreamingDevice),
                              AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE,
                                AvrcpControllerService.KEY_STATE_RELEASED);
                   /* set autoconnect priority of non-streaming device to PRIORITY_ON and priority
                    *  of streaming device to PRIORITY_AUTO_CONNECT */
                   avrcpService.onDeviceUpdated(device);
                   setConnectionPolicy(otherDevice, BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
                   setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
                   mDatabaseManager.setConnectionForA2dpSrc(device);
                   break;
               }
           }
       } else if (mStreamingDevice == null && device != null) {
           Log.d(TAG, "Prev Device: Null. New Streaming Device: " + device);
           // No Action Required
       }
    }

    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        if (DBG) Log.d(TAG, "getDevicesMatchingConnectionStates" + Arrays.toString(states));
        List<BluetoothDevice> deviceList = new ArrayList<>();
        Set<BluetoothDevice> bondedDevices = mAdapter.getBondedDevices();
        int connectionState;
        for (BluetoothDevice device : bondedDevices) {
            connectionState = getConnectionState(device);
            if (DBG) Log.d(TAG, "Device: " + device + "State: " + connectionState);
            for (int i = 0; i < states.length; i++) {
                if (connectionState == states[i]) {
                    deviceList.add(device);
                }
            }
        }
        if (DBG) Log.d(TAG, deviceList.toString());
        Log.d(TAG, "GetDevicesDone");
        return deviceList;
    }

    /**
     * Get the current connection state of the profile
     *
     * @param device is the remote bluetooth device
     * @return {@link BluetoothProfile#STATE_DISCONNECTED} if this profile is disconnected,
     * {@link BluetoothProfile#STATE_CONNECTING} if this profile is being connected,
     * {@link BluetoothProfile#STATE_CONNECTED} if this profile is connected, or
     * {@link BluetoothProfile#STATE_DISCONNECTING} if this profile is being disconnected
     */
    public synchronized int getConnectionState(BluetoothDevice device) {
        A2dpSinkStateMachine stateMachine = mDeviceStateMap.get(device);
        return (stateMachine == null) ? BluetoothProfile.STATE_DISCONNECTED
                : stateMachine.getState();
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
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_PRIVILEGED)
    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH_PRIVILEGED permission");
        if (DBG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }

        if (!mDatabaseManager.setProfileConnectionPolicy(device, BluetoothProfile.A2DP_SINK,
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
     * @param device the remote device
     * @return connection policy of the specified device
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_PRIVILEGED)
    public int getConnectionPolicy(BluetoothDevice device) {
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH_PRIVILEGED permission");
        return mDatabaseManager
                .getProfileConnectionPolicy(device, BluetoothProfile.A2DP_SINK);
    }


    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        ProfileService.println(sb, "Devices Tracked = " + mDeviceStateMap.size());
        for (A2dpSinkStateMachine stateMachine : mDeviceStateMap.values()) {
            ProfileService.println(sb,
                    "==== StateMachine for " + stateMachine.getDevice() + " ====");
            stateMachine.dump(sb);
        }
    }

    /**
     * Get the current Bluetooth Audio focus state
     *
     * @return focus
     */
    public static int getFocusState() {
        return sService.mA2dpSinkStreamHandler.getFocusState();
    }

    boolean isA2dpPlaying(BluetoothDevice device) {
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH_PRIVILEGED permission");
        Log.d(TAG, "isA2dpPlaying(" + device + ")");
        A2dpSinkStateMachine sm = null;
        synchronized (mBtA2dpLock) {
            sm = mDeviceStateMap.get(device);
            if (sm == null) {
                 return false;
            }
        }
        return mA2dpSinkStreamHandler.isPlaying();
    }

    BluetoothAudioConfig getAudioConfig(BluetoothDevice device) {
        A2dpSinkStateMachine stateMachine = mDeviceStateMap.get(device);
        // a state machine instance doesn't exist. maybe it is already gone?
        if (stateMachine == null) {
            return null;
        }
        return stateMachine.getAudioConfig();
    }

    /* JNI interfaces*/

    private static native void classInitNative();

    private native void initNative();

    private native void cleanupNative();

    native boolean connectA2dpNative(byte[] address);

    native boolean disconnectA2dpNative(byte[] address);

    /**
     * inform A2DP decoder of the current audio focus
     *
     * @param focusGranted
     */
    @VisibleForTesting
    public native void informAudioFocusStateNative(int focusGranted);

    /**
     * inform A2DP decoder the desired audio gain
     *
     * @param gain
     */
    @VisibleForTesting
    public native void informAudioTrackGainNative(float gain);

    private void onConnectionStateChanged(byte[] address, int state) {
        BluetoothDevice device = getDevice(address);
        Log.d(TAG, "onConnectionStateChanged. State = " + state + ", device:" + device
                + ", streaming:" + mStreamingDevice);

        StackEvent event = StackEvent.connectionStateChanged(getDevice(address), state);
        A2dpSinkStateMachine stateMachine = getOrCreateStateMachine(event.mDevice);

        if (stateMachine == null || device == null) {
            Log.e(TAG, "State Machine not found for device:" + device + ". Return.");
            return;
        }

        // If streaming device is disconnected, release audio focus and update mStreamingDevice
        if (state == BluetoothProfile.STATE_DISCONNECTED && device.equals(mStreamingDevice)) {
            Log.d(TAG, "Release Audio Focus for Streaming device: " + device);
            mA2dpSinkStreamHandler.obtainMessage(
                    A2dpSinkStreamHandler.RELEASE_FOCUS).sendToTarget();

            //While streaming device got diconnected,
            //make other device prority to auto connect, so that it would
            //get reconnect when BT turn off/On
            for (A2dpSinkStateMachine otherSm: mDeviceStateMap.values()) {
               BluetoothDevice otherDevice = otherSm.getDevice();
               if (otherDevice != null && !otherDevice.equals(mStreamingDevice)) {
                   Log.d(TAG, "Other connected device: " + otherDevice);
                   setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_UNKNOWN);
                   setConnectionPolicy(otherDevice, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
                   mDatabaseManager.setConnectionForA2dpSrc(otherDevice);
               }
            }
            mStreamingDevice = null;
        }

        // Intiate Handoff operations when state has been connectiond
        if (state == BluetoothProfile.STATE_CONNECTED) {
            if (mStreamingDevice != null && !mStreamingDevice.equals(device)) {
                Log.d(TAG, "current connected device: " + device + "is different from previous device");
                initiateHandoffOperations(device);
                mStreamingDevice = device;
            } else if (device != null) {
                mStreamingDevice = device;
            }
        }
        stateMachine.sendMessage(A2dpSinkStateMachine.STACK_EVENT, event);
    }

    private void onAudioStateChanged(byte[] address, int state) {
        BluetoothDevice device = getDevice(address);
        Log.d(TAG, "onAudioStateChanged. Audio State = " + state + ", device:" + device);

        A2dpSinkStateMachine stateMachine = mDeviceStateMap.get(device);
        if (stateMachine == null) {
            return;
        }

        if (state == StackEvent.AUDIO_STATE_STARTED) {
            initiateHandoffOperations(device);
            mStreamingDevice = device;
            mA2dpSinkStreamHandler.obtainMessage(
                    A2dpSinkStreamHandler.SRC_STR_START).sendToTarget();
        } else if (state == StackEvent.AUDIO_STATE_STOPPED) {
            mA2dpSinkStreamHandler.obtainMessage(
                    A2dpSinkStreamHandler.SRC_STR_STOP).sendToTarget();
        }
    }

    private void onAudioConfigChanged(byte[] address, int sampleRate, int channelCount) {
        BluetoothDevice device = getDevice(address);
        Log.d(TAG, "onAudioConfigChanged:- device:" + device + " samplerate:" + sampleRate
                + ", channelCount:" + channelCount);

        StackEvent event = StackEvent.audioConfigChanged(getDevice(address), sampleRate,
                channelCount);
        A2dpSinkStateMachine stateMachine = getOrCreateStateMachine(event.mDevice);
        stateMachine.sendMessage(A2dpSinkStateMachine.STACK_EVENT, event);
    }
}
