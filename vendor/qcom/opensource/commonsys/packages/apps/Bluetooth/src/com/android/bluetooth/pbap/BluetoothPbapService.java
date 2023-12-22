/*
 * Copyright (c) 2008-2009, Motorola, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * - Neither the name of the Motorola, Inc. nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 */


package com.android.bluetooth.pbap;

import static android.Manifest.permission.BLUETOOTH_CONNECT;

import android.annotation.RequiresPermission;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.BluetoothUuid;
import android.bluetooth.IBluetoothPbap;
import android.content.AttributionSource;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.database.sqlite.SQLiteException;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.PowerManager;
import android.os.UserManager;
import android.sysprop.BluetoothProperties;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.android.bluetooth.IObexConnectionHandler;
import com.android.bluetooth.ObexServerSockets;
import com.android.bluetooth.R;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.btservice.storage.DatabaseManager;
import com.android.bluetooth.sdp.SdpManager;
import com.android.bluetooth.util.DevicePolicyUtils;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;

public class BluetoothPbapService extends ProfileService implements IObexConnectionHandler {
    private static final String TAG = "BluetoothPbapService";
    private static final String LOG_TAG = "BluetoothPbap";

    /**
     * To enable PBAP DEBUG/VERBOSE logging - run below cmd in adb shell, and
     * restart com.android.bluetooth process. only enable DEBUG log:
     * "setprop log.tag.BluetoothPbapService DEBUG"; enable both VERBOSE and
     * DEBUG log: "setprop log.tag.BluetoothPbap VERBOSE"
     */

    public static final boolean DEBUG = true;

    public static final boolean VERBOSE = Log.isLoggable(LOG_TAG, Log.VERBOSE);

    private static final String PBAP_ACTIVITY
            = BluetoothPbapActivity.class.getCanonicalName();

    /**
     * Intent indicating incoming obex authentication request which is from
     * PCE(Carkit)
     */
    static final String AUTH_CHALL_ACTION = "com.android.bluetooth.pbap.authchall";

    /**
     * Intent indicating obex session key input complete by user which is sent
     * from BluetoothPbapActivity
     */
    static final String AUTH_RESPONSE_ACTION = "com.android.bluetooth.pbap.authresponse";

    /**
     * Intent indicating user canceled obex authentication session key input
     * which is sent from BluetoothPbapActivity
     */
    static final String AUTH_CANCELLED_ACTION = "com.android.bluetooth.pbap.authcancelled";

    /**
     * Intent indicating timeout for user confirmation, which is sent to
     * BluetoothPbapActivity
     */
    static final String USER_CONFIRM_TIMEOUT_ACTION =
            "com.android.bluetooth.pbap.userconfirmtimeout";

    /**
     * Intent Extra name indicating session key which is sent from
     * BluetoothPbapActivity
     */
    static final String EXTRA_SESSION_KEY = "com.android.bluetooth.pbap.sessionkey";
    static final String EXTRA_DEVICE = "com.android.bluetooth.pbap.device";
    static final String THIS_PACKAGE_NAME = "com.android.bluetooth";

    static final int MSG_ACQUIRE_WAKE_LOCK = 5004;
    static final int MSG_RELEASE_WAKE_LOCK = 5005;
    static final int MSG_STATE_MACHINE_DONE = 5006;

    static final int START_LISTENER = 1;
    static final int USER_TIMEOUT = 2;
    static final int SHUTDOWN = 3;
    static final int LOAD_CONTACTS = 4;
    static final int CONTACTS_LOADED = 5;
    static final int CHECK_SECONDARY_VERSION_COUNTER = 6;
    static final int ROLLOVER_COUNTERS = 7;
    static final int GET_LOCAL_TELEPHONY_DETAILS = 8;
    static final int CLEANUP_HANDLER_TASKS = 9;
    static final int HANDLE_VERSION_UPDATE_NOTIFICATION = 10;

    static final int USER_CONFIRM_TIMEOUT_VALUE = 30000;
    static final int RELEASE_WAKE_LOCK_DELAY = 10000;
    static final int CLEANUP_HANDLER_DELAY = 50;
    static final int VERSION_UPDATE_NOTIFICATION_DELAY = 500;

