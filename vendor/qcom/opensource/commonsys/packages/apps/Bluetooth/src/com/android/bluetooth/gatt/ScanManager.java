/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.bluetooth.gatt;

import android.annotation.RequiresPermission;
import android.app.ActivityManager;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.display.DisplayManager;
import android.location.LocationManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;
import android.view.Display;

import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AdapterService;

import java.util.ArrayDeque;
import java.util.Collections;
import java.util.Deque;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Class that handles Bluetooth LE scan related operations.
 *
 * @hide
 */
public class ScanManager {
    private static final boolean DBG = GattServiceConfig.DBG;
    private static final String TAG = GattServiceConfig.TAG_PREFIX + "ScanManager";

    /**
     * Scan params corresponding to regular scan setting
     */
    private static final int SCAN_MODE_LOW_POWER_WINDOW_MS = 140;
    private static final int SCAN_MODE_LOW_POWER_INTERVAL_MS = 1400;
    private static final int SCAN_MODE_BALANCED_WINDOW_MS = 183;
    private static final int SCAN_MODE_BALANCED_INTERVAL_MS = 730;
    private static final int SCAN_MODE_LOW_LATENCY_WINDOW_MS = 100;
    private static final int SCAN_MODE_LOW_LATENCY_INTERVAL_MS = 100;
    public static final int SCAN_MODE_SCREEN_OFF_LOW_POWER_WINDOW_MS = 512;
    public static final int SCAN_MODE_SCREEN_OFF_LOW_POWER_INTERVAL_MS = 10240;
    public static final int SCAN_MODE_SCREEN_OFF_BALANCED_WINDOW_MS = 183;
    public static final int SCAN_MODE_SCREEN_OFF_BALANCED_INTERVAL_MS = 730;

    // Result type defined in bt stack. Need to be accessed by GattService.
    static final int SCAN_RESULT_TYPE_TRUNCATED = 1;
    static final int SCAN_RESULT_TYPE_FULL = 2;
    static final int SCAN_RESULT_TYPE_BOTH = 3;

    // Internal messages for handling BLE scan operations.
    private static final int MSG_START_BLE_SCAN = 0;
    private static final int MSG_STOP_BLE_SCAN = 1;
    private static final int MSG_FLUSH_BATCH_RESULTS = 2;
    private static final int MSG_SCAN_TIMEOUT = 3;
    private static final int MSG_SUSPEND_SCANS = 4;
    private static final int MSG_RESUME_SCANS = 5;
    private static final int MSG_IMPORTANCE_CHANGE = 6;
    private static final int MSG_SCREEN_ON = 7;
    private static final int MSG_SCREEN_OFF = 8;
    private static final int MSG_REVERT_SCAN_MODE_UPGRADE = 9;
    /*For suspending both unfiltered & filtered scans*/
    private static final int MSG_SUSPEND_SCAN_ALL = 10;
    private static final String ACTION_REFRESH_BATCHED_SCAN =
            "com.android.bluetooth.gatt.REFRESH_BATCHED_SCAN";


    // Timeout for each controller operation.
    private static final int OPERATION_TIME_OUT_MILLIS = 500;
    private static final int MAX_IS_UID_FOREGROUND_MAP_SIZE = 500;

    private int mLastConfiguredScanSettingLE1M = Integer.MIN_VALUE;
    private int mLastConfiguredScanSettingLECoded = Integer.MIN_VALUE;
    // Scan parameters for batch scan.
    private BatchScanParams mBatchScanParms;

    private Integer mCurUsedTrackableAdvertisements;
    private final GattService mService;
    private final AdapterService mAdapterService;
    private BroadcastReceiver mBatchAlarmReceiver;
    private boolean mBatchAlarmReceiverRegistered;
    private boolean mIsAptXLowLatencyModeEnabled;
    private ScanNative mScanNative;
    private volatile ClientHandler mHandler;

    private Set<ScanClient> mRegularScanClients;
    private Set<ScanClient> mBatchClients;
    private Set<ScanClient> mSuspendedScanClients;
    private Set<ScanClient> mPendingScanClients;
    private SparseIntArray mPriorityMap = new SparseIntArray();

    private CountDownLatch mLatch;

    private DisplayManager mDm;

    private ActivityManager mActivityManager;
    private LocationManager mLocationManager;
    private static final int FOREGROUND_IMPORTANCE_CUTOFF =
            ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND_SERVICE;
    private static final boolean DEFAULT_UID_IS_FOREGROUND = true;
    private static final int SCAN_MODE_APP_IN_BACKGROUND = ScanSettings.SCAN_MODE_LOW_POWER;
    private final SparseBooleanArray mIsUidForegroundMap = new SparseBooleanArray();
    private boolean mScreenOn = false;

    private class UidImportance {
        public int uid;
        public int importance;

        UidImportance(int uid, int importance) {
            this.uid = uid;
            this.importance = importance;
        }
    }

    private class PhyInfo {
        public int scanPhy;
        public int scanModeLE1M;
        public int scanModeLECoded;

        PhyInfo(int scanPhy, int scanModeLE1M, int scanModeLECoded) {
            this.scanPhy = scanPhy;
            this.scanModeLE1M = scanModeLE1M;
            this.scanModeLECoded = scanModeLECoded;
        }
    };

    ScanManager(GattService service, AdapterService adapterService) {
        mRegularScanClients =
                Collections.newSetFromMap(new ConcurrentHashMap<ScanClient, Boolean>());
        mBatchClients = Collections.newSetFromMap(new ConcurrentHashMap<ScanClient, Boolean>());
        mPendingScanClients =
                Collections.newSetFromMap(new ConcurrentHashMap<ScanClient, Boolean>());
        mSuspendedScanClients =
                Collections.newSetFromMap(new ConcurrentHashMap<ScanClient, Boolean>());
        mService = service;
        mScanNative = new ScanNative();
        mCurUsedTrackableAdvertisements = 0;
        mDm = (DisplayManager) mService.getSystemService(Context.DISPLAY_SERVICE);
        mActivityManager = (ActivityManager) mService.getSystemService(Context.ACTIVITY_SERVICE);
        mLocationManager = (LocationManager) mService.getSystemService(Context.LOCATION_SERVICE);
        mAdapterService = adapterService;

        mPriorityMap.put(ScanSettings.SCAN_MODE_OPPORTUNISTIC, 0);
        mPriorityMap.put(ScanSettings.SCAN_MODE_SCREEN_OFF, 1);
        mPriorityMap.put(ScanSettings.SCAN_MODE_LOW_POWER, 2);
        mPriorityMap.put(ScanSettings.SCAN_MODE_SCREEN_OFF_BALANCED, 3);
        // BALANCED and AMBIENT_DISCOVERY now have the same settings and priority.
        mPriorityMap.put(ScanSettings.SCAN_MODE_BALANCED, 4);
        mPriorityMap.put(ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY, 4);
        mPriorityMap.put(ScanSettings.SCAN_MODE_LOW_LATENCY, 5);
    }

    void start() {
        HandlerThread thread = new HandlerThread("BluetoothScanManager");
        thread.start();
        mHandler = new ClientHandler(thread.getLooper());
        if (mDm != null) {
            mDm.registerDisplayListener(mDisplayListener, null);
        }
        if (mActivityManager != null) {
            mActivityManager.addOnUidImportanceListener(mUidImportanceListener,
                    FOREGROUND_IMPORTANCE_CUTOFF);
        }
        IntentFilter locationIntentFilter = new IntentFilter(LocationManager.MODE_CHANGED_ACTION);
        mService.registerReceiver(mLocationReceiver, locationIntentFilter);
    }

