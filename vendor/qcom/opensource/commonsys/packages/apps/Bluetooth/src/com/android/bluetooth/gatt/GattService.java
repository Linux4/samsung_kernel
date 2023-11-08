/*
  Not a contribution
*/

/*
 * Copyright (C) 2017 The Android Open Source Project
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
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
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

package com.android.bluetooth.gatt;

import static com.android.bluetooth.Utils.checkCallerTargetSdk;
import static com.android.bluetooth.Utils.enforceBluetoothPrivilegedPermission;

import android.annotation.RequiresPermission;
import android.annotation.SuppressLint;
import android.app.AppOpsManager;
import android.app.PendingIntent;
import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.IBluetoothGatt;
import android.bluetooth.IBluetoothGattCallback;
import android.bluetooth.IBluetoothGattServerCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertisingSetParameters;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.IAdvertisingSetCallback;
import android.bluetooth.le.IPeriodicAdvertisingCallback;
import android.bluetooth.le.IScannerCallback;
import android.bluetooth.le.PeriodicAdvertisingParameters;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.companion.AssociationInfo;
import android.companion.ICompanionDeviceManager;
import android.content.AttributionSource;
import android.content.Context;
import android.content.Intent;
import android.net.MacAddress;
import android.os.Binder;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.ParcelUuid;
import android.os.RemoteException;
import android.os.DeadObjectException;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.WorkSource;
import android.provider.DeviceConfig;
import android.provider.Settings;
import android.sysprop.BluetoothProperties;
import android.text.format.DateUtils;
import android.util.Log;


import android.os.HandlerThread;
import android.os.Looper;

import com.android.bluetooth.BluetoothMetricsProto;
import com.android.bluetooth.R;
import com.android.bluetooth.Utils;
import com.android.bluetooth.btservice.AbstractionLayer;
import com.android.bluetooth.btservice.AdapterService;
import com.android.bluetooth.btservice.BluetoothAdapterProxy;
import com.android.bluetooth.btservice.ProfileService;
import com.android.bluetooth.util.NumberUtils;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.HexDump;
import com.android.modules.utils.SynchronousResultReceiver;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.TimeUnit;
import java.util.function.Predicate;

/**
 * Provides Bluetooth Gatt profile, as a service in
 * the Bluetooth application.
 * @hide
 */
public class GattService extends ProfileService {
    private static final boolean DBG = GattServiceConfig.DBG;
    private static final boolean VDBG = GattServiceConfig.VDBG;
    private static final String TAG = GattServiceConfig.TAG_PREFIX + "GattService";
    private static final String UUID_SUFFIX = "-0000-1000-8000-00805f9b34fb";
    private static final String UUID_ZERO_PAD = "00000000";

    static final int SCAN_FILTER_ENABLED = 1;
    static final int SCAN_FILTER_MODIFIED = 2;

    private static final int MAC_ADDRESS_LENGTH = 6;
    // Batch scan related constants.
    private static final int TRUNCATED_RESULT_SIZE = 11;
    private static final int TIME_STAMP_LENGTH = 2;

    /**
     * The default floor value for LE batch scan report delays greater than 0
     */
    private static final long DEFAULT_REPORT_DELAY_FLOOR = 5000 ;

    // onFoundLost related constants
    private static final int ADVT_STATE_ONFOUND = 0;
    private static final int ADVT_STATE_ONLOST = 1;

    private static final int ET_LEGACY_MASK = 0x10;
    private static final int ET_CONNECTABLE_MASK = 0x01;
    private static final UUID HID_SERVICE_UUID =
            UUID.fromString("00001812-0000-1000-8000-00805F9B34FB");

    private static final UUID[] HID_UUIDS = {
            UUID.fromString("00002A4A-0000-1000-8000-00805F9B34FB"),
            UUID.fromString("00002A4B-0000-1000-8000-00805F9B34FB"),
            UUID.fromString("00002A4C-0000-1000-8000-00805F9B34FB"),
            UUID.fromString("00002A4D-0000-1000-8000-00805F9B34FB")
    };

    private static final UUID ANDROID_TV_REMOTE_SERVICE_UUID =
            UUID.fromString("AB5E0001-5A21-4F05-BC7D-AF01F617B664");

    private static final UUID FIDO_SERVICE_UUID =
            UUID.fromString("0000FFFD-0000-1000-8000-00805F9B34FB"); // U2F

    /**
     * Example raw beacons captured from a Blue Charm BC011
     */
    private static final String[] TEST_MODE_BEACONS = new String[] {
            "020106",
            "0201060303AAFE1716AAFE10EE01626C7565636861726D626561636F6E730009168020691E0EFE13551109426C7565436861726D5F313639363835000000",
            "0201060303AAFE1716AAFE00EE626C7565636861726D31000000000001000009168020691E0EFE13551109426C7565436861726D5F313639363835000000",
            "0201060303AAFE1116AAFE20000BF017000008874803FB93540916802069080EFE13551109426C7565436861726D5F313639363835000000000000000000",
            "0201061AFF4C000215426C7565436861726D426561636F6E730EFE1355C509168020691E0EFE13551109426C7565436861726D5F31363936383500000000",
    };

