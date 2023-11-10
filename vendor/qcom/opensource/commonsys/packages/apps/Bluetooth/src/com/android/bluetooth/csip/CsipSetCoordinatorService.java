/******************************************************************************
 *  Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *      * Neither the name of The Linux Foundation nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 *  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

package com.android.bluetooth.csip;

import static android.Manifest.permission.BLUETOOTH_CONNECT;
import static android.Manifest.permission.BLUETOOTH_PRIVILEGED;
import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;
import android.annotation.CallbackExecutor;
import android.bluetooth.BluetoothDeviceGroup;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.DeviceGroup;
import android.bluetooth.IBluetoothDeviceGroup;
import android.bluetooth.IBluetoothGroupCallback;
import android.bluetooth.BluetoothGroupCallback;
import android.bluetooth.BluetoothCsipSetCoordinator;
import android.bluetooth.IBluetoothCsipSetCoordinator;
import android.bluetooth.IBluetoothCsipSetCoordinatorCallback;
import android.bluetooth.IBluetoothCsipSetCoordinatorLockCallback;
import android.content.AttributionSource;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.os.ParcelUuid;
import android.os.SystemProperties;
import android.annotation.NonNull;
import android.annotation.Nullable;
import com.android.internal.annotations.VisibleForTesting;
import com.android.modules.utils.SynchronousResultReceiver;

import android.util.Log;
import android.util.Pair;

import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.Config;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.ServiceFactory;
import com.android.bluetooth.btservice.ProfileService.IProfileServiceBinder;
import com.android.bluetooth.Utils;

import java.util.ArrayList;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Executor;
import java.util.stream.Collectors;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.UUID;

/**
 * Provides Bluetooth CSIP Client profile, as a service in the Bluetooth application.
 * @hide
 */
public class CsipSetCoordinatorService extends ProfileService {
    private static final boolean DBG = true;
    private static final String TAG = "CsipSetCoordinatorService";
    protected static final boolean VDBG = Log.isLoggable(TAG, Log.VERBOSE);

    private CsipSetCoordinatorScanner mGroupScanner;

    private static CsipSetCoordinatorService sCsipSetCoordinatorService;

    private static AdapterService mAdapterService;

    private CsipSetCoordinatorNativeInterface mGroupNativeInterface;

    private CsipSetCoordinatorAppMap mAppMap = new CsipSetCoordinatorAppMap();

    private static CopyOnWriteArrayList<DeviceGroup> mCoordinatedSets
            = new CopyOnWriteArrayList<DeviceGroup>();

    private static HashMap<Integer, ArrayList<BluetoothDevice>> sGroupIdToDeviceMap
            = new HashMap<>();
    private static HashMap<Integer, byte[]> setSirkMap = new HashMap<Integer, byte[]>();

    private final int INVALID_APP_ID = 0x10;
    private static final int INVALID_SET_ID = 0x10;
    private static final UUID EMPTY_UUID = UUID.fromString("00000000-0000-0000-0000-000000000000");

    /* parameters to hold details for ongoing set discovery and pending set discovery */
    private SetDiscoveryRequest mCurrentSetDisc = null;
    private SetDiscoveryRequest mPendingSetDisc = null;

    /* Constants for Coordinated set properties */
    private static final String SET_ID = "SET_ID";
    private static final String INCLUDING_SRVC = "INCLUDING_SRVC";
    private static final String SIZE = "SIZE";
    private static final String SIRK = "SIRK";
    private static final String LOCK_SUPPORT = "LOCK_SUPPORT";

    private static int mLocalAppId = -1;

    // Upper limit of all CSIP devices: Bonded or Connected
    private static final int MAX_CSIS_STATE_MACHINES = 10;

    private final Map<BluetoothDevice, CsipSetCoordinatorStateMachine> mStateMachines =
            new HashMap<>();

    private final Map<Integer, Pair<UUID, IBluetoothCsipSetCoordinatorLockCallback>> mLocks =
            new ConcurrentHashMap<>();

    private final Map<ParcelUuid, Map<Executor, IBluetoothCsipSetCoordinatorCallback>> mCallbacks =
            new HashMap<>();
    private final Map<Integer, ParcelUuid> mGroupIdToUuidMap = new HashMap<>();

    private final Map<BluetoothDevice, Set<Integer>> mDeviceGroupIdMap = new ConcurrentHashMap<>();

    private HandlerThread mStateMachinesThread;
    private BroadcastReceiver mConnectionStateChangedReceiver;

    private volatile CsipOptsHandler mHandler;
    private boolean mIsOptScanStarted = false;
    private final boolean OPT_SCAN_DISABLE = false;
    private final boolean OPT_SCAN_ENABLE = true;
    private final int MSG_START_STOP_OPTSCAN = 0;
    private final int MSG_RSI_DATA_FOUND = 1;

    private class SetDiscoveryRequest {
        private int mAppId = INVALID_APP_ID;
        private int mSetId = INVALID_SET_ID;
        private boolean mDiscInProgress = false;

        SetDiscoveryRequest() {
            mAppId = INVALID_APP_ID;
            mSetId = INVALID_SET_ID;
            mDiscInProgress = false;
        }

        SetDiscoveryRequest(int appId, int setId, boolean inProgress) {
            mAppId = appId;
            mSetId = setId;
            mDiscInProgress = inProgress;
        }
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action == null) {
                Log.e(TAG, "Received intent with null action");
                return;
            }