    private PowerManager.WakeLock mWakeLock;

    private static String sLocalPhoneNum;
    private static String sLocalPhoneName;

    private ObexServerSockets mServerSockets = null;
    private DatabaseManager mDatabaseManager;

    private static final int SDP_PBAP_SERVER_VERSION = 0x0102;
    private static final int SDP_PBAP_SUPPORTED_REPOSITORIES = 0x0001;
    private static final int SDP_PBAP_SUPPORTED_FEATURES = 0x021F;

    /* PBAP will use Bluetooth notification ID from 1000000 (included) to 2000000 (excluded).
       The notification ID should be unique in Bluetooth package. */
    private static final int PBAP_NOTIFICATION_ID_START = 1000000;
    private static final int PBAP_NOTIFICATION_ID_END = 2000000;

    protected int mSdpHandle = -1;

    protected Context mContext;

    private PbapHandler mSessionStatusHandler;
    private HandlerThread mHandlerThread;
    private final HashMap<BluetoothDevice, PbapStateMachine> mPbapStateMachineMap = new HashMap<>();
    private volatile int mNextNotificationId = PBAP_NOTIFICATION_ID_START;

    // package and class name to which we send intent to check phone book access permission
    private static final String ACCESS_AUTHORITY_PACKAGE = "com.android.settings";
    private static final String ACCESS_AUTHORITY_CLASS =
            "com.android.settings.bluetooth.BluetoothPermissionRequest";

    private Thread mThreadLoadContacts;
    private boolean mContactsLoaded = false;

    private Thread mThreadUpdateSecVersionCounter;

    private static BluetoothPbapService sBluetoothPbapService;

    private class BluetoothPbapContentObserver extends ContentObserver {
        BluetoothPbapContentObserver() {
            super(new Handler());
        }

        @Override
        public void onChange(boolean selfChange) {
            Log.d(TAG, " onChange on contact uri ");
            sendUpdateRequest();
        }
    }

    public static boolean isEnabled() {
        return BluetoothProperties.isProfilePbapServerEnabled().orElse(false);
    }

    private void sendUpdateRequest() {
        if (mContactsLoaded) {
            if (!mSessionStatusHandler.hasMessages(CHECK_SECONDARY_VERSION_COUNTER)) {
                mSessionStatusHandler.sendMessage(
                        mSessionStatusHandler.obtainMessage(CHECK_SECONDARY_VERSION_COUNTER));
            }
        }
    }

    private BluetoothPbapContentObserver mContactChangeObserver;

