/*
 * Copyright 2022 The Android Open Source Project
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

package com.android.bluetooth.lebroadcast;

import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothLeBroadcastMetadata;
import android.bluetooth.BluetoothLeBroadcastReceiveState;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.IBluetoothLeBroadcastAssistant;
import android.bluetooth.IBluetoothLeBroadcastAssistantCallback;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Intent;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.util.Log;

import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Broacast Assistant Scan Service
 */
public class BassClientService extends ProfileService {
    private static final boolean BASS_DBG = true;
    private static final String TAG = BassClientService.class.getSimpleName();
    private static final int INVALID_SOURCE_ID = -1;
    private static BassClientService sService;

    private BluetoothAdapter mBluetoothAdapter = null;
    private AdapterService mAdapterService;
    private HandlerThread mCallbackHandlerThread;
    private Callbacks mCallbacks;

    public Callbacks getCallbacks() {
        return mCallbacks;
    }

    @Override
    protected IProfileServiceBinder initBinder() {
        return new BluetoothLeBroadcastAssistantBinder(this);
    }

    @Override
    protected void create() {
        Log.i(TAG, "create()");
    }

    @Override
    protected boolean start() {
        if (BASS_DBG) {
            Log.d(TAG, "start()");
        }
        if (sService != null) {
            throw new IllegalStateException("start() called twice");
        }
        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService(),
                "AdapterService cannot be null when BassClientService starts");
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        mCallbackHandlerThread = new HandlerThread(TAG);
        mCallbackHandlerThread.start();
        mCallbacks = new Callbacks(mCallbackHandlerThread.getLooper());
        setBassClientService(this);
        return true;
    }

    @Override
    protected boolean stop() {
        if (BASS_DBG) {
            Log.d(TAG, "stop()");
        }
        if (mCallbackHandlerThread != null) {
            mCallbackHandlerThread.quitSafely();
            mCallbackHandlerThread = null;
        }
        setBassClientService(null);
        return true;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.d(TAG, "Need to unregister app");
        return super.onUnbind(intent);
    }

    private static synchronized void setBassClientService(BassClientService instance) {
        if (BASS_DBG) {
            Log.d(TAG, "setBassClientService(): set to: " + instance);
        }
        sService = instance;
    }

    /**
     * Get the BassClientService instance
     *
     * @return BassClientService instance
     */
    public static synchronized BassClientService getBassClientService() {
        if (sService == null) {
            Log.w(TAG, "getBassClientService(): service is NULL");
            return null;
        }
        if (!sService.isAvailable()) {
            Log.w(TAG, "getBassClientService(): service is not available");
            return null;
        }
        return sService;
    }

    /**
     * Connects the bass profile to the passed in device
     *
     * @param device is the device with which we will connect the Bass profile
     * @return true if BAss profile successfully connected, false otherwise
     */
    public boolean connect(BluetoothDevice device) {
        if (BASS_DBG) {
            Log.d(TAG, "connect(): " + device);
        }
        if (device == null) {
            Log.e(TAG, "connect: device is null");
            return false;
        }
        if (getConnectionPolicy(device) == BluetoothProfile.CONNECTION_POLICY_UNKNOWN) {
            Log.e(TAG, "connect: unknown connection policy");
            return false;
        }
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        return mBCService.connect(device);
    }

    /**
     * Disconnects Bassclient profile for the passed in device
     *
     * @param device is the device with which we want to disconnected the BAss client profile
     * @return true if Bass client profile successfully disconnected, false otherwise
     */
    public boolean disconnect(BluetoothDevice device) {
        if (BASS_DBG) {
            Log.d(TAG, "disconnect(): " + device);
        }
        if (device == null) {
            Log.e(TAG, "disconnect: device is null");
            return false;
        }
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        return mBCService.disconnect(device);
    }

    /**
     * Check whether can connect to a peer device. The check considers a number of factors during
     * the evaluation.
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

    /**
     * Get connection state of remote device
     *
     * @param sink the remote device
     * @return connection state
     */
    public int getConnectionState(BluetoothDevice sink) {
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        return mBCService.getConnectionState(sink);
    }

    /**
     * Get a list of all LE Audio Broadcast Sinks with the specified connection states.
     * @param states states array representing the connection states
     * @return a list of devices that match the provided connection states
     */
    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        if (states == null) {
            return devices;
        }
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        return mBCService.getDevicesMatchingConnectionStates(states);
    }

    /**
     * Get a list of all LE Audio Broadcast Sinks connected with the LE Audio Broadcast Assistant.
     * @return list of connected devices
     */
    List<BluetoothDevice> getConnectedDevices() {
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        return mBCService.getConnectedDevices();
    }

    /**
     * Set the connectionPolicy of the Broadcast Audio Scan Service profile.
     *
     * <p>The connection policy can be one of:
     * {@link BluetoothProfile#CONNECTION_POLICY_ALLOWED},
     * {@link BluetoothProfile#CONNECTION_POLICY_FORBIDDEN},
     * {@link BluetoothProfile#CONNECTION_POLICY_UNKNOWN}
     *
     * @param device paired bluetooth device
     * @param connectionPolicy is the connection policy to set to for this profile
     * @return true if connectionPolicy is set, false on error
     */
    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        if (BASS_DBG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }
        boolean setSuccessfully =
                mAdapterService.getDatabase().setProfileConnectionPolicy(device,
                        BluetoothProfile.BC_PROFILE, connectionPolicy);
        if (setSuccessfully && connectionPolicy == BluetoothProfile.CONNECTION_POLICY_ALLOWED) {
            connect(device);
        } else if (setSuccessfully
                && connectionPolicy == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
            disconnect(device);
        }
        return setSuccessfully;
    }

    /**
     * Get the connection policy of the profile.
     *
     * <p>The connection policy can be any of:
     * {@link BluetoothProfile#CONNECTION_POLICY_ALLOWED},
     * {@link BluetoothProfile#CONNECTION_POLICY_FORBIDDEN},
     * {@link BluetoothProfile#CONNECTION_POLICY_UNKNOWN}
     *
     * @param device paired bluetooth device
     * @return connection policy of the device
     */
    public int getConnectionPolicy(BluetoothDevice device) {
        return mAdapterService
                .getDatabase()
                .getProfileConnectionPolicy(device, BluetoothProfile.BC_PROFILE);
    }

    /**
     * Register callbacks that will be invoked during scan offloading.
     *
     * @param cb callbacks to be invoked
     */
    public void registerCallback(IBluetoothLeBroadcastAssistantCallback cb) {
        Log.i(TAG, "registerCallback");
        mCallbacks.register(cb);
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        mBCService.setBassClientSevice(this);
        return;
    }

    /**
     * Unregister callbacks that are invoked during scan offloading.
     *
     * @param cb callbacks to be unregistered
     */
    public void unregisterCallback(IBluetoothLeBroadcastAssistantCallback cb) {
        Log.i(TAG, "unregisterCallback");
        mCallbacks.unregister(cb);
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        mBCService.setBassClientSevice(null);
        return;
    }

    /**
     * Search for LE Audio Broadcast Sources on behalf of all devices connected via Broadcast Audio
     * Scan Service, filtered by filters
     *
     * @param filters ScanFilters for finding exact Broadcast Source
     */
    public void startSearchingForSources(List<ScanFilter> filters) {
        log("startSearchingForSources");
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        mBCService.startSearchingForSources(filters);
    }

    /**
     * Stops an ongoing search for nearby Broadcast Sources
     */
    public void stopSearchingForSources() {
        log("stopSearchingForSources");
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        mBCService.stopSearchingForSources();
    }

    /**
     * Return true if a search has been started by this application
     * @return true if a search has been started by this application
     */
    public boolean isSearchInProgress() {
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        return mBCService.isSearchInProgress();
    }

    void selectSource(BluetoothDevice sink, ScanResult result, boolean autoTrigger) {
    }

    /**
     * Add a Broadcast Source to the Broadcast Sink
     *
     * @param sink Broadcast Sink to which the Broadcast Source should be added
     * @param sourceMetadata Broadcast Source metadata to be added to the Broadcast Sink
     * @param isGroupOp set to true If Application wants to perform this operation for all
     *                  coordinated set members, False otherwise
     */
    public void addSource(BluetoothDevice sink, BluetoothLeBroadcastMetadata sourceMetadata,
            boolean isGroupOp) {
        log("addSource: device: " + sink + " sourceMetadata" + sourceMetadata
                + " isGroupOp " + isGroupOp);
        if (sourceMetadata == null) {
            log("Error bad parameters: sourceMetadata = " + sourceMetadata);
            mCallbacks.notifySourceAddFailed(sink, sourceMetadata,
                    BluetoothStatusCodes.ERROR_BAD_PARAMETERS);
            return;
        }
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        mBCService.addSource(sink, sourceMetadata, isGroupOp);
    }

    /**
     * Modify the Broadcast Source information on a Broadcast Sink
     *
     * @param sink representing the Broadcast Sink to which the Broadcast
     *               Source should be updated
     * @param sourceId source ID as delivered in onSourceAdded
     * @param updatedMetadata updated Broadcast Source metadata to be updated on the Broadcast Sink
     */
    public void modifySource(BluetoothDevice sink, int sourceId,
            BluetoothLeBroadcastMetadata updatedMetadata) {
        log("modifySource: device: " + sink + " sourceId " + sourceId);
        if (sourceId == INVALID_SOURCE_ID
                    || updatedMetadata == null) {
            log("Error bad parameters: sourceId = " + sourceId
                    + " updatedMetadata = " + updatedMetadata);
            mCallbacks.notifySourceModifyFailed(sink, sourceId,
                    BluetoothStatusCodes.ERROR_BAD_PARAMETERS);
            return;
        }
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        mBCService.modifySource(sink, sourceId, updatedMetadata);
    }

    /**
     * Removes the Broadcast Source from a Broadcast Sink
     *
     * @param sink representing the Broadcast Sink from which a Broadcast
     *               Source should be removed
     * @param sourceId source ID as delivered in onSourceAdded
     */
    public void removeSource(BluetoothDevice sink, int sourceId) {
        log("removeSource: device = " + sink
                + "sourceId " + sourceId);
        if (sourceId == INVALID_SOURCE_ID) {
            log("Error bad parameters: sourceId = " + sourceId);
            mCallbacks.notifySourceRemoveFailed(sink, sourceId,
                    BluetoothStatusCodes.ERROR_BAD_PARAMETERS);
            return;
        }
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        mBCService.removeSource(sink, sourceId);
    }

    /**
     * Get information about all Broadcast Sources
     *
     * @param sink Broadcast Sink from which to get all Broadcast Sources
     * @return the list of Broadcast Receive State {@link BluetoothLeBroadcastReceiveState}
     */
    public List<BluetoothLeBroadcastReceiveState> getAllSources(BluetoothDevice sink) {
        log("getAllSources for " + sink);
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        return mBCService.getAllSources(sink);
    }

    /**
     * Get maximum number of sources that can be added to this Broadcast Sink
     *
     * @param sink Broadcast Sink device
     * @return maximum number of sources that can be added to this Broadcast Sink
     */
    int getMaximumSourceCapacity(BluetoothDevice sink) {
        log("getMaximumSourceCapacity: device = " + sink);
        LeBroadcastAssistantServIntf mBCService = LeBroadcastAssistantServIntf.get();
        return mBCService.getMaximumSourceCapacity(sink);
    }

    static void log(String msg) {
        if (BASS_DBG) {
            Log.d(TAG, msg);
        }
    }

    /**
     * Callback handler
     */
    public static class Callbacks extends Handler {
        private static final int MSG_SEARCH_STARTED = 1;
        private static final int MSG_SEARCH_STARTED_FAILED = 2;
        private static final int MSG_SEARCH_STOPPED = 3;
        private static final int MSG_SEARCH_STOPPED_FAILED = 4;
        private static final int MSG_SOURCE_FOUND = 5;
        private static final int MSG_SOURCE_ADDED = 6;
        private static final int MSG_SOURCE_ADDED_FAILED = 7;
        private static final int MSG_SOURCE_MODIFIED = 8;
        private static final int MSG_SOURCE_MODIFIED_FAILED = 9;
        private static final int MSG_SOURCE_REMOVED = 10;
        private static final int MSG_SOURCE_REMOVED_FAILED = 11;
        private static final int MSG_RECEIVESTATE_CHANGED = 12;

        private final RemoteCallbackList<IBluetoothLeBroadcastAssistantCallback>
                mCallbacks = new RemoteCallbackList<>();

        Callbacks(Looper looper) {
            super(looper);
        }

        public void register(IBluetoothLeBroadcastAssistantCallback callback) {
            mCallbacks.register(callback);
        }

        public void unregister(IBluetoothLeBroadcastAssistantCallback callback) {
            mCallbacks.unregister(callback);
        }

        @Override
        public void handleMessage(Message msg) {
            final int n = mCallbacks.beginBroadcast();
            for (int i = 0; i < n; i++) {
                final IBluetoothLeBroadcastAssistantCallback callback =
                        mCallbacks.getBroadcastItem(i);
                try {
                    invokeCallback(callback, msg);
                } catch (RemoteException e) {
                    Log.e(TAG, "Stack:" + Log.getStackTraceString(e));
                }
            }
            mCallbacks.finishBroadcast();
        }

        private class ObjParams {
            Object mObj1;
            Object mObj2;
            ObjParams(Object o1, Object o2) {
                mObj1 = o1;
                mObj2 = o2;
            }
        }

        private void invokeCallback(IBluetoothLeBroadcastAssistantCallback callback,
                Message msg) throws RemoteException {
            final int reason = msg.arg1;
            final int sourceId = msg.arg2;
            ObjParams param;
            BluetoothDevice sink;

            switch (msg.what) {
                case MSG_SEARCH_STARTED:
                    callback.onSearchStarted(reason);
                    break;
                case MSG_SEARCH_STARTED_FAILED:
                    callback.onSearchStartFailed(reason);
                    break;
                case MSG_SEARCH_STOPPED:
                    callback.onSearchStopped(reason);
                    break;
                case MSG_SEARCH_STOPPED_FAILED:
                    callback.onSearchStopFailed(reason);
                    break;
                case MSG_SOURCE_FOUND:
                    callback.onSourceFound((BluetoothLeBroadcastMetadata) msg.obj);
                    break;
                case MSG_SOURCE_ADDED:
                    callback.onSourceAdded((BluetoothDevice) msg.obj, sourceId, reason);
                    break;
                case MSG_SOURCE_ADDED_FAILED:
                    param = (ObjParams) msg.obj;
                    sink = (BluetoothDevice) param.mObj1;
                    BluetoothLeBroadcastMetadata metadata =
                            (BluetoothLeBroadcastMetadata) param.mObj2;
                    callback.onSourceAddFailed(sink, metadata, reason);
                    break;
                case MSG_SOURCE_MODIFIED:
                    callback.onSourceModified((BluetoothDevice) msg.obj, sourceId, reason);
                    break;
                case MSG_SOURCE_MODIFIED_FAILED:
                    callback.onSourceModifyFailed((BluetoothDevice) msg.obj, sourceId, reason);
                    break;
                case MSG_SOURCE_REMOVED:
                    callback.onSourceRemoved((BluetoothDevice) msg.obj, sourceId, reason);
                    break;
                case MSG_SOURCE_REMOVED_FAILED:
                    callback.onSourceRemoveFailed((BluetoothDevice) msg.obj, sourceId, reason);
                    break;
                case MSG_RECEIVESTATE_CHANGED:
                    param = (ObjParams) msg.obj;
                    sink = (BluetoothDevice) param.mObj1;
                    BluetoothLeBroadcastReceiveState state =
                            (BluetoothLeBroadcastReceiveState) param.mObj2;
                    callback.onReceiveStateChanged(sink, sourceId, state);
                    break;
                default:
                    Log.e(TAG, "Invalid msg: " + msg.what);
                    break;
            }
        }

        public void notifySearchStarted(int reason) {
            obtainMessage(MSG_SEARCH_STARTED, reason, 0).sendToTarget();
        }

        public void notifySearchStartFailed(int reason) {
            obtainMessage(MSG_SEARCH_STARTED_FAILED, reason, 0).sendToTarget();
        }

        public void notifySearchStopped(int reason) {
            obtainMessage(MSG_SEARCH_STOPPED, reason, 0).sendToTarget();
        }

        public void notifySearchStopFailed(int reason) {
            obtainMessage(MSG_SEARCH_STOPPED_FAILED, reason, 0).sendToTarget();
        }

        public void notifySourceFound(BluetoothLeBroadcastMetadata source) {
            obtainMessage(MSG_SOURCE_FOUND, 0, 0, source).sendToTarget();
        }

        public void notifySourceAdded(BluetoothDevice sink, int sourceId, int reason) {
            obtainMessage(MSG_SOURCE_ADDED, reason, sourceId, sink).sendToTarget();
        }

        public void notifySourceAddFailed(BluetoothDevice sink, BluetoothLeBroadcastMetadata source,
                int reason) {
            ObjParams param = new ObjParams(sink, source);
            obtainMessage(MSG_SOURCE_ADDED_FAILED, reason, 0, param).sendToTarget();
        }

        public void notifySourceModified(BluetoothDevice sink, int sourceId, int reason) {
            obtainMessage(MSG_SOURCE_MODIFIED, reason, sourceId, sink).sendToTarget();
        }

        public void notifySourceModifyFailed(BluetoothDevice sink, int sourceId, int reason) {
            obtainMessage(MSG_SOURCE_MODIFIED_FAILED, reason, sourceId, sink).sendToTarget();
        }

        public void notifySourceRemoved(BluetoothDevice sink, int sourceId, int reason) {
            obtainMessage(MSG_SOURCE_REMOVED, reason, sourceId, sink).sendToTarget();
        }

        public void notifySourceRemoveFailed(BluetoothDevice sink, int sourceId, int reason) {
            obtainMessage(MSG_SOURCE_REMOVED_FAILED, reason, sourceId, sink).sendToTarget();
        }

        public void notifyReceiveStateChanged(BluetoothDevice sink, int sourceId,
                BluetoothLeBroadcastReceiveState state) {
            ObjParams param = new ObjParams(sink, state);
            obtainMessage(MSG_RECEIVESTATE_CHANGED, 0, sourceId, param).sendToTarget();
        }
    }

    /** Binder object: must be a static class or memory leak may occur */
    @VisibleForTesting
    static class BluetoothLeBroadcastAssistantBinder extends IBluetoothLeBroadcastAssistant.Stub
            implements IProfileServiceBinder {
        private BassClientService mService;

        private BassClientService getService() {
            if (!Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkCallerIsSystemOrActiveOrManagedUser(mService, TAG)) {
                return null;
            }
            return mService;
        }

        BluetoothLeBroadcastAssistantBinder(BassClientService svc) {
            mService = svc;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public int getConnectionState(BluetoothDevice sink) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return BluetoothProfile.STATE_DISCONNECTED;
                }
                return service.getConnectionState(sink);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return BluetoothProfile.STATE_DISCONNECTED;
            }
        }

        @Override
        public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return Collections.emptyList();
                }
                return service.getDevicesMatchingConnectionStates(states);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return Collections.emptyList();
            }
        }

        @Override
        public List<BluetoothDevice> getConnectedDevices() {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return Collections.emptyList();
                }
                return service.getConnectedDevices();
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return Collections.emptyList();
            }
        }

        @Override
        public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return false;
                }
                return service.setConnectionPolicy(device, connectionPolicy);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }

        @Override
        public int getConnectionPolicy(BluetoothDevice device) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return BluetoothProfile.CONNECTION_POLICY_FORBIDDEN;
                }
                return service.getConnectionPolicy(device);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return BluetoothProfile.CONNECTION_POLICY_FORBIDDEN;
            }
        }

        @Override
        public void registerCallback(IBluetoothLeBroadcastAssistantCallback cb) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return;
                }
                enforceBluetoothPrivilegedPermission(service);
                service.registerCallback(cb);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }

        @Override
        public void unregisterCallback(IBluetoothLeBroadcastAssistantCallback cb) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return;
                }
                enforceBluetoothPrivilegedPermission(service);
                service.unregisterCallback(cb);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }

        @Override
        public void startSearchingForSources(List<ScanFilter> filters) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return;
                }
                service.startSearchingForSources(filters);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }

        @Override
        public void stopSearchingForSources() {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return;
                }
                service.stopSearchingForSources();
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }

        @Override
        public boolean isSearchInProgress() {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return false;
                }
                return service.isSearchInProgress();
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return false;
            }
        }

        @Override
        public void addSource(
                BluetoothDevice sink, BluetoothLeBroadcastMetadata sourceMetadata,
                boolean isGroupOp) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return;
                }
                service.addSource(sink, sourceMetadata, isGroupOp);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }

        @Override
        public void modifySource(
                BluetoothDevice sink, int sourceId, BluetoothLeBroadcastMetadata updatedMetadata) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return;
                }
                service.modifySource(sink, sourceId, updatedMetadata);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }

        @Override
        public void removeSource(BluetoothDevice sink, int sourceId) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return;
                }
                service.removeSource(sink, sourceId);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
            }
        }

        @Override
        public List<BluetoothLeBroadcastReceiveState> getAllSources(BluetoothDevice sink) {
            try {
                BassClientService service = getService();
                if (sink == null) {
                    Log.e(TAG, "Service is null");
                    return Collections.emptyList();
                }
                return service.getAllSources(sink);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return Collections.emptyList();
            }
        }

        @Override
        public int getMaximumSourceCapacity(BluetoothDevice sink) {
            try {
                BassClientService service = getService();
                if (service == null) {
                    Log.e(TAG, "Service is null");
                    return 0;
                }
                return service.getMaximumSourceCapacity(sink);
            } catch (RuntimeException e) {
                Log.e(TAG, "Stack:" + Log.getStackTraceString(new Throwable()));
                return 0;
            }
        }
    }
}
