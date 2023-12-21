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

package com.android.bluetooth.avrcpcontroller;

import android.annotation.RequiresPermission;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothAvrcpPlayerSettings;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.IBluetoothAvrcpController;
import android.content.AttributionSource;
import android.content.Intent;
import android.media.MediaDescription;
import android.media.browse.MediaBrowser.MediaItem;
import android.media.session.PlaybackState;
import android.os.Bundle;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.ProfileService;
import com.android.modules.utils.SynchronousResultReceiver;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;

/**
 * Provides Bluetooth AVRCP Controller profile, as a service in the Bluetooth application.
 */
public class AvrcpControllerService extends ProfileService {
    static final String TAG = "AvrcpControllerService";
    static final int MAXIMUM_CONNECTED_DEVICES = 5;
    static final boolean DBG = true;
    static final boolean VDBG = true;

    public static final String MEDIA_ITEM_UID_KEY = "media-item-uid-key";
    /*
     *  Play State Values from JNI
     */
    private static final byte JNI_PLAY_STATUS_STOPPED = 0x00;
    private static final byte JNI_PLAY_STATUS_PLAYING = 0x01;
    private static final byte JNI_PLAY_STATUS_PAUSED = 0x02;
    private static final byte JNI_PLAY_STATUS_FWD_SEEK = 0x03;
    private static final byte JNI_PLAY_STATUS_REV_SEEK = 0x04;
    private static final byte JNI_PLAY_STATUS_ERROR = -1;

    /* Folder/Media Item scopes.
     * Keep in sync with AVRCP 1.6 sec. 6.10.1
     */
    public static final byte BROWSE_SCOPE_PLAYER_LIST = 0x00;
    public static final byte BROWSE_SCOPE_VFS = 0x01;
    public static final byte BROWSE_SCOPE_SEARCH = 0x02;
    public static final byte BROWSE_SCOPE_NOW_PLAYING = 0x03;

    /* Folder navigation directions
     * This is borrowed from AVRCP 1.6 spec and must be kept with same values
     */
    public static final byte FOLDER_NAVIGATION_DIRECTION_UP = 0x00;
    public static final byte FOLDER_NAVIGATION_DIRECTION_DOWN = 0x01;

    /*
     * KeyCoded for Pass Through Commands
     */
    public static final int PASS_THRU_CMD_ID_PLAY = 0x44;
    public static final int PASS_THRU_CMD_ID_PAUSE = 0x46;
    public static final int PASS_THRU_CMD_ID_VOL_UP = 0x41;
    public static final int PASS_THRU_CMD_ID_VOL_DOWN = 0x42;
    public static final int PASS_THRU_CMD_ID_STOP = 0x45;
    public static final int PASS_THRU_CMD_ID_FF = 0x49;
    public static final int PASS_THRU_CMD_ID_REWIND = 0x48;
    public static final int PASS_THRU_CMD_ID_FORWARD = 0x4B;
    public static final int PASS_THRU_CMD_ID_BACKWARD = 0x4C;

    /* Key State Variables */
    public static final int KEY_STATE_PRESSED = 0;
    public static final int KEY_STATE_RELEASED = 1;

    public static final String ACTION_TRACK_EVENT =
             "android.bluetooth.avrcp-controller.profile.action.TRACK_EVENT";
    public static final String EXTRA_PLAYBACK =
            "android.bluetooth.avrcp-controller.profile.extra.PLAYBACK";



    static BrowseTree sBrowseTree;
    private static AvrcpControllerService sService;
    private final BluetoothAdapter mAdapter;

    protected Map<BluetoothDevice, AvrcpControllerStateMachine> mDeviceStateMap =
            new ConcurrentHashMap<>(1);

    static {
        classInitNative();
    }

