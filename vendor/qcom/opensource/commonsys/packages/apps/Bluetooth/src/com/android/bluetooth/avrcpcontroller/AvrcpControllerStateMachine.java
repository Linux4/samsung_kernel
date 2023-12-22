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

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothAvrcpController;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.browse.MediaBrowser.MediaItem;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Bundle;
import android.os.Message;
import android.util.Log;
import android.util.SparseArray;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.R;
import com.android.bluetooth.Utils;
import com.android.bluetooth.a2dpsink.A2dpSinkService;
import com.android.bluetooth.btservice.MetricsLogger;
import com.android.bluetooth.btservice.ProfileService;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;

import java.util.ArrayList;
import java.util.List;
/**
 * Provides Bluetooth AVRCP Controller State Machine responsible for all remote control connections
 * and interactions with a remote controlable device.
 */
class AvrcpControllerStateMachine extends StateMachine {
    static final String TAG = "AvrcpControllerStateMachine";
    static final boolean DBG = true;

    //0->99 Events from Outside
    public static final int CONNECT = 1;
    public static final int DISCONNECT = 2;
    public static final int MSG_DEVICE_UPDATED = 3;

    //100->199 Internal Events
    protected static final int CLEANUP = 100;
    private static final int CONNECT_TIMEOUT = 101;

    //200->299 Events from Native
    static final int STACK_EVENT = 200;
    static final int MESSAGE_INTERNAL_CMD_TIMEOUT = 201;

    static final int MESSAGE_PROCESS_SET_ABS_VOL_CMD = 203;
    static final int MESSAGE_PROCESS_REGISTER_ABS_VOL_NOTIFICATION = 204;
    static final int MESSAGE_PROCESS_TRACK_CHANGED = 205;
    static final int MESSAGE_PROCESS_PLAY_POS_CHANGED = 206;
    static final int MESSAGE_PROCESS_PLAY_STATUS_CHANGED = 207;
    static final int MESSAGE_PROCESS_VOLUME_CHANGED_NOTIFICATION = 208;
    static final int MESSAGE_PROCESS_GET_FOLDER_ITEMS = 209;
    static final int MESSAGE_PROCESS_GET_FOLDER_ITEMS_OUT_OF_RANGE = 210;
    static final int MESSAGE_PROCESS_GET_PLAYER_ITEMS = 211;
    static final int MESSAGE_PROCESS_FOLDER_PATH = 212;
    static final int MESSAGE_PROCESS_SET_BROWSED_PLAYER = 213;
    static final int MESSAGE_PROCESS_SET_ADDRESSED_PLAYER = 214;
    static final int MESSAGE_PROCESS_ADDRESSED_PLAYER_CHANGED = 215;
    static final int MESSAGE_PROCESS_NOW_PLAYING_CONTENTS_CHANGED = 216;
    static final int MESSAGE_PROCESS_SUPPORTED_APPLICATION_SETTINGS = 217;
    static final int MESSAGE_PROCESS_CURRENT_APPLICATION_SETTINGS = 218;
    static final int MESSAGE_PROCESS_AVAILABLE_PLAYER_CHANGED = 219;

    //300->399 Events for Browsing
    static final int MESSAGE_GET_FOLDER_ITEMS = 300;
    static final int MESSAGE_PLAY_ITEM = 301;
    static final int MSG_AVRCP_PASSTHRU = 302;

    static final int MESSAGE_INTERNAL_ABS_VOL_TIMEOUT = 404;

    /*
     * Base value for absolute volume from JNI
     */
    private static final int ABS_VOL_BASE = 127;

    /*
     * Notification types for Avrcp protocol JNI.
     */
    private static final byte NOTIFICATION_RSP_TYPE_INTERIM = 0x00;
    private static final byte NOTIFICATION_RSP_TYPE_CHANGED = 0x01;

    private final AudioManager mAudioManager;
    protected final BluetoothDevice mDevice;
    private BluetoothDevice mA2dpDevice = null;
    protected final byte[] mDeviceAddress;
    protected final AvrcpControllerService mService;
    protected final Disconnected mDisconnected;
    protected final Connecting mConnecting;
    protected final Connected mConnected;
    protected final Disconnecting mDisconnecting;
    private A2dpSinkService mA2dpSinkService;

    protected int mMostRecentState = BluetoothProfile.STATE_DISCONNECTED;

    boolean mRemoteControlConnected = false;
    boolean mBrowsingConnected = false;
    final BrowseTree mBrowseTree;
    private boolean smActive = false;
    private AvrcpPlayer mAddressedPlayer = new AvrcpPlayer();
    private RemoteDevice mRemoteDevice;
    private int mPreviousPercentageVol = -1;
    private int mAddressedPlayerId = -1;
    private SparseArray<AvrcpPlayer> mAvailablePlayerList = new SparseArray<AvrcpPlayer>();
    // Only accessed from State Machine processMessage
    private int mVolumeChangedNotificationsToIgnore = 0;

