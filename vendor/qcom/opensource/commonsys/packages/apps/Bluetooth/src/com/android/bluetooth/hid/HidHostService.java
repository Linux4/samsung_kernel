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

package com.android.bluetooth.hid;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;

import android.annotation.RequiresPermission;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHidHost;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.IBluetoothHidHost;
import android.content.AttributionSource;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.UserHandle;
import android.sysprop.BluetoothProperties;
import android.util.Log;
import androidx.annotation.VisibleForTesting;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.MetricsLogger;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.modules.utils.SynchronousResultReceiver;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Provides Bluetooth Hid Host profile, as a service in
 * the Bluetooth application.
 * @hide
 */
public class HidHostService extends ProfileService {
    private static final boolean DBG = true;
    private static final String TAG = "BluetoothHidHostService";

    private Map<BluetoothDevice, Integer> mInputDevices;
    private boolean mNativeAvailable;
    private static HidHostService sHidHostService;
    private BluetoothDevice mTargetDevice = null;

    private DatabaseManager mDatabaseManager;

    private static final int MESSAGE_CONNECT = 1;
    private static final int MESSAGE_DISCONNECT = 2;
    private static final int MESSAGE_CONNECT_STATE_CHANGED = 3;
    private static final int MESSAGE_GET_PROTOCOL_MODE = 4;
    private static final int MESSAGE_VIRTUAL_UNPLUG = 5;
    private static final int MESSAGE_ON_GET_PROTOCOL_MODE = 6;
    private static final int MESSAGE_SET_PROTOCOL_MODE = 7;
    private static final int MESSAGE_GET_REPORT = 8;
    private static final int MESSAGE_ON_GET_REPORT = 9;
    private static final int MESSAGE_SET_REPORT = 10;
    private static final int MESSAGE_ON_VIRTUAL_UNPLUG = 12;
    private static final int MESSAGE_ON_HANDSHAKE = 13;
    private static final int MESSAGE_GET_IDLE_TIME = 14;
    private static final int MESSAGE_ON_GET_IDLE_TIME = 15;
    private static final int MESSAGE_SET_IDLE_TIME = 16;

    static {
        classInitNative();
    }

    public static boolean isEnabled() {
        return BluetoothProperties.isProfileHidHostEnabled().orElse(false);
    }

    @Override
    public IProfileServiceBinder initBinder() {
        return new BluetoothHidHostBinder(this);
    }

    @Override
    protected boolean start() {
        mDatabaseManager = Objects.requireNonNull(AdapterService.getAdapterService().getDatabase(),
                "DatabaseManager cannot be null when HidHostService starts");

        mInputDevices = Collections.synchronizedMap(new HashMap<BluetoothDevice, Integer>());
        initializeNative();
        mNativeAvailable = true;
        setHidHostService(this);
        return true;
    }

    @Override
    protected boolean stop() {
        if (DBG) {
            Log.d(TAG, "Stopping Bluetooth HidHostService");
        }
        return true;
    }

    @Override
    protected void cleanup() {
        if (DBG) Log.d(TAG, "Stopping Bluetooth HidHostService");
        if (mNativeAvailable) {
            cleanupNative();
            mNativeAvailable = false;
        }

        if (mInputDevices != null) {
            for (BluetoothDevice device : mInputDevices.keySet()) {
                int inputDeviceState = getConnectionState(device);
                if (inputDeviceState != BluetoothProfile.STATE_DISCONNECTED) {
                    broadcastConnectionState(device, BluetoothProfile.STATE_DISCONNECTED);
                }
            }
            mInputDevices.clear();
        }
        // TODO(b/72948646): this should be moved to stop()
        setHidHostService(null);
    }

    public static synchronized HidHostService getHidHostService() {
        if (sHidHostService == null) {
            Log.w(TAG, "getHidHostService(): service is null");
            return null;
        }
        if (!sHidHostService.isAvailable()) {
            Log.w(TAG, "getHidHostService(): service is not available ");
            return null;
        }
        return sHidHostService;
    }

    private static synchronized void setHidHostService(HidHostService instance) {
        if (DBG) {
            Log.d(TAG, "setHidHostService(): set to: " + instance);
        }
        sHidHostService = instance;
    }

