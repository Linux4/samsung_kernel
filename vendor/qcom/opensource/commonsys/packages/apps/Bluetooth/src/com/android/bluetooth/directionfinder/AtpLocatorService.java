/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 */

package com.android.bluetooth.directionfinder;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;

import android.annotation.RequiresPermission;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothLeDirectionFinder;
import android.bluetooth.IBluetoothLeDirectionFinder;
import android.bluetooth.IBluetoothLeDirectionFinderCallback;
import android.content.AttributionSource;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.sysprop.BluetoothProperties;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;
import com.android.internal.annotations.VisibleForTesting;
import com.android.modules.utils.SynchronousResultReceiver;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public class AtpLocatorService extends ProfileService {
    private static final boolean DBG = true;
    private static final String TAG = "AtpLocatorService";

    private static AtpLocatorService sAtpLocatorService;
    private AdapterService mAdapterService;

    private Map<BluetoothDevice, ArrayList<IBluetoothLeDirectionFinderCallback>> mAppCallbackMap =
            new HashMap<BluetoothDevice, ArrayList<IBluetoothLeDirectionFinderCallback>>();

    public static boolean isEnabled() {
        return BluetoothProperties.isProfileVcpControllerEnabled().orElse(false);
    }

    @Override
    protected IProfileServiceBinder initBinder() {
        return new BluetoothAtpLocatorBinder(this);
    }

    @Override
    protected void create() {
        if (DBG) {
            Log.d(TAG, "create()");
        }
    }

    @Override
    protected boolean start() {
        if (DBG) {
            Log.d(TAG, "start()");
        }
        if (sAtpLocatorService != null) {
            throw new IllegalStateException("start() called twice");
        }

        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService(),
                "AdapterService cannot be null when AtpLocatorService starts");

        // Mark service as started
        setAtpLocatorService(this);

        //Initialize ATP JNI, stack
        Log.d(TAG, "start(), Initializing ATP JNI and stack from service");
        AtpLocator mAtpLocator = AtpLocator.make(this);

        return true;
    }

    @Override
    protected boolean stop() {
        if (DBG) {
            Log.d(TAG, "stop()");
        }
        if (sAtpLocatorService == null) {
            Log.w(TAG, "stop() called before start()");
            return true;
        }

        AtpLocator mAtpLocator = AtpLocator.make(this);
        mAtpLocator.doQuit();

        // Mark service as stopped
        setAtpLocatorService(null);
        mAdapterService = null;

        return true;
    }

    @Override
    protected void cleanup() {
        if (DBG) {
            Log.d(TAG, "cleanup()");
        }
    }

    /**
     * Get the AtpLocatorService instance
     * @return AtpLocatorService instance
     */
    public static synchronized AtpLocatorService getAtpLocatorService() {
        if (DBG) {
            Log.d(TAG, "getAtpLocatorService()");
        }
        if (sAtpLocatorService == null) {
            Log.w(TAG, "getAtpLocatorService(): service is NULL");
            return null;
        }

        if (!sAtpLocatorService.isAvailable()) {
            Log.w(TAG, "getAtpLocatorService(): service is not available");
            return null;
        }
        return sAtpLocatorService;
    }

    private static synchronized void setAtpLocatorService(AtpLocatorService instance) {
        if (DBG) {
            Log.d(TAG, "setAtpLocatorService(): set to: " + instance);
        }
        sAtpLocatorService = instance;
    }

    /**
     * Establish ATP profile connection
     *
     * @param device to which ATP profile connection needs to be
     *        established.
     * @return true, if connect request is sent successfully,
     *         false, otherwise.
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    public boolean connect(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "connect(): " + device);
        }

        AtpLocator mAtpLocator = AtpLocator.make(this);
        return mAtpLocator.connect(device);
    }

    /**
     * Disconnect ATP profile connection
     *
     * @param device to which ATP profile connection needs to be
     *        disconnected.
     * @return true, if disconnect request is sent successfully,
     *         false, otherwise.
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    public boolean disconnect(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "disconnect(): " + device);
        }

        AtpLocator mAtpLocator = AtpLocator.make(this);
        return mAtpLocator.disconnect(device);
    }

    /**
     * Get Connected Devices
     *
     * @return List of connected devices
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    public List<BluetoothDevice> getConnectedDevices() {
        AtpLocator mAtpLocator = AtpLocator.make(this);
        return mAtpLocator.getConnectedDevices();
    }

    /**
     * Get devices matching connection states
     *
     * @param states array of states to match
     * @return List of connected devices which match the criteria
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if (states == null) {
            return devices;
        }

        AtpLocator mAtpLocator = AtpLocator.make(this);
        return mAtpLocator.getDevicesMatchingConnectionStates(states);
    }

    /**
     * Get the current connection state of the ATP
     *
     * @param device is the remote bluetooth device
     * @return {@link BluetoothProfile#STATE_DISCONNECTED} if ATP is disconnected,
     * {@link BluetoothProfile#STATE_CONNECTING} if ATP is being connected,
     * {@link BluetoothProfile#STATE_CONNECTED} if ATP is connected, or
     * {@link BluetoothProfile#STATE_DISCONNECTING} if ATP is being disconnected
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    public int getConnectionState(BluetoothDevice device) {
        AtpLocator mAtpLocator = AtpLocator.make(this);
        return mAtpLocator.getConnectionState(device);
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
     * @param device the remote device
     * @param connectionPolicy is the connection policy to set to for this profile
     * @return true on success, otherwise false
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        if (DBG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }
        mAdapterService.getDatabase()
                .setProfileConnectionPolicy(device, BluetoothProfile.LE_DIRECTION_FINDING,
                        connectionPolicy);
        if (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            connect(device);
        } else if (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            disconnect(device);
        }
        return true;
    }

    /**
     * Get connection policy
     *
     * @param device is the remote bluetooth device
     * @return Connection policy
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    public int getConnectionPolicy(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "Get connectionPolicy for device " + device);
        }
        return mAdapterService.getDatabase()
                .getProfileConnectionPolicy(device, BluetoothProfile.LE_DIRECTION_FINDING);
    }

    void registerAppCallback(BluetoothDevice device, IBluetoothLeDirectionFinderCallback cb) {
        if (DBG) {
            Log.d(TAG, "registerAppCallback");
        }

        ArrayList<IBluetoothLeDirectionFinderCallback> cbs = mAppCallbackMap.get(device);
        if (cbs == null) {
            Log.i(TAG, "registerAppCallback: entry doesn't exists");
            cbs = new ArrayList<IBluetoothLeDirectionFinderCallback>();
        }
        cbs.add(cb);
        mAppCallbackMap.put(device, cbs);

        AtpLocator mAtpLocator = AtpLocator.make(this);
        mAtpLocator.setAtpLocatorService(this);

        return;
    }

    void unregisterAppCallback(BluetoothDevice device, IBluetoothLeDirectionFinderCallback cb) {
        if (DBG) {
            Log.d(TAG, "unregisterAppCallback");
        }

        ArrayList<IBluetoothLeDirectionFinderCallback> cbs = mAppCallbackMap.get(device);
        if (cbs == null) {
            Log.i(TAG, "unregisterAppCallback: cb list is null");
            return;
        } else {
           boolean ret = cbs.remove(cb);
           Log.i(TAG, "unregisterAppCallback: ret value of removal from list:" + ret);
        }
        if (cbs.size() != 0) {
            mAppCallbackMap.replace(device, cbs);
        } else {
            Log.i(TAG, "unregisterAppCallback: Remove the complete entry");
            mAppCallbackMap.remove(device);
        }

        AtpLocator mAtpLocator = AtpLocator.make(this);
        mAtpLocator.setAtpLocatorService(null);
        return;
    }

    /**
     * {@hide}
     * Enable/Disable Ble Direction Finding to remote device
     *
     * @param device: remote device instance
     * @param samplingEnable: sampling Enable
     * @param slotDurations: slot Durations
     * @param enable: CTE enable
     * @param cteReqInt: CTE request interval
     * @param reqCteLen: Requested CTE length
     * @param reqCteType: Requested CTE Type
     * @return void
     */
    public void enableBleDirectionFinding(BluetoothDevice device, int samplingEnable,
            int slotDurations, int enable, int cteReqInt, int reqCteLen, int dirFindingType) {
        if (DBG) {
            Log.d(TAG, "enableBleDirectionFinding");
        }

        AtpLocator mAtpLocator = AtpLocator.make(this);
        mAtpLocator.enableBleDirectionFinding(device, samplingEnable, slotDurations,
                enable, cteReqInt, reqCteLen, dirFindingType);
    }

    /**
     * Binder object: must be a static class or memory leak may occur
     */
    @VisibleForTesting
    static class BluetoothAtpLocatorBinder extends IBluetoothLeDirectionFinder.Stub
            implements IProfileServiceBinder {
        private AtpLocatorService mService;

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
        private AtpLocatorService getService(AttributionSource source) {
            if (!Utils.checkCallerIsSystemOrActiveUser(TAG)
                    || !Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkConnectPermissionForDataDelivery(mService, source, TAG)) {
                return null;
            }
            return mService;
        }

        BluetoothAtpLocatorBinder(AtpLocatorService svc) {
            mService = svc;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public boolean connect(BluetoothDevice device, AttributionSource source) {
            AtpLocatorService service = getService(source);
            boolean defaultValue = false;
            if (service != null) {
                defaultValue = service.connect(device);
            }
            return defaultValue;
        }

        @Override
        public boolean disconnect(BluetoothDevice device, AttributionSource source) {
            AtpLocatorService service = getService(source);
            boolean defaultValue = false;
            if (service != null) {
                defaultValue = service.disconnect(device);
            }
            return defaultValue;
        }

        @Override
        public List<BluetoothDevice> getConnectedDevices(AttributionSource source) {
            AtpLocatorService service = getService(source);
            List<BluetoothDevice> defaultValue = new ArrayList<>();
            if (service != null) {
                defaultValue = service.getConnectedDevices();
            }
            return defaultValue;
        }

        @Override
        public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states,
                AttributionSource source) {
            AtpLocatorService service = getService(source);
            List<BluetoothDevice> defaultValue = new ArrayList<>();
            if (service != null) {
                defaultValue = service.getDevicesMatchingConnectionStates(states);
            }
            return defaultValue;
        }

        @Override
        public int getConnectionState(BluetoothDevice device, AttributionSource source) {
            AtpLocatorService service = getService(source);
            int defaultValue = BluetoothProfile.STATE_DISCONNECTED;
            if (service != null) {
                defaultValue = service.getConnectionState(device);
            }
            return defaultValue;
        }

        @Override
        public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy,
                AttributionSource source) {
            AtpLocatorService service = getService(source);
            boolean defaultValue = false;
            if (service != null) {
                defaultValue = service.setConnectionPolicy(device, connectionPolicy);
            }
            return defaultValue;
        }

        @Override
        public int getConnectionPolicy(BluetoothDevice device, AttributionSource source) {
            AtpLocatorService service = getService(source);
            int defaultValue = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
            if (service != null) {
                defaultValue = service.getConnectionPolicy(device);
            }
            return defaultValue;
        }

        @Override
        public void enableBleDirectionFinding(BluetoothDevice device, int samplingEnable,
                int slotDurations, int enable, int cteReqInt, int reqCteLen, int dirFindingType,
                AttributionSource source) {
            AtpLocatorService service = getService(source);
            if (service != null) {
                service.enableBleDirectionFinding(device, samplingEnable, slotDurations,
                        enable, cteReqInt, reqCteLen, dirFindingType);
            }
        }

        @Override
        public void registerAppCallback(BluetoothDevice device,
                IBluetoothLeDirectionFinderCallback callback,
                AttributionSource source) {
            AtpLocatorService service = getService(source);
            if (service == null ||
                    !Utils.checkConnectPermissionForDataDelivery(
                                    service, source, TAG)) {
                return;
            }
            service.registerAppCallback(device, callback);
        }

        @Override
        public void unregisterAppCallback(BluetoothDevice device,
                IBluetoothLeDirectionFinderCallback callback,
                AttributionSource source) {
            AtpLocatorService service = getService(source);
            if (service == null|| !Utils.checkConnectPermissionForDataDelivery(
                    service, source, TAG)) {
                return;
            }
            service.unregisterAppCallback(device, callback);
        }
    }

    /**
     * Enable/Disable Ble Direction Finding callback
     *
     * @param device is the remote device for ATP connection
     * @param status of enable/disable Ble Direction Finding
     * @return void
     */
    public void onEnableBleDirectionFinding(BluetoothDevice device, int status) {
        ArrayList<IBluetoothLeDirectionFinderCallback> cbs = mAppCallbackMap.get(device);
        if (cbs == null) {
            Log.e(TAG, "no App callback for this device" + device);
            return;
        }

        for (IBluetoothLeDirectionFinderCallback cb : cbs) {
            try {
                cb.onEnableBleDirectionFinding(device, status);
            } catch (RemoteException e)  {
                Log.e(TAG, "Exception while calling onEnableBleDirectionFinding");
            }
        }
    }

    /**
     * LE AoA results callback
     *
     * @param device is the remote device for ATP connection
     * @param status of LE AoA results received
     * @param azimuth value of node
     * @param azimuthUnc Uncertainty of azimuth result
     * @param elevation value fo node
     * @param elevationUnc Uncertainty of elevation result
     * @return void
     */
    public void onLeAoaResults(BluetoothDevice device, int status, double azimuth,
            int azimuthUnc, double elevation, int elevationUnc) {
        ArrayList<IBluetoothLeDirectionFinderCallback> cbs = mAppCallbackMap.get(device);
        if (cbs == null) {
            Log.e(TAG, "no App callback for this device" + device);
            return;
        }

        for (IBluetoothLeDirectionFinderCallback cb : cbs) {
            try {
                cb.onLeAoaResults(device, status, azimuth, azimuthUnc,
                                  elevation, elevationUnc);
            } catch (RemoteException e)  {
                Log.e(TAG, "Exception while calling onLeAoaResults");
            }
        }
    }

    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
    }
}