    GetFolderList mGetFolderList = null;

    //Number of items to get in a single fetch
    static final int ITEM_PAGE_SIZE = 20;
    static final int CMD_TIMEOUT_MILLIS = 10000;
    static final int ABS_VOL_TIMEOUT_MILLIS = 1000; //1s

    AvrcpControllerStateMachine(BluetoothDevice device, AvrcpControllerService service) {
        super(TAG);
        setDbg(DBG);
        mDevice = device;
        mDeviceAddress = Utils.getByteAddress(mDevice);
        mService = service;
        logD(device.toString());

        mBrowseTree = new BrowseTree(mDevice);
        mDisconnected = new Disconnected();
        mConnecting = new Connecting();
        mConnected = new Connected();
        mDisconnecting = new Disconnecting();

        addState(mDisconnected);
        addState(mConnecting);
        addState(mConnected);
        addState(mDisconnecting);

        smActive = true;
        mGetFolderList = new GetFolderList();
        addState(mGetFolderList, mConnected);

        mRemoteDevice = new RemoteDevice(device);

        mAudioManager = (AudioManager) service.getSystemService(Context.AUDIO_SERVICE);

        Log.d(TAG, "Setting initial state: Disconnected: " + mDevice);
        setInitialState(mDisconnected);
    }

    BrowseTree.BrowseNode findNode(String parentMediaId) {
        logD("FindNode");
        return mBrowseTree.findBrowseNodeByID(parentMediaId);
    }

    /**
     * Get the current connection state
     *
     * @return current State
     */
    public int getState() {
        return mMostRecentState;
    }

    /**
     * Get the underlying device tracked by this state machine
     *
     * @return device in focus
     */
    public synchronized BluetoothDevice getDevice() {
        return mDevice;
    }

    /**
     * send the connection event asynchronously
     */
    public boolean connect(StackEvent event) {
        if (event.mBrowsingConnected) {
            onBrowsingConnected();
        }
        mRemoteControlConnected = event.mRemoteControlConnected;
        sendMessage(CONNECT);
        return true;
    }

    /**
     * send the Disconnect command asynchronously
     */
    public void disconnect() {
        sendMessage(DISCONNECT);
    }

    /**
     * Dump the current State Machine to the string builder.
     *
     * @param sb output string
     */
    public void dump(StringBuilder sb) {
        ProfileService.println(sb, "mDevice: " + mDevice.getAddress() + "("
                + mDevice.getName() + ") " + this.toString());
    }

    @Override
    protected void unhandledMessage(Message msg) {
        Log.w(TAG, "Unhandled message in state " + getCurrentState() + "msg.what=" + msg.what);
    }

    private static void logD(String message) {
        if (DBG) {
            Log.d(TAG, message);
        }
    }

    synchronized void onBrowsingConnected() {
        if (mBrowsingConnected || (!smActive)) return;
        mService.sBrowseTree.mRootNode.addChild(mBrowseTree.mRootNode);
        BluetoothMediaBrowserService.notifyChanged(mService
                .sBrowseTree.mRootNode);
        BluetoothMediaBrowserService.notifyChanged(mAddressedPlayer.getPlaybackState());
        mBrowsingConnected = true;
    }

    synchronized void onBrowsingDisconnected() {
        if (!mBrowsingConnected || (!smActive)) return;
        mAddressedPlayer.setPlayStatus(PlaybackState.STATE_ERROR);
        mAddressedPlayer.updateCurrentTrack(null);
        if (mBrowseTree != null && mBrowseTree.mNowPlayingNode != null) {
            mBrowseTree.mNowPlayingNode.setCached(false);
            BluetoothMediaBrowserService.notifyChanged(mBrowseTree.mNowPlayingNode);
        }
        PlaybackState.Builder pbb = new PlaybackState.Builder();
        pbb.setState(PlaybackState.STATE_ERROR, PlaybackState.PLAYBACK_POSITION_UNKNOWN,
                1.0f).setActions(0);
        pbb.setErrorMessage(mService.getString(R.string.bluetooth_disconnected));
        BluetoothMediaBrowserService.notifyChanged(pbb.build());
        if (mBrowseTree != null && mBrowseTree.mRootNode != null) {
            mService.sBrowseTree.mRootNode.removeChild(
                     mBrowseTree.mRootNode);
            BluetoothMediaBrowserService.notifyChanged(mService.sBrowseTree.mRootNode);
        }
        BluetoothMediaBrowserService.trackChanged(null);
        mBrowsingConnected = false;
    }

    private void notifyChanged(BrowseTree.BrowseNode node) {
        BluetoothMediaBrowserService.notifyChanged(node);
    }

    void requestContents(BrowseTree.BrowseNode node) {
        sendMessage(MESSAGE_GET_FOLDER_ITEMS, node);

        logD("Fetching " + node);
    }

