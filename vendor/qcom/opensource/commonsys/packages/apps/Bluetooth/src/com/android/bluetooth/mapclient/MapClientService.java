/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
*/

package com.android.bluetooth.mapclient;

import android.Manifest;
import android.annotation.RequiresPermission;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.IBluetoothMapClient;
import android.bluetooth.SdpMasRecord;
import android.content.AttributionSource;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.ParcelUuid;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;

public class MapClientService extends ProfileService {
    private static final String TAG = "MapClientService";

    static final boolean DBG = true;
    static final boolean VDBG = false;

    static final int MAXIMUM_CONNECTED_DEVICES = 4;

    private Map<BluetoothDevice, MceStateMachine> mMapInstanceMap = new ConcurrentHashMap<>(1);
    private MnsService mMnsServer;

    private AdapterService mAdapterService;
    private DatabaseManager mDatabaseManager;
    private static MapClientService sMapClientService;
    private MapBroadcastReceiver mMapReceiver;

    public static synchronized MapClientService getMapClientService() {
        if (sMapClientService == null) {
            Log.w(TAG, "getMapClientService(): service is null");
            return null;
        }
        if (!sMapClientService.isAvailable()) {
            Log.w(TAG, "getMapClientService(): service is not available ");
            return null;
        }
        return sMapClientService;
    }

    private static synchronized void setMapClientService(MapClientService instance) {
        if (DBG) {
            Log.d(TAG, "setMapClientService(): set to: " + instance);
        }
        sMapClientService = instance;
    }

    @VisibleForTesting
    Map<BluetoothDevice, MceStateMachine> getInstanceMap() {
        return mMapInstanceMap;
    }