    /**
     * Keep the arguments passed in for the PendingIntent.
     */
    class PendingIntentInfo {
        public PendingIntent intent;
        public ScanSettings settings;
        public List<ScanFilter> filters;
        public String callingPackage;

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof PendingIntentInfo)) {
                return false;
            }
            return intent.equals(((PendingIntentInfo) other).intent);
        }
    }

    /**
     * List of our registered scanners.
     */
    class ScannerMap extends ContextMap<IScannerCallback, PendingIntentInfo> {}

    ScannerMap mScannerMap = new ScannerMap();

    /**
     * List of our registered clients.
     */
    class ClientMap extends ContextMap<IBluetoothGattCallback, Void> {}

    ClientMap mClientMap = new ClientMap();

    /**
     * List of our registered server apps.
     */
    class ServerMap extends ContextMap<IBluetoothGattServerCallback, Void> {}

    ServerMap mServerMap = new ServerMap();

    /**
     * Server handle map.
     */
    HandleMap mHandleMap = new HandleMap();
    private List<UUID> mAdvertisingServiceUuids = new ArrayList<UUID>();

    private int mMaxScanFilters;

    private static final int NUM_SCAN_EVENTS_KEPT = 20;

    /**
     * Internal list of scan events to use with the proto
     */
    private final ArrayDeque<BluetoothMetricsProto.ScanEvent> mScanEvents =
            new ArrayDeque<>(NUM_SCAN_EVENTS_KEPT);

    /**
     * Set of restricted (which require a BLUETOOTH_PRIVILEGED permission) handles per connectionId.
     */
    private final Map<Integer, Set<Integer>> mRestrictedHandles = new HashMap<>();

    /**
     * HashMap used to synchronize writeCharacteristic calls mapping remote device address to
     * available permit (connectId or -1).
     */
    private final HashMap<String, Integer> mPermits = new HashMap<>();

    private BluetoothAdapter mAdapter;
    private AdapterService mAdapterService;
    private BluetoothAdapterProxy mBluetoothAdapterProxy;
    private AdvertiseManager mAdvertiseManager;
    private PeriodicScanManager mPeriodicScanManager;
    private ScanManager mScanManager;
    private AppOpsManager mAppOps;
    private ICompanionDeviceManager mCompanionManager;
    private String mExposureNotificationPackage;
    private Handler mTestModeHandler;
    private final Object mTestModeLock = new Object();

    public static boolean isEnabled() {
        return BluetoothProperties.isProfileGattEnabled().orElse(false);
    }

    /**
     */
    private final Predicate<ScanResult> mLocationDenylistPredicate = (scanResult) -> {
        final MacAddress parsedAddress = MacAddress
                .fromString(scanResult.getDevice().getAddress());
        if (mAdapterService.getLocationDenylistMac().test(parsedAddress.toByteArray())) {
            Log.v(TAG, "Skipping device matching denylist: " + parsedAddress);
            return true;
        }
        final ScanRecord scanRecord = scanResult.getScanRecord();
        if (scanRecord.matchesAnyField(mAdapterService.getLocationDenylistAdvertisingData())) {
            Log.v(TAG, "Skipping data matching denylist: " + scanRecord);
            return true;
        }
        return false;
    };

    private static GattService sGattService;
    private boolean mNativeAvailable;

    /**
     * Reliable write queue
     */
    private Set<String> mReliableQueue = new HashSet<String>();

    private ScanResultHandler mScanResultHandler = null;

    public class AdvertisingReport {
        int eventType;
        int addressType;
        String address;
        int primaryPhy;
        int secondaryPhy;
        int advertisingSid;
        int txPower;
        int rssi;
        int periodicAdvInt;
        byte[] advData;

        public AdvertisingReport(int eventType, int addressType, String address, int primaryPhy,
            int secondaryPhy, int advertisingSid, int txPower, int rssi, int periodicAdvInt,
            byte[] advData) {
            this.eventType = eventType;
            this.addressType = addressType;
            this.address = address;
            this.primaryPhy = primaryPhy;
            this.secondaryPhy = secondaryPhy;
            this.advertisingSid = advertisingSid;
            this.txPower = txPower;
            this.rssi = rssi;
            this.periodicAdvInt = periodicAdvInt;
            this.advData = advData;
        }
    };

    static {
        if (DBG) Log.d(TAG, "classInitNative called");
        System.loadLibrary("bluetooth_qti_jni");
        classInitNative();
    }

    @Override
    protected IProfileServiceBinder initBinder() {
        return new BluetoothGattBinder(this);
    }

    @Override
    protected boolean start() {
        if (DBG) {
            Log.d(TAG, "start()");
        }
        mExposureNotificationPackage = getString(R.string.exposure_notification_package);
        Settings.Global.putInt(
                getContentResolver(), "bluetooth_sanitized_exposure_notification_supported", 1);
        initializeNative();
        mNativeAvailable = true;
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        mAdapterService = AdapterService.getAdapterService();
        mBluetoothAdapterProxy = BluetoothAdapterProxy.getInstance();
        mCompanionManager = ICompanionDeviceManager.Stub.asInterface(
                ServiceManager.getService(Context.COMPANION_DEVICE_SERVICE));
        mAppOps = getSystemService(AppOpsManager.class);
        mAdvertiseManager = new AdvertiseManager(this, AdapterService.getAdapterService());
        mAdvertiseManager.start();

        mScanManager = new ScanManager(this, mAdapterService, mBluetoothAdapterProxy);
        mScanManager.start();

        mPeriodicScanManager = new PeriodicScanManager(AdapterService.getAdapterService());
        mPeriodicScanManager.start();

        HandlerThread thread = new HandlerThread("ScanResultHandler");
        thread.start();
        Looper looper = thread.getLooper();

        mScanResultHandler = new ScanResultHandler(looper);

        setGattService(this);
        return true;
    }

    @Override
    protected boolean stop() {
        if (DBG) {
            Log.d(TAG, "stop()");
        }

        if (mScanResultHandler != null) {
            // Perform cleanup
            mScanResultHandler.removeCallbacksAndMessages(null);
            Looper looper = mScanResultHandler.getLooper();
            if (looper != null) {
                Log.i(TAG, "Quit looper");
                looper.quit();
            }
            Log.i(TAG, "Remove Handler");
            mScanResultHandler = null;
        }

        setGattService(null);
        mScannerMap.clear();
        mClientMap.clear();
        mServerMap.clear();
        mHandleMap.clear();
        mReliableQueue.clear();
        if (mNativeAvailable) {
            mNativeAvailable = false;
            cleanupNative();
            if (mAdvertiseManager != null) {
                mAdvertiseManager.cleanup();
            }
            if (mScanManager != null) {
                mScanManager.cleanup();
            }
            if (mPeriodicScanManager != null) {
                mPeriodicScanManager.cleanup();
            }
        }
        return true;
    }

    @Override
    protected void cleanup() {
        if (DBG) {
            Log.d(TAG, "cleanup()");
        }

        if (mNativeAvailable) {
            mNativeAvailable = false;
            cleanupNative();
            if (mAdvertiseManager != null) {
                mAdvertiseManager.cleanup();
            }
            if (mScanManager != null) {
                mScanManager.cleanup();
            }
            if (mPeriodicScanManager != null) {
                mPeriodicScanManager.cleanup();
            }
        }
    }

    // While test mode is enabled, pretend as if the underlying stack
    // discovered a specific set of well-known beacons every second
    @Override
    protected void setTestModeEnabled(boolean enableTestMode) {
        synchronized (mTestModeLock) {
            if (mTestModeHandler == null) {
                mTestModeHandler = new Handler(getMainLooper()) {
                    public void handleMessage(Message msg) {
                        synchronized (mTestModeLock) {
                            if (!GattService.this.isTestModeEnabled()) {
                                return;
                            }
                            for (String test : TEST_MODE_BEACONS) {
                                onScanResultInternal(0x1b, 0x1, "DD:34:02:05:5C:4D", 1, 0, 0xff,
                                        127, -54, 0x0, HexDump.hexStringToByteArray(test));
                            }
                            sendEmptyMessageDelayed(0, DateUtils.SECOND_IN_MILLIS);
                        }
                    }
                };
            }
            if (enableTestMode && !isTestModeEnabled()) {
                super.setTestModeEnabled(true);
                mTestModeHandler.removeMessages(0);
                mTestModeHandler.sendEmptyMessageDelayed(0, DateUtils.SECOND_IN_MILLIS);
            } else if (!enableTestMode && isTestModeEnabled()) {
                super.setTestModeEnabled(false);
                mTestModeHandler.removeMessages(0);
                mTestModeHandler.sendEmptyMessage(0);
            }
        }
    }

    /**
     * Get the current instance of {@link GattService}
     *
     * @return current instance of {@link GattService}
     */
    @VisibleForTesting
    public static synchronized GattService getGattService() {
        if (sGattService == null) {
            Log.w(TAG, "getGattService(): service is null");
            return null;
        }
        if (!sGattService.isAvailable()) {
            Log.w(TAG, "getGattService(): service is not available");
            return null;
        }
        return sGattService;
    }

    @VisibleForTesting
    ScanManager getScanManager() {
        if (mScanManager == null) {
            Log.w(TAG, "getScanManager(): scan manager is null");
            return null;
        }
        return mScanManager;
    }

    private static synchronized void setGattService(GattService instance) {
        if (DBG) {
            Log.d(TAG, "setGattService(): set to: " + instance);
        }
        sGattService = instance;
    }

    // Suppressed because we are conditionally enforcing
    @SuppressLint("AndroidFrameworkRequiresPermission")
    private void permissionCheck(UUID characteristicUuid) {
        if (!isHidCharUuid(characteristicUuid)) {
            return;
        }
        enforceBluetoothPrivilegedPermission(this);
    }

    // Suppressed because we are conditionally enforcing
    @SuppressLint("AndroidFrameworkRequiresPermission")
    private void permissionCheck(int connId, int handle) {
        if (!isHandleRestricted(connId, handle)) {
            return;
        }
        enforceBluetoothPrivilegedPermission(this);
    }

    // Suppressed since we're not actually enforcing here
    @SuppressLint("AndroidFrameworkRequiresPermission")
    private void permissionCheck(ClientMap.App app, int connId, int handle) {
        if (!isHandleRestricted(connId, handle) || app.hasBluetoothPrivilegedPermission) {
            return;
        }
        enforceBluetoothPrivilegedPermission(this);
        app.hasBluetoothPrivilegedPermission = true;
    }

    private boolean isHandleRestricted(int connId, int handle) {
        Set<Integer> restrictedHandles = mRestrictedHandles.get(connId);
        return restrictedHandles != null && restrictedHandles.contains(handle);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (GattDebugUtils.handleDebugAction(this, intent)) {
            return Service.START_NOT_STICKY;
        }
        return super.onStartCommand(intent, flags, startId);
    }

    /**
     * DeathReceipient handlers used to unregister applications that
     * disconnect ungracefully (ie. crash or forced close).
     */

    class ScannerDeathRecipient implements IBinder.DeathRecipient {
        int mScannerId;

        ScannerDeathRecipient(int scannerId) {
            mScannerId = scannerId;
        }

        @Override
        public void binderDied() {
            if (DBG) {
                Log.d(TAG, "Binder is dead - unregistering scanner (" + mScannerId + ")!");
            }

            ScanClient client = getScanClient(mScannerId);
            if (client != null) {
                client.appDied = true;
                stopScan(client.scannerId, getAttributionSource());
            }
        }

        private ScanClient getScanClient(int clientIf) {
            for (ScanClient client : mScanManager.getRegularScanQueue()) {
                if (client.scannerId == clientIf) {
                    return client;
                }
            }
            for (ScanClient client : mScanManager.getBatchScanQueue()) {
                if (client.scannerId == clientIf) {
                    return client;
                }
            }
            for (ScanClient client : mScanManager.getPendingScanQueue()) {
                if (client.scannerId == clientIf) {
                    return client;
                }
            }
            return null;
        }
    }

    class ServerDeathRecipient implements IBinder.DeathRecipient {
        int mAppIf;

        ServerDeathRecipient(int appIf) {
            mAppIf = appIf;
        }

        @Override
        public void binderDied() {
            if (DBG) {
                Log.d(TAG, "Binder is dead - unregistering server (" + mAppIf + ")!");
            }
            unregisterServer(mAppIf, getAttributionSource());
        }
    }

    class ClientDeathRecipient implements IBinder.DeathRecipient {
        int mAppIf;

        ClientDeathRecipient(int appIf) {
            mAppIf = appIf;
        }

        @Override
        public void binderDied() {
            if (DBG) {
                Log.d(TAG, "Binder is dead - unregistering client (" + mAppIf + ")!");
            }
            unregisterClient(mAppIf, getAttributionSource());
        }
    }

    /**
     * Handlers for incoming service calls
     */
    private static class BluetoothGattBinder extends IBluetoothGatt.Stub
            implements IProfileServiceBinder {
        private GattService mService;

        BluetoothGattBinder(GattService svc) {
            mService = svc;
        }

        @Override
        public void cleanup() {
            mService = null;
        }

        private GattService getService() {
            if (mService != null && mService.isAvailable()) {
                return mService;
            }
            Log.e(TAG, "getService() - Service requested, but not available!");
            return null;
        }

        @Override
        public void getDevicesMatchingConnectionStates(int[] states,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                receiver.send(getDevicesMatchingConnectionStates(states, attributionSource));
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private List<BluetoothDevice> getDevicesMatchingConnectionStates(int[] states,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return new ArrayList<BluetoothDevice>();
            }
            return service.getDevicesMatchingConnectionStates(states, attributionSource);
        }

        @Override
        public void registerClient(ParcelUuid uuid, IBluetoothGattCallback callback,
                boolean eattSupport, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                registerClient(uuid, callback, eattSupport, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void registerClient(ParcelUuid uuid, IBluetoothGattCallback callback,
                boolean eatt_support, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.registerClient(uuid.getUuid(), callback, eatt_support, attributionSource);
        }

        @Override
        public void unregisterClient(int clientIf, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                unregisterClient(clientIf, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void unregisterClient(int clientIf, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.unregisterClient(clientIf, attributionSource);
        }

        @Override
        public void registerScanner(IScannerCallback callback, WorkSource workSource,
                AttributionSource attributionSource, SynchronousResultReceiver receiver)
                throws RemoteException {
            try {
                registerScanner(callback, workSource, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void registerScanner(IScannerCallback callback, WorkSource workSource,
                AttributionSource attributionSource) throws RemoteException {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.registerScanner(callback, workSource, attributionSource);
        }

        @Override
        public void unregisterScanner(int scannerId, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                unregisterScanner(scannerId, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void unregisterScanner(int scannerId, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.unregisterScanner(scannerId, attributionSource);
        }

        @Override
        public void startScan(int scannerId, ScanSettings settings, List<ScanFilter> filters,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                startScan(scannerId, settings, filters,
                        attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void startScan(int scannerId, ScanSettings settings, List<ScanFilter> filters,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.startScan(scannerId, settings, filters, attributionSource);
        }

        @Override
        public void startScanForIntent(PendingIntent intent, ScanSettings settings,
                List<ScanFilter> filters, AttributionSource attributionSource,
                SynchronousResultReceiver receiver)
                throws RemoteException {
            try {
                startScanForIntent(intent, settings,
                        filters, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void startScanForIntent(PendingIntent intent, ScanSettings settings,
                List<ScanFilter> filters, AttributionSource attributionSource)
                throws RemoteException {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.registerPiAndStartScan(intent, settings, filters, attributionSource);
        }

        @Override
        public void stopScanForIntent(PendingIntent intent, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) throws RemoteException {
            try {
                stopScanForIntent(intent, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void stopScanForIntent(PendingIntent intent, AttributionSource attributionSource)
                throws RemoteException {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.stopScan(intent, attributionSource);
        }

        @Override
        public void stopScan(int scannerId, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                stopScan(scannerId, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void stopScan(int scannerId, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.stopScan(scannerId, attributionSource);
        }

        @Override
        public void flushPendingBatchResults(int scannerId, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                flushPendingBatchResults(scannerId, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void flushPendingBatchResults(int scannerId, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.flushPendingBatchResults(scannerId, attributionSource);
        }

        @Override
        public void clientConnect(int clientIf, String address, boolean isDirect, int transport,
                boolean opportunistic, int phy, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                clientConnect(clientIf, address, isDirect, transport, opportunistic, phy,
                        attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void clientConnect(int clientIf, String address, boolean isDirect, int transport,
                boolean opportunistic, int phy, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.clientConnect(clientIf, address, isDirect, transport, opportunistic, phy,
                    attributionSource);
        }

        @Override
        public void clientDisconnect(int clientIf, String address,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                clientDisconnect(clientIf, address, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void clientDisconnect(int clientIf, String address,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.clientDisconnect(clientIf, address, attributionSource);
        }

        @Override
        public void clientSetPreferredPhy(int clientIf, String address, int txPhy, int rxPhy,
                int phyOptions, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                clientSetPreferredPhy(clientIf, address, txPhy, rxPhy, phyOptions,
                        attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void clientSetPreferredPhy(int clientIf, String address, int txPhy, int rxPhy,
                int phyOptions, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.clientSetPreferredPhy(clientIf, address, txPhy, rxPhy, phyOptions,
                    attributionSource);
        }

        @Override
        public void clientReadPhy(int clientIf, String address,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                clientReadPhy(clientIf, address, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void clientReadPhy(int clientIf, String address,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.clientReadPhy(clientIf, address, attributionSource);
        }

        @Override
        public void refreshDevice(int clientIf, String address,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                refreshDevice(clientIf, address, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void refreshDevice(int clientIf, String address,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.refreshDevice(clientIf, address, attributionSource);
        }

        @Override
        public void discoverServices(int clientIf, String address,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                discoverServices(clientIf, address, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void discoverServices(int clientIf, String address,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.discoverServices(clientIf, address, attributionSource);
        }

        @Override
        public void discoverServiceByUuid(int clientIf, String address, ParcelUuid uuid,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                discoverServiceByUuid(clientIf, address, uuid, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void discoverServiceByUuid(int clientIf, String address, ParcelUuid uuid,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.discoverServiceByUuid(clientIf, address, uuid.getUuid(), attributionSource);
        }

        @Override
        public void readCharacteristic(int clientIf, String address, int handle, int authReq,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                readCharacteristic(clientIf, address, handle, authReq, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void readCharacteristic(int clientIf, String address, int handle, int authReq,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.readCharacteristic(clientIf, address, handle, authReq, attributionSource);
        }

        @Override
        public void readUsingCharacteristicUuid(int clientIf, String address, ParcelUuid uuid,
                int startHandle, int endHandle, int authReq, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                readUsingCharacteristicUuid(clientIf, address, uuid, startHandle, endHandle,
                        authReq, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void readUsingCharacteristicUuid(int clientIf, String address, ParcelUuid uuid,
                int startHandle, int endHandle, int authReq, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.readUsingCharacteristicUuid(clientIf, address, uuid.getUuid(), startHandle,
                    endHandle, authReq, attributionSource);
        }

        @Override
        public void writeCharacteristic(int clientIf, String address, int handle, int writeType,
                int authReq, byte[] value, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                receiver.send(writeCharacteristic(clientIf, address, handle, writeType, authReq,
                            value, attributionSource));
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private int writeCharacteristic(int clientIf, String address, int handle, int writeType,
                int authReq, byte[] value, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return BluetoothStatusCodes.ERROR_PROFILE_SERVICE_NOT_BOUND;
            }
            return service.writeCharacteristic(clientIf, address, handle, writeType, authReq, value,
                    attributionSource);
        }

        @Override
        public void readDescriptor(int clientIf, String address, int handle, int authReq,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                readDescriptor(clientIf, address, handle, authReq, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void readDescriptor(int clientIf, String address, int handle, int authReq,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.readDescriptor(clientIf, address, handle, authReq, attributionSource);
        }

        @Override
        public void writeDescriptor(int clientIf, String address, int handle, int authReq,
                byte[] value, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                receiver.send(writeDescriptor(clientIf, address, handle, authReq, value,
                            attributionSource));
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private int writeDescriptor(int clientIf, String address, int handle, int authReq,
                byte[] value, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return BluetoothStatusCodes.ERROR_PROFILE_SERVICE_NOT_BOUND;
            }
            return service.writeDescriptor(clientIf, address, handle, authReq, value,
                    attributionSource);
        }

        @Override
        public void beginReliableWrite(int clientIf, String address,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                beginReliableWrite(clientIf, address, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void beginReliableWrite(int clientIf, String address,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.beginReliableWrite(clientIf, address, attributionSource);
        }

        @Override
        public void endReliableWrite(int clientIf, String address, boolean execute,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                endReliableWrite(clientIf, address, execute, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void endReliableWrite(int clientIf, String address, boolean execute,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.endReliableWrite(clientIf, address, execute, attributionSource);
        }

        @Override
        public void registerForNotification(int clientIf, String address, int handle,
                boolean enable, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                registerForNotification(clientIf, address, handle, enable, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void registerForNotification(int clientIf, String address, int handle,
                boolean enable, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.registerForNotification(clientIf, address, handle, enable, attributionSource);
        }

        @Override
        public void readRemoteRssi(int clientIf, String address,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                readRemoteRssi(clientIf, address, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void readRemoteRssi(int clientIf, String address,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.readRemoteRssi(clientIf, address, attributionSource);
        }

        @Override
        public void configureMTU(int clientIf, String address, int mtu,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                configureMTU(clientIf, address, mtu, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void configureMTU(int clientIf, String address, int mtu,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.configureMTU(clientIf, address, mtu, attributionSource);
        }

        @Override
        public void connectionParameterUpdate(int clientIf, String address,
                int connectionPriority, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                connectionParameterUpdate(clientIf, address, connectionPriority, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void connectionParameterUpdate(int clientIf, String address,
                int connectionPriority, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.connectionParameterUpdate(
                    clientIf, address, connectionPriority, attributionSource);
        }

        @Override
        public void leConnectionUpdate(int clientIf, String address,
                int minConnectionInterval, int maxConnectionInterval,
                int peripheralLatency, int supervisionTimeout,
                int minConnectionEventLen, int maxConnectionEventLen,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                leConnectionUpdate(clientIf, address, minConnectionInterval, maxConnectionInterval,
                        peripheralLatency, supervisionTimeout, minConnectionEventLen,
                        maxConnectionEventLen, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void leConnectionUpdate(int clientIf, String address,
                int minConnectionInterval, int maxConnectionInterval,
                int peripheralLatency, int supervisionTimeout,
                int minConnectionEventLen, int maxConnectionEventLen,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.leConnectionUpdate(clientIf, address, minConnectionInterval,
                                       maxConnectionInterval, peripheralLatency,
                                       supervisionTimeout, minConnectionEventLen,
                                       maxConnectionEventLen, attributionSource);
        }

        @Override
        public void subrateModeRequest(int clientIf, String address,
                int subrateMode, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.subrateModeRequest(clientIf, address, subrateMode,
                                       attributionSource);
        }

        @Override
        public void leSubrateRequest(int clientIf, String address,
                int subrateMin, int subrateMax, int maxLatency,
                int contNumber, int supervisionTimeout,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.leSubrateRequest(clientIf, address, subrateMin, subrateMax, maxLatency,
                                     contNumber, supervisionTimeout, attributionSource);
        }

        @Override
        public void registerServer(ParcelUuid uuid, IBluetoothGattServerCallback callback,
                boolean eattSupport, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                registerServer(uuid, callback, eattSupport, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void registerServer(ParcelUuid uuid, IBluetoothGattServerCallback callback,
                boolean eatt_support, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.registerServer(uuid.getUuid(), callback, eatt_support, attributionSource);
        }

        @Override
        public void unregisterServer(int serverIf, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                unregisterServer(serverIf, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void unregisterServer(int serverIf, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.unregisterServer(serverIf, attributionSource);
        }

        @Override
        public void serverConnect(int serverIf, String address, boolean isDirect, int transport,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                serverConnect(serverIf, address, isDirect, transport, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void serverConnect(int serverIf, String address, boolean isDirect, int transport,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.serverConnect(serverIf, address, isDirect, transport, attributionSource);
        }

        @Override
        public void serverDisconnect(int serverIf, String address,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                serverDisconnect(serverIf, address, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void serverDisconnect(int serverIf, String address,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.serverDisconnect(serverIf, address, attributionSource);
        }

        @Override
        public void serverSetPreferredPhy(int serverIf, String address, int txPhy, int rxPhy,
                int phyOptions, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                serverSetPreferredPhy(serverIf, address, txPhy, rxPhy, phyOptions,
                        attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void serverSetPreferredPhy(int serverIf, String address, int txPhy, int rxPhy,
                int phyOptions, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.serverSetPreferredPhy(
                    serverIf, address, txPhy, rxPhy, phyOptions, attributionSource);
        }

        @Override
        public void serverReadPhy(int clientIf, String address, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                serverReadPhy(clientIf, address, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void serverReadPhy(int clientIf, String address,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.serverReadPhy(clientIf, address, attributionSource);
        }

        @Override
        public void addService(int serverIf, BluetoothGattService svc,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                addService(serverIf, svc, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void addService(int serverIf, BluetoothGattService svc,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }

            service.addService(serverIf, svc, attributionSource);
        }

        @Override
        public void removeService(int serverIf, int handle, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                removeService(serverIf, handle, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void removeService(int serverIf, int handle, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.removeService(serverIf, handle, attributionSource);
        }

        @Override
        public void clearServices(int serverIf, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                clearServices(serverIf, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void clearServices(int serverIf, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.clearServices(serverIf, attributionSource);
        }

        @Override
        public void sendResponse(int serverIf, String address, int requestId, int status,
                int offset, byte[] value, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                sendResponse(serverIf, address, requestId, status, offset, value,
                        attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void sendResponse(int serverIf, String address, int requestId, int status,
                int offset, byte[] value, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.sendResponse(
                    serverIf, address, requestId, status, offset, value, attributionSource);
        }

        @Override
        public void sendNotification(int serverIf, String address, int handle, boolean confirm,
                byte[] value, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                receiver.send(sendNotification(serverIf, address, handle, confirm, value,
                            attributionSource));
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private int sendNotification(int serverIf, String address, int handle, boolean confirm,
                byte[] value, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return BluetoothStatusCodes.ERROR_PROFILE_SERVICE_NOT_BOUND;
            }
            return service.sendNotification(serverIf, address, handle, confirm, value,
                attributionSource);
        }

        @Override
        public void startAdvertisingSet(AdvertisingSetParameters parameters,
                AdvertiseData advertiseData, AdvertiseData scanResponse,
                PeriodicAdvertisingParameters periodicParameters, AdvertiseData periodicData,
                int duration, int maxExtAdvEvents, IAdvertisingSetCallback callback,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                startAdvertisingSet(parameters, advertiseData, scanResponse, periodicParameters,
                        periodicData, duration, maxExtAdvEvents, callback, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void startAdvertisingSet(AdvertisingSetParameters parameters,
                AdvertiseData advertiseData, AdvertiseData scanResponse,
                PeriodicAdvertisingParameters periodicParameters, AdvertiseData periodicData,
                int duration, int maxExtAdvEvents, IAdvertisingSetCallback callback,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.startAdvertisingSet(parameters, advertiseData, scanResponse, periodicParameters,
                    periodicData, duration, maxExtAdvEvents, callback, attributionSource);
        }

        @Override
        public void stopAdvertisingSet(IAdvertisingSetCallback callback,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                stopAdvertisingSet(callback, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void stopAdvertisingSet(IAdvertisingSetCallback callback,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.stopAdvertisingSet(callback, attributionSource);
        }

        @Override
        public void getOwnAddress(int advertiserId, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                getOwnAddress(advertiserId, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void getOwnAddress(int advertiserId, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.getOwnAddress(advertiserId, attributionSource);
        }

        @Override
        public void enableAdvertisingSet(int advertiserId, boolean enable, int duration,
                int maxExtAdvEvents, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                enableAdvertisingSet(advertiserId, enable, duration, maxExtAdvEvents,
                        attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void enableAdvertisingSet(int advertiserId, boolean enable, int duration,
                int maxExtAdvEvents, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.enableAdvertisingSet(
                    advertiserId, enable, duration, maxExtAdvEvents, attributionSource);
        }

        @Override
        public void setAdvertisingData(int advertiserId, AdvertiseData data,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                setAdvertisingData(advertiserId, data, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void setAdvertisingData(int advertiserId, AdvertiseData data,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.setAdvertisingData(advertiserId, data, attributionSource);
        }

        @Override
        public void setScanResponseData(int advertiserId, AdvertiseData data,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                setScanResponseData(advertiserId, data, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void setScanResponseData(int advertiserId, AdvertiseData data,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.setScanResponseData(advertiserId, data, attributionSource);
        }

        @Override
        public void setAdvertisingParameters(int advertiserId,
                AdvertisingSetParameters parameters, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                setAdvertisingParameters(advertiserId, parameters, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void setAdvertisingParameters(int advertiserId,
                AdvertisingSetParameters parameters, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.setAdvertisingParameters(advertiserId, parameters, attributionSource);
        }

        @Override
        public void setPeriodicAdvertisingParameters(int advertiserId,
                PeriodicAdvertisingParameters parameters, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                setPeriodicAdvertisingParameters(advertiserId, parameters, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void setPeriodicAdvertisingParameters(int advertiserId,
                PeriodicAdvertisingParameters parameters, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.setPeriodicAdvertisingParameters(advertiserId, parameters, attributionSource);
        }

        @Override
        public void setPeriodicAdvertisingData(int advertiserId, AdvertiseData data,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                setPeriodicAdvertisingData(advertiserId, data, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void setPeriodicAdvertisingData(int advertiserId, AdvertiseData data,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.setPeriodicAdvertisingData(advertiserId, data, attributionSource);
        }

        @Override
        public void setPeriodicAdvertisingEnable(int advertiserId, boolean enable,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                setPeriodicAdvertisingEnable(advertiserId, enable, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void setPeriodicAdvertisingEnable(int advertiserId, boolean enable,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.setPeriodicAdvertisingEnable(advertiserId, enable, attributionSource);
        }

        @Override
        public void registerSync(ScanResult scanResult, int skip, int timeout,
                IPeriodicAdvertisingCallback callback, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                registerSync(scanResult, skip, timeout, callback, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void registerSync(ScanResult scanResult, int skip, int timeout,
                IPeriodicAdvertisingCallback callback, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.registerSync(scanResult, skip, timeout, callback, attributionSource);
        }

        @Override
        public void transferSync(BluetoothDevice bda, int serviceData , int syncHandle,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                transferSync(bda, serviceData , syncHandle, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        public void transferSync(BluetoothDevice bda, int serviceData , int syncHandle,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.transferSync(bda, serviceData , syncHandle, attributionSource);
        }

        @Override
        public void transferSetInfo(BluetoothDevice bda, int serviceData , int advHandle,
                IPeriodicAdvertisingCallback callback, AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                transferSetInfo(bda, serviceData , advHandle, callback, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        public void transferSetInfo(BluetoothDevice bda, int serviceData , int advHandle,
                IPeriodicAdvertisingCallback callback, AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.transferSetInfo(bda, serviceData , advHandle, callback, attributionSource);
        }

        @Override
        public void unregisterSync(IPeriodicAdvertisingCallback callback,
                AttributionSource attributionSource, SynchronousResultReceiver receiver) {
            try {
                unregisterSync(callback, attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        public void unregisterSync(IPeriodicAdvertisingCallback callback,
                AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.unregisterSync(callback, attributionSource);
        }

        @Override
        public void disconnectAll(AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                disconnectAll(attributionSource);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void disconnectAll(AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.disconnectAll(attributionSource);
        }

        @Override
        public void unregAll(AttributionSource source, SynchronousResultReceiver receiver) {
            try {
                unregAll(source);
                receiver.send(null);
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private void unregAll(AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return;
            }
            service.unregAll(attributionSource);
        }

        @Override
        public void numHwTrackFiltersAvailable(AttributionSource attributionSource,
                SynchronousResultReceiver receiver) {
            try {
                receiver.send(numHwTrackFiltersAvailable(attributionSource));
            } catch (RuntimeException e) {
                receiver.propagateException(e);
            }
        }
        private int numHwTrackFiltersAvailable(AttributionSource attributionSource) {
            GattService service = getService();
            if (service == null) {
                return 0;
            }
            return service.numHwTrackFiltersAvailable(attributionSource);
        }
    }

    ;

    /**************************************************************************
     * Callback functions - CLIENT
     *************************************************************************/

    // EN format defined here:
    // https://blog.google/documents/70/Exposure_Notification_-_Bluetooth_Specification_v1.2.2.pdf
    private static final byte[] EXPOSURE_NOTIFICATION_FLAGS_PREAMBLE = new byte[] {
        // size 2, flag field, flags byte (value is not important)
        (byte) 0x02, (byte) 0x01
    };
    private static final int EXPOSURE_NOTIFICATION_FLAGS_LENGTH = 0x2 + 1;
    private static final byte[] EXPOSURE_NOTIFICATION_PAYLOAD_PREAMBLE = new byte[] {
        // size 3, complete 16 bit UUID, EN UUID
        (byte) 0x03, (byte) 0x03, (byte) 0x6F, (byte) 0xFD,
        // size 23, data for 16 bit UUID, EN UUID
        (byte) 0x17, (byte) 0x16, (byte) 0x6F, (byte) 0xFD,
        // ...payload
    };
    private static final int EXPOSURE_NOTIFICATION_PAYLOAD_LENGTH = 0x03 + 0x17 + 2;

    private static boolean arrayStartsWith(byte[] array, byte[] prefix) {
        if (array.length < prefix.length) {
            return false;
        }
        for (int i = 0; i < prefix.length; i++) {
            if (prefix[i] != array[i]) {
                return false;
            }
        }
        return true;
    }

    ScanResult getSanitizedExposureNotification(ScanResult result) {
        ScanRecord record = result.getScanRecord();
        // Remove the flags part of the payload, if present
        if (record.getBytes().length > EXPOSURE_NOTIFICATION_FLAGS_LENGTH
                && arrayStartsWith(record.getBytes(), EXPOSURE_NOTIFICATION_FLAGS_PREAMBLE)) {
            record = ScanRecord.parseFromBytes(
                    Arrays.copyOfRange(
                            record.getBytes(),
                            EXPOSURE_NOTIFICATION_FLAGS_LENGTH,
                            record.getBytes().length));
        }

        if (record.getBytes().length != EXPOSURE_NOTIFICATION_PAYLOAD_LENGTH) {
            return null;
        }
        if (!arrayStartsWith(record.getBytes(), EXPOSURE_NOTIFICATION_PAYLOAD_PREAMBLE)) {
            return null;
        }

        return new ScanResult(null, 0, 0, 0, 0, 0, result.getRssi(), 0, record, 0);
    }

    void HandleScanResult(int eventType, int addressType, String address, int primaryPhy,
            int secondaryPhy, int advertisingSid, int txPower, int rssi, int periodicAdvInt,
            byte[] advData) {
        // When in testing mode, ignore all real-world events
        if (isTestModeEnabled()) return;
        onScanResultInternal(eventType, addressType, address, primaryPhy, secondaryPhy,
                advertisingSid, txPower, rssi, periodicAdvInt, advData);
    }

    void onScanResultInternal(int eventType, int addressType, String address, int primaryPhy,
            int secondaryPhy, int advertisingSid, int txPower, int rssi, int periodicAdvInt,
            byte[] advData) {
        if (VDBG) {
            Log.d(TAG, "onScanResultInternal() - eventType=0x" + Integer.toHexString(eventType)
                    + ", addressType=" + addressType + ", address=" + address + ", primaryPhy="
                    + primaryPhy + ", secondaryPhy=" + secondaryPhy + ", advertisingSid=0x"
                    + Integer.toHexString(advertisingSid) + ", txPower=" + txPower + ", rssi="
                    + rssi + ", periodicAdvInt=0x" + Integer.toHexString(periodicAdvInt));
        }

        byte[] legacyAdvData = Arrays.copyOfRange(advData, 0, 62);

        for (ScanClient client : mScanManager.getRegularScanQueue()) {
            ScannerMap.App app = mScannerMap.getById(client.scannerId);
            if (app == null) {
                continue;
            }

            BluetoothDevice device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(address);

            ScanSettings settings = client.settings;
            byte[] scanRecordData;
            // This is for compability with applications that assume fixed size scan data.
            if (settings.getLegacy()) {
                if ((eventType & ET_LEGACY_MASK) == 0) {
                    // If this is legacy scan, but nonlegacy result - skip.
                    continue;
                } else {
                    // Some apps are used to fixed-size advertise data.
                    scanRecordData = legacyAdvData;
                }
            } else {
                scanRecordData = advData;
            }

            ScanResult result = null;
            ScanRecord record = ScanRecord.parseFromBytes(scanRecordData);
            Map<ParcelUuid, byte[]> listOfUuids = record.getServiceData();
            boolean isPrevClient = false;
            String BS_ID = null;
            AdapterService adapterService = AdapterService.getAdapterService();
            if (adapterService != null) {
                BS_ID = adapterService.getBSId();
            }
            if (listOfUuids != null && BS_ID != null) {
                 isPrevClient = listOfUuids.containsKey(ParcelUuid.fromString(BS_ID));
            }

            if (client.allowAddressTypeInResults && isPrevClient) {
                Log.d(TAG, "populate AddressType for previleged client");
                result =
                    new ScanResult(device, addressType, eventType, primaryPhy, secondaryPhy, advertisingSid,
                            txPower, rssi, periodicAdvInt,
                            record,
                            SystemClock.elapsedRealtimeNanos());
            } else {
                result = new ScanResult(device, eventType, primaryPhy, secondaryPhy, advertisingSid,
                            txPower, rssi, periodicAdvInt,
                            record,
                            SystemClock.elapsedRealtimeNanos());
            }
            if (client.hasDisavowedLocation) {
                if (mLocationDenylistPredicate.test(result)) {
                    continue;
                }
            }
            boolean hasPermission = hasScanResultPermission(client);
            if (!hasPermission) {
                for (String associatedDevice : client.associatedDevices) {
                    if (associatedDevice.equalsIgnoreCase(address)) {
                        hasPermission = true;
                        break;
                    }
                }
            }


            if (!hasPermission && client.eligibleForSanitizedExposureNotification) {
                ScanResult sanitized = getSanitizedExposureNotification(result);
                if (sanitized != null) {
                    hasPermission = true;
                    result = sanitized;
                }
            }

            if (!hasPermission && client.callingPackage != null
                               && client.callingPackage.equals("com.android.bluetooth")) {
                hasPermission = true;
            }

            if (!hasPermission || !matchesFilters(client, result)) {
                continue;
            }

            if ((settings.getCallbackType() & ScanSettings.CALLBACK_TYPE_ALL_MATCHES) == 0) {
                continue;
            }

            try {
                app.appScanStats.addResult(client.scannerId);
                if (app.callback != null) {
                    app.callback.onScanResult(result);
                } else {
                    // Send the PendingIntent
                    ArrayList<ScanResult> results = new ArrayList<>();
                    results.add(result);
                    sendResultsByPendingIntent(app.info, results,
                            ScanSettings.CALLBACK_TYPE_ALL_MATCHES);
                }
            } catch (RemoteException | PendingIntent.CanceledException e) {
                Log.e(TAG, "Exception: " + e);
                mScannerMap.remove(client.scannerId);
                mScanManager.stopScan(client.scannerId);
            }
        }
    }

    public final class ScanResultHandler extends Handler {
        private static final String TAG = "ScanResultHandler";
        public static final int MSG_BLE_ADVERTISING_REPORT = 1;

        private ScanResultHandler(Looper looper) {
            super(looper);
            Log.i(TAG, "ScanResultHandler ");
        }

        @Override
        public void handleMessage(Message msg) {
            if (VDBG) Log.v(TAG, "Handler(): got msg=" + msg.what);

            if (msg.what == MSG_BLE_ADVERTISING_REPORT) {
                AdvertisingReport advReport = (AdvertisingReport) msg.obj;

                if (advReport != null) {
                    HandleScanResult(advReport.eventType, advReport.addressType,
                                     advReport.address, advReport.primaryPhy,
                                     advReport.secondaryPhy, advReport.advertisingSid,
                                     advReport.txPower, advReport.rssi,
                                     advReport.periodicAdvInt, advReport.advData);
                }
            }
        }
    }

    void onScanResult(int eventType, int addressType, String address, int primaryPhy,
            int secondaryPhy, int advertisingSid, int txPower, int rssi, int periodicAdvInt,
            byte[] advData) {
        if (VDBG) {
            Log.d(TAG, "onScanResult() - eventType=0x" + Integer.toHexString(eventType)
                    + ", addressType=" + addressType + ", address=" + address + ", primaryPhy="
                    + primaryPhy + ", secondaryPhy=" + secondaryPhy + ", advertisingSid=0x"
                    + Integer.toHexString(advertisingSid) + ", txPower=" + txPower + ", rssi="
                    + rssi + ", periodicAdvInt=0x" + Integer.toHexString(periodicAdvInt));
        }
        // When in testing mode, ignore all real-world events
        if (isTestModeEnabled()) {
            return;
        }
        if (mScanResultHandler == null) {
            Log.d(TAG, "onScanResult: mScanResultHandler is null.");
            return;
        }

        AdvertisingReport advReport = new AdvertisingReport(eventType, addressType, address,
                           primaryPhy, secondaryPhy, advertisingSid, txPower, rssi,
                           periodicAdvInt, advData);
        Message message = new Message();
        message.what = ScanResultHandler.MSG_BLE_ADVERTISING_REPORT;
        message.obj = advReport;
        mScanResultHandler.sendMessage(message);
    }

    private void sendResultByPendingIntent(PendingIntentInfo pii, ScanResult result,
            int callbackType, ScanClient client) {
        ArrayList<ScanResult> results = new ArrayList<>();
        results.add(result);
        try {
            sendResultsByPendingIntent(pii, results, callbackType);
        } catch (PendingIntent.CanceledException e) {
            final long token = Binder.clearCallingIdentity();
            try {
                stopScan(client.scannerId, getAttributionSource());
                unregisterScanner(client.scannerId, getAttributionSource());
            } finally {
                Binder.restoreCallingIdentity(token);
            }
        }
    }

    private void sendResultsByPendingIntent(PendingIntentInfo pii, ArrayList<ScanResult> results,
            int callbackType) throws PendingIntent.CanceledException {
        Intent extrasIntent = new Intent();
        extrasIntent.putParcelableArrayListExtra(BluetoothLeScanner.EXTRA_LIST_SCAN_RESULT,
                results);
        extrasIntent.putExtra(BluetoothLeScanner.EXTRA_CALLBACK_TYPE, callbackType);
        pii.intent.send(this, 0, extrasIntent);
    }

    private void sendErrorByPendingIntent(PendingIntentInfo pii, int errorCode)
            throws PendingIntent.CanceledException {
        Intent extrasIntent = new Intent();
        extrasIntent.putExtra(BluetoothLeScanner.EXTRA_ERROR_CODE, errorCode);
        pii.intent.send(this, 0, extrasIntent);
    }

    void onScannerRegistered(int status, int scannerId, long uuidLsb, long uuidMsb)
            throws RemoteException {
        UUID uuid = new UUID(uuidMsb, uuidLsb);
        if (DBG) {
            Log.d(TAG, "onScannerRegistered() - UUID=" + uuid + ", scannerId=" + scannerId
                    + ", status=" + status);
        }

        // First check the callback map
        ScannerMap.App cbApp = mScannerMap.getByUuid(uuid);
        if (cbApp != null) {
            if (status == 0) {
                cbApp.id = scannerId;
                // If app is callback based, setup a death recipient. App will initiate the start.
                // Otherwise, if PendingIntent based, start the scan directly.
                if (cbApp.callback != null) {
                    cbApp.linkToDeath(new ScannerDeathRecipient(scannerId));
                } else {
                    continuePiStartScan(scannerId, cbApp);
                }
            } else {
                mScannerMap.remove(scannerId);
            }
            if (cbApp.callback != null) {
                cbApp.callback.onScannerRegistered(status, scannerId);
            }
        }
    }

    /** Determines if the given scan client has the appropriate permissions to receive callbacks. */
    private boolean hasScanResultPermission(final ScanClient client) {
        if (client.hasNetworkSettingsPermission
                || client.hasNetworkSetupWizardPermission
                || client.hasScanWithoutLocationPermission) {
            return true;
        }
        if (client.hasDisavowedLocation) {
            return true;
        }
        return client.hasLocationPermission && !Utils.blockedByLocationOff(this, client.userHandle);
    }

    // Check if a scan record matches a specific filters.
    private boolean matchesFilters(ScanClient client, ScanResult scanResult) {
        if (client.filters == null || client.filters.isEmpty()) {
            return true;
        }
        for (ScanFilter filter : client.filters) {
            if (filter.matches(scanResult)) {
                return true;
            }
        }
        return false;
    }

    void onClientRegistered(int status, int clientIf, long uuidLsb, long uuidMsb)
            throws RemoteException {
        UUID uuid = new UUID(uuidMsb, uuidLsb);
        if (DBG) {
            Log.d(TAG, "onClientRegistered() - UUID=" + uuid + ", clientIf=" + clientIf);
        }
        ClientMap.App app = mClientMap.getByUuid(uuid);
        if (app != null) {
            if (status == 0) {
                app.id = clientIf;
                app.linkToDeath(new ClientDeathRecipient(clientIf));
            } else {
                mClientMap.remove(uuid);
            }
            app.callback.onClientRegistered(status, clientIf);
        }
    }

    void onConnected(int clientIf, int connId, int status, String address) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onConnected() - clientIf=" + clientIf + ", connId=" + connId + ", address="
                    + address);
        }

        if (status == 0) {
            mClientMap.addConnection(clientIf, connId, address);

            // Allow one writeCharacteristic operation at a time for each connected remote device.
            synchronized (mPermits) {
                Log.d(TAG, "onConnected() - adding permit for address="
                    + address);
                mPermits.putIfAbsent(address, -1);
            }
        }
        ClientMap.App app = mClientMap.getById(clientIf);
        if (app != null) {
            app.callback.onClientConnectionState(status, clientIf,
                    (status == BluetoothGatt.GATT_SUCCESS), address);
        }
    }

    void onDisconnected(int clientIf, int connId, int status, String address)
            throws RemoteException {
        if (DBG) {
            Log.d(TAG,
                    "onDisconnected() - clientIf=" + clientIf + ", connId=" + connId + ", address="
                            + address);
        }

        mClientMap.removeConnection(clientIf, connId);
        ClientMap.App app = mClientMap.getById(clientIf);

        // Remove AtomicBoolean representing permit if no other connections rely on this remote device.
        if (!mClientMap.getConnectedDevices().contains(address)) {
            synchronized (mPermits) {
                Log.d(TAG, "onDisconnected() - removing permit for address="
                    + address);
                mPermits.remove(address);
            }
        } else {
            synchronized (mPermits) {
                if (mPermits.get(address) == connId) {
                    Log.d(TAG, "onDisconnected() - set permit -1 for address=" + address);
                    mPermits.put(address, -1);
                }
            }
        }

        if (app != null) {
            app.callback.onClientConnectionState(status, clientIf, false, address);
        }
    }

    void onClientPhyUpdate(int connId, int txPhy, int rxPhy, int status) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onClientPhyUpdate() - connId=" + connId + ", status=" + status);
        }

        String address = mClientMap.addressByConnId(connId);
        if (address == null) {
            return;
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app == null) {
            return;
        }

        app.callback.onPhyUpdate(address, txPhy, rxPhy, status);
    }

    void onClientPhyRead(int clientIf, String address, int txPhy, int rxPhy, int status)
            throws RemoteException {
        if (DBG) {
            Log.d(TAG,
                    "onClientPhyRead() - address=" + address + ", status=" + status + ", clientIf="
                            + clientIf);
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId == null) {
            Log.d(TAG, "onClientPhyRead() - no connection to " + address);
            return;
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app == null) {
            return;
        }

        app.callback.onPhyRead(address, txPhy, rxPhy, status);
    }

    void onClientConnUpdate(int connId, int interval, int latency, int timeout, int status)
            throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onClientConnUpdate() - connId=" + connId + ", status=" + status);
        }

        String address = mClientMap.addressByConnId(connId);
        if (address == null) {
            return;
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app == null) {
            return;
        }

        app.callback.onConnectionUpdated(address, interval, latency, timeout, status);
    }

    void onServiceChanged(int connId) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onServiceChanged - connId=" + connId);
        }

        String address = mClientMap.addressByConnId(connId);
        if (address == null) {
            return;
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app == null) {
            return;
        }

        app.callback.onServiceChanged(address);
    }

    void onClientSubrateChange(int connId, int subrateFactor, int latency, int contNum,
            int timeout, int status)
            throws RemoteException {
        Log.d(TAG, "onClientSubrateChange() - connId=" + connId + ", status=" + status);

        String address = mClientMap.addressByConnId(connId);
        if (address == null) {
            return;
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app == null) {
            return;
        }

        app.callback.onSubrateChange(address, subrateFactor, latency, contNum, timeout, status);
    }

    void onServerPhyUpdate(int connId, int txPhy, int rxPhy, int status) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onServerPhyUpdate() - connId=" + connId + ", status=" + status);
        }

        String address = mServerMap.addressByConnId(connId);
        if (address == null) {
            return;
        }

        ServerMap.App app = mServerMap.getByConnId(connId);
        if (app == null) {
            return;
        }

        app.callback.onPhyUpdate(address, txPhy, rxPhy, status);
    }

    void onServerPhyRead(int serverIf, String address, int txPhy, int rxPhy, int status)
            throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onServerPhyRead() - address=" + address + ", status=" + status);
        }

        Integer connId = mServerMap.connIdByAddress(serverIf, address);
        if (connId == null) {
            Log.d(TAG, "onServerPhyRead() - no connection to " + address);
            return;
        }

        ServerMap.App app = mServerMap.getByConnId(connId);
        if (app == null) {
            return;
        }

        app.callback.onPhyRead(address, txPhy, rxPhy, status);
    }

    void onServerConnUpdate(int connId, int interval, int latency, int timeout, int status)
            throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onServerConnUpdate() - connId=" + connId + ", status=" + status);
        }

        String address = mServerMap.addressByConnId(connId);
        if (address == null) {
            return;
        }

        ServerMap.App app = mServerMap.getByConnId(connId);
        if (app == null) {
            return;
        }

        app.callback.onConnectionUpdated(address, interval, latency, timeout, status);
    }

    void onServerSubrateChange(int connId, int subrateFactor, int latency, int contNum,
            int timeout, int status)
            throws RemoteException {
        Log.d(TAG, "onServerSubrateChange() - connId=" + connId + ", status=" + status);

        String address = mServerMap.addressByConnId(connId);
        if (address == null) {
            return;
        }

        ServerMap.App app = mServerMap.getByConnId(connId);
        if (app == null) {
            return;
        }

        app.callback.onSubrateChange(address, subrateFactor, latency, contNum, timeout, status);
    }


    void onSearchCompleted(int connId, int status) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onSearchCompleted() - connId=" + connId + ", status=" + status);
        }
        // Gatt DB is ready!

        // This callback was called from the jni_workqueue thread. If we make request to the stack
        // on the same thread, it might cause deadlock. Schedule request on a new thread instead.
        Thread t = new Thread(new Runnable() {
            @Override
            public void run() {
                gattClientGetGattDbNative(connId);
            }
        });
        t.start();
    }

    GattDbElement getSampleGattDbElement() {
        return new GattDbElement();
    }

    void onGetGattDb(int connId, ArrayList<GattDbElement> db) throws RemoteException {
        String address = mClientMap.addressByConnId(connId);

        if (DBG) {
            Log.d(TAG, "onGetGattDb() - address=" + address);
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app == null || app.callback == null) {
            Log.e(TAG, "app or callback is null");
            return;
        }

        List<BluetoothGattService> dbOut = new ArrayList<BluetoothGattService>();
        Set<Integer> restrictedIds = new HashSet<>();

        BluetoothGattService currSrvc = null;
        BluetoothGattCharacteristic currChar = null;
        boolean isRestrictedSrvc = false;
        boolean isHidSrvc = false;
        boolean isRestrictedChar = false;

        for (GattDbElement el : db) {
            switch (el.type) {
                case GattDbElement.TYPE_PRIMARY_SERVICE:
                case GattDbElement.TYPE_SECONDARY_SERVICE:
                    if (DBG) {
                        Log.d(TAG, "got service with UUID=" + el.uuid + " id: " + el.id);
                    }

                    currSrvc = new BluetoothGattService(el.uuid, el.id, el.type);
                    dbOut.add(currSrvc);
                    isRestrictedSrvc =
                            isFidoSrvcUuid(el.uuid) || isAndroidTvRemoteSrvcUuid(el.uuid);
                    isHidSrvc = isHidSrvcUuid(el.uuid);
                    if (isRestrictedSrvc) {
                        restrictedIds.add(el.id);
                    }
                    break;

                case GattDbElement.TYPE_CHARACTERISTIC:
                    if (DBG) {
                        Log.d(TAG, "got characteristic with UUID=" + el.uuid + " id: " + el.id);
                    }

                    currChar = new BluetoothGattCharacteristic(el.uuid, el.id, el.properties, 0);
                    currSrvc.addCharacteristic(currChar);
                    isRestrictedChar = isRestrictedSrvc || (isHidSrvc && isHidCharUuid(el.uuid));
                    if (isRestrictedChar) {
                        restrictedIds.add(el.id);
                    }
                    break;

                case GattDbElement.TYPE_DESCRIPTOR:
                    if (DBG) {
                        Log.d(TAG, "got descriptor with UUID=" + el.uuid + " id: " + el.id);
                    }

                    currChar.addDescriptor(new BluetoothGattDescriptor(el.uuid, el.id, 0));
                    if (isRestrictedChar) {
                        restrictedIds.add(el.id);
                    }
                    break;

                case GattDbElement.TYPE_INCLUDED_SERVICE:
                    if (DBG) {
                        Log.d(TAG, "got included service with UUID=" + el.uuid + " id: " + el.id
                                + " startHandle: " + el.startHandle);
                    }

                    currSrvc.addIncludedService(
                            new BluetoothGattService(el.uuid, el.startHandle, el.type));
                    break;

                default:
                    Log.e(TAG, "got unknown element with type=" + el.type + " and UUID=" + el.uuid
                            + " id: " + el.id);
            }
        }

        if (!restrictedIds.isEmpty()) {
            mRestrictedHandles.put(connId, restrictedIds);
        }
        // Search is complete when there was error, or nothing more to process
        app.callback.onSearchComplete(address, dbOut, 0 /* status */);
    }

    void onRegisterForNotifications(int connId, int status, int registered, int handle) {
        String address = mClientMap.addressByConnId(connId);

        if (DBG) {
            Log.d(TAG, "onRegisterForNotifications() - address=" + address + ", status=" + status
                    + ", registered=" + registered + ", handle=" + handle);
        }
    }

    void onNotify(int connId, String address, int handle, boolean isNotify, byte[] data)
            throws RemoteException {

        if (VDBG) {
            Log.d(TAG, "onNotify() - address=" + address + ", handle=" + handle + ", length="
                    + data.length);
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app != null) {
            try {
                permissionCheck(connId, handle);
            } catch (SecurityException ex) {
                // Only throws on apps with target SDK T+ as this old API did not throw prior to T
                if (checkCallerTargetSdk(this, app.name, Build.VERSION_CODES.TIRAMISU)) {
                    throw ex;
                }
                Log.w(TAG, "onNotify() - permission check failed!");
                return;
            }
            app.callback.onNotify(address, handle, data);
        }
    }

    void onReadCharacteristic(int connId, int status, int handle, byte[] data)
            throws RemoteException {
        String address = mClientMap.addressByConnId(connId);

        if (VDBG) {
            Log.d(TAG, "onReadCharacteristic() - address=" + address + ", status=" + status
                    + ", length=" + data.length);
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app != null) {
            app.callback.onCharacteristicRead(address, status, handle, data);
        }
    }

    void onWriteCharacteristic(int connId, int status, int handle, byte[] data)
            throws RemoteException {
        String address = mClientMap.addressByConnId(connId);
        synchronized (mPermits) {
            Log.d(TAG, "onWriteCharacteristic() - increasing permit for address="
                    + address);
            mPermits.put(address, -1);
        }

        if (VDBG) {
            Log.d(TAG, "onWriteCharacteristic() - address=" + address + ", status=" + status
                    + ", length=" + data.length);
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app == null) {
            return;
        }

        if (!app.isCongested) {
            app.callback.onCharacteristicWrite(address, status, handle, data);
        } else {
            if (status == BluetoothGatt.GATT_CONNECTION_CONGESTED) {
                status = BluetoothGatt.GATT_SUCCESS;
            }
            CallbackInfo callbackInfo = new CallbackInfo.Builder(address, status)
                    .setHandle(handle)
                    .setValue(data)
                    .build();
            app.queueCallback(callbackInfo);
        }
    }

    void onExecuteCompleted(int connId, int status) throws RemoteException {
        String address = mClientMap.addressByConnId(connId);
        if (VDBG) {
            Log.d(TAG, "onExecuteCompleted() - address=" + address + ", status=" + status);
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app != null) {
            app.callback.onExecuteWrite(address, status);
        }
    }

    void onReadDescriptor(int connId, int status, int handle, byte[] data) throws RemoteException {
        String address = mClientMap.addressByConnId(connId);

        if (VDBG) {
            Log.d(TAG,
                    "onReadDescriptor() - address=" + address + ", status=" + status + ", length="
                            + data.length);
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app != null) {
            app.callback.onDescriptorRead(address, status, handle, data);
        }
    }

    void onWriteDescriptor(int connId, int status, int handle, byte[] data)
            throws RemoteException {
        String address = mClientMap.addressByConnId(connId);

        if (VDBG) {
            Log.d(TAG, "onWriteDescriptor() - address=" + address + ", status=" + status
                    + ", length=" + data.length);
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app != null) {
            app.callback.onDescriptorWrite(address, status, handle, data);
        }
    }

    void onReadRemoteRssi(int clientIf, String address, int rssi, int status)
            throws RemoteException {
        if (DBG) {
            Log.d(TAG,
                    "onReadRemoteRssi() - clientIf=" + clientIf + " address=" + address + ", rssi="
                            + rssi + ", status=" + status);
        }

        ClientMap.App app = mClientMap.getById(clientIf);
        if (app != null) {
            app.callback.onReadRemoteRssi(address, rssi, status);
        }
    }

    void onScanFilterEnableDisabled(int action, int status, int clientIf) {
        if (DBG) {
            Log.d(TAG, "onScanFilterEnableDisabled() - clientIf=" + clientIf + ", status=" + status
                    + ", action=" + action);
        }
        mScanManager.callbackDone(clientIf, status);
    }

    void onScanFilterParamsConfigured(int action, int status, int clientIf, int availableSpace) {
        if (DBG) {
            Log.d(TAG,
                    "onScanFilterParamsConfigured() - clientIf=" + clientIf + ", status=" + status
                            + ", action=" + action + ", availableSpace=" + availableSpace);
        }
        mScanManager.callbackDone(clientIf, status);
    }

    void onScanFilterConfig(int action, int status, int clientIf, int filterType,
            int availableSpace) {
        if (DBG) {
            Log.d(TAG, "onScanFilterConfig() - clientIf=" + clientIf + ", action = " + action
                    + " status = " + status + ", filterType=" + filterType + ", availableSpace="
                    + availableSpace);
        }

        mScanManager.callbackDone(clientIf, status);
    }

    void onBatchScanStorageConfigured(int status, int clientIf) {
        if (DBG) {
            Log.d(TAG,
                    "onBatchScanStorageConfigured() - clientIf=" + clientIf + ", status=" + status);
        }
        mScanManager.callbackDone(clientIf, status);
    }

    // TODO: split into two different callbacks : onBatchScanStarted and onBatchScanStopped.
    void onBatchScanStartStopped(int startStopAction, int status, int clientIf) {
        if (DBG) {
            Log.d(TAG, "onBatchScanStartStopped() - clientIf=" + clientIf + ", status=" + status
                    + ", startStopAction=" + startStopAction);
        }
        mScanManager.callbackDone(clientIf, status);
    }

    ScanClient findBatchScanClientById(int scannerId) {
        for (ScanClient client : mScanManager.getBatchScanQueue()) {
            if (client.scannerId == scannerId) {
                return client;
            }
        }
        return null;
    }

    void onBatchScanReports(int status, int scannerId, int reportType, int numRecords,
            byte[] recordData) throws RemoteException {
        // When in testing mode, ignore all real-world events
        if (isTestModeEnabled()) return;

        onBatchScanReportsInternal(status, scannerId, reportType, numRecords, recordData);
    }

    void onBatchScanReportsInternal(int status, int scannerId, int reportType, int numRecords,
            byte[] recordData) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onBatchScanReports() - scannerId=" + scannerId + ", status=" + status
                    + ", reportType=" + reportType + ", numRecords=" + numRecords);
        }
        mScanManager.callbackDone(scannerId, status);
        Set<ScanResult> results = parseBatchScanResults(numRecords, reportType, recordData);
        if (reportType == ScanManager.SCAN_RESULT_TYPE_TRUNCATED) {
            // We only support single client for truncated mode.
            ScannerMap.App app = mScannerMap.getById(scannerId);
            if (app == null) {
                return;
            }

            ScanClient client = findBatchScanClientById(scannerId);
            if (client == null) {
                return;
            }

            ArrayList<ScanResult> permittedResults;
            if (hasScanResultPermission(client)) {
                permittedResults = new ArrayList<ScanResult>(results);
            } else {
                permittedResults = new ArrayList<ScanResult>();
                for (ScanResult scanResult : results) {
                    for (String associatedDevice : client.associatedDevices) {
                        if (associatedDevice.equalsIgnoreCase(scanResult.getDevice()
                                    .getAddress())) {
                            permittedResults.add(scanResult);
                        }
                    }
                }
                if (permittedResults.isEmpty()) {
                    return;
                }
            }

            if (client.hasDisavowedLocation) {
                permittedResults.removeIf(mLocationDenylistPredicate);
            }

            if (app.callback != null) {
                app.callback.onBatchScanResults(permittedResults);
            } else {
                // PendingIntent based
                try {
                    sendResultsByPendingIntent(app.info, permittedResults,
                            ScanSettings.CALLBACK_TYPE_ALL_MATCHES);
                } catch (PendingIntent.CanceledException e) {
                }
            }
        } else {
            for (ScanClient client : mScanManager.getFullBatchScanQueue()) {
                // Deliver results for each client.
                deliverBatchScan(client, results);
            }
        }
    }

    private void sendBatchScanResults(ScannerMap.App app, ScanClient client,
            ArrayList<ScanResult> results) {
        try {
            if (app.callback != null) {
                app.callback.onBatchScanResults(results);
            } else {
                sendResultsByPendingIntent(app.info, results,
                        ScanSettings.CALLBACK_TYPE_ALL_MATCHES);
            }
        } catch (RemoteException | PendingIntent.CanceledException e) {
            Log.e(TAG, "Exception: " + e);
            mScannerMap.remove(client.scannerId);
            mScanManager.stopScan(client.scannerId);
        }
    }

    // Check and deliver scan results for different scan clients.
    private void deliverBatchScan(ScanClient client, Set<ScanResult> allResults)
            throws RemoteException {
        ScannerMap.App app = mScannerMap.getById(client.scannerId);
        if (app == null) {
            Log.e(TAG, "app not found from id(" + client.scannerId + ") for received callback");
            return;
        }

        ArrayList<ScanResult> permittedResults;
        if (hasScanResultPermission(client)) {
            permittedResults = new ArrayList<ScanResult>(allResults);
        } else {
            permittedResults = new ArrayList<ScanResult>();
            for (ScanResult scanResult : allResults) {
                for (String associatedDevice : client.associatedDevices) {
                    if (associatedDevice.equalsIgnoreCase(scanResult.getDevice().getAddress())) {
                        permittedResults.add(scanResult);
                    }
                }
            }
            if (permittedResults.isEmpty()) {
                return;
            }
        }

        if (client.filters == null || client.filters.isEmpty()) {
            sendBatchScanResults(app, client, permittedResults);
            // TODO: Question to reviewer: Shouldn't there be a return here?
        }
        // Reconstruct the scan results.
        ArrayList<ScanResult> results = new ArrayList<ScanResult>();
        for (ScanResult scanResult : permittedResults) {
            if (matchesFilters(client, scanResult)) {
                results.add(scanResult);
            }
        }
        sendBatchScanResults(app, client, results);
    }

    private Set<ScanResult> parseBatchScanResults(int numRecords, int reportType,
            byte[] batchRecord) {
        if (numRecords == 0) {
            return Collections.emptySet();
        }
        if (DBG) {
            Log.d(TAG, "current time is " + SystemClock.elapsedRealtimeNanos());
        }
        if (reportType == ScanManager.SCAN_RESULT_TYPE_TRUNCATED) {
            return parseTruncatedResults(numRecords, batchRecord);
        } else {
            return parseFullResults(numRecords, batchRecord);
        }
    }

    private Set<ScanResult> parseTruncatedResults(int numRecords, byte[] batchRecord) {
        if (DBG) {
            Log.d(TAG, "batch record " + Arrays.toString(batchRecord));
        }
        Set<ScanResult> results = new HashSet<ScanResult>(numRecords);
        long now = SystemClock.elapsedRealtimeNanos();
        for (int i = 0; i < numRecords; ++i) {
            byte[] record =
                    extractBytes(batchRecord, i * TRUNCATED_RESULT_SIZE, TRUNCATED_RESULT_SIZE);
            byte[] address = extractBytes(record, 0, 6);
            reverse(address);
            BluetoothDevice device = mAdapter.getRemoteDevice(address);
            int rssi = record[8];
            long timestampNanos = now - parseTimestampNanos(extractBytes(record, 9, 2));
            results.add(getScanResultInstance(device, ScanRecord.parseFromBytes(new byte[0]), rssi,
                    timestampNanos));
        }
        return results;
    }

    @VisibleForTesting
    long parseTimestampNanos(byte[] data) {
        long timestampUnit = NumberUtils.littleEndianByteArrayToInt(data);
        // Timestamp is in every 50 ms.
        return TimeUnit.MILLISECONDS.toNanos(timestampUnit * 50);
    }

    private Set<ScanResult> parseFullResults(int numRecords, byte[] batchRecord) {
        if (DBG) {
            Log.d(TAG, "Batch record : " + Arrays.toString(batchRecord));
        }
        Set<ScanResult> results = new HashSet<ScanResult>(numRecords);
        int position = 0;
        long now = SystemClock.elapsedRealtimeNanos();
        while (position < batchRecord.length) {
            byte[] address = extractBytes(batchRecord, position, 6);
            // TODO: remove temp hack.
            reverse(address);
            BluetoothDevice device = mAdapter.getRemoteDevice(address);
            position += 6;
            // Skip address type.
            position++;
            // Skip tx power level.
            position++;
            int rssi = batchRecord[position++];
            long timestampNanos = now - parseTimestampNanos(extractBytes(batchRecord, position, 2));
            position += 2;

            // Combine advertise packet and scan response packet.
            int advertisePacketLen = batchRecord[position++];
            byte[] advertiseBytes = extractBytes(batchRecord, position, advertisePacketLen);
            position += advertisePacketLen;
            int scanResponsePacketLen = batchRecord[position++];
            byte[] scanResponseBytes = extractBytes(batchRecord, position, scanResponsePacketLen);
            position += scanResponsePacketLen;
            byte[] scanRecord = new byte[advertisePacketLen + scanResponsePacketLen];
            System.arraycopy(advertiseBytes, 0, scanRecord, 0, advertisePacketLen);
            System.arraycopy(scanResponseBytes, 0, scanRecord, advertisePacketLen,
                    scanResponsePacketLen);
            if (DBG) {
                Log.d(TAG, "ScanRecord : " + Arrays.toString(scanRecord));
            }
            results.add(getScanResultInstance(device, ScanRecord.parseFromBytes(scanRecord), rssi,
                    timestampNanos));
        }
        return results;
    }

    // Reverse byte array.
    private void reverse(byte[] address) {
        int len = address.length;
        for (int i = 0; i < len / 2; ++i) {
            byte b = address[i];
            address[i] = address[len - 1 - i];
            address[len - 1 - i] = b;
        }
    }

    // Helper method to extract bytes from byte array.
    private static byte[] extractBytes(byte[] scanRecord, int start, int length) {
        byte[] bytes = new byte[length];
        System.arraycopy(scanRecord, start, bytes, 0, length);
        return bytes;
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    void onBatchScanThresholdCrossed(int clientIf) {
        if (DBG) {
            Log.d(TAG, "onBatchScanThresholdCrossed() - clientIf=" + clientIf);
        }
        flushPendingBatchResults(clientIf, getAttributionSource());
    }

    AdvtFilterOnFoundOnLostInfo createOnTrackAdvFoundLostObject(int clientIf, int advPktLen,
            byte[] advPkt, int scanRspLen, byte[] scanRsp, int filtIndex, int advState,
            int advInfoPresent, String address, int addrType, int txPower, int rssiValue,
            int timeStamp) {

        return new AdvtFilterOnFoundOnLostInfo(clientIf, advPktLen, advPkt, scanRspLen, scanRsp,
                filtIndex, advState, advInfoPresent, address, addrType, txPower, rssiValue,
                timeStamp);
    }

    void onTrackAdvFoundLost(AdvtFilterOnFoundOnLostInfo trackingInfo) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onTrackAdvFoundLost() - scannerId= " + trackingInfo.getClientIf()
                    + " address = " + trackingInfo.getAddress() + " adv_state = "
                    + trackingInfo.getAdvState());
        }

        ScannerMap.App app = mScannerMap.getById(trackingInfo.getClientIf());
        if (app == null || (app.callback == null && app.info == null)) {
            Log.e(TAG, "app or callback is null");
            return;
        }

        BluetoothDevice device =
                BluetoothAdapter.getDefaultAdapter().getRemoteDevice(trackingInfo.getAddress());
        int advertiserState = trackingInfo.getAdvState();
        ScanResult result =
                getScanResultInstance(device, ScanRecord.parseFromBytes(trackingInfo.getResult()),
                        trackingInfo.getRSSIValue(), SystemClock.elapsedRealtimeNanos());

        for (ScanClient client : mScanManager.getRegularScanQueue()) {
            if (client.scannerId == trackingInfo.getClientIf()) {
                ScanSettings settings = client.settings;
                if ((advertiserState == ADVT_STATE_ONFOUND) && (
                        (settings.getCallbackType() & ScanSettings.CALLBACK_TYPE_FIRST_MATCH)
                                != 0)) {
                    if (app.callback != null) {
                        app.callback.onFoundOrLost(true, result);
                    } else {
                        sendResultByPendingIntent(app.info, result,
                                ScanSettings.CALLBACK_TYPE_FIRST_MATCH, client);
                    }
                } else if ((advertiserState == ADVT_STATE_ONLOST) && (
                        (settings.getCallbackType() & ScanSettings.CALLBACK_TYPE_MATCH_LOST)
                                != 0)) {
                    if (app.callback != null) {
                        app.callback.onFoundOrLost(false, result);
                    } else {
                        sendResultByPendingIntent(app.info, result,
                                ScanSettings.CALLBACK_TYPE_MATCH_LOST, client);
                    }
                } else {
                    if (DBG) {
                        Log.d(TAG, "Not reporting onlost/onfound : " + advertiserState
                                + " scannerId = " + client.scannerId + " callbackType "
                                + settings.getCallbackType());
                    }
                }
            }
        }
    }

    void onScanParamSetupCompleted(int status, int scannerId) throws RemoteException {
        ScannerMap.App app = mScannerMap.getById(scannerId);
        if (app == null || app.callback == null) {
            Log.e(TAG, "Advertise app or callback is null");
            return;
        }
        if (DBG) {
            Log.d(TAG, "onScanParamSetupCompleted : " + status);
        }
    }

    // callback from ScanManager for dispatch of errors apps.
    void onScanManagerErrorCallback(int scannerId, int errorCode) throws RemoteException {
        ScannerMap.App app = mScannerMap.getById(scannerId);
        if (app == null || (app.callback == null && app.info == null)) {
            Log.e(TAG, "App or callback is null");
            return;
        }
        if (app.callback != null) {
            app.callback.onScanManagerErrorCallback(errorCode);
        } else {
            try {
                sendErrorByPendingIntent(app.info, errorCode);
            } catch (PendingIntent.CanceledException e) {
                Log.e(TAG, "Error sending error code via PendingIntent:" + e);
            }
        }
    }

    void onConfigureMTU(int connId, int status, int mtu) throws RemoteException {
        String address = mClientMap.addressByConnId(connId);

        if (DBG) {
            Log.d(TAG,
                    "onConfigureMTU() address=" + address + ", status=" + status + ", mtu=" + mtu);
        }

        ClientMap.App app = mClientMap.getByConnId(connId);
        if (app != null) {
            app.callback.onConfigureMTU(address, mtu, status);
        }
    }

    void onClientCongestion(int connId, boolean congested) throws RemoteException {
        if (VDBG) {
            Log.d(TAG, "onClientCongestion() - connId=" + connId + ", congested=" + congested);
        }

        ClientMap.App app = mClientMap.getByConnId(connId);

        if (app != null) {
            app.isCongested = congested;
            while (!app.isCongested) {
                CallbackInfo callbackInfo = app.popQueuedCallback();
                if (callbackInfo == null) {
                    return;
                }
                app.callback.onCharacteristicWrite(callbackInfo.address, callbackInfo.status,
                        callbackInfo.handle, callbackInfo.value);
            }
        }
    }

    /**************************************************************************
     * GATT Service functions - Shared CLIENT/SERVER
     *************************************************************************/

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    List<BluetoothDevice> getDevicesMatchingConnectionStates(
            int[] states, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource,
                "GattService getDevicesMatchingConnectionStates")) {
            return new ArrayList<>(0);
        }

        Map<BluetoothDevice, Integer> deviceStates = new HashMap<BluetoothDevice, Integer>();

        // Add paired LE devices

        Set<BluetoothDevice> bondedDevices = mAdapter.getBondedDevices();
        for (BluetoothDevice device : bondedDevices) {
            if (getDeviceType(device) != AbstractionLayer.BT_DEVICE_TYPE_BREDR) {
                deviceStates.put(device, BluetoothProfile.STATE_DISCONNECTED);
            }
        }

        // Add connected deviceStates

        Set<String> connectedDevices = new HashSet<String>();
        connectedDevices.addAll(mClientMap.getConnectedDevices());
        connectedDevices.addAll(mServerMap.getConnectedDevices());

        for (String address : connectedDevices) {
            BluetoothDevice device = mAdapter.getRemoteDevice(address);
            if (device != null) {
                deviceStates.put(device, BluetoothProfile.STATE_CONNECTED);
            }
        }

        // Create matching device sub-set

        List<BluetoothDevice> deviceList = new ArrayList<BluetoothDevice>();

        for (Map.Entry<BluetoothDevice, Integer> entry : deviceStates.entrySet()) {
            for (int state : states) {
                if (entry.getValue() == state) {
                    deviceList.add(entry.getKey());
                }
            }
        }
        if (VDBG) Log.v(TAG, "getDevicesMatchingConnectionStates: State = "
                        + Arrays.toString(states) + ", deviceList = " + deviceList);
        return deviceList;
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    void registerScanner(IScannerCallback callback, WorkSource workSource,
            AttributionSource attributionSource) throws RemoteException {
        if (!Utils.checkScanPermissionForDataDelivery(
                this, attributionSource, "GattService registerScanner")) {
            return;
        }

        UUID uuid = UUID.randomUUID();
        if (DBG) {
            Log.d(TAG, "registerScanner() - UUID=" + uuid);
        }

        enforceImpersonatationPermissionIfNeeded(workSource);

        AppScanStats app = mScannerMap.getAppScanStatsByUid(Binder.getCallingUid());
        if (app != null && app.isScanningTooFrequently()
                && !Utils.checkCallerHasPrivilegedPermission(this)) {
            Log.e(TAG, "App '" + app.appName + "' is scanning too frequently");
            callback.onScannerRegistered(ScanCallback.SCAN_FAILED_SCANNING_TOO_FREQUENTLY, -1);
            return;
        }

        mScannerMap.add(uuid, workSource, callback, null, this);
        mScanManager.registerScanner(uuid);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    void unregisterScanner(int scannerId, AttributionSource attributionSource) {
        if (!Utils.checkScanPermissionForDataDelivery(
                this, attributionSource, "GattService unregisterScanner")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "unregisterScanner() - scannerId=" + scannerId);
        }
        mScannerMap.remove(scannerId);
        mScanManager.unregisterScanner(scannerId);
    }

    private List<String> getAssociatedDevices(String callingPackage, UserHandle userHandle) {
        if (mCompanionManager == null) {
            return new ArrayList<String>();
        }

        List<String> macAddresses = new ArrayList();

        final long identity = Binder.clearCallingIdentity();
        try {
            for (AssociationInfo info : mCompanionManager.getAssociations(
                    callingPackage, userHandle.getIdentifier())) {
                macAddresses.add(info.getDeviceMacAddress().toString());
            }
        } catch (SecurityException se) {
            // Not an app with associated devices
        } catch (RemoteException re) {
            Log.e(TAG, "Cannot reach companion device service", re);
        } catch (Exception e) {
            Log.e(TAG, "Cannot check device associations for " + callingPackage, e);
        } finally {
            Binder.restoreCallingIdentity(identity);
        }
        return macAddresses;
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    void startScan(int scannerId, ScanSettings settings, List<ScanFilter> filters,
            AttributionSource attributionSource) {
        if (DBG) {
            Log.d(TAG, "start scan with filters. callingPackage: "
                    + attributionSource.getPackageName());
        }
        if (!Utils.checkScanPermissionForDataDelivery(
            this, attributionSource, "Starting GATT scan.")) {
            return;
        }

        enforcePrivilegedPermissionIfNeeded(settings);
        String callingPackage = attributionSource.getPackageName();
        settings = enforceReportDelayFloor(settings);
        enforcePrivilegedPermissionIfNeeded(filters);
        final ScanClient scanClient = new ScanClient(scannerId, settings, filters);
        scanClient.userHandle = UserHandle.of(UserHandle.getCallingUserId());
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        scanClient.eligibleForSanitizedExposureNotification =
                callingPackage.equals(mExposureNotificationPackage);
        scanClient.hasDisavowedLocation =
                Utils.hasDisavowedLocationForScan(this, attributionSource, isTestModeEnabled());
        scanClient.isQApp = checkCallerTargetSdk(this, callingPackage, Build.VERSION_CODES.Q);
        if (!scanClient.hasDisavowedLocation) {
            if (scanClient.isQApp) {
                scanClient.hasLocationPermission = Utils.checkCallerHasFineLocation(
                        this, attributionSource, scanClient.userHandle);
            } else {
                scanClient.hasLocationPermission = Utils.checkCallerHasCoarseOrFineLocation(
                        this, attributionSource, scanClient.userHandle);
            }
        }
        if (callingPackage != null &&
            callingPackage.equals("com.android.bluetooth")) {
            if (DBG) {
                Log.d(TAG, "allowAddressTypeInResults only for Bluetooth apk");
            }
            scanClient.allowAddressTypeInResults  = true;
        }

        scanClient.callingPackage = callingPackage;
        scanClient.hasNetworkSettingsPermission =
                Utils.checkCallerHasNetworkSettingsPermission(this);
        scanClient.hasNetworkSetupWizardPermission =
                Utils.checkCallerHasNetworkSetupWizardPermission(this);
        scanClient.hasScanWithoutLocationPermission =
                Utils.checkCallerHasScanWithoutLocationPermission(this);
        scanClient.associatedDevices = getAssociatedDevices(callingPackage, scanClient.userHandle);

        AppScanStats app = mScannerMap.getAppScanStatsById(scannerId);
        ScannerMap.App cbApp = mScannerMap.getById(scannerId);
        if (app != null) {
            scanClient.stats = app;
            boolean isFilteredScan = (filters != null) && !filters.isEmpty();
            boolean isCallbackScan = false;
            if (cbApp != null) {
                isCallbackScan = cbApp.callback != null;
            }
            app.recordScanStart(settings, filters, isFilteredScan, isCallbackScan, scannerId);
        }

        mScanManager.addPendingScanToQueue(scanClient);
        mScanManager.startScan(scanClient);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    void registerPiAndStartScan(PendingIntent pendingIntent, ScanSettings settings,
            List<ScanFilter> filters, AttributionSource attributionSource) {
        if (DBG) {
            Log.d(TAG, "start scan with filters, for PendingIntent. callingPackage: "
                    + attributionSource.getPackageName());
        }
        if (!Utils.checkScanPermissionForDataDelivery(
            this, attributionSource, "Starting GATT scan.")) {
            return;
        }
        enforcePrivilegedPermissionIfNeeded(settings);
        settings = enforceReportDelayFloor(settings);
        enforcePrivilegedPermissionIfNeeded(filters);
        UUID uuid = UUID.randomUUID();
        if (DBG) {
            Log.d(TAG, "startScan(PI) - UUID=" + uuid);
        }
        String callingPackage = attributionSource.getPackageName();
        PendingIntentInfo piInfo = new PendingIntentInfo();
        piInfo.intent = pendingIntent;
        piInfo.settings = settings;
        piInfo.filters = filters;
        piInfo.callingPackage = callingPackage;

        // Don't start scan if the Pi scan already in mScannerMap.
        if (mScannerMap.getByContextInfo(piInfo) != null) {
            Log.d(TAG, "Don't startScan(PI) since the same Pi scan already in mScannerMap.");
            return;
        }

        ScannerMap.App app = mScannerMap.add(uuid, null, null, piInfo, this);
        app.mUserHandle = UserHandle.of(UserHandle.getCallingUserId());
        mAppOps.checkPackage(Binder.getCallingUid(), callingPackage);
        app.mEligibleForSanitizedExposureNotification =
                callingPackage.equals(mExposureNotificationPackage);

        app.mHasDisavowedLocation =
                Utils.hasDisavowedLocationForScan(this, attributionSource, isTestModeEnabled());

        if (!app.mHasDisavowedLocation) {
            try {
                if (checkCallerTargetSdk(this, callingPackage, Build.VERSION_CODES.Q)) {
                    app.hasLocationPermission = Utils.checkCallerHasFineLocation(
                            this, attributionSource, app.mUserHandle);
                } else {
                    app.hasLocationPermission = Utils.checkCallerHasCoarseOrFineLocation(
                            this, attributionSource, app.mUserHandle);
                }
            } catch (SecurityException se) {
                // No need to throw here. Just mark as not granted.
                app.hasLocationPermission = false;
            }
        }
        app.mHasNetworkSettingsPermission =
                Utils.checkCallerHasNetworkSettingsPermission(this);
        app.mHasNetworkSetupWizardPermission =
                Utils.checkCallerHasNetworkSetupWizardPermission(this);
        app.mHasScanWithoutLocationPermission =
                Utils.checkCallerHasScanWithoutLocationPermission(this);
        app.mAssociatedDevices = getAssociatedDevices(callingPackage, app.mUserHandle);
        mScanManager.registerScanner(uuid);
    }

    void continuePiStartScan(int scannerId, ScannerMap.App app) {
        final PendingIntentInfo piInfo = app.info;
        final ScanClient scanClient =
                new ScanClient(scannerId, piInfo.settings, piInfo.filters);
        scanClient.hasLocationPermission = app.hasLocationPermission;
        scanClient.userHandle = app.mUserHandle;
        scanClient.isQApp = checkCallerTargetSdk(this, app.name, Build.VERSION_CODES.Q);
        scanClient.eligibleForSanitizedExposureNotification =
                app.mEligibleForSanitizedExposureNotification;
        scanClient.hasNetworkSettingsPermission = app.mHasNetworkSettingsPermission;
        scanClient.hasNetworkSetupWizardPermission = app.mHasNetworkSetupWizardPermission;
        scanClient.hasScanWithoutLocationPermission = app.mHasScanWithoutLocationPermission;
        scanClient.associatedDevices = app.mAssociatedDevices;
        scanClient.hasDisavowedLocation = app.mHasDisavowedLocation;

        AppScanStats scanStats = mScannerMap.getAppScanStatsById(scannerId);
        if (scanStats != null) {
            scanClient.stats = scanStats;
            boolean isFilteredScan = (piInfo.filters != null) && !piInfo.filters.isEmpty();
            scanStats.recordScanStart(
                    piInfo.settings, piInfo.filters, isFilteredScan, false, scannerId);
        }

        mScanManager.startScan(scanClient);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    void flushPendingBatchResults(int scannerId, AttributionSource attributionSource) {
        if (!Utils.checkScanPermissionForDataDelivery(
                this, attributionSource, "GattService flushPendingBatchResults")) {
            return;
        }
        if (DBG) {
            Log.d(TAG, "flushPendingBatchResults - scannerId=" + scannerId);
        }
        mScanManager.flushBatchScanResults(new ScanClient(scannerId));
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    void stopScan(int scannerId, AttributionSource attributionSource) {
        if (!Utils.checkScanPermissionForDataDelivery(
                this, attributionSource, "GattService stopScan")) {
            return;
        }
        int scanQueueSize =
                mScanManager.getBatchScanQueue().size() + mScanManager.getRegularScanQueue().size();
        if (DBG) {
            Log.d(TAG, "stopScan() - queue size =" + scanQueueSize);
        }

        AppScanStats app = null;
        app = mScannerMap.getAppScanStatsById(scannerId);
        if (app != null) {
            app.recordScanStop(scannerId);
        }
        if (mScanManager != null) {
            mScanManager.stopScan(scannerId);
        }
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    void stopScan(PendingIntent intent, AttributionSource attributionSource) {
        if (!Utils.checkScanPermissionForDataDelivery(
                this, attributionSource, "GattService stopScan")) {
            return;
        }

        PendingIntentInfo pii = new PendingIntentInfo();
        pii.intent = intent;
        ScannerMap.App app = mScannerMap.getByContextInfo(pii);
        if (VDBG) {
            Log.d(TAG, "stopScan(PendingIntent): app found = " + app);
        }
        if (app != null) {
            final int scannerId = app.id;
            stopScan(scannerId, attributionSource);
            // Also unregister the scanner
            unregisterScanner(scannerId, attributionSource);
        }
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void disconnectAll(AttributionSource attributionSource) {
        if (DBG) {
            Log.d(TAG, "disconnectAll()");
        }
        Map<Integer, String> connMap = mClientMap.getConnectedMap();
        for (Map.Entry<Integer, String> entry : connMap.entrySet()) {
            if (DBG) {
                Log.d(TAG, "disconnecting addr:" + entry.getValue());
            }
            clientDisconnect(entry.getKey(), entry.getValue(), attributionSource);
            //clientDisconnect(int clientIf, String address)
        }
    }

    boolean isScanClient(int clientIf) {
        for (ScanClient client : mScanManager.getRegularScanQueue()) {
            if (client.scannerId == clientIf) {
                return true;
            }
        }
        for (ScanClient client : mScanManager.getBatchScanQueue()) {
            if (client.scannerId == clientIf) {
                return true;
            }
        }
        if (VDBG) Log.v(TAG, "clientIf: " + clientIf + " is not a ScanClient");
        return false;
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void unregAll(AttributionSource attributionSource) {
        for (Integer appId : mClientMap.getAllAppsIds()) {
            if (DBG) {
                Log.d(TAG, "unreg:" + appId);
            }
            unregisterClient(appId, attributionSource);
        }
        for (Integer appId : mServerMap.getAllAppsIds()) {
            if (DBG) Log.d(TAG, "unreg:" + appId);
            unregisterServer(appId, attributionSource);
        }
        for (Integer appId : mScannerMap.getAllAppsIds()) {
            if (DBG) Log.d(TAG, "unreg:" + appId);
            if (isScanClient(appId)) {
                ScanClient client = new ScanClient(appId);
                stopScan(client.scannerId, attributionSource);
                unregisterScanner(appId, attributionSource);
            }
        }
        if (mAdvertiseManager != null) {
            mAdvertiseManager.stopAdvertisingSets();
        }
    }

    /**************************************************************************
     * Clear all pending Scanning and Advertising sets
     *************************************************************************/
    public void clearPendingOperations() {
        for (Integer appId : mScannerMap.getAllAppsIds()) {
            if (DBG) Log.d(TAG, "clearPendingOperations ID: " + appId);
            if (isScanClient(appId)) {
                AppScanStats app = mScannerMap.getAppScanStatsById(appId);
                if (app != null) {
                    app.recordScanStop(appId);
                }

                ScanClient client = new ScanClient(appId);
                mScanManager.cancelScans(client);
                unregisterScanner(appId, getAttributionSource());
            }
        }
        if (mAdvertiseManager != null) {
            mAdvertiseManager.stopAdvertisingSets();
        }
    }

    public void setAptXLowLatencyMode(boolean enabled){
        mScanManager.setAptXLowLatencyMode(enabled);
    }

    /**************************************************************************
     * PERIODIC SCANNING
     *************************************************************************/
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    void registerSync(ScanResult scanResult, int skip, int timeout,
            IPeriodicAdvertisingCallback callback, AttributionSource attributionSource) {
        if (!Utils.checkScanPermissionForDataDelivery(
                this, attributionSource, "GattService registerSync")) {
            return;
        }
        mPeriodicScanManager.startSync(scanResult, skip, timeout, callback);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_SCAN)
    void unregisterSync(
            IPeriodicAdvertisingCallback callback, AttributionSource attributionSource) {
        if (!Utils.checkScanPermissionForDataDelivery(
                this, attributionSource, "GattService unregisterSync")) {
            return;
        }
        mPeriodicScanManager.stopSync(callback);
    }

    void transferSync(BluetoothDevice bda, int service_data, int sync_handle, AttributionSource attributionSource) {
        if (!Utils.checkScanPermissionForDataDelivery(this, attributionSource, "GattService transferSync")) {
            return;
        }
        mPeriodicScanManager.transferSync(bda, service_data, sync_handle);
    }

    void transferSetInfo(BluetoothDevice bda, int service_data,
                  int adv_handle, IPeriodicAdvertisingCallback callback,
                  AttributionSource attributionSource) {
        if (!Utils.checkScanPermissionForDataDelivery(this,attributionSource, "GattService transferSetInfo")) {
            return;
        }
        mPeriodicScanManager.transferSetInfo(bda, service_data, adv_handle, callback);
    }
    /**************************************************************************
     * ADVERTISING SET
     *************************************************************************/
    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    void startAdvertisingSet(AdvertisingSetParameters parameters, AdvertiseData advertiseData,
            AdvertiseData scanResponse, PeriodicAdvertisingParameters periodicParameters,
            AdvertiseData periodicData, int duration, int maxExtAdvEvents,
            IAdvertisingSetCallback callback, AttributionSource attributionSource) {
        if (!Utils.checkAdvertisePermissionForDataDelivery(
                this, attributionSource, "GattService startAdvertisingSet")) {
            return;
        }
        if (parameters.getOwnAddressType() != AdvertisingSetParameters.ADDRESS_TYPE_DEFAULT) {
            Utils.enforceBluetoothPrivilegedPermission(this);
        }
        mAdvertiseManager.startAdvertisingSet(parameters, advertiseData, scanResponse,
                periodicParameters, periodicData, duration, maxExtAdvEvents, callback);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    void stopAdvertisingSet(IAdvertisingSetCallback callback, AttributionSource attributionSource) {
        if (!Utils.checkAdvertisePermissionForDataDelivery(
                this, attributionSource, "GattService stopAdvertisingSet")) {
            return;
        }
        mAdvertiseManager.stopAdvertisingSet(callback);
    }

    @RequiresPermission(allOf = {
            android.Manifest.permission.BLUETOOTH_ADVERTISE,
            android.Manifest.permission.BLUETOOTH_PRIVILEGED,
    })
    void getOwnAddress(int advertiserId, AttributionSource attributionSource) {
        if (!Utils.checkAdvertisePermissionForDataDelivery(
                this, attributionSource, "GattService getOwnAddress")) {
            return;
        }
        enforceBluetoothPrivilegedPermission(this);
        mAdvertiseManager.getOwnAddress(advertiserId);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    void enableAdvertisingSet(int advertiserId, boolean enable, int duration, int maxExtAdvEvents,
            AttributionSource attributionSource) {
        if (!Utils.checkAdvertisePermissionForDataDelivery(
                this, attributionSource, "GattService enableAdvertisingSet")) {
            return;
        }
        mAdvertiseManager.enableAdvertisingSet(advertiserId, enable, duration, maxExtAdvEvents);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    void setAdvertisingData(
            int advertiserId, AdvertiseData data, AttributionSource attributionSource) {
        if (!Utils.checkAdvertisePermissionForDataDelivery(
                this, attributionSource, "GattService setAdvertisingData")) {
            return;
        }
        mAdvertiseManager.setAdvertisingData(advertiserId, data);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    void setScanResponseData(
            int advertiserId, AdvertiseData data, AttributionSource attributionSource) {
        if (!Utils.checkAdvertisePermissionForDataDelivery(
                this, attributionSource, "GattService setScanResponseData")) {
            return;
        }
        mAdvertiseManager.setScanResponseData(advertiserId, data);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    void setAdvertisingParameters(int advertiserId, AdvertisingSetParameters parameters,
            AttributionSource attributionSource) {
        if (!Utils.checkAdvertisePermissionForDataDelivery(
                this, attributionSource, "GattService setAdvertisingParameters")) {
            return;
        }
        mAdvertiseManager.setAdvertisingParameters(advertiserId, parameters);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    void setPeriodicAdvertisingParameters(int advertiserId,
            PeriodicAdvertisingParameters parameters, AttributionSource attributionSource) {
        if (!Utils.checkAdvertisePermissionForDataDelivery(
                this, attributionSource, "GattService setPeriodicAdvertisingParameters")) {
            return;
        }
        mAdvertiseManager.setPeriodicAdvertisingParameters(advertiserId, parameters);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    void setPeriodicAdvertisingData(
            int advertiserId, AdvertiseData data, AttributionSource attributionSource) {
        if (!Utils.checkAdvertisePermissionForDataDelivery(
                this, attributionSource, "GattService setPeriodicAdvertisingData")) {
            return;
        }
        mAdvertiseManager.setPeriodicAdvertisingData(advertiserId, data);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_ADVERTISE)
    void setPeriodicAdvertisingEnable(
            int advertiserId, boolean enable, AttributionSource attributionSource) {
        if (!Utils.checkAdvertisePermissionForDataDelivery(
                this, attributionSource, "GattService setPeriodicAdvertisingEnable")) {
            return;
        }
        mAdvertiseManager.setPeriodicAdvertisingEnable(advertiserId, enable);
    }

    /**************************************************************************
     * GATT Service functions - CLIENT
     *************************************************************************/

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void registerClient(UUID uuid, IBluetoothGattCallback callback, boolean eatt_support,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService registerClient")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "registerClient() - UUID=" + uuid);
        }
        mClientMap.add(uuid, null, callback, null, this);
        gattClientRegisterAppNative(uuid.getLeastSignificantBits(), uuid.getMostSignificantBits(),
                eatt_support);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void unregisterClient(int clientIf, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService unregisterClient")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "unregisterClient() - clientIf=" + clientIf);
        }
        mClientMap.remove(clientIf);
        gattClientUnregisterAppNative(clientIf);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void clientConnect(int clientIf, String address, boolean isDirect, int transport,
            boolean opportunistic, int phy, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService clientConnect")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "clientConnect() - address=" + address + ", isDirect=" + isDirect
                    + ", opportunistic=" + opportunistic + ", phy=" + phy);
        }
        gattClientConnectNative(clientIf, address, isDirect, transport, opportunistic, phy);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void clientDisconnect(int clientIf, String address, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService clientDisconnect")) {
            return;
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (DBG) {
            Log.d(TAG, "clientDisconnect() - address=" + address + ", connId=" + connId);
        }

        gattClientDisconnectNative(clientIf, address, connId != null ? connId : 0);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void clientSetPreferredPhy(int clientIf, String address, int txPhy, int rxPhy, int phyOptions,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService clientSetPreferredPhy")) {
            return;
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId == null) {
            if (DBG) {
                Log.d(TAG, "clientSetPreferredPhy() - no connection to " + address);
            }
            return;
        }

        if (DBG) {
            Log.d(TAG, "clientSetPreferredPhy() - address=" + address + ", connId=" + connId);
        }
        gattClientSetPreferredPhyNative(clientIf, address, txPhy, rxPhy, phyOptions);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void clientReadPhy(int clientIf, String address, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService clientReadPhy")) {
            return;
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId == null) {
            if (DBG) {
                Log.d(TAG, "clientReadPhy() - no connection to " + address);
            }
            return;
        }

        if (DBG) {
            Log.d(TAG, "clientReadPhy() - address=" + address + ", connId=" + connId);
        }
        gattClientReadPhyNative(clientIf, address);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    int numHwTrackFiltersAvailable(AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService numHwTrackFiltersAvailable")) {
            return 0;
        }
        return (AdapterService.getAdapterService().getTotalNumOfTrackableAdvertisements()
                - mScanManager.getCurrentUsedTrackingAdvertisement());
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    synchronized List<ParcelUuid> getRegisteredServiceUuids(AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService getRegisteredServiceUuids")) {
            return new ArrayList<>(0);
        }
        List<ParcelUuid> serviceUuids = new ArrayList<ParcelUuid>();
        for (HandleMap.Entry entry : mHandleMap.mEntries) {
            serviceUuids.add(new ParcelUuid(entry.uuid));
        }
        return serviceUuids;
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    List<String> getConnectedDevices(AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService getConnectedDevices")) {
            return new ArrayList<>(0);
        }

        Set<String> connectedDevAddress = new HashSet<String>();
        connectedDevAddress.addAll(mClientMap.getConnectedDevices());
        connectedDevAddress.addAll(mServerMap.getConnectedDevices());
        List<String> connectedDeviceList = new ArrayList<String>(connectedDevAddress);
        return connectedDeviceList;
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void refreshDevice(int clientIf, String address, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService refreshDevice")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "refreshDevice() - address=" + address);
        }
        gattClientRefreshNative(clientIf, address);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void discoverServices(int clientIf, String address, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService discoverServices")) {
            return;
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (DBG) {
            Log.d(TAG, "discoverServices() - address=" + address + ", connId=" + connId);
        }

        if (connId != null) {
            gattClientSearchServiceNative(connId, true, 0, 0);
        } else {
            Log.e(TAG, "discoverServices() - No connection for " + address + "...");
        }
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void discoverServiceByUuid(
            int clientIf, String address, UUID uuid, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService discoverServiceByUuid")) {
            return;
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId != null) {
            gattClientDiscoverServiceByUuidNative(connId, uuid.getLeastSignificantBits(),
                    uuid.getMostSignificantBits());
        } else {
            Log.e(TAG, "discoverServiceByUuid() - No connection for " + address + "...");
        }
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void readCharacteristic(int clientIf, String address, int handle, int authReq,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService readCharacteristic")) {
            return;
        }

        if (VDBG) {
            Log.d(TAG, "readCharacteristic() - address=" + address);
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId == null) {
            Log.e(TAG, "readCharacteristic() - No connection for " + address + "...");
            return;
        }

        try {
            permissionCheck(connId, handle);
        } catch (SecurityException ex) {
            String callingPackage = attributionSource.getPackageName();
            // Only throws on apps with target SDK T+ as this old API did not throw prior to T
            if (checkCallerTargetSdk(this, callingPackage, Build.VERSION_CODES.TIRAMISU)) {
                throw ex;
            }
            Log.w(TAG, "readCharacteristic() - permission check failed!");
            return;
        }

        gattClientReadCharacteristicNative(connId, handle, authReq);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void readUsingCharacteristicUuid(int clientIf, String address, UUID uuid, int startHandle,
            int endHandle, int authReq, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService readUsingCharacteristicUuid")) {
            return;
        }

        if (VDBG) {
            Log.d(TAG, "readUsingCharacteristicUuid() - address=" + address);
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId == null) {
            Log.e(TAG, "readUsingCharacteristicUuid() - No connection for " + address + "...");
            return;
        }

        try {
            permissionCheck(uuid);
        } catch (SecurityException ex) {
            String callingPackage = attributionSource.getPackageName();
            // Only throws on apps with target SDK T+ as this old API did not throw prior to T
            if (checkCallerTargetSdk(this, callingPackage, Build.VERSION_CODES.TIRAMISU)) {
                throw ex;
            }
            Log.w(TAG, "readUsingCharacteristicUuid() - permission check failed!");
            return;
        }

        gattClientReadUsingCharacteristicUuidNative(connId, uuid.getLeastSignificantBits(),
                uuid.getMostSignificantBits(), startHandle, endHandle, authReq);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    int writeCharacteristic(int clientIf, String address, int handle, int writeType, int authReq,
            byte[] value, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService writeCharacteristic")) {
            return BluetoothStatusCodes.ERROR_UNKNOWN;
        }

        if (VDBG) {
            Log.d(TAG, "writeCharacteristic() - address=" + address);
        }

        if (mReliableQueue.contains(address)) {
            writeType = 3; // Prepared write
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId == null) {
            Log.e(TAG, "writeCharacteristic() - No connection for " + address + "...");
            return BluetoothStatusCodes.ERROR_UNKNOWN;
        }
        permissionCheck(connId, handle);

        Log.d(TAG, "writeCharacteristic() - trying to acquire permit.");
        // Lock the thread until onCharacteristicWrite callback comes back.
        synchronized (mPermits) {
            Integer permit = mPermits.get(address);
            if (permit == null) {
                Log.d(TAG, "writeCharacteristic() -  atomicBoolean uninitialized!");
                return BluetoothStatusCodes.ERROR_UNKNOWN;
            }

            boolean success = (permit == -1);
            if (!success) {
                 Log.d(TAG, "writeCharacteristic() - no permit available.");
                 return BluetoothStatusCodes.ERROR_GATT_WRITE_REQUEST_BUSY;
            }
            mPermits.put(address, connId);
        }

        gattClientWriteCharacteristicNative(connId, handle, writeType, authReq, value);
        return BluetoothStatusCodes.SUCCESS;
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void readDescriptor(int clientIf, String address, int handle, int authReq,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService readDescriptor")) {
            return;
        }

        if (VDBG) {
            Log.d(TAG, "readDescriptor() - address=" + address);
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId == null) {
            Log.e(TAG, "readDescriptor() - No connection for " + address + "...");
            return;
        }

        try {
            permissionCheck(connId, handle);
        } catch (SecurityException ex) {
            String callingPackage = attributionSource.getPackageName();
            // Only throws on apps with target SDK T+ as this old API did not throw prior to T
            if (checkCallerTargetSdk(this, callingPackage, Build.VERSION_CODES.TIRAMISU)) {
                throw ex;
            }
            Log.w(TAG, "readDescriptor() - permission check failed!");
            return;
        }

        gattClientReadDescriptorNative(connId, handle, authReq);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    int writeDescriptor(int clientIf, String address, int handle, int authReq, byte[] value,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService writeDescriptor")) {
            return BluetoothStatusCodes.ERROR_MISSING_BLUETOOTH_CONNECT_PERMISSION;
        }
        if (VDBG) {
            Log.d(TAG, "writeDescriptor() - address=" + address);
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId == null) {
            Log.e(TAG, "writeDescriptor() - No connection for " + address + "...");
            return BluetoothStatusCodes.ERROR_DEVICE_NOT_CONNECTED;
        }

        //(xxx,b/226044120)
        permissionCheck(connId, handle);

        gattClientWriteDescriptorNative(connId, handle, authReq, value);
        return BluetoothStatusCodes.SUCCESS;
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void beginReliableWrite(int clientIf, String address, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService beginReliableWrite")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "beginReliableWrite() - address=" + address);
        }
        mReliableQueue.add(address);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void endReliableWrite(
            int clientIf, String address, boolean execute, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService endReliableWrite")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "endReliableWrite() - address=" + address + " execute: " + execute);
        }
        mReliableQueue.remove(address);

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId != null) {
            gattClientExecuteWriteNative(connId, execute);
        }
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void registerForNotification(int clientIf, String address, int handle, boolean enable,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService registerForNotification")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "registerForNotification() - address=" + address + " enable: " + enable);
        }

        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId == null) {
            Log.e(TAG, "registerForNotification() - No connection for " + address + "...");
            return;
        }

        try {
            permissionCheck(connId, handle);
        } catch (SecurityException ex) {
            String callingPackage = attributionSource.getPackageName();
            // Only throws on apps with target SDK T+ as this old API did not throw prior to T
            if (checkCallerTargetSdk(this, callingPackage, Build.VERSION_CODES.TIRAMISU)) {
                throw ex;
            }
            Log.w(TAG, "registerForNotification() - permission check failed!");
            return;
        }

        gattClientRegisterForNotificationsNative(clientIf, address, handle, enable);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void readRemoteRssi(int clientIf, String address, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService readRemoteRssi")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "readRemoteRssi() - address=" + address);
        }
        gattClientReadRemoteRssiNative(clientIf, address);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void configureMTU(int clientIf, String address, int mtu, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService configureMTU")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "configureMTU() - address=" + address + " mtu=" + mtu);
        }
        Integer connId = mClientMap.connIdByAddress(clientIf, address);
        if (connId != null) {
            gattClientConfigureMTUNative(connId, mtu);
        } else {
            Log.e(TAG, "configureMTU() - No connection for " + address + "...");
        }
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void connectionParameterUpdate(int clientIf, String address, int connectionPriority,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService connectionParameterUpdate")) {
            return;
        }

        int minInterval;
        int maxInterval;

        // Peripheral latency
        int latency;

        // Link supervision timeout is measured in N * 10ms
        int timeout = 500; // 5s

        switch (connectionPriority) {
            case BluetoothGatt.CONNECTION_PRIORITY_HIGH:
                minInterval = getResources().getInteger(R.integer.gatt_high_priority_min_interval);
                maxInterval = getResources().getInteger(R.integer.gatt_high_priority_max_interval);
                latency = getResources().getInteger(R.integer.gatt_high_priority_latency);
                break;

            case BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER:
                minInterval = getResources().getInteger(R.integer.gatt_low_power_min_interval);
                maxInterval = getResources().getInteger(R.integer.gatt_low_power_max_interval);
                latency = getResources().getInteger(R.integer.gatt_low_power_latency);
                break;

            default:
                // Using the values for CONNECTION_PRIORITY_BALANCED.
                minInterval =
                        getResources().getInteger(R.integer.gatt_balanced_priority_min_interval);
                maxInterval =
                        getResources().getInteger(R.integer.gatt_balanced_priority_max_interval);
                latency = getResources().getInteger(R.integer.gatt_balanced_priority_latency);
                break;
        }

        if (DBG) {
            Log.d(TAG, "connectionParameterUpdate() - address=" + address + "params="
                    + connectionPriority + " interval=" + minInterval + "/" + maxInterval);
        }
        gattConnectionParameterUpdateNative(clientIf, address, minInterval, maxInterval, latency,
                timeout, 0, 0);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void leConnectionUpdate(int clientIf, String address, int minInterval,
                            int maxInterval, int peripheralLatency,
                            int supervisionTimeout, int minConnectionEventLen,
                            int maxConnectionEventLen, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService leConnectionUpdate")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "leConnectionUpdate() - address=" + address + ", intervals="
                        + minInterval + "/" + maxInterval + ", latency=" + peripheralLatency
                        + ", timeout=" + supervisionTimeout + "msec" + ", min_ce="
                        + minConnectionEventLen + ", max_ce=" + maxConnectionEventLen);


        }
        gattConnectionParameterUpdateNative(clientIf, address, minInterval, maxInterval,
                                            peripheralLatency, supervisionTimeout,
                                            minConnectionEventLen, maxConnectionEventLen);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    public void subrateModeRequest(int clientIf, String address,
            int subrateMode, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService subrateModeRequest")) {
            return;
        }

        int subrateMin;
        int subrateMax;
        int maxLatency;
        int contNumber;
        // Link supervision timeout is measured in N * 10ms
        int supervisionTimeout = 500; // 5s

        switch (subrateMode) {
            case BluetoothGatt.SUBRATE_REQ_HIGH:
                subrateMin =
                        getResources().getInteger(R.integer.subrate_mode_high_priority_min_subrate);
                subrateMax =
                        getResources().getInteger(R.integer.subrate_mode_high_priority_max_subrate);
                maxLatency =
                        getResources().getInteger(R.integer.subrate_mode_high_priority_latency);
                contNumber =
                        getResources().getInteger(R.integer.subrate_mode_high_priority_cont_number);
                break;

            case BluetoothGatt.SUBRATE_REQ_LOW_POWER:
                subrateMin =
                        getResources().getInteger(R.integer.subrate_mode_low_power_min_subrate);
                subrateMax =
                        getResources().getInteger(R.integer.subrate_mode_low_power_max_subrate);
                maxLatency = getResources().getInteger(R.integer.subrate_mode_low_power_latency);
                contNumber = getResources().getInteger(R.integer.subrate_mode_low_power_cont_number);
                break;

            default:
                // Using the values for SUBRATE_REQ_BALANCED.
                subrateMin =
                        getResources().getInteger(R.integer.subrate_mode_balanced_min_subrate);
                subrateMax =
                        getResources().getInteger(R.integer.subrate_mode_balanced_max_subrate);
                maxLatency = getResources().getInteger(R.integer.subrate_mode_balanced_latency);
                contNumber = getResources().getInteger(R.integer.subrate_mode_balanced_cont_number);
                break;
        }

        if (DBG) {
            Log.d(TAG, "subrateModeRequest() - address=" + address + ", subrate min/max="
                  + subrateMin + "/" + subrateMax + ", maxLatency=" + maxLatency
                  + " continuation Number=" + contNumber +", timeout=" + supervisionTimeout);
        }

        gattSubrateRequestNative(clientIf, address, subrateMin, subrateMax, maxLatency,
                                 contNumber, supervisionTimeout);
    }

    void leSubrateRequest(int clientIf, String address, int subrateMin, int subrateMax,
            int maxLatency, int contNumber, int supervisionTimeout,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService leSubrateRequest")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "leSubrateRequest() - address=" + address + ", subrate min/max="
                  + subrateMin + "/" + subrateMax + ", maxLatency=" + maxLatency
                  + " continuation Number=" + contNumber +", timeout=" + supervisionTimeout);
        }

        gattSubrateRequestNative(clientIf, address, subrateMin, subrateMax, maxLatency,
                                 contNumber, supervisionTimeout);
    }

    /**************************************************************************
     * Callback functions - SERVER
     *************************************************************************/

    void onServerRegistered(int status, int serverIf, long uuidLsb, long uuidMsb)
            throws RemoteException {

        UUID uuid = new UUID(uuidMsb, uuidLsb);
        if (DBG) {
            Log.d(TAG, "onServerRegistered() - UUID=" + uuid + ", serverIf=" + serverIf);
        }
        ServerMap.App app = mServerMap.getByUuid(uuid);
        if (app != null) {
            app.id = serverIf;
            app.linkToDeath(new ServerDeathRecipient(serverIf));
            app.callback.onServerRegistered(status, serverIf);
        }
    }

    void onServiceAdded(int status, int serverIf, List<GattDbElement> service)
            throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onServiceAdded(), status=" + status);
        }

        if (status != 0) {
            return;
        }

        GattDbElement svcEl = service.get(0);
        int srvcHandle = svcEl.attributeHandle;

        BluetoothGattService svc = null;

        for (GattDbElement el : service) {
            if (el.type == GattDbElement.TYPE_PRIMARY_SERVICE) {
                mHandleMap.addService(serverIf, el.attributeHandle, el.uuid,
                        BluetoothGattService.SERVICE_TYPE_PRIMARY, 0, false);
                svc = new BluetoothGattService(svcEl.uuid, svcEl.attributeHandle,
                        BluetoothGattService.SERVICE_TYPE_PRIMARY);
            } else if (el.type == GattDbElement.TYPE_SECONDARY_SERVICE) {
                mHandleMap.addService(serverIf, el.attributeHandle, el.uuid,
                        BluetoothGattService.SERVICE_TYPE_SECONDARY, 0, false);
                svc = new BluetoothGattService(svcEl.uuid, svcEl.attributeHandle,
                        BluetoothGattService.SERVICE_TYPE_SECONDARY);
            } else if (el.type == GattDbElement.TYPE_CHARACTERISTIC) {
                mHandleMap.addCharacteristic(serverIf, el.attributeHandle, el.uuid, srvcHandle);
                svc.addCharacteristic(
                        new BluetoothGattCharacteristic(el.uuid, el.attributeHandle, el.properties,
                                el.permissions));
            } else if (el.type == GattDbElement.TYPE_DESCRIPTOR) {
                mHandleMap.addDescriptor(serverIf, el.attributeHandle, el.uuid, srvcHandle);
                List<BluetoothGattCharacteristic> chars = svc.getCharacteristics();
                chars.get(chars.size() - 1)
                        .addDescriptor(new BluetoothGattDescriptor(el.uuid, el.attributeHandle,
                                el.permissions));
            }
        }
        mHandleMap.setStarted(serverIf, srvcHandle, true);

        ServerMap.App app = mServerMap.getById(serverIf);
        if (app != null) {
            app.callback.onServiceAdded(status, svc);
        }
    }

    void onServiceStopped(int status, int serverIf, int srvcHandle) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onServiceStopped() srvcHandle=" + srvcHandle + ", status=" + status);
        }
        if (status == 0) {
            mHandleMap.setStarted(serverIf, srvcHandle, false);
        }
        stopNextService(serverIf, status);
    }

    void onServiceDeleted(int status, int serverIf, int srvcHandle) {
        if (DBG) {
            Log.d(TAG, "onServiceDeleted() srvcHandle=" + srvcHandle + ", status=" + status);
        }
        mHandleMap.deleteService(serverIf, srvcHandle);
    }

    void onClientConnected(String address, boolean connected, int connId, int serverIf)
            throws RemoteException {

        if (DBG) {
            Log.d(TAG,
                    "onClientConnected() connId=" + connId + ", address=" + address + ", connected="
                            + connected);
        }

        ServerMap.App app = mServerMap.getById(serverIf);
        if (app == null) {
            Log.e(TAG, "app not found from id(" + serverIf + ") for received callback");
            return;
        }

        if (connected) {
            mServerMap.addConnection(serverIf, connId, address);
        } else {
            mServerMap.removeConnection(serverIf, connId);
        }

        app.callback.onServerConnectionState((byte) 0, serverIf, connected, address);
    }

    void onServerReadCharacteristic(String address, int connId, int transId, int handle, int offset,
            boolean isLong) throws RemoteException {
        if (VDBG) {
            Log.d(TAG, "onServerReadCharacteristic() connId=" + connId + ", address=" + address
                    + ", handle=" + handle + ", requestId=" + transId + ", offset=" + offset);
        }

        HandleMap.Entry entry = mHandleMap.getByHandle(handle);
        if (entry == null) {
            return;
        }

        mHandleMap.addRequest(transId, handle);

        ServerMap.App app = mServerMap.getById(entry.serverIf);
        if (app == null) {
            Log.e(TAG, "app not found from id(" + entry.serverIf + ") for received callback");
            return;
        }

        app.callback.onCharacteristicReadRequest(address, transId, offset, isLong, handle);
    }

    void onServerReadDescriptor(String address, int connId, int transId, int handle, int offset,
            boolean isLong) throws RemoteException {
        if (VDBG) {
            Log.d(TAG, "onServerReadDescriptor() connId=" + connId + ", address=" + address
                    + ", handle=" + handle + ", requestId=" + transId + ", offset=" + offset);
        }

        HandleMap.Entry entry = mHandleMap.getByHandle(handle);
        if (entry == null) {
            return;
        }

        mHandleMap.addRequest(transId, handle);

        ServerMap.App app = mServerMap.getById(entry.serverIf);
        if (app == null) {
            Log.e(TAG, "app not found from id(" + entry.serverIf + ") for received callback");
            return;
        }

        app.callback.onDescriptorReadRequest(address, transId, offset, isLong, handle);
    }

    void onServerWriteCharacteristic(String address, int connId, int transId, int handle,
            int offset, int length, boolean needRsp, boolean isPrep, byte[] data)
            throws RemoteException {
        if (VDBG) {
            Log.d(TAG, "onServerWriteCharacteristic() connId=" + connId + ", address=" + address
                    + ", handle=" + handle + ", requestId=" + transId + ", isPrep=" + isPrep
                    + ", offset=" + offset);
        }

        HandleMap.Entry entry = mHandleMap.getByHandle(handle);
        if (entry == null) {
            return;
        }

        mHandleMap.addRequest(transId, handle);

        ServerMap.App app = mServerMap.getById(entry.serverIf);
        if (app == null) {
            Log.e(TAG, "app not found from id(" + entry.serverIf + ") for received callback");
            return;
        }

        try {
            app.callback.onCharacteristicWriteRequest(address, transId, offset, length, isPrep,
                    needRsp, handle, data);
        } catch(DeadObjectException e) {
            Log.e(TAG, "error sending onServerWriteCharacteristic callback", e);
            unregisterServer(entry.serverIf);
            return;
        }
    }

    void onServerWriteDescriptor(String address, int connId, int transId, int handle, int offset,
            int length, boolean needRsp, boolean isPrep, byte[] data) throws RemoteException {
        if (VDBG) {
            Log.d(TAG, "onAttributeWrite() connId=" + connId + ", address=" + address + ", handle="
                    + handle + ", requestId=" + transId + ", isPrep=" + isPrep + ", offset="
                    + offset);
        }

        HandleMap.Entry entry = mHandleMap.getByHandle(handle);
        if (entry == null) {
            return;
        }

        mHandleMap.addRequest(transId, handle);

        ServerMap.App app = mServerMap.getById(entry.serverIf);
        if (app == null) {
            Log.e(TAG, "app not found from id(" + entry.serverIf + ") for received callback");
            return;
        }

        app.callback.onDescriptorWriteRequest(address, transId, offset, length, isPrep, needRsp,
                handle, data);
    }

    void onExecuteWrite(String address, int connId, int transId, int execWrite)
            throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onExecuteWrite() connId=" + connId + ", address=" + address + ", transId="
                    + transId);
        }

        ServerMap.App app = mServerMap.getByConnId(connId);
        if (app == null) {
            Log.e(TAG, "app not found from connId(" + connId + ") for received callback");
            return;
        }

        app.callback.onExecuteWrite(address, transId, execWrite == 1);
    }

    void onResponseSendCompleted(int status, int attrHandle) {
        if (DBG) {
            Log.d(TAG, "onResponseSendCompleted() handle=" + attrHandle);
        }
    }

    void onNotificationSent(int connId, int status) throws RemoteException {
        if (VDBG) {
            Log.d(TAG, "onNotificationSent() connId=" + connId + ", status=" + status);
        }

        String address = mServerMap.addressByConnId(connId);
        if (address == null) {
            Log.e(TAG, "address not found for given connId:" + connId);
            return;
        }

        ServerMap.App app = mServerMap.getByConnId(connId);
        if (app == null) {
            Log.e(TAG, "app not found from connId(" + connId + ") for received callback");
            return;
        }

        if (!app.isCongested) {
            app.callback.onNotificationSent(address, status);
        } else {
            if (status == BluetoothGatt.GATT_CONNECTION_CONGESTED) {
                status = BluetoothGatt.GATT_SUCCESS;
            }
            app.queueCallback(new CallbackInfo.Builder(address, status).build());
        }
    }

    void onServerCongestion(int connId, boolean congested) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onServerCongestion() - connId=" + connId + ", congested=" + congested);
        }

        ServerMap.App app = mServerMap.getByConnId(connId);
        if (app == null) {
            Log.e(TAG, "app not found from connId(" + connId + ") for received callback");
            return;
        }

        app.isCongested = congested;
        while (!app.isCongested) {
            CallbackInfo callbackInfo = app.popQueuedCallback();
            if (callbackInfo == null) {
                return;
            }
            app.callback.onNotificationSent(callbackInfo.address, callbackInfo.status);
        }
    }

    void onMtuChanged(int connId, int mtu) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "onMtuChanged() - connId=" + connId + ", mtu=" + mtu);
        }

        String address = mServerMap.addressByConnId(connId);
        if (address == null) {
            Log.e(TAG, "address not found for given connId: " + connId);
            return;
        }

        ServerMap.App app = mServerMap.getByConnId(connId);
        if (app == null) {
            Log.e(TAG, "app not found from connId(" + connId + ") for received callback");
            return;
        }

        app.callback.onMtuChanged(address, mtu);
    }

    /**************************************************************************
     * GATT Service functions - SERVER
     *************************************************************************/

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void registerServer(UUID uuid, IBluetoothGattServerCallback callback, boolean eatt_support,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService registerServer")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "registerServer() - UUID=" + uuid);
        }
        mServerMap.add(uuid, null, callback, null, this);
        gattServerRegisterAppNative(uuid.getLeastSignificantBits(), uuid.getMostSignificantBits(),
                eatt_support);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void unregisterServer(int serverIf, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService unregisterServer")) {
            return;
        }

        unregisterServer(serverIf);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void serverConnect(int serverIf, String address, boolean isDirect, int transport,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService serverConnect")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "serverConnect() - address=" + address);
        }
        gattServerConnectNative(serverIf, address, isDirect, transport);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void serverDisconnect(int serverIf, String address, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService serverDisconnect")) {
            return;
        }

        Integer connId = mServerMap.connIdByAddress(serverIf, address);
        if (DBG) {
            Log.d(TAG, "serverDisconnect() - address=" + address + ", connId=" + connId);
        }

        gattServerDisconnectNative(serverIf, address, connId != null ? connId : 0);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void serverSetPreferredPhy(int serverIf, String address, int txPhy, int rxPhy, int phyOptions,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService serverSetPreferredPhy")) {
            return;
        }

        Integer connId = mServerMap.connIdByAddress(serverIf, address);
        if (connId == null) {
            if (DBG) {
                Log.d(TAG, "serverSetPreferredPhy() - no connection to " + address);
            }
            return;
        }

        if (DBG) {
            Log.d(TAG, "serverSetPreferredPhy() - address=" + address + ", connId=" + connId);
        }
        gattServerSetPreferredPhyNative(serverIf, address, txPhy, rxPhy, phyOptions);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void serverReadPhy(int serverIf, String address, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService serverReadPhy")) {
            return;
        }

        Integer connId = mServerMap.connIdByAddress(serverIf, address);
        if (connId == null) {
            if (DBG) {
                Log.d(TAG, "serverReadPhy() - no connection to " + address);
            }
            return;
        }

        if (DBG) {
            Log.d(TAG, "serverReadPhy() - address=" + address + ", connId=" + connId);
        }
        gattServerReadPhyNative(serverIf, address);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void addService(
            int serverIf, BluetoothGattService service, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService addService")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "addService() - uuid=" + service.getUuid());
        }

        List<GattDbElement> db = new ArrayList<GattDbElement>();

        if (service.getType() == BluetoothGattService.SERVICE_TYPE_PRIMARY) {
            db.add(GattDbElement.createPrimaryService(service.getUuid()));
        } else {
            db.add(GattDbElement.createSecondaryService(service.getUuid()));
        }

        for (BluetoothGattService includedService : service.getIncludedServices()) {
            int inclSrvcHandle = includedService.getInstanceId();

            if (mHandleMap.checkServiceExists(includedService.getUuid(), inclSrvcHandle)) {
                db.add(GattDbElement.createIncludedService(inclSrvcHandle));
            } else {
                Log.e(TAG,
                        "included service with UUID " + includedService.getUuid() + " not found!");
            }
        }

        for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
            int permission =
                    ((characteristic.getKeySize() - 7) << 12) + characteristic.getPermissions();
            db.add(GattDbElement.createCharacteristic(characteristic.getUuid(),
                    characteristic.getProperties(), permission));

            for (BluetoothGattDescriptor descriptor : characteristic.getDescriptors()) {
                permission =
                        ((characteristic.getKeySize() - 7) << 12) + descriptor.getPermissions();
                db.add(GattDbElement.createDescriptor(descriptor.getUuid(), permission));
            }
        }

        gattServerAddServiceNative(serverIf, db);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void removeService(int serverIf, int handle, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService removeService")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "removeService() - handle=" + handle);
        }

        gattServerDeleteServiceNative(serverIf, handle);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void clearServices(int serverIf, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService clearServices")) {
            return;
        }

        if (DBG) {
            Log.d(TAG, "clearServices()");
        }
        deleteServices(serverIf);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    void sendResponse(int serverIf, String address, int requestId, int status, int offset,
            byte[] value, AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService sendResponse")) {
            return;
        }


        if (VDBG) {
            Log.d(TAG, "sendResponse() - address=" + address + " serverIf: " + serverIf
                    + "requestId: " + requestId + " status: " + status + " value: "
                    + Arrays.toString(value));
        }

        int handle = 0;
        HandleMap.Entry entry = mHandleMap.getByRequestId(requestId);
        if (entry != null) {
            handle = entry.handle;
        }

        Integer connId = mServerMap.connIdByAddress(serverIf, address);
        gattServerSendResponseNative(serverIf, connId != null ? connId : 0, requestId,
                (byte) status, handle, offset, value, (byte) 0);
        mHandleMap.deleteRequest(requestId);
    }

    @RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT)
    int sendNotification(int serverIf, String address, int handle, boolean confirm, byte[] value,
            AttributionSource attributionSource) {
        if (!Utils.checkConnectPermissionForDataDelivery(
                this, attributionSource, "GattService sendNotification")) {
            return BluetoothStatusCodes.ERROR_MISSING_BLUETOOTH_CONNECT_PERMISSION;
        }

        if (VDBG) {
            Log.d(TAG, "sendNotification() - address = " + address + " handle = " + handle
                    + " confirm = " + confirm + " value = " + Arrays.toString(value));
        }

        Integer connId = mServerMap.connIdByAddress(serverIf, address);
        if (connId == null || connId == 0) {
            return BluetoothStatusCodes.ERROR_DEVICE_NOT_CONNECTED;
        }

        if (confirm) {
            gattServerSendIndicationNative(serverIf, handle, connId, value);
        } else {
            gattServerSendNotificationNative(serverIf, handle, connId, value);
        }

        return BluetoothStatusCodes.SUCCESS;
    }


    /**************************************************************************
     * Private functions
     *************************************************************************/

    private void unregisterServer(int serverIf) {
        if (DBG) {
            Log.d(TAG, "unregisterServer() - serverIf=" + serverIf);
        }

        deleteServices(serverIf);

        mServerMap.remove(serverIf);
        gattServerUnregisterAppNative(serverIf);
    }

    private boolean isHidSrvcUuid(final UUID uuid) {
        return HID_SERVICE_UUID.equals(uuid);
    }

    private boolean isHidCharUuid(final UUID uuid) {
        for (UUID hidUuid : HID_UUIDS) {
            if (hidUuid.equals(uuid)) {
                return true;
            }
        }
        return false;
    }

    private boolean isAndroidTvRemoteSrvcUuid(final UUID uuid) {
        return ANDROID_TV_REMOTE_SERVICE_UUID.equals(uuid);
    }

    private boolean isFidoSrvcUuid(final UUID uuid) {
        return FIDO_SERVICE_UUID.equals(uuid);
    }

    private int getDeviceType(BluetoothDevice device) {
        int type = gattClientGetDeviceTypeNative(device.getAddress());
        if (DBG) {
            Log.d(TAG, "getDeviceType() - device=" + device + ", type=" + type);
        }
        return type;
    }

    private boolean needsPrivilegedPermissionForScan(ScanSettings settings) {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        // BLE scan only mode needs special permission.
        if (adapter.getState() != BluetoothAdapter.STATE_ON) {
            return true;
        }

        // Regular scan, no special permission.
        if (settings == null) {
            return false;
        }

        // Ambient discovery mode, needs privileged permission.
        if (settings.getScanMode() == ScanSettings.SCAN_MODE_AMBIENT_DISCOVERY) {
            return true;
        }

        // Regular scan, no special permission.
        if (settings.getReportDelayMillis() == 0) {
            return false;
        }

        // Batch scan, truncated mode needs permission.
        return settings.getScanResultType() == ScanSettings.SCAN_RESULT_TYPE_ABBREVIATED;
    }

    /*
     * The {@link ScanFilter#setDeviceAddress} API overloads are @SystemApi access methods.  This
     * requires that the permissions be BLUETOOTH_PRIVILEGED.
     */
    @SuppressLint("AndroidFrameworkRequiresPermission")
    private void enforcePrivilegedPermissionIfNeeded(List<ScanFilter> filters) {
        if (DBG) {
            Log.d(TAG, "enforcePrivilegedPermissionIfNeeded(" + filters + ")");
        }
        // Some 3p API cases may have null filters, need to allow
        if (filters != null) {
            for (ScanFilter filter : filters) {
                // The only case to enforce here is if there is an address
                // If there is an address, enforce if the correct combination criteria is met.
                if (filter.getDeviceAddress() != null) {
                    // At this point we have an address, that means a caller used the
                    // setDeviceAddress(address) public API for the ScanFilter
                    // We don't want to enforce if the type is PUBLIC and the IRK is null
                    // However, if we have a different type that means the caller used a new
                    // @SystemApi such as setDeviceAddress(address, type) or
                    // setDeviceAddress(address, type, irk) which are both @SystemApi and require
                    // permissions to be enforced
                    if (filter.getAddressType()
                            == BluetoothDevice.ADDRESS_TYPE_PUBLIC && filter.getIrk() == null) {
                        // Do not enforce
                    } else {
                        enforceBluetoothPrivilegedPermission(this);
                    }
                }
            }
        }
    }


    @SuppressLint("AndroidFrameworkRequiresPermission")
    private void enforcePrivilegedPermissionIfNeeded(ScanSettings settings) {
        if (needsPrivilegedPermissionForScan(settings)) {
            enforceBluetoothPrivilegedPermission(this);
        }
    }

    // Enforce caller has UPDATE_DEVICE_STATS permission, which allows the caller to blame other
    // apps for Bluetooth usage. A {@link SecurityException} will be thrown if the caller app does
    // not have UPDATE_DEVICE_STATS permission.
    @RequiresPermission(android.Manifest.permission.UPDATE_DEVICE_STATS)
    private void enforceImpersonatationPermission() {
        enforceCallingOrSelfPermission(android.Manifest.permission.UPDATE_DEVICE_STATS,
                "Need UPDATE_DEVICE_STATS permission");
    }

    @SuppressLint("AndroidFrameworkRequiresPermission")
    private void enforceImpersonatationPermissionIfNeeded(WorkSource workSource) {
        if (workSource != null) {
            enforceImpersonatationPermission();
        }
    }

    /**
     * Ensures the report delay is either 0 or at least the floor value (5000ms)
     *
     * @param  settings are the scan settings passed into a request to start le scanning
     * @return the passed in ScanSettings object if the report delay is 0 or above the floor value;
     *         a new ScanSettings object with the report delay being the floor value if the original
     *         report delay was between 0 and the floor value (exclusive of both)
     */
    private ScanSettings enforceReportDelayFloor(ScanSettings settings) {
        if (settings.getReportDelayMillis() == 0) {
            return settings;
        }

        // Need to clear identity to pass device config permission check
        long callerToken = Binder.clearCallingIdentity();
        long floor = DeviceConfig.getLong(DeviceConfig.NAMESPACE_BLUETOOTH, "report_delay",
                DEFAULT_REPORT_DELAY_FLOOR);
        Binder.restoreCallingIdentity(callerToken);

        if (settings.getReportDelayMillis() > floor) {
            return settings;
        } else {
            return new ScanSettings.Builder()
                    .setCallbackType(settings.getCallbackType())
                    .setLegacy(settings.getLegacy())
                    .setMatchMode(settings.getMatchMode())
                    .setNumOfMatches(settings.getNumOfMatches())
                    .setPhy(settings.getPhy())
                    .setReportDelay(floor)
                    .setScanMode(settings.getScanMode())
                    .setScanResultType(settings.getScanResultType())
                    .build();
        }
    }

    private void stopNextService(int serverIf, int status) throws RemoteException {
        if (DBG) {
            Log.d(TAG, "stopNextService() - serverIf=" + serverIf + ", status=" + status);
        }

        if (status == 0) {
            List<HandleMap.Entry> entries = mHandleMap.getEntries();
            for (HandleMap.Entry entry : entries) {
                if (entry.type != HandleMap.TYPE_SERVICE || entry.serverIf != serverIf
                        || !entry.started) {
                    continue;
                }

                gattServerStopServiceNative(serverIf, entry.handle);
                return;
            }
        }
    }

    private void deleteServices(int serverIf) {
        if (DBG) {
            Log.d(TAG, "deleteServices() - serverIf=" + serverIf);
        }

        /*
         * Figure out which handles to delete.
         * The handles are copied into a new list to avoid race conditions.
         */
        List<Integer> handleList = new ArrayList<Integer>();
        for (HandleMap.Entry entry : mHandleMap.getEntries()) {
            if (entry.type != HandleMap.TYPE_SERVICE || entry.serverIf != serverIf) {
                continue;
            }
            handleList.add(entry.handle);
        }

        if (VDBG) Log.v(TAG, "delete Services handleList : " + handleList);

        /* Now actually delete the services.... */
        for (Integer handle : handleList) {
            gattServerDeleteServiceNative(serverIf, handle);
        }
    }

    void dumpRegisterId(StringBuilder sb) {
        sb.append("  Scanner:\n");
        for (Integer appId : mScannerMap.getAllAppsIds()) {
            println(sb, "    app_if: " + appId + ", appName: " + mScannerMap.getById(appId).name);
        }
        sb.append("  Client:\n");
        for (Integer appId : mClientMap.getAllAppsIds()) {
            println(sb, "    app_if: " + appId + ", appName: " + mClientMap.getById(appId).name);
        }
        sb.append("  Server:\n");
        for (Integer appId : mServerMap.getAllAppsIds()) {
            println(sb, "    app_if: " + appId + ", appName: " + mServerMap.getById(appId).name);
        }
        sb.append("\n\n");
    }

    @Override
    public void dump(StringBuilder sb) {
        super.dump(sb);
        println(sb, "mAdvertisingServiceUuids:");
        for (UUID uuid : mAdvertisingServiceUuids) {
            println(sb, "  " + uuid);
        }

        println(sb, "mMaxScanFilters: " + mMaxScanFilters);

        sb.append("\nRegistered App\n");
        dumpRegisterId(sb);

        sb.append("GATT Scanner Map\n");
        mScannerMap.dump(sb);

        sb.append("GATT Client Map\n");
        mClientMap.dump(sb);

        sb.append("GATT Server Map\n");
        mServerMap.dump(sb);

        sb.append("GATT Handle Map\n");
        mHandleMap.dump(sb);
    }

    void addScanEvent(BluetoothMetricsProto.ScanEvent event) {
        synchronized (mScanEvents) {
            if (mScanEvents.size() == NUM_SCAN_EVENTS_KEPT) {
                mScanEvents.remove();
            }
            mScanEvents.add(event);
        }
    }

    @Override
    public void dumpProto(BluetoothMetricsProto.BluetoothLog.Builder builder) {
        synchronized (mScanEvents) {
            builder.addAllScanEvent(mScanEvents);
        }
    }

    private ScanResult getScanResultInstance(BluetoothDevice device, ScanRecord scanRecord,
             int rssi, long timestampNanos) {
         int eventType = (ScanResult.DATA_COMPLETE << 5) | ET_LEGACY_MASK | ET_CONNECTABLE_MASK;
         int txPower = 127;
         int periodicAdvertisingInterval = 0;
         return new ScanResult(device, eventType, BluetoothDevice.PHY_LE_1M, ScanResult.PHY_UNUSED,
             ScanResult.SID_NOT_PRESENT, txPower, rssi, periodicAdvertisingInterval,
             scanRecord, timestampNanos);
    }

     /**************************************************************************
     * GATT Test functions
     *************************************************************************/

    void gattTestCommand(int command, UUID uuid1, String bda1, int p1, int p2, int p3, int p4,
            int p5) {
        if (bda1 == null) {
            bda1 = "00:00:00:00:00:00";
        }
        if (uuid1 != null) {
            gattTestNative(command, uuid1.getLeastSignificantBits(), uuid1.getMostSignificantBits(),
                    bda1, p1, p2, p3, p4, p5);
        } else {
            gattTestNative(command, 0, 0, bda1, p1, p2, p3, p4, p5);
        }
    }

    private native void gattTestNative(int command, long uuid1Lsb, long uuid1Msb, String bda1,
            int p1, int p2, int p3, int p4, int p5);

    /**************************************************************************
     * Native functions prototypes
     *************************************************************************/

    private static native void classInitNative();

    private native void initializeNative();

    private native void cleanupNative();

    private native int gattClientGetDeviceTypeNative(String address);

    private native void gattClientRegisterAppNative(long appUuidLsb, long appUuidMsb,
            boolean eatt_support);

    private native void gattClientUnregisterAppNative(int clientIf);

    private native void gattClientConnectNative(int clientIf, String address, boolean isDirect,
            int transport, boolean opportunistic, int initiatingPhys);

    private native void gattClientDisconnectNative(int clientIf, String address, int connId);

    private native void gattClientSetPreferredPhyNative(int clientIf, String address, int txPhy,
            int rxPhy, int phyOptions);

    private native void gattClientReadPhyNative(int clientIf, String address);

    private native void gattClientRefreshNative(int clientIf, String address);

    private native void gattClientSearchServiceNative(int connId, boolean searchAll,
            long serviceUuidLsb, long serviceUuidMsb);

    private native void gattClientDiscoverServiceByUuidNative(int connId, long serviceUuidLsb,
            long serviceUuidMsb);

    private native void gattClientGetGattDbNative(int connId);

    private native void gattClientReadCharacteristicNative(int connId, int handle, int authReq);

    private native void gattClientReadUsingCharacteristicUuidNative(int connId, long uuidMsb,
            long uuidLsb, int sHandle, int eHandle, int authReq);

    private native void gattClientReadDescriptorNative(int connId, int handle, int authReq);

    private native void gattClientWriteCharacteristicNative(int connId, int handle, int writeType,
            int authReq, byte[] value);

    private native void gattClientWriteDescriptorNative(int connId, int handle, int authReq,
            byte[] value);

    private native void gattClientExecuteWriteNative(int connId, boolean execute);

    private native void gattClientRegisterForNotificationsNative(int clientIf, String address,
            int handle, boolean enable);

    private native void gattClientReadRemoteRssiNative(int clientIf, String address);

    private native void gattClientConfigureMTUNative(int connId, int mtu);

    private native void gattConnectionParameterUpdateNative(int clientIf, String address,
            int minInterval, int maxInterval, int latency, int timeout, int minConnectionEventLen,
            int maxConnectionEventLen);

    private native void gattServerRegisterAppNative(long appUuidLsb, long appUuidMsb,
            boolean eatt_support);

    private native void gattServerUnregisterAppNative(int serverIf);

    private native void gattServerConnectNative(int serverIf, String address, boolean isDirect,
            int transport);

    private native void gattServerDisconnectNative(int serverIf, String address, int connId);

    private native void gattServerSetPreferredPhyNative(int clientIf, String address, int txPhy,
            int rxPhy, int phyOptions);

    private native void gattServerReadPhyNative(int clientIf, String address);

    private native void gattServerAddServiceNative(int serverIf, List<GattDbElement> service);

    private native void gattServerStopServiceNative(int serverIf, int svcHandle);

    private native void gattServerDeleteServiceNative(int serverIf, int svcHandle);

    private native void gattServerSendIndicationNative(int serverIf, int attrHandle, int connId,
            byte[] val);

    private native void gattServerSendNotificationNative(int serverIf, int attrHandle, int connId,
            byte[] val);

    private native void gattServerSendResponseNative(int serverIf, int connId, int transId,
            int status, int handle, int offset, byte[] val, int authReq);

    private native void gattSubrateRequestNative(int clientIf, String address, int subrateMin,
            int subrateMax, int maxLatency, int contNumber, int supervisionTimeout);
}
