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
 */

package com.android.bluetooth.gatt;

import android.bluetooth.le.IPeriodicAdvertisingCallback;
import android.bluetooth.le.PeriodicAdvertisingReport;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.os.IBinder;
import android.os.IInterface;
import android.os.RemoteException;
import android.util.Log;

import com.android.bluetooth.btservice.AdapterService;
import android.bluetooth.BluetoothAdapter;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import android.bluetooth.BluetoothDevice;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Manages Bluetooth LE Periodic scans
 *
 * @hide
 */
class PeriodicScanManager {
    private static final boolean DBG = GattServiceConfig.DBG;
    private static final String TAG = GattServiceConfig.TAG_PREFIX + "SyncManager";

    private final AdapterService mAdapterService;
    private final BluetoothAdapter mAdapter;
    //Map<IBinder, SyncInfo> mSyncs = Collections.synchronizedMap(new HashMap<>());
    Map<IBinder, SyncInfo> mSyncs = new ConcurrentHashMap<>();
    Map<IBinder, SyncTransferInfo> mSyncTransfers = Collections.synchronizedMap(new HashMap<>());
    static int sTempRegistrationId = -1;
    private int PA_SOURCE_LOCAL = 1;
    private int PA_SOURCE_REMOTE = 2;
    /**
     * Constructor of {@link SyncManager}.
     */
    PeriodicScanManager(AdapterService adapterService) {
        if (DBG) {
            Log.d(TAG, "advertise manager created");
        }
        mAdapterService = adapterService;
        mAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    void start() {
        initializeNative();
    }

    void cleanup() {
        if (DBG) {
            Log.d(TAG, "cleanup()");
        }
        cleanupNative();
        mSyncs.clear();
        sTempRegistrationId = -1;
    }

    class SyncTransferInfo {
        public String address;
        public SyncDeathRecipient deathRecipient;
        public IPeriodicAdvertisingCallback callback;

        SyncTransferInfo(String address, IPeriodicAdvertisingCallback callback) {
            this.address = address;
            this.callback = callback;
        }
    }
    class SyncInfo {
        /* When id is negative, the registration is ongoing. When the registration finishes, id
         * becomes equal to sync_handle */
        public Integer id;
        public Integer adv_sid;
        public String address;
        public Integer skip;
        public Integer timeout;
        public SyncDeathRecipient deathRecipient;
        public IPeriodicAdvertisingCallback callback;

        SyncInfo(Integer id, Integer adv_sid, String address, Integer skip, Integer timeout,
                SyncDeathRecipient deathRecipient,
                IPeriodicAdvertisingCallback callback) {
            this.id = id;
            this.adv_sid = adv_sid;
            this.address = address;
            this.skip = skip;
            this.timeout = timeout;
            this.deathRecipient = deathRecipient;
            this.callback = callback;
        }
    }
    Map.Entry<IBinder, SyncTransferInfo> findSyncTransfer(String address) {
        Map.Entry<IBinder, SyncTransferInfo> entry = null;
        for (Map.Entry<IBinder, SyncTransferInfo> e : mSyncTransfers.entrySet()) {
            if (e.getValue().address.equals(address)) {
                entry = e;
                break;
            }
        }
        return entry;
    }
    IBinder toBinder(IPeriodicAdvertisingCallback e) {
        return ((IInterface) e).asBinder();
    }

    class SyncDeathRecipient implements IBinder.DeathRecipient {
        public IPeriodicAdvertisingCallback callback;

        SyncDeathRecipient(IPeriodicAdvertisingCallback callback) {
            this.callback = callback;
        }

        @Override
        public void binderDied() {
            if (DBG) {
                Log.d(TAG, "Binder is dead - unregistering advertising set");
            }
            stopSync(callback);
        }
    }

    Map.Entry<IBinder, SyncInfo> findSync(int syncHandle) {
        Map.Entry<IBinder, SyncInfo> entry = null;
        for (Map.Entry<IBinder, SyncInfo> e : mSyncs.entrySet()) {
            if (e.getValue().id == syncHandle) {
                entry = e;
                break;
            }
        }
        return entry;
    }

    Map.Entry<IBinder, SyncInfo> findMatchingSync(int adv_sid, String address) {
        Map.Entry<IBinder, SyncInfo> entry = null;
        for (Map.Entry<IBinder, SyncInfo> e : mSyncs.entrySet()) {
            if (e.getValue().adv_sid == adv_sid && e.getValue().address.equals(address)) {
                return entry = e;
            }
        }
        return entry;
    }
    Map<IBinder, SyncInfo> findAllSync(int syncHandle) {
        Map <IBinder, SyncInfo> syncMap = new HashMap<IBinder, SyncInfo>();
        for (Map.Entry<IBinder, SyncInfo> e : mSyncs.entrySet()) {
            if (e.getValue().id == syncHandle) {
                syncMap.put(e.getKey(),new SyncInfo(e.getValue().id, e.getValue().adv_sid, e.getValue().address,
                e.getValue().skip, e.getValue().timeout, e.getValue().deathRecipient, e.getValue().callback));
            }
        }
        return syncMap;
    }
    void onSyncStarted(int regId, int syncHandle, int sid, int addressType, String address, int phy,
            int interval, int status) throws Exception {
        if (DBG) {
            Log.d(TAG,
                    "onSyncStarted() - regId=" + regId + ", syncHandle=" + syncHandle + ", status="
                            + status);
        }
        Map<IBinder, SyncInfo> syncMap = findAllSync(regId);
        if (syncMap.size() == 0) {
            Log.d(TAG,"onSyncStarted() - no callback found for regId " + regId);
            stopSyncNative(syncHandle);
            return;
        }

        synchronized (mSyncs) {
            for (Map.Entry<IBinder, SyncInfo> e : mSyncs.entrySet()) {
                if (e.getValue().id == regId) {
                    IPeriodicAdvertisingCallback callback = e.getValue().callback;
                    if (status == 0) {
                        Log.d(TAG,"onSyncStarted: updating id with syncHandle " + syncHandle);
                        e.setValue(new SyncInfo(syncHandle, sid, address, e.getValue().skip,
                            e.getValue().timeout, e.getValue().deathRecipient, callback));
                        callback.onSyncEstablished(syncHandle, mAdapter.getRemoteDevice(address),
                                           sid, e.getValue().skip, e.getValue().timeout, status);
                    } else {
                        callback.onSyncEstablished(syncHandle, mAdapter.getRemoteDevice(address),
                                           sid, e.getValue().skip, e.getValue().timeout, status);
                        IBinder binder = e.getKey();
                        binder.unlinkToDeath(e.getValue().deathRecipient, 0);
                        mSyncs.remove(binder);
                    }
                }
            }
        }
    }
    void onSyncReport(int syncHandle, int txPower, int rssi, int dataStatus, byte[] data)
            throws Exception {
        if (DBG) {
            Log.d(TAG, "onSyncReport() - syncHandle=" + syncHandle);
        }

        Map<IBinder, SyncInfo> syncMap = findAllSync(syncHandle);
        if (syncMap.size() == 0) {
            Log.i(TAG, "onSyncReport() - no callback found for syncHandle " + syncHandle);
            return;
        }
        for (Map.Entry<IBinder, SyncInfo> e :syncMap.entrySet()) {
            IPeriodicAdvertisingCallback callback = e.getValue().callback;
            PeriodicAdvertisingReport report =
                    new PeriodicAdvertisingReport(syncHandle, txPower, rssi, dataStatus,
                            ScanRecord.parseFromBytes(data));
            callback.onPeriodicAdvertisingReport(report);
        }
    }

    void onSyncLost(int syncHandle) throws Exception {
        if (DBG) {
            Log.d(TAG, "onSyncLost() - syncHandle=" + syncHandle);
        }
        Map<IBinder, SyncInfo> syncMap = findAllSync(syncHandle);
        if (syncMap.size() == 0) {
            Log.i(TAG, "onSyncLost() - no callback found for syncHandle " + syncHandle);
            return;
        }
        for (Map.Entry<IBinder, SyncInfo> e :syncMap.entrySet()) {
            IPeriodicAdvertisingCallback callback = e.getValue().callback;
            IBinder binder = toBinder(callback);
            synchronized(mSyncs) {
                mSyncs.remove(binder);
            }
            callback.onSyncLost(syncHandle);

        }
    }

    void onBigInfoReport(int syncHandle, boolean encrypted)
        throws Exception {
        if (DBG) {
            Log.d(TAG, "onBigInfoReport() - syncHandle=" + syncHandle +
                    " , encrypted=" + encrypted);
        }
        Map<IBinder, SyncInfo> syncMap = findAllSync(syncHandle);
        if (syncMap.isEmpty()) {
            Log.i(TAG, "onBigInfoReport() - no callback found for syncHandle " + syncHandle);
            return;
        }
        for (Map.Entry<IBinder, SyncInfo> e :syncMap.entrySet()) {
            IPeriodicAdvertisingCallback callback = e.getValue().callback;
            callback.onBigInfoAdvertisingReport(syncHandle, encrypted);
        }
    }

    void startSync(ScanResult scanResult, int skip, int timeout,
            IPeriodicAdvertisingCallback callback) {
        SyncDeathRecipient deathRecipient = new SyncDeathRecipient(callback);
        IBinder binder = toBinder(callback);
        try {
            binder.linkToDeath(deathRecipient, 0);
        } catch (RemoteException e) {
            throw new IllegalArgumentException("Can't link to periodic scanner death");
        }

        String address = scanResult.getDevice().getAddress();
        int sid = scanResult.getAdvertisingSid();
        if (DBG) {
            Log.d(TAG,"startSync for Device: " + address + " sid: " + sid);
        }
        synchronized(mSyncs) {
            Map.Entry<IBinder, SyncInfo> entry = findMatchingSync(sid, address);
            if (entry != null) {
                //Found matching sync. Copy sync handle
                if (DBG) {
                    Log.d(TAG,"startSync: Matching entry found");
                }
                mSyncs.put(binder, new SyncInfo(entry.getValue().id, sid, address, entry.getValue().skip,
                           entry.getValue().timeout, deathRecipient, callback));
                if (entry.getValue().id >= 0) {
                    try {
                        callback.onSyncEstablished(entry.getValue().id, mAdapter.getRemoteDevice(address),
                                 sid, entry.getValue().skip, entry.getValue().timeout, 0 /*success*/);
                    } catch (RemoteException e) {
                        throw new IllegalArgumentException("Can't invoke callback");
                    }
                } else {
                   Log.d(TAG, "startSync(): sync pending for same remote");
                }
                return;
            }
        }

        int cbId = --sTempRegistrationId;
        mSyncs.put(binder, new SyncInfo(cbId, sid, address, skip, timeout, deathRecipient, callback));

        if (DBG) {
            Log.d(TAG, "startSync() - reg_id=" + cbId + ", callback: " + binder);
        }
        startSyncNative(sid, address, skip, timeout, cbId);
    }

    void stopSync(IPeriodicAdvertisingCallback callback) {
        IBinder binder = toBinder(callback);
        if (DBG) {
            Log.d(TAG, "stopSync() " + binder);
        }
        SyncInfo sync = null;
        synchronized(mSyncs) {
            sync = mSyncs.remove(binder);
        }
        if (sync == null) {
            Log.e(TAG, "stopSync() - no client found for callback");
            return;
        }

        Integer syncHandle = sync.id;
        binder.unlinkToDeath(sync.deathRecipient, 0);
        Log.d(TAG,"stopSync: " + syncHandle);

        synchronized(mSyncs) {
            Map.Entry<IBinder, SyncInfo> entry = findSync(syncHandle);
            if (entry != null) {
                Log.d(TAG,"stopSync() - another app synced to same PA, not stopping sync");
                return;
            }
        }
        Log.d(TAG,"calling stopSyncNative: " + syncHandle.intValue());
        if (syncHandle < 0) {
            Log.i(TAG, "cancelSync() - sync not established yet");
            cancelSyncNative(sync.adv_sid, sync.address);
        } else {
            stopSyncNative(syncHandle.intValue());
        }
    }

    void onSyncTransferredCallback(int pa_source, int status, String bda) {
        Log.d(TAG, "onSyncTransferredCallback()");
        Map.Entry<IBinder, SyncTransferInfo>entry = findSyncTransfer(bda);
        if (entry != null) {
            mSyncTransfers.remove(entry);
            IPeriodicAdvertisingCallback callback = entry.getValue().callback;
            try {
                callback.onSyncTransferred(mAdapter.getRemoteDevice(bda), status);
            } catch (RemoteException e) {
                throw new IllegalArgumentException("Can't find callback for sync transfer");
            }
        }
    }
    void transferSync(BluetoothDevice bda, int service_data, int sync_handle) {
        Log.d(TAG, "transferSync()");
        Map.Entry<IBinder, SyncInfo> entry = findSync(sync_handle);
        if (entry == null) {
            Log.d(TAG,"transferSync: callback not registered");
            return;
        }
        //check for duplicate transfers
        mSyncTransfers.put(entry.getKey(), new SyncTransferInfo(bda.getAddress(), entry.getValue().callback));
        syncTransferNative(PA_SOURCE_REMOTE, bda.getAddress(), service_data, sync_handle);
    }

    void transferSetInfo(BluetoothDevice bda, int service_data,
                  int adv_handle, IPeriodicAdvertisingCallback callback) {
        SyncDeathRecipient deathRecipient = new SyncDeathRecipient(callback);
        IBinder binder = toBinder(callback);
        if (DBG) {
            Log.d(TAG, "transferSetInfo() " + binder);
        }
        try {
            binder.linkToDeath(deathRecipient, 0);
        } catch (RemoteException e) {
            throw new IllegalArgumentException("Can't link to periodic scanner death");
        }
        mSyncTransfers.put(binder, new SyncTransferInfo(bda.getAddress(), callback));
        TransferSetInfoNative(PA_SOURCE_LOCAL, bda.getAddress(), service_data, adv_handle);
    }

    static {
        classInitNative();
    }

    private static native void classInitNative();

    private native void initializeNative();

    private native void cleanupNative();

    private native void startSyncNative(int sid, String address, int skip, int timeout, int regId);

    private native void stopSyncNative(int syncHandle);

    private native void cancelSyncNative(int sid, String address);

    private native void syncTransferNative(int pa_source, String address, int service_data, int sync_handle);

    private native void TransferSetInfoNative(int pa_source, String address, int service_data, int adv_handle);
}