    public AvrcpControllerService() {
        super();
        mAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    @Override
    protected boolean start() {
        initNative();
        sBrowseTree = new BrowseTree(null);
        sService = this;

        // Start the media browser service.
        Intent startIntent = new Intent(this, BluetoothMediaBrowserService.class);
        startService(startIntent);
        return true;
    }

    @Override
    protected boolean stop() {
        Intent stopIntent = new Intent(this, BluetoothMediaBrowserService.class);
        stopService(stopIntent);
        for (AvrcpControllerStateMachine stateMachine : mDeviceStateMap.values()) {
            stateMachine.doQuit();
        }

        sService = null;
        sBrowseTree = null;
        return true;
    }

    public static AvrcpControllerService getAvrcpControllerService() {
        return sService;
    }

    protected AvrcpControllerStateMachine newStateMachine(BluetoothDevice device) {
        return new AvrcpControllerStateMachine(device, this);
    }

    private void refreshContents(BrowseTree.BrowseNode node) {
        if (node.mDevice == null) {
            return;
        }
        AvrcpControllerStateMachine stateMachine = getStateMachine(node.mDevice);
        if (stateMachine != null) {
            stateMachine.requestContents(node);
        }
    }

    /*Java API*/

    /**
     * Get a List of MediaItems that are children of the specified media Id
     *
     * @param parentMediaId The player or folder to get the contents of
     * @return List of Children if available, an empty list if there are none,
     * or null if a search must be performed.
     */
    public synchronized List<MediaItem> getContents(String parentMediaId) {
        if (DBG) Log.d(TAG, "getContents(" + parentMediaId + ")");

        BrowseTree.BrowseNode requestedNode = sBrowseTree.findBrowseNodeByID(parentMediaId);
        if (requestedNode == null) {
            for (AvrcpControllerStateMachine stateMachine : mDeviceStateMap.values()) {
                requestedNode = stateMachine.findNode(parentMediaId);
                if (requestedNode != null) {
                    Log.d(TAG, "Found a node");
                    break;
                }
            }
        }

        if (requestedNode == null) {
            if (DBG) Log.d(TAG, "Didn't find a node");
            return null;
        } else {
            if (!requestedNode.isCached()) {
                if (DBG) Log.d(TAG, "node is not cached");
                refreshContents(requestedNode);
            }
            if (DBG) Log.d(TAG, "Returning contents");
            return requestedNode.getContents();
        }
    }

    @Override
    protected IProfileServiceBinder initBinder() {
        return new AvrcpControllerServiceBinder(this);
    }

    //Binder object: Must be static class or memory leak may occur
    private static class AvrcpControllerServiceBinder extends IBluetoothAvrcpController.Stub
            implements IProfileServiceBinder {
        private AvrcpControllerService mService;

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
        private AvrcpControllerService getService(AttributionSource source) {
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

        AvrcpControllerServiceBinder(AvrcpControllerService service) {
            mService = service;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public void getConnectedDevices(AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                AvrcpControllerService service = getService(source);
                List<BluetoothDevice> defaultValue = new ArrayList<BluetoothDevice>(0);
                if (service != null) {
                    defaultValue = service.getConnectedDevices();
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
                AvrcpControllerService service = getService(source);
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
        public void getConnectionState(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                AvrcpControllerService service = getService(source);
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
        public void sendGroupNavigationCmd(BluetoothDevice device, int keyCode, int keyState,
                AttributionSource source, SynchronousResultReceiver receiver) {
            Log.w(TAG, "sendGroupNavigationCmd not implemented");
            return;
        }

        @Override
        public void setPlayerApplicationSetting(BluetoothAvrcpPlayerSettings settings,
                AttributionSource source, SynchronousResultReceiver receiver) {
            Log.w(TAG, "setPlayerApplicationSetting not implemented");
            return;
        }

        @Override
        public void getPlayerSettings(BluetoothDevice device,
                AttributionSource source, SynchronousResultReceiver receiver) {
            Log.w(TAG, "getPlayerSettings not implemented");
            return;
        }
    }


    /* JNI API*/
    // Called by JNI when a passthrough key was received.
    private void handlePassthroughRsp(int id, int keyState, byte[] address) {
        if (DBG) {
            Log.d(TAG, "passthrough response received as: key: " + id
                    + " state: " + keyState + "address:" + address);
        }
    }

    private void handleGroupNavigationRsp(int id, int keyState) {
        if (DBG) {
            Log.d(TAG, "group navigation response received as: key: " + id + " state: "
                    + keyState);
        }
    }

    // Called by JNI when a device has connected or disconnected.
    private synchronized void onConnectionStateChanged(boolean remoteControlConnected,
            boolean browsingConnected, byte[] address) {
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        if (DBG) {
            Log.d(TAG, "onConnectionStateChanged " + remoteControlConnected + " "
                    + browsingConnected + device);
        }
        if (device == null) {
            Log.e(TAG, "onConnectionStateChanged Device is null");
            return;
        }

        StackEvent event =
                StackEvent.connectionStateChanged(remoteControlConnected, browsingConnected);
        AvrcpControllerStateMachine stateMachine = getOrCreateStateMachine(device);
        if (stateMachine == null) {
            Log.e(TAG, "onConnectionStateChanged: mAvrcpCtSm is null, return");
            return;
        }

        if (remoteControlConnected || browsingConnected) {
            stateMachine.connect(event);
        } else {
            stateMachine.disconnect();
        }
    }

    // Called by JNI to notify Avrcp of features supported by the Remote device.
    private void getRcFeatures(byte[] address, int features, int caPsm) {
        /*Log.i(TAG, " getRcFeatures caPsm :" + caPsm);
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        Message msg = mAvrcpCtSm.obtainMessage(
            AvrcpControllerStateMachine.MESSAGE_PROCESS_RC_FEATURES, features, caPsm, device);
        mAvrcpCtSm.sendMessage(msg); */
    }

    // Called by JNI
    private void setPlayerAppSettingRsp(byte[] address, byte accepted) {
        /* Do Nothing. */
    }

    // Called by JNI when remote wants to receive absolute volume notifications.
    private synchronized void handleRegisterNotificationAbsVol(byte[] address, byte label) {
        if (DBG) {
            Log.d(TAG, "handleRegisterNotificationAbsVol");
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_REGISTER_ABS_VOL_NOTIFICATION,
                    (int) label);
        }
    }

    // Called by JNI when remote wants to set absolute volume.
    private synchronized void handleSetAbsVolume(byte[] address, byte absVol, byte label) {
        if (DBG) {
            Log.d(TAG, "handleSetAbsVolume ");
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_SET_ABS_VOL_CMD,
                    absVol, label);
        }
    }

    // Called by JNI when a track changes and local AvrcpController is registered for updates.
    private synchronized void onTrackChanged(byte[] address, byte numAttributes, int[] attributes,
            String[] attribVals) {
        if (DBG) {
            Log.d(TAG, "onTrackChanged");
        }

        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_TRACK_CHANGED,
                    TrackInfo.getMetadata(attributes, attribVals));
        }

    }

     private void onElementAttributeUpdate(byte[] address, byte numAttributes, int[] attributes,
            String[] attribVals) {

     }

    // Called by JNI periodically based upon timer to update play position
    private synchronized void onPlayPositionChanged(byte[] address, int songLen,
            int currSongPosition) {
        if (DBG) {
            Log.d(TAG, "onPlayPositionChanged pos " + currSongPosition);
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_PLAY_POS_CHANGED,
                    songLen, currSongPosition);
        }
    }

    // Called by JNI on changes of play status
    private synchronized void onPlayStatusChanged(byte[] address, byte playStatus) {
        if (DBG) {
            Log.d(TAG, "onPlayStatusChanged " + playStatus);
        }
        int playbackState = PlaybackState.STATE_NONE;
        switch (playStatus) {
            case JNI_PLAY_STATUS_STOPPED:
                playbackState = PlaybackState.STATE_STOPPED;
                break;
            case JNI_PLAY_STATUS_PLAYING:
                playbackState = PlaybackState.STATE_PLAYING;
                break;
            case JNI_PLAY_STATUS_PAUSED:
                playbackState = PlaybackState.STATE_PAUSED;
                break;
            case JNI_PLAY_STATUS_FWD_SEEK:
                playbackState = PlaybackState.STATE_FAST_FORWARDING;
                break;
            case JNI_PLAY_STATUS_REV_SEEK:
                playbackState = PlaybackState.STATE_REWINDING;
                break;
            default:
                playbackState = PlaybackState.STATE_NONE;
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_PLAY_STATUS_CHANGED, playbackState);
        }
    }

