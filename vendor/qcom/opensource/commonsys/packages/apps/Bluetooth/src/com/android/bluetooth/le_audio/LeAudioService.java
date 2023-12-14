/*
 * Copyright 2020 HIMSA II K/S - www.himsa.com.
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

package com.android.bluetooth.le_audio;

import static android.Manifest.permission.BLUETOOTH_CONNECT;
import static android.bluetooth.IBluetoothLeAudio.LE_AUDIO_GROUP_ID_INVALID;

import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;

import android.annotation.RequiresPermission;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothLeAudio;
import android.bluetooth.BluetoothLeAudioCodecConfig;
import android.bluetooth.BluetoothLeAudioCodecStatus;
import android.bluetooth.BluetoothLeAudioContentMetadata;
import android.bluetooth.BluetoothLeBroadcastMetadata;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.IBluetoothLeAudio;
import android.bluetooth.IBluetoothLeAudioCallback;
import android.bluetooth.IBluetoothLeBroadcastCallback;
import android.content.AttributionSource;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.bluetooth.BluetoothAdapter;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Parcel;
import android.os.ParcelUuid;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.sysprop.BluetoothProperties;
import android.util.Log;
import android.util.Pair;
import android.os.SystemProperties;

import android.bluetooth.DeviceGroup;
import android.bluetooth.BluetoothDeviceGroup;
import com.android.bluetooth.CsipWrapper;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.ServiceFactory;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.bluetooth.lebroadcast.LeBroadcastServIntf;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;
import com.android.modules.utils.SynchronousResultReceiver;

import com.android.bluetooth.apm.ActiveDeviceManagerServiceIntf;
import com.android.bluetooth.apm.ApmConstIntf;
import com.android.bluetooth.apm.ApmConst;
import com.android.bluetooth.apm.MediaAudioIntf;
import com.android.bluetooth.apm.CallAudioIntf;
import com.android.bluetooth.apm.VolumeManagerIntf;
import com.android.bluetooth.acm.AcmServIntf;
import com.android.internal.util.ArrayUtils;

import java.util.Arrays;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.SortedSet;
import java.util.TreeSet;

/**
 * Provides Bluetooth LeAudio profile, as a service in the Bluetooth application.
 * @hide
 */
public class LeAudioService extends ProfileService {
    private static final boolean DBG = true;
    private static final String TAG = "LeAudioService";

    // Timeout for state machine thread join, to prevent potential ANR.
    private static final int SM_THREAD_JOIN_TIMEOUT_MS = 1000;

    // Invalid CSIP Coodinator Set Id
    private static final int INVALID_SET_ID = 0x10;

    private static final int NON_CSIP_MIN_GROUP_ID = 0x11;
    private static final int NON_CSIP_MAX_GROUP_ID = 0x20;

    // Upper limit of all LeAudio devices: Bonded or Connected
    private static final int MAX_LE_AUDIO_STATE_MACHINES = 10;
    private static LeAudioService sLeAudioService;

    /**
     * Indicates group audio support for input direction
     */
    private static final int AUDIO_DIRECTION_INPUT_BIT = 0x01;

    /**
     * Indicates group audio support for output direction
     */
    private static final int AUDIO_DIRECTION_OUTPUT_BIT = 0x02;

    /*
     * Indicates no active contexts
     */
    private static final int ACTIVE_CONTEXTS_NONE = 0;

    /*
     * Brodcast profile used by the lower layers
     */
    private static final int BROADCAST_PROFILE_SONIFICATION = 0;
    private static final int BROADCAST_PROFILE_MEDIA = 1;

    private static final String ACTION_LE_AUDIO_CONNECTION_TRIGGER =
                        "android.bluetooth.action.LE_AUDIO_CONN_TRIGGER";

    private static int mPtsMediaAndVoice = 0;
    private static boolean mPtsTmapConfBandC = false;
    private BluetoothAdapter mAdapter;

    private AdapterService mAdapterService;
    private DatabaseManager mDatabaseManager;
    private HandlerThread mStateMachinesThread;
    private BluetoothDevice mActiveAudioOutDevice;
    private BluetoothDevice mActiveAudioInDevice;
    private int mLeActiveGroupID;
    private LeAudioCodecConfig mLeAudioCodecConfig;
    ServiceFactory mServiceFactory = new ServiceFactory();

    //LeAudioNativeInterface mLeAudioNativeInterface;
    boolean mLeAudioNativeIsInitialized = false;
    BluetoothDevice mHfpHandoverDevice = null;
    //LeAudioBroadcasterNativeInterface mLeAudioBroadcasterNativeInterface = null;
    @VisibleForTesting
    AudioManager mAudioManager;

    @VisibleForTesting
    RemoteCallbackList<IBluetoothLeBroadcastCallback> mBroadcastCallbacks;

    @VisibleForTesting
    RemoteCallbackList<IBluetoothLeAudioCallback> mLeAudioCallbacks;

    private class LeAudioGroupDescriptor {
        LeAudioGroupDescriptor() {
            mIsConnected = false;
            mIsActive = false;
            mActiveContexts = ACTIVE_CONTEXTS_NONE;
            mCodecStatus = null;
        }

        public Boolean mIsConnected;
        public Boolean mIsActive;
        public Integer mActiveContexts;
        public BluetoothLeAudioCodecStatus mCodecStatus;
    }

    List<BluetoothLeAudioCodecConfig> mInputLocalCodecCapabilities = new ArrayList<>();
    List<BluetoothLeAudioCodecConfig> mOutputLocalCodecCapabilities = new ArrayList<>();

    private final Map<Integer, LeAudioGroupDescriptor> mGroupDescriptors = new LinkedHashMap<>();
    //private final Map<BluetoothDevice, LeAudioStateMachine> mStateMachines = new LinkedHashMap<>();

    private final Map<BluetoothDevice, Integer> mDeviceGroupIdMap = new LinkedHashMap<>();
    //private final Map<BluetoothDevice, Integer> mDeviceGroupIdMap = new ConcurrentHashMap<>();
    private final Map<BluetoothDevice, Integer> mDeviceAudioLocationMap = new ConcurrentHashMap<>();

    private final int mContextSupportingInputAudio = BluetoothLeAudio.CONTEXT_TYPE_CONVERSATIONAL
            | BluetoothLeAudio.CONTEXT_TYPE_VOICE_ASSISTANTS;

    private final int mContextSupportingOutputAudio = BluetoothLeAudio.CONTEXT_TYPE_CONVERSATIONAL
            | BluetoothLeAudio.CONTEXT_TYPE_MEDIA
            | BluetoothLeAudio.CONTEXT_TYPE_GAME
            | BluetoothLeAudio.CONTEXT_TYPE_INSTRUCTIONAL
            | BluetoothLeAudio.CONTEXT_TYPE_VOICE_ASSISTANTS
            | BluetoothLeAudio.CONTEXT_TYPE_LIVE
            | BluetoothLeAudio.CONTEXT_TYPE_SOUND_EFFECTS
            | BluetoothLeAudio.CONTEXT_TYPE_NOTIFICATIONS
            | BluetoothLeAudio.CONTEXT_TYPE_RINGTONE
            | BluetoothLeAudio.CONTEXT_TYPE_ALERTS
            | BluetoothLeAudio.CONTEXT_TYPE_EMERGENCY_ALARM;

    private BroadcastReceiver mBondStateChangedReceiver;
    private BroadcastReceiver mConnectionStateChangedReceiver;
    private BroadcastReceiver mActiveDeviceChangedReceiver;
    private Handler mHandler = new Handler(Looper.getMainLooper());

    private final Map<Integer, Integer> mBroadcastStateMap = new HashMap<>();
    private final Map<Integer, Boolean> mBroadcastsPlaybackMap = new HashMap<>();
    private final List<BluetoothLeBroadcastMetadata> mBroadcastMetadataList = new ArrayList<>();
    private static SortedSet<Integer> mNonCSIPGruopID = new TreeSet();

    @Override
    protected IProfileServiceBinder initBinder() {
        return new BluetoothLeAudioBinder(this);
    }

    @Override
    protected void create() {
        Log.i(TAG, "create()");
    }

    @Override
    protected boolean start() {
        Log.i(TAG, "start()");
        if (sLeAudioService != null) {
            throw new IllegalStateException("start() called twice");
        }

        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService(),
                "AdapterService cannot be null when LeAudioService starts");
        //mLeAudioNativeInterface = Objects.requireNonNull(LeAudioNativeInterface.getInstance(),
        //        "LeAudioNativeInterface cannot be null when LeAudioService starts");
        mDatabaseManager = Objects.requireNonNull(mAdapterService.getDatabase(),
                "DatabaseManager cannot be null when LeAudioService starts");

        mAudioManager = getSystemService(AudioManager.class);
        Objects.requireNonNull(mAudioManager,
                "AudioManager cannot be null when LeAudioService starts");

        // Start handler thread for state machines
        /*mStateMachines.clear();
        mStateMachinesThread = new HandlerThread("LeAudioService.StateMachines");
        mStateMachinesThread.start();*/

        mDeviceGroupIdMap.clear();
        mDeviceAudioLocationMap.clear();
        mBroadcastStateMap.clear();
        mBroadcastMetadataList.clear();
        mBroadcastsPlaybackMap.clear();

        mGroupDescriptors.clear();

        // Setup broadcast receivers
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        mBondStateChangedReceiver = new BondStateChangedReceiver();
        registerReceiver(mBondStateChangedReceiver, filter);
        filter = new IntentFilter();
        filter.addAction(BluetoothLeAudio.ACTION_LE_AUDIO_CONNECTION_STATE_CHANGED);
        mConnectionStateChangedReceiver = new ConnectionStateChangedReceiver();
        registerReceiver(mConnectionStateChangedReceiver, filter);
        filter = new IntentFilter();
        filter.addAction(BluetoothLeAudio.ACTION_LE_AUDIO_ACTIVE_DEVICE_CHANGED);
        mActiveDeviceChangedReceiver = new ActiveDeviceChangedReceiver();
        registerReceiver(mActiveDeviceChangedReceiver, filter);
        mLeAudioCallbacks = new RemoteCallbackList<IBluetoothLeAudioCallback>();

        // Initialize Broadcast native interface
        /*if (mAdapterService.isLeAudioBroadcastSourceSupported()) {
            mBroadcastCallbacks = new RemoteCallbackList<IBluetoothLeBroadcastCallback>();
            mLeAudioBroadcasterNativeInterface = Objects.requireNonNull(
                    LeAudioBroadcasterNativeInterface.getInstance(),
                    "LeAudioBroadcasterNativeInterface cannot be null when LeAudioService starts");
            mLeAudioBroadcasterNativeInterface.init();
        } else {
            Log.w(TAG, "Le Audio Broadcasts not supported.");
        }*/

        // Mark service as started
        setLeAudioService(this);

        // Setup codec config
        mLeAudioCodecConfig = new LeAudioCodecConfig(this);
        mLeActiveGroupID = LE_AUDIO_GROUP_ID_INVALID;