    private void parseIntent(final Intent intent) {
        String action = intent.getAction();
        if (DEBUG) {
            Log.d(TAG, "action: " + action);
        }
        if (BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY.equals(action)) {
            int requestType = intent.getIntExtra(BluetoothDevice.EXTRA_ACCESS_REQUEST_TYPE,
                    BluetoothDevice.REQUEST_TYPE_PHONEBOOK_ACCESS);
            if (requestType != BluetoothDevice.REQUEST_TYPE_PHONEBOOK_ACCESS) {
                return;
            }

            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            synchronized (mPbapStateMachineMap) {
                PbapStateMachine sm = mPbapStateMachineMap.get(device);
                if (sm == null) {
                    Log.w(TAG, "device not connected! device=" + device);
                    return;
                }
                mSessionStatusHandler.removeMessages(USER_TIMEOUT, sm);
                int access = intent.getIntExtra(BluetoothDevice.EXTRA_CONNECTION_ACCESS_RESULT,
                        BluetoothDevice.CONNECTION_ACCESS_NO);
                boolean savePreference = intent.getBooleanExtra(
                        BluetoothDevice.EXTRA_ALWAYS_ALLOWED, false);

                if (access == BluetoothDevice.CONNECTION_ACCESS_YES) {
                    if (savePreference) {
                        device.setPhonebookAccessPermission(BluetoothDevice.ACCESS_ALLOWED);
                        if (DEBUG) {
                            Log.v(TAG, "setPhonebookAccessPermission(ACCESS_ALLOWED)");
                        }
                    }
                    sm.sendMessage(PbapStateMachine.AUTHORIZED);
                } else {
                    if (savePreference) {
                        device.setPhonebookAccessPermission(BluetoothDevice.ACCESS_REJECTED);
                        if (DEBUG) {
                            Log.v(TAG, "setPhonebookAccessPermission(ACCESS_REJECTED)");
                        }
                    }
                    sm.sendMessage(PbapStateMachine.REJECTED);
                }
            }
        } else if (AUTH_RESPONSE_ACTION.equals(action)) {
            String sessionKey = intent.getStringExtra(EXTRA_SESSION_KEY);
            BluetoothDevice device = intent.getParcelableExtra(EXTRA_DEVICE);
            synchronized (mPbapStateMachineMap) {
                PbapStateMachine sm = mPbapStateMachineMap.get(device);
                if (sm == null) {
                    return;
                }
                Message msg = sm.obtainMessage(PbapStateMachine.AUTH_KEY_INPUT, sessionKey);
                sm.sendMessage(msg);
            }
        } else if (AUTH_CANCELLED_ACTION.equals(action)) {
            BluetoothDevice device = intent.getParcelableExtra(EXTRA_DEVICE);
            synchronized (mPbapStateMachineMap) {
                PbapStateMachine sm = mPbapStateMachineMap.get(device);
                if (sm == null) {
                    return;
                }
                sm.sendMessage(PbapStateMachine.AUTH_CANCELLED);
            }
        } else if (BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(action)) {
            int bondState = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE,
                    BluetoothDevice.ERROR);
            if (bondState == BluetoothDevice.BOND_BONDED && BluetoothPbapFixes.isSupportedPbap12) {
                BluetoothDevice remoteDevice = intent.getParcelableExtra(
                        BluetoothDevice.EXTRA_DEVICE);
                mSessionStatusHandler.sendMessageDelayed(
                            mSessionStatusHandler.obtainMessage(
                            HANDLE_VERSION_UPDATE_NOTIFICATION, remoteDevice),
                            VERSION_UPDATE_NOTIFICATION_DELAY);
            }
        } else {
            Log.w(TAG, "Unhandled intent action: " + action);
        }
    }

    private BroadcastReceiver mPbapReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            parseIntent(intent);
        }
    };

    private void closeService() {
        if (VERBOSE) {
            Log.v(TAG, "Pbap Service closeService");
        }

        BluetoothPbapUtils.savePbapParams(this);

        if (mWakeLock != null) {
            mWakeLock.release();
            mWakeLock = null;
        }

        cleanUpServerSocket();

        if (mSessionStatusHandler != null) {
            mSessionStatusHandler.removeCallbacksAndMessages(null);
        }

        mSessionStatusHandler.sendMessageDelayed(
            mSessionStatusHandler.obtainMessage(CLEANUP_HANDLER_TASKS), CLEANUP_HANDLER_DELAY);
    }

    private void cleanUpHandlerTasks() {
        Log.d(TAG, "quitHandlerThread");
        if (mHandlerThread != null) {
            mHandlerThread.quitSafely();
        }
    }

    private void cleanUpServerSocket() {
        // Step 1, 2: clean up active server session and connection socket
        synchronized (mPbapStateMachineMap) {
            for (PbapStateMachine stateMachine : mPbapStateMachineMap.values()) {
                stateMachine.sendMessage(PbapStateMachine.DISCONNECT);
            }
        }
        // Step 3: clean up SDP record
        cleanUpSdpRecord();
        // Step 4: clean up existing server sockets
        if (mServerSockets != null) {
            mServerSockets.shutdown(false);
            mServerSockets = null;
        }
    }

    private void createSdpRecord() {
        if (mSdpHandle > -1) {
            Log.w(TAG, "createSdpRecord, SDP record already created");
        }
        BluetoothPbapFixes.getFeatureSupport(mContext);
        BluetoothPbapFixes.createSdpRecord(mServerSockets, this);

        if (DEBUG) {
            Log.d(TAG, "created Sdp record, mSdpHandle=" + mSdpHandle);
        }
    }

    private void cleanUpSdpRecord() {
        if (mSdpHandle < 0) {
            Log.w(TAG, "cleanUpSdpRecord, SDP record never created");
            return;
        }
        int sdpHandle = mSdpHandle;
        mSdpHandle = -1;
        SdpManager sdpManager = SdpManager.getDefaultManager();
        if (DEBUG) {
            Log.d(TAG, "cleanUpSdpRecord, mSdpHandle=" + sdpHandle);
        }
        if (sdpManager == null) {
            Log.e(TAG, "sdpManager is null");
        } else if (!sdpManager.removeSdpRecord(sdpHandle)) {
            Log.w(TAG, "cleanUpSdpRecord, removeSdpRecord failed, sdpHandle=" + sdpHandle);
        }
    }

    private class PbapHandler extends Handler {
        private PbapHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            if (VERBOSE) {
                Log.v(TAG, "Handler(): got msg=" + msg.what);
            }

            switch (msg.what) {
                case START_LISTENER:
                    mServerSockets = ObexServerSockets.createWithFixedChannels
                            (sBluetoothPbapService, SdpManager.PBAP_RFCOMM_CHANNEL,
                            SdpManager.PBAP_L2CAP_PSM);
                    if (mServerSockets == null) {
                        Log.w(TAG, "ObexServerSockets.create() returned null");
                        break;
                    }
                    createSdpRecord();
                    // fetch Pbap Params to check if significant change has happened to Database
                    BluetoothPbapUtils.fetchPbapParams(mContext);
                    break;
                case USER_TIMEOUT:
                    Intent intent = new Intent(BluetoothDevice.ACTION_CONNECTION_ACCESS_CANCEL);
                    intent.setPackage(getString(R.string.pairing_ui_package));
                    PbapStateMachine stateMachine = (PbapStateMachine) msg.obj;
                    intent.putExtra(BluetoothDevice.EXTRA_DEVICE, stateMachine.getRemoteDevice());
                    intent.putExtra(BluetoothDevice.EXTRA_ACCESS_REQUEST_TYPE,
                            BluetoothDevice.REQUEST_TYPE_PHONEBOOK_ACCESS);
                    sendBroadcast(intent, BLUETOOTH_CONNECT,
                            Utils.getTempAllowlistBroadcastOptions());
                    stateMachine.sendMessage(PbapStateMachine.REJECTED);
                    break;
                case MSG_ACQUIRE_WAKE_LOCK:
                    if (mWakeLock == null) {
                        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
                        mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,
                                "StartingObexPbapTransaction");
                        mWakeLock.setReferenceCounted(false);
                        mWakeLock.acquire();
                        Log.w(TAG, "Acquire Wake Lock");
                    }
                    mSessionStatusHandler.removeMessages(MSG_RELEASE_WAKE_LOCK);
                    mSessionStatusHandler.sendMessageDelayed(
                            mSessionStatusHandler.obtainMessage(MSG_RELEASE_WAKE_LOCK),
                            RELEASE_WAKE_LOCK_DELAY);
                    break;
                case MSG_RELEASE_WAKE_LOCK:
                    if (mWakeLock != null) {
                        mWakeLock.release();
                        mWakeLock = null;
                    }
                    break;
                case SHUTDOWN:
                    closeService();
                    break;
                case LOAD_CONTACTS:
                    loadAllContacts();
                    break;
                case CONTACTS_LOADED:
                    mContactsLoaded = true;
                    break;
                case CHECK_SECONDARY_VERSION_COUNTER:
                    updateSecondaryVersion();
                    break;
                case ROLLOVER_COUNTERS:
                    BluetoothPbapUtils.rolloverCounters();
                    break;
                case MSG_STATE_MACHINE_DONE:
                    PbapStateMachine sm = (PbapStateMachine) msg.obj;
                    BluetoothDevice remoteDevice = sm.getRemoteDevice();
                    sm.quitNow();
                    synchronized (mPbapStateMachineMap) {
                        mPbapStateMachineMap.remove(remoteDevice);
                    }
                    break;
                case GET_LOCAL_TELEPHONY_DETAILS:
                    getLocalTelephonyDetails();
                    break;
                case CLEANUP_HANDLER_TASKS:
                    cleanUpHandlerTasks();
                    break;
                case HANDLE_VERSION_UPDATE_NOTIFICATION:
                    BluetoothDevice remoteDev = (BluetoothDevice) msg.obj;
                    BluetoothPbapFixes.handleNotificationTask(
                            sBluetoothPbapService, remoteDev);
                    break;
                default:
                    break;
            }
        }
    }

    /**
     * Get the current connection state of PBAP with the passed in device
     *
     * @param device is the device whose connection state to PBAP we are trying to get
     * @return current connection state, one of {@link BluetoothProfile#STATE_DISCONNECTED},
     * {@link BluetoothProfile#STATE_CONNECTING}, {@link BluetoothProfile#STATE_CONNECTED}, or
     * {@link BluetoothProfile#STATE_DISCONNECTING}
     */
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_PRIVILEGED)
    public int getConnectionState(BluetoothDevice device) {
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH_PRIVILEGED permission");
        if (mPbapStateMachineMap == null) {
            return BluetoothProfile.STATE_DISCONNECTED;
        }

        synchronized (mPbapStateMachineMap) {
            PbapStateMachine sm = mPbapStateMachineMap.get(device);
            if (sm == null) {
                return BluetoothProfile.STATE_DISCONNECTED;
            }
            return sm.getConnectionState();
        }
    }

    List<BluetoothDevice> getConnectedDevices() {
        if (mPbapStateMachineMap == null) {
            return new ArrayList<>();
        }
        synchronized (mPbapStateMachineMap) {
            return new ArrayList<>(mPbapStateMachineMap.keySet());
        }
    }

    List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states) {
        List<BluetoothDevice> devices = new ArrayList<>();
        if (mPbapStateMachineMap == null || states == null) {
            return devices;
        }
        synchronized (mPbapStateMachineMap) {
            for (int state : states) {
                for (BluetoothDevice device : mPbapStateMachineMap.keySet()) {
                    if (state == mPbapStateMachineMap.get(device).getConnectionState()) {
                        devices.add(device);
                    }
                }
            }
        }
        return devices;
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
    public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy) {
        enforceCallingOrSelfPermission(
                BLUETOOTH_PRIVILEGED, "Need BLUETOOTH_PRIVILEGED permission");
        if (DEBUG) {
            Log.d(TAG, "Saved connectionPolicy " + device + " = " + connectionPolicy);
        }

        if (!mDatabaseManager.setProfileConnectionPolicy(device, BluetoothProfile.PBAP,
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
    public int getConnectionPolicy(BluetoothDevice device) {
        if (device == null) {
            throw new IllegalArgumentException("Null device");
        }
        return mDatabaseManager
                .getProfileConnectionPolicy(device, BluetoothProfile.PBAP);
    }

    /**
     * Disconnects pbap server profile with device
     * @param device is the remote bluetooth device
     */
    public void disconnect(BluetoothDevice device) {
        synchronized (mPbapStateMachineMap) {
            PbapStateMachine sm = mPbapStateMachineMap.get(device);
            if (sm != null) {
                sm.sendMessage(PbapStateMachine.DISCONNECT);
            }
        }
    }

    static String getLocalPhoneNum() {
        return sLocalPhoneNum;
    }

    static String getLocalPhoneName() {
        return sLocalPhoneName;
    }

    @Override
    protected IProfileServiceBinder initBinder() {
        return new PbapBinder(this);
    }

    @Override
    protected boolean start() {
        if (VERBOSE) {
            Log.v(TAG, "start()");
        }
        mDatabaseManager = Objects.requireNonNull(AdapterService.getAdapterService().getDatabase(),
            "DatabaseManager cannot be null when PbapService starts");
        setComponentAvailable(PBAP_ACTIVITY, true);
        mContext = this;
        mContactsLoaded = false;
        mHandlerThread = new HandlerThread("PbapHandlerThread");
        mHandlerThread.start();
        mSessionStatusHandler = new PbapHandler(mHandlerThread.getLooper());
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY);
        filter.addAction(AUTH_RESPONSE_ACTION);
        filter.addAction(AUTH_CANCELLED_ACTION);
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        BluetoothPbapConfig.init(this);
        registerReceiver(mPbapReceiver, filter);
        try {
            mContactChangeObserver = new BluetoothPbapContentObserver();
            getContentResolver().registerContentObserver(
                    DevicePolicyUtils.getEnterprisePhoneUri(this), false,
                    mContactChangeObserver);
        } catch (SQLiteException e) {
            Log.e(TAG, "SQLite exception: " + e);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Illegal state exception, content observer is already registered");
        } catch (SecurityException e) {
            Log.e(TAG, "Error while rigistering ContactChangeObserver " + e);
        }

        setBluetoothPbapService(this);

        mSessionStatusHandler.sendMessage(
                mSessionStatusHandler.obtainMessage(GET_LOCAL_TELEPHONY_DETAILS));
        mSessionStatusHandler.sendMessage(mSessionStatusHandler.obtainMessage(LOAD_CONTACTS));
        mSessionStatusHandler.sendMessage(mSessionStatusHandler.obtainMessage(START_LISTENER));
        return true;
    }

    @Override
    protected boolean stop() {
        if (VERBOSE) {
            Log.v(TAG, "stop()");
        }
        setBluetoothPbapService(null);
        if (mSessionStatusHandler != null) {
            mSessionStatusHandler.obtainMessage(SHUTDOWN).sendToTarget();
        }
        mContactsLoaded = false;
        if (mContactChangeObserver == null) {
            Log.i(TAG, "Avoid unregister when receiver it is not registered");
            return true;
        }
        unregisterReceiver(mPbapReceiver);
        getContentResolver().unregisterContentObserver(mContactChangeObserver);
        mContactChangeObserver = null;
        setComponentAvailable(PBAP_ACTIVITY, false);
        return true;
    }

    /**
     * Get the current instance of {@link BluetoothPbapService}
     *
     * @return current instance of {@link BluetoothPbapService}
     */
    @VisibleForTesting
    public static synchronized BluetoothPbapService getBluetoothPbapService() {
        if (sBluetoothPbapService == null) {
            Log.w(TAG, "getBluetoothPbapService(): service is null");
            return null;
        }
        if (!sBluetoothPbapService.isAvailable()) {
            Log.w(TAG, "getBluetoothPbapService(): service is not available");
            return null;
        }
        return sBluetoothPbapService;
    }

    private static synchronized void setBluetoothPbapService(BluetoothPbapService instance) {
        if (DEBUG) {
            Log.d(TAG, "setBluetoothPbapService(): set to: " + instance);
        }
        sBluetoothPbapService = instance;
    }

    @Override
    protected void setCurrentUser(int userId) {
        Log.i(TAG, "setCurrentUser(" + userId + ")");
        UserManager userManager = (UserManager) getSystemService(Context.USER_SERVICE);
        if (userManager.isUserUnlocked(userId)) {
            setUserUnlocked(userId);
        }
    }

    @Override
    protected void setUserUnlocked(int userId) {
        Log.i(TAG, "setUserUnlocked(" + userId + ")");
        sendUpdateRequest();
    }

    private static class PbapBinder extends IBluetoothPbap.Stub implements IProfileServiceBinder {
        private BluetoothPbapService mService;

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
        private BluetoothPbapService getService(AttributionSource source) {
            if (!Utils.checkServiceAvailable(mService, TAG)
                    || !Utils.checkCallerIsSystemOrActiveOrManagedUser(mService, TAG)
                    || !Utils.checkConnectPermissionForDataDelivery(mService, source, TAG)) {
                return null;
            }
            return mService;
        }

        PbapBinder(BluetoothPbapService service) {
            if (VERBOSE) {
                Log.v(TAG, "PbapBinder()");
            }
            mService = service;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        @Override
        public List<BluetoothDevice> getConnectedDevices(AttributionSource source) {
            if (DEBUG) {
                Log.d(TAG, "getConnectedDevices");
            }
            BluetoothPbapService service = getService(source);
            if (service == null) {
                return new ArrayList<>(0);
            }
            return service.getConnectedDevices();
        }

        @Override
        public List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states,
                AttributionSource source) {
            if (DEBUG) {
                Log.d(TAG, "getDevicesMatchingConnectionStates");
            }
            BluetoothPbapService service = getService(source);
            if (service == null) {
                return new ArrayList<>(0);
            }
            return service.getDevicesMatchingConnectionStates(states);
        }

        @Override
        public int getConnectionState(BluetoothDevice device, AttributionSource source) {
            if (DEBUG) {
                Log.d(TAG, "getConnectionState: " + device);
            }
            BluetoothPbapService service = getService(source);
            if (service == null) {
                return BluetoothAdapter.STATE_DISCONNECTED;
            }
            return service.getConnectionState(device);
        }

        @Override
        public boolean setConnectionPolicy(BluetoothDevice device, int connectionPolicy,
                AttributionSource source) {
            if (DEBUG) {
                Log.d(TAG, "setConnectionPolicy for device: " + device + ", policy:"
                        + connectionPolicy);
            }
            BluetoothPbapService service = getService(source);
            if (service == null) {
                return false;
            }
            return service.setConnectionPolicy(device, connectionPolicy);
        }

        @Override
        public void disconnect(BluetoothDevice device, AttributionSource source) {
            if (DEBUG) {
                Log.d(TAG, "disconnect");
            }
            BluetoothPbapService service = getService(source);
            if (service == null) {
                return;
            }
            service.disconnect(device);
        }
    }

    @Override
    public boolean onConnect(BluetoothDevice remoteDevice, BluetoothSocket socket) {
        if (remoteDevice == null || socket == null) {
            Log.e(TAG, "onConnect(): Unexpected null. remoteDevice=" + remoteDevice
                    + " socket=" + socket);
            return false;
        }
        if (getConnectedDevices().size() >= BluetoothPbapFixes.MAX_CONNECTED_DEVICES) {
            Log.i(TAG, "Cannot connect to " + remoteDevice + " multiple devices connected already");
            return false;
        }
        PbapStateMachine sm = PbapStateMachine.make(this, mHandlerThread.getLooper(), remoteDevice,
                socket,  this, mSessionStatusHandler, mNextNotificationId);
        mNextNotificationId++;
        if (mNextNotificationId == PBAP_NOTIFICATION_ID_END) {
            mNextNotificationId = PBAP_NOTIFICATION_ID_START;
        }
        synchronized (mPbapStateMachineMap) {
            mPbapStateMachineMap.put(remoteDevice, sm);
        }
        sm.sendMessage(PbapStateMachine.REQUEST_PERMISSION);
        return true;
    }

    /**
     * Get the phonebook access permission for the device; if unknown, ask the user.
     * Send the result to the state machine.
     * @param stateMachine PbapStateMachine which sends the request
     */
    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_PRIVILEGED)
    public void checkOrGetPhonebookPermission(PbapStateMachine stateMachine) {
        BluetoothDevice device = stateMachine.getRemoteDevice();
        int permission = device.getPhonebookAccessPermission();
        if (DEBUG) {
            Log.d(TAG, "getPhonebookAccessPermission() = " + permission);
        }

        if (permission == BluetoothDevice.ACCESS_ALLOWED) {
            setConnectionPolicy(device, BluetoothProfile.CONNECTION_POLICY_ALLOWED);
            stateMachine.sendMessage(PbapStateMachine.AUTHORIZED);
        } else if (permission == BluetoothDevice.ACCESS_REJECTED) {
            stateMachine.sendMessage(PbapStateMachine.REJECTED);
        } else { // permission == BluetoothDevice.ACCESS_UNKNOWN
            Intent intent = new Intent(BluetoothDevice.ACTION_CONNECTION_ACCESS_REQUEST);
            intent.setClassName(BluetoothPbapService.ACCESS_AUTHORITY_PACKAGE,
                    BluetoothPbapService.ACCESS_AUTHORITY_CLASS);
            intent.putExtra(BluetoothDevice.EXTRA_ACCESS_REQUEST_TYPE,
                    BluetoothDevice.REQUEST_TYPE_PHONEBOOK_ACCESS);
            intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
            intent.putExtra(BluetoothDevice.EXTRA_PACKAGE_NAME, this.getPackageName());
            this.sendOrderedBroadcast(intent, BLUETOOTH_CONNECT,
                    Utils.getTempAllowlistBroadcastOptions(), null/* resultReceiver */,
                    null/* scheduler */, Activity.RESULT_OK/* initialCode */, null/* initialData */,
                    null/* initialExtras */);
            if (VERBOSE) {
                Log.v(TAG, "waiting for authorization for connection from: " + device);
            }
            /* In case car kit time out and try to use HFP for phonebook
             * access, while UI still there waiting for user to confirm */
            Message msg = mSessionStatusHandler.obtainMessage(BluetoothPbapService.USER_TIMEOUT,
                    stateMachine);
            mSessionStatusHandler.sendMessageDelayed(msg, USER_CONFIRM_TIMEOUT_VALUE);
            /* We will continue the process when we receive
             * BluetoothDevice.ACTION_CONNECTION_ACCESS_REPLY from Settings app. */
        }
    }

    /**
     * Called when an unrecoverable error occurred in an accept thread.
     * Close down the server socket, and restart.
     */
    @Override
    public synchronized void onAcceptFailed() {
        Log.w(TAG, "PBAP server socket accept thread failed. Restarting the server socket");

        if (mWakeLock != null) {
            mWakeLock.release();
            mWakeLock = null;
        }

        cleanUpServerSocket();

        if (mSessionStatusHandler != null) {
            mSessionStatusHandler.removeCallbacksAndMessages(null);
        }

        synchronized (mPbapStateMachineMap) {
            mPbapStateMachineMap.clear();
        }

        mSessionStatusHandler.sendMessage(mSessionStatusHandler.obtainMessage(START_LISTENER));
    }

    private void loadAllContacts() {
        if (mThreadLoadContacts == null) {
            Runnable r = new Runnable() {
                @Override
                public void run() {
                    try {
                        BluetoothPbapUtils.loadAllContacts(mContext,
                                mSessionStatusHandler);
                    } catch (Exception e) {
                        Log.e(TAG, "loadAllContacts failed: " + e);
                    } finally {
                        mThreadLoadContacts = null;
                    }
                }
            };
            mThreadLoadContacts = new Thread(r);
            mThreadLoadContacts.start();
        }
    }

    private void updateSecondaryVersion() {
        if (mThreadUpdateSecVersionCounter == null) {
            Runnable r = new Runnable() {
                @Override
                public void run() {
                    try {
                        BluetoothPbapUtils.updateSecondaryVersionCounter(mContext,
                                mSessionStatusHandler);
                    } catch (Exception e) {
                        Log.e(TAG, "updateSecondaryVersion counter failed: " + e);
                    } finally {
                        mThreadUpdateSecVersionCounter = null;
                    }
                }
            };
            mThreadUpdateSecVersionCounter = new Thread(r);
            mThreadUpdateSecVersionCounter.start();
        }
    }

    private void getLocalTelephonyDetails() {
        TelephonyManager tm = (TelephonyManager) getSystemService(Context.TELEPHONY_SERVICE);
        if (tm != null) {
            sLocalPhoneNum = tm.getLine1Number();
            sLocalPhoneName = this.getString(R.string.localPhoneName);
        }
        if (VERBOSE)
            Log.v(TAG, "Local Phone Details- Number:" + sLocalPhoneNum
                    + ", Name:" + sLocalPhoneName);
    }
}