    void cleanup() {
        mRegularScanClients.clear();
        mBatchClients.clear();
        mSuspendedScanClients.clear();
        mPendingScanClients.clear();
        mScanNative.cleanup();

        if (mActivityManager != null) {
            try {
                mActivityManager.removeOnUidImportanceListener(mUidImportanceListener);
            } catch (IllegalArgumentException e) {
                Log.w(TAG, "exception when invoking removeOnUidImportanceListener", e);
            }
        }

        if (mDm != null) {
            mDm.unregisterDisplayListener(mDisplayListener);
        }

        if (mHandler != null) {
            // Shut down the thread
            mHandler.removeCallbacksAndMessages(null);
            Looper looper = mHandler.getLooper();
            if (looper != null) {
                looper.quitSafely();
            }
            mHandler = null;
        }

        try {
            mService.unregisterReceiver(mLocationReceiver);
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "exception when invoking unregisterReceiver(mLocationReceiver)", e);
        }
    }

    void registerScanner(UUID uuid) {
        mScanNative.registerScannerNative(uuid.getLeastSignificantBits(),
                uuid.getMostSignificantBits());
    }

    void unregisterScanner(int scannerId) {
        mScanNative.unregisterScannerNative(scannerId);
    }

    /**
     * Returns the regular scan queue.
     */
    Set<ScanClient> getRegularScanQueue() {
        return mRegularScanClients;
    }

    /**
     * Returns batch scan queue.
     */
    Set<ScanClient> getBatchScanQueue() {
        return mBatchClients;
    }

    /**
     * Returns the pending scan queue.
     */
    Set<ScanClient> getPendingScanQueue() {
        return mPendingScanClients;
    }

    /**
     * Adding the pending scan to the queue.
     */
    void addPendingScanToQueue(ScanClient client) {
        mPendingScanClients.add(client);
    }
    /**
     * Returns a set of full batch scan clients.
     */
    Set<ScanClient> getFullBatchScanQueue() {
        // TODO: split full batch scan clients and truncated batch clients so we don't need to
        // construct this every time.
        Set<ScanClient> fullBatchClients = new HashSet<ScanClient>();
        for (ScanClient client : mBatchClients) {
            if (client.settings.getScanResultType() == ScanSettings.SCAN_RESULT_TYPE_FULL) {
                fullBatchClients.add(client);
            }
        }
        return fullBatchClients;
    }

    void startScan(ScanClient client) {
        if (DBG) {
            Log.d(TAG, "startScan() " + client);
        }
        sendMessage(MSG_START_BLE_SCAN, client);
    }

    void stopScan(int scannerId) {
        ScanClient client = mScanNative.getBatchScanClient(scannerId);
        if (client == null) {
            client = mScanNative.getRegularScanClient(scannerId);
        }
        sendMessage(MSG_STOP_BLE_SCAN, client);
    }

    void cancelScans(ScanClient client) {
      mHandler.handleStopScan(client);
    }

    void flushBatchScanResults(ScanClient client) {
        sendMessage(MSG_FLUSH_BATCH_RESULTS, client);
    }

    void callbackDone(int scannerId, int status) {
        if (DBG) {
            Log.d(TAG, "callback done for scannerId - " + scannerId + " status - " + status);
        }
        if (status == 0) {
            mLatch.countDown();
        }
        // TODO: add a callback for scan failure.
    }

    private void sendMessage(int what, ScanClient client) {
        final ClientHandler handler = mHandler;
        if (handler == null) {
            Log.d(TAG, "sendMessage: mHandler is null.");
            return;
        }
        Message message = new Message();
        message.what = what;
        message.obj = client;
        handler.sendMessage(message);
    }

    private boolean isFilteringSupported() {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        return adapter.isOffloadedFilteringSupported();
    }

    // Handler class that handles BLE scan operations.
    private class ClientHandler extends Handler {

        ClientHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            Log.i(TAG, "msg.what = " + msg.what);
            switch (msg.what) {
                case MSG_START_BLE_SCAN:
                    handleStartScan((ScanClient) msg.obj);
                    break;
                case MSG_STOP_BLE_SCAN:
                    handleStopScan((ScanClient) msg.obj);
                    break;
                case MSG_FLUSH_BATCH_RESULTS:
                    handleFlushBatchResults((ScanClient) msg.obj);
                    break;
                case MSG_SCAN_TIMEOUT:
                    mScanNative.regularScanTimeout((ScanClient) msg.obj);
                    break;
                case MSG_SUSPEND_SCANS:
                    handleSuspendScans();
                    break;
                case MSG_RESUME_SCANS:
                    handleResumeScans();
                    break;
                case MSG_SCREEN_OFF:
                    handleScreenOff();
                    break;
                case MSG_SCREEN_ON:
                    handleScreenOn();
                    break;
                case MSG_REVERT_SCAN_MODE_UPGRADE:
                    revertScanModeUpgrade((ScanClient) msg.obj);
                    break;
                case MSG_IMPORTANCE_CHANGE:
                    handleImportanceChange((UidImportance) msg.obj);
                    break;
                case MSG_SUSPEND_SCAN_ALL:
                    handleSuspendScanAll();
                    break;
                default:
                    // Shouldn't happen.
                    Log.e(TAG, "received an unkown message : " + msg.what);
            }
        }

        void handleStartScan(ScanClient client) {
            if (DBG) {
                Log.d(TAG, "handling starting scan");
            }

            if (!isScanSupported(client)) {
                Log.e(TAG, "Scan settings not supported");
                return;
            }

            if (mRegularScanClients.contains(client) || mBatchClients.contains(client)) {
                Log.e(TAG, "Scan already started");
                return;
            }

            if (requiresScreenOn(client) && !isScreenOn()) {
                Log.w(TAG, "Cannot start unfiltered scan in screen-off. This scan will be resumed "
                        + "later: " + client.scannerId);
                mSuspendedScanClients.add(client);
                if (client.stats != null) {
                    client.stats.recordScanSuspend(client.scannerId);
                }
                return;
            }

            final boolean locationEnabled = mLocationManager.isLocationEnabled();
            if (requiresLocationOn(client) && !locationEnabled) {
                Log.i(TAG, "Cannot start unfiltered scan in location-off. This scan will be"
                        + " resumed when location is on: " + client.scannerId);
                mSuspendedScanClients.add(client);
                if (client.stats != null) {
                    client.stats.recordScanSuspend(client.scannerId);
                }
                return;
            }

            if (isAptXLowLatencyModeEnabled()) {
                Log.i(TAG, "Cannot start Scan when aptX LL mode is enabled. This scan will be"
                        + " resumed when aptX LL mode is disabled: " + client.scannerId);
                mSuspendedScanClients.add(client);
                if (client.stats != null) {
                    client.stats.recordScanSuspend(client.scannerId);
                }
                return;
            }

            if (mPendingScanClients.contains(client))
                mPendingScanClients.remove(client);
            // Begin scan operations.
            if (isBatchClient(client)) {
                mBatchClients.add(client);
                mScanNative.startBatchScan(client);
            } else {
                updateScanModeBeforeStart(client);
                mRegularScanClients.add(client);
                boolean ret = mScanNative.startRegularScan(client);
                if (!ret) {
                    mRegularScanClients.remove(client);
                    return;
                }

                if (!mScanNative.isOpportunisticScanClient(client)) {
                    mScanNative.configureRegularScanParams();

                    if (!mScanNative.isExemptFromScanDowngrade(client)) {
                        Message msg = obtainMessage(MSG_SCAN_TIMEOUT);
                        msg.obj = client;
                        // Only one timeout message should exist at any time
                        sendMessageDelayed(msg, mAdapterService.getScanTimeoutMillis());
                    }
                }
            }
            client.started = true;
        }

        private boolean requiresScreenOn(ScanClient client) {
            boolean isFiltered = (client.filters != null) && !client.filters.isEmpty();
            return !mScanNative.isOpportunisticScanClient(client) && !isFiltered;
        }

        private boolean requiresLocationOn(ScanClient client) {
            boolean isFiltered = (client.filters != null) && !client.filters.isEmpty();
            return !client.hasDisavowedLocation && !isFiltered;
        }

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
        void handleStopScan(ScanClient client) {
            boolean appDied;
            int scannerId;
            if (client == null) {
                return;
            }
            if (DBG) {
                Log.d(TAG, "handling stopping scan " + client);
            }

            if (mSuspendedScanClients.contains(client)) {
                mSuspendedScanClients.remove(client);
            }

            // The caller may pass a dummy client with only clientIf
            // and appDied status. Perform the operation on the
            // actual client in that case.
            appDied = client.appDied;
            scannerId = client.scannerId;
            client = mScanNative.getRegularScanClient(scannerId);
            if (client == null) {
                if (DBG) Log.d(TAG, "regular client is null");
                client = mScanNative.getBatchScanClient(scannerId);

                if(client == null) {
                    if (DBG) Log.d(TAG,"batch client is null");
                    return;
                }
            }
            removeMessages(MSG_REVERT_SCAN_MODE_UPGRADE, client);
            if (mRegularScanClients.contains(client)) {
                mScanNative.stopRegularScan(client);

                if (mScanNative.numRegularScanClients() == 0) {
                    removeMessages(MSG_SCAN_TIMEOUT);
                }

                if (!mScanNative.isOpportunisticScanClient(client)) {
                    mScanNative.configureRegularScanParams();
                }
            } else if (mBatchClients.contains(client)) {
                mScanNative.stopBatchScan(client);
            }
            if (appDied) {
                if (DBG) {
                    Log.d(TAG, "app died, unregister scanner - " + client.scannerId);
                }
                mService.unregisterScanner(client.scannerId, mService.getAttributionSource());
            }
        }

        void handleFlushBatchResults(ScanClient client) {
            if (!mBatchClients.contains(client)) {
                return;
            }
            mScanNative.flushBatchResults(client.scannerId);
        }

        private boolean isBatchClient(ScanClient client) {
            if (client == null || client.settings == null) {
                return false;
            }
            ScanSettings settings = client.settings;
            return settings.getCallbackType() == ScanSettings.CALLBACK_TYPE_ALL_MATCHES
                    && settings.getReportDelayMillis() != 0;
        }

        private boolean isScanSupported(ScanClient client) {
            if (client == null || client.settings == null) {
                return true;
            }
            ScanSettings settings = client.settings;
            if (isFilteringSupported()) {
                return true;
            }
            return settings.getCallbackType() == ScanSettings.CALLBACK_TYPE_ALL_MATCHES
                    && settings.getReportDelayMillis() == 0;
        }

        void handleScreenOff() {
            if (!mScreenOn) {
                return;
            }
            mScreenOn = false;
            if (DBG) {
                Log.d(TAG, "handleScreenOff()");
            }
            handleSuspendScans();
            updateRegularScanClientsScreenOff();
        }

        @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
        void handleSuspendScans() {
            for (ScanClient client : mRegularScanClients) {
                if ((requiresScreenOn(client) && !isScreenOn())
                        || (requiresLocationOn(client) && !mLocationManager.isLocationEnabled())) {
                    /*Suspend unfiltered scans*/
                    if (client.stats != null) {
                        client.stats.recordScanSuspend(client.scannerId);
                    }
                    if (DBG) {
                        Log.d(TAG, "suspend scan " + client);
                    }
                    handleStopScan(client);
                    mSuspendedScanClients.add(client);
                }
            }
        }
        void handleSuspendScanAll() {
            for (ScanClient client : mRegularScanClients) {
                if (!mScanNative.isOpportunisticScanClient(client)) {
                    /*Suspend both unfiltered & filtered scans*/
                    if (client.stats != null) {
                        client.stats.recordScanSuspend(client.scannerId);
                    }
                    handleStopScan(client);
                    mSuspendedScanClients.add(client);
                }
            }
        }

        private void updateRegularScanClientsScreenOff() {
            boolean updatedScanParams = false;
            for (ScanClient client : mRegularScanClients) {
                if (updateScanModeScreenOff(client)) {
                    updatedScanParams = true;
                    if (DBG) {
                        Log.d(TAG, "Scan mode update during screen off" + client);
                    }
                }
            }
            if (updatedScanParams) {
                mScanNative.configureRegularScanParams();
            }
        }

        private boolean updateScanModeScreenOff(ScanClient client) {
            if (!isAppForeground(client) && !mScanNative.isOpportunisticScanClient(client)) {
                return client.updateScanMode(ScanSettings.SCAN_MODE_SCREEN_OFF);
            }

            // The following codes are effectively only for services
            // Apps are either already or will be soon handled by handleImportanceChange().
            switch (client.scanModeApp) {
                case ScanSettings.SCAN_MODE_LOW_POWER:
                    return client.updateScanMode(ScanSettings.SCAN_MODE_SCREEN_OFF);
                case ScanSettings.SCAN_MODE_BALANCED:
                case ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY:
                    return client.updateScanMode(ScanSettings.SCAN_MODE_SCREEN_OFF_BALANCED);
                case ScanSettings.SCAN_MODE_OPPORTUNISTIC:
                case ScanSettings.SCAN_MODE_LOW_LATENCY:
                default:
                    return false;
            }
        }

        /**
         * Services and Apps are assumed to be in the foreground by default unless it changes to the
         * background triggering onUidImportance().
         */
        private boolean isAppForeground(ScanClient client) {
            return mIsUidForegroundMap.get(client.appUid, DEFAULT_UID_IS_FOREGROUND);
        }

        private boolean updateScanModeBeforeStart(ScanClient client) {
            if (upgradeScanModeBeforeStart(client)) {
                return true;
            }
            if (isScreenOn()) {
                return updateScanModeScreenOn(client);
            } else {
                return updateScanModeScreenOff(client);
            }
        }

        private boolean upgradeScanModeBeforeStart(ScanClient client) {
            if (client.started || mAdapterService.getScanUpgradeDurationMillis() == 0) {
                return false;
            }
            if (client.stats == null || client.stats.hasRecentScan()) {
                return false;
            }
            if (!isAppForeground(client) || isBatchClient(client)) {
                return false;
            }

            if (upgradeScanModeByOneLevel(client)) {
                Message msg = obtainMessage(MSG_REVERT_SCAN_MODE_UPGRADE);
                msg.obj = client;
                if (DBG) {
                    Log.d(TAG, "scanMode is upgraded for " + client);
                }
                sendMessageDelayed(msg, mAdapterService.getScanUpgradeDurationMillis());
                return true;
            }
            return false;
        }

        private boolean upgradeScanModeByOneLevel(ScanClient client) {
            switch (client.scanModeApp) {
                case ScanSettings.SCAN_MODE_LOW_POWER:
                    return client.updateScanMode(ScanSettings.SCAN_MODE_BALANCED);
                case ScanSettings.SCAN_MODE_BALANCED:
                case ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY:
                    return client.updateScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY);
                case ScanSettings.SCAN_MODE_OPPORTUNISTIC:
                case ScanSettings.SCAN_MODE_LOW_LATENCY:
                default:
                    return false;
            }
        }

        void revertScanModeUpgrade(ScanClient client) {
            if (mPriorityMap.get(client.settings.getScanMode())
                    <= mPriorityMap.get(client.scanModeApp)) {
                return;
            }
            if (client.updateScanMode(client.scanModeApp)) {
                if (DBG) {
                    Log.d(TAG, "scanMode upgrade is reverted for " + client);
                }
                mScanNative.configureRegularScanParams();
            }
        }

        private boolean updateScanModeScreenOn(ScanClient client) {
            int newScanMode =  (isAppForeground(client)
                    || mScanNative.isOpportunisticScanClient(client))
                    ? client.scanModeApp : SCAN_MODE_APP_IN_BACKGROUND;
            return client.updateScanMode(newScanMode);
        }

        void handleScreenOn() {
            if (mScreenOn) {
                return;
            }
            mScreenOn = true;
            if (DBG) {
                Log.d(TAG, "handleScreenOn()");
            }
            handleResumeScans();
            updateRegularScanClientsScreenOn();
        }

        void handleResumeScans() {
            for (ScanClient client : mSuspendedScanClients) {
                if ((!requiresScreenOn(client) || isScreenOn())
                        && (!requiresLocationOn(client) || mLocationManager.isLocationEnabled())) {
                    if (client.stats != null) {
                        client.stats.recordScanResume(client.scannerId);
                    }
                    if (DBG) {
                        Log.d(TAG, "resume scan " + client);
                    }
                    handleStartScan(client);
                }
            }
            mSuspendedScanClients.clear();
        }

        private void updateRegularScanClientsScreenOn() {
            boolean updatedScanParams = false;
            for (ScanClient client : mRegularScanClients) {
                if (updateScanModeScreenOn(client)) {
                    updatedScanParams = true;
                }
            }
            if (updatedScanParams) {
                mScanNative.configureRegularScanParams();
            }
        }
    }

    /**
     * Parameters for batch scans.
     */
    class BatchScanParams {
        public int scanMode;
        public int fullScanscannerId;
        public int truncatedScanscannerId;

        BatchScanParams() {
            scanMode = -1;
            fullScanscannerId = -1;
            truncatedScanscannerId = -1;
        }

        @Override
        public boolean equals(Object obj) {
            if (this == obj) {
                return true;
            }
            if (obj == null || getClass() != obj.getClass()) {
                return false;
            }
            BatchScanParams other = (BatchScanParams) obj;
            return scanMode == other.scanMode && fullScanscannerId == other.fullScanscannerId
                    && truncatedScanscannerId == other.truncatedScanscannerId;

        }
    }

    public int getCurrentUsedTrackingAdvertisement() {
        Log.d(TAG, "mCurUsedTrackableAdvertisements: "
                + mCurUsedTrackableAdvertisements);
        return mCurUsedTrackableAdvertisements;
    }

    private class ScanNative {

        // Delivery mode defined in bt stack.
        private static final int DELIVERY_MODE_IMMEDIATE = 0;
        private static final int DELIVERY_MODE_ON_FOUND_LOST = 1;
        private static final int DELIVERY_MODE_BATCH = 2;
        private static final int DELIVERY_MODE_ROUTE = 8;

        private static final int ONFOUND_SIGHTINGS_AGGRESSIVE = 1;
        private static final int ONFOUND_SIGHTINGS_STICKY = 4;

        private static final int ALL_PASS_FILTER_INDEX_REGULAR_SCAN = 1;
        private static final int ALL_PASS_FILTER_INDEX_BATCH_SCAN = 2;
        private static final int ALL_PASS_FILTER_SELECTION = 0;

        private static final int DISCARD_OLDEST_WHEN_BUFFER_FULL = 0;


        /**
         * Onfound/onlost for scan settings
         */
        private static final int MATCH_MODE_AGGRESSIVE_TIMEOUT_FACTOR = (1);
        private static final int MATCH_MODE_STICKY_TIMEOUT_FACTOR = (3);
        private static final int ONLOST_FACTOR = 2;
        private static final int ONLOST_ONFOUND_BASE_TIMEOUT_MS = 500;

        // The logic is AND for each filter field.
        private static final int LIST_LOGIC_TYPE = 0x1111111;
        private static final int FILTER_LOGIC_TYPE = 1;
        // Filter indices that are available to user. It's sad we need to maintain filter index.
        private final Deque<Integer> mFilterIndexStack;
        // Map of scannerId and Filter indices used by client.
        private final Map<Integer, Deque<Integer>> mClientFilterIndexMap;
        // Keep track of the clients that uses ALL_PASS filters.
        private final Set<Integer> mAllPassRegularClients = new HashSet<>();
        private final Set<Integer> mAllPassBatchClients = new HashSet<>();

        private AlarmManager mAlarmManager;
        private PendingIntent mBatchScanIntervalIntent;

        ScanNative() {
            mFilterIndexStack = new ArrayDeque<Integer>();
            mClientFilterIndexMap = new HashMap<Integer, Deque<Integer>>();

            mAlarmManager = (AlarmManager) mService.getSystemService(Context.ALARM_SERVICE);
            Intent batchIntent = new Intent(ACTION_REFRESH_BATCHED_SCAN, null);
            batchIntent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
            mBatchScanIntervalIntent = PendingIntent.getBroadcast(mService, 0, batchIntent,
                    PendingIntent.FLAG_IMMUTABLE);
            IntentFilter filter = new IntentFilter();
            filter.addAction(ACTION_REFRESH_BATCHED_SCAN);
            mBatchAlarmReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    Log.d(TAG, "awakened up at time " + SystemClock.elapsedRealtime());
                    String action = intent.getAction();

                    if (action.equals(ACTION_REFRESH_BATCHED_SCAN)) {
                        if (mBatchClients.isEmpty()) {
                            return;
                        }
                        try {
                            // Note this actually flushes all pending batch data.
                            if (mBatchClients.iterator().hasNext()) {
                                flushBatchScanResults(mBatchClients.iterator().next());
                            }
                        } catch (NoSuchElementException e) {
                            Log.e(TAG, "Element not found: " + e);
                        }
                    }
                }
            };
            mService.registerReceiver(mBatchAlarmReceiver, filter);
            mBatchAlarmReceiverRegistered = true;
        }

        private void resetCountDownLatch() {
            mLatch = new CountDownLatch(1);
        }

        // Returns true if mLatch reaches 0, false if timeout or interrupted.
        private boolean waitForCallback() {
            try {
                return mLatch.await(OPERATION_TIME_OUT_MILLIS, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }

        void configureRegularScanParams() {
            if (DBG) {
                Log.d(TAG, "configureRegularScanParams() - queue=" + mRegularScanClients.size());
            }
            int curScanSettingLE1M = Integer.MIN_VALUE;
            int curScanSettingLECoded = Integer.MIN_VALUE;
            int scanPhy = BluetoothDevice.PHY_LE_1M;
            ScanClient client = getAggressiveClient(mRegularScanClients);
            PhyInfo phyInfoResult = getPhyInfo(mRegularScanClients);
            boolean scanModeChanged = false;
            int scanWindowLE1M = Integer.MIN_VALUE;
            int scanIntervalLE1M = Integer.MIN_VALUE;
            int scanWindowLECoded = Integer.MIN_VALUE;
            int scanIntervalLECoded = Integer.MIN_VALUE;
            int[] scanInterval = new int[2];
            int[] scanWindow  = new int[2];
            int phyCnt = 0;


            if (phyInfoResult != null) {
                curScanSettingLE1M = phyInfoResult.scanModeLE1M;
                curScanSettingLECoded = phyInfoResult.scanModeLECoded;
                scanPhy = phyInfoResult.scanPhy;
            }

            if (DBG) {
                Log.d(TAG, "configureRegularScanParams() - ScanSetting LE 1M Scan mode=" + curScanSettingLE1M
                        + "ScanSetting LE Coded Scan mode=" + curScanSettingLECoded
                        + " mLastConfiguredScanSettingLE1M=" + mLastConfiguredScanSettingLE1M
                        + " mLastConfiguredScanSettingLECoded=" + mLastConfiguredScanSettingLECoded
                        + "scanPhy=" + scanPhy);
            }

            /* Check whether scan mode for LE 1M PHY has changed and compute the appropriate
             * scan interval and scan window values
             */
            if ((curScanSettingLE1M != Integer.MIN_VALUE
                    && curScanSettingLE1M != ScanSettings.SCAN_MODE_OPPORTUNISTIC)) {
                if ((curScanSettingLE1M != mLastConfiguredScanSettingLE1M) ||
                       (curScanSettingLECoded != mLastConfiguredScanSettingLECoded)) {
                    scanModeChanged = true;
                    ScanSettings settings = new ScanSettings.Builder().setScanMode(curScanSettingLE1M).build();
                    scanWindowLE1M = getScanWindowMillis(settings);
                    scanIntervalLE1M = getScanIntervalMillis(settings);
                    // convert scanWindow and scanInterval from ms to LE scan units(0.625ms)
                    scanWindow[phyCnt] = Utils.millsToUnit(scanWindowLE1M);
                    scanInterval[phyCnt] = Utils.millsToUnit(scanIntervalLE1M);
                    phyCnt++;
                }
            }
            else {
                mLastConfiguredScanSettingLE1M = curScanSettingLE1M;
                if (DBG) {
                    Log.d(TAG, "configureRegularScanParams() - queue empty, scan stopped");
                }
            }

            /* Check whether scan mode for LE Coded PHY has changed and compute the appropriate
             * scan interval and scan window values
             */
            if((curScanSettingLECoded != Integer.MIN_VALUE
                    && curScanSettingLECoded != ScanSettings.SCAN_MODE_OPPORTUNISTIC)) {
                if ((curScanSettingLECoded != mLastConfiguredScanSettingLECoded) ||
                       (curScanSettingLE1M != mLastConfiguredScanSettingLE1M)) {
                    scanModeChanged = true;
                    ScanSettings settings = new ScanSettings.Builder().setScanMode(curScanSettingLECoded).build();
                    scanWindowLECoded = getScanWindowMillis(settings);
                    scanIntervalLECoded = getScanIntervalMillis(settings);
                    // convert scanWindow and scanInterval from ms to LE scan units(0.625ms)
                    scanWindow[phyCnt] = Utils.millsToUnit(scanWindowLECoded);
                    scanInterval[phyCnt] = Utils.millsToUnit(scanIntervalLECoded);
                }
            }
            else {
                mLastConfiguredScanSettingLECoded = curScanSettingLECoded;
                if (DBG) {
                    Log.d(TAG, "configureRegularScanParams() - queue emtpy, scan stopped");
                }
            }
            if(scanModeChanged) {
                gattClientScanNative(false);
                if (DBG) {
                    Log.d(TAG, "configureRegularScanParams - scanInterval LE 1M = " + scanIntervalLE1M
                            + "configureRegularScanParams - scanWindow LE 1M= " + scanWindowLE1M
                            + "configureRegularScanParams - scanInterval LE Coded= " + scanIntervalLECoded
                            + "configureRegularScanParams - scanWindow LE Coded= " + scanWindowLECoded);
                }
                gattSetScanParametersNative(client.scannerId, scanPhy, scanInterval, scanWindow);
                gattClientScanNative(true);
                mLastConfiguredScanSettingLE1M = curScanSettingLE1M;
                mLastConfiguredScanSettingLECoded = curScanSettingLECoded;
            }
        }

        ScanClient getAggressiveClient(Set<ScanClient> cList) {
            ScanClient result = null;
            int currentScanModePriority = Integer.MIN_VALUE;
            for (ScanClient client : cList) {
                int priority = mPriorityMap.get(client.settings.getScanMode());
                if (priority > currentScanModePriority) {
                    result = client;
                    currentScanModePriority = priority;
                }
            }
            return result;
        }

        PhyInfo getPhyInfo(Set<ScanClient> cList) {
            PhyInfo result = null;
            int curScanSettingLE1M = Integer.MIN_VALUE;
            int curScanSettingLECoded = Integer.MIN_VALUE;
            int curScanPhy = BluetoothDevice.PHY_LE_1M;
            int aggregateScanPhy = BluetoothDevice.PHY_LE_1M;
            for (ScanClient client : cList) {
                // Get the most aggresive scan mode for each PHY
                curScanPhy = client.settings.getPhy();
                if (((curScanPhy & BluetoothDevice.PHY_LE_1M)== BluetoothDevice.PHY_LE_1M) &&
                        (client.settings.getScanMode() > curScanSettingLE1M)) {
                    curScanSettingLE1M = client.settings.getScanMode();
                }
                if (((curScanPhy & BluetoothDevice.PHY_LE_CODED)== BluetoothDevice.PHY_LE_CODED) &&
                        (client.settings.getScanMode() > curScanSettingLECoded)) {
                    curScanSettingLECoded = client.settings.getScanMode();
                }
                aggregateScanPhy |= client.settings.getPhy();
            }
            result = new PhyInfo(aggregateScanPhy, curScanSettingLE1M, curScanSettingLECoded);
            return result;
        }

        boolean startRegularScan(ScanClient client) {
            if (isFilteringSupported() && mFilterIndexStack.isEmpty()
                    && mClientFilterIndexMap.isEmpty()) {
                initFilterIndexStack();
            }

            if (isRoutingScanClient(client) && (client.filters.size() > mFilterIndexStack.size())) {
                try {
                    mService.onScanManagerErrorCallback(client.scannerId,
                                ScanCallback.SCAN_FAILED_OUT_OF_HARDWARE_RESOURCES);
                    Log.e(TAG, "startRegularScan, routing scan out of HARDWARE_RESOURCES");
                } catch (RemoteException e) {
                    Log.e(TAG, "startRegularScan, routing scan failed on onScanManagerCallback", e);
                }
                return false;
            }

            if (isFilteringSupported()) {
                configureScanFilters(client);
            }
            // Start scan native only for the first client.
            if (numRegularScanClients() == 1
                    && client.settings != null
                    && client.settings.getScanMode() != ScanSettings.SCAN_MODE_OPPORTUNISTIC) {
               if (DBG) {
                    Log.d(TAG, "start gattClientScanNative from startRegularScan()");
                }
                gattClientScanNative(true);
            }
            return true;
        }

        private int numRegularScanClients() {
            int num = 0;
            for (ScanClient client : mRegularScanClients) {
                if (client.settings.getScanMode() != ScanSettings.SCAN_MODE_OPPORTUNISTIC) {
                    num++;
                }
            }
            return num;
        }

        void startBatchScan(ScanClient client) {
            if (mFilterIndexStack.isEmpty() && isFilteringSupported()) {
                initFilterIndexStack();
            }
            configureScanFilters(client);
            if (!isOpportunisticScanClient(client)) {
                // Reset batch scan. May need to stop the existing batch scan and update scan
                // params.
                resetBatchScan(client);
            }
        }

        private boolean isExemptFromScanDowngrade(ScanClient client) {
            return isOpportunisticScanClient(client) || isFirstMatchScanClient(client)
                    || !shouldUseAllPassFilter(client) || isRoutingScanClient(client);
        }

        private boolean isOpportunisticScanClient(ScanClient client) {
            return client.settings.getScanMode() == ScanSettings.SCAN_MODE_OPPORTUNISTIC;
        }

        private boolean isFirstMatchScanClient(ScanClient client) {
            return (client.settings.getCallbackType() & ScanSettings.CALLBACK_TYPE_FIRST_MATCH)
                    != 0;
        }

        private boolean isRoutingScanClient(ScanClient client) {
            return client.settings.getCallbackType() == ScanSettings.CALLBACK_TYPE_SENSOR_ROUTING;
        }

        private void resetBatchScan(ScanClient client) {
            int scannerId = client.scannerId;
            BatchScanParams batchScanParams = getBatchScanParams();
            // Stop batch if batch scan params changed and previous params is not null.
            if (mBatchScanParms != null && (!mBatchScanParms.equals(batchScanParams))) {
                if (DBG) {
                    Log.d(TAG, "stopping BLe Batch");
                }
                resetCountDownLatch();
                gattClientStopBatchScanNative(scannerId);
                waitForCallback();
                // Clear pending results as it's illegal to config storage if there are still
                // pending results.
                flushBatchResults(scannerId);
            }
            // Start batch if batchScanParams changed and current params is not null.
            if (batchScanParams != null && (!batchScanParams.equals(mBatchScanParms))) {
                int notifyThreshold = 95;
                if (DBG) {
                    Log.d(TAG, "Starting BLE batch scan");
                }
                int resultType = getResultType(batchScanParams);
                int fullScanPercent = getFullScanStoragePercent(resultType);
                resetCountDownLatch();
                if (DBG) {
                    Log.d(TAG, "configuring batch scan storage, appIf " + client.scannerId);
                }
                gattClientConfigBatchScanStorageNative(client.scannerId, fullScanPercent,
                        100 - fullScanPercent, notifyThreshold);
                waitForCallback();
                resetCountDownLatch();
                int scanInterval =
                        Utils.millsToUnit(getBatchScanIntervalMillis(batchScanParams.scanMode));
                int scanWindow =
                        Utils.millsToUnit(getBatchScanWindowMillis(batchScanParams.scanMode));
                gattClientStartBatchScanNative(scannerId, resultType, scanInterval, scanWindow, 0,
                        DISCARD_OLDEST_WHEN_BUFFER_FULL);
                waitForCallback();
            }
            mBatchScanParms = batchScanParams;
            setBatchAlarm();
        }

        private int getFullScanStoragePercent(int resultType) {
            switch (resultType) {
                case SCAN_RESULT_TYPE_FULL:
                    return 100;
                case SCAN_RESULT_TYPE_TRUNCATED:
                    return 0;
                case SCAN_RESULT_TYPE_BOTH:
                    return 50;
                default:
                    return 50;
            }
        }

        private BatchScanParams getBatchScanParams() {
            if (mBatchClients.isEmpty()) {
                return null;
            }
            BatchScanParams params = new BatchScanParams();
            ScanClient winner = getAggressiveClient(mBatchClients);
            if (winner != null) {
                params.scanMode = winner.settings.getScanMode();
            }
            // TODO: split full batch scan results and truncated batch scan results to different
            // collections.
            for (ScanClient client : mBatchClients) {
                if (client.settings.getScanResultType() == ScanSettings.SCAN_RESULT_TYPE_FULL) {
                    params.fullScanscannerId = client.scannerId;
                } else {
                    params.truncatedScanscannerId = client.scannerId;
                }
            }
            return params;
        }

        // Batched scan doesn't require high duty cycle scan because scan result is reported
        // infrequently anyway. To avoid redefining paramete sets, map to the low duty cycle
        // parameter set as follows.
        private int getBatchScanWindowMillis(int scanMode) {
            ContentResolver resolver = mService.getContentResolver();
            switch (scanMode) {
                case ScanSettings.SCAN_MODE_LOW_LATENCY:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_BALANCED_WINDOW_MS,
                        SCAN_MODE_BALANCED_WINDOW_MS);
                default:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_LOW_POWER_WINDOW_MS,
                        SCAN_MODE_LOW_POWER_WINDOW_MS);
            }
        }

        private int getBatchScanIntervalMillis(int scanMode) {
            ContentResolver resolver = mService.getContentResolver();
            switch (scanMode) {
                case ScanSettings.SCAN_MODE_LOW_LATENCY:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_BALANCED_INTERVAL_MS,
                        SCAN_MODE_BALANCED_INTERVAL_MS);
                default:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_LOW_POWER_INTERVAL_MS,
                        SCAN_MODE_LOW_POWER_INTERVAL_MS);
            }
        }

        // Set the batch alarm to be triggered within a short window after batch interval. This
        // allows system to optimize wake up time while still allows a degree of precise control.
        private void setBatchAlarm() {
            // Cancel any pending alarm just in case.
            mAlarmManager.cancel(mBatchScanIntervalIntent);
            if (mBatchClients.isEmpty()) {
                return;
            }
            long batchTriggerIntervalMillis = getBatchTriggerIntervalMillis();
            // Allows the alarm to be triggered within
            // [batchTriggerIntervalMillis, 1.1 * batchTriggerIntervalMillis]
            long windowLengthMillis = batchTriggerIntervalMillis / 10;
            long windowStartMillis = SystemClock.elapsedRealtime() + batchTriggerIntervalMillis;
            mAlarmManager.setWindow(AlarmManager.ELAPSED_REALTIME_WAKEUP, windowStartMillis,
                    windowLengthMillis, mBatchScanIntervalIntent);
        }

        void stopRegularScan(ScanClient client) {
            // Remove scan filters and recycle filter indices.
            if (client == null) {
                return;
            }
            int deliveryMode = getDeliveryMode(client);
            if (deliveryMode == DELIVERY_MODE_ON_FOUND_LOST) {
                for (ScanFilter filter : client.filters) {
                    int entriesToFree = getNumOfTrackingAdvertisements(client.settings);
                    if (!manageAllocationOfTrackingAdvertisement(entriesToFree, false)) {
                        Log.e(TAG, "Error freeing for onfound/onlost filter resources "
                                + entriesToFree);
                        try {
                            mService.onScanManagerErrorCallback(client.scannerId,
                                    ScanCallback.SCAN_FAILED_INTERNAL_ERROR);
                        } catch (RemoteException e) {
                            Log.e(TAG, "failed on onScanManagerCallback at freeing", e);
                        }
                    }
                }
            }
            mRegularScanClients.remove(client);
            if (numRegularScanClients() == 0) {
                if (DBG) {
                    Log.d(TAG, "stop gattClientScanNative");
                }
                gattClientScanNative(false);
            }
            removeScanFilters(client.scannerId);
        }

        void regularScanTimeout(ScanClient client) {
            if (client.stats != null) {
                if (!isExemptFromScanDowngrade(client) && client.stats.isScanningTooLong()) {
                    Log.w(TAG,
                    "Moving scan client to opportunistic (scannerId " + client.scannerId + ")");
                    setOpportunisticScanClient(client);
                    removeScanFilters(client.scannerId);
                    client.stats.setScanTimeout(client.scannerId);
                }
            }

            // The scan should continue for background scans
            configureRegularScanParams();
            if (numRegularScanClients() == 0) {
                if (DBG) {
                    Log.d(TAG, "stop gattClientScanNative");
                }
                gattClientScanNative(false);
            }
        }

        void setOpportunisticScanClient(ScanClient client) {
            // TODO: Add constructor to ScanSettings.Builder
            // that can copy values from an existing ScanSettings object
            ScanSettings.Builder builder = new ScanSettings.Builder();
            ScanSettings settings = client.settings;
            builder.setScanMode(ScanSettings.SCAN_MODE_OPPORTUNISTIC);
            builder.setCallbackType(settings.getCallbackType());
            builder.setScanResultType(settings.getScanResultType());
            builder.setReportDelay(settings.getReportDelayMillis());
            builder.setNumOfMatches(settings.getNumOfMatches());
            client.settings = builder.build();
        }

        // Find the regular scan client information.
        ScanClient getRegularScanClient(int scannerId) {
            for (ScanClient client : mRegularScanClients) {
                if (client.scannerId == scannerId) {
                    return client;
                }
            }
            return null;
        }

        void stopBatchScan(ScanClient client) {
            mBatchClients.remove(client);
            removeScanFilters(client.scannerId);
            if (!isOpportunisticScanClient(client)) {
                resetBatchScan(client);
            }
        }

        void flushBatchResults(int scannerId) {
            if (DBG) {
                Log.d(TAG, "flushPendingBatchResults - scannerId = " + scannerId);
            }
            if (mBatchScanParms.fullScanscannerId != -1) {
                resetCountDownLatch();
                gattClientReadScanReportsNative(mBatchScanParms.fullScanscannerId,
                        SCAN_RESULT_TYPE_FULL);
                waitForCallback();
            }
            if (mBatchScanParms.truncatedScanscannerId != -1) {
                resetCountDownLatch();
                gattClientReadScanReportsNative(mBatchScanParms.truncatedScanscannerId,
                        SCAN_RESULT_TYPE_TRUNCATED);
                waitForCallback();
            }
            setBatchAlarm();
        }

        void cleanup() {
            mAlarmManager.cancel(mBatchScanIntervalIntent);
            // Protect against multiple calls of cleanup.
            if (mBatchAlarmReceiverRegistered) {
                mService.unregisterReceiver(mBatchAlarmReceiver);
            }
            mBatchAlarmReceiverRegistered = false;
        }

        private long getBatchTriggerIntervalMillis() {
            long intervalMillis = Long.MAX_VALUE;
            for (ScanClient client : mBatchClients) {
                if (client.settings != null && client.settings.getReportDelayMillis() > 0) {
                    intervalMillis =
                            Math.min(intervalMillis, client.settings.getReportDelayMillis());
                }
            }
            return intervalMillis;
        }

        // Add scan filters. The logic is:
        // If no offload filter can/needs to be set, set ALL_PASS filter.
        // Otherwise offload all filters to hardware and enable all filters.
        private void configureScanFilters(ScanClient client) {
            int scannerId = client.scannerId;
            int deliveryMode = getDeliveryMode(client);
            int trackEntries = 0;

            // Do not add any filters set by opportunistic scan clients
            if (isOpportunisticScanClient(client)) {
                return;
            }

            if (!shouldAddAllPassFilterToController(client, deliveryMode)) {
                return;
            }

            resetCountDownLatch();
            gattClientScanFilterEnableNative(scannerId, true);
            waitForCallback();

            if (shouldUseAllPassFilter(client)) {
                int filterIndex =
                        (deliveryMode == DELIVERY_MODE_BATCH) ? ALL_PASS_FILTER_INDEX_BATCH_SCAN
                                : ALL_PASS_FILTER_INDEX_REGULAR_SCAN;
                resetCountDownLatch();
                // Don't allow Onfound/onlost with all pass
                configureFilterParamter(scannerId, client, ALL_PASS_FILTER_SELECTION, filterIndex,
                        0);
                waitForCallback();
            } else {
                Log.d(TAG, "Available Filter size: " + mFilterIndexStack.size());
                Deque<Integer> clientFilterIndices = new ArrayDeque<Integer>();
                for (ScanFilter filter : client.filters) {
                    ScanFilterQueue queue = new ScanFilterQueue();
                    queue.addScanFilter(filter);
                    ScanFilterQueue.Entry[] entries = queue.toArray();
                    int featureSelection = queue.getFeatureSelection();
                    int filterIndex = mFilterIndexStack.pop();

                    if (entries != null && entries.length > 0) {
                        resetCountDownLatch();
                        gattClientScanFilterAddNative(scannerId, entries, filterIndex);
                        waitForCallback();
                    }

                    resetCountDownLatch();
                    if (deliveryMode == DELIVERY_MODE_ON_FOUND_LOST) {
                        trackEntries = getNumOfTrackingAdvertisements(client.settings);
                        if (!manageAllocationOfTrackingAdvertisement(trackEntries, true)) {
                            Log.e(TAG, "No hardware resources for onfound/onlost filter "
                                    + trackEntries);
                            try {
                                mService.onScanManagerErrorCallback(scannerId,
                                        ScanCallback.SCAN_FAILED_INTERNAL_ERROR);
                            } catch (RemoteException e) {
                                Log.e(TAG, "failed on onScanManagerCallback", e);
                            }
                        }
                    }
                    configureFilterParamter(scannerId, client, featureSelection, filterIndex,
                            trackEntries);
                    waitForCallback();
                    clientFilterIndices.add(filterIndex);
                }
                mClientFilterIndexMap.put(scannerId, clientFilterIndices);
            }
        }

        // Check whether the filter should be added to controller.
        // Note only on ALL_PASS filter should be added.
        private boolean shouldAddAllPassFilterToController(ScanClient client, int deliveryMode) {
            // Not an ALL_PASS client, need to add filter.
            if (!shouldUseAllPassFilter(client)) {
                return true;
            }

            if (deliveryMode == DELIVERY_MODE_BATCH) {
                mAllPassBatchClients.add(client.scannerId);
                return mAllPassBatchClients.size() == 1;
            } else {
                mAllPassRegularClients.add(client.scannerId);
                return mAllPassRegularClients.size() == 1;
            }
        }

        private void removeScanFilters(int scannerId) {
            Deque<Integer> filterIndices = mClientFilterIndexMap.remove(scannerId);
            if (filterIndices != null) {
                mFilterIndexStack.addAll(filterIndices);
                for (Integer filterIndex : filterIndices) {
                    resetCountDownLatch();
                    gattClientScanFilterParamDeleteNative(scannerId, filterIndex);
                    waitForCallback();
                }
            }
            // Remove if ALL_PASS filters are used.
            removeFilterIfExisits(mAllPassRegularClients, scannerId,
                    ALL_PASS_FILTER_INDEX_REGULAR_SCAN);
            removeFilterIfExisits(mAllPassBatchClients, scannerId,
                    ALL_PASS_FILTER_INDEX_BATCH_SCAN);
        }

        private void removeFilterIfExisits(Set<Integer> clients, int scannerId, int filterIndex) {
            if (!clients.contains(scannerId)) {
                return;
            }
            clients.remove(scannerId);
            // Remove ALL_PASS filter iff no app is using it.
            if (clients.isEmpty()) {
                resetCountDownLatch();
                gattClientScanFilterParamDeleteNative(scannerId, filterIndex);
                waitForCallback();
            }
        }

        private ScanClient getBatchScanClient(int scannerId) {
            for (ScanClient client : mBatchClients) {
                if (client.scannerId == scannerId) {
                    return client;
                }
            }
            return null;
        }

        /**
         * Return batch scan result type value defined in bt stack.
         */
        private int getResultType(BatchScanParams params) {
            if (params.fullScanscannerId != -1 && params.truncatedScanscannerId != -1) {
                return SCAN_RESULT_TYPE_BOTH;
            }
            if (params.truncatedScanscannerId != -1) {
                return SCAN_RESULT_TYPE_TRUNCATED;
            }
            if (params.fullScanscannerId != -1) {
                return SCAN_RESULT_TYPE_FULL;
            }
            return -1;
        }

        // Check if ALL_PASS filter should be used for the client.
        private boolean shouldUseAllPassFilter(ScanClient client) {
            if (client == null) {
                return true;
            }
            if (isRoutingScanClient(client)) {
                return false;
            }
            if (client.filters == null || client.filters.isEmpty()) {
                return true;
            }
            return client.filters.size() > mFilterIndexStack.size();
        }

        private void initFilterIndexStack() {
            int maxFiltersSupported =
                    AdapterService.getAdapterService().getNumOfOffloadedScanFilterSupported();
            // Start from index 3 as:
            // index 0 is reserved for ALL_PASS filter in Settings app.
            // index 1 is reserved for ALL_PASS filter for regular scan apps.
            // index 2 is reserved for ALL_PASS filter for batch scan apps.
            for (int i = 3; i < maxFiltersSupported; ++i) {
                mFilterIndexStack.add(i);
            }
        }

        // Configure filter parameters.
        private void configureFilterParamter(int scannerId, ScanClient client, int featureSelection,
                int filterIndex, int numOfTrackingEntries) {
            int deliveryMode = getDeliveryMode(client);
            int rssiThreshold = Byte.MIN_VALUE;
            ScanSettings settings = client.settings;
            int onFoundTimeout = getOnFoundOnLostTimeoutMillis(settings, true);
            int onLostTimeout = getOnFoundOnLostTimeoutMillis(settings, false);
            int onFoundCount = getOnFoundOnLostSightings(settings);
            onLostTimeout = 10000;
            if (DBG) {
                Log.d(TAG, "configureFilterParamter " + onFoundTimeout + " " + onLostTimeout + " "
                        + onFoundCount + " " + numOfTrackingEntries + ", deliveryMode=" + deliveryMode);
            }
            FilterParams filtValue =
                    new FilterParams(scannerId, filterIndex, featureSelection, LIST_LOGIC_TYPE,
                            FILTER_LOGIC_TYPE, rssiThreshold, rssiThreshold, deliveryMode,
                            onFoundTimeout, onLostTimeout, onFoundCount, numOfTrackingEntries);
            gattClientScanFilterParamAddNative(filtValue);
        }

        // Get delivery mode based on scan settings.
        private int getDeliveryMode(ScanClient client) {
            if (client == null) {
                return DELIVERY_MODE_IMMEDIATE;
            }
            ScanSettings settings = client.settings;
            if (settings == null) {
                return DELIVERY_MODE_IMMEDIATE;
            }
            if ((settings.getCallbackType() & ScanSettings.CALLBACK_TYPE_FIRST_MATCH) != 0
                    || (settings.getCallbackType() & ScanSettings.CALLBACK_TYPE_MATCH_LOST) != 0) {
                return DELIVERY_MODE_ON_FOUND_LOST;
            }
            if (isRoutingScanClient(client)) {
                return DELIVERY_MODE_ROUTE;
            }
            return settings.getReportDelayMillis() == 0 ? DELIVERY_MODE_IMMEDIATE
                    : DELIVERY_MODE_BATCH;
        }

        private int getScanWindowMillis(ScanSettings settings) {
            ContentResolver resolver = mService.getContentResolver();
            if (settings == null) {
                return Settings.Global.getInt(
                    resolver,
                    Settings.Global.BLE_SCAN_LOW_POWER_WINDOW_MS,
                    SCAN_MODE_LOW_POWER_WINDOW_MS);
            }

            switch (settings.getScanMode()) {
                case ScanSettings.SCAN_MODE_LOW_LATENCY:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_LOW_LATENCY_WINDOW_MS,
                        SCAN_MODE_LOW_LATENCY_WINDOW_MS);
                case ScanSettings.SCAN_MODE_BALANCED:
                case ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_BALANCED_WINDOW_MS,
                        SCAN_MODE_BALANCED_WINDOW_MS);
                case ScanSettings.SCAN_MODE_LOW_POWER:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_LOW_POWER_WINDOW_MS,
                        SCAN_MODE_LOW_POWER_WINDOW_MS);
                case ScanSettings.SCAN_MODE_SCREEN_OFF:
                    return mAdapterService.getScreenOffLowPowerWindowMillis();
                case ScanSettings.SCAN_MODE_SCREEN_OFF_BALANCED:
                    return mAdapterService.getScreenOffBalancedWindowMillis();
                default:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_LOW_POWER_WINDOW_MS,
                        SCAN_MODE_LOW_POWER_WINDOW_MS);
            }
        }

        private int getScanIntervalMillis(ScanSettings settings) {
            ContentResolver resolver = mService.getContentResolver();
            if (settings == null) {
                return Settings.Global.getInt(
                    resolver,
                    Settings.Global.BLE_SCAN_LOW_POWER_INTERVAL_MS,
                    SCAN_MODE_LOW_POWER_INTERVAL_MS);
            }
            switch (settings.getScanMode()) {
                case ScanSettings.SCAN_MODE_LOW_LATENCY:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_LOW_LATENCY_INTERVAL_MS,
                        SCAN_MODE_LOW_LATENCY_INTERVAL_MS);
                case ScanSettings.SCAN_MODE_BALANCED:
                case ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_BALANCED_INTERVAL_MS,
                        SCAN_MODE_BALANCED_INTERVAL_MS);
                case ScanSettings.SCAN_MODE_LOW_POWER:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_LOW_POWER_INTERVAL_MS,
                        SCAN_MODE_LOW_POWER_INTERVAL_MS);
                case ScanSettings.SCAN_MODE_SCREEN_OFF:
                    return mAdapterService.getScreenOffLowPowerIntervalMillis();
                case ScanSettings.SCAN_MODE_SCREEN_OFF_BALANCED:
                    return mAdapterService.getScreenOffBalancedIntervalMillis();
                default:
                    return Settings.Global.getInt(
                        resolver,
                        Settings.Global.BLE_SCAN_LOW_POWER_INTERVAL_MS,
                        SCAN_MODE_LOW_POWER_INTERVAL_MS);
            }
        }

        private int getOnFoundOnLostTimeoutMillis(ScanSettings settings, boolean onFound) {
            int factor;
            int timeout = ONLOST_ONFOUND_BASE_TIMEOUT_MS;

            if (settings.getMatchMode() == ScanSettings.MATCH_MODE_AGGRESSIVE) {
                factor = MATCH_MODE_AGGRESSIVE_TIMEOUT_FACTOR;
            } else {
                factor = MATCH_MODE_STICKY_TIMEOUT_FACTOR;
            }
            if (!onFound) {
                factor = factor * ONLOST_FACTOR;
            }
            return (timeout * factor);
        }

        private int getOnFoundOnLostSightings(ScanSettings settings) {
            if (settings == null) {
                return ONFOUND_SIGHTINGS_AGGRESSIVE;
            }
            if (settings.getMatchMode() == ScanSettings.MATCH_MODE_AGGRESSIVE) {
                return ONFOUND_SIGHTINGS_AGGRESSIVE;
            } else {
                return ONFOUND_SIGHTINGS_STICKY;
            }
        }

        private int getNumOfTrackingAdvertisements(ScanSettings settings) {
            if (settings == null) {
                return 0;
            }
            int val = 0;
            int maxTotalTrackableAdvertisements =
                    AdapterService.getAdapterService().getTotalNumOfTrackableAdvertisements();
            // controller based onfound onlost resources are scarce commodity; the
            // assignment of filters to num of beacons to track is configurable based
            // on hw capabilities. Apps give an intent and allocation of onfound
            // resources or failure there of is done based on availibility - FCFS model
            switch (settings.getNumOfMatches()) {
                case ScanSettings.MATCH_NUM_ONE_ADVERTISEMENT:
                    val = 1;
                    break;
                case ScanSettings.MATCH_NUM_FEW_ADVERTISEMENT:
                    val = 2;
                    break;
                case ScanSettings.MATCH_NUM_MAX_ADVERTISEMENT:
                    val = maxTotalTrackableAdvertisements / 2;
                    break;
                default:
                    val = 1;
                    if (DBG) {
                        Log.d(TAG, "Invalid setting for getNumOfMatches() "
                                + settings.getNumOfMatches());
                    }
            }
            return val;
        }

        private boolean manageAllocationOfTrackingAdvertisement(int numOfTrackableAdvertisement,
                boolean allocate) {
            int maxTotalTrackableAdvertisements =
                    AdapterService.getAdapterService().getTotalNumOfTrackableAdvertisements();
            synchronized (mCurUsedTrackableAdvertisements) {
                int availableEntries =
                        maxTotalTrackableAdvertisements - mCurUsedTrackableAdvertisements;
                if (allocate) {
                    if (availableEntries >= numOfTrackableAdvertisement) {
                        mCurUsedTrackableAdvertisements += numOfTrackableAdvertisement;
                        return true;
                    } else {
                        return false;
                    }
                } else {
                    if (numOfTrackableAdvertisement > mCurUsedTrackableAdvertisements) {
                        return false;
                    } else {
                        mCurUsedTrackableAdvertisements -= numOfTrackableAdvertisement;
                        return true;
                    }
                }
            }
        }


        /************************** Regular scan related native methods **************************/
        private native void registerScannerNative(long appUuidLsb, long appUuidMsb);

        private native void unregisterScannerNative(int scannerId);

        private native void gattClientScanNative(boolean start);

        private native void gattSetScanParametersNative(int clientIf, int scan_phy, int[] scanInterval,
                                                        int[] scanWindow);

        /************************** Filter related native methods ********************************/
        private native void gattClientScanFilterAddNative(int clientId,
                ScanFilterQueue.Entry[] entries, int filterIndex);

        private native void gattClientScanFilterParamAddNative(FilterParams filtValue);

        // Note this effectively remove scan filters for ALL clients.
        private native void gattClientScanFilterParamClearAllNative(int clientIf);

        private native void gattClientScanFilterParamDeleteNative(int clientIf, int filtIndex);

        private native void gattClientScanFilterClearNative(int clientIf, int filterIndex);

        private native void gattClientScanFilterEnableNative(int clientIf, boolean enable);

        /************************** Batch related native methods *********************************/
        private native void gattClientConfigBatchScanStorageNative(int clientIf,
                int maxFullReportsPercent, int maxTruncatedReportsPercent,
                int notifyThresholdPercent);

        private native void gattClientStartBatchScanNative(int clientIf, int scanMode,
                int scanIntervalUnit, int scanWindowUnit, int addressType, int discardRule);

        private native void gattClientStopBatchScanNative(int clientIf);

        private native void gattClientReadScanReportsNative(int clientIf, int scanType);
    }

    private boolean isScreenOn() {
        Display[] displays = mDm.getDisplays();

        if (displays == null) {
            return false;
        }

        for (Display display : displays) {
            if (display.getState() == Display.STATE_ON) {
                return true;
            }
        }

        return false;
    }

    public boolean isAptXLowLatencyModeEnabled() {
        Log.d(TAG, "isAptXLowLatencyModeEnabled: " + mIsAptXLowLatencyModeEnabled);
        return mIsAptXLowLatencyModeEnabled;
    }

    public void setAptXLowLatencyMode(boolean enabled){
        Log.d(TAG, "setAptXLowLatencyMode: mIsAptXLowLatencyModeEnabled: "
            + mIsAptXLowLatencyModeEnabled + "enabled: " + enabled);
        mIsAptXLowLatencyModeEnabled = enabled;
        if (mIsAptXLowLatencyModeEnabled) {
            sendMessage(MSG_SUSPEND_SCAN_ALL, null);
        } else {
            sendMessage(MSG_RESUME_SCANS, null);
        }
    }


    private final DisplayManager.DisplayListener mDisplayListener =
            new DisplayManager.DisplayListener() {
                @Override
                public void onDisplayAdded(int displayId) {}

                @Override
                public void onDisplayRemoved(int displayId) {}

                @Override
                public void onDisplayChanged(int displayId) {
                    if (isScreenOn()) {
                        sendMessage(MSG_SCREEN_ON, null);
                    } else {
                        sendMessage(MSG_SCREEN_OFF, null);
                    }
                }
            };

    private ActivityManager.OnUidImportanceListener mUidImportanceListener =
            new ActivityManager.OnUidImportanceListener() {
                @Override
                public void onUidImportance(final int uid, final int importance) {
                    if (mService.mScannerMap.getAppScanStatsByUid(uid) != null) {
                        Message message = new Message();
                        message.what = MSG_IMPORTANCE_CHANGE;
                        message.obj = new UidImportance(uid, importance);
                        mHandler.sendMessage(message);
                    }
                }
            };

    private BroadcastReceiver mLocationReceiver =
            new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    String action = intent.getAction();
                    if (LocationManager.MODE_CHANGED_ACTION.equals(action)) {
                        final boolean locationEnabled = mLocationManager.isLocationEnabled();
                        if (locationEnabled) {
                            sendMessage(MSG_RESUME_SCANS, null);
                        } else {
                            sendMessage(MSG_SUSPEND_SCANS, null);
                        }
                    }
                }
            };

    private void handleImportanceChange(UidImportance imp) {
        if (imp == null) {
            return;
        }
        int uid = imp.uid;
        int importance = imp.importance;
        boolean updatedScanParams = false;
        boolean isForeground = importance <= ActivityManager.RunningAppProcessInfo
                .IMPORTANCE_FOREGROUND_SERVICE;

        if (mIsUidForegroundMap.size() < MAX_IS_UID_FOREGROUND_MAP_SIZE) {
            mIsUidForegroundMap.put(uid, isForeground);
        }

        for (ScanClient client : mRegularScanClients) {
            if (client.appUid != uid) {
                continue;
            }
            if (isForeground) {
                if (client.updateScanMode(client.scanModeApp)) {
                    updatedScanParams = true;
                }
            } else {
                // Skip scan mode update in any of following cases
                //   1. screen is already off which triggers handleScreenOff()
                //   2. opportunistics scan
                if (mScreenOn && !mScanNative.isOpportunisticScanClient(client)
                        && client.updateScanMode(SCAN_MODE_APP_IN_BACKGROUND)) {
                    updatedScanParams = true;
                }
            }
            if (DBG) {
                Log.d(TAG, "uid " + uid + " isForeground " + isForeground
                        + " scanMode " + client.settings.getScanMode());
            }
        }

        if (updatedScanParams) {
            mScanNative.configureRegularScanParams();
        }
    }
}