    void nowPlayingContentChanged() {
        mBrowseTree.mNowPlayingNode.setCached(false);
        sendMessage(MESSAGE_GET_FOLDER_ITEMS, mBrowseTree.mNowPlayingNode);
    }

    protected class Disconnected extends State {
        @Override
        public void enter() {
            logD("Enter Disconnected mDevice:" + mDevice);
            if (mMostRecentState != BluetoothProfile.STATE_DISCONNECTED) {
                sendMessage(CLEANUP);
            }
            mA2dpDevice = null;
            broadcastConnectionStateChanged(BluetoothProfile.STATE_DISCONNECTED);
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CONNECT:
                    logD("Connect");
                    transitionTo(mConnecting);
                    break;
                case CLEANUP:
                    mService.removeStateMachine(AvrcpControllerStateMachine.this);
                    doQuit();
                    break;
            }
            return true;
        }
    }

    protected class Connecting extends State {
        @Override
        public void enter() {
            logD("Enter Connecting");
            broadcastConnectionStateChanged(BluetoothProfile.STATE_CONNECTING);
            transitionTo(mConnected);
        }
    }


    class Connected extends State {
        private static final String STATE_TAG = "Avrcp.ConnectedAvrcpController";
        private int mCurrentlyHeldKey = 0;

        @Override
        public void enter() {
            if (mMostRecentState == BluetoothProfile.STATE_CONNECTING) {
                BluetoothDevice device =
                        BluetoothAdapter.getDefaultAdapter().getRemoteDevice(mDeviceAddress);
                mA2dpDevice = device;
                broadcastConnectionStateChanged(BluetoothProfile.STATE_CONNECTED);
                BluetoothMediaBrowserService.addressedPlayerChanged(mSessionCallbacks);
            } else {
                logD("ReEnteringConnected");
            }
            super.enter();
        }

        @Override
        public boolean processMessage(Message msg) {
            logD(STATE_TAG + " processMessage " + msg.what);
            switch (msg.what) {
                case MESSAGE_PROCESS_SET_ABS_VOL_CMD:
                    mVolumeChangedNotificationsToIgnore++;
                    removeMessages(MESSAGE_INTERNAL_ABS_VOL_TIMEOUT);
                    sendMessageDelayed(MESSAGE_INTERNAL_ABS_VOL_TIMEOUT,
                            ABS_VOL_TIMEOUT_MILLIS);
                    setAbsVolume(msg.arg1, msg.arg2);
                    return true;

                case MESSAGE_GET_FOLDER_ITEMS:
                    transitionTo(mGetFolderList);
                    return true;

                case MESSAGE_PLAY_ITEM:
                    //Set Addressed Player
                    playItem((BrowseTree.BrowseNode) msg.obj);
                    return true;

                case MSG_AVRCP_PASSTHRU:
                    passThru(msg.arg1);
                    return true;

                case MESSAGE_PROCESS_TRACK_CHANGED:
                    mAddressedPlayer.updateCurrentTrack((MediaMetadata) msg.obj);
                    BluetoothMediaBrowserService.trackChanged((MediaMetadata) msg.obj);
                    return true;

                case MESSAGE_PROCESS_PLAY_STATUS_CHANGED:
                     int status = msg.arg1;
                     mAddressedPlayer.setPlayStatus(status);
                     BluetoothMediaBrowserService.notifyChanged(mAddressedPlayer.getPlaybackState());
                     broadcastPlayBackStateChanged(mAddressedPlayer.getPlaybackState());
                     BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(mDeviceAddress);
                     mA2dpSinkService = A2dpSinkService.getA2dpSinkService();
                    if (mA2dpSinkService == null) {
                        Log.e(TAG, "mA2dpSinkService is null, return");
                    }
                    if (status == PlaybackState.STATE_PLAYING) {
                        mA2dpSinkService.informTGStatePlaying(device, true);
                    } else if (status == PlaybackState.STATE_PAUSED
                            || status == PlaybackState.STATE_STOPPED) {
                        mA2dpSinkService.informTGStatePlaying(device, false);
                    }
                    return true;

                case MESSAGE_PROCESS_PLAY_POS_CHANGED:
                    if (msg.arg2 != -1) {
                        mAddressedPlayer.setPlayTime(msg.arg2);

                        BluetoothMediaBrowserService.notifyChanged(
                                mAddressedPlayer.getPlaybackState());
                    }
                    return true;

                case MESSAGE_PROCESS_ADDRESSED_PLAYER_CHANGED:
                    mAddressedPlayerId = msg.arg1;
                    logD("AddressedPlayer = " + mAddressedPlayerId);
                    AvrcpPlayer updatedPlayer = mAvailablePlayerList.get(mAddressedPlayerId);
                    if (updatedPlayer != null) {
                        mAddressedPlayer = updatedPlayer;
                        logD("AddressedPlayer = " + mAddressedPlayer.getName());
                    } else {
                        mBrowseTree.mRootNode.setCached(false);
                        mBrowseTree.mRootNode.setExpectedChildren(255);
                        BluetoothMediaBrowserService.notifyChanged(mBrowseTree.mRootNode);
                    }
                    return true;

                // case MESSAGE_PROCESS_SUPPORTED_APPLICATION_SETTINGS:
                //     mAddressedPlayer.setSupportedPlayerApplicationSettings(
                //             (PlayerApplicationSettings) msg.obj);
                //     BluetoothMediaBrowserService.notifyChanged(mAddressedPlayer.getPlaybackState());
                //     return true;
                //
                // case MESSAGE_PROCESS_CURRENT_APPLICATION_SETTINGS:
                //     mAddressedPlayer.setCurrentPlayerApplicationSettings(
                //             (PlayerApplicationSettings) msg.obj);
                //     BluetoothMediaBrowserService.notifyChanged(mAddressedPlayer.getPlaybackState());
                //     return true;

                case MESSAGE_PROCESS_AVAILABLE_PLAYER_CHANGED:
                    processAvailablePlayerChanged();
                    return true;

                case DISCONNECT:
                    transitionTo(mDisconnecting);
                    return true;

                case MSG_DEVICE_UPDATED:
                    msgDeviceUpdated((BluetoothDevice)msg.obj);
                    return true;

                case MESSAGE_PROCESS_REGISTER_ABS_VOL_NOTIFICATION: {
                    mRemoteDevice.setNotificationLabel(msg.arg1);
                    mRemoteDevice.setAbsVolNotificationRequested(true);
                    int percentageVol = getVolumePercentage();
                    if (DBG) {
                        Log.d(TAG, " Sending Interim Response = " + percentageVol + " label "
                                + msg.arg1);
                    }
                    AvrcpControllerService.sendRegisterAbsVolRspNative(
                            mRemoteDevice.getBluetoothAddress(), NOTIFICATION_RSP_TYPE_INTERIM,
                            percentageVol, mRemoteDevice.getNotificationLabel());
                    }
                    return true;

                case MESSAGE_PROCESS_VOLUME_CHANGED_NOTIFICATION: {
                    if (mVolumeChangedNotificationsToIgnore > 0) {
                        mVolumeChangedNotificationsToIgnore--;
                        if (mVolumeChangedNotificationsToIgnore == 0) {
                            removeMessages(MESSAGE_INTERNAL_ABS_VOL_TIMEOUT);
                        }
                    } else {
                        if (mRemoteDevice.getAbsVolNotificationRequested()) {
                            int percentageVol = getVolumePercentage();
                            Log.d(TAG, " percentageVol = " + percentageVol);
                            if (percentageVol != mPreviousPercentageVol) {
                                if (DBG) {
                                    Log.d(TAG, " Sending Changed Response = " + percentageVol +
                                          " label: " + msg.arg1 + " mPreviousPercentageVol: " +
                                          mPreviousPercentageVol);
                                }
                                AvrcpControllerService.sendRegisterAbsVolRspNative(
                                        mRemoteDevice.getBluetoothAddress(),
                                        NOTIFICATION_RSP_TYPE_CHANGED, percentageVol,
                                        mRemoteDevice.getNotificationLabel());
                                mPreviousPercentageVol = percentageVol;
                                mRemoteDevice.setAbsVolNotificationRequested(false);
                            }
                        }
                    }
                    }
                    return true;

                case MESSAGE_INTERNAL_ABS_VOL_TIMEOUT:
                    // Volume changed notifications should come back promptly from the
                    // AudioManager, if for some reason some notifications were squashed don't
                    // prevent future notifications.
                    if (DBG) Log.d(TAG, "Timed out on volume changed notification");
                    mVolumeChangedNotificationsToIgnore = 0;
                    return true;

                default:
                    return super.processMessage(msg);
            }

        }

        private void playItem(BrowseTree.BrowseNode node) {
            if (node == null) {
                Log.w(TAG, "Invalid item to play");
            } else {
                mService.playItemNative(
                        mDeviceAddress, node.getScope(),
                        node.getBluetoothID(), 0);
            }
        }

        private synchronized void passThru(int cmd) {
            logD("msgPassThru " + cmd);
            // Some keys should be held until the next event.
            if (mCurrentlyHeldKey != 0) {
                mService.sendPassThroughCommandNative(
                        mDeviceAddress, mCurrentlyHeldKey,
                        AvrcpControllerService.KEY_STATE_RELEASED);

                if (mCurrentlyHeldKey == cmd) {
                    // Return to prevent starting FF/FR operation again
                    mCurrentlyHeldKey = 0;
                    return;
                } else {
                    // FF/FR is in progress and other operation is desired
                    // so after stopping FF/FR, not returning so that command
                    // can be sent for the desired operation.
                    mCurrentlyHeldKey = 0;
                }
            }

            // Send the pass through.
            mService.sendPassThroughCommandNative(mDeviceAddress, cmd,
                    AvrcpControllerService.KEY_STATE_PRESSED);

            if (isHoldableKey(cmd)) {
                // Release cmd next time a command is sent.
                mCurrentlyHeldKey = cmd;
            } else {
                mService.sendPassThroughCommandNative(mDeviceAddress,
                        cmd, AvrcpControllerService.KEY_STATE_RELEASED);
            }
        }

        private synchronized void msgDeviceUpdated(BluetoothDevice device) {
            if (device != null && device.equals(mA2dpDevice)) {
                return;
            }
            Log.d(TAG, "msgDeviceUpdated. Previous: " + mA2dpDevice + " New: " + device);
            // We are connected to a new device via A2DP now.
            mA2dpDevice = device;
            Log.w(TAG, "mA2dpDevice: " + mA2dpDevice +
                                      " mBrowsingConnected: " + mBrowsingConnected);
            if (mBrowsingConnected) {
                //To do
                BluetoothMediaBrowserService.notifyChanged(mService.sBrowseTree.mRootNode);
                //BluetoothMediaBrowserService.notifyChanged(BrowseTree.ROOT);
            }

            int Playstate = mAddressedPlayer.getPlayStatus();
            MediaMetadata mediaMetadata = mAddressedPlayer.getCurrentTrack();
            Log.d(TAG, "Media metadata " + mediaMetadata + " playback state " + Playstate);
            mAddressedPlayer.setPlayStatus(Playstate);
            mAddressedPlayer.updateCurrentTrack(mediaMetadata);
        }

        private boolean isHoldableKey(int cmd) {
            return (cmd == AvrcpControllerService.PASS_THRU_CMD_ID_REWIND)
                    || (cmd == AvrcpControllerService.PASS_THRU_CMD_ID_FF);
        }

        private void processAvailablePlayerChanged() {
            logD("processAvailablePlayerChanged");
            mBrowseTree.mRootNode.setCached(false);
            mBrowseTree.mRootNode.setExpectedChildren(255);
            BluetoothMediaBrowserService.notifyChanged(mBrowseTree.mRootNode);
        }
    }

    // Handle the get folder listing action
    // a) Fetch the listing of folders
    // b) Once completed return the object listing
    class GetFolderList extends State {
        private static final String STATE_TAG = "Avrcp.GetFolderList";

        boolean mAbort;
        BrowseTree.BrowseNode mBrowseNode;
        BrowseTree.BrowseNode mNextStep;

        @Override
        public void enter() {
            logD(STATE_TAG + " Entering GetFolderList");
            // Setup the timeouts.
            sendMessageDelayed(MESSAGE_INTERNAL_CMD_TIMEOUT, CMD_TIMEOUT_MILLIS);
            super.enter();
            mAbort = false;
            Message msg = getCurrentMessage();
            if (msg.what == MESSAGE_GET_FOLDER_ITEMS) {
                {
                    logD(STATE_TAG + " new Get Request");
                    mBrowseNode = (BrowseTree.BrowseNode) msg.obj;
                }
            }

            if (mBrowseNode == null) {
                transitionTo(mConnected);
            } else {
                navigateToFolderOrRetrieve(mBrowseNode);
            }
        }

        @Override
        public boolean processMessage(Message msg) {
            logD(STATE_TAG + " processMessage " + msg.what);
            switch (msg.what) {
                case MESSAGE_PROCESS_GET_FOLDER_ITEMS:
                    ArrayList<MediaItem> folderList = (ArrayList<MediaItem>) msg.obj;
                    int endIndicator = mBrowseNode.getExpectedChildren() - 1;
                    logD("GetFolderItems: End " + endIndicator
                            + " received " + folderList.size());

                    // Always update the node so that the user does not wait forever
                    // for the list to populate.
                    mBrowseNode.addChildren(folderList);
                    notifyChanged(mBrowseNode);

                    if (mBrowseNode.getChildrenCount() >= endIndicator || folderList.size() == 0
                            || mAbort) {
                        // If we have fetched all the elements or if the remotes sends us 0 elements
                        // (which can lead us into a loop since mCurrInd does not proceed) we simply
                        // abort.
                        mBrowseNode.setCached(true);
                        transitionTo(mConnected);
                    } else {
                        // Fetch the next set of items.
                        fetchContents(mBrowseNode);
                        // Reset the timeout message since we are doing a new fetch now.
                        removeMessages(MESSAGE_INTERNAL_CMD_TIMEOUT);
                        sendMessageDelayed(MESSAGE_INTERNAL_CMD_TIMEOUT, CMD_TIMEOUT_MILLIS);
                    }
                    break;
                case MESSAGE_PROCESS_SET_BROWSED_PLAYER:
                    mBrowseTree.setCurrentBrowsedPlayer(mNextStep.getID(), msg.arg1, msg.arg2);
                    removeMessages(MESSAGE_INTERNAL_CMD_TIMEOUT);
                    sendMessageDelayed(MESSAGE_INTERNAL_CMD_TIMEOUT, CMD_TIMEOUT_MILLIS);
                    navigateToFolderOrRetrieve(mBrowseNode);
                    break;

                case MESSAGE_PROCESS_FOLDER_PATH:
                    mBrowseTree.setCurrentBrowsedFolder(mNextStep.getID());
                    mBrowseTree.getCurrentBrowsedFolder().setExpectedChildren(msg.arg1);

                    if (mAbort) {
                        transitionTo(mConnected);
                    } else {
                        removeMessages(MESSAGE_INTERNAL_CMD_TIMEOUT);
                        sendMessageDelayed(MESSAGE_INTERNAL_CMD_TIMEOUT, CMD_TIMEOUT_MILLIS);
                        navigateToFolderOrRetrieve(mBrowseNode);
                    }
                    break;

                case MESSAGE_PROCESS_GET_PLAYER_ITEMS:
                    BrowseTree.BrowseNode rootNode = mBrowseTree.mRootNode;
                    if (!rootNode.isCached()) {
                        List<AvrcpPlayer> playerList = (List<AvrcpPlayer>) msg.obj;
                        mAvailablePlayerList.clear();
                        for (AvrcpPlayer player : playerList) {
                            mAvailablePlayerList.put(player.getId(), player);
                        }
                        rootNode.addChildren(playerList);
                        mBrowseTree.setCurrentBrowsedFolder(BrowseTree.ROOT);
                        rootNode.setExpectedChildren(playerList.size());
                        rootNode.setCached(true);
                        notifyChanged(rootNode);
                    }
                    transitionTo(mConnected);
                    break;

                case MESSAGE_INTERNAL_CMD_TIMEOUT:
                    // We have timed out to execute the request, we should simply send
                    // whatever listing we have gotten until now.
                    Log.w(TAG, "TIMEOUT");
                    transitionTo(mConnected);
                    break;

                case MESSAGE_PROCESS_GET_FOLDER_ITEMS_OUT_OF_RANGE:
                    // If we have gotten an error for OUT OF RANGE we have
                    // already sent all the items to the client hence simply
                    // transition to Connected state here.
                    mBrowseNode.setCached(true);
                    transitionTo(mConnected);
                    break;

                case MESSAGE_GET_FOLDER_ITEMS:
                    if (!mBrowseNode.equals(msg.obj)) {
                        if (shouldAbort(mBrowseNode.getScope(),
                                 ((BrowseTree.BrowseNode) msg.obj).getScope())) {
                            mAbort = true;
                        }
                        deferMessage(msg);
                        logD("GetFolderItems: Go Get Another Directory");
                    } else {
                        logD("GetFolderItems: Get The Same Directory, ignore");
                    }
                    break;

                case CONNECT:
                case DISCONNECT:
                case MSG_AVRCP_PASSTHRU:
                case MESSAGE_PROCESS_SET_ABS_VOL_CMD:
                case MESSAGE_PROCESS_REGISTER_ABS_VOL_NOTIFICATION:
                case MESSAGE_PROCESS_TRACK_CHANGED:
                case MESSAGE_PROCESS_PLAY_POS_CHANGED:
                case MESSAGE_PROCESS_PLAY_STATUS_CHANGED:
                case MESSAGE_PROCESS_VOLUME_CHANGED_NOTIFICATION:
                case MESSAGE_PLAY_ITEM:
                case MESSAGE_PROCESS_ADDRESSED_PLAYER_CHANGED:
                    // All of these messages should be handled by parent state immediately.
                    return false;

                default:
                    logD(STATE_TAG + " deferring message " + msg.what
                                + " to connected!");
                    deferMessage(msg);
            }
            return true;
        }

        /**
         * shouldAbort calculates the cases where fetching the current directory is no longer
         * necessary.
         *
         * @return true:  a new folder in the same scope
         *                a new player while fetching contents of a folder
         *         false: other cases, specifically Now Playing while fetching a folder
         */
        private boolean shouldAbort(int currentScope, int fetchScope) {
            if ((currentScope == fetchScope)
                    || (currentScope == AvrcpControllerService.BROWSE_SCOPE_VFS
                    && fetchScope == AvrcpControllerService.BROWSE_SCOPE_PLAYER_LIST)) {
                return true;
            }
            return false;
        }

        private void fetchContents(BrowseTree.BrowseNode target) {
            int start = target.getChildrenCount();
            int end = Math.min(target.getExpectedChildren(), target.getChildrenCount()
                    + ITEM_PAGE_SIZE) - 1;
            switch (target.getScope()) {
                case AvrcpControllerService.BROWSE_SCOPE_PLAYER_LIST:
                    AvrcpControllerService.getPlayerListNative(mDeviceAddress,
                            start, end);
                    break;
                case AvrcpControllerService.BROWSE_SCOPE_NOW_PLAYING:
                    AvrcpControllerService.getNowPlayingListNative(
                            mDeviceAddress, start, end);
                    break;
                case AvrcpControllerService.BROWSE_SCOPE_VFS:
                    AvrcpControllerService.getFolderListNative(mDeviceAddress,
                            start, end);
                    break;
                default:
                    Log.e(TAG, STATE_TAG + " Scope " + target.getScope()
                            + " cannot be handled here.");
            }
        }

        /* One of several things can happen when trying to get a folder list
         *
         *
         * 0: The folder handle is no longer valid
         * 1: The folder contents can be retrieved directly (NowPlaying, Root, Current)
         * 2: The folder is a browsable player
         * 3: The folder is a non browsable player
         * 4: The folder is not a child of the current folder
         * 5: The folder is a child of the current folder
         *
         */
        private void navigateToFolderOrRetrieve(BrowseTree.BrowseNode target) {
            mNextStep = mBrowseTree.getNextStepToFolder(target);
            logD("NAVIGATING From "
                    + mBrowseTree.getCurrentBrowsedFolder().toString());
            logD("NAVIGATING Toward " + target.toString());
            if (mNextStep == null) {
                return;
            } else if (target.equals(mBrowseTree.mNowPlayingNode)
                    || target.equals(mBrowseTree.mRootNode)
                    || mNextStep.equals(mBrowseTree.getCurrentBrowsedFolder())) {
                fetchContents(mNextStep);
            } else if (mNextStep.isPlayer()) {
                logD("NAVIGATING Player " + mNextStep.toString());
                if (mNextStep.isBrowsable()) {
                    AvrcpControllerService.setBrowsedPlayerNative(
                            mDeviceAddress, (int) mNextStep.getBluetoothID());
                } else {
                    logD("Player doesn't support browsing");
                    mNextStep.setCached(true);
                    transitionTo(mConnected);
                }
            } else if (mNextStep.equals(mBrowseTree.mNavigateUpNode)) {
                logD("NAVIGATING UP " + mNextStep.toString());
                mNextStep = mBrowseTree.getCurrentBrowsedFolder().getParent();
                mBrowseTree.getCurrentBrowsedFolder().setCached(false);

                AvrcpControllerService.changeFolderPathNative(
                        mDeviceAddress,
                        AvrcpControllerService.FOLDER_NAVIGATION_DIRECTION_UP,
                        0);

            } else {
                logD("NAVIGATING DOWN " + mNextStep.toString());
                AvrcpControllerService.changeFolderPathNative(
                        mDeviceAddress,
                        AvrcpControllerService.FOLDER_NAVIGATION_DIRECTION_DOWN,
                        mNextStep.getBluetoothID());
            }
        }

        @Override
        public void exit() {
            removeMessages(MESSAGE_INTERNAL_CMD_TIMEOUT);
            mBrowseNode = null;
            super.exit();
        }
    }

    protected class Disconnecting extends State {
        @Override
        public void enter() {
            onBrowsingDisconnected();
            broadcastConnectionStateChanged(BluetoothProfile.STATE_DISCONNECTING);
            transitionTo(mDisconnected);
        }
    }

    private final BroadcastReceiver mBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            Log.d(TAG, "onReceive(): action: " + action);
            if (action.equals(AudioManager.VOLUME_CHANGED_ACTION)) {
                int streamType = intent.getIntExtra(AudioManager.EXTRA_VOLUME_STREAM_TYPE, -1);
                if (streamType == AudioManager.STREAM_MUSIC) {
                    sendMessage(MESSAGE_PROCESS_VOLUME_CHANGED_NOTIFICATION);
                }
            }
        }
    };

    void registerReceiver(BluetoothDevice device) {
        if (DBG) {
            Log.d(TAG, " Register receiver for device: " + device);
        }
        IntentFilter filter = new IntentFilter(AudioManager.VOLUME_CHANGED_ACTION);
        mService.registerReceiver(mBroadcastReceiver, filter);
    }

    void doQuit() {
        Log.d(TAG, "doQuit()");
        try {
            mService.unregisterReceiver(mBroadcastReceiver);
        } catch (IllegalArgumentException expected) {
            // If the receiver was never registered unregister will throw an
            // IllegalArgumentException.
        }
        synchronized(AvrcpControllerStateMachine.this) {
            smActive = false;
        }
        // we should disacrd, all currently queuedup messages.
        quitNow();
    }

    private void setAbsVolume(int absVol, int label) {
        int maxVolume = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        int currIndex = mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC);

        /* If SetAbsVolume Control Cmd is received from non-Streaming device then the
         * requested volume level would be accepted at SINK device, and current Abs vol level
         * at DUT (sink: rendering device) will be sent in changed response. */
        Log.d(TAG, "Streaming device: " + A2dpSinkService.getCurrentStreamingDevice()
                + " Device: " + mDevice + " absVol: " + absVol + " label: " + label);
        if (!mDevice.equals(A2dpSinkService.getCurrentStreamingDevice())) {
            Log.w(TAG, "Volume change request came from non-streaming device," +
                    "respond with accepted absVol: " + absVol + "at Sink");
            AvrcpControllerService.sendAbsVolRspNative(mRemoteDevice.getBluetoothAddress(), absVol,
                label);
            int percentageVol = getVolumePercentage();
            AvrcpControllerService.sendRegisterAbsVolRspNative(mRemoteDevice.getBluetoothAddress(),
                NOTIFICATION_RSP_TYPE_CHANGED, percentageVol, mRemoteDevice.getNotificationLabel());
            mPreviousPercentageVol = percentageVol;
            return;
        }
        int newIndex = (maxVolume * absVol) / ABS_VOL_BASE;
        logD(" setAbsVolume =" + absVol + " maxVol = " + maxVolume
                + " cur = " + currIndex + " new = " + newIndex);
        /*
         * In some cases change in percentage is not sufficient enough to warrant
         * change in index values which are in range of 0-15. For such cases
         * no action is required
         */
        if (newIndex != currIndex) {
            mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, newIndex,
                    AudioManager.FLAG_SHOW_UI);
        }
        AvrcpControllerService.sendAbsVolRspNative(mDeviceAddress, absVol, label);
    }

    private int getVolumePercentage() {
        int maxVolume = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        int currIndex = mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
        int percentageVol = ((currIndex * ABS_VOL_BASE) / maxVolume);
        logD("maxVolume: " + maxVolume + " currIndex: " + currIndex +
                                         " percentageVol: " + percentageVol);
        return percentageVol;
    }

    MediaSession.Callback mSessionCallbacks = new MediaSession.Callback() {
        @Override
        public void onPlay() {
            logD("onPlay");
            onPrepare();
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_PLAY);
        }

        @Override
        public void onPause() {
            logD("onPause");
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_PAUSE);
        }

        @Override
        public void onSkipToNext() {
            logD("onSkipToNext");
            onPrepare();
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_FORWARD);
        }

        @Override
        public void onSkipToPrevious() {
            logD("onSkipToPrevious");
            onPrepare();
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_BACKWARD);
        }

        @Override
        public void onSkipToQueueItem(long id) {
            logD("onSkipToQueueItem" + id);
            onPrepare();
            BrowseTree.BrowseNode node = mBrowseTree.getTrackFromNowPlayingList((int) id);
            if (node != null) {
                sendMessage(MESSAGE_PLAY_ITEM, node);
            }
        }

        @Override
        public void onStop() {
            logD("onStop");
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_STOP);
        }

        @Override
        public void onPrepare() {
            logD("onPrepare");
            A2dpSinkService a2dpSinkService = A2dpSinkService.getA2dpSinkService();
            if (a2dpSinkService != null) {
                a2dpSinkService.requestAudioFocus(mDevice, true);
            }
        }

        @Override
        public void onRewind() {
            logD("onRewind");
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_REWIND);
        }

        @Override
        public void onFastForward() {
            logD("onFastForward");
            sendMessage(MSG_AVRCP_PASSTHRU, AvrcpControllerService.PASS_THRU_CMD_ID_FF);
        }

        @Override
        public void onPlayFromMediaId(String mediaId, Bundle extras) {
            logD("onPlayFromMediaId");
            // Play the item if possible.
            onPrepare();
            BrowseTree.BrowseNode node = mBrowseTree.findBrowseNodeByID(mediaId);
            sendMessage(MESSAGE_PLAY_ITEM, node);
        }
    };

    protected void broadcastConnectionStateChanged(int currentState) {
        if (mMostRecentState == currentState) {
            return;
        }
        if (currentState == BluetoothProfile.STATE_CONNECTED) {
            MetricsLogger.logProfileConnectionEvent(
                    BluetoothMetricsProto.ProfileId.AVRCP_CONTROLLER);
        }
        logD("Connection state " + mDevice + ": " + mMostRecentState + "->" + currentState);
        Intent intent = new Intent(BluetoothAvrcpController.ACTION_CONNECTION_STATE_CHANGED);
        intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, mMostRecentState);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, currentState);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, mDevice);
        intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT);
        mMostRecentState = currentState;
        mService.sendBroadcast(intent, BLUETOOTH_CONNECT, Utils.getTempAllowlistBroadcastOptions());
    }

    private void broadcastPlayBackStateChanged(PlaybackState state) {
        Intent intent = new Intent(AvrcpControllerService.ACTION_TRACK_EVENT);
        intent.putExtra(AvrcpControllerService.EXTRA_PLAYBACK, state);
        if (DBG) {
            Log.d(TAG, " broadcastPlayBackStateChanged = " + state.toString());
        }
        mService.sendBroadcast(intent, BLUETOOTH_CONNECT);
    }


    private boolean shouldRequestFocus() {
        return mService.getResources()
                .getBoolean(R.bool.a2dp_sink_automatically_request_audio_focus);
    }
}
