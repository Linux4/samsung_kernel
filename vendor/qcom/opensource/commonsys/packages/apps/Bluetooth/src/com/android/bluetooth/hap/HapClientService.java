/*
 * Copyright 2021 HIMSA II K/S - www.himsa.com.
 * Represented by EHIMA - www.ehima.com
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

package com.android.bluetooth.hap;

import static android.Manifest.permission.BLUETOOTH_CONNECT;
import static android.Manifest.permission.BLUETOOTH_PRIVILEGED;

import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;

import android.annotation.Nullable;
import android.bluetooth.BluetoothCsipSetCoordinator;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHapClient;
import android.bluetooth.BluetoothHapPresetInfo;
import android.bluetooth.BluetoothLeAudio;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.IBluetoothHapClient;
import android.bluetooth.IBluetoothHapClientCallback;
import android.content.AttributionSource;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.HandlerThread;
import android.os.ParcelUuid;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.sysprop.BluetoothProperties;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.ServiceFactory;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.bluetooth.csip.CsipSetCoordinatorService;
import com.android.internal.annotations.VisibleForTesting;
import com.android.modules.utils.SynchronousResultReceiver;

import java.math.BigInteger;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Objects;

/**
 * Provides Bluetooth Hearing Access profile, as a service.
 * @hide
 */
public class HapClientService extends ProfileService {
    private static final boolean DBG = true;
    private static final String TAG = "HapClientService";

    // Upper limit of all HearingAccess devices: Bonded or Connected
    private static final int MAX_HEARING_ACCESS_STATE_MACHINES = 10;
    private static final int SM_THREAD_JOIN_TIMEOUT_MS = 1000;
    private static HapClientService sHapClient;
    private final Map<BluetoothDevice, HapClientStateMachine> mStateMachines =
            new HashMap<>();
    @VisibleForTesting
    HapClientNativeInterface mHapClientNativeInterface;
    private AdapterService mAdapterService;
    private DatabaseManager mDatabaseManager;
    private HandlerThread mStateMachinesThread;
    private BroadcastReceiver mBondStateChangedReceiver;
    private BroadcastReceiver mConnectionStateChangedReceiver;

    private final Map<BluetoothDevice, Integer> mDeviceCurrentPresetMap = new HashMap<>();
    private final Map<BluetoothDevice, Integer> mDeviceFeaturesMap = new HashMap<>();
    private final Map<BluetoothDevice, List<BluetoothHapPresetInfo>> mPresetsMap =
            new HashMap<>();

    @VisibleForTesting
    RemoteCallbackList<IBluetoothHapClientCallback> mCallbacks;

    @VisibleForTesting
    ServiceFactory mFactory = new ServiceFactory();

    public static boolean isEnabled() {
        return BluetoothProperties.isProfileHapClientEnabled().orElse(false);
    }

    private static synchronized void setHapClient(HapClientService instance) {
        if (DBG) {
            Log.d(TAG, "setHapClient(): set to: " + instance);
        }
        sHapClient = instance;
    }

    /**
     * Get the HapClientService instance
     * @return HapClientService instance
     */
    public static synchronized HapClientService getHapClientService() {
        if (sHapClient == null) {
            Log.w(TAG, "getHapClientService(): service is NULL");
            return null;
        }

        if (!sHapClient.isAvailable()) {
            Log.w(TAG, "getHapClientService(): service is not available");
            return null;
        }
        return sHapClient;
    }

    @Override
    protected void create() {
        if (DBG) {
            Log.d(TAG, "create()");
        }
    }

    @Override
    protected void cleanup() {
        if (DBG) {
            Log.d(TAG, "cleanup()");
        }
    }

    @Override
    protected IProfileServiceBinder initBinder() {
        return new BluetoothHapClientBinder(this);
    }

    @Override
    protected boolean start() {
        if (DBG) {
            Log.d(TAG, "start()");
        }

        if (sHapClient != null) {
            throw new IllegalStateException("start() called twice");
        }

        // Get AdapterService, HapClientNativeInterface, DatabaseManager, AudioManager.
        // None of them can be null.
        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService(),
                "AdapterService cannot be null when HapClientService starts");
        mDatabaseManager = Objects.requireNonNull(mAdapterService.getDatabase(),
                "DatabaseManager cannot be null when HapClientService starts");
        mHapClientNativeInterface = Objects.requireNonNull(
                HapClientNativeInterface.getInstance(),
                "HapClientNativeInterface cannot be null when HapClientService starts");

        // Start handler thread for state machines
        mStateMachines.clear();
        mStateMachinesThread = new HandlerThread("HapClientService.StateMachines");
        mStateMachinesThread.start();

        // Setup broadcast receivers
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        mBondStateChangedReceiver = new BondStateChangedReceiver();
        registerReceiver(mBondStateChangedReceiver, filter);
        filter = new IntentFilter();
        filter.addAction(BluetoothHapClient.ACTION_HAP_CONNECTION_STATE_CHANGED);
        mConnectionStateChangedReceiver = new ConnectionStateChangedReceiver();
        registerReceiver(mConnectionStateChangedReceiver, filter, Context.RECEIVER_NOT_EXPORTED);

        mCallbacks = new RemoteCallbackList<IBluetoothHapClientCallback>();

        // Initialize native interface
        mHapClientNativeInterface.init();

        // Mark service as started
        setHapClient(this);

