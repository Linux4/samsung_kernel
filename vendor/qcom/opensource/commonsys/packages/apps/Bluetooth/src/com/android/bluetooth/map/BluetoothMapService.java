/*
* Copyright (C) 2014 Samsung System LSI
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

package com.android.bluetooth.map;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;

import android.annotation.RequiresPermission;
import android.app.Activity;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothMap;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.IBluetoothMap;
import android.bluetooth.SdpMnsRecord;
import android.content.AttributionSource;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.IntentFilter.MalformedMimeTypeException;
import android.Manifest;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.PowerManager;
import android.os.RemoteException;
import android.os.SystemProperties;
import android.provider.Settings;
import android.sysprop.BluetoothProperties;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseArray;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.R;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.MetricsLogger;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.internal.annotations.VisibleForTesting;
import com.android.modules.utils.SynchronousResultReceiver;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;
import java.util.Set;

public class BluetoothMapService extends ProfileService {
    private static final String TAG = "BluetoothMapService";
    private static final String LOG_TAG = "BluetoothMap";
    /**
     * To enable MAP DEBUG/VERBOSE logging - run below cmd in adb shell, and
     * restart com.android.bluetooth process. only enable DEBUG log:
     * "setprop log.tag.BluetoothMapService DEBUG"; enable both VERBOSE and
     * DEBUG log: "setprop log.tag.BluetoothMapService VERBOSE"
     */

    public static final boolean DEBUG = true; //FIXME set to false;

    public static final boolean VERBOSE = Log.isLoggable(LOG_TAG, Log.VERBOSE);

    /**
     * Intent indicating timeout for user confirmation, which is sent to
     * BluetoothMapActivity
     */
    public static final String USER_CONFIRM_TIMEOUT_ACTION =
            "com.android.bluetooth.map.USER_CONFIRM_TIMEOUT";
    private static final int USER_CONFIRM_TIMEOUT_VALUE = 25000;

    // Intent indicating that the email settings activity should be opened
    static final String ACTION_SHOW_MAPS_SETTINGS =
            "android.btmap.intent.action.SHOW_MAPS_SETTINGS";

    static final int MSG_SERVERSESSION_CLOSE = 5000;
    static final int MSG_SESSION_ESTABLISHED = 5001;
    static final int MSG_SESSION_DISCONNECTED = 5002;
    static final int MSG_MAS_CONNECT = 5003; // Send at MAS connect, including the MAS_ID
    static final int MSG_MAS_CONNECT_CANCEL = 5004; // Send at auth. declined
    static final int MSG_ACQUIRE_WAKE_LOCK = 5005;
    static final int MSG_RELEASE_WAKE_LOCK = 5006;
    static final int MSG_MNS_SDP_SEARCH = 5007;
    static final int MSG_OBSERVER_REGISTRATION = 5008;

    private static final int START_LISTENER = 1;
    private static final int USER_TIMEOUT = 2;
    private static final int DISCONNECT_MAP = 3;
    private static final int SHUTDOWN = 4;
    private static final int UPDATE_MAS_INSTANCES = 6;
    private static final int CREATE_MAS_INSTANCES = 5;
    private static final int RELEASE_WAKE_LOCK_DELAY = 10000;
    private PowerManager.WakeLock mWakeLock = null;

    static final int UPDATE_MAS_INSTANCES_ACCOUNT_ADDED = 0;
    static final int UPDATE_MAS_INSTANCES_ACCOUNT_REMOVED = 1;
    static final int UPDATE_MAS_INSTANCES_ACCOUNT_RENAMED = 2;
    static final int UPDATE_MAS_INSTANCES_ACCOUNT_DISCONNECT = 3;

    private static final int MAS_ID_SMS_MMS = 0;

    private BluetoothAdapter mAdapter;
    private DatabaseManager mDatabaseManager;

    private BluetoothMnsObexClient mBluetoothMnsObexClient = null;

    // mMasInstances: A list of the active MasInstances using the MasId for the key
    private SparseArray<BluetoothMapMasInstance> mMasInstances =
            new SparseArray<BluetoothMapMasInstance>(1);
    // mMasInstanceMap: A list of the active MasInstances using the account for the key
    private HashMap<BluetoothMapAccountItem, BluetoothMapMasInstance> mMasInstanceMap =
            new HashMap<BluetoothMapAccountItem, BluetoothMapMasInstance>(1);

    // The remote connected device - protect access
    private static BluetoothDevice sRemoteDevice = null;

    ArrayList<BluetoothMapAccountItem> mEnabledAccounts = null;
    private static String sRemoteDeviceName = null;

    private int mState;
    BluetoothMapAppObserver mAppObserver = null;
    private AlarmManager mAlarmManager = null;

    private boolean mIsWaitingAuthorization = false;
    private boolean mRemoveTimeoutMsg = false;
    private boolean mRegisteredMapReceiver = false;
    private int mPermission = BluetoothDevice.ACCESS_UNKNOWN;
    private boolean mAccountChanged = false;
    private boolean mSdpSearchInitiated = false;
    private SdpMnsRecord mMnsRecord = null;
    private MapServiceMessageHandler mSessionStatusHandler;
    private boolean mServiceStarted = false;

    private static BluetoothMapService sBluetoothMapService;

    private boolean mSmsCapable = true;

    private static final ParcelUuid[] MAP_UUIDS = {
            BluetoothUuid.MAP, BluetoothUuid.MNS,
    };

    public BluetoothMapService() {
        mState = BluetoothMap.STATE_DISCONNECTED;
        BluetoothMap.invalidateBluetoothGetConnectionStateCache();
    }

    public static boolean isEnabled() {
        return BluetoothProperties.isProfileMapServerEnabled().orElse(false);
    }

    private synchronized void closeService() {
        if (DEBUG) {
            Log.d(TAG, "closeService() in");
        }
        if (mBluetoothMnsObexClient != null) {
            mBluetoothMnsObexClient.shutdown();
            mBluetoothMnsObexClient = null;
        }
        int numMasInstances = mMasInstances.size();
        for (int i = 0; i < numMasInstances; i++) {
            mMasInstances.valueAt(i).shutdown();
        }
        mMasInstances.clear();

        mIsWaitingAuthorization = false;
        mPermission = BluetoothDevice.ACCESS_UNKNOWN;
        setState(BluetoothMap.STATE_DISCONNECTED);

        if (mWakeLock != null) {
            mWakeLock.release();
            if (VERBOSE) {
                Log.v(TAG, "CloseService(): Release Wake Lock");
            }
            mWakeLock = null;
        }

        sRemoteDevice = null;
        // no need to invalidate cache here because setState did it above

        if (mSessionStatusHandler == null) {
            return;
        }

        // Perform cleanup in Handler running on worker Thread
        mSessionStatusHandler.removeCallbacksAndMessages(null);
        Looper looper = mSessionStatusHandler.getLooper();
        if (looper != null) {
            looper.quit();
            if (VERBOSE) {
                Log.i(TAG, "Quit looper");
            }
        }
        mSessionStatusHandler = null;

        if (DEBUG) {
            Log.v(TAG, "MAP Service closeService out");
        }
    }

    /**
     * Starts the Socket listener threads for each MAS
     */
    private void startSocketListeners(int masId) {
        if (masId == -1) {
            for (int i = 0, c = mMasInstances.size(); i < c; i++) {
                mMasInstances.valueAt(i).startSocketListeners();
            }
        } else {
            BluetoothMapMasInstance masInst = mMasInstances.get(masId); // returns null for -1
            if (masInst != null) {
                masInst.startSocketListeners();
            } else {
                Log.w(TAG, "startSocketListeners(): Invalid MasId: " + masId);
            }
        }
    }

    /**
     * Start a MAS instance for SMS/MMS and each e-mail account.
     */
    private void startObexServerSessions() {
        if (DEBUG) {
            Log.d(TAG, "Map Service START ObexServerSessions()");
        }

        // Acquire the wakeLock before starting Obex transaction thread
        if (mWakeLock == null) {
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            mWakeLock =
                    pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "StartingObexMapTransaction");
            mWakeLock.setReferenceCounted(false);
            mWakeLock.acquire();
            if (VERBOSE) {
                Log.v(TAG, "startObexSessions(): Acquire Wake Lock");
            }
        }

        if (mBluetoothMnsObexClient == null) {
            mBluetoothMnsObexClient =
                    new BluetoothMnsObexClient(sRemoteDevice, mMnsRecord, mSessionStatusHandler);
        }

        boolean connected = false;
        for (int i = 0, c = mMasInstances.size(); i < c; i++) {
            try {
                if (mMasInstances.valueAt(i).startObexServerSession(mBluetoothMnsObexClient)) {
                    connected = true;
                }
            } catch (IOException e) {
                Log.w(TAG, "IOException occured while starting an obexServerSession restarting"
                        + " the listener", e);
                mMasInstances.valueAt(i).restartObexServerSession();
            } catch (RemoteException e) {
                Log.w(TAG, "RemoteException occured while starting an obexServerSession restarting"
                        + " the listener", e);
                mMasInstances.valueAt(i).restartObexServerSession();
            }
        }
        if (connected) {
            setState(BluetoothMap.STATE_CONNECTED);
        }

        mSessionStatusHandler.removeMessages(MSG_RELEASE_WAKE_LOCK);
        mSessionStatusHandler.sendMessageDelayed(
                mSessionStatusHandler.obtainMessage(MSG_RELEASE_WAKE_LOCK),
                RELEASE_WAKE_LOCK_DELAY);

        if (DEBUG) {
            Log.v(TAG, "startObexServerSessions() success!");
        }
    }

    public Handler getHandler() {
        return mSessionStatusHandler;
    }

    /**
     * Restart a MAS instances.
     * @param masId use -1 to stop all instances
     */
    private void stopObexServerSessions(int masId) {
        if (DEBUG) {
            Log.d(TAG, "MAP Service STOP ObexServerSessions() masId: " + masId);
        }

        boolean lastMasInst = true;

        if (masId != -1) {
            for (int i = 0, c = mMasInstances.size(); i < c; i++) {
                BluetoothMapMasInstance masInst = mMasInstances.valueAt(i);
                if (masInst.getMasId() != masId && masInst.isStarted()) {
                    lastMasInst = false;
                }
            }
        } // Else just close down it all

        // Shutdown the MNS client - this must happen before MAS close
        if (mBluetoothMnsObexClient != null ) {
            if (lastMasInst) {
                mBluetoothMnsObexClient.shutdown();
                mBluetoothMnsObexClient = null;
            } else if(masId != -1 && mMasInstances != null) {
                BluetoothMapFixes.handleMnsShutdown(mMasInstances,masId);
            }
        }

        BluetoothMapMasInstance masInst = mMasInstances.get(masId); // returns null for -1
        if (masInst != null) {
            masInst.restartObexServerSession();
        } else if (masId == -1) {
            for (int i = 0, c = mMasInstances.size(); i < c; i++) {
                mMasInstances.valueAt(i).restartObexServerSession();
            }
        }

        if (lastMasInst) {
            setState(BluetoothMap.STATE_DISCONNECTED);
            mPermission = BluetoothDevice.ACCESS_UNKNOWN;
            sRemoteDevice = null;
            // no need to invalidate cache here because setState did it above
            if (mAccountChanged) {
                updateMasInstances(UPDATE_MAS_INSTANCES_ACCOUNT_DISCONNECT);
            }
        }

        // Release the wake lock at disconnect
        if (mWakeLock != null && lastMasInst) {
            mSessionStatusHandler.removeMessages(MSG_ACQUIRE_WAKE_LOCK);
            mSessionStatusHandler.removeMessages(MSG_RELEASE_WAKE_LOCK);
            mWakeLock.release();
            if (VERBOSE) {
                Log.v(TAG, "stopObexServerSessions(): Release Wake Lock");
            }
        }
    }

    private final class MapServiceMessageHandler extends Handler {
        Context mContxt;
        private MapServiceMessageHandler(Context contxt, Looper looper) {
            super(looper);
            mContxt = contxt;
        }

        @Override
        public void handleMessage(Message msg) {
            if (DEBUG) {
                Log.v(TAG, "Handler(): got msg=" + msg.what);
            }

            switch (msg.what) {
                case CREATE_MAS_INSTANCES:
                    Log.d(TAG, "CREATE_MAS_INSTANCES ");
                    BluetoothMapFixes.createMasInstances(BluetoothMapService.this);
                    break;
                case UPDATE_MAS_INSTANCES:
                    updateMasInstancesHandler();
                    break;
                case START_LISTENER:
                    startSocketListeners(msg.arg1);
                    break;
                case MSG_MAS_CONNECT:
                    onConnectHandler(msg.arg1);
                    break;
                case MSG_MAS_CONNECT_CANCEL:
                    /* TODO: We need to handle this by accepting the connection and reject at
                     * OBEX level, by using ObexRejectServer - add timeout to handle clients not
                     * closing the transport channel.
                     */
                    stopObexServerSessions(-1);
                    break;
                case USER_TIMEOUT:
                    if (mIsWaitingAuthorization) {
                        Intent intent = new Intent(BluetoothDevice.ACTION_CONNECTION_ACCESS_CANCEL);
                        intent.setPackage(getString(R.string.pairing_ui_package));
                        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, sRemoteDevice);
                        intent.putExtra(BluetoothDevice.EXTRA_ACCESS_REQUEST_TYPE,
                                BluetoothDevice.REQUEST_TYPE_MESSAGE_ACCESS);
                        sendBroadcast(intent, BLUETOOTH_CONNECT,
                                Utils.getTempAllowlistBroadcastOptions());
                        cancelUserTimeoutAlarm();
                        mIsWaitingAuthorization = false;
                        stopObexServerSessions(-1);
                    }
                    break;
                case MSG_SERVERSESSION_CLOSE:
                    stopObexServerSessions(msg.arg1);
                    break;
                case MSG_SESSION_ESTABLISHED:
                    break;
                case MSG_SESSION_DISCONNECTED:
                    // handled elsewhere
                    break;
                case DISCONNECT_MAP:
                    disconnectMap((BluetoothDevice) msg.obj);
                    break;
                case SHUTDOWN:
                    // Call close from this handler to avoid starting because of pending messages
                    closeService();
                    break;
                case MSG_ACQUIRE_WAKE_LOCK:
                    if (VERBOSE) {
                        Log.v(TAG, "Acquire Wake Lock request message");
                    }
                    if (mWakeLock == null) {
                        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
                        mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,
                                "StartingObexMapTransaction");
                        mWakeLock.setReferenceCounted(false);
                    }
                    if (!mWakeLock.isHeld()) {
                        mWakeLock.acquire();
                        if (DEBUG) {
                            Log.d(TAG, "  Acquired Wake Lock by message");
                        }
                    }
                    mSessionStatusHandler.removeMessages(MSG_RELEASE_WAKE_LOCK);
                    mSessionStatusHandler.sendMessageDelayed(
                            mSessionStatusHandler.obtainMessage(MSG_RELEASE_WAKE_LOCK),
                            RELEASE_WAKE_LOCK_DELAY);
                    break;
                case MSG_RELEASE_WAKE_LOCK:
                    if (VERBOSE) {
                        Log.v(TAG, "Release Wake Lock request message");
                    }
                    if (mWakeLock != null) {
                        mWakeLock.release();
                        if (DEBUG) {
                            Log.d(TAG, "  Released Wake Lock by message");
                        }
                    }
                    break;
                case MSG_MNS_SDP_SEARCH:
                    if (sRemoteDevice != null) {
                        if (DEBUG) {
                            Log.d(TAG, "MNS SDP Initiate Search ..");
                        }
                        sRemoteDevice.sdpSearch(BluetoothMnsObexClient.BLUETOOTH_UUID_OBEX_MNS);
                    } else {
                        Log.w(TAG, "remoteDevice info not available");
                    }
                    break;
                case MSG_OBSERVER_REGISTRATION:
                    if (DEBUG) {
                        Log.d(TAG, "ContentObserver Registration MASID: " + msg.arg1 + " Enable: "
                                + msg.arg2);
                    }
                    BluetoothMapMasInstance masInst = mMasInstances.get(msg.arg1);
                    if (masInst != null && masInst.mObserver != null) {
                        try {
                            if (msg.arg2 == BluetoothMapAppParams.NOTIFICATION_STATUS_YES) {
                                masInst.mObserver.registerObserver();
                            } else {
                                masInst.mObserver.unregisterObserver();
                            }
                        } catch (RemoteException e) {
                            Log.e(TAG, "ContentObserverRegistarion Failed: " + e);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }

    private void onConnectHandler(int masId) {
        if (mIsWaitingAuthorization || sRemoteDevice == null || mSdpSearchInitiated) {
            return;
        }
        BluetoothMapMasInstance masInst = mMasInstances.get(masId);
        // Need to ensure we are still allowed.
        if (DEBUG) {
            Log.d(TAG, "mPermission = " + mPermission);
        }
        if (mPermission == BluetoothDevice.ACCESS_ALLOWED) {
            try {
                if (VERBOSE) {
                    Log.v(TAG, "incoming connection accepted from: " + sRemoteDeviceName
                            + " automatically as trusted device");
                }
                if (mBluetoothMnsObexClient != null && masInst != null) {
                    masInst.startObexServerSession(mBluetoothMnsObexClient);
                } else {
                    startObexServerSessions();
                }
            } catch (IOException ex) {
                Log.e(TAG, "catch IOException starting obex server session", ex);
            } catch (RemoteException ex) {
                Log.e(TAG, "catch RemoteException starting obex server session", ex);
            }
        }
    }

    public int getState() {
        return mState;
    }

    public static BluetoothDevice getRemoteDevice() {
        return sRemoteDevice;
    }

    private void setState(int state) {
        setState(state, BluetoothMap.RESULT_SUCCESS);
    }

    private synchronized void setState(int state, int result) {
        if (state != mState) {
            if (DEBUG) {
                Log.d(TAG, "Map state " + mState + " -> " + state + ", result = " + result);
            }
            int prevState = mState;
            mState = state;
            BluetoothMap.invalidateBluetoothGetConnectionStateCache();
            Intent intent = new Intent(BluetoothMap.ACTION_CONNECTION_STATE_CHANGED);
            intent.putExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, prevState);
            intent.putExtra(BluetoothProfile.EXTRA_STATE, mState);
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, sRemoteDevice);
            sendBroadcast(intent, BLUETOOTH_CONNECT, Utils.getTempAllowlistBroadcastOptions());
        }
    }

    /**
     * Disconnects MAP from the supplied device
     *
     * @param device is the device on which we want to disconnect MAP
     */
    public void disconnect(BluetoothDevice device) {
        mSessionStatusHandler.sendMessage(
                mSessionStatusHandler.obtainMessage(DISCONNECT_MAP, 0, 0, device));
    }

    void disconnectMap(BluetoothDevice device) {
        if (DEBUG) {
            Log.d(TAG, "disconnectMap");
        }
        if (getRemoteDevice() != null && getRemoteDevice().equals(device)) {
            switch (mState) {
                case BluetoothMap.STATE_CONNECTED:
                    // Disconnect all connections and restart all MAS instances
                    stopObexServerSessions(-1);
                    break;
                default:
                    break;
            }
        }
    }

    private List<BluetoothDevice> getConnectedDevices() {
        List<BluetoothDevice> devices = new ArrayList<>();
        synchronized (this) {
            if (mState == BluetoothMap.STATE_CONNECTED && sRemoteDevice != null) {
                devices.add(sRemoteDevice);
            }
        }
        return devices;
    }

    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        List<BluetoothDevice> deviceList = new ArrayList<>();
        Set<BluetoothDevice> bondedDevices = mAdapter.getBondedDevices();
        if (bondedDevices == null) {
            return deviceList;
        }
        synchronized (this) {
            for (BluetoothDevice device : bondedDevices) {
                ParcelUuid[] featureUuids = device.getUuids();
                if (!BluetoothUuid.containsAnyUuid(featureUuids, MAP_UUIDS)) {
                    continue;
                }
                int connectionState = getConnectionState(device);
                for (int state : states) {
                    if (connectionState == state) {
                        deviceList.add(device);
                    }
                }
            }
        }
        return deviceList;
    }

    /**
     * Gets the connection state of MAP with the passed in device.
     *
     * @param device is the device whose connection state we are querying
     * @return {@link BluetoothProfile#STATE_CONNECTED} if MAP is connected to this device,
     * {@link BluetoothProfile#STATE_DISCONNECTED} otherwise
     */
    public int getConnectionState(BluetoothDevice device) {
        synchronized (this) {
            if (getState() == BluetoothMap.STATE_CONNECTED && getRemoteDevice() != null
                    && getRemoteDevice().equals(device)) {
                return BluetoothProfile.STATE_CONNECTED;
            } else {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
        }
    }

    /**
     * Set connection policy of the profile and tries to disconnect it if connectionPolicy is
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
    boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        if (DEBUG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }

        if (!mDatabaseManager.setProfileConnectionPolicy(device, BluetoothProfile.MAP,
                 connectionPolicy)) {
            return false;
        }
        if (connectionPolicy == BluetoothProfile.CONNECTION_POLICY_FORBIDDEN) {
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
    int getConnectionPolicy(BluetoothDevice device) {
        enforceCallingOrSelfPermission(BLUETOOTH_PRIVILEGED,
                "Need BLUETOOTH_PRIVILEGED permission");
        return mDatabaseManager
                .getProfileConnectionPolicy(device, BluetoothProfile.MAP);
    }

    @Override
    protected IProfileServiceBinder initBinder() {
        return new BluetoothMapBinder(this);
    }

    @Override
    protected boolean start() {
        if (DEBUG) {
            Log.d(TAG, "start()");
        }

        mDatabaseManager = Objects.requireNonNull(AdapterService.getAdapterService().getDatabase(),
                "DatabaseManager cannot be null when MapService starts");

        HandlerThread thread = new HandlerThread("BluetoothMapHandler");
        thread.start();
        Looper looper = thread.getLooper();
        mSessionStatusHandler = new MapServiceMessageHandler(this, looper);

        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY);
        filter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
        filter.addAction(BluetoothDevice.ACTION_SDP_RECORD);
        filter.addAction(ACTION_SHOW_MAPS_SETTINGS);
        filter.addAction(USER_CONFIRM_TIMEOUT_ACTION);

        // We need two filters, since Type only applies to the ACTION_MESSAGE_SENT
        IntentFilter filterMessageSent = new IntentFilter();
        filterMessageSent.addAction(BluetoothMapContentObserver.ACTION_MESSAGE_SENT);
        try {
            filterMessageSent.addDataType("message/*");
        } catch (MalformedMimeTypeException e) {
            Log.e(TAG, "Wrong mime type!!!", e);
        }
        if (!mRegisteredMapReceiver) {
            try {
                registerReceiver(mMapReceiver, filter);
                // We need WRITE_SMS permission to handle messages in
                // actionMessageSentDisconnected()
                registerReceiver(mMapReceiver, filterMessageSent,
                        Manifest.permission.WRITE_SMS, null);
                mRegisteredMapReceiver = true;
            } catch (Exception e) {
                Log.e(TAG,"Unable to register map receiver",e);
            }
        }
        mAdapter = BluetoothAdapter.getDefaultAdapter();

        // Move SDP records creation to Handler Thread instead of main thread.
        BluetoothMapFixes.sendCreateMasInstances(this, CREATE_MAS_INSTANCES);

        TelephonyManager tm = (TelephonyManager) getSystemService(Context.TELEPHONY_SERVICE);
        mSmsCapable = tm.isSmsCapable();
        if (DEBUG) {
             Log.d(TAG, "mSmsCapable :" + mSmsCapable);
        }

        sendStartListenerMessage(-1);
        setBluetoothMapService(this);
        mServiceStarted = true;
        return true;
    }

    /**
     * Get the current instance of {@link BluetoothMapService}
     *
     * @return current instance of {@link BluetoothMapService}
     */
    @VisibleForTesting
    public static synchronized BluetoothMapService getBluetoothMapService() {
        if (sBluetoothMapService == null) {
            Log.w(TAG, "getBluetoothMapService(): service is null");
            return null;
        }
        if (!sBluetoothMapService.isAvailable()) {
            Log.w(TAG, "getBluetoothMapService(): service is not available");
            return null;
        }
        return sBluetoothMapService;
    }

    private static synchronized void setBluetoothMapService(BluetoothMapService instance) {
        if (DEBUG) {
            Log.d(TAG, "setBluetoothMapService(): set to: " + instance);
        }
        sBluetoothMapService = instance;
    }

    /**
     * Call this to trigger an update of the MAS instance list.
     * No changes will be applied unless in disconnected state
     */
    void updateMasInstances(int action) {
        mSessionStatusHandler.obtainMessage(UPDATE_MAS_INSTANCES, action, 0).sendToTarget();
    }

    /**
     * Update the active MAS Instances according the difference between mEnabledDevices
     * and the current list of accounts.
     * Will only make changes if state is disconnected.
     *
     * How it works:
     * 1) Build two lists of accounts
     *      newAccountList - all accounts from mAppObserver
     *      newAccounts - accounts that have been enabled since mEnabledAccounts
     *                    was last updated.
     *      mEnabledAccounts - The accounts which are left
     * 2) Stop and remove all MasInstances in mEnabledAccounts
     * 3) Add and start MAS instances for accounts on the new list.
     * Called at:
     *  - Each change in accounts
     *  - Each disconnect - before MasInstances restart.
     */
    private void updateMasInstancesHandler() {
        if (DEBUG) {
            Log.d(TAG, "updateMasInstancesHandler() state = " + getState());
        }
        if (BluetoothMapFixes.checkMapAppObserver(mAppObserver))
            return;
        if (getState() == BluetoothMap.STATE_CONNECTING) {
            mAccountChanged = true;
            return;
        }

        ArrayList<BluetoothMapAccountItem> newAccountList = mAppObserver.getEnabledAccountItems();
        ArrayList<BluetoothMapAccountItem> newAccounts = new ArrayList<>();

        Log.d(TAG, "new Account List size = " + newAccountList.size());
        SystemProperties.set("vendor.bluetooth.emailaccountcount",
                String.valueOf(newAccountList.size()));

        for (BluetoothMapAccountItem account : newAccountList) {
            if (!mEnabledAccounts.remove(account)) {
                newAccounts.add(account);
            }
        }

        // Remove all disabled/removed accounts
        if (mEnabledAccounts.size() > 0) {
            for (BluetoothMapAccountItem account : mEnabledAccounts) {
                BluetoothMapMasInstance masInst = mMasInstanceMap.remove(account);
                if (VERBOSE) {
                    Log.v(TAG, "  Removing account: " + account + " masInst = " + masInst);
                }
                if (masInst != null) {
                    masInst.shutdown();
                    mMasInstances.remove(masInst.getMasId());
                }
            }
        }

        // Add any newly created accounts
        for (BluetoothMapAccountItem account : newAccounts) {
            if (VERBOSE) {
                Log.v(TAG, "  Adding account: " + account);
            }
            int masId = getNextMasId();
            BluetoothMapMasInstance newInst =
                    new BluetoothMapMasInstance(this, this, account, masId, false);
            mMasInstances.append(masId, newInst);
            mMasInstanceMap.put(account, newInst);
            // Start the new instance
            if (mAdapter.isEnabled()) {
                newInst.startSocketListeners();
            }
        }

        mEnabledAccounts = newAccountList;
        if (VERBOSE) {
            Log.v(TAG, "  Enabled accounts:");
            for (BluetoothMapAccountItem account : mEnabledAccounts) {
                Log.v(TAG, "   " + account);
            }
            Log.v(TAG, "  Active MAS instances:");
            for (int i = 0, c = mMasInstances.size(); i < c; i++) {
                BluetoothMapMasInstance masInst = mMasInstances.valueAt(i);
                Log.v(TAG, "   " + masInst);
            }
        }
        mAccountChanged = false;
    }

    /**
     * Return a free key greater than the largest key in use.
     * If the key 255 is in use, the first free masId will be returned.
     * @return a free MasId
     */
    private int getNextMasId() {
        // Find the largest masId in use
        int largestMasId = 0;
        for (int i = 0, c = mMasInstances.size(); i < c; i++) {
            int masId = mMasInstances.keyAt(i);
            if (masId > largestMasId) {
                largestMasId = masId;
            }
        }
        if (largestMasId < 0xff) {
            return largestMasId + 1;
        }
        // If 0xff is already in use, wrap and choose the first free MasId.
        for (int i = 1; i <= 0xff; i++) {
            if (mMasInstances.get(i) == null) {
                return i;
            }
        }
        return 0xff; // This will never happen, as we only allow 10 e-mail accounts to be enabled
    }

    void createMasInstances() {
        int masId = MAS_ID_SMS_MMS;

        if (mSmsCapable) {
            // Add the SMS/MMS instance
            BluetoothMapMasInstance smsMmsInst =
                    new BluetoothMapMasInstance(this, this, null, masId, true);
            mMasInstances.append(masId, smsMmsInst);
            mMasInstanceMap.put(null, smsMmsInst);
            masId++;
        }

        if (mEnabledAccounts != null) {
            // get list of accounts already set to be visible through MAP
            for (BluetoothMapAccountItem account : mEnabledAccounts) {
                BluetoothMapMasInstance newInst =
                        new BluetoothMapMasInstance(this, this, account, masId, false);
                mMasInstances.append(masId, newInst);
                mMasInstanceMap.put(account, newInst);
                masId++;
            }
        }
    }

    @Override
    protected boolean stop() {
        if (DEBUG) {
            Log.d(TAG, "stop()");
        }
        if (!mServiceStarted) {
            if (DEBUG) {
                Log.d(TAG, "mServiceStarted is false - Ignoring");
            }
            return true;
        }
        setBluetoothMapService(null);
        mServiceStarted = false;
        if (mRegisteredMapReceiver) {
            mRegisteredMapReceiver = false;
            unregisterReceiver(mMapReceiver);
            if (mAppObserver != null)
                mAppObserver.shutdown();
        }
        setState(BluetoothMap.STATE_DISCONNECTED, BluetoothMap.RESULT_CANCELED);
        sendShutdownMessage();
        return true;
    }

    /**
     * Called from each MAS instance when a connection is received.
     * @param remoteDevice The device connecting
     * @param masInst a reference to the calling MAS instance.
     * @return true if the connection was accepted, false otherwise
     */
    public boolean onConnect(BluetoothDevice remoteDevice, BluetoothMapMasInstance masInst) {
        boolean sendIntent = false;
        boolean cancelConnection = false;

        // As this can be called from each MasInstance, we need to lock access to member variables
        synchronized (this) {
            if (sRemoteDevice == null) {
                sRemoteDevice = remoteDevice;
                if (getState() == BluetoothMap.STATE_CONNECTED) {
                    BluetoothMap.invalidateBluetoothGetConnectionStateCache();
                }
                sRemoteDeviceName = sRemoteDevice.getName();
                // In case getRemoteName failed and return null
                if (TextUtils.isEmpty(sRemoteDeviceName)) {
                    sRemoteDeviceName = getString(R.string.defaultname);
                }

                mPermission = sRemoteDevice.getMessageAccessPermission();
                if (mPermission == BluetoothDevice.ACCESS_UNKNOWN) {
                    sendIntent = true;
                    mIsWaitingAuthorization = true;
                    setUserTimeoutAlarm();
                } else if (mPermission == BluetoothDevice.ACCESS_REJECTED) {
                    cancelConnection = true;
                } else if (mPermission == BluetoothDevice.ACCESS_ALLOWED) {
                    sRemoteDevice.sdpSearch(BluetoothMnsObexClient.BLUETOOTH_UUID_OBEX_MNS);
                    mSdpSearchInitiated = true;
                }
            } else if (!remoteDevice.equals(sRemoteDevice)) {
                Log.w(TAG, "Unexpected connection from a second Remote Device received. name: " + (
                        (remoteDevice == null) ? "unknown" : remoteDevice.getName()));
                return false;
            } // Else second connection to same device, just continue
        }

        if (sendIntent) {
            // This will trigger Settings app's dialog.
            Intent intent = new Intent(BluetoothDevice.ACTION_CONNECTION_ACCESS_REQUEST);
            intent.setPackage(getString(R.string.pairing_ui_package));
            intent.putExtra(BluetoothDevice.EXTRA_ACCESS_REQUEST_TYPE,
                    BluetoothDevice.REQUEST_TYPE_MESSAGE_ACCESS);
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, sRemoteDevice);
            sendOrderedBroadcast(intent, BLUETOOTH_CONNECT,
                    Utils.getTempAllowlistBroadcastOptions(), null, null,
                    Activity.RESULT_OK, null, null);

            if (VERBOSE) {
                Log.v(TAG, "waiting for authorization for connection from: " + sRemoteDeviceName);
            }
            //Queue USER_TIMEOUT to disconnect MAP OBEX session. If user doesn't
            //accept or reject authorization request
        } else if (cancelConnection) {
            sendConnectCancelMessage();
        } else if (mPermission == BluetoothDevice.ACCESS_ALLOWED) {
            // Signal to the service that we have a incoming connection.
            sendConnectMessage(masInst.getMasId());
            MetricsLogger.logProfileConnectionEvent(BluetoothMetricsProto.ProfileId.MAP);
        }
        return true;
    }

    private void setUserTimeoutAlarm() {
        if (DEBUG) {
            Log.d(TAG, "SetUserTimeOutAlarm()");
        }
        if (mAlarmManager == null) {
            mAlarmManager = (AlarmManager) this.getSystemService(Context.ALARM_SERVICE);
        }
        mRemoveTimeoutMsg = true;
        Intent timeoutIntent = new Intent(USER_CONFIRM_TIMEOUT_ACTION);
        PendingIntent pIntent = PendingIntent.getBroadcast(this, 0, timeoutIntent,
                PendingIntent.FLAG_IMMUTABLE);
        mAlarmManager.set(AlarmManager.RTC_WAKEUP,
                System.currentTimeMillis() + USER_CONFIRM_TIMEOUT_VALUE, pIntent);
    }

    private void cancelUserTimeoutAlarm() {
        if (DEBUG) {
            Log.d(TAG, "cancelUserTimeOutAlarm()");
        }
        Intent timeoutIntent = new Intent(USER_CONFIRM_TIMEOUT_ACTION);
        PendingIntent pIntent = PendingIntent.getBroadcast(this, 0, timeoutIntent,
                PendingIntent.FLAG_IMMUTABLE);
        pIntent.cancel();

        AlarmManager alarmManager = (AlarmManager) this.getSystemService(Context.ALARM_SERVICE);
        alarmManager.cancel(pIntent);
        mRemoveTimeoutMsg = false;
    }

    /**
     * Start the incoming connection listeners for a MAS ID
     * @param masId the MasID to start. Use -1 to start all listeners.
     */
    void sendStartListenerMessage(int masId) {
        if (mSessionStatusHandler != null && !mSessionStatusHandler.hasMessages(START_LISTENER)) {
            Message msg = mSessionStatusHandler.obtainMessage(START_LISTENER, masId, 0);
            /* We add a small delay here to ensure the call returns true before this message is
             * handled. It seems wrong to add a delay, but the alternative is to build a lock
             * system to handle synchronization, which isn't nice either... */
            mSessionStatusHandler.sendMessageDelayed(msg, 20);
        } else if (mSessionStatusHandler != null) {
            if (DEBUG) {
                Log.w(TAG, "mSessionStatusHandler START_LISTENER message already in Queue");
            }
        }
    }

    private void sendConnectMessage(int masId) {
        if (mSessionStatusHandler != null) {
            Message msg = mSessionStatusHandler.obtainMessage(MSG_MAS_CONNECT, masId, 0);
            /* We add a small delay here to ensure onConnect returns true before this message is
             * handled. It seems wrong, but the alternative is to store a reference to the
             * connection in this message, which isn't nice either... */
            mSessionStatusHandler.sendMessageDelayed(msg, 20);
        } // Can only be null during shutdown
    }

    private void sendConnectTimeoutMessage() {
        if (DEBUG) {
            Log.d(TAG, "sendConnectTimeoutMessage()");
        }
        if (mSessionStatusHandler != null) {
            Message msg = mSessionStatusHandler.obtainMessage(USER_TIMEOUT);
            msg.sendToTarget();
        } // Can only be null during shutdown
    }

    private void sendConnectCancelMessage() {
        if (mSessionStatusHandler != null) {
            Message msg = mSessionStatusHandler.obtainMessage(MSG_MAS_CONNECT_CANCEL);
            msg.sendToTarget();
        } // Can only be null during shutdown
    }

    private void sendShutdownMessage() {
        if (VERBOSE) Log.d(TAG, "sendShutdownMessage() In");
        // Pending messages are no longer valid. To speed up things, simply delete them.
        if (mRemoveTimeoutMsg) {
            Intent timeoutIntent = new Intent(USER_CONFIRM_TIMEOUT_ACTION);
            sendBroadcast(timeoutIntent, null, Utils.getTempAllowlistBroadcastOptions());
            mIsWaitingAuthorization = false;
            cancelUserTimeoutAlarm();
        }
        if (mSessionStatusHandler == null) {
            Log.w(TAG, "mSessionStatusHandler is null");
            return;
        }
        if (mSessionStatusHandler.hasMessages(SHUTDOWN)) {
            if (DEBUG) {
                Log.w(TAG, "mSessionStatusHandler shutdown message already in Queue");
            }
            return;
        }
        mSessionStatusHandler.removeCallbacksAndMessages(null);
        // Request release of all resources
        Message msg = mSessionStatusHandler.obtainMessage(SHUTDOWN);
        if (!mSessionStatusHandler.sendMessage(msg)) {
            Log.w(TAG, "mSessionStatusHandler shutdown message could not be sent");
        }
    }

    private MapBroadcastReceiver mMapReceiver = new MapBroadcastReceiver();

    private class MapBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (DEBUG) {
                Log.d(TAG, "onReceive: " + action);
            }
            if (action.equals(USER_CONFIRM_TIMEOUT_ACTION)) {
                if (DEBUG) {
                    Log.d(TAG, "USER_CONFIRM_TIMEOUT ACTION Received.");
                }
                sendConnectTimeoutMessage();
            } else if (action.equals(BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY)) {

                int requestType = intent.getIntExtra(BluetoothDevice.EXTRA_ACCESS_REQUEST_TYPE,
                        BluetoothDevice.REQUEST_TYPE_PHONEBOOK_ACCESS);

                if (DEBUG) {
                    Log.d(TAG, "Received ACTION_CONNECTION_ACCESS_REPLY:" + requestType
                            + "isWaitingAuthorization:" + mIsWaitingAuthorization);
                }
                if ((!mIsWaitingAuthorization) || (requestType
                        != BluetoothDevice.REQUEST_TYPE_MESSAGE_ACCESS)) {
                    return;
                }

                mIsWaitingAuthorization = false;
                if (mRemoveTimeoutMsg) {
                    mSessionStatusHandler.removeMessages(USER_TIMEOUT);
                    cancelUserTimeoutAlarm();
                    setState(BluetoothMap.STATE_DISCONNECTED);
                }

                if (intent.getIntExtra(BluetoothDevice.EXTRA_CONNECTION_ACCESS_RESULT,
                        BluetoothDevice.CONNECTION_ACCESS_NO)
                        == BluetoothDevice.CONNECTION_ACCESS_YES) {
                    // Bluetooth connection accepted by user
                    mPermission = BluetoothDevice.ACCESS_ALLOWED;
                    if (intent.getBooleanExtra(BluetoothDevice.EXTRA_ALWAYS_ALLOWED, false)) {
                        boolean result = sRemoteDevice.setMessageAccessPermission(
                                BluetoothDevice.ACCESS_ALLOWED);
                        if (DEBUG) {
                            Log.d(TAG,
                                    "setMessageAccessPermission(ACCESS_ALLOWED) result=" + result);
                        }
                    }

                    sRemoteDevice.sdpSearch(BluetoothMnsObexClient.BLUETOOTH_UUID_OBEX_MNS);
                    mSdpSearchInitiated = true;
                } else {
                    // Auth. declined by user, serverSession should not be running, but
                    // call stop anyway to restart listener.
                    mPermission = BluetoothDevice.ACCESS_REJECTED;
                    if (intent.getBooleanExtra(BluetoothDevice.EXTRA_ALWAYS_ALLOWED, false)) {
                        boolean result = sRemoteDevice.setMessageAccessPermission(
                                BluetoothDevice.ACCESS_REJECTED);
                        if (DEBUG) {
                            Log.d(TAG,
                                    "setMessageAccessPermission(ACCESS_REJECTED) result=" + result);
                        }
                    }
                    sendConnectCancelMessage();
                }
            } else if (action.equals(BluetoothDevice.ACTION_SDP_RECORD)) {
                if (DEBUG) {
                    Log.d(TAG, "Received ACTION_SDP_RECORD.");
                }
                ParcelUuid uuid = intent.getParcelableExtra(BluetoothDevice.EXTRA_UUID);
                BluetoothDevice remoteDevice = intent.getParcelableExtra(
                        BluetoothDevice.EXTRA_DEVICE);
                if (VERBOSE) {
                    Log.v(TAG, "Received UUID: " + uuid.toString());
                    Log.v(TAG, "expected UUID: "
                            + BluetoothMnsObexClient.BLUETOOTH_UUID_OBEX_MNS.toString());
                }
                if (uuid.equals(BluetoothMnsObexClient.BLUETOOTH_UUID_OBEX_MNS)) {
                    mMnsRecord = intent.getParcelableExtra(BluetoothDevice.EXTRA_SDP_RECORD);
                    int status = intent.getIntExtra(BluetoothDevice.EXTRA_SDP_SEARCH_STATUS, -1);
                    if (VERBOSE) {
                        Log.v(TAG, " -> MNS Record:" + mMnsRecord);
                        Log.v(TAG, " -> status: " + status);
                    }
                    if (mBluetoothMnsObexClient != null && !mSdpSearchInitiated) {
                        mBluetoothMnsObexClient.setMnsRecord(mMnsRecord);
                    }
                    if (status != -1 && mMnsRecord != null) {
                        BluetoothMapFixes.showNotification(BluetoothMapService.this, remoteDevice);
                        for (int i = 0, c = mMasInstances.size(); i < c; i++) {
                            mMasInstances.valueAt(i)
                                    .setRemoteFeatureMask(mMnsRecord.getSupportedFeatures(),
                                    mMnsRecord.getProfileVersion(), remoteDevice);
                        }
                    }
                    if (mSdpSearchInitiated) {
                        mSdpSearchInitiated = false; // done searching
                        sendConnectMessage(-1); // -1 indicates all MAS instances
                    }
                }
            } else if (action.equals(ACTION_SHOW_MAPS_SETTINGS)) {
                if (VERBOSE) {
                    Log.v(TAG, "Received ACTION_SHOW_MAPS_SETTINGS.");
                }

                Intent in = new Intent(context, BluetoothMapSettings.class);
                in.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
                context.startActivity(in);
            } else if (action.equals(BluetoothMapContentObserver.ACTION_MESSAGE_SENT)) {
                int result = getResultCode();
                boolean handled = false;
                if (mSmsCapable && mMasInstances != null) {
                    BluetoothMapMasInstance masInst = mMasInstances.get(MAS_ID_SMS_MMS);
                    if (masInst != null) {
                        intent.putExtra(BluetoothMapContentObserver.EXTRA_MESSAGE_SENT_RESULT,
                                result);
                        handled = masInst.handleSmsSendIntent(context, intent);
                    }
                }
                if (!handled) {
                    // Move the SMS to the correct folder.
                    try {
                        BluetoothMapContentObserver.actionMessageSentDisconnected(context, intent,
                                result);
                    } catch(IllegalArgumentException e) {
                        return;
                    }
                }
            } else if (action.equals(BluetoothDevice.ACTION_ACL_DISCONNECTED)) {
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);

                if (sRemoteDevice == null || device == null) {
                    Log.i(TAG, "sRemoteDevice :" + sRemoteDevice + " device:" + device);
                    return;
                }

                if (VERBOSE) {
                    Log.v(TAG, "ACL disconnected for " + device
                        + " mIsWaitingAuthorization :" + mIsWaitingAuthorization);
                }

                if (mIsWaitingAuthorization && device.equals(sRemoteDevice)) {
                    // Send any pending timeout now, since ACL got disconnected
                    mSessionStatusHandler.removeMessages(USER_TIMEOUT);
                    mSessionStatusHandler.obtainMessage(USER_TIMEOUT).sendToTarget();
                }
            }
        }
    }

    //Binder object: Must be static class or memory leak may occur

    /**
     * This class implements the IBluetoothMap interface - or actually it validates the
     * preconditions for calling the actual functionality in the MapService, and calls it.
     */
    private static class BluetoothMapBinder extends IBluetoothMap.Stub
            implements IProfileServiceBinder {
        private BluetoothMapService mService;

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
        private BluetoothMapService getService(AttributionSource source) {
            if (!Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkCallerIsSystemOrActiveOrManagedUser(mService, TAG)
                    || !Utils.checkConnectPermissionForDataDelivery(mService, source, TAG)) {
                return null;
            }
            return mService;
        }

        BluetoothMapBinder(BluetoothMapService service) {
            if (VERBOSE) {
                Log.v(TAG, "BluetoothMapBinder()");
            }
            mService = service;
        }

        @Override
        public synchronized void cleanup() {
            mService = null;
        }

        @Override
        public void getState(AttributionSource source, SynchronousResultReceiver receiver) {
            if (VERBOSE) {
                Log.v(TAG, "getState()");
            }
            try {
                BluetoothMapService service = getService(source);
                int result = BluetoothMap.STATE_DISCONNECTED;
                if (service != null) {
                    result = service.getState();
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getClient(AttributionSource source, SynchronousResultReceiver receiver) {
            if (VERBOSE) {
                Log.v(TAG, "getClient()");
            }
            try {
                BluetoothMapService service = getService(source);
                BluetoothDevice client = null;
                if (service != null) {
                    client = BluetoothMapService.getRemoteDevice();
                }
                if (VERBOSE) {
                    Log.v(TAG, "getClient() - returning " + client);
                }
                receiver.send(client);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void isConnected(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            if (VERBOSE) {
                Log.v(TAG, "isConnected()");
            }
            try {
                BluetoothMapService service = getService(source);
                boolean result = false;
                if (service != null) {
                    result = service.getConnectionState(device)
                        == BluetoothProfile.STATE_CONNECTED;
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void disconnect(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            if (VERBOSE) {
                Log.v(TAG, "disconnect()");
            }
            try {
                BluetoothMapService service = getService(source);
                boolean result = false;
                if (service != null) {
                    service.disconnect(device);
                    result = true;
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectedDevices(AttributionSource source,
                SynchronousResultReceiver receiver) {
            if (VERBOSE) {
                Log.v(TAG, "getConnectedDevices()");
            }
            try {
                BluetoothMapService service = getService(source);
                List<BluetoothDevice> connectedDevices = new ArrayList<>(0);
                if (service != null) {
                    enforceBluetoothPrivilegedPermission(service);
                    connectedDevices = service.getConnectedDevices();
                }
                receiver.send(connectedDevices);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getDevicesMatchingConnectionStates(int[] states,
                AttributionSource source, SynchronousResultReceiver receiver) {
            if (VERBOSE) {
                Log.v(TAG, "getDevicesMatchingConnectionStates()");
            }
            try {
                BluetoothMapService service = getService(source);
                List<BluetoothDevice> devices = new ArrayList<>(0);
                if (service != null) {
                    devices = service.getDevicesMatchingConnectionStates(states);
                }
                receiver.send(devices);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectionState(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            if (VERBOSE) {
                Log.v(TAG, "getConnectionState()");
            }
            try {
                BluetoothMapService service = getService(source);
                int state = BluetoothProfile.STATE_DISCONNECTED;
                if (service != null) {
                    state = service.getConnectionState(device);
                }
                receiver.send(state);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void setConnectionPolicy(BluetoothDevice device, int connectionPolicy,
                AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                BluetoothMapService service = getService(source);
                boolean result = false;
                if (service != null) {
                    result = service.setConnectionPolicy(device, connectionPolicy);
                }
                receiver.send(result);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }

        @Override
        public void getConnectionPolicy(BluetoothDevice device, AttributionSource source,
                SynchronousResultReceiver receiver) {
            try {
                BluetoothMapService service = getService(source);
                int policy = BluetoothProfile.CONNECTION_POLICY_UNKNOWN;
                if (service != null) {
                    policy = service.getConnectionPolicy(device);
                }
                receiver.send(policy);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
    }

    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        println(sb, "mRemoteDevice: " + sRemoteDevice);
        println(sb, "sRemoteDeviceName: " + sRemoteDeviceName);
        println(sb, "mState: " + mState);
        if (mAppObserver != null)
            println(sb, "mAppObserver: " + mAppObserver);
        println(sb, "mIsWaitingAuthorization: " + mIsWaitingAuthorization);
        println(sb, "mRemoveTimeoutMsg: " + mRemoveTimeoutMsg);
        println(sb, "mPermission: " + mPermission);
        println(sb, "mAccountChanged: " + mAccountChanged);
        println(sb, "mBluetoothMnsObexClient: " + mBluetoothMnsObexClient);
        println(sb, "mMasInstanceMap:");
        for (BluetoothMapAccountItem key : mMasInstanceMap.keySet()) {
            println(sb, "  " + key + " : " + mMasInstanceMap.get(key));
        }
        println(sb, "mEnabledAccounts:");
        if (mEnabledAccounts != null) {
            for (BluetoothMapAccountItem account : mEnabledAccounts) {
                println(sb, "  " + account);
            }
        }
    }
}