    // Called by JNI to report remote Player's capabilities
    private synchronized void handlePlayerAppSetting(byte[] address, byte[] playerAttribRsp,
            int rspLen) {
        if (DBG) {
            Log.d(TAG, "handlePlayerAppSetting rspLen = " + rspLen);
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            PlayerApplicationSettings supportedSettings =
                    PlayerApplicationSettings.makeSupportedSettings(playerAttribRsp);
        }
        /* Do nothing */

    }

    private synchronized void onPlayerAppSettingChanged(byte[] address, byte[] playerAttribRsp,
            int rspLen) {
        if (DBG) {
            Log.d(TAG, "onPlayerAppSettingChanged ");
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {

            PlayerApplicationSettings desiredSettings =
                    PlayerApplicationSettings.makeSettings(playerAttribRsp);
        }
        /* Do nothing */
    }

    private void onAvailablePlayerChanged(byte[] address) {
        if (DBG) {
            Log.d(TAG," onAvailablePlayerChanged");
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_AVAILABLE_PLAYER_CHANGED);
        }
    }

    // Browsing related JNI callbacks.
    void handleGetFolderItemsRsp(byte[] address, int status, MediaItem[] items) {
        if (DBG) {
            Log.d(TAG, "handleGetFolderItemsRsp called with status " + status + " items "
                    + items.length + " items.");
        }
        for (MediaItem item : items) {
            if (VDBG) {
                Log.d(TAG, "media item: " + item + " uid: "
                        + item.getDescription().getMediaId());
            }
        }
        ArrayList<MediaItem> itemsList = new ArrayList<>();
        for (MediaItem item : items) {
            itemsList.add(item);
        }

        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {

            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_GET_FOLDER_ITEMS,
                    itemsList);
        }
    }


    void handleGetPlayerItemsRsp(byte[] address, AvrcpPlayer[] items) {
        if (DBG) {
            Log.d(TAG, "handleGetFolderItemsRsp called with " + items.length + " items.");
        }

        for (AvrcpPlayer item : items) {
            if (VDBG) {
                Log.d(TAG, "bt player item: " + item);
            }
        }
        List<AvrcpPlayer> itemsList = new ArrayList<>();
        for (AvrcpPlayer p : items) {
            itemsList.add(p);
        }

        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_GET_PLAYER_ITEMS,
                    itemsList);
        }
    }

    // JNI Helper functions to convert native objects to java.
    MediaItem createFromNativeMediaItem(long uid, int type, String name, int[] attrIds,
            String[] attrVals) {
        if (VDBG) {
            Log.d(TAG, "createFromNativeMediaItem uid: " + uid + " type " + type + " name "
                    + name + " attrids " + attrIds + " attrVals " + attrVals);
        }
        MediaDescription.Builder mdb = new MediaDescription.Builder();

        Bundle mdExtra = new Bundle();
        mdExtra.putLong(MEDIA_ITEM_UID_KEY, uid);
        mdb.setExtras(mdExtra);


        // Generate a random UUID. We do this since database unaware TGs can send multiple
        // items with same MEDIA_ITEM_UID_KEY.
        mdb.setMediaId(UUID.randomUUID().toString());
        // Concise readable name.
        mdb.setTitle(name);

        // We skip the attributes since we can query them using UID for the item above
        // Also MediaDescription does not give an easy way to provide this unless we pass
        // it as an MediaMetadata which is put inside the extras.
        return new MediaItem(mdb.build(), MediaItem.FLAG_PLAYABLE);
    }

    MediaItem createFromNativeFolderItem(long uid, int type, String name, int playable) {
        if (VDBG) {
            Log.d(TAG, "createFromNativeFolderItem uid: " + uid + " type " + type + " name "
                    + name + " playable " + playable);
        }
        MediaDescription.Builder mdb = new MediaDescription.Builder();

        Bundle mdExtra = new Bundle();
        mdExtra.putLong(MEDIA_ITEM_UID_KEY, uid);
        mdb.setExtras(mdExtra);

        // Generate a random UUID. We do this since database unaware TGs can send multiple
        // items with same MEDIA_ITEM_UID_KEY.
        mdb.setMediaId(UUID.randomUUID().toString());
        // Concise readable name.
        mdb.setTitle(name);

        return new MediaItem(mdb.build(), MediaItem.FLAG_BROWSABLE);
    }

    AvrcpPlayer createFromNativePlayerItem(int id, String name, byte[] transportFlags,
            int playStatus, int playerType) {
        if (VDBG) {
            Log.d(TAG,
                    "createFromNativePlayerItem name: " + name + " transportFlags "
                            + transportFlags + " play status " + playStatus + " player type "
                            + playerType);
        }
        AvrcpPlayer player = new AvrcpPlayer(id, name, transportFlags, playStatus, playerType);
        return player;
    }

    private void handleChangeFolderRsp(byte[] address, int count) {
        if (DBG) {
            Log.d(TAG, "handleChangeFolderRsp count: " + count);
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);
        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_FOLDER_PATH,
                    count);
        }
    }

    private void handleSetBrowsedPlayerRsp(byte[] address, int items, int depth) {
        if (DBG) {
            Log.d(TAG, "handleSetBrowsedPlayerRsp depth: " + depth);
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(AvrcpControllerStateMachine.MESSAGE_PROCESS_SET_BROWSED_PLAYER,
                    items, depth);
        }
    }

    private void handleSetAddressedPlayerRsp(byte[] address, int status) {
        if (DBG) {
            Log.d(TAG, "handleSetAddressedPlayerRsp status: " + status);
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_SET_ADDRESSED_PLAYER);
        }
    }

    private void handleAddressedPlayerChanged(byte[] address, int id) {
        if (DBG) {
            Log.d(TAG, "handleAddressedPlayerChanged id: " + id);
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.sendMessage(
                    AvrcpControllerStateMachine.MESSAGE_PROCESS_ADDRESSED_PLAYER_CHANGED, id);
        }
    }

    private void handleNowPlayingContentChanged(byte[] address) {
        if (DBG) {
            Log.d(TAG, "handleNowPlayingContentChanged");
        }
        BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);

        AvrcpControllerStateMachine stateMachine = getStateMachine(device);
        if (stateMachine != null) {
            stateMachine.nowPlayingContentChanged();
        }
    }

    /* Generic Profile Code */

    /**
     * Disconnect the given Bluetooth device.
     *
     * @return true if disconnect is successful, false otherwise.
     */
    public synchronized boolean disconnect(BluetoothDevice device) {
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "MAP disconnect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }
        AvrcpControllerStateMachine stateMachine = mDeviceStateMap.get(device);
        // a map state machine instance doesn't exist. maybe it is already gone?
        if (stateMachine == null) {
            return false;
        }
        int connectionState = stateMachine.getState();
        if (connectionState != BluetoothProfile.STATE_CONNECTED
                && connectionState != BluetoothProfile.STATE_CONNECTING) {
            return false;
        }
        stateMachine.disconnect();
        if (DBG) {
            StringBuilder sb = new StringBuilder();
            dump(sb);
            Log.d(TAG, "MAP disconnect device: " + device
                    + ", InstanceMap start state: " + sb.toString());
        }
        return true;
    }

    void removeStateMachine(AvrcpControllerStateMachine stateMachine) {
        mDeviceStateMap.remove(stateMachine.getDevice());
    }

    public List<BluetoothDevice> getConnectedDevices() {
        return getDevicesMatchingConnectionStates(new int[]{BluetoothAdapter.STATE_CONNECTED});
    }

    protected AvrcpControllerStateMachine getStateMachine(BluetoothDevice device) {
        return mDeviceStateMap.get(device);
    }

    protected AvrcpControllerStateMachine getOrCreateStateMachine(BluetoothDevice device) {
        AvrcpControllerStateMachine stateMachine = mDeviceStateMap.get(device);
        if (stateMachine == null) {
            stateMachine = newStateMachine(device);
            mDeviceStateMap.put(device, stateMachine);
            Log.d(TAG, "AvrcpControllerStateMachine start() called: " + device);
            stateMachine.start();
            Log.d(TAG, "AvrcpcontrollerSM started for device: " + device);
            stateMachine.registerReceiver(device);
        }
        return stateMachine;
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

    synchronized int getConnectionState(BluetoothDevice device) {
        AvrcpControllerStateMachine stateMachine = mDeviceStateMap.get(device);
        return (stateMachine == null) ? BluetoothProfile.STATE_DISCONNECTED
                : stateMachine.getState();
    }

    public void onDeviceUpdated(BluetoothDevice device) {
        AvrcpControllerStateMachine stateMachine = mDeviceStateMap.get(device);
        if (stateMachine != null) {
            Log.d(TAG, "Send device: " + device + " updated meassage to Avrcpstatemachine");
            stateMachine.sendMessage(AvrcpControllerStateMachine.MSG_DEVICE_UPDATED,
                    device);
        }
    }

    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        ProfileService.println(sb, "Devices Tracked = " + mDeviceStateMap.size());

        for (AvrcpControllerStateMachine stateMachine : mDeviceStateMap.values()) {
            ProfileService.println(sb,
                    "==== StateMachine for " + stateMachine.getDevice() + " ====");
            stateMachine.dump(sb);
        }
        sb.append("\n  sBrowseTree: " + sBrowseTree.toString());
    }

    /*JNI*/
    private static native void classInitNative();

    private native void initNative();

    private native void cleanupNative();

    public native boolean sendPassThroughCommandNative(byte[] address, int keyCode, int keyState);

    static native boolean sendGroupNavigationCommandNative(byte[] address, int keyCode,
            int keyState);

    static native void setPlayerApplicationSettingValuesNative(byte[] address, byte numAttrib,
            byte[] atttibIds, byte[] attribVal);

    /* This api is used to send response to SET_ABS_VOL_CMD */
    static native void sendAbsVolRspNative(byte[] address, int absVol, int label);

    /* This api is used to inform remote for any volume level changes */
    static native void sendRegisterAbsVolRspNative(byte[] address, byte rspType, int absVol,
            int label);

    /* API used to fetch the playback state */
    static native void getPlaybackStateNative(byte[] address);

    /* API used to fetch the current now playing list */
    static native void getNowPlayingListNative(byte[] address, int start, int end);

    /* API used to fetch the current folder's listing */
    static native void getFolderListNative(byte[] address, int start, int end);

    /* API used to fetch the listing of players */
    static native void getPlayerListNative(byte[] address, int start, int end);

    /* API used to change the folder */
    static native void changeFolderPathNative(byte[] address, byte direction, long uid);

    static native void playItemNative(byte[] address, byte scope, long uid, int uidCounter);

    static native void setBrowsedPlayerNative(byte[] address, int playerId);

    static native void setAddressedPlayerNative(byte[] address, int playerId);

    /* This api is used to fetch ElementAttributes */
    native static void getElementAttributesNative(byte[] address, byte numAttributes,
                                                   byte[] attribIds);
}