        return true;
    }

    @Override
    protected boolean stop() {
        if (DBG) {
            Log.d(TAG, "stop()");
        }
        if (sHapClient == null) {
            Log.w(TAG, "stop() called before start()");
            return true;
        }

        // Marks service as stopped
        setHapClient(null);

        // Unregister broadcast receivers
        unregisterReceiver(mBondStateChangedReceiver);
        mBondStateChangedReceiver = null;
        unregisterReceiver(mConnectionStateChangedReceiver);
        mConnectionStateChangedReceiver = null;

        // Destroy state machines and stop handler thread
        synchronized (mStateMachines) {
            for (HapClientStateMachine sm : mStateMachines.values()) {
                sm.doQuit();
                sm.cleanup();
            }
            mStateMachines.clear();
        }

        if (mStateMachinesThread != null) {
            try {
                mStateMachinesThread.quitSafely();
                mStateMachinesThread.join(SM_THREAD_JOIN_TIMEOUT_MS);
                mStateMachinesThread = null;
            } catch (InterruptedException e) {
                // Do not rethrow as we are shutting down anyway
            }
        }

        // Cleanup GATT interface
        mHapClientNativeInterface.cleanup();
        mHapClientNativeInterface = null;

        // Cleanup the internals
        mDeviceCurrentPresetMap.clear();
        mDeviceFeaturesMap.clear();
        mPresetsMap.clear();

        if (mCallbacks != null) {
            mCallbacks.kill();
        }

        // Clear AdapterService
        mAdapterService = null;

        return true;
    }

    @VisibleForTesting
    void bondStateChanged(BluetoothDevice device, int bondState) {
        if (DBG) {
            Log.d(TAG, "Bond state changed for device: " + device + " state: " + bondState);
        }

        // Remove state machine if the bonding for a device is removed
        if (bondState != BluetoothDevice.BOND_NONE) {
            return;
        }

        mDeviceCurrentPresetMap.remove(device);
        mDeviceFeaturesMap.remove(device);
        mPresetsMap.remove(device);

        synchronized (mStateMachines) {
            HapClientStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                return;
            }
            if (sm.getConnectionState() != BluetoothProfile.STATE_DISCONNECTED) {
                Log.i(TAG, "Disconnecting device because it was unbonded.");
                disconnect(device);
                return;
            }
            removeStateMachine(device);
        }
    }

    private void removeStateMachine(BluetoothDevice device) {
        synchronized (mStateMachines) {
            HapClientStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                Log.w(TAG, "removeStateMachine: device " + device
                        + " does not have a state machine");
                return;
            }
            Log.i(TAG, "removeStateMachine: removing state machine for device: " + device);
            sm.doQuit();
            sm.cleanup();
            mStateMachines.remove(device);
        }
    }

    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        enforceCallingOrSelfPermission(BLUETOOTH_CONNECT, "Need BLUETOOTH_CONNECT permission");
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if (states == null) {
            return devices;
        }
        final BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
        if (bondedDevices == null) {
            return devices;
        }
        synchronized (mStateMachines) {
            for (BluetoothDevice device : bondedDevices) {
                final ParcelUuid[] featureUuids = device.getUuids();
                if (!Utils.arrayContains(featureUuids, BluetoothUuid.HAS)) {
                    continue;
                }
                int connectionState = BluetoothProfile.STATE_DISCONNECTED;
                HapClientStateMachine sm = mStateMachines.get(device);
                if (sm != null) {
                    connectionState = sm.getConnectionState();
                }
                for (int state : states) {
                    if (connectionState == state) {
                        devices.add(device);
                        break;
                    }
                }
            }
            return devices;
        }
    }

    List<BluetoothDevice> getConnectedDevices() {
        synchronized (mStateMachines) {
            List<BluetoothDevice> devices = new ArrayList<>();
            for (HapClientStateMachine sm : mStateMachines.values()) {
                if (sm.isConnected()) {
                    devices.add(sm.getDevice());
                }
            }
            return devices;
        }
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
        enforceCallingOrSelfPermission(BLUETOOTH_CONNECT, "Need BLUETOOTH_CONNECT permission");
        synchronized (mStateMachines) {
            HapClientStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return sm.getConnectionState();
        }
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
     * @param device           the remote device
     * @param connectionPolicy is the connection policy to set to for this profile
     * @return true on success, otherwise false
     */
    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        enforceBluetoothPrivilegedPermission(this);
        if (DBG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }
        mDatabaseManager.setProfileConnectionPolicy(device, BluetoothProfile.HAP_CLIENT,
                        connectionPolicy);
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
        return mDatabaseManager.getProfileConnectionPolicy(device, BluetoothProfile.HAP_CLIENT);
    }

    /**
     * Check whether can connect to a peer device.
     * The check considers a number of factors during the evaluation.
     *
     * @param device the peer device to connect to
     * @return true if connection is allowed, otherwise false
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public boolean okToConnect(BluetoothDevice device) {
        // Check if this is an incoming connection in Quiet mode.
        if (mAdapterService.isQuietModeEnabled()) {
            Log.e(TAG, "okToConnect: cannot connect to " + device + " : quiet mode enabled");
            return false;
        }
        // Check connection policy and accept or reject the connection.
        int connectionPolicy = getConnectionPolicy(device);
        int bondState = mAdapterService.getBondState(device);
        // Allow this connection only if the device is bonded. Any attempt to connect while
        // bonding would potentially lead to an unauthorized connection.
        if (bondState != BluetoothDevice.BOND_BONDED) {
            Log.w(TAG, "okToConnect: return false, bondState=" + bondState);
            return false;
        } else if (connectionPolicy != BluetoothProfile.CONNECTION_POLICY_UNKNOWN
                && connectionPolicy != BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            // Otherwise, reject the connection if connectionPolicy is not valid.
            Log.w(TAG, "okToConnect: return false, connectionPolicy=" + connectionPolicy);
            return false;
        }
        return true;
    }

    @VisibleForTesting
    synchronized void connectionStateChanged(BluetoothDevice device, int fromState,
                                             int toState) {
        if ((device == null) || (fromState == toState)) {
            Log.e(TAG, "connectionStateChanged: unexpected invocation. device=" + device
                    + " fromState=" + fromState + " toState=" + toState);
            return;
        }

        // Check if the device is disconnected - if unbond, remove the state machine
        if (toState == BluetoothProfile.STATE_DISCONNECTED) {
            int bondState = mAdapterService.getBondState(device);
            if (bondState == BluetoothDevice.BOND_NONE) {
                if (DBG) {
                    Log.d(TAG, device + " is unbond. Remove state machine");
                }
                removeStateMachine(device);
            }
        }
    }

    /**
     * Connects the hearing access service client to the passed in device
     *
     * @param device is the device with which we will connect the hearing access service client
     * @return true if hearing access service client successfully connected, false otherwise
     */
    public boolean connect(BluetoothDevice device) {
        enforceBluetoothPrivilegedPermission(this);
        if (DBG) {
            Log.d(TAG, "connect(): " + device);
        }
        if (device == null) {
            return false;
        }

        if (getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            return false;
        }
        ParcelUuid[] featureUuids = mAdapterService.getRemoteUuids(device);
        if (!Utils.arrayContains(featureUuids, BluetoothUuid.HAS)) {
            Log.e(TAG, "Cannot connect to " + device
                    + " : Remote does not have Hearing Access Service UUID");
            return false;
        }
        synchronized (mStateMachines) {
            HapClientStateMachine smConnect = getOrCreateStateMachine(device);
            if (smConnect == null) {
                Log.e(TAG, "Cannot connect to " + device + " : no state machine");
            }
            smConnect.sendMessage(HapClientStateMachine.CONNECT);
        }

        return true;
    }

    /**
     * Disconnects hearing access service client for the passed in device
     *
     * @param device is the device with which we want to disconnect the hearing access service
     * client
     * @return true if hearing access service client successfully disconnected, false otherwise
     */
    public boolean disconnect(BluetoothDevice device) {
        enforceBluetoothPrivilegedPermission(this);
        if (DBG) {
            Log.d(TAG, "disconnect(): " + device);
        }
        if (device == null) {
            return false;
        }
        synchronized (mStateMachines) {
            HapClientStateMachine sm = mStateMachines.get(device);
            if (sm != null) {
                sm.sendMessage(HapClientStateMachine.DISCONNECT);
            }
        }

        return true;
    }

    private HapClientStateMachine getOrCreateStateMachine(BluetoothDevice device) {
        if (device == null) {
            Log.e(TAG, "getOrCreateStateMachine failed: device cannot be null");
            return null;
        }
        synchronized (mStateMachines) {
            HapClientStateMachine sm = mStateMachines.get(device);
            if (sm != null) {
                return sm;
            }
            // Limit the maximum number of state machines to avoid DoS attack
            if (mStateMachines.size() >= MAX_HEARING_ACCESS_STATE_MACHINES) {
                Log.e(TAG, "Maximum number of HearingAccess state machines reached: "
                        + MAX_HEARING_ACCESS_STATE_MACHINES);
                return null;
            }
            if (DBG) {
                Log.d(TAG, "Creating a new state machine for " + device);
            }
            sm = HapClientStateMachine.make(device, this,
                    mHapClientNativeInterface, mStateMachinesThread.getLooper());
            mStateMachines.put(device, sm);
            return sm;
        }
    }

    /**
     * Gets the hearing access device group of the passed device
     *
     * @param device is the device with which we want to get the group identifier for
     * @return group ID if device is part of the coordinated group, 0 otherwise
     */
    public int getHapGroup(BluetoothDevice device) {
        CsipSetCoordinatorService csipClient = mFactory.getCsipSetCoordinatorService();

        if (csipClient != null) {
            Map<Integer, ParcelUuid> groups = csipClient.getGroupUuidMapByDevice(device);
            for (Map.Entry<Integer, ParcelUuid> entry : groups.entrySet()) {
                if (entry.getValue().equals(BluetoothUuid.CAP)) {
                    return entry.getKey();
                }
            }
        }
        return BluetoothCsipSetCoordinator.GROUP_ID_INVALID;
    }

    /**
     * Gets the currently active preset index for a HA device
     *
     * @param device is the device for which we want to get the currently active preset
     * @return active preset index
     */
    public int getActivePresetIndex(BluetoothDevice device) {
        return mDeviceCurrentPresetMap.getOrDefault(device,
                BluetoothHapClient.PRESET_INDEX_UNAVAILABLE);
    }

    /**
     * Gets the currently active preset info for a HA device
     *
     * @param device is the device for which we want to get the currently active preset info
     * @return active preset info or null if not available
     */
    public @Nullable BluetoothHapPresetInfo getActivePresetInfo(BluetoothDevice device) {
        int index = getActivePresetIndex(device);
        if (index == BluetoothHapClient.PRESET_INDEX_UNAVAILABLE) return null;

        List<BluetoothHapPresetInfo> current_presets = mPresetsMap.get(device);
        if (current_presets != null) {
            for (BluetoothHapPresetInfo preset : current_presets) {
                if (preset.getIndex() == index) {
                    return preset;
                }
            }
        }

        return null;
    }

    /**
     * Selects the currently active preset for a HA device
     *
     * @param device is the device for which we want to set the active preset
     * @param presetIndex is an index of one of the available presets
     */
    public void selectPreset(BluetoothDevice device, int presetIndex) {
        if (presetIndex == BluetoothHapClient.PRESET_INDEX_UNAVAILABLE) {
            if (mCallbacks != null) {
                int n = mCallbacks.beginBroadcast();
                for (int i = 0; i < n; i++) {
                    try {
                        mCallbacks.getBroadcastItem(i).onPresetSelectionFailed(device,
                                BluetoothStatusCodes.ERROR_HAP_INVALID_PRESET_INDEX);
                    } catch (RemoteException e) {
                        continue;
                    }
                }
                mCallbacks.finishBroadcast();
            }
            return;
        }

        mHapClientNativeInterface.selectActivePreset(device, presetIndex);
    }

    /**
     * Selects the currently active preset for a HA device group.
     *
     * @param groupId is the device group identifier for which want to set the active preset
     * @param presetIndex is an index of one of the available presets
     */
    public void selectPresetForGroup(int groupId, int presetIndex) {
        int status = BluetoothStatusCodes.SUCCESS;

        if (!isGroupIdValid(groupId)) {
            status = BluetoothStatusCodes.ERROR_CSIP_INVALID_GROUP_ID;
        } else if (!isPresetIndexValid(groupId, presetIndex)) {
            status = BluetoothStatusCodes.ERROR_HAP_INVALID_PRESET_INDEX;
        }

        if (status != BluetoothStatusCodes.SUCCESS) {
            if (mCallbacks != null) {
                int n = mCallbacks.beginBroadcast();
                for (int i = 0; i < n; i++) {
                    try {
                        mCallbacks.getBroadcastItem(i)
                                .onPresetSelectionForGroupFailed(groupId, status);
                    } catch (RemoteException e) {
                        continue;
                    }
                }
                mCallbacks.finishBroadcast();
            }
            return;
        }

        mHapClientNativeInterface.groupSelectActivePreset(groupId, presetIndex);
    }

    /**
     * Sets the next preset as a currently active preset for a HA device
     *
     * @param device is the device for which we want to set the active preset
     */
    public void switchToNextPreset(BluetoothDevice device) {
        mHapClientNativeInterface.nextActivePreset(device);
    }

    /**
     * Sets the next preset as a currently active preset for a HA device group
     *
     * @param groupId is the device group identifier for which want to set the active preset
     */
    public void switchToNextPresetForGroup(int groupId) {
        mHapClientNativeInterface.groupNextActivePreset(groupId);
    }

    /**
     * Sets the previous preset as a currently active preset for a HA device
     *
     * @param device is the device for which we want to set the active preset
     */
    public void switchToPreviousPreset(BluetoothDevice device) {
        mHapClientNativeInterface.previousActivePreset(device);
    }

    /**
     * Sets the previous preset as a currently active preset for a HA device group
     *
     * @param groupId is the device group identifier for which want to set the active preset
     */
    public void switchToPreviousPresetForGroup(int groupId) {
        mHapClientNativeInterface.groupPreviousActivePreset(groupId);
    }

    /**
     * Requests the preset name
     *
     * @param device is the device for which we want to get the preset name
     * @param presetIndex is an index of one of the available presets
     * @return a preset Info corresponding to the requested preset index or null if not available
     */
    public @Nullable BluetoothHapPresetInfo getPresetInfo(BluetoothDevice device, int presetIndex) {
        BluetoothHapPresetInfo defaultValue = null;
        if (presetIndex == BluetoothHapClient.PRESET_INDEX_UNAVAILABLE) return defaultValue;

        if (Utils.isPtsTestMode()) {
            /* We want native to be called for PTS testing even we have all
             * the data in the cache here
             */
            mHapClientNativeInterface.getPresetInfo(device, presetIndex);
        }
        List<BluetoothHapPresetInfo> current_presets = mPresetsMap.get(device);
        if (current_presets != null) {
            for (BluetoothHapPresetInfo preset : current_presets) {
                if (preset.getIndex() == presetIndex) {
                    return preset;
                }
            }
        }

        return defaultValue;
    }

    /**
     * Requests all presets info
     *
     * @param device is the device for which we want to get all presets info
     * @return a list of all presets Info
     */
    public List<BluetoothHapPresetInfo> getAllPresetInfo(BluetoothDevice device) {
        if (mPresetsMap.containsKey(device)) {
            return mPresetsMap.get(device);
        }
        return Collections.emptyList();
    }

    /**
     * Requests features
     *
     * @param device is the device for which we want to get features
     * @return integer with feature bits set
     */
    public int getFeatures(BluetoothDevice device) {
        if (mDeviceFeaturesMap.containsKey(device)) {
            return mDeviceFeaturesMap.get(device);
        }
        return 0x00;
    }

    private int stackEventPresetInfoReasonToProfileStatus(int statusCode) {
        switch (statusCode) {
            case HapClientStackEvent.PRESET_INFO_REASON_ALL_PRESET_INFO:
                return BluetoothStatusCodes.REASON_LOCAL_STACK_REQUEST;
            case HapClientStackEvent.PRESET_INFO_REASON_PRESET_INFO_UPDATE:
                return BluetoothStatusCodes.REASON_REMOTE_REQUEST;
            case HapClientStackEvent.PRESET_INFO_REASON_PRESET_DELETED:
                return BluetoothStatusCodes.REASON_REMOTE_REQUEST;
            case HapClientStackEvent.PRESET_INFO_REASON_PRESET_AVAILABILITY_CHANGED:
                return BluetoothStatusCodes.REASON_REMOTE_REQUEST;
            case HapClientStackEvent.PRESET_INFO_REASON_PRESET_INFO_REQUEST_RESPONSE:
                return BluetoothStatusCodes.REASON_LOCAL_APP_REQUEST;
            default:
                return BluetoothStatusCodes.ERROR_UNKNOWN;
        }
    }

    private void notifyPresetInfoChanged(BluetoothDevice device, int infoReason) {
        List current_presets = mPresetsMap.get(device);
        if (current_presets == null) return;

        if (mCallbacks != null) {
            int n = mCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mCallbacks.getBroadcastItem(i).onPresetInfoChanged(device, current_presets,
                            stackEventPresetInfoReasonToProfileStatus(infoReason));
                } catch (RemoteException e) {
                    continue;
                }
            }
            mCallbacks.finishBroadcast();
        }
    }

    private void notifyPresetInfoForGroupChanged(int groupId, int infoReason) {
        List<BluetoothDevice> all_group_devices = getGroupDevices(groupId);
        for (BluetoothDevice dev : all_group_devices) {
            notifyPresetInfoChanged(dev, infoReason);
        }
    }

    private void notifyFeaturesAvailable(BluetoothDevice device, int features) {
        Log.d(TAG, "HAP device: " + device + ", features: " + String.format("0x%04X", features));
    }

    private void notifyActivePresetChanged(BluetoothDevice device, int presetIndex,
            int reasonCode) {
        if (mCallbacks != null) {
            int n = mCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mCallbacks.getBroadcastItem(i).onPresetSelected(device, presetIndex,
                            reasonCode);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mCallbacks.finishBroadcast();
        }
    }

    private void notifyActivePresetChangedForGroup(int groupId, int presetIndex, int reasonCode) {
        List<BluetoothDevice> all_group_devices = getGroupDevices(groupId);
        for (BluetoothDevice dev : all_group_devices) {
            notifyActivePresetChanged(dev, presetIndex, reasonCode);
        }
    }

    private int stackEventStatusToProfileStatus(int statusCode) {
        switch (statusCode) {
            case HapClientStackEvent.STATUS_SET_NAME_NOT_ALLOWED:
                return BluetoothStatusCodes.ERROR_REMOTE_OPERATION_REJECTED;
            case HapClientStackEvent.STATUS_OPERATION_NOT_SUPPORTED:
                return BluetoothStatusCodes.ERROR_REMOTE_OPERATION_NOT_SUPPORTED;
            case HapClientStackEvent.STATUS_OPERATION_NOT_POSSIBLE:
                return BluetoothStatusCodes.ERROR_REMOTE_OPERATION_REJECTED;
            case HapClientStackEvent.STATUS_INVALID_PRESET_NAME_LENGTH:
                return BluetoothStatusCodes.ERROR_HAP_PRESET_NAME_TOO_LONG;
            case HapClientStackEvent.STATUS_INVALID_PRESET_INDEX:
                return BluetoothStatusCodes.ERROR_HAP_INVALID_PRESET_INDEX;
            case HapClientStackEvent.STATUS_GROUP_OPERATION_NOT_SUPPORTED:
                return BluetoothStatusCodes.ERROR_REMOTE_OPERATION_NOT_SUPPORTED;
            case HapClientStackEvent.STATUS_PROCEDURE_ALREADY_IN_PROGRESS:
                return BluetoothStatusCodes.ERROR_UNKNOWN;
            default:
                return BluetoothStatusCodes.ERROR_UNKNOWN;
        }
    }

    private void notifySelectActivePresetFailed(BluetoothDevice device, int statusCode) {
        if (mCallbacks != null) {
            int n = mCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mCallbacks.getBroadcastItem(i).onPresetSelectionFailed(device,
                            stackEventStatusToProfileStatus(statusCode));
                } catch (RemoteException e) {
                    continue;
                }
            }
            mCallbacks.finishBroadcast();
        }
    }

    private void notifySelectActivePresetForGroupFailed(int groupId, int statusCode) {
        if (mCallbacks != null) {
            int n = mCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mCallbacks.getBroadcastItem(i).onPresetSelectionForGroupFailed(groupId,
                            stackEventStatusToProfileStatus(statusCode));
                } catch (RemoteException e) {
                    continue;
                }
            }
            mCallbacks.finishBroadcast();
        }
    }

    private void notifySetPresetNameFailed(BluetoothDevice device, int statusCode) {
        if (mCallbacks != null) {
            int n = mCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mCallbacks.getBroadcastItem(i).onSetPresetNameFailed(device,
                            stackEventStatusToProfileStatus(statusCode));
                } catch (RemoteException e) {
                    continue;
                }
            }
            mCallbacks.finishBroadcast();
        }
    }

    private void notifySetPresetNameForGroupFailed(int groupId, int statusCode) {
        if (mCallbacks != null) {
            int n = mCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mCallbacks.getBroadcastItem(i).onSetPresetNameForGroupFailed(groupId,
                            stackEventStatusToProfileStatus(statusCode));
                } catch (RemoteException e) {
                    continue;
                }
            }
            mCallbacks.finishBroadcast();
        }
    }

    private boolean isPresetIndexValid(BluetoothDevice device, int presetIndex) {
        if (presetIndex == BluetoothHapClient.PRESET_INDEX_UNAVAILABLE) return false;

        List<BluetoothHapPresetInfo> device_presets = mPresetsMap.get(device);
        if (device_presets != null) {
            for (BluetoothHapPresetInfo preset : device_presets) {
                if (preset.getIndex() == presetIndex) {
                    return true;
                }
            }
        }
        return false;
    }

    private boolean isPresetIndexValid(int groupId, int presetIndex) {
        List<BluetoothDevice> all_group_devices = getGroupDevices(groupId);
        if (all_group_devices.isEmpty()) return false;

        for (BluetoothDevice device : all_group_devices) {
            if (!isPresetIndexValid(device, presetIndex)) return false;
        }
        return true;
    }


    private boolean isGroupIdValid(int groupId) {
        if (groupId == BluetoothCsipSetCoordinator.GROUP_ID_INVALID) return false;

        CsipSetCoordinatorService csipClient = mFactory.getCsipSetCoordinatorService();
        if (csipClient != null) {
            List<Integer> groups = csipClient.getAllGroupIds(BluetoothUuid.CAP);
            return groups.contains(groupId);
        }
        return false;
    }

    /**
     * Sets the preset name
     *
     * @param device is the device for which we want to get the preset name
     * @param presetIndex is an index of one of the available presets
     * @param name is a new name for a preset
     */
    public void setPresetName(BluetoothDevice device, int presetIndex, String name) {
        if (!isPresetIndexValid(device, presetIndex)) {
            if (mCallbacks != null) {
                int n = mCallbacks.beginBroadcast();
                for (int i = 0; i < n; i++) {
                    try {
                        mCallbacks.getBroadcastItem(i).onSetPresetNameFailed(device,
                                BluetoothStatusCodes.ERROR_HAP_INVALID_PRESET_INDEX);
                    } catch (RemoteException e) {
                        continue;
                    }
                }
                mCallbacks.finishBroadcast();
            }
            return;
        }
        // WARNING: We should check cache if preset exists and is writable, but then we would still
        //          need a way to trigger this action with an invalid index or on a non-writable
        //          preset for tests purpose.
        mHapClientNativeInterface.setPresetName(device, presetIndex, name);
    }

    /**
     * Sets the preset name
     *
     * @param groupId is the device group identifier
     * @param presetIndex is an index of one of the available presets
     * @param name is a new name for a preset
     */
    public void setPresetNameForGroup(int groupId, int presetIndex, String name) {
        int status = BluetoothStatusCodes.SUCCESS;

        if (!isGroupIdValid(groupId)) {
            status = BluetoothStatusCodes.ERROR_CSIP_INVALID_GROUP_ID;
        } else if (!isPresetIndexValid(groupId, presetIndex)) {
            status = BluetoothStatusCodes.ERROR_HAP_INVALID_PRESET_INDEX;
        }
        if (status != BluetoothStatusCodes.SUCCESS) {
            if (mCallbacks != null) {
                int n = mCallbacks.beginBroadcast();
                for (int i = 0; i < n; i++) {
                    try {
                        mCallbacks.getBroadcastItem(i).onSetPresetNameForGroupFailed(groupId,
                                status);
                    } catch (RemoteException e) {
                        continue;
                    }
                }
                mCallbacks.finishBroadcast();
            }
            return;
        }

        mHapClientNativeInterface.groupSetPresetName(groupId, presetIndex, name);
    }

    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        for (HapClientStateMachine sm : mStateMachines.values()) {
            sm.dump(sb);
        }
    }

    private boolean isPresetCoordinationSupported(BluetoothDevice device) {
        Integer features = mDeviceFeaturesMap.getOrDefault(device, 0x00);
        return BigInteger.valueOf(features).testBit(
                HapClientStackEvent.FEATURE_BIT_NUM_SYNCHRONIZATED_PRESETS);
    }

    void updateDevicePresetsCache(BluetoothDevice device, int infoReason,
            List<BluetoothHapPresetInfo> presets) {
        switch (infoReason) {
            case HapClientStackEvent.PRESET_INFO_REASON_ALL_PRESET_INFO:
                mPresetsMap.put(device, presets);
                break;
            case HapClientStackEvent.PRESET_INFO_REASON_PRESET_INFO_UPDATE:
            case HapClientStackEvent.PRESET_INFO_REASON_PRESET_AVAILABILITY_CHANGED:
            case HapClientStackEvent.PRESET_INFO_REASON_PRESET_INFO_REQUEST_RESPONSE: {
                List current_presets = mPresetsMap.get(device);
                if (current_presets != null) {
                    ListIterator<BluetoothHapPresetInfo> iter = current_presets.listIterator();
                    for (BluetoothHapPresetInfo new_preset : presets) {
                        while (iter.hasNext()) {
                            if (iter.next().getIndex() == new_preset.getIndex()) {
                                iter.remove();
                            }
                        }
                    }
                    current_presets.addAll(presets);
                    mPresetsMap.put(device, current_presets);
                } else {
                    mPresetsMap.put(device, presets);
                }
            }
                break;

            case HapClientStackEvent.PRESET_INFO_REASON_PRESET_DELETED: {
                List current_presets = mPresetsMap.get(device);
                if (current_presets != null) {
                    ListIterator<BluetoothHapPresetInfo> iter = current_presets.listIterator();
                    for (BluetoothHapPresetInfo new_preset : presets) {
                        while (iter.hasNext()) {
                            if (iter.next().getIndex() == new_preset.getIndex()) {
                                iter.remove();
                            }
                        }
                    }
                    mPresetsMap.put(device, current_presets);
                }
            }
                break;

            default:
                break;
        }
    }

    private List<BluetoothDevice> getGroupDevices(int groupId) {
        List<BluetoothDevice> devices = new ArrayList<>();

        CsipSetCoordinatorService csipClient = mFactory.getCsipSetCoordinatorService();
        if (csipClient != null) {
            if (groupId != BluetoothLeAudio.GROUP_ID_INVALID) {
                //Phanee TODO:
                //devices = csipClient.getGroupUuidMapByDevice(groupId);
            }
        }
        return devices;
    }

    /**
     * Handle messages from native (JNI) to Java
     *
     * @param stackEvent the event that need to be handled
     */
    public void messageFromNative(HapClientStackEvent stackEvent) {
        // Decide which event should be sent to the state machine
        if (stackEvent.type == HapClientStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED) {
            resendToStateMachine(stackEvent);
            return;
        }

        Intent intent = null;
        BluetoothDevice device = stackEvent.device;

        switch (stackEvent.type) {
            case (HapClientStackEvent.EVENT_TYPE_DEVICE_AVAILABLE): {
                int features = stackEvent.valueInt1;

                if (device != null) {
                    mDeviceFeaturesMap.put(device, features);

                    intent = new Intent(BluetoothHapClient.ACTION_HAP_DEVICE_AVAILABLE);
                    intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
                    intent.putExtra(BluetoothHapClient.EXTRA_HAP_FEATURES, features);
                }
            } break;

            case (HapClientStackEvent.EVENT_TYPE_DEVICE_FEATURES): {
                int features = stackEvent.valueInt1;

                if (device != null) {
                    mDeviceFeaturesMap.put(device, features);
                    notifyFeaturesAvailable(device, features);
                }
            } return;

            case (HapClientStackEvent.EVENT_TYPE_ON_ACTIVE_PRESET_SELECTED): {
                int currentPresetIndex = stackEvent.valueInt1;
                int groupId = stackEvent.valueInt2;

                if (device != null) {
                    mDeviceCurrentPresetMap.put(device, currentPresetIndex);
                    // FIXME: Add app request queueing to support other reasons
                    int reasonCode = BluetoothStatusCodes.REASON_LOCAL_STACK_REQUEST;
                    notifyActivePresetChanged(device, currentPresetIndex, reasonCode);

                } else if (groupId != BluetoothCsipSetCoordinator.GROUP_ID_INVALID) {
                    List<BluetoothDevice> all_group_devices = getGroupDevices(groupId);
                    for (BluetoothDevice dev : all_group_devices) {
                        mDeviceCurrentPresetMap.put(dev, currentPresetIndex);
                    }
                    // FIXME: Add app request queueing to support other reasons
                    int reasonCode = BluetoothStatusCodes.REASON_LOCAL_STACK_REQUEST;
                    notifyActivePresetChangedForGroup(groupId, currentPresetIndex, reasonCode);
                }
            } return;

            case (HapClientStackEvent.EVENT_TYPE_ON_ACTIVE_PRESET_SELECT_ERROR): {
                int groupId = stackEvent.valueInt2;
                int statusCode = stackEvent.valueInt1;

                if (device != null) {
                    notifySelectActivePresetFailed(device, statusCode);
                } else if (groupId != BluetoothCsipSetCoordinator.GROUP_ID_INVALID) {
                    notifySelectActivePresetForGroupFailed(groupId, statusCode);
                }
            } break;

            case (HapClientStackEvent.EVENT_TYPE_ON_PRESET_INFO): {
                int presetIndex = stackEvent.valueInt1;
                int infoReason = stackEvent.valueInt2;
                int groupId = stackEvent.valueInt3;
                ArrayList presets = stackEvent.valueList;

                if (device != null) {
                    updateDevicePresetsCache(device, infoReason, presets);
                    notifyPresetInfoChanged(device, infoReason);

                } else if (groupId != BluetoothCsipSetCoordinator.GROUP_ID_INVALID) {
                    List<BluetoothDevice> all_group_devices = getGroupDevices(groupId);
                    for (BluetoothDevice dev : all_group_devices) {
                        updateDevicePresetsCache(dev, infoReason, presets);
                    }
                    notifyPresetInfoForGroupChanged(groupId, infoReason);
                }

            } return;

            case (HapClientStackEvent.EVENT_TYPE_ON_PRESET_NAME_SET_ERROR): {
                int statusCode = stackEvent.valueInt1;
                int presetIndex = stackEvent.valueInt2;
                int groupId = stackEvent.valueInt3;

                if (device != null) {
                    notifySetPresetNameFailed(device, statusCode);
                } else if (groupId != BluetoothCsipSetCoordinator.GROUP_ID_INVALID) {
                    notifySetPresetNameForGroupFailed(groupId, statusCode);
                }
            } break;

            case (HapClientStackEvent.EVENT_TYPE_ON_PRESET_INFO_ERROR): {
                // Used only to report back on hidden API calls used for testing.
                Log.d(TAG, stackEvent.toString());
            } break;

            default:
                return;
        }

        if (intent != null) {
            sendBroadcast(intent, BLUETOOTH_PRIVILEGED);
        }
    }

    private void resendToStateMachine(HapClientStackEvent stackEvent) {
        synchronized (mStateMachines) {
            BluetoothDevice device = stackEvent.device;
            HapClientStateMachine sm = mStateMachines.get(device);

            if (sm == null) {
                if (stackEvent.type == HapClientStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED) {
                    switch (stackEvent.valueInt1) {
                        case HapClientStackEvent.CONNECTION_STATE_CONNECTED:
                        case HapClientStackEvent.CONNECTION_STATE_CONNECTING:
                            sm = getOrCreateStateMachine(device);
                            break;
                        default:
                            break;
                    }
                }
            }
            if (sm == null) {
                Log.e(TAG, "Cannot process stack event: no state machine: " + stackEvent);
                return;
            }
            sm.sendMessage(HapClientStateMachine.STACK_EVENT, stackEvent);
        }
    }

    /**
     * Binder object: must be a static class or memory leak may occur
     */
    @VisibleForTesting
    static class BluetoothHapClientBinder extends IBluetoothHapClient.Stub
            implements IProfileServiceBinder {
        @VisibleForTesting
        boolean mIsTesting = false;
        private HapClientService mService;

        BluetoothHapClientBinder(HapClientService svc) {
            mService = svc;
        }

        private HapClientService getService(AttributionSource source) {
            if (mIsTesting) {
                return mService;
            }
            if (!Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkCallerIsSystemOrActiveOrManagedUser(mService, TAG)
                    || !Utils.checkConnectPermissionForDataDelivery(mService, source, TAG)) {
                Log.w(TAG, "Hearing Access call not allowed for non-active user");
                return null;
            }

            if (mService != null && mService.isAvailable()) {
                return mService;
            }
            return null;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public void getConnectedDevices(AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                List<BluetoothDevice> defaultValue = new ArrayList<>();
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getConnectedDevices();
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getDevicesMatchingConnectionStates(int[] states,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                List<BluetoothDevice> defaultValue = new ArrayList<>();
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getDevicesMatchingConnectionStates(states);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectionState(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                int defaultValue = BluetoothProfile.STATE_DISCONNECTED;
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getConnectionState(device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setConnectionPolicy(BluetoothDevice device, int connectionPolicy,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                boolean defaultValue = false;
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.setConnectionPolicy(device, connectionPolicy);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectionPolicy(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                int defaultValue = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getConnectionPolicy(device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getActivePresetIndex(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                int defaultValue = BluetoothHapClient.PRESET_INDEX_UNAVAILABLE;
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getActivePresetIndex(device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getActivePresetInfo(BluetoothDevice device,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                BluetoothHapPresetInfo defaultValue = null;
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getActivePresetInfo(device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getHapGroup(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                int defaultValue = BluetoothCsipSetCoordinator.GROUP_ID_INVALID;
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getHapGroup(device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void selectPreset(BluetoothDevice device, int presetIndex,
                AttributionSource source) {
            if (source == null) {
                Log.w(TAG, "source cannot be null");
                return;
            }

            HapClientService service = getService(source);
            if (service == null) {
                Log.w(TAG, "service is null");
                return;
            }
            enforceBluetoothPrivilegedPermission(service);
            service.selectPreset(device, presetIndex);
        }

        @Override
        public void selectPresetForGroup(int groupId, int presetIndex, AttributionSource source) {
            if (source == null) {
                Log.w(TAG, "source cannot be null");
                return;
            }

            HapClientService service = getService(source);
            if (service == null) {
                Log.w(TAG, "service is null");
                return;
            }
            enforceBluetoothPrivilegedPermission(service);
            service.selectPresetForGroup(groupId, presetIndex);
        }

        @Override
        public void switchToNextPreset(BluetoothDevice device, AttributionSource source) {
            if (source == null) {
                Log.w(TAG, "source cannot be null");
                return;
            }

            HapClientService service = getService(source);
            if (service == null) {
                Log.w(TAG, "service is null");
                return;
            }
            enforceBluetoothPrivilegedPermission(service);
            service.switchToNextPreset(device);
        }

        @Override
        public void switchToNextPresetForGroup(int groupId, AttributionSource source) {
            if (source == null) {
                Log.w(TAG, "source cannot be null");
                return;
            }

            HapClientService service = getService(source);
            if (service == null) {
                Log.w(TAG, "service is null");
                return;
            }
            enforceBluetoothPrivilegedPermission(service);
            service.switchToNextPresetForGroup(groupId);
        }

        @Override
        public void switchToPreviousPreset(BluetoothDevice device, AttributionSource source) {
            if (source == null) {
                Log.w(TAG, "source cannot be null");
                return;
            }

            HapClientService service = getService(source);
            if (service == null) {
                Log.w(TAG, "service is null");
                return;
            }
            enforceBluetoothPrivilegedPermission(service);
            service.switchToPreviousPreset(device);
        }

        @Override
        public void switchToPreviousPresetForGroup(int groupId, AttributionSource source) {
            if (source == null) {
                Log.w(TAG, "source cannot be null");
                return;
            }

            HapClientService service = getService(source);
            if (service == null) {
                Log.w(TAG, "service is null");
                return;
            }
            enforceBluetoothPrivilegedPermission(service);
            service.switchToPreviousPresetForGroup(groupId);
        }

        @Override
        public void getPresetInfo(BluetoothDevice device, int presetIndex,
                AttributionSource source, SynchronousResultReceiver receiver) {

            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                BluetoothHapPresetInfo defaultValue = null;
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getPresetInfo(device, presetIndex);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getAllPresetInfo(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                List<BluetoothHapPresetInfo> defaultValue = new ArrayList<>();
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getAllPresetInfo(device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getFeatures(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                int defaultValue = 0x00;
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getFeatures(device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setPresetName(BluetoothDevice device, int presetIndex, String name,
                AttributionSource source) {
            if (device == null) {
                Log.w(TAG, "device cannot be null");
                return;
            }
            if (name == null) {
                Log.w(TAG, "name cannot be null");
                return;
            }
            if (source == null) {
                Log.w(TAG, "source cannot be null");
                return;
            }

            HapClientService service = getService(source);
            if (service == null) {
                Log.w(TAG, "service is null");
                return;
            }
            enforceBluetoothPrivilegedPermission(service);
            service.setPresetName(device, presetIndex, name);
        }

        @Override
        public void setPresetNameForGroup(int groupId, int presetIndex, String name,
                AttributionSource source) {
            if (name == null) {
                Log.w(TAG, "name cannot be null");
                return;
            }
            if (source == null) {
                Log.w(TAG, "source cannot be null");
                return;
            }
            HapClientService service = getService(source);
            if (service == null) {
                Log.w(TAG, "service is null");
                return;
            }
            enforceBluetoothPrivilegedPermission(service);
            service.setPresetNameForGroup(groupId, presetIndex, name);
        }

        @Override
        public void registerCallback(IBluetoothHapClientCallback callback,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(callback, "callback cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("Service is unavailable");
                }
                enforceBluetoothPrivilegedPermission(service);
                service.mCallbacks.register(callback);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void unregisterCallback(IBluetoothHapClientCallback callback,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(callback, "callback cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                HapClientService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("Service is unavailable");
                }
                enforceBluetoothPrivilegedPermission(service);
                service.mCallbacks.unregister(callback);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
    }

    // Remove state machine if the bonding for a device is removed
    private class BondStateChangedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(intent.getAction())) {
                return;
            }
            int state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                    BluetoothDevice.ERROR);
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            Objects.requireNonNull(device, "ACTION_BOND_STATE_CHANGED with no EXTRA_DEVICE");
            bondStateChanged(device, state);
        }
    }

    private class ConnectionStateChangedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!BluetoothHapClient.ACTION_HAP_CONNECTION_STATE_CHANGED.equals(
                    intent.getAction())) {
                return;
            }
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            int toState = intent.getIntExtra(BluetoothProfile.EXTRA_STATE, -1);
            int fromState = intent.getIntExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, -1);
            connectionStateChanged(device, fromState, toState);
        }
    }
}