    /**
     * Connect the given Bluetooth device.
     *
     * @param device
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
            Log.d(TAG, "MAP connect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }
        if (getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.w(TAG, "Connection not allowed: <" + device.getAddress()
                    + "> is CONNECTION_POLICY_FORBIDDEN");
            return false;
        }
        MceStateMachine mapStateMachine = mMapInstanceMap.get(device);
        if (mapStateMachine == null) {
            // a map state machine instance doesn't exist yet, create a new one if we can.
            if (mMapInstanceMap.size() < MAXIMUM_CONNECTED_DEVICES) {
                addDeviceToMapAndConnect(device);
                return true;
            } else {
                // Maxed out on the number of allowed connections.
                // see if some of the current connections can be cleaned-up, to make room.
                removeUncleanAccounts();
                if (mMapInstanceMap.size() < MAXIMUM_CONNECTED_DEVICES) {
                    addDeviceToMapAndConnect(device);
                    return true;
                } else {
                    Log.e(TAG, "Maxed out on the number of allowed MAP connections. "
                            + "Connect request rejected on " + device);
                    return false;
                }
            }
        }

        // statemachine already exists in the map.
        int state = getConnectionState(device);
        if (state == BluetoothProfile.STATE_CONNECTED
                || state == BluetoothProfile.STATE_CONNECTING) {
            Log.w(TAG, "Received connect request while already connecting/connected.");
            return true;
        }

        // Statemachine exists but not in connecting or connected state! it should
        // have been removed form the map. lets get rid of it and add a new one.
        if (DBG) {
            Log.d(TAG, "Statemachine exists for a device in unexpected state: " + state);
        }
        mMapInstanceMap.remove(device);
        addDeviceToMapAndConnect(device);
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "MAP connect device: " + device
                    + ", InstanceMap end state: " + sb.toString());
        }
        return true;
    }

    private synchronized void addDeviceToMapAndConnect(BluetoothDevice device) {
        // When creating a new statemachine, its state is set to CONNECTING - which will trigger
        // connect.
        MceStateMachine mapStateMachine = new MceStateMachine(this, device);
        mMapInstanceMap.put(device, mapStateMachine);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_PRIVILEGED)
    public synchronized boolean disconnect(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "MAP disconnect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }
        MceStateMachine mapStateMachine = mMapInstanceMap.get(device);
        // a map state machine instance doesn't exist. maybe it is already gone?
        if (mapStateMachine == null) {
            return false;
        }
        int connectionState = mapStateMachine.getState();
        if (connectionState != BluetoothProfile.STATE_CONNECTED
                && connectionState != BluetoothProfile.STATE_CONNECTING) {
            return false;
        }
        mapStateMachine.disconnect();
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "MAP disconnect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }
        return true;
    }

    public List<BluetoothDevice> getConnectedDevices() {
        return getDevicesMatchingConnectionStates(new int[]{BluetoothAdapter.STATE_CONNECTED});
    }

    MceStateMachine getMceStateMachineForDevice(BluetoothDevice device) {
        return mMapInstanceMap.get(device);
    }

    public synchronized List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        if (DBG) Log.d(TAG, "getDevicesMatchingConnectionStates" + Arrays.toString(states));
        List<BluetoothDevice> deviceList = new ArrayList<>();
        BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
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
        return deviceList;
    }

    public synchronized int getConnectionState(BluetoothDevice device) {
        MceStateMachine mapStateMachine = mMapInstanceMap.get(device);
        // a map state machine instance doesn't exist yet, create a new one if we can.
        return (mapStateMachine == null) ? BluetoothProfile.STATE_DISCONNECTED
                : mapStateMachine.getState();
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
        if (VDBG) {
            Log.v(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");

        if (!mDatabaseManager.setProfileConnectionPolicy(device, BluetoothProfile.MAP_CLIENT,
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
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_PRIVILEGED)
    public int getConnectionPolicy(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        return mDatabaseManager
                .getProfileConnectionPolicy(device, BluetoothProfile.MAP_CLIENT);
    }

    public synchronized boolean sendMessage(BluetoothDevice device, Uri[] contacts, String message,
            PendingIntent sentIntent, PendingIntent deliveredIntent) {
        MceStateMachine mapStateMachine = mMapInstanceMap.get(device);
        return mapStateMachine != null
                && mapStateMachine.sendMapMessage(contacts, message, sentIntent, deliveredIntent);
    }

    @Override
    public IProfileServiceBinder initBinder() {
        return new Binder(this);
    }

    @Override
    protected synchronized boolean start() {
        Log.e(TAG, "start()");

        mAdapterService = AdapterService.getAdapterService();
        mDatabaseManager = Objects.requireNonNull(AdapterService.getAdapterService().getDatabase(),
                "DatabaseManager cannot be null when MapClientService starts");

        if (mMnsServer == null) {
            mMnsServer = MapUtils.newMnsServiceInstance(this);
            if (mMnsServer == null) {
                // this can't happen
                Log.w(TAG, "MnsService is *not* created!");
                return false;
            }
        }

        mMapReceiver = new MapBroadcastReceiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_SDP_RECORD);
        filter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
        registerReceiver(mMapReceiver, filter);
        removeUncleanAccounts();
        setMapClientService(this);
        return true;
    }

    @Override
    protected synchronized boolean stop() {
        if (DBG) {
            Log.d(TAG, "stop()");
        }

        if (mMapReceiver != null) {
            unregisterReceiver(mMapReceiver);
            mMapReceiver = null;
        }
        if (mMnsServer != null) {
            mMnsServer.stop();
        }
        for (MceStateMachine stateMachine : mMapInstanceMap.values()) {
            if (stateMachine.getState() == BluetoothAdapter.STATE_CONNECTED) {
                stateMachine.disconnect();
            }
            stateMachine.doQuit();
        }
        return true;
    }

    @Override
    protected void cleanup() {
        if (DBG) {
            Log.d(TAG, "in Cleanup");
        }
        removeUncleanAccounts();
        // TODO(b/72948646): should be moved to stop()
        setMapClientService(null);
    }

    /**
     * cleanupDevice removes the associated state machine from the instance map
     *
     * @param device BluetoothDevice address of remote device
     */
    @VisibleForTesting
    public void cleanupDevice(BluetoothDevice device) {
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "Cleanup device: " + device + ", InstanceMap start state: "
                    + sb.toString());
        }
        synchronized (mMapInstanceMap) {
            MceStateMachine stateMachine = mMapInstanceMap.get(device);
            if (stateMachine != null) {
                mMapInstanceMap.remove(device);
            }
        }
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "Cleanup device: " + device + ", InstanceMap end state: "
                    + sb.toString());
        }
    }

    @VisibleForTesting
    void removeUncleanAccounts() {
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "removeUncleanAccounts:InstanceMap end state: "
                    + sb.toString());
        }
        Iterator iterator = mMapInstanceMap.entrySet().iterator();
        while (iterator.hasNext()) {
            Map.Entry<BluetoothDevice, MceStateMachine> profileConnection =
                    (Map.Entry) iterator.next();
            if (profileConnection.getValue().getState() == BluetoothProfile.STATE_DISCONNECTED) {
                iterator.remove();
            }
        }
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "removeUncleanAccounts:InstanceMap end state: "
                    + sb.toString());
        }
    }

    public synchronized boolean getUnreadMessages(BluetoothDevice device) {
        MceStateMachine mapStateMachine = mMapInstanceMap.get(device);
        if (mapStateMachine == null) {
            return false;
        }
        return mapStateMachine.getUnreadMessages();
    }

    /**
     * Returns the SDP record's MapSupportedFeatures field (see Bluetooth MAP 1.4 spec, page 114).
     * @param device The Bluetooth device to get this value for.
     * @return the SDP record's MapSupportedFeatures field.
     */
    public synchronized int getSupportedFeatures(BluetoothDevice device) {
        MceStateMachine mapStateMachine = mMapInstanceMap.get(device);
        if (mapStateMachine == null) {
            if (DBG) Log.d(TAG, "in getSupportedFeatures, returning 0");
            return 0;
        }
        return mapStateMachine.getSupportedFeatures();
    }

    public synchronized boolean setMessageStatus(BluetoothDevice device, String handle, int status) {
        MceStateMachine mapStateMachine = mMapInstanceMap.get(device);
        if (mapStateMachine == null) {
            return false;
        }
        return mapStateMachine.setMessageStatus(handle, status);
    }
    public synchronized boolean sendImage(BluetoothDevice device, Uri[] contacts, String ImagePath,
            PendingIntent sentIntent, PendingIntent deliveredIntent) {
        MceStateMachine mapStateMachine = mMapInstanceMap.get(device);
         return mapStateMachine != null
                 && mapStateMachine.sendMapImageMessage(contacts, ImagePath, sentIntent, deliveredIntent);
    }

    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        for (MceStateMachine stateMachine : mMapInstanceMap.values()) {
            stateMachine.dump(sb);
        }
    }

    //Binder object: Must be static class or memory leak may occur

    /**
     * This class implements the IClient interface - or actually it validates the
     * preconditions for calling the actual functionality in the MapClientService, and calls it.
     */
    private static class Binder extends IBluetoothMapClient.Stub implements IProfileServiceBinder {
        private MapClientService mService;

        Binder(MapClientService service) {
            if (VDBG) {
                Log.v(TAG, "Binder()");
            }
            mService = service;
        }

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
        private MapClientService getService(AttributionSource source) {
            if (!(MapUtils.isSystemUser() || Utils.checkCallerIsSystemOrActiveUser(TAG))
                    || !Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkConnectPermissionForDataDelivery(mService, source, TAG)) {
                return null;
            }
            return mService;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public boolean isConnected(BluetoothDevice device, AttributionSource source) {
            if (VDBG) {
                Log.v(TAG, "isConnected()");
            }
            MapClientService service = getService(source);
            if (service == null) {
                return false;
            }
            return service.getConnectionState(device) == BluetoothProfile.STATE_CONNECTED;
        }

        @Override
        public boolean connect(BluetoothDevice device, AttributionSource source) {
            if (VDBG) {
                Log.v(TAG, "connect()");
            }
            MapClientService service = getService(source);
            if (service == null) {
                return false;
            }
            return service.connect(device);
        }

        @Override
        public boolean disconnect(BluetoothDevice device, AttributionSource source) {
            if (VDBG) {
                Log.v(TAG, "disconnect()");
            }
            MapClientService service = getService(source);
            if (service == null) {
                return false;
            }
            return service.disconnect(device);
        }

        @Override
        public List<BluetoothDevice> getConnectedDevices(AttributionSource source) {
            if (VDBG) {
                Log.v(TAG, "getConnectedDevices()");
            }
            MapClientService service = getService(source);
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getConnectedDevices();
        }

        @Override
        public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states,
                AttributionSource source) {
            if (VDBG) {
                Log.v(TAG, "getDevicesMatchingConnectionStates()");
            }
            MapClientService service = getService(source);
            if (service == null) {
                return new ArrayList<BluetoothDevice>(0);
            }
            return service.getDevicesMatchingConnectionStates(states);
        }

        @Override
        public int getConnectionState(BluetoothDevice device, AttributionSource source) {
            if (VDBG) {
                Log.v(TAG, "getConnectionState()");
            }
            MapClientService service = getService(source);
            if (service == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return service.getConnectionState(device);
        }

        @Override
        public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy,
                AttributionSource source) {
            MapClientService service = getService(source);
            if (service == null) {
                return false;
            }
            return service.setConnectionPolicy(device, connectionPolicy);
        }

        @Override
        public int getConnectionPolicy(BluetoothDevice device, AttributionSource source) {
            MapClientService service = getService(source);
            if (service == null) {
                return BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
            }
            return service.getConnectionPolicy(device);
        }

        @Override
        public boolean sendMessage(BluetoothDevice device, Uri[] contacts, String message,
                PendingIntent sentIntent, PendingIntent deliveredIntent, AttributionSource source) {
            MapClientService service = getService(source);
            if (service == null) {
                return false;
            }
            if (DBG) Log.d(TAG, "Checking Permission of sendMessage");
            mService.enforceCallingOrSelfPermission(Manifest.permission.SEND_SMS,
                    "Need SEND_SMS permission");

            return service.sendMessage(device, contacts, message, sentIntent, deliveredIntent);
        }

        @Override
        public boolean sendImage(BluetoothDevice device, Uri[] contacts, String ImagePath,
                PendingIntent sentIntent, PendingIntent deliveredIntent, AttributionSource source) {
            MapClientService service = getService(source);
            if (service == null) {
                return false;
            }
            if (DBG) Log.d(TAG, "Checking Permission of sendMessage");
            mService.enforceCallingOrSelfPermission(Manifest.permission.SEND_SMS,
                    "Need SEND_SMS permission");
            return service.sendImage(device, contacts, ImagePath, sentIntent, deliveredIntent);
        }

        @Override
        public boolean getUnreadMessages(BluetoothDevice device, AttributionSource source) {
            MapClientService service = getService(source);
            if (service == null) {
                return false;
            }
            mService.enforceCallingOrSelfPermission(Manifest.permission.READ_SMS,
                    "Need READ_SMS permission");
            return service.getUnreadMessages(device);
        }

        @Override
        public int getSupportedFeatures(BluetoothDevice device, AttributionSource source) {
            MapClientService service = getService(source);
            if (service == null) {
                if (DBG) {
                    Log.d(TAG,
                            "in MapClientService getSupportedFeatures stub, returning 0");
                }
                return 0;
            }
            return service.getSupportedFeatures(device);
        }

        @Override
        public boolean setMessageStatus(BluetoothDevice device, String handle, int status,
                AttributionSource source) {
            MapClientService service = getService(source);
            if (service == null) {
                return false;
            }
            mService.enforceCallingOrSelfPermission(Manifest.permission.READ_SMS,
                    "Need READ_SMS permission");
            return service.setMessageStatus(device, handle, status);
        }
    }

    private class MapBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (DBG) {
                Log.d(TAG, "onReceive: " + action);
            }
            if (!action.equals(BluetoothDevice.ACTION_ACL_DISCONNECTED)
                    && !action.equals(BluetoothDevice.ACTION_SDP_RECORD)) {
                // we don't care about this intent
                return;
            }
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if (device == null) {
                Log.e(TAG, "broadcast has NO device param!");
                return;
            }
            if (DBG) {
                Log.d(TAG, "broadcast has device: (" + device.getAddress() + ")");
            }
            MceStateMachine stateMachine = mMapInstanceMap.get(device);
            if (stateMachine == null) {
                Log.e(TAG, "No Statemachine found for the device from broadcast");
                return;
            }

            if (action.equals(BluetoothDevice.ACTION_ACL_DISCONNECTED)) {
                if (stateMachine.getState() == BluetoothProfile.STATE_CONNECTED) {
                    stateMachine.disconnect();
                }
            }

            if (action.equals(BluetoothDevice.ACTION_SDP_RECORD)) {
                ParcelUuid uuid = intent.getParcelableExtra(BluetoothDevice.EXTRA_UUID);
                if (DBG) {
                    Log.d(TAG, "UUID of SDP: " + uuid);
                }

                if (uuid.equals(BluetoothUuid.MAS)) {
                    // Check if we have a valid SDP record.
                    SdpMasRecord masRecord =
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_SDP_RECORD);
                    if (DBG) {
                        Log.d(TAG, "SDP = " + masRecord);
                    }
                    int status = intent.getIntExtra(BluetoothDevice.EXTRA_SDP_SEARCH_STATUS, -1);
                    if (masRecord == null) {
                        Log.w(TAG, "SDP search ended with no MAS record. Status: " + status);
                        return;
                    }
                    stateMachine.obtainMessage(MceStateMachine.MSG_MAS_SDP_DONE,
                            masRecord).sendToTarget();
                }
            }
        }
    }
}
