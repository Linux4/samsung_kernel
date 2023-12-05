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

package com.android.bluetooth.groupclient;

import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.BluetoothDeviceGroup;
import android.bluetooth.DeviceGroup;
import android.bluetooth.IBluetoothDeviceGroup;
import android.bluetooth.IBluetoothGroupCallback;
import android.bluetooth.BluetoothGroupCallback;
import android.content.AttributionSource;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ParcelUuid;
import android.os.SystemProperties;

import android.util.Log;

import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.Config;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.ServiceFactory;
import com.android.bluetooth.Utils;

import java.util.ArrayList;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.UUID;

/**
 * Provides Bluetooth CSIP Client profile, as a service in the Bluetooth application.
 * @hide
 */
public class GroupService extends ProfileService {
    private static final boolean DBG = true;
    private static final String TAG = "BluetoothGroupService";
    protected static final boolean VDBG = Log.isLoggable(TAG, Log.VERBOSE);

    private GroupScanner mGroupScanner;

    private static GroupService sGroupService;

    private AdapterService mAdapterService;

    GroupClientNativeInterface mGroupNativeInterface;

    GroupAppMap mAppMap = new GroupAppMap();

    private static CopyOnWriteArrayList<DeviceGroup> mCoordinatedSets
            = new CopyOnWriteArrayList<DeviceGroup>();

    private static HashMap<Integer, byte[]> setSirkMap = new HashMap<Integer, byte[]>();

