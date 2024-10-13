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
package com.android.bluetooth.gatt;

import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanSettings;
import android.os.Binder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.WorkSource;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.BluetoothStatsLog;
import com.android.bluetooth.btservice.AdapterService;
import com.android.internal.app.IBatteryStats;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;

/**
 * ScanStats class helps keep track of information about scans
 * on a per application basis.
 * @hide
 */
/*package*/ class AppScanStats {
    private static final String TAG = AppScanStats.class.getSimpleName();

    static final DateFormat DATE_FORMAT = new SimpleDateFormat("MM-dd HH:mm:ss");

    // Weight is the duty cycle of the scan mode
    static final int OPPORTUNISTIC_WEIGHT = 0;
    static final int LOW_POWER_WEIGHT = 10;
    static final int AMBIENT_DISCOVERY_WEIGHT = 20;
    static final int BALANCED_WEIGHT = 25;
    static final int LOW_LATENCY_WEIGHT = 100;
    static final int LARGE_SCAN_TIME_GAP_MS = 24000;

    /* ContextMap here is needed to grab Apps and Connections */ ContextMap mContextMap;

    // GattService is needed to add scan event protos to be dumped later
    final GattService mGattService;

    /* Battery stats is used to keep track of scans and result stats */ IBatteryStats
            mBatteryStats;

    private final AdapterService mAdapterService;

    class LastScan {
        public long duration;
        public long suspendDuration;
        public long suspendStartTime;
        public boolean isSuspended;
        public long timestamp;
        public boolean isOpportunisticScan;
        public boolean isTimeout;
        public boolean isBackgroundScan;
        public boolean isFilterScan;
        public boolean isCallbackScan;
        public boolean isBatchScan;
        public int results;
        public int scannerId;
        public int scanMode;
        public int scanCallbackType;
        public String filterString;

        LastScan(long timestamp, boolean isFilterScan, boolean isCallbackScan, int scannerId,
                int scanMode, int scanCallbackType) {
            this.duration = 0;
            this.timestamp = timestamp;
            this.isOpportunisticScan = false;
            this.isTimeout = false;
            this.isBackgroundScan = false;
            this.isFilterScan = isFilterScan;
            this.isCallbackScan = isCallbackScan;
            this.isBatchScan = false;
            this.scanMode = scanMode;
            this.scanCallbackType = scanCallbackType;
            this.results = 0;
            this.scannerId = scannerId;
            this.suspendDuration = 0;
            this.suspendStartTime = 0;
            this.isSuspended = false;
            this.filterString = "";
        }
    }

    static int getNumScanDurationsKept() {
        return AdapterService.getAdapterService().getScanQuotaCount();
    }

    // This constant defines the time window an app can scan multiple times.
    // Any single app can scan up to |NUM_SCAN_DURATIONS_KEPT| times during
    // this window. Once they reach this limit, they must wait until their
    // earliest recorded scan exits this window.
    static long getExcessiveScanningPeriodMillis() {
        return AdapterService.getAdapterService().getScanQuotaWindowMillis();
    }

    // Maximum msec before scan gets downgraded to opportunistic
    static long getScanTimeoutMillis() {
        return AdapterService.getAdapterService().getScanTimeoutMillis();
    }

    // Scan mode upgrade duration after scanStart()
    static long getScanUpgradeDurationMillis() {
        return AdapterService.getAdapterService().getScanUpgradeDurationMillis();
    }

    public String appName;
    public WorkSource mWorkSource; // Used for BatteryStats and BluetoothStatsLog
    private int mScansStarted = 0;
    private int mScansStopped = 0;
    public boolean isRegistered = false;
    private long mScanStartTime = 0;
    private long mTotalActiveTime = 0;
    private long mTotalSuspendTime = 0;
    private long mTotalScanTime = 0;
    private long mOppScanTime = 0;
    private long mLowPowerScanTime = 0;
    private long mBalancedScanTime = 0;
    private long mLowLantencyScanTime = 0;
    private long mAmbientDiscoveryScanTime = 0;
    private int mOppScan = 0;
    private int mLowPowerScan = 0;
    private int mBalancedScan = 0;
    private int mLowLantencyScan = 0;
    private int mAmbientDiscoveryScan = 0;
    private List<LastScan> mLastScans = new ArrayList<LastScan>();
    private HashMap<Integer, LastScan> mOngoingScans = new HashMap<Integer, LastScan>();
    public long startTime = 0;
    public long stopTime = 0;
    public int results = 0;

    AppScanStats(String name, WorkSource source, ContextMap map, GattService service) {
        appName = name;
        mContextMap = map;
        mGattService = service;
        mBatteryStats = IBatteryStats.Stub.asInterface(ServiceManager.getService("batterystats"));

        if (source == null) {
            // Bill the caller if the work source isn't passed through
            source = new WorkSource(Binder.getCallingUid(), appName);
        }
        mWorkSource = source;
        mAdapterService = Objects.requireNonNull(AdapterService.getAdapterService());
    }

    synchronized void addResult(int scannerId) {
        LastScan scan = getScanFromScannerId(scannerId);
        if (scan != null) {
            scan.results++;

            // Only update battery stats after receiving 100 new results in order
            // to lower the cost of the binder transaction
            if (scan.results % 100 == 0) {
                try {
                    mBatteryStats.noteBleScanResults(mWorkSource, 100);
                } catch (RemoteException e) {
                    /* ignore */
                }
                BluetoothStatsLog.write(
                        BluetoothStatsLog.BLE_SCAN_RESULT_RECEIVED, mWorkSource, 100);
            }
        }

        results++;
    }

    boolean isScanning() {
        return !mOngoingScans.isEmpty();
    }

    LastScan getScanFromScannerId(int scannerId) {
        return mOngoingScans.get(scannerId);
    }

    synchronized void recordScanStart(ScanSettings settings, List<ScanFilter> filters,
            boolean isFilterScan, boolean isCallbackScan, int scannerId) {
        LastScan existingScan = getScanFromScannerId(scannerId);
        if (existingScan != null) {
            return;
        }
        this.mScansStarted++;
        startTime = SystemClock.elapsedRealtime();

        LastScan scan = new LastScan(startTime, isFilterScan, isCallbackScan, scannerId,
                settings.getScanMode(), settings.getCallbackType());
        if (settings != null) {
            scan.isOpportunisticScan = scan.scanMode == ScanSettings.SCAN_MODE_OPPORTUNISTIC;
            scan.isBackgroundScan =
                    (scan.scanCallbackType & ScanSettings.CALLBACK_TYPE_FIRST_MATCH) != 0;
            scan.isBatchScan =
                    settings.getCallbackType() == ScanSettings.CALLBACK_TYPE_ALL_MATCHES
                    && settings.getReportDelayMillis() != 0;
            switch (scan.scanMode) {
                case ScanSettings.SCAN_MODE_OPPORTUNISTIC:
                    mOppScan++;
                    break;
                case ScanSettings.SCAN_MODE_LOW_POWER:
                    mLowPowerScan++;
                    break;
                case ScanSettings.SCAN_MODE_BALANCED:
                    mBalancedScan++;
                    break;
                case ScanSettings.SCAN_MODE_LOW_LATENCY:
                    mLowLantencyScan++;
                    break;
                case ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY:
                    mAmbientDiscoveryScan++;
                    break;
            }
        }

        if (isFilterScan) {
            for (ScanFilter filter : filters) {
                scan.filterString +=
                      "\n      └ " + filterToStringWithoutNullParam(filter);
            }
        }

        BluetoothMetricsProto.ScanEvent scanEvent = BluetoothMetricsProto.ScanEvent.newBuilder()
                .setScanEventType(BluetoothMetricsProto.ScanEvent.ScanEventType.SCAN_EVENT_START)
                .setScanTechnologyType(
                        BluetoothMetricsProto.ScanEvent.ScanTechnologyType.SCAN_TECH_TYPE_LE)
                .setEventTimeMillis(System.currentTimeMillis())
                .setInitiator(truncateAppName(appName)).build();
        mGattService.addScanEvent(scanEvent);

        if (!isScanning()) {
            mScanStartTime = startTime;
        }
        try {
            boolean isUnoptimized =
                    !(scan.isFilterScan || scan.isBackgroundScan || scan.isOpportunisticScan);
            mBatteryStats.noteBleScanStarted(mWorkSource, isUnoptimized);
        } catch (RemoteException e) {
            /* ignore */
        }
        BluetoothStatsLog.write(BluetoothStatsLog.BLE_SCAN_STATE_CHANGED, mWorkSource,
                BluetoothStatsLog.BLE_SCAN_STATE_CHANGED__STATE__ON,
                scan.isFilterScan, scan.isBackgroundScan, scan.isOpportunisticScan);

        mOngoingScans.put(scannerId, scan);
    }

    synchronized void recordScanStop(int scannerId) {
        LastScan scan = getScanFromScannerId(scannerId);
        if (scan == null) {
            return;
        }
        this.mScansStopped++;
        stopTime = SystemClock.elapsedRealtime();
        long scanDuration = stopTime - scan.timestamp;
        scan.duration = scanDuration;
        if (scan.isSuspended) {
            long suspendDuration = stopTime - scan.suspendStartTime;
            scan.suspendDuration += suspendDuration;
            mTotalSuspendTime += suspendDuration;
        }
        mOngoingScans.remove(scannerId);
        if (mLastScans.size() >= mAdapterService.getScanQuotaCount()) {
            mLastScans.remove(0);
        }
        mLastScans.add(scan);

        BluetoothMetricsProto.ScanEvent scanEvent = BluetoothMetricsProto.ScanEvent.newBuilder()
                .setScanEventType(BluetoothMetricsProto.ScanEvent.ScanEventType.SCAN_EVENT_STOP)
                .setScanTechnologyType(
                        BluetoothMetricsProto.ScanEvent.ScanTechnologyType.SCAN_TECH_TYPE_LE)
                .setEventTimeMillis(System.currentTimeMillis())
                .setInitiator(truncateAppName(appName))
                .setNumberResults(scan.results)
                .build();
        mGattService.addScanEvent(scanEvent);

        mTotalScanTime += scanDuration;
        long activeDuration = scanDuration - scan.suspendDuration;
        mTotalActiveTime += activeDuration;
        switch (scan.scanMode) {
            case ScanSettings.SCAN_MODE_OPPORTUNISTIC:
                mOppScanTime += activeDuration;
                break;
            case ScanSettings.SCAN_MODE_LOW_POWER:
                mLowPowerScanTime += activeDuration;
                break;
            case ScanSettings.SCAN_MODE_BALANCED:
                mBalancedScanTime += activeDuration;
                break;
            case ScanSettings.SCAN_MODE_LOW_LATENCY:
                mLowLantencyScanTime += activeDuration;
                break;
            case ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY:
                mAmbientDiscoveryScanTime += activeDuration;
                break;
        }

        try {
            // Inform battery stats of any results it might be missing on scan stop
            boolean isUnoptimized =
                    !(scan.isFilterScan || scan.isBackgroundScan || scan.isOpportunisticScan);
            mBatteryStats.noteBleScanResults(mWorkSource, scan.results % 100);
            mBatteryStats.noteBleScanStopped(mWorkSource, isUnoptimized);
        } catch (RemoteException e) {
            /* ignore */
        }
        BluetoothStatsLog.write(BluetoothStatsLog.BLE_SCAN_RESULT_RECEIVED, mWorkSource, scan.results % 100);
        BluetoothStatsLog.write(BluetoothStatsLog.BLE_SCAN_STATE_CHANGED, mWorkSource,
                BluetoothStatsLog.BLE_SCAN_STATE_CHANGED__STATE__OFF,
                scan.isFilterScan, scan.isBackgroundScan, scan.isOpportunisticScan);
    }

    synchronized void recordScanSuspend(int scannerId) {
        LastScan scan = getScanFromScannerId(scannerId);
        if (scan == null || scan.isSuspended) {
            return;
        }
        scan.suspendStartTime = SystemClock.elapsedRealtime();
        scan.isSuspended = true;
    }

    synchronized void recordScanResume(int scannerId) {
        LastScan scan = getScanFromScannerId(scannerId);
        long suspendDuration = 0;
        if (scan == null || !scan.isSuspended) {
            return;
        }
        scan.isSuspended = false;
        stopTime = SystemClock.elapsedRealtime();
        suspendDuration = stopTime - scan.suspendStartTime;
        scan.suspendDuration += suspendDuration;
        mTotalSuspendTime += suspendDuration;
    }

    synchronized void setScanTimeout(int scannerId) {
        if (!isScanning()) {
            return;
        }

        LastScan scan = getScanFromScannerId(scannerId);
        if (scan != null) {
            scan.isTimeout = true;
        }
    }

    synchronized boolean isScanningTooFrequently() {
        if (mLastScans.size() < mAdapterService.getScanQuotaCount()) {
            return false;
        }

        return (SystemClock.elapsedRealtime() - mLastScans.get(0).timestamp)
                < mAdapterService.getScanQuotaWindowMillis();
    }

    synchronized boolean isScanningTooLong() {
        if (!isScanning()) {
            return false;
        }
        return (SystemClock.elapsedRealtime() - mScanStartTime)
                > mAdapterService.getScanTimeoutMillis();
    }

    synchronized boolean hasRecentScan() {
        if (!isScanning() || mLastScans.isEmpty()) {
            return false;
        }
        LastScan lastScan = mLastScans.get(mLastScans.size() - 1);
        return ((SystemClock.elapsedRealtime() - lastScan.duration - lastScan.timestamp)
                < LARGE_SCAN_TIME_GAP_MS);
    }

    // This function truncates the app name for privacy reasons. Apps with
    // four part package names or more get truncated to three parts, and apps
    // with three part package names names get truncated to two. Apps with two
    // or less package names names are untouched.
    // Examples: one.two.three.four => one.two.three
    //           one.two.three => one.two
    private String truncateAppName(String name) {
        String initiator = name;
        String[] nameSplit = initiator.split("\\.");
        if (nameSplit.length > 3) {
            initiator = nameSplit[0] + "." + nameSplit[1] + "." + nameSplit[2];
        } else if (nameSplit.length == 3) {
            initiator = nameSplit[0] + "." + nameSplit[1];
        }

        return initiator;
    }

    private static String filterToStringWithoutNullParam(ScanFilter filter) {
        String filterString = "BluetoothLeScanFilter [";
        if (filter.getDeviceName() != null) {
            filterString += " DeviceName=" + filter.getDeviceName();
        }
        if (filter.getDeviceAddress() != null) {
            filterString += " DeviceAddress=" + filter.getDeviceAddress();
        }
        if (filter.getServiceUuid() != null) {
            filterString += " ServiceUuid=" + filter.getServiceUuid();
        }
        if (filter.getServiceUuidMask() != null) {
            filterString += " ServiceUuidMask=" + filter.getServiceUuidMask();
        }
        if (filter.getServiceSolicitationUuid() != null) {
            filterString += " ServiceSolicitationUuid=" + filter.getServiceSolicitationUuid();
        }
        if (filter.getServiceSolicitationUuidMask() != null) {
            filterString +=
                  " ServiceSolicitationUuidMask=" + filter.getServiceSolicitationUuidMask();
        }
        if (filter.getServiceDataUuid() != null) {
            filterString += " ServiceDataUuid=" + Objects.toString(filter.getServiceDataUuid());
        }
        if (filter.getServiceData() != null) {
            filterString += " ServiceData=" + Arrays.toString(filter.getServiceData());
        }
        if (filter.getServiceDataMask() != null) {
            filterString += " ServiceDataMask=" + Arrays.toString(filter.getServiceDataMask());
        }
        if (filter.getManufacturerId() >= 0) {
            filterString += " ManufacturerId=" + filter.getManufacturerId();
        }
        if (filter.getManufacturerData() != null) {
            filterString += " ManufacturerData=" + Arrays.toString(filter.getManufacturerData());
        }
        if (filter.getManufacturerDataMask() != null) {
            filterString +=
                  " ManufacturerDataMask=" +  Arrays.toString(filter.getManufacturerDataMask());
        }
        filterString += " ]";
        return filterString;
    }


    private static String scanModeToString(int scanMode) {
        switch (scanMode) {
            case ScanSettings.SCAN_MODE_OPPORTUNISTIC:
                return "OPPORTUNISTIC";
            case ScanSettings.SCAN_MODE_LOW_LATENCY:
                return "LOW_LATENCY";
            case ScanSettings.SCAN_MODE_BALANCED:
                return "BALANCED";
            case ScanSettings.SCAN_MODE_LOW_POWER:
                return "LOW_POWER";
            case ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY:
                return "AMBIENT_DISCOVERY";
            default:
                return "UNKNOWN(" + scanMode + ")";
        }
    }

    private static String callbackTypeToString(int callbackType) {
        switch (callbackType) {
            case ScanSettings.CALLBACK_TYPE_ALL_MATCHES:
                return "ALL_MATCHES";
            case ScanSettings.CALLBACK_TYPE_FIRST_MATCH:
                return "FIRST_MATCH";
            case ScanSettings.CALLBACK_TYPE_MATCH_LOST:
                return "LOST";
            default:
                return callbackType == (ScanSettings.CALLBACK_TYPE_FIRST_MATCH
                    | ScanSettings.CALLBACK_TYPE_MATCH_LOST) ? "[FIRST_MATCH | LOST]" : "UNKNOWN: "
                    + callbackType;
        }
    }

    synchronized void dumpToString(StringBuilder sb) {
        long currentTime = System.currentTimeMillis();
        long currTime = SystemClock.elapsedRealtime();
        long Score = 0;
        long scanDuration = 0;
        long suspendDuration = 0;
        long activeDuration = 0;
        long totalActiveTime = mTotalActiveTime;
        long totalSuspendTime = mTotalSuspendTime;
        long totalScanTime = mTotalScanTime;
        long oppScanTime = mOppScanTime;
        long lowPowerScanTime = mLowPowerScanTime;
        long balancedScanTime = mBalancedScanTime;
        long lowLatencyScanTime = mLowLantencyScanTime;
        long ambientDiscoveryScanTime = mAmbientDiscoveryScanTime;
        int oppScan = mOppScan;
        int lowPowerScan = mLowPowerScan;
        int balancedScan = mBalancedScan;
        int lowLatencyScan = mLowLantencyScan;
        int ambientDiscoveryScan = mAmbientDiscoveryScan;

        if (!mOngoingScans.isEmpty()) {
            for (Integer key : mOngoingScans.keySet()) {
                LastScan scan = mOngoingScans.get(key);
                scanDuration = currTime - scan.timestamp;

                if (scan.isSuspended) {
                    suspendDuration = currTime - scan.suspendStartTime;
                    totalSuspendTime += suspendDuration;
                }

                totalScanTime += scanDuration;
                totalSuspendTime += suspendDuration;
                activeDuration = scanDuration - scan.suspendDuration - suspendDuration;
                totalActiveTime += activeDuration;
                switch (scan.scanMode) {
                    case ScanSettings.SCAN_MODE_OPPORTUNISTIC:
                        oppScanTime += activeDuration;
                        break;
                    case ScanSettings.SCAN_MODE_LOW_POWER:
                        lowPowerScanTime += activeDuration;
                        break;
                    case ScanSettings.SCAN_MODE_BALANCED:
                        balancedScanTime += activeDuration;
                        break;
                    case ScanSettings.SCAN_MODE_LOW_LATENCY:
                        lowLatencyScanTime += activeDuration;
                        break;
                    case ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY:
                        ambientDiscoveryScan += activeDuration;
                        break;
                }
            }
        }
        Score = (oppScanTime * OPPORTUNISTIC_WEIGHT + lowPowerScanTime * LOW_POWER_WEIGHT
              + balancedScanTime * BALANCED_WEIGHT + lowLatencyScanTime * LOW_LATENCY_WEIGHT
              + ambientDiscoveryScanTime * AMBIENT_DISCOVERY_WEIGHT) / 100;

        sb.append("  " + appName);
        if (isRegistered) {
            sb.append(" (Registered)");
        }

        sb.append("\n  LE scans (started/stopped)                                  : "
                + mScansStarted + " / " + mScansStopped);
        sb.append("\n  Scan time in ms (active/suspend/total)                      : "
                + totalActiveTime + " / " + totalSuspendTime + " / " + totalScanTime);
        sb.append("\n  Scan time with mode in ms "
                + "(Opp/LowPower/Balanced/LowLatency/AmbientDiscovery):"
                + oppScanTime + " / " + lowPowerScanTime + " / " + balancedScanTime + " / "
                + lowLatencyScanTime + " / " + ambientDiscoveryScanTime);
        sb.append("\n  Scan mode counter (Opp/LowPower/Balanced/LowLatency/AmbientDiscovery):"
                + oppScan + " / " + lowPowerScan + " / " + balancedScan + " / " + lowLatencyScan
                + " / " + ambientDiscoveryScan);
        sb.append("\n  Score                                                       : " + Score);
        sb.append("\n  Total number of results                                     : " + results);

        if (!mLastScans.isEmpty()) {
            sb.append("\n  Last " + mLastScans.size()
                    + " scans                                                :");

            for (int i = 0; i < mLastScans.size(); i++) {
                LastScan scan = mLastScans.get(i);
                Date timestamp = new Date(currentTime - currTime + scan.timestamp);
                sb.append("\n    " + DATE_FORMAT.format(timestamp) + " - ");
                sb.append(scan.duration + "ms ");
                if (scan.isOpportunisticScan) {
                    sb.append("Opp ");
                }
                if (scan.isBackgroundScan) {
                    sb.append("Back ");
                }
                if (scan.isTimeout) {
                    sb.append("Forced ");
                }
                if (scan.isFilterScan) {
                    sb.append("Filter ");
                }
                sb.append(scan.results + " results");
                sb.append(" (" + scan.scannerId + ") ");
                if (scan.isCallbackScan) {
                    sb.append("CB ");
                } else {
                    sb.append("PI ");
                }
                if (scan.isBatchScan) {
                    sb.append("Batch Scan");
                } else {
                    sb.append("Regular Scan");
                }
                if (scan.suspendDuration != 0) {
                    activeDuration = scan.duration - scan.suspendDuration;
                    sb.append("\n      └ " + "Suspended Time: " + scan.suspendDuration
                            + "ms, Active Time: " + activeDuration);
                }
                sb.append("\n      └ " + "Scan Config: [ ScanMode="
                        + scanModeToString(scan.scanMode) + ", callbackType="
                        + callbackTypeToString(scan.scanCallbackType) + " ]");
                if (scan.isFilterScan) {
                    sb.append(scan.filterString);
                }
            }
        }

        if (!mOngoingScans.isEmpty()) {
            sb.append("\n  Ongoing scans                                               :");
            for (Integer key : mOngoingScans.keySet()) {
                LastScan scan = mOngoingScans.get(key);
                Date timestamp = new Date(currentTime - currTime + scan.timestamp);
                sb.append("\n    " + DATE_FORMAT.format(timestamp) + " - ");
                sb.append((currTime - scan.timestamp) + "ms ");
                if (scan.isOpportunisticScan) {
                    sb.append("Opp ");
                }
                if (scan.isBackgroundScan) {
                    sb.append("Back ");
                }
                if (scan.isTimeout) {
                    sb.append("Forced ");
                }
                if (scan.isFilterScan) {
                    sb.append("Filter ");
                }
                if (scan.isSuspended) {
                    sb.append("Suspended ");
                }
                sb.append(scan.results + " results");
                sb.append(" (" + scan.scannerId + ") ");
                if (scan.isCallbackScan) {
                    sb.append("CB ");
                } else {
                    sb.append("PI ");
                }
                if (scan.isBatchScan) {
                    sb.append("Batch Scan");
                } else {
                    sb.append("Regular Scan");
                }
                if (scan.suspendStartTime != 0) {
                    long duration = scan.suspendDuration + (scan.isSuspended ? (currTime
                            - scan.suspendStartTime) : 0);
                    activeDuration = scan.duration - scan.suspendDuration;
                    sb.append("\n      └ " + "Suspended Time:" + scan.suspendDuration
                            + "ms, Active Time:" + activeDuration);
                }
                sb.append("\n      └ " + "Scan Config: [ ScanMode="
                        + scanModeToString(scan.scanMode) + ", callbackType="
                        + callbackTypeToString(scan.scanCallbackType) + " ]");
                if (scan.isFilterScan) {
                    sb.append(scan.filterString);
                }
            }
        }

        ContextMap.App appEntry = mContextMap.getByName(appName);
        if (appEntry != null && isRegistered) {
            sb.append("\n  Application ID                     : " + appEntry.id);
            sb.append("\n  UUID                               : " + appEntry.uuid);

            List<ContextMap.Connection> connections = mContextMap.getConnectionByApp(appEntry.id);

            sb.append("\n  Connections: " + connections.size());

            Iterator<ContextMap.Connection> ii = connections.iterator();
            while (ii.hasNext()) {
                ContextMap.Connection connection = ii.next();
                long connectionTime = currTime - connection.startTime;
                Date timestamp = new Date(currentTime - currTime + connection.startTime);
                sb.append("\n    " + DATE_FORMAT.format(timestamp) + " - ");
                sb.append((connectionTime) + "ms ");
                sb.append(": " + connection.address + " (" + connection.connId + ")");
            }
        }
        sb.append("\n\n");
    }
}