    private final Handler mHandler = new Handler() {

        @Override
        public void handleMessage(Message msg) {
            if (DBG) Log.v(TAG, "handleMessage(): msg.what=" + msg.what);

            switch (msg.what) {
                case MESSAGE_CONNECT: {
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    if (!connectHidNative(Utils.getByteAddress(device))) {
                        broadcastConnectionState(device, BluetoothProfile.STATE_DISCONNECTING);
                        broadcastConnectionState(device, BluetoothProfile.STATE_DISCONNECTED);
                        break;
                    }
                    mTargetDevice = device;
                }
                break;
                case MESSAGE_DISCONNECT: {
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    if (!disconnectHidNative(Utils.getByteAddress(device))) {
                        broadcastConnectionState(device, BluetoothProfile.STATE_DISCONNECTING);
                        broadcastConnectionState(device, BluetoothProfile.STATE_DISCONNECTED);
                        break;
                    }
                }
                break;
                case MESSAGE_CONNECT_STATE_CHANGED: {
                    BluetoothDevice device = getDevice((byte[]) msg.obj);
                    int halState = msg.arg1;
                    Integer prevStateInteger = mInputDevices.get(device);
                    int prevState =
                            (prevStateInteger == null) ? BluetoothHidHost.STATE_DISCONNECTED
                                    : prevStateInteger;
                    if (DBG) {
                        Log.d(TAG, "MESSAGE_CONNECT_STATE_CHANGED newState:" + convertHalState(
                                halState) + ", prevState:" + prevState);
                    }
                    if (halState == CONN_STATE_CONNECTED
                            && prevState == BluetoothHidHost.STATE_DISCONNECTED
                            && (!okToConnect(device))) {
                        if (DBG) {
                            Log.d(TAG, "Incoming HID connection rejected");
                        }
                        if (disconnectRemote(device)) {
                            disconnectHidNative(Utils.getByteAddress(device));
                        } else {
                            virtualUnPlugNative(Utils.getByteAddress(device));
                        }
                    } else {
                        broadcastConnectionState(device, convertHalState(halState));
                    }
                    if (halState == CONN_STATE_CONNECTED && (mTargetDevice != null
                            && mTargetDevice.equals(device))) {
                        mTargetDevice = null;
                        // local device originated connection to hid device, move out
                        // of quiet mode
                        AdapterService adapterService = AdapterService.getAdapterService();
                        adapterService.enable(false);
                    }
                }
                break;
                case MESSAGE_GET_PROTOCOL_MODE: {
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    if (!getProtocolModeNative(Utils.getByteAddress(device))) {
                        Log.e(TAG, "Error: get protocol mode native returns false");
                    }
                }
                break;

                case MESSAGE_ON_GET_PROTOCOL_MODE: {
                    BluetoothDevice device = getDevice((byte[]) msg.obj);
                    int protocolMode = msg.arg1;
                    broadcastProtocolMode(device, protocolMode);
                }
                break;
                case MESSAGE_VIRTUAL_UNPLUG: {
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    if (!virtualUnPlugNative(Utils.getByteAddress(device))) {
                        Log.e(TAG, "Error: virtual unplug native returns false");
                    }
                }
                break;
                case MESSAGE_SET_PROTOCOL_MODE: {
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    byte protocolMode = (byte) msg.arg1;
                    Log.d(TAG, "sending set protocol mode(" + protocolMode + ")");
                    if (!setProtocolModeNative(Utils.getByteAddress(device), protocolMode)) {
                        Log.e(TAG, "Error: set protocol mode native returns false");
                    }
                }
                break;
                case MESSAGE_GET_REPORT: {
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    Bundle data = msg.getData();
                    byte reportType = data.getByte(BluetoothHidHost.EXTRA_REPORT_TYPE);
                    byte reportId = data.getByte(BluetoothHidHost.EXTRA_REPORT_ID);
                    int bufferSize = data.getInt(BluetoothHidHost.EXTRA_REPORT_BUFFER_SIZE);
                    if (!getReportNative(Utils.getByteAddress(device), reportType, reportId,
                            bufferSize)) {
                        Log.e(TAG, "Error: get report native returns false");
                    }
                }
                break;
                case MESSAGE_ON_GET_REPORT: {
                    BluetoothDevice device = getDevice((byte[]) msg.obj);
                    Bundle data = msg.getData();
                    byte[] report = data.getByteArray(BluetoothHidHost.EXTRA_REPORT);
                    int bufferSize = data.getInt(BluetoothHidHost.EXTRA_REPORT_BUFFER_SIZE);
                    broadcastReport(device, report, bufferSize);
                }
                break;
                case MESSAGE_ON_HANDSHAKE: {
                    BluetoothDevice device = getDevice((byte[]) msg.obj);
                    int status = msg.arg1;
                    broadcastHandshake(device, status);
                }
                break;
                case MESSAGE_SET_REPORT: {
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    Bundle data = msg.getData();
                    byte reportType = data.getByte(BluetoothHidHost.EXTRA_REPORT_TYPE);
                    String report = data.getString(BluetoothHidHost.EXTRA_REPORT);
                    if (!setReportNative(Utils.getByteAddress(device), reportType, report)) {
                        Log.e(TAG, "Error: set report native returns false");
                    }
                }
                break;
                case MESSAGE_ON_VIRTUAL_UNPLUG: {
                    BluetoothDevice device = getDevice((byte[]) msg.obj);
                    int status = msg.arg1;
                    broadcastVirtualUnplugStatus(device, status);
                }
                break;
                case MESSAGE_GET_IDLE_TIME: {
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    if (!getIdleTimeNative(Utils.getByteAddress(device))) {
                        Log.e(TAG, "Error: get idle time native returns false");
                    }
                }
                break;
                case MESSAGE_ON_GET_IDLE_TIME: {
                    BluetoothDevice device = getDevice((byte[]) msg.obj);
                    int idleTime = msg.arg1;
                    broadcastIdleTime(device, idleTime);
                }
                break;
                case MESSAGE_SET_IDLE_TIME: {
                    BluetoothDevice device = (BluetoothDevice) msg.obj;
                    Bundle data = msg.getData();
                    byte idleTime = data.getByte(BluetoothHidHost.EXTRA_IDLE_TIME);
                    if (!setIdleTimeNative(Utils.getByteAddress(device), idleTime)) {
                        Log.e(TAG, "Error: get idle time native returns false");
                    }
                }
                break;
            }
        }
    };