    private static final int INVALID_APP_ID = 0x10;
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
                    }
                    break;
            }

        }
    };


    @Override
    protected IProfileServiceBinder initBinder() {
        return new GroupBinder(this);
    }

    @Override
    protected boolean start() {
        if (DBG) {
            Log.d(TAG, "start()");
        }

        mGroupNativeInterface = Objects.requireNonNull(GroupClientNativeInterface.getInstance(),
                "GroupClientNativeInterface cannot be null when GroupService starts");

        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService(),
                "AdapterService cannot be null when GroupService starts");

        mGroupScanner = new GroupScanner(this);

        mGroupNativeInterface.init();
        setGroupService(this);

        // register receiver for Bluetooth State change
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        registerReceiver(mReceiver, filter);

        return true;
    }

    private static synchronized void setGroupService(GroupService instance) {
        if (DBG) {
            Log.d(TAG, "setGroupService(): set to: " + instance);
        }
        sGroupService = instance;
    }

    protected boolean stop() {
        if (DBG) {
            Log.d(TAG, "stop()");
        }

        if (mGroupScanner != null) {
            mGroupScanner.cleanup();
        }

        if (sGroupService == null) {
            Log.w(TAG, "stop() called already..");
            return true;
        }
        try {
            unregisterReceiver(mReceiver);
        } catch (IllegalArgumentException e) {
            Log.w(TAG, e.getMessage());
        }

        // Cleanup native interface
        mGroupNativeInterface.cleanup();
        mGroupNativeInterface = null;

        // Mark service as stopped
        setGroupService(null);

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
     * Get the GroupService instance
     * @return GroupService instance
     */
    public static synchronized GroupService getGroupService() {
        if (sGroupService == null) {
            if (DBG) Log.w(TAG, "getGroupService(): service is NULL");
            return null;
        }

        if (!sGroupService.isAvailable()) {
            if (DBG) Log.w(TAG, "getGroupService(): service is not available");
            return null;
        }

        return sGroupService;
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
                    setSirkMap.put(setId, GroupScanner.hexStringToByteArray(propSplit[1]));
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

    private static class GroupBinder
            extends IBluetoothDeviceGroup.Stub implements IProfileServiceBinder {
        private GroupService mService;

        private GroupService getService() {
            if (mService != null && mService.isAvailable()) {
                return mService;
            }
            return null;
        }

        GroupBinder(GroupService service) {
            if (DBG) {
                Log.v(TAG, "GroupBinder()");
            }
            mService = service;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public void connect(int appId, BluetoothDevice device, AttributionSource source) {
            if (DBG) {
                Log.d(TAG, "connect Device " + device);
            }

            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(
                    service, source, "connect")) {
                return;
            }
            service.connect(appId, device);
        }

        @Override
        public void disconnect(int appId, BluetoothDevice device, AttributionSource source) {
            if (DBG) {
                Log.d(TAG, "disconnect Device " + device);
            }

            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(
                    service, source, "disconnect")) {
                return;
            }
            service.disconnect(appId, device);
        }

        @Override
        public void registerGroupClientApp(ParcelUuid uuid,
                IBluetoothGroupCallback callback, AttributionSource source) {
            if (VDBG) {
                Log.d(TAG, "registerGroupClientApp");
            }
            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(
                    service, source, "registerGroupClientApp")) {
                return;
            }
            service.registerGroupClientApp(uuid.getUuid(), callback, null);
        }

        @Override
        public void unregisterGroupClientApp(int appId, AttributionSource source) {
            if (VDBG) {
                    Log.d(TAG, "unregisterGroupClientApp");
            }
            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(
                    service, source, "unregisterGroupClientApp")) {
                return;
            }
            service.unregisterGroupClientApp(appId);
        }

        @Override
        public void setExclusiveAccess(int appId, int groupId, List<BluetoothDevice> devices,
                   int value, AttributionSource source) {
            if (VDBG) {
                Log.d(TAG, "setExclusiveAccess");
            }
            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(
                    service, source, "setExclusiveAccess")) {
                return;
            }
            service.setLockValue(appId, groupId, devices, value);
        }

        @Override
        public void startGroupDiscovery(int appId, int groupId
                , AttributionSource source) throws RemoteException  {
            if (VDBG) {
                Log.d(TAG, "startGroupDiscovery");
            }
            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(service,
                    source, "startGroupDiscovery")) {
                return;
            }
            service.startSetDiscovery(appId, groupId);
        }

        @Override
        public void stopGroupDiscovery(int appId, int groupId, AttributionSource source) {
            if (VDBG) {
                Log.d(TAG, "stopGroupDiscovery");
            }
            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(
                    service, source, "stopGroupDiscovery")) {
                return;
            }
            enforceBluetoothPrivilegedPermission(service);
            service.stopSetDiscovery(appId, groupId);
        }

        @Override
        public void getExclusiveAccessStatus(int appId, int groupId,
                List<BluetoothDevice> devices, AttributionSource source) {
            if (DBG) {
                Log.d(TAG, "getExclusiveAccessStatus");
            }
            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(
                service, source, "getExclusiveAccessStatus")) {
                return;
            }
            enforceBluetoothPrivilegedPermission(service);
        }

        @Override
        public List<DeviceGroup> getDiscoveredGroups(boolean mPublicAddr
                , AttributionSource source) {
            if (DBG) {
                Log.d(TAG, "getDiscoveredGroups");
            }
            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(
                service, source, "getDiscoveredGroups")) {
                return null;
            }
            return service.getDiscoveredCoordinatedSets(mPublicAddr);
        }

        @Override
        public DeviceGroup getDeviceGroup(int setId, boolean mPublicAddr,
                AttributionSource source) {
            if (DBG) {
                Log.d(TAG, "getDeviceGroup");
            }
            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(
                service, source, "getDeviceGroup")) {
                return null;
            }
            return (service.getCoordinatedSet(setId, mPublicAddr));
        }

        @Override
        public int getRemoteDeviceGroupId (BluetoothDevice device, ParcelUuid uuid,
                boolean mPublicAddr, AttributionSource source) {
            if (DBG) {
                Log.d(TAG, "getRemoteDeviceGroupId");
            }
            GroupService service = getService();
            if (service == null || !Utils.checkConnectPermissionForDataDelivery(
                service, source, "getRemoteDeviceGroupId")) {
                return INVALID_SET_ID;
            }
            return service.getRemoteDeviceGroupId(device, uuid, mPublicAddr);
        }

        @Override
        public boolean isGroupDiscoveryInProgress (int setId, AttributionSource source) {
            if (DBG) {
                Log.d(TAG, "isGroupDiscoveryInProgress");
            }
            GroupService service = getService();
            if (service == null || !Utils.checkScanPermissionForDataDelivery(
                service, source, "isGroupDiscoveryInProgress")) {
                return false;
            }
            return service.isSetDiscoveryInProgress(setId);
        }
    };

    /**
     * DeathReceipient handler to unregister applications those are
     * disconnected ungracefully (ie. crash or forced close).
     */
    class GroupAppDeathRecipient implements IBinder.DeathRecipient {
        int mAppId;

        GroupAppDeathRecipient(int appId) {
            mAppId = appId;
            Log.i(TAG, "GroupAppDeathRecipient");
        }

        @Override
        public void binderDied() {
            if (DBG) {
                Log.d(TAG, "Binder is dead - unregistering app (" + mAppId + ")!");
            }

            mAppMap.remove(mAppId);
            unregisterGroupClientApp(mAppId);
        }

    }

    /* for registration of other Bluetooth profile in Bluetooth App Space*/
    public void registerGroupClientModule(BluetoothGroupCallback callback) {
        Log.d(TAG, "registerGroupClientModule");

        UUID uuid;

        if (mGroupNativeInterface == null) return;
        // Generate an unique UUID for Bluetooth Modules which is not used by others apps
        do {
            uuid = UUID.randomUUID();
        } while(mAppMap.appUuids.contains(uuid));

        registerGroupClientApp(uuid, null, callback);
    }

    /* Registers CSIP App or module with CSIP native layer */
    public void registerGroupClientApp(UUID uuid, IBluetoothGroupCallback appCb,
            BluetoothGroupCallback localCallback) {
        if (DBG) {
            Log.d(TAG, "registerGroupClientApp: UUID = " + uuid.toString());
        }

        boolean isLocal = false;
        if (localCallback != null) {
            isLocal = true;
        }

        mAppMap.add(uuid, isLocal, appCb, localCallback);
        mGroupNativeInterface.registerCsipApp(uuid.getLeastSignificantBits(),
                uuid.getMostSignificantBits());
    }

    /* Unregisters Bluetooth module (BT profile) with CSIP*/
    public void unregisterGroupClientModule(int appId) {
        unregisterGroupClientApp(appId);
    }

    /* Unregisters App/Module with CSIP*/
    public void unregisterGroupClientApp(int appId) {
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
    public void startSetDiscovery(int appId, int setId) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "startGroupDiscovery. setId = " + setId + " Initiating appId = " + appId);
        }

        // Get Apllication details
        GroupAppMap.GroupClientApp app = mAppMap.getById(appId);
        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + appId);
            return;
        }

        DeviceGroup cSet = getCoordinatedSet(setId, true);
        if (cSet == null || !setSirkMap.containsKey(setId)) {
            Log.e(TAG, "Invalid Group Id: " + setId);
            mCurrentSetDisc = null;
            app.appCb.onGroupDiscoveryStatusChanged(setId,
                    BluetoothDeviceGroup.GROUP_DISCOVERY_STOPPED,
                    BluetoothDeviceGroup.DISCOVERY_NOT_STARTED_INVALID_PARAMS);
            return;
        }

        /* check if all set members are already discovered */
        int setSize = cSet.getDeviceGroupSize();
        if (setSize != 0 && cSet.getTotalDiscoveredGroupDevices() >= setSize) {
            app.appCb.onGroupDiscoveryStatusChanged(setId,
                    BluetoothDeviceGroup.GROUP_DISCOVERY_STOPPED,
                    BluetoothDeviceGroup.DISCOVERY_COMPLETED);
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

        mGroupScanner.startSetDiscovery(setId, sirk, transport,
                cSet.getDeviceGroupSize(), cSet.getDeviceGroupMembers());
        mCurrentSetDisc.mDiscInProgress = true;

        try {
            if (app.appCb != null) {
                app.appCb.onGroupDiscoveryStatusChanged(setId,
                        BluetoothDeviceGroup.GROUP_DISCOVERY_STARTED,
                        BluetoothDeviceGroup.DISCOVERY_STARTED_BY_APPL);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Exception : " + e);
        }
    }

    /* Stops the set members discovery for the requested coordinated set */
    public void stopSetDiscovery(int appId, int setId) {
        if (DBG) {
            Log.d(TAG, "stopGroupDiscovery: appId = " + appId + " groupId = " + setId);
        }

        // Get Apllication details
        GroupAppMap.GroupClientApp app = mAppMap.getById(appId);

        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + appId);
            return;
        }

        // check if requesting app is stopping the discovery
        if (mCurrentSetDisc == null || mCurrentSetDisc.mAppId != appId) {
            Log.e(TAG, " Either no discovery in progress or Stop Request from"
                   + " App which has not started Group Discovery");
            return;
        }

        mGroupScanner.stopSetDiscovery(setId, BluetoothDeviceGroup.DISCOVERY_STOPPED_BY_APPL);
    }

    /* Reading lock status of coordinated set for ordered access procedure */
    public void getLockStatus(int setId, List<BluetoothDevice> devices) {
        //TODO: Future enhancement
    }

    /* To connect to Coordinated Set Device */
    public void connect (int appId, BluetoothDevice device) {
        Log.d(TAG, "connect Device: " + device + ", appId: " + appId);
        if (mGroupNativeInterface == null) return;
        mGroupNativeInterface.connectSetDevice(appId, device);
    }

    /* To disconnect from Coordinated Set Device */
    public void disconnect (int appId, BluetoothDevice device) {
        Log.d(TAG, "disconnect Device: " + device + ", appId: " + appId);
        if (mGroupNativeInterface == null) return;
        mGroupNativeInterface.disconnectSetDevice(appId, device);
    }

    public List<DeviceGroup> getDiscoveredCoordinatedSets() {
        return getDiscoveredCoordinatedSets(true);
    }

    /* returns all discovered coordinated sets  */
    public List<DeviceGroup> getDiscoveredCoordinatedSets(boolean mPublicAddr) {
        if (DBG) {
            Log.d(TAG, "getDiscoveredGroups");
        }

        /* Add logic to replace random addresses to public addresses if requested */
        // Iterate on coordinated sets
        // check address type. Replace with public if requested
        if (mPublicAddr) {
            List<DeviceGroup> coordinatedSets = new ArrayList<DeviceGroup>();
            AdapterService adapterService = Objects.requireNonNull(
                    AdapterService.getAdapterService(),
                    "AdapterService cannot be null");
            for (DeviceGroup set: mCoordinatedSets) {
                DeviceGroup cSet = new DeviceGroup(
                            set.getDeviceGroupId(), set.getDeviceGroupSize(),
                            new ArrayList<BluetoothDevice>(),
                            set.getIncludingServiceUUID(), set.isExclusiveAccessSupported());
                for (BluetoothDevice device: set.getDeviceGroupMembers()) {
                    BluetoothDevice publicDevice = device;
                    if (adapterService.isIgnoreDevice(device)) {
                        publicDevice = adapterService.getIdentityAddress(device);
                    }
                    cSet.getDeviceGroupMembers().add(publicDevice);
                }
                coordinatedSets.add(cSet);
            }
            return coordinatedSets;
        }

        return mCoordinatedSets;
    }

    public static DeviceGroup getCoordinatedSet(int setId) {
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
                        if (device.getBondState() == BluetoothDevice.BOND_BONDED) {
                            BluetoothDevice publicDevice = device;
                            if (adapterService.isIgnoreDevice(device)) {
                                publicDevice = adapterService.getIdentityAddress(device);
                            }
                            set.getDeviceGroupMembers().add(publicDevice);
                        }
                    }
                    return set;
                }
            }
        }

        return null;
    }

    public boolean isSetDiscoveryInProgress (int setId) {
        if (DBG) {
            Log.d(TAG, "isGroupDiscoveryInProgress: groupId = " + setId);
        }

        if (mCurrentSetDisc != null && mCurrentSetDisc.mSetId == setId
                && mCurrentSetDisc.mDiscInProgress)
            return true;
        return false;
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
        Log.d(TAG, "removeDeviceFromDeviceGroup: setId = " + setId + ", Device: " + device);

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
        Log.d(TAG, "onGroupClientAppRegistered: appId: " + appId + ", UUID: " + uuid.toString());

        GroupAppMap.GroupClientApp app = mAppMap.getByUuid(uuid);

        if (app == null) {
            Log.e(TAG, "Application not found for UUID: " + uuid.toString());
            return;
        }

        app.appId = appId;
        // Give callback to the application that app has been registered
        try {
            if (app.isRegistered && app.isLocal) {
                app.mCallback.onGroupClientAppRegistered(status, appId);
            } else if (app.isRegistered && !app.isLocal) {
                app.linkToDeath(new GroupAppDeathRecipient(appId));
                app.appCb.onGroupClientAppRegistered(status, appId);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Exception : " + e);
        }
    }

    /* When CSIP Profile connection state has been changed */
    protected void onConnectionStateChanged(int appId, BluetoothDevice device,
                int state, int status) {
        Log.d(TAG, "onConnectionStateChanged: appId: " + appId + ", device: " + device
                + ", State: " + state + ", Status: " + status);

        GroupAppMap.GroupClientApp app = mAppMap.getById(appId);

        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + appId);
            return;
        }

        try {
            if (app.isRegistered && app.isLocal) {
                app.mCallback.onConnectionStateChanged(state, device);
            } else if (app.isRegistered && !app.isLocal) {
                app.appCb.onConnectionStateChanged(state, device);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Exception : " + e);
        }
    }

    /* When a new set member is discovered as a part of Set Discovery procedure */
    protected void onSetMemberFound (int setId, BluetoothDevice device) {
        Log.d(TAG, "onGroupDeviceFound: groupId: " + setId + ", device: " + device);
        for (DeviceGroup cSet: mCoordinatedSets) {
            if (cSet.getDeviceGroupId() == setId
                    && !cSet.getDeviceGroupMembers().contains(device)) {
                cSet.getDeviceGroupMembers().add(device);
                break;
            }
        }

        // Give callback to adapterservice to initiate bonding if required
        mAdapterService.processGroupMember(setId, device);

        // Give callback to the application that started Set Discovery
        GroupAppMap.GroupClientApp app = mAppMap.getById(mCurrentSetDisc.mAppId);

        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + mCurrentSetDisc.mAppId);
            return;
        }

        try {
            if (app.appCb != null) {
                app.appCb.onGroupDeviceFound(setId, device);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Exception : " + e);
        }
    }

    /* When set discovery procedure has been completed */
    protected void onSetDiscoveryCompleted (int setId,
            int totalDiscovered, int reason) {
        Log.d(TAG, "onGroupDiscoveryCompleted: groupId: " + setId + ", totalDiscovered = "
                + totalDiscovered + "reason: " + reason);

        // mark Set Discovery procedure as completed
        mCurrentSetDisc.mDiscInProgress = false;
        // Give callback to the application that Set Discovery has been completed
        GroupAppMap.GroupClientApp app = mAppMap.getById(mCurrentSetDisc.mAppId);

        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + mCurrentSetDisc.mAppId);
            return;
        }

        try {
            if (app.appCb != null) {
                app.appCb.onGroupDiscoveryStatusChanged(setId,
                        BluetoothDeviceGroup.GROUP_DISCOVERY_STOPPED, reason);
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
        } catch (RemoteException e) {
            Log.e(TAG, "Exception : " + e);
        }
    }

    /* Callback received from CSIP native layer when a new Coordinated set has been
     * identified with remote device */
    protected void onNewSetFound(int setId, BluetoothDevice device, int size,
            byte[] sirk, UUID pSrvcUuid, boolean lockSupport) {
        Log.d(TAG, "onNewGroupFound: Address : " + device + ", groupId: " + setId
                + ", size: " + size + ", uuid: " + pSrvcUuid.toString());

        // Form Coordinated Set Object and store in ArrayList
        List<BluetoothDevice> devices = new ArrayList<BluetoothDevice>();
        devices.add(device);
        DeviceGroup cSet = new DeviceGroup(setId, size, devices,
                new ParcelUuid(pSrvcUuid), lockSupport);
        mCoordinatedSets.add(cSet);

        // Store sirk in hashmap of setId, sirk
        setSirkMap.put(setId, sirk);

        // Give Callback to all registered application
        try {
            for (GroupAppMap.GroupClientApp app: mAppMap.mApps) {
                if (app.isRegistered && !app.isLocal) {
                     if (app.appCb != null)//temp check
                    app.appCb.onNewGroupFound(setId, device, new ParcelUuid(pSrvcUuid));
                }
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Exception : " + e);
        }
    }

    /* Callback received from CSIP native layer when undiscovered set member is connected */
    protected void onNewSetMemberFound (int setId, BluetoothDevice device) {
        Log.d(TAG, "onNewGroupDeviceFound: groupId = " + setId + ", Device = " + device);

        if (mAdapterService == null) {
            Log.e(TAG, "AdapterService instance is NULL. Return.");
            return;
        }

        if (mAdapterService.isIgnoreDevice(device)) {
            device = mAdapterService.getIdentityAddress(device);
        }
        // check if this device is already part of an already existing coordinated set
        /* Scenario: When a device was not discovered during initial set discovery
         *           procedure and later user had explicitely paired with this device
         *           from pair new device UI option. Not required to send onSetMemberFound
         *           callback to application*/
        if (setSirkMap.containsKey(setId)) {
            for (DeviceGroup cSet: mCoordinatedSets) {
                if (cSet.getDeviceGroupId() == setId &&
                        (!cSet.getDeviceGroupMembers().contains(device))) {
                    cSet.getDeviceGroupMembers().add(device);
                    break;
                }
            }
            return;
        }
    }

    /* callback received when lock status is changed for requested coordinated set*/
    protected void onLockStatusChanged (int appId, int setId, int value, int status,
            List<BluetoothDevice> devices) {
        Log.d(TAG, "onExclusiveAccessChanged: appId = " + appId + ", groupId = " + setId +
                ", value = " + value + ", status = " + status + ", devices = " + devices);
        GroupAppMap.GroupClientApp app = mAppMap.getById(appId);

        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + appId);
            return;
        }

        try {
            if (app.isRegistered && app.isLocal) {
                app.mCallback.onExclusiveAccessChanged(setId, value, status, devices);
            } else if (app.isRegistered && !app.isLocal) {
                app.appCb.onExclusiveAccessChanged(setId, value, status, devices);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Exception : " + e);
        }
    }

    /* Callback received when earlier denied lock is now available */
    protected void onLockAvailable (int appId, int setId, BluetoothDevice device) {
        Log.d(TAG, "onExclusiveAccessAvailable: Remote(" + device + "), Group Id: "
                + setId + ", App Id: " + appId);

        GroupAppMap.GroupClientApp app = mAppMap.getById(appId);
        if (app == null) {
            Log.e(TAG, "Application not found for appId: " + appId);
            return;
        }

        try {
            if (app.isRegistered && app.isLocal) {
                app.mCallback.onExclusiveAccessAvailable(setId, device);
            } else if (app.isRegistered && !app.isLocal) {
                app.appCb.onExclusiveAccessAvailable(setId, device);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Exception : " + e);
        }

    }

    /* Callback received when set size has been changed */
    /* TODO: Scanarios are unknown. Actions are to be decided */
    protected void onSetSizeChanged (int setId, int size, BluetoothDevice device) {
        Log.d(TAG, "onGroupSizeChanged: Group Id: " + setId + ", New Size: " + size +
          ", Notifying device: " + device);

        // TODO: Logic to be incorporated once use case is understood
    }

    /* Callback received when set SIRK has been changed */
    /* TODO: Scanarios are unknown. Actions are to be decided */
    protected void onSetSirkChanged(int setId, byte[] sirk, BluetoothDevice device) {
        Log.d(TAG, "onGroupIdChanged Group Id: " + setId + ", Notifying device: " + device);

        // TODO: Logic to be incorporated once use case is understood
    }

}