        // Delay the call to init by posting it. This ensures TBS and MCS are fully initialized
        // before we start accepting connections
        //mHandler.post(() ->
        //        mLeAudioNativeInterface.init(mLeAudioCodecConfig.getCodecConfigOffloading()));

        mPtsTmapConfBandC =
          SystemProperties.getBoolean("persist.vendor.btstack.pts_tmap_conf_B_and_C", false);

        mPtsMediaAndVoice =
                   SystemProperties.getInt("persist.vendor.service.bt.leaudio.VandM", 3);

        Log.d(TAG, "start(): mPtsMediaAndVoice: " + mPtsMediaAndVoice +
                   ", mPtsTmapConfBandC: " + mPtsTmapConfBandC);

        if (mPtsMediaAndVoice == 2 && mPtsTmapConfBandC) {
            filter = new IntentFilter();
            filter.addAction(ACTION_LE_AUDIO_CONNECTION_TRIGGER);
            registerReceiver(mLeAudioServiceReceiver, filter);
        }

        return true;
    }

    @Override
    protected boolean stop() {
        Log.i(TAG, "stop()");
        if (sLeAudioService == null) {
            Log.w(TAG, "stop() called before start()");
            return true;
        }

        setActiveDevice(null);
        //Don't wait for async call with INACTIVE group status, clean active
        //device for active group.
        for (Map.Entry<Integer, LeAudioGroupDescriptor> entry : mGroupDescriptors.entrySet()) {
            LeAudioGroupDescriptor descriptor = entry.getValue();
            Integer group_id = entry.getKey();
            if (descriptor.mIsActive) {
                descriptor.mIsActive = false;
                updateActiveDevices(group_id, descriptor.mActiveContexts,
                        ACTIVE_CONTEXTS_NONE, descriptor.mIsActive);
                break;
            }
        }

        // Cleanup native interfaces
        //mLeAudioNativeInterface.cleanup();
        //mLeAudioNativeInterface = null;

        // Set the service and BLE devices as inactive
        setLeAudioService(null);

        // Unregister broadcast receivers
        unregisterReceiver(mBondStateChangedReceiver);
        mBondStateChangedReceiver = null;
        unregisterReceiver(mConnectionStateChangedReceiver);
        mConnectionStateChangedReceiver = null;
        unregisterReceiver(mActiveDeviceChangedReceiver);
        mActiveDeviceChangedReceiver = null;

        // Destroy state machines and stop handler thread
        /*synchronized (mStateMachines) {
            for (LeAudioStateMachine sm : mStateMachines.values()) {
                sm.doQuit();
                sm.cleanup();
            }
            mStateMachines.clear();
        }*/

        mDeviceGroupIdMap.clear();
        mDeviceAudioLocationMap.clear();
        mGroupDescriptors.clear();

        if (mBroadcastCallbacks != null) {
            mBroadcastCallbacks.kill();
        }

        if (mLeAudioCallbacks != null) {
            mLeAudioCallbacks.kill();
        }

        mBroadcastStateMap.clear();
        mBroadcastsPlaybackMap.clear();
        mBroadcastMetadataList.clear();

        /*if (mLeAudioBroadcasterNativeInterface != null) {
            mLeAudioBroadcasterNativeInterface.cleanup();
            mLeAudioBroadcasterNativeInterface = null;
        }*/

        /*if (mStateMachinesThread != null) {
            try {
                mStateMachinesThread.quitSafely();
                mStateMachinesThread.join(SM_THREAD_JOIN_TIMEOUT_MS);
                mStateMachinesThread = null;
            } catch (InterruptedException e) {
                // Do not rethrow as we are shutting down anyway
            }
        }*/

        mLeAudioCodecConfig = null;
        mAudioManager = null;
        mAdapterService = null;
        mAudioManager = null;

        return true;
    }

    @Override
    protected void cleanup() {
        Log.i(TAG, "cleanup()");
    }

    public static synchronized LeAudioService getLeAudioService() {
        if (sLeAudioService == null) {
            Log.w(TAG, "getLeAudioService(): service is NULL");
            return null;
        }
        if (!sLeAudioService.isAvailable()) {
            Log.w(TAG, "getLeAudioService(): service is not available");
            return null;
        }
        return sLeAudioService;
    }

    private static synchronized void setLeAudioService(LeAudioService instance) {
        if (DBG) {
            Log.d(TAG, "setLeAudioService(): set to: " + instance);
        }
        sLeAudioService = instance;
    }

    public boolean connect(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "connect(): " + device);
        }

        if (getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            Log.e(TAG, "Cannot connect to " + device + " : CONNECTION_POLICY_FORBIDDEN");
            return false;
        }
        ParcelUuid[] featureUuids = mAdapterService.getRemoteUuids(device);
        if (!ArrayUtils.contains(featureUuids, BluetoothUuid.LE_AUDIO)) {
            Log.e(TAG, "Cannot connect to " + device + " : Remote does not have LE_AUDIO UUID");
            return false;
        }

        /*synchronized (mStateMachines) {
            LeAudioStateMachine sm = getOrCreateStateMachine(device);
            if (sm == null) {
                Log.e(TAG, "Ignored connect request for " + device + " : no state machine");
                return false;
            }
            sm.sendMessage(LeAudioStateMachine.CONNECT);
        }*/

        // Connect other devices from this group
        //connectSet(device);

        CallAudioIntf mCallAudio = CallAudioIntf.get();
        MediaAudioIntf mMediaAudio = MediaAudioIntf.get();

        Log.d(TAG, "connect(): mPtsMediaAndVoice: " + mPtsMediaAndVoice +
                   ", mPtsTmapConfBandC: " + mPtsTmapConfBandC);

        if (!mPtsTmapConfBandC &&
            (mPtsMediaAndVoice == 2 || mPtsMediaAndVoice == 3)) {
            if (mCallAudio != null) {
                mCallAudio.connect(device);
            }
        }

        if (!mPtsTmapConfBandC &&
            (mPtsMediaAndVoice == 1 || mPtsMediaAndVoice == 3)) {
            if (mMediaAudio != null) {
                mMediaAudio.connect(device);
            }
        }

        return true;
    }

    /**
     * Disconnects LE Audio for the remote bluetooth device
     *
     * @param device is the device with which we would like to disconnect LE Audio
     * @return true if profile disconnected, false if device not connected over LE Audio
     */
    public boolean disconnect(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, "disconnect(): " + device);
        }

        // Disconnect this device
        /*synchronized (mStateMachines) {
            LeAudioStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                Log.e(TAG, "Ignored disconnect request for " + device
                        + " : no state machine");
                return false;
            }
            sm.sendMessage(LeAudioStateMachine.DISCONNECT);
        }

        // Disconnect other devices from this group
        int groupId = getGroupId(device);
        if (groupId != LE_AUDIO_GROUP_ID_INVALID) {
            for (BluetoothDevice storedDevice : mDeviceGroupIdMap.keySet()) {
                if (device.equals(storedDevice)) {
                    continue;
                }
                if (getGroupId(storedDevice) != groupId) {
                    continue;
                }
                synchronized (mStateMachines) {
                    LeAudioStateMachine sm = mStateMachines.get(storedDevice);
                    if (sm == null) {
                        Log.e(TAG, "Ignored disconnect request for " + storedDevice
                                + " : no state machine");
                        continue;
                    }
                    sm.sendMessage(LeAudioStateMachine.DISCONNECT);
                }
            }
        }*/

        //AcmServIntf mAcmService = AcmServIntf.get();
        //mAcmService.disconnect(device);
        CallAudioIntf mCallAudio = CallAudioIntf.get();
        MediaAudioIntf mMediaAudio = MediaAudioIntf.get();

        if (mMediaAudio != null) {
            mMediaAudio.disconnect(device);
        }

        if (mCallAudio != null) {
            mCallAudio.disconnect(device);
        }

        return true;
    }

    public List<BluetoothDevice> getConnectedDevices() {
        List<BluetoothDevice> devices = new ArrayList<>(0);
        AcmServIntf mAcmService = AcmServIntf.get();
        devices = mAcmService.getConnectedDevices();
        /*synchronized (mStateMachines) {
            List<BluetoothDevice> devices = new ArrayList<>();
            for (LeAudioStateMachine sm : mStateMachines.values()) {
                if (sm.isConnected()) {
                    devices.add(sm.getDevice());
                }
            }
            return devices;
        }*/
        return devices;
    }

    public BluetoothDevice getConnectedGroupLeadDevice(int groupId) {
         Log.w(TAG, "requested getConnectedGroupLeadDevice for groupId " + groupId);
         BluetoothDevice lead_device = null;
         if (groupId == LE_AUDIO_GROUP_ID_INVALID) {
             Log.w(TAG, "Invalid groupId requested return device " + lead_device);
             return lead_device;
         }

         ActiveDeviceManagerServiceIntf activeDeviceManager =
                 ActiveDeviceManagerServiceIntf.get();
         BluetoothDevice curr_active_device =
           activeDeviceManager.getActiveAbsoluteDevice(ApmConstIntf.AudioFeatures.MEDIA_AUDIO);

         Log.w(TAG, "req groupId " + groupId + " active group " + mLeActiveGroupID);

         if (groupId < INVALID_SET_ID) {
             lead_device = getFirstDeviceFromGroup(groupId);
             Log.w(TAG, "returning group lead device from CSIP group " + lead_device);
         } else {
             if (groupId == INVALID_SET_ID) {
                 if (mLeActiveGroupID >= INVALID_SET_ID) {
                     lead_device = curr_active_device;
                 } else {
                     // TO-DO: check for better way
                     if (mNonCSIPGruopID.size() > 0) {
                         for (int gid = NON_CSIP_MIN_GROUP_ID;gid <= NON_CSIP_MAX_GROUP_ID;++gid) {
                             List<BluetoothDevice> d = getGroupDevices(gid);
                             lead_device = d.isEmpty() ? null : d.get(0);
                             if (lead_device != null) {
                                 Log.w(TAG, "return 1st connected grp lead device " + lead_device +
                                         " for group id " + groupId + " placeholder gid " + gid);
                                 break;
                             }
                         }
                     }
                     Log.w(TAG, "returning group lead device from non-CSIP group " + lead_device);
                 }
             } else {
                 List<BluetoothDevice> d = getGroupDevices(groupId);
                 lead_device = d.isEmpty() ? null : d.get(0);
                 Log.w(TAG, "returning group lead device from non CSIP group " + lead_device);
             }
         }

         Log.w(TAG, "returning group lead device " + lead_device + " for group " + groupId);
         return lead_device;
   }

    public List<BluetoothDevice> getConnectedGroupLeadDevices() {
        List<BluetoothDevice> result = new ArrayList<>();
        List<BluetoothDevice> devices = getConnectedDevices();
        SortedSet<Integer> group = new TreeSet();
        for (BluetoothDevice dev : devices) {
            int groupId = getGroupId(dev);
            if (!group.contains(groupId)) {
                group.add(groupId);
                result.add(dev);
            }
        }
        /*for (Map.Entry<Integer, LeAudioGroupDescriptor> entry : mGroupDescriptors.entrySet()) {
            Integer groupId = entry.getKey();
            devices.add(getFirstDeviceFromGroup(groupId));
        }*/
        return result;
    }

    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        List<BluetoothDevice> devices = new ArrayList<>();

        /*if (states == null) {
            return devices;
        }
        final BluetoothDevice[] bondedDevices = mAdapterService.getBondedDevices();
        if (bondedDevices == null) {
            return devices;
        }

        synchronized (mStateMachines) {
            for (BluetoothDevice device : bondedDevices) {
                final ParcelUuid[] featureUuids = device.getUuids();
                if (!Utils.arrayContains(featureUuids, BluetoothUuid.LE_AUDIO)) {
                    continue;
                }
                int connectionState = BluetoothProfile.STATE_DISCONNECTED;
                LeAudioStateMachine sm = mStateMachines.get(device);
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
        }*/

        AcmServIntf mAcmService = AcmServIntf.get();
        devices = mAcmService.getDevicesMatchingConnectionStates(states);
        return devices;
    }

    /**
     * Get the list of devices that have state machines.
     *
     * @return the list of devices that have state machines
     */
    @VisibleForTesting
    List<BluetoothDevice> getDevices() {
        /*
        List<BluetoothDevice> devices = new ArrayList<>();
        synchronized (mStateMachines) {
            for (LeAudioStateMachine sm : mStateMachines.values()) {
                devices.add(sm.getDevice());
            }
            return devices;
        }*/
        List<BluetoothDevice> devices = new ArrayList<>();
        AcmServIntf mAcmService = AcmServIntf.get();
        devices = mAcmService.getDevices();
        return devices;
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
        /*synchronized (mStateMachines) {
            LeAudioStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return sm.getConnectionState();
        }*/

        AcmServIntf mAcmService = AcmServIntf.get();
        return mAcmService.getConnectionState(device);
    }

    /**
     * Add device to the given group.
     * @param groupId group ID the device is being added to
     * @param device the active device
     * @return true on success, otherwise false
     */
    public boolean groupAddNode(int groupId, BluetoothDevice device) {
        //return mLeAudioNativeInterface.groupAddNode(groupId, device);
        return false;
    }

    /**
     * Remove device from a given group.
     * @param groupId group ID the device is being removed from
     * @param device the active device
     * @return true on success, otherwise false
     */
    public boolean groupRemoveNode(int groupId, BluetoothDevice device) {
        //return mLeAudioNativeInterface.groupRemoveNode(groupId, device);
        return false;
    }

    /**
     * Checks if given group exists.
     * @param group_id group Id to verify
     * @return true given group exists, otherwise false
     */
    public boolean isValidDeviceGroup(int group_id) {
        /*return (group_id != LE_AUDIO_GROUP_ID_INVALID) ?
                mDeviceGroupIdMap.containsValue(group_id) :
                false; */
        return false;
    }

    /**
     * Get all the devices within a given group.
     * @param group_id group Id to verify
     * @return all devices within a given group or empty list
     */
    public List<BluetoothDevice> getGroupDevices(int group_id) {
        Log.d(TAG, "getGroupDevices for group ID " + group_id);
        List<BluetoothDevice> result = new ArrayList<>();
        if (group_id == LE_AUDIO_GROUP_ID_INVALID)
            return result;
        /*if (group_id != LE_AUDIO_GROUP_ID_INVALID) {
            for (BluetoothDevice storedDevice : mDeviceGroupIdMap.keySet()) {
                if (getGroupId(storedDevice) == group_id) {
                    result.add(storedDevice);
                }
            }
        }*/
        if (group_id >= INVALID_SET_ID) {
            for (Map.Entry<BluetoothDevice, Integer> entry : mDeviceGroupIdMap.entrySet()) {
                if (entry.getValue() == group_id) {
                    Log.d(TAG, "non-CSIP device " + entry.getKey());
                    result.add(entry.getKey());
                    break;
                }
            }
            return result;
        } else {
            Log.d(TAG, "CSIP device group ID " + group_id);
            return getCsipGroupMembers(group_id);
        }
    }

    /**
     * Get supported group audio direction from available context.
     *
     * @param activeContext bitset of active context to be matched with possible audio direction
     * support.
     * @return matched possible audio direction support masked bitset
     * {@link AUDIO_DIRECTION_INPUT_BIT} if input audio is supported
     * {@link AUDIO_DIRECTION_OUTPUT_BIT} if output audio is supported
     */
    private Integer getAudioDirectionsFromActiveContextsMap(Integer activeContexts) {
        Integer supportedAudioDirections = 0;

        /*if ((activeContexts & mContextSupportingInputAudio) != 0) {
          supportedAudioDirections |= AUDIO_DIRECTION_INPUT_BIT;
        }
        if ((activeContexts & mContextSupportingOutputAudio) != 0) {
          supportedAudioDirections |= AUDIO_DIRECTION_OUTPUT_BIT;
        }*/

        return supportedAudioDirections;
    }

    private Integer getActiveGroupId() {
        /*for (Map.Entry<Integer, LeAudioGroupDescriptor> entry : mGroupDescriptors.entrySet()) {
            LeAudioGroupDescriptor descriptor = entry.getValue();
            if (descriptor.mIsActive) {
                return entry.getKey();
            }
        }
        return LE_AUDIO_GROUP_ID_INVALID;*/
        return -1;
    }

    public void registerLeBroadcastCallback(IBluetoothLeBroadcastCallback callback,
            AttributionSource source, SynchronousResultReceiver receiver) {
        LeBroadcastServIntf leBroadcastService = LeBroadcastServIntf.get();
        leBroadcastService.registerLeBroadcastCallback(callback, source, receiver);
    }

    public void unregisterLeBroadcastCallback(IBluetoothLeBroadcastCallback callback,
            AttributionSource source, SynchronousResultReceiver receiver) {
        LeBroadcastServIntf leBroadcastService = LeBroadcastServIntf.get();
        leBroadcastService.unregisterLeBroadcastCallback(callback, source, receiver);
    }

    /**
     * Creates LeAudio Broadcast instance.
     * @param metadata metadata buffer with TLVs
     * @param audioProfile broadcast audio profile
     * @param broadcastCode optional code if broadcast should be encrypted
     */
    public void createBroadcast(BluetoothLeAudioContentMetadata metadata, byte[] broadcastCode) {
        /*if (mLeAudioBroadcasterNativeInterface == null) {
            Log.w(TAG, "Native interface not available.");
            return;
        }
        mLeAudioBroadcasterNativeInterface.createBroadcast(metadata.getRawMetadata(),
                BROADCAST_PROFILE_MEDIA, broadcastCode);*/
    }

    /**
     * Start LeAudio Broadcast instance.
     * @param contentMetadata metadata buffer with TLVs
     * @param broadcastCode optional code if broadcast should be encrypted
     * @param source attribute source for permission check
     */
    public void startBroadcast(BluetoothLeAudioContentMetadata contentMetadata,
            byte[] broadcastCode, AttributionSource source) {
        LeBroadcastServIntf leBroadcastService = LeBroadcastServIntf.get();
        leBroadcastService.startBroadcast(contentMetadata, broadcastCode, source);
    }

    /**
     * Updates LeAudio Broadcast instance metadata.
     * @param broadcastId broadcast instance identifier
     * @param contentMetadata metadata buffer with TLVs
     * @param source attribute source for permission check
     */
    public void updateBroadcast(int broadcastId,
            BluetoothLeAudioContentMetadata contentMetadata, AttributionSource source) {
        LeBroadcastServIntf leBroadcastService = LeBroadcastServIntf.get();
        leBroadcastService.updateBroadcast(broadcastId, contentMetadata, source);
    }

    /**
     * Stop LeAudio Broadcast instance.
     * @param broadcastId broadcast instance identifier
     * @param source attribute source for permission check
     */
    public void stopBroadcast(int broadcastId, AttributionSource source) {
        LeBroadcastServIntf leBroadcastService = LeBroadcastServIntf.get();
        leBroadcastService.stopBroadcast(broadcastId, source);
    }

    /**
     * Destroy LeAudio Broadcast instance.
     * @param broadcastId broadcast instance identifier
     */
    public void destroyBroadcast(int broadcastId) {
        /*if (mLeAudioBroadcasterNativeInterface == null) {
            Log.w(TAG, "Native interface not available.");
            return;
        }
        if (DBG) Log.d(TAG, "destroyBroadcast");
        mLeAudioBroadcasterNativeInterface.destroyBroadcast(broadcastId);*/
    }

    /**
     * Get LeAudio Broadcast id.
     * @param instanceId broadcast instance identifier
     */
    public void getBroadcastId(int instanceId) {
        /*if (mLeAudioBroadcasterNativeInterface == null) {
            Log.w(TAG, "Native interface not available.");
            return;
        }
        mLeAudioBroadcasterNativeInterface.getBroadcastId(instanceId);*/
    }

    /**
     * Checks if Broadcast instance is playing.
     * @param broadcastId broadcast instance identifier
     * @param source attribute source for permission check
     * @param receiver synchronously wait receiver for the response from client
     * @return true if it is playing, false otherwise
     */
    public boolean isPlaying(int broadcastId, AttributionSource source,
            SynchronousResultReceiver receiver) {
        LeBroadcastServIntf leBroadcastService = LeBroadcastServIntf.get();
        return leBroadcastService.isPlaying(broadcastId, source, receiver);
    }

    /**
     * Get all broadcast metadata.
     * @param source attribute source for permission check
     * @param receiver synchronously wait receiver for the response from client
     * @return list of broadcast source
     */
    public List<BluetoothLeBroadcastMetadata> getAllBroadcastMetadata(AttributionSource source,
            SynchronousResultReceiver receiver) {
        LeBroadcastServIntf leBroadcastService = LeBroadcastServIntf.get();
        return leBroadcastService.getAllBroadcastMetadata(source, receiver);
    }

    /**
     * Get the maximum number of supported simultaneous broadcasts.
     * @param source attribute source for permission check
     * @param receiver synchronously wait receiver for the response from client
     * @return maximum number of broadcasts
     */
    public int getMaximumNumberOfBroadcasts(AttributionSource source,
            SynchronousResultReceiver receiver) {
        LeBroadcastServIntf leBroadcastService = LeBroadcastServIntf.get();
        return leBroadcastService.getMaximumNumberOfBroadcasts(source, receiver);
    }

    private BluetoothDevice getFirstDeviceFromGroup(Integer groupId) {
         Log.d(TAG, "requested grp " + groupId + " map size " +  mDeviceGroupIdMap.size());
         BluetoothDevice first_group_device = null;
         if (groupId == LE_AUDIO_GROUP_ID_INVALID)
             return first_group_device;

         List<BluetoothDevice> devices = getConnectedDevices();
         Log.d(TAG, "connected devices size " +  devices.size());
         if (devices.isEmpty())
             return first_group_device;

         for (Map.Entry<BluetoothDevice, Integer> entry : mDeviceGroupIdMap.entrySet()) {
             Log.d(TAG, "Device " + entry.getKey() + " grp " + entry.getValue());
             if (entry.getValue() == groupId) {
                 if (groupId == mLeActiveGroupID) {
                     first_group_device = entry.getKey();
                     break;
                 } else {
                     if (devices.contains(entry.getKey())) {
                         first_group_device = entry.getKey();
                         break;
                    }
                 }
             }
         }
        /*if (groupId != LE_AUDIO_GROUP_ID_INVALID) {
            for(Map.Entry<BluetoothDevice, Integer> entry : mDeviceGroupIdMap.entrySet()) {
                if (entry.getValue() != groupId) {
                    continue;
                }

                LeAudioStateMachine sm = mStateMachines.get(entry.getKey());
                if (sm == null || sm.getConnectionState() != BluetoothProfile.STATE_CONNECTED) {
                    continue;
                }

                return entry.getKey();
            }
        }*/

        Log.d(TAG, "returning first group device as " + first_group_device);
        return first_group_device;
    }

    private boolean updateActiveInDevice(BluetoothDevice device, Integer groupId,
                                            Integer oldActiveContexts,
                                            Integer newActiveContexts) {
        /*Integer oldSupportedAudioDirections =
                getAudioDirectionsFromActiveContextsMap(oldActiveContexts);
        Integer newSupportedAudioDirections =
                getAudioDirectionsFromActiveContextsMap(newActiveContexts);

        boolean oldSupportedByDeviceInput = (oldSupportedAudioDirections
                & AUDIO_DIRECTION_INPUT_BIT) != 0;
        boolean newSupportedByDeviceInput = (newSupportedAudioDirections
                & AUDIO_DIRECTION_INPUT_BIT) != 0;*/

        /*
         * Do not update input if neither previous nor current device support input
         */
        /*if (!oldSupportedByDeviceInput && !newSupportedByDeviceInput) {
            Log.d(TAG, "updateActiveInDevice: Device does not support input.");
            return false;
        }

        if (device != null && mActiveAudioInDevice != null) {
            int previousGroupId = getGroupId(mActiveAudioInDevice);
            if (previousGroupId == groupId) {*/
                /* This is thes same group as aleady notified to the system.
                * Therefore do not change the device we have connected to the group,
                * unless, previous one is disconnected now
                */
                /*if (mActiveAudioInDevice.isConnected()) {
                    device = mActiveAudioInDevice;
                }
            } else {*/
                /* Mark old group as no active */
                /*LeAudioGroupDescriptor descriptor = mGroupDescriptors.get(previousGroupId);
                descriptor.mIsActive = false;
            }
        }

        BluetoothDevice previousInDevice = mActiveAudioInDevice;*/

        /*
         * Update input if:
         * - Device changed
         *     OR
         * - Device stops / starts supporting input
         */
        /*if (!Objects.equals(device, previousInDevice)
                || (oldSupportedByDeviceInput != newSupportedByDeviceInput)) {
            mActiveAudioInDevice = newSupportedByDeviceInput ? device : null;
            if (DBG) {
                Log.d(TAG, " handleBluetoothActiveDeviceChanged  previousInDevice: "
                            + previousInDevice + ", mActiveAudioInDevice" + mActiveAudioInDevice
                            + " isLeOutput: false");
            }
            mAudioManager.handleBluetoothActiveDeviceChanged(mActiveAudioInDevice,previousInDevice,
                    BluetoothProfileConnectionInfo.createLeAudioInfo(false, false));

            return true;
        }
        Log.d(TAG, "updateActiveInDevice: Nothing to do.");
        return false;*/
        return false;
    }

    private boolean updateActiveOutDevice(BluetoothDevice device, Integer groupId,
                                       Integer oldActiveContexts,
                                       Integer newActiveContexts) {
        /*Integer oldSupportedAudioDirections =
                getAudioDirectionsFromActiveContextsMap(oldActiveContexts);
        Integer newSupportedAudioDirections =
                getAudioDirectionsFromActiveContextsMap(newActiveContexts);

        boolean oldSupportedByDeviceOutput = (oldSupportedAudioDirections
                & AUDIO_DIRECTION_OUTPUT_BIT) != 0;
        boolean newSupportedByDeviceOutput = (newSupportedAudioDirections
                & AUDIO_DIRECTION_OUTPUT_BIT) != 0;*/

        /*
         * Do not update output if neither previous nor current device support output
         */
        /*if (!oldSupportedByDeviceOutput && !newSupportedByDeviceOutput) {
            Log.d(TAG, "updateActiveOutDevice: Device does not support output.");
            return false;
        }

        if (device != null && mActiveAudioOutDevice != null) {
            int previousGroupId = getGroupId(mActiveAudioOutDevice);
            if (previousGroupId == groupId) {*/
                /* This is the same group as already notified to the system.
                * Therefore do not change the device we have connected to the group,
                * unless, previous one is disconnected now
                */
                /*if (mActiveAudioOutDevice.isConnected()) {
                    device = mActiveAudioOutDevice;
                }
            } else {
                Log.i(TAG, " Switching active group from " + previousGroupId + " to " + groupId);*/
                /* Mark old group as no active */
                /*LeAudioGroupDescriptor descriptor = mGroupDescriptors.get(previousGroupId);
                descriptor.mIsActive = false;
            }
        }

        BluetoothDevice previousOutDevice = mActiveAudioOutDevice;*/

        /*
         * Update output if:
         * - Device changed
         *     OR
         * - Device stops / starts supporting output
         */
        /*if (!Objects.equals(device, previousOutDevice)
                || (oldSupportedByDeviceOutput != newSupportedByDeviceOutput)) {
            mActiveAudioOutDevice = newSupportedByDeviceOutput ? device : null;
            final boolean suppressNoisyIntent = (mActiveAudioOutDevice != null)
                    || (getConnectionState(previousOutDevice) == BluetoothProfile.STATE_CONNECTED);

            if (DBG) {
                Log.d(TAG, " handleBluetoothActiveDeviceChanged previousOutDevice: "
                            + previousOutDevice + ", mActiveOutDevice: " + mActiveAudioOutDevice
                            + " isLeOutput: true");
            }
            mAudioManager.handleBluetoothActiveDeviceChanged(mActiveAudioOutDevice,
                    previousOutDevice,
                    BluetoothProfileConnectionInfo.createLeAudioInfo(suppressNoisyIntent, true));
            return true;
        }
        Log.d(TAG, "updateActiveOutDevice: Nothing to do.");
        return false;*/
        return false;
    }

    /**
     * Report the active devices change to the active device manager and the media framework.
     * @param groupId id of group which devices should be updated
     * @param newActiveContexts new active contexts for group of devices
     * @param oldActiveContexts old active contexts for group of devices
     * @param isActive if there is new active group
     * @return true if group is active after change false otherwise.
     */
    private boolean updateActiveDevices(Integer groupId, Integer oldActiveContexts,
            Integer newActiveContexts, boolean isActive) {
        /*BluetoothDevice device = null;

        if (isActive) {
            device = getFirstDeviceFromGroup(groupId);
        }

        boolean outReplaced =
            updateActiveOutDevice(device, groupId, oldActiveContexts, newActiveContexts);
        boolean inReplaced =
            updateActiveInDevice(device, groupId, oldActiveContexts, newActiveContexts);

        if (outReplaced || inReplaced) {
            Intent intent = new Intent(BluetoothLeAudio.ACTION_LE_AUDIO_ACTIVE_DEVICE_CHANGED);
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, mActiveAudioOutDevice);
            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT
                    | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
            sendBroadcast(intent, BLUETOOTH_CONNECT);
        }

        return mActiveAudioOutDevice != null;*/
        return false;
    }

    /**
     * Set the active device group.
     * @param groupId group Id to set active
     */
    private void setActiveDeviceGroup(BluetoothDevice device) {
        /*int groupId = LE_AUDIO_GROUP_ID_INVALID;

        if (device != null) {
            groupId = mDeviceGroupIdMap.getOrDefault(device, LE_AUDIO_GROUP_ID_INVALID);
        }

        int currentlyActiveGroupId = getActiveGroupId();
        if (DBG) {
            Log.d(TAG, "setActiveDeviceGroup = " + groupId +
                       ", currentlyActiveGroupId = " + currentlyActiveGroupId +
                       ", device: " + device);
        }

        if (groupId == currentlyActiveGroupId) {
            Log.w(TAG, "group is already active");
            return;
        }

        mLeAudioNativeInterface.groupSetActive(groupId);*/
    }

    /**
     * Set the active group represented by device.
     *
     * @param device the new active device
     * @return true on success, otherwise false
     */
    public boolean setActiveDevice(BluetoothDevice device) {
        /*synchronized (mStateMachines) {
            // Clear active group
            if (device == null) {
                setActiveDeviceGroup(device);
                return true;
            }
            if (getConnectionState(device) != BluetoothProfile.STATE_CONNECTED) {
                Log.e(TAG, "setActiveDevice(" + device + "): failed because group device is not " +
                        "connected");
                return false;
            }
            setActiveDeviceGroup(device);
            return true;
        }*/
        ActiveDeviceManagerServiceIntf activeDeviceManager =
                                            ActiveDeviceManagerServiceIntf.get();
        activeDeviceManager.setActiveDevice(device,
                                            ApmConstIntf.AudioFeatures.CALL_AUDIO);
        activeDeviceManager.setActiveDevice(device,
                                            ApmConstIntf.AudioFeatures.MEDIA_AUDIO);
        return true;
    }

    public boolean setActiveDeviceBlocking(BluetoothDevice device) {
        Log.d(TAG, "setActiveDeviceBlocking() for device: " + device);
        ActiveDeviceManagerServiceIntf activeDeviceManager =
                                            ActiveDeviceManagerServiceIntf.get();
        activeDeviceManager.setActiveDeviceBlocking(device,
                                            ApmConstIntf.AudioFeatures.CALL_AUDIO);
        activeDeviceManager.setActiveDeviceBlocking(device,
                                            ApmConstIntf.AudioFeatures.MEDIA_AUDIO);
        return true;
    }

    /**
     * Get the active LE audio device.
     *
     * Note: When LE audio group is active, one of the Bluetooth device address
     * which belongs to the group, represents the active LE audio group.
     * Internally, this address is translated to LE audio group id.
     *
     * @return List of two elements. First element is an active output device
     *         and second element is an active input device.
     */
    public List<BluetoothDevice> getActiveDevices() {
        if (DBG) {
            Log.d(TAG, "getActiveDevices");
        }
        ArrayList<BluetoothDevice> activeDevices = new ArrayList<>();
        /*activeDevices.add(null);
        activeDevices.add(null);
        synchronized (mStateMachines) {
            int currentlyActiveGroupId = getActiveGroupId();
            if (currentlyActiveGroupId == LE_AUDIO_GROUP_ID_INVALID) {
                return activeDevices;
            }
                activeDevices.add(0, mActiveAudioOutDevice);
                activeDevices.add(1, mActiveAudioInDevice);
        }*/

        activeDevices.add(0, null);
        activeDevices.add(1, null);
        if (ApmConstIntf.getQtiLeAudioEnabled()) {
            Log.d(TAG, "QTI LeAudio is enabled, return empty list");
            return activeDevices;
        }

        ActiveDeviceManagerServiceIntf activeDeviceManager =
                                            ActiveDeviceManagerServiceIntf.get();
        mActiveAudioOutDevice =
            activeDeviceManager.getActiveAbsoluteDevice(ApmConstIntf.AudioFeatures.MEDIA_AUDIO);
        mActiveAudioInDevice =
            activeDeviceManager.getActiveAbsoluteDevice(ApmConstIntf.AudioFeatures.CALL_AUDIO);

        int ActiveAudioMediaProfile =
            activeDeviceManager.getActiveProfile(ApmConstIntf.AudioFeatures.MEDIA_AUDIO);
        int ActiveAudioCallProfile =
            activeDeviceManager.getActiveProfile(ApmConstIntf.AudioFeatures.CALL_AUDIO);

        if (ActiveAudioMediaProfile == ApmConst.AudioProfiles.TMAP_MEDIA ||
            ActiveAudioMediaProfile == ApmConst.AudioProfiles.BAP_MEDIA ||
            ActiveAudioMediaProfile == ApmConst.AudioProfiles.BAP_GCP ||
            ActiveAudioMediaProfile == ApmConst.AudioProfiles.BAP_GCP_VBC ||
            ActiveAudioCallProfile == ApmConst.AudioProfiles.TMAP_CALL ||
            ActiveAudioCallProfile == ApmConst.AudioProfiles.BAP_CALL)
            activeDevices.add(0, mActiveAudioOutDevice);

        if (ActiveAudioMediaProfile == ApmConst.AudioProfiles.BAP_RECORDING ||
            ActiveAudioMediaProfile == ApmConst.AudioProfiles.BAP_GCP_VBC ||
            ActiveAudioCallProfile == ApmConst.AudioProfiles.TMAP_CALL ||
            ActiveAudioCallProfile == ApmConst.AudioProfiles.BAP_CALL) {
            if ((ActiveAudioMediaProfile == ApmConst.AudioProfiles.BROADCAST_LE) &&
                (ActiveAudioCallProfile == ApmConst.AudioProfiles.TMAP_CALL ||
                 ActiveAudioCallProfile == ApmConst.AudioProfiles.BAP_CALL)) {
                activeDevices.add(0, mActiveAudioInDevice);
                activeDevices.add(1, mActiveAudioInDevice);
            } else {
                activeDevices.add(1, mActiveAudioInDevice);
            }
        }

        Log.d(TAG, "getActiveDevices: LeAudio devices: Out[" + activeDevices.get(0) +
                                              "] - In[" + activeDevices.get(1) + "]");

        return activeDevices;
    }

    public void onLeCodecConfigChange(BluetoothDevice device,
            BluetoothLeAudioCodecStatus codecStatus, int audioType) {

        int groupId = getGroupId(device);
        Log.d(TAG, "onLeCodecConfigChange(): device: " + device + ", groupId: " + groupId);

        if (groupId != LE_AUDIO_GROUP_ID_INVALID) {
            notifyUnicastCodecConfigChanged(groupId, codecStatus);
        }
    }

    /*void connectSet(BluetoothDevice device) {
        int groupId = getGroupId(device);
        if (groupId == LE_AUDIO_GROUP_ID_INVALID) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "connect() others from group id: " + groupId);
        }

        for (BluetoothDevice storedDevice : mDeviceGroupIdMap.keySet()) {
            if (device.equals(storedDevice)) {
                continue;
            }

            if (getGroupId(storedDevice) != groupId) {
                continue;
            }

            if (DBG) {
                Log.d(TAG, "connect(): " + device);
            }

            synchronized (mStateMachines) {
                 LeAudioStateMachine sm = getOrCreateStateMachine(storedDevice);
                 if (sm == null) {
                     Log.e(TAG, "Ignored connect request for " + storedDevice
                             + " : no state machine");
                     continue;
                 }
                 sm.sendMessage(LeAudioStateMachine.CONNECT);
             }
         }

    }

    // Suppressed since this is part of a local process
    @SuppressLint("AndroidFrameworkRequiresPermission")
    void messageFromNative(LeAudioStackEvent stackEvent) {
        Log.d(TAG, "Message from native: " + stackEvent);
        BluetoothDevice device = stackEvent.device;
        Intent intent = null;

        if (stackEvent.type == LeAudioStackEvent.EVENT_TYPE_CONNECTION_STATE_CHANGED) {
        // Some events require device state machine
            synchronized (mStateMachines) {
                LeAudioStateMachine sm = mStateMachines.get(device);
                if (sm == null) {
                    switch (stackEvent.valueInt1) {
                        case LeAudioStackEvent.CONNECTION_STATE_CONNECTED:
                        case LeAudioStackEvent.CONNECTION_STATE_CONNECTING:
                            sm = getOrCreateStateMachine(device);
                            // Incoming connection try to connect other devices from the group
                            connectSet(device);
                            break;
                        default:
                            break;
                    }
                }

                if (sm == null) {
                    Log.e(TAG, "Cannot process stack event: no state machine: " + stackEvent);
                    return;
                }

                sm.sendMessage(LeAudioStateMachine.STACK_EVENT, stackEvent);
                return;
            }
        } else if (stackEvent.type == LeAudioStackEvent.EVENT_TYPE_GROUP_NODE_STATUS_CHANGED) {
            int group_id = stackEvent.valueInt1;
            int node_status = stackEvent.valueInt2;

            Objects.requireNonNull(stackEvent.device,
                    "Device should never be null, event: " + stackEvent);

            switch (node_status) {
                case LeAudioStackEvent.GROUP_NODE_ADDED:
                    mDeviceGroupIdMap.put(device, group_id);
                    LeAudioGroupDescriptor descriptor = mGroupDescriptors.get(group_id);
                    if (descriptor == null) {
                        mGroupDescriptors.put(group_id, new LeAudioGroupDescriptor());
                    }
                    break;
                case LeAudioStackEvent.GROUP_NODE_REMOVED:
                    mDeviceGroupIdMap.remove(device);
                    if (mDeviceGroupIdMap.containsValue(group_id) == false) {
                        mGroupDescriptors.remove(group_id);
                    }
                    break;
                default:
                    break;
            }

            intent = new Intent(BluetoothLeAudio.ACTION_LE_AUDIO_GROUP_NODE_STATUS_CHANGED);
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
            intent.putExtra(BluetoothLeAudio.EXTRA_LE_AUDIO_GROUP_ID, group_id);
            intent.putExtra(BluetoothLeAudio.EXTRA_LE_AUDIO_GROUP_NODE_STATUS, node_status);
        } else if (stackEvent.type == LeAudioStackEvent.EVENT_TYPE_AUDIO_CONF_CHANGED) {
            int direction = stackEvent.valueInt1;
            int group_id = stackEvent.valueInt2;
            int snk_audio_location = stackEvent.valueInt3;
            int src_audio_location = stackEvent.valueInt4;
            int available_contexts = stackEvent.valueInt5;

            LeAudioGroupDescriptor descriptor = mGroupDescriptors.get(group_id);
            if (descriptor != null) {
                if (descriptor.mIsActive) {
                    updateActiveDevices(group_id, descriptor.mActiveContexts,
                                        available_contexts, descriptor.mIsActive);
                }
                descriptor.mActiveContexts = available_contexts;
            } else {
                Log.e(TAG, "no descriptors for group: " + group_id);
            }

            intent = new Intent(BluetoothLeAudio.ACTION_LE_AUDIO_CONF_CHANGED);
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
            intent.putExtra(BluetoothLeAudio.EXTRA_LE_AUDIO_GROUP_ID, group_id);
            intent.putExtra(BluetoothLeAudio.EXTRA_LE_AUDIO_DIRECTION, direction);
            intent.putExtra(BluetoothLeAudio.EXTRA_LE_AUDIO_SINK_LOCATION, snk_audio_location);
            intent.putExtra(BluetoothLeAudio.EXTRA_LE_AUDIO_SOURCE_LOCATION, src_audio_location);
            intent.putExtra(BluetoothLeAudio.EXTRA_LE_AUDIO_AVAILABLE_CONTEXTS, available_contexts);
        } else if (stackEvent.type == LeAudioStackEvent.EVENT_TYPE_GROUP_STATUS_CHANGED) {
            int group_id = stackEvent.valueInt1;
            int group_status = stackEvent.valueInt2;
            boolean send_intent = false;

            switch (group_status) {
                case LeAudioStackEvent.GROUP_STATUS_ACTIVE: {
                    LeAudioGroupDescriptor descriptor = mGroupDescriptors.get(group_id);
                    if (descriptor != null) {
                        if (!descriptor.mIsActive) {
                            descriptor.mIsActive = true;
                            updateActiveDevices(group_id, ACTIVE_CONTEXTS_NONE,
                                                descriptor.mActiveContexts, descriptor.mIsActive);
                            send_intent = true;
                        }
                    } else {
                        Log.e(TAG, "no descriptors for group: " + group_id);
                    }
                    break;
                }
                case LeAudioStackEvent.GROUP_STATUS_INACTIVE: {
                    LeAudioGroupDescriptor descriptor = mGroupDescriptors.get(group_id);
                    if (descriptor != null) {
                        if (descriptor.mIsActive) {
                            descriptor.mIsActive = false;
                            updateActiveDevices(group_id, descriptor.mActiveContexts,
                                    ACTIVE_CONTEXTS_NONE, descriptor.mIsActive);
                            send_intent = true;
                        }
                    } else {
                        Log.e(TAG, "no descriptors for group: " + group_id);
                    }
                    break;
                }
                default:
                    break;
            }

            if (send_intent) {
                intent = new Intent(BluetoothLeAudio.ACTION_LE_AUDIO_GROUP_STATUS_CHANGED);
                intent.putExtra(BluetoothLeAudio.EXTRA_LE_AUDIO_GROUP_ID, group_id);
                intent.putExtra(BluetoothLeAudio.EXTRA_LE_AUDIO_GROUP_STATUS, group_status);
            }

        } else if (stackEvent.type == LeAudioStackEvent.EVENT_TYPE_BROADCAST_CREATED) {
            // TODO: Implement
        } else if (stackEvent.type == LeAudioStackEvent.EVENT_TYPE_BROADCAST_DESTROYED) {
            // TODO: Implement
        } else if (stackEvent.type == LeAudioStackEvent.EVENT_TYPE_BROADCAST_STATE) {
            int instanceId = stackEvent.valueInt1;
            int state = stackEvent.valueInt2;

            if (state == LeAudioStackEvent.BROADCAST_STATE_STREAMING) {
                mBroadcastStateMap.put(instanceId, state);
                if (mBroadcastStateMap.size() == 1) {
                    if (!Objects.equals(device, mActiveAudioOutDevice)) {
                        BluetoothDevice previousDevice = mActiveAudioOutDevice;
                        mActiveAudioOutDevice = device;
                        mAudioManager.handleBluetoothActiveDeviceChanged(mActiveAudioOutDevice,
                                previousDevice,
                                BluetoothProfileConnectionInfo.createLeAudioInfo(false, true));
                    }
                }
            } else {
                mBroadcastStateMap.remove(instanceId);
                if (mBroadcastStateMap.size() == 0) {
                    if (Objects.equals(device, mActiveAudioOutDevice)) {
                        BluetoothDevice previousDevice = mActiveAudioOutDevice;
                        mActiveAudioOutDevice = null;
                        mAudioManager.handleBluetoothActiveDeviceChanged(mActiveAudioOutDevice,
                                previousDevice,
                                BluetoothProfileConnectionInfo.createLeAudioInfo(true, true));
                    }
                }
            }
            // TODO: Implement
        } else if (stackEvent.type == LeAudioStackEvent.EVENT_TYPE_BROADCAST_ID) {
            // TODO: Implement
        }

        if (intent != null) {
            sendBroadcast(intent, BLUETOOTH_CONNECT);
        }
    }

    private LeAudioStateMachine getOrCreateStateMachine(BluetoothDevice device) {
        if (device == null) {
            Log.e(TAG, "getOrCreateStateMachine failed: device cannot be null");
            return null;
        }
        synchronized (mStateMachines) {
            LeAudioStateMachine sm = mStateMachines.get(device);
            if (sm != null) {
                return sm;
            }
            // Limit the maximum number of state machines to avoid DoS attack
            if (mStateMachines.size() >= MAX_LE_AUDIO_STATE_MACHINES) {
                Log.e(TAG, "Maximum number of LeAudio state machines reached: "
                        + MAX_LE_AUDIO_STATE_MACHINES);
                return null;
            }
            if (DBG) {
                Log.d(TAG, "Creating a new state machine for " + device);
            }
            sm = LeAudioStateMachine.make(device, this,
                    mLeAudioNativeInterface, mStateMachinesThread.getLooper());
            mStateMachines.put(device, sm);
            return sm;
        }
    }*/

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

    /**
     * Process a change in the bonding state for a device.
     *
     * @param device the device whose bonding state has changed
     * @param bondState the new bond state for the device. Possible values are:
     * {@link BluetoothDevice#BOND_NONE},
     * {@link BluetoothDevice#BOND_BONDING},
     * {@link BluetoothDevice#BOND_BONDED}.
     */
    @VisibleForTesting
    void bondStateChanged(BluetoothDevice device, int bondState) {
        if (DBG) {
            Log.d(TAG, "Bond state changed for device: " + device + " state: " + bondState);
        }

        /*if (bondState == BluetoothDevice.BOND_NONE) {
            return;
        }

        // Remove state machine if the bonding for a device is removed
        int groupId = getGroupId(device);
        if (groupId != LE_AUDIO_GROUP_ID_INVALID) {
            // In case device is still in the group, let's remove it
            mLeAudioNativeInterface.groupRemoveNode(groupId, device);
        }

        synchronized (mStateMachines) {
            LeAudioStateMachine sm = mStateMachines.get(device);
            if (sm == null) {
                return;
            }
            if (sm.getConnectionState() != BluetoothProfile.STATE_DISCONNECTED) {
                return;
            }
            removeStateMachine(device);
        }*/

        AcmServIntf mAcmService = AcmServIntf.get();
        mAcmService.bondStateChanged(device, bondState);
    }

    /*private void removeStateMachine(BluetoothDevice device) {
        synchronized (mStateMachines) {
            LeAudioStateMachine sm = mStateMachines.get(device);
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
    }*/

    private List<BluetoothDevice> getConnectedPeerDevices(int groupId) {
        List<BluetoothDevice> result = new ArrayList<>();
        for (BluetoothDevice peerDevice : getConnectedDevices()) {
            if (getGroupId(peerDevice) == groupId) {
                result.add(peerDevice);
            }
        }
        return result;
    }

    private int getNonCsipGroupId() {
        int groupid = LE_AUDIO_GROUP_ID_INVALID;
        if (mNonCSIPGruopID.size() == 0) {
            groupid = NON_CSIP_MIN_GROUP_ID;
        } else if (mNonCSIPGruopID.last() < NON_CSIP_MAX_GROUP_ID) {
            groupid = mNonCSIPGruopID.last() + 1;
        } else if (mNonCSIPGruopID.first() > NON_CSIP_MIN_GROUP_ID) {
            groupid = mNonCSIPGruopID.first() - 1;
        } else {
            int first_groupid_avail = mNonCSIPGruopID.first() + 1;
            while (first_groupid_avail < NON_CSIP_MAX_GROUP_ID - 1) {
                if (!mNonCSIPGruopID.contains(first_groupid_avail)) {
                    groupid = first_groupid_avail;
                    break;
                }
                first_groupid_avail++;
            }
        }
        Log.d(TAG, "Returning non-CSIP group ID " + groupid);
        return groupid;
    }

    private List<BluetoothDevice> getCsipGroupMembers(int groupid) {
        CsipWrapper csipWrapper = CsipWrapper.getInstance();
        DeviceGroup set = csipWrapper.getCoordinatedSet(groupid);
        if (set != null)
            return set.getDeviceGroupMembers();
        return null;
    }

    @VisibleForTesting
    synchronized void connectionStateChanged(BluetoothDevice device, int fromState,
                                                     int toState) {
        Log.e(TAG, "connectionStateChanged: invocation. device=" + device
                + " fromState=" + fromState + " toState=" + toState);
        if ((device == null) || (fromState == toState)) {
            Log.e(TAG, "connectionStateChanged: unexpected invocation. device=" + device
                    + " fromState=" + fromState + " toState=" + toState);
            return;
        }
        if (toState == BluetoothProfile.STATE_CONNECTING) {
            Log.d(TAG, "connectionStateChanged as connecting for device " + device);
            CsipWrapper csipWrapper = CsipWrapper.getInstance();
            int groupId = csipWrapper.getRemoteDeviceGroupId(device, null);
            if (groupId != LE_AUDIO_GROUP_ID_INVALID) {
                if (groupId == INVALID_SET_ID) {
                    groupId = getNonCsipGroupId();
                    mNonCSIPGruopID.add(groupId);
                    Log.d(TAG, "non-CSIP device group ID " + groupId);
                } else {
                    Log.d(TAG, "CSIP device group ID " + groupId);
                }
                Log.d(TAG, "Add device " + device + " to group " + groupId);
                mDeviceGroupIdMap.put(device, groupId);
            }
        } else if (toState == BluetoothProfile.STATE_CONNECTED) {
            /*int myGroupId = getGroupId(device);
            if (myGroupId == LE_AUDIO_GROUP_ID_INVALID
                    || getConnectedPeerDevices(myGroupId).size() == 1) {
                // Log LE Audio connection event if we are the first device in a set
                // Or when the GroupId has not been found
                // MetricsLogger.logProfileConnectionEvent(
                //         BluetoothMetricsProto.ProfileId.LE_AUDIO);
            }

            LeAudioGroupDescriptor descriptor = mGroupDescriptors.get(myGroupId);
            if (descriptor != null) {
                descriptor.mIsConnected = true;
                /* HearingAid activates device after connection
                 * A2dp makes active device via activedevicemanager - connection intent
                 */
                //setActiveDevice(device);
            /*} else {
                Log.e(TAG, "no descriptors for group: " + myGroupId);
            }

            McpService mcpService = mServiceFactory.getMcpService();
            if (mcpService != null) {
                mcpService.setDeviceAuthorized(device, true);
            }*/
        }
        // Check if the device is disconnected - if unbond, remove the state machine
        if (toState == BluetoothProfile.STATE_DISCONNECTED) {
            Log.d(TAG, "connectionStateChanged as disconnected for device " + device);
            int bondState = mAdapterService.getBondState(device);
            if (bondState == BluetoothDevice.BOND_NONE) {
                if (DBG) {
                    Log.d(TAG, device + " is unbond. Remove state machine");
                }
                //removeStateMachine(device);
                AcmServIntf mAcmService = AcmServIntf.get();
                mAcmService.removeStateMachine(device);
            }
            int groupId = getGroupId(device);
            if (groupId == LE_AUDIO_GROUP_ID_INVALID) {
                Log.d(TAG, "Disconnect for Invalid group ID return ");
                return;
            }

            if (groupId >= INVALID_SET_ID) {
                Log.d(TAG, "non-CSIP remove device " + device + " group ID " + groupId);
                mDeviceGroupIdMap.remove(device);
                if (groupId > INVALID_SET_ID)
                    mNonCSIPGruopID.remove(groupId);
            } else {
                if (getConnectedPeerDevices(groupId).size() == 0) {
                    List<BluetoothDevice> removedevices = new ArrayList<BluetoothDevice>();
                    for (Map.Entry<BluetoothDevice,Integer> e : mDeviceGroupIdMap.entrySet()) {
                        if (e.getValue() == groupId) {
                            Log.d(TAG,"CSIP remove device " + e.getKey() + " grpID " + groupId);
                            removedevices.add(e.getKey());
                        }
                    }
                    for (BluetoothDevice dev : removedevices) {
                        mDeviceGroupIdMap.remove(dev);
                    }
                    removedevices.clear();
                }
            }

            //Todo
            //setActiveDevice(null);

            /*McpService mcpService = mServiceFactory.getMcpService();
            if (mcpService != null) {
                mcpService.setDeviceAuthorized(device, false);
            }

            int myGroupId = getGroupId(device);
            LeAudioGroupDescriptor descriptor = mGroupDescriptors.get(myGroupId);
            if (descriptor == null) {
                Log.e(TAG, "no descriptors for group: " + myGroupId);
                return;
            }

            if (getConnectedPeerDevices(myGroupId).isEmpty()){
                descriptor.mIsConnected = false;
                if (descriptor.mIsActive) {*/
                    /* Notify Native layer */
                    /*setActiveDevice(null);
                    descriptor.mIsActive = false;/*
                    /* Update audio framework */
                    /*updateActiveDevices(myGroupId,
                                    descriptor.mActiveContexts,
                                    descriptor.mActiveContexts,
                                    descriptor.mIsActive);
                    return;
                }
            }

            if (descriptor.mIsActive) {
                updateActiveDevices(myGroupId,
                                    descriptor.mActiveContexts,
                                    descriptor.mActiveContexts,
                                    descriptor.mIsActive);
            }*/
        }
    }

    synchronized void onActiveDeviceChanged(BluetoothDevice device) {
        mLeActiveGroupID = (device != null) ? getGroupId(device) : LE_AUDIO_GROUP_ID_INVALID;
        Log.d(TAG, "onActiveDeviceChanged device " + device + " groupID " + mLeActiveGroupID);
    }

    private class ConnectionStateChangedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (!BluetoothLeAudio.ACTION_LE_AUDIO_CONNECTION_STATE_CHANGED.equals(intent.getAction())) {
                return;
            }
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            int toState = intent.getIntExtra(BluetoothProfile.EXTRA_STATE, -1);
            int fromState = intent.getIntExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, -1);
            connectionStateChanged(device, fromState, toState);
        }
    }

    private class ActiveDeviceChangedReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if(!BluetoothLeAudio.ACTION_LE_AUDIO_ACTIVE_DEVICE_CHANGED.equals(intent.getAction())) {
                return;
            }
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            onActiveDeviceChanged(device);
        }
    }

    private final BroadcastReceiver mLeAudioServiceReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();

            Log.d(TAG, "action: " + action);
            if (action == null) {
                Log.w(TAG, "mHeadsetReceiver, action is null");
                return;
            }

            switch (action) {
                case ACTION_LE_AUDIO_CONNECTION_TRIGGER: {
                    String device1 = intent.getStringExtra("bd_add_1");
                    Log.d(TAG, "device Intent address: " + device1);
                    BluetoothDevice bddevice1 = null, bddevice2 = null;

                    mAdapter = BluetoothAdapter.getDefaultAdapter();
                    if (mAdapter != null) {
                        bddevice1 = mAdapter.getRemoteDevice(device1);
                    }

                    Log.d(TAG, "bddevice1: " + bddevice1 + ", bddevice2: " + bddevice2);

                    CallAudioIntf mCallAudio = CallAudioIntf.get();
                    MediaAudioIntf mMediaAudio = MediaAudioIntf.get();
                    Log.d(TAG, "connect(): mPtsMediaAndVoice: " + mPtsMediaAndVoice +
                               ", mPtsTmapConfBandC: " + mPtsTmapConfBandC);

                    if (mPtsTmapConfBandC && mPtsMediaAndVoice == 2) {
                        if (mCallAudio != null) {
                            mCallAudio.connect(bddevice1);
                        }
                    } break;
                }
            }
        }
    };

   /**
     * Check whether can connect to a peer device.
     * The check considers a number of factors during the evaluation.
     *
     * @param device the peer device to connect to
     * @return true if connection is allowed, otherwise false
     */
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

    /**
     * Get device audio location.
     * @param device LE Audio capable device
     * @return the sink audioi location that this device currently exposed
     */
    public int getAudioLocation(BluetoothDevice device) {
        /*if (device == null) {
            return BluetoothLeAudio.AUDIO_LOCATION_INVALID;
        }
        return mDeviceAudioLocationMap.getOrDefault(device,
                BluetoothLeAudio.AUDIO_LOCATION_INVALID);*/
        return -1;
    }

    /**
     * Set In Call state
     * @param inCall True if device in call (any state), false otherwise.
     */
    public void setInCall(boolean inCall) {
        // if (!mLeAudioNativeIsInitialized) {
        //     Log.e(TAG, "Le Audio not initialized properly.");
        //     return;
        // }
        // mLeAudioNativeInterface.setInCall(inCall);
    }

    /**
     * Set Inactive by HFP during handover
     */
    public void setInactiveForHfpHandover(BluetoothDevice hfpHandoverDevice) {
        if (!mLeAudioNativeIsInitialized) {
            Log.e(TAG, "Le Audio not initialized properly.");
            return;
        }
        if (getActiveGroupId() != LE_AUDIO_GROUP_ID_INVALID) {
            mHfpHandoverDevice = hfpHandoverDevice;
            setActiveDevice(null);
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
     * @param device the remote device
     * @param connectionPolicy is the connection policy to set to for this profile
     * @return true on success, otherwise false
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_PRIVILEGED)
    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (DBG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }

        if (!mDatabaseManager.setProfileConnectionPolicy(device, BluetoothProfile.LE_AUDIO,
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
    public int getConnectionPolicy(BluetoothDevice device) {
        return mDatabaseManager
                .getProfileConnectionPolicy(device, BluetoothProfile.LE_AUDIO);
    }

    /**
     * Get device group id. Devices with same group id belong to same group (i.e left and right
     * earbud)
     * @param device LE Audio capable device
     * @return group id that this device currently belongs to
     */
    public int getGroupId(BluetoothDevice device) {
        Log.d(TAG, "getGroupId device: " + device);
        if (device == null) {
            Log.d(TAG, "device is null");
            return LE_AUDIO_GROUP_ID_INVALID;
        }
        CsipWrapper csipWrapper = CsipWrapper.getInstance();
        int setId = csipWrapper.getRemoteDeviceGroupId(device, null);
        if (setId == INVALID_SET_ID)
            setId = mDeviceGroupIdMap.getOrDefault(device, INVALID_SET_ID);
        Log.d(TAG, "getGroupId device: " + device + " groupId: " + setId);
        return setId;
    }

    /**
     * Set the user application ccid along with used context type
     * @param userUuid user uuid
     * @param ccid content control id
     * @param contextType context type
     */
    public void setCcidInformation(ParcelUuid userUuid, int ccid, int contextType) {
        /* for the moment we care only for GMCS and GTBS */
        /*if (userUuid != BluetoothUuid.GENERIC_MEDIA_CONTROL
                && userUuid.getUuid() != TbsGatt.UUID_GTBS) {
            return;
        }
        if (!mLeAudioNativeIsInitialized) {
            Log.e(TAG, "Le Audio not initialized properly.");
            return;
        }
        mLeAudioNativeInterface.setCcidInformation(ccid, contextType);*/
    }

    /**
     * Set volume for streaming devices
     * @param volume volume to set
     */
    public void setVolume(int volume) {
        if (DBG) {
            Log.d(TAG, "SetVolume " + volume);
        }

        VolumeManagerIntf mVolumeManager = VolumeManagerIntf.get();
        mVolumeManager.setLeAudioVolume(volume);
    }

    private void notifyGroupNodeAdded(BluetoothDevice device, int groupId) {
        /*if (mLeAudioCallbacks != null) {
            int n = mLeAudioCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mLeAudioCallbacks.getBroadcastItem(i).onGroupNodeAdded(device, groupId);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mLeAudioCallbacks.finishBroadcast();
        }*/
    }

    private void notifyGroupNodeRemoved(BluetoothDevice device, int groupId) {
        /*if (mLeAudioCallbacks != null) {
            int n = mLeAudioCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mLeAudioCallbacks.getBroadcastItem(i).onGroupNodeRemoved(device, groupId);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mLeAudioCallbacks.finishBroadcast();
        }*/
    }

    private void notifyGroupStatusChanged(int groupId, int status) {
        /*if (mLeAudioCallbacks != null) {
            int n = mLeAudioCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mLeAudioCallbacks.getBroadcastItem(i).onGroupStatusChanged(groupId, status);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mLeAudioCallbacks.finishBroadcast();
        }*/
    }

    private void notifyUnicastCodecConfigChanged(int groupId,
                                                 BluetoothLeAudioCodecStatus status) {
        if (mLeAudioCallbacks != null) {
            int n = mLeAudioCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                Log.d(TAG, "notifyUnicastCodecConfigChanged ");
                try {
                    mLeAudioCallbacks.getBroadcastItem(i).onCodecConfigChanged(groupId, status);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mLeAudioCallbacks.finishBroadcast();
        }
    }

    private void notifyBroadcastStarted(Integer broadcastId, int reason) {
        /*if (mBroadcastCallbacks != null) {
            int n = mBroadcastCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mBroadcastCallbacks.getBroadcastItem(i).onBroadcastStarted(reason, broadcastId);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mBroadcastCallbacks.finishBroadcast();
        }*/
    }

    private void notifyBroadcastStartFailed(Integer broadcastId, int reason) {
        /*if (mBroadcastCallbacks != null) {
            int n = mBroadcastCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mBroadcastCallbacks.getBroadcastItem(i).onBroadcastStartFailed(reason);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mBroadcastCallbacks.finishBroadcast();
        }*/
    }

    private void notifyOnBroadcastStopped(Integer broadcastId, int reason) {
        /*if (mBroadcastCallbacks != null) {
            int n = mBroadcastCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mBroadcastCallbacks.getBroadcastItem(i).onBroadcastStopped(reason, broadcastId);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mBroadcastCallbacks.finishBroadcast();
        }*/
    }

    private void notifyOnBroadcastStopFailed(int reason) {
        /*if (mBroadcastCallbacks != null) {
            int n = mBroadcastCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mBroadcastCallbacks.getBroadcastItem(i).onBroadcastStopFailed(reason);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mBroadcastCallbacks.finishBroadcast();
        }*/
    }

    private void notifyPlaybackStarted(Integer broadcastId, int reason) {
        /*if (mBroadcastCallbacks != null) {
            int n = mBroadcastCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mBroadcastCallbacks.getBroadcastItem(i).onPlaybackStarted(reason, broadcastId);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mBroadcastCallbacks.finishBroadcast();
        }*/
    }

    private void notifyPlaybackStopped(Integer broadcastId, int reason) {
        /*if (mBroadcastCallbacks != null) {
            int n = mBroadcastCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mBroadcastCallbacks.getBroadcastItem(i).onPlaybackStopped(reason, broadcastId);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mBroadcastCallbacks.finishBroadcast();
        }*/
    }

    private void notifyBroadcastUpdated(int broadcastId, int reason) {
        /*if (mBroadcastCallbacks != null) {
            int n = mBroadcastCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mBroadcastCallbacks.getBroadcastItem(i).onBroadcastUpdated(reason, broadcastId);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mBroadcastCallbacks.finishBroadcast();
        }*/
    }

    private void notifyBroadcastUpdateFailed(int broadcastId, int reason) {
        /*if (mBroadcastCallbacks != null) {
            int n = mBroadcastCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mBroadcastCallbacks.getBroadcastItem(i)
                            .onBroadcastUpdateFailed(reason, broadcastId);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mBroadcastCallbacks.finishBroadcast();
        }*/
    }

    private void notifyBroadcastMetadataChanged(int broadcastId,
            BluetoothLeBroadcastMetadata metadata) {
        /*if (mBroadcastCallbacks != null) {
            int n = mBroadcastCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                try {
                    mBroadcastCallbacks.getBroadcastItem(i)
                            .onBroadcastMetadataChanged(broadcastId, metadata);
                } catch (RemoteException e) {
                    continue;
                }
            }
            mBroadcastCallbacks.finishBroadcast();
        }*/
    }

    /**
     * Gets the current codec status (configuration and capability).
     *
     * @param groupId the group id
     * @return the current codec status
     * @hide
     */
    public BluetoothLeAudioCodecStatus getCodecStatus(int groupId) {
        if (DBG) {
            Log.d(TAG, "getCodecStatus(" + groupId + ")");
        }
        /*LeAudioGroupDescriptor descriptor = mGroupDescriptors.get(groupId);
        if (descriptor != null) {
            return descriptor.mCodecStatus;
        }*/

        BluetoothLeAudioCodecStatus leAudioCodecStatus = null;
        MediaAudioIntf mMediaAudio = MediaAudioIntf.get();
        leAudioCodecStatus = mMediaAudio.getLeAudioCodecStatus(groupId);
        return leAudioCodecStatus;
    }

    /**
     * Sets the codec configuration preference.
     *
     * @param groupId the group id
     * @param inputCodecConfig the input codec configuration preference
     * @param outputCodecConfig the output codec configuration preference
     * @hide
     */
    public void setCodecConfigPreference(int groupId,
                                         BluetoothLeAudioCodecConfig inputCodecConfig,
                                         BluetoothLeAudioCodecConfig outputCodecConfig) {
        if (DBG) {
            Log.d(TAG, "setCodecConfigPreference(" + groupId + "): "
                    + Objects.toString(inputCodecConfig)
                    + Objects.toString(outputCodecConfig));
        }
        /*LeAudioGroupDescriptor descriptor = mGroupDescriptors.get(groupId);
        if (descriptor == null) {
            Log.e(TAG, "setCodecConfigPreference: Invalid groupId, " + groupId);
            return;
        }

        if (inputCodecConfig == null || outputCodecConfig == null) {
            Log.e(TAG, "setCodecConfigPreference: Codec config can't be null");
            return;
        }*/

        /* We support different configuration for input and output but codec type
         * shall be same */
        /*if (inputCodecConfig.getCodecType() != outputCodecConfig.getCodecType()) {
            Log.e(TAG, "setCodecConfigPreference: Input codec type: "
                    + inputCodecConfig.getCodecType()
                    + "does not match output codec type: " + outputCodecConfig.getCodecType());
            return;
        }

        if (descriptor.mCodecStatus == null) {
            Log.e(TAG, "setCodecConfigPreference: Codec status is null");
            return;
        }

        mLeAudioNativeInterface.setCodecConfigPreference(groupId,
                                inputCodecConfig, outputCodecConfig);*/

        MediaAudioIntf mMediaAudio = MediaAudioIntf.get();
        mMediaAudio.setLeAudioCodecConfigPreference(groupId,
                                                    inputCodecConfig,
                                                    outputCodecConfig);
    }


    /**
     * Binder object: must be a static class or memory leak may occur
     */
    @VisibleForTesting
    static class BluetoothLeAudioBinder extends IBluetoothLeAudio.Stub
            implements IProfileServiceBinder {
        private LeAudioService mService;

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
        private LeAudioService getService(AttributionSource source) {
            if (!Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkCallerIsSystemOrActiveOrManagedUser(mService, TAG)
                    || !Utils.checkConnectPermissionForDataDelivery(mService, source, TAG)) {
                return null;
            }
            return mService;
        }

        BluetoothLeAudioBinder(LeAudioService svc) {
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

                LeAudioService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
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
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                boolean defaultValue = false;
                if (service != null) {
                    defaultValue = service.disconnect(device);
                }
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

                LeAudioService service = getService(source);
                List<BluetoothDevice> defaultValue = new ArrayList<>(0);
                if (service != null) {
                    defaultValue = service.getConnectedDevices();
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectedGroupLeadDevice(int groupId, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                BluetoothDevice defaultValue = null;
                if (service != null) {
                    defaultValue = service.getConnectedGroupLeadDevice(groupId);
                }
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

                LeAudioService service = getService(source);
                List<BluetoothDevice> defaultValue = new ArrayList<>(0);
                if (service != null) {
                    defaultValue = service.getDevicesMatchingConnectionStates(states);
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
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                int defaultValue = BluetoothProfile.STATE_DISCONNECTED;
                if (service != null) {
                    defaultValue = service.getConnectionState(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setActiveDevice(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                boolean defaultValue = false;
                boolean defaultValueVoice = false;
                boolean defaultValueMedia = false;
                if (service != null) {
                    ActiveDeviceManagerServiceIntf activeDeviceManager =
                                                ActiveDeviceManagerServiceIntf.get();
                    defaultValueVoice = activeDeviceManager.setActiveDevice(device,
                                          ApmConstIntf.AudioFeatures.CALL_AUDIO, true);
                    defaultValueMedia = activeDeviceManager.setActiveDevice(device,
                                          ApmConstIntf.AudioFeatures.MEDIA_AUDIO, true);
                    defaultValue = (defaultValueVoice & defaultValueMedia);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getActiveDevices(AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                List<BluetoothDevice> defaultValue = new ArrayList<>();
                if (service != null) {
                    defaultValue = service.getActiveDevices();
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getAudioLocation(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                int defaultValue = BluetoothLeAudio.AUDIO_LOCATION_INVALID;
                if (service != null) {
                    enforceBluetoothPrivilegedPermission(service);
                    defaultValue = service.getAudioLocation(device);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setConnectionPolicy(BluetoothDevice device, int connectionPolicy,
                AttributionSource source, SynchronousResultReceiver receiver) {
            Objects.requireNonNull(device, "device cannot be null");
            Objects.requireNonNull(source, "source cannot be null");
            Objects.requireNonNull(receiver, "receiver cannot be null");

            try {
                LeAudioService service = getService(source);
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
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                int defaultValue = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
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
        public void setCcidInformation(ParcelUuid userUuid, int ccid, int contextType,
                AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(userUuid, "userUuid cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                service.setCcidInformation(userUuid, ccid, contextType);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getGroupId(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                int defaultValue = LE_AUDIO_GROUP_ID_INVALID;
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                defaultValue = service.getGroupId(device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void groupAddNode(int group_id, BluetoothDevice device,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                boolean defaultValue = false;
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.groupAddNode(group_id, device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setInCall(boolean inCall, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                service.setInCall(inCall);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setInactiveForHfpHandover(BluetoothDevice hfpHandoverDevice,
                AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                service.setInactiveForHfpHandover(hfpHandoverDevice);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void groupRemoveNode(int groupId, BluetoothDevice device,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(device, "device cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                boolean defaultValue = false;
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                defaultValue = service.groupRemoveNode(groupId, device);
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setVolume(int volume, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                if (service == null) {
                    throw new IllegalStateException("service is null");
                }
                enforceBluetoothPrivilegedPermission(service);
                service.setVolume(volume);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void registerCallback(IBluetoothLeAudioCallback callback,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(callback, "callback cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                if ((service == null) || (service.mLeAudioCallbacks == null)) {
                    throw new IllegalStateException("Service is unavailable: " + service);
                }

                enforceBluetoothPrivilegedPermission(service);
                service.mLeAudioCallbacks.register(callback);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void unregisterCallback(IBluetoothLeAudioCallback callback,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                Objects.requireNonNull(callback, "callback cannot be null");
                Objects.requireNonNull(source, "source cannot be null");
                Objects.requireNonNull(receiver, "receiver cannot be null");

                LeAudioService service = getService(source);
                if ((service == null) || (service.mLeAudioCallbacks == null)) {
                    throw new IllegalStateException("Service is unavailable");
                }

                enforceBluetoothPrivilegedPermission(service);

                service.mLeAudioCallbacks.unregister(callback);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void registerLeBroadcastCallback(IBluetoothLeBroadcastCallback callback,
                AttributionSource source, SynchronousResultReceiver receiver) {
            LeAudioService service = getService(source);
            if ((service == null)) {
                receiver.propagateException(new IllegalStateException("Service is unavailable"));
                return;
            }

            try {
                enforceBluetoothPrivilegedPermission(service);
                service.registerLeBroadcastCallback(callback, source, receiver);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void unregisterLeBroadcastCallback(IBluetoothLeBroadcastCallback callback,
                AttributionSource source, SynchronousResultReceiver receiver) {
            LeAudioService service = getService(source);
            if ((service == null)) {
                receiver.propagateException(new IllegalStateException("Service is unavailable"));
                return;
            }

            try {
                enforceBluetoothPrivilegedPermission(service);
                service.unregisterLeBroadcastCallback(callback, source, receiver);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void startBroadcast(BluetoothLeAudioContentMetadata contentMetadata,
                byte[] broadcastCode, AttributionSource source) {
            LeAudioService service = getService(source);
            if (service != null) {
                enforceBluetoothPrivilegedPermission(service);
                service.startBroadcast(contentMetadata, broadcastCode, source);
            }
        }

        @Override
        public void stopBroadcast(int broadcastId, AttributionSource source) {
            LeAudioService service = getService(source);
            if (service != null) {
                enforceBluetoothPrivilegedPermission(service);
                service.stopBroadcast(broadcastId, source);
            }
        }

        @Override
        public void updateBroadcast(int broadcastId,
                BluetoothLeAudioContentMetadata contentMetadata, AttributionSource source) {
            LeAudioService service = getService(source);
            if (service != null) {
                enforceBluetoothPrivilegedPermission(service);
                service.updateBroadcast(broadcastId, contentMetadata, source);
            }
        }

        @Override
        public void isPlaying(int broadcastId, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                boolean defaultValue = false;
                LeAudioService service = getService(source);
                if (service != null) {
                    enforceBluetoothPrivilegedPermission(service);
                    defaultValue = service.isPlaying(broadcastId,
                            source, receiver);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getAllBroadcastMetadata(AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                List<BluetoothLeBroadcastMetadata> defaultValue = new ArrayList<>();
                LeAudioService service = getService(source);
                if (service != null) {
                    enforceBluetoothPrivilegedPermission(service);
                    defaultValue = service.getAllBroadcastMetadata(source, receiver);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getMaximumNumberOfBroadcasts(AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                int defaultValue = 0;
                LeAudioService service = getService(source);
                if (service != null) {
                    enforceBluetoothPrivilegedPermission(service);
                    defaultValue = service.getMaximumNumberOfBroadcasts(
                            source, receiver);
                }
                receiver.send(defaultValue);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getCodecStatus(int groupId,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                LeAudioService service = getService(source);
                BluetoothLeAudioCodecStatus codecStatus = null;
                if (service != null) {
                    enforceBluetoothPrivilegedPermission(service);
                    codecStatus = service.getCodecStatus(groupId);
                }
                receiver.send(codecStatus);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setCodecConfigPreference(int groupId,
                BluetoothLeAudioCodecConfig inputCodecConfig,
                BluetoothLeAudioCodecConfig outputCodecConfig,
                AttributionSource source) {
            LeAudioService service = getService(source);
            if (service == null) {
                return;
            }

            enforceBluetoothPrivilegedPermission(service);
            service.setCodecConfigPreference(groupId, inputCodecConfig, outputCodecConfig);
        }
    }

    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        ProfileService.println(sb, "State machines: ");
        /*for (LeAudioStateMachine sm : mStateMachines.values()) {
            sm.dump(sb);
        }
        ProfileService.println(sb, "Active Groups information: ");
        ProfileService.println(sb, "  currentlyActiveGroupId: " + getActiveGroupId());
        ProfileService.println(sb, "  mActiveAudioOutDevice: " + mActiveAudioOutDevice);
        ProfileService.println(sb, "  mActiveAudioInDevice: " + mActiveAudioInDevice);

        for (Map.Entry<Integer, LeAudioGroupDescriptor> entry : mGroupDescriptors.entrySet()) {
            LeAudioGroupDescriptor descriptor = entry.getValue();
            Integer groupId = entry.getKey();
            ProfileService.println(sb, "  Group: " + groupId);
            ProfileService.println(sb, "    isActive: " + descriptor.mIsActive);
            ProfileService.println(sb, "    isConnected: " + descriptor.mIsConnected);
            ProfileService.println(sb, "    mActiveContexts: " + descriptor.mActiveContexts);
            ProfileService.println(sb, "    group lead: " + getFirstDeviceFromGroup(groupId));
        }*/
    }
}