    /**
     * Handlers for incoming service calls
     */
    private static class BluetoothHidHostBinder extends IBluetoothHidHost.Stub
            implements IProfileServiceBinder {
        private HidHostService mService;

        BluetoothHidHostBinder(HidHostService svc) {
            mService = svc;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
        private HidHostService getService(AttributionSource source) {
            if (!Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkCallerIsSystemOrActiveOrManagedUser(mService, TAG)
                    || !Utils.checkConnectPermissionForDataDelivery(mService, source, TAG)) {
                return null;
            }
            return mService;
        }

@Override
        public void connect(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    enforceBluetoothPrivilegedPermission(service);
                    defaultValue = service.connect(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void disconnect(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    enforceBluetoothPrivilegedPermission(service);
                    defaultValue = service.disconnect(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectionState(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                int defaultValue = BluetoothHidHost.STATE_DISCONNECTED;
                if (service != null) {
                    defaultValue = service.getConnectionState(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectedDevices(AttributionSource source,
                SynchronousResultReceiver receiver) {
            getDevicesMatchingConnectionStates(new int[] { BluetoothProfile.STATE_CONNECTED },
                    source, receiver);
        }

        @Override
        public void getDevicesMatchingConnectionStates(int[] states,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                List<BluetoothDevice> defaultValue = new ArrayList<BluetoothDevice>(0);
                if (service != null) {
                    defaultValue = service.getDevicesMatchingConnectionStates(states);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setConnectionPolicy(BluetoothDevice device, int connectionPolicy,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    enforceBluetoothPrivilegedPermission(service);
                    defaultValue = service.setConnectionPolicy(device, connectionPolicy);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectionPolicy(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                int defaultValue = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
                if (service != null) {
                    enforceBluetoothPrivilegedPermission(service);
                    defaultValue = service.getConnectionPolicy(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        /* The following APIs regarding test app for compliance */
        @Override
        public void getProtocolMode(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.getProtocolMode(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void virtualUnplug(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.virtualUnplug(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setProtocolMode(BluetoothDevice device, int protocolMode,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.setProtocolMode(device, protocolMode);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getReport(BluetoothDevice device, byte reportType, byte reportId,
                int bufferSize, AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.getReport(device, reportType, reportId, bufferSize);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setReport(BluetoothDevice device, byte reportType, String report,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.setReport(device, reportType, report);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void sendData(BluetoothDevice device, String report, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.sendData(device, report);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setIdleTime(BluetoothDevice device, byte idleTime,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.setIdleTime(device, idleTime);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getIdleTime(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                HidHostService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.getIdleTime(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
    }

    ;

    //APIs

    /**
     * Connects the hid host profile for the passed in device
     *
     * @param device is the device with which to connect the hid host profile
     * @return true if connection request is passed down to mHandler.
     */
    public boolean connect(BluetoothDevice device) {
        if (DBG) Log.d(TAG, "connect: " + device.getAddress());
        if (getConnectionState(device) != BluetoothHidHost.STATE_DISCONNECTED) {
            Log.e(TAG, "Hid Device not disconnected: " + device);
            return false;
        }
        if (getConnectionPolicy(device) == BluetoothHidHost.CONNECTION_POLICY_FORBIDDEN) {
            Log.e(TAG, "Hid Device CONNECTION_POLICY_FORBIDDEN: " + device);
            return false;
        }

        Message msg = mHandler.obtainMessage(MESSAGE_CONNECT, device);
        mHandler.sendMessage(msg);
        return true;
    }

    /**
     * Disconnects the hid host profile from the passed in device
     *
     * @param device is the device with which to disconnect the hid host profile
     * @return true
     */
    public boolean disconnect(BluetoothDevice device) {
        if (DBG) Log.d(TAG, "disconnect: " + device.getAddress());
        Message msg = mHandler.obtainMessage(MESSAGE_DISCONNECT, device);
        mHandler.sendMessage(msg);
        return true;
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
    public int getConnectionState(BluetoothDevice device) {
        if (DBG) Log.d(TAG, "getConnectionState: " + device.getAddress());
        if (mInputDevices.get(device) == null) {
            return BluetoothHidHost.STATE_DISCONNECTED;
        }
        return mInputDevices.get(device);
    }

    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        if (DBG) Log.d(TAG, "getDevicesMatchingConnectionStates()");
        List<BluetoothDevice> inputDevices = new ArrayList<BluetoothDevice>();

        for (BluetoothDevice device : mInputDevices.keySet()) {
            int inputDeviceState = getConnectionState(device);
            for (int state : states) {
                if (state == inputDeviceState) {
                    inputDevices.add(device);
                    break;
                }
            }
        }
        return inputDevices;
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
    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        if (DBG) {
            Log.d(TAG, "setConnectionPolicy: " + device.getAddress());
        }

        if (!mDatabaseManager.setProfileConnectionPolicy(device, BluetoothProfile.HID_HOST,
                  connectionPolicy)) {
            return false;
        }
        if (DBG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
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
        if (DBG) {
            Log.d(TAG, "getConnectionPolicy: " + device.getAddress());
        }
        return mDatabaseManager
                .getProfileConnectionPolicy(device, BluetoothProfile.HID_HOST);
    }

    /* The following APIs regarding test app for compliance */
    boolean getProtocolMode(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "getProtocolMode: " + device.getAddress());
        }
        int state = this.getConnectionState(device);
        if (state != BluetoothHidHost.STATE_CONNECTED) {
            return false;
        }
        Message msg = mHandler.obtainMessage(MESSAGE_GET_PROTOCOL_MODE, device);
        mHandler.sendMessage(msg);
        return true;
    }

    boolean virtualUnplug(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "virtualUnplug: " + device.getAddress());
        }
        int state = this.getConnectionState(device);
        if (state != BluetoothHidHost.STATE_CONNECTED) {
            return false;
        }
        Message msg = mHandler.obtainMessage(MESSAGE_VIRTUAL_UNPLUG, device);
        mHandler.sendMessage(msg);
        return true;
    }

    boolean setProtocolMode(BluetoothDevice device, int protocolMode) {
        if (DBG) {
            Log.d(TAG, "setProtocolMode: " + device.getAddress());
        }
        int state = this.getConnectionState(device);
        if (state != BluetoothHidHost.STATE_CONNECTED) {
            return false;
        }
        Message msg = mHandler.obtainMessage(MESSAGE_SET_PROTOCOL_MODE);
        msg.obj = device;
        msg.arg1 = protocolMode;
        mHandler.sendMessage(msg);
        return true;
    }

    boolean getReport(BluetoothDevice device, byte reportType, byte reportId, int bufferSize) {
        if (DBG) {
            Log.d(TAG, "getReport: " + device.getAddress());
        }
        int state = this.getConnectionState(device);
        if (state != BluetoothHidHost.STATE_CONNECTED) {
            return false;
        }
        Message msg = mHandler.obtainMessage(MESSAGE_GET_REPORT);
        msg.obj = device;
        Bundle data = new Bundle();
        data.putByte(BluetoothHidHost.EXTRA_REPORT_TYPE, reportType);
        data.putByte(BluetoothHidHost.EXTRA_REPORT_ID, reportId);
        data.putInt(BluetoothHidHost.EXTRA_REPORT_BUFFER_SIZE, bufferSize);
        msg.setData(data);
        mHandler.sendMessage(msg);
        return true;
    }

    boolean setReport(BluetoothDevice device, byte reportType, String report) {
        if (DBG) {
            Log.d(TAG, "setReport: " + device.getAddress());
        }
        int state = this.getConnectionState(device);
        if (state != BluetoothHidHost.STATE_CONNECTED) {
            return false;
        }
        Message msg = mHandler.obtainMessage(MESSAGE_SET_REPORT);
        msg.obj = device;
        Bundle data = new Bundle();
        data.putByte(BluetoothHidHost.EXTRA_REPORT_TYPE, reportType);
        data.putString(BluetoothHidHost.EXTRA_REPORT, report);
        msg.setData(data);
        mHandler.sendMessage(msg);
        return true;

    }

    boolean sendData(BluetoothDevice device, String report) {
        if (DBG) {
            Log.d(TAG, "sendData: " + device.getAddress());
        }
        int state = this.getConnectionState(device);
        if (state != BluetoothHidHost.STATE_CONNECTED) {
            return false;
        }

        return sendDataNative(Utils.getByteAddress(device), report);
    }

    boolean getIdleTime(BluetoothDevice device) {
        if (DBG) Log.d(TAG, "getIdleTime: " + device.getAddress());
        int state = this.getConnectionState(device);
        if (state != BluetoothHidHost.STATE_CONNECTED) {
            return false;
        }
        Message msg = mHandler.obtainMessage(MESSAGE_GET_IDLE_TIME, device);
        mHandler.sendMessage(msg);
        return true;
    }

    boolean setIdleTime(BluetoothDevice device, byte idleTime) {
        if (DBG) Log.d(TAG, "setIdleTime: " + device.getAddress());
        int state = this.getConnectionState(device);
        if (state != BluetoothHidHost.STATE_CONNECTED) {
            return false;
        }
        Message msg = mHandler.obtainMessage(MESSAGE_SET_IDLE_TIME);
        msg.obj = device;
        Bundle data = new Bundle();
        data.putByte(BluetoothHidHost.EXTRA_IDLE_TIME, idleTime);
        msg.setData(data);
        mHandler.sendMessage(msg);
        return true;
    }

    private void onGetProtocolMode(byte[] address, int mode) {
        if (DBG) Log.d(TAG, "onGetProtocolMode()");
        Message msg = mHandler.obtainMessage(MESSAGE_ON_GET_PROTOCOL_MODE);
        msg.obj = address;
        msg.arg1 = mode;
        mHandler.sendMessage(msg);
    }

    private void onGetIdleTime(byte[] address, int idleTime) {
        if (DBG) Log.d(TAG, "onGetIdleTime()");
        Message msg = mHandler.obtainMessage(MESSAGE_ON_GET_IDLE_TIME);
        msg.obj = address;
        msg.arg1 = idleTime;
        mHandler.sendMessage(msg);
    }

    private void onGetReport(byte[] address, byte[] report, int rptSize) {
        if (DBG) Log.d(TAG, "onGetReport()");
        Message msg = mHandler.obtainMessage(MESSAGE_ON_GET_REPORT);
        msg.obj = address;
        Bundle data = new Bundle();
        data.putByteArray(BluetoothHidHost.EXTRA_REPORT, report);
        data.putInt(BluetoothHidHost.EXTRA_REPORT_BUFFER_SIZE, rptSize);
        msg.setData(data);
        mHandler.sendMessage(msg);
    }

    private void onHandshake(byte[] address, int status) {
        if (DBG) Log.d(TAG, "onHandshake: status=" + status);
        Message msg = mHandler.obtainMessage(MESSAGE_ON_HANDSHAKE);
        msg.obj = address;
        msg.arg1 = status;
        mHandler.sendMessage(msg);
    }

    private void onVirtualUnplug(byte[] address, int status) {
        if (DBG) Log.d(TAG, "onVirtualUnplug: status=" + status);
        Message msg = mHandler.obtainMessage(MESSAGE_ON_VIRTUAL_UNPLUG);
        msg.obj = address;
        msg.arg1 = status;
        mHandler.sendMessage(msg);
    }

    private void onConnectStateChanged(byte[] address, int state) {
        if (DBG) Log.d(TAG, "onConnectStateChanged: state=" + state);
        Message msg = mHandler.obtainMessage(MESSAGE_CONNECT_STATE_CHANGED);
        msg.obj = address;
        msg.arg1 = state;
        mHandler.sendMessage(msg);
    }

    // This method does not check for error conditon (newState == prevState)
    private void broadcastConnectionState(BluetoothDevice device, int newState) {
        Integer prevStateInteger = mInputDevices.get(device);
        int prevState = (prevStateInteger == null) ? BluetoothHidHost.STATE_DISCONNECTED
                : prevStateInteger;
        if (prevState == newState) {
            Log.w(TAG, "no state change: " + newState);
            return;
        }
        if (newState == BluetoothProfile.STATE_CONNECTED) {
            MetricsLogger.logProfileConnectionEvent(BluetoothMetricsProto.ProfileId.HID_HOST);
        }
        mInputDevices.put(device, newState);

        /* Notifying the connection state change of the profile before sending the intent for
           connection state change, as it was causing a race condition, with the UI not being
           updated with the correct connection state. */
        Log.d(TAG, "Connection state " + device + ": " + prevState + "->" + newState);
        Intent intent = new Intent(BluetoothHidHost.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, prevState);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, newState);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        sendBroadcastAsUser(intent, UserHandle.ALL, BLUETOOTH_CONNECT,
                Utils.getTempAllowlistBroadcastOptions());
    }

    private void broadcastHandshake(BluetoothDevice device, int status) {
        Intent intent = new Intent(BluetoothHidHost.ACTION_HANDSHAKE);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothHidHost.EXTRA_STATUS, status);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        sendBroadcast(intent, BLUETOOTH_CONNECT, Utils.getTempAllowlistBroadcastOptions());
    }

    private void broadcastProtocolMode(BluetoothDevice device, int protocolMode) {
        Intent intent = new Intent(BluetoothHidHost.ACTION_PROTOCOL_MODE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothHidHost.EXTRA_PROTOCOL_MODE, protocolMode);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        sendBroadcast(intent, BLUETOOTH_CONNECT, Utils.getTempAllowlistBroadcastOptions());
        if (DBG) {
            Log.d(TAG, "Protocol Mode (" + device + "): " + protocolMode);
        }
    }

    private void broadcastReport(BluetoothDevice device, byte[] report, int rptSize) {
        Intent intent = new Intent(BluetoothHidHost.ACTION_REPORT);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothHidHost.EXTRA_REPORT, report);
        intent.putExtra(BluetoothHidHost.EXTRA_REPORT_BUFFER_SIZE, rptSize);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        sendBroadcast(intent, BLUETOOTH_CONNECT, Utils.getTempAllowlistBroadcastOptions());
    }

    private void broadcastVirtualUnplugStatus(BluetoothDevice device, int status) {
        Intent intent = new Intent(BluetoothHidHost.ACTION_VIRTUAL_UNPLUG_STATUS);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothHidHost.EXTRA_VIRTUAL_UNPLUG_STATUS, status);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        sendBroadcast(intent, BLUETOOTH_CONNECT, Utils.getTempAllowlistBroadcastOptions());
    }

    private void broadcastIdleTime(BluetoothDevice device, int idleTime) {
        Intent intent = new Intent(BluetoothHidHost.ACTION_IDLE_TIME_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothHidHost.EXTRA_IDLE_TIME, idleTime);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        sendBroadcast(intent, BLUETOOTH_CONNECT, Utils.getTempAllowlistBroadcastOptions());
        if (DBG) {
            Log.d(TAG, "Idle time (" + device + "): " + idleTime);
        }
    }

    /**
     * Check whether can connect to a peer device.
     * The check considers a number of factors during the evaluation.
     *
     * @param device the peer device to connect to
     * @return true if connection is allowed, otherwise false
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public boolean okToConnect(BluetoothDevice device) {
        AdapterService adapterService = AdapterService.getAdapterService();
        // Check if adapter service is null.
        if (adapterService == null) {
            Log.w(TAG, "okToConnect: adapter service is null");
            return false;
        }
        // Check if this is an incoming connection in Quiet mode.
        if (adapterService.isQuietModeEnabled() && mTargetDevice == null) {
            Log.w(TAG, "okToConnect: return false as quiet mode enabled");
            return false;
        }
        // Check priority and accept or reject the connection.
        int connectionPolicy = getConnectionPolicy(device);
        int bondState = adapterService.getBondState(device);
        // Allow this connection only if the device is bonded. Any attempt to connect while
        // bonding would potentially lead to an unauthorized connection.
        if (bondState == BluetoothDevice.BOND_NONE) {
            Log.w(TAG, "okToConnect: return false, bondState :" + bondState);
            return false;
        } else if (connectionPolicy != BluetoothProfile.CONNECTION_POLICY_UNKNOWN
                && connectionPolicy != BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            // Otherwise, reject the connection if connectionPolicy is not valid.
            Log.w(TAG, "okToConnect: return false, connectionPolicy=" + connectionPolicy);
            return false;
        }
        return true;
    }

    /**
     * Check whether need to disconnect or virtual unplug remote device
     * The check considers a number of factors during the evaluation.
     *
     * @param device the peer device to connect to
     * @return true if remote device is to be disconnected, otherwise remote
     * device needs to be virtually unplugged
     */
    private boolean disconnectRemote(BluetoothDevice device) {
        AdapterService adapterService = AdapterService.getAdapterService();
        // Check if adapter service is null.
        if (adapterService == null) {
            Log.w(TAG, "disconnectRemote: adapter service is null");
            return false;
        }
        // Check if this is an incoming connection in Quiet mode.
        if (adapterService.isQuietModeEnabled() && mTargetDevice == null) {
            Log.w(TAG, "disconnectRemote: return false as quiet mode enabled");
            return false;
        }
        int connectionPolicy = getConnectionPolicy(device);
        int bondState = adapterService.getBondState(device);
        // Disconnect remote device if bonded and connection policy is forbidden
        if (bondState == BluetoothDevice.BOND_BONDED &&
                connectionPolicy == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.w(TAG, "disconnectRemote: return true");
            return true;
        }
        return false;
    }

    private static int convertHalState(int halState) {
        switch (halState) {
            case CONN_STATE_CONNECTED:
                return BluetoothProfile.STATE_CONNECTED;
            case CONN_STATE_CONNECTING:
                return BluetoothProfile.STATE_CONNECTING;
            case CONN_STATE_DISCONNECTED:
                return BluetoothProfile.STATE_DISCONNECTED;
            case CONN_STATE_DISCONNECTING:
                return BluetoothProfile.STATE_DISCONNECTING;
            default:
                Log.e(TAG, "bad hid connection state: " + halState);
                return BluetoothProfile.STATE_DISCONNECTED;
        }
    }

    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        println(sb, "mTargetDevice: " + mTargetDevice);
        println(sb, "mInputDevices:");
        for (BluetoothDevice device : mInputDevices.keySet()) {
            println(sb, "  " + device + " : " + mInputDevices.get(device));
        }
    }

    // Constants matching Hal header file bt_hh.h
    // bthh_connection_state_t
    private static final int CONN_STATE_CONNECTED = 0;
    private static final int CONN_STATE_CONNECTING = 1;
    private static final int CONN_STATE_DISCONNECTED = 2;
    private static final int CONN_STATE_DISCONNECTING = 3;

    private static native void classInitNative();

    private native void initializeNative();

    private native void cleanupNative();

    private native boolean connectHidNative(byte[] btAddress);

    private native boolean disconnectHidNative(byte[] btAddress);

    private native boolean getProtocolModeNative(byte[] btAddress);

    private native boolean virtualUnPlugNative(byte[] btAddress);

    private native boolean setProtocolModeNative(byte[] btAddress, byte protocolMode);

    private native boolean getReportNative(byte[] btAddress, byte reportType, byte reportId,
            int bufferSize);

    private native boolean setReportNative(byte[] btAddress, byte reportType, String report);

    private native boolean sendDataNative(byte[] btAddress, String report);

    private native boolean setIdleTimeNative(byte[] btAddress, byte idleTime);

    private native boolean getIdleTimeNative(byte[] btAddress);
}