            switch (action) {
                case BluetoothDevice.ACTION_BOND_STATE_CHANGED:
                    BluetoothDevice device = intent.getParcelableExtra(
                            BluetoothDevice.EXTRA_DEVICE);
                    int bondState = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                        BluetoothDevice.ERROR);
                    if (bondState == BluetoothDevice.BOND_NONE) {
                        int setId = getRemoteDeviceGroupId(device, null);
                        if (setId < BluetoothDeviceGroup.INVALID_GROUP_ID) {
                            Log.i(TAG, " Group Device "+ device +
                                    " unpaired. Group ID: " + setId);
                            removeSetMemberFromCSet(setId, device);
                        }
                        synchronized (mStateMachines) {
                            CsipSetCoordinatorStateMachine sm = mStateMachines.get(device);
                            if (sm == null) {
                                return;
                            }
                            if (sm.getConnectionState() != BluetoothProfile.STATE_DISCONNECTED) {
                                return;
                            }
                            removeStateMachine(device);
                        }
                    }
                    break;
            }

        }
    };


    @Override
    protected IProfileServiceBinder initBinder() {
        return new BluetoothCsisBinder(this);
    }

    @Override
    protected boolean start() {
        if (DBG) {
            Log.d(TAG, "start()");
        }

        mGroupNativeInterface = Objects.requireNonNull
                (CsipSetCoordinatorNativeInterface.getInstance(),
                "CsipNativeInterface cannot be null when CsipSetCoordinatorService starts");

        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService(),
                "AdapterService cannot be null when CsipSetCoordinatorService starts");

        mGroupScanner = new CsipSetCoordinatorScanner(this);
        HandlerThread thread = new HandlerThread("OptsHandlerThread");
        thread.start();
        mHandler = new CsipOptsHandler(mGroupScanner.getmLooper());
        // Start handler thread for state machines
        mStateMachines.clear();
        mStateMachinesThread = new HandlerThread("CsipSetCoordinatorService.StateMachines");
        mStateMachinesThread.start();
        mGroupNativeInterface.init();
        setCsipSetCoordinatorService(this);
        // register receiver for Bluetooth State change
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        registerReceiver(mReceiver, filter);
        filter = new IntentFilter();
        filter.addAction(BluetoothCsipSetCoordinator.ACTION_CSIS_CONNECTION_STATE_CHANGED);
        mConnectionStateChangedReceiver = new ConnectionStateChangedReceiver();
        registerReceiver(mConnectionStateChangedReceiver, filter);
        registerGroupClientModule(mBluetoothGroupCallback);
        updateBondedDevices();
        startOrStopOpportunisticScan();
        return true;
    }

    private static synchronized void setCsipSetCoordinatorService(
            CsipSetCoordinatorService instance) {
        if (DBG) {
            Log.d(TAG, "setCsipSetCoordinatorService() set to: " + instance);
        }
        sCsipSetCoordinatorService = instance;
    }

    protected boolean stop() {
        if (DBG) {
            Log.d(TAG, "stop()");
        }

        if (sCsipSetCoordinatorService == null) {
            Log.w(TAG, "stop() called already..");
            return true;
        }

        if (mIsOptScanStarted) {
            setOpportunisticScan(OPT_SCAN_DISABLE);
        }
        if (mGroupScanner != null) {
            mGroupScanner.cleanup();
        }
        if (mConnectionStateChangedReceiver != null) {
            try {
                unregisterReceiver(mReceiver);
                unregisterReceiver(mConnectionStateChangedReceiver);
                mConnectionStateChangedReceiver = null;
            } catch (IllegalArgumentException e) {
                Log.w(TAG, e.getMessage());
            }
        }
        if (mLocalAppId != -1) {
            unregisterGroupClientModule(mLocalAppId);
        }
        // Cleanup native interface
        mGroupNativeInterface.cleanup();
        mGroupNativeInterface = null;

        // Mark service as stopped
        setCsipSetCoordinatorService(null);

        // Destroy state machines and stop handler thread
        synchronized (mStateMachines) {
            for (CsipSetCoordinatorStateMachine sm : mStateMachines.values()) {
                sm.doQuit();
                sm.cleanup();
            }
            mStateMachines.clear();
        }

        if (mStateMachinesThread != null) {
            mStateMachinesThread.quitSafely();
            mStateMachinesThread = null;
        }

        mLocks.clear();
        mDeviceGroupIdMap.clear();
        mCallbacks.clear();
        mGroupIdToUuidMap.clear();

        // cleanup initializations
        mGroupScanner = null;
        mAdapterService = null;

        return true;
    }

    @Override
    protected void cleanup() {
        if (DBG) {
            Log.d(TAG, "cleanup()");
        }

        // Cleanup native interface
        if (mGroupNativeInterface != null) {
            mGroupNativeInterface.cleanup();
            mGroupNativeInterface = null;
        }

        // cleanup initializations
        mGroupScanner = null;
        mAdapterService = null;
    }

    /**
     * Get the CsipSetCoordinatorService instance
     * @return CsipSetCoordinatorService instance
     */
    public static synchronized CsipSetCoordinatorService getCsipSetCoordinatorService() {
        if (sCsipSetCoordinatorService == null) {
            Log.w(TAG, "getCsipSetCoordinatorService(): service is NULL");
            return null;
        }
        return sCsipSetCoordinatorService;
    }

    /* API to load coordinated set from bonded device on BT ON */
    public static void loadDeviceGroupFromBondedDevice (
            BluetoothDevice device, String setDetails) {
        String[] csets = setDetails.split(" ");
        if (VDBG) Log.v(TAG, " Device is part of " + csets.length + " device groups");
        for (String setInfo: csets) {
            String[] setProperties = setInfo.split("~");
            int setId = INVALID_SET_ID, size = 0;
            UUID inclSrvcUuid = UUID.fromString("00000000-0000-0000-0000-000000000000");
            boolean lockSupport = false;
            for (String property: setProperties) {
                if (VDBG) Log.v(TAG, "Property = " + property);
                String[] propSplit = property.split(":");
                if (propSplit[0].equals(SET_ID)) {
                    setId = Integer.parseInt(propSplit[1]);
                } else if (propSplit[0].equals(INCLUDING_SRVC)) {
                    inclSrvcUuid = UUID.fromString(propSplit[1]);
                } else if (propSplit[0].equals(SIZE)) {
                    size = Integer.parseInt(propSplit[1]);
                } else if (propSplit[0].equals(SIRK) && setId != 16) {
                    setSirkMap.put(setId,
                            CsipSetCoordinatorScanner.hexStringToByteArray(propSplit[1]));
                } else if (propSplit[0].equals(LOCK_SUPPORT)) {
                    lockSupport = Boolean.parseBoolean(propSplit[1]);
                }
            }
            DeviceGroup set = getCoordinatedSet(setId, false);
            if (set == null) {
                List<BluetoothDevice> members = new ArrayList<BluetoothDevice>();
                members.add(device);
                set = new DeviceGroup(setId, size, members,
                        new ParcelUuid(inclSrvcUuid), lockSupport);
                mCoordinatedSets.add(set);
            } else {
                if (!set.getDeviceGroupMembers().contains(device)) {
                    set.getDeviceGroupMembers().add(device);
                }
            }
            if (VDBG) Log.v(TAG, "Device " + device + " loaded in Group ("+ setId +")"
                            + " Devices: " + set.getDeviceGroupMembers());
        }
    }

   /* API to accept PSRI data from EIR packet */
    public void handleEIRGroupData(BluetoothDevice device, String data) {
        mGroupScanner.handleEIRGroupData(device, data.getBytes());
    }

    /**
     * Binder object: must be a static class or memory leak may occur
     */
    @VisibleForTesting
    static class BluetoothCsisBinder
            extends IBluetoothCsipSetCoordinator.Stub implements IProfileServiceBinder {
        private CsipSetCoordinatorService mService;

        private CsipSetCoordinatorService getService(AttributionSource source) {
            if (!Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkCallerIsSystemOrActiveOrManagedUser(mService, TAG)) {
                return null;
            }
            return mService;
        }

        BluetoothCsisBinder(CsipSetCoordinatorService svc) {
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
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                boolean defaultValue = false;
                CsipSetCoordinatorService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.connect(mLocalAppId, device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void disconnect(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                boolean defaultValue = false;
                CsipSetCoordinatorService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.disconnect(mLocalAppId, device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectedDevices(AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                List<BluetoothDevice> defaultValue = new ArrayList<>();
                CsipSetCoordinatorService service = getService(source);
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
                CsipSetCoordinatorService service = getService(source);
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
                CsipSetCoordinatorService service = getService(source);
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
                CsipSetCoordinatorService service = getService(source);
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
                CsipSetCoordinatorService service = getService(source);
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
        public void lockGroup(
                int groupId, @NonNull IBluetoothCsipSetCoordinatorLockCallback callback,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(callback, "callback cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                ParcelUuid defaultValue = null;
                CsipSetCoordinatorService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                UUID lockUuid = service.groupLock(groupId, callback);
                defaultValue = lockUuid == null ? null : new ParcelUuid(lockUuid);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void unlockGroup(@NonNull ParcelUuid lockUuid, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(lockUuid, "lockUuid cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                CsipSetCoordinatorService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                service.groupUnlock(lockUuid.getUuid());
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getAllGroupIds(ParcelUuid uuid, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(uuid, "uuid cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");
                List<Integer> defaultValue = new ArrayList<Integer>();
                CsipSetCoordinatorService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getAllGroupIds(uuid);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getGroupUuidMapByDevice(BluetoothDevice device,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Map<Integer, ParcelUuid> defaultValue = null;
                CsipSetCoordinatorService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getGroupUuidMapByDevice(device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getDesiredGroupSize(int groupId, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                int defaultValue = IBluetoothCsipSetCoordinator.CSIS_GROUP_SIZE_UNKNOWN;
                CsipSetCoordinatorService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.getDesiredGroupSize(groupId);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
    }

    /* for registration of other Bluetooth profile in Bluetooth App Space*/
    public void registerGroupClientModule(BluetoothGroupCallback callback) {
        UUID uuid;
        if (mGroupNativeInterface == null) return;
        // Generate an unique UUID for Bluetooth Modules which is not used by others apps
        do {
            uuid = UUID.randomUUID();
        } while (mAppMap.appUuids.contains(uuid));
        if (DBG) {
            Log.d(TAG, "registerGroupClientApp: UUID = " + uuid.toString());
        }
        boolean isLocal = true;
        mAppMap.add(uuid, isLocal, callback);
        mGroupNativeInterface.registerCsipApp(uuid.getLeastSignificantBits(),
                uuid.getMostSignificantBits());
    }

    /* Unregisters Bluetooth module (BT profile) with CSIP*/
    public void unregisterGroupClientModule(int appId) {
        if (DBG) {
            Log.d(TAG, "unregisterGroupClientApp: appId = " + appId);
        }
        if (mGroupNativeInterface == null) return;
        mAppMap.remove(appId);
        mGroupNativeInterface.unregisterCsipApp(appId);
    }

    /* API to request change in lock value */
    public void setLockValue(int appId, int setId, List<BluetoothDevice> devices,
                   int value) {
        if (DBG) {
            Log.d(TAG, "setExclusiveAccess: appId = " + appId + ", setId: " + setId +
                    ", value = " + value + ", set Members = " + devices);
        }
        if (mGroupNativeInterface == null) return;
        // appId and setId validation is done at stack layer
        mGroupNativeInterface.setLockValue(appId, setId, devices, value);
    }

    /* Starts the set members discovery for the requested coordinated set */
    private void startSetDiscovery(int appId, int setId) {
        if (DBG) {
            Log.d(TAG, "startGroupDiscovery. setId = " + setId + " Initiating appId = " + appId);
        }
        // Get Application details
        CsipSetCoordinatorAppMap.CsipSetCoordinatorClientApp app = mAppMap.getById(appId);
        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + appId);
            return;
        }
        DeviceGroup cSet = getCoordinatedSet(setId, true);
        if (cSet == null || !setSirkMap.containsKey(setId)) {
            Log.e(TAG, "Invalid Group Id: " + setId);
            mCurrentSetDisc = null;
            return;
        }
        /* check if all set members are already discovered */
        int setSize = cSet.getDeviceGroupSize();
        if (setSize != 0 && cSet.getTotalDiscoveredGroupDevices() >= setSize) {
            return;
        }
        if (mCurrentSetDisc != null && mCurrentSetDisc.mDiscInProgress) {
            Log.e(TAG, "Group Discovery is already in Progress for Group: "
                + mCurrentSetDisc.mSetId + " from AppId: " + mCurrentSetDisc.mAppId
                + " Stop current Group discovery");
            mPendingSetDisc = new SetDiscoveryRequest(appId, setId, false);
            mGroupScanner.stopSetDiscovery(mCurrentSetDisc.mSetId, 0);
            return;
        } else if (mCurrentSetDisc == null) {
            mCurrentSetDisc = new SetDiscoveryRequest(appId, setId, false);
        }

        int transport;
        byte[] sirk;

        sirk = setSirkMap.get(setId);

        /*TODO: Optimize logic if device type is UNKNOWN */
        try {
            BluetoothDevice device = cSet.getDeviceGroupMembers().get(0);
            transport = device.getType();
        } catch (IndexOutOfBoundsException e) {
            Log.e(TAG, "Invalid Group- No device found : " + e);
            mCurrentSetDisc = null;
            // Debug logs
            for (DeviceGroup dGroup: mCoordinatedSets) {
                Log.i(TAG, " Set ID: " + dGroup.getDeviceGroupId() +
                           " Members: " + dGroup.getDeviceGroupMembers());
            }
            return;
        }
        if (mIsOptScanStarted) {
            setOpportunisticScan(OPT_SCAN_DISABLE);
        }
        mGroupScanner.startSetDiscovery(setId, sirk, transport,
                cSet.getDeviceGroupSize(), cSet.getDeviceGroupMembers());
        mCurrentSetDisc.mDiscInProgress = true;
    }

    /**
     * Connect the given Bluetooth device.
     *
     * @return true if connection is successful, false otherwise.
     */
    public boolean connect(int appId, BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
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
        if (!Utils.arrayContains(featureUuids, BluetoothUuid.COORDINATED_SET)
                && !Utils.isPtsTestMode()) {
            Log.e(TAG, "Cannot connect to " + device + " : Remote does not have CSIS UUID");
            return false;
        }

        synchronized (mStateMachines) {
            CsipSetCoordinatorStateMachine smConnect = getOrCreateStateMachine(device);
            if (smConnect == null) {
                Log.e(TAG, "Cannot connect to " + device + " : no state machine");
            }
            smConnect.sendMessage(CsipSetCoordinatorStateMachine.CONNECT, appId);
        }

        return true;
    }

    /* To disconnect from Coordinated Set Device */
    public boolean disconnect (int appId, BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (DBG) Log.d(TAG, "disconnect Device " + device + ", appId " + appId);
        if (device == null) {
            return false;
        }
        synchronized (mStateMachines) {
            CsipSetCoordinatorStateMachine sm = getOrCreateStateMachine(device);
            if (sm != null) {
                sm.sendMessage(CsipSetCoordinatorStateMachine.DISCONNECT, appId);
            }
        }
        return true;
    }

    public DeviceGroup getCoordinatedSet(int setId) {
        return getCoordinatedSet(setId, true);
    }

    /* returns requested coordinated set */
    public static DeviceGroup getCoordinatedSet(int setId, boolean mPublicAddr) {
        if (DBG) {
            Log.d(TAG, "getDeviceGroup : groupId = " + setId
                    + " mPublicAddr: " + mPublicAddr);
        }
        AdapterService adapterService = Objects.requireNonNull(
                AdapterService.getAdapterService(), "AdapterService cannot be null");
        for (DeviceGroup cSet: mCoordinatedSets) {
            if (cSet.getDeviceGroupId() == setId) {
                if (!mPublicAddr) {
                    return cSet;
                // Public addresses are requested. Replace address with public addr
                } else {
                    DeviceGroup set = new DeviceGroup(
                            cSet.getDeviceGroupId(), cSet.getDeviceGroupSize(),
                            new ArrayList<BluetoothDevice>(),
                            cSet.getIncludingServiceUUID(), cSet.isExclusiveAccessSupported());
                    for (BluetoothDevice device: cSet.getDeviceGroupMembers()) {
                        if (mAdapterService.getBondState(device) == BluetoothDevice.BOND_BONDED) {
                            BluetoothDevice publicDevice = device;
                            set.getDeviceGroupMembers().add(publicDevice);
                        }
                    }
                    return set;
                }
            }
        }

        return null;
    }

    public int getRemoteDeviceGroupId (BluetoothDevice device, ParcelUuid uuid) {
        return getRemoteDeviceGroupId(device, uuid, true);
    }

    public int getRemoteDeviceGroupId (BluetoothDevice device, ParcelUuid uuid,
            boolean mPublicAddr) {
        if (DBG) {
            Log.d(TAG, "getRemoteDeviceGroupId: device = " + device + " uuid = " + uuid
                    + ", mPublicAddr = " + mPublicAddr);
        }

        if (mAdapterService == null) {
            Log.e(TAG, "AdapterService instance is NULL. Return.");
            return INVALID_SET_ID;
        }

        BluetoothDevice setDevice = null;
        if (mPublicAddr && mAdapterService.isIgnoreDevice(device)) {
            setDevice = mAdapterService.getIdentityAddress(device);
        }

        if (uuid == null) {
            uuid = new ParcelUuid(EMPTY_UUID);
        }

        for (DeviceGroup cSet: mCoordinatedSets) {
            if ((cSet.getDeviceGroupMembers().contains(device) ||
                    cSet.getDeviceGroupMembers().contains(setDevice))
                    && cSet.getIncludingServiceUUID().equals(uuid)) {
                return cSet.getDeviceGroupId();
            }
        }

        return INVALID_SET_ID;
    }

    /* This API is called when pairing with LE Audio capable set member fails or
     * when set member is unpaired. Removing the set member from list gives option
     * to user to rediscover it */
    public void removeSetMemberFromCSet(int setId, BluetoothDevice device) {
        if (DBG) Log.d(TAG, "removeDeviceFromDeviceGroup: setId = " + setId
                + ", Device: " + device);
        removeDevice(device, setId);
        DeviceGroup cSet = getCoordinatedSet(setId, false);
        if (cSet != null) {
            cSet.getDeviceGroupMembers().remove(device);
            if (cSet.getDeviceGroupMembers().size() == 0) {
                Log.i(TAG, "Last device unpaired. Removing Device Group from database");
                mCoordinatedSets.remove(cSet);
                setSirkMap.remove(setId);
                if (getCoordinatedSet(setId, false) == null) {
                    Log.i(TAG, "Set " + setId + " removed completely");
                }
                return;
            }
        }

        DeviceGroup cSet1 = getCoordinatedSet(setId, true);
        if (cSet1 != null) {
            cSet1.getDeviceGroupMembers().remove(device);
            if (cSet1.getDeviceGroupMembers().size() == 0) {
                Log.i(TAG, "Last device unpaired. Removing Device Group from database");
                mCoordinatedSets.remove(cSet);
                setSirkMap.remove(setId);
                if (getCoordinatedSet(setId, false) == null) {
                    Log.i(TAG, "Set " + setId + " removed completely");
                }
            }
        }
    }

    public void printAllCoordinatedSets() {
        if (VDBG) {
            for (DeviceGroup set: mCoordinatedSets) {
                Log.i(TAG, "GROUP_ID: " + set.getDeviceGroupId()
                    + ", size = " + set.getDeviceGroupSize()
                    + ", discovered = " + set.getTotalDiscoveredGroupDevices()
                    + ", Including Srvc Uuid = "+ set.getIncludingServiceUUID()
                    + ", devices = " + set.getDeviceGroupMembers());
            }
        }
    }

    /* Callback received from CSIP native layer when an APP/module has been registered */
    protected void onCsipAppRegistered (int status, int appId, UUID uuid) {
        if (DBG) {
            Log.d(TAG, "onCsipAppRegistered appId : " + appId + ", UUID: " + uuid.toString());
        }

        CsipSetCoordinatorAppMap.CsipSetCoordinatorClientApp app = mAppMap.getByUuid(uuid);

        if (app == null) {
            Log.e(TAG, "Application not found for UUID: " + uuid.toString());
            return;
        }

        app.appId = appId;
        // Give callback to the application that app has been registered
            if (app.isRegistered && app.isLocal) {
                app.mCallback.onGroupClientAppRegistered(status, appId);
            } else{
                Log.e(TAG, "onCsipAppRegistered error  : " );
            }
    }

    /* When CSIP Profile connection state has been changed */
    protected void onConnectionStateChanged(int appId, BluetoothDevice device,
                int state, int status) {
        if (DBG) {
            Log.d(TAG, "onConnectionStateChanged: appId: " + appId + ", device: "
                   + device + ", State: " + state + ", Status: " + status);
        }

        CsipSetCoordinatorAppMap.CsipSetCoordinatorClientApp app
                = mAppMap.getById(appId);

        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + appId);
            return;
        }

            if (app.isRegistered && app.isLocal) {
                app.mCallback.onConnectionStateChanged(state, device);
            }
    }

    /* When a new set member is discovered as a part of Set Discovery procedure */
    protected void onSetMemberFound (int setId, BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "onSetMemberFound setId: " + setId + ", device: " + device);
        }
        for (DeviceGroup cSet: mCoordinatedSets) {
            if (cSet.getDeviceGroupId() == setId
                    && !cSet.getDeviceGroupMembers().contains(device)) {
                cSet.getDeviceGroupMembers().add(device);
                break;
            }
        }
        sendSetMemberAvailableIntent(setId, device);
    }

    /* When set discovery procedure has been completed */
    protected void onSetDiscoveryCompleted (int setId,
            int totalDiscovered, int reason) {
        Log.d(TAG, "onSetDiscoveryCompleted: setId: " + setId + ", totalDiscovered = "
                + totalDiscovered + "reason: " + reason);

        // mark Set Discovery procedure as completed
        mCurrentSetDisc.mDiscInProgress = false;
        // Give callback to the application that Set Discovery has been completed
        CsipSetCoordinatorAppMap.CsipSetCoordinatorClientApp app
                = mAppMap.getById(mCurrentSetDisc.mAppId);

        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + mCurrentSetDisc.mAppId);
            return;
        }

            DeviceGroup cSet = getCoordinatedSet(setId, false);
            if (VDBG && cSet != null) {
                Log.i(TAG, "Device Group: groupId" + setId + ", devices: "
                        + cSet.getDeviceGroupMembers());
            }

            if (mPendingSetDisc != null) {
                mCurrentSetDisc = mPendingSetDisc;
                mPendingSetDisc = null;
                startSetDiscovery(mCurrentSetDisc.mAppId, mCurrentSetDisc.mSetId);
            } else {
                mCurrentSetDisc = null;
            }
            startOrStopOpportunisticScan();
    }

    /* Callback received from CSIP native layer when a new Coordinated set has been
     * identified with remote device */
    private void onNewSetFound(int setId, BluetoothDevice device, int size,
            byte[] sirk, UUID pSrvcUuid, boolean lockSupport) {
        if (DBG) {
            Log.d(TAG, "onNewSetFound: device : " + device + ", setId: " + setId
                + ", size: " + size + ", uuid: " + pSrvcUuid.toString());
        }
        // Form Coordinated Set Object and store in ArrayList
        List<BluetoothDevice> devices = new ArrayList<BluetoothDevice>();
        devices.add(device);
        DeviceGroup cSet = new DeviceGroup(setId, size, devices,
                new ParcelUuid(pSrvcUuid), lockSupport);
        mCoordinatedSets.add(cSet);

        // Store sirk in hashmap of setId, sirk
        setSirkMap.put(setId, sirk);
       addDevice(device, setId, new ParcelUuid(pSrvcUuid));
        // Discover remaining set members
        startSetDiscovery(mLocalAppId, setId);
    }

    /* Callback received from CSIP native layer when undiscovered set member is connected */
    private void onNewSetMemberFound (int setId, BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "onNewSetMemberFound: setId = " + setId + ", Device = " + device);
        }
        // Required to group set members in UI
        addDevice(device, setId, new ParcelUuid(EMPTY_UUID));
        if (mAdapterService == null) {
            Log.e(TAG, "AdapterService instance is NULL. Return.");
            return;
        }

        // check if this device is already part of an already existing coordinated set
        /* Scenario: When a device was not discovered during initial set discovery
         *           procedure and later user had explicitely paired with this device
         *           from pair new device UI option. Not required to send onSetMemberFound
         *           callback to application*/
        if (setSirkMap.containsKey(setId)) {
            for (DeviceGroup cSet: mCoordinatedSets) {
                int groupId = cSet.getDeviceGroupId();
                if (groupId == setId) {
                    handleSetMemberAvailable(device, setId); // Check is required to notify ?
                }
                if (groupId == setId &&
                        (!cSet.getDeviceGroupMembers().contains(device))) {
                    cSet.getDeviceGroupMembers().add(device);
                    break;
                }
            }
            return;
        }
    }

    private void handleSetMemberAvailable(BluetoothDevice device, int setId) {
        ParcelUuid uuid  = null;
        for (DeviceGroup cSet : mCoordinatedSets) {
            int groupId = cSet.getDeviceGroupId();
            if (groupId == setId) {
                if (cSet.getIncludingServiceUUID() != null) {
                    uuid = cSet.getIncludingServiceUUID();
                }
            }
        }
        if (mCallbacks.isEmpty()) {
            return;
        }
        if (uuid == null) {
            return;
        }
        for (Map.Entry<Executor, IBluetoothCsipSetCoordinatorCallback> entry :
            mCallbacks.get(uuid).entrySet()) {
            Log.d(TAG, " executing " + uuid + " " + entry.getKey());
            try {
                executeCallback(entry.getKey(), entry.getValue(), device, setId);
            } catch (RemoteException e) {
                throw e.rethrowFromSystemServer();
            }
        }
    }

    /* callback received when lock status is changed for requested coordinated set*/
    protected void onLockStatusChanged (int appId, int setId, int value, int status,
            List<BluetoothDevice> devices) {
        if (DBG) {
            Log.d(TAG, "onLockStatusChanged: appId = " + appId + ", setId = " + setId +
                ", value = " + value + ", status = " + status + ", devices = " + devices);
        }
        CsipSetCoordinatorAppMap.CsipSetCoordinatorClientApp app = mAppMap.getById(appId);

        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + appId);
            return;
        }

        if (app.isRegistered && app.isLocal) {
            app.mCallback.onExclusiveAccessChanged(setId, value, status, devices);
        }
    }

    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (DBG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }
        mAdapterService.getDatabase().setProfileConnectionPolicy(
                device, BluetoothProfile.CSIP_SET_COORDINATOR, connectionPolicy);
        if (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            connect(mLocalAppId, device);
        } else if (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            disconnect(mLocalAppId, device);
        }
        return true;
    }

    /**
     * Get the connection policy of the profile.
     *
     * @param device the remote device
     * @return connection policy of the specified device
     */
    public int getConnectionPolicy(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        return mAdapterService.getDatabase().getProfileConnectionPolicy(
                device, BluetoothProfile.CSIP_SET_COORDINATOR);
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
        // Check connectionPolicy and accept or reject the connection.
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

    private CsipSetCoordinatorStateMachine getOrCreateStateMachine(BluetoothDevice device) {
        if (device == null) {
            Log.e(TAG, "getOrCreateStateMachine failed: device cannot be null");
            return null;
        }
        synchronized (mStateMachines) {
            CsipSetCoordinatorStateMachine sm = mStateMachines.get(device);
            if (sm != null) {
                return sm;
            }
            // Limit the maximum number of state machines to avoid DoS attack
            if (mStateMachines.size() >= MAX_CSIS_STATE_MACHINES) {
                Log.e(TAG,
                        "Maximum number of CSIS state machines reached: "
                                + MAX_CSIS_STATE_MACHINES);
                return null;
            }
            if (DBG) {
                Log.d(TAG, "Creating a new state machine for " + device);
            }
            sm = CsipSetCoordinatorStateMachine.make(device, this,
                    mGroupNativeInterface, mStateMachinesThread.getLooper());
            mStateMachines.put(device, sm);
            return sm;
        }
    }

    private BluetoothGroupCallback mBluetoothGroupCallback = new BluetoothGroupCallback() {

        public void onGroupClientAppRegistered(int status, int appId) {
            if (DBG) Log.d(TAG, "onGroupClientAppRegistered status " + status
                    + " appId :" + appId);
            if (status == 0 ) {
                mLocalAppId = appId;
            }
        }
    };

    public int getAppId() {
        return mLocalAppId;
    }

    public List<BluetoothDevice> getConnectedDevices() {
        enforceCallingOrSelfPermission(BLUETOOTH_CONNECT, "Need BLUETOOTH_CONNECT permission");
        synchronized (mStateMachines) {
            List<BluetoothDevice> devices = new ArrayList<>();
            for (CsipSetCoordinatorStateMachine sm : mStateMachines.values()) {
                if (sm.isConnected()) {
                    devices.add(sm.getDevice());
                }
            }
            return devices;
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
                if (!Utils.arrayContains(featureUuids, BluetoothUuid.COORDINATED_SET)) {
                    continue;
                }
                int connectionState = BluetoothProfile.STATE_DISCONNECTED;
                CsipSetCoordinatorStateMachine sm = mStateMachines.get(device);
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
            CsipSetCoordinatorStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return sm.getConnectionState();
        }
    }

    /**
     * Lock a given group.
     * @param groupId group ID to lock
     * @param callback callback with the lock request result
     * @return unique lock identifier used for unlocking
     *
     * @hide
     */
    public @Nullable UUID groupLock(
            int groupId, @NonNull IBluetoothCsipSetCoordinatorLockCallback callback) {
        if (callback == null) {
            return null;
        }

        UUID uuid = UUID.randomUUID();
        synchronized (mLocks) {
            if (mLocks.containsKey(groupId)) {
                try {
                    callback.onGroupLockSet(groupId,
                            BluetoothStatusCodes.ERROR_CSIP_GROUP_LOCKED_BY_OTHER, true);
                } catch (RemoteException e) {
                    throw e.rethrowFromSystemServer();
                }
                return null;
            }
            mLocks.put(groupId, new Pair<>(uuid, callback));
        }
        mGroupNativeInterface.setLockValue(mLocalAppId, groupId, null,
                BluetoothDeviceGroup.ACCESS_GRANTED);
        return uuid;
    }

    /**
     * Unlock a given group.
     * @param lockUuid unique lock identifier used for unlocking
     *
     * @hide
     */
    public void groupUnlock(@NonNull UUID lockUuid) {
        if (lockUuid == null) {
            return;
        }
        synchronized (mLocks) {
            for (Map.Entry<Integer, Pair<UUID, IBluetoothCsipSetCoordinatorLockCallback>> entry :
                    mLocks.entrySet()) {
                Pair<UUID, IBluetoothCsipSetCoordinatorLockCallback> uuidCbPair = entry.getValue();
                if (uuidCbPair.first.equals(lockUuid)) {
                    mGroupNativeInterface.setLockValue(mLocalAppId, entry.getKey(), null,
                            BluetoothDeviceGroup.ACCESS_RELEASED);
                    return;
                }
            }
        }
    }

    int getApiStatusCode(int nativeResult) {
        switch (nativeResult) {
            case IBluetoothCsipSetCoordinator.CSIS_GROUP_LOCK_SUCCESS:
                return BluetoothStatusCodes.SUCCESS;
            case IBluetoothCsipSetCoordinator.CSIS_GROUP_LOCK_FAILED_INVALID_GROUP:
                return BluetoothStatusCodes.ERROR_CSIP_INVALID_GROUP_ID;
            case IBluetoothCsipSetCoordinator.CSIS_GROUP_LOCK_FAILED_GROUP_NOT_CONNECTED:
                return BluetoothStatusCodes.ERROR_DEVICE_NOT_CONNECTED;
            case IBluetoothCsipSetCoordinator.CSIS_GROUP_LOCK_FAILED_LOCKED_BY_OTHER:
                return BluetoothStatusCodes.ERROR_CSIP_GROUP_LOCKED_BY_OTHER;
            case IBluetoothCsipSetCoordinator.CSIS_LOCKED_GROUP_MEMBER_LOST:
                return BluetoothStatusCodes.ERROR_CSIP_INVALID_GROUP_ID;
            case IBluetoothCsipSetCoordinator.CSIS_GROUP_LOCK_FAILED_OTHER_REASON:
            default:
                Log.e(TAG, " Unknown status code: " + nativeResult);
                return BluetoothStatusCodes.ERROR_UNKNOWN;
        }
    }

    void handleGroupLockChanged(int groupId, int status, boolean isLocked) {
        synchronized (mLocks) {
            if (!mLocks.containsKey(groupId)) {
                return;
            }
            IBluetoothCsipSetCoordinatorLockCallback cb = mLocks.get(groupId).second;
            try {
                cb.onGroupLockSet(groupId, getApiStatusCode(status), isLocked);
            } catch (RemoteException e) {
                throw e.rethrowFromSystemServer();
            }
            // Unlocking invalidates the existing lock if exist
            if (!isLocked) {
                mLocks.remove(groupId);
            }
        }
    }

    void messageFromNative(CsipSetCoordinatorStackEvent stackEvent) {
        BluetoothDevice device = stackEvent.device;
        Log.d(TAG, "Message from native: " + stackEvent);
        Intent intent = null;
        int groupId = stackEvent.valueInt1;
        if (stackEvent.type == CsipSetCoordinatorStackEvent.EVENT_TYPE_DEVICE_AVAILABLE) {
            Objects.requireNonNull(device, "Device should never be null, event: " + stackEvent);

            intent = new Intent(BluetoothCsipSetCoordinator.ACTION_CSIS_DEVICE_AVAILABLE);
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, stackEvent.device);
            intent.putExtra(BluetoothCsipSetCoordinator.EXTRA_CSIS_GROUP_ID, groupId);
            intent.putExtra(
                    BluetoothCsipSetCoordinator.EXTRA_CSIS_GROUP_SIZE, stackEvent.valueInt2);
            intent.putExtra(
                    BluetoothCsipSetCoordinator.EXTRA_CSIS_GROUP_TYPE_UUID, stackEvent.valueUuid1);
            onNewSetFound(groupId, device, stackEvent.valueInt2, stackEvent.sirk,
                    stackEvent.valueUuid1, stackEvent.lockSupport);
        } else if (stackEvent.type
                == CsipSetCoordinatorStackEvent.EVENT_TYPE_SET_MEMBER_AVAILABLE) {
            Objects.requireNonNull(device, "Device should never be null, event: " + stackEvent);
            /* Notify registered parties */
            //handleSetMemberAvailable(device, stackEvent.valueInt1);
             onNewSetMemberFound(groupId, device);
            /* Sent intent as well */
            intent = new Intent(BluetoothCsipSetCoordinator.ACTION_CSIS_SET_MEMBER_AVAILABLE);
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
            intent.putExtra(BluetoothCsipSetCoordinator.EXTRA_CSIS_GROUP_ID, groupId);
        } else if (stackEvent.type == CsipSetCoordinatorStackEvent.EVENT_TYPE_GROUP_LOCK_CHANGED) {
            int lock_status = stackEvent.valueInt2;
            boolean lock_state = stackEvent.valueBool1;
            handleGroupLockChanged(groupId, lock_status, lock_state);
        }

        if (intent != null) {
            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                    | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
            sendBroadcast(intent, BLUETOOTH_PRIVILEGED);
        }

        synchronized (mStateMachines) {
            CsipSetCoordinatorStateMachine sm = mStateMachines.get(device);

            if (stackEvent.type
                    == CsipSetCoordinatorStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED) {
                if (sm == null) {
                    switch (stackEvent.valueInt1) {
                        case CsipSetCoordinatorStackEvent.CONNECTION_STATE_CONNECTED:
                        case CsipSetCoordinatorStackEvent.CONNECTION_STATE_CONNECTING:
                            sm = getOrCreateStateMachine(device);
                            break;
                        default:
                            break;
                    }
                }

                if (sm == null) {
                    Log.e(TAG, "Cannot process stack event: no state machine: " + stackEvent);
                    return;
                }
                sm.sendMessage(CsipSetCoordinatorStateMachine.STACK_EVENT, stackEvent);
            }
        }
    }

    /**
     * Register for CSIS
     */
    public void registerCsisMemberObserver(@CallbackExecutor Executor executor, ParcelUuid uuid,
            IBluetoothCsipSetCoordinatorCallback callback) {
        Map<Executor, IBluetoothCsipSetCoordinatorCallback> entries =
                mCallbacks.getOrDefault(uuid, null);
        if (entries == null) {
            entries = new HashMap<>();
            entries.put(executor, callback);
            Log.d(TAG, " Csis adding new callback for " + uuid);
            mCallbacks.put(uuid, entries);
            return;
        }

        if (entries.containsKey(executor)) {
            if (entries.get(executor) == callback) {
                Log.d(TAG, " Execute and callback already added " + uuid);
                return;
            }
        }

        Log.d(TAG, " Csis adding callback " + uuid);
        entries.put(executor, callback);
    }

    private void executeCallback(Executor exec, IBluetoothCsipSetCoordinatorCallback callback,
            BluetoothDevice device, int groupId) throws RemoteException {
        exec.execute(() -> {
            try {
                callback.onCsisSetMemberAvailable(device, groupId);
            } catch (RemoteException e) {
                throw e.rethrowFromSystemServer();
            }
        });
    }

    private void removeStateMachine(BluetoothDevice device) {
        synchronized (mStateMachines) {
            CsipSetCoordinatorStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                Log.w(TAG,
                        "removeStateMachine: device " + device + " does not have a state machine");
                return;
            }
            if (DBG) {
                Log.d(TAG, "removeStateMachine " + device);
            }
            sm.doQuit();
            sm.cleanup();
            mStateMachines.remove(device);
        }
    }

    private class ConnectionStateChangedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!BluetoothCsipSetCoordinator.ACTION_CSIS_CONNECTION_STATE_CHANGED.equals(
                        intent.getAction())) {
                return;
            }
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            int toState = intent.getIntExtra(BluetoothProfile.EXTRA_STATE, -1);
            int fromState = intent.getIntExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, -1);
            connectionStateChanged(device, fromState, toState);
        }
    }

    synchronized void connectionStateChanged(BluetoothDevice device, int fromState, int toState) {
        if ((device == null) || (fromState == toState)) {
            Log.e(TAG,
                    "connectionStateChanged: unexpected invocation. device=" + device
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
     * Get group desired size
     * @param groupId group ID
     * @return the number of group members
     */
    private int getDesiredGroupSize(int groupId) {
        int size = IBluetoothCsipSetCoordinator.CSIS_GROUP_SIZE_UNKNOWN;
        for (DeviceGroup cSet : mCoordinatedSets) {
            if (cSet.getDeviceGroupId() == groupId) {
                size = cSet.getDeviceGroupSize();
                break;
            }
        }
        return size;
    }

    /**
     * Get collection of group IDs for a given UUID
     * @param uuid
     * @return list of group IDs
     */
    public List<Integer> getAllGroupIds(ParcelUuid uuid) {
        return mGroupIdToUuidMap.entrySet()
                .stream()
                .filter(e -> uuid.equals(e.getValue()))
                .map(Map.Entry::getKey)
                .collect(Collectors.toList());
    }

    /**
     * Get device's groups/
     * @param device
     * @return map of group id and related uuids.
     */
    public Map<Integer, ParcelUuid> getGroupUuidMapByDevice(BluetoothDevice device) {
        Set<Integer> device_groups = mDeviceGroupIdMap.getOrDefault(device, new HashSet<>());
        return mGroupIdToUuidMap.entrySet()
                .stream()
                .filter(e -> device_groups.contains(e.getKey()))
                .collect(Collectors.toMap(Map.Entry::getKey, Map.Entry::getValue));
    }

    private void sendSetMemberAvailableIntent(int setid, BluetoothDevice device) {
        if (DBG) Log.d(TAG, "sendSetMemberAvailableIntent setid " + setid + " dev " + device);
        Intent intent = new Intent(BluetoothCsipSetCoordinator.ACTION_CSIS_SET_MEMBER_AVAILABLE);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothCsipSetCoordinator.EXTRA_CSIS_GROUP_ID, setid);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        sendBroadcast(intent, BLUETOOTH_PRIVILEGED);
    }

    private void updateBondedDevices() {
        for (DeviceGroup group : mCoordinatedSets) {
            ParcelUuid parcel_uuid = group.getIncludingServiceUUID();
            int setid = group.getDeviceGroupId();
            List<BluetoothDevice> devicelist = group.getDeviceGroupMembers();
            for (BluetoothDevice dev : devicelist) {
                addDevice(dev, setid, parcel_uuid);
            }
        }
        if (DBG) Log.d(TAG, " updateBondedDevices " + mDeviceGroupIdMap);
    }

    private void addDevice(BluetoothDevice device, int setid, ParcelUuid parcel_uuid) {
        if (!getAllGroupIds(parcel_uuid).contains(setid)) {
            mGroupIdToUuidMap.put(setid, parcel_uuid);
        }
        if (!mDeviceGroupIdMap.containsKey(device)) {
            mDeviceGroupIdMap.put(device, new HashSet<Integer>());
        }
        Set<Integer> all_device_groups = mDeviceGroupIdMap.get(device);
        all_device_groups.add(setid);
    }

    // Remove set Device from Map when set member is unpaired
    private void removeDevice(BluetoothDevice device, int setid) {
        if (mDeviceGroupIdMap.containsKey(device)) {
            Set<Integer> deviceGroups = mDeviceGroupIdMap.get(device);
            if (deviceGroups.size() == 1) {
                mDeviceGroupIdMap.remove(device);
                mGroupIdToUuidMap.remove(setid);
            } else {
                Set<Integer> all_device_groups = mDeviceGroupIdMap.get(device);
                all_device_groups.remove(setid);
            }
            if (DBG)
                Log.d(TAG, "removeDeviceFromDeviceGroup After remove setId = " + setid
                        + ", Device: " + device + " mDeviceGroupIdMap " + mDeviceGroupIdMap);
         }
    }

    // Handler for Opportunistic scan operations and set member resolution.
    private class CsipOptsHandler extends Handler {
        CsipOptsHandler(Looper looper) {
            super(looper);
        }

        public void handleMessage(Message msg) {
            switch (msg.what) {
            case MSG_START_STOP_OPTSCAN:
                if (mGroupNativeInterface != null) {
                    mGroupNativeInterface.setOpportunisticScan(mIsOptScanStarted);
                    if (DBG)
                        Log.d(TAG, "MSG_START_STOP_OPTSCAN " + mIsOptScanStarted);
                }
                break;
            case MSG_RSI_DATA_FOUND:
                RsiData rsiData = (RsiData) msg.obj;
                startSetMemberResolution(rsiData.data, rsiData.device);
                break;
            default:
                break;
            }
        }
    }

    /* RSI Data */
    class RsiData {
        BluetoothDevice device;
        byte[] data;

        RsiData(BluetoothDevice device, byte[] rsiData) {
            this.device = device;
            data = rsiData;
        }
    }

    void setOpportunisticScan(boolean isStart) {
        if (mIsOptScanStarted != isStart && mHandler != null) {
            mIsOptScanStarted = isStart;
            mHandler.sendMessage(mHandler.obtainMessage(MSG_START_STOP_OPTSCAN));
        } else {
            if (DBG)
                Log.d(TAG, "setOpportunisticScan ignored mIsOptScanStarted " + mIsOptScanStarted
                        + " mHandler " + mHandler);
        }
    }

    void startOrStopOpportunisticScan() {
        if (mCurrentSetDisc != null || mPendingSetDisc != null) {
            Log.i(TAG, "startOrStopOpportunisticScan currentSetDisc " + mCurrentSetDisc
                    + " PendingSetDisc " + mPendingSetDisc);
            return;
        }
        for (DeviceGroup cSet : mCoordinatedSets) {
            if (isSetInComplete(cSet)) {
                setOpportunisticScan(OPT_SCAN_ENABLE);
                break;
            }
        }
    }

    public void onRsiDataFound(byte data[], BluetoothDevice device) {
        if (mAdapterService.getBondState(device) != BluetoothDevice.BOND_NONE) {
            Log.i(TAG, "onRsiDataFound Device bonded ignore " + device);
            return;
        }
        if (VDBG)
            Log.v(TAG, "onRsiDataFound " + device);
        if (mHandler != null) {
            RsiData rsiData = new RsiData(device, data);
            mHandler.sendMessage(mHandler.obtainMessage(MSG_RSI_DATA_FOUND, rsiData));
        }
    }

    private void startSetMemberResolution(byte data[], BluetoothDevice device) {
        if (VDBG) {
            mGroupScanner.printByteArrayInHex(data, "rsidata");
        }
        Set<Integer> setid = setSirkMap.keySet();
        for (int id : setid) {
            byte[] sirk = setSirkMap.get(id);
            DeviceGroup deviceGroup = getCoordinatedSet(id, false);
            if ((sirk != null) && (deviceGroup != null) && isSetInComplete(deviceGroup)) {
                if (mGroupScanner.startPsriResolution(data, sirk, device, id)) {
                    break;
                }
            }
        }
    }

    private boolean isSetInComplete(DeviceGroup deviceGroup) {
        return deviceGroup.getDeviceGroupSize() != deviceGroup.getTotalDiscoveredGroupDevices();
    }
}
